/*
 * "$Id: banners.h,v 1.2 2001/01/22 15:03:58 mike Exp $"
 *
 *   Banner definitions for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2001 by Easy Software Products.
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
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 */

/*
 * Banner information structure...
 */

typedef struct
{
  char		name[256];	/* Name of banner */
  char		filename[1024];	/* Full filename */
  mime_type_t	*filetype;	/* Filetype for banner */
} banner_t;


/*
 * Globals...
 */

VAR int		NumBanners	VALUE(0);
				/* Number of banner files available */
VAR banner_t	*Banners	VALUE(NULL);
				/* Available banner files */


/*
 * Prototypes...
 */

extern void	AddBanner(const char *name, const char *filename);
extern banner_t	*FindBanner(const char *name);
extern void	LoadBanners(const char *d);


/*
 * End of "$Id: banners.h,v 1.2 2001/01/22 15:03:58 mike Exp $".
 */
