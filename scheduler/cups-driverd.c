/*
 * "$Id$"
 *
 *   PPD/driver support for the Common UNIX Printing System (CUPS).
 *
 *   This program handles listing and installing both static PPD files
 *   in CUPS_DATADIR/model and dynamically generated PPD files using
 *   the driver helper programs in CUPS_SERVERBIN/driver.
 *
 *   Copyright 1997-2007 by Easy Software Products.
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
 *   free_array()    - Free an array of strings.
 *   list_ppds()     - List PPD files.
 *   load_ppds()     - Load PPD files recursively.
 *   load_drivers()  - Load driver-generated PPD files.
 */

/*
 * Include necessary headers...
 */

#include "util.h"
#include <cups/dir.h>
#include <cups/transcode.h>


/*
 * Private PPD functions...
 */

extern cups_encoding_t	_ppdGetEncoding(const char *name);


/*
 * Constants...
 */

#define PPD_SYNC	0x50504434	/* Sync word for ppds.dat (PPD4) */
#define PPD_MAX_LANG	32		/* Maximum languages */
#define PPD_MAX_PROD	8		/* Maximum products */
#define PPD_MAX_VERS	8		/* Maximum versions */

#define PPD_TYPE_POSTSCRIPT	0	/* PostScript PPD */
#define PPD_TYPE_PDF		1	/* PDF PPD */
#define PPD_TYPE_RASTER		2	/* CUPS raster PPD */
#define PPD_TYPE_FAX		3	/* Facsimile/MFD PPD */
#define PPD_TYPE_UNKNOWN	4	/* Other/hybrid PPD */

static const char *ppd_types[] =
{
  "postscript",
  "pdf",
  "raster",
  "fax",
  "unknown"
};


/*
 * PPD information structures...
 */

typedef struct				/**** PPD record ****/
{
  time_t	mtime;			/* Modification time */
  size_t	size;			/* Size in bytes */
  int		model_number;		/* cupsModelNumber */
  int		type;			/* ppd-type */
  char		name[512],		/* PPD name */
		languages[PPD_MAX_LANG][6],
					/* LanguageVersion/cupsLanguages */
		products[PPD_MAX_PROD][128],
					/* Product strings */
		psversions[PPD_MAX_VERS][32],
					/* PSVersion strings */
		make[128],		/* Manufacturer */
		make_and_model[128],	/* NickName/ModelName */
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

static ppd_info_t	*add_ppd(const char *name, const char *language,
		        	 const char *make, const char *make_and_model,
				 const char *device_id, const char *product,
				 const char *psversion, time_t mtime,
				 size_t size, int model_number, int type);
static int		cat_ppd(const char *name, int request_id);
static int		compare_names(const ppd_info_t *p0,
			              const ppd_info_t *p1);
static int		compare_ppds(const ppd_info_t *p0,
			             const ppd_info_t *p1);
static void		free_array(cups_array_t *a);
static int		list_ppds(int request_id, int limit, const char *opt);
static int		load_drivers(void);
static int		load_ppds(const char *d, const char *p, int descend);


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
    return (cat_ppd(argv[2], 0));
  else if (argc == 4 && !strcmp(argv[1], "get"))
    return (cat_ppd(argv[3], atoi(argv[2])));
  else if (argc == 5 && !strcmp(argv[1], "list"))
    return (list_ppds(atoi(argv[2]), atoi(argv[3]), argv[4]));
  else
  {
    fputs("Usage: cups-driverd cat ppd-name\n", stderr);
    fputs("Usage: cups-driverd get request_id ppd-name\n", stderr);
    fputs("Usage: cups-driverd list request_id limit options\n", stderr);
    return (1);
  }
}


/*
 * 'add_ppd()' - Add a PPD file.
 */

static ppd_info_t *			/* O - PPD */
add_ppd(const char *name,		/* I - PPD name */
        const char *language,		/* I - LanguageVersion */
        const char *make,		/* I - Manufacturer */
	const char *make_and_model,	/* I - NickName/ModelName */
	const char *device_id,		/* I - 1284DeviceID */
	const char *product,		/* I - Product */
	const char *psversion,		/* I - PSVersion */
        time_t     mtime,		/* I - Modification time */
	size_t     size,		/* I - File size */
	int        model_number,	/* I - Model number */
	int        type)		/* I - Driver type */
{
  ppd_info_t	*ppd;			/* PPD */
  char		*recommended;		/* Foomatic driver string */


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
      fprintf(stderr,
              "ERROR: [cups-driverd] Ran out of memory for %d PPD files!\n",
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

  ppd->found               = 1;
  ppd->record.mtime        = mtime;
  ppd->record.size         = size;
  ppd->record.model_number = model_number;
  ppd->record.type         = type;

  strlcpy(ppd->record.name, name, sizeof(ppd->record.name));
  strlcpy(ppd->record.languages[0], language,
          sizeof(ppd->record.languages[0]));
  strlcpy(ppd->record.products[0], product, sizeof(ppd->record.products[0]));
  strlcpy(ppd->record.psversions[0], psversion,
          sizeof(ppd->record.psversions[0]));
  strlcpy(ppd->record.make, make, sizeof(ppd->record.make));
  strlcpy(ppd->record.make_and_model, make_and_model,
          sizeof(ppd->record.make_and_model));
  strlcpy(ppd->record.device_id, device_id, sizeof(ppd->record.device_id));

 /*
  * Strip confusing (and often wrong) "recommended" suffix added by
  * Foomatic drivers...
  */

  if ((recommended = strstr(ppd->record.make_and_model,
                            " (recommended)")) != NULL)
    *recommended = '\0';

 /*
  * Return the new PPD pointer...
  */

  return (ppd);
}


/*
 * 'cat_ppd()' - Copy a PPD file to stdout.
 */

static int				/* O - Exit code */
cat_ppd(const char *name,		/* I - PPD name */
        int        request_id)		/* I - Request ID for response? */
{
  char		scheme[256],		/* Scheme from PPD name */
		*sptr;			/* Pointer into scheme */
  char		line[1024];		/* Line/filename */
  char		message[2048];		/* status-message */


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

  puts("Content-Type: application/ipp\n");

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

      if (request_id > 0)
      {
        snprintf(message, sizeof(message), "Unable to access \"%s\" - %s",
		 line, strerror(errno));

	cupsdSendIPPHeader(IPP_NOT_FOUND, request_id);
	cupsdSendIPPGroup(IPP_TAG_OPERATION);
	cupsdSendIPPString(IPP_TAG_CHARSET, "attributes-charset", "utf-8");
	cupsdSendIPPString(IPP_TAG_LANGUAGE, "attributes-natural-language",
	                   "en-US");
        cupsdSendIPPString(IPP_TAG_TEXT, "status-message", message);
        cupsdSendIPPTrailer();
      }

      return (1);
    }

   /*
    * Yes, let it cat the PPD file...
    */

    if (request_id)
    {
      cupsdSendIPPHeader(IPP_OK, request_id);
      cupsdSendIPPGroup(IPP_TAG_OPERATION);
      cupsdSendIPPString(IPP_TAG_CHARSET, "attributes-charset", "utf-8");
      cupsdSendIPPString(IPP_TAG_LANGUAGE, "attributes-natural-language",
			 "en-US");
      cupsdSendIPPTrailer();
    }

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

      if (request_id)
      {
	snprintf(message, sizeof(message), "Bad PPD name \"%s\"!", name);

	cupsdSendIPPHeader(IPP_OK, request_id);
	cupsdSendIPPGroup(IPP_TAG_OPERATION);
	cupsdSendIPPString(IPP_TAG_CHARSET, "attributes-charset", "utf-8");
	cupsdSendIPPString(IPP_TAG_LANGUAGE, "attributes-natural-language",
			   "en-US");
        cupsdSendIPPString(IPP_TAG_TEXT, "status-message", message);
	cupsdSendIPPTrailer();
      }

      return (1);
    }

   /*
    * Try opening the file...
    */

#ifdef __APPLE__
    if (!strncmp(name, "System/Library/Printers/PPDs/Contents/Resources/", 48) ||
        !strncmp(name, "Library/Printers/PPDs/Contents/Resources/", 41))
    {
     /*
      * Map ppd-name to Mac OS X standard locations...
      */

      snprintf(line, sizeof(line), "/%s", name);
    }
    else

#elif defined(__linux)
    if (!strncmp(name, "lsb/usr/", 8))
    {
     /*
      * Map ppd-name to LSB standard /usr/share/ppd location...
      */

      snprintf(line, sizeof(line), "/usr/share/ppd/%s", name + 8);
    }
    else if (!strncmp(name, "lsb/opt/", 8))
    {
     /*
      * Map ppd-name to LSB standard /opt/share/ppd location...
      */

      snprintf(line, sizeof(line), "/opt/share/ppd/%s", name + 8);
    }
    else if (!strncmp(name, "lsb/local/", 10))
    {
     /*
      * Map ppd-name to LSB standard /usr/local/share/ppd location...
      */

      snprintf(line, sizeof(line), "/usr/local/share/ppd/%s", name + 10);
    }
    else

#endif /* __APPLE__ */
    {
      if ((datadir = getenv("CUPS_DATADIR")) == NULL)
	datadir = CUPS_DATADIR;

      snprintf(line, sizeof(line), "%s/model/%s", datadir, name);
    }

    if ((fp = cupsFileOpen(line, "r")) == NULL)
    {
      fprintf(stderr, "ERROR: [cups-driverd] Unable to open \"%s\" - %s\n",
              line, strerror(errno));

      if (request_id)
      {
	snprintf(message, sizeof(message), "Unable to open \"%s\" - %s",
		 line, strerror(errno));

	cupsdSendIPPHeader(IPP_NOT_FOUND, request_id);
	cupsdSendIPPGroup(IPP_TAG_OPERATION);
	cupsdSendIPPString(IPP_TAG_CHARSET, "attributes-charset", "utf-8");
	cupsdSendIPPString(IPP_TAG_LANGUAGE, "attributes-natural-language",
			   "en-US");
        cupsdSendIPPString(IPP_TAG_TEXT, "status-message", message);
	cupsdSendIPPTrailer();
      }

      return (1);
    }

    if (request_id)
    {
      cupsdSendIPPHeader(IPP_OK, request_id);
      cupsdSendIPPGroup(IPP_TAG_OPERATION);
      cupsdSendIPPString(IPP_TAG_CHARSET, "attributes-charset", "utf-8");
      cupsdSendIPPString(IPP_TAG_LANGUAGE, "attributes-natural-language",
                         "en-US");
      cupsdSendIPPTrailer();
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
    return (strcasecmp(p0->record.languages[0],
                       p1->record.languages[0]));
}


/*
 * 'free_array()' - Free an array of strings.
 */

static void
free_array(cups_array_t *a)		/* I - Array to free */
{
  char	*ptr;				/* Pointer to string */


  for (ptr = (char *)cupsArrayFirst(a);
       ptr;
       ptr = (char *)cupsArrayNext(a))
    free(ptr);

  cupsArrayDelete(a);
}


/*
 * 'list_ppds()' - List PPD files.
 */

static int				/* O - Exit code */
list_ppds(int        request_id,	/* I - Request ID */
          int        limit,		/* I - Limit */
	  const char *opt)		/* I - Option argument */
{
  int		i, j;			/* Looping vars */
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
		*device_id,		/* ppd-device-id option */
		*language,		/* ppd-natural-language option */
		*make,			/* ppd-make option */
		*make_and_model,	/* ppd-make-and-model option */
		*model_number_str,	/* ppd-model-number option */
		*product,		/* ppd-product option */
		*psversion,		/* ppd-psversion option */
		*type_str;		/* ppd-type option */
  int		model_number,		/* ppd-model-number value */
		type,			/* ppd-type value */
		mam_len,		/* Length of ppd-make-and-model */
		device_id_len,		/* Length of ppd-device-id */
		send_device_id,		/* Send ppd-device-id? */
		send_make,		/* Send ppd-make? */
		send_make_and_model,	/* Send ppd-make-and-model? */
		send_model_number,	/* Send ppd-model-number? */
		send_name,		/* Send ppd-name? */
		send_natural_language,	/* Send ppd-natural-language? */
		send_product,		/* Send ppd-product? */
		send_psversion,		/* Send ppd-psversion? */
		send_type,		/* Send ppd-type? */
		sent_header;		/* Sent the IPP header? */


  fprintf(stderr,
          "DEBUG2: [cups-driverd] list_ppds(request_id=%d, limit=%d, "
          "opt=\"%s\"\n", request_id, limit, opt);

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
  if ((fp = cupsFileOpen(filename, "r")) != NULL)
  {
   /*
    * See if we have the right sync word...
    */

    unsigned ppdsync;			/* Sync word */

    if (cupsFileRead(fp, (char *)&ppdsync, sizeof(ppdsync))
            == sizeof(ppdsync) &&
        ppdsync == PPD_SYNC &&
        !stat(filename, &fileinfo) &&
	((fileinfo.st_size - sizeof(ppdsync)) % sizeof(ppd_rec_t)) == 0 &&
	(NumPPDs = (fileinfo.st_size - sizeof(ppdsync)) /
	           sizeof(ppd_rec_t)) > 0)
    {
     /*
      * We have a ppds.dat file, so read it!
      */

      if ((PPDs = malloc(sizeof(ppd_info_t) * NumPPDs)) == NULL)
	fprintf(stderr,
		"ERROR: [cups-driverd] Unable to allocate memory for %d "
		"PPD files!\n", NumPPDs);
      else
      {
        AllocPPDs = NumPPDs;

	for (i = NumPPDs, ppd = PPDs; i > 0; i --, ppd ++)
	{
	  cupsFileRead(fp, (char *)&(ppd->record), sizeof(ppd_rec_t));
	  ppd->found = 0;
	}

	fprintf(stderr, "INFO: [cups-driverd] Read \"%s\", %d PPDs...\n",
		filename, NumPPDs);
      }
    }

    cupsFileClose(fp);
  }
  
 /*
  * Load all PPDs in the specified directory and below...
  */

  SortedPPDs = NumPPDs;

  if ((cups_datadir = getenv("CUPS_DATADIR")) == NULL)
    cups_datadir = CUPS_DATADIR;

  snprintf(model, sizeof(model), "%s/model", cups_datadir);
  load_ppds(model, "", 1);

#ifdef __APPLE__
 /*
  * Load PPDs from standard Mac OS X locations...
  */

  load_ppds("/Library/Printers/PPDs/Contents/Resources",
            "Library/Printers/PPDs/Contents/Resources", 0);
  load_ppds("/Library/Printers/PPDs/Contents/Resources/en.lproj",
            "Library/Printers/PPDs/Contents/Resources/en.lproj", 0);
  load_ppds("/System/Library/Printers/PPDs/Contents/Resources",
            "System/Library/Printers/PPDs/Contents/Resources", 0);
  load_ppds("/System/Library/Printers/PPDs/Contents/Resources/en.lproj",
            "System/Library/Printers/PPDs/Contents/Resources/en.lproj", 0);

#elif defined(__linux)
 /*
  * Load PPDs from LSB-defined locations...
  */

  load_ppds("/usr/local/share/ppds", "lsb/local", 1);
  load_ppds("/usr/share/ppds", "lsb/usr", 1);
  load_ppds("/opt/share/ppds", "lsb/opt", 1);
#endif /* __APPLE__ */

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
      unsigned ppdsync = PPD_SYNC;	/* Sync word */


      cupsFileWrite(fp, (char *)&ppdsync, sizeof(ppdsync));

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

  add_ppd("raw", "en", "Raw", "Raw Queue", "", "", "", 0, 0, 0,
          PPD_TYPE_UNKNOWN);

 /*
  * Sort the PPDs by make and model...
  */

  if (NumPPDs > 1)
    qsort(PPDs, NumPPDs, sizeof(ppd_info_t),
          (int (*)(const void *, const void *))compare_ppds);

 /*
  * Send IPP attributes...
  */

  num_options      = cupsParseOptions(opt, 0, &options);
  requested        = cupsGetOption("requested-attributes", num_options, options);
  device_id        = cupsGetOption("ppd-device-id", num_options, options);
  language         = cupsGetOption("ppd-natural-language", num_options, options);
  make             = cupsGetOption("ppd-make", num_options, options);
  make_and_model   = cupsGetOption("ppd-make-and-model", num_options, options);
  model_number_str = cupsGetOption("ppd-model-number", num_options, options);
  product          = cupsGetOption("ppd-product", num_options, options);
  psversion        = cupsGetOption("ppd-psversion", num_options, options);
  type_str         = cupsGetOption("ppd-type", num_options, options);

  if (make_and_model)
    mam_len = strlen(make_and_model);
  else
    mam_len = 0;

  if (device_id)
    device_id_len = strlen(device_id);
  else
    device_id_len = 0;

  if (model_number_str)
    model_number = atoi(model_number_str);
  else
    model_number = 0;

  if (type_str)
  {
    for (type = 0;
         type < (int)(sizeof(ppd_types) / sizeof(ppd_types[0]));
	 type ++)
      if (!strcmp(type_str, ppd_types[type]))
        break;

    if (type >= (int)(sizeof(ppd_types) / sizeof(ppd_types[0])))
    {
      fprintf(stderr, "ERROR: [cups-driverd] Bad ppd-type=\"%s\" ignored!\n",
              type_str);
      type_str = NULL;
    }
  }

  if (requested)
    fprintf(stderr, "DEBUG: [cups-driverd] requested-attributes=\"%s\"\n",
	    requested);
  if (device_id)
    fprintf(stderr, "DEBUG: [cups-driverd] ppd-device-id=\"%s\"\n",
	    device_id);
  if (language)
    fprintf(stderr, "DEBUG: [cups-driverd] ppd-natural-language=\"%s\"\n",
	    language);
  if (make)
    fprintf(stderr, "DEBUG: [cups-driverd] ppd-make=\"%s\"\n",
	    make);
  if (make_and_model)
    fprintf(stderr, "DEBUG: [cups-driverd] ppd-make-and-model=\"%s\"\n",
	    make_and_model);
  if (model_number_str)
    fprintf(stderr, "DEBUG: [cups-driverd] ppd-model-number=\"%s\"\n",
	    model_number_str);
  if (product)
    fprintf(stderr, "DEBUG: [cups-driverd] ppd-product=\"%s\"\n",
	    product);
  if (psversion)
    fprintf(stderr, "DEBUG: [cups-driverd] ppd-psversion=\"%s\"\n",
	    psversion);
  if (type_str)
    fprintf(stderr, "DEBUG: [cups-driverd] ppd-type=\"%s\"\n", type_str);

  if (!requested || strstr(requested, "all"))
  {
    send_name             = 1;
    send_make             = 1;
    send_make_and_model   = 1;
    send_model_number     = 1;
    send_natural_language = 1;
    send_device_id        = 1;
    send_product          = 1;
    send_psversion        = 1;
    send_type             = 1;
  }
  else
  {
    send_name             = strstr(requested, "ppd-name") != NULL;
    send_make             = strstr(requested, "ppd-make,") != NULL ||
                            strstr(requested, ",ppd-make") != NULL ||
                            !strcmp(requested, "ppd-make");
    send_make_and_model   = strstr(requested, "ppd-make-and-model") != NULL;
    send_model_number     = strstr(requested, "ppd-model-number") != NULL;
    send_natural_language = strstr(requested, "ppd-natural-language") != NULL;
    send_device_id        = strstr(requested, "ppd-device-id") != NULL;
    send_product          = strstr(requested, "ppd-product") != NULL;
    send_psversion        = strstr(requested, "ppd-psversion") != NULL;
    send_type             = strstr(requested, "ppd-type") != NULL;
  }

  puts("Content-Type: application/ipp\n");

  sent_header = 0;

  if (limit <= 0 || limit > NumPPDs)
    count = NumPPDs;
  else
    count = limit;

  for (i = NumPPDs, ppd = PPDs; count > 0 && i > 0; i --, ppd ++)
  {
   /*
    * Filter PPDs based on make, model, or device ID...
    */

    if (device_id && strncasecmp(ppd->record.device_id, device_id,
                                 device_id_len))
      continue;				/* TODO: implement smart compare */

    if (language)
    {
      for (j = 0; j < PPD_MAX_LANG; j ++)
	if (!ppd->record.languages[j][0] ||
	    !strcasecmp(ppd->record.languages[j], language))
	  break;

      if (j >= PPD_MAX_LANG || !ppd->record.languages[j][0])
	continue;
    }

    if (make && strcasecmp(ppd->record.make, make))
      continue;

    if (make_and_model && strncasecmp(ppd->record.make_and_model,
                                      make_and_model, mam_len))
      continue;

    if (model_number_str && ppd->record.model_number != model_number)
      continue;

    if (product)
    {
      for (j = 0; j < PPD_MAX_PROD; j ++)
	if (!ppd->record.products[j][0] ||
	    !strcasecmp(ppd->record.products[j], product))
	  break;

      if (j >= PPD_MAX_PROD || !ppd->record.products[j][0])
	continue;
    }

    if (psversion)
    {
      for (j = 0; j < PPD_MAX_VERS; j ++)
	if (!ppd->record.psversions[j][0] ||
	    !strcasecmp(ppd->record.psversions[j], psversion))
	  break;

      if (j >= PPD_MAX_VERS || !ppd->record.psversions[j][0])
	continue;
    }

    if (type_str && ppd->record.type != type)
      continue;

   /*
    * Send this PPD...
    */

    if (!sent_header)
    {
      sent_header = 1;

      cupsdSendIPPHeader(IPP_OK, request_id);
      cupsdSendIPPGroup(IPP_TAG_OPERATION);
      cupsdSendIPPString(IPP_TAG_CHARSET, "attributes-charset", "utf-8");
      cupsdSendIPPString(IPP_TAG_LANGUAGE, "attributes-natural-language", "en-US");
    }

    fprintf(stderr, "DEBUG: [cups-driverd] Sending %s (%s)...\n",
	    ppd->record.name, ppd->record.make_and_model);

    count --;

    cupsdSendIPPGroup(IPP_TAG_PRINTER);

    if (send_name)
      cupsdSendIPPString(IPP_TAG_NAME, "ppd-name", ppd->record.name);

    if (send_natural_language)
    {
      cupsdSendIPPString(IPP_TAG_LANGUAGE, "ppd-natural-language",
			 ppd->record.languages[0]);

      for (j = 1; j < PPD_MAX_LANG && ppd->record.languages[j][0]; j ++)
	cupsdSendIPPString(IPP_TAG_LANGUAGE, "", ppd->record.languages[j]);
    }

    if (send_make)
      cupsdSendIPPString(IPP_TAG_TEXT, "ppd-make", ppd->record.make);

    if (send_make_and_model)
      cupsdSendIPPString(IPP_TAG_TEXT, "ppd-make-and-model",
			 ppd->record.make_and_model);

    if (send_device_id)
      cupsdSendIPPString(IPP_TAG_TEXT, "ppd-device-id",
			 ppd->record.device_id);

    if (send_product)
    {
      cupsdSendIPPString(IPP_TAG_TEXT, "ppd-product",
			 ppd->record.products[0]);

      for (j = 1; j < PPD_MAX_PROD && ppd->record.products[j][0]; j ++)
	cupsdSendIPPString(IPP_TAG_TEXT, "", ppd->record.products[j]);
    }

    if (send_psversion)
    {
      cupsdSendIPPString(IPP_TAG_TEXT, "ppd-psversion",
			 ppd->record.psversions[0]);

      for (j = 1; j < PPD_MAX_VERS && ppd->record.psversions[j][0]; j ++)
	cupsdSendIPPString(IPP_TAG_TEXT, "", ppd->record.psversions[j]);
    }

    if (send_type)
      cupsdSendIPPString(IPP_TAG_KEYWORD, "ppd-type",
			 ppd_types[ppd->record.type]);

    if (send_model_number)
      cupsdSendIPPInteger(IPP_TAG_INTEGER, "ppd-model-number",
			  ppd->record.model_number);

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

  if (!sent_header)
  {
    cupsdSendIPPHeader(IPP_NOT_FOUND, request_id);
    cupsdSendIPPGroup(IPP_TAG_OPERATION);
    cupsdSendIPPString(IPP_TAG_CHARSET, "attributes-charset", "utf-8");
    cupsdSendIPPString(IPP_TAG_LANGUAGE, "attributes-natural-language", "en-US");
  }

  cupsdSendIPPTrailer();

  return (0);
}


/*
 * 'load_ppds()' - Load PPD files recursively.
 */

static int				/* O - 1 on success, 0 on failure */
load_ppds(const char *d,		/* I - Actual directory */
          const char *p,		/* I - Virtual path in name */
	  int        descend)		/* I - Descend into directories? */
{
  int		i;			/* Looping var */
  cups_file_t	*fp;			/* Pointer to file */
  cups_dir_t	*dir;			/* Directory pointer */
  cups_dentry_t	*dent;			/* Directory entry */
  char		filename[1024],		/* Name of PPD or directory */
		line[256],		/* Line from backend */
		*ptr,			/* Pointer into name */
		name[128],		/* Name of PPD file */
		lang_version[64],	/* PPD LanguageVersion */
		lang_encoding[64],	/* PPD LanguageEncoding */
		country[64],		/* Country code */
		manufacturer[256],	/* Manufacturer */
		make_model[256],	/* Make and Model */
		model_name[256],	/* ModelName */
		nick_name[256],		/* NickName */
		device_id[256],		/* 1284DeviceID */
		product[256],		/* Product */
		psversion[256];		/* PSVersion */
  int		model_number,		/* cupsModelNumber */
		type;			/* ppd-type */
  cups_array_t	*products,		/* Product array */
		*psversions,		/* PSVersion array */
		*cups_languages;	/* cupsLanguages array */
  ppd_info_t	*ppd,			/* New PPD file */
		key;			/* Search key */
  int		new_ppd;		/* Is this a new PPD? */
  struct				/* LanguageVersion translation table */
  {
    const char	*version,		/* LanguageVersion string */
		*language;		/* Language code */
  }		languages[] =
  {
    { "chinese",	"zh" },
    { "danish",		"da" },
    { "dutch",		"nl" },
    { "english",	"en" },
    { "finnish",	"fi" },
    { "french",		"fr" },
    { "german",		"de" },
    { "greek",		"el" },
    { "italian",	"it" },
    { "japanese",	"ja" },
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
    fprintf(stderr,
            "ERROR: [cups-driverd] Unable to open PPD directory \"%s\": %s\n",
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

      if (descend)
	if (!load_ppds(filename, name, 1))
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

    cups_languages = cupsArrayNew(NULL, NULL);
    products       = cupsArrayNew(NULL, NULL);
    psversions     = cupsArrayNew(NULL, NULL);

    model_name[0]    = '\0';
    nick_name[0]     = '\0';
    manufacturer[0]  = '\0';
    device_id[0]     = '\0';
    lang_encoding[0] = '\0';
    strcpy(lang_version, "en");
    model_number     = 0;
    type             = PPD_TYPE_POSTSCRIPT;

    while (cupsFileGets(fp, line, sizeof(line)) != NULL)
    {
      if (!strncmp(line, "*Manufacturer:", 14))
	sscanf(line, "%*[^\"]\"%255[^\"]", manufacturer);
      else if (!strncmp(line, "*ModelName:", 11))
	sscanf(line, "%*[^\"]\"%127[^\"]", model_name);
      else if (!strncmp(line, "*LanguageEncoding:", 18))
	sscanf(line, "%*[^:]:%63s", lang_encoding);
      else if (!strncmp(line, "*LanguageVersion:", 17))
	sscanf(line, "%*[^:]:%63s", lang_version);
      else if (!strncmp(line, "*NickName:", 10))
	sscanf(line, "%*[^\"]\"%255[^\"]", nick_name);
      else if (!strncasecmp(line, "*1284DeviceID:", 14))
	sscanf(line, "%*[^\"]\"%255[^\"]", device_id);
      else if (!strncmp(line, "*Product:", 9))
      {
	sscanf(line, "%*[^\"]\"(%255[^)]", product);
	cupsArrayAdd(products, strdup(product));
      }
      else if (!strncmp(line, "*PSVersion:", 11))
      {
	sscanf(line, "%*[^\"]\"%255[^\"]", psversion);
	cupsArrayAdd(psversions, strdup(psversion));
      }
      else if (!strncmp(line, "*cupsLanguages:", 15))
      {
        char	*start;			/* Start of language */


        for (start = line + 15; *start && isspace(*start & 255); start ++);

	if (*start++ == '\"')
	{
	  while (*start)
	  {
	    for (ptr = start + 1;
	         *ptr && *ptr != '\"' && !isspace(*ptr & 255);
		 ptr ++);

            if (*ptr)
	    {
	      *ptr++ = '\0';

	      while (isspace(*ptr & 255))
	        *ptr++ = '\0';
            }

            cupsArrayAdd(cups_languages, strdup(start));
	    start = ptr;
	  }
	}
      }
      else if (!strncmp(line, "*cupsFax:", 9))
      {
        for (ptr = line + 9; isspace(*ptr & 255); ptr ++);

	if (!strncasecmp(ptr, "true", 4))
          type = PPD_TYPE_FAX;
      }
      else if (!strncmp(line, "*cupsFilter:", 12) &&
               (type == PPD_TYPE_POSTSCRIPT || type == PPD_TYPE_UNKNOWN))
      {
        if (strstr(line + 12, "application/vnd.cups-raster"))
	  type = PPD_TYPE_RASTER;
        else if (strstr(line + 12, "application/vnd.cups-pdf"))
	  type = PPD_TYPE_PDF;
	else
	  type = PPD_TYPE_UNKNOWN;
      }
      else if (!strncmp(line, "*cupsModelNumber:", 17))
        sscanf(line, "*cupsModelNumber:%d", &model_number);
      else if (!strncmp(line, "*OpenUI", 7))
      {
       /*
        * Stop early if we have a NickName or ModelName attributes
	* before the first OpenUI...
	*/

        if ((model_name[0] || nick_name[0]) && cupsArrayCount(products) > 0 &&
	    cupsArrayCount(psversions) > 0)
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
      cupsCharsetToUTF8((cups_utf8_t *)make_model, nick_name,
                        sizeof(make_model), _ppdGetEncoding(lang_encoding));
    else
      strcpy(make_model, model_name);

    while (isspace(make_model[0] & 255))
      _cups_strcpy(make_model, make_model + 1);

    if (!make_model[0] || cupsArrayCount(products) == 0 ||
        cupsArrayCount(psversions) == 0)
    {
     /*
      * We don't have all the info needed, so skip this file...
      */

      if (!make_model[0])
        fprintf(stderr, "WARNING: Missing NickName and ModelName in %s!\n",
	        filename);

      if (cupsArrayCount(products) == 0)
        fprintf(stderr, "WARNING: Missing Product in %s!\n", filename);

      if (cupsArrayCount(psversions) == 0)
        fprintf(stderr, "WARNING: Missing PSVersion in %s!\n", filename);

      free_array(products);
      free_array(psversions);
      free_array(cups_languages);

      continue;
    }

    if (model_name[0])
      cupsArrayAdd(products, strdup(model_name));

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
    * Fix the lang_version as needed...
    */

    if ((ptr = strchr(lang_version, '-')) != NULL)
      *ptr++ = '\0';
    else if ((ptr = strchr(lang_version, '_')) != NULL)
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
      if (!strcasecmp(languages[i].version, lang_version))
        break;

    if (i < (int)(sizeof(languages) / sizeof(languages[0])))
    {
     /*
      * Found a known language...
      */

      snprintf(lang_version, sizeof(lang_version), "%s%s",
               languages[i].language, country);
    }
    else
    {
     /*
      * Unknown language; use "xx"...
      */

      strcpy(lang_version, "xx");
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

      ppd = add_ppd(name, lang_version, manufacturer, make_model, device_id,
                    (char *)cupsArrayFirst(products),
                    (char *)cupsArrayFirst(psversions),
                    dent->fileinfo.st_mtime, dent->fileinfo.st_size,
		    model_number, type);

      if (!ppd)
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

      ppd->found               = 1;
      ppd->record.mtime        = dent->fileinfo.st_mtime;
      ppd->record.size         = dent->fileinfo.st_size;
      ppd->record.model_number = model_number;
      ppd->record.type         = type;

      strlcpy(ppd->record.name, name, sizeof(ppd->record.name));
      strlcpy(ppd->record.make, manufacturer, sizeof(ppd->record.make));
      strlcpy(ppd->record.make_and_model, make_model,
              sizeof(ppd->record.make_and_model));
      strlcpy(ppd->record.languages[0], lang_version,
              sizeof(ppd->record.languages[0]));
      strlcpy(ppd->record.products[0], (char *)cupsArrayFirst(products),
              sizeof(ppd->record.products[0]));
      strlcpy(ppd->record.psversions[0], (char *)cupsArrayFirst(psversions),
              sizeof(ppd->record.psversions[0]));
      strlcpy(ppd->record.device_id, device_id, sizeof(ppd->record.device_id));
    }

   /*
    * Add remaining products, versions, and languages...
    */

    for (i = 1;
         i < PPD_MAX_PROD && (ptr = (char *)cupsArrayNext(products)) != NULL;
	 i ++)
      strlcpy(ppd->record.products[i], ptr,
              sizeof(ppd->record.products[0]));

    for (i = 1;
         i < PPD_MAX_VERS && (ptr = (char *)cupsArrayNext(psversions)) != NULL;
	 i ++)
      strlcpy(ppd->record.psversions[i], ptr,
              sizeof(ppd->record.psversions[0]));

    for (i = 1, ptr = (char *)cupsArrayFirst(cups_languages);
         i < PPD_MAX_LANG && ptr;
	 i ++, ptr = (char *)cupsArrayNext(cups_languages))
      strlcpy(ppd->record.languages[i], ptr,
              sizeof(ppd->record.languages[0]));

   /*
    * Free products, versions, and languages...
    */

    free_array(cups_languages);
    free_array(products);
    free_array(psversions);

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
  int		i;			/* Looping var */
  char		*start,			/* Start of value */
		*ptr;			/* Pointer into string */
  const char	*server_bin;		/* CUPS_SERVERBIN env variable */
  char		drivers[1024];		/* Location of driver programs */
  FILE		*fp;			/* Pipe to driver program */
  cups_dir_t	*dir;			/* Directory pointer */
  cups_dentry_t *dent;			/* Directory entry */
  char		filename[1024],		/* Name of driver */
		line[2048],		/* Line from driver */
		name[512],		/* ppd-name */
		make[128],		/* ppd-make */
		make_and_model[128],	/* ppd-make-and-model */
		device_id[128],		/* ppd-device-id */
		languages[128],		/* ppd-natural-language */
		product[128],		/* ppd-product */
		psversion[128],		/* ppd-psversion */
		type_str[128];		/* ppd-type */
  int		type;			/* PPD type */
  ppd_info_t	*ppd;			/* Newly added PPD */


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
	*   "ppd-name" ppd-natural-language "ppd-make" "ppd-make-and-model" \
	*       "ppd-device-id" "ppd-product" "ppd-psversion"
	*/

        device_id[0] = '\0';
	product[0]   = '\0';
	psversion[0] = '\0';
	strcpy(type_str, "postscript");

        if (sscanf(line, "\"%511[^\"]\"%127s%*[ \t]\"%127[^\"]\""
	                 "%*[ \t]\"%127[^\"]\"%*[ \t]\"%127[^\"]\""
			 "%*[ \t]\"%127[^\"]\"%*[ \t]\"%127[^\"]\""
			 "%*[ \t]\"%127[^\"]\"",
	           name, languages, make, make_and_model,
		   device_id, product, psversion, type_str) < 4)
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

          if ((start = strchr(languages, ',')) != NULL)
	    *start++ = '\0';

	  for (type = 0;
               type < (int)(sizeof(ppd_types) / sizeof(ppd_types[0]));
	       type ++)
	    if (!strcmp(type_str, ppd_types[type]))
              break;

	  if (type >= (int)(sizeof(ppd_types) / sizeof(ppd_types[0])))
	  {
	    fprintf(stderr, "ERROR: [cups-driverd] Bad ppd-type \"%s\" ignored!\n",
        	    type_str);
	    type = PPD_TYPE_UNKNOWN;
	  }

          ppd = add_ppd(name, languages, make, make_and_model, device_id,
	                product, psversion, 0, 0, 0, type);

          if (!ppd)
	  {
            cupsDirClose(dir);
	    return (0);
	  }

          if (start && *start)
	  {
	    for (i = 1; i < PPD_MAX_LANG && *start; i ++)
	    {
	      if ((ptr = strchr(start, ',')) != NULL)
	        *ptr++ = '\0';
	      else
	        ptr = start + strlen(start);

              strlcpy(ppd->record.languages[i], start,
	              sizeof(ppd->record.languages[0]));

	      start = ptr;
	    }
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
