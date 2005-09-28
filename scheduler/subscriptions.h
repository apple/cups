/*
 * "$Id$"
 *
 *   Subscription definitions for the Common UNIX Printing System (CUPS) scheduler.
 *
 *   Copyright 1997-2005 by Easy Software Products, all rights reserved.
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
 *       Hollywood, Maryland 20636 USA
 *
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 */

/*
 * Event mask enumeration...
 */

typedef enum
{
  /* Individual printer events... */
  CUPSD_EVENT_PRINTER_RESTARTED = 0x0001,
					/* Sent after printer restarted */
  CUPSD_EVENT_PRINTER_SHUTDOWN = 0x0002,/* Sent after printer shutdown */
  CUPSD_EVENT_PRINTER_STOPPED = 0x0004,	/* Sent after printer stopped */
  CUPSD_EVENT_PRINTER_FINISHINGS_CHANGED = 0x0008,
					/* Sent after finishings-supported changed */
  CUPSD_EVENT_PRINTER_MEDIA_CHANGED = 0x0010,
					/* Sent after media-supported changed */
  CUPSD_EVENT_PRINTER_ADDED = 0x0020,	/* Sent after printer added */
  CUPSD_EVENT_PRINTER_DELETED = 0x0040,	/* Sent after printer deleted */
  CUPSD_EVENT_PRINTER_MODIFIED = 0x0080,/* Sent after printer modified */

  /* Convenience printer event groupings... */
  CUPSD_EVENT_PRINTER_STATE_CHANGED = 0x0007,
					/* RESTARTED + SHUTDOWN + STOPPED */
  CUPSD_EVENT_PRINTER_CONFIG_CHANGED = 0x0018,
					/* FINISHINGS_CHANGED + MEDIA_CHANGED */
  CUPSD_EVENT_PRINTER_CHANGED = 0x00ff,	/* All of the above */

  /* Individual job events... */
  CUPSD_EVENT_JOB_CREATED = 0x0100,	/* Send after job is created */
  CUPSD_EVENT_JOB_COMPLETED = 0x0200,	/* Sent after job is completed */
  CUPSD_EVENT_JOB_STOPPED = 0x0400,	/* Sent after job is stopped */
  CUPSD_EVENT_JOB_CONFIG_CHANGED = 0x0800,
					/* Sent after set-job-attributes */
  CUPSD_EVENT_JOB_PROGRESS = 0x1000,	/* Sent for each page */

  /* Convenience job event grouping... */
  CUPSD_EVENT_JOB_STATE_CHANGED = 0x0700,
					/* CREATED + COMPLETED + STOPPED */

  /* Server events... */
  CUPSD_EVENT_SERVER_RESTARTED = 0x2000,/* Sent after server restarts */
  CUPSD_EVENT_SERVER_STARTED = 0x4000,	/* Sent when server first starts */
  CUPSD_EVENT_SERVER_STOPPED = 0x8000,	/* Sent when server is stopped */
  CUPSD_EVENT_SERVER_AUDIT = 0x10000,	/* Security-related stuff */

  /* Everything and nothing... */
  CUPSD_EVENT_NONE = 0,			/* Nothing */
  CUPSD_EVENT_ALL = 0x1ffff		/* Everything */
} cupsd_eventmask_t;


/*
 * Notiification support structures...
 */

typedef struct cupsd_event_s		/**** Event structure ****/
{
  cupsd_eventmask_t	event;		/* Event */
  time_t		time;		/* Time of event */
  ipp_t			*attrs;		/* Notification message */
  cupsd_printer_t	*dest;		/* Associated printer, if any */
  cupsd_job_t		*job;		/* Associated job, if any */
} cupsd_event_t; 

typedef struct cupsd_subscription_s	/**** Subscription structure ****/
{
  int			id;		/* subscription-id */
  unsigned		mask;		/* Event mask */
  char			*owner;		/* notify-subscriber-user-name */
  char			*recipient;	/* notify-recipient-uri, if applicable */
  unsigned char		user_data[64];	/* notify-user-data */
  int			user_data_len;	/* Length of notify-user-data */
  int			lease;		/* notify-lease-time */
  int			interval;	/* notify-interval */
  cupsd_printer_t	*dest;		/* notify-printer-uri, if any */
  cupsd_job_t		*job;		/* notify-job-id, if any */
  int			pid;		/* Process ID of notifier */
  int			pipe;		/* Pipe to notifier */
  int			status;		/* Exit status of notifier */
  time_t		last;		/* Time of last notification */
  time_t		expire;		/* Lease expiration time */
  int			first_event_id,	/* First event-id in cache */
			next_event_id,	/* Next event-id to use */
			num_events;	/* Number of cached events */
  cupsd_event_t		**events;	/* Cached events */
} cupsd_subscription_t;


/*
 * Globals...
 */

VAR int		MaxSubscriptions VALUE(100),
					/* Overall subscription limit */
		MaxSubscriptionsPerJob VALUE(0),
					/* Per-job subscription limit */
		MaxSubscriptionsPerPrinter VALUE(0),
					/* Per-printer subscription limit */
		MaxSubscriptionsPerUser VALUE(0),
					/* Per-user subscription limit */
		NextSubscriptionId VALUE(1),
					/* Next subscription ID */
		DefaultLeaseTime VALUE(86400),
					/* Default notify-lease-time */
		MaxLeaseTime VALUE(0),	/* Maximum notify-lease-time */
		NumSubscriptions VALUE(0);
					/* Number of active subscriptions */
VAR cupsd_subscription_t **Subscriptions VALUE(NULL);
					/* Active subscriptions */

VAR int		MaxEvents VALUE(100),	/* Maximum number of events */
		NumEvents VALUE(0);	/* Number of active events */
VAR cupsd_event_t **Events VALUE(NULL);	/* Active events */

VAR int		NotifierPipes[2] VALUE2(-1, -1);
					/* Pipes for notifier error/debug output */
VAR cupsd_statbuf_t *NotifierStatusBuffer VALUE(NULL);
					/* Status buffer for pipes */


/*
 * Prototypes...
 */

extern void	cupsdAddEvent(cupsd_eventmask_t event, cupsd_printer_t *dest,
		              cupsd_job_t *job, const char *text, ...);
extern cupsd_subscription_t *
		cupsdAddSubscription(unsigned mask,
		                     cupsd_printer_t *dest,
		                     cupsd_job_t *job, const char *uri);
extern void	cupsdDeleteAllEvents(void);
extern void	cupsdDeleteAllSubscriptions(void);
extern void	cupsdDeleteSubscription(cupsd_subscription_t *sub, int update);
extern const char *
		cupsdEventName(cupsd_eventmask_t event);
extern cupsd_eventmask_t
		cupsdEventValue(const char *name);

extern cupsd_subscription_t *
		cupsdFindSubscription(int id);
extern void	cupsdExpireSubscriptions(cupsd_printer_t *dest,
		                         cupsd_job_t *job);
extern void	cupsdLoadAllSubscriptions(void);
extern void	cupsdSaveAllSubscriptions(void);
extern void	cupsdSendNotification(cupsd_subscription_t *sub,
		                      cupsd_event_t *event);
extern void	cupsdStopAllNotifiers(void);
extern void	cupsdUpdateNotiferStatus(void);


/*
 * End of "$Id$".
 */
