/*
 * "$Id$"
 *
 *   MD5 MacOS X compatibility header for the Common UNIX Printing
 *   System (CUPS).
 *
 *   This file just defines aliases to the (private) CUPS MD5 functions.
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

void md5_init(_cups_md5_state_t *pms)
     { _cupsMD5Init(pms); }
void md5_append(_cups_md5_state_t *pms, const unsigned char *data, int nbytes)
     { _cupsMD5Append(pms, data, nbytes); }
void md5_finish(_cups_md5_state_t *pms, unsigned char digest[16])
     { _cupsMD5Finish(pms, digest); }

/*
 * End of "$Id$".
 */
