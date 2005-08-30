/*
 * "$Id$"
 *
 *   Device scanning mini-daemon for the Common UNIX Printing System (CUPS).
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
 *   main()            - Scan for devices and return an IPP response.
 *   add_dev()         - Add a new device to the list.
 *   compare_devs()    - Compare device names for sorting.
 *   sigalrm_handler() - Handle alarm signals for backends that get hung
 */

/*
 * Include necessary headers...
 */

#include "util.h"
#include <cups/dir.h>


/*
 * Device information structure...
 */

typedef struct
{
  char	device_class[128],		/* Device class */
	device_make_and_model[128],	/* Make and model, if known */
	device_info[128],		/* Device info/description */
	device_uri[1024];		/* Device URI */
} dev_info_t;


/*
 * Local globals...
 */

static int		alarm_tripped;	/* Non-zero if alarm was tripped */
static int		num_devs,	/* Number of devices */
			alloc_devs;	/* Number of allocated entries */
static dev_info_t	*devs;		/* Device info */


/*
 * Local functions...
 */

static dev_info_t	*add_dev(const char *device_class,
			         const char *device_make_and_model,
				 const char *device_info,
				 const char *device_uri);
static int		compare_devs(const dev_info_t *p0,
			             const dev_info_t *p1);
static void		sigalrm_handler(int sig);


/*
 * 'main()' - Scan for devices and return an IPP response.
 *
 * Usage:
 *
 *    cups-deviced request_id limit options
 */

int					/* O - Exit code */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
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
  int		send_class,		/* Send device-class attribute? */
		send_info,		/* Send device-info attribute? */
		send_make_and_model,	/* Send device-make-and-model attribute? */
		send_uri;		/* Send device-uri attribute? */
  dev_info_t	*dev;			/* Current device */
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;		/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */


 /*
  * Check the command-line...
  */

  if (argc != 4)
  {
    fputs("Usage: cups-deviced request_id limit options\n", stderr);
    return (1);
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
 * 'add_dev()' - Add a new device to the list.
 */

static dev_info_t *			/* O - New device or NULL on error */
add_dev(
    const char *device_class,		/* I - Device class */
    const char *device_make_and_model,	/* I - Device make and model */
    const char *device_info,		/* I - Device information */
    const char *device_uri)		/* I - Device URI */
{
  dev_info_t	*dev;			/* New device */


  if (num_devs >= alloc_devs)
  {
   /*
    * Allocate (more) memory for the devices...
    */

    if (alloc_devs == 0)
      dev = malloc(sizeof(dev_info_t) * 16);
    else
      dev = realloc(devs, sizeof(dev_info_t) * (alloc_devs + 16));

    if (dev == NULL)
    {
      fprintf(stderr, "ERROR: [cups-deviced] Ran out of memory for %d devices!\n",
	      alloc_devs + 16);
      return (NULL);
    }

    devs = dev;
    alloc_devs += 16;
  }

 /*
  * Add a new device at the end of the array...
  */

  dev = devs + num_devs;
  num_devs ++;

  memset(dev, 0, sizeof(dev_info_t));

 /*
  * Copy the strings and return...
  */

  strlcpy(dev->device_class, device_class, sizeof(dev->device_class));
  strlcpy(dev->device_make_and_model, device_make_and_model,
          sizeof(dev->device_make_and_model));
  strlcpy(dev->device_info, device_info, sizeof(dev->device_info));
  strlcpy(dev->device_uri, device_uri, sizeof(dev->device_uri));

  return (dev);
}


/*
 * 'compare_devs()' - Compare device names for sorting.
 */

static int				/* O - Result of comparison */
compare_devs(const dev_info_t *d0,	/* I - First device */
             const dev_info_t *d1)	/* I - Second device */
{
  int		diff;			/* Difference between strings */


 /*
  * Sort devices by device-info, device-class, and device-uri...
  */

  if ((diff = cupsdCompareNames(d0->device_info, d1->device_info)) != 0)
    return (diff);
  else if ((diff = strcasecmp(d0->device_class, d1->device_class)) != 0)
    return (diff);
  else
    return (strcasecmp(d0->device_uri, d1->device_uri));
}


/*
 * 'sigalrm_handler()' - Handle alarm signals for backends that get hung
 *                       trying to list the available devices...
 */

static void
sigalrm_handler(int sig)		/* I - Signal number */
{
  (void)sig;	/* remove compiler warnings... */

  alarm_tripped = 1;
}


/*
 * End of "$Id$".
 */
