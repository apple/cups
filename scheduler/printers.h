/*
 * "$Id$"
 *
 *   Printer definitions for the Common UNIX Printing System (CUPS) scheduler.
 *
 *   Copyright 2007 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 */

#ifdef HAVE_DNSSD
#  include <dns_sd.h>
#endif /* HAVE_DNSSD */

/*
 * Quota data...
 */

typedef struct
{
  char		username[33];		/* User data */
  time_t	next_update;		/* Next update time */
  int		page_count,		/* Count of pages */
		k_count;		/* Count of kilobytes */
} cupsd_quota_t;


/*
 * Printer/class information structure...
 */

typedef struct cupsd_printer_s
{
  char		*uri,			/* Printer URI */
		*hostname,		/* Host printer resides on */
		*name,			/* Printer name */
		*location,		/* Location code */
		*make_model,		/* Make and model */
		*info,			/* Description */
		*op_policy,		/* Operation policy name */
		*error_policy;		/* Error policy */
  cupsd_policy_t *op_policy_ptr;	/* Pointer to operation policy */
  int		shared;			/* Shared? */
  int		accepting;		/* Accepting jobs? */
  int		in_implicit_class;	/* In an implicit class? */
  ipp_pstate_t	state;			/* Printer state */
  char		state_message[1024];	/* Printer state message */
  int		num_reasons;		/* Number of printer-state-reasons */
  char		*reasons[16];		/* printer-state-reasons strings */
  time_t	state_time;		/* Time at this state */
  char		*job_sheets[2];		/* Banners/job sheets */
  cups_ptype_t	type;			/* Printer type (color, small, etc.) */
  char		*browse_attrs;		/* Attributes sent with browse data */
  time_t	browse_expire;		/* Expiration time for printer */
  time_t	browse_time;		/* Last time update was sent/received */
  char		*device_uri;		/* Device URI */
  char		*port_monitor;		/* Port monitor */
  int		raw;			/* Raw queue? */
  int		remote;			/* Remote queue? */
  mime_type_t	*filetype,		/* Pseudo-filetype for printer */
		*prefiltertype;		/* Pseudo-filetype for pre-filters */
  cups_array_t	*filetypes;		/* Supported file types */
  void		*job;			/* Current job in queue */
  ipp_t		*attrs;			/* Attributes supported by this printer */
  int		num_printers,		/* Number of printers in class */
		last_printer;		/* Last printer job was sent to */
  struct cupsd_printer_s **printers;	/* Printers in class */
  int		quota_period,		/* Period for quotas */
		page_limit,		/* Maximum number of pages */
		k_limit;		/* Maximum number of kilobytes */
  cups_array_t	*quotas;		/* Quota records */
  int		deny_users,		/* 1 = deny, 0 = allow */
		num_users;		/* Number of allowed/denied users */
  const char	**users;		/* Allowed/denied users */
  int		num_history;		/* Number of history collections */
  ipp_t		**history;		/* History data */
  int		sequence_number;	/* Increasing sequence number */
  int		num_options;		/* Number of default options */
  cups_option_t	*options;		/* Default options */
  int		num_auth_info_required;	/* Number of required auth fields */
  const char	*auth_info_required[4];	/* Required authentication fields */
  char		*alert,			/* PSX printer-alert value */
		*alert_description;	/* PSX printer-alert-description value */
  time_t	marker_time;		/* Last time marker attributes were updated */

#ifdef __APPLE__
  char		*recoverable;		/* com.apple.print.recoverable-message */
#endif /* __APPLE__ */

#ifdef HAVE_DNSSD
  char		*reg_name,		/* Name used for service registration */
		*product,		/* PPD Product string */
		*pdl,			/* pdl value for TXT record */
		*txt_record;		/* TXT record contents */
  int		txt_len;		/* TXT record length */
  DNSServiceRef	dnssd_ipp_ref;		/* DNSServiceRegister ref for _ipp */
  int		dnssd_ipp_fd;		/* File descriptor for DNSServiceRegister reference */
#endif /* HAVE_DNSSD */
} cupsd_printer_t;


/*
 * Globals...
 */

VAR ipp_t		*CommonData	VALUE(NULL);
					/* Common printer object attrs */
VAR cups_array_t	*CommonDefaults	VALUE(NULL);
					/* Common -default option names */
VAR cups_array_t	*Printers	VALUE(NULL),
					/* Printer list */
			*ImplicitPrinters VALUE(NULL);
					/* Implicit class printers */
VAR cupsd_printer_t	*DefaultPrinter	VALUE(NULL);
					/* Default printer */
VAR char		*DefaultPolicy	VALUE(NULL);
					/* Default policy name */
VAR cupsd_policy_t	*DefaultPolicyPtr
					VALUE(NULL);
					/* Pointer to default policy */


/*
 * Prototypes...
 */

extern cupsd_printer_t	*cupsdAddPrinter(const char *name);
extern void		cupsdAddPrinterHistory(cupsd_printer_t *p);
extern void		cupsdAddPrinterUser(cupsd_printer_t *p,
			                    const char *username);
extern void		cupsdCreateCommonData(void);
extern void		cupsdDeleteAllPrinters(void);
extern void		cupsdDeletePrinter(cupsd_printer_t *p, int update);
extern cupsd_printer_t	*cupsdFindDest(const char *name);
extern cupsd_printer_t	*cupsdFindPrinter(const char *name);
extern cupsd_quota_t	*cupsdFindQuota(cupsd_printer_t *p,
			                const char *username);
extern void		cupsdFreePrinterUsers(cupsd_printer_t *p);
extern void		cupsdFreeQuotas(cupsd_printer_t *p);
extern void		cupsdLoadAllPrinters(void);
extern void		cupsdRenamePrinter(cupsd_printer_t *p,
			                   const char *name);
extern char		*cupsdSanitizeURI(const char *uri, char *buffer,
			                  int buflen);
extern void		cupsdSaveAllPrinters(void);
extern int		cupsdSetAuthInfoRequired(cupsd_printer_t *p,
			                         const char *values,
						 ipp_attribute_t *attr);
extern void		cupsdSetPrinterAttr(cupsd_printer_t *p,
			                    const char *name, char *value);
extern void		cupsdSetPrinterAttrs(cupsd_printer_t *p);
extern void		cupsdSetPrinterReasons(cupsd_printer_t *p,
			                       const char *s);
extern void		cupsdSetPrinterState(cupsd_printer_t *p, ipp_pstate_t s,
			                     int update);
#define			cupsdStartPrinter(p,u) cupsdSetPrinterState((p), IPP_PRINTER_IDLE, (u))
extern void		cupsdStopPrinter(cupsd_printer_t *p, int update);
extern void		cupsdUpdatePrinters(void);
extern cupsd_quota_t	*cupsdUpdateQuota(cupsd_printer_t *p,
			                  const char *username, int pages,
					  int k);
extern const char	*cupsdValidateDest(const char *uri,
			        	   cups_ptype_t *dtype,
					   cupsd_printer_t **printer);
extern void		cupsdWritePrintcap(void);


/*
 * End of "$Id$".
 */
