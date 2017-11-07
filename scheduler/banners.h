/*
 * Banner definitions for the CUPS scheduler.
 *
 * Copyright 2007-2010 by Apple Inc.
 * Copyright 1997-2006 by Easy Software Products.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more information.
 */

/*
 * Banner information structure...
 */

typedef struct				/**** Banner file information ****/
{
  char		*name;			/* Name of banner */
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

extern cupsd_banner_t	*cupsdFindBanner(const char *name);
extern void		cupsdLoadBanners(const char *d);
