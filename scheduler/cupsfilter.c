/*
 * "$Id$"
 *
 *   CUPS filtering program for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007 by Apple Inc.
 *   Copyright 1997-2006 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
 *   main()            - Main entry for the test program.
 *   escape_options()  - Convert an options array to a string.
 *   exec_filter()     - Execute a single filter.
 *   exec_filters()    - Execute filters for the given file and options.
 *   read_cupsd_conf() - Read the cupsd.conf file to get the filter settings.
 *   set_string()      - Copy and set a string.
 *   usage()           - Show program usage...
 */

/*
 * Include necessary headers...
 */

#include <cups/cups.h>
#include <cups/i18n.h>
#include <cups/string.h>
#include "mime.h"
#include <stdlib.h>
#include <unistd.h>


/*
 * Local globals...
 */

static char		*DataDir = NULL;/* CUPS_DATADIR environment variable */
static char		*FontPath = NULL;
					/* CUPS_FONTPATH environment variable */
static mime_filter_t	GZIPFilter =	/* gziptoany filter */
{
  NULL,					/* Source type */
  NULL,					/* Destination type */
  0,					/* Cost */
  "gziptoany"				/* Filter program to run */
};
static char		*Path = NULL;	/* PATH environment variable */
static char		*PPD = NULL;	/* PPD environment variable */
static char		*ServerBin = NULL;
					/* CUPS_SERVERBIN environment variable */
static char		*ServerRoot = NULL;
					/* CUPS_SERVERROOT environment variable */
static char		*RIPCache = NULL;
					/* RIP_CACHE environment variable */


/*
 * Local functions...
 */

static char	*escape_options(int num_options, cups_option_t *options);
static int	exec_filter(mime_filter_t *filter, char **argv, char **envp,
		            int infd, int outfd);
static int	exec_filters(cups_array_t *filters, const char *filename,
		             const char *title, int num_options,
			     cups_option_t *options);
static int	read_cupsd_conf(const char *filename);
static void	set_string(char **s, const char *val);
static void	usage(const char *opt);


/*
 * 'main()' - Main entry for the test program.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  int		i;			/* Looping vars */
  const char	*opt;			/* Current option */
  char		super[MIME_MAX_SUPER],	/* Super-type name */
		type[MIME_MAX_TYPE];	/* Type name */
  int		compression;		/* Compression of file */
  int		cost;			/* Cost of filters */
  mime_t	*mime;			/* MIME database */
  char		*filename;		/* File to filter */
  char		cupsdconf[1024];	/* cupsd.conf file */
  const char	*server_root;		/* CUPS_SERVERROOT environment variable */
  mime_type_t	*src,			/* Source type */
		*dst;			/* Destination type */
  cups_array_t	*filters;		/* Filters for the file */
  int		num_options;		/* Number of options */
  cups_option_t	*options;		/* Options */
  const char	*title;			/* Title string */


 /*
  * Setup defaults...
  */

  mime        = NULL;
  src         = NULL;
  dst         = NULL;
  filename    = NULL;
  num_options = 0;
  options     = NULL;
  title       = NULL;
  super[0]    = '\0';
  type[0]     = '\0';

  if ((server_root = getenv("CUPS_SERVERROOT")) == NULL)
    server_root = CUPS_SERVERROOT;

  snprintf(cupsdconf, sizeof(cupsdconf), "%s/cupsd.conf", server_root);

 /*
  * Process command-line arguments...
  */

  _cupsSetLocale(argv);

  for (i = 1; i < argc; i ++)
    if (argv[i][0] == '-')
    {
      for (opt = argv[i] + 1; *opt; opt ++)
        switch (*opt)
	{
	  case '-' : /* Next argument is a filename... */
	      i ++;
	      if (i < argc && !filename)
	        filename = argv[i];
	      else
	        usage(opt);
	      break;

          case 'c' : /* Specify cupsd.conf file location... */
	      i ++;
	      if (i < argc)
	        strlcpy(cupsdconf, argv[i], sizeof(cupsdconf));
	      else
	        usage(opt);
	      break;

          case 'm' : /* Specify destination MIME type... */
	      i ++;
	      if (i < argc)
	      {
	        if (sscanf(argv[i], "%15[^/]/%255s", super, type) != 2)
		  usage(opt);
	      }
	      else
	        usage(opt);
	      break;

          case 'n' : /* Specify number of copies... */
	      i ++;
	      if (i < argc)
	        num_options = cupsAddOption("copies", argv[i], num_options,
		                            &options);
	      else
	        usage(opt);
	      break;

          case 'o' : /* Specify option... */
	      i ++;
	      if (i < argc)
	        num_options = cupsParseOptions(argv[i], num_options, &options);
	      else
	        usage(opt);
	      break;

          case 't' : /* Specify number of copies... */
	      i ++;
	      if (i < argc)
	        title = argv[i];
	      else
	        usage(opt);
	      break;

	  default : /* Something we don't understand... */
	      usage(opt);
	      break;
	}
    }
    else if (!filename)
      filename = argv[i];
    else
    {
      _cupsLangPuts(stderr,
                    _("cupsfilter: Only one filename can be specified!\n"));
      usage(NULL);
    }

  if (!filename || !super[0] || !type[0])
    usage(NULL);

 /*
  * Load the cupsd.conf file and create the MIME database...
  */

  if (read_cupsd_conf(cupsdconf))
    return (1);

  if ((mime = mimeLoad(ServerRoot, Path)) == NULL)
  {
    _cupsLangPrintf(stderr,
                    _("cupsfilter: Unable to read MIME database from \"%s\"!\n"),
		    ServerRoot);
    return (1);
  }

 /*
  * Get the source and destination types...
  */

  if ((src = mimeFileType(mime, filename, filename, &compression)) == NULL)
  {
    _cupsLangPrintf(stderr,
                    _("cupsfilter: Unable to determine MIME type of \"%s\"!\n"),
		    filename);
    return (1);
  }

  if ((dst = mimeType(mime, super, type)) == NULL)
  {
    _cupsLangPrintf(stderr,
                    _("cupsfilter: Unknown destination MIME type %s/%s!\n"),
		    super, type);
    return (1);
  }

 /*
  * Figure out how to filter the file...
  */

  if (src == dst)
  {
   /*
    * Special case - no filtering needed...
    */

    filters = cupsArrayNew(NULL, NULL);
    cupsArrayAdd(filters, &GZIPFilter);
  }
  else if ((filters = mimeFilter(mime, src, dst, &cost)) == NULL)
  {
    _cupsLangPrintf(stderr,
                    _("cupsfilter: No filter to convert from %s/%s to %s/%s!\n"),
		    src->super, src->type, dst->super, dst->type);
    return (1);
  }
  else if (compression)
    cupsArrayInsert(filters, &GZIPFilter);

 /*
  * Do it!
  */

  return (exec_filters(filters, filename, title, num_options, options));
}


/*
 * 'escape_options()' - Convert an options array to a string.
 */

static char *				/* O - Option string */
escape_options(
    int           num_options,		/* I - Number of options */
    cups_option_t *options)		/* I - Options */
{
  int		i;			/* Looping var */
  cups_option_t	*option;		/* Current option */
  int		bytes;			/* Number of bytes needed */
  char		*s,			/* Option string */
		*sptr,			/* Pointer into string */
		*vptr;			/* Pointer into value */


 /*
  * Figure out the worst-case number of bytes we need for the option string.
  */

  for (i = num_options, option = options, bytes = 1; i > 0; i --, option ++)
    bytes += 2 * (strlen(option->name) + strlen(option->value)) + 2;

  s = malloc(bytes);

 /*
  * Copy the options to the string...
  */

  for (i = num_options, option = options, sptr = s; i > 0; i --, option ++)
  {
    if (sptr > s)
      *sptr++ = ' ';

    strcpy(sptr, option->name);
    sptr += strlen(sptr);
    *sptr++ = '=';

    for (vptr = option->value; *vptr;)
    {
      if (strchr("\\ \t\n", *vptr))
        *sptr++ = '\\';

      *sptr++ = *vptr++;
    }
  }

  *sptr = '\0';

  return (s);
}


/*
 * 'exec_filter()' - Execute a single filter.
 */

static int				/* O - Process ID or -1 on error */
exec_filter(mime_filter_t *filter,	/* I - Filter to execute */
            char          **argv,	/* I - Argument list */
	    char          **envp,	/* I - Environment list */
	    int           infd,		/* I - Stdin file descriptor */
	    int           outfd)	/* I - Stdout file descriptor */
{
  return (0);
}


/*
 * 'exec_filters()' - Execute filters for the given file and options.
 */

static int				/* O - 0 on success, 1 on error */
exec_filters(cups_array_t  *filters,	/* I - Array of filters to run */
             const char    *filename,	/* I - File to filter */
	     const char    *title,	/* I - Job title */
             int           num_options,	/* I - Number of filter options */
	     cups_option_t *options)	/* I - Filter options */
{
  return (0);
}


/*
 * 'read_cupsd_conf()' - Read the cupsd.conf file to get the filter settings.
 */

static int				/* O - 0 on success, 1 on error */
read_cupsd_conf(const char *filename)	/* I - File to read */
{
  return (0);
}


/*
 * 'set_string()' - Copy and set a string.
 */

static void
set_string(char       **s,		/* O - Copy of string */
           const char *val)		/* I - String to copy */
{
  if (*s)
    free(*s);

  *s = strdup(val);
}


/*
 * 'usage()' - Show program usage...
 */

static void
usage(const char *opt)			/* I - Incorrect option, if any */
{
  if (opt)
    _cupsLangPrintf(stderr, _("%s: Unknown option '%c'!\n"), *opt);

  _cupsLangPuts(stdout,
                _("Usage: cupsfilter -m mime/type [ options ] filename(s)\n"
		  "\n"
		  "Options:\n"
		  "\n"
		  "  -c cupsd.conf   Set cupsd.conf file to use\n"
		  "  -n copies       Set number of copies\n"
		  "  -o name=value   Set option(s)\n"
		  "  -t title        Set title\n"));

  exit(1);
}


/*
 * End of "$Id$".
 */
