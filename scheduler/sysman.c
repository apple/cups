/*
 * "$Id$"
 *
 *   System management definitions for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007-2008 by Apple Inc.
 *   Copyright 2006 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
 *   cupsdCleanDirty()               - Write dirty config and state files.
 *   cupsdMarkDirty()                - Mark config or state files as needing a
 *                                     write.
 *   cupsdSetBusyState()             - Let the system know when we are busy
 *                                     doing something.
 *   cupsdStartSystemMonitor()       - Start monitoring for system change.
 *   cupsdStopSystemMonitor()        - Stop monitoring for system change.
 *   sysEventThreadEntry()           - A thread to receive power and computer
 *                                     name change notifications.
 *   sysEventPowerNotifier()         - Handle power notification events.
 *   sysEventConfigurationNotifier() - Computer name changed notification
 *                                     callback.
 *   sysEventTimerNotifier()         - Handle delayed event notifications.
 *   sysUpdate()                     - Update the current system state.
 */


/*
 * Include necessary headers...
 */

#include "cupsd.h"


/*
 * The system management functions cover disk and power management which
 * are primarily used on portable computers.
 *
 * Disk management involves delaying the write of certain configuration
 * and state files to minimize the number of times the disk has to spin
 * up.
 *
 * Power management support is currently only implemented on MacOS X, but
 * essentially we use four functions to let the OS know when it is OK to
 * put the system to idle sleep, typically when we are not in the middle of
 * printing a job.
 *
 * Once put to sleep, we invalidate all remote printers since it is common
 * to wake up in a new location/on a new wireless network.
 */


/*
 * 'cupsdCleanDirty()' - Write dirty config and state files.
 */

void
cupsdCleanDirty(void)
{
  if (DirtyFiles & CUPSD_DIRTY_PRINTERS)
    cupsdSaveAllPrinters();

  if (DirtyFiles & CUPSD_DIRTY_CLASSES)
    cupsdSaveAllClasses();

  if (DirtyFiles & CUPSD_DIRTY_REMOTE)
    cupsdSaveRemoteCache();

  if (DirtyFiles & CUPSD_DIRTY_PRINTCAP)
    cupsdWritePrintcap();

  if (DirtyFiles & CUPSD_DIRTY_JOBS)
  {
    cupsd_job_t	*job;			/* Current job */

    cupsdSaveAllJobs();

    for (job = (cupsd_job_t *)cupsArrayFirst(Jobs);
         job;
	 job = (cupsd_job_t *)cupsArrayNext(Jobs))
      if (job->dirty)
        cupsdSaveJob(job);
  }

  if (DirtyFiles & CUPSD_DIRTY_SUBSCRIPTIONS)
    cupsdSaveAllSubscriptions();

  DirtyFiles     = CUPSD_DIRTY_NONE;
  DirtyCleanTime = 0;
}


/*
 * 'cupsdMarkDirty()' - Mark config or state files as needing a write.
 */

void
cupsdMarkDirty(int what)		/* I - What file(s) are dirty? */
{
  DirtyFiles |= what;

  if (!DirtyCleanTime)
    DirtyCleanTime = time(NULL) + DirtyCleanInterval;

  cupsdSetBusyState();
}


/*
 * 'cupsdSetBusyState()' - Let the system know when we are busy doing something.
 */

void
cupsdSetBusyState(void)
{
  int		newbusy;		/* New busy state */
  static int	busy = 0;		/* Current busy state */


  newbusy = DirtyCleanTime ||
            cupsArrayCount(PrintingJobs) ||
	    cupsArrayCount(ActiveClients);

  if (newbusy != busy)
  {
    busy = newbusy;

    if (busy)
      cupsdLogMessage(CUPSD_LOG_DEBUG2,
                      "cupsdSetBusyState: Server no longer busy...");
    else
      cupsdLogMessage(CUPSD_LOG_DEBUG2,
                      "cupsdSetBusyState: Server is now busy...");
  }
}


#ifdef __APPLE__
/*
 * This is the Apple-specific system event code.  It works by creating
 * a worker thread that waits for events from the OS and relays them
 * to the main thread via a traditional pipe.
 */

/*
 * Include MacOS-specific headers...
 */

#  include <IOKit/IOKitLib.h>
#  include <IOKit/IOMessage.h>
#  include <IOKit/pwr_mgt/IOPMLib.h>
#  include <SystemConfiguration/SystemConfiguration.h>
#  include <pthread.h>


/*
 * Constants...
 */

#  define SYSEVENT_CANSLEEP	0x1	/* Decide whether to allow sleep or not */
#  define SYSEVENT_WILLSLEEP	0x2	/* Computer will go to sleep */
#  define SYSEVENT_WOKE		0x4	/* Computer woke from sleep */
#  define SYSEVENT_NETCHANGED	0x8	/* Network changed */
#  define SYSEVENT_NAMECHANGED	0x10	/* Computer name changed */


/* 
 * Structures... 
 */

typedef struct cupsd_sysevent_s		/*** System event data ****/
{
  unsigned char	event;			/* Event bit field */
  io_connect_t	powerKernelPort;	/* Power context data */
  long		powerNotificationID;	/* Power event data */
} cupsd_sysevent_t;


typedef struct cupsd_thread_data_s	/*** Thread context data  ****/
{
  cupsd_sysevent_t	sysevent;	/* System event */
  CFRunLoopTimerRef	timerRef;	/* Timer to delay some change *
					 * notifications              */
} cupsd_thread_data_t;


/* 
 * Local globals... 
 */

static pthread_t	SysEventThread = NULL;
					/* Thread to host a runloop */
static pthread_mutex_t	SysEventThreadMutex = { 0 };
					/* Coordinates access to shared gloabals */ 
static pthread_cond_t	SysEventThreadCond = { 0 };
					/* Thread initialization complete condition */
static CFRunLoopRef	SysEventRunloop = NULL;
					/* The runloop. Access must be protected! */
static CFStringRef	ComputerNameKey = NULL,
					/* Computer name key */
			NetworkGlobalKeyIPv4 = NULL,
					/* Network global IPv4 key */
			NetworkGlobalKeyIPv6 = NULL,
					/* Network global IPv6 key */
			NetworkGlobalKeyDNS = NULL,
					/* Network global DNS key */
			HostNamesKey = NULL,
					/* Host name key */
			NetworkInterfaceKeyIPv4 = NULL,
					/* Netowrk interface key */
			NetworkInterfaceKeyIPv6 = NULL;
					/* Netowrk interface key */


/* 
 * Local functions... 
 */

static void	*sysEventThreadEntry(void);
static void	sysEventPowerNotifier(void *context, io_service_t service,
		                      natural_t messageType,
				      void *messageArgument);
static void	sysEventConfigurationNotifier(SCDynamicStoreRef store,
		                              CFArrayRef changedKeys,
					      void *context);
static void	sysEventTimerNotifier(CFRunLoopTimerRef timer, void *context);
static void	sysUpdate(void);


/*
 * 'cupsdStartSystemMonitor()' - Start monitoring for system change.
 */

void
cupsdStartSystemMonitor(void)
{
  int	flags;				/* fcntl flags on pipe */


  if (cupsdOpenPipe(SysEventPipes))
  {
    cupsdLogMessage(CUPSD_LOG_ERROR, "System event monitor pipe() failed - %s!",
                    strerror(errno));
    return;
  }

  cupsdAddSelect(SysEventPipes[0], (cupsd_selfunc_t)sysUpdate, NULL, NULL);

 /*
  * Set non-blocking mode on the descriptor we will be receiving notification
  * events on.
  */

  flags = fcntl(SysEventPipes[0], F_GETFL, 0);
  fcntl(SysEventPipes[0], F_SETFL, flags | O_NONBLOCK);

 /*
  * Start the thread that runs the runloop...
  */

  pthread_mutex_init(&SysEventThreadMutex, NULL);
  pthread_cond_init(&SysEventThreadCond, NULL);
  pthread_create(&SysEventThread, NULL, (void *(*)())sysEventThreadEntry, NULL);
}


/*
 * 'cupsdStopSystemMonitor()' - Stop monitoring for system change.
 */

void
cupsdStopSystemMonitor(void)
{
  CFRunLoopRef	rl;			/* The event handler runloop */


  if (SysEventThread)
  {
   /*
    * Make sure the thread has completed it's initialization and
    * stored it's runloop reference in the shared global.
    */

    pthread_mutex_lock(&SysEventThreadMutex);

    if (!SysEventRunloop)
      pthread_cond_wait(&SysEventThreadCond, &SysEventThreadMutex);

    rl              = SysEventRunloop;
    SysEventRunloop = NULL;

    pthread_mutex_unlock(&SysEventThreadMutex);

    if (rl)
      CFRunLoopStop(rl);

    pthread_join(SysEventThread, NULL);
    pthread_mutex_destroy(&SysEventThreadMutex);
    pthread_cond_destroy(&SysEventThreadCond);
  }

  if (SysEventPipes[0] >= 0)
  {
    cupsdRemoveSelect(SysEventPipes[0]);
    cupsdClosePipe(SysEventPipes);
  }
}


/*
 * 'sysEventThreadEntry()' - A thread to receive power and computer name
 *                           change notifications.
 */

static void *				/* O - Return status/value */
sysEventThreadEntry(void)
{
  io_object_t		powerNotifierObj;
					/* Power notifier object */
  IONotificationPortRef powerNotifierPort;
					/* Power notifier port */
  SCDynamicStoreRef	store    = NULL;/* System Config dynamic store */
  CFRunLoopSourceRef	powerRLS = NULL,/* Power runloop source */
			storeRLS = NULL;/* System Config runloop source */
  CFStringRef		key[5],		/* System Config keys */
			pattern[2];	/* System Config patterns */
  CFArrayRef		keys = NULL,	/* System Config key array*/
			patterns = NULL;/* System Config pattern array */
  SCDynamicStoreContext	storeContext;	/* Dynamic store context */
  CFRunLoopTimerContext timerContext;	/* Timer context */
  cupsd_thread_data_t	threadData;	/* Thread context data for the *
					 * runloop notifiers           */


 /*
  * Register for power state change notifications
  */

  bzero(&threadData, sizeof(threadData));

  threadData.sysevent.powerKernelPort =
      IORegisterForSystemPower(&threadData, &powerNotifierPort,
                               sysEventPowerNotifier, &powerNotifierObj);

  if (threadData.sysevent.powerKernelPort)
  {
    powerRLS = IONotificationPortGetRunLoopSource(powerNotifierPort);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), powerRLS, kCFRunLoopDefaultMode);
  }
  else
    DEBUG_puts("sysEventThreadEntry: error registering for system power "
               "notifications");

 /*
  * Register for system configuration change notifications
  */

  bzero(&storeContext, sizeof(storeContext));
  storeContext.info = &threadData;

  store = SCDynamicStoreCreate(NULL, CFSTR("cupsd"),
                               sysEventConfigurationNotifier, &storeContext);

  if (!ComputerNameKey)
    ComputerNameKey = SCDynamicStoreKeyCreateComputerName(NULL);

  if (!NetworkGlobalKeyIPv4)
    NetworkGlobalKeyIPv4 =
        SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
                                                   kSCDynamicStoreDomainState,
						   kSCEntNetIPv4);

  if (!NetworkGlobalKeyIPv6)
    NetworkGlobalKeyIPv6 =
        SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
                                                   kSCDynamicStoreDomainState,
						   kSCEntNetIPv6);

  if (!NetworkGlobalKeyDNS)
    NetworkGlobalKeyDNS = 
	SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL, 
						   kSCDynamicStoreDomainState,
						   kSCEntNetDNS);

  if (!HostNamesKey)
    HostNamesKey = SCDynamicStoreKeyCreateHostNames(NULL);

  if (!NetworkInterfaceKeyIPv4)
    NetworkInterfaceKeyIPv4 =
        SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
	                                              kSCDynamicStoreDomainState,
						      kSCCompAnyRegex,
						      kSCEntNetIPv4);

  if (!NetworkInterfaceKeyIPv6)
    NetworkInterfaceKeyIPv6 =
        SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
	                                              kSCDynamicStoreDomainState,
						      kSCCompAnyRegex,
						      kSCEntNetIPv6);

  if (store && ComputerNameKey && HostNamesKey &&
      NetworkGlobalKeyIPv4 && NetworkGlobalKeyIPv6 && NetworkGlobalKeyDNS &&
      NetworkInterfaceKeyIPv4 && NetworkInterfaceKeyIPv6)
  {
    key[0]     = ComputerNameKey;
    key[1]     = NetworkGlobalKeyIPv4;
    key[2]     = NetworkGlobalKeyIPv6;
    key[3]     = NetworkGlobalKeyDNS;
    key[4]     = HostNamesKey;

    pattern[0] = NetworkInterfaceKeyIPv4;
    pattern[1] = NetworkInterfaceKeyIPv6;

    keys     = CFArrayCreate(NULL, (const void **)key,
                                    sizeof(key) / sizeof(key[0]),
				    &kCFTypeArrayCallBacks);

    patterns = CFArrayCreate(NULL, (const void **)pattern,
                             sizeof(pattern) / sizeof(pattern[0]),
			     &kCFTypeArrayCallBacks);

    if (keys && patterns &&
        SCDynamicStoreSetNotificationKeys(store, keys, patterns))
    {
      if ((storeRLS = SCDynamicStoreCreateRunLoopSource(NULL, store, 0))
              != NULL)
      {
	CFRunLoopAddSource(CFRunLoopGetCurrent(), storeRLS,
	                   kCFRunLoopDefaultMode);
      }
      else
	DEBUG_printf(("sysEventThreadEntry: SCDynamicStoreCreateRunLoopSource "
	              "failed: %s\n", SCErrorString(SCError())));
    }
    else
      DEBUG_printf(("sysEventThreadEntry: SCDynamicStoreSetNotificationKeys "
                    "failed: %s\n", SCErrorString(SCError())));
  }
  else
    DEBUG_printf(("sysEventThreadEntry: SCDynamicStoreCreate failed: %s\n",
                  SCErrorString(SCError())));

  if (keys)
    CFRelease(keys);

  if (patterns)
    CFRelease(patterns);

 /*
  * Set up a timer to delay the wake change notifications.
  *
  * The initial time is set a decade or so into the future, we'll adjust
  * this later.
  */

  bzero(&timerContext, sizeof(timerContext));
  timerContext.info = &threadData;

  threadData.timerRef =
      CFRunLoopTimerCreate(NULL,
                           CFAbsoluteTimeGetCurrent() + (86400L * 365L * 10L), 
			   86400L * 365L * 10L, 0, 0, sysEventTimerNotifier,
			   &timerContext);
  CFRunLoopAddTimer(CFRunLoopGetCurrent(), threadData.timerRef,
                    kCFRunLoopDefaultMode);

 /*
  * Store our runloop in a global so the main thread can use it to stop us.
  */

  pthread_mutex_lock(&SysEventThreadMutex);

  SysEventRunloop = CFRunLoopGetCurrent();

  pthread_cond_signal(&SysEventThreadCond);
  pthread_mutex_unlock(&SysEventThreadMutex);

 /*
  * Disappear into the runloop until it's stopped by the main thread.
  */

  CFRunLoopRun();

 /*
  * Clean up before exiting.
  */

  if (threadData.timerRef)
  {
    CFRunLoopRemoveTimer(CFRunLoopGetCurrent(), threadData.timerRef,
                         kCFRunLoopDefaultMode);
    CFRelease(threadData.timerRef);
  }

  if (threadData.sysevent.powerKernelPort)
  {
    CFRunLoopRemoveSource(CFRunLoopGetCurrent(), powerRLS,
                          kCFRunLoopDefaultMode);
    IODeregisterForSystemPower(&powerNotifierObj);
    IOServiceClose(threadData.sysevent.powerKernelPort);
    IONotificationPortDestroy(powerNotifierPort);
  }

  if (storeRLS)
  {
    CFRunLoopRemoveSource(CFRunLoopGetCurrent(), storeRLS,
                          kCFRunLoopDefaultMode);
    CFRunLoopSourceInvalidate(storeRLS);
    CFRelease(storeRLS);
  }

  if (store)
    CFRelease(store);

  pthread_exit(NULL);
}


/*
 * 'sysEventPowerNotifier()' - Handle power notification events.
 */

static void
sysEventPowerNotifier(
    void         *context,		/* I - Thread context data */
    io_service_t service,		/* I - Unused service info */
    natural_t    messageType,		/* I - Type of message */
    void         *messageArgument)	/* I - Message data */
{
  int			sendit = 1;	/* Send event to main thread?    *
					 * (0 = no, 1 = yes, 2 = delayed */
  cupsd_thread_data_t	*threadData;	/* Thread context data */


  threadData = (cupsd_thread_data_t *)context;

  (void)service;			/* anti-compiler-warning-code */

  switch (messageType)
  {
    case kIOMessageCanSystemPowerOff:
    case kIOMessageCanSystemSleep:
	threadData->sysevent.event |= SYSEVENT_CANSLEEP;
	break;

    case kIOMessageSystemWillRestart:
    case kIOMessageSystemWillPowerOff:
    case kIOMessageSystemWillSleep:
	threadData->sysevent.event |= SYSEVENT_WILLSLEEP;
	break;

    case kIOMessageSystemHasPoweredOn:
       /* 
	* Because powered on is followed by a net-changed event, delay
	* before sending it.
	*/

        sendit = 2;
	threadData->sysevent.event |= SYSEVENT_WOKE;
	break;

    case kIOMessageSystemWillNotPowerOff:
    case kIOMessageSystemWillNotSleep:
#ifdef kIOMessageSystemWillPowerOn
    case kIOMessageSystemWillPowerOn:
#endif /* kIOMessageSystemWillPowerOn */
    default:
	sendit = 0;
	break;
  }

  if (sendit == 0)
    IOAllowPowerChange(threadData->sysevent.powerKernelPort,
                       (long)messageArgument);
  else
  {
    threadData->sysevent.powerNotificationID = (long)messageArgument;

    if (sendit == 1)
    {
     /* 
      * Send the event to the main thread now.
      */

      write(SysEventPipes[1], &threadData->sysevent,
	    sizeof(threadData->sysevent));
      threadData->sysevent.event = 0;
    }
    else
    {
     /* 
      * Send the event to the main thread after 1 to 2 seconds.
      */

      CFRunLoopTimerSetNextFireDate(threadData->timerRef,
                                    CFAbsoluteTimeGetCurrent() + 2);
    }
  }
}


/*
 * 'sysEventConfigurationNotifier()' - Computer name changed notification
 *                                     callback.
 */

static void
sysEventConfigurationNotifier(
    SCDynamicStoreRef store,		/* I - System data (unused) */
    CFArrayRef        changedKeys,	/* I - Changed data */
    void              *context)		/* I - Thread context data */
{
  cupsd_thread_data_t	*threadData;	/* Thread context data */


  threadData = (cupsd_thread_data_t *)context;
  
  (void)store;				/* anti-compiler-warning-code */

  CFRange range = CFRangeMake(0, CFArrayGetCount(changedKeys));

  if (CFArrayContainsValue(changedKeys, range, ComputerNameKey))
    threadData->sysevent.event |= SYSEVENT_NAMECHANGED;
  else
  {
    threadData->sysevent.event |= SYSEVENT_NETCHANGED;

   /*
    * Indicate the network interface list needs updating...
    */

    NetIFUpdate = 1;
  }

 /*
  * Because we registered for several different kinds of change notifications 
  * this callback usually gets called several times in a row. We use a timer to 
  * de-bounce these so we only end up generating one event for the main thread.
  */

  CFRunLoopTimerSetNextFireDate(threadData->timerRef, 
  				CFAbsoluteTimeGetCurrent() + 5);
}


/*
 * 'sysEventTimerNotifier()' - Handle delayed event notifications.
 */

static void
sysEventTimerNotifier(
    CFRunLoopTimerRef timer,		/* I - Timer information */
    void              *context)		/* I - Thread context data */
{
  cupsd_thread_data_t	*threadData;	/* Thread context data */


  threadData = (cupsd_thread_data_t *)context;

 /*
  * If an event is still pending send it to the main thread.
  */

  if (threadData->sysevent.event)
  {
    write(SysEventPipes[1], &threadData->sysevent,
          sizeof(threadData->sysevent));
    threadData->sysevent.event = 0;
  }
}


/*
 * 'sysUpdate()' - Update the current system state.
 */

static void
sysUpdate(void)
{
  int			i;		/* Looping var */
  cupsd_sysevent_t	sysevent;	/* The system event */
  cupsd_printer_t	*p;		/* Printer information */


 /*
  * Drain the event pipe...
  */

  while (read((int)SysEventPipes[0], &sysevent, sizeof(sysevent))
             == sizeof(sysevent))
  {
    if (sysevent.event & SYSEVENT_CANSLEEP)
    {
     /*
      * If there are active printers that don't have the connecting-to-device
      * printer-state-reason then cancel the sleep request (i.e. this reason
      * indicates a job that is not yet connected to the printer)...
      */

      for (p = (cupsd_printer_t *)cupsArrayFirst(Printers);
           p;
	   p = (cupsd_printer_t *)cupsArrayNext(Printers))
      {
        if (p->job)
        {
	  for (i = 0; i < p->num_reasons; i ++)
	    if (!strcmp(p->reasons[i], "connecting-to-device"))
	      break;

	  if (!p->num_reasons || i >= p->num_reasons)
	    break;
        }
      }

      if (p)
      {
        cupsdLogMessage(CUPSD_LOG_INFO,
	                "System sleep canceled because printer %s is active",
	                p->name);
        IOCancelPowerChange(sysevent.powerKernelPort,
	                    sysevent.powerNotificationID);
      }
      else
      {
	cupsdLogMessage(CUPSD_LOG_DEBUG, "System wants to sleep");
        IOAllowPowerChange(sysevent.powerKernelPort,
	                   sysevent.powerNotificationID);
      }
    }

    if (sysevent.event & SYSEVENT_WILLSLEEP)
    {
      cupsdLogMessage(CUPSD_LOG_DEBUG, "System going to sleep");

      Sleeping = 1;

      cupsdStopAllJobs(0);

      for (p = (cupsd_printer_t *)cupsArrayFirst(Printers);
           p;
	   p = (cupsd_printer_t *)cupsArrayNext(Printers))
      {
	if (p->type & CUPS_PRINTER_DISCOVERED)
	{
	  cupsdLogMessage(CUPSD_LOG_DEBUG,
	                  "Deleting remote destination \"%s\"", p->name);
	  cupsArraySave(Printers);
	  cupsdDeletePrinter(p, 0);
	  cupsArrayRestore(Printers);
	}
	else
	{
	  cupsdLogMessage(CUPSD_LOG_DEBUG,
	                  "Deregistering local printer \"%s\"", p->name);
	  cupsdDeregisterPrinter(p, 0);
	}
      }

      cupsdCleanDirty();

      IOAllowPowerChange(sysevent.powerKernelPort,
                         sysevent.powerNotificationID);
    }

    if (sysevent.event & SYSEVENT_WOKE)
    {
      cupsdLogMessage(CUPSD_LOG_DEBUG, "System woke from sleep");
      IOAllowPowerChange(sysevent.powerKernelPort,
                         sysevent.powerNotificationID);
      Sleeping = 0;
      cupsdCheckJobs();
    }

    if (sysevent.event & SYSEVENT_NETCHANGED)
    {
      if (!Sleeping)
      {
        cupsdLogMessage(CUPSD_LOG_DEBUG,
	                "System network configuration changed");

       /*
        * Resetting browse_time before calling cupsdSendBrowseList causes
	* browse packets to be sent for local shared printers.
        */

	for (p = (cupsd_printer_t *)cupsArrayFirst(Printers);
	     p;
	     p = (cupsd_printer_t *)cupsArrayNext(Printers))
	  p->browse_time = 0;

        cupsdSendBrowseList();
	cupsdRestartPolling();
      }
      else
        cupsdLogMessage(CUPSD_LOG_DEBUG,
	                "System network configuration changed; "
			"ignored while sleeping");
    }

    if (sysevent.event & SYSEVENT_NAMECHANGED)
    {
      if (!Sleeping)
      {
        cupsdLogMessage(CUPSD_LOG_DEBUG, "Computer name changed");

       /*
	* De-register the individual printers...
	*/

	for (p = (cupsd_printer_t *)cupsArrayFirst(Printers);
	     p;
	     p = (cupsd_printer_t *)cupsArrayNext(Printers))
	  cupsdDeregisterPrinter(p, 1);

       /*
        * Update the computer name...
	*/

	cupsdUpdateDNSSDName();

       /*
	* Now re-register them...
	*/

	for (p = (cupsd_printer_t *)cupsArrayFirst(Printers);
	     p;
	     p = (cupsd_printer_t *)cupsArrayNext(Printers))
	{
	  p->browse_time = 0;
	  cupsdRegisterPrinter(p);
	}
      }
      else
        cupsdLogMessage(CUPSD_LOG_DEBUG,
	                "Computer name changed; ignored while sleeping");
    }
  }
}
#endif	/* __APPLE__ */


/*
 * End of "$Id$".
 */
