/*
 * "$Id$"
 *
 * Copyright © 2005-2007 Apple Inc. All rights reserved.
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
 *   USB port on Darwin backend for the Common UNIX Printing System (CUPS).
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <sys/sysctl.h>
#include <libgen.h>
#include <mach/mach.h>	
#include <mach/mach_error.h>
#include <mach/mach_time.h>
#include <cups/debug.h>
#include <cups/i18n.h>
#include <cups/sidechannel.h>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/IOCFPlugIn.h>

#include <pthread.h>


/* 
 * WAITEOF_DELAY is number of seconds we'll wait for responses from
 * the printer after we've finished sending all the data 
 */
#define WAITEOF_DELAY			7
#define DEFAULT_TIMEOUT			60L

#define	USB_INTERFACE_KIND		CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID190)
#define kUSBLanguageEnglish		0x409

#define PRINTER_POLLING_INTERVAL	5				/* seconds */
#define INITIAL_LOG_INTERVAL		(PRINTER_POLLING_INTERVAL)
#define SUBSEQUENT_LOG_INTERVAL		(3*INITIAL_LOG_INTERVAL)

#define kUSBPrinterClassTypeID		(CFUUIDGetConstantUUIDWithBytes(NULL, 0x06, 0x04, 0x7D, 0x16, 0x53, 0xA2, 0x11, 0xD6, 0x92, 0x06, 0x00, 0x30, 0x65, 0x52, 0x45, 0x92))
#define	kUSBPrinterClassInterfaceID	(CFUUIDGetConstantUUIDWithBytes(NULL, 0x03, 0x34, 0x6D, 0x74, 0x53, 0xA3, 0x11, 0xD6, 0x9E, 0xA1, 0x76, 0x30, 0x65, 0x52, 0x45, 0x92))

#define kUSBClassDriverProperty		CFSTR("USB Printing Class")

#define kUSBGenericTOPrinterClassDriver	CFSTR("/System/Library/Printers/Libraries/USBGenericTOPrintingClass.plugin")
#define kUSBPrinterClassDeviceNotOpen	-9664	/*kPMInvalidIOMContext*/
#define kWriteBufferSize		2048


#pragma mark -
/*
 * Section 5.3 USB Printing Class spec
 */
#define kUSBPrintingSubclass			1
#define kUSBPrintingProtocolNoOpen		0
#define kUSBPrintingProtocolUnidirectional	1
#define kUSBPrintingProtocolBidirectional	2

typedef IOUSBInterfaceInterface190	**printer_interface_t;

typedef struct iodevice_request_s		/**** Device request ****/
{
  UInt8		requestType;			
  UInt8		request;
  UInt16	value;
  UInt16	index;
  UInt16	length;
  void		*buffer;	
} iodevice_request_t;

typedef union {					/**** Centronics status byte ****/
  char		b;
  struct {
    unsigned	reserved0:2;
    unsigned	paperError:1;
    unsigned	select:1;
    unsigned	notError:1;
    unsigned	reserved1:3;
  } status;
} centronics_status_t;

typedef struct classdriver_context_s		/**** Classdriver context ****/
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
  UInt8		  	outpipe;		/* mandatory bulkOut pipe */
  UInt8			inpipe;			/* optional bulkIn pipe */

  /* general class requests */
  kern_return_t (*DeviceRequest)( struct classdriver_context_s **printer, iodevice_request_t *iorequest, UInt16 timeout );
  kern_return_t	(*GetString)( struct classdriver_context_s **printer, UInt8 whichString, UInt16 language, UInt16 timeout, CFStringRef *result );

  /* standard printer class requests */
  kern_return_t	(*SoftReset)( struct classdriver_context_s **printer, UInt16 timeout );
  kern_return_t	(*GetCentronicsStatus)( struct classdriver_context_s **printer, centronics_status_t *result, UInt16 timeout );
  kern_return_t	(*GetDeviceID)( struct classdriver_context_s **printer, CFStringRef *devid, UInt16 timeout );

  /* standard bulk device requests */
  kern_return_t (*ReadPipe)( struct classdriver_context_s **printer, UInt8 *buffer, UInt32 *count );
  kern_return_t (*WritePipe)( struct classdriver_context_s **printer, UInt8 *buffer, UInt32 *count, Boolean eoj );

  /* interface requests */
  kern_return_t (*Open)( struct classdriver_context_s **printer, UInt32 location, UInt8 protocol );
  kern_return_t (*Abort)( struct classdriver_context_s **printer );
  kern_return_t (*Close)( struct classdriver_context_s **printer );

  /* initialize and terminate */
  kern_return_t (*Initialize)( struct classdriver_context_s **printer, struct classdriver_context_s **baseclass );
  kern_return_t (*Terminate)( struct classdriver_context_s **printer );

} classdriver_context_t;


typedef Boolean (*iterator_callback_t)(void *refcon, io_service_t obj);

typedef struct iterator_reference_s {		/**** Iterator reference data */
  iterator_callback_t callback;
  void		*userdata;
  Boolean	keepRunning;
} iterator_reference_t;

typedef struct printer_data_s {			/**** Printer context data ****/
  io_service_t		  printerObj;
  classdriver_context_t  **printerDriver;

  pthread_cond_t	readCompleteCondition;
  pthread_mutex_t	readMutex;
  int			done;

  const char		*uri;
  CFStringRef		make;
  CFStringRef		model;
  CFStringRef		serial;

  UInt32		location;
  Boolean		waitEOF;
  
  CFRunLoopTimerRef statusTimer;

  pthread_cond_t	reqWaitCompCond;
  pthread_mutex_t	reqWaitMutex;
  pthread_mutex_t	waitCloseMutex;
  pthread_mutex_t	writeCompMutex;
  int			writeDone;
  int			reqWaitDone;
  int			reqWqitFlag;
  int			directionalFlag;	/* 0=uni, 1=bidi */
  ssize_t		dataSize;
  ssize_t		dataOffset;
  char			dataBuffer[kWriteBufferSize];
} printer_data_t;


/*
 * Local functions...
 */

static Boolean list_device_callback(void *refcon, io_service_t obj);
static Boolean find_device_callback(void *refcon, io_service_t obj);
static void statusTimerCallback(CFRunLoopTimerRef timer, void *info);
static void iterate_printers(iterator_callback_t callBack, void *userdata);
static void device_added(void *userdata, io_iterator_t iterator);
static void copy_deviceinfo(CFStringRef deviceIDString, CFStringRef *make, CFStringRef *model, CFStringRef *serial);
static void release_deviceinfo(CFStringRef *make, CFStringRef *model, CFStringRef *serial);
static kern_return_t load_classdriver(CFStringRef driverPath, printer_interface_t intf, classdriver_context_t ***driver);
static kern_return_t unload_classdriver(classdriver_context_t ***classDriver);
static kern_return_t load_printerdriver(printer_data_t *printer, CFStringRef *driverBundlePath);
static kern_return_t registry_open(printer_data_t *printer, CFStringRef *driverBundlePath);
static kern_return_t registry_close(printer_data_t *printer);
static OSStatus copy_deviceid(classdriver_context_t **printer, CFStringRef *deviceID);
static void copy_devicestring(io_service_t usbInterface, CFStringRef *deviceID, UInt32 *deviceLocation);
static CFStringRef copy_value_for_key(CFStringRef deviceID, CFStringRef *keys);
static CFStringRef cfstr_create_and_trim(const char *cstr);
static void parse_options(const char *options, char *serial, UInt32 *location, Boolean *waitEOF);
static void setup_cfLanguage(void);
static void *read_thread(void *reference);
static void *reqestWait_thread(void *reference);
static void usbSoftReset(printer_data_t *userData, cups_sc_status_t *status);
static void usbDrainOutput(printer_data_t *userData, cups_sc_status_t *status);
static void usbGetBidirectional(printer_data_t *userData, cups_sc_status_t *status, char *data, int *datalen);
static void usbGetDeviceID(printer_data_t *userData, cups_sc_status_t *status, char *data, int *datalen);
static void usbGetDevState(printer_data_t *userData, cups_sc_status_t *status, char *data, int *datalen);


#if defined(__i386__)
static pid_t	child_pid;					/* Child PID */
static void run_ppc_backend(int argc, char *argv[], int fd);	/* Starts child backend process running as a ppc executable */
static void sigterm_handler(int sig);				/* SIGTERM handler */
#endif /* __i386__ */

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
  iterate_printers(list_device_callback, NULL);
}


/*
 * 'print_device()' - Print a file to a USB device.
 */

int					/* O - Exit status */
print_device(const char *uri,		/* I - Device URI */
             const char *hostname,	/* I - Hostname/manufacturer */
             const char *resource,	/* I - Resource/modelname */
	     const char *options,	/* I - Device options/serial number */
	     int        fd,		/* I - File descriptor to print */
	     int        copies,		/* I - Copies to print */
	     int	argc,		/* I - Number of command-line arguments (6 or 7) */
	     char	*argv[])	/* I - Command-line arguments */
{
  printer_data_t  printer_data = { 0x0 };		/* Printer context */
  char		  serial[1024];				/* Serial number buffer */
  OSStatus	  status = noErr;			/* Function results */
  pthread_t	  thr;					/* Read thread */
  char		  buffer[2048];				/* Write buffer */
  int		  thread_created = 0;			/* Thread created? */
  int		  countdown = INITIAL_LOG_INTERVAL;	/* Logging interval */
  pthread_cond_t  *readCompleteConditionPtr = NULL;	/* Read complete condition */
  pthread_mutex_t *readMutexPtr = NULL;			/* Read mutex */
  CFStringRef	  driverBundlePath;			/* Class driver path */
  int             reqWait_create = 0;			/* RequestWait thread created? */
  pthread_t       reqWaitThread;			/* RequestWait thread */
  pthread_cond_t  *reqWaitCompCondPtr = NULL;		/* RequestWait complete condition */
  pthread_mutex_t *reqWaitMutexPtr = NULL;		/* RequestWait mutex */
  pthread_mutex_t *waitCloseMutexPtr = NULL;		/* wait close mutex */
  pthread_mutex_t *writeCompMutexPtr = NULL;		/* write complete mutex */

  setup_cfLanguage();
  parse_options(options, serial, &printer_data.location, &printer_data.waitEOF);

  if (resource[0] == '/')
    resource++;

  printer_data.uri = uri;
  
  printer_data.make   = cfstr_create_and_trim(hostname);
  printer_data.model  = cfstr_create_and_trim(resource);
  printer_data.serial = cfstr_create_and_trim(serial);

  fputs("STATE: +connecting-to-device\n", stderr);

  do {
    if (printer_data.printerObj != 0x0) {
      IOObjectRelease(printer_data.printerObj);			
      unload_classdriver(&printer_data.printerDriver);
      printer_data.printerObj = 0x0;
      printer_data.printerDriver = 0x0;
    }

    fprintf(stderr, "DEBUG: Looking for '%s %s'\n", hostname, resource);
    iterate_printers(find_device_callback, &printer_data);		

    fputs("DEBUG: Opening connection\n", stderr);

    driverBundlePath = NULL;
    status = registry_open(&printer_data, &driverBundlePath);
#if defined(__i386__)
    /*
     * If we were unable to load the class drivers for this printer it's probably because they're ppc-only.
     * In this case try to fork & exec this backend as a ppc executable so we can use them...
     */
    if (status == -2 /* kPMInvalidIOMContext */) {
      run_ppc_backend(argc, argv, fd);
      /* Never returns here */
    }
#endif /* __i386__ */
    if (status ==  -2) {
     /*
      * If we still were unable to load the class drivers for this printer log
      * the error and stop the queue...
      */

      if (driverBundlePath == NULL || !CFStringGetCString(driverBundlePath, buffer, sizeof(buffer), kCFStringEncodingUTF8))
        strlcpy(buffer, "USB class driver", sizeof(buffer));

      fputs("STATE: +apple-missing-usbclassdriver-error\n", stderr);
      fprintf(stderr, _("FATAL: Could not load %s\n"), buffer);

      if (driverBundlePath)
	CFRelease(driverBundlePath);

      return CUPS_BACKEND_STOP;
    }

    if (driverBundlePath)
      CFRelease(driverBundlePath);

    if (status != noErr) {
      sleep( PRINTER_POLLING_INTERVAL );
      countdown -= PRINTER_POLLING_INTERVAL;
      if ( countdown <= 0 ) {
	fprintf(stderr, _("INFO: Printer busy (status:0x%08x)\n"), (int)status);
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

  if (!fd) {
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

  if (status == noErr) {
    if (pthread_cond_init(&printer_data.readCompleteCondition, NULL) == 0)	
      readCompleteConditionPtr = &printer_data.readCompleteCondition;

    if (pthread_mutex_init(&printer_data.readMutex, NULL) == 0)
      readMutexPtr = &printer_data.readMutex;

    printer_data.done = 0;

    if (pthread_create(&thr, NULL, read_thread, &printer_data) == 0)
      thread_created = 1;

    if (thread_created == 0) 
      fputs(_("WARNING: Couldn't create read channel\n"), stderr);

    if (pthread_cond_init(&printer_data.reqWaitCompCond, NULL) == 0)	
      reqWaitCompCondPtr = &printer_data.reqWaitCompCond;

    if (pthread_mutex_init(&printer_data.reqWaitMutex, NULL) == 0)
      reqWaitMutexPtr = &printer_data.reqWaitMutex;

    printer_data.reqWaitDone = 0;
    printer_data.reqWqitFlag = 0;

    if (pthread_create(&reqWaitThread, NULL, reqestWait_thread, &printer_data) == 0)
      reqWait_create = 1;

    if (reqWait_create == 0) 
      fputs(_("WARNING: Couldn't create sidechannel thread!\n"), stderr);

    if (pthread_mutex_init(&printer_data.waitCloseMutex, NULL) == 0)
      waitCloseMutexPtr = &printer_data.waitCloseMutex;

    if (pthread_mutex_init(&printer_data.writeCompMutex, NULL) == 0)
      writeCompMutexPtr = &printer_data.writeCompMutex;
  }

  /*
   * The main thread sends the print file...
   */

  printer_data.writeDone = 0;
  printer_data.dataSize = 0;
  printer_data.dataOffset = 0;
  pthread_mutex_lock(writeCompMutexPtr);

  while (status == noErr && copies-- > 0) {
    UInt32		wbytes;			/* Number of bytes written */
    ssize_t		nbytes;			/* Number of bytes read */
    off_t		tbytes = 0;		/* Total number of bytes written */

    fputs(_("INFO: Sending data\n"), stderr);

    if (STDIN_FILENO != fd) {
      fputs("PAGE: 1 1", stderr);
      lseek( fd, 0, SEEK_SET );
    }

    while (status == noErr && (nbytes = read(fd, buffer, sizeof(buffer))) > 0) {
      char *bufptr = buffer;
      tbytes += nbytes;

      while (nbytes > 0 && status == noErr) {
	if (printer_data.writeDone) {
	  printer_data.dataSize = nbytes;
	  printer_data.dataOffset = bufptr - buffer;
	  memcpy(printer_data.dataBuffer, buffer, nbytes);

	  status = -1;
	  break;
	}

	wbytes = nbytes;
	status = (*(printer_data.printerDriver))->WritePipe( printer_data.printerDriver, (UInt8*)bufptr, &wbytes, 0 /* nbytes > wbytes? 0: feof(fp) */ );
	if (wbytes < 0 || noErr != status) {
	  OSStatus err = (*(printer_data.printerDriver))->Abort(printer_data.printerDriver);
	  fprintf(stderr, _("ERROR: %ld: Unable to send print file to printer (canceled:%ld)\n"), status, err);
	  break;
	}

	nbytes -= wbytes;
	bufptr += wbytes;
      }

      if (fd != 0 && status == noErr)
	fprintf(stderr, _("DEBUG: Sending print file, %lld bytes...\n"), (off_t)tbytes);
    }
  }

  printer_data.writeDone = 1;
  pthread_mutex_unlock(writeCompMutexPtr);

  if (thread_created) {
    /* Signal the read thread that we are done... */
    printer_data.done = 1;

    /* Give the read thread WAITEOF_DELAY seconds to complete all the data. If
     * we are not signaled in that time then force the thread to exit by setting
     * the waiteof to be false. Plese note that this relies on us using the timeout
     * class driver.
     */
    struct timespec sleepUntil = { time(NULL) + WAITEOF_DELAY, 0 };
    pthread_mutex_lock(&printer_data.readMutex);
    if (pthread_cond_timedwait(&printer_data.readCompleteCondition, &printer_data.readMutex, (const struct timespec *)&sleepUntil) != 0)
      printer_data.waitEOF = false;
    pthread_mutex_unlock(&printer_data.readMutex);
    pthread_join( thr,NULL);				/* wait for the child thread to return */
  }

  if (reqWait_create) {
    /* Signal the cupsSideChannelDoRequest wait thread that we are done... */
    printer_data.reqWaitDone = 1;

    /* 
     * Give the cupsSideChannelDoRequest wait thread WAITEOF_DELAY seconds to complete
     * all the data. If we are not signaled in that time then force the thread to exit
     * by setting the waiteof to be false. Plese note that this relies on us using the
     * timeout class driver.
     */
    struct timespec reqWaitSleepUntil = { time(NULL) + WAITEOF_DELAY, 0 };
    pthread_mutex_lock(&printer_data.reqWaitMutex);

    while (!printer_data.reqWqitFlag) {
      if (pthread_cond_timedwait(&printer_data.reqWaitCompCond,
                                 &printer_data.reqWaitMutex,
				 (const struct timespec *)&reqWaitSleepUntil) != 0) {
	printer_data.waitEOF = false;
	printer_data.reqWqitFlag = 1;
      }
    }
    pthread_mutex_unlock(&printer_data.reqWaitMutex);
    pthread_join(reqWaitThread,NULL);			/* wait for the child thread to return */
  }

  /* interface close wait mutex(for softreset) */
  pthread_mutex_lock(waitCloseMutexPtr);
  pthread_mutex_unlock(waitCloseMutexPtr);

  /*
   * Close the connection and input file and general clean up...
   */
  registry_close(&printer_data);

  if (STDIN_FILENO != fd)
    close(fd);

  if (readCompleteConditionPtr != NULL)
    pthread_cond_destroy(&printer_data.readCompleteCondition);

  if (readMutexPtr != NULL)
    pthread_mutex_destroy(&printer_data.readMutex);

  if (waitCloseMutexPtr != NULL)
    pthread_mutex_destroy(&printer_data.waitCloseMutex);

  if (writeCompMutexPtr != NULL)
    pthread_mutex_destroy(&printer_data.writeCompMutex);

  if (reqWaitCompCondPtr != NULL)
    pthread_cond_destroy(&printer_data.reqWaitCompCond);

  if (reqWaitMutexPtr != NULL)
    pthread_mutex_destroy(&printer_data.reqWaitMutex);

  if (printer_data.make != NULL)
    CFRelease(printer_data.make);

  if (printer_data.model != NULL)
    CFRelease(printer_data.model);

  if (printer_data.serial != NULL)
    CFRelease(printer_data.serial);

  if (printer_data.printerObj != 0x0)
    IOObjectRelease(printer_data.printerObj);

  return status;
}

#pragma mark -
/*
 * 'list_device_callback()' - list_device iterator callback.
 */

static Boolean list_device_callback(void *refcon, io_service_t obj)
{
  Boolean keepRunning = (obj != 0x0);

  if (keepRunning) {
    CFStringRef deviceIDString = NULL;
    UInt32 deviceLocation = 0;

    copy_devicestring(obj, &deviceIDString, &deviceLocation);
    if (deviceIDString != NULL) {
      CFStringRef make = NULL,  model = NULL, serial = NULL;
      char uristr[1024], makestr[1024], modelstr[1024], serialstr[1024];
      char optionsstr[1024], idstr[1024];

      copy_deviceinfo(deviceIDString, &make, &model, &serial);

      modelstr[0] = '/';

      CFStringGetCString(deviceIDString, idstr, sizeof(idstr),
                         kCFStringEncodingUTF8);

      if (make)
        CFStringGetCString(make, makestr, sizeof(makestr),
	                   kCFStringEncodingUTF8);
      else
        strcpy(makestr, "Unknown");

      if (model)
	CFStringGetCString(model, &modelstr[1], sizeof(modelstr)-1,
      			   kCFStringEncodingUTF8);
      else
        strcpy(modelstr + 1, "Printer");

     /*
      * Fix common HP 1284 bug...
      */

      if (!strcasecmp(makestr, "Hewlett-Packard"))
        strcpy(makestr, "HP");

      if (!strncasecmp(modelstr + 1, "hp ", 3))
        _cups_strcpy(modelstr + 1, modelstr + 4);

      optionsstr[0] = '\0';
      if (serial != NULL)
      {
        CFStringGetCString(serial, serialstr, sizeof(serialstr), kCFStringEncodingUTF8);
	snprintf(optionsstr, sizeof(optionsstr), "?serial=%s", serialstr);
      }
      else if (deviceLocation != 0)
      {
	snprintf(optionsstr, sizeof(optionsstr), "?location=%lx", deviceLocation);
      }

      httpAssembleURI(HTTP_URI_CODING_ALL, uristr, sizeof(uristr), "usb", NULL, makestr, 0, modelstr);
      strncat(uristr, optionsstr, sizeof(uristr));

      printf("direct %s \"%s %s\" \"%s %s USB\" \"%s\"\n", uristr, makestr,
             &modelstr[1], makestr, &modelstr[1], idstr);

      release_deviceinfo(&make, &model, &serial);
      CFRelease(deviceIDString);
    }
  }

  return keepRunning;
}


/*
 * 'find_device_callback()' - print_device iterator callback.
 */

static Boolean find_device_callback(void *refcon, io_service_t obj)
{
  Boolean keepLooking = true;
  printer_data_t *userData = (printer_data_t *)refcon;

  if (obj != 0x0) {
    CFStringRef idString = NULL;
    UInt32 location = -1;

    copy_devicestring(obj, &idString, &location);
    if (idString != NULL) {
      CFStringRef make = NULL,  model = NULL, serial = NULL;

      copy_deviceinfo(idString, &make, &model, &serial);
      if (CFStringCompare(make, userData->make, kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
	if (CFStringCompare(model, userData->model, kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
	  if (userData->serial != NULL) {
	    if (serial != NULL && CFStringCompare(serial, userData->serial, kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
	      IOObjectRetain(obj);
	      userData->printerObj = obj;
	      keepLooking = false;
	    }
	  }
	  else {
	    if (userData->printerObj != 0) {
	      IOObjectRetain(userData->printerObj);
	    }
	    userData->printerObj = obj;
	    IOObjectRetain(obj);

	    if (userData->location == 0 || userData->location == location) {
	      keepLooking = false;
	    }
	  }
	}
      }

      release_deviceinfo(&make, &model, &serial);
      CFRelease(idString);
    }
  }
  else {		
    keepLooking = (userData->printerObj == 0);
    if (obj == 0x0 && keepLooking) {
      CFRunLoopTimerContext context = { 0, userData, NULL, NULL, NULL };
      CFRunLoopTimerRef timer = CFRunLoopTimerCreate(NULL, CFAbsoluteTimeGetCurrent() + 1.0, 10, 0x0, 0x0, statusTimerCallback, &context);
      if (timer != NULL) {
	CFRunLoopAddTimer(CFRunLoopGetCurrent(), timer, kCFRunLoopDefaultMode);
	userData->statusTimer = timer;
      }
    }
  }
  
  if (!keepLooking && userData->statusTimer != NULL) {
    fputs("STATE: -offline-error\n", stderr);
    fputs(_("INFO: Printer is now on-line.\n"), stderr);
    CFRunLoopRemoveTimer(CFRunLoopGetCurrent(), userData->statusTimer, kCFRunLoopDefaultMode);
    CFRelease(userData->statusTimer);
    userData->statusTimer = NULL;
  }

  return keepLooking;
}

static void statusTimerCallback (CFRunLoopTimerRef timer, void *info)
{
  fputs("STATE: +offline-error\n", stderr);
  fputs(_("INFO: Printer is currently off-line.\n"), stderr);
}

#pragma mark -
/*
 * 'iterate_printers()' - iterate over all the printers.
 */

static void iterate_printers(iterator_callback_t callBack, void *userdata)
{
  mach_port_t	masterPort = 0x0;
  kern_return_t kr = IOMasterPort (bootstrap_port, &masterPort);

  if (kr == kIOReturnSuccess && masterPort != 0x0) {
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

    kr = IOServiceAddMatchingNotification(addNotification, kIOMatchedNotification, usbPrinterMatchDictionary, &device_added, &reference, &addIterator);
    if (addIterator != 0x0) {
      device_added (&reference, addIterator);

      if (reference.keepRunning) {
	CFRunLoopAddSource(CFRunLoopGetCurrent(), IONotificationPortGetRunLoopSource(addNotification), kCFRunLoopDefaultMode);
	CFRunLoopRun();
      }
      IOObjectRelease(addIterator);
    }
    mach_port_deallocate(mach_task_self(), masterPort);
  }
}


/*
 * 'device_added()' - device added notifier.
 */

static void device_added(void *userdata, io_iterator_t iterator)
{	
  iterator_reference_t *reference = userdata;

  io_service_t obj;
  while (reference->keepRunning && (obj = IOIteratorNext(iterator)) != 0x0) {
    if (reference->callback != NULL) {
      reference->keepRunning = reference->callback(reference->userdata, obj);
    }
    IOObjectRelease(obj);
  }

  /* One last call to the call back now that we are not longer have printers left to iterate...
   */
  if (reference->keepRunning)
    reference->keepRunning = reference->callback(reference->userdata, 0x0);

  if (!reference->keepRunning) {
    CFRunLoopStop(CFRunLoopGetCurrent());
  }
}


#pragma mark -
/*
 * 'copy_deviceinfo()' - Copy strings from the 1284 device ID.
 */

static void copy_deviceinfo(CFStringRef deviceIDString, CFStringRef *make, CFStringRef *model, CFStringRef *serial)
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

static void release_deviceinfo(CFStringRef *make, CFStringRef *model, CFStringRef *serial)
{
  if (make != NULL && *make != NULL) {
    CFRelease(*make);
    *make = NULL;
  }

  if (model != NULL && *model != NULL) {
    CFRelease(*model);
    *model = NULL;
  }

  if (serial != NULL && *serial != NULL) {
    CFRelease(*serial);
    *serial = NULL;
  }
}


#pragma mark -
/*
 * 'load_classdriver()' - Load a classdriver.
 */

static kern_return_t load_classdriver(CFStringRef driverPath, printer_interface_t intf, classdriver_context_t ***printerDriver)
{
  kern_return_t kr = kUSBPrinterClassDeviceNotOpen;
  classdriver_context_t **driver = NULL;
  CFStringRef bundle = (driverPath == NULL ? kUSBGenericTOPrinterClassDriver : driverPath);

  if ( NULL != bundle ) {
    CFURLRef url = CFURLCreateWithFileSystemPath(NULL, bundle, kCFURLPOSIXPathStyle, true);
    CFPlugInRef plugin = (url != NULL ? CFPlugInCreate(NULL, url) : NULL);

    if (url != NULL) 
      CFRelease(url);

    if (plugin != NULL) {
      CFArrayRef factories = CFPlugInFindFactoriesForPlugInTypeInPlugIn(kUSBPrinterClassTypeID, plugin);
      if (factories != NULL && CFArrayGetCount(factories) > 0)  {
	CFUUIDRef factoryID = CFArrayGetValueAtIndex(factories, 0);
	IUnknownVTbl **iunknown = CFPlugInInstanceCreate(NULL, factoryID, kUSBPrinterClassTypeID);
	if (NULL != iunknown) {
	  kr = (*iunknown)->QueryInterface(iunknown, CFUUIDGetUUIDBytes(kUSBPrinterClassInterfaceID), (LPVOID *)&driver);
	  if (kr == kIOReturnSuccess && driver != NULL) {					
	    classdriver_context_t **genericDriver = NULL;
	    if (driverPath != NULL && CFStringCompare(driverPath, kUSBGenericTOPrinterClassDriver, 0) != kCFCompareEqualTo) {
	      kr = load_classdriver(NULL, intf, &genericDriver);
	    }

	    if (kr == kIOReturnSuccess) {
	      (*driver)->interface = intf;
	      (*driver)->Initialize(driver, genericDriver);

	      (*driver)->plugin = plugin;
	      (*driver)->interface = intf;
	      *printerDriver = driver;
	    }
	  }
	  (*iunknown)->Release(iunknown);
	}
	CFRelease(factories);
      }
    }
  }

#ifdef DEBUG
  char bundlestr[1024];
  CFStringGetCString(bundle, bundlestr, sizeof(bundlestr), kCFStringEncodingUTF8);
  fprintf(stderr, "DEBUG: load_classdriver(%s) (kr:0x%08x)\n", bundlestr, (int)kr);
#endif /* DEBUG */

  return kr;
}


/*
 * 'unload_classdriver()' - Unload a classdriver.
 */

static kern_return_t unload_classdriver(classdriver_context_t ***classDriver)
{
  if (*classDriver != NULL) {
    (**classDriver)->Release(*classDriver);
    *classDriver = NULL;
  }

  return kIOReturnSuccess;
}


/*
 * 'load_printerdriver()' - Load a vendor's (or generic) classdriver.
 *
 * If driverBundlePath is not NULL on return it is the callers responsbility to release it!
 */

static kern_return_t load_printerdriver(printer_data_t *printer, CFStringRef *driverBundlePath)
{
  IOCFPlugInInterface	**iodev = NULL;
  SInt32		score;
  kern_return_t		kr;
  printer_interface_t	intf;
  HRESULT		res;

  kr = IOCreatePlugInInterfaceForService(printer->printerObj, kIOUSBInterfaceUserClientTypeID, kIOCFPlugInInterfaceID, &iodev, &score);
  if (kr == kIOReturnSuccess)
  {
    if ((res = (*iodev)->QueryInterface(iodev, USB_INTERFACE_KIND, (LPVOID *) &intf)) == noErr)
    {
      *driverBundlePath = IORegistryEntryCreateCFProperty(printer->printerObj, kUSBClassDriverProperty, NULL, kNilOptions);

      kr = load_classdriver(*driverBundlePath, intf, &printer->printerDriver);

      if (kr != kIOReturnSuccess)
	(*intf)->Release(intf);
    }
    IODestroyPlugInInterface(iodev);
  }
  return kr;
}


/*
 * 'registry_open()' - Open a connection to the printer.
 */

static kern_return_t registry_open(printer_data_t *printer, CFStringRef *driverBundlePath)
{
  printer->directionalFlag = 0;

  kern_return_t kr = load_printerdriver(printer, driverBundlePath);
  if (kr != kIOReturnSuccess) {
    kr = -2;
  }

  if (printer->printerDriver != NULL) {
    kr = (*(printer->printerDriver))->Open(printer->printerDriver, printer->location, kUSBPrintingProtocolBidirectional);
    if (kr != kIOReturnSuccess || (*(printer->printerDriver))->interface == NULL) {
      kr = (*(printer->printerDriver))->Open(printer->printerDriver, printer->location, kUSBPrintingProtocolUnidirectional);
      if (kr == kIOReturnSuccess) {
	if ((*(printer->printerDriver))->interface == NULL) {
	  (*(printer->printerDriver))->Close(printer->printerDriver);
	  kr = -1;
	}
      }
    } else {
      printer->directionalFlag = 1;
    }
  }

  if (kr != kIOReturnSuccess) {
    unload_classdriver(&printer->printerDriver);
  }

  return kr;
}


/*
 * 'registry_close()' - Close the connection to the printer.
 */

static kern_return_t registry_close(printer_data_t *printer)
{
  if (printer->printerDriver != NULL) {
    (*(printer->printerDriver))->Close(printer->printerDriver);
  }
  unload_classdriver(&printer->printerDriver);
  return kIOReturnSuccess;
}


/*
 * 'copy_deviceid()' - Copy the 1284 device id string.
 */

static OSStatus copy_deviceid(classdriver_context_t **printer, CFStringRef *deviceID)
{
  CFStringRef devID = NULL,

  deviceMake = NULL,
  deviceModel = NULL,
  deviceSerial = NULL;

  OSStatus err = (*printer)->GetDeviceID(printer, &devID, DEFAULT_TIMEOUT);

  copy_deviceinfo(devID, &deviceMake, &deviceModel, &deviceSerial);

  if (deviceMake == NULL || deviceModel == NULL || deviceSerial == NULL) {
    IOUSBDeviceDescriptor	desc;
    iodevice_request_t		request;

    request.requestType = USBmakebmRequestType( kUSBIn,  kUSBStandard, kUSBDevice );
    request.request = kUSBRqGetDescriptor;
    request.value = (kUSBDeviceDesc << 8) | 0;
    request.index = 0;
    request.length = sizeof(desc);
    request.buffer = &desc;
    err = (*printer)->DeviceRequest(printer, &request, DEFAULT_TIMEOUT);
    if (err == kIOReturnSuccess) {
      CFMutableStringRef newDevID = CFStringCreateMutable(NULL, 0);

      if (deviceMake == NULL) {
	CFStringRef data = NULL;
	err = (*printer)->GetString(printer, desc.iManufacturer, kUSBLanguageEnglish, DEFAULT_TIMEOUT, &data);
	if (data != NULL) {
	  CFStringAppendFormat(newDevID, NULL, CFSTR("MFG:%@;"), data);
	  CFRelease(data);
	}
      }

      if (deviceModel == NULL) {
	CFStringRef data = NULL;
	err = (*printer)->GetString(printer, desc.iProduct, kUSBLanguageEnglish, DEFAULT_TIMEOUT, &data);
	if (data != NULL) {
	  CFStringAppendFormat(newDevID, NULL, CFSTR("MDL:%@;"), data);
	  CFRelease(data);
	}
      }

      if (deviceSerial == NULL && desc.iSerialNumber != 0) {
	CFStringRef data = NULL;
	err = (*printer)->GetString(printer, desc.iSerialNumber, kUSBLanguageEnglish, DEFAULT_TIMEOUT, &data);
	if (data != NULL) {
	  CFStringAppendFormat(newDevID, NULL, CFSTR("SERN:%@;"), data);
	  CFRelease(data);
	}
      }

      if (devID != NULL) {
	CFStringAppend(newDevID, devID);
	CFRelease(devID);
      }

      *deviceID = newDevID;
    }
  }
  else {
    *deviceID = devID;
  }
  release_deviceinfo(&deviceMake, &deviceModel, &deviceSerial);

  return err;
}


/*
 * 'copy_devicestring()' - Copy the 1284 device id string.
 */

static void copy_devicestring(io_service_t usbInterface, CFStringRef *deviceID, UInt32 *deviceLocation)
{
  IOCFPlugInInterface	**iodev = NULL;
  SInt32		score;
  kern_return_t		kr;
  printer_interface_t	intf;
  HRESULT		res;
  classdriver_context_t	**klassDriver = NULL;
  CFStringRef		driverBundlePath;

  kr = IOCreatePlugInInterfaceForService(usbInterface, kIOUSBInterfaceUserClientTypeID, 
						       kIOCFPlugInInterfaceID, &iodev, &score);
  if (kr == kIOReturnSuccess)
  {
    if ((res = (*iodev)->QueryInterface(iodev, USB_INTERFACE_KIND, (LPVOID *) &intf)) == noErr)
    {
      /* ignore the result for location id... */
      (void)(*intf)->GetLocationID(intf, deviceLocation);

      driverBundlePath = IORegistryEntryCreateCFProperty( usbInterface, kUSBClassDriverProperty, NULL, kNilOptions );

      kr = load_classdriver(driverBundlePath, intf, &klassDriver);

      if (kr != kIOReturnSuccess && driverBundlePath != NULL)
	kr = load_classdriver(NULL, intf, &klassDriver);

      if (kr == kIOReturnSuccess && klassDriver != NULL)			
	  kr = copy_deviceid(klassDriver, deviceID);						

      unload_classdriver(&klassDriver);

      if (driverBundlePath != NULL)
	CFRelease(driverBundlePath);

      /* (*intf)->Release(intf); */
    }		
    IODestroyPlugInInterface(iodev);
  }
}


#pragma mark -
/*
 * 'copy_value_for_key()' - Copy value string associated with a key.
 */

static CFStringRef copy_value_for_key(CFStringRef deviceID, CFStringRef *keys)
{
  CFStringRef	value = NULL;
  CFArrayRef	kvPairs = deviceID != NULL ? CFStringCreateArrayBySeparatingStrings(NULL, deviceID, CFSTR(";")) : NULL;
  CFIndex	max = kvPairs != NULL ? CFArrayGetCount(kvPairs) : 0;
  CFIndex	idx = 0;

  while (idx < max && value == NULL) {
    CFStringRef kvpair = CFArrayGetValueAtIndex(kvPairs, idx);
    CFIndex idxx = 0;
    while (keys[idxx] != NULL && value == NULL) {			
      CFRange range = CFStringFind(kvpair, keys[idxx], kCFCompareCaseInsensitive);
      if (range.length != -1) {
	if (range.location != 0) {
	  CFMutableStringRef theString = CFStringCreateMutableCopy(NULL, 0, kvpair);
	  CFStringTrimWhitespace(theString);
	  range = CFStringFind(theString, keys[idxx], kCFCompareCaseInsensitive);
	  if (range.location == 0) {
	    value = CFStringCreateWithSubstring(NULL, theString, CFRangeMake(range.length, CFStringGetLength(theString) - range.length));
	  }
	  CFRelease(theString);
	}
	else {
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
 * 'cfstr_create_and_trim()' - Create a CFString from a c-string and 
 *			       trim it's whitespace characters.
 */

CFStringRef cfstr_create_and_trim(const char *cstr)
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
 * 'parse_options()' - Parse uri options.
 */

static void parse_options(const char *options, char *serial, UInt32 *location, Boolean *waitEOF)
{
  char	*serialnumber;		/* ?serial=<serial> or ?location=<location> */
  char	optionName[255],	/* Name of option */
	value[255],		/* Value of option */
	*ptr;			/* Pointer into name or value */

  if (serial)
    *serial = '\0';
  if (location)
    *location = 0;

  if (!options)
    return;

  serialnumber = NULL;

  while (*options != '\0') {
    /* Get the name... */
    for (ptr = optionName; *options && *options != '=' && *options != '+'; )
      *ptr++ = *options++;

    *ptr = '\0';
    value[0] = '\0';

    if (*options == '=') {
      /* Get the value... */
      options ++;

      for (ptr = value; *options && *options != '+';)
	*ptr++ = *options++;

      *ptr = '\0';

      if (*options == '+')
	options ++;
    }
    else if (*options == '+') {
      options ++;
    }

    /*
     * Process the option...
     */
    if (strcasecmp(optionName, "waiteof") == 0) {
      if (strcasecmp(value, "on") == 0 ||
	  strcasecmp(value, "yes") == 0 ||
	  strcasecmp(value, "true") == 0) {
	*waitEOF = true;
      }
      else if (strcasecmp(value, "off")   == 0 ||
	       strcasecmp(value, "no")    == 0 ||
	       strcasecmp(value, "false") == 0) {
	*waitEOF = false;
      }
      else {
	fprintf(stderr, _("WARNING: Boolean expected for waiteof option \"%s\"\n"), value);
      }
    }
    else if (strcasecmp(optionName, "serial") == 0) {
      strcpy(serial, value);
      serialnumber = serial;
    }
    else if (strcasecmp(optionName, "location") == 0 && location) {
      *location = strtol(value, NULL, 16);
    }
  }

  return;
}


/*!
 * @function	setup_cfLanguage
 * @abstract	Convert the contents of the CUPS 'LANG' environment
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

  requestedLang = getenv("LANG");
  if (requestedLang != NULL) {
    lang[0] = CFStringCreateWithCString(kCFAllocatorDefault, requestedLang, kCFStringEncodingUTF8);
    langArray = CFArrayCreate(kCFAllocatorDefault, (const void **)lang, sizeof(lang) / sizeof(lang[0]), &kCFTypeArrayCallBacks);

    CFPreferencesSetAppValue(CFSTR("AppleLanguages"), langArray, kCFPreferencesCurrentApplication);
    DEBUG_printf((stderr, "DEBUG: usb: AppleLanguages = \"%s\"\n", requestedLang));

    CFRelease(lang[0]);
    CFRelease(langArray);
  } else {
    fputs("DEBUG: usb: LANG environment variable missing.\n", stderr);
  }
}

#pragma mark -
#if defined(__i386__)
/*!
 * @function	run_ppc_backend
 *
 * @abstract	Starts child backend process running as a ppc executable.
 *
 * @result	Never returns; always calls exit().
 *
 * @discussion	
 */
static void run_ppc_backend(int argc, char *argv[], int fd)
{
  int	i;
  int	exitstatus = 0;
  int	childstatus;
  pid_t	waitpid_status;
  char	*my_argv[32];
  char	*usb_ppc_status;

  /*
   * If we're running as i386 and couldn't load the class driver (because they'it's
   * ppc-only) then try to re-exec ourselves in ppc mode to try again. If we don't have
   * a ppc architecture we may be running i386 again so guard against this by setting
   * and testing an environment variable...
   */
  usb_ppc_status = getenv("USB_PPC_STATUS");

  if (usb_ppc_status == NULL) {
    /* Catch SIGTERM if we are _not_ printing data from
     * stdin (otherwise you can't cancel raw jobs...)
     */

    if (fd != 0) {
#ifdef HAVE_SIGSET /* Use System V signals over POSIX to avoid bugs */
      sigset(SIGTERM, sigterm_handler);
#elif defined(HAVE_SIGACTION)
      struct sigaction action;	/* Actions for POSIX signals */
      memset(&action, 0, sizeof(action));
      sigaddset(&action.sa_mask, SIGTERM);
      action.sa_handler = sigterm_handler;
      sigaction(SIGTERM, &action, NULL);
#else
      signal(SIGTERM, sigterm_handler);
#endif /* HAVE_SIGSET */
    }

    if ((child_pid = fork()) == 0) {
      /* Child comes here. */
      setenv("USB_PPC_STATUS", "1", false);

      /* Tell the kernel we want the next exec call to favor the ppc architecture... */
      int mib[] = { CTL_KERN, KERN_AFFINITY, 1, 1 };
      int namelen = 4;
      sysctl(mib, namelen, NULL, NULL, NULL, 0);

      /* Set up the arguments and call exec... */
      for (i = 0; i < argc && i < (sizeof(my_argv)/sizeof(my_argv[0])) - 1; i++)
	my_argv[i] = argv[i];

      my_argv[i] = NULL;

      execv("/usr/libexec/cups/backend/usb", my_argv);

      fprintf(stderr, "DEBUG: execv: %s\n", strerror(errno));
      exitstatus = errno;
    }
    else if (child_pid > 0) {
      /* Parent comes here. 
       *
       * Close the fds we won't be using then wait for the child backend to exit.
       */
      close(fd);
      close(1);

      fprintf(stderr, "DEBUG: Started usb(ppc) backend (PID %d)\n", (int)child_pid);

      while ((waitpid_status = waitpid(child_pid, &childstatus, 0)) == (pid_t)-1 && errno == EINTR)
        usleep(1000);

      if (WIFSIGNALED(childstatus)) {
	exitstatus = WTERMSIG(childstatus);
	fprintf(stderr, "DEBUG: usb(ppc) backend %d crashed on signal %d!\n", child_pid, exitstatus);
      }
      else {
	if ((exitstatus = WEXITSTATUS(childstatus)) != 0)
	  fprintf(stderr, "DEBUG: usb(ppc) backend %d stopped with status %d!\n", child_pid, exitstatus);
	else
	  fprintf(stderr, "DEBUG: PID %d exited with no errors\n", child_pid);
      }
    }
    else {
      /* fork() error */
      fprintf(stderr, "DEBUG: fork: %s\n", strerror(errno));
      exitstatus = errno;
    }
  }
  else {
    fputs("DEBUG: usb child running i386 again\n", stderr);
    exitstatus = ENOENT;
  }

  exit(exitstatus);
}

/*
 * 'sigterm_handler()' - SIGTERM handler.
 */

static void sigterm_handler(int sig)
{
  /* If we started a child process pass the signal on to it...
   */
  if (child_pid)
    kill(child_pid, sig);

  exit(1);
}

#endif /* __i386__ */


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

static void parse_pserror (char *sockBuffer, int len)
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
  while (pLineEnd != NULL) {
    *pLineEnd++ = '\0';

    pCommentBegin = strstr(gErrorBuffer,"%%[");
    pCommentEnd = strstr(gErrorBuffer, "]%%");
    if (pCommentBegin != gErrorBuffer && pCommentEnd != NULL) {
      pCommentEnd += 3;            /* Skip past "]%%" */
      *pCommentEnd = '\0';         /* There's always room for the nul */

      if (strncasecmp(pCommentBegin, "%%[ Error:", 10) == 0)
	logLevel = "DEBUG";
      else if (strncasecmp(pCommentBegin, "%%[ Flushing", 12) == 0)
	logLevel = "DEBUG";
      else
	logLevel = "INFO";

      if ((logstrlen = snprintf(logstr, sizeof(logstr), "%s: %s\n", logLevel, pCommentBegin)) >= sizeof(logstr)) {
	/* If the string was trucnated make sure it has a linefeed before the nul */
	logstrlen = sizeof(logstr) - 1;
	logstr[logstrlen - 1] = '\n';
      }
      write(STDERR_FILENO, logstr, logstrlen);
    }

    /* move everything over... */
    strcpy(gErrorBuffer, pLineEnd);
    gErrorBufferPtr = gErrorBuffer;
    pLineEnd = (char *)next_line((const char *)gErrorBuffer);
  }
}
#endif /* PARSE_PS_ERRORS */


/*
 * 'read_thread()' - A thread to read the backchannel data.
 */

static void *read_thread(void *reference)
{
  /* post a read to the device and write results to stdout
   * the final pending read will be Aborted in the main thread
   */
  UInt8				readbuffer[512];
  UInt32			rbytes;
  kern_return_t			readstatus;
  printer_data_t		*userData = (printer_data_t *)reference;
  classdriver_context_t	**classdriver = userData->printerDriver;
  struct mach_timebase_info	timeBaseInfo;
  uint64_t			start,
				delay;

  /* Calculate what 250 milliSeconds are in mach absolute time...
   */
  mach_timebase_info(&timeBaseInfo);
  delay = ((uint64_t)250000000 * (uint64_t)timeBaseInfo.denom) / (uint64_t)timeBaseInfo.numer;

  do {
    /* Remember when we started so we can throttle the loop after the read call...
     */
    start = mach_absolute_time();

    rbytes = sizeof(readbuffer);
    readstatus = (*classdriver)->ReadPipe( classdriver, readbuffer, &rbytes );
    if ( kIOReturnSuccess == readstatus && rbytes > 0 ) {

      cupsBackChannelWrite((char*)readbuffer, rbytes, 1.0);

      /* cntrl-d is echoed by the printer.
       * NOTES: 
       *   Xerox Phaser 6250D doesn't echo the cntrl-d.
       *   Xerox Phaser 6250D doesn't always send the product query.
       */
      if (userData->waitEOF && readbuffer[rbytes-1] == 0x4)
	break;
#ifdef PARSE_PS_ERRORS
      parse_pserror(readbuffer, rbytes);
#endif
    }

    /* Make sure this loop executes no more than once every 250 miliseconds...
     */
    if ((readstatus != kIOReturnSuccess || rbytes == 0) && (userData->waitEOF || !userData->done))
      mach_wait_until(start + delay);

  } while ( userData->waitEOF || !userData->done );	/* Abort from main thread tests error here */

  /* Let the other thread (main thread) know that we have completed the read thread...
   */
  pthread_mutex_lock(&userData->readMutex);
  pthread_cond_signal(&userData->readCompleteCondition);
  pthread_mutex_unlock(&userData->readMutex);

  return NULL;
}

/*
 * 'reqestWait_thread()' - A thread cupsSideChannelDoRequest wait.
 */
static void *reqestWait_thread(void *reference) {
  printer_data_t *userData = (printer_data_t *)reference;
  int datalen;
  cups_sc_command_t command;
  cups_sc_status_t status;
  uint64_t start, delay;
  struct mach_timebase_info timeBaseInfo;
  char data[2048];

  /*
   * Calculate what 100 milliSeconds are in mach absolute time...
   */
  mach_timebase_info(&timeBaseInfo);
  delay = ((uint64_t)100000000 * (uint64_t)timeBaseInfo.denom) / (uint64_t)timeBaseInfo.numer;

  /* interface close wait mutex lock. */
  pthread_mutex_lock(&(userData->waitCloseMutex));

  do {
    /* 
     * Remember when we started so we can throttle the loop after the cupsSideChannelDoRequest call...
     */
    start = mach_absolute_time();

    /* Poll for a command... */
    command=0;
    datalen = sizeof(data);
    bzero(data, sizeof(data));

    if (!cupsSideChannelRead(&command, &status, data, &datalen, 0.0)) {
      datalen = sizeof(data);

      switch (command) {
	case CUPS_SC_CMD_SOFT_RESET:
	    /* do a soft reset */
	    usbSoftReset(userData, &status);
	    datalen = 0;
	    userData->reqWaitDone = 1;
	    break;
	case CUPS_SC_CMD_DRAIN_OUTPUT:
	    /* drain all pending output */
	    usbDrainOutput(userData, &status);
	    datalen = 0;
	    break;
	case CUPS_SC_CMD_GET_BIDI:
	    /* return whether the connection is bidirectional */
	    usbGetBidirectional(userData, &status, data, &datalen);
	    break;
	case CUPS_SC_CMD_GET_DEVICE_ID:
	    /* return the IEEE-1284 device ID */
	    usbGetDeviceID(userData, &status, data, &datalen);
	    break;
	case CUPS_SC_CMD_GET_STATE:
	    /* return the device state */
	    usbGetDevState(userData, &status, data, &datalen);
	    break;
	default:
	    status  = CUPS_SC_STATUS_NOT_IMPLEMENTED;
	    datalen = 0;
	    break;
      }

      if (userData->writeDone) {
        status = CUPS_SC_STATUS_NONE;
      }

      /* Send a response... */
      cupsSideChannelWrite(command, status, data, datalen, 1.0);
    }

    /*
     * Make sure this loop executes no more than once every 500 miliseconds...
     */
    if ((userData->waitEOF) || (!userData->reqWaitDone)) {
      mach_wait_until(start + delay);
    }
  } while(!userData->reqWaitDone);

  sleep(1);
  pthread_mutex_lock(&userData->reqWaitMutex);
  userData->reqWqitFlag = 1;
  pthread_cond_signal(&userData->reqWaitCompCond);
  pthread_mutex_unlock(&userData->reqWaitMutex);

  /* interface close wait mutex unlock. */
  pthread_mutex_unlock(&(userData->waitCloseMutex));

  return NULL;
}

#pragma mark -
/*
 * 'usbSoftReset'
 */
static void usbSoftReset(printer_data_t *userData, cups_sc_status_t *status) {
  OSStatus err;

  /* write stop. */
  userData->writeDone = 1;

  /* Abort (print_device()-WritePipe kIOReturnAborted return) */
  if (userData->printerDriver != NULL)
    err = (*(userData->printerDriver))->Abort(userData->printerDriver);

  /* print_device() WritePipe_Loop break wait. */
  pthread_mutex_lock(&(userData->writeCompMutex));
  pthread_mutex_unlock(&(userData->writeCompMutex));

  /* SoftReset */
  if (userData->printerDriver != NULL)
    (*(userData->printerDriver))->SoftReset(userData->printerDriver, 0);

  if (status != NULL)
    *status  = CUPS_SC_STATUS_OK;
}

/*
 * 'usbDrainOutput'
 */
static void usbDrainOutput(printer_data_t *userData, cups_sc_status_t *status) {
  OSStatus osSts = noErr;	/* Function results */
  OSStatus err = noErr;
  UInt32  wbytes;			/* Number of bytes written */
  ssize_t nbytes;			/* Number of bytes read */
  char *bufptr;

  bufptr = userData->dataBuffer+userData->dataOffset;
  nbytes = userData->dataSize;

  while((nbytes > 0) && (osSts == noErr)) {
    wbytes = nbytes;
    osSts = (*(userData->printerDriver))->WritePipe(userData->printerDriver, (UInt8*)bufptr, &wbytes, 0);

    if (wbytes < 0 || noErr != osSts) {
      if (osSts != kIOReturnAborted) {
	err = (*(userData->printerDriver))->Abort(userData->printerDriver);
	break;
      }
    }

    nbytes -= wbytes;
    bufptr += wbytes;
  }

  if (status != NULL) {
    if ((osSts != noErr) || (err != noErr)) {
      *status  = CUPS_SC_STATUS_IO_ERROR;
    } else {
      *status  = CUPS_SC_STATUS_OK;
    }
  }
}

/*
 * 'usbGetBidirectional'
 */
static void usbGetBidirectional(printer_data_t *userData, cups_sc_status_t *status, char *data, int *datalen) {
  *data = userData->directionalFlag;
  *datalen = 1;

  if (status != NULL)
    *status = CUPS_SC_STATUS_OK;
}

/*
 * 'usbGetDeviceID'
 */
static void usbGetDeviceID(printer_data_t *userData, cups_sc_status_t *status, char *data, int *datalen) {
  UInt32 deviceLocation = 0;
  CFStringRef deviceIDString = NULL;

  /* GetDeviceID */
  copy_devicestring(userData->printerObj, &deviceIDString, &deviceLocation);
  CFStringGetCString(deviceIDString, data, *datalen, kCFStringEncodingUTF8);
  *datalen = strlen(data);

  if (status != NULL) {
    *status  = CUPS_SC_STATUS_OK;
  }
}

/*
 * 'usbGetDevState'
 */
static void usbGetDevState(printer_data_t *userData, cups_sc_status_t *status, char *data, int *datalen) {
  *data = CUPS_SC_STATE_ONLINE;
  *datalen = 1;

  if (status != NULL) {
    *status = CUPS_SC_STATUS_OK;
  }
}

/*
 * End of "$Id$".
 */
