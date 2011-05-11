/*
 * "$Id: cups-private.h 9596 2011-03-11 18:26:36Z mike $"
 *
 *   Private definitions for CUPS.
 *
 *   Copyright 2007-2011 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 */

#ifndef _CUPS_CUPS_PRIVATE_H_
#  define _CUPS_CUPS_PRIVATE_H_

/*
 * Include necessary headers...
 */

#  include <cups/cups.h>
#  include "string-private.h"
#  include "debug-private.h"
#  include "ppd-private.h"
#  include "http-private.h"
#  include "ipp-private.h"
#  include "language-private.h"
#  include "pwg-private.h"
#  include "thread-private.h"
#  ifdef __APPLE__
#    include <sys/cdefs.h>
#    include <CoreFoundation/CoreFoundation.h>
#  endif /* __APPLE__ */


/*
 * C++ magic...
 */

#  ifdef __cplusplus
extern "C" {
#  endif /* __cplusplus */


/*
 * Types...
 */

typedef struct _cups_globals_s		/**** CUPS global state data ****/
{
  /* Multiple places... */
  const char		*cups_datadir,	/* CUPS_DATADIR environment var */
			*cups_serverbin,/* CUPS_SERVERBIN environment var */
			*cups_serverroot,
					/* CUPS_SERVERROOT environment var */
			*cups_statedir,	/* CUPS_STATEDIR environment var */
			*localedir;	/* LOCALDIR environment var */

  /* adminutil.c */
  time_t		cupsd_update;	/* Last time we got or set cupsd.conf */
  char			cupsd_hostname[HTTP_MAX_HOST];
					/* Hostname for connection */
  int			cupsd_num_settings;
					/* Number of server settings */
  cups_option_t		*cupsd_settings;/* Server settings */

  /* auth.c */
#  ifdef HAVE_GSSAPI
  char			gss_service_name[32];
  					/* Kerberos service name */
#  endif /* HAVE_GSSAPI */

  /* backend.c */
  char			resolved_uri[1024];
					/* Buffer for cupsBackendDeviceURI */

  /* file.c */
  cups_file_t		*stdio_files[3];/* stdin, stdout, stderr */

  /* http.c */
  char			http_date[256];	/* Date+time buffer */

  /* http-addr.c */
  unsigned		ip_addr;	/* Packed IPv4 address */
  char			*ip_ptrs[2];	/* Pointer to packed address */
  struct hostent	hostent;	/* Host entry for IP address */
#  ifdef HAVE_GETADDRINFO
  char			hostname[1024];	/* Hostname */
#  endif /* HAVE_GETADDRINFO */
  int			need_res_init;	/* Need to reinitialize resolver? */

  /* ipp.c */
  ipp_uchar_t		ipp_date[11];	/* RFC-1903 date/time data */
  _ipp_buffer_t		*ipp_buffers;	/* Buffer list */

  /* ipp-support.c */
  int			ipp_port;	/* IPP port number */
  char			ipp_unknown[255];
					/* Unknown error statuses */

  /* language.c */
  cups_lang_t		*lang_default;	/* Default language */
#  ifdef __APPLE__
  char			language[32];	/* Cached language */
#  endif /* __APPLE__ */

  /* ppd.c */
  ppd_status_t		ppd_status;	/* Status of last ppdOpen*() */
  int			ppd_line;	/* Current line number */
  ppd_conform_t		ppd_conform;	/* Level of conformance required */

  /* pwg-media.c */
  cups_array_t		*leg_size_lut,	/* Lookup table for legacy names */
			*ppd_size_lut,	/* Lookup table for PPD names */
			*pwg_size_lut;	/* Lookup table for PWG names */
  _pwg_media_t		pwg_media;	/* PWG media data for custom size */
  char			pwg_name[65];	/* PWG media name for custom size */

  /* request.c */
  http_t		*http;		/* Current server connection */
  ipp_status_t		last_error;	/* Last IPP error */
  char			*last_status_message;
					/* Last IPP status-message */

  /* snmp.c */
  char			snmp_community[255];
					/* Default SNMP community name */
  int			snmp_debug;	/* Log SNMP IO to stderr? */

  /* tempfile.c */
  char			tempfile[1024];	/* cupsTempFd/File buffer */

  /* usersys.c */
  http_encryption_t	encryption;	/* Encryption setting */
  char			user[65],	/* User name */
			server[256],	/* Server address */
			servername[256];/* Server hostname */
  cups_password_cb2_t	password_cb;	/* Password callback */
  void			*password_data;	/* Password user data */
  http_tls_credentials_t tls_credentials;
					/* Default client credentials */
  cups_client_cert_cb_t	client_cert_cb;	/* Client certificate callback */
  void			*client_cert_data;
					/* Client certificate user data */
  cups_server_cert_cb_t	server_cert_cb;	/* Server certificate callback */
  void			*server_cert_data;
					/* Server certificate user data */
  int			any_root,	/* Allow any root */
			expired_certs,	/* Allow expired certs */
			expired_root;	/* Allow expired root */

  /* util.c */
  char			def_printer[256];
					/* Default printer */
  char			ppd_filename[HTTP_MAX_URI];
					/* PPD filename */
} _cups_globals_t;


/*
 * Prototypes...
 */

#  ifdef __APPLE__
extern CFStringRef	_cupsAppleCopyDefaultPaperID(void);
extern CFStringRef	_cupsAppleCopyDefaultPrinter(void);
extern int		_cupsAppleGetUseLastPrinter(void);
extern void		_cupsAppleSetDefaultPaperID(CFStringRef name);
extern void		_cupsAppleSetDefaultPrinter(CFStringRef name);
extern void		_cupsAppleSetUseLastPrinter(int uselast);
#  endif /* __APPLE__ */

extern http_t		*_cupsConnect(void);
extern int		_cupsGet1284Values(const char *device_id,
			                   cups_option_t **values);
extern int		_cupsGetDests(http_t *http, ipp_op_t op,
			              const char *name, cups_dest_t **dests);
extern const char	*_cupsGetPassword(const char *prompt);
extern void		_cupsGlobalLock(void);
extern _cups_globals_t	*_cupsGlobals(void);
extern void		_cupsGlobalUnlock(void);
#  ifdef HAVE_GSSAPI
extern const char	*_cupsGSSServiceName(void);
#  endif /* HAVE_GSSAPI */
extern int		_cupsNextDelay(int current, int *previous);
extern void		_cupsSetDefaults(void);
extern void		_cupsSetError(ipp_status_t status, const char *message,
			              int localize);
extern void		_cupsSetHTTPError(http_status_t status);
#  ifdef HAVE_GSSAPI
extern int		_cupsSetNegotiateAuthString(http_t *http,
			                            const char *method,
						    const char *resource);
#  endif /* HAVE_GSSAPI */
extern char		*_cupsUserDefault(char *name, size_t namesize);


/*
 * C++ magic...
 */

#  ifdef __cplusplus
}
#  endif /* __cplusplus */
#endif /* !_CUPS_CUPS_PRIVATE_H_ */

/*
 * End of "$Id: cups-private.h 9596 2011-03-11 18:26:36Z mike $".
 */
