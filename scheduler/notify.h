/*
 * "$Id: notify.h,v 1.1.2.1 2004/07/02 19:12:48 mike Exp $"
 *
 *   Notification definitions for the Common UNIX Printing System (CUPS)
 *   scheduler.
 *
 *   Copyright 1997-2004 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Easy Software Products and are protected by Federal
 *   copyright law.  Distribution and use rights are outlined in the file
 *   "LICENSE.txt" which should have been included with this file.  If this
 *   file is missing or damaged please contact Easy Software Products
 *   at:
 *
 *       Attn: CUPS Licensing Information
 *       Easy Software Products
 *       44141 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3142 USA
 *
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 */


/*
 * Event mask constants...
 */

enum
{
  /* Standard IPP notifications */
  CUPSD_EVENT_IPP_PRINTER_STATE_CHANGED = 0x00000007,
  CUPSD_EVENT_IPP_PRINTER_RESTARTED = 0x00000001,
  CUPSD_EVENT_IPP_PRINTER_SHUTDOWN = 0x00000002,
  CUPSD_EVENT_IPP_PRINTER_STOPPED = 0x00000004,
  CUPSD_EVENT_IPP_PRINTER_CONFIG_CHANGED = 0x000000018,
  CUPSD_EVENT_IPP_PRINTER_MEDIA_CHANGED = 0x00000008,
  CUPSD_EVENT_IPP_PRINTER_FINISHINGS_CHANGED = 0x00000010,
  CUPSD_EVENT_IPP_PRINTER_QUEUE_ORDER_CHANGED = 0x00000020,
  CUPSD_EVENT_IPP_JOB_STATE_CHANGED = 0x000001c0,
  CUPSD_EVENT_IPP_JOB_CREATED = 0x00000040,
  CUPSD_EVENT_IPP_JOB_COMPLETED = 0x00000080,
  CUPSD_EVENT_IPP_JOB_STOPPED = 0x00000100,
  CUPSD_EVENT_IPP_JOB_CONFIG_CHANGED = 0x00000200,
  CUPSD_EVENT_IPP_JOB_PROGRESS = 0x00000400,

  /* CUPS event extensions */
  CUPSD_EVENT_CUPS_QUEUE = 0x003f0000,
  CUPSD_EVENT_CUPS_QUEUE_ADDED = 0x00030000,
  CUPSD_EVENT_CUPS_PRINTER_ADDED = 0x00010000,
  CUPSD_EVENT_CUPS_CLASS_ADDED = 0x00020000,
  CUPSD_EVENT_CUPS_QUEUE_CHANGED = 0x000c0000,
  CUPSD_EVENT_CUPS_PRINTER_CHANGED = 0x00040000,
  CUPSD_EVENT_CUPS_CLASS_CHANGED = 0x00080000,
  CUPSD_EVENT_CUPS_QUEUE_DELETED = 0x00300000,
  CUPSD_EVENT_CUPS_PRINTER_DELETED = 0x00100000,
  CUPSD_EVENT_CUPS_CLASS_DELETED = 0x00200000,
  CUPSD_EVENT_CUPS_JOB_MOVED = 0x00400000,
  CUPSD_EVENT_CUPS_DEVICE = 0x03800000,
  CUPSD_EVENT_CUPS_DEVICE_ADDED = 0x00800000,
  CUPSD_EVENT_CUPS_DEVICE_CHANGED = 0x01000000,
  CUPSD_EVENT_CUPS_DEVICE_DELETED = 0x02000000,
  CUPSD_EVENT_CUPS_OPERATION = 0x04000000
};


/*
 * Types and structures...
 */

typedef struct cupsd_subscription_str	/**** Subscription object ****/
{
  struct cupsd_subscription_str *next;	/* Pointer to next subscription */
  int		id;			/* subscription-id */
  unsigned	mask;			/* Event mask */
  ipp_t		*attrs;			/* Subscription attributes */
  int		job_id;			/* Subscription Job ID */
  char		*dest;			/* Subscription printer/class  */
  cups_ptype_t	dtype;			/* Type of destination */
  char		*recipient;		/* Recipient of subscription */
  int		pid;			/* PID of notifier process */
  int		notify_pipe;		/* Pipe to process */
  int		status_pipe;		/* Pipe from process */
  int		status;			/* Exit status of notifier */
  char		*buffer;		/* Status buffer */
  int		bufused;		/* Amount of buffer in use */
  time_t	last_time;		/* Time of last notification */
} cupsd_subscription_t;

typedef struct cupsd_event_str  	/**** Event object ****/
{
  struct cupsd_event_str *next;		/* Pointer to next event */
  int		id;			/* event-id */
  time_t	event_time;		/* event-time */
  ipp_t		*attrs;			/* Event attributes */
  unsigned	mask;			/* Event mask */
  int		job_id;			/* Event job ID */
  char		*dest;			/* Event printer/class */
  cups_ptype_t	dtype;			/* Type of destination */
} cupsd_event_t;


/*
 * Globals...
 */


VAR int			NumEvents	VALUE(0),
					/* Number of active events */
			MaxEvents	VALUE(100);
					/* Maximum number of events to hold */
VAR cupsd_event_t	*Events		VALUE(NULL),
					/* List of events */
			*LastEvent	VALUE(NULL);
					/* Last event in list */

VAR int			MaxSubscriptions VALUE(100),
					/* Maximum number of subscriptions */
			MaxSubscriptionsPerUser VALUE(0),
					/* Maximum subscriptions per user */
			MaxSubscriptionsPerPrinter VALUE(0),
					/* Maximum subscriptions per printer */
			MaxSubscriptionsPerJob VALUE(0),
					/* Maximum subscriptions per job */
			NumSubscriptions VALUE(0);
					/* Number of subscriptions */
VAR cupsd_subscription_t *Subscriptions	VALUE(NULL),
					/* List of subscriptions */
			*LastSubcription VALUE(NULL);
					/* Last subscription in list */


/*
 * End of "$Id: notify.h,v 1.1.2.1 2004/07/02 19:12:48 mike Exp $".
 */
