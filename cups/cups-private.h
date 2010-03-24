/*
 * "$Id$"
 *
 *   Private definitions for CUPS.
 *
 *   Copyright 2007-2010 by Apple Inc.
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
#  include "http-private.h"
#  include "ipp-private.h"
#  include "language-private.h"
#  include "pwg-private.h"
#  ifdef HAVE_PTHREAD_H
#    include <pthread.h>
#  endif /* HAVE_PTHREAD_H */


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

  /* util.c */
  http_t		*http;		/* Current server connection */
  ipp_status_t		last_error;	/* Last IPP error */
  char			*last_status_message;
					/* Last IPP status-message */

  char			def_printer[256];
					/* Default printer */
  char			ppd_filename[HTTP_MAX_URI];
					/* PPD filename */
} _cups_globals_t;


/*
 * Prototypes...
 */

extern http_t		*_cupsConnect(void);
extern int		_cupsGet1284Values(const char *device_id,
			                   cups_option_t **values);
extern const char	*_cupsGetPassword(const char *prompt);
extern _cups_globals_t	*_cupsGlobals(void);
extern void		_cupsSetDefaults(void);
extern void		_cupsSetError(ipp_status_t status, const char *message,
			              int localize);
extern void		_cupsSetHTTPError(http_status_t status);
extern char		*_cupsUserDefault(char *name, size_t namesize);


/*
 * C++ magic...
 */

#  ifdef __cplusplus
}
#  endif /* __cplusplus */
#endif /* !_CUPS_CUPS_PRIVATE_H_ */

/*
 * End of "$Id$".
 */
