/*
 * "$Id: banners.c,v 1.5.2.5 2003/03/30 21:49:15 mike Exp $"
 *
 *   Banner routines for the Common UNIX Printing System (CUPS).
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
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 *   AddBanner()   - Add a banner to the array.
 *   FindBanner()  - Find a named banner.
 *   LoadBanners() - Load all available banner files...
 *   compare()     - Compare two banners.
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"


/*
 * Local functions...
 */

static int	compare(const banner_t *b0, const banner_t *b1);


/*
 * 'AddBanner()' - Add a banner to the array.
 */

void
AddBanner(const char *name,	/* I - Name of banner */
          const char *filename)	/* I - Filename for banner */
{
  mime_type_t	*filetype;	/* Filetype */
  banner_t	*temp;		/* New banner data */


 /*
  * See what the filetype is...
  */

  if ((filetype = mimeFileType(MimeDatabase, filename, NULL)) == NULL)
  {
    LogMessage(L_WARN, "AddBanner: Banner \"%s\" is of an unknown file type - skipping!",
               name);
    return;
  }

 /*
  * Allocate memory...
  */

  if (NumBanners == 0)
    temp = malloc(sizeof(banner_t));
  else
    temp = realloc(Banners, sizeof(banner_t) * (NumBanners + 1));

  if (temp == NULL)
  {
    LogMessage(L_ERROR, "AddBanner: Ran out of memory adding a banner!");
    return;
  }

 /*
  * Copy the new banner data over...
  */

  Banners = temp;
  temp    += NumBanners;
  NumBanners ++;

  memset(temp, 0, sizeof(banner_t));
  strlcpy(temp->name, name, sizeof(temp->name));
  temp->filetype = filetype;
}


/*
 * 'FindBanner()' - Find a named banner.
 */

banner_t *			/* O - Pointer to banner or NULL */
FindBanner(const char *name)	/* I - Name of banner */
{
  banner_t	key;		/* Search key */


  strlcpy(key.name, name, sizeof(key.name));

  return ((banner_t *)bsearch(&key, Banners, NumBanners, sizeof(banner_t),
                              (int (*)(const void *, const void *))compare));
}


/*
 * 'LoadBanners()' - Load all available banner files...
 */

void
LoadBanners(const char *d)	/* I - Directory to search */
{
  DIR		*dir;		/* Directory pointer */
  DIRENT	*dent;		/* Directory entry */
  char		filename[1024],	/* Name of banner */
		*ext;		/* Pointer to extension */
  struct stat	fileinfo;	/* File information */


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

  if ((dir = opendir(d)) == NULL)
  {
    LogMessage(L_ERROR, "LoadBanners: Unable to open banner directory \"%s\": %s",
               d, strerror(errno));
    return;
  }

 /*
  * Read entries, skipping directories and backup files.
  */

  while ((dent = readdir(dir)) != NULL)
  {
   /*
    * Check the file to make sure it isn't a directory or a backup
    * file of some sort...
    */

    snprintf(filename, sizeof(filename), "%s/%s", d, dent->d_name);

    if (stat(filename, &fileinfo))
    {
      LogMessage(L_WARN, "LoadBanners: Unable to state \"%s\" banner: %s",
                 dent->d_name, strerror(errno));
      continue;
    }

    if (S_ISDIR(fileinfo.st_mode))
      continue;

    if (dent->d_name[0] == '~')
      continue;

    if ((ext = strrchr(dent->d_name, '.')) != NULL)
      if (strcmp(ext, ".bck") == 0 ||
          strcmp(ext, ".bak") == 0 ||
	  strcmp(ext, ".sav") == 0)
	continue;

   /*
    * Must be a valid file; add it!
    */

    AddBanner(dent->d_name, filename);
  }

 /*
  * Close the directory and sort as needed...
  */

  closedir(dir);

  if (NumBanners > 1)
    qsort(Banners, NumBanners, sizeof(banner_t),
          (int (*)(const void *, const void *))compare);
}


/*
 * 'compare()' - Compare two banners.
 */

static int			/* O - -1 if name0 < name1, etc. */
compare(const banner_t *b0,	/* I - First banner */
        const banner_t *b1)	/* I - Second banner */
{
  return (strcasecmp(b0->name, b1->name));
}


/*
 * End of "$Id: banners.c,v 1.5.2.5 2003/03/30 21:49:15 mike Exp $".
 */
