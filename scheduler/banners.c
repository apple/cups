/*
 * "$Id: banners.c 6649 2007-07-11 21:46:42Z mike $"
 *
 *   Banner routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007-2008 by Apple Inc.
 *   Copyright 1997-2006 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
 *   cupsdFindBanner()  - Find a named banner.
 *   cupsdLoadBanners() - Load all available banner files...
 *   add_banner()       - Add a banner to the array.
 *   compare_banners()  - Compare two banners.
 *   free_banners()     - Free all banners.
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"
#include <cups/dir.h>


/*
 * Local functions...
 */

static void	add_banner(const char *name, const char *filename);
static int	compare_banners(const cupsd_banner_t *b0,
		                const cupsd_banner_t *b1);
static void	free_banners(void);


/*
 * 'cupsdFindBanner()' - Find a named banner.
 */

cupsd_banner_t *			/* O - Pointer to banner or NULL */
cupsdFindBanner(const char *name)	/* I - Name of banner */
{
  cupsd_banner_t	key;		/* Search key */


  key.name = (char *)name;

  return ((cupsd_banner_t *)cupsArrayFind(Banners, &key));
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

  free_banners();

 /*
  * Try opening the banner directory...
  */

  if ((dir = cupsDirOpen(d)) == NULL)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR, "cupsdLoadBanners: Unable to open banner directory \"%s\": %s",
               d, strerror(errno));
    return;
  }

 /*
  * Read entries, skipping directories and backup files.
  */

  Banners = cupsArrayNew((cups_array_func_t)compare_banners, NULL);

  while ((dent = cupsDirRead(dir)) != NULL)
  {
   /*
    * Check the file to make sure it isn't a directory or a backup
    * file of some sort...
    */

    snprintf(filename, sizeof(filename), "%s/%s", d, dent->filename);

    if (S_ISDIR(dent->fileinfo.st_mode))
      continue;

    if (dent->filename[0] == '~' ||
        dent->filename[strlen(dent->filename) - 1] == '~')
      continue;

    if ((ext = strrchr(dent->filename, '.')) != NULL)
      if (!strcmp(ext, ".bck") ||
          !strcmp(ext, ".bak") ||
	  !strcmp(ext, ".sav"))
	continue;

   /*
    * Must be a valid file; add it!
    */

    add_banner(dent->filename, filename);
  }

 /*
  * Close the directory...
  */

  cupsDirClose(dir);
}


/*
 * 'add_banner()' - Add a banner to the array.
 */

static void
add_banner(const char *name,		/* I - Name of banner */
           const char *filename)	/* I - Filename for banner */
{
  mime_type_t		*filetype;	/* Filetype */
  cupsd_banner_t	*temp;		/* New banner data */


 /*
  * See what the filetype is...
  */

  if ((filetype = mimeFileType(MimeDatabase, filename, NULL, NULL)) == NULL)
  {
    cupsdLogMessage(CUPSD_LOG_WARN,
                    "add_banner: Banner \"%s\" (\"%s\") is of an unknown file "
		    "type - skipping!", name, filename);
    return;
  }

 /*
  * Allocate memory...
  */

  if ((temp = calloc(1, sizeof(cupsd_banner_t))) == NULL)
  {
    cupsdLogMessage(CUPSD_LOG_WARN,
                    "add_banner: Unable to allocate memory for banner \"%s\" - "
		    "skipping!", name);
    return;
  }

 /*
  * Copy the new banner data over...
  */

  if ((temp->name = strdup(name)) == NULL)
  {
    cupsdLogMessage(CUPSD_LOG_WARN,
                    "add_banner: Unable to allocate memory for banner \"%s\" - "
		    "skipping!", name);
    free(temp);
    return;
  }

  temp->filetype = filetype;

  cupsArrayAdd(Banners, temp);
}


/*
 * 'compare_banners()' - Compare two banners.
 */

static int				/* O - -1 if name0 < name1, etc. */
compare_banners(
    const cupsd_banner_t *b0,		/* I - First banner */
    const cupsd_banner_t *b1)		/* I - Second banner */
{
  return (strcasecmp(b0->name, b1->name));
}


/*
 * 'free_banners()' - Free all banners.
 */

static void
free_banners(void)
{
  cupsd_banner_t	*temp;		/* Current banner */


  for (temp = (cupsd_banner_t *)cupsArrayFirst(Banners);
       temp;
       temp = (cupsd_banner_t *)cupsArrayNext(Banners))
  {
    free(temp->name);
    free(temp);
  }

  cupsArrayDelete(Banners);
  Banners = NULL;
}


/*
 * End of "$Id: banners.c 6649 2007-07-11 21:46:42Z mike $".
 */
