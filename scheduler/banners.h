/*
 * "$Id$"
 *
 *   Banner definitions for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2006 by Easy Software Products.
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

/*
 * Banner information structure...
 */

typedef struct
{
  char		name[256];		/* Name of banner */
  mime_type_t	*filetype;		/* Filetype for banner */
} cupsd_banner_t;


/*
 * Globals...
 */

VAR cups_array_t	*Banners	VALUE(NULL);
					/* Available banner files */


/*
 * Prototypes...
 */

extern void		cupsdAddBanner(const char *name, const char *filename);
extern cupsd_banner_t	*cupsdFindBanner(const char *name);
extern void		cupsdFreeBanners(void);
extern void		cupsdLoadBanners(const char *d);


/*
 * End of "$Id$".
 */
