/*
 * "$Id: ppds.c,v 1.14.2.2 2001/12/26 16:52:55 mike Exp $"
 *
 *   PPD scanning routines for the Common UNIX Printing System (CUPS).
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


/*
 * PPD information structure...
 */

typedef struct
{
  char	ppd_make[128],			/* Manufacturer */
	ppd_make_and_model[256],	/* Make and model */
	ppd_name[256],			/* PPD filename */
	ppd_natural_language[16];	/* Natural language */
} ppd_info_t;


/*
 * Local globals...
 */

static int		num_ppds,	/* Number of PPD files */
			alloc_ppds;	/* Number of allocated entries */
static ppd_info_t	*ppds;		/* PPD file info */


/*
 * Local functions...
 */

static int	check_ppds(const char *d, time_t mtime, int *count);
static int	compare_ppds(const ppd_info_t *p0, const ppd_info_t *p1);
static void	load_ppds(const char *d, const char *p);


/*
 * 'LoadPPDs()' - Load PPD files from the specified directory...
 */

void
LoadPPDs(const char *d)		/* I - Directory to scan... */
{
  int		i;		/* Looping var */
  int		count;		/* Number of PPD files seen */
  ppd_info_t	*ppd;		/* Current PPD file */
  FILE		*fp;		/* ppds.dat file */
  struct stat	fileinfo;	/* ppds.dat information */
  char		filename[1024];	/* ppds.dat filename */


 /*
  * See if we need to reload the PPD files...
  */

  snprintf(filename, sizeof(filename), "%s/ppds.dat", ServerRoot);
  if (stat(filename, &fileinfo))
    i = 1;
  else
  {
    count = 0;
    i     = check_ppds(d, fileinfo.st_mtime, &count);

    if (fileinfo.st_size != (count * sizeof(ppd_info_t)))
      i = 1;
  }

  if (i)
  {
   /*
    * Load all PPDs in the specified directory and below...
    */

    num_ppds   = 0;
    alloc_ppds = 0;
    ppds       = (ppd_info_t *)0;

    load_ppds(d, "");

   /*
    * Sort the PPDs...
    */

    if (num_ppds > 1)
      qsort(ppds, num_ppds, sizeof(ppd_info_t),
            (int (*)(const void *, const void *))compare_ppds);

   /*
    * Write the new ppds.dat file...
    */

    if ((fp = fopen(filename, "wb")) != NULL)
    {
      fwrite(ppds, num_ppds, sizeof(ppd_info_t), fp);
      fclose(fp);
      LogMessage(L_INFO, "LoadPPDs: Wrote %s (%d PPDs)...", filename, num_ppds);
    }
    else
      LogMessage(L_ERROR, "LoadPPDs: Unable to write %s...", filename);
  }
  else if ((num_ppds = fileinfo.st_size / sizeof(ppd_info_t)) > 0)
  {
   /*
    * Load the ppds.dat file instead...
    */

    alloc_ppds = num_ppds;

    if ((ppds = malloc(sizeof(ppd_info_t) * num_ppds)) == NULL)
    {
      LogMessage(L_ERROR, "LoadPPDs: Unable to allocate memory for %d PPD files!",
                 num_ppds);
      num_ppds   = 0;
      alloc_ppds = 0;
    }
    else if ((fp = fopen(filename, "rb")) != NULL)
    {
      fread(ppds, num_ppds, sizeof(ppd_info_t), fp);
      fclose(fp);
      LogMessage(L_INFO, "LoadPPDs: Read %s...", filename);
    }
    else
    {
      LogMessage(L_ERROR, "LoadPPDs: Unable to read %s...", filename);
      num_ppds = 0;
    }
  }

 /*
  * Create the list of PPDs...
  */

  PPDs = ippNew();

 /*
  * First the raw driver...
  */

  ippAddString(PPDs, IPP_TAG_PRINTER, IPP_TAG_NAME,
               "ppd-name", NULL, "raw");
  ippAddString(PPDs, IPP_TAG_PRINTER, IPP_TAG_TEXT,
               "ppd-make", NULL, "Raw");
  ippAddString(PPDs, IPP_TAG_PRINTER, IPP_TAG_TEXT,
               "ppd-make-and-model", NULL, "Raw Queue");
  ippAddString(PPDs, IPP_TAG_PRINTER, IPP_TAG_LANGUAGE,
               "ppd-natural-language", NULL, "en");

 /*
  * Then the PPD files...
  */

  for (i = num_ppds, ppd = ppds; i > 0; i --, ppd ++)
  {
    ippAddSeparator(PPDs);

    ippAddString(PPDs, IPP_TAG_PRINTER, IPP_TAG_NAME,
                 "ppd-name", NULL, ppd->ppd_name);
    ippAddString(PPDs, IPP_TAG_PRINTER, IPP_TAG_TEXT,
                 "ppd-make", NULL, ppd->ppd_make);
    ippAddString(PPDs, IPP_TAG_PRINTER, IPP_TAG_TEXT,
                 "ppd-make-and-model", NULL, ppd->ppd_make_and_model);
    ippAddString(PPDs, IPP_TAG_PRINTER, IPP_TAG_LANGUAGE,
                 "ppd-natural-language", NULL, ppd->ppd_natural_language);
  }

 /*
  * Free the memory used...
  */

  if (alloc_ppds)
  {
    free(ppds);
    alloc_ppds = 0;
  }
}


/*
 * 'check_ppds()' - Check to see if we need to regenerate the PPD file
 *                  list...
 */

static int			/* O - 1 if reload needed, 0 otherwise */
check_ppds(const char *d,	/* I - Directory to scan */
           time_t     mtime,	/* I - Modification time of ppds.dat */
           int        *count)	/* IO - Number of PPD files seen */
{
  DIR		*dir;		/* Directory pointer */
  DIRENT	*dent;		/* Directory entry */
  struct stat	fileinfo;	/* File information */
  char		filename[1024];	/* Name of file */


  if ((dir = opendir(d)) == NULL)
  {
    LogMessage(L_ERROR, "LoadPPDs: Unable to open PPD directory \"%s\": %s",
               d, strerror(errno));
    return (1);
  }

  while ((dent = readdir(dir)) != NULL)
  {
   /*
    * Skip "." and ".."...
    */

    if (dent->d_name[0] == '.')
      continue;

   /*
    * Check the modification time of the file or directory...
    */

    snprintf(filename, sizeof(filename), "%s/%s", d, dent->d_name);

    if (stat(filename, &fileinfo))
      continue;

    if (fileinfo.st_mtime >= mtime)
    {
      closedir(dir);
      return (1);
    }

    if (S_ISDIR(fileinfo.st_mode))
    {
     /*
      * Do subdirectory...
      */

      if (check_ppds(filename, mtime, count))
      {
        closedir(dir);
        return (1);
      }
    }
    else
      (*count) ++;
  }

  closedir(dir);

  return (0);
}


/*
 * 'compare_ppds()' - Compare PPD file make and model names for sorting.
 */

static int				/* O - Result of comparison */
compare_ppds(const ppd_info_t *p0,	/* I - First PPD file */
             const ppd_info_t *p1)	/* I - Second PPD file */
{
  const char	*s,			/* First name */
		*t;			/* Second name */
  int		diff,			/* Difference between digits */
		digits;			/* Number of digits */


 /*
  * First compare manufacturers...
  */

  if ((diff = strcasecmp(p0->ppd_make, p1->ppd_make)) != 0)
    return (diff);

 /* 
  * Then compare names...
  */

  s = p0->ppd_make_and_model;
  t = p1->ppd_make_and_model;

 /*
  * Loop through both nicknames, returning only when a difference is
  * seen.  Also, compare whole numbers rather than just characters, too!
  */

  while (*s && *t)
  {
    if (isdigit(*s) && isdigit(*t))
    {
     /*
      * Got a number; start by skipping leading 0's...
      */

      while (*s == '0')
        s ++;
      while (*t == '0')
        t ++;

     /*
      * Skip equal digits...
      */

      while (isdigit(*s) && *s == *t)
      {
        s ++;
	t ++;
      }

     /*
      * Bounce out if *s and *t aren't both digits...
      */

      if (isdigit(*s) && !isdigit(*t))
        return (1);
      else if (!isdigit(*s) && isdigit(*t))
        return (-1);
      else if (!isdigit(*s) || !isdigit(*t))
        continue;     

      if (*s < *t)
        diff = -1;
      else
        diff = 1;

     /*
      * Figure out how many more digits there are...
      */

      digits = 0;
      s ++;
      t ++;

      while (isdigit(*s))
      {
        digits ++;
	s ++;
      }

      while (isdigit(*t))
      {
        digits --;
	t ++;
      }

     /*
      * Return if the number or value of the digits is different...
      */

      if (digits < 0)
        return (-1);
      else if (digits > 0)
        return (1);
      else if (diff)
        return (diff);
    }
    else if (tolower(*s) < tolower(*t))
      return (-1);
    else if (tolower(*s) > tolower(*t))
      return (1);
    else
    {
      s ++;
      t ++;
    }
  }

 /*
  * Return the results of the final comparison...
  */

  if (*s)
    return (1);
  else if (*t)
    return (-1);
  else
    return (strcasecmp(p0->ppd_natural_language, p1->ppd_natural_language));
}


/*
 * 'load_ppds()' - Load PPD files recursively.
 */

static void
load_ppds(const char *d,		/* I - Actual directory */
          const char *p)		/* I - Virtual path in name */
{
  int		i;			/* Looping var */
  buf_t		fp;			/* Pointer to file */
  DIR		*dir;			/* Directory pointer */
  DIRENT	*dent;			/* Directory entry */
  struct stat	fileinfo;		/* File information */
  char		filename[1024],		/* Name of PPD or directory */
		line[1024],		/* Line from backend */
		*ptr,			/* Pointer into name */
		name[128],		/* Name of PPD file */
		language[64],		/* PPD language version */
		country[64],		/* Country code */
		manufacturer[256],	/* Manufacturer */
		make_model[256],	/* Make and Model */
		model_name[256],	/* ModelName */
		nick_name[256];		/* NickName */
  ppd_info_t	*ppd,			/* New PPD file */
		key;			/* Search key */
  int		new_ppd;		/* Is this a new PPD? */
  struct				/* LanguageVersion translation table */
  {
    const char	*version,		/* LanguageVersion string */
		*language;		/* Language code */
  }		languages[] =
  {
    { "chinese",	"cn" },
    { "english",	"en" },
    { "french",		"fr" },
    { "german",		"de" },
    { "danish",		"da" },
    { "finnish",	"fi" },
    { "italian",	"it" },
    { "dutch",		"du" },
    { "japanese",	"jp" },
    { "norwegian",	"no" },
    { "polish",		"pl" },
    { "portugese",	"pt" },
    { "russian",	"ru" },
    { "swedish",	"sv" },
    { "turkish",	"tr" }
  };


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
    {
      strncpy(name, dent->d_name, sizeof(name) - 1);
      name[sizeof(name) - 1] = '\0';
    }

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
     /*
      * Nope, close the file and continue...
      */

#ifdef HAVE_LIBZ
      gzclose(fp);
#else
      fclose(fp);
#endif /* HAVE_LIBZ */

      continue;
    }

   /*
    * Now read until we get the NickName field...
    */

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
	sscanf(line, "%*[^:]:%63s", language);
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

    if (!manufacturer[0] || strcmp(manufacturer, "ESP") == 0)
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
      else if (strncasecmp(manufacturer, "herk", 4) == 0 ||
               strncasecmp(manufacturer, "linotype", 8) == 0)
	strcpy(manufacturer, "LHAG");
      else
	strcpy(manufacturer, "Other");

     /*
      * Hack for various vendors...
      */

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
      else if (strcasecmp(manufacturer, "ps") == 0 ||
               strcasecmp(manufacturer, "colorpass") == 0)
	strcpy(manufacturer, "Canon");
      else if (strncasecmp(manufacturer, "primera", 7) == 0)
	strcpy(manufacturer, "Fargo");
      else if (strcasecmp(manufacturer, "designjet") == 0)
	strcpy(manufacturer, "HP");
    }
    else if (strncasecmp(manufacturer, "LHAG", 4) == 0 ||
             strncasecmp(manufacturer, "linotype", 8) == 0)
      strcpy(manufacturer, "LHAG");

   /*
    * Fix the language as needed...
    */

    if ((ptr = strchr(language, '-')) != NULL)
      *ptr++ = '\0';
    else if ((ptr = strchr(language, '_')) != NULL)
      *ptr++ = '\0';

    if (ptr)
    {
     /*
      * Setup the country suffix...
      */

      country[0] = '_';
      strcpy(country + 1, ptr);
    }
    else
    {
     /*
      * No country suffix...
      */

      country[0] = '\0';
    }

    for (i = 0; i < (int)(sizeof(languages) / sizeof(languages[0])); i ++)
      if (strcasecmp(languages[i].version, language) == 0)
        break;

    if (i < (int)(sizeof(languages) / sizeof(languages[0])))
    {
     /*
      * Found a known language...
      */

      snprintf(language, sizeof(language), "%s%s", languages[i].language, 
               country);
    }
    else
    {
     /*
      * Unknown language; use "xx"...
      */

      strcpy(language, "xx");
    }

   /*
    * Add the PPD file...
    */

    if (num_ppds >= alloc_ppds)
    {
     /*
      * Allocate (more) memory for the PPD files...
      */

      if (alloc_ppds == 0)
        ppd = malloc(sizeof(ppd_info_t) * 32);
      else
        ppd = realloc(ppds, sizeof(ppd_info_t) * (alloc_ppds + 32));

      if (ppd == NULL)
      {
        LogMessage(L_ERROR, "load_ppds: Ran out of memory for %d PPD files!",
	           alloc_ppds + 32);
        closedir(dir);
	return;
      }

      ppds = ppd;
      alloc_ppds += 32;
    }

    ppd = ppds + num_ppds;
    num_ppds ++;

    memset(ppd, 0, sizeof(ppd_info_t));
    strncpy(ppd->ppd_name, name, sizeof(ppd->ppd_name) - 1);
    strncpy(ppd->ppd_make, manufacturer, sizeof(ppd->ppd_make) - 1);
    strncpy(ppd->ppd_make_and_model, make_model,
            sizeof(ppd->ppd_make_and_model) - 1);
    strncpy(ppd->ppd_natural_language, language,
            sizeof(ppd->ppd_natural_language) - 1);

    LogMessage(L_DEBUG, "LoadPPDs: Added ppd \"%s\"...", name);
  }

  closedir(dir);
}


/*
 * End of "$Id: ppds.c,v 1.14.2.2 2001/12/26 16:52:55 mike Exp $".
 */
