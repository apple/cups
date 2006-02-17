/*
 * "$Id$"
 *
 *   PPD/driver support for the Common UNIX Printing System (CUPS).
 *
 *   This program handles listing and installing both static PPD files
 *   in CUPS_DATADIR/model and dynamically generated PPD files using
 *   the driver helper programs in CUPS_SERVERBIN/driver.
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
 *
 * Contents:
 *
 *   main()          - Scan for drivers and return an IPP response.
 *   add_ppd()       - Add a PPD file.
 *   cat_ppd()       - Copy a PPD file to stdout.
 *   compare_names() - Compare PPD filenames for sorting.
 *   compare_ppds()  - Compare PPD file make and model names for sorting.
 *   list_ppds()     - List PPD files.
 *   load_ppds()     - Load PPD files recursively.
 *   load_drivers()  - Load driver-generated PPD files.
 */

/*
 * Include necessary headers...
 */

#include "util.h"
#include <cups/dir.h>


/*
 * PPD information structures...
 */

typedef struct				/**** PPD record ****/
{
  time_t	mtime;			/* Modification time */
  size_t	size;			/* Size in bytes */
  char		name[512 - sizeof(time_t) - sizeof(size_t)],
					/* PPD name */
		natural_language[128],	/* Natural language(s) */
		make[128],		/* Manufacturer */
		make_and_model[128],	/* Make and model */
		device_id[128];		/* IEEE 1284 Device ID */
} ppd_rec_t;

typedef struct				/**** In-memory record ****/
{
  int		found;			/* 1 if PPD is found */
  ppd_rec_t	record;			/* PPDs.dat record */
} ppd_info_t;


/*
 * Globals...
 */

int		NumPPDs,		/* Number of PPD files */
		SortedPPDs,		/* Number of sorted PPD files */
		AllocPPDs;		/* Number of allocated entries */
ppd_info_t	*PPDs;			/* PPD file info */
int		ChangedPPD;		/* Did we change the PPD database? */


/*
 * Local functions...
 */

static ppd_info_t	*add_ppd(const char *name, const char *natural_language,
		         const char *make, const char *make_and_model,
			 const char *device_id, time_t mtime, size_t size);
static int		cat_ppd(const char *name);
static int		compare_names(const ppd_info_t *p0,
			              const ppd_info_t *p1);
static int		compare_ppds(const ppd_info_t *p0,
			             const ppd_info_t *p1);
static int		list_ppds(int request_id, int limit, const char *opt);
static int		load_drivers(void);
static int		load_ppds(const char *d, const char *p);


/*
 * 'main()' - Scan for drivers and return an IPP response.
 *
 * Usage:
 *
 *    cups-driverd request_id limit options
 */

int					/* O - Exit code */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
 /*
  * Install or list PPDs...
  */

  if (argc == 3 && !strcmp(argv[1], "cat"))
    return (cat_ppd(argv[2]));
  else if (argc == 5 && !strcmp(argv[1], "list"))
    return (list_ppds(atoi(argv[2]), atoi(argv[3]), argv[4]));
  else
  {
    fputs("Usage: cups-driverd cat ppd-name\n", stderr);
    fputs("Usage: cups-driverd list request_id limit options\n", stderr);
    return (1);
  }
}


/*
 * 'add_ppd()' - Add a PPD file.
 */

static ppd_info_t *			/* O - PPD */
add_ppd(const char *name,		/* I - PPD name */
        const char *natural_language,	/* I - Language(s) */
        const char *make,		/* I - Manufacturer */
	const char *make_and_model,	/* I - NickName */
	const char *device_id,		/* I - 1284DeviceId */
        time_t     mtime,		/* I - Modification time */
	size_t     size)		/* I - File size */
{
  ppd_info_t	*ppd;			/* PPD */


 /*
  * Add a new PPD file...
  */

  if (NumPPDs >= AllocPPDs)
  {
   /*
    * Allocate (more) memory for the PPD files...
    */

    AllocPPDs += 128;

    if (!PPDs)
      ppd = malloc(sizeof(ppd_info_t) * AllocPPDs);
    else
      ppd = realloc(PPDs, sizeof(ppd_info_t) * AllocPPDs);

    if (ppd == NULL)
    {
      fprintf(stderr, "ERROR: [cups-driverd] Ran out of memory for %d PPD files!\n",
	      AllocPPDs);
      return (NULL);
    }

    PPDs = ppd;
  }

  ppd = PPDs + NumPPDs;
  NumPPDs ++;

 /*
  * Zero-out the PPD data and copy the values over...
  */

  memset(ppd, 0, sizeof(ppd_info_t));

  ppd->found        = 1;
  ppd->record.mtime = mtime;
  ppd->record.size  = size;

  strlcpy(ppd->record.name, name, sizeof(ppd->record.name));
  strlcpy(ppd->record.natural_language, natural_language,
          sizeof(ppd->record.natural_language));
  strlcpy(ppd->record.make, make, sizeof(ppd->record.make));
  strlcpy(ppd->record.make_and_model, make_and_model,
          sizeof(ppd->record.make_and_model));
  strlcpy(ppd->record.device_id, device_id, sizeof(ppd->record.device_id));

 /*
  * Return the new PPD pointer...
  */

  return (ppd);
}


/*
 * 'cat_ppd()' - Copy a PPD file to stdout.
 */

static int				/* O - Exit code */
cat_ppd(const char *name)		/* I - PPD name */
{
  char		scheme[256],		/* Scheme from PPD name */
		*sptr;			/* Pointer into scheme */
  char		line[1024];		/* Line/filename */


 /*
  * Figure out if this is a static or dynamic PPD file...
  */

  strlcpy(scheme, name, sizeof(scheme));
  if ((sptr = strchr(scheme, ':')) != NULL)
  {
    *sptr = '\0';

    if (!strcmp(scheme, "file"))
    {
     /*
      * "file:name" == "name"...
      */

      name += 5;
      scheme[0] = '\0';
    }
  }
  else
    scheme[0] = '\0';

  if (scheme[0])
  {
   /*
    * Dynamic PPD, see if we have a driver program to support it...
    */

    const char	*serverbin;		/* CUPS_SERVERBIN env var */


    if ((serverbin = getenv("CUPS_SERVERBIN")) == NULL)
      serverbin = CUPS_SERVERBIN;

    snprintf(line, sizeof(line), "%s/driver/%s", serverbin, scheme);
    if (access(line, X_OK))
    {
     /*
      * File does not exist or is not executable...
      */

      fprintf(stderr, "ERROR: [cups-driverd] Unable to access \"%s\" - %s\n",
              line, strerror(errno));
      return (1);
    }

   /*
    * Yes, let it cat the PPD file...
    */

    if (execl(line, scheme, "cat", name, (char *)NULL))
    {
     /*
      * Unable to execute driver...
      */

      fprintf(stderr, "ERROR: [cups-driverd] Unable to execute \"%s\" - %s\n",
              line, strerror(errno));
      return (1);
    }
  }
  else
  {
   /*
    * Static PPD, see if we have a valid path and it exists...
    */

    cups_file_t	*fp;			/* PPD file */
    const char	*datadir;		/* CUPS_DATADIR env var */


    if (name[0] == '/' || strstr(name, "../") || strstr(name, "/.."))
    {
     /*
      * Bad name...
      */

      fprintf(stderr, "ERROR: [cups-driverd] Bad PPD name \"%s\"!\n", name);
      return (1);
    }

   /*
    * Try opening the file...
    */

    if ((datadir = getenv("CUPS_DATADIR")) == NULL)
      datadir = CUPS_DATADIR;

    snprintf(line, sizeof(line), "%s/model/%s", datadir, name);
    if ((fp = cupsFileOpen(line, "r")) == NULL)
    {
      fprintf(stderr, "ERROR: [cups-driverd] Unable to open \"%s\" - %s\n",
              line, strerror(errno));
      return (1);
    }

   /*
    * Now copy the file to stdout...
    */

    while (cupsFileGets(fp, line, sizeof(line)))
      puts(line);

    cupsFileClose(fp);
  }

 /*
  * Return with no errors...
  */

  return (0);
}


/*
 * 'compare_names()' - Compare PPD filenames for sorting.
 */

static int				/* O - Result of comparison */
compare_names(const ppd_info_t *p0,	/* I - First PPD file */
              const ppd_info_t *p1)	/* I - Second PPD file */
{
  return (strcasecmp(p0->record.name, p1->record.name));
}


/*
 * 'compare_ppds()' - Compare PPD file make and model names for sorting.
 */

static int				/* O - Result of comparison */
compare_ppds(const ppd_info_t *p0,	/* I - First PPD file */
             const ppd_info_t *p1)	/* I - Second PPD file */
{
  int	diff;				/* Difference between strings */


 /*
  * First compare manufacturers...
  */

  if ((diff = strcasecmp(p0->record.make, p1->record.make)) != 0)
    return (diff);
  else if ((diff = cupsdCompareNames(p0->record.make_and_model,
                                     p1->record.make_and_model)) != 0)
    return (diff);
  else
    return (strcasecmp(p0->record.natural_language,
                       p1->record.natural_language));
}


/*
 * 'list_ppds()' - List PPD files.
 */

static int				/* O - Exit code */
list_ppds(int        request_id,	/* I - Request ID */
          int        limit,		/* I - Limit */
	  const char *opt)		/* I - Option argument */
{
  int		i;			/* Looping var */
  int		count;			/* Number of PPDs to send */
  ppd_info_t	*ppd;			/* Current PPD file */
  cups_file_t	*fp;			/* ppds.dat file */
  struct stat	fileinfo;		/* ppds.dat information */
  char		filename[1024],		/* ppds.dat filename */
		model[1024];		/* Model directory */
  const char	*cups_cachedir;		/* CUPS_CACHEDIR environment variable */
  const char	*cups_datadir;		/* CUPS_DATADIR environment variable */
  int		num_options;		/* Number of options */
  cups_option_t	*options;		/* Options */
  const char	*requested,		/* requested-attributes option */
		*make;			/* ppd-make option */
  int		send_natural_language,	/* Send ppd-natural-language attribute? */
		send_make,		/* Send ppd-make attribute? */
		send_make_and_model,	/* Send ppd-make-and-model attribute? */
		send_name,		/* Send ppd-name attribute? */
		send_device_id;		/* Send ppd-device-id attribute? */


  fprintf(stderr, "DEBUG2: [cups-driverd] list_ppds(request_id=%d, limit=%d, opt=\"%s\"\n",
          request_id, limit, opt);

 /*
  * See if we a PPD database file...
  */

  NumPPDs    = 0;
  AllocPPDs  = 0;
  PPDs       = (ppd_info_t *)NULL;
  ChangedPPD = 0;

  if ((cups_cachedir = getenv("CUPS_CACHEDIR")) == NULL)
    cups_cachedir = CUPS_CACHEDIR;

  snprintf(filename, sizeof(filename), "%s/ppds.dat", cups_cachedir);
  if (!stat(filename, &fileinfo) &&
      (fileinfo.st_size % sizeof(ppd_rec_t)) == 0 &&
      (NumPPDs = fileinfo.st_size / sizeof(ppd_rec_t)) > 0)
  {
   /*
    * We have a ppds.dat file, so read it!
    */

    AllocPPDs = NumPPDs;

    if ((PPDs = malloc(sizeof(ppd_info_t) * NumPPDs)) == NULL)
    {
      fprintf(stderr,
              "ERROR: [cups-driverd] Unable to allocate memory for %d "
	      "PPD files!\n", NumPPDs);
      NumPPDs   = 0;
      AllocPPDs = 0;
    }
    else if ((fp = cupsFileOpen(filename, "r")) != NULL)
    {
      for (i = NumPPDs, ppd = PPDs; i > 0; i --, ppd ++)
      {
        cupsFileRead(fp, (char *)&(ppd->record), sizeof(ppd_rec_t));
	ppd->found = 0;
      }

      cupsFileClose(fp);

      fprintf(stderr, "INFO: [cups-driverd] Read \"%s\", %d PPDs...\n",
              filename, NumPPDs);
    }
    else
    {
      fprintf(stderr, "ERROR: [cups-driverd] Unable to read \"%s\" - %s\n", filename,
              strerror(errno));
      NumPPDs = 0;
    }
  }

 /*
  * Load all PPDs in the specified directory and below...
  */

  SortedPPDs = NumPPDs;

  if ((cups_datadir = getenv("CUPS_DATADIR")) == NULL)
    cups_datadir = CUPS_DATADIR;

  snprintf(model, sizeof(model), "%s/model", cups_datadir);
  load_ppds(model, "");

 /*
  * Cull PPD files that are no longer present...
  */

  for (i = NumPPDs, ppd = PPDs; i > 0; i --, ppd ++)
    if (!ppd->found)
    {
     /*
      * Remove this PPD file from the list...
      */

      if (i > 1)
        memmove(ppd, ppd + 1, (i - 1) * sizeof(ppd_info_t));

      NumPPDs --;
      ppd --;
    }

 /*
  * Sort the PPDs by name...
  */

  if (NumPPDs > 1)
    qsort(PPDs, NumPPDs, sizeof(ppd_info_t),
          (int (*)(const void *, const void *))compare_names);

 /*
  * Write the new ppds.dat file...
  */

  if (ChangedPPD)
  {
    if ((fp = cupsFileOpen(filename, "w")) != NULL)
    {
      for (i = NumPPDs, ppd = PPDs; i > 0; i --, ppd ++)
	cupsFileWrite(fp, (char *)&(ppd->record), sizeof(ppd_rec_t));

      cupsFileClose(fp);

      fprintf(stderr, "INFO: [cups-driverd] Wrote \"%s\", %d PPDs...\n",
              filename, NumPPDs);
    }
    else
      fprintf(stderr, "ERROR: [cups-driverd] Unable to write \"%s\" - %s\n",
              filename, strerror(errno));
  }
  else
    fputs("INFO: [cups-driverd] No new or changed PPDs...\n", stderr);

 /*
  * Scan for dynamic PPD files...
  */

  load_drivers();

 /*
  * Add the raw driver...
  */

  add_ppd("raw", "en", "Raw", "Raw Queue", "", 0, 0);

 /*
  * Sort the PPDs by make and model...
  */

  if (NumPPDs > 1)
    qsort(PPDs, NumPPDs, sizeof(ppd_info_t),
          (int (*)(const void *, const void *))compare_ppds);

 /*
  * Send IPP attributes...
  */

  num_options = cupsParseOptions(opt, 0, &options);
  requested   = cupsGetOption("requested-attributes", num_options, options);
  make        = cupsGetOption("ppd-make", num_options, options);

  fprintf(stderr, "DEBUG: [cups-driverd] requested=\"%s\"\n",
          requested ? requested : "(nil)");

  if (!requested || strstr(requested, "all"))
  {
    send_name             = 1;
    send_make             = 1;
    send_make_and_model   = 1;
    send_natural_language = 1;
    send_device_id        = 1;
  }
  else
  {
    send_name             = strstr(requested, "ppd-name") != NULL;
    send_make             = strstr(requested, "ppd-make,") != NULL ||
                            strstr(requested, ",ppd-make") != NULL ||
                            !strcmp(requested, "ppd-make");
    send_make_and_model   = strstr(requested, "ppd-make-and-model") != NULL;
    send_natural_language = strstr(requested, "ppd-natural-language") != NULL;
    send_device_id        = strstr(requested, "ppd-device-id") != NULL;
  }

  puts("Content-Type: application/ipp\n");

  cupsdSendIPPHeader(IPP_OK, request_id);
  cupsdSendIPPGroup(IPP_TAG_OPERATION);
  cupsdSendIPPString(IPP_TAG_CHARSET, "attributes-charset", "utf-8");
  cupsdSendIPPString(IPP_TAG_LANGUAGE, "attributes-natural-language", "en-US");

  if (limit <= 0 || limit > NumPPDs)
    count = NumPPDs;
  else
    count = limit;

  for (i = NumPPDs, ppd = PPDs; count > 0 && i > 0; i --, ppd ++)
    if (!make || !strcasecmp(ppd->record.make, make))
    {
     /*
      * Send this PPD...
      */

      fprintf(stderr, "DEBUG: [cups-driverd] Sending %s (%s)...\n",
              ppd->record.name, ppd->record.make_and_model);

      count --;

      cupsdSendIPPGroup(IPP_TAG_PRINTER);

      if (send_name)
        cupsdSendIPPString(IPP_TAG_NAME, "ppd-name", ppd->record.name);

      if (send_natural_language)
        cupsdSendIPPString(IPP_TAG_LANGUAGE, "ppd-natural-language",
	                   ppd->record.natural_language);

      if (send_make)
        cupsdSendIPPString(IPP_TAG_TEXT, "ppd-make", ppd->record.make);

      if (send_make_and_model)
        cupsdSendIPPString(IPP_TAG_TEXT, "ppd-make-and-model",
	                   ppd->record.make_and_model);

      if (send_device_id)
        cupsdSendIPPString(IPP_TAG_TEXT, "ppd-device-id",
	                   ppd->record.device_id);

     /*
      * If we have only requested the ppd-make attribute, then skip
      * the remaining PPDs with this make...
      */

      if (requested && !strcmp(requested, "ppd-make"))
      {
        const char	*this_make;	/* This ppd-make */


        for (this_make = ppd->record.make, i --, ppd ++; i > 0; i --, ppd ++)
	  if (strcasecmp(this_make, ppd->record.make))
	    break;

        i ++;
	ppd --;
      }
    }

  cupsdSendIPPTrailer();

  return (0);
}


/*
 * 'load_ppds()' - Load PPD files recursively.
 */

static int				/* O - 1 on success, 0 on failure */
load_ppds(const char *d,		/* I - Actual directory */
          const char *p)		/* I - Virtual path in name */
{
  int		i;			/* Looping var */
  cups_file_t	*fp;			/* Pointer to file */
  cups_dir_t	*dir;			/* Directory pointer */
  cups_dentry_t	*dent;			/* Directory entry */
  char		filename[1024],		/* Name of PPD or directory */
		line[256],		/* Line from backend */
		*ptr,			/* Pointer into name */
		name[128],		/* Name of PPD file */
		language[64],		/* PPD language version */
		country[64],		/* Country code */
		manufacturer[256],	/* Manufacturer */
		make_model[256],	/* Make and Model */
		model_name[256],	/* ModelName */
		nick_name[256],		/* NickName */
		device_id[256];		/* 1284DeviceId */
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
    { "danish",		"da" },
    { "dutch",		"nl" },
    { "english",	"en" },
    { "finnish",	"fi" },
    { "french",		"fr" },
    { "german",		"de" },
    { "greek",		"el" },
    { "italian",	"it" },
    { "japanese",	"jp" },
    { "norwegian",	"no" },
    { "polish",		"pl" },
    { "portuguese",	"pt" },
    { "russian",	"ru" },
    { "slovak",		"sk" },
    { "spanish",	"es" },
    { "swedish",	"sv" },
    { "turkish",	"tr" }
  };


  if ((dir = cupsDirOpen(d)) == NULL)
  {
    fprintf(stderr, "ERROR: [cups-driverd] Unable to open PPD directory \"%s\": %s\n",
            d, strerror(errno));
    return (0);
  }

  while ((dent = cupsDirRead(dir)) != NULL)
  {
   /*
    * Skip files/directories starting with "."...
    */

    if (dent->filename[0] == '.')
      continue;

   /*
    * See if this is a file...
    */

    snprintf(filename, sizeof(filename), "%s/%s", d, dent->filename);

    if (p[0])
      snprintf(name, sizeof(name), "%s/%s", p, dent->filename);
    else
      strlcpy(name, dent->filename, sizeof(name));

    if (S_ISDIR(dent->fileinfo.st_mode))
    {
     /*
      * Do subdirectory...
      */

      if (!load_ppds(filename, name))
      {
        cupsDirClose(dir);
        return (1);
      }

      continue;
    }

   /*
    * See if this file has been scanned before...
    */

    if (SortedPPDs > 0)
    {
      strcpy(key.record.name, name);

      ppd = bsearch(&key, PPDs, SortedPPDs, sizeof(ppd_info_t),
                    (int (*)(const void *, const void *))compare_names);

      if (ppd &&
          ppd->record.size == dent->fileinfo.st_size &&
	  ppd->record.mtime == dent->fileinfo.st_mtime)
      {
        ppd->found = 1;
        continue;
      }
    }
    else
      ppd = NULL;

   /*
    * No, file is new/changed, so re-scan it...
    */

    if ((fp = cupsFileOpen(filename, "r")) == NULL)
      continue;

   /*
    * Now see if this is a PPD file...
    */

    line[0] = '\0';
    cupsFileGets(fp, line, sizeof(line));

    if (strncmp(line, "*PPD-Adobe:", 11))
    {
     /*
      * Nope, close the file and continue...
      */

      cupsFileClose(fp);

      continue;
    }

   /*
    * Now read until we get the NickName field...
    */

    model_name[0]   = '\0';
    nick_name[0]    = '\0';
    manufacturer[0] = '\0';
    device_id[0]    = '\0';
    strcpy(language, "en");

    while (cupsFileGets(fp, line, sizeof(line)) != NULL)
    {
      if (!strncmp(line, "*Manufacturer:", 14))
	sscanf(line, "%*[^\"]\"%255[^\"]", manufacturer);
      else if (!strncmp(line, "*ModelName:", 11))
	sscanf(line, "%*[^\"]\"%127[^\"]", model_name);
      else if (!strncmp(line, "*LanguageVersion:", 17))
	sscanf(line, "%*[^:]:%63s", language);
      else if (!strncmp(line, "*NickName:", 10))
	sscanf(line, "%*[^\"]\"%255[^\"]", nick_name);
      else if (!strncmp(line, "*1284DeviceId:", 14))
	sscanf(line, "%*[^\"]\"%255[^\"]", device_id);
      else if (!strncmp(line, "*OpenUI", 7))
      {
       /*
        * Stop early if we have a NickName or ModelName attributes
	* before the first OpenUI...
	*/

        if (model_name[0] || nick_name[0])
	  break;
      }
    }

   /*
    * Close the file...
    */

    cupsFileClose(fp);

   /*
    * See if we got all of the required info...
    */

    if (nick_name[0])
      strcpy(make_model, nick_name);
    else
      strcpy(make_model, model_name);

    while (isspace(make_model[0] & 255))
      _cups_strcpy(make_model, make_model + 1);

    if (!make_model[0])
      continue;	/* Nope... */

   /*
    * See if we got a manufacturer...
    */

    while (isspace(manufacturer[0] & 255))
      _cups_strcpy(manufacturer, manufacturer + 1);

    if (!manufacturer[0] || !strcmp(manufacturer, "ESP"))
    {
     /*
      * Nope, copy the first part of the make and model then...
      */

      strlcpy(manufacturer, make_model, sizeof(manufacturer));

     /*
      * Truncate at the first space, dash, or slash, or make the
      * manufacturer "Other"...
      */

      for (ptr = manufacturer; *ptr; ptr ++)
	if (*ptr == ' ' || *ptr == '-' || *ptr == '/')
	  break;

      if (*ptr && ptr > manufacturer)
	*ptr = '\0';
      else if (!strncasecmp(manufacturer, "agfa", 4))
	strcpy(manufacturer, "AGFA");
      else if (!strncasecmp(manufacturer, "herk", 4) ||
               !strncasecmp(manufacturer, "linotype", 8))
	strcpy(manufacturer, "LHAG");
      else
	strcpy(manufacturer, "Other");

     /*
      * Hack for various vendors...
      */

      if (!strcasecmp(manufacturer, "XPrint"))
	strcpy(manufacturer, "Xerox");
      else if (!strcasecmp(manufacturer, "Eastman"))
	strcpy(manufacturer, "Kodak");
      else if (!strcasecmp(manufacturer, "laserwriter"))
	strcpy(manufacturer, "Apple");
      else if (!strcasecmp(manufacturer, "colorpoint"))
	strcpy(manufacturer, "Seiko");
      else if (!strcasecmp(manufacturer, "fiery"))
	strcpy(manufacturer, "EFI");
      else if (!strcasecmp(manufacturer, "ps") ||
               !strcasecmp(manufacturer, "colorpass"))
	strcpy(manufacturer, "Canon");
      else if (!strncasecmp(manufacturer, "primera", 7))
	strcpy(manufacturer, "Fargo");
      else if (!strcasecmp(manufacturer, "designjet"))
	strcpy(manufacturer, "HP");
    }
    else if (!strncasecmp(manufacturer, "LHAG", 4) ||
             !strncasecmp(manufacturer, "linotype", 8))
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
      _cups_strcpy(country + 1, ptr);
    }
    else
    {
     /*
      * No country suffix...
      */

      country[0] = '\0';
    }

    for (i = 0; i < (int)(sizeof(languages) / sizeof(languages[0])); i ++)
      if (!strcasecmp(languages[i].version, language))
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

    new_ppd = !ppd;

    if (new_ppd)
    {
     /*
      * Add new PPD file...
      */

      fprintf(stderr, "DEBUG: [cups-driverd] Adding ppd \"%s\"...\n", name);

      if (!add_ppd(name, language, manufacturer, make_model, device_id,
                   dent->fileinfo.st_mtime, dent->fileinfo.st_size))
      {
        cupsDirClose(dir);
      	return (0);
      }
    }
    else
    {
     /*
      * Update existing record...
      */

      fprintf(stderr, "DEBUG: [cups-driverd] Updating ppd \"%s\"...\n", name);

      memset(ppd, 0, sizeof(ppd_info_t));

      ppd->found        = 1;
      ppd->record.mtime = dent->fileinfo.st_mtime;
      ppd->record.size  = dent->fileinfo.st_size;

      strlcpy(ppd->record.name, name, sizeof(ppd->record.name));
      strlcpy(ppd->record.make, manufacturer, sizeof(ppd->record.make));
      strlcpy(ppd->record.make_and_model, make_model,
              sizeof(ppd->record.make_and_model));
      strlcpy(ppd->record.natural_language, language,
              sizeof(ppd->record.natural_language));
      strlcpy(ppd->record.device_id, device_id, sizeof(ppd->record.device_id));
    }

    ChangedPPD = 1;
  }

  cupsDirClose(dir);

  return (1);
}


/*
 * 'load_drivers()' - Load driver-generated PPD files.
 */

static int				/* O - 1 on success, 0 on failure */
load_drivers(void)
{
  const char	*server_bin;		/* CUPS_SERVERBIN environment variable */
  char		drivers[1024];		/* Location of driver programs */
  FILE		*fp;			/* Pipe to driver program */
  cups_dir_t	*dir;			/* Directory pointer */
  cups_dentry_t *dent;			/* Directory entry */
  char		filename[1024],		/* Name of driver */
		line[2048],		/* Line from driver */
		name[512],		/* ppd-name */
		natural_language[128],	/* ppd-natural-language */
		make[128],		/* ppd-make */
		make_and_model[256],	/* ppd-make-and-model */
		device_id[256];		/* ppd-device-id */


 /*
  * Try opening the driver directory...
  */

  if ((server_bin = getenv("CUPS_SERVERBIN")) == NULL)
    server_bin = CUPS_SERVERBIN;

  snprintf(drivers, sizeof(drivers), "%s/driver", server_bin);

  if ((dir = cupsDirOpen(drivers)) == NULL)
  {
    fprintf(stderr, "ERROR: [cups-driverd] Unable to open driver directory "
                    "\"%s\": %s\n",
            drivers, strerror(errno));
    return (0);
  }

 /*
  * Loop through all of the device drivers...
  */

  while ((dent = cupsDirRead(dir)) != NULL)
  {
   /*
    * Only look at executable files...
    */

    if (!(dent->fileinfo.st_mode & 0111) || !S_ISREG(dent->fileinfo.st_mode))
      continue;

   /*
    * Run the driver with no arguments and collect the output...
    */

    snprintf(filename, sizeof(filename), "%s/%s list", drivers, dent->filename);
    if ((fp = popen(filename, "r")) != NULL)
    {
      while (fgets(line, sizeof(line), fp) != NULL)
      {
       /*
        * Each line is of the form:
	*
	*   \"ppd-name\" ppd-natural-language "ppd-make" "ppd-make-and-model"
	*/

        device_id[0] = '\0';

        if (sscanf(line, "\"%511[^\"]\"%127s%*[ \t]\"%127[^\"]\""
	                 "%*[ \t]\"%256[^\"]\"%*[ \t]\"%256[^\"]\"",
	           name, natural_language, make, make_and_model,
		   device_id) < 4)
        {
	 /*
	  * Bad format; strip trailing newline and write an error message.
	  */

          if (line[strlen(line) - 1] == '\n')
	    line[strlen(line) - 1] = '\0';

	  fprintf(stderr, "ERROR: [cups-driverd] Bad line from \"%s\": %s\n",
	          dent->filename, line);
	  break;
        }
	else
	{
	 /*
	  * Add the device to the array of available devices...
	  */

          if (!add_ppd(name, natural_language, make, make_and_model, device_id,
	               0, 0))
	  {
            cupsDirClose(dir);
	    return (0);
	  }

          fprintf(stderr, "DEBUG: [cups-driverd] Added dynamic PPD \"%s\"...\n",
	          name);
	}
      }

      pclose(fp);
    }
    else
      fprintf(stderr, "WARNING: [cups-driverd] Unable to execute \"%s\": %s\n",
              filename, strerror(errno));
  }

  cupsDirClose(dir);

  return (1);
}


/*
 * End of "$Id$".
 */
