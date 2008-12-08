/*
 * "$Id: scsi-irix.c 6834 2007-08-22 18:29:25Z mike $"
 *
 *   IRIX SCSI printer support for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007 by Apple Inc.
 *   Copyright 2003-2005 by Easy Software Products, all rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or
 *   without modification, are permitted provided that the
 *   following conditions are met:
 *
 *     1. Redistributions of source code must retain the above
 *	  copyright notice, this list of conditions and the
 *	  following disclaimer.
 *
 *     2. Redistributions in binary form must reproduce the
 *	  above copyright notice, this list of conditions and
 *	  the following disclaimer in the documentation and/or
 *	  other materials provided with the distribution.
 *
 *     3. All advertising materials mentioning features or use
 *	  of this software must display the following
 *	  acknowledgement:
 *
 *	    This product includes software developed by Easy
 *	    Software Products.
 *
 *     4. The name of Easy Software Products may not be used to
 *	  endorse or promote products derived from this software
 *	  without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS
 *   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
 *   BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *   MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *   DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS
 *   BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 *   OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 *   DAMAGE.
 *
 * Contents:
 *
 *   list_devices() - List the available SCSI printer devices.
 *   print_device() - Print a file to a SCSI device.
 */

/*
 * Include necessary headers.
 */

#include <bstring.h>		/* memcpy() and friends */
#include <sys/dsreq.h>		/* SCSI interface stuff */


/*
 * 'list_devices()' - List the available SCSI printer devices.
 */

void
list_devices(void)
{
  printf("direct scsi \"Unknown\" \"%s\"\n",
         _cupsLangString(cupsLangDefault(), _("SCSI Printer")));
}


/*
 * 'print_device()' - Print a file to a SCSI device.
 */

int					/* O - Print status */
print_device(const char *resource,	/* I - SCSI device */
             int        fd,		/* I - File to print */
	     int        copies)		/* I - Number of copies to print */
{
  int		scsi_fd;		/* SCSI file descriptor */
  char		buffer[8192];		/* Data buffer */
  int		bytes;			/* Number of bytes */
  int		try;			/* Current try */
  dsreq_t	scsi_req;		/* SCSI request */
  char		scsi_cmd[6];		/* SCSI command data */
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;		/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */


 /*
  * Make sure we have a valid resource name...
  */

  if (strncmp(resource, "/dev/scsi/", 10) != 0)
  {
    _cupsLangPrintf(stderr, _("ERROR: Bad SCSI device file \"%s\"!\n"),
                    resource);
    return (CUPS_BACKEND_STOP);
  }

 /*
  * Open the SCSI device file...
  */

  fputs("STATE: +connecting-to-device\n", stderr);

  do
  {
    if ((scsi_fd = open(resource, O_RDWR | O_EXCL)) == -1)
    {
      if (getenv("CLASS") != NULL)
      {
       /*
        * If the CLASS environment variable is set, the job was submitted
	* to a class and not to a specific queue.  In this case, we want
	* to abort immediately so that the job can be requeued on the next
	* available printer in the class.
	*/

        _cupsLangPuts(stderr,
	              _("INFO: Unable to contact printer, queuing on next "
			"printer in class...\n"));

       /*
        * Sleep 5 seconds to keep the job from requeuing too rapidly...
	*/

	sleep(5);

        return (1);
      }

      if (errno != EAGAIN && errno != EBUSY)
      {
	_cupsLangPrintf(stderr,
	                _("ERROR: Unable to open device file \"%s\": %s\n"),
			resource, strerror(errno));
	return (CUPS_BACKEND_FAILED);
      }
      else
      {
        _cupsLangPuts(stderr,
	              _("INFO: Printer busy; will retry in 30 seconds...\n"));
        sleep(30);
      }
    }
  }
  while (scsi_fd == -1);

  fputs("STATE: -connecting-to-device\n", stderr);

 /*
  * Now that we are "connected" to the port, ignore SIGTERM so that we
  * can finish out any page data the driver sends (e.g. to eject the
  * current page...  Only ignore SIGTERM if we are printing data from
  * stdin (otherwise you can't cancel raw jobs...)
  */

  if (fd != 0)
  {
#ifdef HAVE_SIGSET /* Use System V signals over POSIX to avoid bugs */
    sigset(SIGTERM, SIG_IGN);
#elif defined(HAVE_SIGACTION)
    memset(&action, 0, sizeof(action));

    sigemptyset(&action.sa_mask);
    action.sa_handler = SIG_IGN;
    sigaction(SIGTERM, &action, NULL);
#else
    signal(SIGTERM, SIG_IGN);
#endif /* HAVE_SIGSET */
  }

 /*
  * Copy the print file to the device...
  */

  while (copies > 0)
  {
    if (fd != 0)
      lseek(fd, 0, SEEK_SET);

    while ((bytes = read(fd, buffer, sizeof(buffer))) > 0)
    {
      memset(&scsi_req, 0, sizeof(scsi_req));

      scsi_req.ds_flags   = DSRQ_WRITE;
      scsi_req.ds_time    = 60 * 1000;
      scsi_req.ds_cmdbuf  = scsi_cmd;
      scsi_req.ds_cmdlen  = 6;
      scsi_req.ds_databuf = buffer;
      scsi_req.ds_datalen = bytes;

      scsi_cmd[0] = 0x0a;	/* Group 0 print command */
      scsi_cmd[1] = 0x00;
      scsi_cmd[2] = bytes / 65536;
      scsi_cmd[3] = bytes / 256;
      scsi_cmd[4] = bytes;
      scsi_cmd[5] = 0x00;

      for (try = 0; try < 10; try ++)
	if (ioctl(scsi_fd, DS_ENTER, &scsi_req) < 0 ||
            scsi_req.ds_status != 0)
        {
	  _cupsLangPrintf(stderr,
			  _("WARNING: SCSI command timed out (%d); "
			    "retrying...\n"), scsi_req.ds_status);
          sleep(try + 1);
	}
	else
          break;

      if (try >= 10)
      {
	_cupsLangPrintf(stderr, _("ERROR: Unable to send print data (%d)\n"),
			scsi_req.ds_status);
        close(scsi_fd);
	return (CUPS_BACKEND_FAILED);
      }
    }

    copies --;
  }

 /*
  * Close the device and return...
  */

  close(fd);

  return (CUPS_BACKEND_OK);
}


/*
 * End of "$Id: scsi-irix.c 6834 2007-08-22 18:29:25Z mike $".
 */
