/*
 * "$Id: ppds.c,v 1.1 2000/01/30 13:11:08 mike Exp $"
 *
 *   PPD scanning routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2000 by Easy Software Products.
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
 *   LoadPPDs()  - Load PPD files from the specified directory...
 *   load_ppds() - Load PPD files recursively.
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"
#include <ctype.h>

#ifdef HAVE_LIBZ
#  include <zlib.h>
#endif /* HAVE_LIBZ */

#if HAVE_DIRENT_H
#  include <dirent.h>
typedef struct dirent DIRENT;
#  define NAMLEN(dirent) strlen((dirent)->d_name)
#else
#  if HAVE_SYS_NDIR_H
#    include <sys/ndir.h>
#  endif
#  if HAVE_SYS_DIR_H
#    include <sys/dir.h>
#  endif
#  if HAVE_NDIR_H
#    include <ndir.h>
#  endif
typedef struct direct DIRENT;
#  define NAMLEN(dirent) (dirent)->d_namlen
#endif


/*
 * Local functions...
 */

static void	load_ppds(const char *d, const char *p);


/*
 * 'LoadPPDs()' - Load PPD files from the specified directory...
 */

void
LoadPPDs(const char *d)		/* I - Directory to scan... */
{
  PPDs = ippNew();

  load_ppds(d, "");
}


/*
 * 'load_ppds()' - Load PPD files recursively.
 */

static void
load_ppds(const char *d,		/* I - Actual directory */
          const char *p)		/* I - Virtual path in name */
{
#ifdef HAVE_LIBZ
  gzFile	fp;			/* Pointer to file */
#else
  FILE		*fp;			/* Pointer to file */
#endif /* HAVE_LIBZ */
  DIR		*dir;			/* Directory pointer */
  DIRENT	*dent;			/* Directory entry */
  struct stat	fileinfo;		/* File information */
  char		filename[1024],		/* Name of backend */
		line[1024],		/* Line from backend */
		*ptr,			/* Pointer into name */
		name[128],		/* Name of PPD file */
		language[64],		/* Device class */
		manufacturer[1024],	/* Manufacturer */
		make_model[256];	/* Make and model */


  if ((dir = opendir(d)) == NULL)
  {
    LogMessage(L_ERROR, "LoadPPDs: Unable to open PPD directory \"%s\": %s",
               d, strerror(errno));
    return;
  }

  while ((dent = readdir(dir)) != NULL)
  {
   /*
    * Skip "." and ".."...
    */

    if (dent->d_name[0] == '.')
      continue;

   /*
    * See if this is a file...
    */

    snprintf(filename, sizeof(filename), "%s/%s", d, dent->d_name);

    if (p[0])
      snprintf(name, sizeof(name), "%s/%s", p, dent->d_name);
    else
      strcpy(name, dent->d_name);

    if (stat(filename, &fileinfo))
      continue;

    if (S_ISDIR(fileinfo.st_mode))
    {
     /*
      * Do subdirectory...
      */

      load_ppds(filename, name);
      continue;
    }

#ifdef HAVE_LIBZ
    if ((fp = gzopen(filename, "rb")) == NULL)
#else
    if ((fp = fopen(filename, "rb")) == NULL)
#endif /* HAVE_LIBZ */
      continue;

   /*
    * Yup, now see if this is a PPD file...
    */

    line[0] = '\0';
#ifdef HAVE_LIBZ
    gzgets(fp, line, sizeof(line));
#else
    fgets(line, sizeof(line), fp);
#endif /* HAVE_LIBZ */

    if (strncmp(line, "*PPD-Adobe:", 11) != 0)
    {
      // Nope, close the file and continue...
#ifdef HAVE_LIBZ
      gzclose(fp);
#else
      fclose(fp);
#endif /* HAVE_LIBZ */

      continue;
    }

    // Now read until we get the NickName field...
    make_model[0]   = '\0';
    manufacturer[0] = '\0';
    strcpy(language, "en");

#ifdef HAVE_LIBZ
    while (gzgets(fp, line, sizeof(line)) != NULL)
#else
    while (fgets(line, sizeof(line), fp) != NULL)
#endif /* HAVE_LIBZ */
    {
      if (strncmp(line, "*Manufacturer:", 14) == 0)
	sscanf(line, "%*[^\"]\"%255[^\"]", manufacturer);
      else if (strncmp(line, "*ModelName:", 11) == 0)
	sscanf(line, "%*[^\"]\"%127[^\"]", make_model);
      else if (strncmp(line, "*LanguageVersion:", 17) == 0)
	sscanf(line, "%*[^\"]\"%63[^\"]", language);
      else if (strncmp(line, "*NickName:", 10) == 0)
      {
	sscanf(line, "%*[^\"]\"%255[^\"]", make_model);
	break;
      }
    }

   /*
    * Close the file...
    */

#ifdef HAVE_LIBZ
    gzclose(fp);
#else
    fclose(fp);
#endif /* HAVE_LIBZ */

   /*
    * See if we got all of the required info...
    */

    while (isspace(make_model[0]))
      strcpy(make_model, make_model + 1);

    if (!make_model[0])
      continue;	/* Nope... */

   /*
    * See if we got a manufacturer...
    */

    while (isspace(manufacturer[0]))
      strcpy(manufacturer, manufacturer + 1);

    if (manufacturer[0] || strcmp(manufacturer, "ESP") == 0)
    {
     /*
      * Nope, copy the first part of the make and model then...
      */

      strncpy(manufacturer, make_model, sizeof(manufacturer) - 1);

     /*
      * Truncate at the first space, dash, or slash, or make the
      * manufacturer "Other"...
      */

      for (ptr = manufacturer; *ptr; ptr ++)
	if (*ptr == ' ' || *ptr == '-' || *ptr == '/')
	  break;

      if (*ptr && ptr > manufacturer)
	*ptr = '\0';
      else if (strncasecmp(manufacturer, "agfa", 4) == 0)
	strcpy(manufacturer, "AGFA");
      else if (strncasecmp(manufacturer, "ps-ipu", 6) == 0)
	strcpy(manufacturer, "Canon");
      else if (strncasecmp(manufacturer, "herk", 4) == 0)
	strcpy(manufacturer, "Linotype");
      else
	strcpy(manufacturer, "Other");

      // Hack for various vendors...
      if (strcasecmp(manufacturer, "XPrint") == 0)
	strcpy(manufacturer, "Xerox");
      else if (strcasecmp(manufacturer, "Eastman") == 0)
	strcpy(manufacturer, "Kodak");
      else if (strcasecmp(manufacturer, "laserwriter") == 0)
	strcpy(manufacturer, "Apple");
      else if (strcasecmp(manufacturer, "colorpoint") == 0)
	strcpy(manufacturer, "Seiko");
      else if (strcasecmp(manufacturer, "fiery") == 0)
	strcpy(manufacturer, "EFI");
      else if (strncasecmp(manufacturer, "primera", 7) == 0)
	strcpy(manufacturer, "Fargo");
    }

   /*
    * Fix the language as needed...
    */

    if (strcasecmp(language, "german") == 0)
      strcpy(language, "de");
    else if (strcasecmp(language, "spanish") == 0)
      strcpy(language, "es");
    else if (strlen(language) > 2)
    {
     /*
      * en, fr, it, etc.
      */

      language[0] = tolower(language[0]);
      language[1] = tolower(language[1]);
      language[2] = '\0';
    }

   /*
    * Add the PPD file...
    */

    ippAddString(PPDs, IPP_TAG_PRINTER, IPP_TAG_NAME,
                 "ppd-name", NULL, name);
    ippAddString(PPDs, IPP_TAG_PRINTER, IPP_TAG_TEXT,
                 "ppd-make", NULL, manufacturer);
    ippAddString(PPDs, IPP_TAG_PRINTER, IPP_TAG_TEXT,
                 "ppd-make-and-model", NULL, make_model);
    ippAddString(PPDs, IPP_TAG_PRINTER, IPP_TAG_LANGUAGE,
                 "ppd-natural-language", NULL, language);
    ippAddSeparator(PPDs);

    LogMessage(L_DEBUG, "LoadPPDs: Adding ppd \"%s\"...", name);
  }

  closedir(dir);
}


/*
 * End of "$Id: ppds.c,v 1.1 2000/01/30 13:11:08 mike Exp $".
 */
