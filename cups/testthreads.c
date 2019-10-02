/*
 * Threaded test program for CUPS.
 *
 * Copyright © 2012-2019 by Apple Inc.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more
 * information.
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <errno.h>
#include <cups/cups.h>
#include <cups/thread-private.h>


/*
 * Local functions...
 */

static int	enum_dests_cb(void *_name, unsigned flags, cups_dest_t *dest);
static void	*run_query(cups_dest_t *dest);
static void	show_supported(http_t *http, cups_dest_t *dest, cups_dinfo_t *dinfo, const char *option, const char *value);


/*
 * 'main()' - Main entry.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
 /*
  * Go through all the available destinations to find the requested one...
  */

  (void)argc;

  cupsEnumDests(CUPS_DEST_FLAGS_NONE, -1, NULL, 0, 0, enum_dests_cb, argv[1]);

  return (0);
}


/*
 * 'enum_dests_cb()' - Destination enumeration function...
 */

static int				/* O - 1 to continue, 0 to stop */
enum_dests_cb(void        *_name,	/* I - Printer name, if any */
              unsigned    flags,	/* I - Enumeration flags */
              cups_dest_t *dest)	/* I - Found destination */
{
  const char		*name = (const char *)_name;
					/* Printer name */
  cups_dest_t		*cdest;		/* Copied destination */


  (void)flags;

 /*
  * If a name was specified, compare it...
  */

  if (name && strcasecmp(name, dest->name))
    return (1);				/* Continue */

 /*
  * Copy the destination and run the query on a separate thread...
  */

  cupsCopyDest(dest, 0, &cdest);
  _cupsThreadWait(_cupsThreadCreate((_cups_thread_func_t)run_query, cdest));

  cupsFreeDests(1, cdest);

 /*
  * Continue if no name was specified or the name matches...
  */

  return (!name || !strcasecmp(name, dest->name));
}


/*
 * 'run_query()' - Query printer capabilities on a separate thread.
 */

static void *				/* O - Return value (not used) */
run_query(cups_dest_t *dest)		/* I - Destination to query */
{
  http_t	*http;			/* Connection to destination */
  cups_dinfo_t	*dinfo;			/* Destination info */
  unsigned	dflags = CUPS_DEST_FLAGS_NONE;
					/* Destination flags */


  if ((http = cupsConnectDest(dest, dflags, 300, NULL, NULL, 0, NULL, NULL)) == NULL)
  {
    printf("testthreads: Unable to connect to destination \"%s\": %s\n", dest->name, cupsLastErrorString());
    return (NULL);
  }

  if ((dinfo = cupsCopyDestInfo(http, dest)) == NULL)
  {
    printf("testdest: Unable to get information for destination \"%s\": %s\n", dest->name, cupsLastErrorString());
    return (NULL);
  }

  printf("\n%s:\n", dest->name);

  show_supported(http, dest, dinfo, NULL, NULL);

  return (NULL);
}



/*
 * 'show_supported()' - Show supported options, values, etc.
 */

static void
show_supported(http_t       *http,	/* I - Connection to destination */
	       cups_dest_t  *dest,	/* I - Destination */
	       cups_dinfo_t *dinfo,	/* I - Destination information */
	       const char   *option,	/* I - Option, if any */
	       const char   *value)	/* I - Value, if any */
{
  ipp_attribute_t	*attr;		/* Attribute */
  int			i,		/* Looping var */
			count;		/* Number of values */


  if (!option)
  {
    attr = cupsFindDestSupported(http, dest, dinfo, "job-creation-attributes");
    if (attr)
    {
      count = ippGetCount(attr);
      for (i = 0; i < count; i ++)
        show_supported(http, dest, dinfo, ippGetString(attr, i, NULL), NULL);
    }
    else
    {
      static const char * const options[] =
      {					/* List of standard options */
        CUPS_COPIES,
	CUPS_FINISHINGS,
	CUPS_MEDIA,
	CUPS_NUMBER_UP,
	CUPS_ORIENTATION,
	CUPS_PRINT_COLOR_MODE,
	CUPS_PRINT_QUALITY,
	CUPS_SIDES
      };

      puts("    No job-creation-attributes-supported attribute, probing instead.");

      for (i = 0; i < (int)(sizeof(options) / sizeof(options[0])); i ++)
        if (cupsCheckDestSupported(http, dest, dinfo, options[i], NULL))
	  show_supported(http, dest, dinfo, options[i], NULL);
    }
  }
  else if (!value)
  {
    printf("    %s (%s - %s)\n", option, cupsLocalizeDestOption(http, dest, dinfo, option), cupsCheckDestSupported(http, dest, dinfo, option, NULL) ? "supported" : "not-supported");

    if ((attr = cupsFindDestSupported(http, dest, dinfo, option)) != NULL)
    {
      count = ippGetCount(attr);

      switch (ippGetValueTag(attr))
      {
        case IPP_TAG_INTEGER :
	    for (i = 0; i < count; i ++)
              printf("        %d\n", ippGetInteger(attr, i));
	    break;

        case IPP_TAG_ENUM :
	    for (i = 0; i < count; i ++)
	    {
	      int val = ippGetInteger(attr, i);
	      char valstr[256];

              snprintf(valstr, sizeof(valstr), "%d", val);
              printf("        %s (%s)\n", ippEnumString(option, ippGetInteger(attr, i)), cupsLocalizeDestValue(http, dest, dinfo, option, valstr));
            }
	    break;

        case IPP_TAG_RANGE :
	    for (i = 0; i < count; i ++)
	    {
	      int upper, lower = ippGetRange(attr, i, &upper);

              printf("        %d-%d\n", lower, upper);
	    }
	    break;

        case IPP_TAG_RESOLUTION :
	    for (i = 0; i < count; i ++)
	    {
	      int xres, yres;
	      ipp_res_t units;
	      xres = ippGetResolution(attr, i, &yres, &units);

              if (xres == yres)
                printf("        %d%s\n", xres, units == IPP_RES_PER_INCH ? "dpi" : "dpcm");
	      else
                printf("        %dx%d%s\n", xres, yres, units == IPP_RES_PER_INCH ? "dpi" : "dpcm");
	    }
	    break;

	case IPP_TAG_KEYWORD :
	    for (i = 0; i < count; i ++)
              printf("        %s (%s)\n", ippGetString(attr, i, NULL), cupsLocalizeDestValue(http, dest, dinfo, option, ippGetString(attr, i, NULL)));
	    break;

	case IPP_TAG_TEXTLANG :
	case IPP_TAG_NAMELANG :
	case IPP_TAG_TEXT :
	case IPP_TAG_NAME :
	case IPP_TAG_URI :
	case IPP_TAG_URISCHEME :
	case IPP_TAG_CHARSET :
	case IPP_TAG_LANGUAGE :
	case IPP_TAG_MIMETYPE :
	    for (i = 0; i < count; i ++)
              printf("        %s\n", ippGetString(attr, i, NULL));
	    break;

        case IPP_TAG_STRING :
	    for (i = 0; i < count; i ++)
	    {
	      int j, len;
	      unsigned char *data = ippGetOctetString(attr, i, &len);

              fputs("        ", stdout);
	      for (j = 0; j < len; j ++)
	      {
	        if (data[j] < ' ' || data[j] >= 0x7f)
		  printf("<%02X>", data[j]);
		else
		  putchar(data[j]);
              }
              putchar('\n');
	    }
	    break;

        case IPP_TAG_BOOLEAN :
	    break;

        default :
	    printf("        %s\n", ippTagString(ippGetValueTag(attr)));
	    break;
      }
    }

  }
  else if (cupsCheckDestSupported(http, dest, dinfo, option, value))
    puts("YES");
  else
    puts("NO");
}
