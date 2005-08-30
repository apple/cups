/*
 * "$Id$"
 *
 *   PPD/driver support for the Common UNIX Printing System (CUPS).
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
 *   LoadPPDs()      - Load PPD files from the specified directory...
 *   buf_read()      - Read a buffer of data into memory...
 *   check_ppds()    - Check to see if we need to regenerate the PPD file
 *                     list...
 *   compare_names() - Compare PPD filenames for sorting.
 *   compare_ppds()  - Compare PPD file make and model names for sorting.
 *   load_ppds()     - Load PPD files recursively.
 */

/*
 * Include necessary headers...
 */

#include "util.h"
#include <cups/dir.h>


/*
 * PPD information structures...
 */

typedef struct
{
  time_t	mtime;			/* Modification time */
  size_t	size;			/* Size in bytes */
  char		name[512 - sizeof(time_t) - sizeof(size_t)],
					/* PPD name */
		natural_language[128],	/* Natural language(s) */
		make[128],		/* Manufacturer */
		make_and_model[256];	/* Make and model */
} ppd_rec_t;

typedef struct
{
  int		found;			/* 1 if PPD is found */
  ppd_rec_t	record;			/* ppds.dat record */
} ppd_info_t;


/*
 * Local globals...
 */

static int		num_ppds,	/* Number of PPD files */
			sorted_ppds,	/* Number of sorted PPD files */
			alloc_ppds;	/* Number of allocated entries */
static ppd_info_t	*ppds;		/* PPD file info */
static int		changed_ppd;	/* Did we change the PPD database? */


/*
 * Local functions...
 */

static ppd_info_t	*add_ppd(const char *name, const char *natural_language,
			         const char *make, const char *make_and_model,
				 time_t mtime, size_t size);
static int		cat_ppd(const char *name);
static int		compare_names(const ppd_info_t *p0,
			              const ppd_info_t *p1);
static int		compare_ppds(const ppd_info_t *p0,
			             const ppd_info_t *p1);
static int		list_ppds(int request_id, int limit,
			          const char *optarg);
static void		load_ppds(const char *d, const char *p);


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
  * Check the command-line...
  */

  if (argc < 3 ||
      (!strcmp(argv[1], "cat") && argc != 3) ||
      (!strcmp(argv[1], "list") && argc != 5))
  {
    fputs("Usage: cups-driverd cat ppd-name\n", stderr);
    fputs("Usage: cups-driverd list request_id limit options\n", stderr);
    return (1);
  }

 /*
  * Install or list PPDs...
  */

  if (!strcmp(argv[1], "install"))
    return (install_ppd(argv[2]));
  else
    return (list_ppds(atoi(argv[2]), atoi(argv[3]), argv[4]));
}


/*
 * 'add_ppd()' - Add a PPD file.
 */

static ppd_info_t *			/* O - PPD */
add_ppd(const char *name,		/* I - PPD name */
        const char *natural_language,	/* I - Language(s) */
        const char *make,		/* I - Manufacturer */
	const char *make_and_model,	/* I - NickName */
        time_t     mtime,		/* I - Modification time */
	size_t     size)		/* I - File size */
{
}


/*
 * 'cat_ppd()' - Copy a PPD file to stdout.
 */

static int				/* O - Exit code */
cat_ppd(const char *name)		/* I - PPD name */
{
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
	  const char *optarg)		/* I - Option argument */
{
  const char	*server_bin;		/* CUPS_SERVERBIN environment variable */
  char		backends[1024];		/* Location of backends */
  int		count;			/* Number of devices from backend */
  int		compat;			/* Compatibility device? */
  FILE		*fp;			/* Pipe to device backend */
  cups_dir_t	*dir;			/* Directory pointer */
  cups_dentry_t *dent;			/* Directory entry */
  char		filename[1024],		/* Name of backend */
		line[2048],		/* Line from backend */
		dclass[64],		/* Device class */
		uri[1024],		/* Device URI */
		info[128],		/* Device info */
		make_model[256];	/* Make and model */
  int		num_options;		/* Number of options */
  cups_option_t	*options;		/* Options */
  const char	*requested;		/* requested-attributes option */
  int		send_natural_language,	/* Send ppd-natural-language attribute? */
		send_make,		/* Send ppd-make attribute? */
		send_make_and_model,	/* Send ppd-make-and-model attribute? */
		send_name;		/* Send ppd-name attribute? */
  dev_info_t	*dev;			/* Current device */
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;		/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */
  int		i;			/* Looping var */
  ppd_info_t	*ppd;			/* Current PPD file */
  cups_file_t	*fp;			/* ppds.dat file */
  struct stat	fileinfo;		/* ppds.dat information */
  char		filename[1024];		/* ppds.dat filename */


 /*
  * See if we a PPD database file...
  */

  num_ppds    = 0;
  alloc_ppds  = 0;
  ppds        = (ppd_info_t *)0;
  changed_ppd = 0;

  snprintf(filename, sizeof(filename), "%s/ppds.dat", ServerRoot);
  if (!stat(filename, &fileinfo) &&
      (num_ppds = fileinfo.st_size / sizeof(ppd_rec_t)) > 0)
  {
   /*
    * We have a ppds.dat file, so read it!
    */

    alloc_ppds = num_ppds;

    if ((ppds = malloc(sizeof(ppd_info_t) * num_ppds)) == NULL)
    {
      LogMessage(L_ERROR, "LoadPPDs: Unable to allocate memory for %d PPD files!",
                 num_ppds);
      num_ppds   = 0;
      alloc_ppds = 0;
    }
    else if ((fp = cupsFileOpen(filename, "rb")) != NULL)
    {
      for (i = num_ppds, ppd = ppds; i > 0; i --, ppd ++)
      {
        cupsFileRead(fp, (char *)&(ppd->record), sizeof(ppd_rec_t));
	ppd->found = 0;
      }

      cupsFileClose(fp);

      LogMessage(L_INFO, "LoadPPDs: Read \"%s\", %d PPDs...", filename,
                 num_ppds);

     /*
      * Sort the PPDs by name...
      */

      if (num_ppds > 1)
      {
	qsort(ppds, num_ppds, sizeof(ppd_info_t),
              (int (*)(const void *, const void *))compare_names);
      }
    }
    else
    {
      LogMessage(L_ERROR, "LoadPPDs: Unable to read \"%s\" - %s", filename,
                 strerror(errno));
      num_ppds = 0;
    }
  }

 /*
  * Load all PPDs in the specified directory and below...
  */

  sorted_ppds = num_ppds;

  load_ppds(d, "");

 /*
  * Cull PPD files that are no longer present...
  */

  for (i = num_ppds, ppd = ppds; i > 0; i --, ppd ++)
    if (!ppd->found)
    {
     /*
      * Remove this PPD file from the list...
      */

      if (i > 1)
        memmove(ppd, ppd + 1, (i - 1) * sizeof(ppd_info_t));

      num_ppds --;
      ppd --;
    }

 /*
  * Sort the PPDs by make and model...
  */

  if (num_ppds > 1)
    qsort(ppds, num_ppds, sizeof(ppd_info_t),
          (int (*)(const void *, const void *))compare_ppds);

 /*
  * Write the new ppds.dat file...
  */

  if (changed_ppd)
  {
    if ((fp = cupsFileOpen(filename, "wb")) != NULL)
    {
      for (i = num_ppds, ppd = ppds; i > 0; i --, ppd ++)
	cupsFileWrite(fp, (char *)&(ppd->record), sizeof(ppd_rec_t));

      cupsFileClose(fp);

      LogMessage(L_INFO, "LoadPPDs: Wrote \"%s\", %d PPDs...", filename,
        	 num_ppds);
    }
    else
      LogMessage(L_ERROR, "LoadPPDs: Unable to write \"%s\" - %s", filename,
        	 strerror(errno));
  }
  else
    LogMessage(L_INFO, "LoadPPDs: No new or changed PPDs...");

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
                 "ppd-name", NULL, ppd->record.ppd_name);
    ippAddString(PPDs, IPP_TAG_PRINTER, IPP_TAG_TEXT,
                 "ppd-make", NULL, ppd->record.ppd_make);
    ippAddString(PPDs, IPP_TAG_PRINTER, IPP_TAG_TEXT,
                 "ppd-make-and-model", NULL, ppd->record.ppd_make_and_model);
    ippAddString(PPDs, IPP_TAG_PRINTER, IPP_TAG_LANGUAGE,
                 "ppd-natural-language", NULL, ppd->record.ppd_natural_language);
  }

 /*
  * Free the memory used...
  */

  if (alloc_ppds)
  {
    free(ppds);
    alloc_ppds = 0;
  }


  num_options = cupsParseOptions(argv[3], 0, &options);
  requested   = cupsGetOption("requested-attributes", num_options, options);

  if (!requested || strstr(requested, "all"))
  {
    send_class          = 1;
    send_info           = 1;
    send_make_and_model = 1;
    send_uri            = 1;
  }
  else
  {
    send_class          = strstr(requested, "device-class") != NULL;
    send_info           = strstr(requested, "device-info") != NULL;
    send_make_and_model = strstr(requested, "device-make-and-model") != NULL;
    send_uri            = strstr(requested, "device-uri") != NULL;
  }

 /*
  * Try opening the backend directory...
  */

  if ((server_bin = getenv("CUPS_SERVERBIN")) == NULL)
    server_bin = CUPS_SERVERBIN;

  snprintf(backends, sizeof(backends), "%s/backend", server_bin);

  if ((dir = cupsDirOpen(backends)) == NULL)
  {
    fprintf(stderr, "ERROR: [cups-deviced] Unable to open backend directory \"%s\": %s",
            backends, strerror(errno));
    return (1);
  }

 /*
  * Setup the devices array...
  */

  alloc_devs = 0;
  num_devs   = 0;
  devs       = (dev_info_t *)0;

 /*
  * Loop through all of the device backends...
  */

  while ((dent = cupsDirRead(dir)) != NULL)
  {
   /*
    * Run the backend with no arguments and collect the output...
    */

    snprintf(filename, sizeof(filename), "%s/%s", backends, dent->filename);
    if ((fp = popen(filename, "r")) != NULL)
    {
     /*
      * Set an alarm for the first read from the backend; this avoids
      * problems when a backend is hung getting device information.
      */

#ifdef HAVE_SIGSET /* Use System V signals over POSIX to avoid bugs */
      sigset(SIGALRM, sigalrm_handler);
#elif defined(HAVE_SIGACTION)
      memset(&action, 0, sizeof(action));

      sigemptyset(&action.sa_mask);
      sigaddset(&action.sa_mask, SIGALRM);
      action.sa_handler = sigalrm_handler;
      sigaction(SIGALRM, &action, NULL);
#else
      signal(SIGALRM, sigalrm_handler);
#endif /* HAVE_SIGSET */

      alarm_tripped = 0;
      count         = 0;
      compat        = !strcmp(dent->filename, "smb");

      alarm(30);

      while (fgets(line, sizeof(line), fp) != NULL)
      {
       /*
        * Reset the alarm clock...
	*/

        alarm(30);

       /*
        * Each line is of the form:
	*
	*   class URI "make model" "name"
	*/

        if (!strncasecmp(line, "Usage", 5))
	  compat = 1;
        else if (sscanf(line, "%63s%1023s%*[ \t]\"%255[^\"]\"%*[ \t]\"%127[^\"]",
	                dclass, uri, make_model, info) != 4)
        {
	 /*
	  * Bad format; strip trailing newline and write an error message.
	  */

          if (line[strlen(line) - 1] == '\n')
	    line[strlen(line) - 1] = '\0';

	  fprintf(stderr, "ERROR: [cups-deviced] Bad line from \"%s\": %s\n",
	          dent->filename, line);
          compat = 1;
	  break;
        }
	else
	{
	 /*
	  * Add the device to the array of available devices...
	  */

          dev = add_dev(dclass, make_model, info, uri);
	  if (!dev)
	  {
            cupsDirClose(dir);
	    return (1);
	  }

          fprintf(stderr, "DEBUG: [cups-deviced] Added device \"%s\"...\n", uri);
	  count ++;
	}
      }

     /*
      * Turn the alarm clock off and close the pipe to the command...
      */

      alarm(0);

      if (alarm_tripped)
        fprintf(stderr, "WARNING: [cups-deviced] Backend \"%s\" did not respond within 30 seconds!\n",
	        dent->filename);

      pclose(fp);

     /*
      * Hack for backends that don't support the CUPS 1.1 calling convention:
      * add a network device with the method == backend name.
      */

      if (count == 0 && compat)
      {
	snprintf(line, sizeof(line), "Unknown Network Device (%s)",
	         dent->filename);

        dev = add_dev("network", line, "Unknown", dent->filename);
	if (!dev)
	{
          cupsDirClose(dir);
	  return (1);
	}

        fprintf(stderr, "DEBUG: [cups-deviced] Compatibility device \"%s\"...\n",
	        dent->filename);
      }
    }
    else
      fprintf(stderr, "WARNING: [cups-deviced] Unable to execute \"%s\" backend: %s\n",
              dent->filename, strerror(errno));
  }

  cupsDirClose(dir);

 /*
  * Sort the available devices...
  */

  if (num_devs > 1)
    qsort(devs, num_devs, sizeof(dev_info_t),
          (int (*)(const void *, const void *))compare_devs);

 /*
  * Output the list of devices...
  */

  puts("Content-Type: application/ipp\n");

  cupsdSendIPPHeader(IPP_OK, atoi(argv[1]));
  cupsdSendIPPGroup(IPP_TAG_OPERATION);
  cupsdSendIPPString(IPP_TAG_CHARSET, "attributes-charset", "utf-8");
  cupsdSendIPPString(IPP_TAG_LANGUAGE, "attributes-natural-language", "en-US");

  if ((count = atoi(argv[2])) <= 0)
    count = num_devs;

  if (count > num_devs)
    count = num_devs;

  for (dev = devs; count > 0; count --, dev ++)
  {
   /*
    * Add strings to attributes...
    */

    cupsdSendIPPGroup(IPP_TAG_PRINTER);
    if (send_class)
      cupsdSendIPPString(IPP_TAG_KEYWORD, "device-class", dev->device_class);
    if (send_info)
      cupsdSendIPPString(IPP_TAG_TEXT, "device-info", dev->device_info);
    if (send_make_and_model)
      cupsdSendIPPString(IPP_TAG_TEXT, "device-make-and-model",
                	 dev->device_make_and_model);
    if (send_uri)
      cupsdSendIPPString(IPP_TAG_URI, "device-uri", dev->device_uri);
  }

  cupsdSendIPPTrailer();

 /*
  * Free the devices array and return...
  */

  if (alloc_devs)
    free(devs);

  return (0);
}


/*
 * 'load_ppds()' - Load PPD files recursively.
 */

static void
load_ppds(const char *d,		/* I - Actual directory */
          const char *p)		/* I - Virtual path in name */
{
  int		i;			/* Looping var */
  cups_file_t	*fp;			/* Pointer to file */
  DIR		*dir;			/* Directory pointer */
  struct dirent	*dent;			/* Directory entry */
  struct stat	fileinfo;		/* File information */
  char		filename[1024],		/* Name of PPD or directory */
		line[256],		/* Line from backend */
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
      strlcpy(name, dent->d_name, sizeof(name));

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

   /*
    * See if this file has been scanned before...
    */

    if (sorted_ppds > 0)
    {
      strcpy(key.record.ppd_name, name);

      ppd = bsearch(&key, ppds, sorted_ppds, sizeof(ppd_info_t),
                    (int (*)(const void *, const void *))compare_names);

      if (ppd &&
          ppd->record.ppd_size == fileinfo.st_size &&
	  ppd->record.ppd_mtime == fileinfo.st_mtime)
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

    if ((fp = cupsFileOpen(filename, "rb")) == NULL)
      continue;

   /*
    * Now see if this is a PPD file...
    */

    line[0] = '\0';
    cupsFileGets(fp, line, sizeof(line));

    if (strncmp(line, "*PPD-Adobe:", 11) != 0)
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
    strcpy(language, "en");

    while (cupsFileGets(fp, line, sizeof(line)) != NULL)
    {
      if (strncmp(line, "*Manufacturer:", 14) == 0)
	sscanf(line, "%*[^\"]\"%255[^\"]", manufacturer);
      else if (strncmp(line, "*ModelName:", 11) == 0)
	sscanf(line, "%*[^\"]\"%127[^\"]", model_name);
      else if (strncmp(line, "*LanguageVersion:", 17) == 0)
	sscanf(line, "%*[^:]:%63s", language);
      else if (strncmp(line, "*NickName:", 10) == 0)
	sscanf(line, "%*[^\"]\"%255[^\"]", nick_name);
      else if (strncmp(line, "*OpenUI", 7) == 0)
      {
       /*
        * Stop early if we have a NickName or ModelName attributes
	* before the first OpenUI...
	*/

        if (model_name[0] || nick_name[0])
	  break;
      }

     /*
      * Stop early if we have both the Manufacturer and NickName
      * attributes...
      */

      if (manufacturer[0] && nick_name[0])
        break;
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
      cups_strcpy(make_model, make_model + 1);

    if (!make_model[0])
      continue;	/* Nope... */

   /*
    * See if we got a manufacturer...
    */

    while (isspace(manufacturer[0] & 255))
      cups_strcpy(manufacturer, manufacturer + 1);

    if (!manufacturer[0] || strcmp(manufacturer, "ESP") == 0)
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
      cups_strcpy(country + 1, ptr);
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

    new_ppd = !ppd;

    if (new_ppd)
    {
     /*
      * Allocate memory for the new PPD file...
      */

      LogMessage(L_DEBUG, "LoadPPDs: Adding ppd \"%s\"...", name);

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
    }
    else
      LogMessage(L_DEBUG, "LoadPPDs: Updating ppd \"%s\"...", name);

   /*
    * Zero the PPD record and copy the info over...
    */

    memset(ppd, 0, sizeof(ppd_info_t));

    ppd->found            = 1;
    ppd->record.ppd_mtime = fileinfo.st_mtime;
    ppd->record.ppd_size  = fileinfo.st_size;

    strlcpy(ppd->record.ppd_name, name,
            sizeof(ppd->record.ppd_name));
    strlcpy(ppd->record.ppd_make, manufacturer,
            sizeof(ppd->record.ppd_make));
    strlcpy(ppd->record.ppd_make_and_model, make_model,
            sizeof(ppd->record.ppd_make_and_model));
    strlcpy(ppd->record.ppd_natural_language, language,
            sizeof(ppd->record.ppd_natural_language));

    changed_ppd = 1;
  }

  closedir(dir);
}


/*
 * End of "$Id: ppds.c 4598 2005-08-25 21:36:26Z mike $".
 */
