/*
 * "$Id$"
 *
 *   Backend definitions for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2005 by Easy Software Products.
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

#ifndef _CUPS_BACKEND_H_
#  define _CUPS_BACKEND_H_


/*
 * Constants...
 */

typedef enum cups_backend_e		/**** Backend exit codes ****/
{
  CUPS_BACKEND_OK = 0,			/* Job completed successfully */
  CUPS_BACKEND_FAILED = 1,		/* Job failed, use error-policy */
  CUPS_BACKEND_CANCEL = 2,		/* Job failed, cancel job */
  CUPS_BACKEND_HOLD = 3,		/* Job failed, hold job */
  CUPS_BACKEND_STOP = 4,		/* Job failed, stop queue */
  CUPS_BACKEND_AUTH_REQUIRED = 5	/* Job failed, authentication required */
} cups_backend_t;


#endif /* !_CUPS_BACKEND_H_ */

/*
 * End of "$Id$".
 */
