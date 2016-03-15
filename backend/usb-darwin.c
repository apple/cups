/*
* "$Id: usb-darwin.c 11670 2014-03-04 14:53:59Z msweet $"
*
* Copyright 2005-2014 Apple Inc. All rights reserved.
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
* license, under Apple's copyrights in this original Apple software (the
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
*/

/*
 * Include necessary headers.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <libgen.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach/mach_time.h>
#include <cups/debug-private.h>
#include <cups/file-private.h>
#include <cups/sidechannel.h>
#include <cups/language-private.h>
#include "backend-private.h"
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <libproc.h>
#include <asl.h>
#include <spawn.h>
#include <pthread.h>

extern char **environ;


/*
 * DEBUG_WRITES, if defined, causes the backend to write data to the printer in
 * 512 byte increments, up to 8192 bytes, to make debugging with a USB bus
 * analyzer easier.
 */

#define DEBUG_WRITES 0

/*
 * WAIT_EOF_DELAY is number of seconds we'll wait for responses from
 * the printer after we've finished sending all the data
 */
#define WAIT_EOF_DELAY			7
#define WAIT_SIDE_DELAY			3
#define DEFAULT_TIMEOUT			5000L

#define	USB_INTERFACE_KIND		CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID190)
#define kUSBLanguageEnglish		0x409

#define PRINTER_POLLING_INTERVAL	5			/* seconds */
#define INITIAL_LOG_INTERVAL		PRINTER_POLLING_INTERVAL
#define SUBSEQUENT_LOG_INTERVAL		3 * INITIAL_LOG_INTERVAL

#define kUSBPrinterClassTypeID		CFUUIDGetConstantUUIDWithBytes(NULL, 0x06, 0x04, 0x7D, 0x16, 0x53, 0xA2, 0x11, 0xD6, 0x92, 0x06, 0x00, 0x30, 0x65, 0x52, 0x45, 0x92)
#define	kUSBPrinterClassInterfaceID	CFUUIDGetConstantUUIDWithBytes(NULL, 0x03, 0x34, 0x6D, 0x74, 0x53, 0xA3, 0x11, 0xD6, 0x9E, 0xA1, 0x76, 0x30, 0x65, 0x52, 0x45, 0x92)

#define kUSBClassDriverProperty		CFSTR("USB Printing Class")

#define kUSBGenericTOPrinterClassDriver	CFSTR("/System/Library/Printers/Libraries/USBGenericPrintingClass.plugin")
#define kUSBPrinterClassDeviceNotOpen	-9664	/*kPMInvalidIOMContext*/

#define CRSetCrashLogMessage(m) _crc_make_setter(message, m)
#define _crc_make_setter(attr, arg) (gCRAnnotations.attr = (uint64_t)(unsigned long)(arg))
#define CRASH_REPORTER_CLIENT_HIDDEN __attribute__((visibility("hidden")))
#define CRASHREPORTER_ANNOTATIONS_VERSION 4
#define CRASHREPORTER_ANNOTATIONS_SECTION "__crash_info"

struct crashreporter_annotations_t {
	uint64_t version;		// unsigned long
	uint64_t message;		// char *
	uint64_t signature_string;	// char *
	uint64_t backtrace;		// char *
	uint64_t message2;		// char *
	uint64_t thread;		// uint64_t
	uint64_t dialog_mode;		// unsigned int
};

CRASH_REPORTER_CLIENT_HIDDEN
struct crashreporter_annotations_t gCRAnnotations
	__attribute__((section("__DATA," CRASHREPORTER_ANNOTATIONS_SECTION)))
	= { CRASHREPORTER_ANNOTATIONS_VERSION, 0, 0, 0, 0, 0, 0 };

/*
 * Section 5.3 USB Printing Class spec
 */
#define kUSBPrintingSubclass			1
#define kUSBPrintingProtocolNoOpen		0
#define kUSBPrintingProtocolUnidirectional	1
#define kUSBPrintingProtocolBidirectional	2

typedef IOUSBInterfaceInterface190	**printer_interface_t;

typedef struct iodevice_request_s	/**** Device request ****/
{
  UInt8		requestType;
  UInt8		request;
  UInt16	value;
  UInt16	index;
  UInt16	length;
  void		*buffer;
} iodevice_request_t;

typedef union				/**** Centronics status byte ****/
{
  char		b;
  struct
  {
    unsigned	reserved0:2;
    unsigned	paperError:1;
    unsigned	select:1;
    unsigned	notError:1;
    unsigned	reserved1:3;
  } status;
} centronics_status_t;

typedef struct classdriver_s		/**** g.classdriver context ****/
{
  IUNKNOWN_C_GUTS;
  CFPlugInRef		plugin;			/* release plugin */
  IUnknownVTbl		**factory;		/* Factory */
  void			*vendorReference;	/* vendor class specific usage */
  UInt32		location;		/* unique location in bus topology */
  UInt8			interfaceNumber;	/* Interface number */
  UInt16		vendorID;		/* Vendor id */
  UInt16		productID;		/* Product id */
  printer_interface_t	interface;		/* identify the device to IOKit */
  UInt8			outpipe;		/* mandatory bulkOut pipe */
  UInt8			inpipe;			/* optional bulkIn pipe */

  /* general class requests */
  kern_return_t (*DeviceRequest)(struct classdriver_s **printer, iodevice_request_t *iorequest, UInt16 timeout);
  kern_return_t	(*GetString)(struct classdriver_s **printer, UInt8 whichString, UInt16 language, UInt16 timeout, CFStringRef *result);

  /* standard printer class requests */
  kern_return_t	(*SoftReset)(struct classdriver_s **printer, UInt16 timeout);
  kern_return_t	(*GetCentronicsStatus)(struct classdriver_s **printer, centronics_status_t *result, UInt16 timeout);
  kern_return_t	(*GetDeviceID)(struct classdriver_s **printer, CFStringRef *devid, UInt16 timeout);

  /* standard bulk device requests */
  kern_return_t (*ReadPipe)(struct classdriver_s **printer, UInt8 *buffer, UInt32 *count);
  kern_return_t (*WritePipe)(struct classdriver_s **printer, UInt8 *buffer, UInt32 *count, Boolean eoj);

  /* interface requests */
  kern_return_t (*Open)(struct classdriver_s **printer, UInt32 location, UInt8 protocol);
  kern_return_t (*Abort)(struct classdriver_s **printer);
  kern_return_t (*Close)(struct classdriver_s **printer);

  /* initialize and terminate */
  kern_return_t (*Initialize)(struct classdriver_s **printer, struct classdriver_s **baseclass);
  kern_return_t (*Terminate)(struct classdriver_s **printer);

} classdriver_t;

typedef Boolean (*iterator_callback_t)(void *refcon, io_service_t obj);

typedef struct iterator_reference_s	/**** Iterator reference data */
{
  iterator_callback_t callback;
  void		*userdata;
  Boolean	keepRunning;
} iterator_reference_t;

typedef struct globals_s
{
  io_service_t		printer_obj;
  classdriver_t		**classdriver;

  pthread_mutex_t	read_thread_mutex;
  pthread_cond_t	read_thread_cond;
  int			read_thread_stop;
  int			read_thread_done;

  pthread_mutex_t	readwrite_lock_mutex;
  pthread_cond_t	readwrite_lock_cond;
  int			readwrite_lock;

  CFStringRef		make;
  CFStringRef		model;
  CFStringRef		serial;
  UInt32		location;
  UInt8			interfaceNum;

  CFRunLoopTimerRef 	status_timer;

  int			print_fd;	/* File descriptor to print */
  ssize_t		print_bytes;	/* Print bytes read */
#if DEBUG_WRITES
  ssize_t		debug_bytes;	/* Current bytes to read */
#endif /* DEBUG_WRITES */

  Boolean		wait_eof;
  int			drain_output;	/* Drain all pending output */
  int			bidi_flag;	/* 0=unidirectional, 1=bidirectional */

  pthread_mutex_t	sidechannel_thread_mutex;
  pthread_cond_t	sidechannel_thread_cond;
  int			sidechannel_thread_stop;
  int			sidechannel_thread_done;
} globals_t;


/*
 * Globals...
 */

globals_t g = { 0 };			/* Globals */
int Iterating = 0;			/* Are we iterating the bus? */


/*
 * Local functions...
 */

static Boolean find_device_cb(void *refcon, io_service_t obj);
static Boolean list_device_cb(void *refcon, io_service_t obj);
static CFStringRef cfstr_create_trim(const char *cstr);
static CFStringRef copy_value_for_key(CFStringRef deviceID, CFStringRef *keys);
static kern_return_t load_classdriver(CFStringRef driverPath, printer_interface_t interface, classdriver_t ***printerDriver);
static kern_return_t load_printerdriver(CFStringRef *driverBundlePath);
static kern_return_t registry_close(void);
static kern_return_t registry_open(CFStringRef *driverBundlePath);
static kern_return_t unload_classdriver(classdriver_t ***classdriver);
static OSStatus copy_deviceid(classdriver_t **printer, CFStringRef *deviceID);
static void *read_thread(void *reference);
static void *sidechannel_thread(void *reference);
static void copy_deviceinfo(CFStringRef deviceIDString, CFStringRef *make, CFStringRef *model, CFStringRef *serial);
static void copy_devicestring(io_service_t usbInterface, CFStringRef *deviceID, UInt32 *deviceLocation, UInt8 *interfaceNum);
static void device_added(void *userdata, io_iterator_t iterator);
static void get_device_id(cups_sc_status_t *status, char *data, int *datalen);
static void iterate_printers(iterator_callback_t callBack, void *userdata);
static void parse_options(char *options, char *serial, int serial_size, UInt32 *location, Boolean *wait_eof);
static void release_deviceinfo(CFStringRef *make, CFStringRef *model, CFStringRef *serial);
static void setup_cfLanguage(void);
static void soft_reset(void);
static void status_timer_cb(CFRunLoopTimerRef timer, void *info);
static void log_usb_class_driver(int is_64bit);
#define IS_64BIT 1
#define IS_NOT_64BIT 0

#if defined(__i386__) || defined(__x86_64__)
static pid_t	child_pid;		/* Child PID */
static void run_legacy_backend(int argc, char *argv[], int fd);	/* Starts child backend process running as a ppc executable */
#endif /* __i386__ || __x86_64__ */
static void sigterm_handler(int sig);	/* SIGTERM handler */
static void sigquit_handler(int sig, siginfo_t *si, void *unused);

#ifdef PARSE_PS_ERRORS
static const char *next_line (const char *buffer);
static void parse_pserror (char *sockBuffer, int len);
#endif /* PARSE_PS_ERRORS */

#pragma mark -

/*
 * 'list_devices()' - List all USB devices.
 */

void list_devices()
{
  iterate_printers(list_device_cb, NULL);
}


/*
 * 'print_device()' - Print a file to a USB device.
 */

int					/* O - Exit status */
print_device(const char *uri,		/* I - Device URI */
             const char *hostname,	/* I - Hostname/manufacturer */
             const char *resource,	/* I - Resource/modelname */
	     char       *options,	/* I - Device options/serial number */
	     int        print_fd,	/* I - File descriptor to print */
	     int        copies,		/* I - Copies to print */
	     int	argc,		/* I - Number of command-line arguments (6 or 7) */
	     char	*argv[])	/* I - Command-line arguments */
{
  char		  serial[1024];		/* Serial number buffer */
  OSStatus	  status;		/* Function results */
  IOReturn	  iostatus;		/* Current IO status */
  pthread_t	  read_thread_id,	/* Read thread */
		  sidechannel_thread_id;/* Side-channel thread */
  int		  have_sidechannel = 0;	/* Was the side-channel thread started? */
  struct stat     sidechannel_info;	/* Side-channel file descriptor info */
  char		  print_buffer[8192],	/* Print data buffer */
		  *print_ptr;		/* Pointer into print data buffer */
  UInt32	  location;		/* Unique location in bus topology */
  fd_set	  input_set;		/* Input set for select() */
  CFStringRef	  driverBundlePath;	/* Class driver path */
  int		  countdown,		/* Logging interval */
		  nfds;			/* Number of file descriptors */
  ssize_t	  total_bytes;		/* Total bytes written */
  UInt32	  bytes;		/* Bytes written */
  struct timeval  *timeout,		/* Timeout pointer */
		  tv;			/* Time value */
  struct timespec cond_timeout;		/* pthread condition timeout */
  struct sigaction action;		/* Actions for POSIX signals */


  (void)uri;

 /*
  * Catch SIGQUIT to determine who is sending it...
  */

  memset(&action, 0, sizeof(action));
  action.sa_sigaction = sigquit_handler;
  action.sa_flags = SA_SIGINFO;
  sigaction(SIGQUIT, &action, NULL);

 /*
  * See if the side-channel descriptor is valid...
  */

  have_sidechannel = !fstat(CUPS_SC_FD, &sidechannel_info) &&
                     S_ISSOCK(sidechannel_info.st_mode);

 /*
  * Localize using CoreFoundation...
  */

  setup_cfLanguage();

  parse_options(options, serial, sizeof(serial), &location, &g.wait_eof);

  if (resource[0] == '/')
    resource++;

  g.print_fd	= print_fd;
  g.make	= cfstr_create_trim(hostname);
  g.model	= cfstr_create_trim(resource);
  g.serial	= cfstr_create_trim(serial);
  g.location	= location;

  if (!g.make || !g.model)
  {
    fprintf(stderr, "DEBUG: Fatal USB error.\n");
    _cupsLangPrintFilter(stderr, "ERROR",
                         _("There was an unrecoverable USB error."));

    if (!g.make)
      fputs("DEBUG: USB make string is NULL\n", stderr);
    if (!g.model)
      fputs("DEBUG: USB model string is NULL\n", stderr);

    return (CUPS_BACKEND_STOP);
  }

  fputs("STATE: +connecting-to-device\n", stderr);

  countdown = INITIAL_LOG_INTERVAL;

  do
  {
    if (g.printer_obj)
    {
      IOObjectRelease(g.printer_obj);
      unload_classdriver(&g.classdriver);
      g.printer_obj = 0x0;
      g.classdriver = 0x0;
    }

    fprintf(stderr, "DEBUG: Looking for '%s %s'\n", hostname, resource);

    iterate_printers(find_device_cb, NULL);

    fputs("DEBUG: Opening connection\n", stderr);

    driverBundlePath = NULL;

    status = registry_open(&driverBundlePath);

#if defined(__i386__) || defined(__x86_64__)
    /*
     * If we were unable to load the class drivers for this printer it's
     * probably because they're ppc or i386. In this case try to run this
     * backend as i386 or ppc executables so we can use them...
     */
    if (status == -2)
    {
      run_legacy_backend(argc, argv, print_fd);
      /* Never returns here */
    }
#endif /* __i386__ || __x86_64__ */

    if (status ==  -2)
    {
     /*
      * If we still were unable to load the class drivers for this printer log
      * the error and stop the queue...
      */

      if (driverBundlePath == NULL || !CFStringGetCString(driverBundlePath, print_buffer, sizeof(print_buffer), kCFStringEncodingUTF8))
        strlcpy(print_buffer, "USB class driver", sizeof(print_buffer));

      fputs("STATE: +apple-missing-usbclassdriver-error\n", stderr);
      _cupsLangPrintFilter(stderr, "ERROR",
			   _("There was an unrecoverable USB error."));
      fprintf(stderr, "DEBUG: Could not load %s\n", print_buffer);

      if (driverBundlePath)
	CFRelease(driverBundlePath);

      return (CUPS_BACKEND_STOP);
    }

#ifdef __x86_64__
    if (status == noErr && driverBundlePath != NULL && CFStringCompare(driverBundlePath, kUSBGenericTOPrinterClassDriver, 0) != kCFCompareEqualTo)
      log_usb_class_driver(IS_64BIT);
#endif /* __x86_64__ */

    if (driverBundlePath)
      CFRelease(driverBundlePath);

    if (status != noErr)
    {
      sleep(PRINTER_POLLING_INTERVAL);
      countdown -= PRINTER_POLLING_INTERVAL;
      if (countdown <= 0)
      {
	_cupsLangPrintFilter(stderr, "INFO",
		             _("Waiting for printer to become available."));
	fprintf(stderr, "DEBUG: USB printer status: 0x%08x\n", (int)status);
	countdown = SUBSEQUENT_LOG_INTERVAL;	/* subsequent log entries, every 15 seconds */
      }
    }
  } while (status != noErr);

  fputs("STATE: -connecting-to-device\n", stderr);

  /*
   * Now that we are "connected" to the port, ignore SIGTERM so that we
   * can finish out any page data the driver sends (e.g. to eject the
   * current page...  Only ignore SIGTERM if we are printing data from
   * stdin (otherwise you can't cancel raw jobs...)
   */

  if (!print_fd)
  {
    memset(&action, 0, sizeof(action));

    sigemptyset(&action.sa_mask);
    action.sa_handler = SIG_IGN;
    sigaction(SIGTERM, &action, NULL);
  }

 /*
  * Start the side channel thread if the descriptor is valid...
  */

  pthread_mutex_init(&g.readwrite_lock_mutex, NULL);
  pthread_cond_init(&g.readwrite_lock_cond, NULL);
  g.readwrite_lock = 1;

  if (have_sidechannel)
  {
    g.sidechannel_thread_stop = 0;
    g.sidechannel_thread_done = 0;

    pthread_cond_init(&g.sidechannel_thread_cond, NULL);
    pthread_mutex_init(&g.sidechannel_thread_mutex, NULL);

    if (pthread_create(&sidechannel_thread_id, NULL, sidechannel_thread, NULL))
    {
      fprintf(stderr, "DEBUG: Fatal USB error.\n");
      _cupsLangPrintFilter(stderr, "ERROR",
			   _("There was an unrecoverable USB error."));
      fputs("DEBUG: Couldn't create side-channel thread\n", stderr);
      registry_close();
      return (CUPS_BACKEND_STOP);
    }
  }

 /*
  * Get the read thread going...
  */

  g.read_thread_stop = 0;
  g.read_thread_done = 0;

  pthread_cond_init(&g.read_thread_cond, NULL);
  pthread_mutex_init(&g.read_thread_mutex, NULL);

  if (pthread_create(&read_thread_id, NULL, read_thread, NULL))
  {
    fprintf(stderr, "DEBUG: Fatal USB error.\n");
    _cupsLangPrintFilter(stderr, "ERROR",
                         _("There was an unrecoverable USB error."));
    fputs("DEBUG: Couldn't create read thread\n", stderr);
    registry_close();
    return (CUPS_BACKEND_STOP);
  }

 /*
  * The main thread sends the print file...
  */

  g.drain_output = 0;
  g.print_bytes	 = 0;
  total_bytes	 = 0;
  print_ptr	 = print_buffer;

  while (status == noErr && copies-- > 0)
  {
    _cupsLangPrintFilter(stderr, "INFO", _("Sending data to printer."));

    if (print_fd != STDIN_FILENO)
    {
      fputs("PAGE: 1 1\n", stderr);
      lseek(print_fd, 0, SEEK_SET);
    }

    while (status == noErr)
    {
      FD_ZERO(&input_set);

      if (!g.print_bytes)
	FD_SET(print_fd, &input_set);

     /*
      * Calculate select timeout...
      *   If we have data waiting to send timeout is 100ms.
      *   else if we're draining print_fd timeout is 0.
      *   else we're waiting forever...
      */

      if (g.print_bytes)
      {
	tv.tv_sec  = 0;
	tv.tv_usec = 100000;		/* 100ms */
	timeout = &tv;
      }
      else if (g.drain_output)
      {
	tv.tv_sec  = 0;
	tv.tv_usec = 0;
	timeout = &tv;
      }
      else
	timeout = NULL;

     /*
      * I/O is unlocked around select...
      */

      pthread_mutex_lock(&g.readwrite_lock_mutex);
      g.readwrite_lock = 0;
      pthread_cond_signal(&g.readwrite_lock_cond);
      pthread_mutex_unlock(&g.readwrite_lock_mutex);

      nfds = select(print_fd + 1, &input_set, NULL, NULL, timeout);

     /*
      * Reacquire the lock...
      */

      pthread_mutex_lock(&g.readwrite_lock_mutex);
      while (g.readwrite_lock)
	pthread_cond_wait(&g.readwrite_lock_cond, &g.readwrite_lock_mutex);
      g.readwrite_lock = 1;
      pthread_mutex_unlock(&g.readwrite_lock_mutex);

      if (nfds < 0)
      {
	if (errno == EINTR && total_bytes == 0)
	{
	  fputs("DEBUG: Received an interrupt before any bytes were "
	        "written, aborting\n", stderr);
          registry_close();
          return (CUPS_BACKEND_OK);
	}
	else if (errno != EAGAIN && errno != EINTR)
	{
	  _cupsLangPrintFilter(stderr, "ERROR",
	                       _("Unable to read print data."));
	  perror("DEBUG: select");
	  registry_close();
          return (CUPS_BACKEND_FAILED);
	}
      }

     /*
      * If drain output has finished send a response...
      */

      if (g.drain_output && !nfds && !g.print_bytes)
      {
	/* Send a response... */
	cupsSideChannelWrite(CUPS_SC_CMD_DRAIN_OUTPUT, CUPS_SC_STATUS_OK, NULL, 0, 1.0);
	g.drain_output = 0;
      }

     /*
      * Check if we have print data ready...
      */

      if (FD_ISSET(print_fd, &input_set))
      {
#if DEBUG_WRITES
	g.debug_bytes += 512;
        if (g.debug_bytes > sizeof(print_buffer))
	  g.debug_bytes = 512;

	g.print_bytes = read(print_fd, print_buffer, g.debug_bytes);

#else
	g.print_bytes = read(print_fd, print_buffer, sizeof(print_buffer));
#endif /* DEBUG_WRITES */

	if (g.print_bytes < 0)
	{
	 /*
	  * Read error - bail if we don't see EAGAIN or EINTR...
	  */

	  if (errno != EAGAIN && errno != EINTR)
	  {
	    _cupsLangPrintFilter(stderr, "ERROR",
				 _("Unable to read print data."));
	    perror("DEBUG: read");
	    registry_close();
	    return (CUPS_BACKEND_FAILED);
	  }

	  g.print_bytes = 0;
	}
	else if (g.print_bytes == 0)
	{
	 /*
	  * End of file, break out of the loop...
	  */

	  break;
	}

	print_ptr = print_buffer;

	fprintf(stderr, "DEBUG: Read %d bytes of print data...\n",
		(int)g.print_bytes);
      }

      if (g.print_bytes)
      {
	bytes    = g.print_bytes;
	iostatus = (*g.classdriver)->WritePipe(g.classdriver, (UInt8*)print_ptr, &bytes, 0);

       /*
	* Ignore timeout errors, but retain the number of bytes written to
	* avoid sending duplicate data...
	*/

	if (iostatus == kIOUSBTransactionTimeout)
	{
	  fputs("DEBUG: Got USB transaction timeout during write\n", stderr);
	  iostatus = 0;
	}

       /*
        * If we've stalled, retry the write...
	*/

	else if (iostatus == kIOUSBPipeStalled)
	{
	  fputs("DEBUG: Got USB pipe stalled during write\n", stderr);

	  bytes    = g.print_bytes;
	  iostatus = (*g.classdriver)->WritePipe(g.classdriver, (UInt8*)print_ptr, &bytes, 0);
	}

       /*
	* Retry a write after an aborted write since we probably just got
	* SIGTERM...
	*/

	else if (iostatus == kIOReturnAborted)
	{
	  fputs("DEBUG: Got USB return aborted during write\n", stderr);

	  IOReturn err = (*g.classdriver)->Abort(g.classdriver);
	  fprintf(stderr, "DEBUG: USB class driver Abort returned %x\n", err);

#if DEBUG_WRITES
          sleep(5);
#endif /* DEBUG_WRITES */

	  bytes    = g.print_bytes;
	  iostatus = (*g.classdriver)->WritePipe(g.classdriver, (UInt8*)print_ptr, &bytes, 0);
        }

	if (iostatus)
	{
	 /*
	  * Write error - bail if we don't see an error we can retry...
	  */

	  _cupsLangPrintFilter(stderr, "ERROR",
	                       _("Unable to send data to printer."));
	  fprintf(stderr, "DEBUG: USB class driver WritePipe returned %x\n",
	          iostatus);

	  IOReturn err = (*g.classdriver)->Abort(g.classdriver);
	  fprintf(stderr, "DEBUG: USB class driver Abort returned %x\n",
	          err);

	  status = CUPS_BACKEND_FAILED;
	  break;
	}
	else if (bytes > 0)
	{
	  fprintf(stderr, "DEBUG: Wrote %d bytes of print data...\n", (int)bytes);

	  g.print_bytes -= bytes;
	  print_ptr   += bytes;
	  total_bytes += bytes;
	}
      }

      if (print_fd != 0 && status == noErr)
	fprintf(stderr, "DEBUG: Sending print file, %lld bytes...\n",
		(off_t)total_bytes);
    }
  }

  fprintf(stderr, "DEBUG: Sent %lld bytes...\n", (off_t)total_bytes);

 /*
  * Signal the side channel thread to exit...
  */

  if (have_sidechannel)
  {
    close(CUPS_SC_FD);
    pthread_mutex_lock(&g.readwrite_lock_mutex);
    g.readwrite_lock = 0;
    pthread_cond_signal(&g.readwrite_lock_cond);
    pthread_mutex_unlock(&g.readwrite_lock_mutex);

    g.sidechannel_thread_stop = 1;
    pthread_mutex_lock(&g.sidechannel_thread_mutex);

    if (!g.sidechannel_thread_done)
    {
      gettimeofday(&tv, NULL);
      cond_timeout.tv_sec  = tv.tv_sec + WAIT_SIDE_DELAY;
      cond_timeout.tv_nsec = tv.tv_usec * 1000;

      while (!g.sidechannel_thread_done)
      {
	if (pthread_cond_timedwait(&g.sidechannel_thread_cond,
				   &g.sidechannel_thread_mutex,
				   &cond_timeout) != 0)
	  break;
      }
    }

    pthread_mutex_unlock(&g.sidechannel_thread_mutex);
  }

 /*
  * Signal the read thread to exit then wait 7 seconds for it to complete...
  */

  g.read_thread_stop = 1;

  pthread_mutex_lock(&g.read_thread_mutex);

  if (!g.read_thread_done)
  {
    fputs("DEBUG: Waiting for read thread to exit...\n", stderr);

    gettimeofday(&tv, NULL);
    cond_timeout.tv_sec  = tv.tv_sec + WAIT_EOF_DELAY;
    cond_timeout.tv_nsec = tv.tv_usec * 1000;

    while (!g.read_thread_done)
    {
      if (pthread_cond_timedwait(&g.read_thread_cond, &g.read_thread_mutex,
				 &cond_timeout) != 0)
	break;
    }

   /*
    * If it didn't exit abort the pending read and wait an additional second...
    */

    if (!g.read_thread_done)
    {
      fputs("DEBUG: Read thread still active, aborting the pending read...\n",
	    stderr);

      g.wait_eof = 0;

      (*g.classdriver)->Abort(g.classdriver);

      gettimeofday(&tv, NULL);
      cond_timeout.tv_sec  = tv.tv_sec + 1;
      cond_timeout.tv_nsec = tv.tv_usec * 1000;

      while (!g.read_thread_done)
      {
	if (pthread_cond_timedwait(&g.read_thread_cond, &g.read_thread_mutex,
				   &cond_timeout) != 0)
	  break;
      }
    }
  }

  pthread_mutex_unlock(&g.read_thread_mutex);

 /*
  * Close the connection and input file and general clean up...
  */

  registry_close();

  if (print_fd != STDIN_FILENO)
    close(print_fd);

  if (g.make != NULL)
    CFRelease(g.make);

  if (g.model != NULL)
    CFRelease(g.model);

  if (g.serial != NULL)
    CFRelease(g.serial);

  if (g.printer_obj != 0x0)
    IOObjectRelease(g.printer_obj);

  return status;
}


/*
 * 'read_thread()' - Thread to read the backchannel data on.
 */

static void *read_thread(void *reference)
{
  UInt8				readbuffer[512];
  UInt32			rbytes;
  kern_return_t			readstatus;
  struct mach_timebase_info	timeBaseInfo;
  uint64_t			start,
				delay;


  (void)reference;

  /* Calculate what 250 milliSeconds are in mach absolute time...
   */
  mach_timebase_info(&timeBaseInfo);
  delay = ((uint64_t)250000000 * (uint64_t)timeBaseInfo.denom) / (uint64_t)timeBaseInfo.numer;

  do
  {
   /*
    * Remember when we started so we can throttle the loop after the read call...
    */

    start = mach_absolute_time();

    rbytes = sizeof(readbuffer);
    readstatus = (*g.classdriver)->ReadPipe(g.classdriver, readbuffer, &rbytes);
    if (readstatus == kIOReturnSuccess && rbytes > 0)
    {
      fprintf(stderr, "DEBUG: Read %d bytes of back-channel data...\n",
              (int)rbytes);
      cupsBackChannelWrite((char*)readbuffer, rbytes, 1.0);

      /* cntrl-d is echoed by the printer.
       * NOTES:
       *   Xerox Phaser 6250D doesn't echo the cntrl-d.
       *   Xerox Phaser 6250D doesn't always send the product query.
       */
      if (g.wait_eof && readbuffer[rbytes-1] == 0x4)
	break;

#ifdef PARSE_PS_ERRORS
      parse_pserror(readbuffer, rbytes);
#endif
    }
    else if (readstatus == kIOUSBTransactionTimeout)
      fputs("DEBUG: Got USB transaction timeout during read\n", stderr);
    else if (readstatus == kIOUSBPipeStalled)
      fputs("DEBUG: Got USB pipe stalled during read\n", stderr);
    else if (readstatus == kIOReturnAborted)
      fputs("DEBUG: Got USB return aborted during read\n", stderr);

   /*
    * Make sure this loop executes no more than once every 250 miliseconds...
    */

    if ((readstatus != kIOReturnSuccess || rbytes == 0) && (g.wait_eof || !g.read_thread_stop))
      mach_wait_until(start + delay);

  } while (g.wait_eof || !g.read_thread_stop);	/* Abort from main thread tests error here */

 /*
  * Let the main thread know that we have completed the read thread...
  */

  pthread_mutex_lock(&g.read_thread_mutex);
  g.read_thread_done = 1;
  pthread_cond_signal(&g.read_thread_cond);
  pthread_mutex_unlock(&g.read_thread_mutex);

  return NULL;
}


/*
 * 'sidechannel_thread()' - Handle side-channel requests.
 */

static void*
sidechannel_thread(void *reference)
{
  cups_sc_command_t	command;	/* Request command */
  cups_sc_status_t	status;		/* Request/response status */
  char			data[2048];	/* Request/response data */
  int			datalen;	/* Request/response data size */


  (void)reference;

  do
  {
    datalen = sizeof(data);

    if (cupsSideChannelRead(&command, &status, data, &datalen, 1.0))
    {
      if (status == CUPS_SC_STATUS_TIMEOUT)
	continue;
      else
	break;
    }

    switch (command)
    {
      case CUPS_SC_CMD_SOFT_RESET:	/* Do a soft reset */
	  fputs("DEBUG: CUPS_SC_CMD_SOFT_RESET received from driver...\n",
		stderr);

          if ((*g.classdriver)->SoftReset != NULL)
	  {
	    soft_reset();
	    cupsSideChannelWrite(command, CUPS_SC_STATUS_OK, NULL, 0, 1.0);
	    fputs("DEBUG: Returning status CUPS_STATUS_OK with no bytes...\n",
	          stderr);
	  }
	  else
	  {
	    cupsSideChannelWrite(command, CUPS_SC_STATUS_NOT_IMPLEMENTED,
	                         NULL, 0, 1.0);
	    fputs("DEBUG: Returning status CUPS_STATUS_NOT_IMPLEMENTED with "
	          "no bytes...\n", stderr);
	  }
	  break;

      case CUPS_SC_CMD_DRAIN_OUTPUT:	/* Drain all pending output */
	  fputs("DEBUG: CUPS_SC_CMD_DRAIN_OUTPUT received from driver...\n",
		stderr);

	  g.drain_output = 1;
	  break;

      case CUPS_SC_CMD_GET_BIDI:		/* Is the connection bidirectional? */
	  fputs("DEBUG: CUPS_SC_CMD_GET_BIDI received from driver...\n",
		stderr);

	  data[0] = g.bidi_flag;
	  cupsSideChannelWrite(command, CUPS_SC_STATUS_OK, data, 1, 1.0);

	  fprintf(stderr,
	          "DEBUG: Returned CUPS_SC_STATUS_OK with 1 byte (%02X)...\n",
		  data[0]);
	  break;

      case CUPS_SC_CMD_GET_DEVICE_ID:	/* Return IEEE-1284 device ID */
	  fputs("DEBUG: CUPS_SC_CMD_GET_DEVICE_ID received from driver...\n",
		stderr);

	  datalen = sizeof(data);
	  get_device_id(&status, data, &datalen);
	  cupsSideChannelWrite(command, CUPS_SC_STATUS_OK, data, datalen, 1.0);

          if (datalen < sizeof(data))
	    data[datalen] = '\0';
	  else
	    data[sizeof(data) - 1] = '\0';

	  fprintf(stderr,
	          "DEBUG: Returning CUPS_SC_STATUS_OK with %d bytes (%s)...\n",
		  datalen, data);
	  break;

      case CUPS_SC_CMD_GET_STATE:		/* Return device state */
	  fputs("DEBUG: CUPS_SC_CMD_GET_STATE received from driver...\n",
		stderr);

	  data[0] = CUPS_SC_STATE_ONLINE;
	  cupsSideChannelWrite(command, CUPS_SC_STATUS_OK, data, 1, 1.0);

	  fprintf(stderr,
	          "DEBUG: Returned CUPS_SC_STATUS_OK with 1 byte (%02X)...\n",
		  data[0]);
	  break;

      default:
	  fprintf(stderr, "DEBUG: Unknown side-channel command (%d) received "
			  "from driver...\n", command);

	  cupsSideChannelWrite(command, CUPS_SC_STATUS_NOT_IMPLEMENTED,
			       NULL, 0, 1.0);

	  fputs("DEBUG: Returned CUPS_SC_STATUS_NOT_IMPLEMENTED with no bytes...\n",
		stderr);
	  break;
    }
  }
  while (!g.sidechannel_thread_stop);

  pthread_mutex_lock(&g.sidechannel_thread_mutex);
  g.sidechannel_thread_done = 1;
  pthread_cond_signal(&g.sidechannel_thread_cond);
  pthread_mutex_unlock(&g.sidechannel_thread_mutex);

  return NULL;
}


#pragma mark -
/*
 * 'iterate_printers()' - Iterate over all the printers.
 */

static void iterate_printers(iterator_callback_t callBack,
			     void *userdata)
{
  Iterating = 1;

  mach_port_t	masterPort = 0x0;
  kern_return_t kr = IOMasterPort (bootstrap_port, &masterPort);

  if (kr == kIOReturnSuccess && masterPort != 0x0)
  {
    io_iterator_t addIterator = 0x0;

    iterator_reference_t reference = { callBack, userdata, true };
    IONotificationPortRef addNotification = IONotificationPortCreate(masterPort);

    int klass = kUSBPrintingClass;
    int subklass = kUSBPrintingSubclass;

    CFNumberRef usb_klass = CFNumberCreate(NULL, kCFNumberIntType, &klass);
    CFNumberRef usb_subklass = CFNumberCreate(NULL, kCFNumberIntType, &subklass);
    CFMutableDictionaryRef usbPrinterMatchDictionary = IOServiceMatching(kIOUSBInterfaceClassName);

    CFDictionaryAddValue(usbPrinterMatchDictionary, CFSTR("bInterfaceClass"), usb_klass);
    CFDictionaryAddValue(usbPrinterMatchDictionary, CFSTR("bInterfaceSubClass"), usb_subklass);

    CFRelease(usb_klass);
    CFRelease(usb_subklass);

    IOServiceAddMatchingNotification(addNotification, kIOMatchedNotification, usbPrinterMatchDictionary, &device_added, &reference, &addIterator);
    if (addIterator != 0x0)
    {
      device_added (&reference, addIterator);

      if (reference.keepRunning)
      {
	CFRunLoopAddSource(CFRunLoopGetCurrent(), IONotificationPortGetRunLoopSource(addNotification), kCFRunLoopDefaultMode);
	CFRunLoopRun();
      }
      IOObjectRelease(addIterator);
    }
    mach_port_deallocate(mach_task_self(), masterPort);
  }

  Iterating = 0;
}


/*
 * 'device_added()' - Device added notifier.
 */

static void device_added(void *userdata,
			 io_iterator_t iterator)
{
  iterator_reference_t *reference = userdata;

  io_service_t obj;
  while (reference->keepRunning && (obj = IOIteratorNext(iterator)) != 0x0)
  {
    if (reference->callback != NULL)
      reference->keepRunning = reference->callback(reference->userdata, obj);

    IOObjectRelease(obj);
  }

  /* One last call to the call back now that we are not longer have printers left to iterate...
   */
  if (reference->keepRunning && reference->callback)
    reference->keepRunning = reference->callback(reference->userdata, 0x0);

  if (!reference->keepRunning)
    CFRunLoopStop(CFRunLoopGetCurrent());
}


/*
 * 'list_device_cb()' - list_device iterator callback.
 */

static Boolean list_device_cb(void *refcon,
			      io_service_t obj)
{
  Boolean keepRunning = (obj != 0x0);


  (void)refcon;

  if (keepRunning)
  {
    CFStringRef deviceIDString = NULL;
    UInt32 deviceLocation = 0;
    UInt8	interfaceNum = 0;

    copy_devicestring(obj, &deviceIDString, &deviceLocation, &interfaceNum);
    if (deviceIDString != NULL)
    {
      CFStringRef make = NULL,  model = NULL, serial = NULL;
      char uristr[1024], makestr[1024], modelstr[1024], serialstr[1024];
      char optionsstr[1024], idstr[1024], make_modelstr[1024];

      copy_deviceinfo(deviceIDString, &make, &model, &serial);
      CFStringGetCString(deviceIDString, idstr, sizeof(idstr),
                         kCFStringEncodingUTF8);
      backendGetMakeModel(idstr, make_modelstr, sizeof(make_modelstr));

      modelstr[0] = '/';

      if (!make ||
          !CFStringGetCString(make, makestr, sizeof(makestr),
			      kCFStringEncodingUTF8))
        strlcpy(makestr, "Unknown", sizeof(makestr));

      if (!model ||
          !CFStringGetCString(model, &modelstr[1], sizeof(modelstr)-1,
			      kCFStringEncodingUTF8))
        strlcpy(modelstr + 1, "Printer", sizeof(modelstr) - 1);

      optionsstr[0] = '\0';
      if (serial != NULL)
      {
        CFStringGetCString(serial, serialstr, sizeof(serialstr), kCFStringEncodingUTF8);
	snprintf(optionsstr, sizeof(optionsstr), "?serial=%s", serialstr);
      }
      else if (deviceLocation != 0)
	snprintf(optionsstr, sizeof(optionsstr), "?location=%x", (unsigned)deviceLocation);

      httpAssembleURI(HTTP_URI_CODING_ALL, uristr, sizeof(uristr), "usb", NULL, makestr, 0, modelstr);
      strlcat(uristr, optionsstr, sizeof(uristr));

      cupsBackendReport("direct", uristr, make_modelstr, make_modelstr, idstr,
                        NULL);

      release_deviceinfo(&make, &model, &serial);
      CFRelease(deviceIDString);
    }
  }

  return keepRunning;
}


/*
 * 'find_device_cb()' - print_device iterator callback.
 */

static Boolean find_device_cb(void *refcon,
			      io_service_t obj)
{
  Boolean keepLooking = true;

  if (obj != 0x0)
  {
    CFStringRef idString = NULL;
    UInt32 location = -1;
    UInt8	interfaceNum = 0;

    copy_devicestring(obj, &idString, &location, &interfaceNum);
    if (idString != NULL)
    {
      CFStringRef make = NULL,  model = NULL, serial = NULL;

      copy_deviceinfo(idString, &make, &model, &serial);
      if (make && CFStringCompare(make, g.make, kCFCompareCaseInsensitive) == kCFCompareEqualTo)
      {
	if (model && CFStringCompare(model, g.model, kCFCompareCaseInsensitive) == kCFCompareEqualTo)
	{
	  if (g.serial != NULL && CFStringGetLength(g.serial) > 0)
	  {
	    if (serial != NULL && CFStringCompare(serial, g.serial, kCFCompareCaseInsensitive) == kCFCompareEqualTo)
	    {
	      IOObjectRetain(obj);
	      g.printer_obj = obj;
	      keepLooking = false;
	    }
	  }
	  else
	  {
	    if (g.printer_obj != 0)
	      IOObjectRelease(g.printer_obj);

	    g.printer_obj = obj;
	    IOObjectRetain(obj);

	    if (g.location == 0 || g.location == location)
	      keepLooking = false;
	  }
	  if ( !keepLooking )
		g.interfaceNum = interfaceNum;
	}
      }

      release_deviceinfo(&make, &model, &serial);
      CFRelease(idString);
    }
  }
  else
  {
    keepLooking = (g.printer_obj == 0);
    if (obj == 0x0 && keepLooking)
    {
      CFRunLoopTimerContext context = { 0, refcon, NULL, NULL, NULL };
      CFRunLoopTimerRef timer = CFRunLoopTimerCreate(NULL, CFAbsoluteTimeGetCurrent() + 1.0, 10, 0x0, 0x0, status_timer_cb, &context);
      if (timer != NULL)
      {
	CFRunLoopAddTimer(CFRunLoopGetCurrent(), timer, kCFRunLoopDefaultMode);
	g.status_timer = timer;
      }
    }
  }

  if (!keepLooking && g.status_timer != NULL)
  {
    fputs("STATE: -offline-report\n", stderr);
    _cupsLangPrintFilter(stderr, "INFO", _("The printer is now online."));
    CFRunLoopRemoveTimer(CFRunLoopGetCurrent(), g.status_timer, kCFRunLoopDefaultMode);
    CFRelease(g.status_timer);
    g.status_timer = NULL;
  }

  return keepLooking;
}


/*
 * 'status_timer_cb()' - Status timer callback.
 */

static void status_timer_cb(CFRunLoopTimerRef timer,
			    void *info)
{
  (void)timer;
  (void)info;

  fputs("STATE: +offline-report\n", stderr);
  _cupsLangPrintFilter(stderr, "INFO", _("The printer is offline."));

  if (getenv("CLASS") != NULL)
  {
   /*
    * If the CLASS environment variable is set, the job was submitted
    * to a class and not to a specific queue.  In this case, we want
    * to abort immediately so that the job can be requeued on the next
    * available printer in the class.
    *
    * Sleep 5 seconds to keep the job from requeuing too rapidly...
    */

    sleep(5);

    exit(CUPS_BACKEND_FAILED);
  }
}


#pragma mark -
/*
 * 'copy_deviceinfo()' - Copy strings from the 1284 device ID.
 */

static void copy_deviceinfo(CFStringRef deviceIDString,
			    CFStringRef *make,
			    CFStringRef *model,
			    CFStringRef *serial)
{
  CFStringRef modelKeys[]  = { CFSTR("MDL:"), CFSTR("MODEL:"), NULL };
  CFStringRef makeKeys[]   = { CFSTR("MFG:"), CFSTR("MANUFACTURER:"), NULL };
  CFStringRef serialKeys[] = { CFSTR("SN:"),  CFSTR("SERN:"), NULL };

  if (make != NULL)
    *make = copy_value_for_key(deviceIDString, makeKeys);

  if (model != NULL)
    *model = copy_value_for_key(deviceIDString, modelKeys);

  if (serial != NULL)
    *serial = copy_value_for_key(deviceIDString, serialKeys);
}


/*
 * 'release_deviceinfo()' - Release deviceinfo strings.
 */

static void release_deviceinfo(CFStringRef *make,
			       CFStringRef *model,
			       CFStringRef *serial)
{
  if (make != NULL && *make != NULL)
  {
    CFRelease(*make);
    *make = NULL;
  }

  if (model != NULL && *model != NULL)
  {
    CFRelease(*model);
    *model = NULL;
  }

  if (serial != NULL && *serial != NULL)
  {
    CFRelease(*serial);
    *serial = NULL;
  }
}


#pragma mark -
/*
 * 'load_classdriver()' - Load a classdriver.
 */

static kern_return_t load_classdriver(CFStringRef	    driverPath,
				      printer_interface_t   interface,
				      classdriver_t	    ***printerDriver)
{
  kern_return_t	kr = kUSBPrinterClassDeviceNotOpen;
  classdriver_t	**driver = NULL;
  CFStringRef	bundle = driverPath ? driverPath : kUSBGenericTOPrinterClassDriver;
  char 		bundlestr[1024];	/* Bundle path */
  CFURLRef	url;			/* URL for driver */
  CFPlugInRef	plugin = NULL;		/* Plug-in address */


  CFStringGetCString(bundle, bundlestr, sizeof(bundlestr), kCFStringEncodingUTF8);

 /*
  * Validate permissions for the class driver...
  */

  _cups_fc_result_t result = _cupsFileCheck(bundlestr,
                                            _CUPS_FILE_CHECK_DIRECTORY, 1,
                                            Iterating ? NULL : _cupsFileCheckFilter, NULL);

  if (result && driverPath)
    return (load_classdriver(NULL, interface, printerDriver));
  else if (result)
    return (kr);

 /*
  * Try loading the class driver...
  */

  url = CFURLCreateWithFileSystemPath(NULL, bundle, kCFURLPOSIXPathStyle, true);

  if (url)
  {
    plugin = CFPlugInCreate(NULL, url);
    CFRelease(url);
  }
  else
    plugin = NULL;

  if (plugin)
  {
    CFArrayRef factories = CFPlugInFindFactoriesForPlugInTypeInPlugIn(kUSBPrinterClassTypeID, plugin);
    if (factories != NULL && CFArrayGetCount(factories) > 0)
    {
      CFUUIDRef factoryID = CFArrayGetValueAtIndex(factories, 0);
      IUnknownVTbl **iunknown = CFPlugInInstanceCreate(NULL, factoryID, kUSBPrinterClassTypeID);
      if (iunknown != NULL)
      {
	kr = (*iunknown)->QueryInterface(iunknown, CFUUIDGetUUIDBytes(kUSBPrinterClassInterfaceID), (LPVOID *)&driver);
	if (kr == kIOReturnSuccess && driver != NULL)
	{
	  classdriver_t **genericDriver = NULL;
	  if (driverPath != NULL && CFStringCompare(driverPath, kUSBGenericTOPrinterClassDriver, 0) != kCFCompareEqualTo)
	    kr = load_classdriver(NULL, interface, &genericDriver);

	  if (kr == kIOReturnSuccess)
	  {
	    (*driver)->interface = interface;
	    (*driver)->Initialize(driver, genericDriver);

	    (*driver)->plugin = plugin;
	    (*driver)->interface = interface;
	    *printerDriver = driver;
	  }
	}
	(*iunknown)->Release(iunknown);
      }
      CFRelease(factories);
    }
  }

  fprintf(stderr, "DEBUG: load_classdriver(%s) (kr:0x%08x)\n", bundlestr, (int)kr);

  return (kr);
}


/*
 * 'unload_classdriver()' - Unload a classdriver.
 */

static kern_return_t unload_classdriver(classdriver_t ***classdriver)
{
  if (*classdriver != NULL)
  {
    (**classdriver)->Release(*classdriver);
    *classdriver = NULL;
  }

  return kIOReturnSuccess;
}


/*
 * 'load_printerdriver()' - Load vendor's classdriver.
 *
 * If driverBundlePath is not NULL on return it is the callers responsbility to release it!
 */

static kern_return_t load_printerdriver(CFStringRef *driverBundlePath)
{
  IOCFPlugInInterface	**iodev = NULL;
  SInt32		score;
  kern_return_t		kr;
  printer_interface_t	interface;
  HRESULT		res;

  kr = IOCreatePlugInInterfaceForService(g.printer_obj, kIOUSBInterfaceUserClientTypeID, kIOCFPlugInInterfaceID, &iodev, &score);
  if (kr == kIOReturnSuccess)
  {
    if ((res = (*iodev)->QueryInterface(iodev, USB_INTERFACE_KIND, (LPVOID *) &interface)) == noErr)
    {
      *driverBundlePath = IORegistryEntryCreateCFProperty(g.printer_obj, kUSBClassDriverProperty, NULL, kNilOptions);

      kr = load_classdriver(*driverBundlePath, interface, &g.classdriver);

      if (kr != kIOReturnSuccess)
	(*interface)->Release(interface);
    }
    IODestroyPlugInInterface(iodev);
  }
  return kr;
}


/*
 * 'registry_open()' - Open a connection to the printer.
 */

static kern_return_t registry_open(CFStringRef *driverBundlePath)
{
  g.bidi_flag = 0;	/* 0=unidirectional */

  kern_return_t kr = load_printerdriver(driverBundlePath);
  if (kr != kIOReturnSuccess)
    kr = -2;

  if (g.classdriver != NULL)
  {
  	(*g.classdriver)->interfaceNumber = g.interfaceNum;
    kr = (*g.classdriver)->Open(g.classdriver, g.location, kUSBPrintingProtocolBidirectional);
    if (kr != kIOReturnSuccess || (*g.classdriver)->interface == NULL)
    {
      kr = (*g.classdriver)->Open(g.classdriver, g.location, kUSBPrintingProtocolUnidirectional);
      if (kr == kIOReturnSuccess)
      {
	if ((*g.classdriver)->interface == NULL)
	{
	  (*g.classdriver)->Close(g.classdriver);
	  kr = -1;
	}
      }
    }
    else
      g.bidi_flag = 1;	/* 1=bidirectional */
  }

  if (kr != kIOReturnSuccess)
    unload_classdriver(&g.classdriver);

  return kr;
}


/*
 * 'registry_close()' - Close the connection to the printer.
 */

static kern_return_t registry_close(void)
{
  if (g.classdriver != NULL)
    (*g.classdriver)->Close(g.classdriver);

  unload_classdriver(&g.classdriver);
  return kIOReturnSuccess;
}


/*
 * 'copy_deviceid()' - Copy the 1284 device id string.
 */

static OSStatus copy_deviceid(classdriver_t **classdriver,
			      CFStringRef *deviceID)
{
  CFStringRef devID = NULL;
  CFStringRef deviceMake = NULL;
  CFStringRef deviceModel = NULL;
  CFStringRef deviceSerial = NULL;

  *deviceID = NULL;

  OSStatus err = (*classdriver)->GetDeviceID(classdriver, &devID, DEFAULT_TIMEOUT);

  copy_deviceinfo(devID, &deviceMake, &deviceModel, &deviceSerial);

  if (deviceMake == NULL || deviceModel == NULL || deviceSerial == NULL)
  {
    IOUSBDeviceDescriptor	desc;
    iodevice_request_t		request;

    request.requestType = USBmakebmRequestType(kUSBIn,  kUSBStandard, kUSBDevice);
    request.request = kUSBRqGetDescriptor;
    request.value = (kUSBDeviceDesc << 8) | 0;
    request.index = 0;
    request.length = sizeof(desc);
    request.buffer = &desc;
    err = (*classdriver)->DeviceRequest(classdriver, &request, DEFAULT_TIMEOUT);
    if (err == kIOReturnSuccess)
    {
      CFMutableStringRef newDevID = CFStringCreateMutable(NULL, 0);

      if (deviceMake == NULL)
      {
	CFStringRef data = NULL;
	err = (*classdriver)->GetString(classdriver, desc.iManufacturer, kUSBLanguageEnglish, DEFAULT_TIMEOUT, &data);
	if (data != NULL)
	{
	  CFStringAppendFormat(newDevID, NULL, CFSTR("MFG:%@;"), data);
	  CFRelease(data);
	}
      }

      if (deviceModel == NULL)
      {
	CFStringRef data = NULL;
	err = (*classdriver)->GetString(classdriver, desc.iProduct, kUSBLanguageEnglish, DEFAULT_TIMEOUT, &data);
	if (data != NULL)
	{
	  CFStringAppendFormat(newDevID, NULL, CFSTR("MDL:%@;"), data);
	  CFRelease(data);
	}
      }

      if (deviceSerial == NULL && desc.iSerialNumber != 0)
      {
	err = (*classdriver)->GetString(classdriver, desc.iSerialNumber, kUSBLanguageEnglish, DEFAULT_TIMEOUT, &deviceSerial);
	if (deviceSerial != NULL)
	{
	  CFStringAppendFormat(newDevID, NULL, CFSTR("SERN:%@;"), deviceSerial);
	}
      }

      if (devID != NULL)
      {
	CFStringAppend(newDevID, devID);
	CFRelease(devID);
      }

      *deviceID = newDevID;
    }
  }
  else
  {
    *deviceID = devID;
  }

  if (*deviceID == NULL)
      return err;

  /* Remove special characters from the serial number */
  CFRange range = (deviceSerial != NULL ? CFStringFind(deviceSerial, CFSTR("+"), 0) : CFRangeMake(0, 0));
  if (range.length == 1) {
      range = CFStringFind(*deviceID, deviceSerial, 0);

      CFMutableStringRef deviceIDString = CFStringCreateMutableCopy(NULL, 0, *deviceID);
      CFStringFindAndReplace(deviceIDString, CFSTR("+"), CFSTR(""), range, 0);
      CFRelease(*deviceID);
      *deviceID = deviceIDString;
  }

  release_deviceinfo(&deviceMake, &deviceModel, &deviceSerial);

  return err;
}


/*
 * 'copy_devicestring()' - Copy the 1284 device id string.
 */

static void copy_devicestring(io_service_t usbInterface,
			      CFStringRef *deviceID,
			      UInt32 *deviceLocation,
			      UInt8	*interfaceNumber )
{
  IOCFPlugInInterface	**iodev = NULL;
  SInt32		score;
  kern_return_t		kr;
  printer_interface_t	interface;
  HRESULT		res;
  classdriver_t	**klassDriver = NULL;
  CFStringRef		driverBundlePath;

  if ((kr = IOCreatePlugInInterfaceForService(usbInterface,
					 kIOUSBInterfaceUserClientTypeID,
					 kIOCFPlugInInterfaceID,
					 &iodev, &score)) == kIOReturnSuccess)
  {
    if ((res = (*iodev)->QueryInterface(iodev, USB_INTERFACE_KIND, (LPVOID *)
					&interface)) == noErr)
    {
      (*interface)->GetLocationID(interface, deviceLocation);
      (*interface)->GetInterfaceNumber(interface, interfaceNumber);

      driverBundlePath = IORegistryEntryCreateCFProperty(usbInterface,
							 kUSBClassDriverProperty,
							 NULL, kNilOptions);

      kr = load_classdriver(driverBundlePath, interface, &klassDriver);

      if (kr != kIOReturnSuccess && driverBundlePath != NULL)
	kr = load_classdriver(NULL, interface, &klassDriver);

      if (kr == kIOReturnSuccess && klassDriver != NULL)
	  copy_deviceid(klassDriver, deviceID);

      unload_classdriver(&klassDriver);

      if (driverBundlePath != NULL)
	CFRelease(driverBundlePath);

      /* (*interface)->Release(interface); */
    }
    IODestroyPlugInInterface(iodev);
  }
}


#pragma mark -
/*
 * 'copy_value_for_key()' - Copy value string associated with a key.
 */

static CFStringRef copy_value_for_key(CFStringRef deviceID,
				      CFStringRef *keys)
{
  CFStringRef	value = NULL;
  CFArrayRef	kvPairs = deviceID != NULL ? CFStringCreateArrayBySeparatingStrings(NULL, deviceID, CFSTR(";")) : NULL;
  CFIndex	max = kvPairs != NULL ? CFArrayGetCount(kvPairs) : 0;
  CFIndex	idx = 0;

  while (idx < max && value == NULL)
  {
    CFStringRef kvpair = CFArrayGetValueAtIndex(kvPairs, idx);
    CFIndex idxx = 0;
    while (keys[idxx] != NULL && value == NULL)
    {
      CFRange range = CFStringFind(kvpair, keys[idxx], kCFCompareCaseInsensitive);
      if (range.length != -1)
      {
	if (range.location != 0)
	{
	  CFMutableStringRef theString = CFStringCreateMutableCopy(NULL, 0, kvpair);
	  CFStringTrimWhitespace(theString);
	  range = CFStringFind(theString, keys[idxx], kCFCompareCaseInsensitive);
	  if (range.location == 0)
	    value = CFStringCreateWithSubstring(NULL, theString, CFRangeMake(range.length, CFStringGetLength(theString) - range.length));

	  CFRelease(theString);
	}
	else
	{
	  CFStringRef theString = CFStringCreateWithSubstring(NULL, kvpair, CFRangeMake(range.length, CFStringGetLength(kvpair) - range.length));
	  CFMutableStringRef theString2 = CFStringCreateMutableCopy(NULL, 0, theString);
	  CFRelease(theString);

	  CFStringTrimWhitespace(theString2);
	  value = theString2;
	}
      }
      idxx++;
    }
    idx++;
  }

  if (kvPairs != NULL)
    CFRelease(kvPairs);
  return value;
}


/*
 * 'cfstr_create_trim()' - Create CFString and trim whitespace characters.
 */

CFStringRef cfstr_create_trim(const char *cstr)
{
  CFStringRef		cfstr;
  CFMutableStringRef	cfmutablestr = NULL;

  if ((cfstr = CFStringCreateWithCString(NULL, cstr, kCFStringEncodingUTF8)) != NULL)
  {
    if ((cfmutablestr = CFStringCreateMutableCopy(NULL, 1024, cfstr)) != NULL)
      CFStringTrimWhitespace(cfmutablestr);

    CFRelease(cfstr);
  }
  return (CFStringRef) cfmutablestr;
}


#pragma mark -
/*
 * 'parse_options()' - Parse URI options.
 */

static void parse_options(char *options,
			  char *serial,
			  int serial_size,
			  UInt32 *location,
			  Boolean *wait_eof)
{
  char	sep,				/* Separator character */
	*name,				/* Name of option */
	*value;				/* Value of option */


  if (serial)
    *serial = '\0';
  if (location)
    *location = 0;

  if (!options)
    return;

  while (*options)
  {
   /*
    * Get the name...
    */

    name = options;

    while (*options && *options != '=' && *options != '+' && *options != '&')
      options ++;

    if ((sep = *options) != '\0')
      *options++ = '\0';

    if (sep == '=')
    {
     /*
      * Get the value...
      */

      value = options;

      while (*options && *options != '+' && *options != '&')
	options ++;

      if (*options)
	*options++ = '\0';
    }
    else
      value = (char *)"";

   /*
    * Process the option...
    */

    if (!_cups_strcasecmp(name, "waiteof"))
    {
      if (!_cups_strcasecmp(value, "on") ||
	  !_cups_strcasecmp(value, "yes") ||
	  !_cups_strcasecmp(value, "true"))
	*wait_eof = true;
      else if (!_cups_strcasecmp(value, "off") ||
	       !_cups_strcasecmp(value, "no") ||
	       !_cups_strcasecmp(value, "false"))
	*wait_eof = false;
      else
	_cupsLangPrintFilter(stderr, "WARNING",
			     _("Boolean expected for waiteof option \"%s\"."),
			     value);
    }
    else if (!_cups_strcasecmp(name, "serial"))
      strlcpy(serial, value, serial_size);
    else if (!_cups_strcasecmp(name, "location") && location)
      *location = strtol(value, NULL, 16);
  }
}


/*!
 * @function	setup_cfLanguage
 * @abstract	Convert the contents of the CUPS 'APPLE_LANGUAGE' environment
 *		variable into a one element CF array of languages.
 *
 * @discussion	Each submitted job comes with a natural language. CUPS passes
 * 		that language in an environment variable. We take that language
 * 		and jam it into the AppleLanguages array so that CF will use
 * 		it when reading localized resources. We need to do this before
 * 		any CF code reads and caches the languages array, so this function
 *		should be called early in main()
 */
static void setup_cfLanguage(void)
{
  CFStringRef	lang[1] = {NULL};
  CFArrayRef	langArray = NULL;
  const char	*requestedLang = NULL;

  if ((requestedLang = getenv("APPLE_LANGUAGE")) == NULL)
    requestedLang = getenv("LANG");

  if (requestedLang != NULL)
  {
    lang[0] = CFStringCreateWithCString(kCFAllocatorDefault, requestedLang, kCFStringEncodingUTF8);
    langArray = CFArrayCreate(kCFAllocatorDefault, (const void **)lang, sizeof(lang) / sizeof(lang[0]), &kCFTypeArrayCallBacks);

    CFPreferencesSetValue(CFSTR("AppleLanguages"), langArray, kCFPreferencesCurrentApplication, kCFPreferencesAnyUser, kCFPreferencesAnyHost);
    fprintf(stderr, "DEBUG: usb: AppleLanguages=\"%s\"\n", requestedLang);

    CFRelease(lang[0]);
    CFRelease(langArray);
  }
  else
    fputs("DEBUG: usb: LANG and APPLE_LANGUAGE environment variables missing.\n", stderr);
}

#pragma mark -
#if defined(__i386__) || defined(__x86_64__)
/*!
 * @function	run_legacy_backend
 *
 * @abstract	Starts child backend process running as a ppc or i386 executable.
 *
 * @result	Never returns; always calls exit().
 *
 * @discussion
 */
static void run_legacy_backend(int argc,
			       char *argv[],
			       int fd)
{
  int	i;
  int	exitstatus = 0;
  int	childstatus;
  pid_t	waitpid_status;
  char	*my_argv[32];
  char	*usb_legacy_status;

 /*
  * If we're running as x86_64 or i386 and couldn't load the class driver
  * (because it's ppc or i386), then try to re-exec ourselves in ppc or i386
  * mode to try again. If we don't have a ppc or i386 architecture we may be
  * running with the same architecture again so guard against this by setting
  * and testing an environment variable...
  */

#  ifdef __x86_64__
  usb_legacy_status = getenv("USB_I386_STATUS");
#  else
  usb_legacy_status = getenv("USB_PPC_STATUS");
#  endif /* __x86_64__ */

  if (!usb_legacy_status)
  {
    log_usb_class_driver(IS_NOT_64BIT);

   /*
    * Setup a SIGTERM handler then block it before forking...
    */

    int			err;		/* posix_spawn result */
    struct sigaction	action;		/* POSIX signal action */
    sigset_t		newmask,	/* New signal mask */
			oldmask;	/* Old signal mask */
    char		usbpath[1024];	/* Path to USB backend */
    const char		*cups_serverbin;/* Path to CUPS binaries */


    memset(&action, 0, sizeof(action));
    sigaddset(&action.sa_mask, SIGTERM);
    action.sa_handler = sigterm_handler;
    sigaction(SIGTERM, &action, NULL);

    sigemptyset(&newmask);
    sigaddset(&newmask, SIGTERM);
    sigprocmask(SIG_BLOCK, &newmask, &oldmask);

   /*
    * Set the environment variable...
    */

#  ifdef __x86_64__
    setenv("USB_I386_STATUS", "1", false);
#  else
    setenv("USB_PPC_STATUS", "1", false);
#  endif /* __x86_64__ */

   /*
    * Tell the kernel to use the specified CPU architecture...
    */

#  ifdef __x86_64__
    cpu_type_t cpu = CPU_TYPE_I386;
#  else
    cpu_type_t cpu = CPU_TYPE_POWERPC;
#  endif /* __x86_64__ */
    size_t ocount = 1;
    posix_spawnattr_t attrs;

    if (!posix_spawnattr_init(&attrs))
    {
      posix_spawnattr_setsigdefault(&attrs, &oldmask);
      if (posix_spawnattr_setbinpref_np(&attrs, 1, &cpu, &ocount) || ocount != 1)
      {
#  ifdef __x86_64__
	perror("DEBUG: Unable to set binary preference to i386");
#  else
	perror("DEBUG: Unable to set binary preference to ppc");
#  endif /* __x86_64__ */
	_cupsLangPrintFilter(stderr, "ERROR",
	                     _("Unable to use legacy USB class driver."));
	exit(CUPS_BACKEND_STOP);
      }
    }

   /*
    * Set up the arguments and call posix_spawn...
    */

    if ((cups_serverbin = getenv("CUPS_SERVERBIN")) == NULL)
      cups_serverbin = CUPS_SERVERBIN;
    snprintf(usbpath, sizeof(usbpath), "%s/backend/usb", cups_serverbin);

    for (i = 0; i < argc && i < (sizeof(my_argv) / sizeof(my_argv[0])) - 1; i ++)
      my_argv[i] = argv[i];

    my_argv[i] = NULL;

    if ((err = posix_spawn(&child_pid, usbpath, NULL, &attrs, my_argv,
                           environ)) != 0)
    {
      fprintf(stderr, "DEBUG: Unable to exec %s: %s\n", usbpath,
              strerror(err));
      _cupsLangPrintFilter(stderr, "ERROR",
                           _("Unable to use legacy USB class driver."));
      exit(CUPS_BACKEND_STOP);
    }

   /*
    * Unblock signals...
    */

    sigprocmask(SIG_SETMASK, &oldmask, NULL);

   /*
    * Close the fds we won't be using then wait for the child backend to exit.
    */

    close(fd);
    close(1);

    fprintf(stderr, "DEBUG: Started usb(legacy) backend (PID %d)\n",
            (int)child_pid);

    while ((waitpid_status = waitpid(child_pid, &childstatus, 0)) == (pid_t)-1 && errno == EINTR)
      usleep(1000);

    if (WIFSIGNALED(childstatus))
    {
      exitstatus = CUPS_BACKEND_STOP;
      fprintf(stderr, "DEBUG: usb(legacy) backend %d crashed on signal %d\n",
              child_pid, WTERMSIG(childstatus));
    }
    else
    {
      if ((exitstatus = WEXITSTATUS(childstatus)) != 0)
	fprintf(stderr,
	        "DEBUG: usb(legacy) backend %d stopped with status %d\n",
		child_pid, exitstatus);
      else
	fprintf(stderr, "DEBUG: usb(legacy) backend %d exited with no errors\n",
	        child_pid);
    }
  }
  else
  {
    fputs("DEBUG: usb(legacy) backend running native again\n", stderr);
    exitstatus = CUPS_BACKEND_STOP;
  }

  exit(exitstatus);
}
#endif /* __i386__ || __x86_64__ */


/*
 * 'sigterm_handler()' - SIGTERM handler.
 */

static void
sigterm_handler(int sig)		/* I - Signal */
{
#if defined(__i386__) || defined(__x86_64__)
 /*
  * If we started a child process pass the signal on to it...
  */

  if (child_pid)
  {
   /*
    * If we started a child process pass the signal on to it...
    */

    int	status;

    kill(child_pid, sig);
    while (waitpid(child_pid, &status, 0) < 0 && errno == EINTR);

    if (WIFEXITED(status))
      exit(WEXITSTATUS(status));
    else if (status == SIGTERM || status == SIGKILL)
      exit(0);
    else
    {
      fprintf(stderr, "DEBUG: Child crashed on signal %d\n", status);
      exit(CUPS_BACKEND_STOP);
    }
  }
#endif /* __i386__ || __x86_64__ */
}


/*
 * 'sigquit_handler()' - SIGQUIT handler.
 */

static void sigquit_handler(int sig, siginfo_t *si, void *unused)
{
  char  *path;
  char	pathbuf[PROC_PIDPATHINFO_MAXSIZE];
  static char msgbuf[256] = "";


  (void)sig;
  (void)unused;

  if (proc_pidpath(si->si_pid, pathbuf, sizeof(pathbuf)) > 0 &&
      (path = basename(pathbuf)) != NULL)
    snprintf(msgbuf, sizeof(msgbuf), "SIGQUIT sent by %s(%d)", path, (int)si->si_pid);
  else
    snprintf(msgbuf, sizeof(msgbuf), "SIGQUIT sent by PID %d", (int)si->si_pid);

  CRSetCrashLogMessage(msgbuf);

  abort();
}


#ifdef PARSE_PS_ERRORS
/*
 * 'next_line()' - Find the next line in a buffer.
 */

static const char *next_line (const char *buffer)
{
  const char *cptr, *lptr = NULL;

  for (cptr = buffer; *cptr && lptr == NULL; cptr++)
    if (*cptr == '\n' || *cptr == '\r')
      lptr = cptr;
  return lptr;
}


/*
 * 'parse_pserror()' - Scan the backchannel data for postscript errors.
 */

static void parse_pserror(char *sockBuffer,
			  int len)
{
  static char  gErrorBuffer[1024] = "";
  static char *gErrorBufferPtr = gErrorBuffer;
  static char *gErrorBufferEndPtr = gErrorBuffer + sizeof(gErrorBuffer);

  char *pCommentBegin, *pCommentEnd, *pLineEnd;
  char *logLevel;
  char logstr[1024];
  int  logstrlen;

  if (gErrorBufferPtr + len > gErrorBufferEndPtr - 1)
    gErrorBufferPtr = gErrorBuffer;
  if (len > sizeof(gErrorBuffer) - 1)
    len = sizeof(gErrorBuffer) - 1;

  memcpy(gErrorBufferPtr, (const void *)sockBuffer, len);
  gErrorBufferPtr += len;
  *(gErrorBufferPtr + 1) = '\0';

  pLineEnd = (char *)next_line((const char *)gErrorBuffer);
  while (pLineEnd != NULL)
  {
    *pLineEnd++ = '\0';

    pCommentBegin = strstr(gErrorBuffer,"%%[");
    pCommentEnd = strstr(gErrorBuffer, "]%%");
    if (pCommentBegin != gErrorBuffer && pCommentEnd != NULL)
    {
      pCommentEnd += 3;            /* Skip past "]%%" */
      *pCommentEnd = '\0';         /* There's always room for the nul */

      if (_cups_strncasecmp(pCommentBegin, "%%[ Error:", 10) == 0)
	logLevel = "DEBUG";
      else if (_cups_strncasecmp(pCommentBegin, "%%[ Flushing", 12) == 0)
	logLevel = "DEBUG";
      else
	logLevel = "INFO";

      if ((logstrlen = snprintf(logstr, sizeof(logstr), "%s: %s\n", logLevel, pCommentBegin)) >= sizeof(logstr))
      {
	/* If the string was trucnated make sure it has a linefeed before the nul */
	logstrlen = sizeof(logstr) - 1;
	logstr[logstrlen - 1] = '\n';
      }
      write(STDERR_FILENO, logstr, logstrlen);
    }

    /* move everything over... */
    strlcpy(gErrorBuffer, pLineEnd, sizeof(gErrorBuffer));
    gErrorBufferPtr = gErrorBuffer;
    pLineEnd = (char *)next_line((const char *)gErrorBuffer);
  }
}
#endif /* PARSE_PS_ERRORS */


/*
 * 'soft_reset()' - Send a soft reset to the device.
 */

static void soft_reset(void)
{
  fd_set	  input_set;		/* Input set for select() */
  struct timeval  tv;			/* Time value */
  char		  buffer[2048];		/* Buffer */
  struct timespec cond_timeout;		/* pthread condition timeout */

 /*
  * Send an abort once a second until the I/O lock is released by the main thread...
  */

  pthread_mutex_lock(&g.readwrite_lock_mutex);
  while (g.readwrite_lock)
  {
    (*g.classdriver)->Abort(g.classdriver);

    gettimeofday(&tv, NULL);
    cond_timeout.tv_sec  = tv.tv_sec + 1;
    cond_timeout.tv_nsec = tv.tv_usec * 1000;

    while (g.readwrite_lock)
    {
      if (pthread_cond_timedwait(&g.readwrite_lock_cond,
				 &g.readwrite_lock_mutex,
				 &cond_timeout) != 0)
	break;
    }
  }

  g.readwrite_lock = 1;
  pthread_mutex_unlock(&g.readwrite_lock_mutex);

 /*
  * Flush bytes waiting on print_fd...
  */

  g.print_bytes = 0;

  FD_ZERO(&input_set);
  FD_SET(g.print_fd, &input_set);

  tv.tv_sec  = 0;
  tv.tv_usec = 0;

  while (select(g.print_fd+1, &input_set, NULL, NULL, &tv) > 0)
    if (read(g.print_fd, buffer, sizeof(buffer)) <= 0)
      break;

 /*
  * Send the reset...
  */

  (*g.classdriver)->SoftReset(g.classdriver, DEFAULT_TIMEOUT);

 /*
  * Release the I/O lock...
  */

  pthread_mutex_lock(&g.readwrite_lock_mutex);
  g.readwrite_lock = 0;
  pthread_cond_signal(&g.readwrite_lock_cond);
  pthread_mutex_unlock(&g.readwrite_lock_mutex);
}


/*
 * 'get_device_id()' - Return IEEE-1284 device ID.
 */

static void get_device_id(cups_sc_status_t *status,
			  char *data,
			  int *datalen)
{
  CFStringRef deviceIDString = NULL;

  /* GetDeviceID */
  copy_deviceid(g.classdriver, &deviceIDString);

  if (deviceIDString)
  {
    CFStringGetCString(deviceIDString, data, *datalen, kCFStringEncodingUTF8);
    *datalen = strlen(data);
    CFRelease(deviceIDString);
  }
  *status  = CUPS_SC_STATUS_OK;
}


static void
log_usb_class_driver(int is_64bit)	/* I - Is the USB class driver 64-bit? */
{
 /*
  * Report the usage of legacy USB class drivers to Apple if the user opts into providing
  * feedback to Apple...
  */

  aslmsg aslm = asl_new(ASL_TYPE_MSG);
  if (aslm)
  {
    ppd_file_t *ppd = ppdOpenFile(getenv("PPD"));
    const char *make_model = ppd ? ppd->nickname : NULL;
    ppd_attr_t *version = ppdFindAttr(ppd, "FileVersion", "");

    asl_set(aslm, "com.apple.message.domain", "com.apple.printing.usb.64bit");
    asl_set(aslm, "com.apple.message.result", is_64bit ? "yes" : "no");
    asl_set(aslm, "com.apple.message.signature", make_model ? make_model : "Unknown");
    asl_set(aslm, "com.apple.message.signature2", version ? version->value : "?.?");
    asl_set(aslm, "com.apple.message.summarize", "YES");
    asl_log(NULL, aslm, ASL_LEVEL_NOTICE, "");
    asl_free(aslm);
  }
}


/*
 * End of "$Id: usb-darwin.c 11670 2014-03-04 14:53:59Z msweet $".
 */
