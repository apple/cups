/*
 * "$Id$"
 *
 *   Backend support definitions for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2007 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Easy Software Products and are protected by Federal
 *   copyright law.  Distribution and use rights are outlined in the file
 *   "LICENSE" which should have been included with this file.  If this
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

#ifndef _CUPS_BACKEND_PRIVATE_H_
#  define _CUPS_BACKEND_PRIVATE_H_


/*
 * Include necessary headers.
 */

#  include <cups/backend.h>
#  include <cups/sidechannel.h>
#  include <cups/cups.h>
#  include <cups/debug.h>
#  include <cups/i18n.h>
#  include <stdlib.h>
#  include <errno.h>
#  include <cups/string.h>
#  include <signal.h>


/*
 * C++ magic...
 */

#  ifdef __cplusplus
extern "C" {
#  endif /* __cplusplus */


/*
 * Prototypes...
 */

extern int	backendGetDeviceID(int fd, char *device_id, int device_id_size,
		                   char *make_model, int make_model_size,
				   const char *scheme, char *uri, int uri_size);
extern int	backendGetMakeModel(const char *device_id, char *make_model,
			            int make_model_size);
extern ssize_t	backendRunLoop(int print_fd, int device_fd, int use_bc,
		               void (*side_cb)(int print_fd, int device_fd,
			                       int use_bc));


#  ifdef __cplusplus
}
#  endif /* __cplusplus */
#endif /* !_CUPS_BACKEND_PRIVATE_H_ */


/*
 * End of "$Id$".
 */
