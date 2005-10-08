/*
 * "$Id$"
 *
 *   Global variable definitions for the Common UNIX Printing System (CUPS).
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
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 */

#ifndef _CUPS_GLOBALS_H_
#  define _CUPS_GLOBALS_H_

/*
 * Include necessary headers...
 */

#  include "string.h"
#  include "cups.h"
#  include "language.h"
#  include "normalize.h"
#  include "transcode.h"

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
 * To make libcups thread safe, define thread safe globals (aka thread-
 * specific data) for the static variables used in the library.
 */

typedef struct _cups_globals_s		/**** CUPS global state data ****/
{
  /* http.c */
  char			http_date[256];	/* Date+time buffer */

  /* http-addr.c */
  unsigned		ip_addr;	/* Packed IPv4 address */
  char			*ip_ptrs[2];	/* Pointer to packed address */
  struct hostent	hostent;	/* Host entry for IP address */
#  ifdef HAVE_GETADDRINFO
  char			hostname[1024];	/* Hostname */
#  endif /* HAVE_GETADDRINFO */

  /* ipp.c */
  ipp_uchar_t		ipp_date[11];	/* RFC-1903 date/time data */

  /* ipp-support.c */
  int			ipp_port;	/* IPP port number */
  char			ipp_unknown[255];
					/* Unknown error statuses */

  /* normalize.c */
  _cups_norm_map_t	*normmap_cache;	/* Normalize Map cache */
  _cups_fold_map_t	*foldmap_cache;	/* Case Fold cache */
  _cups_prop_map_t	*propmap_cache;	/* Char Prop Map Cache */
  _cups_comb_map_t	*combmap_cache;	/* Comb Class Map Cache */
  _cups_break_map_t	*breakmap_cache;/* Line Break Map Cache */

  /* language.c */
  cups_lang_t		*lang_cache;	/* Language string cache */
#  ifdef __APPLE__
#    ifdef HAVE_CF_LOCALE_ID
  char			language[32];	/* Cached language */
#    else
  const char		*language;	/* Cached language */
#    endif /* HAVE_CF_LOCALE_ID */
#  endif /* __APPLE__ */

  /* ppd.c */
  ppd_status_t		ppd_status;	/* Status of last ppdOpen*() */
  int			ppd_line;	/* Current line number */
  ppd_conform_t		ppd_conform;	/* Level of conformance required */

  /* tempfile.c */
  char			tempfile[1024];	/* cupsTempFd/File buffer */

  /* transcode.c */
  _cups_cmap_t		*cmap_cache;	/* SBCS Charmap Cache */
  _cups_vmap_t		*vmap_cache;	/* VBCS Charmap Cache */

  /* usersys.c */
  http_encryption_t	encryption;	/* Encryption setting */
  char			user[65],	/* User name */
			server[256],	/* Server address */
			servername[256];/* Server hostname */
  const char		*(*password_cb)(const char *);
					/* Password callback */

  /* util.c */
  http_t		*http;		/* Current server connection */
  ipp_status_t		last_error;	/* Last IPP error */
  char			def_printer[256];
					/* Default printer */
  char			ppd_filename[HTTP_MAX_URI];
					/* PPD filename */
} _cups_globals_t;


/*
 * Prototypes...
 */

extern const char	*_cupsGetPassword(const char *prompt);
extern _cups_globals_t	*_cupsGlobals(void);


/*
 * C++ magic...
 */

#  ifdef __cplusplus
}
#  endif /* __cplusplus */
#endif /* !_CUPS_GLOBALS_H_ */

/*
 * End of "$Id$".
 */
