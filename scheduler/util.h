/*
 * "$Id: util.h 177 2006-06-21 00:20:03Z jlovell $"
 *
 *   Mini-daemon utility definitions for the Common UNIX Printing System (CUPS).
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
 */

#ifndef _CUPSD_UTIL_H_
#  define _CUPSD_UTIL_H_

/*
 * Include necessary headers...
 */

#  include <cups/cups.h>
#  include <cups/file.h>
#  include <cups/string.h>
#  include <stdlib.h>
#  include <errno.h>
#  include <signal.h>
#  include <dirent.h>


/*
 * Prototypes...
 */

extern int	cupsdCompareNames(const char *s, const char *t);
extern void	cupsdSendIPPGroup(ipp_tag_t group_tag);
extern void	cupsdSendIPPHeader(ipp_status_t status_code, int request_id);
extern void	cupsdSendIPPInteger(ipp_tag_t value_tag, const char *name,
		                    int value);
extern void	cupsdSendIPPString(ipp_tag_t value_tag, const char *name,
		                   const char *value);
extern void	cupsdSendIPPTrailer(void);


#endif /* !_CUPSD_UTIL_H_ */

/*
 * End of "$Id: util.h 177 2006-06-21 00:20:03Z jlovell $".
 */
