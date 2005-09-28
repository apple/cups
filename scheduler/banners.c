/*
 * "$Id$"
 *
 *   Banner routines for the Common UNIX Printing System (CUPS).
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
 * Contents:
 *
 *   cupsdAddBanner()   - Add a banner to the array.
 *   cupsdFindBanner()  - Find a named banner.
 *   cupsdLoadBanners() - Load all available banner files...
 *   compare()     - Compare two banners.
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"
#include <cups/dir.h>


/*
 * Local functions...
 */

static int	compare(const cupsd_banner_t *b0, const cupsd_banner_t *b1);


/*
 * 'cupsdAddBanner()' - Add a banner to the array.
 */

void
cupsdAddBanner(const char *name,	/* I - Name of banner */
          const char *filename)	/* I - Filename for banner */
{
  mime_type_t	*filetype;	/* Filetype */
  cupsd_banner_t	*temp;		/* New banner data */


 /*
  * See what the filetype is...
  */

  if ((filetype = mimeFileType(MimeDatabase, filename, NULL)) == NULL)
  {
    cupsdLogMessage(L_WARN, "cupsdAddBanner: Banner \"%s\" (\"%s\") is of an unknown file type - skipping!",
               name, filename);
    return;
  }

 /*
  * Allocate memory...
  */

  if (NumBanners == 0)
    temp = malloc(sizeof(cupsd_banner_t));
  else
    temp = realloc(Banners, sizeof(cupsd_banner_t) * (NumBanners + 1));

  if (temp == NULL)
  {
    cupsdLogMessage(L_ERROR, "cupsdAddBanner: Ran out of memory adding a banner!");
    return;
  }

 /*
  * Copy the new banner data over...
  */

  Banners = temp;
  temp    += NumBanners;
  NumBanners ++;

  memset(temp, 0, sizeof(cupsd_banner_t));
  strlcpy(temp->name, name, sizeof(temp->name));
  temp->filetype = filetype;
}


/*
 * 'cupsdFindBanner()' - Find a named banner.
 */

cupsd_banner_t *			/* O - Pointer to banner or NULL */
cupsdFindBanner(const char *name)	/* I - Name of banner */
{
  cupsd_banner_t	key;		/* Search key */


  strlcpy(key.name, name, sizeof(key.name));

  return ((cupsd_banner_t *)bsearch(&key, Banners, NumBanners, sizeof(cupsd_banner_t),
                              (int (*)(const void *, const void *))compare));
}


/*
 * 'cupsdLoadBanners()' - Load all available banner files...
 */

void
cupsdLoadBanners(const char *d)		/* I - Directory to search */
{
  cups_dir_t	*dir;			/* Directory pointer */
  cups_dentry_t	*dent;			/* Directory entry */
  char		filename[1024],		/* Name of banner */
		*ext;			/* Pointer to extension */


 /*
  * Free old banner info...
  */

  if (NumBanners)
  {
    free(Banners);
    NumBanners = 0;
  }

 /*
  * Try opening the banner directory...
  */

  if ((dir = cupsDirOpen(d)) == NULL)
  {
    cupsdLogMessage(L_ERROR, "cupsdLoadBanners: Unable to open banner directory \"%s\": %s",
               d, strerror(errno));
    return;
  }

 /*
  * Read entries, skipping directories and backup files.
  */

  while ((dent = cupsDirRead(dir)) != NULL)
  {
   /*
    * Check the file to make sure it isn't a directory or a backup
    * file of some sort...
    */

    snprintf(filename, sizeof(filename), "%s/%s", d, dent->filename);

    if (S_ISDIR(dent->fileinfo.st_mode))
      continue;

    if (dent->filename[0] == '~')
      continue;

    if ((ext = strrchr(dent->filename, '.')) != NULL)
      if (!strcmp(ext, ".bck") ||
          !strcmp(ext, ".bak") ||
	  !strcmp(ext, ".sav"))
	continue;

   /*
    * Must be a valid file; add it!
    */

    cupsdAddBanner(dent->filename, filename);
  }

 /*
  * Close the directory and sort as needed...
  */

  cupsDirClose(dir);

  if (NumBanners > 1)
    qsort(Banners, NumBanners, sizeof(cupsd_banner_t),
          (int (*)(const void *, const void *))compare);
}


/*
 * 'compare()' - Compare two banners.
 */

static int			/* O - -1 if name0 < name1, etc. */
compare(const cupsd_banner_t *b0,	/* I - First banner */
        const cupsd_banner_t *b1)	/* I - Second banner */
{
  return (strcasecmp(b0->name, b1->name));
}


/*
 * End of "$Id$".
 */
