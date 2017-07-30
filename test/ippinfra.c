/*
 * "$Id: ippinfra.c 12237 2014-11-03 13:07:32Z msweet $"
 *
 * Sample IPP INFRA server for CUPS.
 *
 * Copyright 2010-2014 by Apple Inc.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * This file is subject to the Apple OS-Developed Software exception.
 */

/*
 * Disable private and deprecated stuff so we can verify that the public API
 * is sufficient to implement a server.
 */

#define _IPP_PRIVATE_STRUCTURES 0	/* Disable private IPP stuff */
#define _CUPS_NO_DEPRECATED 1		/* Disable deprecated stuff */


/*
 * Include necessary headers...
 */

#include <config.h>			/* CUPS configuration header */
#include <cups/cups.h>			/* Public API */
#include <cups/string-private.h>	/* CUPS string functions */
#include <cups/thread-private.h>	/* For multithreading functions */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>

#ifdef WIN32
#  include <fcntl.h>
#  include <io.h>
#  include <process.h>
#  define WEXITSTATUS(s) (s)
#  include <winsock2.h>
typedef ULONG nfds_t;
#  define poll WSAPoll
#else
extern char **environ;

#  include <sys/fcntl.h>
#  include <sys/wait.h>
#  include <poll.h>
#endif /* WIN32 */

#ifdef HAVE_SYS_MOUNT_H
#  include <sys/mount.h>
#endif /* HAVE_SYS_MOUNT_H */
#ifdef HAVE_SYS_STATFS_H
#  include <sys/statfs.h>
#endif /* HAVE_SYS_STATFS_H */
#ifdef HAVE_SYS_STATVFS_H
#  include <sys/statvfs.h>
#endif /* HAVE_SYS_STATVFS_H */
#ifdef HAVE_SYS_VFS_H
#  include <sys/vfs.h>
#endif /* HAVE_SYS_VFS_H */

#ifdef HAVE_PTHREAD_H
typedef pthread_cond_t _cups_cond_t;
#  define _CUPS_COND_INITIALIZER PTHREAD_COND_INITIALIZER
#  define _cupsCondBroadcast(c) pthread_cond_broadcast(c)
#  define _cupsCondDeinit(c)	pthread_cond_destroy(c)
#  define _cupsCondInit(c)	pthread_cond_init((c), NULL)
#  define _cupsCondWait(c,m)	pthread_cond_wait((c),(m))
#  define _cupsMutexDeinit(m)	pthread_mutex_destroy(m)
#  define _cupsRWDeinit(rw)	pthread_rwlock_destroy(rw)
#else
typedef char _cups_cond_t;
#  define _CUPS_COND_INITIALIZER 0
#  define _cupsCondBroadcast(c)
#  define _cupsCondDeinit(c)
#  define _cupsCondInit(c)	*(c)=0
#  define _cupsCondWait(c,m)	0
#  define _cupsMutexDeinit(m)
#  define _cupsRWDeinit(rw)
#endif /* HAVE_PTHREAD_H */


/*
 * Constants...
 */

/* New IPP operation codes from IPP INFRA */
#  define _IPP_OP_ACKNOWLEDGE_DOCUMENT			(ipp_op_t)0x003f
#  define _IPP_OP_ACKNOWLEDGE_IDENTIFY_PRINTER		(ipp_op_t)0x0040
#  define _IPP_OP_ACKNOWLEDGE_JOB			(ipp_op_t)0x0041
#  define _IPP_OP_FETCH_DOCUMENT			(ipp_op_t)0x0042
#  define _IPP_OP_FETCH_JOB				(ipp_op_t)0x0043
#  define _IPP_OP_GET_OUTPUT_DEVICE_ATTRIBUTES		(ipp_op_t)0x0044
#  define _IPP_OP_UPDATE_ACTIVE_JOBS			(ipp_op_t)0x0045
#  define _IPP_OP_UPDATE_DOCUMENT_STATUS		(ipp_op_t)0x0047
#  define _IPP_OP_UPDATE_JOB_STATUS			(ipp_op_t)0x0048
#  define _IPP_OP_UPDATE_OUTPUT_DEVICE_ATTRIBUTES	(ipp_op_t)0x0049
#  define _IPP_OP_DEREGISTER_OUTPUT_DEVICE		(ipp_op_t)0x204b

/* New IPP status code from IPP INFRA */
#  define _IPP_STATUS_ERROR_NOT_FETCHABLE		(ipp_status_t)0x0420

/* Maximum lease duration value from RFC 3995 - 2^26-1 or ~2 years */
#  define _IPP_NOTIFY_LEASE_DURATION_MAX		67108863
/* But a value of 0 means "never expires"... */
#  define _IPP_NOTIFY_LEASE_DURATION_FOREVER		0
/* Default duration is 1 day */
#  define _IPP_NOTIFY_LEASE_DURATION_DEFAULT		86400


/*
 * Event mask enumeration...
 */

enum _ipp_event_e			/* notify-events bit values */
{
  _IPP_EVENT_DOCUMENT_COMPLETED = 0x00000001,
  _IPP_EVENT_DOCUMENT_CONFIG_CHANGED = 0x00000002,
  _IPP_EVENT_DOCUMENT_CREATED = 0x00000004,
  _IPP_EVENT_DOCUMENT_FETCHABLE = 0x00000008,
  _IPP_EVENT_DOCUMENT_STATE_CHANGED = 0x00000010,
  _IPP_EVENT_DOCUMENT_STOPPED = 0x00000020,
  _IPP_EVENT_JOB_COMPLETED = 0x00000040,
  _IPP_EVENT_JOB_CONFIG_CHANGED = 0x00000080,
  _IPP_EVENT_JOB_CREATED = 0x00000100,
  _IPP_EVENT_JOB_FETCHABLE = 0x00000200,
  _IPP_EVENT_JOB_PROGRESS = 0x00000400,
  _IPP_EVENT_JOB_STATE_CHANGED = 0x00000800,
  _IPP_EVENT_JOB_STOPPED = 0x00001000,
  _IPP_EVENT_PRINTER_CONFIG_CHANGED = 0x00002000,
  _IPP_EVENT_PRINTER_FINISHINGS_CHANGED = 0x00004000,
  _IPP_EVENT_PRINTER_MEDIA_CHANGED = 0x00008000,
  _IPP_EVENT_PRINTER_QUEUE_ORDER_CHANGED = 0x00010000,
  _IPP_EVENT_PRINTER_RESTARTED = 0x00020000,
  _IPP_EVENT_PRINTER_SHUTDOWN = 0x00040000,
  _IPP_EVENT_PRINTER_STATE_CHANGED = 0x00080000,
  _IPP_EVENT_PRINTER_STOPPED = 0x00100000,

  /* "Wildcard" values... */
  _IPP_EVENT_NONE = 0x00000000,		/* Nothing */
  _IPP_EVENT_DOCUMENT_ALL = 0x0000003f,
  _IPP_EVENT_DOCUMENT_STATE_ALL = 0x00000037,
  _IPP_EVENT_JOB_ALL = 0x00001fc0,
  _IPP_EVENT_JOB_STATE_ALL = 0x00001940,
  _IPP_EVENT_PRINTER_ALL = 0x001fe000,
  _IPP_EVENT_PRINTER_CONFIG_ALL = 0x0000e000,
  _IPP_EVENT_PRINTER_STATE_ALL = 0x001e0000,
  _IPP_EVENT_ALL = 0x001fffff		/* Everything */
};
typedef unsigned int _ipp_event_t;	/* Bitfield for notify-events */
#define _IPP_EVENT_DEFAULT _IPP_EVENT_JOB_COMPLETED
#define _IPP_EVENT_DEFAULT_STRING "job-completed"
static const char * const _ipp_events[] =
{					/* Strings for bits */
  "document-completed",
  "document-config-changed",
  "document-created",
  "document-fetchable",
  "document-state-changed",
  "document-stopped",
  "job-completed",
  "job-config-changed",
  "job-created",
  "job-fetchable",
  "job-progress",
  "job-state-changed",
  "job-stopped",
  "printer-config-changed",
  "printer-finishings-changed",
  "printer-media-changed",
  "printer-queue-order-changed",
  "printer-restarted",
  "printer-shutdown",
  "printer-state-changed",
  "printer-stopped"
};

enum _ipp_jreason_e			/* job-state-reasons bit values */
{
  _IPP_JREASON_NONE = 0x00000000,	/* none */
  _IPP_JREASON_ABORTED_BY_SYSTEM = 0x00000001,
  _IPP_JREASON_COMPRESSION_ERROR = 0x00000002,
  _IPP_JREASON_DOCUMENT_ACCESS_ERROR = 0x00000004,
  _IPP_JREASON_DOCUMENT_FORMAT_ERROR = 0x00000008,
  _IPP_JREASON_DOCUMENT_PASSWORD_ERROR = 0x00000010,
  _IPP_JREASON_DOCUMENT_PERMISSION_ERROR = 0x00000020,
  _IPP_JREASON_DOCUMENT_SECURITY_ERROR = 0x00000040,
  _IPP_JREASON_DOCUMENT_UNPRINTABLE_ERROR = 0x00000080,
  _IPP_JREASON_ERRORS_DETECTED = 0x00000100,
  _IPP_JREASON_JOB_CANCELED_AT_DEVICE = 0x00000200,
  _IPP_JREASON_JOB_CANCELED_BY_USER = 0x00000400,
  _IPP_JREASON_JOB_COMPLETED_SUCCESSFULLY = 0x00000800,
  _IPP_JREASON_JOB_COMPLETED_WITH_ERRORS = 0x00001000,
  _IPP_JREASON_JOB_COMPLETED_WITH_WARNINGS = 0x00002000,
  _IPP_JREASON_JOB_DATA_INSUFFICIENT = 0x00004000,
  _IPP_JREASON_JOB_FETCHABLE = 0x00008000,
  _IPP_JREASON_JOB_INCOMING = 0x00010000,
  _IPP_JREASON_JOB_PASSWORD_WAIT = 0x00020000,
  _IPP_JREASON_JOB_PRINTING = 0x00040000,
  _IPP_JREASON_JOB_QUEUED = 0x00080000,
  _IPP_JREASON_JOB_SPOOLING = 0x00100000,
  _IPP_JREASON_JOB_STOPPED = 0x00200000,
  _IPP_JREASON_JOB_TRANSFORMING = 0x00400000,
  _IPP_JREASON_PRINTER_STOPPED = 0x00800000,
  _IPP_JREASON_PRINTER_STOPPED_PARTLY = 0x01000000,
  _IPP_JREASON_PROCESSING_TO_STOP_POINT = 0x02000000,
  _IPP_JREASON_QUEUED_IN_DEVICE = 0x04000000,
  _IPP_JREASON_WARNINGS_DETECTED = 0x08000000
};
typedef unsigned int _ipp_jreason_t;	/* Bitfield for job-state-reasons */
static const char * const _ipp_jreasons[] =
{					/* Strings for bits */
  "aborted-by-system",
  "compression-error",
  "document-access-error",
  "document-format-error",
  "document-password-error",
  "document-permission-error",
  "document-security-error",
  "document-unprintable-error",
  "errors-detected",
  "job-canceled-at-device",
  "job-canceled-by-user",
  "job-completed-successfully",
  "job-completed-with-errors",
  "job-completed-with-warnings",
  "job-data-insufficient",
  "job-fetchable",
  "job-incoming",
  "job-password-wait",
  "job-printing",
  "job-queued",
  "job-spooling",
  "job-stopped",
  "job-transforming",
  "printer-stopped",
  "printer-stopped-partly",
  "processing-to-stop-point",
  "queued-in-device",
  "warnings-detected"
};

enum _ipp_preason_e			/* printer-state-reasons bit values */
{
  _IPP_PREASON_NONE = 0x0000,		/* none */
  _IPP_PREASON_OTHER = 0x0001,		/* other */
  _IPP_PREASON_COVER_OPEN = 0x0002,	/* cover-open */
  _IPP_PREASON_INPUT_TRAY_MISSING = 0x0004,
					/* input-tray-missing */
  _IPP_PREASON_MARKER_SUPPLY_EMPTY = 0x0008,
					/* marker-supply-empty */
  _IPP_PREASON_MARKER_SUPPLY_LOW = 0x0010,
					/* marker-supply-low */
  _IPP_PREASON_MARKER_WASTE_ALMOST_FULL = 0x0020,
					/* marker-waste-almost-full */
  _IPP_PREASON_MARKER_WASTE_FULL = 0x0040,
					/* marker-waste-full */
  _IPP_PREASON_MEDIA_EMPTY = 0x0080,	/* media-empty */
  _IPP_PREASON_MEDIA_JAM = 0x0100,	/* media-jam */
  _IPP_PREASON_MEDIA_LOW = 0x0200,	/* media-low */
  _IPP_PREASON_MEDIA_NEEDED = 0x0400,	/* media-needed */
  _IPP_PREASON_MOVING_TO_PAUSED = 0x0800,
					/* moving-to-paused */
  _IPP_PREASON_PAUSED = 0x1000,		/* paused */
  _IPP_PREASON_SPOOL_AREA_FULL = 0x2000,/* spool-area-full */
  _IPP_PREASON_TONER_EMPTY = 0x4000,	/* toner-empty */
  _IPP_PREASON_TONER_LOW = 0x8000	/* toner-low */
};
typedef unsigned int _ipp_preason_t;	/* Bitfield for printer-state-reasons */
static const char * const _ipp_preasons[] =
{					/* Strings for bits */
  "other",
  "cover-open",
  "input-tray-missing",
  "marker-supply-empty",
  "marker-supply-low",
  "marker-waste-almost-full",
  "marker-waste-full",
  "media-empty",
  "media-jam",
  "media-low",
  "media-needed",
  "moving-to-paused",
  "paused",
  "spool-area-full",
  "toner-empty",
  "toner-low"
};


/*
 * Structures...
 */

typedef struct _ipp_filter_s		/**** Attribute filter ****/
{
  cups_array_t		*ra;		/* Requested attributes */
  ipp_tag_t		group_tag;	/* Group to copy */
} _ipp_filter_t;

typedef struct _ipp_job_s _ipp_job_t;

typedef struct _ipp_device_s		/**** Output Device data ****/
{
  _cups_rwlock_t	rwlock;		/* Printer lock */
  char			*name,		/* printer-name (mapped to output-device) */
			*uuid;		/* output-device-uuid */
  ipp_t			*attrs;		/* All printer attributes */
  ipp_pstate_t		state;		/* printer-state value */
  _ipp_preason_t	reasons;	/* printer-state-reasons values */
} _ipp_device_t;

typedef struct _ipp_printer_s		/**** Printer data ****/
{
  _cups_rwlock_t	rwlock;		/* Printer lock */
  int			ipv4,		/* IPv4 listener */
			ipv6;		/* IPv6 listener */
  char			*name,		/* printer-name */
			*directory,	/* Spool directory */
			*hostname,	/* Hostname */
			*uri,		/* printer-uri-supported */
			*proxy_user,	/* Proxy username */
			*proxy_pass;	/* Proxy password */
  int			port;		/* Port */
  size_t		urilen;		/* Length of printer URI */
  cups_array_t		*devices;	/* Associated devices */
  ipp_t			*attrs;		/* Static attributes */
  ipp_t			*dev_attrs;	/* Current device attributes */
  time_t		start_time;	/* Startup time */
  time_t		config_time;	/* printer-config-change-time */
  ipp_pstate_t		state,		/* printer-state value */
			dev_state;	/* Current device printer-state value */
  _ipp_preason_t	state_reasons,	/* printer-state-reasons values */
			dev_reasons;	/* Current device printer-state-reasons values */
  time_t		state_time;	/* printer-state-change-time */
  cups_array_t		*jobs,		/* Jobs */
			*active_jobs,	/* Active jobs */
			*completed_jobs;/* Completed jobs */
  _ipp_job_t		*processing_job;/* Current processing job */
  int			next_job_id;	/* Next job-id value */
  cups_array_t		*subscriptions;	/* Subscriptions */
  int			next_sub_id;	/* Next notify-subscription-id value */
} _ipp_printer_t;

struct _ipp_job_s			/**** Job data ****/
{
  int			id;		/* job-id */
  _cups_rwlock_t	rwlock;		/* Job lock */
  const char		*name,		/* job-name */
			*username,	/* job-originating-user-name */
			*format;	/* document-format */
  int			priority;	/* job-priority */
  char			*dev_uuid;	/* output-device-uuid-assigned */
  ipp_jstate_t		state,		/* job-state value */
			dev_state;	/* output-device-job-state value */
  _ipp_jreason_t	state_reasons,	/* job-state-reasons values */
  			dev_state_reasons;
		      			/* output-device-job-state-reasons values */
  char			*dev_state_message;
					/* output-device-job-state-message value */
  time_t		created,	/* time-at-creation value */
			processing,	/* time-at-processing value */
			completed;	/* time-at-completed value */
  int			impressions,	/* job-impressions value */
			impcompleted;	/* job-impressions-completed value */
  ipp_t			*attrs;		/* Attributes */
  int			cancel;		/* Non-zero when job canceled */
  char			*filename;	/* Print file name */
  int			fd;		/* Print file descriptor */
  _ipp_printer_t	*printer;	/* Printer */
};

typedef struct _ipp_subscription_s	/**** Subscription data ****/
{
  int			id;		/* notify-subscription-id */
  const char		*uuid;		/* notify-subscription-uuid */
  _cups_rwlock_t	rwlock;		/* Subscription lock */
  _ipp_event_t		mask;		/* Event mask */
  _ipp_printer_t	*printer;	/* Printer */
  _ipp_job_t		*job;		/* Job, if any */
  ipp_t			*attrs;		/* Attributes */
  const char		*username;	/* notify-subscriber-user-name */
  int			lease;		/* notify-lease-duration */
  int			interval;	/* notify-time-interval */
  time_t		expire;		/* Lease expiration time */
  int			first_sequence,	/* First notify-sequence-number in cache */
			last_sequence;	/* Last notify-sequence-number used */
  cups_array_t		*events;	/* Events (ipp_t *'s) */
  int			pending_delete;	/* Non-zero when the subscription is about to be deleted/canceled */
} _ipp_subscription_t;

typedef struct _ipp_client_s		/**** Client data ****/
{
  http_t		*http;		/* HTTP connection */
  ipp_t			*request,	/* IPP request */
			*response;	/* IPP response */
  time_t		start;		/* Request start time */
  http_state_t		operation;	/* Request operation */
  ipp_op_t		operation_id;	/* IPP operation-id */
  char			uri[1024],	/* Request URI */
			*options;	/* URI options */
  http_addr_t		addr;		/* Client address */
  char			hostname[256],	/* Client hostname */
			username[32];	/* Client authenticated username */
  _ipp_printer_t	*printer;	/* Printer */
  _ipp_job_t		*job;		/* Current job, if any */
  int			fetch_compression,
					/* Compress file? */
			fetch_file;	/* File to fetch */
} _ipp_client_t;


/*
 * Local functions...
 */

static void		add_event(_ipp_printer_t *printer, _ipp_job_t *job, _ipp_event_t event, const char *message, ...) __attribute__((__format__(__printf__, 4, 5)));
static void		check_jobs(_ipp_printer_t *printer);
static void		clean_jobs(_ipp_printer_t *printer);
static int		compare_active_jobs(_ipp_job_t *a, _ipp_job_t *b);
static int		compare_completed_jobs(_ipp_job_t *a, _ipp_job_t *b);
static int		compare_devices(_ipp_device_t *a, _ipp_device_t *b);
static int		compare_jobs(_ipp_job_t *a, _ipp_job_t *b);
static void		copy_attributes(ipp_t *to, ipp_t *from, cups_array_t *ra,
			                ipp_tag_t group_tag, int quickcopy);
static void		copy_job_attributes(_ipp_client_t *client,
			                    _ipp_job_t *job, cups_array_t *ra);
static void		copy_job_state_reasons(ipp_t *ipp, ipp_tag_t group_tag, _ipp_job_t *job);
static void		copy_printer_state_reasons(ipp_t *ipp, ipp_tag_t group_tag, _ipp_printer_t *printer);
static void		copy_subscription_attributes(_ipp_client_t *client, _ipp_subscription_t *sub, cups_array_t *ra);
static _ipp_client_t	*create_client(_ipp_printer_t *printer, int sock);
static _ipp_device_t	*create_device(_ipp_client_t *client);
static _ipp_job_t	*create_job(_ipp_client_t *client);
static void		create_job_filename(_ipp_printer_t *printer, _ipp_job_t *job, const char *format, char *fname, size_t fnamesize);
static int		create_listener(int family, int port);
static _ipp_subscription_t *create_subscription(_ipp_printer_t *printer, _ipp_job_t *job, int interval, int lease, const char *username, ipp_attribute_t *notify_events, ipp_attribute_t *notify_attributes, ipp_attribute_t *notify_user_data);
static _ipp_printer_t	*create_printer(const char *servername, int port, const char *name, const char *directory, const char *proxy_user, const char *proxy_pass);
static void		debug_attributes(const char *title, ipp_t *ipp,
			                 int response);
static void		delete_client(_ipp_client_t *client);
static void		delete_device(_ipp_device_t *device);
static void		delete_job(_ipp_job_t *job);
static void		delete_printer(_ipp_printer_t *printer);
static void		delete_subscription(_ipp_subscription_t *sub);
static int		filter_cb(_ipp_filter_t *filter, ipp_t *dst, ipp_attribute_t *attr);
static _ipp_device_t	*find_device(_ipp_client_t *client);
static _ipp_job_t	*find_job(_ipp_client_t *client, int job_id);
static _ipp_subscription_t *find_subscription(_ipp_client_t *client, int sub_id);
static _ipp_jreason_t	get_job_state_reasons_bits(ipp_attribute_t *attr);
static _ipp_event_t	get_notify_events_bits(ipp_attribute_t *attr);
static const char	*get_notify_subscribed_event(_ipp_event_t event);
static _ipp_preason_t	get_printer_state_reasons_bits(ipp_attribute_t *attr);
static void		html_escape(_ipp_client_t *client, const char *s,
			            size_t slen);
static void		html_footer(_ipp_client_t *client);
static void		html_header(_ipp_client_t *client, const char *title);
static void		html_printf(_ipp_client_t *client, const char *format, ...) __attribute__((__format__(__printf__, 2, 3)));
static void		ipp_acknowledge_document(_ipp_client_t *client);
static void		ipp_acknowledge_identify_printer(_ipp_client_t *client);
static void		ipp_acknowledge_job(_ipp_client_t *client);
static void		ipp_cancel_job(_ipp_client_t *client);
static void		ipp_cancel_my_jobs(_ipp_client_t *client);
static void		ipp_cancel_subscription(_ipp_client_t *client);
static void		ipp_close_job(_ipp_client_t *client);
static void		ipp_create_job(_ipp_client_t *client);
static void		ipp_create_xxx_subscriptions(_ipp_client_t *client);
static void		ipp_deregister_output_device(_ipp_client_t *client);
static void		ipp_fetch_document(_ipp_client_t *client);
static void		ipp_fetch_job(_ipp_client_t *client);
static void		ipp_get_document_attributes(_ipp_client_t *client);
static void		ipp_get_documents(_ipp_client_t *client);
static void		ipp_get_job_attributes(_ipp_client_t *client);
static void		ipp_get_jobs(_ipp_client_t *client);
static void		ipp_get_notifications(_ipp_client_t *client);
static void		ipp_get_output_device_attributes(_ipp_client_t *client);
static void		ipp_get_printer_attributes(_ipp_client_t *client);
static void		ipp_get_printer_supported_values(_ipp_client_t *client);
static void		ipp_get_subscription_attributes(_ipp_client_t *client);
static void		ipp_get_subscriptions(_ipp_client_t *client);
static void		ipp_identify_printer(_ipp_client_t *client);
static void		ipp_print_job(_ipp_client_t *client);
static void		ipp_print_uri(_ipp_client_t *client);
static void		ipp_renew_subscription(_ipp_client_t *client);
static void		ipp_send_document(_ipp_client_t *client);
static void		ipp_send_uri(_ipp_client_t *client);
static void		ipp_update_active_jobs(_ipp_client_t *client);
static void		ipp_update_document_status(_ipp_client_t *client);
static void		ipp_update_job_status(_ipp_client_t *client);
static void		ipp_update_output_device_attributes(_ipp_client_t *client);
static void		ipp_validate_document(_ipp_client_t *client);
static void		ipp_validate_job(_ipp_client_t *client);
//static int		parse_options(_ipp_client_t *client, cups_option_t **options);
static void		*process_client(_ipp_client_t *client);
static int		process_http(_ipp_client_t *client);
static int		process_ipp(_ipp_client_t *client);
static void		*process_job(_ipp_job_t *job);
static int		respond_http(_ipp_client_t *client, http_status_t code,
				     const char *content_coding,
				     const char *type, size_t length);
static void		respond_ipp(_ipp_client_t *client, ipp_status_t status,
			            const char *message, ...)
			__attribute__ ((__format__ (__printf__, 3, 4)));
static void		respond_unsupported(_ipp_client_t *client,
			                    ipp_attribute_t *attr);
static void		run_printer(_ipp_printer_t *printer);
static char		*time_string(time_t tv, char *buffer, size_t bufsize);
static void		update_device_attributes_no_lock(_ipp_printer_t *printer);
static void		update_device_state_no_lock(_ipp_printer_t *printer);
static void		usage(int status) __attribute__((noreturn));
static int		valid_doc_attributes(_ipp_client_t *client);
static int		valid_job_attributes(_ipp_client_t *client);


/*
 * Globals...
 */

static int		KeepFiles = 0,
			Verbosity = 0;
//static _cups_mutex_t	SubscriptionMutex = _CUPS_MUTEX_INITIALIZER;
static _cups_cond_t	SubscriptionCondition = _CUPS_COND_INITIALIZER;


/*
 * 'main()' - Main entry to the sample infrastructure server.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  int		i;			/* Looping var */
  const char	*opt,			/* Current option character */
		*servername = NULL,	/* Server host name */
		*name = NULL;		/* Printer name */
#ifdef HAVE_SSL
  const char	*keypath = NULL;	/* Keychain path */
#endif /* HAVE_SSL */
  int		port = 0;		/* Port number (0 = auto) */
  char		directory[1024] = "",	/* Spool directory */
		hostname[1024],		/* Auto-detected hostname */
		proxy_user[256] = "",	/* Proxy username */
		*proxy_pass = NULL;	/* Proxy password */
  _ipp_printer_t *printer;		/* Printer object */


 /*
  * Parse command-line arguments...
  */

  for (i = 1; i < argc; i ++)
    if (argv[i][0] == '-')
    {
      for (opt = argv[i] + 1; *opt; opt ++)
      {
        switch (*opt)
	{
#ifdef HAVE_SSL
	  case 'K' : /* -K keypath */
	      i ++;
	      if (i >= argc)
	        usage(1);
	      keypath = argv[i];
	      break;
#endif /* HAVE_SSL */

	  case 'd' : /* -d spool-directory */
	      i ++;
	      if (i >= argc)
	        usage(1);
	      strlcpy(directory, argv[i], sizeof(directory));
	      break;

          case 'h' : /* -h (show help) */
	      usage(0);

	  case 'k' : /* -k (keep files) */
	      KeepFiles = 1;
	      break;

	  case 'n' : /* -n hostname */
	      i ++;
	      if (i >= argc)
	        usage(1);
	      servername = argv[i];
	      break;

	  case 'p' : /* -p port */
	      i ++;
	      if (i >= argc || !isdigit(argv[i][0] & 255))
	        usage(1);
	      port = atoi(argv[i]);
	      break;

          case 'u' : /* -u user:pass */
	      i ++;
	      if (i >= argc)
	        usage(1);
	      strlcpy(proxy_user, argv[i], sizeof(proxy_user));
	      if ((proxy_pass = strchr(proxy_user, ':')) != NULL)
	        *proxy_pass++ = '\0';
	      break;

	  case 'v' : /* -v (be verbose) */
	      Verbosity ++;
	      break;

          default : /* Unknown */
	      fprintf(stderr, "Unknown option \"-%c\".\n", *opt);
	      usage(1);
	}
      }
    }
    else if (!name)
    {
      name = argv[i];
    }
    else
    {
      fprintf(stderr, "Unexpected command-line argument \"%s\"\n", argv[i]);
      usage(1);
    }

  if (!name)
    usage(1);

 /*
  * Apply defaults as needed...
  */

  if (!servername)
    servername = httpGetHostname(NULL, hostname, sizeof(hostname));

  if (!port)
  {
#ifdef WIN32
   /*
    * Windows is almost always used as a single user system, so use a default port
    * number of 8631.
    */

    port = 8631;

#else
   /*
    * Use 8000 + UID mod 1000 for the default port number...
    */

    port = 8000 + ((int)getuid() % 1000);
#endif /* WIN32 */

    fprintf(stderr, "Listening on port %d.\n", port);
  }

  if (!directory[0])
  {
    snprintf(directory, sizeof(directory), "/tmp/ippserver.%d", (int)getpid());

    if (mkdir(directory, 0777) && errno != EEXIST)
    {
      fprintf(stderr, "Unable to create spool directory \"%s\": %s\n",
	      directory, strerror(errno));
      usage(1);
    }

    if (Verbosity)
      fprintf(stderr, "Using spool directory \"%s\".\n", directory);
  }

  if (!proxy_user[0])
  {
    strlcpy(proxy_user, "test", sizeof(proxy_user));

    if (Verbosity)
      fputs("Using proxy username \"test\".\n", stderr);
  }

  if (!proxy_pass)
  {
    proxy_pass = "test123";

    if (Verbosity)
      fputs("Using proxy password \"test123\".\n", stderr);
  }

#ifdef HAVE_SSL
  cupsSetServerCredentials(keypath, servername, 1);
#endif /* HAVE_SSL */

 /*
  * Create the printer...
  */

  if ((printer = create_printer(servername, port, name, directory, proxy_user, proxy_pass)) == NULL)
    return (1);

 /*
  * Run the print service...
  */

  run_printer(printer);

 /*
  * Destroy the printer and exit...
  */

  delete_printer(printer);

  return (0);
}


/*
 * 'add_event()' - Add an event to a subscription.
 */

static void
add_event(_ipp_printer_t *printer,	/* I - Printer */
          _ipp_job_t     *job,		/* I - Job, if any */
	  _ipp_event_t   event,		/* I - Event */
	  const char     *message,	/* I - Printf-style notify-text message */
	  ...)				/* I - Additional printf arguments */
{
  _ipp_subscription_t *sub;		/* Current subscription */
  ipp_t		*n;			/* Notify attributes */
  char		text[1024];		/* notify-text value */
  va_list	ap;			/* Argument pointer */


  va_start(ap, message);
  vsnprintf(text, sizeof(text), message, ap);
  va_end(ap);

  for (sub = (_ipp_subscription_t *)cupsArrayFirst(printer->subscriptions);
       sub;
       sub = (_ipp_subscription_t *)cupsArrayNext(printer->subscriptions))
  {
    if (sub->mask & event && (!sub->job || job == sub->job))
    {
      _cupsRWLockWrite(&sub->rwlock);

      n = ippNew();
      ippAddString(n, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_CHARSET, "notify-charset", NULL, "utf-8");
      ippAddString(n, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_LANGUAGE, "notify-natural-language", NULL, "en");
      ippAddInteger(n, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_INTEGER, "notify-printer-up-time", time(NULL) - printer->start_time);
      ippAddString(n, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_URI, "notify-printer-uri", NULL, printer->uri);
      if (job)
	ippAddInteger(n, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_INTEGER, "notify-job-id", job->id);
      ippAddInteger(n, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_INTEGER, "notify-subcription-id", sub->id);
      ippAddString(n, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_URI, "notify-subscription-uuid", NULL, sub->uuid);
      ippAddInteger(n, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_INTEGER, "notify-sequence-number", ++ sub->last_sequence);
      ippAddString(n, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_KEYWORD, "notify-subscribed-event", NULL, get_notify_subscribed_event(event));
      ippAddString(n, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_TEXT, "notify-text", NULL, text);
      if (event & _IPP_EVENT_PRINTER_ALL)
      {
	ippAddInteger(n, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_ENUM, "printer-state", printer->state);
	copy_printer_state_reasons(n, IPP_TAG_EVENT_NOTIFICATION, printer);
      }
      if (event & _IPP_EVENT_JOB_ALL)
      {
	ippAddInteger(n, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_ENUM, "job-state", job->state);
	copy_job_state_reasons(n, IPP_TAG_EVENT_NOTIFICATION, job);
	if (event == _IPP_EVENT_JOB_CREATED)
	{
	  ippAddString(n, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_NAME, "job-name", NULL, job->name);
	  ippAddString(n, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_NAME, "job-originating-user-name", NULL, job->username);
	}
      }

      cupsArrayAdd(sub->events, n);
      if (cupsArrayCount(sub->events) > 100)
      {
        n = (ipp_t *)cupsArrayFirst(sub->events);
	cupsArrayRemove(sub->events, n);
	ippDelete(n);
	sub->first_sequence ++;
      }

      _cupsRWUnlock(&sub->rwlock);
      _cupsCondBroadcast(&SubscriptionCondition);
    }
  }
}


/*
 * 'check_jobs()' - Check for new jobs to process.
 */

static void
check_jobs(_ipp_printer_t *printer)	/* I - Printer */
{
  _ipp_job_t	*job;			/* Current job */


  if (printer->processing_job)
    return;

  _cupsRWLockWrite(&(printer->rwlock));
  for (job = (_ipp_job_t *)cupsArrayFirst(printer->active_jobs);
       job;
       job = (_ipp_job_t *)cupsArrayNext(printer->active_jobs))
  {
    if (job->state == IPP_JSTATE_PENDING)
    {
      if (!_cupsThreadCreate((_cups_thread_func_t)process_job, job))
      {
        job->state     = IPP_JSTATE_ABORTED;
	job->completed = time(NULL);

        add_event(printer, job, _IPP_EVENT_JOB_COMPLETED, "Job aborted because creation of processing thread failed.");
      }
      break;
    }
  }
  _cupsRWUnlock(&(printer->rwlock));
}


/*
 * 'clean_jobs()' - Clean out old (completed) jobs.
 */

static void
clean_jobs(_ipp_printer_t *printer)	/* I - Printer */
{
  _ipp_job_t	*job;			/* Current job */
  time_t	cleantime;		/* Clean time */


  if (cupsArrayCount(printer->jobs) == 0)
    return;

  cleantime = time(NULL) - 60;

  _cupsRWLockWrite(&(printer->rwlock));
  for (job = (_ipp_job_t *)cupsArrayFirst(printer->jobs);
       job;
       job = (_ipp_job_t *)cupsArrayNext(printer->jobs))
    if (job->completed && job->completed < cleantime)
    {
      cupsArrayRemove(printer->jobs, job);
      delete_job(job);
    }
    else
      break;
  _cupsRWUnlock(&(printer->rwlock));
}


/*
 * 'compare_active_jobs()' - Compare two active jobs.
 */

static int				/* O - Result of comparison */
compare_active_jobs(_ipp_job_t *a,	/* I - First job */
                    _ipp_job_t *b)	/* I - Second job */
{
  int	diff;				/* Difference */


  if ((diff = b->priority - a->priority) == 0)
    diff = b->id - a->id;

  return (diff);
}


/*
 * 'compare_completed_jobs()' - Compare two completed jobs.
 */

static int				/* O - Result of comparison */
compare_completed_jobs(_ipp_job_t *a,	/* I - First job */
                       _ipp_job_t *b)	/* I - Second job */
{
  int	diff;				/* Difference */


  if ((diff = a->completed - b->completed) == 0)
    diff = b->id - a->id;

  return (diff);
}


/*
 * 'compare_devices()' - Compare two devices...
 */

static int				/* O - Result of comparison */
compare_devices(_ipp_device_t *a,	/* I - First device */
                _ipp_device_t *b)	/* I - Second device */
{
  return (strcmp(a->uuid, b->uuid));
}


/*
 * 'compare_jobs()' - Compare two jobs.
 */

static int				/* O - Result of comparison */
compare_jobs(_ipp_job_t *a,		/* I - First job */
             _ipp_job_t *b)		/* I - Second job */
{
  return (b->id - a->id);
}


/*
 * 'copy_attributes()' - Copy attributes from one request to another.
 */

static void
copy_attributes(ipp_t        *to,	/* I - Destination request */
	        ipp_t        *from,	/* I - Source request */
	        cups_array_t *ra,	/* I - Requested attributes */
	        ipp_tag_t    group_tag,	/* I - Group to copy */
	        int          quickcopy)	/* I - Do a quick copy? */
{
  _ipp_filter_t	filter;			/* Filter data */


  filter.ra        = ra;
  filter.group_tag = group_tag;

  ippCopyAttributes(to, from, quickcopy, (ipp_copycb_t)filter_cb, &filter);
}


/*
 * 'copy_job_attrs()' - Copy job attributes to the response.
 */

static void
copy_job_attributes(
    _ipp_client_t *client,		/* I - Client */
    _ipp_job_t    *job,			/* I - Job */
    cups_array_t  *ra)			/* I - requested-attributes */
{
  copy_attributes(client->response, job->attrs, ra, IPP_TAG_JOB, 0);

  if (!ra || cupsArrayFind(ra, "date-time-at-completed"))
  {
    if (job->completed)
      ippAddDate(client->response, IPP_TAG_JOB, "date-time-at-completed", ippTimeToDate(job->completed));
    else
      ippAddOutOfBand(client->response, IPP_TAG_JOB, IPP_TAG_NOVALUE, "date-time-at-completed");
  }

  if (!ra || cupsArrayFind(ra, "date-time-at-processing"))
  {
    if (job->processing)
      ippAddDate(client->response, IPP_TAG_JOB, "date-time-at-processing", ippTimeToDate(job->processing));
    else
      ippAddOutOfBand(client->response, IPP_TAG_JOB, IPP_TAG_NOVALUE, "date-time-at-processing");
  }

  if (!ra || cupsArrayFind(ra, "job-impressions"))
    ippAddInteger(client->response, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-impressions", job->impressions);

  if (!ra || cupsArrayFind(ra, "job-impressions-completed"))
    ippAddInteger(client->response, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-impressions-completed", job->impcompleted);

  if (!ra || cupsArrayFind(ra, "job-printer-up-time"))
    ippAddInteger(client->response, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-printer-up-time", (int)(time(NULL) - client->printer->start_time));

  if (!ra || cupsArrayFind(ra, "job-state"))
    ippAddInteger(client->response, IPP_TAG_JOB, IPP_TAG_ENUM,
		  "job-state", job->state);

  if (!ra || cupsArrayFind(ra, "job-state-message"))
  {
    if (job->dev_state_message)
    {
      ippAddString(client->response, IPP_TAG_JOB, IPP_TAG_TEXT, "job-state-message", NULL, job->dev_state_message);
    }
    else
    {
      const char *message = "";		/* Message string */

      switch (job->state)
      {
	case IPP_JSTATE_PENDING :
	    message = "Job pending.";
	    break;

	case IPP_JSTATE_HELD :
	    if (job->state_reasons & _IPP_JREASON_JOB_INCOMING)
	      message = "Job incoming.";
	    else if (ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_ZERO))
	      message = "Job held.";
	    else
	      message = "Job created.";
	    break;

	case IPP_JSTATE_PROCESSING :
	    if (job->state_reasons & _IPP_JREASON_PROCESSING_TO_STOP_POINT)
	    {
	      if (job->cancel)
		message = "Cancel in progress.";
	      else
	        message = "Abort in progress.";
	    }
	    else
	      message = "Job printing.";
	    break;

	case IPP_JSTATE_STOPPED :
	    message = "Job stopped.";
	    break;

	case IPP_JSTATE_CANCELED :
	    message = "Job canceled.";
	    break;

	case IPP_JSTATE_ABORTED :
	    message = "Job aborted.";
	    break;

	case IPP_JSTATE_COMPLETED :
	    message = "Job completed.";
	    break;
      }

      ippAddString(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_TEXT), "job-state-message", NULL, message);
    }
  }

  if (!ra || cupsArrayFind(ra, "job-state-reasons"))
    copy_job_state_reasons(client->response, IPP_TAG_JOB, job);
/*
    switch (job->state)
    {
      case IPP_JSTATE_PENDING :
	  ippAddString(client->response, IPP_TAG_JOB,
	               IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-state-reasons",
		       NULL, "none");
	  break;

      case IPP_JSTATE_HELD :
          if (job->fd >= 0)
	    ippAddString(client->response, IPP_TAG_JOB,
	                 IPP_CONST_TAG(IPP_TAG_KEYWORD),
	                 "job-state-reasons", NULL, "job-incoming");
	  else if (ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_ZERO))
	    ippAddString(client->response, IPP_TAG_JOB,
	                 IPP_CONST_TAG(IPP_TAG_KEYWORD),
	                 "job-state-reasons", NULL, "job-hold-until-specified");
          else
	    ippAddString(client->response, IPP_TAG_JOB,
	                 IPP_CONST_TAG(IPP_TAG_KEYWORD),
	                 "job-state-reasons", NULL, "job-data-insufficient");
	  break;

      case IPP_JSTATE_PROCESSING :
	  if (job->cancel)
	    ippAddString(client->response, IPP_TAG_JOB,
	                 IPP_CONST_TAG(IPP_TAG_KEYWORD),
	                 "job-state-reasons", NULL, "processing-to-stop-point");
	  else
	    ippAddString(client->response, IPP_TAG_JOB,
	                 IPP_CONST_TAG(IPP_TAG_KEYWORD),
	                 "job-state-reasons", NULL, "job-printing");
	  break;

      case IPP_JSTATE_STOPPED :
	  ippAddString(client->response, IPP_TAG_JOB,
	               IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-state-reasons",
		       NULL, "job-stopped");
	  break;

      case IPP_JSTATE_CANCELED :
	  ippAddString(client->response, IPP_TAG_JOB,
	               IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-state-reasons",
		       NULL, "job-canceled-by-user");
	  break;

      case IPP_JSTATE_ABORTED :
	  ippAddString(client->response, IPP_TAG_JOB,
	               IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-state-reasons",
		       NULL, "aborted-by-system");
	  break;

      case IPP_JSTATE_COMPLETED :
	  ippAddString(client->response, IPP_TAG_JOB,
	               IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-state-reasons",
		       NULL, "job-completed-successfully");
	  break;
    }
*/

  if (!ra || cupsArrayFind(ra, "time-at-completed"))
    ippAddInteger(client->response, IPP_TAG_JOB,
                  job->completed ? IPP_TAG_INTEGER : IPP_TAG_NOVALUE,
                  "time-at-completed", (int)(job->completed - client->printer->start_time));

  if (!ra || cupsArrayFind(ra, "time-at-processing"))
    ippAddInteger(client->response, IPP_TAG_JOB,
                  job->processing ? IPP_TAG_INTEGER : IPP_TAG_NOVALUE,
                  "time-at-processing", (int)(job->processing - client->printer->start_time));
}


/*
 * 'copy_job_state_reasons()' - Copy printer-state-reasons values.
 */

static void
copy_job_state_reasons(
    ipp_t      *ipp,			/* I - Attributes */
    ipp_tag_t  group_tag,		/* I - Group */
    _ipp_job_t *job)			/* I - Printer */
{
  _ipp_jreason_t	creasons;	/* Combined job-state-reasons */


  creasons = job->state_reasons | job->dev_state_reasons;

  if (!creasons)
  {
    ippAddString(ipp, group_tag, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-state-reasons", NULL, "none");
  }
  else
  {
    int			i,		/* Looping var */
			num_reasons = 0;/* Number of reasons */
    _ipp_jreason_t	reason;		/* Current reason */
    const char		*reasons[32];	/* Reason strings */

    for (i = 0, reason = 1; i < (int)(sizeof(_ipp_jreasons) / sizeof(_ipp_jreasons[0])); i ++, reason <<= 1)
    {
      if (creasons & reason)
        reasons[num_reasons ++] = _ipp_jreasons[i];
    }

    ippAddStrings(ipp, group_tag, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-state-reasons", num_reasons, NULL, reasons);
  }
}


/*
 * 'copy_printer_state_reasons()' - Copy printer-state-reasons values.
 */

static void
copy_printer_state_reasons(
    ipp_t          *ipp,		/* I - Attributes */
    ipp_tag_t      group_tag,		/* I - Group */
    _ipp_printer_t *printer)		/* I - Printer */
{
  _ipp_preason_t	creasons = printer->state_reasons | printer->dev_reasons;
					/* Combined reasons */


  if (creasons == _IPP_PREASON_NONE)
  {
    ippAddString(ipp, group_tag, IPP_CONST_TAG(IPP_TAG_KEYWORD), "printer-state-reasons", NULL, "none");
  }
  else
  {
    int			i,		/* Looping var */				num_reasons = 0;/* Number of reasons */
    _ipp_preason_t	reason;		/* Current reason */
    const char		*reasons[32];	/* Reason strings */

    for (i = 0, reason = 1; i < (int)(sizeof(_ipp_preasons) / sizeof(_ipp_preasons[0])); i ++, reason <<= 1)
    {
      if (creasons & reason)
	reasons[num_reasons ++] = _ipp_preasons[i];
    }

    ippAddStrings(ipp, group_tag, IPP_CONST_TAG(IPP_TAG_KEYWORD), "printer-state-reasons", num_reasons, NULL, reasons);
  }
}


/*
 * 'copy_sub_attrs()' - Copy job attributes to the response.
 */

static void
copy_subscription_attributes(
    _ipp_client_t       *client,	/* I - Client */
    _ipp_subscription_t *sub,		/* I - Subscription */
    cups_array_t        *ra)		/* I - requested-attributes */
{
  copy_attributes(client->response, sub->attrs, ra, IPP_TAG_SUBSCRIPTION, 0);

  if (!ra || cupsArrayFind(ra, "notify-lease-expiration-time"))
    ippAddInteger(client->response, IPP_TAG_SUBSCRIPTION, IPP_TAG_INTEGER, "notify-lease-expiration-time", (int)(sub->expire - client->printer->start_time));

  if (!ra || cupsArrayFind(ra, "notify-printer-up-time"))
    ippAddInteger(client->response, IPP_TAG_SUBSCRIPTION, IPP_TAG_INTEGER, "notify-printer-up-time", (int)(time(NULL) - client->printer->start_time));

  if (!ra || cupsArrayFind(ra, "notify-sequence-number"))
    ippAddInteger(client->response, IPP_TAG_SUBSCRIPTION, IPP_TAG_INTEGER, "notify-sequence-number", sub->last_sequence);
}


/*
 * 'create_client()' - Accept a new network connection and create a client
 *                     object.
 */

static _ipp_client_t *			/* O - Client */
create_client(_ipp_printer_t *printer,	/* I - Printer */
              int            sock)	/* I - Listen socket */
{
  _ipp_client_t	*client;		/* Client */


  if ((client = calloc(1, sizeof(_ipp_client_t))) == NULL)
  {
    perror("Unable to allocate memory for client");
    return (NULL);
  }

  client->printer    = printer;
  client->fetch_file = -1;

 /*
  * Accept the client and get the remote address...
  */

  if ((client->http = httpAcceptConnection(sock, 1)) == NULL)
  {
    perror("Unable to accept client connection");

    free(client);

    return (NULL);
  }

  httpGetHostname(client->http, client->hostname, sizeof(client->hostname));

  if (Verbosity)
    fprintf(stderr, "Accepted connection from %s\n", client->hostname);

  return (client);
}


/*
 * 'create_device()' - Create an output device tracking object.
 */

static _ipp_device_t *			/* O - Device */
create_device(_ipp_client_t *client)	/* I - Client */
{
  _ipp_device_t		*device;	/* Device */
  ipp_attribute_t	*uuid;		/* output-device-uuid */


  if ((uuid = ippFindAttribute(client->request, "output-device-uuid", IPP_TAG_URI)) == NULL)
    return (NULL);

  if ((device = calloc(1, sizeof(_ipp_device_t))) == NULL)
    return (NULL);

  _cupsRWInit(&device->rwlock);

  device->uuid  = strdup(ippGetString(uuid, 0, NULL));
  device->state = IPP_PSTATE_STOPPED;

  _cupsRWLockWrite(&client->printer->rwlock);
  cupsArrayAdd(client->printer->devices, device);
  _cupsRWUnlock(&client->printer->rwlock);

  return (device);
}


/*
 * 'create_job()' - Create a new job object from a Print-Job or Create-Job
 *                  request.
 */

static _ipp_job_t *			/* O - Job */
create_job(_ipp_client_t *client)	/* I - Client */
{
  _ipp_job_t		*job;		/* Job */
  ipp_attribute_t	*attr;		/* Job attribute */
  char			uri[1024],	/* job-uri value */
			uuid[64];	/* job-uuid value */


  _cupsRWLockWrite(&(client->printer->rwlock));

 /*
  * Allocate and initialize the job object...
  */

  if ((job = calloc(1, sizeof(_ipp_job_t))) == NULL)
  {
    perror("Unable to allocate memory for job");
    return (NULL);
  }

  job->printer    = client->printer;
  job->attrs      = ippNew();
  job->state      = IPP_JSTATE_HELD;
  job->fd         = -1;

 /*
  * Copy all of the job attributes...
  */

  copy_attributes(job->attrs, client->request, NULL, IPP_TAG_JOB, 0);

 /*
  * Get the requesting-user-name, document format, and priority...
  */

  if ((attr = ippFindAttribute(client->request, "job-priority", IPP_TAG_INTEGER)) != NULL)
    job->priority = ippGetInteger(attr, 0);
  else
    job->priority = 50;

  if ((attr = ippFindAttribute(client->request, "requesting-user-name", IPP_TAG_NAME)) != NULL)
    job->username = ippGetString(attr, 0, NULL);
  else
    job->username = "anonymous";

  ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_NAME, "job-originating-user-name", NULL, job->username);

  if (ippGetOperation(client->request) != IPP_OP_CREATE_JOB)
  {
    if ((attr = ippFindAttribute(job->attrs, "document-format-detected", IPP_TAG_MIMETYPE)) != NULL)
      job->format = ippGetString(attr, 0, NULL);
    else if ((attr = ippFindAttribute(job->attrs, "document-format-supplied", IPP_TAG_MIMETYPE)) != NULL)
      job->format = ippGetString(attr, 0, NULL);
    else
      job->format = "application/octet-stream";
  }

  if ((attr = ippFindAttribute(client->request, "job-impressions", IPP_TAG_INTEGER)) != NULL)
    job->impressions = ippGetInteger(attr, 0);

  if ((attr = ippFindAttribute(client->request, "job-name", IPP_TAG_NAME)) != NULL)
    job->name = ippGetString(attr, 0, NULL);

 /*
  * Add job description attributes and add to the jobs array...
  */

  job->id = client->printer->next_job_id ++;

  snprintf(uri, sizeof(uri), "%s/%d", client->printer->uri, job->id);
  httpAssembleUUID(client->printer->hostname, client->printer->port, client->printer->name, job->id, uuid, sizeof(uuid));

  ippAddDate(job->attrs, IPP_TAG_JOB, "date-time-at-creation", ippTimeToDate(time(&job->created)));
  ippAddInteger(job->attrs, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-id", job->id);
  ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_URI, "job-uri", NULL, uri);
  ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_URI, "job-uuid", NULL, uuid);
  ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_URI, "job-printer-uri", NULL, client->printer->uri);
  ippAddInteger(job->attrs, IPP_TAG_JOB, IPP_TAG_INTEGER, "time-at-creation", (int)(job->created - client->printer->start_time));

  cupsArrayAdd(client->printer->jobs, job);
  cupsArrayAdd(client->printer->active_jobs, job);

  _cupsRWUnlock(&(client->printer->rwlock));

  return (job);
}


/*
 * 'create_job_filename()' - Create the filename for a document in a job.
 */

static void create_job_filename(
    _ipp_printer_t *printer,		/* I - Printer */
    _ipp_job_t     *job,		/* I - Job */
    const char     *format,		/* I - Format or NULL */
    char           *fname,		/* I - Filename buffer */
    size_t         fnamesize)		/* I - Size of filename buffer */
{
  char			name[256],	/* "Safe" filename */
			*nameptr;	/* Pointer into filename */
  const char		*ext,		/* Filename extension */
			*job_name;	/* job-name value */
  ipp_attribute_t	*job_name_attr;	/* job-name attribute */


 /*
  * Make a name from the job-name attribute...
  */

  if ((job_name_attr = ippFindAttribute(job->attrs, "job-name", IPP_TAG_NAME)) != NULL)
    job_name = ippGetString(job_name_attr, 0, NULL);
  else
    job_name = "untitled";

  for (nameptr = name; *job_name && nameptr < (name + sizeof(name) - 1); job_name ++)
    if (isalnum(*job_name & 255) || *job_name == '-')
      *nameptr++ = (char)tolower(*job_name & 255);
    else
      *nameptr++ = '_';

  *nameptr = '\0';

 /*
  * Figure out the extension...
  */

  if (!format)
    format = job->format;

  if (!strcasecmp(format, "image/jpeg"))
    ext = "jpg";
  else if (!strcasecmp(format, "image/png"))
    ext = "png";
  else if (!strcasecmp(format, "image/pwg-raster"))
    ext = "ras";
  else if (!strcasecmp(format, "image/urf"))
    ext = "urf";
  else if (!strcasecmp(format, "application/pdf"))
    ext = "pdf";
  else if (!strcasecmp(format, "application/postscript"))
    ext = "ps";
  else
    ext = "prn";

 /*
  * Create a filename with the job-id, job-name, and document-format (extension)...
  */

  snprintf(fname, fnamesize, "%s/%d-%s.%s", printer->directory, job->id, name, ext);
}


/*
 * 'create_listener()' - Create a listener socket.
 */

static int				/* O - Listener socket or -1 on error */
create_listener(int family,		/* I - Address family */
                int port)		/* I - Port number */
{
  int			sock;		/* Listener socket */
  http_addrlist_t	*addrlist;	/* Listen address */
  char			service[255];	/* Service port */


  snprintf(service, sizeof(service), "%d", port);
  if ((addrlist = httpAddrGetList(NULL, family, service)) == NULL)
    return (-1);

  sock = httpAddrListen(&(addrlist->addr), port);

  httpAddrFreeList(addrlist);

  return (sock);
}


/*
 * 'create_printer()' - Create, register, and listen for connections to a
 *                      printer object.
 */

static _ipp_printer_t *			/* O - Printer */
create_printer(const char *servername,	/* I - Server hostname (NULL for default) */
               int        port,		/* I - Port number */
               const char *name,	/* I - printer-name */
	       const char *directory,	/* I - Spool directory */
	       const char *proxy_user,	/* I - Proxy account username */
	       const char *proxy_pass)	/* I - Proxy account password */
{
  _ipp_printer_t	*printer;	/* Printer */
  char			uri[1024],	/* Printer URI */
			adminurl[1024],	/* printer-more-info URI */
			supplyurl[1024],/* printer-supply-info-uri URI */
			uuid[128];	/* printer-uuid */
  int			k_supported;	/* Maximum file size supported */
#ifdef HAVE_STATVFS
  struct statvfs	spoolinfo;	/* FS info for spool directory */
  double		spoolsize;	/* FS size */
#elif defined(HAVE_STATFS)
  struct statfs		spoolinfo;	/* FS info for spool directory */
  double		spoolsize;	/* FS size */
#endif /* HAVE_STATVFS */
  static const char * const versions[] =/* ipp-versions-supported values */
  {
    "1.0",
    "1.1",
    "2.0"
  };
  static const char * const features[] =/* ipp-features-supported values */
  {
    "document-object",
    "ipp-everywhere",
    "infrastructure-printer",
    "page-overrides"
  };
  static const int	ops[] =		/* operations-supported values */
  {
    IPP_OP_PRINT_JOB,
    IPP_OP_PRINT_URI,
    IPP_OP_VALIDATE_JOB,
    IPP_OP_CREATE_JOB,
    IPP_OP_SEND_DOCUMENT,
    IPP_OP_SEND_URI,
    IPP_OP_CANCEL_JOB,
    IPP_OP_GET_JOB_ATTRIBUTES,
    IPP_OP_GET_JOBS,
    IPP_OP_GET_PRINTER_ATTRIBUTES,
    IPP_OP_GET_PRINTER_SUPPORTED_VALUES,
    IPP_OP_CREATE_PRINTER_SUBSCRIPTIONS,
    IPP_OP_CREATE_JOB_SUBSCRIPTIONS,
    IPP_OP_GET_SUBSCRIPTION_ATTRIBUTES,
    IPP_OP_GET_SUBSCRIPTIONS,
    IPP_OP_RENEW_SUBSCRIPTION,
    IPP_OP_CANCEL_SUBSCRIPTION,
    IPP_OP_GET_NOTIFICATIONS,
    IPP_OP_GET_DOCUMENT_ATTRIBUTES,
    IPP_OP_GET_DOCUMENTS,
    IPP_OP_CANCEL_MY_JOBS,
    IPP_OP_CLOSE_JOB,
    IPP_OP_IDENTIFY_PRINTER,
    IPP_OP_VALIDATE_DOCUMENT,
    _IPP_OP_ACKNOWLEDGE_DOCUMENT,
    _IPP_OP_ACKNOWLEDGE_IDENTIFY_PRINTER,
    _IPP_OP_ACKNOWLEDGE_JOB,
    _IPP_OP_FETCH_DOCUMENT,
    _IPP_OP_FETCH_JOB,
    _IPP_OP_GET_OUTPUT_DEVICE_ATTRIBUTES,
    _IPP_OP_UPDATE_ACTIVE_JOBS,
    _IPP_OP_UPDATE_DOCUMENT_STATUS,
    _IPP_OP_UPDATE_JOB_STATUS,
    _IPP_OP_UPDATE_OUTPUT_DEVICE_ATTRIBUTES,
    _IPP_OP_DEREGISTER_OUTPUT_DEVICE
  };
  static const char * const charsets[] =/* charset-supported values */
  {
    "us-ascii",
    "utf-8"
  };
  static const char * const compressions[] =/* compression-supported values */
  {
#ifdef HAVE_LIBZ
    "deflate",
    "gzip",
#endif /* HAVE_LIBZ */
    "none"
  };
  static const char * const notify_attributes[] =
  {					/* notify-attributes-supported */
    "printer-state-change-time",
    "notify-lease-expiration-time",
    "notify-subscriber-user-name"
  };
  static const char * const reference_uri_schemes_supported[] =
  {					/* reference-uri-schemes-supported */
    "file",
    "ftp",
    "http"
#ifdef HAVE_SSL
    , "https"
#endif /* HAVE_SSL */
  };
  static const char * const which_jobs[] =
  {					/* which-jobs-supported values */
    "completed",
    "not-completed",
    "aborted",
    "all",
    "canceled",
    "pending",
    "pending-held",
    "processing",
    "processing-stopped"
  };


 /*
  * Allocate memory for the printer...
  */

  if ((printer = calloc(1, sizeof(_ipp_printer_t))) == NULL)
  {
    perror("ippserver: Unable to allocate memory for printer");
    return (NULL);
  }

  printer->ipv4           = -1;
  printer->ipv6           = -1;
  printer->name           = strdup(name);
  printer->directory      = strdup(directory);
  printer->hostname       = strdup(servername);
  printer->port           = port;
  printer->start_time     = time(NULL);
  printer->config_time    = printer->start_time;
  printer->state          = IPP_PSTATE_IDLE;
  printer->state_reasons  = _IPP_PREASON_NONE;
  printer->state_time     = printer->start_time;
  printer->jobs           = cupsArrayNew3((cups_array_func_t)compare_jobs, NULL, NULL, 0, NULL, (cups_afree_func_t)delete_job);
  printer->active_jobs    = cupsArrayNew((cups_array_func_t)compare_active_jobs, NULL);
  printer->completed_jobs = cupsArrayNew((cups_array_func_t)compare_completed_jobs, NULL);
  printer->next_job_id    = 1;

  httpAssembleURI(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
		  printer->hostname, printer->port, "/ipp/print");
  printer->uri    = strdup(uri);
  printer->urilen = strlen(uri);

  if (proxy_user)
    printer->proxy_user = strdup(proxy_user);
  if (proxy_pass)
    printer->proxy_pass = strdup(proxy_pass);

  printer->devices = cupsArrayNew((cups_array_func_t)compare_devices, NULL);

  _cupsRWInit(&(printer->rwlock));

 /*
  * Create the listener sockets...
  */

  if ((printer->ipv4 = create_listener(AF_INET, printer->port)) < 0)
  {
    perror("Unable to create IPv4 listener");
    goto bad_printer;
  }

  if ((printer->ipv6 = create_listener(AF_INET6, printer->port)) < 0)
  {
    perror("Unable to create IPv6 listener");
    goto bad_printer;
  }

 /*
  * Prepare values for the printer attributes...
  */

  httpAssembleURI(HTTP_URI_CODING_ALL, adminurl, sizeof(adminurl), "http", NULL, printer->hostname, printer->port, "/");
  httpAssembleURI(HTTP_URI_CODING_ALL, supplyurl, sizeof(supplyurl), "http", NULL, printer->hostname, printer->port, "/supplies");

  if (Verbosity)
  {
    fprintf(stderr, "printer-more-info=\"%s\"\n", adminurl);
    fprintf(stderr, "printer-supply-info-uri=\"%s\"\n", supplyurl);
    fprintf(stderr, "printer-uri=\"%s\"\n", uri);
  }

 /*
  * Get the maximum spool size based on the size of the filesystem used for
  * the spool directory.  If the host OS doesn't support the statfs call
  * or the filesystem is larger than 2TiB, always report INT_MAX.
  */

#ifdef HAVE_STATVFS
  if (statvfs(printer->directory, &spoolinfo))
    k_supported = INT_MAX;
  else if ((spoolsize = (double)spoolinfo.f_frsize *
                        spoolinfo.f_blocks / 1024) > INT_MAX)
    k_supported = INT_MAX;
  else
    k_supported = (int)spoolsize;

#elif defined(HAVE_STATFS)
  if (statfs(printer->directory, &spoolinfo))
    k_supported = INT_MAX;
  else if ((spoolsize = (double)spoolinfo.f_bsize *
                        spoolinfo.f_blocks / 1024) > INT_MAX)
    k_supported = INT_MAX;
  else
    k_supported = (int)spoolsize;

#else
  k_supported = INT_MAX;
#endif /* HAVE_STATVFS */

 /*
  * Create the printer attributes.  This list of attributes is sorted to improve
  * performance when the client provides a requested-attributes attribute...
  */

  printer->attrs = ippNew();

  /* charset-configured */
  ippAddString(printer->attrs, IPP_TAG_PRINTER,
               IPP_CONST_TAG(IPP_TAG_CHARSET),
               "charset-configured", NULL, "utf-8");

  /* charset-supported */
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER,
                IPP_CONST_TAG(IPP_TAG_CHARSET),
                "charset-supported", sizeof(charsets) / sizeof(charsets[0]),
		NULL, charsets);

  /* compression-supported */
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER,
                IPP_CONST_TAG(IPP_TAG_KEYWORD),
	        "compression-supported",
	        (int)(sizeof(compressions) / sizeof(compressions[0])), NULL,
	        compressions);

  /* generated-natural-language-supported */
  ippAddString(printer->attrs, IPP_TAG_PRINTER,
               IPP_CONST_TAG(IPP_TAG_LANGUAGE),
               "generated-natural-language-supported", NULL, "en");

  /* ipp-features-supported */
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "ipp-features-supported", sizeof(features) / sizeof(features[0]), NULL, features);

  /* ipp-versions-supported */
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "ipp-versions-supported", sizeof(versions) / sizeof(versions[0]), NULL, versions);

  /* ippget-event-life */
  ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "ippget-event-life", 300);

  /* job-ids-supported */
  ippAddBoolean(printer->attrs, IPP_TAG_PRINTER, "job-ids-supported", 1);

  /* job-k-octets-supported */
  ippAddRange(printer->attrs, IPP_TAG_PRINTER, "job-k-octets-supported", 0,
	      k_supported);

  /* job-priority-default */
  ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                "job-priority-default", 50);

  /* job-priority-supported */
  ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                "job-priority-supported", 100);

  /* multiple-document-jobs-supported */
  ippAddBoolean(printer->attrs, IPP_TAG_PRINTER, "multiple-document-jobs-supported", 0);

  /* multiple-operation-time-out */
  ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "multiple-operation-time-out", 60);

  /* multiple-operation-time-out-action */
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "multiple-operation-time-out-action", NULL, "abort-job");

  /* natural-language-configured */
  ippAddString(printer->attrs, IPP_TAG_PRINTER,
               IPP_CONST_TAG(IPP_TAG_LANGUAGE),
               "natural-language-configured", NULL, "en");

  /* notify-attributes-supported */
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "notify-attributes-supported", sizeof(notify_attributes) / sizeof(notify_attributes[0]), NULL, notify_attributes);

  /* notify-events-default */
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "notify-events-default", NULL, "job-completed");

  /* notify-events-supported */
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "notify-events-supported", sizeof(_ipp_events) / sizeof(_ipp_events[0]), NULL, _ipp_events);

  /* notify-lease-duration-default */
  ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "notify-lease-duration-default", 86400);

  /* notify-lease-duration-supported */
  ippAddRange(printer->attrs, IPP_TAG_PRINTER, "notify-lease-duration-supported", 0, _IPP_NOTIFY_LEASE_DURATION_MAX);

  /* notify-max-events-supported */
  ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "notify-lease-duration-default", (int)(sizeof(_ipp_events) / sizeof(_ipp_events[0])));

  /* notify-pull-method-supported */
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "notify-pull-method-supported", NULL, "ippget");

  /* operations-supported */
  ippAddIntegers(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM,
		 "operations-supported", sizeof(ops) / sizeof(ops[0]), ops);

  /* printer-get-attributes-supported */
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "printer-get-attributes-supported", NULL, "document-format");

  /* printer-is-accepting-jobs */
  ippAddBoolean(printer->attrs, IPP_TAG_PRINTER, "printer-is-accepting-jobs", 1);

  /* printer-info */
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-info", NULL, name);

  /* printer-more-info */
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-more-info", NULL, adminurl);

  /* printer-name */
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_NAME, "printer-name", NULL, name);

  /* printer-supply-info-uri */
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-supply-info-uri", NULL, supplyurl);

  /* printer-uri-supported */
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-uri-supported", NULL, uri);

  /* printer-uuid */
  httpAssembleUUID(printer->hostname, port, name, 0, uuid, sizeof(uuid));
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-uuid", NULL, uuid);

  /* reference-uri-scheme-supported */
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER,
                IPP_CONST_TAG(IPP_TAG_URISCHEME),
                "reference-uri-schemes-supported",
                (int)(sizeof(reference_uri_schemes_supported) /
                      sizeof(reference_uri_schemes_supported[0])),
                NULL, reference_uri_schemes_supported);

  /* uri-authentication-supported */
  ippAddString(printer->attrs, IPP_TAG_PRINTER,
               IPP_CONST_TAG(IPP_TAG_KEYWORD),
               "uri-authentication-supported", NULL, "basic");

  /* uri-security-supported */
  ippAddString(printer->attrs, IPP_TAG_PRINTER,
               IPP_CONST_TAG(IPP_TAG_KEYWORD),
               "uri-security-supported", NULL, "tls");

  /* which-jobs-supported */
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER,
                IPP_CONST_TAG(IPP_TAG_KEYWORD),
                "which-jobs-supported",
                sizeof(which_jobs) / sizeof(which_jobs[0]), NULL, which_jobs);

  debug_attributes("Printer", printer->attrs, 0);

 /*
  * Return it!
  */

  return (printer);


 /*
  * If we get here we were unable to create the printer...
  */

  bad_printer:

  delete_printer(printer);
  return (NULL);
}


/*
 * 'create_subscription()' - Create a new subscription object from a
 *                           Print-Job, Create-Job, or Create-xxx-Subscription
 *                           request.
 */

static _ipp_subscription_t *		/* O - Subscription object */
create_subscription(
    _ipp_printer_t  *printer,		/* I - Printer */
    _ipp_job_t      *job,		/* I - Job, if any */
    int             interval,		/* I - Interval for progress events */
    int             lease,		/* I - Lease duration */
    const char      *username,		/* I - User creating the subscription */
    ipp_attribute_t *notify_events,	/* I - Events to monitor */
    ipp_attribute_t *notify_attributes,	/* I - Attributes to report */
    ipp_attribute_t *notify_user_data)	/* I - User data, if any */
{
  _ipp_subscription_t	*sub;		/* Subscription */
  ipp_attribute_t	*attr;		/* Subscription attribute */
  char			uuid[64];	/* notify-subscription-uuid value */


 /*
  * Allocate and initialize the subscription object...
  */

  if ((sub = calloc(1, sizeof(_ipp_subscription_t))) == NULL)
  {
    perror("Unable to allocate memory for subscription");
    return (NULL);
  }

  _cupsRWLockWrite(&(printer->rwlock));

  sub->id       = printer->next_sub_id ++;
  sub->mask     = notify_events ? get_notify_events_bits(notify_events) : _IPP_EVENT_DEFAULT;
  sub->printer  = printer;
  sub->job      = job;
  sub->interval = interval;
  sub->lease    = lease;
  sub->attrs    = ippNew();

  if (lease)
    sub->expire = time(NULL) + sub->lease;
  else
    sub->expire = INT_MAX;

  _cupsRWInit(&(sub->rwlock));

 /*
  * Add subscription description attributes and add to the subscriptions
  * array...
  */

  ippAddInteger(sub->attrs, IPP_TAG_SUBSCRIPTION, IPP_TAG_INTEGER, "notify-subscription-id", sub->id);

  httpAssembleUUID(printer->hostname, printer->port, printer->name, -sub->id, uuid, sizeof(uuid));
  attr = ippAddString(sub->attrs, IPP_TAG_SUBSCRIPTION, IPP_TAG_URI, "notify-subscription-uuid", NULL, uuid);
  sub->uuid = ippGetString(attr, 0, NULL);

  ippAddString(sub->attrs, IPP_TAG_SUBSCRIPTION, IPP_TAG_URI, "notify-printer-uri", NULL, printer->uri);

  if (job)
    ippAddInteger(sub->attrs, IPP_TAG_SUBSCRIPTION, IPP_TAG_INTEGER, "notify-job-id", job->id);
  else
    ippAddInteger(sub->attrs, IPP_TAG_SUBSCRIPTION, IPP_TAG_INTEGER, "notify-lease-duration", sub->lease);

  attr = ippAddString(sub->attrs, IPP_TAG_SUBSCRIPTION, IPP_TAG_NAME, "notify-subscriber-user-name", NULL, username);
  sub->username = ippGetString(attr, 0, NULL);

  if (notify_events)
    ippCopyAttribute(sub->attrs, notify_events, 0);
  else
    ippAddString(sub->attrs, IPP_TAG_SUBSCRIPTION, IPP_CONST_TAG(IPP_TAG_KEYWORD), "notify-events", NULL, _IPP_EVENT_DEFAULT_STRING);

  ippAddString(sub->attrs, IPP_TAG_SUBSCRIPTION, IPP_CONST_TAG(IPP_TAG_KEYWORD), "notify-pull-method", NULL, "ippget");

  if (notify_attributes)
    ippCopyAttribute(sub->attrs, notify_attributes, 0);

  if (notify_user_data)
    ippCopyAttribute(sub->attrs, notify_user_data, 0);

  sub->events = cupsArrayNew3(NULL, NULL, NULL, 0, NULL, (cups_afree_func_t)ippDelete);

  cupsArrayAdd(printer->subscriptions, sub);

  _cupsRWUnlock(&(printer->rwlock));

  return (sub);
}


/*
 * 'debug_attributes()' - Print attributes in a request or response.
 */

static void
debug_attributes(const char *title,	/* I - Title */
                 ipp_t      *ipp,	/* I - Request/response */
                 int        type)	/* I - 0 = object, 1 = request, 2 = response */
{
  ipp_tag_t		group_tag;	/* Current group */
  ipp_attribute_t	*attr;		/* Current attribute */
  char			buffer[2048];	/* String buffer for value */
  int			major, minor;	/* Version */


  if (Verbosity <= 1)
    return;

  fprintf(stderr, "%s:\n", title);
  major = ippGetVersion(ipp, &minor);
  fprintf(stderr, "  version=%d.%d\n", major, minor);
  if (type == 1)
    fprintf(stderr, "  operation-id=%s(%04x)\n",
            ippOpString(ippGetOperation(ipp)), ippGetOperation(ipp));
  else if (type == 2)
    fprintf(stderr, "  status-code=%s(%04x)\n",
            ippErrorString(ippGetStatusCode(ipp)), ippGetStatusCode(ipp));
  fprintf(stderr, "  request-id=%d\n\n", ippGetRequestId(ipp));

  for (attr = ippFirstAttribute(ipp), group_tag = IPP_TAG_ZERO;
       attr;
       attr = ippNextAttribute(ipp))
  {
    if (ippGetGroupTag(attr) != group_tag)
    {
      group_tag = ippGetGroupTag(attr);
      fprintf(stderr, "  %s\n", ippTagString(group_tag));
    }

    if (ippGetName(attr))
    {
      ippAttributeString(attr, buffer, sizeof(buffer));
      fprintf(stderr, "    %s (%s%s) %s\n", ippGetName(attr),
	      ippGetCount(attr) > 1 ? "1setOf " : "",
	      ippTagString(ippGetValueTag(attr)), buffer);
    }
  }
}


/*
 * 'delete_client()' - Close the socket and free all memory used by a client
 *                     object.
 */

static void
delete_client(_ipp_client_t *client)	/* I - Client */
{
  if (Verbosity)
    fprintf(stderr, "Closing connection from %s\n", client->hostname);

 /*
  * Flush pending writes before closing...
  */

  httpFlushWrite(client->http);

 /*
  * Free memory...
  */

  httpClose(client->http);

  ippDelete(client->request);
  ippDelete(client->response);

  free(client);
}


/*
 * 'delete_device()' - Remove a device from a printer.
 *
 * Note: Caller is responsible for locking the printer object.
 */

static void
delete_device(_ipp_device_t *device)	/* I - Device */
{
 /*
  * Free memory used for the device...
  */

  _cupsRWDeinit(&device->rwlock);

  if (device->name)
    free(device->name);

  free(device->uuid);

  ippDelete(device->attrs);

  free(device);
}


/*
 * 'delete_job()' - Remove from the printer and free all memory used by a job
 *                  object.
 */

static void
delete_job(_ipp_job_t *job)		/* I - Job */
{
  if (Verbosity)
    fprintf(stderr, "Removing job #%d from history.\n", job->id);

  _cupsRWLockWrite(&job->rwlock);

  ippDelete(job->attrs);

  if (job->filename)
  {
    if (!KeepFiles)
      unlink(job->filename);

    free(job->filename);
  }

  _cupsRWDeinit(&job->rwlock);

  free(job);
}


/*
 * 'delete_printer()' - Unregister, close listen sockets, and free all memory
 *                      used by a printer object.
 */

static void
delete_printer(_ipp_printer_t *printer)	/* I - Printer */
{
  _cupsRWLockWrite(&printer->rwlock);

  if (printer->ipv4 >= 0)
    close(printer->ipv4);

  if (printer->ipv6 >= 0)
    close(printer->ipv6);

  if (printer->name)
    free(printer->name);
  if (printer->directory)
    free(printer->directory);
  if (printer->hostname)
    free(printer->hostname);
  if (printer->uri)
    free(printer->uri);
  if (printer->proxy_user)
    free(printer->proxy_user);
  if (printer->proxy_pass)
    free(printer->proxy_pass);


  ippDelete(printer->attrs);
  ippDelete(printer->dev_attrs);

  cupsArrayDelete(printer->active_jobs);
  cupsArrayDelete(printer->completed_jobs);
  cupsArrayDelete(printer->jobs);
  cupsArrayDelete(printer->subscriptions);

  _cupsRWDeinit(&printer->rwlock);

  free(printer);
}


/*
 * 'delete_subscription()' - Delete a subscription.
 */

static void
delete_subscription(
    _ipp_subscription_t *sub)		/* I - Subscription */
{
  sub->pending_delete = 1;

  _cupsCondBroadcast(&SubscriptionCondition);

  _cupsRWLockWrite(&sub->rwlock);

  ippDelete(sub->attrs);
  cupsArrayDelete(sub->events);

  _cupsRWDeinit(&sub->rwlock);

  free(sub);
}


/*
 * 'filter_cb()' - Filter printer attributes based on the requested array.
 */

static int				/* O - 1 to copy, 0 to ignore */
filter_cb(_ipp_filter_t   *filter,	/* I - Filter parameters */
          ipp_t           *dst,		/* I - Destination (unused) */
	  ipp_attribute_t *attr)	/* I - Source attribute */
{
 /*
  * Filter attributes as needed...
  */

  (void)dst;

  ipp_tag_t group = ippGetGroupTag(attr);
  const char *name = ippGetName(attr);

  if ((filter->group_tag != IPP_TAG_ZERO && group != filter->group_tag && group != IPP_TAG_ZERO) || !name || (!strcmp(name, "media-col-database") && !cupsArrayFind(filter->ra, (void *)name)))
    return (0);

  return (!filter->ra || cupsArrayFind(filter->ra, (void *)name) != NULL);
}


/*
 * 'find_device()' - Find a device.
 */

static _ipp_device_t *			/* I - Device */
find_device(_ipp_client_t *client)	/* I - Client */
{
  ipp_attribute_t	*uuid;		/* output-device-uuid */
  _ipp_device_t		key,		/* Search key */
			*device;	/* Matching device */


  if ((uuid = ippFindAttribute(client->request, "output-device-uuid", IPP_TAG_URI)) == NULL)
    return (NULL);

  key.uuid = (char *)ippGetString(uuid, 0, NULL);

  _cupsRWLockRead(&client->printer->rwlock);
  device = (_ipp_device_t *)cupsArrayFind(client->printer->devices, &key);
  _cupsRWUnlock(&client->printer->rwlock);

  return (device);
}


/*
 * 'find_job()' - Find a job specified in a request.
 */

static _ipp_job_t *			/* O - Job or NULL */
find_job(_ipp_client_t *client,		/* I - Client */
         int           job_id)		/* I - Job ID to find or 0 to lookup */
{
  ipp_attribute_t	*attr;		/* job-id or job-uri attribute */
  _ipp_job_t		key,		/* Job search key */
			*job;		/* Matching job, if any */


  if (job_id > 0)
  {
    key.id = job_id;
  }
  else if ((attr = ippFindAttribute(client->request, "job-uri", IPP_TAG_URI)) != NULL)
  {
    const char *uri = ippGetString(attr, 0, NULL);

    if (!strncmp(uri, client->printer->uri, client->printer->urilen) &&
        uri[client->printer->urilen] == '/')
      key.id = atoi(uri + client->printer->urilen + 1);
    else
      return (NULL);
  }
  else if ((attr = ippFindAttribute(client->request, "job-id", IPP_TAG_INTEGER)) != NULL)
  {
    key.id = ippGetInteger(attr, 0);
  }

  _cupsRWLockRead(&(client->printer->rwlock));
  job = (_ipp_job_t *)cupsArrayFind(client->printer->jobs, &key);
  _cupsRWUnlock(&(client->printer->rwlock));

  return (job);
}


/*
 * 'find_subscription()' - Find a subcription.
 */

static _ipp_subscription_t *		/* O - Subscription */
find_subscription(_ipp_client_t *client,/* I - Client */
                  int           sub_id)	/* I - Subscription ID or 0 */
{
  ipp_attribute_t	*notify_subscription_id;
					/* notify-subscription-id */
  _ipp_subscription_t	key,		/* Search key */
			*sub;		/* Matching subscription */


  if (sub_id > 0)
    key.id = sub_id;
  else if ((notify_subscription_id = ippFindAttribute(client->request, "notify-subscription-id", IPP_TAG_INTEGER)) == NULL)
    return (NULL);
  else
    key.id = ippGetInteger(notify_subscription_id, 0);

  _cupsRWLockRead(&client->printer->rwlock);
  sub = (_ipp_subscription_t *)cupsArrayFind(client->printer->subscriptions, &key);
  _cupsRWUnlock(&client->printer->rwlock);

  return (sub);
}


/*
 * 'get_job_state_reasons_bits()' - Get the bits associates with "job-state-reasons" values.
 */

static _ipp_jreason_t			/* O - Bits */
get_job_state_reasons_bits(
    ipp_attribute_t *attr)		/* I - "job-state-reasons" attribute */
{
  int			i, j,		/* Looping vars */
			count;		/* Number of "job-state-reasons" values */
  const char		*keyword;	/* "job-state-reasons" value */
  _ipp_jreason_t	jreasons = _IPP_JREASON_NONE;
					/* Bits for "job-state-reasons" values */


  count = ippGetCount(attr);
  for (i = 0; i < count; i ++)
  {
    keyword = ippGetString(attr, i, NULL);

    for (j = 0; j < (int)(sizeof(_ipp_jreasons) / sizeof(_ipp_jreasons[0])); j ++)
    {
      if (!strcmp(keyword, _ipp_jreasons[j]))
      {
        jreasons |= 1 << j;
	break;
      }
    }
  }

  return (jreasons);
}


/*
 * 'get_notify_event_bits()' - Get the bits associated with "notify-events" values.
 */

static _ipp_event_t			/* O - Bits */
get_notify_events_bits(
    ipp_attribute_t *attr)		/* I - "notify-events" attribute */
{
  int		i, j,			/* Looping vars */
		count;			/* Number of "notify-events" values */
  const char	*keyword;		/* "notify-events" value */
  _ipp_event_t	events = _IPP_EVENT_NONE;
					/* Bits for "notify-events" values */


  count = ippGetCount(attr);
  for (i = 0; i < count; i ++)
  {
    keyword = ippGetString(attr, i, NULL);

    for (j = 0; j < (int)(sizeof(_ipp_events) / sizeof(_ipp_events[0])); j ++)
    {
      if (!strcmp(keyword, _ipp_jreasons[j]))
      {
        events |= 1 << j;
	break;
      }
    }
  }

  return (events);
}


/*
 * 'get_notify_subscribed_event()' - Get the event name.
 */

static const char *			/* O - Event name */
get_notify_subscribed_event(
    _ipp_event_t event)			/* I - Event bit */
{
  int		i;			/* Looping var */
  _ipp_event_t	mask;			/* Current mask */

  for (i = 0, mask = 1; i < (int)(sizeof(_ipp_events) / sizeof(_ipp_events[0])); i ++, mask <<= 1)
    if (event & mask)
      return (_ipp_events[i]);

  return ("none");
}


/*
 * 'get_printer_state_reasons_bits()' - Get the bits associated with "printer-state-reasons" values.
 */

static _ipp_preason_t			/* O - Bits */
get_printer_state_reasons_bits(
    ipp_attribute_t *attr)		/* I - "printer-state-reasons" bits */
{
  int			i, j,		/* Looping vars */
			count;		/* Number of "printer-state-reasons" values */
  const char		*keyword;	/* "printer-state-reasons" value */
  _ipp_preason_t	preasons = _IPP_PREASON_NONE;
					/* Bits for "printer-state-reasons" values */


  count = ippGetCount(attr);
  for (i = 0; i < count; i ++)
  {
    keyword = ippGetString(attr, i, NULL);

    for (j = 0; j < (int)(sizeof(_ipp_preasons) / sizeof(_ipp_preasons[0])); j ++)
    {
      if (!strcmp(keyword, _ipp_preasons[j]))
      {
        preasons |= 1 << j;
	break;
      }
    }
  }

  return (preasons);
}


/*
 * 'html_escape()' - Write a HTML-safe string.
 */

static void
html_escape(_ipp_client_t *client,	/* I - Client */
	    const char    *s,		/* I - String to write */
	    size_t        slen)		/* I - Number of characters to write */
{
  const char	*start,			/* Start of segment */
		*end;			/* End of string */


  start = s;
  end   = s + (slen > 0 ? slen : strlen(s));

  while (*s && s < end)
  {
    if (*s == '&' || *s == '<')
    {
      if (s > start)
        httpWrite2(client->http, start, (size_t)(s - start));

      if (*s == '&')
        httpWrite2(client->http, "&amp;", 5);
      else
        httpWrite2(client->http, "&lt;", 4);

      start = s + 1;
    }

    s ++;
  }

  if (s > start)
    httpWrite2(client->http, start, (size_t)(s - start));
}


/*
 * 'html_footer()' - Show the web interface footer.
 *
 * This function also writes the trailing 0-length chunk.
 */

static void
html_footer(_ipp_client_t *client)	/* I - Client */
{
  html_printf(client,
	      "</div>\n"
	      "</body>\n"
	      "</html>\n");
  httpWrite2(client->http, "", 0);
}


/*
 * 'html_header()' - Show the web interface header and title.
 */

static void
html_header(_ipp_client_t *client,	/* I - Client */
            const char    *title)	/* I - Title */
{
  html_printf(client,
	      "<!doctype html>\n"
	      "<html>\n"
	      "<head>\n"
	      "<title>%s</title>\n"
	      "<link rel=\"shortcut icon\" href=\"/icon.png\" type=\"image/png\">\n"
	      "<link rel=\"apple-touch-icon\" href=\"/icon.png\" type=\"image/png\">\n"
	      "<meta http-equiv=\"X-UA-Compatible\" content=\"IE=9\">\n"
	      "<meta name=\"viewport\" content=\"width=device-width\">\n"
	      "<style>\n"
	      "body { font-family: sans-serif; margin: 0; }\n"
	      "div.body { padding: 0px 10px 10px; }\n"
	      "blockquote { background: #dfd; border-radius: 5px; color: #006; padding: 10px; }\n"
	      "table.form { border-collapse: collapse; margin-top: 10px; width: 100%%; }\n"
	      "table.form td, table.form th { padding: 5px 2px; width: 50%%; }\n"
	      "table.form th { text-align: right; }\n"
	      "table.striped { border-bottom: solid thin black; border-collapse: collapse; width: 100%%; }\n"
	      "table.striped tr:nth-child(even) { background: #fcfcfc; }\n"
	      "table.striped tr:nth-child(odd) { background: #f0f0f0; }\n"
	      "table.striped th { background: white; border-bottom: solid thin black; text-align: left; vertical-align: bottom; }\n"
	      "table.striped td { margin: 0; padding: 5px; vertical-align: top; }\n"
	      "table.nav { border-collapse: collapse; width: 100%%; }\n"
	      "table.nav td { margin: 0; text-align: center; }\n"
	      "td.nav a, td.nav a:active, td.nav a:hover, td.nav a:hover:link, td.nav a:hover:link:visited, td.nav a:link, td.nav a:link:visited, td.nav a:visited { background: inherit; color: inherit; font-size: 80%%; text-decoration: none; }\n"
	      "td.nav { background: #333; color: #fff; padding: 4px 8px; width: 33%%; }\n"
	      "td.nav.sel { background: #fff; color: #000; font-weight: bold; }\n"
	      "td.nav:hover { background: #666; color: #fff; }\n"
	      "td.nav:active { background: #000; color: #ff0; }\n"
	      "</style>\n"
	      "</head>\n"
	      "<body>\n"
	      "<table class=\"nav\"><tr>"
	      "<td class=\"nav%s\"><a href=\"/\">Status</a></td>"
	      "<td class=\"nav%s\"><a href=\"/supplies\">Supplies</a></td>"
	      "<td class=\"nav%s\"><a href=\"/media\">Media</a></td>"
	      "</tr></table>\n"
	      "<div class=\"body\">\n", title, !strcmp(client->uri, "/") ? " sel" : "", !strcmp(client->uri, "/supplies") ? " sel" : "", !strcmp(client->uri, "/media") ? " sel" : "");
}


/*
 * 'html_printf()' - Send formatted text to the client, quoting as needed.
 */

static void
html_printf(_ipp_client_t *client,	/* I - Client */
	    const char    *format,	/* I - Printf-style format string */
	    ...)			/* I - Additional arguments as needed */
{
  va_list	ap;			/* Pointer to arguments */
  const char	*start;			/* Start of string */
  char		size,			/* Size character (h, l, L) */
		type;			/* Format type character */
  int		width,			/* Width of field */
		prec;			/* Number of characters of precision */
  char		tformat[100],		/* Temporary format string for sprintf() */
		*tptr,			/* Pointer into temporary format */
		temp[1024];		/* Buffer for formatted numbers */
  char		*s;			/* Pointer to string */


 /*
  * Loop through the format string, formatting as needed...
  */

  va_start(ap, format);
  start = format;

  while (*format)
  {
    if (*format == '%')
    {
      if (format > start)
        httpWrite2(client->http, start, (size_t)(format - start));

      tptr    = tformat;
      *tptr++ = *format++;

      if (*format == '%')
      {
        httpWrite2(client->http, "%", 1);
        format ++;
	start = format;
	continue;
      }
      else if (strchr(" -+#\'", *format))
        *tptr++ = *format++;

      if (*format == '*')
      {
       /*
        * Get width from argument...
	*/

	format ++;
	width = va_arg(ap, int);

	snprintf(tptr, sizeof(tformat) - (size_t)(tptr - tformat), "%d", width);
	tptr += strlen(tptr);
      }
      else
      {
	width = 0;

	while (isdigit(*format & 255))
	{
	  if (tptr < (tformat + sizeof(tformat) - 1))
	    *tptr++ = *format;

	  width = width * 10 + *format++ - '0';
	}
      }

      if (*format == '.')
      {
	if (tptr < (tformat + sizeof(tformat) - 1))
	  *tptr++ = *format;

        format ++;

        if (*format == '*')
	{
         /*
	  * Get precision from argument...
	  */

	  format ++;
	  prec = va_arg(ap, int);

	  snprintf(tptr, sizeof(tformat) - (size_t)(tptr - tformat), "%d", prec);
	  tptr += strlen(tptr);
	}
	else
	{
	  prec = 0;

	  while (isdigit(*format & 255))
	  {
	    if (tptr < (tformat + sizeof(tformat) - 1))
	      *tptr++ = *format;

	    prec = prec * 10 + *format++ - '0';
	  }
	}
      }

      if (*format == 'l' && format[1] == 'l')
      {
        size = 'L';

	if (tptr < (tformat + sizeof(tformat) - 2))
	{
	  *tptr++ = 'l';
	  *tptr++ = 'l';
	}

	format += 2;
      }
      else if (*format == 'h' || *format == 'l' || *format == 'L')
      {
	if (tptr < (tformat + sizeof(tformat) - 1))
	  *tptr++ = *format;

        size = *format++;
      }
      else
        size = 0;


      if (!*format)
      {
        start = format;
        break;
      }

      if (tptr < (tformat + sizeof(tformat) - 1))
        *tptr++ = *format;

      type  = *format++;
      *tptr = '\0';
      start = format;

      switch (type)
      {
	case 'E' : /* Floating point formats */
	case 'G' :
	case 'e' :
	case 'f' :
	case 'g' :
	    if ((size_t)(width + 2) > sizeof(temp))
	      break;

	    sprintf(temp, tformat, va_arg(ap, double));

            httpWrite2(client->http, temp, strlen(temp));
	    break;

        case 'B' : /* Integer formats */
	case 'X' :
	case 'b' :
        case 'd' :
	case 'i' :
	case 'o' :
	case 'u' :
	case 'x' :
	    if ((size_t)(width + 2) > sizeof(temp))
	      break;

#  ifdef HAVE_LONG_LONG
            if (size == 'L')
	      sprintf(temp, tformat, va_arg(ap, long long));
	    else
#  endif /* HAVE_LONG_LONG */
            if (size == 'l')
	      sprintf(temp, tformat, va_arg(ap, long));
	    else
	      sprintf(temp, tformat, va_arg(ap, int));

            httpWrite2(client->http, temp, strlen(temp));
	    break;

	case 'p' : /* Pointer value */
	    if ((size_t)(width + 2) > sizeof(temp))
	      break;

	    sprintf(temp, tformat, va_arg(ap, void *));

            httpWrite2(client->http, temp, strlen(temp));
	    break;

        case 'c' : /* Character or character array */
            if (width <= 1)
            {
              temp[0] = (char)va_arg(ap, int);
              temp[1] = '\0';
              html_escape(client, temp, 1);
            }
            else
              html_escape(client, va_arg(ap, char *), (size_t)width);
	    break;

	case 's' : /* String */
	    if ((s = va_arg(ap, char *)) == NULL)
	      s = "(null)";

            html_escape(client, s, strlen(s));
	    break;
      }
    }
    else
      format ++;
  }

  if (format > start)
    httpWrite2(client->http, start, (size_t)(format - start));

  va_end(ap);
}


/*
 * 'ipp_acknowledge_document()' - Acknowledge receipt of a document.
 */

static void
ipp_acknowledge_document(
    _ipp_client_t *client)		/* I - Client */
{
  _ipp_device_t		*device;	/* Device */
  _ipp_job_t		*job;		/* Job */
  ipp_attribute_t	*attr;		/* Attribute */


  if ((device = find_device(client)) == NULL)
  {
    respond_ipp(client, IPP_STATUS_ERROR_NOT_FOUND, "Device was not found.");
    return;
  }

  if ((job = find_job(client, 0)) == NULL)
  {
    respond_ipp(client, IPP_STATUS_ERROR_NOT_FOUND, "Job was not found.");
    return;
  }

  if (!job->dev_uuid || strcmp(job->dev_uuid, device->uuid))
  {
    respond_ipp(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job not assigned to device.");
    return;
  }

  if ((attr = ippFindAttribute(client->request, "document-number", IPP_TAG_ZERO)) == NULL || ippGetGroupTag(attr) != IPP_TAG_OPERATION || ippGetValueTag(attr) != IPP_TAG_INTEGER || ippGetCount(attr) != 1 || ippGetInteger(attr, 0) != 1)
  {
    respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST, attr ? "Bad document-number attribute." : "Missing document-number attribute.");
    return;
  }

  respond_ipp(client, IPP_STATUS_OK, NULL);
}


/*
 * 'ipp_acknowledge_identify_printer()' - Acknowledge an identify command.
 */

static void
ipp_acknowledge_identify_printer(
    _ipp_client_t *client)		/* I - Client */
{
  // TODO: Implement this!
  respond_ipp(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Need to implement this.");
}


/*
 * 'ipp_acknowledge_job()' - Acknowledge receipt of a job.
 */

static void
ipp_acknowledge_job(
    _ipp_client_t *client)		/* I - Client */
{
  _ipp_device_t		*device;	/* Device */
  _ipp_job_t		*job;		/* Job */


  if ((device = find_device(client)) == NULL)
  {
    respond_ipp(client, IPP_STATUS_ERROR_NOT_FOUND, "Device was not found.");
    return;
  }

  if ((job = find_job(client, 0)) == NULL)
  {
    respond_ipp(client, IPP_STATUS_ERROR_NOT_FOUND, "Job was not found.");
    return;
  }

  if (job->dev_uuid && strcmp(job->dev_uuid, device->uuid))
  {
    respond_ipp(client, IPP_STATUS_ERROR_NOT_AUTHORIZED, "Job not assigned to device.");
    return;
  }

  if (!(job->state_reasons & _IPP_JREASON_JOB_FETCHABLE))
  {
    respond_ipp(client, _IPP_STATUS_ERROR_NOT_FETCHABLE, "Job not fetchable.");
    return;
  }

  if (!job->dev_uuid)
    job->dev_uuid = strdup(device->uuid);

  job->state_reasons &= (_ipp_jreason_t)~_IPP_JREASON_JOB_FETCHABLE;

  add_event(client->printer, job, _IPP_EVENT_JOB_STATE_CHANGED, "Job acknowledged.");

  respond_ipp(client, IPP_STATUS_OK, NULL);
}


/*
 * 'ipp_cancel_job()' - Cancel a job.
 */

static void
ipp_cancel_job(_ipp_client_t *client)	/* I - Client */
{
  _ipp_job_t		*job;		/* Job information */


 /*
  * Get the job...
  */

  if ((job = find_job(client, 0)) == NULL)
  {
    respond_ipp(client, IPP_STATUS_ERROR_NOT_FOUND, "Job does not exist.");
    return;
  }

 /*
  * See if the job is already completed, canceled, or aborted; if so,
  * we can't cancel...
  */

  switch (job->state)
  {
    case IPP_JSTATE_CANCELED :
	respond_ipp(client, IPP_STATUS_ERROR_NOT_POSSIBLE,
		    "Job #%d is already canceled - can\'t cancel.", job->id);
        break;

    case IPP_JSTATE_ABORTED :
	respond_ipp(client, IPP_STATUS_ERROR_NOT_POSSIBLE,
		    "Job #%d is already aborted - can\'t cancel.", job->id);
        break;

    case IPP_JSTATE_COMPLETED :
	respond_ipp(client, IPP_STATUS_ERROR_NOT_POSSIBLE,
		    "Job #%d is already completed - can\'t cancel.", job->id);
        break;

    default :
       /*
        * Cancel the job...
	*/

	_cupsRWLockWrite(&(client->printer->rwlock));

	if (job->state == IPP_JSTATE_PROCESSING ||
	    (job->state == IPP_JSTATE_HELD && job->fd >= 0))
          job->cancel = 1;
	else
	{
	  job->state     = IPP_JSTATE_CANCELED;
	  job->completed = time(NULL);
	}

	_cupsRWUnlock(&(client->printer->rwlock));

        add_event(client->printer, job, _IPP_EVENT_JOB_COMPLETED, NULL);

	respond_ipp(client, IPP_STATUS_OK, NULL);
        break;
  }
}


/*
 * 'ipp_cancel_my_jobs()' - Cancel a user's jobs.
 */

static void
ipp_cancel_my_jobs(
    _ipp_client_t *client)		/* I - Client */
{
  // TODO: Implement this!
  respond_ipp(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Need to implement this.");
}


/*
 * 'ipp_cancel_subscription()' - Cancel a subscription.
 */

static void
ipp_cancel_subscription(
    _ipp_client_t *client)		/* I - Client */
{
  _ipp_subscription_t	*sub;		/* Subscription */


  if ((sub = find_subscription(client, 0)) == NULL)
  {
    respond_ipp(client, IPP_STATUS_ERROR_NOT_FOUND, "Subscription was not found.");
    return;
  }

  _cupsRWLockWrite(&client->printer->rwlock);
  cupsArrayRemove(client->printer->subscriptions, sub);
  delete_subscription(sub);
  _cupsRWUnlock(&client->printer->rwlock);
  respond_ipp(client, IPP_STATUS_OK, NULL);
}


/*
 * 'ipp_close_job()' - Close an open job.
 */

static void
ipp_close_job(_ipp_client_t *client)	/* I - Client */
{
  _ipp_job_t		*job;		/* Job information */


 /*
  * Get the job...
  */

  if ((job = find_job(client, 0)) == NULL)
  {
    respond_ipp(client, IPP_STATUS_ERROR_NOT_FOUND, "Job does not exist.");
    return;
  }

 /*
  * See if the job is already completed, canceled, or aborted; if so,
  * we can't cancel...
  */

  switch (job->state)
  {
    case IPP_JSTATE_CANCELED :
	respond_ipp(client, IPP_STATUS_ERROR_NOT_POSSIBLE,
		    "Job #%d is canceled - can\'t close.", job->id);
        break;

    case IPP_JSTATE_ABORTED :
	respond_ipp(client, IPP_STATUS_ERROR_NOT_POSSIBLE,
		    "Job #%d is aborted - can\'t close.", job->id);
        break;

    case IPP_JSTATE_COMPLETED :
	respond_ipp(client, IPP_STATUS_ERROR_NOT_POSSIBLE,
		    "Job #%d is completed - can\'t close.", job->id);
        break;

    case IPP_JSTATE_PROCESSING :
    case IPP_JSTATE_STOPPED :
	respond_ipp(client, IPP_STATUS_ERROR_NOT_POSSIBLE,
		    "Job #%d is already closed.", job->id);
        break;

    default :
	respond_ipp(client, IPP_STATUS_OK, NULL);
        break;
  }
}


/*
 * 'ipp_create_job()' - Create a job object.
 */

static void
ipp_create_job(_ipp_client_t *client)	/* I - Client */
{
  _ipp_job_t		*job;		/* New job */
  cups_array_t		*ra;		/* Attributes to send in response */


 /*
  * Validate print job attributes...
  */

  if (!valid_job_attributes(client))
  {
    httpFlush(client->http);
    return;
  }

 /*
  * Do we have a file to print?
  */

  if (httpGetState(client->http) == HTTP_STATE_POST_RECV)
  {
    respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST,
                "Unexpected document data following request.");
    return;
  }

 /*
  * Create the job...
  */

  if ((job = create_job(client)) == NULL)
  {
    respond_ipp(client, IPP_STATUS_ERROR_TOO_MANY_JOBS, "Too many jobs are queued.");
    return;
  }

 /*
  * Return the job info...
  */

  respond_ipp(client, IPP_STATUS_OK, NULL);

  ra = cupsArrayNew((cups_array_func_t)strcmp, NULL);
  cupsArrayAdd(ra, "job-id");
  cupsArrayAdd(ra, "job-state");
  cupsArrayAdd(ra, "job-state-message");
  cupsArrayAdd(ra, "job-state-reasons");
  cupsArrayAdd(ra, "job-uri");

  copy_job_attributes(client, job, ra);
  cupsArrayDelete(ra);

 /*
  * Add any subscriptions...
  */

  client->job = job;
  ipp_create_xxx_subscriptions(client);
}


/*
 * 'ipp_create_xxx_subscriptions()' - Create job and printer subscriptions.
 */

static void
ipp_create_xxx_subscriptions(
    _ipp_client_t *client)
{
  _ipp_subscription_t	*sub;		/* Subscription */
  ipp_attribute_t	*attr;		/* Subscription attribute */
  const char		*username;	/* requesting-user-name or
					   authenticated username */
  int			num_subs = 0,	/* Number of subscriptions */
			ok_subs = 0;	/* Number of good subscriptions */


 /*
  * For the Create-xxx-Subscriptions operations, queue up a successful-ok
  * response...
  */

  if (ippGetOperation(client->request) == IPP_OP_CREATE_JOB_SUBSCRIPTIONS || ippGetOperation(client->request) == IPP_OP_CREATE_PRINTER_SUBSCRIPTIONS)
    respond_ipp(client, IPP_STATUS_OK, NULL);

 /*
  * Get the authenticated user name, if any...
  */

  if (client->username[0])
    username = client->username;
  else if ((attr = ippFindAttribute(client->request, "requesting-user-name", IPP_TAG_NAME)) != NULL && ippGetGroupTag(attr) == IPP_TAG_OPERATION && ippGetCount(attr) == 1)
    username = ippGetString(attr, 0, NULL);
  else
    username = "guest";

 /*
  * Skip past the initial attributes to the first subscription group.
  */

  attr = ippFirstAttribute(client->request);
  while (attr && ippGetGroupTag(attr) != IPP_TAG_SUBSCRIPTION)
    attr = ippNextAttribute(client->request);

  while (attr)
  {
    _ipp_job_t		*job = NULL;	/* Job */
    const char		*attrname,	/* Attribute name */
			*pullmethod = NULL;
    					/* notify-pull-method */
    ipp_attribute_t	*notify_attributes = NULL,
					/* notify-attributes */
			*notify_events = NULL,
					/* notify-events */
			*notify_user_data = NULL;
					/* notify-user-data */
    int			interval = 0,	/* notify-time-interval */
			lease = _IPP_NOTIFY_LEASE_DURATION_DEFAULT;
					/* notify-lease-duration */
    ipp_status_t	status = IPP_STATUS_OK;
					/* notify-status-code */

    num_subs ++;

    while (attr)
    {
      if ((attrname = ippGetName(attr)) == NULL)
        break;

      if (!strcmp(attrname, "notify-recipient-uri"))
      {
       /*
        * Push notifications not supported.
	*/

        status = IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES;
	ippCopyAttribute(client->response, attr, 0);
      }
      else if (!strcmp(attrname, "notify-pull-method"))
      {
	pullmethod = ippGetString(attr, 0, NULL);

        if (ippGetValueTag(attr) != IPP_TAG_KEYWORD || ippGetCount(attr) != 1 || !pullmethod || strcmp(pullmethod, "ippget"))
	{
          ippCopyAttribute(client->response, attr, 0);
	  pullmethod = NULL;
	  status     = IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES;
	}
      }
      else if (!strcmp(attrname, "notify-attributes"))
      {
        if (ippGetValueTag(attr) != IPP_TAG_KEYWORD)
	{
	  status = IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES;
	  ippCopyAttribute(client->response, attr, 0);
	}

	notify_attributes = attr;
      }
      else if (!strcmp(attrname, "notify-charset"))
      {
        if (ippGetValueTag(attr) != IPP_TAG_CHARSET || ippGetCount(attr) != 1 ||
	    (strcmp(ippGetString(attr, 0, NULL), "us-ascii") && strcmp(ippGetString(attr, 0, NULL), "utf-8")))
	{
	  status = IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES;
	  ippCopyAttribute(client->response, attr, 0);
	}
      }
      else if (!strcmp(attrname, "notify-natural-language"))
      {
        if (ippGetValueTag(attr) !=  IPP_TAG_LANGUAGE || ippGetCount(attr) != 1 || strcmp(ippGetString(attr, 0, NULL), "en"))
        {
	  status = IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES;
	  ippCopyAttribute(client->response, attr, 0);
	}
      }
      else if (!strcmp(attrname, "notify-user-data"))
      {
        int	datalen;		/* Length of data */

        if (ippGetValueTag(attr) != IPP_TAG_STRING || ippGetCount(attr) != 1 || !ippGetOctetString(attr, 0, &datalen) || datalen > 63)
	{
	  status = IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES;
	  ippCopyAttribute(client->response, attr, 0);
	}
	else
	  notify_user_data = attr;
      }
      else if (!strcmp(attrname, "notify-events"))
      {
        if (ippGetValueTag(attr) != IPP_TAG_KEYWORD)
	{
	  status = IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES;
	  ippCopyAttribute(client->response, attr, 0);
	}
	else
          notify_events = attr;
      }
      else if (!strcmp(attrname, "notify-lease-duration"))
      {
        if (ippGetValueTag(attr) != IPP_TAG_INTEGER || ippGetCount(attr) != 1 || ippGetInteger(attr, 0) < 0)
	{
	  status = IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES;
	  ippCopyAttribute(client->response, attr, 0);
	}
	else
          lease = ippGetInteger(attr, 0);
      }
      else if (!strcmp(attrname, "notify-time-interval"))
      {
        if (ippGetValueTag(attr) != IPP_TAG_INTEGER || ippGetCount(attr) != 1 || ippGetInteger(attr, 0) < 0)
	{
	  status = IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES;
	  ippCopyAttribute(client->response, attr, 0);
	}
	else
          interval = ippGetInteger(attr, 0);
      }
      else if (!strcmp(attrname, "notify-job-id"))
      {
        if (ippGetOperation(client->request) != IPP_OP_CREATE_JOB_SUBSCRIPTIONS || ippGetValueTag(attr) != IPP_TAG_INTEGER || ippGetInteger(attr, 0) < 1)
        {
	  status = IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES;
	  ippCopyAttribute(client->response, attr, 0);
	}
	else if ((job = find_job(client, ippGetInteger(attr, 0))) == NULL)
	{
	  status = IPP_STATUS_ERROR_NOT_FOUND;
	  ippCopyAttribute(client->response, attr, 0);
	}
      }

      attr = ippNextAttribute(client->request);
    }

    if (status)
    {
      ippAddInteger(client->response, IPP_TAG_SUBSCRIPTION, IPP_TAG_ENUM, "notify-status-code", status);
    }
    else if (!pullmethod)
    {
      ippAddInteger(client->response, IPP_TAG_SUBSCRIPTION, IPP_TAG_ENUM, "notify-status-code", IPP_STATUS_ERROR_BAD_REQUEST);
    }
    else
    {
      switch (ippGetOperation(client->request))
      {
	case IPP_OP_PRINT_JOB :
	case IPP_OP_PRINT_URI :
	case IPP_OP_CREATE_JOB :
	    job = client->job;
	    break;

	default :
	    break;
      }

      if ((sub = create_subscription(client->printer, job, interval, lease, username, notify_events, notify_attributes, notify_user_data)) == NULL)
      {
        ippAddInteger(client->response, IPP_TAG_SUBSCRIPTION, IPP_TAG_INTEGER, "notify-subscription-id", sub->id);
        ok_subs ++;
      }
      else
        ippAddInteger(client->response, IPP_TAG_SUBSCRIPTION, IPP_TAG_ENUM, "notify-status-code", IPP_STATUS_ERROR_INTERNAL);
    }
  }

  if (ok_subs == 0)
    ippSetStatusCode(client->response, IPP_STATUS_ERROR_IGNORED_ALL_SUBSCRIPTIONS);
  else if (ok_subs != num_subs)
    ippSetStatusCode(client->response, IPP_STATUS_OK_IGNORED_SUBSCRIPTIONS);
}


/*
 * 'ipp_deregister_output_device()' - Unregister an output device.
 */

static void
ipp_deregister_output_device(
    _ipp_client_t *client)		/* I - Client */
{
  _ipp_device_t	*device;		/* Device */


 /*
  * Find the device...
  */

  if ((device = find_device(client)) == NULL)
  {
    respond_ipp(client, IPP_STATUS_ERROR_NOT_FOUND, "Output device not found.");
    return;
  }

 /*
  * Remove the device from the printer...
  */

  _cupsRWLockWrite(&client->printer->rwlock);

  cupsArrayRemove(client->printer->devices, device);

  update_device_attributes_no_lock(client->printer);
  update_device_state_no_lock(client->printer);

  _cupsRWUnlock(&client->printer->rwlock);

 /*
  * Delete the device...
  */

  delete_device(device);

  respond_ipp(client, IPP_STATUS_OK, NULL);
}


/*
 * 'ipp_fetch_document()' - Download a document.
 */

static void
ipp_fetch_document(
    _ipp_client_t *client)		/* I - Client */
{
  _ipp_device_t		*device;	/* Device */
  _ipp_job_t		*job;		/* Job */
  ipp_attribute_t	*attr;		/* Attribute */
  int			compression;	/* compression */
  char			filename[1024];	/* Job filename */
  const char		*format;	/* document-format */


  if ((device = find_device(client)) == NULL)
  {
    respond_ipp(client, IPP_STATUS_ERROR_NOT_FOUND, "Device was not found.");
    return;
  }

  if ((job = find_job(client, 0)) == NULL)
  {
    respond_ipp(client, IPP_STATUS_ERROR_NOT_FOUND, "Job was not found.");
    return;
  }

  if (!job->dev_uuid || strcmp(job->dev_uuid, device->uuid))
  {
    respond_ipp(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job not assigned to device.");
    return;
  }

  if ((attr = ippFindAttribute(client->request, "document-number", IPP_TAG_ZERO)) == NULL || ippGetGroupTag(attr) != IPP_TAG_OPERATION || ippGetValueTag(attr) != IPP_TAG_INTEGER || ippGetCount(attr) != 1 || ippGetInteger(attr, 0) != 1)
  {
    respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST, attr ? "Bad document-number attribute." : "Missing document-number attribute.");
    return;
  }

  if ((attr = ippFindAttribute(client->request, "compression-accepted", IPP_TAG_KEYWORD)) != NULL)
    compression = !strcmp(ippGetString(attr, 0, NULL), "gzip");
  else
    compression = 0;

  if ((attr = ippFindAttribute(client->request, "document-format-accepted", IPP_TAG_MIMETYPE)) != NULL)
  {
    int	i,				/* Looping var */
	count = ippGetCount(attr);	/* Number of values */


    for (i = 0; i < count; i ++)
    {
      format = ippGetString(attr, i, NULL);

      create_job_filename(client->printer, job, NULL, filename, sizeof(filename));

      if (!access(filename, R_OK))
        break;
    }

    if (i >= count)
    {
      respond_ipp(client, _IPP_STATUS_ERROR_NOT_FETCHABLE, "Document not available in requested format.");
      return;
    }
  }
  else if ((attr = ippFindAttribute(job->attrs, "document-format", IPP_TAG_MIMETYPE)) != NULL)
    format = ippGetString(attr, 0, NULL);
  else
  {
    respond_ipp(client, _IPP_STATUS_ERROR_NOT_FETCHABLE, "Document format unknown.");
    return;
  }

  respond_ipp(client, IPP_STATUS_OK, NULL);
  ippAddString(client->response, IPP_TAG_OPERATION, IPP_TAG_MIMETYPE, "document-format", NULL, format);
  ippAddString(client->response, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "compression", NULL, compression ? "gzip" : "none");

  client->fetch_file = open(filename, O_RDONLY);
}


/*
 * 'ipp_fetch_job()' - Download a job.
 */

static void
ipp_fetch_job(_ipp_client_t *client)	/* I - Client */
{
  _ipp_device_t		*device;	/* Device */
  _ipp_job_t		*job;		/* Job */


  if ((device = find_device(client)) == NULL)
  {
    respond_ipp(client, IPP_STATUS_ERROR_NOT_FOUND, "Device was not found.");
    return;
  }

  if ((job = find_job(client, 0)) == NULL)
  {
    respond_ipp(client, IPP_STATUS_ERROR_NOT_FOUND, "Job was not found.");
    return;
  }

  if (job->dev_uuid && strcmp(job->dev_uuid, device->uuid))
  {
    respond_ipp(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job not assigned to device.");
    return;
  }

  if (!(job->state_reasons & _IPP_JREASON_JOB_FETCHABLE))
  {
    respond_ipp(client, _IPP_STATUS_ERROR_NOT_FETCHABLE, "Job not fetchable.");
    return;
  }

  respond_ipp(client, IPP_STATUS_OK, NULL);
  copy_attributes(client->response, job->attrs, NULL, IPP_TAG_JOB, 0);
}


/*
 * 'ipp_get_document_attributes()' - Get the attributes for a document object.
 *
 * Note: This implementation only supports single document jobs so we
 *       synthesize the information for a single document from the job.
 */

static void
ipp_get_document_attributes(
    _ipp_client_t *client)		/* I - Client */
{
  // TODO: Implement this!
  respond_ipp(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Need to implement this.");
}


/*
 * 'ipp_get_documents()' - Get the list of documents in a job.
 *
 * Note: This implementation only supports single document jobs so we
 *       synthesize the information for a single document from the job.
 */

static void
ipp_get_documents(_ipp_client_t *client)/* I - Client */
{
  // TODO: Implement this!
  respond_ipp(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Need to implement this.");
}


/*
 * 'ipp_get_job_attributes()' - Get the attributes for a job object.
 */

static void
ipp_get_job_attributes(
    _ipp_client_t *client)		/* I - Client */
{
  _ipp_job_t	*job;			/* Job */
  cups_array_t	*ra;			/* requested-attributes */


  if ((job = find_job(client, 0)) == NULL)
  {
    respond_ipp(client, IPP_STATUS_ERROR_NOT_FOUND, "Job not found.");
    return;
  }

  respond_ipp(client, IPP_STATUS_OK, NULL);

  ra = ippCreateRequestedArray(client->request);
  copy_job_attributes(client, job, ra);
  cupsArrayDelete(ra);
}


/*
 * 'ipp_get_jobs()' - Get a list of job objects.
 */

static void
ipp_get_jobs(_ipp_client_t *client)	/* I - Client */
{
  ipp_attribute_t	*attr;		/* Current attribute */
  const char		*which_jobs = NULL;
					/* which-jobs values */
  int			job_comparison;	/* Job comparison */
  ipp_jstate_t		job_state;	/* job-state value */
  int			first_job_id,	/* First job ID */
			limit,		/* Maximum number of jobs to return */
			count;		/* Number of jobs that match */
  const char		*username;	/* Username */
  _ipp_job_t		*job;		/* Current job pointer */
  cups_array_t		*ra;		/* Requested attributes array */


 /*
  * See if the "which-jobs" attribute have been specified...
  */

  if ((attr = ippFindAttribute(client->request, "which-jobs",
                               IPP_TAG_KEYWORD)) != NULL)
  {
    which_jobs = ippGetString(attr, 0, NULL);
    fprintf(stderr, "%s Get-Jobs which-jobs=%s", client->hostname, which_jobs);
  }

  if (!which_jobs || !strcmp(which_jobs, "not-completed"))
  {
    job_comparison = -1;
    job_state      = IPP_JSTATE_STOPPED;
  }
  else if (!strcmp(which_jobs, "completed"))
  {
    job_comparison = 1;
    job_state      = IPP_JSTATE_CANCELED;
  }
  else if (!strcmp(which_jobs, "aborted"))
  {
    job_comparison = 0;
    job_state      = IPP_JSTATE_ABORTED;
  }
  else if (!strcmp(which_jobs, "all"))
  {
    job_comparison = 1;
    job_state      = IPP_JSTATE_PENDING;
  }
  else if (!strcmp(which_jobs, "canceled"))
  {
    job_comparison = 0;
    job_state      = IPP_JSTATE_CANCELED;
  }
  else if (!strcmp(which_jobs, "pending"))
  {
    job_comparison = 0;
    job_state      = IPP_JSTATE_PENDING;
  }
  else if (!strcmp(which_jobs, "pending-held"))
  {
    job_comparison = 0;
    job_state      = IPP_JSTATE_HELD;
  }
  else if (!strcmp(which_jobs, "processing"))
  {
    job_comparison = 0;
    job_state      = IPP_JSTATE_PROCESSING;
  }
  else if (!strcmp(which_jobs, "processing-stopped"))
  {
    job_comparison = 0;
    job_state      = IPP_JSTATE_STOPPED;
  }
  else
  {
    respond_ipp(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES,
                "The which-jobs value \"%s\" is not supported.", which_jobs);
    ippAddString(client->response, IPP_TAG_UNSUPPORTED_GROUP, IPP_TAG_KEYWORD,
                 "which-jobs", NULL, which_jobs);
    return;
  }

 /*
  * See if they want to limit the number of jobs reported...
  */

  if ((attr = ippFindAttribute(client->request, "limit",
                               IPP_TAG_INTEGER)) != NULL)
  {
    limit = ippGetInteger(attr, 0);

    fprintf(stderr, "%s Get-Jobs limit=%d", client->hostname, limit);
  }
  else
    limit = 0;

  if ((attr = ippFindAttribute(client->request, "first-job-id",
                               IPP_TAG_INTEGER)) != NULL)
  {
    first_job_id = ippGetInteger(attr, 0);

    fprintf(stderr, "%s Get-Jobs first-job-id=%d", client->hostname,
            first_job_id);
  }
  else
    first_job_id = 1;

 /*
  * See if we only want to see jobs for a specific user...
  */

  username = NULL;

  if ((attr = ippFindAttribute(client->request, "my-jobs",
                               IPP_TAG_BOOLEAN)) != NULL)
  {
    int my_jobs = ippGetBoolean(attr, 0);

    fprintf(stderr, "%s Get-Jobs my-jobs=%s\n", client->hostname,
            my_jobs ? "true" : "false");

    if (my_jobs)
    {
      if ((attr = ippFindAttribute(client->request, "requesting-user-name",
					IPP_TAG_NAME)) == NULL)
      {
	respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST,
	            "Need requesting-user-name with my-jobs.");
	return;
      }

      username = ippGetString(attr, 0, NULL);

      fprintf(stderr, "%s Get-Jobs requesting-user-name=\"%s\"\n",
              client->hostname, username);
    }
  }

 /*
  * OK, build a list of jobs for this printer...
  */

  ra = ippCreateRequestedArray(client->request);

  respond_ipp(client, IPP_STATUS_OK, NULL);

  _cupsRWLockRead(&(client->printer->rwlock));

  for (count = 0, job = (_ipp_job_t *)cupsArrayFirst(client->printer->jobs);
       (limit <= 0 || count < limit) && job;
       job = (_ipp_job_t *)cupsArrayNext(client->printer->jobs))
  {
   /*
    * Filter out jobs that don't match...
    */

    if ((job_comparison < 0 && job->state > job_state) ||
	(job_comparison == 0 && job->state != job_state) ||
	(job_comparison > 0 && job->state < job_state) ||
	job->id < first_job_id ||
	(username && job->username &&
	 strcasecmp(username, job->username)))
      continue;

    if (count > 0)
      ippAddSeparator(client->response);

    count ++;
    copy_job_attributes(client, job, ra);
  }

  cupsArrayDelete(ra);

  _cupsRWUnlock(&(client->printer->rwlock));
}


/*
 * 'ipp_get_notifications()' - Get notification events for one or more subscriptions.
 */

static void
ipp_get_notifications(
    _ipp_client_t *client)		/* I - Client */
{
  ipp_attribute_t	*sub_ids,	/* notify-subscription-ids */
			*seq_nums,	/* notify-sequence-numbers */
			*notify_wait;	/* Wait for events? */
  int			i,		/* Looping vars */
			count,		/* Number of IDs */
			first = 1,	/* First event? */
			seq_num;	/* Sequence number */
  _ipp_subscription_t	*sub;		/* Current subscription */
  ipp_t			*event;		/* Current event */


  if ((sub_ids = ippFindAttribute(client->request, "notify-subscription-ids", IPP_TAG_INTEGER)) == NULL)
  {
    respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing notify-subscription-ids attribute.");
    return;
  }

  count       = ippGetCount(sub_ids);
  seq_nums    = ippFindAttribute(client->request, "notify-sequence-numbers", IPP_TAG_INTEGER);
  notify_wait = ippFindAttribute(client->request, "notify-wait", IPP_TAG_BOOLEAN);

  if (seq_nums && count != ippGetCount(seq_nums))
  {
    respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST, "The notify-subscription-ids and notify-sequence-numbers attributes are different lengths.");
    return;
  }

  respond_ipp(client, IPP_STATUS_OK, NULL);
  ippAddInteger(client->response, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "notify-get-interval", 30);

  for (i = 0; i < count; i ++)
  {
    if ((sub = find_subscription(client, ippGetInteger(sub_ids, i))) == NULL)
      continue;

    seq_num = ippGetInteger(seq_nums, i);
    if (seq_num < sub->first_sequence)
      seq_num = sub->first_sequence;

    if (seq_num > sub->last_sequence)
      continue;

    for (event = (ipp_t *)cupsArrayIndex(sub->events, seq_num - sub->first_sequence);
         event;
	 event = (ipp_t *)cupsArrayNext(sub->events))
    {
      if (first)
        first = 0;
      else
        ippAddSeparator(client->response);

      ippCopyAttributes(client->response, event, 0, NULL, NULL);
    }
  }
}


/*
 * 'ipp_get_output_device_attributes()' - Get attributes for an output device.
 */

static void
ipp_get_output_device_attributes(
    _ipp_client_t *client)		/* I - Client */
{
  // TODO: Implement this!
  respond_ipp(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Need to implement this.");
}


/*
 * 'ipp_get_printer_attributes()' - Get the attributes for a printer object.
 */

static void
ipp_get_printer_attributes(
    _ipp_client_t *client)		/* I - Client */
{
  cups_array_t		*ra;		/* Requested attributes array */
  _ipp_printer_t	*printer;	/* Printer */


 /*
  * Send the attributes...
  */

  ra      = ippCreateRequestedArray(client->request);
  printer = client->printer;

  respond_ipp(client, IPP_STATUS_OK, NULL);

  _cupsRWLockRead(&(printer->rwlock));

  copy_attributes(client->response, printer->attrs, ra, IPP_TAG_ZERO,
		  IPP_TAG_CUPS_CONST);
  copy_attributes(client->response, printer->dev_attrs, ra, IPP_TAG_ZERO, IPP_TAG_ZERO);

  if (!ra || cupsArrayFind(ra, "printer-config-change-date-time"))
    ippAddDate(client->response, IPP_TAG_PRINTER, "printer-config-change-date-time", ippTimeToDate(printer->config_time));

  if (!ra || cupsArrayFind(ra, "printer-config-change-time"))
    ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "printer-config-change-time", (int)(printer->config_time - printer->start_time));

  if (!ra || cupsArrayFind(ra, "printer-current-time"))
    ippAddDate(client->response, IPP_TAG_PRINTER, "printer-current-time", ippTimeToDate(time(NULL)));


  if (!ra || cupsArrayFind(ra, "printer-state"))
    ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_ENUM,
                  "printer-state", printer->state > printer->dev_state ? printer->state : printer->dev_state);

  if (!ra || cupsArrayFind(ra, "printer-state-change-date-time"))
    ippAddDate(client->response, IPP_TAG_PRINTER, "printer-state-change-date-time", ippTimeToDate(printer->state_time));

  if (!ra || cupsArrayFind(ra, "printer-state-change-time"))
    ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "printer-state-change-time", (int)(printer->state_time - printer->start_time));

  if (!ra || cupsArrayFind(ra, "printer-state-message"))
  {
    static const char * const messages[] = { "Idle.", "Printing.", "Stopped." };

    if (printer->state > printer->dev_state)
      ippAddString(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_TEXT), "printer-state-message", NULL, messages[printer->state - IPP_PSTATE_IDLE]);
    else
      ippAddString(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_TEXT), "printer-state-message", NULL, messages[printer->dev_state - IPP_PSTATE_IDLE]);
  }

  if (!ra || cupsArrayFind(ra, "printer-state-reasons"))
    copy_printer_state_reasons(client->response, IPP_TAG_PRINTER, printer);

  if (!ra || cupsArrayFind(ra, "printer-up-time"))
    ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "printer-up-time", (int)(time(NULL) - printer->start_time));

  if (!ra || cupsArrayFind(ra, "queued-job-count"))
    ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "queued-job-count", cupsArrayCount(printer->active_jobs));

  _cupsRWUnlock(&(printer->rwlock));

  cupsArrayDelete(ra);
}


/*
 * 'ipp_get_printer_supported_values()' - Return the supported values for
 *                                        the infrastructure printer.
 */

static void
ipp_get_printer_supported_values(
    _ipp_client_t *client)		/* I - Client */
{
  cups_array_t	*ra = ippCreateRequestedArray(client->request);
					/* Requested attributes */


  respond_ipp(client, IPP_STATUS_OK, NULL);

  copy_attributes(client->response, client->printer->attrs, ra, IPP_TAG_PRINTER, 1);

  cupsArrayDelete(ra);
}


/*
 * 'ipp_get_subscription_attributes()' - Get attributes for a subscription.
 */

static void
ipp_get_subscription_attributes(
    _ipp_client_t *client)		/* I - Client */
{
  _ipp_subscription_t	*sub;		/* Subscription */
  cups_array_t		*ra = ippCreateRequestedArray(client->request);
					/* Requested attributes */


  if ((sub = find_subscription(client, 0)) == NULL)
  {
    respond_ipp(client, IPP_STATUS_ERROR_NOT_FOUND, "Subscription was not found.");
  }
  else
  {
    respond_ipp(client, IPP_STATUS_OK, NULL);
    copy_subscription_attributes(client, sub, ra);
  }

  cupsArrayDelete(ra);
}


/*
 * 'ipp_get_subscriptions()' - Get attributes for all subscriptions.
 */

static void
ipp_get_subscriptions(
    _ipp_client_t *client)		/* I - Client */
{
  _ipp_subscription_t	*sub;		/* Current subscription */
  cups_array_t		*ra = ippCreateRequestedArray(client->request);
					/* Requested attributes */
  int			first = 1;	/* First time? */


  respond_ipp(client, IPP_STATUS_OK, NULL);
  _cupsRWLockRead(&client->printer->rwlock);
  for (sub = (_ipp_subscription_t *)cupsArrayFirst(client->printer->subscriptions);
       sub;
       sub = (_ipp_subscription_t *)cupsArrayNext(client->printer->subscriptions))
  {
    if (first)
      first = 0;
    else
      ippAddSeparator(client->response);

    copy_subscription_attributes(client, sub, ra);
  }

  cupsArrayDelete(ra);
}


/*
 * 'ipp_identify_printer()' - Beep or display a message.
 */

static void
ipp_identify_printer(
    _ipp_client_t *client)		/* I - Client */
{
  /* TODO: Do something */

  respond_ipp(client, IPP_STATUS_OK, NULL);
}


/*
 * 'ipp_print_job()' - Create a job object with an attached document.
 */

static void
ipp_print_job(_ipp_client_t *client)	/* I - Client */
{
  _ipp_job_t		*job;		/* New job */
  char			filename[1024],	/* Filename buffer */
			buffer[4096];	/* Copy buffer */
  ssize_t		bytes;		/* Bytes read */
  cups_array_t		*ra;		/* Attributes to send in response */


 /*
  * Validate print job attributes...
  */

  if (!valid_job_attributes(client))
  {
    httpFlush(client->http);
    return;
  }

 /*
  * Do we have a file to print?
  */

  if (httpGetState(client->http) == HTTP_STATE_POST_SEND)
  {
    respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST, "No file in request.");
    return;
  }

 /*
  * Print the job...
  */

  if ((job = create_job(client)) == NULL)
  {
    respond_ipp(client, IPP_STATUS_ERROR_BUSY,
                "Currently printing another job.");
    return;
  }

 /*
  * Create a file for the request data...
  */

  create_job_filename(client->printer, job, NULL, filename, sizeof(filename));

  if (Verbosity)
    fprintf(stderr, "Creating job file \"%s\", format \"%s\".\n", filename, job->format);

  if ((job->fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0600)) < 0)
  {
    job->state = IPP_JSTATE_ABORTED;

    respond_ipp(client, IPP_STATUS_ERROR_INTERNAL,
                "Unable to create print file: %s", strerror(errno));
    return;
  }

  while ((bytes = httpRead2(client->http, buffer, sizeof(buffer))) > 0)
  {
    if (write(job->fd, buffer, (size_t)bytes) < bytes)
    {
      int error = errno;		/* Write error */

      job->state = IPP_JSTATE_ABORTED;

      close(job->fd);
      job->fd = -1;

      unlink(filename);

      respond_ipp(client, IPP_STATUS_ERROR_INTERNAL,
                  "Unable to write print file: %s", strerror(error));
      return;
    }
  }

  if (bytes < 0)
  {
   /*
    * Got an error while reading the print data, so abort this job.
    */

    job->state = IPP_JSTATE_ABORTED;

    close(job->fd);
    job->fd = -1;

    unlink(filename);

    respond_ipp(client, IPP_STATUS_ERROR_INTERNAL,
                "Unable to read print file.");
    return;
  }

  if (close(job->fd))
  {
    int error = errno;		/* Write error */

    job->state = IPP_JSTATE_ABORTED;
    job->fd    = -1;

    unlink(filename);

    respond_ipp(client, IPP_STATUS_ERROR_INTERNAL,
                "Unable to write print file: %s", strerror(error));
    return;
  }

  job->fd       = -1;
  job->filename = strdup(filename);
  job->state    = IPP_JSTATE_PENDING;

 /*
  * Process the job, if possible...
  */

  check_jobs(client->printer);
  
 /*
  * Return the job info...
  */

  respond_ipp(client, IPP_STATUS_OK, NULL);

  ra = cupsArrayNew((cups_array_func_t)strcmp, NULL);
  cupsArrayAdd(ra, "job-id");
  cupsArrayAdd(ra, "job-state");
  cupsArrayAdd(ra, "job-state-message");
  cupsArrayAdd(ra, "job-state-reasons");
  cupsArrayAdd(ra, "job-uri");

  copy_job_attributes(client, job, ra);
  cupsArrayDelete(ra);

 /*
  * Process any pending subscriptions...
  */

  client->job = job;
  ipp_create_xxx_subscriptions(client);
}


/*
 * 'ipp_print_uri()' - Create a job object with a referenced document.
 */

static void
ipp_print_uri(_ipp_client_t *client)	/* I - Client */
{
  _ipp_job_t		*job;		/* New job */
  ipp_attribute_t	*uri;		/* document-uri */
  char			scheme[256],	/* URI scheme */
			userpass[256],	/* Username and password info */
			hostname[256],	/* Hostname */
			resource[1024];	/* Resource path */
  int			port;		/* Port number */
  http_uri_status_t	uri_status;	/* URI decode status */
  http_encryption_t	encryption;	/* Encryption to use, if any */
  http_t		*http;		/* Connection for http/https URIs */
  http_status_t		status;		/* Access status for http/https URIs */
  int			infile;		/* Input file for local file URIs */
  char			filename[1024],	/* Filename buffer */
			buffer[4096];	/* Copy buffer */
  ssize_t		bytes;		/* Bytes read */
  cups_array_t		*ra;		/* Attributes to send in response */
  static const char * const uri_status_strings[] =
  {					/* URI decode errors */
    "URI too large.",
    "Bad arguments to function.",
    "Bad resource in URI.",
    "Bad port number in URI.",
    "Bad hostname in URI.",
    "Bad username in URI.",
    "Bad scheme in URI.",
    "Bad/empty URI."
  };


 /*
  * Validate print job attributes...
  */

  if (!valid_job_attributes(client))
  {
    httpFlush(client->http);
    return;
  }

 /*
  * Do we have a file to print?
  */

  if (httpGetState(client->http) == HTTP_STATE_POST_RECV)
  {
    respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST,
                "Unexpected document data following request.");
    return;
  }

 /*
  * Do we have a document URI?
  */

  if ((uri = ippFindAttribute(client->request, "document-uri",
                              IPP_TAG_URI)) == NULL)
  {
    respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing document-uri.");
    return;
  }

  if (ippGetCount(uri) != 1)
  {
    respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST,
                "Too many document-uri values.");
    return;
  }

  uri_status = httpSeparateURI(HTTP_URI_CODING_ALL, ippGetString(uri, 0, NULL),
                               scheme, sizeof(scheme), userpass,
                               sizeof(userpass), hostname, sizeof(hostname),
                               &port, resource, sizeof(resource));
  if (uri_status < HTTP_URI_STATUS_OK)
  {
    respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST, "Bad document-uri: %s",
                uri_status_strings[uri_status - HTTP_URI_STATUS_OVERFLOW]);
    return;
  }

  if (strcmp(scheme, "file") &&
#ifdef HAVE_SSL
      strcmp(scheme, "https") &&
#endif /* HAVE_SSL */
      strcmp(scheme, "http"))
  {
    respond_ipp(client, IPP_STATUS_ERROR_URI_SCHEME,
                "URI scheme \"%s\" not supported.", scheme);
    return;
  }

  if (!strcmp(scheme, "file") && access(resource, R_OK))
  {
    respond_ipp(client, IPP_STATUS_ERROR_DOCUMENT_ACCESS,
                "Unable to access URI: %s", strerror(errno));
    return;
  }

 /*
  * Print the job...
  */

  if ((job = create_job(client)) == NULL)
  {
    respond_ipp(client, IPP_STATUS_ERROR_BUSY,
                "Currently printing another job.");
    return;
  }

 /*
  * Create a file for the request data...
  */

  if (!strcasecmp(job->format, "image/jpeg"))
    snprintf(filename, sizeof(filename), "%s/%d.jpg",
             client->printer->directory, job->id);
  else if (!strcasecmp(job->format, "image/png"))
    snprintf(filename, sizeof(filename), "%s/%d.png",
             client->printer->directory, job->id);
  else if (!strcasecmp(job->format, "application/pdf"))
    snprintf(filename, sizeof(filename), "%s/%d.pdf",
             client->printer->directory, job->id);
  else if (!strcasecmp(job->format, "application/postscript"))
    snprintf(filename, sizeof(filename), "%s/%d.ps",
             client->printer->directory, job->id);
  else
    snprintf(filename, sizeof(filename), "%s/%d.prn",
             client->printer->directory, job->id);

  if ((job->fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0600)) < 0)
  {
    job->state = IPP_JSTATE_ABORTED;

    respond_ipp(client, IPP_STATUS_ERROR_INTERNAL,
                "Unable to create print file: %s", strerror(errno));
    return;
  }

  if (!strcmp(scheme, "file"))
  {
    if ((infile = open(resource, O_RDONLY)) < 0)
    {
      respond_ipp(client, IPP_STATUS_ERROR_DOCUMENT_ACCESS,
                  "Unable to access URI: %s", strerror(errno));
      return;
    }

    do
    {
      if ((bytes = read(infile, buffer, sizeof(buffer))) < 0 &&
          (errno == EAGAIN || errno == EINTR))
        bytes = 1;
      else if (bytes > 0 && write(job->fd, buffer, (size_t)bytes) < bytes)
      {
	int error = errno;		/* Write error */

	job->state = IPP_JSTATE_ABORTED;

	close(job->fd);
	job->fd = -1;

	unlink(filename);
	close(infile);

	respond_ipp(client, IPP_STATUS_ERROR_INTERNAL,
		    "Unable to write print file: %s", strerror(error));
	return;
      }
    }
    while (bytes > 0);

    close(infile);
  }
  else
  {
#ifdef HAVE_SSL
    if (port == 443 || !strcmp(scheme, "https"))
      encryption = HTTP_ENCRYPTION_ALWAYS;
    else
#endif /* HAVE_SSL */
    encryption = HTTP_ENCRYPTION_IF_REQUESTED;

    if ((http = httpConnect2(hostname, port, NULL, AF_UNSPEC, encryption,
                             1, 30000, NULL)) == NULL)
    {
      respond_ipp(client, IPP_STATUS_ERROR_DOCUMENT_ACCESS,
                  "Unable to connect to %s: %s", hostname,
		  cupsLastErrorString());
      job->state = IPP_JSTATE_ABORTED;

      close(job->fd);
      job->fd = -1;

      unlink(filename);
      return;
    }

    httpClearFields(http);
    httpSetField(http, HTTP_FIELD_ACCEPT_LANGUAGE, "en");
    if (httpGet(http, resource))
    {
      respond_ipp(client, IPP_STATUS_ERROR_DOCUMENT_ACCESS,
                  "Unable to GET URI: %s", strerror(errno));

      job->state = IPP_JSTATE_ABORTED;

      close(job->fd);
      job->fd = -1;

      unlink(filename);
      httpClose(http);
      return;
    }

    while ((status = httpUpdate(http)) == HTTP_STATUS_CONTINUE);

    if (status != HTTP_STATUS_OK)
    {
      respond_ipp(client, IPP_STATUS_ERROR_DOCUMENT_ACCESS,
                  "Unable to GET URI: %s", httpStatus(status));

      job->state = IPP_JSTATE_ABORTED;

      close(job->fd);
      job->fd = -1;

      unlink(filename);
      httpClose(http);
      return;
    }

    while ((bytes = httpRead2(http, buffer, sizeof(buffer))) > 0)
    {
      if (write(job->fd, buffer, (size_t)bytes) < bytes)
      {
	int error = errno;		/* Write error */

	job->state = IPP_JSTATE_ABORTED;

	close(job->fd);
	job->fd = -1;

	unlink(filename);
	httpClose(http);

	respond_ipp(client, IPP_STATUS_ERROR_INTERNAL,
		    "Unable to write print file: %s", strerror(error));
	return;
      }
    }

    httpClose(http);
  }

  if (close(job->fd))
  {
    int error = errno;		/* Write error */

    job->state = IPP_JSTATE_ABORTED;
    job->fd    = -1;

    unlink(filename);

    respond_ipp(client, IPP_STATUS_ERROR_INTERNAL,
                "Unable to write print file: %s", strerror(error));
    return;
  }

  job->fd       = -1;
  job->filename = strdup(filename);
  job->state    = IPP_JSTATE_PENDING;

  /* TODO: Do something different here - only process if the printer is idle */
 /*
  * Process the job...
  */

  check_jobs(client->printer);

 /*
  * Return the job info...
  */

  respond_ipp(client, IPP_STATUS_OK, NULL);

  ra = cupsArrayNew((cups_array_func_t)strcmp, NULL);
  cupsArrayAdd(ra, "job-id");
  cupsArrayAdd(ra, "job-state");
  cupsArrayAdd(ra, "job-state-reasons");
  cupsArrayAdd(ra, "job-uri");

  copy_job_attributes(client, job, ra);
  cupsArrayDelete(ra);

 /*
  * Process any pending subscriptions...
  */

  client->job = job;
  ipp_create_xxx_subscriptions(client);
}


/*
 * 'ipp_renew_subscription()' - Renew a subscription.
 */

static void
ipp_renew_subscription(
    _ipp_client_t *client)		/* I - Client */
{
  _ipp_subscription_t	*sub;		/* Subscription */
  ipp_attribute_t	*attr;		/* notify-lease-duration */
  int			lease;		/* Lease duration in seconds */


  if ((sub = find_subscription(client, 0)) == NULL)
  {
    respond_ipp(client, IPP_STATUS_ERROR_NOT_FOUND, "Subscription was not found.");
    return;
  }

  if (sub->job)
  {
    respond_ipp(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Per-job subscriptions cannot be renewed.");
    return;
  }

  if ((attr = ippFindAttribute(client->request, "notify-lease-duration", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetGroupTag(attr) != IPP_TAG_SUBSCRIPTION || ippGetValueTag(attr) != IPP_TAG_INTEGER || ippGetCount(attr) != 1 || ippGetInteger(attr, 0) < 0)
    {
      respond_ipp(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "Bad notify-lease-duration.");
      return;
    }

    lease = ippGetInteger(attr, 0);
  }
  else
    lease = _IPP_NOTIFY_LEASE_DURATION_DEFAULT;

  sub->lease = lease;

  if (lease)
    sub->expire = time(NULL) + sub->lease;
  else
    sub->expire = INT_MAX;

  respond_ipp(client, IPP_STATUS_OK, NULL);
}


/*
 * 'ipp_send_document()' - Add an attached document to a job object created with
 *                         Create-Job.
 */

static void
ipp_send_document(_ipp_client_t *client)/* I - Client */
{
  _ipp_job_t		*job;		/* Job information */
  char			filename[1024],	/* Filename buffer */
			buffer[4096];	/* Copy buffer */
  ssize_t		bytes;		/* Bytes read */
  ipp_attribute_t	*attr;		/* Current attribute */
  cups_array_t		*ra;		/* Attributes to send in response */


 /*
  * Get the job...
  */

  if ((job = find_job(client, 0)) == NULL)
  {
    respond_ipp(client, IPP_STATUS_ERROR_NOT_FOUND, "Job does not exist.");
    httpFlush(client->http);
    return;
  }

 /*
  * See if we already have a document for this job or the job has already
  * in a non-pending state...
  */

  if (job->state > IPP_JSTATE_HELD)
  {
    respond_ipp(client, IPP_STATUS_ERROR_NOT_POSSIBLE,
                "Job is not in a pending state.");
    httpFlush(client->http);
    return;
  }
  else if (job->filename || job->fd >= 0)
  {
    respond_ipp(client, IPP_STATUS_ERROR_MULTIPLE_JOBS_NOT_SUPPORTED,
                "Multiple document jobs are not supported.");
    httpFlush(client->http);
    return;
  }

  if ((attr = ippFindAttribute(client->request, "last-document",
                               IPP_TAG_ZERO)) == NULL)
  {
    respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST,
                "Missing required last-document attribute.");
    httpFlush(client->http);
    return;
  }
  else if (ippGetValueTag(attr) != IPP_TAG_BOOLEAN || ippGetCount(attr) != 1 ||
           !ippGetBoolean(attr, 0))
  {
    respond_unsupported(client, attr);
    httpFlush(client->http);
    return;
  }

 /*
  * Validate document attributes...
  */

  if (!valid_doc_attributes(client))
  {
    httpFlush(client->http);
    return;
  }

  copy_attributes(job->attrs, client->request, NULL, IPP_TAG_JOB, 0);

 /*
  * Get the document format for the job...
  */

  _cupsRWLockWrite(&(client->printer->rwlock));

  if ((attr = ippFindAttribute(job->attrs, "document-format-detected", IPP_TAG_MIMETYPE)) != NULL)
    job->format = ippGetString(attr, 0, NULL);
  else if ((attr = ippFindAttribute(job->attrs, "document-format-supplied", IPP_TAG_MIMETYPE)) != NULL)
    job->format = ippGetString(attr, 0, NULL);
  else
    job->format = "application/octet-stream";

 /*
  * Create a file for the request data...
  */

  create_job_filename(client->printer, job, NULL, filename, sizeof(filename));

  if (Verbosity)
    fprintf(stderr, "Creating job file \"%s\", format \"%s\".\n", filename, job->format);

  job->fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0600);

  _cupsRWUnlock(&(client->printer->rwlock));

  if (job->fd < 0)
  {
    job->state = IPP_JSTATE_ABORTED;

    respond_ipp(client, IPP_STATUS_ERROR_INTERNAL,
                "Unable to create print file: %s", strerror(errno));
    return;
  }

  while ((bytes = httpRead2(client->http, buffer, sizeof(buffer))) > 0)
  {
    if (write(job->fd, buffer, (size_t)bytes) < bytes)
    {
      int error = errno;		/* Write error */

      job->state = IPP_JSTATE_ABORTED;

      close(job->fd);
      job->fd = -1;

      unlink(filename);

      respond_ipp(client, IPP_STATUS_ERROR_INTERNAL,
                  "Unable to write print file: %s", strerror(error));
      return;
    }
  }

  if (bytes < 0)
  {
   /*
    * Got an error while reading the print data, so abort this job.
    */

    job->state = IPP_JSTATE_ABORTED;

    close(job->fd);
    job->fd = -1;

    unlink(filename);

    respond_ipp(client, IPP_STATUS_ERROR_INTERNAL,
                "Unable to read print file.");
    return;
  }

  if (close(job->fd))
  {
    int error = errno;			/* Write error */

    job->state = IPP_JSTATE_ABORTED;
    job->fd    = -1;

    unlink(filename);

    respond_ipp(client, IPP_STATUS_ERROR_INTERNAL,
                "Unable to write print file: %s", strerror(error));
    return;
  }

  _cupsRWLockWrite(&(client->printer->rwlock));

  job->fd       = -1;
  job->filename = strdup(filename);
  job->state    = IPP_JSTATE_PENDING;

  _cupsRWUnlock(&(client->printer->rwlock));

 /*
  * Process the job, if possible...
  */

  check_jobs(client->printer);
  
 /*
  * Return the job info...
  */

  respond_ipp(client, IPP_STATUS_OK, NULL);

  ra = cupsArrayNew((cups_array_func_t)strcmp, NULL);
  cupsArrayAdd(ra, "job-id");
  cupsArrayAdd(ra, "job-state");
  cupsArrayAdd(ra, "job-state-reasons");
  cupsArrayAdd(ra, "job-uri");

  copy_job_attributes(client, job, ra);
  cupsArrayDelete(ra);
}


/*
 * 'ipp_send_uri()' - Add a referenced document to a job object created with
 *                    Create-Job.
 */

static void
ipp_send_uri(_ipp_client_t *client)	/* I - Client */
{
  _ipp_job_t		*job;		/* Job information */
  ipp_attribute_t	*uri;		/* document-uri */
  char			scheme[256],	/* URI scheme */
			userpass[256],	/* Username and password info */
			hostname[256],	/* Hostname */
			resource[1024];	/* Resource path */
  int			port;		/* Port number */
  http_uri_status_t	uri_status;	/* URI decode status */
  http_encryption_t	encryption;	/* Encryption to use, if any */
  http_t		*http;		/* Connection for http/https URIs */
  http_status_t		status;		/* Access status for http/https URIs */
  int			infile;		/* Input file for local file URIs */
  char			filename[1024],	/* Filename buffer */
			buffer[4096];	/* Copy buffer */
  ssize_t		bytes;		/* Bytes read */
  ipp_attribute_t	*attr;		/* Current attribute */
  cups_array_t		*ra;		/* Attributes to send in response */
  static const char * const uri_status_strings[] =
  {					/* URI decode errors */
    "URI too large.",
    "Bad arguments to function.",
    "Bad resource in URI.",
    "Bad port number in URI.",
    "Bad hostname in URI.",
    "Bad username in URI.",
    "Bad scheme in URI.",
    "Bad/empty URI."
  };


 /*
  * Get the job...
  */

  if ((job = find_job(client, 0)) == NULL)
  {
    respond_ipp(client, IPP_STATUS_ERROR_NOT_FOUND, "Job does not exist.");
    httpFlush(client->http);
    return;
  }

 /*
  * See if we already have a document for this job or the job has already
  * in a non-pending state...
  */

  if (job->state > IPP_JSTATE_HELD)
  {
    respond_ipp(client, IPP_STATUS_ERROR_NOT_POSSIBLE,
                "Job is not in a pending state.");
    httpFlush(client->http);
    return;
  }
  else if (job->filename || job->fd >= 0)
  {
    respond_ipp(client, IPP_STATUS_ERROR_MULTIPLE_JOBS_NOT_SUPPORTED,
                "Multiple document jobs are not supported.");
    httpFlush(client->http);
    return;
  }

  if ((attr = ippFindAttribute(client->request, "last-document",
                               IPP_TAG_ZERO)) == NULL)
  {
    respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST,
                "Missing required last-document attribute.");
    httpFlush(client->http);
    return;
  }
  else if (ippGetValueTag(attr) != IPP_TAG_BOOLEAN || ippGetCount(attr) != 1 ||
           !ippGetBoolean(attr, 0))
  {
    respond_unsupported(client, attr);
    httpFlush(client->http);
    return;
  }

 /*
  * Validate document attributes...
  */

  if (!valid_doc_attributes(client))
  {
    httpFlush(client->http);
    return;
  }

 /*
  * Do we have a file to print?
  */

  if (httpGetState(client->http) == HTTP_STATE_POST_RECV)
  {
    respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST,
                "Unexpected document data following request.");
    return;
  }

 /*
  * Do we have a document URI?
  */

  if ((uri = ippFindAttribute(client->request, "document-uri",
                              IPP_TAG_URI)) == NULL)
  {
    respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing document-uri.");
    return;
  }

  if (ippGetCount(uri) != 1)
  {
    respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST,
                "Too many document-uri values.");
    return;
  }

  uri_status = httpSeparateURI(HTTP_URI_CODING_ALL, ippGetString(uri, 0, NULL),
                               scheme, sizeof(scheme), userpass,
                               sizeof(userpass), hostname, sizeof(hostname),
                               &port, resource, sizeof(resource));
  if (uri_status < HTTP_URI_STATUS_OK)
  {
    respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST, "Bad document-uri: %s",
                uri_status_strings[uri_status - HTTP_URI_STATUS_OVERFLOW]);
    return;
  }

  if (strcmp(scheme, "file") &&
#ifdef HAVE_SSL
      strcmp(scheme, "https") &&
#endif /* HAVE_SSL */
      strcmp(scheme, "http"))
  {
    respond_ipp(client, IPP_STATUS_ERROR_URI_SCHEME,
                "URI scheme \"%s\" not supported.", scheme);
    return;
  }

  if (!strcmp(scheme, "file") && access(resource, R_OK))
  {
    respond_ipp(client, IPP_STATUS_ERROR_DOCUMENT_ACCESS,
                "Unable to access URI: %s", strerror(errno));
    return;
  }

 /*
  * Get the document format for the job...
  */

  _cupsRWLockWrite(&(client->printer->rwlock));

  if ((attr = ippFindAttribute(job->attrs, "document-format",
                               IPP_TAG_MIMETYPE)) != NULL)
    job->format = ippGetString(attr, 0, NULL);
  else
    job->format = "application/octet-stream";

 /*
  * Create a file for the request data...
  */

  if (!strcasecmp(job->format, "image/jpeg"))
    snprintf(filename, sizeof(filename), "%s/%d.jpg",
             client->printer->directory, job->id);
  else if (!strcasecmp(job->format, "image/png"))
    snprintf(filename, sizeof(filename), "%s/%d.png",
             client->printer->directory, job->id);
  else if (!strcasecmp(job->format, "application/pdf"))
    snprintf(filename, sizeof(filename), "%s/%d.pdf",
             client->printer->directory, job->id);
  else if (!strcasecmp(job->format, "application/postscript"))
    snprintf(filename, sizeof(filename), "%s/%d.ps",
             client->printer->directory, job->id);
  else
    snprintf(filename, sizeof(filename), "%s/%d.prn",
             client->printer->directory, job->id);

  job->fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0600);

  _cupsRWUnlock(&(client->printer->rwlock));

  if (job->fd < 0)
  {
    job->state = IPP_JSTATE_ABORTED;

    respond_ipp(client, IPP_STATUS_ERROR_INTERNAL,
                "Unable to create print file: %s", strerror(errno));
    return;
  }

  if (!strcmp(scheme, "file"))
  {
    if ((infile = open(resource, O_RDONLY)) < 0)
    {
      respond_ipp(client, IPP_STATUS_ERROR_DOCUMENT_ACCESS,
                  "Unable to access URI: %s", strerror(errno));
      return;
    }

    do
    {
      if ((bytes = read(infile, buffer, sizeof(buffer))) < 0 &&
          (errno == EAGAIN || errno == EINTR))
        bytes = 1;
      else if (bytes > 0 && write(job->fd, buffer, (size_t)bytes) < bytes)
      {
	int error = errno;		/* Write error */

	job->state = IPP_JSTATE_ABORTED;

	close(job->fd);
	job->fd = -1;

	unlink(filename);
	close(infile);

	respond_ipp(client, IPP_STATUS_ERROR_INTERNAL,
		    "Unable to write print file: %s", strerror(error));
	return;
      }
    }
    while (bytes > 0);

    close(infile);
  }
  else
  {
#ifdef HAVE_SSL
    if (port == 443 || !strcmp(scheme, "https"))
      encryption = HTTP_ENCRYPTION_ALWAYS;
    else
#endif /* HAVE_SSL */
    encryption = HTTP_ENCRYPTION_IF_REQUESTED;

    if ((http = httpConnect2(hostname, port, NULL, AF_UNSPEC, encryption,
                             1, 30000, NULL)) == NULL)
    {
      respond_ipp(client, IPP_STATUS_ERROR_DOCUMENT_ACCESS,
                  "Unable to connect to %s: %s", hostname,
		  cupsLastErrorString());
      job->state = IPP_JSTATE_ABORTED;

      close(job->fd);
      job->fd = -1;

      unlink(filename);
      return;
    }

    httpClearFields(http);
    httpSetField(http, HTTP_FIELD_ACCEPT_LANGUAGE, "en");
    if (httpGet(http, resource))
    {
      respond_ipp(client, IPP_STATUS_ERROR_DOCUMENT_ACCESS,
                  "Unable to GET URI: %s", strerror(errno));

      job->state = IPP_JSTATE_ABORTED;

      close(job->fd);
      job->fd = -1;

      unlink(filename);
      httpClose(http);
      return;
    }

    while ((status = httpUpdate(http)) == HTTP_STATUS_CONTINUE);

    if (status != HTTP_STATUS_OK)
    {
      respond_ipp(client, IPP_STATUS_ERROR_DOCUMENT_ACCESS,
                  "Unable to GET URI: %s", httpStatus(status));

      job->state = IPP_JSTATE_ABORTED;

      close(job->fd);
      job->fd = -1;

      unlink(filename);
      httpClose(http);
      return;
    }

    while ((bytes = httpRead2(http, buffer, sizeof(buffer))) > 0)
    {
      if (write(job->fd, buffer, (size_t)bytes) < bytes)
      {
	int error = errno;		/* Write error */

	job->state = IPP_JSTATE_ABORTED;

	close(job->fd);
	job->fd = -1;

	unlink(filename);
	httpClose(http);

	respond_ipp(client, IPP_STATUS_ERROR_INTERNAL,
		    "Unable to write print file: %s", strerror(error));
	return;
      }
    }

    httpClose(http);
  }

  if (close(job->fd))
  {
    int error = errno;		/* Write error */

    job->state = IPP_JSTATE_ABORTED;
    job->fd    = -1;

    unlink(filename);

    respond_ipp(client, IPP_STATUS_ERROR_INTERNAL,
                "Unable to write print file: %s", strerror(error));
    return;
  }

  _cupsRWLockWrite(&(client->printer->rwlock));

  job->fd       = -1;
  job->filename = strdup(filename);
  job->state    = IPP_JSTATE_PENDING;

  _cupsRWUnlock(&(client->printer->rwlock));

 /*
  * Process the job, if possible...
  */

  check_jobs(client->printer);
  
 /*
  * Return the job info...
  */

  respond_ipp(client, IPP_STATUS_OK, NULL);

  ra = cupsArrayNew((cups_array_func_t)strcmp, NULL);
  cupsArrayAdd(ra, "job-id");
  cupsArrayAdd(ra, "job-state");
  cupsArrayAdd(ra, "job-state-reasons");
  cupsArrayAdd(ra, "job-uri");

  copy_job_attributes(client, job, ra);
  cupsArrayDelete(ra);
}


/*
 * 'ipp_update_active_jobs()' - Update the list of active jobs.
 */

static void
ipp_update_active_jobs(
    _ipp_client_t *client)		/* I - Client */
{
  _ipp_device_t		*device;	/* Output device */
  _ipp_job_t		*job;		/* Job */
  ipp_attribute_t	*job_ids,	/* job-ids */
			*job_states;	/* output-device-job-states */
  int			i,		/* Looping var */
			count,		/* Number of values */
			num_different = 0,
					/* Number of jobs with different states */
			different[1000],/* Jobs with different states */
			num_unsupported = 0,
					/* Number of unsupported job-ids */
			unsupported[1000];
					/* Unsupported job-ids */
  ipp_jstate_t		states[1000];	/* Different job state values */


 /*
  * Process the job-ids and output-device-job-states values...
  */

  if ((device = find_device(client)) == NULL)
  {
    respond_ipp(client, IPP_STATUS_ERROR_NOT_FOUND, "Device was not found.");
    return;
  }

  if ((job_ids = ippFindAttribute(client->request, "job-ids", IPP_TAG_ZERO)) == NULL || ippGetGroupTag(job_ids) != IPP_TAG_OPERATION || ippGetValueTag(job_ids) != IPP_TAG_INTEGER)
  {
    respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST, job_ids ? "Bad job-ids attribute." : "Missing required job-ids attribute.");
    return;
  }

  if ((job_states = ippFindAttribute(client->request, "output-device-job-states", IPP_TAG_ZERO)) == NULL || ippGetGroupTag(job_states) != IPP_TAG_OPERATION || ippGetValueTag(job_states) != IPP_TAG_ENUM)
  {
    respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST, job_ids ? "Bad output-device-job-states attribute." : "Missing required output-device-job-states attribute.");
    return;
  }

  count = ippGetCount(job_ids);
  if (count != ippGetCount(job_states))
  {
    respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST, "The job-ids and output-device-job-states attributes do not have the same number of values.");
    return;
  }

  for (i = 0; i < count; i ++)
  {
    if ((job = find_job(client, ippGetInteger(job_ids, i))) == NULL || !job->dev_uuid || strcmp(job->dev_uuid, device->uuid))
    {
      if (num_unsupported < 1000)
        unsupported[num_unsupported ++] = ippGetInteger(job_ids, i);
    }
    else
    {
      ipp_jstate_t	state = (ipp_jstate_t)ippGetInteger(job_states, i);

      if (job->state >= IPP_JSTATE_STOPPED && state != job->state)
      {
        if (num_different < 1000)
	{
	  different[num_different] = job->id;
	  states[num_different ++] = job->state;
	}
      }
      else
        job->dev_state = state;
    }
  }

 /*
  * Then look for jobs assigned to the device but not listed...
  */

  for (job = (_ipp_job_t *)cupsArrayFirst(client->printer->jobs);
       job && num_different < 1000;
       job = (_ipp_job_t *)cupsArrayNext(client->printer->jobs))
  {
    if (job->dev_uuid && !strcmp(job->dev_uuid, device->uuid) && !ippContainsInteger(job_ids, job->id))
    {
      different[num_different] = job->id;
      states[num_different ++] = job->state;
    }
  }

  respond_ipp(client, IPP_STATUS_OK, NULL);

  if (num_different > 0)
  {
    ippAddIntegers(client->response, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "job-ids", num_different, different);
    ippAddIntegers(client->response, IPP_TAG_OPERATION, IPP_TAG_ENUM, "output-device-job-states", num_different, (int *)states);
  }

  if (num_unsupported > 0)
  {
    ippAddIntegers(client->response, IPP_TAG_UNSUPPORTED_GROUP, IPP_TAG_INTEGER, "job-ids", num_unsupported, unsupported);
  }
}


/*
 * 'ipp_update_document_status()' - Update the state of a document.
 */

static void
ipp_update_document_status(
    _ipp_client_t *client)		/* I - Client */
{
  _ipp_device_t		*device;	/* Device */
  _ipp_job_t		*job;		/* Job */
  ipp_attribute_t	*attr;		/* Attribute */


  if ((device = find_device(client)) == NULL)
  {
    respond_ipp(client, IPP_STATUS_ERROR_NOT_FOUND, "Device was not found.");
    return;
  }

  if ((job = find_job(client, 0)) == NULL)
  {
    respond_ipp(client, IPP_STATUS_ERROR_NOT_FOUND, "Job was not found.");
    return;
  }

  if (!job->dev_uuid || strcmp(job->dev_uuid, device->uuid))
  {
    respond_ipp(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job not assigned to device.");
    return;
  }

  if ((attr = ippFindAttribute(client->request, "document-number", IPP_TAG_ZERO)) == NULL || ippGetGroupTag(attr) != IPP_TAG_OPERATION || ippGetValueTag(attr) != IPP_TAG_INTEGER || ippGetCount(attr) != 1 || ippGetInteger(attr, 0) != 1)
  {
    respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST, attr ? "Bad document-number attribute." : "Missing document-number attribute.");
    return;
  }

  if ((attr = ippFindAttribute(client->request, "impressions-completed", IPP_TAG_INTEGER)) != NULL)
  {
    job->impcompleted = ippGetInteger(attr, 0);
    add_event(client->printer, job, _IPP_EVENT_JOB_PROGRESS, NULL);
  }

  respond_ipp(client, IPP_STATUS_OK, NULL);
}


/*
 * 'ipp_update_job_status()' - Update the state of a job.
 */

static void
ipp_update_job_status(
    _ipp_client_t *client)		/* I - Client */
{
  _ipp_device_t		*device;	/* Device */
  _ipp_job_t		*job;		/* Job */
  ipp_attribute_t	*attr;		/* Attribute */
  _ipp_event_t		events = _IPP_EVENT_NONE;
					/* Event(s) */


  if ((device = find_device(client)) == NULL)
  {
    respond_ipp(client, IPP_STATUS_ERROR_NOT_FOUND, "Device was not found.");
    return;
  }

  if ((job = find_job(client, 0)) == NULL)
  {
    respond_ipp(client, IPP_STATUS_ERROR_NOT_FOUND, "Job was not found.");
    return;
  }

  if (!job->dev_uuid || strcmp(job->dev_uuid, device->uuid))
  {
    respond_ipp(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job not assigned to device.");
    return;
  }

  if ((attr = ippFindAttribute(client->request, "job-impressions-completed", IPP_TAG_INTEGER)) != NULL)
  {
    job->impcompleted = ippGetInteger(attr, 0);
    events |= _IPP_EVENT_JOB_PROGRESS;
  }

  if ((attr = ippFindAttribute(client->request, "output-device-job-state", IPP_TAG_ENUM)) != NULL)
  {
    job->dev_state = (ipp_jstate_t)ippGetInteger(attr, 0);
    events |= _IPP_EVENT_JOB_STATE_CHANGED;
  }

  if ((attr = ippFindAttribute(client->request, "output-device-job-state-reasons", IPP_TAG_KEYWORD)) != NULL)
  {
    job->dev_state_reasons = get_job_state_reasons_bits(attr);
    events |= _IPP_EVENT_JOB_STATE_CHANGED;
  }

  if (events)
    add_event(client->printer, job, events, NULL);

  respond_ipp(client, IPP_STATUS_OK, NULL);
}


/*
 * 'ipp_update_output_device_attributes()' - Update the values for an output device.
 */

static void
ipp_update_output_device_attributes(
    _ipp_client_t *client)		/* I - Client */
{
  _ipp_device_t		*device;	/* Device */
  ipp_attribute_t	*attr,		/* Current attribute */
			*dev_attr;	/* Device attribute */
  _ipp_event_t		events = _IPP_EVENT_NONE;
					/* Config/state changed? */


  if ((device = find_device(client)) == NULL)
  {
    if ((device = create_device(client)) == NULL)
    {
      respond_ipp(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Unable to add output device.");
      return;
    }
  }

  _cupsRWLockWrite(&device->rwlock);

  attr = ippFirstAttribute(client->request);
  while (attr && ippGetGroupTag(attr) != IPP_TAG_PRINTER)
    attr = ippNextAttribute(client->request);

  for (; attr; attr = ippNextAttribute(client->request))
  {
    const char	*attrname = ippGetName(attr),
					/* Attribute name */
		*dotptr;		/* Pointer to dot in name */

   /*
    * Skip attributes we don't care about...
    */

    if (!attrname)
      continue;

    if (strncmp(attrname, "copies", 6) && strncmp(attrname, "document-format", 15) && strncmp(attrname, "finishings", 10) && strncmp(attrname, "media", 5) && strncmp(attrname, "print-", 6) && strncmp(attrname, "sides", 5) && strncmp(attrname, "printer-alert", 13) && strncmp(attrname, "printer-input", 13) && strncmp(attrname, "printer-output", 14) && strncmp(attrname, "printer-resolution", 18) && strncmp(attrname, "pwg-raster", 10) && strncmp(attrname, "urf-", 4))
      continue;

    if (strncmp(attrname, "printer-alert", 13) || strncmp(attrname, "printer-state", 13))
      events |= _IPP_EVENT_PRINTER_CONFIG_CHANGED;
    else
      events |= _IPP_EVENT_PRINTER_STATE_CHANGED;

    if (!strcmp(attrname, "media-col-ready") || !strcmp(attrname, "media-ready"))
      events |= _IPP_EVENT_PRINTER_MEDIA_CHANGED;

    if (!strcmp(attrname, "finishings-col-ready") || !strcmp(attrname, "finishings-ready"))
      events |= _IPP_EVENT_PRINTER_FINISHINGS_CHANGED;

    if ((dotptr = strrchr(attrname, '.')) != NULL && isdigit(dotptr[1] & 255))
    {
#if 0
     /*
      * Sparse representation: name.NNN or name.NNN-NNN
      */

      char	temp[256],		/* Temporary name string */
		*tempptr;		/* Pointer into temporary string */
      int	low, high;		/* Low and high numbers in range */

      low = (int)strtol(dotptr + 1, (char **)&dotptr, 10);
      if (dotptr && *dotptr == '-')
        high = (int)strtol(dotptr + 1, NULL, 10);
      else
        high = low;

      strlcpy(temp, attrname, sizeof(temp));
      if ((tempptr = strrchr(temp, '.')) != NULL)
        *tempptr = '\0';

      if ((dev_attr = ippFindAttribute(device->attrs, temp, IPP_TAG_ZERO)) != NULL)
      {
      }
      else
#endif /* 0 */
        respond_unsupported(client, attr);
    }
    else
    {
     /*
      * Regular representation - replace or delete current attribute,
      * if any...
      */

      if ((dev_attr = ippFindAttribute(device->attrs, attrname, IPP_TAG_ZERO)) != NULL)
        ippDeleteAttribute(device->attrs, dev_attr);

      if (ippGetValueTag(attr) != IPP_TAG_DELETEATTR)
        ippCopyAttribute(device->attrs, attr, 0);
    }
  }

  _cupsRWUnlock(&device->rwlock);

  if (events)
  {
    _cupsRWLockWrite(&client->printer->rwlock);
    if (events & _IPP_EVENT_PRINTER_CONFIG_CHANGED)
      update_device_attributes_no_lock(client->printer);
    if (events & _IPP_EVENT_PRINTER_STATE_CHANGED)
      update_device_state_no_lock(client->printer);
    _cupsRWUnlock(&client->printer->rwlock);

    add_event(client->printer, NULL, events, NULL);
  }
}


/*
 * 'ipp_validate_document()' - Validate document creation attributes.
 */

static void
ipp_validate_document(
    _ipp_client_t *client)		/* I - Client */
{
  if (valid_doc_attributes(client))
    respond_ipp(client, IPP_STATUS_OK, NULL);
}


/*
 * 'ipp_validate_job()' - Validate job creation attributes.
 */

static void
ipp_validate_job(_ipp_client_t *client)	/* I - Client */
{
  if (valid_job_attributes(client))
    respond_ipp(client, IPP_STATUS_OK, NULL);
}


#if 0
/*
 * 'parse_options()' - Parse URL options into CUPS options.
 *
 * The client->options string is destroyed by this function.
 */

static int				/* O - Number of options */
parse_options(_ipp_client_t *client,	/* I - Client */
              cups_option_t **options)	/* O - Options */
{
  char	*name,				/* Name */
      	*value,				/* Value */
	*next;				/* Next name=value pair */
  int	num_options = 0;		/* Number of options */


  *options = NULL;

  for (name = client->options; name && *name; name = next)
  {
    if ((value = strchr(name, '=')) == NULL)
      break;

    *value++ = '\0';
    if ((next = strchr(value, '&')) != NULL)
      *next++ = '\0';

    num_options = cupsAddOption(name, value, num_options, options);
  }

  return (num_options);
}
#endif /* 0 */


/*
 * 'process_client()' - Process client requests on a thread.
 */

static void *				/* O - Exit status */
process_client(_ipp_client_t *client)	/* I - Client */
{
 /*
  * Loop until we are out of requests or timeout (30 seconds)...
  */

#ifdef HAVE_SSL
  int first_time = 1;			/* First time request? */
#endif /* HAVE_SSL */

  while (httpWait(client->http, 30000))
  {
#ifdef HAVE_SSL
    if (first_time)
    {
     /*
      * See if we need to negotiate a TLS connection...
      */

      char buf[1];			/* First byte from client */

      if (recv(httpGetFd(client->http), buf, 1, MSG_PEEK) == 1 && (!buf[0] || !strchr("DGHOPT", buf[0])))
      {
        fprintf(stderr, "%s Starting HTTPS session.\n", client->hostname);

	if (httpEncryption(client->http, HTTP_ENCRYPTION_ALWAYS))
	{
	  fprintf(stderr, "%s Unable to encrypt connection: %s\n", client->hostname, cupsLastErrorString());
	  break;
        }

        fprintf(stderr, "%s Connection now encrypted.\n", client->hostname);
      }

      first_time = 0;
    }
#endif /* HAVE_SSL */

    if (!process_http(client))
      break;
  }

 /*
  * Close the conection to the client and return...
  */

  delete_client(client);

  return (NULL);
}


/*
 * 'process_http()' - Process a HTTP request.
 */

int					/* O - 1 on success, 0 on failure */
process_http(_ipp_client_t *client)	/* I - Client connection */
{
  char			uri[1024];	/* URI */
  http_state_t		http_state;	/* HTTP state */
  http_status_t		http_status;	/* HTTP status */
  ipp_state_t		ipp_state;	/* State of IPP transfer */
  char			scheme[32],	/* Method/scheme */
			userpass[128],	/* Username:password */
			hostname[HTTP_MAX_HOST];
					/* Hostname */
  int			port;		/* Port number */
  const char		*encoding;	/* Content-Encoding value */
  static const char * const http_states[] =
  {					/* Strings for logging HTTP method */
    "WAITING",
    "OPTIONS",
    "GET",
    "GET_SEND",
    "HEAD",
    "POST",
    "POST_RECV",
    "POST_SEND",
    "PUT",
    "PUT_RECV",
    "DELETE",
    "TRACE",
    "CONNECT",
    "STATUS",
    "UNKNOWN_METHOD",
    "UNKNOWN_VERSION"
  };


 /*
  * Clear state variables...
  */

  ippDelete(client->request);
  ippDelete(client->response);

  client->request   = NULL;
  client->response  = NULL;
  client->operation = HTTP_STATE_WAITING;

 /*
  * Read a request from the connection...
  */

  while ((http_state = httpReadRequest(client->http, uri,
                                       sizeof(uri))) == HTTP_STATE_WAITING)
    usleep(1);

 /*
  * Parse the request line...
  */

  if (http_state == HTTP_STATE_ERROR)
  {
    if (httpError(client->http) == EPIPE)
      fprintf(stderr, "%s Client closed connection.\n", client->hostname);
    else
      fprintf(stderr, "%s Bad request line (%s).\n", client->hostname,
              strerror(httpError(client->http)));

    return (0);
  }
  else if (http_state == HTTP_STATE_UNKNOWN_METHOD)
  {
    fprintf(stderr, "%s Bad/unknown operation.\n", client->hostname);
    respond_http(client, HTTP_STATUS_BAD_REQUEST, NULL, NULL, 0);
    return (0);
  }
  else if (http_state == HTTP_STATE_UNKNOWN_VERSION)
  {
    fprintf(stderr, "%s Bad HTTP version.\n", client->hostname);
    respond_http(client, HTTP_STATUS_BAD_REQUEST, NULL, NULL, 0);
    return (0);
  }

  fprintf(stderr, "%s %s %s\n", client->hostname, http_states[http_state],
          uri);

 /*
  * Separate the URI into its components...
  */

  if (httpSeparateURI(HTTP_URI_CODING_MOST, uri, scheme, sizeof(scheme),
		      userpass, sizeof(userpass),
		      hostname, sizeof(hostname), &port,
		      client->uri, sizeof(client->uri)) < HTTP_URI_STATUS_OK &&
      (http_state != HTTP_STATE_OPTIONS || strcmp(uri, "*")))
  {
    fprintf(stderr, "%s Bad URI \"%s\".\n", client->hostname, uri);
    respond_http(client, HTTP_STATUS_BAD_REQUEST, NULL, NULL, 0);
    return (0);
  }

  if ((client->options = strchr(client->uri, '?')) != NULL)
    *(client->options)++ = '\0';

 /*
  * Process the request...
  */

  client->start     = time(NULL);
  client->operation = httpGetState(client->http);

 /*
  * Parse incoming parameters until the status changes...
  */

  while ((http_status = httpUpdate(client->http)) == HTTP_STATUS_CONTINUE);

  if (http_status != HTTP_STATUS_OK)
  {
    respond_http(client, HTTP_STATUS_BAD_REQUEST, NULL, NULL, 0);
    return (0);
  }

  if (!httpGetField(client->http, HTTP_FIELD_HOST)[0] &&
      httpGetVersion(client->http) >= HTTP_VERSION_1_1)
  {
   /*
    * HTTP/1.1 and higher require the "Host:" field...
    */

    respond_http(client, HTTP_STATUS_BAD_REQUEST, NULL, NULL, 0);
    return (0);
  }

 /*
  * Handle HTTP Upgrade...
  */

  if (!strcasecmp(httpGetField(client->http, HTTP_FIELD_CONNECTION),
                        "Upgrade"))
  {
#ifdef HAVE_SSL
    if (strstr(httpGetField(client->http, HTTP_FIELD_UPGRADE), "TLS/") != NULL && !httpIsEncrypted(client->http))
    {
      if (!respond_http(client, HTTP_STATUS_SWITCHING_PROTOCOLS, NULL, NULL, 0))
        return (0);

      fprintf(stderr, "%s Upgrading to encrypted connection.\n", client->hostname);

      if (httpEncryption(client->http, HTTP_ENCRYPTION_REQUIRED))
      {
        fprintf(stderr, "%s Unable to encrypt connection: %s\n", client->hostname, cupsLastErrorString());
	return (0);
      }

      fprintf(stderr, "%s Connection now encrypted.\n", client->hostname);
    }
    else
#endif /* HAVE_SSL */

    if (!respond_http(client, HTTP_STATUS_NOT_IMPLEMENTED, NULL, NULL, 0))
      return (0);
  }

 /*
  * Handle HTTP Expect...
  */

  if (httpGetExpect(client->http) &&
      (client->operation == HTTP_STATE_POST ||
       client->operation == HTTP_STATE_PUT))
  {
    if (httpGetExpect(client->http) == HTTP_STATUS_CONTINUE)
    {
     /*
      * Send 100-continue header...
      */

      if (!respond_http(client, HTTP_STATUS_CONTINUE, NULL, NULL, 0))
	return (0);
    }
    else
    {
     /*
      * Send 417-expectation-failed header...
      */

      if (!respond_http(client, HTTP_STATUS_EXPECTATION_FAILED, NULL, NULL, 0))
	return (0);
    }
  }

 /*
  * Handle new transfers...
  */

  encoding = httpGetContentEncoding(client->http);

  switch (client->operation)
  {
    case HTTP_STATE_OPTIONS :
       /*
	* Do OPTIONS command...
	*/

	return (respond_http(client, HTTP_STATUS_OK, NULL, NULL, 0));

    case HTTP_STATE_HEAD :
#if 0 /* TODO: Work out icon support */
        if (!strcmp(client->uri, "/icon.png"))
	  return (respond_http(client, HTTP_STATUS_OK, NULL, "image/png", 0));
	else
#endif /* 0 */
	if (!strcmp(client->uri, "/") || !strcmp(client->uri, "/media") || !strcmp(client->uri, "/supplies"))
	  return (respond_http(client, HTTP_STATUS_OK, NULL, "text/html", 0));
	else
	  return (respond_http(client, HTTP_STATUS_NOT_FOUND, NULL, NULL, 0));

    case HTTP_STATE_GET :
#if 0 /* TODO: Work out icon support */
        if (!strcmp(client->uri, "/icon.png"))
	{
	 /*
	  * Send PNG icon file.
	  */

          int		fd;		/* Icon file */
	  struct stat	fileinfo;	/* Icon file information */
	  char		buffer[4096];	/* Copy buffer */
	  ssize_t	bytes;		/* Bytes */

          fprintf(stderr, "Icon file is \"%s\".\n", client->printer->icon);

          if (!stat(client->printer->icon, &fileinfo) &&
	      (fd = open(client->printer->icon, O_RDONLY)) >= 0)
	  {
	    if (!respond_http(client, HTTP_STATUS_OK, NULL, "image/png",
	                      (size_t)fileinfo.st_size))
	    {
	      close(fd);
	      return (0);
	    }

	    while ((bytes = read(fd, buffer, sizeof(buffer))) > 0)
	      httpWrite2(client->http, buffer, (size_t)bytes);

	    httpFlushWrite(client->http);

	    close(fd);
	  }
	  else
	    return (respond_http(client, HTTP_STATUS_NOT_FOUND, NULL, NULL, 0));
	}
	else
#endif /* 0 */
	if (!strcmp(client->uri, "/"))
	{
	 /*
	  * Show web status page...
	  */

          _ipp_job_t	*job;		/* Current job */
	  int		i;		/* Looping var */
	  _ipp_preason_t reason;	/* Current reason */
	  static const char * const reasons[] =
	  {				/* Reason strings */
	    "Other",
	    "Cover Open",
	    "Input Tray Missing",
	    "Marker Supply Empty",
	    "Marker Supply Low",
	    "Marker Waste Almost Full",
	    "Marker Waste Full",
	    "Media Empty",
	    "Media Jam",
	    "Media Low",
	    "Media Needed",
	    "Moving to Paused",
	    "Paused",
	    "Spool Area Full",
	    "Toner Empty",
	    "Toner Low"
	  };

          if (!respond_http(client, HTTP_STATUS_OK, encoding, "text/html", 0))
	    return (0);

          html_header(client, client->printer->name);
          html_printf(client,
		      "<p><img align=\"right\" src=\"/icon.png\" width=\"64\" height=\"64\"><b>ippserver (" CUPS_SVERSION ")</b></p>\n"
		      "<p>%s, %d job(s).", client->printer->state == IPP_PSTATE_IDLE ? "Idle" : client->printer->state == IPP_PSTATE_PROCESSING ? "Printing" : "Stopped", cupsArrayCount(client->printer->jobs));
	  for (i = 0, reason = 1; i < (int)(sizeof(reasons) / sizeof(reasons[0])); i ++, reason <<= 1)
	    if (client->printer->state_reasons & reason)
	      html_printf(client, "\n<br>&nbsp;&nbsp;&nbsp;&nbsp;%s", reasons[i]);
	  html_printf(client, "</p>\n");
	  
          if (cupsArrayCount(client->printer->jobs) > 0)
	  {
            _cupsRWLockRead(&(client->printer->rwlock));

	    html_printf(client, "<table class=\"striped\" summary=\"Jobs\"><thead><tr><th>Job #</th><th>Name</th><th>Owner</th><th>When</th></tr></thead><tbody>\n");
	    for (job = (_ipp_job_t *)cupsArrayFirst(client->printer->jobs); job; job = (_ipp_job_t *)cupsArrayNext(client->printer->jobs))
	    {
	      char	when[256],	/* When job queued/started/finished */
			hhmmss[64];	/* Time HH:MM:SS */

              switch (job->state)
	      {
	        case IPP_JSTATE_PENDING :
	        case IPP_JSTATE_HELD :
		    snprintf(when, sizeof(when), "Queued at %s", time_string(job->created, hhmmss, sizeof(hhmmss)));
		    break;
	        case IPP_JSTATE_PROCESSING :
	        case IPP_JSTATE_STOPPED :
		    snprintf(when, sizeof(when), "Started at %s", time_string(job->processing, hhmmss, sizeof(hhmmss)));
		    break;
	        case IPP_JSTATE_ABORTED :
		    snprintf(when, sizeof(when), "Aborted at %s", time_string(job->completed, hhmmss, sizeof(hhmmss)));
		    break;
	        case IPP_JSTATE_CANCELED :
		    snprintf(when, sizeof(when), "Canceled at %s", time_string(job->completed, hhmmss, sizeof(hhmmss)));
		    break;
	        case IPP_JSTATE_COMPLETED :
		    snprintf(when, sizeof(when), "Completed at %s", time_string(job->completed, hhmmss, sizeof(hhmmss)));
		    break;
	      }

	      html_printf(client, "<tr><td>%d</td><td>%s</td><td>%s</td><td>%s</td></tr>\n", job->id, job->name, job->username, when);
	    }
	    html_printf(client, "</tbody></table>\n");

	    _cupsRWUnlock(&(client->printer->rwlock));
	  }
          html_footer(client);

	  return (1);
	}
#if 0 /* TODO: Pull media and supply info from device attrs */
	else if (!strcmp(client->uri, "/media"))
	{
	 /*
	  * Show web media page...
	  */

	  int		i,		/* Looping var */
			num_options;	/* Number of form options */
	  cups_option_t	*options;	/* Form options */
          static const char * const sizes[] =
	  {				/* Size strings */
	    "ISO A4",
	    "ISO A5",
	    "ISO A6",
	    "DL Envelope",
	    "US Legal",
	    "US Letter",
	    "#10 Envelope",
	    "3x5 Photo",
	    "3.5x5 Photo",
	    "4x6 Photo",
	    "5x7 Photo"
	  };
	  static const char * const types[] =
					  /* Type strings */
	  {
	    "Auto",
	    "Cardstock",
	    "Envelope",
	    "Labels",
	    "Other",
	    "Glossy Photo",
	    "High-Gloss Photo",
	    "Matte Photo",
	    "Satin Photo",
	    "Semi-Gloss Photo",
	    "Plain",
	    "Letterhead",
	    "Transparency"
	  };
	  static const int sheets[] =	/* Number of sheets */
	  {
	    250,
	    100,
	    25,
	    5,
	    0
	  };

          if (!respond_http(client, HTTP_STATUS_OK, encoding, "text/html", 0))
	    return (0);

          html_header(client, client->printer->name);

	  if ((num_options = parse_options(client, &options)) > 0)
	  {
	   /*
	    * WARNING: A real printer/server implementation MUST NOT implement
	    * media updates via a GET request - GET requests are supposed to be
	    * idempotent (without side-effects) and we obviously are not
	    * authenticating access here.  This form is provided solely to
	    * enable testing and development!
	    */

	    const char	*val;		/* Form value */

	    if ((val = cupsGetOption("main_size", num_options, options)) != NULL)
	      client->printer->main_size = atoi(val);
	    if ((val = cupsGetOption("main_type", num_options, options)) != NULL)
	      client->printer->main_type = atoi(val);
	    if ((val = cupsGetOption("main_level", num_options, options)) != NULL)
	      client->printer->main_level = atoi(val);

	    if ((val = cupsGetOption("envelope_size", num_options, options)) != NULL)
	      client->printer->envelope_size = atoi(val);
	    if ((val = cupsGetOption("envelope_level", num_options, options)) != NULL)
	      client->printer->envelope_level = atoi(val);

	    if ((val = cupsGetOption("photo_size", num_options, options)) != NULL)
	      client->printer->photo_size = atoi(val);
	    if ((val = cupsGetOption("photo_type", num_options, options)) != NULL)
	      client->printer->photo_type = atoi(val);
	    if ((val = cupsGetOption("photo_level", num_options, options)) != NULL)
	      client->printer->photo_level = atoi(val);

            if ((client->printer->main_level < 100 && client->printer->main_level > 0) || (client->printer->envelope_level < 25 && client->printer->envelope_level > 0) || (client->printer->photo_level < 25 && client->printer->photo_level > 0))
	      client->printer->state_reasons |= _IPP_PREASON_MEDIA_LOW;
	    else
	      client->printer->state_reasons &= (_ipp_preason_t)~_IPP_PREASON_MEDIA_LOW;

            if ((client->printer->main_level == 0 && client->printer->main_size > _IPP_MEDIA_SIZE_NONE) || (client->printer->envelope_level == 0 && client->printer->envelope_size > _IPP_MEDIA_SIZE_NONE) || (client->printer->photo_level == 0 && client->printer->photo_size > _IPP_MEDIA_SIZE_NONE))
	    {
	      client->printer->state_reasons |= _IPP_PREASON_MEDIA_EMPTY;
	      if (client->printer->active_job)
	        client->printer->state_reasons |= _IPP_PREASON_MEDIA_NEEDED;
	    }
	    else
	      client->printer->state_reasons &= (_ipp_preason_t)~(_IPP_PREASON_MEDIA_EMPTY | _IPP_PREASON_MEDIA_NEEDED);

	    html_printf(client, "<blockquote>Media updated.</blockquote>\n");
          }

          html_printf(client, "<form method=\"GET\" action=\"/media\">\n");

          html_printf(client, "<table class=\"form\" summary=\"Media\">\n");
          html_printf(client, "<tr><th>Main Tray:</th><td><select name=\"main_size\"><option value=\"-1\">None</option>");
          for (i = 0; i < (int)(sizeof(sizes) / sizeof(sizes[0])); i ++)
	    if (!strstr(sizes[i], "Envelope") && !strstr(sizes[i], "Photo"))
	      html_printf(client, "<option value=\"%d\"%s>%s</option>", i, i == client->printer->main_size ? " selected" : "", sizes[i]);
	  html_printf(client, "</select> <select name=\"main_type\"><option value=\"-1\">None</option>");
          for (i = 0; i < (int)(sizeof(types) / sizeof(types[0])); i ++)
	    if (!strstr(types[i], "Photo"))
	      html_printf(client, "<option value=\"%d\"%s>%s</option>", i, i == client->printer->main_type ? " selected" : "", types[i]);
	  html_printf(client, "</select> <select name=\"main_level\">");
          for (i = 0; i < (int)(sizeof(sheets) / sizeof(sheets[0])); i ++)
	    html_printf(client, "<option value=\"%d\"%s>%d sheets</option>", sheets[i], sheets[i] == client->printer->main_level ? " selected" : "", sheets[i]);
	  html_printf(client, "</select></td></tr>\n");

          html_printf(client,
		      "<tr><th>Envelope Feeder:</th><td><select name=\"envelope_size\"><option value=\"-1\">None</option>");
          for (i = 0; i < (int)(sizeof(sizes) / sizeof(sizes[0])); i ++)
	    if (strstr(sizes[i], "Envelope"))
	      html_printf(client, "<option value=\"%d\"%s>%s</option>", i, i == client->printer->envelope_size ? " selected" : "", sizes[i]);
	  html_printf(client, "</select> <select name=\"envelope_level\">");
          for (i = 0; i < (int)(sizeof(sheets) / sizeof(sheets[0])); i ++)
	    html_printf(client, "<option value=\"%d\"%s>%d sheets</option>", sheets[i], sheets[i] == client->printer->envelope_level ? " selected" : "", sheets[i]);
	  html_printf(client, "</select></td></tr>\n");

          html_printf(client,
		      "<tr><th>Photo Tray:</th><td><select name=\"photo_size\"><option value=\"-1\">None</option>");
          for (i = 0; i < (int)(sizeof(sizes) / sizeof(sizes[0])); i ++)
	    if (strstr(sizes[i], "Photo"))
	      html_printf(client, "<option value=\"%d\"%s>%s</option>", i, i == client->printer->photo_size ? " selected" : "", sizes[i]);
	  html_printf(client, "</select> <select name=\"photo_type\"><option value=\"-1\">None</option>");
          for (i = 0; i < (int)(sizeof(types) / sizeof(types[0])); i ++)
	    if (strstr(types[i], "Photo"))
	      html_printf(client, "<option value=\"%d\"%s>%s</option>", i, i == client->printer->photo_type ? " selected" : "", types[i]);
	  html_printf(client, "</select> <select name=\"photo_level\">");
          for (i = 0; i < (int)(sizeof(sheets) / sizeof(sheets[0])); i ++)
	    html_printf(client, "<option value=\"%d\"%s>%d sheets</option>", sheets[i], sheets[i] == client->printer->photo_level ? " selected" : "", sheets[i]);
	  html_printf(client, "</select></td></tr>\n");

	  html_printf(client, "<tr><td></td><td><input type=\"submit\" value=\"Update Media\"></td></tr></table></form>\n");
          html_footer(client);

	  return (1);
	}
	else if (!strcmp(client->uri, "/supplies"))
	{
	 /*
	  * Show web supplies page...
	  */

          int		i, j,		/* Looping vars */
			num_options;	/* Number of form options */
	  cups_option_t	*options;	/* Form options */
	  static const int levels[] = { 0, 5, 10, 25, 50, 75, 90, 95, 100 };

          if (!respond_http(client, HTTP_STATUS_OK, encoding, "text/html", 0))
	    return (0);

          html_header(client, client->printer->name);

	  if ((num_options = parse_options(client, &options)) > 0)
	  {
	   /*
	    * WARNING: A real printer/server implementation MUST NOT implement
	    * supply updates via a GET request - GET requests are supposed to be
	    * idempotent (without side-effects) and we obviously are not
	    * authenticating access here.  This form is provided solely to
	    * enable testing and development!
	    */

	    char	name[64];	/* Form field */
	    const char	*val;		/* Form value */

            client->printer->state_reasons &= (_ipp_preason_t)~(_IPP_PREASON_MARKER_SUPPLY_EMPTY | _IPP_PREASON_MARKER_SUPPLY_LOW | _IPP_PREASON_MARKER_WASTE_ALMOST_FULL | _IPP_PREASON_MARKER_WASTE_FULL | _IPP_PREASON_TONER_EMPTY | _IPP_PREASON_TONER_LOW);

	    for (i = 0; i < (int)(sizeof(printer_supplies) / sizeof(printer_supplies[0])); i ++)
	    {
	      snprintf(name, sizeof(name), "supply_%d", i);
	      if ((val = cupsGetOption(name, num_options, options)) != NULL)
	      {
		int level = client->printer->supplies[i] = atoi(val);
					/* New level */

		if (i < 4)
		{
		  if (level == 0)
		    client->printer->state_reasons |= _IPP_PREASON_TONER_EMPTY;
		  else if (level < 10)
		    client->printer->state_reasons |= _IPP_PREASON_TONER_LOW;
		}
		else
		{
		  if (level == 100)
		    client->printer->state_reasons |= _IPP_PREASON_MARKER_WASTE_FULL;
		  else if (level > 90)
		    client->printer->state_reasons |= _IPP_PREASON_MARKER_WASTE_ALMOST_FULL;
		}
	      }
            }

	    html_printf(client, "<blockquote>Supplies updated.</blockquote>\n");
          }

          html_printf(client, "<form method=\"GET\" action=\"/supplies\">\n");

	  html_printf(client, "<table class=\"form\" summary=\"Supplies\">\n");
	  for (i = 0; i < (int)(sizeof(printer_supplies) / sizeof(printer_supplies[0])); i ++)
	  {
	    html_printf(client, "<tr><th>%s:</th><td><select name=\"supply_%d\">", printer_supplies[i], i);
	    for (j = 0; j < (int)(sizeof(levels) / sizeof(levels[0])); j ++)
	      html_printf(client, "<option value=\"%d\"%s>%d%%</option>", levels[j], levels[j] == client->printer->supplies[i] ? " selected" : "", levels[j]);
	    html_printf(client, "</select></td></tr>\n");
	  }
	  html_printf(client, "<tr><td></td><td><input type=\"submit\" value=\"Update Supplies\"></td></tr>\n</table>\n</form>\n");
          html_footer(client);

	  return (1);
	}
#endif /* 0 */
	else
	  return (respond_http(client, HTTP_STATUS_NOT_FOUND, NULL, NULL, 0));
	break;

    case HTTP_STATE_POST :
	if (strcmp(httpGetField(client->http, HTTP_FIELD_CONTENT_TYPE),
	           "application/ipp"))
        {
	 /*
	  * Not an IPP request...
	  */

	  return (respond_http(client, HTTP_STATUS_BAD_REQUEST, NULL, NULL, 0));
	}

       /*
        * Read the IPP request...
	*/

	client->request = ippNew();

        while ((ipp_state = ippRead(client->http,
                                    client->request)) != IPP_STATE_DATA)
	{
	  if (ipp_state == IPP_STATE_ERROR)
	  {
            fprintf(stderr, "%s IPP read error (%s).\n", client->hostname,
	            cupsLastErrorString());
	    respond_http(client, HTTP_STATUS_BAD_REQUEST, NULL, NULL, 0);
	    return (0);
	  }
	}

       /*
        * Now that we have the IPP request, process the request...
	*/

        return (process_ipp(client));

    default :
        break; /* Anti-compiler-warning-code */
  }

  return (1);
}


/*
 * 'process_ipp()' - Process an IPP request.
 */

static int				/* O - 1 on success, 0 on error */
process_ipp(_ipp_client_t *client)	/* I - Client */
{
  ipp_tag_t		group;		/* Current group tag */
  ipp_attribute_t	*attr;		/* Current attribute */
  ipp_attribute_t	*charset;	/* Character set attribute */
  ipp_attribute_t	*language;	/* Language attribute */
  ipp_attribute_t	*uri;		/* Printer URI attribute */
  int			major, minor;	/* Version number */
  const char		*name;		/* Name of attribute */


  debug_attributes("Request", client->request, 1);

 /*
  * First build an empty response message for this request...
  */

  client->operation_id = ippGetOperation(client->request);
  client->response     = ippNewResponse(client->request);

 /*
  * Then validate the request header and required attributes...
  */

  major = ippGetVersion(client->request, &minor);

  if (major < 1 || major > 2)
  {
   /*
    * Return an error, since we only support IPP 1.x and 2.x.
    */

    respond_ipp(client, IPP_STATUS_ERROR_VERSION_NOT_SUPPORTED,
                "Bad request version number %d.%d.", major, minor);
  }
  else if (ippGetRequestId(client->request) <= 0)
    respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST, "Bad request-id %d.",
                ippGetRequestId(client->request));
  else if (!ippFirstAttribute(client->request))
    respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST,
                "No attributes in request.");
  else
  {
   /*
    * Make sure that the attributes are provided in the correct order and
    * don't repeat groups...
    */

    for (attr = ippFirstAttribute(client->request),
             group = ippGetGroupTag(attr);
	 attr;
	 attr = ippNextAttribute(client->request))
    {
      if (ippGetGroupTag(attr) < group && ippGetGroupTag(attr) != IPP_TAG_ZERO)
      {
       /*
	* Out of order; return an error...
	*/

	respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST,
		    "Attribute groups are out of order (%x < %x).",
		    ippGetGroupTag(attr), group);
	break;
      }
      else
	group = ippGetGroupTag(attr);
    }

    if (!attr)
    {
     /*
      * Then make sure that the first three attributes are:
      *
      *     attributes-charset
      *     attributes-natural-language
      *     printer-uri/job-uri
      */

      attr = ippFirstAttribute(client->request);
      name = ippGetName(attr);
      if (attr && name && !strcmp(name, "attributes-charset") &&
	  ippGetValueTag(attr) == IPP_TAG_CHARSET)
	charset = attr;
      else
	charset = NULL;

      attr = ippNextAttribute(client->request);
      name = ippGetName(attr);

      if (attr && name && !strcmp(name, "attributes-natural-language") &&
	  ippGetValueTag(attr) == IPP_TAG_LANGUAGE)
	language = attr;
      else
	language = NULL;

      if ((attr = ippFindAttribute(client->request, "printer-uri",
                                   IPP_TAG_URI)) != NULL)
	uri = attr;
      else if ((attr = ippFindAttribute(client->request, "job-uri",
                                        IPP_TAG_URI)) != NULL)
	uri = attr;
      else
	uri = NULL;

      if (charset &&
          strcasecmp(ippGetString(charset, 0, NULL), "us-ascii") &&
          strcasecmp(ippGetString(charset, 0, NULL), "utf-8"))
      {
       /*
        * Bad character set...
	*/

	respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST,
	            "Unsupported character set \"%s\".",
	            ippGetString(charset, 0, NULL));
      }
      else if (!charset || !language || !uri)
      {
       /*
	* Return an error, since attributes-charset,
	* attributes-natural-language, and printer-uri/job-uri are required
	* for all operations.
	*/

	respond_ipp(client, IPP_STATUS_ERROR_BAD_REQUEST,
	            "Missing required attributes.");
      }
      else
      {
        char		scheme[32],	/* URI scheme */
			userpass[32],	/* Username/password in URI */
			host[256],	/* Host name in URI */
			resource[256];	/* Resource path in URI */
	int		port;		/* Port number in URI */

        name = ippGetName(uri);

        if (httpSeparateURI(HTTP_URI_CODING_ALL, ippGetString(uri, 0, NULL),
                            scheme, sizeof(scheme),
                            userpass, sizeof(userpass),
                            host, sizeof(host), &port,
                            resource, sizeof(resource)) < HTTP_URI_STATUS_OK)
	  respond_ipp(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES,
	              "Bad %s value '%s'.", name, ippGetString(uri, 0, NULL));
        else if ((!strcmp(name, "job-uri") && strncmp(resource, "/ipp/print/", 11)) ||
                 (!strcmp(name, "printer-uri") && strcmp(resource, "/ipp/print")))
	  respond_ipp(client, IPP_STATUS_ERROR_NOT_FOUND, "%s %s not found.",
		      name, ippGetString(uri, 0, NULL));
	else
	{
	 /*
	  * Try processing the operation...
	  */

	  switch ((int)ippGetOperation(client->request))
	  {
	    case IPP_OP_PRINT_JOB :
		ipp_print_job(client);
		break;

	    case IPP_OP_PRINT_URI :
		ipp_print_uri(client);
		break;

	    case IPP_OP_VALIDATE_JOB :
		ipp_validate_job(client);
		break;

	    case IPP_OP_CREATE_JOB :
		ipp_create_job(client);
		break;

	    case IPP_OP_SEND_DOCUMENT :
		ipp_send_document(client);
		break;

	    case IPP_OP_SEND_URI :
		ipp_send_uri(client);
		break;

	    case IPP_OP_CANCEL_JOB :
		ipp_cancel_job(client);
		break;

	    case IPP_OP_CANCEL_MY_JOBS :
		ipp_cancel_my_jobs(client);
		break;

	    case IPP_OP_GET_JOB_ATTRIBUTES :
		ipp_get_job_attributes(client);
		break;

	    case IPP_OP_GET_JOBS :
		ipp_get_jobs(client);
		break;

	    case IPP_OP_GET_PRINTER_ATTRIBUTES :
		ipp_get_printer_attributes(client);
		break;

	    case IPP_OP_GET_PRINTER_SUPPORTED_VALUES :
		ipp_get_printer_supported_values(client);
		break;

	    case IPP_OP_CLOSE_JOB :
	        ipp_close_job(client);
		break;

	    case IPP_OP_IDENTIFY_PRINTER :
	        ipp_identify_printer(client);
		break;

	    case IPP_OP_CANCEL_SUBSCRIPTION :
	        ipp_cancel_subscription(client);
		break;

            case IPP_OP_CREATE_JOB_SUBSCRIPTIONS :
	    case IPP_OP_CREATE_PRINTER_SUBSCRIPTIONS :
	        ipp_create_xxx_subscriptions(client);
		break;

	    case IPP_OP_GET_NOTIFICATIONS :
	        ipp_get_notifications(client);
		break;

	    case IPP_OP_GET_SUBSCRIPTION_ATTRIBUTES :
	        ipp_get_subscription_attributes(client);
		break;

	    case IPP_OP_GET_SUBSCRIPTIONS :
	        ipp_get_subscriptions(client);
		break;

	    case IPP_OP_RENEW_SUBSCRIPTION :
	        ipp_renew_subscription(client);
		break;

	    case IPP_OP_GET_DOCUMENT_ATTRIBUTES :
		ipp_get_document_attributes(client);
		break;

	    case IPP_OP_GET_DOCUMENTS :
		ipp_get_documents(client);
		break;

	    case IPP_OP_VALIDATE_DOCUMENT :
		ipp_validate_document(client);
		break;

            case _IPP_OP_ACKNOWLEDGE_DOCUMENT :
	        ipp_acknowledge_document(client);
		break;

            case _IPP_OP_ACKNOWLEDGE_IDENTIFY_PRINTER :
	        ipp_acknowledge_identify_printer(client);
		break;

            case _IPP_OP_ACKNOWLEDGE_JOB :
	        ipp_acknowledge_job(client);
		break;

            case _IPP_OP_FETCH_DOCUMENT :
	        ipp_fetch_document(client);
		break;

            case _IPP_OP_FETCH_JOB :
	        ipp_fetch_job(client);
		break;

            case _IPP_OP_GET_OUTPUT_DEVICE_ATTRIBUTES :
	        ipp_get_output_device_attributes(client);
		break;

            case _IPP_OP_UPDATE_ACTIVE_JOBS :
	        ipp_update_active_jobs(client);
		break;

            case _IPP_OP_UPDATE_DOCUMENT_STATUS :
	        ipp_update_document_status(client);
		break;

            case _IPP_OP_UPDATE_JOB_STATUS :
	        ipp_update_job_status(client);
		break;

            case _IPP_OP_UPDATE_OUTPUT_DEVICE_ATTRIBUTES :
	        ipp_update_output_device_attributes(client);
		break;

            case _IPP_OP_DEREGISTER_OUTPUT_DEVICE :
	        ipp_deregister_output_device(client);
		break;

	    default :
		respond_ipp(client, IPP_STATUS_ERROR_OPERATION_NOT_SUPPORTED,
			    "Operation not supported.");
		break;
	  }
	}
      }
    }
  }

 /*
  * Send the HTTP header and return...
  */

  if (httpGetState(client->http) != HTTP_STATE_POST_SEND)
    httpFlush(client->http);		/* Flush trailing (junk) data */

  return (respond_http(client, HTTP_STATUS_OK, NULL, "application/ipp",
                       client->fetch_file >= 0 ? 0 : ippLength(client->response)));
}


/*
 * 'process_job()' - Process a print job.
 */

static void *				/* O - Thread exit status */
process_job(_ipp_job_t *job)		/* I - Job */
{
  job->state                   = IPP_JSTATE_PROCESSING;
  job->printer->state          = IPP_PSTATE_PROCESSING;
  job->processing              = time(NULL);
  job->printer->processing_job = job;

  add_event(job->printer, job, _IPP_EVENT_JOB_STATE_CHANGED, "Job processing.");

 /*
  * TODO: Perform any preprocessing needed...
  */

  // job->state_reasons |= _IPP_JREASON_JOB_TRANSFORMING;
  // job->state_reasons &= ~_IPP_JREASON_JOB_TRANSFORMING;

 /*
  * Set the state to processing-stopped, fetchable, then send a
  * notification.
  */

  job->state         = IPP_JSTATE_STOPPED;
  job->state_reasons |= _IPP_JREASON_JOB_FETCHABLE;

  add_event(job->printer, job, _IPP_EVENT_JOB_STATE_CHANGED, "Job fetchable.");

  return (NULL);
}


/*
 * 'respond_http()' - Send a HTTP response.
 */

int					/* O - 1 on success, 0 on failure */
respond_http(
    _ipp_client_t *client,		/* I - Client */
    http_status_t code,			/* I - HTTP status of response */
    const char    *content_encoding,	/* I - Content-Encoding of response */
    const char    *type,		/* I - MIME media type of response */
    size_t        length)		/* I - Length of response */
{
  char	message[1024];			/* Text message */


  fprintf(stderr, "%s %s\n", client->hostname, httpStatus(code));

  if (code == HTTP_STATUS_CONTINUE)
  {
   /*
    * 100-continue doesn't send any headers...
    */

    return (httpWriteResponse(client->http, HTTP_STATUS_CONTINUE) == 0);
  }

 /*
  * Format an error message...
  */

  if (!type && !length && code != HTTP_STATUS_OK && code != HTTP_STATUS_SWITCHING_PROTOCOLS)
  {
    snprintf(message, sizeof(message), "%d - %s\n", code, httpStatus(code));

    type   = "text/plain";
    length = strlen(message);
  }
  else
    message[0] = '\0';

 /*
  * Send the HTTP response header...
  */

  httpClearFields(client->http);

  if (code == HTTP_STATUS_METHOD_NOT_ALLOWED ||
      client->operation == HTTP_STATE_OPTIONS)
    httpSetField(client->http, HTTP_FIELD_ALLOW, "GET, HEAD, OPTIONS, POST");

  if (type)
  {
    if (!strcmp(type, "text/html"))
      httpSetField(client->http, HTTP_FIELD_CONTENT_TYPE,
                   "text/html; charset=utf-8");
    else
      httpSetField(client->http, HTTP_FIELD_CONTENT_TYPE, type);

    if (content_encoding)
      httpSetField(client->http, HTTP_FIELD_CONTENT_ENCODING, content_encoding);
  }

  httpSetLength(client->http, length);

  if (httpWriteResponse(client->http, code) < 0)
    return (0);

 /*
  * Send the response data...
  */

  if (message[0])
  {
   /*
    * Send a plain text message.
    */

    if (httpPrintf(client->http, "%s", message) < 0)
      return (0);

    if (httpWrite2(client->http, "", 0) < 0)
      return (0);
  }
  else if (client->response)
  {
   /*
    * Send an IPP response...
    */

    debug_attributes("Response", client->response, 2);

    ippSetState(client->response, IPP_STATE_IDLE);

    if (ippWrite(client->http, client->response) != IPP_STATE_DATA)
      return (0);

    if (client->fetch_file >= 0)
    {
      ssize_t	bytes;			/* Bytes read */
      char	buffer[32768];		/* Buffer */

      if (client->fetch_compression)
        httpSetField(client->http, HTTP_FIELD_CONTENT_ENCODING, "gzip");

      while ((bytes = read(client->fetch_file, buffer, sizeof(buffer))) > 0)
        httpWrite2(client->http, buffer, (size_t)bytes);

      httpWrite2(client->http, "", 0);
      close(client->fetch_file);
      client->fetch_file = -1;
    }
  }

  return (1);
}


/*
 * 'respond_ipp()' - Send an IPP response.
 */

static void
respond_ipp(_ipp_client_t *client,	/* I - Client */
            ipp_status_t  status,	/* I - status-code */
	    const char    *message,	/* I - printf-style status-message */
	    ...)			/* I - Additional args as needed */
{
  const char	*formatted = NULL;	/* Formatted message */


  ippSetStatusCode(client->response, status);

  if (message)
  {
    va_list		ap;		/* Pointer to additional args */
    ipp_attribute_t	*attr;		/* New status-message attribute */

    va_start(ap, message);
    if ((attr = ippFindAttribute(client->response, "status-message",
				 IPP_TAG_TEXT)) != NULL)
      ippSetStringfv(client->response, &attr, 0, message, ap);
    else
      attr = ippAddStringfv(client->response, IPP_TAG_OPERATION, IPP_TAG_TEXT,
			    "status-message", NULL, message, ap);
    va_end(ap);

    formatted = ippGetString(attr, 0, NULL);
  }

  if (formatted)
    fprintf(stderr, "%s %s %s (%s)\n", client->hostname,
	    ippOpString(client->operation_id), ippErrorString(status),
	    formatted);
  else
    fprintf(stderr, "%s %s %s\n", client->hostname,
	    ippOpString(client->operation_id), ippErrorString(status));
}


/*
 * 'respond_unsupported()' - Respond with an unsupported attribute.
 */

static void
respond_unsupported(
    _ipp_client_t   *client,		/* I - Client */
    ipp_attribute_t *attr)		/* I - Atribute */
{
  ipp_attribute_t	*temp;		/* Copy of attribute */


  respond_ipp(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES,
              "Unsupported %s %s%s value.", ippGetName(attr),
              ippGetCount(attr) > 1 ? "1setOf " : "",
	      ippTagString(ippGetValueTag(attr)));

  temp = ippCopyAttribute(client->response, attr, 0);
  ippSetGroupTag(client->response, &temp, IPP_TAG_UNSUPPORTED_GROUP);
}


/*
 * 'run_printer()' - Run the printer service.
 */

static void
run_printer(_ipp_printer_t *printer)	/* I - Printer */
{
  int		num_fds;		/* Number of file descriptors */
  struct pollfd	polldata[3];		/* poll() data */
  int		timeout;		/* Timeout for poll() */
  _ipp_client_t	*client;		/* New client */


 /*
  * Setup poll() data for the Bonjour service socket and IPv4/6 listeners...
  */

  polldata[0].fd     = printer->ipv4;
  polldata[0].events = POLLIN;

  polldata[1].fd     = printer->ipv6;
  polldata[1].events = POLLIN;

  num_fds = 2;

 /*
  * Loop until we are killed or have a hard error...
  */

  for (;;)
  {
    if (cupsArrayCount(printer->jobs))
      timeout = 10;
    else
      timeout = -1;

    if (poll(polldata, (nfds_t)num_fds, timeout) < 0 && errno != EINTR)
    {
      perror("poll() failed");
      break;
    }

    if (polldata[0].revents & POLLIN)
    {
      if ((client = create_client(printer, printer->ipv4)) != NULL)
      {
	if (!_cupsThreadCreate((_cups_thread_func_t)process_client, client))
	{
	  perror("Unable to create client thread");
	  delete_client(client);
	}
      }
    }

    if (polldata[1].revents & POLLIN)
    {
      if ((client = create_client(printer, printer->ipv6)) != NULL)
      {
	if (!_cupsThreadCreate((_cups_thread_func_t)process_client, client))
	{
	  perror("Unable to create client thread");
	  delete_client(client);
	}
      }
    }

   /*
    * Clean out old jobs...
    */

    clean_jobs(printer);
  }
}


/*
 * 'time_string()' - Return the local time in hours, minutes, and seconds.
 */

static char *
time_string(time_t tv,			/* I - Time value */
            char   *buffer,		/* I - Buffer */
	    size_t bufsize)		/* I - Size of buffer */
{
  struct tm	*curtime = localtime(&tv);
					/* Local time */

  strftime(buffer, bufsize, "%X", curtime);
  return (buffer);
}


/*
 * 'update_device_attributes_no_lock()' - Update the composite device attributes.
 *
 * Note: Caller MUST lock the printer object for writing before using.
 */

static void
update_device_attributes_no_lock(
    _ipp_printer_t *printer)		/* I - Printer */
{
  _ipp_device_t		*device;	/* Current device */
  ipp_t			*dev_attrs;	/* Device attributes */


 /* TODO: Support multiple output devices, icons, etc... */
  device    = (_ipp_device_t *)cupsArrayFirst(printer->devices);
  dev_attrs = ippNew();

  if (device)
    copy_attributes(dev_attrs, device->attrs, NULL, IPP_TAG_PRINTER, 0);

  ippDelete(printer->dev_attrs);
  printer->dev_attrs = dev_attrs;

  printer->config_time = time(NULL);
}


/*
 * 'update_device_status_no_lock()' - Update the composite device state.
 *
 * Note: Caller MUST lock the printer object for writing before using.
 */

static void
update_device_state_no_lock(
    _ipp_printer_t *printer)		/* I - Printer */
{
  _ipp_device_t		*device;	/* Current device */
  ipp_attribute_t	*attr;		/* Current attribute */


 /* TODO: Support multiple output devices, icons, etc... */
  device = (_ipp_device_t *)cupsArrayFirst(printer->devices);

  if ((attr = ippFindAttribute(device->attrs, "printer-state", IPP_TAG_ENUM)) != NULL)
    printer->dev_state = (ipp_pstate_t)ippGetInteger(attr, 0);
  else
    printer->dev_state = IPP_PSTATE_STOPPED;

  if ((attr = ippFindAttribute(device->attrs, "printer-state-reasons", IPP_TAG_KEYWORD)) != NULL)
    printer->dev_reasons = get_printer_state_reasons_bits(attr);
  else
    printer->dev_reasons = _IPP_PREASON_PAUSED;

  printer->state_time = time(NULL);
}


/*
 * 'usage()' - Show program usage.
 */

static void
usage(int status)			/* O - Exit status */
{
  if (!status)
  {
    puts(CUPS_SVERSION " - Copyright 2010-2014 by Apple Inc. All rights reserved.");
    puts("");
  }

  puts("Usage: ippinfra [options] \"name\"");
  puts("");
  puts("Options:");
  printf("-d spool-directory      Spool directory "
         "(default=/tmp/ippserver.%d)\n", (int)getpid());
  puts("-h                      Show program help");
  puts("-k                      Keep job spool files");
  puts("-n hostname             Hostname for printer");
  puts("-p port                 Port number (default=auto)");
  puts("-u user:pass            Set proxy username and password");
  puts("-v[vvv]                 Be (very) verbose");

  exit(status);
}


/*
 * 'valid_doc_attributes()' - Determine whether the document attributes are
 *                            valid.
 *
 * When one or more document attributes are invalid, this function adds a
 * suitable response and attributes to the unsupported group.
 */

static int				/* O - 1 if valid, 0 if not */
valid_doc_attributes(
    _ipp_client_t *client)		/* I - Client */
{
  int			valid = 1;	/* Valid attributes? */
  ipp_op_t		op = ippGetOperation(client->request);
					/* IPP operation */
  const char		*op_name = ippOpString(op);
					/* IPP operation name */
  ipp_attribute_t	*attr,		/* Current attribute */
			*supported;	/* xxx-supported attribute */
  const char		*compression = NULL,
					/* compression value */
			*format = NULL;	/* document-format value */


 /*
  * Check operation attributes...
  */

  if ((attr = ippFindAttribute(client->request, "compression", IPP_TAG_ZERO)) != NULL)
  {
   /*
    * If compression is specified, only accept a supported value in a Print-Job
    * or Send-Document request...
    */

    compression = ippGetString(attr, 0, NULL);
    supported   = ippFindAttribute(client->printer->attrs,
                                   "compression-supported", IPP_TAG_KEYWORD);

    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_KEYWORD ||
        ippGetGroupTag(attr) != IPP_TAG_OPERATION ||
        (op != IPP_OP_PRINT_JOB && op != IPP_OP_SEND_DOCUMENT &&
         op != IPP_OP_VALIDATE_JOB) ||
        !ippContainsString(supported, compression))
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
    else
    {
      fprintf(stderr, "%s %s compression=\"%s\"\n", client->hostname, op_name, compression);

      ippAddString(client->request, IPP_TAG_JOB, IPP_TAG_KEYWORD, "compression-supplied", NULL, compression);

      if (strcmp(compression, "none"))
      {
	if (Verbosity)
	  fprintf(stderr, "Receiving job file with \"%s\" compression.\n", compression);
        httpSetField(client->http, HTTP_FIELD_CONTENT_ENCODING, compression);
      }
    }
  }

 /*
  * Is it a format we support?
  */

  if ((attr = ippFindAttribute(client->request, "document-format", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_MIMETYPE ||
        ippGetGroupTag(attr) != IPP_TAG_OPERATION)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
    else
    {
      format = ippGetString(attr, 0, NULL);

      fprintf(stderr, "%s %s document-format=\"%s\"\n",
	      client->hostname, op_name, format);

      ippAddString(client->request, IPP_TAG_JOB, IPP_TAG_MIMETYPE, "document-format-supplied", NULL, format);
    }
  }
  else
  {
    format = ippGetString(ippFindAttribute(client->printer->attrs, "document-format-default", IPP_TAG_MIMETYPE), 0, NULL);
    if (!format)
      format = "application/octet-stream"; /* Should never happen */

    attr = ippAddString(client->request, IPP_TAG_OPERATION, IPP_TAG_MIMETYPE, "document-format", NULL, format);
  }

  if (!strcmp(format, "application/octet-stream") && (ippGetOperation(client->request) == IPP_OP_PRINT_JOB || ippGetOperation(client->request) == IPP_OP_SEND_DOCUMENT))
  {
   /*
    * Auto-type the file using the first 8 bytes of the file...
    */

    unsigned char	header[8];	/* First 8 bytes of file */

    memset(header, 0, sizeof(header));
    httpPeek(client->http, (char *)header, sizeof(header));

    if (!memcmp(header, "%PDF", 4))
      format = "application/pdf";
    else if (!memcmp(header, "%!", 2))
      format = "application/postscript";
    else if (!memcmp(header, "\377\330\377", 3) && header[3] >= 0xe0 && header[3] <= 0xef)
      format = "image/jpeg";
    else if (!memcmp(header, "\211PNG", 4))
      format = "image/png";
    else if (!memcmp(header, "RAS2", 4))
      format = "image/pwg-raster";
    else if (!memcmp(header, "UNIRAST", 8))
      format = "image/urf";
    else
      format = NULL;

    if (format)
    {
      fprintf(stderr, "%s %s Auto-typed document-format=\"%s\"\n",
	      client->hostname, op_name, format);

      ippAddString(client->request, IPP_TAG_JOB, IPP_TAG_MIMETYPE, "document-format-detected", NULL, format);
    }
  }

  if (op != IPP_OP_CREATE_JOB && (supported = ippFindAttribute(client->printer->attrs, "document-format-supported", IPP_TAG_MIMETYPE)) != NULL && !ippContainsString(supported, format))
  {
    respond_unsupported(client, attr);
    valid = 0;
  }

 /*
  * document-name
  */

  if ((attr = ippFindAttribute(client->request, "document-name", IPP_TAG_NAME)) != NULL)
    ippAddString(client->request, IPP_TAG_JOB, IPP_TAG_NAME, "document-name-supplied", NULL, ippGetString(attr, 0, NULL));

  return (valid);
}


/*
 * 'valid_job_attributes()' - Determine whether the job attributes are valid.
 *
 * When one or more job attributes are invalid, this function adds a suitable
 * response and attributes to the unsupported group.
 */

static int				/* O - 1 if valid, 0 if not */
valid_job_attributes(
    _ipp_client_t *client)		/* I - Client */
{
  int			i,		/* Looping var */
			valid = 1;	/* Valid attributes? */
  ipp_attribute_t	*attr,		/* Current attribute */
			*supported;	/* xxx-supported attribute */


 /*
  * Check operation attributes...
  */

  valid = valid_doc_attributes(client);

 /*
  * Check the various job template attributes...
  */

  if ((attr = ippFindAttribute(client->request, "copies", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_INTEGER ||
        ippGetInteger(attr, 0) < 1 || ippGetInteger(attr, 0) > 999)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "ipp-attribute-fidelity", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_BOOLEAN)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "job-hold-until", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 ||
        (ippGetValueTag(attr) != IPP_TAG_NAME &&
	 ippGetValueTag(attr) != IPP_TAG_NAMELANG &&
	 ippGetValueTag(attr) != IPP_TAG_KEYWORD) ||
	strcmp(ippGetString(attr, 0, NULL), "no-hold"))
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "job-impressions", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_INTEGER || ippGetInteger(attr, 0) < 0)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "job-name", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 ||
        (ippGetValueTag(attr) != IPP_TAG_NAME &&
	 ippGetValueTag(attr) != IPP_TAG_NAMELANG))
    {
      respond_unsupported(client, attr);
      valid = 0;
    }

    ippSetGroupTag(client->request, &attr, IPP_TAG_JOB);
  }
  else
    ippAddString(client->request, IPP_TAG_JOB, IPP_TAG_NAME, "job-name", NULL, "Untitled");

  if ((attr = ippFindAttribute(client->request, "job-priority", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_INTEGER ||
        ippGetInteger(attr, 0) < 1 || ippGetInteger(attr, 0) > 100)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "job-sheets", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 ||
        (ippGetValueTag(attr) != IPP_TAG_NAME &&
	 ippGetValueTag(attr) != IPP_TAG_NAMELANG &&
	 ippGetValueTag(attr) != IPP_TAG_KEYWORD) ||
	strcmp(ippGetString(attr, 0, NULL), "none"))
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "media", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 ||
        (ippGetValueTag(attr) != IPP_TAG_NAME &&
	 ippGetValueTag(attr) != IPP_TAG_NAMELANG &&
	 ippGetValueTag(attr) != IPP_TAG_KEYWORD))
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
    else
    {
#if 0 /* TODO: Validate media */
      for (i = 0;
           i < (int)(sizeof(media_supported) / sizeof(media_supported[0]));
	   i ++)
        if (!strcmp(ippGetString(attr, 0, NULL), media_supported[i]))
	  break;

      if (i >= (int)(sizeof(media_supported) / sizeof(media_supported[0])))
      {
	respond_unsupported(client, attr);
	valid = 0;
      }
#endif /* 0 */
    }
  }

  if ((attr = ippFindAttribute(client->request, "media-col", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 ||
        ippGetValueTag(attr) != IPP_TAG_BEGIN_COLLECTION)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
    /* TODO: check for valid media-col */
  }

  if ((attr = ippFindAttribute(client->request, "multiple-document-handling", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_KEYWORD ||
        (strcmp(ippGetString(attr, 0, NULL),
		"separate-documents-uncollated-copies") &&
	 strcmp(ippGetString(attr, 0, NULL),
		"separate-documents-collated-copies")))
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "orientation-requested", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_ENUM ||
        ippGetInteger(attr, 0) < IPP_ORIENT_PORTRAIT ||
        ippGetInteger(attr, 0) > IPP_ORIENT_REVERSE_PORTRAIT)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "page-ranges", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetValueTag(attr) != IPP_TAG_RANGE)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "print-quality", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_ENUM ||
        ippGetInteger(attr, 0) < IPP_QUALITY_DRAFT ||
        ippGetInteger(attr, 0) > IPP_QUALITY_HIGH)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "printer-resolution", IPP_TAG_ZERO)) != NULL)
  {
    supported = ippFindAttribute(client->printer->dev_attrs, "printer-resolution-supported", IPP_TAG_RESOLUTION);

    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_RESOLUTION ||
        !supported)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
    else
    {
      int	count,			/* Number of supported values */
		xdpi,			/* Horizontal resolution for job template attribute */
		ydpi,			/* Vertical resolution for job template attribute */
		sydpi;			/* Vertical resolution for supported value */
      ipp_res_t	units,			/* Units for job template attribute */
		sunits;			/* Units for supported value */

      xdpi  = ippGetResolution(attr, 0, &ydpi, &units);
      count = ippGetCount(supported);

      for (i = 0; i < count; i ++)
      {
        if (xdpi == ippGetResolution(supported, i, &sydpi, &sunits) && ydpi == sydpi && units == sunits)
          break;
      }

      if (i >= count)
      {
	respond_unsupported(client, attr);
	valid = 0;
      }
    }
  }

  if ((attr = ippFindAttribute(client->request, "sides", IPP_TAG_ZERO)) != NULL)
  {
    const char *sides = ippGetString(attr, 0, NULL);
					/* "sides" value... */

    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_KEYWORD)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
    else if ((supported = ippFindAttribute(client->printer->dev_attrs, "sides-supported", IPP_TAG_KEYWORD)) != NULL)
    {
      if (!ippContainsString(supported, sides))
      {
	respond_unsupported(client, attr);
	valid = 0;
      }
    }
    else if (strcmp(sides, "one-sided"))
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  return (valid);
}


/*
 * End of "$Id: ippinfra.c 12237 2014-11-03 13:07:32Z msweet $".
 */
