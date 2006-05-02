/*
* "$Id$"
*
* © Copyright 2004 Apple Computer, Inc. All rights reserved.
* 
* IMPORTANT:  This Apple software is supplied to you by Apple Computer,
* Inc. ("Apple") in consideration of your agreement to the following
* terms, and your use, installation, modification or redistribution of
* this Apple software constitutes acceptance of these terms.  If you do
* not agree with these terms, please do not use, install, modify or
* redistribute this Apple software.
* 
* In consideration of your agreement to abide by the following terms, and
* subject to these terms, Apple grants you a personal, non-exclusive
* license, under Apple’s copyrights in this original Apple software (the
* "Apple Software"), to use, reproduce, modify and redistribute the Apple
* Software, with or without modifications, in source and/or binary forms;
* provided that if you redistribute the Apple Software in its entirety and
* without modifications, you must retain this notice and the following
* text and disclaimers in all such redistributions of the Apple Software. 
* Neither the name, trademarks, service marks or logos of Apple Computer,
* Inc. may be used to endorse or promote products derived from the Apple
* Software without specific prior written permission from Apple.  Except
* as expressly stated in this notice, no other rights or licenses, express
* or implied, are granted by Apple herein, including but not limited to
* any patent rights that may be infringed by your derivative works or by
* other works in which the Apple Software may be incorporated.
* 
* The Apple Software is provided by Apple on an "AS IS" basis.  APPLE
* MAKES NO WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION
* THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS
* FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND
* OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS.
* 
* IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL
* OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION,
* MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED
* AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE),
* STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*
*
* This program implements the Printer Access Protocol (PAP) on top of AppleTalk Transaction
* Protocol (ATP). If it were to use the blocking pap functions of the AppleTalk library it 
* would need seperate threads for reading, writing and status.
*
* Contents:
*
*  main()		- Send a file to the specified Appletalk printer.
*  listDevices()	- List all LaserWriter printers in the local zone.
*  printFile()		- Print from a file descriptor to an NBP specified printer.
*  papOpen()		- Open a pap session to a printer.
*  papClose()		- Close a pap session after cleaning up pending transactions.
*  papWrite()		- Write bytes to a printer.
*  papCloseResp()	- Send a pap close response in the rare case we receive a close connection request.
*  papSendRequest()	- Fomrat and send a pap packet.
*  papCancelRequest()	- Cancel a pending pap request.
*  statusUpdate()	- Print printer status to stderr.
*  parseUri()		- Extract the print name and zone from a uri.
*  addPercentEscapes()	- Encode a string with percent escapes.
*  removePercentEscapes	- Returns a string with any percent escape sequences replaced with their equivalent.
*  nbptuple_compare()	- Compare routine for qsort.
*  okayToUseAppleTalk() - Returns true if AppleTalk is available and enabled.
*  connectTimeout()	- Returns the connect timeout preference value.
*  signalHandler()	- handle SIGINT to close the session before quiting.
*/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <termios.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>

#include <sys/fcntl.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/errno.h>

#include <netat/appletalk.h>
#include <netat/atp.h>
#include <netat/ddp.h>
#include <netat/nbp.h>
#include <netat/pap.h>

#include <cups/http.h>

#include <libkern/OSByteOrder.h>

#ifdef HAVE_APPLETALK_AT_PROTO_H
#  include <AppleTalk/at_proto.h>
#else
#define ZIP_DEF_INTERFACE NULL
enum { RUNNING, NOTLOADED, LOADED, OTHERERROR };	/* Appletalk Stack status Function. */

extern int atp_abort(int fd, at_inet_t *dest, u_short tid);
extern int atp_close(int fd);
extern int atp_getreq(int fd, at_inet_t *src, char *buf, int *len, int *userdata, int *xo, u_short *tid, u_char *bitmap, int nowait);
extern int atp_getresp(int fd, u_short *tid, at_resp_t *resp);
extern int atp_look(int fd);
extern int atp_open(at_socket *socket);
extern int atp_sendreq(int fd, at_inet_t *dest, char *buf, int len, int userdata, int xo, int xo_relt, u_short *tid, at_resp_t *resp, at_retry_t *retry, int nowait);
extern int atp_sendrsp(int fd, at_inet_t *dest, int xo, u_short tid, at_resp_t *resp);
extern int checkATStack();
extern int nbp_lookup(at_entity_t *entity, at_nbptuple_t *buf, int max, at_retry_t *retry);
extern int nbp_make_entity(at_entity_t *entity, char *obj, char *type, char *zone);
extern int zip_getmyzone(char *ifName,	at_nvestr_t *zone);
#endif /* HAVE_APPLETALK_AT_PROTO_H */

#include <CoreFoundation/CFURL.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFPreferences.h>

/* Defines */
#define MAX_PRINTERS	500        /* Max number of printers we can lookup in listDevices */
#define PAP_CONNID	0
#define PAP_TYPE	1
#define PAP_EOF		2

#define CONNID_OF(p)	(((u_char *)&p)[0])
#define TYPE_OF(p)	(((u_char *)&p)[1])
#define SEQUENCE_NUM(p)	(((u_char *)&p)[2])
#define IS_PAP_EOF(p)	(((u_char *)&p)[2])

#define  PAPPacketStr(x) \
  ((x) == AT_PAP_TYPE_OPEN_CONN)	? "PAP_OPEN_CONN"        : \
  ((x) == AT_PAP_TYPE_OPEN_CONN_REPLY)	? "PAP_OPEN_CONN_REPLY"  : \
  ((x) == AT_PAP_TYPE_SEND_DATA)	? "PAP_SEND_DATA"        : \
  ((x) == AT_PAP_TYPE_DATA)		? "PAP_DATA"             : \
  ((x) == AT_PAP_TYPE_TICKLE)		? "PAP_TICKLE"           : \
  ((x) == AT_PAP_TYPE_CLOSE_CONN)	? "PAP_CLOSE_CONN"       : \
  ((x) == AT_PAP_TYPE_CLOSE_CONN_REPLY)	? "PAP_CLOSE_CONN_REPLY" : \
  ((x) == AT_PAP_TYPE_SEND_STATUS)	? "PAP_SEND_STATUS"      : \
  ((x) == AT_PAP_TYPE_SEND_STS_REPLY)	? "PAP_SEND_STS_REPLY"   : \
  ((x) == AT_PAP_TYPE_READ_LW)		? "PAP_READ_LW"          : \
  "<Unknown>"

#ifndef true
#define true	1
#define false 	0
#endif

/* Globals */
int       gSockfd	= 0;		/* Socket descriptor                */
at_inet_t gSessionAddr	= { 0 };	/* Address of the session responding socket    */
u_char    gConnID	= 0;		/* PAP session connection id            */
u_short   gSendDataID	= 0;		/* Transaction id of our pending send-data request  */
u_short   gTickleID	= 0;		/* Transaction id of our outstanding tickle request*/
int       gWaitEOF	= false;	/* Option: causes us to wait for a remote's EOF  */
int       gStatusInterval= 5;		/* Option: 0=off else seconds between status requests*/
int       gErrorlogged  = false;	/* If an error was logged don't send any more INFO messages */
int       gDebug	= 0;		/* Option: causes us to emit debugging info    */

/* Local functions */
static int listDevices(void);
static int printFile(char* name, char* type, char* zone, int fdin, int fdout, int fderr, int copies, int argc);
static int papOpen(at_nbptuple_t* tuple, u_char* connID, int* fd, at_inet_t* pap_to, u_char* flowQuantum);
static int papClose(int abort);
static int papWrite(int sockfd, at_inet_t* dest, u_short tid, u_char connID, u_char flowQuantum, char* data, int len, int eof);
static int papCloseResp(int sockfd, at_inet_t* dest, int xo, u_short tid, u_char connID);
static int papSendRequest(int sockfd, at_inet_t* dest, u_char connID, int function, u_char bitmap, int xo, int seqno);
static int papCancelRequest(int sockfd, u_short tid);
static void statusUpdate(char* status, u_char statusLen);
static int parseUri(const char* argv0, char* name, char* type, char* zone);
static int addPercentEscapes(const char* src, char* dst, int dstMax);
static int removePercentEscapes(const char* src, char* dst, int dstMax);
static int nbptuple_compare(const void *p1, const void *p2);
static int okayToUseAppleTalk(void);
static int connectTimeout(void);
static void signalHandler(int sigraised);


/*!
 * @function  main
 * @abstract  Send a file to the specified AppleTalk PAP address.
 *
 * Usage:  printer-uri job-id user title copies options [file]
 *
 * @param  argc  # of arguments
 * @param  argv  array of arguments
 *
 * @result    A non-zero return value for errors
 */
int main (int argc, const char * argv[])
{
  int   err = 0;
  FILE  *fp;				/* Print file */
  int   copies;				/* Number of copies to print */
  char  name[NBP_NVE_STR_SIZE + 1];	/* +1 for a nul */
  char  type[NBP_NVE_STR_SIZE + 1];	/* +1 for a nul */
  char  zone[NBP_NVE_STR_SIZE + 1];	/* +1 for a nul */

  /* Make sure status messages are not buffered... */
  setbuf(stderr, NULL);

  if (argc == 1)
  {
    puts("network pap \"Unknown\" \"AppleTalk Printer Access Protocol (pap)\"");
    return 0;
  }

  if (argc == 2 && strcmp(argv[1], "-discover") == 0)
  {
    /* Ignore errors returned by listDevices - they may be transitory 
    *  and we don't want cupsd to think that pap is forever unusable.
    */
    listDevices();
    return 0;
  }

  if (argc < 6 || argc > 7)
  {
    fprintf(stderr, "argc = %d\n", argc);
    for (err = 0; err < argc; err++) {
      fprintf(stderr, "%02d:%s\n", err, argv[err]);
    }
    fprintf(stderr, "Usage: pap job-id user title copies options [file]\n");
    exit(EINVAL);
  }

  /* If we have 7 arguments, print the file named on the command-line.
  *  Otherwise, send stdin instead...
  */
  if (argc == 6)
  {
    fp   = stdin;
    copies = 1;
  }
  else
  {
    fprintf(stderr, "DEBUG: opening print file \"%s\"\n", argv[6]);

    /* Try to open the print file... */
    if ((fp = fopen(argv[6], "rb")) == NULL)
    {
      fprintf(stderr, "ERROR: unable to open print file \"%s\": %s\n", argv[6], strerror(errno));
      return (1);
    }

    copies = atoi(argv[4]);
  }

  /* Extract the device name and options from the URI... */
  parseUri(argv[0], name, type, zone);

  err = printFile(name, type, zone, fileno(fp), STDOUT_FILENO, STDERR_FILENO, copies, argc);

  if (fp != stdin)
    fclose(fp);

  /* Only clear the last status if there wasn't an error */
  if (err == noErr && !gErrorlogged)
    fprintf(stderr, "INFO:\n");

  return err;
}


/*!
 * @function  listDevices
 * @abstract  Print a list of all LaserWriter type devices registered in the default zone.
 *
 * @result    A non-zero return value for errors
 */
static int listDevices(void)
{
  int  err = noErr;
  int  index;
  int  numberFound;

  at_nvestr_t   at_zone;
  at_entity_t   entity;
  at_nbptuple_t buf[MAX_PRINTERS];
  at_retry_t    retry;
  char		name[NBP_NVE_STR_SIZE+1];
  char		encodedName[(3 * NBP_NVE_STR_SIZE) + 1];
  char		zone[NBP_NVE_STR_SIZE+1];
  char		encodedZone[(3 * NBP_NVE_STR_SIZE) + 1];

  /* Make sure it's okay to use appletalk */
  if (!okayToUseAppleTalk())
  {
    fprintf(stderr, "ERROR: AppleTalk disabled in System Preferences\n");
    return -1;  /* Network is down */
  }

  if ((err = zip_getmyzone(ZIP_DEF_INTERFACE, &at_zone)) != 0)
  {
    perror("ERROR: Unable to get default AppleTalk zone");
    return -2;
  }
  memcpy(zone, at_zone.str, MIN(at_zone.len, sizeof(zone)-1));
  zone[MIN(at_zone.len, sizeof(zone)-1)] = '\0';

  err = addPercentEscapes(zone, encodedZone, sizeof(encodedZone));

  /* Look up all the printers in our zone */
  nbp_make_entity(&entity, "=", "LaserWriter", zone);
  retry.retries = 1;
  retry.interval = 1;
  retry.backoff = 1;

  if ((numberFound = nbp_lookup(&entity, buf, MAX_PRINTERS, &retry)) < 0)
  {
    perror("ERROR: Unable to lookup AppleTalk printers");
    return numberFound;
  }

  if (numberFound >= MAX_PRINTERS)
    fprintf(stderr, "WARNING: Adding only the first %d printers found", MAX_PRINTERS);

  /* Not required but sort them so they look nice */
  qsort(buf, numberFound, sizeof(at_nbptuple_t), nbptuple_compare);

  for (index = 0; index < numberFound; index++) 
  {
    memcpy(name, buf[index].enu_entity.object.str, MIN(buf[index].enu_entity.object.len, sizeof(name)-1));
    name[MIN(buf[index].enu_entity.object.len, sizeof(name)-1)] = '\0';

    if (addPercentEscapes(name, encodedName, sizeof(encodedName)) == 0)
    {
      /* Each line is of the form: "class URI "make model" "name" */
      printf("network pap://%s/%s/LaserWriter \"Unknown\" \"%s\"\n", encodedZone, encodedName, name);
    }
  }
  return numberFound;
}


/*!
 * @function  printFile
 * @abstract  Open a PAP session and send the data from the input socket to the printer.
 *
 * @param  name		NBP name
 * @param  zone		NBP zone
 * @param  type		NBP type
 * @param  fdin		File descriptor to read data from
 * @param  fdout	File descriptor to write printer responses to
 * @param  fderr	File descriptor to write printer status to
 * @param  copies	# of copies to send (in case in the converter couldn't handle this for us).
 * @param  argc		# of command line arguments.
 *
 * @result A non-zero return value for errors
 */
static int printFile(char* name, char* type, char* zone, int fdin, int fdout, int fderr, int copies, int argc)
{
  int	err;
  int	rc;
  int	val;
  int	len, index;

  char	fileBuffer[4096];    /* File buffer */
  int	fileBufferNbytes;
  off_t	fileTbytes;
  int	fileEOFRead;
  int	fileEOFSent;

  char	sockBuffer[4096 + 1];    /* Socket buffer with room for nul */
  char	atpReqBuf[AT_PAP_DATA_SIZE];
  fd_set readSet;

  at_nbptuple_t	tuple;
  at_inet_t	sendDataAddr;
  at_inet_t	src;
  at_resp_t	resp;
  int		userdata, xo, reqlen;
  u_short	tid;
  u_char	bitmap;
  int		maxfdp1;
  struct timeval timeout, *timeoutPtr;
  u_char	flowQuantum = 1;
  u_short	recvSequence = 0;
  time_t	now,
		connect_time,
		elasped_time, 
		sleep_time,
		connect_timeout = -1,
		nextStatusTime = 0;
  at_entity_t	entity;
  at_retry_t	retry;
  Boolean	recoverableErrShown = false;


#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;  /* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */

  /* try to find our printer */
  if ((err = nbp_make_entity(&entity, name, type, zone)) != noErr)
  {
    fprintf(stderr, "ERROR: Unable to make AppleTalk address: %s\n", strerror(errno));
    goto Exit;
  }

 /*
  * Remember when we started looking for the printer.
  */

  connect_time = time(NULL);

  retry.interval = 1;
  retry.retries  = 5;
  retry.backoff  = 0;

  /* Loop forever trying to get an open session with the printer.  */
  for (;;)
  {
    /* Make sure it's okay to use appletalk */
    if (okayToUseAppleTalk())
    {
      /* Resolve the name into an address. Returns the number found or an error */
      if ((err = nbp_lookup(&entity, &tuple, 1, &retry)) > 0)
      {
        if (err > 1)
          fprintf(stderr, "DEBUG: Found more than one printer with the name \"%s\"\n", name);

	if (recoverableErrShown)
	{
	  fprintf(stderr, "INFO: recovered: \n");
	  sleep(5);
	  recoverableErrShown = false;
	}

        /* Open a connection to the device */
        if ((err = papOpen(&tuple, &gConnID, &gSockfd, &gSessionAddr, &flowQuantum)) == 0)
          break;

        fprintf(stderr, "WARNING: Unable to open \"%s:%s\": %s\n", name, zone, strerror(errno));
      }
      else
      {
	fprintf(stderr, "WARNING: recoverable: Printer not responding\n");
	recoverableErrShown = true;
      }
    }
    else
    {
      fprintf(stderr, "WARNING: recoverable: AppleTalk disabled in System Preferences.\n");
      recoverableErrShown = true;
    }

    retry.retries = 3;
    elasped_time = time(NULL) - connect_time;

    if (connect_timeout == -1)
      connect_timeout = connectTimeout();

    if (connect_timeout && elasped_time > connect_timeout)
    {
      fprintf(stderr, "ERROR: Printer not responding\n");
      err = ETIMEDOUT;
      goto Exit;					 	/* Waiting too long... */
    }
    else if (elasped_time < 30 /*(30 * 60)*/)
      sleep_time = 10;					/* Waiting < 30 minutes */
    else if (elasped_time < 60 /*(24 * 60 * 60)*/)
      sleep_time = 30;					/* Waiting < 24 hours */
    else
      sleep_time = 60;					/* Waiting > 24 hours */

    fprintf(stderr, "DEBUG: sleeping %d seconds...\n", (int)sleep_time);
    sleep(sleep_time);
  }

  /*
  * Now that we are connected to the printer ignore SIGTERM so that we
  * can finish out any page data the driver sends (e.g. to eject the
  * current page...  if we are printing data from a file then catch the
  * signal so we can send a PAP Close packet (otherwise you can't cancel 
  * raw jobs...)
  */

#ifdef HAVE_SIGSET /* Use System V signals over POSIX to avoid bugs */
  sigset(SIGTERM, (argc < 7) ? SIG_IGN : signalHandler);
#elif defined(HAVE_SIGACTION)
  memset(&action, 0, sizeof(action));

  sigemptyset(&action.sa_mask);
  action.sa_handler = (argc < 7) ? SIG_IGN : signalHandler;
  sigaction(SIGTERM, &action, NULL);
#else
  signal(SIGTERM, (argc < 7) ? SIG_IGN : signalHandler);

#ifdef DEBUG
  /* Makes debugging easier; otherwise printer will be busy for several minutes */
  signal(SIGINT, signalHandler);
#endif /* DEBUG */

#endif /* HAVE_SIGSET */

  fprintf(stderr, "INFO: Sending data\n");

  sendDataAddr = tuple.enu_addr;

  /* Start the tickle packets and set a timeout alarm  */
  if ((err = papSendRequest(gSockfd, &gSessionAddr, gConnID, AT_PAP_TYPE_TICKLE, 0, false, false)) < 0)
  {
    perror("ERROR: Unable to send PAP tickle request");
    goto Exit;
  }
  signal(SIGALRM, signalHandler);
  alarm(PAP_TIMEOUT);

  /* Prime the pump with an initial send-data packet */
  if ((err = papSendRequest(gSockfd, &gSessionAddr, gConnID, AT_PAP_TYPE_SEND_DATA, 0xFF, true, true)) < 0)
  {
    perror("ERROR: Unable to send initial PAP send data request");
    goto Exit;
  }

  /* Set non-blocking mode on our data source descriptor */
  val = fcntl(fdin, F_GETFL, 0);
  fcntl(fdin, F_SETFL, val | O_NONBLOCK);

  fileBufferNbytes = 0;
  fileTbytes = 0;
  fileEOFRead = fileEOFSent = false;
  maxfdp1 = MAX(fdin, gSockfd) + 1;

  if (gStatusInterval != 0)
  {
    timeout.tv_usec  = 0;
    nextStatusTime = time(NULL) + gStatusInterval;
    timeoutPtr = &timeout;
  }
  else
    timeoutPtr = NULL;


  for (;;)
  {
    /* Set up our descriptors for the select */
    FD_ZERO(&readSet);
    FD_SET(gSockfd, &readSet);

    if (fileBufferNbytes == 0 && fileEOFRead == false)
      FD_SET(fdin, &readSet);

    /* Set the select timeout value based on the next status interval */
    if (gStatusInterval != 0)
    {
      now = time(NULL);
      timeout.tv_sec = (nextStatusTime > now) ? nextStatusTime - now : 1;
    }

    /* Wait here for something interesting to happen */
    if ((err = select(maxfdp1, &readSet, 0, 0, timeoutPtr)) < 0)
    {
      perror("ERROR: select");
      break;
    }

    if (err == 0 || (gStatusInterval != 0 && time(NULL) >= nextStatusTime))
    {
      /* Time to send a status request */
      if ((err = papSendRequest(gSockfd, &tuple.enu_addr, 0, AT_PAP_TYPE_SEND_STATUS, 0x01, false, false)) < 0)
        perror("WARNING: Unable to send PAP status request");

      if (gStatusInterval)
        nextStatusTime = time(NULL) + gStatusInterval;
    }

    /* Was there an event on the input stream? */
    if (FD_ISSET(fdin, &readSet))
    {
      FD_CLR(fdin, &readSet);

      assert(fileBufferNbytes == 0);
      fileBufferNbytes = read(fdin, fileBuffer, MIN(sizeof(fileBuffer), AT_PAP_DATA_SIZE * flowQuantum));
      if (fileBufferNbytes == 0)
        fileEOFRead = true;

      if (fileEOFSent == false && fileBufferNbytes >= 0 && gSendDataID != 0)
      {
        fprintf(stderr, "DEBUG: -> PAP_DATA %d bytes %s\n", fileBufferNbytes, fileEOFRead ? "with EOF" : "");
        papWrite(gSockfd, &sendDataAddr, gSendDataID, gConnID, flowQuantum, fileBuffer, fileBufferNbytes, fileEOFRead);

        fileTbytes += fileBufferNbytes;
        if (argc > 6 && !gErrorlogged)
          fprintf(stderr, "DEBUG: Sending print file, %qd bytes\n", (off_t)fileTbytes);

        fileBufferNbytes = 0;
        gSendDataID = 0;
        if (fileEOFRead)
        {
          fileEOFSent = true;
          if (gWaitEOF == false || fileTbytes == 0)
          {
            err = 0;
            goto Exit;
          }
        }
      }
    }

    /* Was there an event on the output stream? */
    if (FD_ISSET(gSockfd, &readSet))
    {
      if ((rc = atp_look(gSockfd)) < 0)
      {
        perror("ERROR: Unable to look for PAP response");
        break;
      }
  
      if (rc > 0)
      {
        /* It's an ATP response */
        resp.resp[0].iov_base = sockBuffer;
        resp.resp[0].iov_len = sizeof(sockBuffer) - 1;
        resp.bitmap = 0x01;
  
        if ((err = atp_getresp(gSockfd, &tid, &resp)) < 0)
        {
          perror("ERROR: Unable to get PAP response");
          break;
        }
        userdata = resp.userdata[0];
      }
      else
      {
        /* It's an ATP request */
        reqlen = sizeof(atpReqBuf);
        if ((err = atp_getreq(gSockfd, &src, atpReqBuf, &reqlen, &userdata, &xo, &tid, &bitmap, 0)) < 0)
        {
          perror("ERROR: Unable to get PAP request");
          break;
        }
      }

      fprintf(stderr, "DEBUG: <- %s\n", PAPPacketStr(TYPE_OF(userdata)));

      switch (TYPE_OF(userdata))
      {
      case AT_PAP_TYPE_SEND_STS_REPLY:        /* Send-Status-Reply packet */
        if (resp.bitmap & 1)
        {
          char *iov_base = (char *)resp.resp[0].iov_base;
          statusUpdate(&iov_base[5], iov_base[4]);
        }
        break;
      
      case AT_PAP_TYPE_SEND_DATA:            /* Send-Data packet */
        sendDataAddr.socket  = src.socket;
        gSendDataID     = tid;
        recvSequence    = OSReadBigInt16(&SEQUENCE_NUM(userdata), 0);

        if ((fileBufferNbytes > 0 || fileEOFRead) && fileEOFSent == false)
        {
          fprintf(stderr, "DEBUG: -> PAP_DATA %d bytes %s\n", fileBufferNbytes, fileEOFRead ? "with EOF" : "");
          papWrite(gSockfd, &sendDataAddr, gSendDataID, gConnID, flowQuantum, fileBuffer, fileBufferNbytes, fileEOFRead);

          fileTbytes += fileBufferNbytes;
          if (argc > 6 && !gErrorlogged)
            fprintf(stderr, "DEBUG: Sending print file, %qd bytes\n", (off_t)fileTbytes);

          fileBufferNbytes = 0;
          gSendDataID = 0;
          if (fileEOFRead)
          {
            fileEOFSent = true;
            if (gWaitEOF == false)
            {
              err = 0;
              goto Exit;
            }
          }
        }
        break;
    
      case AT_PAP_TYPE_DATA:              /* Data packet */
        for (len=0, index=0; index < ATP_TRESP_MAX; index++)
        {
          if (resp.bitmap & (1 << index))
            len += resp.resp[index].iov_len;
        }

        fprintf(stderr, "DEBUG: <- PAP_DATA %d bytes %s\n", len, IS_PAP_EOF(userdata) ? "with EOF" : "");

        if (len > 0)
        {
          char *pLineBegin, *pCommentEnd, *pChar;
          char *logLevel;
          char logstr[512];
          int  logstrlen;
          
          write(fdout, sockBuffer, len);
          
          sockBuffer[len] = '\0';     /* We always reserve room for the nul so we can use strstr() below*/
          pLineBegin = sockBuffer;
                                        
          /* If there are PostScript status comments in the buffer log them.
           *
           *  This logic shouldn't be in the backend but until we get backchannel
           *  data in CUPS 1.2 it has to live here.
           */
          while (pLineBegin < sockBuffer + len &&
               (pLineBegin = strstr(pLineBegin,    "%%[")) != NULL &&
               (pCommentEnd   = strstr(pLineBegin, "]%%")) != NULL)
          {
            pCommentEnd += 3;            /* Skip past "]%%" */
            *pCommentEnd = '\0';         /* There's always room for the nul */
            
            /* Strip the CRs & LFs before writing it to stderr */
            for (pChar = pLineBegin; pChar < pCommentEnd; pChar++)
              if (*pChar == '\r' || *pChar == '\n')
                *pChar = ' ';
                                                
            if (strncasecmp(pLineBegin, "%%[ Error:", 10) == 0)
            {
              /* logLevel should be "ERROR" here but this causes PrintCenter
              *  to pause the queue which in turn clears this error, which 
              *  restarts the job. So the job ends up in an infinite loop with
              *  the queue being held/un-held. Just make it DEBUG for now until
              *  we fix notifications later.
              */
              logLevel = "DEBUG";
              gErrorlogged = true;
            }
            else if (strncasecmp(pLineBegin, "%%[ Flushing", 12) == 0)
              logLevel = "DEBUG";
            else
              logLevel = "INFO";
            
            if ((logstrlen = snprintf(logstr, sizeof(logstr), "%s: %s\n", logLevel, pLineBegin)) >= sizeof(logstr))
            {
              /* If the string was trucnated make sure it has a linefeed before the nul */
              logstrlen = sizeof(logstr) - 1;
              logstr[logstrlen - 1] = '\n';
            }

            write(fderr, logstr, logstrlen);

            pLineBegin = pCommentEnd + 1;
          }
        }

        if (IS_PAP_EOF(userdata) != 0)
        {
          /* If this is EOF then were we expecting it? */
          if (fileEOFSent == true)
            goto Exit;
          else
          {
            fprintf(stderr, "WARNING: Printer sent unexpected EOF\n");
          }
        }

        if ((err = papSendRequest(gSockfd, &gSessionAddr, gConnID, AT_PAP_TYPE_SEND_DATA, 0xFF, true, true)) < 0)
        {
          fprintf(stderr, "ERROR: Error %d sending PAPSendData resuest: %s\n", err, strerror(errno));
          goto Exit;
        }
        break;
      
      case AT_PAP_TYPE_TICKLE:            /* Tickle packet */
        break;
    
      case AT_PAP_TYPE_CLOSE_CONN:          /* Close-Connection packet */
        /* We shouldn't normally see this. */
        papCloseResp(gSockfd, &gSessionAddr, xo, tid, gConnID);

        /* If this is EOF then were we expecting it? */
        if (fileEOFSent == true)
        {
          fprintf(stderr, "WARNING: Printer sent unexpected EOF\n");
        }
        else
        {
          fprintf(stderr, "ERROR: Printer sent unexpected EOF\n");
        }
        goto Exit;
        break;
    
      case AT_PAP_TYPE_OPEN_CONN:            /* Open-Connection packet */
      case AT_PAP_TYPE_OPEN_CONN_REPLY:        /* Open-Connection-Reply packet */
      case AT_PAP_TYPE_SEND_STATUS:          /* Send-Status packet */
      case AT_PAP_TYPE_CLOSE_CONN_REPLY:        /* Close-Connection-Reply packet */
        fprintf(stderr, "WARNING: Unexpected PAP packet of type %d\n", TYPE_OF(userdata));
        break;
      
      default:
        fprintf(stderr, "WARNING: Unknown PAP packet of type %d\n", TYPE_OF(userdata));
        break;
      }
    
      if (CONNID_OF(userdata) == gConnID)
      {
        /* Reset tickle timer */
        alarm(0);
        alarm(PAP_TIMEOUT);
      }
    }
  }

Exit:
  /*
  * Close the socket and return...
  */
  papClose(false);

  return err;
}


#pragma mark -
/*!
 * @function  papOpen
 * @abstract  Open a pap session to a printer.
 *
 * @param  tuple	nbp address of printer
 * @param  connID	returned pap connection id
 * @param  fd		returned socket descriptor
 * @param  sessionAddr	returned session address
 * @param  flowQuantum	returned flow quantum (usually 8)
 *
 * @result    A non-zero return value for errors
 */
static int papOpen(at_nbptuple_t* tuple, u_char* connID, int* fd, at_inet_t* sessionAddr, u_char* flowQuantum)
{
  int		result,
		openResult;
  long		tm;
  char		data[10], rdata[ATP_DATA_SIZE];
  int		userdata;
  u_char	*puserdata = (u_char *)&userdata;
  at_socket	socket = 0;
  u_short	waitTime;
  int		status;
  at_resp_t	resp;
  at_retry_t	retry;

  if (tuple == NULL)
  {
    errno = EINVAL;
    return -1;
  }

  fprintf(stderr, "INFO: Opening connection\n");

  errno  = 0;
  result  = 0;

  *fd = atp_open(&socket);
  if (*fd < 0)
    return -1;

  /* Build the open connection request packet.
  */
  tm = time(NULL);
  srand(tm);

  *connID = (rand()&0xff) | 0x01;
  puserdata[0] = *connID;
  puserdata[1] = AT_PAP_TYPE_OPEN_CONN;
  puserdata[2] = 0;
  puserdata[3] = 0;

  retry.interval = 2;
  retry.retries = 5;

  resp.bitmap = 0x01;
  resp.resp[0].iov_base = rdata;
  resp.resp[0].iov_len = sizeof(rdata);

  data[0] = socket;
  data[1] = 8;

  for (;;)
  {
    waitTime = (u_short)(time(NULL) - tm);
    OSWriteBigInt16(&data[2], 0, waitTime);

    fprintf(stderr, "DEBUG: -> %s\n", PAPPacketStr(AT_PAP_TYPE_OPEN_CONN));

    status = atp_sendreq(*fd, &tuple->enu_addr, data, 4, userdata, 1, 0, 0, &resp, &retry, 0);

    if (status < 0)
    {
      statusUpdate("Destination unreachable", 23);
      result = EHOSTUNREACH;
      errno = EHOSTUNREACH;
      sleep(1);
      goto Exit;
    }
    else
    {
      puserdata = (u_char *)&resp.userdata[0];
      openResult = OSReadBigInt16(&rdata[2], 0);

      fprintf(stderr, "DEBUG: <- %s, status %d\n", PAPPacketStr(puserdata[1]), openResult);

      /* Just for the sake of our sanity check the other fields in the packet
      */
      if (puserdata[1] != AT_PAP_TYPE_OPEN_CONN_REPLY ||
        (openResult == 0 && (puserdata[0] & 0xff) != *connID))
      {
	result = EINVAL;
	errno = EINVAL;
	goto Exit;
      }
  
      statusUpdate(&rdata[5], rdata[4] & 0xff);

      if (openResult == 0)
	break;        /* Connection established okay, exit from the loop */
    }

    sleep(1);
  }

  /* Update the session address
  */
  sessionAddr->net  = tuple->enu_addr.net;
  sessionAddr->node  = tuple->enu_addr.node;
  sessionAddr->socket  = rdata[0];
  *flowQuantum    = rdata[1];

Exit:
  if (result != 0)
  {
    atp_close(*fd);
    *fd = 0;
  }

  return result;
}


/*!
 * @function  papClose
 * @abstract  End a PAP session by canceling outstanding send-data & tickle 
 *            transactions and sending a PAP close request.
 *
 * @param  abort  If we're aborting then send the close request 
 *		  with 0 retries (not yet implemented)
 *
 * @result  A non-zero return value for errors
 */
static int papClose(int abort)
{
  int		fd;
  u_short	tmpID;
  int		result;
  unsigned char	rdata[ATP_DATA_SIZE];
  int		userdata;
  u_char	*puserdata = (u_char *)&userdata;
  at_resp_t	resp;
  at_retry_t	retry;

  if (gSockfd != 0)
  {
    fd = gSockfd;
    gSockfd = 0;

    alarm(0);
  
    /* Cancel the pending send-data and tickle trnsactions
    */
    if (gSendDataID)
    {
      tmpID = gSendDataID;
      gSendDataID = 0;
      papCancelRequest(fd, tmpID);
    }
  
    if (gTickleID)
    {
      tmpID = gTickleID;
      gTickleID = 0;
      papCancelRequest(fd, tmpID);
    }

    /* This is a workaround for bug #2735145. The problem is papWrite()
    *  returns before the ATP TRel arrives for it. If we send the pap close packet
    *  before this release then the printer can drop the last data packets. 
    *  The effect on an Epson printer is the last page doesn't print, on HP it 
    *  doesn't close the pap session.
    */
    if (gWaitEOF == false)
      sleep(2);

    fprintf(stderr, "DEBUG: -> %s\n", PAPPacketStr(AT_PAP_TYPE_CLOSE_CONN));
  
    puserdata[0] = gConnID;
    puserdata[1] = AT_PAP_TYPE_CLOSE_CONN;
    puserdata[2] = 0;
    puserdata[3] = 0;
  
    retry.interval = 2;
    retry.retries = 5;
  
    resp.bitmap = 0x01;
    resp.resp[0].iov_base = rdata;
    resp.resp[0].iov_len = sizeof(rdata);
  
    result = atp_sendreq(fd, &gSessionAddr, 0, 0, userdata, 1, 0, 0, &resp, &retry, 0);
  
    result = close(fd);
  }
  return noErr;
}


/*!
 * @function  papWrite
 * @abstract  Write bytes to a printer.
 *
 * @param  sockfd	socket descriptor
 * @param  dest		destination address
 * @param  tid		transaction id
 * @param  connID	connection id
 * @param  flowQuantum	returned flow quantum (usually 8)
 * @param  data		pointer to the data
 * @param  len		number of bytes to send
 * @param  eof		pap eof flag
 *
 * @result  A non-zero return value for errors
 */
static int papWrite(int sockfd, at_inet_t* dest, u_short tid, u_char connID, u_char flowQuantum, char* data, int len, int eof)
{
  int		result;
  int		index;
  u_char*	puserdata;
  at_resp_t	resp;

  /* fprintf(stderr, "DEBUG: papWrite(%d%s) to %d,%d,%d; %d\n", len, eof ? " EOF":"", dest->net, dest->node, dest->socket, connID); */

  if (len > AT_PAP_DATA_SIZE * flowQuantum)
  {
    fprintf(stderr, "DEBUG: papWrite() len of %d is too big!\n", len);
    errno = E2BIG;
    return -1;
  }

  /*
  * Break up the outgoing data into a set of
  * response packets to reply to an incoming
  * PAP 'SENDDATA' request
  */
  for (index = 0; index < flowQuantum; index++)
  {
    resp.userdata[index] = 0;
    puserdata = (u_char *)&resp.userdata[index];

    puserdata[PAP_CONNID]  = connID;
    puserdata[PAP_TYPE]    = AT_PAP_TYPE_DATA;
    puserdata[PAP_EOF]    = eof ? 1 : 0;

    resp.resp[index].iov_base = (caddr_t)data;

    if (data)
      data += AT_PAP_DATA_SIZE;

    resp.resp[index].iov_len = MIN((int)len, (int)AT_PAP_DATA_SIZE);
    len -= resp.resp[index].iov_len;
    if (len == 0)
      break;
  }
  resp.bitmap = (1 << (index + 1)) - 1;

  /*
  *  Write out the data as a PAP 'DATA' response
  */
  errno = 0;
  if ((result = atp_sendrsp(sockfd, dest, true, tid, &resp)) < 0)
  {
    fprintf(stderr, "DEBUG: atp_sendrsp() returns %d, errno %d \"%s\"\n", result, errno, strerror(errno));
    return -1;
  }
  return(0);
}


/*!
 * @function  papCloseResp
 * @abstract  Send a pap close response in the rare case we receive a close connection request.
 *
 * @param  sockfd	socket descriptor
 * @param  dest		destination address
 * @param  tid		transaction id
 * @param  connID	connection id
 *
 * @result    A non-zero return value for errors
 */
static int papCloseResp(int sockfd, at_inet_t* dest, int xo, u_short tid, u_char connID)
{
  int		result;
  at_resp_t	resp;

  resp.bitmap = 1;
  resp.userdata[0] = 0;

  ((u_char*)&resp.userdata[0])[PAP_CONNID]  = connID;
  ((u_char*)&resp.userdata[0])[PAP_TYPE]    = AT_PAP_TYPE_CLOSE_CONN_REPLY;

  resp.resp[0].iov_base = NULL;
  resp.resp[0].iov_len = 0;

  if ((result = atp_sendrsp(sockfd, dest, xo, tid, &resp)) < 0)
  {
    fprintf(stderr, "DEBUG: atp_sendrsp() returns %d, errno %d \"%s\"\n", result, errno, strerror(errno));
    return -1;
  }
  return 0;
}


/*!
 * @function  papSendRequest
 * @abstract  Send a pap close response in the rare case we receive a close connection request.
 *
 * @param  sockfd	socket descriptor
 * @param  dest		destination address
 * @param  function	pap function
 * @param  bitmap	bitmap
 * @param  xo		exactly once
 * @param  seqno	sequence number
 *
 * @result  A non-zero return value for errors
 */
static int papSendRequest(int sockfd, at_inet_t* dest, u_char connID, int function, u_char bitmap, int xo, int seqno)
{
  u_short	tid;
  int		err;
  sigset_t	sv, osv;
  int		userdata;
  u_char	*puserdata = (u_char *)&userdata;
  at_retry_t	retry;
  at_resp_t	resp;
  static u_short pap_send_count = 0;

  fprintf(stderr, "DEBUG: -> %s\n", PAPPacketStr(function));

  puserdata[0] = connID;
  puserdata[1] = function;
  resp.bitmap = bitmap;
  retry.interval = 10;
  retry.retries = -1; /* was ATP_INFINITE_RETRIES */
  if (seqno)
  {
    pap_send_count++;
    if (pap_send_count == 0)
      pap_send_count = 1;

    OSWriteBigInt16(&puserdata[2], 0, pap_send_count);
  }
  else
    OSWriteBigInt16(&puserdata[2], 0, 0);

  sigemptyset(&sv);
  sigaddset(&sv, SIGIO);
  sigprocmask(SIG_SETMASK, &sv, &osv);

  err = atp_sendreq(sockfd, dest, 0, 0, userdata, xo, 0, &tid, &resp, &retry, 1);

  sigprocmask(SIG_SETMASK, &osv, NULL);

  return err;
}


/*!
 * @function  papCancelRequest
 * @abstract  Cancel a pending pap request.
 *
 * @param  sockfd	socket descriptor
 * @param  tid		transaction ID
 *
 * @result    A non-zero return value for errors
 */
int papCancelRequest(int sockfd, u_short tid)
{
  sigset_t	sv, osv;

  sigemptyset(&sv);
  sigaddset(&sv, SIGIO);
  sigprocmask(SIG_SETMASK, &sv, &osv);

  if (atp_abort(sockfd, NULL, tid) < 0)
  {
    sigprocmask(SIG_SETMASK, &osv, NULL);
    return -1;
  }
  sigprocmask(SIG_SETMASK, &osv, NULL);

  return 0;
}


#pragma mark -
/*!
 * @function  statusUpdate
 * @abstract  Format and print a PAP status response to stderr.
 *
 * @param  status	The status response string
 * @param  statusLen	The length of the status response string
 */
void statusUpdate(char* status, u_char statusLen)
{
  static char	status_str[255];
  static u_char	last_statusLen  = 0xFF;

  /* Only send this if the status has changed */
  if (statusLen != last_statusLen || memcmp(status, status_str, statusLen) != 0)
  {
    if (statusLen > sizeof(status_str)-1)
      statusLen = sizeof(status_str)-1;
    last_statusLen = statusLen;
    memcpy(status_str, status, statusLen);
    status_str[(int)statusLen] = '\0';
    
    /* 
     * Make sure the status string is in the form of a PostScript comment.
     */

    if (statusLen > 3 && memcmp(status, "%%[", 3) == 0)
      fprintf(stderr, "INFO: %s\n", status_str);
    else
      fprintf(stderr, "INFO: %%%%[ %s ]%%%%\n", status_str);
  }
  return;
}


/*!
 * @function  parseUri
 * @abstract  Parse a PAP URI into it's NBP components.
 *
 * @param  argv0	The PAP URI to parse
 * @param  name		NBP name
 * @param  zone		NBP zone
 * @param  type		NBP type
 *
 * @result    A non-zero return value for errors
 */
static int parseUri(const char* argv0, char* name, char* type, char* zone)
{
  char  method[255],		/* Method in URI */
        hostname[1024],		/* Hostname */
        username[255],		/* Username info (not used) */
        resource[1024],		/* Resource info (device and options) */
        *resourcePtr,
        *typePtr,
        *options,		/* Pointer to options */
        optionName[255],	/* Name of option */
        value[255],		/* Value of option */
        *ptr;			/* Pointer into name or value */
  int   port;			/* Port number (not used) */
  int   statusInterval;		/* */

  /*
  * Extract the device name and options from the URI...
  */
  method[0] = username[0] = hostname[0] = resource[0] = '\0';
  port = 0;

  httpSeparateApple(argv0, method, sizeof(method), 
			   username, sizeof(username),
			   hostname, sizeof(hostname), &port,
			   resource, sizeof(resource), 0);

  /*
  * See if there are any options...
  */
  if ((options = strchr(resource, '?')) != NULL)
  {
    /*
    * Yup, terminate the device name string and move to the first
    * character of the options...
    */
    *options++ = '\0';

    while (*options != '\0')
    {
      /*
      * Get the name...
      */
      for (ptr = optionName; *options && *options != '=' && *options != '+'; )
        *ptr++ = *options++;

      *ptr = '\0';
      value[0] = '\0';

      if (*options == '=')
      {
        /*
        * Get the value...
        */
        
        options ++;
        
        for (ptr = value; *options && *options != '+';)
          *ptr++ = *options++;

        *ptr = '\0';
        
        if (*options == '+')
          options ++;
      }
      else if (*options == '+')
      {
        options ++;
      }

      /*
      * Process the option...
      */
      if (strcasecmp(optionName, "waiteof") == 0)
      {
        /*
        * Set the banner...
        */
        if (strcasecmp(value, "on") == 0 ||
          strcasecmp(value, "yes") == 0 ||
          strcasecmp(value, "true") == 0)
        {
          gWaitEOF = true;
        }
        else if (strcasecmp(value, "off") == 0 ||
            strcasecmp(value, "no") == 0 ||
            strcasecmp(value, "false") == 0)
        {
          gWaitEOF = false;
        }
        else
        {
          fprintf(stderr, "WARNING: Boolean expected for waiteof option \"%s\"\n", value);
        }
      }
      else if (strcasecmp(optionName, "status") == 0)
      {
        statusInterval = atoi(value);
        if (value[0] < '0' || value[0] > '9' || 
          statusInterval < 0)
        {
          fprintf(stderr, "WARNING: number expected for status option \"%s\"\n", value);
        }
        else
        {
          gStatusInterval = statusInterval;
        }
      }
    }
  }

  resourcePtr = resource;

  if (*resourcePtr == '/')
    resourcePtr++;

        /* If the resource has a slash we assume the slash seperates the AppleTalk object
         * name from the AppleTalk type. If the slash is not present we assume the AppleTalk
         * type is LaserWriter.
         */
        typePtr = strchr(resourcePtr, '/');
        if (typePtr != NULL) {
            *typePtr++ = '\0';
        } else {
            typePtr = "LaserWriter";
        }

  removePercentEscapes(hostname,    zone, NBP_NVE_STR_SIZE + 1);
  removePercentEscapes(resourcePtr,  name, NBP_NVE_STR_SIZE + 1);
  removePercentEscapes(typePtr,     type, NBP_NVE_STR_SIZE + 1);

  return 0;
}


/*!
 * @function  addPercentEscapes
 * @abstract  Encode a string with percent escapes
 *
 * @param  src		The source C string
 * @param  dst		Desination buffer
 * @param  dstMax	Size of desination buffer
 *
 * @result    A non-zero return value for errors
 */
static int addPercentEscapes(const char* src, char* dst, int dstMax)
{
  char	c;
  char	*dstEnd = dst + dstMax - 1;	/* -1 to leave room for the NUL */

  while (*src)
  {
    c = *src++;

    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || 
        (c >= '0' && c <= '9') || (c == '.' || c == '-'  || c == '*' || c == '_'))
    {
      if (dst >= dstEnd)
        return -1;

      *dst++ = c;
    }
    else
    {
      if (dst >= dstEnd - 2)
        return -1;

      snprintf(dst, dstEnd - dst, "%%%02x", c);
      dst += 3;
    }
  }

  *dst = '\0';
  return 0;
}


/*!
 * @function  removePercentEscapes
 * @abstract  Returns a string with any percent escape sequences replaced with their equivalent character
 *
 * @param  src		Source buffer
 * @param  srclen	Number of bytes in source buffer
 * @param  dst		Desination buffer
 * @param  dstMax	Size of desination buffer
 *
 * @result    A non-zero return value for errors
 */
static int removePercentEscapes(const char* src, char* dst, int dstMax)
{
  int c;
  const char *dstEnd = dst + dstMax;

  while (*src && dst < dstEnd)
  {
    c = *src++;

    if (c == '%')
    {
      sscanf(src, "%02x", &c);
      src += 2;
    }
    *dst++ = (char)c;
  }

  if (dst >= dstEnd)
    return -1;

  *dst = '\0';
  return 0;
}


/*!
 * @function  nbptuple_compare
 * @abstract  An NBP comparator for qsort.
 *
 * @result    p1<p2: -1, p1=p2: 0, p1>p2: 1
 */
int nbptuple_compare(const void *p1, const void *p2)
{
  int result;
  int len = MIN(((at_nbptuple_t*)p1)->enu_entity.object.len, 
          ((at_nbptuple_t*)p2)->enu_entity.object.len);

  if ((result = memcmp(((at_nbptuple_t*)p1)->enu_entity.object.str, ((at_nbptuple_t*)p2)->enu_entity.object.str, len)) == 0)
  {
    if (((at_nbptuple_t*)p1)->enu_entity.object.len < ((at_nbptuple_t*)p2)->enu_entity.object.len)
      result = -1;
    else if (((at_nbptuple_t*)p1)->enu_entity.object.len > ((at_nbptuple_t*)p2)->enu_entity.object.len)
      result = 1;
    else
      result = 0;
  }
  return result;
}


/*!
 * @function  okayToUseAppleTalk
 * @abstract  Returns true if AppleTalk is available and enabled.
 *
 * @result    non-zero if AppleTalk is enabled
 */
static int okayToUseAppleTalk()
{
  int atStatus = checkATStack();
  
  /* I think the test should be:
   *    return atStatus == RUNNING || atStatus == LOADED;
   * but when I disable AppleTalk from the network control panel and
   * reboot, AppleTalk shows up as loaded. The test empirically becomes
   * the following:
   */
  return atStatus == RUNNING;
}


/*!
 * @function  connectTimeout
 * @abstract  Returns the connect timeout preference value.
 */
static int connectTimeout()
{
  CFPropertyListRef value;
  SInt32 connect_timeout = (7 * 24 * 60 * 60);	/* Default timeout is one week... */

  value = CFPreferencesCopyValue(CFSTR("timeout"), CFSTR("com.apple.print.backends"),
				 kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
  if (value != NULL)
  {
    if (CFGetTypeID(value) == CFNumberGetTypeID())
      CFNumberGetValue(value, kCFNumberSInt32Type, &connect_timeout);

    CFRelease(value);
  }

  return connect_timeout;
}


/*!
 * @function  signalHandler
 * @abstract  A signal handler so we can clean up the pap session before exiting.
 *
 * @param  sigraised	The signal raised
 *
 * @result    Never returns
 */
static void signalHandler(int sigraised)
{
  fprintf(stderr, "ERROR: There was a timeout error while sending data to the printer\n");

  papClose(true);

  _exit(1);
}
