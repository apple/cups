/*
 * "$Id: debug.h,v 1.4.2.5 2004/06/29 03:46:29 mike Exp $"
 *
 *   Debugging macros for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2003 by Easy Software Products.
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
 *       Hollywood, Maryland 20636-3142 USA
 *
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 */

#ifndef _IPP_DEBUG_H_
#  define _IPP_DEBUG_H_

/*
 * Include necessary headers...
 */

#  include <stdio.h>

/*
 * The debug macros are used if you compile with DEBUG defined.
 *
 * Usage:
 *
 *   DEBUG_puts("string")
 *   DEBUG_printf(("format string", arg, arg, ...));
 *
 * Note the extra parenthesis around the DEBUG_printf macro...
 */

#  ifdef DEBUG
#    define DEBUG_puts(x) puts(x)
#    define DEBUG_printf(x) printf x
#  else
#    define DEBUG_puts(x)
#    define DEBUG_printf(x)
#  endif /* DEBUG */

#endif /* !_IPP_DEBUG_H_ */

/*
 * End of "$Id: debug.h,v 1.4.2.5 2004/06/29 03:46:29 mike Exp $".
 */
