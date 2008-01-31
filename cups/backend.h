/*
 * "$Id$"
 *
 *   Backend definitions for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007-2008 by Apple Inc.
 *   Copyright 1997-2005 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 */

#ifndef _CUPS_BACKEND_H_
#  define _CUPS_BACKEND_H_


/*
 * Include necessary headers...
 */

#  include "versioning.h"


/*
 * Constants...
 */

enum cups_backend_e			/**** Backend exit codes ****/
{
  CUPS_BACKEND_OK = 0,			/* Job completed successfully */
  CUPS_BACKEND_FAILED = 1,		/* Job failed, use error-policy */
  CUPS_BACKEND_AUTH_REQUIRED = 2,	/* Job failed, authentication required */
  CUPS_BACKEND_HOLD = 3,		/* Job failed, hold job */
  CUPS_BACKEND_STOP = 4,		/* Job failed, stop queue */
  CUPS_BACKEND_CANCEL = 5		/* Job failed, cancel job */
};
typedef enum cups_backend_e cups_backend_t;
					/**** Backend exit codes ****/


/*
 * Prototypes...
 */

extern const char	*cupsBackendDeviceURI(char **argv) _CUPS_API_1_2;


#endif /* !_CUPS_BACKEND_H_ */

/*
 * End of "$Id$".
 */
