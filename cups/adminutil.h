/*
 * "$Id$"
 *
 *   Administration utility API definitions for the Common UNIX Printing
 *   System (CUPS).
 *
 *   MANY OF THE FUNCTIONS IN THIS HEADER ARE PRIVATE AND SUBJECT TO
 *   CHANGE AT ANY TIME.  USE AT YOUR OWN RISK.
 *
 *   Copyright 2001-2006 by Easy Software Products.
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

#ifndef _CUPS_ADMINUTIL_H_
#  define _CUPS_ADMINUTIL_H_

/*
 * Include necessary headers...
 */

#  include "cups.h"


/*
 * C++ magic...
 */

#  ifdef __cplusplus
extern "C" {
#  endif /* __cplusplus */


/*
 * Constants...
 */

#  define CUPS_SERVER_REMOTE_PRINTERS	"_remote_printers"
#  define CUPS_SERVER_SHARE_PRINTERS	"_share_printers"
#  define CUPS_SERVER_REMOTE_ADMIN	"_remote_admin"
#  define CUPS_SERVER_USER_CANCEL_ANY	"_user_cancel_any"
#  define CUPS_SERVER_DEBUG_LOGGING	"_debug_logging"


/*
 * Functions...
 */

extern int	cupsAdminExportSamba(const char *dest, const char *ppd,
		                     const char *samba_server,
			             const char *samba_user,
				     const char *samba_password,
				     FILE *logfile);
extern char	*cupsAdminCreateWindowsPPD(http_t *http, const char *dest,
		                           char *buffer, int bufsize);

extern int	_cupsAdminGetServerSettings(http_t *http,
			                    int *num_settings,
		                            cups_option_t **settings);
extern int	_cupsAdminSetServerSettings(http_t *http,
		                            int num_settings,
		                            cups_option_t *settings);


#  ifdef __cplusplus
}
#  endif /* __cplusplus */

#endif /* !_CUPS_ADMINUTIL_H_ */

/*
 * End of "$Id$".
 */
