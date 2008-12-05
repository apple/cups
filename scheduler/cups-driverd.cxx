/*
 * "$Id$"
 *
 *   PPD/driver support for the Common UNIX Printing System (CUPS).
 *
 *   This program handles listing and installing both static PPD files
 *   in CUPS_DATADIR/model and dynamically generated PPD files using
 *   the driver helper programs in CUPS_SERVERBIN/driver.
 *
 *   Copyright 2007-2008 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
 *   main()            - Scan for drivers and return an IPP response.
 *   add_ppd()         - Add a PPD file.
 *   cat_drv()         - Generate a PPD from a driver info file.
 *   cat_ppd()         - Copy a PPD file to stdout.
 *   copy_static()     - Copy a static PPD file to stdout.
 *   compare_matches() - Compare PPD match scores for sorting.
 *   compare_names()   - Compare PPD filenames for sorting.
 *   compare_ppds()    - Compare PPD file make and model names for sorting.
 *   free_array()      - Free an array of strings.
 *   list_ppds()       - List PPD files.
 *   load_ppds()       - Load PPD files recursively.
 *   load_drv()        - Load the PPDs from a driver information file.
 *   load_drivers()    - Load driver-generated PPD files.
 *   regex_device_id() - Compile a regular expression based on the 1284 device
 *                       ID.
 *   regex_string()    - Construct a regular expression to compare a simple
 *                       string.
 */

/*
 * Include necessary headers...
 */

#include "util.h"
#include <cups/dir.h>
#include <cups/transcode.h>
#include <cups/ppd-private.h>
#include <ppdc/ppdc.h>
#include <regex.h>


/*
 * Constants...
 */

#define PPD_SYNC	0x50504436	/* Sync word for ppds.dat (PPD6) */
#define PPD_MAX_LANG	32		/* Maximum languages */
#define PPD_MAX_PROD	8		/* Maximum products */
#define PPD_MAX_VERS	8		/* Maximum versions */

#define PPD_TYPE_POSTSCRIPT	0	/* PostScript PPD */
#define PPD_TYPE_PDF		1	/* PDF PPD */
#define PPD_TYPE_RASTER		2	/* CUPS raster PPD */
#define PPD_TYPE_FAX		3	/* Facsimile/MFD PPD */
#define PPD_TYPE_UNKNOWN	4	/* Other/hybrid PPD */
#define PPD_TYPE_DRV		5	/* Driver info file */

static const char * const ppd_types[] =	/* ppd-type values */
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
  off_t		size;			/* Size in bytes */
  int		model_number;		/* cupsModelNumber */
  int		type;			/* ppd-type */
  char		filename[512],		/* Filename */
		name[512],		/* PPD name */
		languages[PPD_MAX_LANG][6],
					/* LanguageVersion/cupsLanguages */
		products[PPD_MAX_PROD][128],
					/* Product strings */
		psversions[PPD_MAX_VERS][32],
					/* PSVersion strings */
		make[128],		/* Manufacturer */
		make_and_model[128],	/* NickName/ModelName */
		device_id[256],		/* IEEE 1284 Device ID */
		scheme[128];		/* PPD scheme */
} ppd_rec_t; 

typedef struct				/**** In-memory record ****/
{
  int		found;			/* 1 if PPD is found */
  int		matches;		/* Match count */
  ppd_rec_t	record;			/* PPDs.dat record */
} ppd_info_t;


/*
 * Globals...
 */

cups_array_t	*PPDsByName = NULL,	/* PPD files sorted by filename and name */
		*PPDsByMakeModel = NULL;/* PPD files sorted by make and model */
int		ChangedPPD;		/* Did we change the PPD database? */


/*
 * Local functions...
 */

static ppd_info_t	*add_ppd(const char *filename, const char *name,
			         const char *language, const char *make,
				 const char *make_and_model,
				 const char *device_id, const char *product,
				 const char *psversion, time_t mtime,
				 size_t size, int model_number, int type,
				 const char *scheme);
static int		cat_drv(const char *name, int request_id);
static int		cat_ppd(const char *name, int request_id);
static int		cat_static(const char *name, int request_id);
static int		compare_matches(const ppd_info_t *p0,
			                const ppd_info_t *p1);
static int		compare_names(const ppd_info_t *p0,
			              const ppd_info_t *p1);
static int		compare_ppds(const ppd_info_t *p0,
			             const ppd_info_t *p1);
static void		free_array(cups_array_t *a);
static int		list_ppds(int request_id, int limit, const char *opt);
static int		load_drivers(cups_array_t *include,
			             cups_array_t *exclude);
static int		load_drv(const char *filename, const char *name,
			         cups_file_t *fp, time_t mtime, off_t size);
static int		load_ppds(const char *d, const char *p, int descend);
static regex_t		*regex_device_id(const char *device_id);
static regex_t		*regex_string(const char *s);


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
add_ppd(const char *filename,		/* I - PPD filename */
        const char *name,		/* I - PPD name */
        const char *language,		/* I - LanguageVersion */
        const char *make,		/* I - Manufacturer */
	const char *make_and_model,	/* I - NickName/ModelName */
	const char *device_id,		/* I - 1284DeviceID */
	const char *product,		/* I - Product */
	const char *psversion,		/* I - PSVersion */
        time_t     mtime,		/* I - Modification time */
	size_t     size,		/* I - File size */
	int        model_number,	/* I - Model number */
	int        type,		/* I - Driver type */
	const char *scheme)		/* I - PPD scheme */
{
  ppd_info_t	*ppd;			/* PPD */
  char		*recommended;		/* Foomatic driver string */


 /*
  * Add a new PPD file...
  */

  if ((ppd = (ppd_info_t *)calloc(1, sizeof(ppd_info_t))) == NULL)
  {
    fprintf(stderr,
	    "ERROR: [cups-driverd] Ran out of memory for %d PPD files!\n",
	    cupsArrayCount(PPDsByName));
    return (NULL);
  }

 /*
  * Zero-out the PPD data and copy the values over...
  */

  ppd->found               = 1;
  ppd->record.mtime        = mtime;
  ppd->record.size         = size;
  ppd->record.model_number = model_number;
  ppd->record.type         = type;

  strlcpy(ppd->record.filename, filename, sizeof(ppd->record.filename));
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
  strlcpy(ppd->record.scheme, scheme, sizeof(ppd->record.scheme));

 /*
  * Strip confusing (and often wrong) "recommended" suffix added by
  * Foomatic drivers...
  */

  if ((recommended = strstr(ppd->record.make_and_model,
                            " (recommended)")) != NULL)
    *recommended = '\0';

 /*
  * Add the PPD to the PPD arrays...
  */

  cupsArrayAdd(PPDsByName, ppd);
  cupsArrayAdd(PPDsByMakeModel, ppd);

 /*
  * Return the new PPD pointer...
  */

  return (ppd);
}


/*
 * 'cat_drv()' - Generate a PPD from a driver info file.
 */

static int				/* O - Exit code */
cat_drv(const char *name,		/* I - PPD name */
        int        request_id)		/* I - Request ID for response? */
{
  const char	*datadir;		// CUPS_DATADIR env var
  ppdcSource	*src;			// PPD source file data
  ppdcDriver	*d;			// Current driver
  cups_file_t	*out;			// Stdout via CUPS file API 
  char		message[2048],		// status-message
		filename[1024],		// Full path to .drv file(s)
		scheme[32],		// URI scheme ("drv")
		userpass[256],		// User/password info (unused)
		host[2],		// Hostname (unused)
		resource[1024],		// Resource path (/dir/to/filename.drv)
		*pc_file_name;		// Filename portion of URI
  int		port;			// Port number (unused)


  // Determine where CUPS has installed the data files...
  if ((datadir = getenv("CUPS_DATADIR")) == NULL)
    datadir = CUPS_DATADIR;

  // Pull out the 
  if (httpSeparateURI(HTTP_URI_CODING_ALL, name, scheme, sizeof(scheme),
                      userpass, sizeof(userpass), host, sizeof(host), &port,
		      resource, sizeof(resource)) < HTTP_URI_OK ||
      strstr(resource, "../") ||
      (pc_file_name = strrchr(resource, '/')) == NULL ||
      pc_file_name == resource)
  {
    fprintf(stderr, "ERROR: Bad PPD name \"%s\"!\n", name);

    if (request_id)
    {
      snprintf(message, sizeof(message), "Bad PPD name \"%s\"!", name);

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

  *pc_file_name++ = '\0';

#ifdef __APPLE__
  if (!strncmp(resource, "/Library/Printers/PPDs.drv/", 27))
    strlcpy(filename, resource, sizeof(filename));
  else
#endif // __APPLE__
  {
    snprintf(filename, sizeof(filename), "%s/drv%s", datadir, resource);
    if (access(filename, 0))
      snprintf(filename, sizeof(filename), "%s/model%s", datadir, resource);
  }

  src = new ppdcSource(filename);

  for (d = (ppdcDriver *)src->drivers->first();
       d;
       d = (ppdcDriver *)src->drivers->next())
    if (!strcasecmp(pc_file_name, d->pc_file_name->value) ||
        (d->file_name && !strcasecmp(pc_file_name, d->file_name->value)))
      break;

  if (d)
  {
    ppdcArray	*locales;		// Locale names
    ppdcCatalog	*catalog;		// Message catalog in .drv file


    fprintf(stderr, "DEBUG: [cups-driverd] %d locales defined in \"%s\"...\n",
            src->po_files->count, filename);

    locales = new ppdcArray();
    for (catalog = (ppdcCatalog *)src->po_files->first();
         catalog;
	 catalog = (ppdcCatalog *)src->po_files->next())
    {
      fprintf(stderr, "DEBUG: [cups-driverd] Adding locale \"%s\"...\n",
              catalog->locale->value);
      locales->add(catalog->locale);
    }

    if (request_id)
    {
      cupsdSendIPPHeader(IPP_OK, request_id);
      cupsdSendIPPGroup(IPP_TAG_OPERATION);
      cupsdSendIPPString(IPP_TAG_CHARSET, "attributes-charset", "utf-8");
      cupsdSendIPPString(IPP_TAG_LANGUAGE, "attributes-natural-language",
			 "en-US");
      cupsdSendIPPTrailer();
      fflush(stdout);
    }

    out = cupsFileStdout();
    d->write_ppd_file(out, NULL, locales, src, PPDC_LFONLY);
    cupsFileClose(out);

    locales->release();
  }
  else
  {
    fprintf(stderr, "ERROR: PPD \"%s\" not found!\n", name);

    if (request_id)
    {
      snprintf(message, sizeof(message), "PPD \"%s\" not found!", name);

      cupsdSendIPPHeader(IPP_NOT_FOUND, request_id);
      cupsdSendIPPGroup(IPP_TAG_OPERATION);
      cupsdSendIPPString(IPP_TAG_CHARSET, "attributes-charset", "utf-8");
      cupsdSendIPPString(IPP_TAG_LANGUAGE, "attributes-natural-language",
			 "en-US");
      cupsdSendIPPString(IPP_TAG_TEXT, "status-message", message);
      cupsdSendIPPTrailer();
    }
  }

  src->release();

  return (!d);
}


/*
 * 'cat_ppd()' - Copy a PPD file to stdout.
 */

static int				/* O - Exit code */
cat_ppd(const char *name,		/* I - PPD name */
        int        request_id)		/* I - Request ID for response? */
{
  char		scheme[256],		/* Scheme from PPD name */
		*sptr,			/* Pointer into scheme */
		line[1024],		/* Line/filename */
		message[2048];		/* status-message */


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

  if (request_id > 0)
    puts("Content-Type: application/ipp\n");

  if (!scheme[0])
    return (cat_static(name, request_id));
  else if (!strcmp(scheme, "drv"))
    return (cat_drv(name, request_id));
  else
  {
   /*
    * Dynamic PPD, see if we have a driver program to support it...
    */

    const char	*serverbin;		/* CUPS_SERVERBIN env var */
    char	*argv[4];		/* Arguments for program */


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

    argv[0] = scheme;
    argv[1] = (char *)"cat";
    argv[2] = (char *)name;
    argv[3] = NULL;

    if (cupsdExec(line, argv))
    {
     /*
      * Unable to execute driver...
      */

      fprintf(stderr, "ERROR: [cups-driverd] Unable to execute \"%s\" - %s\n",
              line, strerror(errno));
      return (1);
    }
  }

 /*
  * Return with no errors...
  */

  return (0);
}


/*
 * 'copy_static()' - Copy a static PPD file to stdout.
 */

static int				/* O - Exit code */
cat_static(const char *name,		/* I - PPD name */
           int        request_id)	/* I - Request ID for response? */
{
  cups_file_t	*fp;			/* PPD file */
  const char	*datadir;		/* CUPS_DATADIR env var */
  char		line[1024],		/* Line/filename */
		message[2048];		/* status-message */


  if (name[0] == '/' || strstr(name, "../") || strstr(name, "/.."))
  {
   /*
    * Bad name...
    */

    fprintf(stderr, "ERROR: [cups-driverd] Bad PPD name \"%s\"!\n", name);

    if (request_id)
    {
      snprintf(message, sizeof(message), "Bad PPD name \"%s\"!", name);

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

  return (0);
}


/*
 * 'compare_matches()' - Compare PPD match scores for sorting.
 */

static int
compare_matches(const ppd_info_t *p0,	/* I - First PPD */
                const ppd_info_t *p1)	/* I - Second PPD */
{
  if (p1->matches != p0->matches)
    return (p1->matches - p0->matches);
  else
    return (cupsdCompareNames(p0->record.make_and_model,
			      p1->record.make_and_model));
}


/*
 * 'compare_names()' - Compare PPD filenames for sorting.
 */

static int				/* O - Result of comparison */
compare_names(const ppd_info_t *p0,	/* I - First PPD file */
              const ppd_info_t *p1)	/* I - Second PPD file */
{
  int	diff;				/* Difference between strings */


  if ((diff = strcasecmp(p0->record.filename, p1->record.filename)) != 0)
    return (diff);
  else
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
  int		i;			/* Looping vars */
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
  cups_array_t	*requested,		/* requested-attributes values */
		*include,		/* PPD schemes to include */
		*exclude;		/* PPD schemes to exclude */
  const char	*device_id,		/* ppd-device-id option */
		*language,		/* ppd-natural-language option */
		*make,			/* ppd-make option */
		*make_and_model,	/* ppd-make-and-model option */
		*model_number_str,	/* ppd-model-number option */
		*product,		/* ppd-product option */
		*psversion,		/* ppd-psversion option */
		*type_str;		/* ppd-type option */
  int		model_number,		/* ppd-model-number value */
		type,			/* ppd-type value */
		make_and_model_len,	/* Length of ppd-make-and-model */
		product_len,		/* Length of ppd-product */
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
  regex_t	*device_id_re,		/* Regular expression for matching device ID */
		*make_and_model_re;	/* Regular expression for matching make and model */
  regmatch_t	re_matches[6];		/* Regular expression matches */
  cups_array_t	*matches;		/* Matching PPDs */


  fprintf(stderr,
          "DEBUG2: [cups-driverd] list_ppds(request_id=%d, limit=%d, "
          "opt=\"%s\"\n", request_id, limit, opt);

 /*
  * See if we a PPD database file...
  */

  PPDsByName      = cupsArrayNew((cups_array_func_t)compare_names, NULL);
  PPDsByMakeModel = cupsArrayNew((cups_array_func_t)compare_ppds, NULL);
  ChangedPPD      = 0;

  if ((cups_cachedir = getenv("CUPS_CACHEDIR")) == NULL)
    cups_cachedir = CUPS_CACHEDIR;

  snprintf(filename, sizeof(filename), "%s/ppds.dat", cups_cachedir);
  if ((fp = cupsFileOpen(filename, "r")) != NULL)
  {
   /*
    * See if we have the right sync word...
    */

    unsigned ppdsync;			/* Sync word */
    int      num_ppds;			/* Number of PPDs */


    if (cupsFileRead(fp, (char *)&ppdsync, sizeof(ppdsync))
            == sizeof(ppdsync) &&
        ppdsync == PPD_SYNC &&
        !stat(filename, &fileinfo) &&
	((fileinfo.st_size - sizeof(ppdsync)) % sizeof(ppd_rec_t)) == 0 &&
	(num_ppds = (fileinfo.st_size - sizeof(ppdsync)) /
	            sizeof(ppd_rec_t)) > 0)
    {
     /*
      * We have a ppds.dat file, so read it!
      */

      for (; num_ppds > 0; num_ppds --)
      {
	if ((ppd = (ppd_info_t *)calloc(1, sizeof(ppd_info_t))) == NULL)
	{
	  fputs("ERROR: [cups-driverd] Unable to allocate memory for PPD!\n",
	        stderr);
	  exit(1);
	}

	if (cupsFileRead(fp, (char *)&(ppd->record), sizeof(ppd_rec_t)) > 0)
	{
	  cupsArrayAdd(PPDsByName, ppd);
	  cupsArrayAdd(PPDsByMakeModel, ppd);
	}
	else
	{
	  free(ppd);
	  break;
	}
      }

      fprintf(stderr, "INFO: [cups-driverd] Read \"%s\", %d PPDs...\n",
	      filename, cupsArrayCount(PPDsByName));
    }

    cupsFileClose(fp);
  }
  
 /*
  * Load all PPDs in the specified directory and below...
  */

  if ((cups_datadir = getenv("CUPS_DATADIR")) == NULL)
    cups_datadir = CUPS_DATADIR;

  snprintf(model, sizeof(model), "%s/model", cups_datadir);
  load_ppds(model, "", 1);

  snprintf(model, sizeof(model), "%s/drv", cups_datadir);
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

  if (!access("/usr/local/share/ppd", 0))
    load_ppds("/usr/local/share/ppd", "lsb/local", 1);
  if (!access("/usr/share/ppd", 0))
    load_ppds("/usr/share/ppd", "lsb/usr", 1);
  if (!access("/opt/share/ppd", 0))
    load_ppds("/opt/share/ppd", "lsb/opt", 1);
#endif /* __APPLE__ */

 /*
  * Cull PPD files that are no longer present...
  */

  for (ppd = (ppd_info_t *)cupsArrayFirst(PPDsByName);
       ppd;
       ppd = (ppd_info_t *)cupsArrayNext(PPDsByName))
    if (!ppd->found)
    {
     /*
      * Remove this PPD file from the list...
      */

      cupsArrayRemove(PPDsByName, ppd);
      cupsArrayRemove(PPDsByMakeModel, ppd);
      free(ppd);
    }

 /*
  * Write the new ppds.dat file...
  */

  if (ChangedPPD)
  {
    if ((fp = cupsFileOpen(filename, "w")) != NULL)
    {
      unsigned ppdsync = PPD_SYNC;	/* Sync word */


      cupsFileWrite(fp, (char *)&ppdsync, sizeof(ppdsync));

      for (ppd = (ppd_info_t *)cupsArrayFirst(PPDsByName);
	   ppd;
	   ppd = (ppd_info_t *)cupsArrayNext(PPDsByName))
	cupsFileWrite(fp, (char *)&(ppd->record), sizeof(ppd_rec_t));

      cupsFileClose(fp);

      fprintf(stderr, "INFO: [cups-driverd] Wrote \"%s\", %d PPDs...\n",
              filename, cupsArrayCount(PPDsByName));
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

  num_options = cupsParseOptions(opt, 0, &options);
  exclude     = cupsdCreateStringsArray(cupsGetOption("exclude-schemes",
                                                      num_options, options));
  include     = cupsdCreateStringsArray(cupsGetOption("exclude-schemes",
						      num_options, options));

  load_drivers(include, exclude);

 /*
  * Add the raw driver...
  */

  add_ppd("", "raw", "en", "Raw", "Raw Queue", "", "", "", 0, 0, 0,
          PPD_TYPE_UNKNOWN, "raw");

 /*
  * Send IPP attributes...
  */

  requested        = cupsdCreateStringsArray(
                         cupsGetOption("requested-attributes", num_options,
			               options));
  device_id        = cupsGetOption("ppd-device-id", num_options, options);
  language         = cupsGetOption("ppd-natural-language", num_options, options);
  make             = cupsGetOption("ppd-make", num_options, options);
  make_and_model   = cupsGetOption("ppd-make-and-model", num_options, options);
  model_number_str = cupsGetOption("ppd-model-number", num_options, options);
  product          = cupsGetOption("ppd-product", num_options, options);
  psversion        = cupsGetOption("ppd-psversion", num_options, options);
  type_str         = cupsGetOption("ppd-type", num_options, options);

  if (make_and_model)
    make_and_model_len = strlen(make_and_model);
  else
    make_and_model_len = 0;

  if (product)
    product_len = strlen(product);
  else
    product_len = 0;

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
  else
    type = 0;

  for (i = 0; i < num_options; i ++)
    fprintf(stderr, "DEBUG: [cups-driverd] %s=\"%s\"\n", options[i].name,
            options[i].value);

  if (!requested || cupsArrayFind(requested, (void *)"all") != NULL)
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
    send_name             = cupsArrayFind(requested,
                                          (void *)"ppd-name") != NULL;
    send_make             = cupsArrayFind(requested,
                                          (void *)"ppd-make") != NULL;
    send_make_and_model   = cupsArrayFind(requested,
                                          (void *)"ppd-make-and-model") != NULL;
    send_model_number     = cupsArrayFind(requested,
                                          (void *)"ppd-model-number") != NULL;
    send_natural_language = cupsArrayFind(requested,
                                          (void *)"ppd-natural-language") != NULL;
    send_device_id        = cupsArrayFind(requested,
                                          (void *)"ppd-device-id") != NULL;
    send_product          = cupsArrayFind(requested,
                                          (void *)"ppd-product") != NULL;
    send_psversion        = cupsArrayFind(requested,
                                          (void *)"ppd-psversion") != NULL;
    send_type             = cupsArrayFind(requested,
                                          (void *)"ppd-type") != NULL;
  }

  puts("Content-Type: application/ipp\n");

  sent_header = 0;

  if (limit <= 0 || limit > cupsArrayCount(PPDsByMakeModel))
    count = cupsArrayCount(PPDsByMakeModel);
  else
    count = limit;

  if (device_id || language || make || make_and_model || model_number_str ||
      product || include || exclude)
  {
    matches = cupsArrayNew((cups_array_func_t)compare_matches, NULL);

    if (device_id)
      device_id_re = regex_device_id(device_id);
    else
      device_id_re = NULL;

    if (make_and_model)
      make_and_model_re = regex_string(make_and_model);
    else
      make_and_model_re = NULL;

    for (ppd = (ppd_info_t *)cupsArrayFirst(PPDsByMakeModel);
	 ppd;
	 ppd = (ppd_info_t *)cupsArrayNext(PPDsByMakeModel))
    {
     /*
      * Filter PPDs based on make, model, product, language, model number,
      * and/or device ID using the "matches" score value.  An exact match
      * for product, make-and-model, or device-id adds 3 to the score.
      * Partial matches for make-and-model yield 1 or 2 points, and matches
      * for the make and language add a single point.  Results are then sorted
      * by score, highest score first.
      */

      if (ppd->record.type < PPD_TYPE_POSTSCRIPT ||
	  ppd->record.type >= PPD_TYPE_DRV)
	continue;

      if (cupsArrayFind(exclude, ppd->record.scheme) ||
          (include && !cupsArrayFind(include, ppd->record.scheme)))
        continue;

      ppd->matches = 0;

      if (device_id_re &&
	  !regexec(device_id_re, ppd->record.device_id,
                   (int)(sizeof(re_matches) / sizeof(re_matches[0])),
		   re_matches, 0))
      {
       /*
        * Add the number of matching values from the device ID - it will be
	* at least 2 (manufacturer and model), and as much as 3 (command set).
	*/

        for (i = 1; i < (int)(sizeof(re_matches) / sizeof(re_matches[0])); i ++)
	  if (re_matches[i].rm_so >= 0)
	    ppd->matches ++;
      }

      if (language)
      {
	for (i = 0; i < PPD_MAX_LANG; i ++)
	  if (!ppd->record.languages[i][0] ||
	      !strcasecmp(ppd->record.languages[i], language))
	  {
	    ppd->matches ++;
	    break;
	  }
      }

      if (make && !strcasecmp(ppd->record.make, make))
        ppd->matches ++;

      if (make_and_model_re &&
          !regexec(make_and_model_re, ppd->record.make_and_model,
	           (int)(sizeof(re_matches) / sizeof(re_matches[0])),
		   re_matches, 0))
      {
	// See how much of the make-and-model string we matched...
	if (re_matches[0].rm_so == 0)
	{
	  if (re_matches[0].rm_eo == make_and_model_len)
	    ppd->matches += 3;		// Exact match
	  else
	    ppd->matches += 2;		// Prefix match
	}
	else
	  ppd->matches ++;		// Infix match
      }

      if (model_number_str && ppd->record.model_number == model_number)
        ppd->matches ++;

      if (product)
      {
	for (i = 0; i < PPD_MAX_PROD; i ++)
	  if (!ppd->record.products[i][0] ||
	      !strcasecmp(ppd->record.products[i], product))
	  {
	    ppd->matches += 3;
	    break;
	  }
      }

      if (psversion)
      {
	for (i = 0; i < PPD_MAX_VERS; i ++)
	  if (!ppd->record.psversions[i][0] ||
	      !strcasecmp(ppd->record.psversions[i], psversion))
	  {
	    ppd->matches ++;
	    break;
	  }
      }

      if (type_str && ppd->record.type == type)
        ppd->matches ++;

      if (ppd->matches)
      {
        fprintf(stderr, "DEBUG: [cups-driverd] %s matches with score %d!\n",
	        ppd->record.name, ppd->matches);
        cupsArrayAdd(matches, ppd);
      }
    }
  }
  else
    matches = PPDsByMakeModel;

  for (ppd = (ppd_info_t *)cupsArrayFirst(matches);
       count > 0 && ppd;
       ppd = (ppd_info_t *)cupsArrayNext(matches))
  {
   /*
    * Skip invalid PPDs...
    */

    if (ppd->record.type < PPD_TYPE_POSTSCRIPT ||
        ppd->record.type >= PPD_TYPE_DRV)
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
      cupsdSendIPPString(IPP_TAG_LANGUAGE, "attributes-natural-language",
                         "en-US");
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

      for (i = 1; i < PPD_MAX_LANG && ppd->record.languages[i][0]; i ++)
	cupsdSendIPPString(IPP_TAG_LANGUAGE, "", ppd->record.languages[i]);
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

      for (i = 1; i < PPD_MAX_PROD && ppd->record.products[i][0]; i ++)
	cupsdSendIPPString(IPP_TAG_TEXT, "", ppd->record.products[i]);
    }

    if (send_psversion)
    {
      cupsdSendIPPString(IPP_TAG_TEXT, "ppd-psversion",
			 ppd->record.psversions[0]);

      for (i = 1; i < PPD_MAX_VERS && ppd->record.psversions[i][0]; i ++)
	cupsdSendIPPString(IPP_TAG_TEXT, "", ppd->record.psversions[i]);
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

    if (cupsArrayFind(requested, (void *)"ppd-make") &&
        cupsArrayCount(requested) == 1)
    {
      const char	*this_make;	/* This ppd-make */


      for (this_make = ppd->record.make,
               ppd = (ppd_info_t *)cupsArrayNext(matches);
	   ppd;
	   ppd = (ppd_info_t *)cupsArrayNext(matches))
	if (strcasecmp(this_make, ppd->record.make))
	  break;

      cupsArrayPrev(matches);
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
		psversion[256],		/* PSVersion */
		temp[512];		/* Temporary make and model */
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
    { "chinese",		"zh" },
    { "czech",			"cs" },
    { "danish",			"da" },
    { "dutch",			"nl" },
    { "english",		"en" },
    { "finnish",		"fi" },
    { "french",			"fr" },
    { "german",			"de" },
    { "greek",			"el" },
    { "hungarian",		"hu" },
    { "italian",		"it" },
    { "japanese",		"ja" },
    { "korean",			"ko" },
    { "norwegian",		"no" },
    { "polish",			"pl" },
    { "portuguese",		"pt" },
    { "russian",		"ru" },
    { "simplified chinese",	"zh_CN" },
    { "slovak",			"sk" },
    { "spanish",		"es" },
    { "swedish",		"sv" },
    { "traditional chinese",	"zh_TW" },
    { "turkish",		"tr" }
  };


  fprintf(stderr, "DEBUG: [cups-driverd] Loading \"%s\"...\n", d);

  if ((dir = cupsDirOpen(d)) == NULL)
  {
    if (errno != ENOENT)
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

    strcpy(key.record.filename, name);
    strcpy(key.record.name, name);

    ppd = (ppd_info_t *)cupsArrayFind(PPDsByName, &key);

    if (ppd &&
	ppd->record.size == dent->fileinfo.st_size &&
	ppd->record.mtime == dent->fileinfo.st_mtime)
    {
      do
      {
        ppd->found = 1;
      }
      while ((ppd = (ppd_info_t *)cupsArrayNext(PPDsByName)) != NULL &&
	     !strcasecmp(ppd->record.filename, name));

      continue;
    }

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
      * Nope, treat it as a driver information file...
      */

      load_drv(filename, name, fp, dent->fileinfo.st_mtime,
               dent->fileinfo.st_size);
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
      {
	sscanf(line, "%*[^\"]\"%255[^\"]", device_id);

        // Make sure device ID ends with a semicolon...
	if (device_id[0] && device_id[strlen(device_id) - 1] != ';')
	  strlcat(device_id, ";", sizeof(device_id));
      }
      else if (!strncmp(line, "*Product:", 9))
      {
	if (sscanf(line, "%*[^\"]\"(%255[^)]", product) == 1)
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
      else if (!strncmp(line, "*cupsFilter:", 12) && type == PPD_TYPE_POSTSCRIPT)
      {
        if (strstr(line + 12, "application/vnd.cups-raster"))
	  type = PPD_TYPE_RASTER;
        else if (strstr(line + 12, "application/vnd.cups-pdf"))
	  type = PPD_TYPE_PDF;
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
    * Normalize the make and model string...
    */

    while (isspace(manufacturer[0] & 255))
      _cups_strcpy(manufacturer, manufacturer + 1);

    if (!strncasecmp(make_model, manufacturer, strlen(manufacturer)))
      strlcpy(temp, make_model, sizeof(temp));
    else
      snprintf(temp, sizeof(temp), "%s %s", manufacturer, make_model);

    _ppdNormalizeMakeAndModel(temp, make_model, sizeof(make_model));

   /*
    * See if we got a manufacturer...
    */

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
      else
	strcpy(manufacturer, "Other");
    }
    else if (!strncasecmp(manufacturer, "LHAG", 4) ||
             !strncasecmp(manufacturer, "linotype", 8))
      strcpy(manufacturer, "LHAG");
    else if (!strncasecmp(manufacturer, "Hewlett", 7))
      strcpy(manufacturer, "HP");

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
    * Record the PPD file...
    */

    new_ppd = !ppd;

    if (new_ppd)
    {
     /*
      * Add new PPD file...
      */

      fprintf(stderr, "DEBUG: [cups-driverd] Adding ppd \"%s\"...\n", name);

      ppd = add_ppd(name, name, lang_version, manufacturer, make_model,
                    device_id, (char *)cupsArrayFirst(products),
                    (char *)cupsArrayFirst(psversions),
                    dent->fileinfo.st_mtime, dent->fileinfo.st_size,
		    model_number, type, "file");

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
 * 'load_drv()' - Load the PPDs from a driver information file.
 */

static int				/* O - 1 on success, 0 on failure */
load_drv(const char  *filename,		/* I - Actual filename */
         const char  *name,		/* I - Name to the rest of the world */
         cups_file_t *fp,		/* I - File to read from */
	 time_t      mtime,		/* I - Mod time of driver info file */
	 off_t       size)		/* I - Size of driver info file */
{
  ppdcSource	*src;			// Driver information file
  ppdcDriver	*d;			// Current driver
  ppdcAttr	*device_id,		// 1284DeviceID attribute
		*product,		// Current product value
		*ps_version,		// PSVersion attribute
		*cups_fax,		// cupsFax attribute
		*nick_name;		// NickName attribute
  ppdcFilter	*filter;		// Current filter
  bool		product_found;		// Found product?
  char		uri[1024],		// Driver URI
		make_model[1024];	// Make and model
  int		type;			// Driver type


 /*
  * Load the driver info file...
  */

  src = new ppdcSource(filename, fp);

  if (src->drivers->count == 0)
  {
    fprintf(stderr,
            "ERROR: [cups-driverd] Bad driver information file \"%s\"!\n",
	    filename);
    src->release();
    return (0);
  }

 /*
  * Add a dummy entry for the file...
  */

  add_ppd(filename, filename, "", "", "", "", "", "", mtime, size, 0,
          PPD_TYPE_DRV, "drv");

 /*
  * Then the drivers in the file...
  */

  for (d = (ppdcDriver *)src->drivers->first();
       d;
       d = (ppdcDriver *)src->drivers->next())
  {
    httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "drv", "", "", 0,
                     "/%s/%s", name,
		     d->file_name ? d->file_name->value :
		                    d->pc_file_name->value);

    device_id  = d->find_attr("1284DeviceID", NULL);
    ps_version = d->find_attr("PSVersion", NULL);
    nick_name  = d->find_attr("NickName", NULL);

    if (nick_name)
      strlcpy(make_model, nick_name->value->value, sizeof(make_model));
    else if (strncasecmp(d->model_name->value, d->manufacturer->value,
                         strlen(d->manufacturer->value)))
      snprintf(make_model, sizeof(make_model), "%s %s, %s",
               d->manufacturer->value, d->model_name->value,
	       d->version->value);
    else
      snprintf(make_model, sizeof(make_model), "%s, %s", d->model_name->value,
               d->version->value);

    if ((cups_fax = d->find_attr("cupsFax", NULL)) != NULL &&
        !strcasecmp(cups_fax->value->value, "true"))
      type = PPD_TYPE_FAX;
    else if (d->type == PPDC_DRIVER_PS)
      type = PPD_TYPE_POSTSCRIPT;
    else if (d->type != PPDC_DRIVER_CUSTOM)
      type = PPD_TYPE_RASTER;
    else
    {
      for (filter = (ppdcFilter *)d->filters->first(),
               type = PPD_TYPE_POSTSCRIPT;
	   filter;
	   filter = (ppdcFilter *)d->filters->next())
        if (strcasecmp(filter->mime_type->value, "application/vnd.cups-raster"))
	  type = PPD_TYPE_RASTER;
        else if (strcasecmp(filter->mime_type->value,
	                    "application/vnd.cups-pdf"))
	  type = PPD_TYPE_PDF;
    }

    for (product = (ppdcAttr *)d->attrs->first(), product_found = false;
         product;
	 product = (ppdcAttr *)d->attrs->next())
      if (!strcmp(product->name->value, "Product"))
      {
        product_found = true;

	add_ppd(filename, uri, "en", d->manufacturer->value, make_model,
		device_id ? device_id->value->value : "",
		product->value->value,
		ps_version ? ps_version->value->value : "(3010) 0",
		mtime, size, d->model_number, type, "drv");
      }

    if (!product_found)
      add_ppd(filename, uri, "en", d->manufacturer->value, make_model,
	      device_id ? device_id->value->value : "",
	      d->model_name->value,
	      ps_version ? ps_version->value->value : "(3010) 0",
	      mtime, size, d->model_number, type, "drv");
  }

  src->release();

  return (1);
}


/*
 * 'load_drivers()' - Load driver-generated PPD files.
 */

static int				/* O - 1 on success, 0 on failure */
load_drivers(cups_array_t *include,	/* I - Drivers to include */
             cups_array_t *exclude)	/* I - Drivers to exclude */
{
  int		i;			/* Looping var */
  char		*start,			/* Start of value */
		*ptr;			/* Pointer into string */
  const char	*server_bin;		/* CUPS_SERVERBIN env variable */
  char		drivers[1024];		/* Location of driver programs */
  int		pid;			/* Process ID for driver program */
  cups_file_t	*fp;			/* Pipe to driver program */
  cups_dir_t	*dir;			/* Directory pointer */
  cups_dentry_t *dent;			/* Directory entry */
  char		*argv[3],		/* Arguments for command */
		filename[1024],		/* Name of driver */
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

  argv[1] = (char *)"list";
  argv[2] = NULL;

  while ((dent = cupsDirRead(dir)) != NULL)
  {
   /*
    * Only look at executable files...
    */

    if (!(dent->fileinfo.st_mode & 0111) || !S_ISREG(dent->fileinfo.st_mode))
      continue;

   /*
    * Include/exclude specific drivers...
    */

    if (cupsArrayFind(exclude, dent->filename) ||
	(include && !cupsArrayFind(include, dent->filename)))
      continue;

   /*
    * Run the driver with no arguments and collect the output...
    */

    argv[0] = dent->filename;
    snprintf(filename, sizeof(filename), "%s/%s", drivers, dent->filename);

    if ((fp = cupsdPipeCommand(&pid, filename, argv, 0)) != NULL)
    {
      while (cupsFileGets(fp, line, sizeof(line)))
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
	    fprintf(stderr,
	            "ERROR: [cups-driverd] Bad ppd-type \"%s\" ignored!\n",
        	    type_str);
	    type = PPD_TYPE_UNKNOWN;
	  }

          ppd = add_ppd("", name, languages, make, make_and_model, device_id,
	                product, psversion, 0, 0, 0, type, dent->filename);

          if (!ppd)
	  {
            cupsDirClose(dir);
	    cupsFileClose(fp);
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

      cupsFileClose(fp);
    }
    else
      fprintf(stderr, "WARNING: [cups-driverd] Unable to execute \"%s\": %s\n",
              filename, strerror(errno));
  }

  cupsDirClose(dir);

  return (1);
}


/*
 * 'regex_device_id()' - Compile a regular expression based on the 1284 device
 *                       ID.
 */

static regex_t *			/* O - Regular expression */
regex_device_id(const char *device_id)	/* I - IEEE-1284 device ID */
{
  char		res[2048],		/* Regular expression string */
		*ptr;			/* Pointer into string */
  regex_t	*re;			/* Regular expression */
  int		cmd;			/* Command set string? */


  fprintf(stderr, "DEBUG: [cups-driverd] regex_device_id(\"%s\")\n", device_id);

 /*
  * Scan the device ID string and insert class, command set, manufacturer, and
  * model attributes to match.  We assume that the device ID in the PPD and the
  * device ID reported by the device itself use the same attribute names and
  * order of attributes.
  */

  ptr = res;

  while (*device_id && ptr < (res + sizeof(res) - 6))
  {
    cmd = !strncasecmp(device_id, "COMMAND SET:", 12) ||
          !strncasecmp(device_id, "CMD:", 4);

    if (cmd || !strncasecmp(device_id, "MANUFACTURER:", 13) ||
        !strncasecmp(device_id, "MFG:", 4) ||
        !strncasecmp(device_id, "MFR:", 4) ||
        !strncasecmp(device_id, "MODEL:", 6) ||
        !strncasecmp(device_id, "MDL:", 4))
    {
      if (ptr > res)
      {
        *ptr++ = '.';
	*ptr++ = '*';
      }

      *ptr++ = '(';

      while (*device_id && *device_id != ';' && ptr < (res + sizeof(res) - 4))
      {
        if (strchr("[]{}().*\\|", *device_id))
	  *ptr++ = '\\';
	*ptr++ = *device_id++;
      }

      if (*device_id == ';' || !*device_id)
        *ptr++ = ';';
      *ptr++ = ')';
      if (cmd)
        *ptr++ = '?';
    }
    else if ((device_id = strchr(device_id, ';')) == NULL)
      break;
    else
      device_id ++;
  }

  *ptr = '\0';

  fprintf(stderr, "DEBUG: [cups-driverd] regex_device_id: \"%s\"\n", res);

 /*
  * Compile the regular expression and return...
  */

  if (res[0] && (re = (regex_t *)calloc(1, sizeof(regex_t))) != NULL)
  {
    if (!regcomp(re, res, REG_EXTENDED | REG_ICASE))
    {
      fputs("DEBUG: [cups-driverd] regex_device_id: OK\n", stderr);
      return (re);
    }

    free(re);
  }

  return (NULL);
}


/*
 * 'regex_string()' - Construct a regular expression to compare a simple string.
 */

static regex_t *			/* O - Regular expression */
regex_string(const char *s)		/* I - String to compare */
{
  char		res[2048],		/* Regular expression string */
		*ptr;			/* Pointer into string */
  regex_t	*re;			/* Regular expression */


  fprintf(stderr, "DEBUG: [cups-driverd] regex_string(\"%s\")\n", s);

 /*
  * Convert the string to a regular expression, escaping special characters
  * as needed.
  */

  ptr = res;

  while (*s && ptr < (res + sizeof(res) - 2))
  {
    if (strchr("[]{}().*\\", *s))
      *ptr++ = '\\';

    *ptr++ = *s++;
  }

  *ptr = '\0';

  fprintf(stderr, "DEBUG: [cups-driverd] regex_string: \"%s\"\n", res);

 /*
  * Create a case-insensitive regular expression...
  */

  if (res[0] && (re = (regex_t *)calloc(1, sizeof(regex_t))) != NULL)
  {
    if (!regcomp(re, res, REG_ICASE))
    {
      fputs("DEBUG: [cups-driverd] regex_string: OK\n", stderr);
      return (re);
    }

    free(re);
  }

  return (NULL);
}


/*
 * End of "$Id$".
 */
