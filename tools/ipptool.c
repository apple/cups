/*
 * ipptool command for CUPS.
 *
 * Copyright © 2007-2019 by Apple Inc.
 * Copyright © 1997-2007 by Easy Software Products.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more
 * information.
 */

/*
 * Include necessary headers...
 */

#include <cups/cups-private.h>
#include <regex.h>
#include <sys/stat.h>
#ifdef _WIN32
#  include <windows.h>
#  ifndef R_OK
#    define R_OK 0
#  endif /* !R_OK */
#else
#  include <signal.h>
#  include <termios.h>
#endif /* _WIN32 */
#ifndef O_BINARY
#  define O_BINARY 0
#endif /* !O_BINARY */


/*
 * Types...
 */

typedef enum _cups_transfer_e		/**** How to send request data ****/
{
  _CUPS_TRANSFER_AUTO,			/* Chunk for files, length for static */
  _CUPS_TRANSFER_CHUNKED,		/* Chunk always */
  _CUPS_TRANSFER_LENGTH			/* Length always */
} _cups_transfer_t;

typedef enum _cups_output_e		/**** Output mode ****/
{
  _CUPS_OUTPUT_QUIET,			/* No output */
  _CUPS_OUTPUT_TEST,			/* Traditional CUPS test output */
  _CUPS_OUTPUT_PLIST,			/* XML plist test output */
  _CUPS_OUTPUT_IPPSERVER,		/* ippserver attribute file output */
  _CUPS_OUTPUT_LIST,			/* Tabular list output */
  _CUPS_OUTPUT_CSV			/* Comma-separated values output */
} _cups_output_t;

typedef enum _cups_with_e		/**** WITH flags ****/
{
  _CUPS_WITH_LITERAL = 0,		/* Match string is a literal value */
  _CUPS_WITH_ALL = 1,			/* Must match all values */
  _CUPS_WITH_REGEX = 2,			/* Match string is a regular expression */
  _CUPS_WITH_HOSTNAME = 4,		/* Match string is a URI hostname */
  _CUPS_WITH_RESOURCE = 8,		/* Match string is a URI resource */
  _CUPS_WITH_SCHEME = 16		/* Match string is a URI scheme */
} _cups_with_t;

typedef struct _cups_expect_s		/**** Expected attribute info ****/
{
  int		optional,		/* Optional attribute? */
		not_expect,		/* Don't expect attribute? */
		expect_all;		/* Expect all attributes to match/not match */
  char		*name,			/* Attribute name */
		*of_type,		/* Type name */
		*same_count_as,		/* Parallel attribute name */
		*if_defined,		/* Only required if variable defined */
		*if_not_defined,	/* Only required if variable is not defined */
		*with_value,		/* Attribute must include this value */
		*with_value_from,	/* Attribute must have one of the values in this attribute */
		*define_match,		/* Variable to define on match */
		*define_no_match,	/* Variable to define on no-match */
		*define_value;		/* Variable to define with value */
  int		repeat_limit,		/* Maximum number of times to repeat */
		repeat_match,		/* Repeat test on match */
		repeat_no_match,	/* Repeat test on no match */
		with_flags,		/* WITH flags  */
		count;			/* Expected count if > 0 */
  ipp_tag_t	in_group;		/* IN-GROUP value */
} _cups_expect_t;

typedef struct _cups_status_s		/**** Status info ****/
{
  ipp_status_t	status;			/* Expected status code */
  char		*if_defined,		/* Only if variable is defined */
		*if_not_defined,	/* Only if variable is not defined */
		*define_match,		/* Variable to define on match */
		*define_no_match,	/* Variable to define on no-match */
		*define_value;		/* Variable to define with value */
  int		repeat_limit,		/* Maximum number of times to repeat */
		repeat_match,		/* Repeat the test when it does not match */
		repeat_no_match;	/* Repeat the test when it matches */
} _cups_status_t;

typedef struct _cups_testdata_s		/**** Test Data ****/
{
  /* Global Options */
  http_encryption_t encryption;		/* Encryption for connection */
  int		family;			/* Address family */
  _cups_output_t output;		/* Output mode */
  int		stop_after_include_error;
					/* Stop after include errors? */
  double	timeout;		/* Timeout for connection */
  int		validate_headers,	/* Validate HTTP headers in response? */
                verbosity;		/* Show all attributes? */

  /* Test Defaults */
  int		def_ignore_errors;	/* Default IGNORE-ERRORS value */
  _cups_transfer_t def_transfer;	/* Default TRANSFER value */
  int		def_version;		/* Default IPP version */

  /* Global State */
  http_t	*http;			/* HTTP connection to printer/server */
  cups_file_t	*outfile;		/* Output file */
  int		show_header,		/* Show the test header? */
		xml_header;		/* 1 if XML plist header was written */
  int		pass,			/* Have we passed all tests? */
		test_count,		/* Number of tests (total) */
		pass_count,		/* Number of tests that passed */
		fail_count,		/* Number of tests that failed */
		skip_count;		/* Number of tests that were skipped */

  /* Per-Test State */
  cups_array_t	*errors;		/* Errors array */
  int		prev_pass,		/* Result of previous test */
		skip_previous;		/* Skip on previous test failure? */
  char		compression[16];	/* COMPRESSION value */
  useconds_t	delay;                  /* Initial delay */
  int		num_displayed;		/* Number of displayed attributes */
  char		*displayed[200];	/* Displayed attributes */
  int		num_expects;		/* Number of expected attributes */
  _cups_expect_t expects[200],		/* Expected attributes */
		*expect,		/* Current expected attribute */
		*last_expect;		/* Last EXPECT (for predicates) */
  char		file[1024],		/* Data filename */
		file_id[1024];		/* File identifier */
  int		ignore_errors;		/* Ignore test failures? */
  char		name[1024];		/* Test name */
  useconds_t	repeat_interval;	/* Repeat interval (delay) */
  int		request_id;		/* Current request ID */
  char		resource[512];		/* Resource for request */
  int		skip_test,		/* Skip this test? */
		num_statuses;		/* Number of valid status codes */
  _cups_status_t statuses[100],		/* Valid status codes */
		*last_status;		/* Last STATUS (for predicates) */
  char		test_id[1024];		/* Test identifier */
  _cups_transfer_t transfer;		/* To chunk or not to chunk */
  int		version;		/* IPP version number to use */
} _cups_testdata_t;


/*
 * Globals...
 */

static int	Cancel = 0;		/* Cancel test? */


/*
 * Local functions...
 */

static void	add_stringf(cups_array_t *a, const char *s, ...) _CUPS_FORMAT(2, 3);
static int      compare_uris(const char *a, const char *b);
static void	copy_hex_string(char *buffer, unsigned char *data, int datalen, size_t bufsize);
static int	do_test(_ipp_file_t *f, _ipp_vars_t *vars, _cups_testdata_t *data);
static int	do_tests(const char *testfile, _ipp_vars_t *vars, _cups_testdata_t *data);
static int	error_cb(_ipp_file_t *f, _cups_testdata_t *data, const char *error);
static int      expect_matches(_cups_expect_t *expect, ipp_tag_t value_tag);
static char	*get_filename(const char *testfile, char *dst, const char *src, size_t dstsize);
static const char *get_string(ipp_attribute_t *attr, int element, int flags, char *buffer, size_t bufsize);
static void	init_data(_cups_testdata_t *data);
static char	*iso_date(const ipp_uchar_t *date);
static void	pause_message(const char *message);
static void	print_attr(cups_file_t *outfile, _cups_output_t output, ipp_attribute_t *attr, ipp_tag_t *group);
static void	print_csv(_cups_testdata_t *data, ipp_t *ipp, ipp_attribute_t *attr, int num_displayed, char **displayed, size_t *widths);
static void	print_fatal_error(_cups_testdata_t *data, const char *s, ...) _CUPS_FORMAT(2, 3);
static void	print_ippserver_attr(_cups_testdata_t *data, ipp_attribute_t *attr, int indent);
static void	print_ippserver_string(_cups_testdata_t *data, const char *s, size_t len);
static void	print_line(_cups_testdata_t *data, ipp_t *ipp, ipp_attribute_t *attr, int num_displayed, char **displayed, size_t *widths);
static void	print_xml_header(_cups_testdata_t *data);
static void	print_xml_string(cups_file_t *outfile, const char *element, const char *s);
static void	print_xml_trailer(_cups_testdata_t *data, int success, const char *message);
#ifndef _WIN32
static void	sigterm_handler(int sig);
#endif /* _WIN32 */
static int	timeout_cb(http_t *http, void *user_data);
static int	token_cb(_ipp_file_t *f, _ipp_vars_t *vars, _cups_testdata_t *data, const char *token);
static void	usage(void) _CUPS_NORETURN;
static const char *with_flags_string(int flags);
static int      with_value(_cups_testdata_t *data, cups_array_t *errors, char *value, int flags, ipp_attribute_t *attr, char *matchbuf, size_t matchlen);
static int      with_value_from(cups_array_t *errors, ipp_attribute_t *fromattr, ipp_attribute_t *attr, char *matchbuf, size_t matchlen);


/*
 * 'main()' - Parse options and do tests.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  int			i;		/* Looping var */
  int			status;		/* Status of tests... */
  char			*opt,		/* Current option */
			name[1024],	/* Name/value buffer */
			*value,		/* Pointer to value */
			filename[1024],	/* Real filename */
			testname[1024];	/* Real test filename */
  const char		*ext,		/* Extension on filename */
			*testfile;	/* Test file to use */
  int			interval,	/* Test interval in microseconds */
			repeat;		/* Repeat count */
  _cups_testdata_t	data;		/* Test data */
  _ipp_vars_t		vars;		/* Variables */
  _cups_globals_t	*cg = _cupsGlobals();
					/* Global data */


#ifndef _WIN32
 /*
  * Catch SIGINT and SIGTERM...
  */

  signal(SIGINT, sigterm_handler);
  signal(SIGTERM, sigterm_handler);
#endif /* !_WIN32 */

 /*
  * Initialize the locale and variables...
  */

  _cupsSetLocale(argv);

  init_data(&data);

  _ippVarsInit(&vars, NULL, (_ipp_ferror_cb_t)error_cb, (_ipp_ftoken_cb_t)token_cb);

  _ippVarsSet(&vars, "date-start", iso_date(ippTimeToDate(time(NULL))));

 /*
  * We need at least:
  *
  *     ipptool URI testfile
  */

  interval = 0;
  repeat   = 0;
  status   = 0;
  testfile = NULL;

  for (i = 1; i < argc; i ++)
  {
    if (!strcmp(argv[i], "--help"))
    {
      usage();
    }
    else if (!strcmp(argv[i], "--ippserver"))
    {
      i ++;

      if (i >= argc)
      {
	_cupsLangPuts(stderr, _("ipptool: Missing filename for \"--ippserver\"."));
	usage();
      }

      if (data.outfile != cupsFileStdout())
	usage();

      if ((data.outfile = cupsFileOpen(argv[i], "w")) == NULL)
      {
	_cupsLangPrintf(stderr, _("%s: Unable to open \"%s\": %s"), "ipptool", argv[i], strerror(errno));
	exit(1);
      }

      data.output = _CUPS_OUTPUT_IPPSERVER;
    }
    else if (!strcmp(argv[i], "--stop-after-include-error"))
    {
      data.stop_after_include_error = 1;
    }
    else if (!strcmp(argv[i], "--version"))
    {
      puts(CUPS_SVERSION);
      return (0);
    }
    else if (argv[i][0] == '-')
    {
      for (opt = argv[i] + 1; *opt; opt ++)
      {
        switch (*opt)
        {
	  case '4' : /* Connect using IPv4 only */
	      data.family = AF_INET;
	      break;

#ifdef AF_INET6
	  case '6' : /* Connect using IPv6 only */
	      data.family = AF_INET6;
	      break;
#endif /* AF_INET6 */

          case 'C' : /* Enable HTTP chunking */
              data.def_transfer = _CUPS_TRANSFER_CHUNKED;
              break;

	  case 'E' : /* Encrypt with TLS */
#ifdef HAVE_SSL
	      data.encryption = HTTP_ENCRYPT_REQUIRED;
#else
	      _cupsLangPrintf(stderr, _("%s: Sorry, no encryption support."),
			      argv[0]);
#endif /* HAVE_SSL */
	      break;

          case 'I' : /* Ignore errors */
	      data.def_ignore_errors = 1;
	      break;

          case 'L' : /* Disable HTTP chunking */
              data.def_transfer = _CUPS_TRANSFER_LENGTH;
              break;

          case 'P' : /* Output to plist file */
	      i ++;

	      if (i >= argc)
	      {
		_cupsLangPrintf(stderr, _("%s: Missing filename for \"-P\"."), "ipptool");
		usage();
              }

              if (data.outfile != cupsFileStdout())
                usage();

              if ((data.outfile = cupsFileOpen(argv[i], "w")) == NULL)
              {
                _cupsLangPrintf(stderr, _("%s: Unable to open \"%s\": %s"), "ipptool", argv[i], strerror(errno));
                exit(1);
              }

	      data.output = _CUPS_OUTPUT_PLIST;

              if (interval || repeat)
	      {
	        _cupsLangPuts(stderr, _("ipptool: \"-i\" and \"-n\" are incompatible with \"-P\" and \"-X\"."));
		usage();
	      }
              break;

	  case 'S' : /* Encrypt with SSL */
#ifdef HAVE_SSL
	      data.encryption = HTTP_ENCRYPT_ALWAYS;
#else
	      _cupsLangPrintf(stderr, _("%s: Sorry, no encryption support."),
			      argv[0]);
#endif /* HAVE_SSL */
	      break;

	  case 'T' : /* Set timeout */
	      i ++;

	      if (i >= argc)
	      {
		_cupsLangPrintf(stderr,
		                _("%s: Missing timeout for \"-T\"."),
		                "ipptool");
		usage();
              }

	      data.timeout = _cupsStrScand(argv[i], NULL, localeconv());
	      break;

	  case 'V' : /* Set IPP version */
	      i ++;

	      if (i >= argc)
	      {
		_cupsLangPrintf(stderr,
		                _("%s: Missing version for \"-V\"."),
		                "ipptool");
		usage();
              }

	      if (!strcmp(argv[i], "1.0"))
	      {
	        data.def_version = 10;
	      }
	      else if (!strcmp(argv[i], "1.1"))
	      {
	        data.def_version = 11;
	      }
	      else if (!strcmp(argv[i], "2.0"))
	      {
	        data.def_version = 20;
	      }
	      else if (!strcmp(argv[i], "2.1"))
	      {
	        data.def_version = 21;
	      }
	      else if (!strcmp(argv[i], "2.2"))
	      {
	        data.def_version = 22;
	      }
	      else
	      {
		_cupsLangPrintf(stderr, _("%s: Bad version %s for \"-V\"."), "ipptool", argv[i]);
		usage();
	      }
	      break;

          case 'X' : /* Produce XML output */
	      data.output = _CUPS_OUTPUT_PLIST;

              if (interval || repeat)
	      {
	        _cupsLangPuts(stderr, _("ipptool: \"-i\" and \"-n\" are incompatible with \"-P\" and \"-X\"."));
		usage();
	      }
	      break;

          case 'c' : /* CSV output */
              data.output = _CUPS_OUTPUT_CSV;
              break;

          case 'd' : /* Define a variable */
	      i ++;

	      if (i >= argc)
	      {
		_cupsLangPuts(stderr,
		              _("ipptool: Missing name=value for \"-d\"."));
		usage();
              }

              strlcpy(name, argv[i], sizeof(name));
	      if ((value = strchr(name, '=')) != NULL)
	        *value++ = '\0';
	      else
	        value = name + strlen(name);

	      _ippVarsSet(&vars, name, value);
	      break;

          case 'f' : /* Set the default test filename */
	      i ++;

	      if (i >= argc)
	      {
		_cupsLangPuts(stderr,
		              _("ipptool: Missing filename for \"-f\"."));
		usage();
              }

              if (access(argv[i], 0))
              {
               /*
                * Try filename.gz...
                */

		snprintf(filename, sizeof(filename), "%s.gz", argv[i]);
                if (access(filename, 0) && filename[0] != '/'
#ifdef _WIN32
                    && (!isalpha(filename[0] & 255) || filename[1] != ':')
#endif /* _WIN32 */
                    )
		{
		  snprintf(filename, sizeof(filename), "%s/ipptool/%s", cg->cups_datadir, argv[i]);
		  if (access(filename, 0))
		  {
		    snprintf(filename, sizeof(filename), "%s/ipptool/%s.gz", cg->cups_datadir, argv[i]);
		    if (access(filename, 0))
		      strlcpy(filename, argv[i], sizeof(filename));
		  }
		}
	      }
              else
		strlcpy(filename, argv[i], sizeof(filename));

	      _ippVarsSet(&vars, "filename", filename);

              if ((ext = strrchr(filename, '.')) != NULL)
              {
               /*
                * Guess the MIME media type based on the extension...
                */

                if (!_cups_strcasecmp(ext, ".gif"))
                  _ippVarsSet(&vars, "filetype", "image/gif");
                else if (!_cups_strcasecmp(ext, ".htm") ||
                         !_cups_strcasecmp(ext, ".htm.gz") ||
                         !_cups_strcasecmp(ext, ".html") ||
                         !_cups_strcasecmp(ext, ".html.gz"))
                  _ippVarsSet(&vars, "filetype", "text/html");
                else if (!_cups_strcasecmp(ext, ".jpg") ||
                         !_cups_strcasecmp(ext, ".jpeg"))
                  _ippVarsSet(&vars, "filetype", "image/jpeg");
                else if (!_cups_strcasecmp(ext, ".pcl") ||
                         !_cups_strcasecmp(ext, ".pcl.gz"))
                  _ippVarsSet(&vars, "filetype", "application/vnd.hp-PCL");
                else if (!_cups_strcasecmp(ext, ".pdf"))
                  _ippVarsSet(&vars, "filetype", "application/pdf");
                else if (!_cups_strcasecmp(ext, ".png"))
                  _ippVarsSet(&vars, "filetype", "image/png");
                else if (!_cups_strcasecmp(ext, ".ps") ||
                         !_cups_strcasecmp(ext, ".ps.gz"))
                  _ippVarsSet(&vars, "filetype", "application/postscript");
                else if (!_cups_strcasecmp(ext, ".pwg") ||
                         !_cups_strcasecmp(ext, ".pwg.gz") ||
                         !_cups_strcasecmp(ext, ".ras") ||
                         !_cups_strcasecmp(ext, ".ras.gz"))
                  _ippVarsSet(&vars, "filetype", "image/pwg-raster");
                else if (!_cups_strcasecmp(ext, ".tif") ||
                         !_cups_strcasecmp(ext, ".tiff"))
                  _ippVarsSet(&vars, "filetype", "image/tiff");
                else if (!_cups_strcasecmp(ext, ".txt") ||
                         !_cups_strcasecmp(ext, ".txt.gz"))
                  _ippVarsSet(&vars, "filetype", "text/plain");
                else if (!_cups_strcasecmp(ext, ".urf") ||
                         !_cups_strcasecmp(ext, ".urf.gz"))
                  _ippVarsSet(&vars, "filetype", "image/urf");
                else if (!_cups_strcasecmp(ext, ".xps"))
                  _ippVarsSet(&vars, "filetype", "application/openxps");
                else
		  _ippVarsSet(&vars, "filetype", "application/octet-stream");
              }
              else
              {
               /*
                * Use the "auto-type" MIME media type...
                */

		_ippVarsSet(&vars, "filetype", "application/octet-stream");
              }
	      break;

          case 'h' : /* Validate response headers */
              data.validate_headers = 1;
              break;

          case 'i' : /* Test every N seconds */
	      i ++;

	      if (i >= argc)
	      {
		_cupsLangPuts(stderr, _("ipptool: Missing seconds for \"-i\"."));
		usage();
              }
	      else
	      {
		interval = (int)(_cupsStrScand(argv[i], NULL, localeconv()) * 1000000.0);
		if (interval <= 0)
		{
		  _cupsLangPuts(stderr, _("ipptool: Invalid seconds for \"-i\"."));
		  usage();
		}
              }

              if ((data.output == _CUPS_OUTPUT_PLIST || data.output == _CUPS_OUTPUT_IPPSERVER) && interval)
	      {
	        _cupsLangPuts(stderr, _("ipptool: \"-i\" and \"-n\" are incompatible with \"--ippserver\", \"-P\", and \"-X\"."));
		usage();
	      }
	      break;

          case 'l' : /* List as a table */
              data.output = _CUPS_OUTPUT_LIST;
              break;

          case 'n' : /* Repeat count */
              i ++;

	      if (i >= argc)
	      {
		_cupsLangPuts(stderr, _("ipptool: Missing count for \"-n\"."));
		usage();
              }
	      else
		repeat = atoi(argv[i]);

              if ((data.output == _CUPS_OUTPUT_PLIST || data.output == _CUPS_OUTPUT_IPPSERVER) && repeat)
	      {
	        _cupsLangPuts(stderr, _("ipptool: \"-i\" and \"-n\" are incompatible with \"--ippserver\", \"-P\", and \"-X\"."));
		usage();
	      }
	      break;

          case 'q' : /* Be quiet */
              data.output = _CUPS_OUTPUT_QUIET;
              break;

          case 't' : /* CUPS test output */
              data.output = _CUPS_OUTPUT_TEST;
              break;

          case 'v' : /* Be verbose */
	      data.verbosity ++;
	      break;

	  default :
	      _cupsLangPrintf(stderr, _("%s: Unknown option \"-%c\"."), "ipptool", *opt);
	      usage();
	}
      }
    }
    else if (!strncmp(argv[i], "ipp://", 6) || !strncmp(argv[i], "http://", 7)
#ifdef HAVE_SSL
	     || !strncmp(argv[i], "ipps://", 7) || !strncmp(argv[i], "https://", 8)
#endif /* HAVE_SSL */
	     )
    {
     /*
      * Set URI...
      */

      if (vars.uri)
      {
        _cupsLangPuts(stderr, _("ipptool: May only specify a single URI."));
        usage();
      }

#ifdef HAVE_SSL
      if (!strncmp(argv[i], "ipps://", 7) || !strncmp(argv[i], "https://", 8))
        data.encryption = HTTP_ENCRYPT_ALWAYS;
#endif /* HAVE_SSL */

      if (!_ippVarsSet(&vars, "uri", argv[i]))
      {
        _cupsLangPrintf(stderr, _("ipptool: Bad URI \"%s\"."), argv[i]);
        return (1);
      }

      if (vars.username[0] && vars.password)
	cupsSetPasswordCB2(_ippVarsPasswordCB, &vars);
    }
    else
    {
     /*
      * Run test...
      */

      if (!vars.uri)
      {
        _cupsLangPuts(stderr, _("ipptool: URI required before test file."));
        _cupsLangPuts(stderr, argv[i]);
	usage();
      }

      if (access(argv[i], 0) && argv[i][0] != '/'
#ifdef _WIN32
          && (!isalpha(argv[i][0] & 255) || argv[i][1] != ':')
#endif /* _WIN32 */
          )
      {
        snprintf(testname, sizeof(testname), "%s/ipptool/%s", cg->cups_datadir, argv[i]);
        if (access(testname, 0))
          testfile = argv[i];
        else
          testfile = testname;
      }
      else
        testfile = argv[i];

      if (!do_tests(testfile, &vars, &data))
        status = 1;
    }
  }

  if (!vars.uri || !testfile)
    usage();

 /*
  * Loop if the interval is set...
  */

  if (data.output == _CUPS_OUTPUT_PLIST)
    print_xml_trailer(&data, !status, NULL);
  else if (interval > 0 && repeat > 0)
  {
    while (repeat > 1)
    {
      usleep((useconds_t)interval);
      do_tests(testfile, &vars, &data);
      repeat --;
    }
  }
  else if (interval > 0)
  {
    for (;;)
    {
      usleep((useconds_t)interval);
      do_tests(testfile, &vars, &data);
    }
  }

  if ((data.output == _CUPS_OUTPUT_TEST || (data.output == _CUPS_OUTPUT_PLIST && data.outfile)) && data.test_count > 1)
  {
   /*
    * Show a summary report if there were multiple tests...
    */

    cupsFilePrintf(cupsFileStdout(), "\nSummary: %d tests, %d passed, %d failed, %d skipped\nScore: %d%%\n", data.test_count, data.pass_count, data.fail_count, data.skip_count, 100 * (data.pass_count + data.skip_count) / data.test_count);
  }

  cupsFileClose(data.outfile);

/*
  * Exit...
  */

  return (status);
}


/*
 * 'add_stringf()' - Add a formatted string to an array.
 */

static void
add_stringf(cups_array_t *a,		/* I - Array */
            const char   *s,		/* I - Printf-style format string */
            ...)			/* I - Additional args as needed */
{
  char		buffer[10240];		/* Format buffer */
  va_list	ap;			/* Argument pointer */


 /*
  * Don't bother is the array is NULL...
  */

  if (!a)
    return;

 /*
  * Format the message...
  */

  va_start(ap, s);
  vsnprintf(buffer, sizeof(buffer), s, ap);
  va_end(ap);

 /*
  * Add it to the array...
  */

  cupsArrayAdd(a, buffer);
}


/*
 * 'compare_uris()' - Compare two URIs...
 */

static int                              /* O - Result of comparison */
compare_uris(const char *a,             /* I - First URI */
             const char *b)             /* I - Second URI */
{
  char  ascheme[32],                    /* Components of first URI */
        auserpass[256],
        ahost[256],
        aresource[256];
  int   aport;
  char  bscheme[32],                    /* Components of second URI */
        buserpass[256],
        bhost[256],
        bresource[256];
  int   bport;
  char  *ptr;                           /* Pointer into string */
  int   result;                         /* Result of comparison */


 /*
  * Separate the URIs into their components...
  */

  if (httpSeparateURI(HTTP_URI_CODING_ALL, a, ascheme, sizeof(ascheme), auserpass, sizeof(auserpass), ahost, sizeof(ahost), &aport, aresource, sizeof(aresource)) < HTTP_URI_STATUS_OK)
    return (-1);

  if (httpSeparateURI(HTTP_URI_CODING_ALL, b, bscheme, sizeof(bscheme), buserpass, sizeof(buserpass), bhost, sizeof(bhost), &bport, bresource, sizeof(bresource)) < HTTP_URI_STATUS_OK)
    return (-1);

 /*
  * Strip trailing dots from the host components, if present...
  */

  if ((ptr = ahost + strlen(ahost) - 1) > ahost && *ptr == '.')
    *ptr = '\0';

  if ((ptr = bhost + strlen(bhost) - 1) > bhost && *ptr == '.')
    *ptr = '\0';

 /*
  * Compare each component...
  */

  if ((result = _cups_strcasecmp(ascheme, bscheme)) != 0)
    return (result);

  if ((result = strcmp(auserpass, buserpass)) != 0)
    return (result);

  if ((result = _cups_strcasecmp(ahost, bhost)) != 0)
    return (result);

  if (aport != bport)
    return (aport - bport);

  if (!_cups_strcasecmp(ascheme, "mailto") || !_cups_strcasecmp(ascheme, "urn"))
    return (_cups_strcasecmp(aresource, bresource));
  else
    return (strcmp(aresource, bresource));
}


/*
 * 'copy_hex_string()' - Copy an octetString to a C string and encode as hex if
 *                       needed.
 */

static void
copy_hex_string(char          *buffer,	/* I - String buffer */
		unsigned char *data,	/* I - octetString data */
		int           datalen,	/* I - octetString length */
		size_t        bufsize)	/* I - Size of string buffer */
{
  char		*bufptr,		/* Pointer into string buffer */
		*bufend = buffer + bufsize - 2;
					/* End of string buffer */
  unsigned char	*dataptr,		/* Pointer into octetString data */
		*dataend = data + datalen;
					/* End of octetString data */
  static const char *hexdigits = "0123456789ABCDEF";
					/* Hex digits */


 /*
  * First see if there are any non-ASCII bytes in the octetString...
  */

  for (dataptr = data; dataptr < dataend; dataptr ++)
    if (*dataptr < 0x20 || *dataptr >= 0x7f)
      break;

  if (dataptr < dataend)
  {
   /*
    * Yes, encode as hex...
    */

    *buffer = '<';

    for (bufptr = buffer + 1, dataptr = data; bufptr < bufend && dataptr < dataend; dataptr ++)
    {
      *bufptr++ = hexdigits[*dataptr >> 4];
      *bufptr++ = hexdigits[*dataptr & 15];
    }

    if (bufptr < bufend)
      *bufptr++ = '>';

    *bufptr = '\0';
  }
  else
  {
   /*
    * No, copy as a string...
    */

    if ((size_t)datalen > bufsize)
      datalen = (int)bufsize - 1;

    memcpy(buffer, data, (size_t)datalen);
    buffer[datalen] = '\0';
  }
}


/*
 * 'do_test()' - Do a single test from the test file.
 */

static int				/* O - 1 on success, 0 on failure */
do_test(_ipp_file_t      *f,		/* I - IPP data file */
        _ipp_vars_t      *vars,		/* I - IPP variables */
        _cups_testdata_t *data)		/* I - Test data */

{
  int	        i,			/* Looping var */
		status_ok,		/* Did we get a matching status? */
		repeat_count = 0,	/* Repeat count */
		repeat_test;		/* Repeat the test? */
  _cups_expect_t *expect;		/* Current expected attribute */
  ipp_t		*request,		/* IPP request */
		*response;		/* IPP response */
  size_t	length;			/* Length of IPP request */
  http_status_t	status;			/* HTTP status */
  cups_array_t	*a;			/* Duplicate attribute array */
  ipp_tag_t	group;			/* Current group */
  ipp_attribute_t *attrptr,		/* Attribute pointer */
		*found;			/* Found attribute */
  char		temp[1024];		/* Temporary string */
  cups_file_t	*reqfile;		/* File to send */
  ssize_t	bytes;			/* Bytes read/written */
  char		buffer[131072];		/* Copy buffer */
  size_t	widths[200];		/* Width of columns */
  const char	*error;			/* Current error */


  if (Cancel)
    return (0);

 /*
  * Take over control of the attributes in the request...
  */

  request  = f->attrs;
  f->attrs = NULL;

 /*
  * Submit the IPP request...
  */

  data->test_count ++;

  ippSetVersion(request, data->version / 10, data->version % 10);
  ippSetRequestId(request, data->request_id);

  if (data->output == _CUPS_OUTPUT_PLIST)
  {
    cupsFilePuts(data->outfile, "<dict>\n");
    cupsFilePuts(data->outfile, "<key>Name</key>\n");
    print_xml_string(data->outfile, "string", data->name);
    if (data->file_id[0])
    {
      cupsFilePuts(data->outfile, "<key>FileId</key>\n");
      print_xml_string(data->outfile, "string", data->file_id);
    }
    if (data->test_id[0])
    {
      cupsFilePuts(data->outfile, "<key>TestId</key>\n");
      print_xml_string(data->outfile, "string", data->test_id);
    }
    cupsFilePuts(data->outfile, "<key>Version</key>\n");
    cupsFilePrintf(data->outfile, "<string>%d.%d</string>\n", data->version / 10, data->version % 10);
    cupsFilePuts(data->outfile, "<key>Operation</key>\n");
    print_xml_string(data->outfile, "string", ippOpString(ippGetOperation(request)));
    cupsFilePuts(data->outfile, "<key>RequestId</key>\n");
    cupsFilePrintf(data->outfile, "<integer>%d</integer>\n", data->request_id);
    cupsFilePuts(data->outfile, "<key>RequestAttributes</key>\n");
    cupsFilePuts(data->outfile, "<array>\n");
    if (ippFirstAttribute(request))
    {
      cupsFilePuts(data->outfile, "<dict>\n");
      for (attrptr = ippFirstAttribute(request), group = ippGetGroupTag(attrptr); attrptr; attrptr = ippNextAttribute(request))
	print_attr(data->outfile, data->output, attrptr, &group);
      cupsFilePuts(data->outfile, "</dict>\n");
    }
    cupsFilePuts(data->outfile, "</array>\n");
  }

  if (data->output == _CUPS_OUTPUT_TEST || (data->output == _CUPS_OUTPUT_PLIST && data->outfile != cupsFileStdout()))
  {
    if (data->verbosity)
    {
      cupsFilePrintf(cupsFileStdout(), "    %s:\n", ippOpString(ippGetOperation(request)));

      for (attrptr = ippFirstAttribute(request); attrptr; attrptr = ippNextAttribute(request))
	print_attr(cupsFileStdout(), _CUPS_OUTPUT_TEST, attrptr, NULL);
    }

    cupsFilePrintf(cupsFileStdout(), "    %-68.68s [", data->name);
  }

  if ((data->skip_previous && !data->prev_pass) || data->skip_test)
  {
    data->skip_count ++;

    ippDelete(request);
    request  = NULL;
    response = NULL;

    if (data->output == _CUPS_OUTPUT_PLIST)
    {
      cupsFilePuts(data->outfile, "<key>Successful</key>\n");
      cupsFilePuts(data->outfile, "<true />\n");
      cupsFilePuts(data->outfile, "<key>Skipped</key>\n");
      cupsFilePuts(data->outfile, "<true />\n");
      cupsFilePuts(data->outfile, "<key>StatusCode</key>\n");
      print_xml_string(data->outfile, "string", "skip");
      cupsFilePuts(data->outfile, "<key>ResponseAttributes</key>\n");
      cupsFilePuts(data->outfile, "<dict />\n");
    }

    if (data->output == _CUPS_OUTPUT_TEST || (data->output == _CUPS_OUTPUT_PLIST && data->outfile != cupsFileStdout()))
      cupsFilePuts(cupsFileStdout(), "SKIP]\n");

    goto skip_error;
  }

  vars->password_tries = 0;

  do
  {
    if (data->delay > 0)
      usleep(data->delay);

    data->delay = data->repeat_interval;
    repeat_count ++;

    status = HTTP_STATUS_OK;

    if (data->transfer == _CUPS_TRANSFER_CHUNKED || (data->transfer == _CUPS_TRANSFER_AUTO && data->file[0]))
    {
     /*
      * Send request using chunking - a 0 length means "chunk".
      */

      length = 0;
    }
    else
    {
     /*
      * Send request using content length...
      */

      length = ippLength(request);

      if (data->file[0] && (reqfile = cupsFileOpen(data->file, "r")) != NULL)
      {
       /*
	* Read the file to get the uncompressed file size...
	*/

	while ((bytes = cupsFileRead(reqfile, buffer, sizeof(buffer))) > 0)
	  length += (size_t)bytes;

	cupsFileClose(reqfile);
      }
    }

   /*
    * Send the request...
    */

    data->prev_pass = 1;
    repeat_test     = 0;
    response        = NULL;

    if (status != HTTP_STATUS_ERROR)
    {
      while (!response && !Cancel && data->prev_pass)
      {
	status = cupsSendRequest(data->http, request, data->resource, length);

#ifdef HAVE_LIBZ
	if (data->compression[0])
	  httpSetField(data->http, HTTP_FIELD_CONTENT_ENCODING, data->compression);
#endif /* HAVE_LIBZ */

	if (!Cancel && status == HTTP_STATUS_CONTINUE && ippGetState(request) == IPP_DATA && data->file[0])
	{
	  if ((reqfile = cupsFileOpen(data->file, "r")) != NULL)
	  {
	    while (!Cancel && (bytes = cupsFileRead(reqfile, buffer, sizeof(buffer))) > 0)
	    {
	      if ((status = cupsWriteRequestData(data->http, buffer, (size_t)bytes)) != HTTP_STATUS_CONTINUE)
		break;
            }

	    cupsFileClose(reqfile);
	  }
	  else
	  {
	    snprintf(buffer, sizeof(buffer), "%s: %s", data->file, strerror(errno));
	    _cupsSetError(IPP_INTERNAL_ERROR, buffer, 0);

	    status = HTTP_STATUS_ERROR;
	  }
	}

       /*
	* Get the server's response...
	*/

	if (!Cancel && status != HTTP_STATUS_ERROR)
	{
	  response = cupsGetResponse(data->http, data->resource);
	  status   = httpGetStatus(data->http);
	}

	if (!Cancel && status == HTTP_STATUS_ERROR && httpError(data->http) != EINVAL &&
#ifdef _WIN32
	    httpError(data->http) != WSAETIMEDOUT)
#else
	    httpError(data->http) != ETIMEDOUT)
#endif /* _WIN32 */
	{
	  if (httpReconnect2(data->http, 30000, NULL))
	    data->prev_pass = 0;
	}
	else if (status == HTTP_STATUS_ERROR || status == HTTP_STATUS_CUPS_AUTHORIZATION_CANCELED)
	{
	  data->prev_pass = 0;
	  break;
	}
	else if (status != HTTP_STATUS_OK)
	{
	  httpFlush(data->http);

	  if (status == HTTP_STATUS_UNAUTHORIZED)
	    continue;

	  break;
	}
      }
    }

    if (!Cancel && status == HTTP_STATUS_ERROR && httpError(data->http) != EINVAL &&
#ifdef _WIN32
	httpError(data->http) != WSAETIMEDOUT)
#else
	httpError(data->http) != ETIMEDOUT)
#endif /* _WIN32 */
    {
      if (httpReconnect2(data->http, 30000, NULL))
	data->prev_pass = 0;
    }
    else if (status == HTTP_STATUS_ERROR)
    {
      if (!Cancel)
	httpReconnect2(data->http, 30000, NULL);

      data->prev_pass = 0;
    }
    else if (status != HTTP_STATUS_OK)
    {
      httpFlush(data->http);
      data->prev_pass = 0;
    }

   /*
    * Check results of request...
    */

    cupsArrayClear(data->errors);

    if (httpGetVersion(data->http) != HTTP_1_1)
    {
      int version = (int)httpGetVersion(data->http);

      add_stringf(data->errors, "Bad HTTP version (%d.%d)", version / 100, version % 100);
    }

    if (data->validate_headers)
    {
      const char *header;               /* HTTP header value */

      if ((header = httpGetField(data->http, HTTP_FIELD_CONTENT_TYPE)) == NULL || _cups_strcasecmp(header, "application/ipp"))
	add_stringf(data->errors, "Bad HTTP Content-Type in response (%s)", header && *header ? header : "<missing>");

      if ((header = httpGetField(data->http, HTTP_FIELD_DATE)) != NULL && *header && httpGetDateTime(header) == 0)
	add_stringf(data->errors, "Bad HTTP Date in response (%s)", header);
    }

    if (!response)
    {
     /*
      * No response, log error...
      */

      add_stringf(data->errors, "IPP request failed with status %s (%s)", ippErrorString(cupsLastError()), cupsLastErrorString());
    }
    else
    {
     /*
      * Collect common attribute values...
      */

      if ((attrptr = ippFindAttribute(response, "job-id", IPP_TAG_INTEGER)) != NULL)
      {
	snprintf(temp, sizeof(temp), "%d", ippGetInteger(attrptr, 0));
	_ippVarsSet(vars, "job-id", temp);
      }

      if ((attrptr = ippFindAttribute(response, "job-uri", IPP_TAG_URI)) != NULL)
	_ippVarsSet(vars, "job-uri", ippGetString(attrptr, 0, NULL));

      if ((attrptr = ippFindAttribute(response, "notify-subscription-id", IPP_TAG_INTEGER)) != NULL)
      {
	snprintf(temp, sizeof(temp), "%d", ippGetInteger(attrptr, 0));
	_ippVarsSet(vars, "notify-subscription-id", temp);
      }

     /*
      * Check response, validating groups and attributes and logging errors
      * as needed...
      */

      if (ippGetState(response) != IPP_DATA)
	add_stringf(data->errors, "Missing end-of-attributes-tag in response (RFC 2910 section 3.5.1)");

      if (data->version)
      {
        int major, minor;		/* IPP version */

        major = ippGetVersion(response, &minor);

        if (major != (data->version / 10) || minor != (data->version % 10))
	  add_stringf(data->errors, "Bad version %d.%d in response - expected %d.%d (RFC 2911 section 3.1.8).", major, minor, data->version / 10, data->version % 10);
      }

      if (ippGetRequestId(response) != data->request_id)
	add_stringf(data->errors, "Bad request ID %d in response - expected %d (RFC 2911 section 3.1.1)", ippGetRequestId(response), data->request_id);

      attrptr = ippFirstAttribute(response);
      if (!attrptr)
      {
	add_stringf(data->errors, "Missing first attribute \"attributes-charset (charset)\" in group operation-attributes-tag (RFC 2911 section 3.1.4).");
      }
      else
      {
	if (!ippGetName(attrptr) || ippGetValueTag(attrptr) != IPP_TAG_CHARSET || ippGetGroupTag(attrptr) != IPP_TAG_OPERATION || ippGetCount(attrptr) != 1 ||strcmp(ippGetName(attrptr), "attributes-charset"))
	  add_stringf(data->errors, "Bad first attribute \"%s (%s%s)\" in group %s, expected \"attributes-charset (charset)\" in group operation-attributes-tag (RFC 2911 section 3.1.4).", ippGetName(attrptr) ? ippGetName(attrptr) : "(null)", ippGetCount(attrptr) > 1 ? "1setOf " : "", ippTagString(ippGetValueTag(attrptr)), ippTagString(ippGetGroupTag(attrptr)));

	attrptr = ippNextAttribute(response);
	if (!attrptr)
	  add_stringf(data->errors, "Missing second attribute \"attributes-natural-language (naturalLanguage)\" in group operation-attributes-tag (RFC 2911 section 3.1.4).");
	else if (!ippGetName(attrptr) || ippGetValueTag(attrptr) != IPP_TAG_LANGUAGE || ippGetGroupTag(attrptr) != IPP_TAG_OPERATION || ippGetCount(attrptr) != 1 || strcmp(ippGetName(attrptr), "attributes-natural-language"))
	  add_stringf(data->errors, "Bad first attribute \"%s (%s%s)\" in group %s, expected \"attributes-natural-language (naturalLanguage)\" in group operation-attributes-tag (RFC 2911 section 3.1.4).", ippGetName(attrptr) ? ippGetName(attrptr) : "(null)", ippGetCount(attrptr) > 1 ? "1setOf " : "", ippTagString(ippGetValueTag(attrptr)), ippTagString(ippGetGroupTag(attrptr)));
      }

      if ((attrptr = ippFindAttribute(response, "status-message", IPP_TAG_ZERO)) != NULL)
      {
        const char *status_message = ippGetString(attrptr, 0, NULL);
						/* String value */

	if (ippGetValueTag(attrptr) != IPP_TAG_TEXT)
	  add_stringf(data->errors, "status-message (text(255)) has wrong value tag %s (RFC 2911 section 3.1.6.2).", ippTagString(ippGetValueTag(attrptr)));
	if (ippGetGroupTag(attrptr) != IPP_TAG_OPERATION)
	  add_stringf(data->errors, "status-message (text(255)) has wrong group tag %s (RFC 2911 section 3.1.6.2).", ippTagString(ippGetGroupTag(attrptr)));
	if (ippGetCount(attrptr) != 1)
	  add_stringf(data->errors, "status-message (text(255)) has %d values (RFC 2911 section 3.1.6.2).", ippGetCount(attrptr));
	if (status_message && strlen(status_message) > 255)
	  add_stringf(data->errors, "status-message (text(255)) has bad length %d (RFC 2911 section 3.1.6.2).", (int)strlen(status_message));
      }

      if ((attrptr = ippFindAttribute(response, "detailed-status-message",
				       IPP_TAG_ZERO)) != NULL)
      {
        const char *detailed_status_message = ippGetString(attrptr, 0, NULL);
						/* String value */

	if (ippGetValueTag(attrptr) != IPP_TAG_TEXT)
	  add_stringf(data->errors,
		      "detailed-status-message (text(MAX)) has wrong "
		      "value tag %s (RFC 2911 section 3.1.6.3).",
		      ippTagString(ippGetValueTag(attrptr)));
	if (ippGetGroupTag(attrptr) != IPP_TAG_OPERATION)
	  add_stringf(data->errors,
		      "detailed-status-message (text(MAX)) has wrong "
		      "group tag %s (RFC 2911 section 3.1.6.3).",
		      ippTagString(ippGetGroupTag(attrptr)));
	if (ippGetCount(attrptr) != 1)
	  add_stringf(data->errors,
		      "detailed-status-message (text(MAX)) has %d values"
		      " (RFC 2911 section 3.1.6.3).",
		      ippGetCount(attrptr));
	if (detailed_status_message && strlen(detailed_status_message) > 1023)
	  add_stringf(data->errors,
		      "detailed-status-message (text(MAX)) has bad "
		      "length %d (RFC 2911 section 3.1.6.3).",
		      (int)strlen(detailed_status_message));
      }

      a = cupsArrayNew((cups_array_func_t)strcmp, NULL);

      for (attrptr = ippFirstAttribute(response), group = ippGetGroupTag(attrptr);
	   attrptr;
	   attrptr = ippNextAttribute(response))
      {
	if (ippGetGroupTag(attrptr) != group)
	{
	  int out_of_order = 0;	/* Are attribute groups out-of-order? */
	  cupsArrayClear(a);

	  switch (ippGetGroupTag(attrptr))
	  {
	    case IPP_TAG_ZERO :
		break;

	    case IPP_TAG_OPERATION :
		out_of_order = 1;
		break;

	    case IPP_TAG_UNSUPPORTED_GROUP :
		if (group != IPP_TAG_OPERATION)
		  out_of_order = 1;
		break;

	    case IPP_TAG_JOB :
	    case IPP_TAG_PRINTER :
		if (group != IPP_TAG_OPERATION && group != IPP_TAG_UNSUPPORTED_GROUP)
		  out_of_order = 1;
		break;

	    case IPP_TAG_SUBSCRIPTION :
		if (group > ippGetGroupTag(attrptr) && group != IPP_TAG_DOCUMENT)
		  out_of_order = 1;
		break;

	    default :
		if (group > ippGetGroupTag(attrptr))
		  out_of_order = 1;
		break;
	  }

	  if (out_of_order)
	    add_stringf(data->errors, "Attribute groups out of order (%s < %s)",
			ippTagString(ippGetGroupTag(attrptr)),
			ippTagString(group));

	  if (ippGetGroupTag(attrptr) != IPP_TAG_ZERO)
	    group = ippGetGroupTag(attrptr);
	}

	if (!ippValidateAttribute(attrptr))
	  cupsArrayAdd(data->errors, (void *)cupsLastErrorString());

	if (ippGetName(attrptr))
	{
	  if (cupsArrayFind(a, (void *)ippGetName(attrptr)) && data->output < _CUPS_OUTPUT_LIST)
	    add_stringf(data->errors, "Duplicate \"%s\" attribute in %s group",
			ippGetName(attrptr), ippTagString(group));

	  cupsArrayAdd(a, (void *)ippGetName(attrptr));
	}
      }

      cupsArrayDelete(a);

     /*
      * Now check the test-defined expected status-code and attribute
      * values...
      */

      for (i = 0, status_ok = 0; i < data->num_statuses; i ++)
      {
	if (data->statuses[i].if_defined &&
	    !_ippVarsGet(vars, data->statuses[i].if_defined))
	  continue;

	if (data->statuses[i].if_not_defined &&
	    _ippVarsGet(vars, data->statuses[i].if_not_defined))
	  continue;

	if (ippGetStatusCode(response) == data->statuses[i].status)
	{
	  status_ok = 1;

	  if (data->statuses[i].repeat_match && repeat_count < data->statuses[i].repeat_limit)
	    repeat_test = 1;

	  if (data->statuses[i].define_match)
	    _ippVarsSet(vars, data->statuses[i].define_match, "1");
	}
	else
	{
	  if (data->statuses[i].repeat_no_match && repeat_count < data->statuses[i].repeat_limit)
	    repeat_test = 1;

	  if (data->statuses[i].define_no_match)
	  {
	    _ippVarsSet(vars, data->statuses[i].define_no_match, "1");
	    status_ok = 1;
	  }
	}
      }

      if (!status_ok && data->num_statuses > 0)
      {
	for (i = 0; i < data->num_statuses; i ++)
	{
	  if (data->statuses[i].if_defined &&
	      !_ippVarsGet(vars, data->statuses[i].if_defined))
	    continue;

	  if (data->statuses[i].if_not_defined &&
	      _ippVarsGet(vars, data->statuses[i].if_not_defined))
	    continue;

	  if (!data->statuses[i].repeat_match || repeat_count >= data->statuses[i].repeat_limit)
	    add_stringf(data->errors, "EXPECTED: STATUS %s (got %s)",
			ippErrorString(data->statuses[i].status),
			ippErrorString(cupsLastError()));
	}

	if ((attrptr = ippFindAttribute(response, "status-message",
					IPP_TAG_TEXT)) != NULL)
	  add_stringf(data->errors, "status-message=\"%s\"", ippGetString(attrptr, 0, NULL));
      }

      for (i = data->num_expects, expect = data->expects; i > 0; i --, expect ++)
      {
	ipp_attribute_t *group_found;	/* Found parent attribute for group tests */

	if (expect->if_defined && !_ippVarsGet(vars, expect->if_defined))
	  continue;

	if (expect->if_not_defined &&
	    _ippVarsGet(vars, expect->if_not_defined))
	  continue;

	if ((found = ippFindAttribute(response, expect->name, IPP_TAG_ZERO)) != NULL && expect->in_group && expect->in_group != ippGetGroupTag(found))
	{
	  while ((found = ippFindNextAttribute(response, expect->name, IPP_TAG_ZERO)) != NULL)
	    if (expect->in_group == ippGetGroupTag(found))
	      break;
	}

	do
	{
	  group_found = found;

          if (expect->in_group && strchr(expect->name, '/'))
          {
            char	group_name[256],/* Parent attribute name */
			*group_ptr;	/* Pointer into parent attribute name */

	    strlcpy(group_name, expect->name, sizeof(group_name));
	    if ((group_ptr = strchr(group_name, '/')) != NULL)
	      *group_ptr = '\0';

	    group_found = ippFindAttribute(response, group_name, IPP_TAG_ZERO);
	  }

	  if ((found && expect->not_expect) ||
	      (!found && !(expect->not_expect || expect->optional)) ||
	      (found && !expect_matches(expect, ippGetValueTag(found))) ||
	      (group_found && expect->in_group && ippGetGroupTag(group_found) != expect->in_group))
	  {
	    if (expect->define_no_match)
	      _ippVarsSet(vars, expect->define_no_match, "1");
	    else if (!expect->define_match && !expect->define_value)
	    {
	      if (found && expect->not_expect && !expect->with_value && !expect->with_value_from)
		add_stringf(data->errors, "NOT EXPECTED: %s", expect->name);
	      else if (!found && !(expect->not_expect || expect->optional))
		add_stringf(data->errors, "EXPECTED: %s", expect->name);
	      else if (found)
	      {
		if (!expect_matches(expect, ippGetValueTag(found)))
		  add_stringf(data->errors, "EXPECTED: %s OF-TYPE %s (got %s)",
			      expect->name, expect->of_type,
			      ippTagString(ippGetValueTag(found)));

		if (expect->in_group && ippGetGroupTag(group_found) != expect->in_group)
		  add_stringf(data->errors, "EXPECTED: %s IN-GROUP %s (got %s).",
			      expect->name, ippTagString(expect->in_group),
			      ippTagString(ippGetGroupTag(group_found)));
	      }
	    }

	    if (expect->repeat_no_match && repeat_count < expect->repeat_limit)
	      repeat_test = 1;
	    break;
	  }

	  if (found)
	    ippAttributeString(found, buffer, sizeof(buffer));

	  if (found && expect->with_value_from && !with_value_from(NULL, ippFindAttribute(response, expect->with_value_from, IPP_TAG_ZERO), found, buffer, sizeof(buffer)))
	  {
	    if (expect->define_no_match)
	      _ippVarsSet(vars, expect->define_no_match, "1");
	    else if (!expect->define_match && !expect->define_value && ((!expect->repeat_match && !expect->repeat_no_match) || repeat_count >= expect->repeat_limit))
	    {
	      add_stringf(data->errors, "EXPECTED: %s WITH-VALUES-FROM %s", expect->name, expect->with_value_from);

	      with_value_from(data->errors, ippFindAttribute(response, expect->with_value_from, IPP_TAG_ZERO), found, buffer, sizeof(buffer));
	    }

	    if (expect->repeat_no_match && repeat_count < expect->repeat_limit)
	      repeat_test = 1;

	    break;
	  }
	  else if (found && !with_value(data, NULL, expect->with_value, expect->with_flags, found, buffer, sizeof(buffer)))
	  {
	    if (expect->define_no_match)
	      _ippVarsSet(vars, expect->define_no_match, "1");
	    else if (!expect->define_match && !expect->define_value &&
		     !expect->repeat_match && (!expect->repeat_no_match || repeat_count >= expect->repeat_limit))
	    {
	      if (expect->with_flags & _CUPS_WITH_REGEX)
		add_stringf(data->errors, "EXPECTED: %s %s /%s/", expect->name, with_flags_string(expect->with_flags), expect->with_value);
	      else
		add_stringf(data->errors, "EXPECTED: %s %s \"%s\"", expect->name, with_flags_string(expect->with_flags), expect->with_value);

	      with_value(data, data->errors, expect->with_value, expect->with_flags, found, buffer, sizeof(buffer));
	    }

	    if (expect->repeat_no_match &&
		repeat_count < expect->repeat_limit)
	      repeat_test = 1;

	    break;
	  }

	  if (found && expect->count > 0 && ippGetCount(found) != expect->count)
	  {
	    if (expect->define_no_match)
	      _ippVarsSet(vars, expect->define_no_match, "1");
	    else if (!expect->define_match && !expect->define_value)
	    {
	      add_stringf(data->errors, "EXPECTED: %s COUNT %d (got %d)", expect->name,
			  expect->count, ippGetCount(found));
	    }

	    if (expect->repeat_no_match &&
		repeat_count < expect->repeat_limit)
	      repeat_test = 1;

	    break;
	  }

	  if (found && expect->same_count_as)
	  {
	    attrptr = ippFindAttribute(response, expect->same_count_as,
				       IPP_TAG_ZERO);

	    if (!attrptr || ippGetCount(attrptr) != ippGetCount(found))
	    {
	      if (expect->define_no_match)
		_ippVarsSet(vars, expect->define_no_match, "1");
	      else if (!expect->define_match && !expect->define_value)
	      {
		if (!attrptr)
		  add_stringf(data->errors,
			      "EXPECTED: %s (%d values) SAME-COUNT-AS %s "
			      "(not returned)", expect->name,
			      ippGetCount(found), expect->same_count_as);
		else if (ippGetCount(attrptr) != ippGetCount(found))
		  add_stringf(data->errors,
			      "EXPECTED: %s (%d values) SAME-COUNT-AS %s "
			      "(%d values)", expect->name, ippGetCount(found),
			      expect->same_count_as, ippGetCount(attrptr));
	      }

	      if (expect->repeat_no_match &&
		  repeat_count < expect->repeat_limit)
		repeat_test = 1;

	      break;
	    }
	  }

	  if (found && expect->define_match)
	    _ippVarsSet(vars, expect->define_match, "1");

	  if (found && expect->define_value)
	  {
	    if (!expect->with_value)
	    {
	      int last = ippGetCount(found) - 1;
					/* Last element in attribute */

	      switch (ippGetValueTag(found))
	      {
		case IPP_TAG_ENUM :
		case IPP_TAG_INTEGER :
		    snprintf(buffer, sizeof(buffer), "%d", ippGetInteger(found, last));
		    break;

		case IPP_TAG_BOOLEAN :
		    if (ippGetBoolean(found, last))
		      strlcpy(buffer, "true", sizeof(buffer));
		    else
		      strlcpy(buffer, "false", sizeof(buffer));
		    break;

		case IPP_TAG_RESOLUTION :
		    {
		      int	xres,	/* Horizontal resolution */
				yres;	/* Vertical resolution */
		      ipp_res_t	units;	/* Resolution units */

		      xres = ippGetResolution(found, last, &yres, &units);

		      if (xres == yres)
			snprintf(buffer, sizeof(buffer), "%d%s", xres, units == IPP_RES_PER_INCH ? "dpi" : "dpcm");
		      else
			snprintf(buffer, sizeof(buffer), "%dx%d%s", xres, yres, units == IPP_RES_PER_INCH ? "dpi" : "dpcm");
		    }
		    break;

		case IPP_TAG_CHARSET :
		case IPP_TAG_KEYWORD :
		case IPP_TAG_LANGUAGE :
		case IPP_TAG_MIMETYPE :
		case IPP_TAG_NAME :
		case IPP_TAG_NAMELANG :
		case IPP_TAG_TEXT :
		case IPP_TAG_TEXTLANG :
		case IPP_TAG_URI :
		case IPP_TAG_URISCHEME :
		    strlcpy(buffer, ippGetString(found, last, NULL), sizeof(buffer));
		    break;

		default :
		    ippAttributeString(found, buffer, sizeof(buffer));
		    break;
	      }
	    }

	    _ippVarsSet(vars, expect->define_value, buffer);
	  }

	  if (found && expect->repeat_match &&
	      repeat_count < expect->repeat_limit)
	    repeat_test = 1;
	}
	while (expect->expect_all && (found = ippFindNextAttribute(response, expect->name, IPP_TAG_ZERO)) != NULL);
      }
    }

   /*
    * If we are going to repeat this test, display intermediate results...
    */

    if (repeat_test)
    {
      if (data->output == _CUPS_OUTPUT_TEST || (data->output == _CUPS_OUTPUT_PLIST && data->outfile != cupsFileStdout()))
      {
	cupsFilePrintf(cupsFileStdout(), "%04d]\n", repeat_count);
\
	if (data->num_displayed > 0)
	{
	  for (attrptr = ippFirstAttribute(response); attrptr; attrptr = ippNextAttribute(response))
	  {
	    const char *attrname = ippGetName(attrptr);
	    if (attrname)
	    {
	      for (i = 0; i < data->num_displayed; i ++)
	      {
		if (!strcmp(data->displayed[i], attrname))
		{
		  print_attr(cupsFileStdout(), _CUPS_OUTPUT_TEST, attrptr, NULL);
		  break;
		}
	      }
	    }
	  }
	}
      }

      if (data->output == _CUPS_OUTPUT_TEST || (data->output == _CUPS_OUTPUT_PLIST && data->outfile != cupsFileStdout()))
      {
	cupsFilePrintf(cupsFileStdout(), "    %-68.68s [", data->name);
      }

      ippDelete(response);
      response = NULL;
    }
  }
  while (repeat_test);

  ippDelete(request);

  request = NULL;

  if (cupsArrayCount(data->errors) > 0)
    data->prev_pass = data->pass = 0;

  if (data->prev_pass)
    data->pass_count ++;
  else
    data->fail_count ++;

  if (data->output == _CUPS_OUTPUT_PLIST)
  {
    cupsFilePuts(data->outfile, "<key>Successful</key>\n");
    cupsFilePuts(data->outfile, data->prev_pass ? "<true />\n" : "<false />\n");
    cupsFilePuts(data->outfile, "<key>StatusCode</key>\n");
    print_xml_string(data->outfile, "string", ippErrorString(cupsLastError()));
    cupsFilePuts(data->outfile, "<key>ResponseAttributes</key>\n");
    cupsFilePuts(data->outfile, "<array>\n");
    cupsFilePuts(data->outfile, "<dict>\n");
    for (attrptr = ippFirstAttribute(response), group = ippGetGroupTag(attrptr);
	 attrptr;
	 attrptr = ippNextAttribute(response))
      print_attr(data->outfile, data->output, attrptr, &group);
    cupsFilePuts(data->outfile, "</dict>\n");
    cupsFilePuts(data->outfile, "</array>\n");
  }
  else if (data->output == _CUPS_OUTPUT_IPPSERVER && response)
  {
    for (attrptr = ippFirstAttribute(response); attrptr; attrptr = ippNextAttribute(response))
    {
      if (!ippGetName(attrptr) || ippGetGroupTag(attrptr) != IPP_TAG_PRINTER)
	continue;

      print_ippserver_attr(data, attrptr, 0);
    }
  }

  if (data->output == _CUPS_OUTPUT_TEST || (data->output == _CUPS_OUTPUT_PLIST && data->outfile != cupsFileStdout()))
  {
    cupsFilePuts(cupsFileStdout(), data->prev_pass ? "PASS]\n" : "FAIL]\n");

    if (!data->prev_pass || (data->verbosity && response))
    {
      cupsFilePrintf(cupsFileStdout(), "        RECEIVED: %lu bytes in response\n", (unsigned long)ippLength(response));
      cupsFilePrintf(cupsFileStdout(), "        status-code = %s (%s)\n", ippErrorString(cupsLastError()), cupsLastErrorString());

      if (data->verbosity && response)
      {
	for (attrptr = ippFirstAttribute(response); attrptr; attrptr = ippNextAttribute(response))
	  print_attr(cupsFileStdout(), _CUPS_OUTPUT_TEST, attrptr, NULL);
      }
    }
  }
  else if (!data->prev_pass && data->output != _CUPS_OUTPUT_QUIET)
    fprintf(stderr, "%s\n", cupsLastErrorString());

  if (data->prev_pass && data->output >= _CUPS_OUTPUT_LIST && !data->verbosity && data->num_displayed > 0)
  {
    size_t	width;			/* Length of value */

    for (i = 0; i < data->num_displayed; i ++)
    {
      widths[i] = strlen(data->displayed[i]);

      for (attrptr = ippFindAttribute(response, data->displayed[i], IPP_TAG_ZERO);
	   attrptr;
	   attrptr = ippFindNextAttribute(response, data->displayed[i], IPP_TAG_ZERO))
      {
	width = ippAttributeString(attrptr, NULL, 0);
	if (width > widths[i])
	  widths[i] = width;
      }
    }

    if (data->output == _CUPS_OUTPUT_CSV)
      print_csv(data, NULL, NULL, data->num_displayed, data->displayed, widths);
    else
      print_line(data, NULL, NULL, data->num_displayed, data->displayed, widths);

    attrptr = ippFirstAttribute(response);

    while (attrptr)
    {
      while (attrptr && ippGetGroupTag(attrptr) <= IPP_TAG_OPERATION)
	attrptr = ippNextAttribute(response);

      if (attrptr)
      {
	if (data->output == _CUPS_OUTPUT_CSV)
	  print_csv(data, response, attrptr, data->num_displayed, data->displayed, widths);
	else
	  print_line(data, response, attrptr, data->num_displayed, data->displayed, widths);

	while (attrptr && ippGetGroupTag(attrptr) > IPP_TAG_OPERATION)
	  attrptr = ippNextAttribute(response);
      }
    }
  }
  else if (!data->prev_pass)
  {
    if (data->output == _CUPS_OUTPUT_PLIST)
    {
      cupsFilePuts(data->outfile, "<key>Errors</key>\n");
      cupsFilePuts(data->outfile, "<array>\n");

      for (error = (char *)cupsArrayFirst(data->errors);
	   error;
	   error = (char *)cupsArrayNext(data->errors))
	print_xml_string(data->outfile, "string", error);

      cupsFilePuts(data->outfile, "</array>\n");
    }

    if (data->output == _CUPS_OUTPUT_TEST || (data->output == _CUPS_OUTPUT_PLIST && data->outfile != cupsFileStdout()))
    {
      for (error = (char *)cupsArrayFirst(data->errors);
	   error;
	   error = (char *)cupsArrayNext(data->errors))
	cupsFilePrintf(cupsFileStdout(), "        %s\n", error);
    }
  }

  if (data->num_displayed > 0 && !data->verbosity && response && (data->output == _CUPS_OUTPUT_TEST || (data->output == _CUPS_OUTPUT_PLIST && data->outfile != cupsFileStdout())))
  {
    for (attrptr = ippFirstAttribute(response); attrptr; attrptr = ippNextAttribute(response))
    {
      if (ippGetName(attrptr))
      {
	for (i = 0; i < data->num_displayed; i ++)
	{
	  if (!strcmp(data->displayed[i], ippGetName(attrptr)))
	  {
	    print_attr(data->outfile, data->output, attrptr, NULL);
	    break;
	  }
	}
      }
    }
  }

  skip_error:

  if (data->output == _CUPS_OUTPUT_PLIST)
    cupsFilePuts(data->outfile, "</dict>\n");

  ippDelete(response);
  response = NULL;

  for (i = 0; i < data->num_statuses; i ++)
  {
    if (data->statuses[i].if_defined)
      free(data->statuses[i].if_defined);
    if (data->statuses[i].if_not_defined)
      free(data->statuses[i].if_not_defined);
    if (data->statuses[i].define_match)
      free(data->statuses[i].define_match);
    if (data->statuses[i].define_no_match)
      free(data->statuses[i].define_no_match);
  }
  data->num_statuses = 0;

  for (i = data->num_expects, expect = data->expects; i > 0; i --, expect ++)
  {
    free(expect->name);
    if (expect->of_type)
      free(expect->of_type);
    if (expect->same_count_as)
      free(expect->same_count_as);
    if (expect->if_defined)
      free(expect->if_defined);
    if (expect->if_not_defined)
      free(expect->if_not_defined);
    if (expect->with_value)
      free(expect->with_value);
    if (expect->define_match)
      free(expect->define_match);
    if (expect->define_no_match)
      free(expect->define_no_match);
    if (expect->define_value)
      free(expect->define_value);
  }
  data->num_expects = 0;

  for (i = 0; i < data->num_displayed; i ++)
    free(data->displayed[i]);
  data->num_displayed = 0;

  return (data->ignore_errors || data->prev_pass);
}


/*
 * 'do_tests()' - Do tests as specified in the test file.
 */

static int				/* O - 1 on success, 0 on failure */
do_tests(const char       *testfile,	/* I - Test file to use */
         _ipp_vars_t      *vars,	/* I - Variables */
         _cups_testdata_t *data)	/* I - Test data */
{
  http_encryption_t encryption;		/* Encryption mode */


 /*
  * Connect to the printer/server...
  */

  if (!_cups_strcasecmp(vars->scheme, "https") || !_cups_strcasecmp(vars->scheme, "ipps"))
    encryption = HTTP_ENCRYPTION_ALWAYS;
  else
    encryption = data->encryption;

  if ((data->http = httpConnect2(vars->host, vars->port, NULL, data->family, encryption, 1, 30000, NULL)) == NULL)
  {
    print_fatal_error(data, "Unable to connect to \"%s\" on port %d - %s", vars->host, vars->port, cupsLastErrorString());
    return (0);
  }

#ifdef HAVE_LIBZ
  httpSetDefaultField(data->http, HTTP_FIELD_ACCEPT_ENCODING, "deflate, gzip, identity");
#else
  httpSetDefaultField(data->http, HTTP_FIELD_ACCEPT_ENCODING, "identity");
#endif /* HAVE_LIBZ */

  if (data->timeout > 0.0)
    httpSetTimeout(data->http, data->timeout, timeout_cb, NULL);

 /*
  * Run tests...
  */

  _ippFileParse(vars, testfile, (void *)data);

 /*
  * Close connection and return...
  */

  httpClose(data->http);
  data->http = NULL;

  return (data->pass);
}


/*
 * 'error_cb()' - Print/add an error message.
 */

static int				/* O - 1 to continue, 0 to stop */
error_cb(_ipp_file_t      *f,		/* I - IPP file data */
         _cups_testdata_t *data,	/* I - Test data */
         const char       *error)	/* I - Error message */
{
  (void)f;

  print_fatal_error(data, "%s", error);

  return (1);
}


/*
 * 'expect_matches()' - Return true if the tag matches the specification.
 */

static int				/* O - 1 if matches, 0 otherwise */
expect_matches(
    _cups_expect_t *expect,		/* I - Expected attribute */
    ipp_tag_t      value_tag)		/* I - Value tag for attribute */
{
  int	match;				/* Match? */
  char	*of_type,			/* Type name to match */
	*next,				/* Next name to match */
	sep;				/* Separator character */


 /*
  * If we don't expect a particular type, return immediately...
  */

  if (!expect->of_type)
    return (1);

 /*
  * Parse the "of_type" value since the string can contain multiple attribute
  * types separated by "," or "|"...
  */

  for (of_type = expect->of_type, match = 0; !match && *of_type; of_type = next)
  {
   /*
    * Find the next separator, and set it (temporarily) to nul if present.
    */

    for (next = of_type; *next && *next != '|' && *next != ','; next ++);

    if ((sep = *next) != '\0')
      *next = '\0';

   /*
    * Support some meta-types to make it easier to write the test file.
    */

    if (!strcmp(of_type, "text"))
      match = value_tag == IPP_TAG_TEXTLANG || value_tag == IPP_TAG_TEXT;
    else if (!strcmp(of_type, "name"))
      match = value_tag == IPP_TAG_NAMELANG || value_tag == IPP_TAG_NAME;
    else if (!strcmp(of_type, "collection"))
      match = value_tag == IPP_TAG_BEGIN_COLLECTION;
    else
      match = value_tag == ippTagValue(of_type);

   /*
    * Restore the separator if we have one...
    */

    if (sep)
      *next++ = sep;
  }

  return (match);
}


/*
 * 'get_filename()' - Get a filename based on the current test file.
 */

static char *				/* O - Filename */
get_filename(const char *testfile,	/* I - Current test file */
             char       *dst,		/* I - Destination filename */
	     const char *src,		/* I - Source filename */
             size_t     dstsize)	/* I - Size of destination buffer */
{
  char			*dstptr;	/* Pointer into destination */
  _cups_globals_t	*cg = _cupsGlobals();
					/* Global data */


  if (*src == '<' && src[strlen(src) - 1] == '>')
  {
   /*
    * Map <filename> to CUPS_DATADIR/ipptool/filename...
    */

    snprintf(dst, dstsize, "%s/ipptool/%s", cg->cups_datadir, src + 1);
    dstptr = dst + strlen(dst) - 1;
    if (*dstptr == '>')
      *dstptr = '\0';
  }
  else if (!access(src, R_OK) || *src == '/'
#ifdef _WIN32
           || (isalpha(*src & 255) && src[1] == ':')
#endif /* _WIN32 */
           )
  {
   /*
    * Use the path as-is...
    */

    strlcpy(dst, src, dstsize);
  }
  else
  {
   /*
    * Make path relative to testfile...
    */

    strlcpy(dst, testfile, dstsize);
    if ((dstptr = strrchr(dst, '/')) != NULL)
      dstptr ++;
    else
      dstptr = dst; /* Should never happen */

    strlcpy(dstptr, src, dstsize - (size_t)(dstptr - dst));
  }

  return (dst);
}


/*
 * 'get_string()' - Get a pointer to a string value or the portion of interest.
 */

static const char *			/* O - Pointer to string */
get_string(ipp_attribute_t *attr,	/* I - IPP attribute */
           int             element,	/* I - Element to fetch */
           int             flags,	/* I - Value ("with") flags */
           char            *buffer,	/* I - Temporary buffer */
	   size_t          bufsize)	/* I - Size of temporary buffer */
{
  const char	*value;			/* Value */
  char		*ptr,			/* Pointer into value */
		scheme[256],		/* URI scheme */
		userpass[256],		/* Username/password */
		hostname[256],		/* Hostname */
		resource[1024];		/* Resource */
  int		port;			/* Port number */


  value = ippGetString(attr, element, NULL);

  if (flags & _CUPS_WITH_HOSTNAME)
  {
    if (httpSeparateURI(HTTP_URI_CODING_ALL, value, scheme, sizeof(scheme), userpass, sizeof(userpass), buffer, (int)bufsize, &port, resource, sizeof(resource)) < HTTP_URI_STATUS_OK)
      buffer[0] = '\0';

    ptr = buffer + strlen(buffer) - 1;
    if (ptr >= buffer && *ptr == '.')
      *ptr = '\0';			/* Drop trailing "." */

    return (buffer);
  }
  else if (flags & _CUPS_WITH_RESOURCE)
  {
    if (httpSeparateURI(HTTP_URI_CODING_ALL, value, scheme, sizeof(scheme), userpass, sizeof(userpass), hostname, sizeof(hostname), &port, buffer, (int)bufsize) < HTTP_URI_STATUS_OK)
      buffer[0] = '\0';

    return (buffer);
  }
  else if (flags & _CUPS_WITH_SCHEME)
  {
    if (httpSeparateURI(HTTP_URI_CODING_ALL, value, buffer, (int)bufsize, userpass, sizeof(userpass), hostname, sizeof(hostname), &port, resource, sizeof(resource)) < HTTP_URI_STATUS_OK)
      buffer[0] = '\0';

    return (buffer);
  }
  else if (ippGetValueTag(attr) == IPP_TAG_URI && (!strncmp(value, "ipp://", 6) || !strncmp(value, "http://", 7) || !strncmp(value, "ipps://", 7) || !strncmp(value, "https://", 8)))
  {
    http_uri_status_t status = httpSeparateURI(HTTP_URI_CODING_ALL, value, scheme, sizeof(scheme), userpass, sizeof(userpass), hostname, sizeof(hostname), &port, resource, sizeof(resource));

    if (status < HTTP_URI_STATUS_OK)
    {
     /*
      * Bad URI...
      */

      buffer[0] = '\0';
    }
    else
    {
     /*
      * Normalize URI with no trailing dot...
      */

      if ((ptr = hostname + strlen(hostname) - 1) >= hostname && *ptr == '.')
	*ptr = '\0';

      httpAssembleURI(HTTP_URI_CODING_ALL, buffer, (int)bufsize, scheme, userpass, hostname, port, resource);
    }

    return (buffer);
  }
  else
    return (value);
}


/*
 * 'init_data()' - Initialize test data.
 */

static void
init_data(_cups_testdata_t *data)	/* I - Data */
{
  memset(data, 0, sizeof(_cups_testdata_t));

  data->output       = _CUPS_OUTPUT_LIST;
  data->outfile      = cupsFileStdout();
  data->family       = AF_UNSPEC;
  data->def_transfer = _CUPS_TRANSFER_AUTO;
  data->def_version  = 11;
  data->errors       = cupsArrayNew3(NULL, NULL, NULL, 0, (cups_acopy_func_t)strdup, (cups_afree_func_t)free);
  data->pass         = 1;
  data->prev_pass    = 1;
  data->request_id   = (CUPS_RAND() % 1000) * 137 + 1;
  data->show_header  = 1;
}


/*
 * 'iso_date()' - Return an ISO 8601 date/time string for the given IPP dateTime
 *                value.
 */

static char *				/* O - ISO 8601 date/time string */
iso_date(const ipp_uchar_t *date)	/* I - IPP (RFC 1903) date/time value */
{
  time_t	utctime;		/* UTC time since 1970 */
  struct tm	utcdate;		/* UTC date/time */
  static char	buffer[255];		/* String buffer */


  utctime = ippDateToTime(date);
  gmtime_r(&utctime, &utcdate);

  snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%02dZ",
	   utcdate.tm_year + 1900, utcdate.tm_mon + 1, utcdate.tm_mday,
	   utcdate.tm_hour, utcdate.tm_min, utcdate.tm_sec);

  return (buffer);
}


/*
 * 'pause_message()' - Display the message and pause until the user presses a key.
 */

static void
pause_message(const char *message)	/* I - Message */
{
#ifdef _WIN32
  HANDLE	tty;			/* Console handle */
  DWORD		mode;			/* Console mode */
  char		key;			/* Key press */
  DWORD		bytes;			/* Bytes read for key press */


 /*
  * Disable input echo and set raw input...
  */

  if ((tty = GetStdHandle(STD_INPUT_HANDLE)) == INVALID_HANDLE_VALUE)
    return;

  if (!GetConsoleMode(tty, &mode))
    return;

  if (!SetConsoleMode(tty, 0))
    return;

#else
  int			tty;		/* /dev/tty - never read from stdin */
  struct termios	original,	/* Original input mode */
			noecho;		/* No echo input mode */
  char			key;		/* Current key press */


 /*
  * Disable input echo and set raw input...
  */

  if ((tty = open("/dev/tty", O_RDONLY)) < 0)
    return;

  if (tcgetattr(tty, &original))
  {
    close(tty);
    return;
  }

  noecho = original;
  noecho.c_lflag &= (tcflag_t)~(ICANON | ECHO | ECHOE | ISIG);

  if (tcsetattr(tty, TCSAFLUSH, &noecho))
  {
    close(tty);
    return;
  }
#endif /* _WIN32 */

 /*
  * Display the prompt...
  */

  cupsFilePrintf(cupsFileStdout(), "%s\n---- PRESS ANY KEY ----", message);

#ifdef _WIN32
 /*
  * Read a key...
  */

  ReadFile(tty, &key, 1, &bytes, NULL);

 /*
  * Cleanup...
  */

  SetConsoleMode(tty, mode);

#else
 /*
  * Read a key...
  */

  read(tty, &key, 1);

 /*
  * Cleanup...
  */

  tcsetattr(tty, TCSAFLUSH, &original);
  close(tty);
#endif /* _WIN32 */

 /*
  * Erase the "press any key" prompt...
  */

  cupsFilePuts(cupsFileStdout(), "\r                       \r");
}


/*
 * 'print_attr()' - Print an attribute on the screen.
 */

static void
print_attr(cups_file_t     *outfile,	/* I  - Output file */
           _cups_output_t  output,	/* I  - Output format */
           ipp_attribute_t *attr,	/* I  - Attribute to print */
           ipp_tag_t       *group)	/* IO - Current group */
{
  int			i,		/* Looping var */
			count;		/* Number of values */
  ipp_attribute_t	*colattr;	/* Collection attribute */


  if (output == _CUPS_OUTPUT_PLIST)
  {
    if (!ippGetName(attr) || (group && *group != ippGetGroupTag(attr)))
    {
      if (ippGetGroupTag(attr) != IPP_TAG_ZERO)
      {
	cupsFilePuts(outfile, "</dict>\n");
	cupsFilePuts(outfile, "<dict>\n");
      }

      if (group)
        *group = ippGetGroupTag(attr);
    }

    if (!ippGetName(attr))
      return;

    print_xml_string(outfile, "key", ippGetName(attr));
    if ((count = ippGetCount(attr)) > 1)
      cupsFilePuts(outfile, "<array>\n");

    switch (ippGetValueTag(attr))
    {
      case IPP_TAG_INTEGER :
      case IPP_TAG_ENUM :
	  for (i = 0; i < count; i ++)
	    cupsFilePrintf(outfile, "<integer>%d</integer>\n", ippGetInteger(attr, i));
	  break;

      case IPP_TAG_BOOLEAN :
	  for (i = 0; i < count; i ++)
	    cupsFilePuts(outfile, ippGetBoolean(attr, i) ? "<true />\n" : "<false />\n");
	  break;

      case IPP_TAG_RANGE :
	  for (i = 0; i < count; i ++)
	  {
	    int lower, upper;		/* Lower and upper ranges */

	    lower = ippGetRange(attr, i, &upper);
	    cupsFilePrintf(outfile, "<dict><key>lower</key><integer>%d</integer><key>upper</key><integer>%d</integer></dict>\n", lower, upper);
	  }
	  break;

      case IPP_TAG_RESOLUTION :
	  for (i = 0; i < count; i ++)
	  {
	    int		xres, yres;	/* Resolution values */
	    ipp_res_t	units;		/* Resolution units */

            xres = ippGetResolution(attr, i, &yres, &units);
	    cupsFilePrintf(outfile, "<dict><key>xres</key><integer>%d</integer><key>yres</key><integer>%d</integer><key>units</key><string>%s</string></dict>\n", xres, yres, units == IPP_RES_PER_INCH ? "dpi" : "dpcm");
	  }
	  break;

      case IPP_TAG_DATE :
	  for (i = 0; i < count; i ++)
	    cupsFilePrintf(outfile, "<date>%s</date>\n", iso_date(ippGetDate(attr, i)));
	  break;

      case IPP_TAG_STRING :
          for (i = 0; i < count; i ++)
          {
            int		datalen;	/* Length of data */
            void	*data = ippGetOctetString(attr, i, &datalen);
					/* Data */
	    char	buffer[IPP_MAX_LENGTH * 5 / 4 + 1];
					/* Base64 output buffer */

	    cupsFilePrintf(outfile, "<data>%s</data>\n", httpEncode64_2(buffer, sizeof(buffer), data, datalen));
          }
          break;

      case IPP_TAG_TEXT :
      case IPP_TAG_NAME :
      case IPP_TAG_KEYWORD :
      case IPP_TAG_URI :
      case IPP_TAG_URISCHEME :
      case IPP_TAG_CHARSET :
      case IPP_TAG_LANGUAGE :
      case IPP_TAG_MIMETYPE :
	  for (i = 0; i < count; i ++)
	    print_xml_string(outfile, "string", ippGetString(attr, i, NULL));
	  break;

      case IPP_TAG_TEXTLANG :
      case IPP_TAG_NAMELANG :
	  for (i = 0; i < count; i ++)
	  {
	    const char *s,		/* String */
			*lang;		/* Language */

            s = ippGetString(attr, i, &lang);
	    cupsFilePuts(outfile, "<dict><key>language</key><string>");
	    print_xml_string(outfile, NULL, lang);
	    cupsFilePuts(outfile, "</string><key>string</key><string>");
	    print_xml_string(outfile, NULL, s);
	    cupsFilePuts(outfile, "</string></dict>\n");
	  }
	  break;

      case IPP_TAG_BEGIN_COLLECTION :
	  for (i = 0; i < count; i ++)
	  {
	    ipp_t *col = ippGetCollection(attr, i);
					/* Collection value */

	    cupsFilePuts(outfile, "<dict>\n");
	    for (colattr = ippFirstAttribute(col); colattr; colattr = ippNextAttribute(col))
	      print_attr(outfile, output, colattr, NULL);
	    cupsFilePuts(outfile, "</dict>\n");
	  }
	  break;

      default :
	  cupsFilePrintf(outfile, "<string>&lt;&lt;%s&gt;&gt;</string>\n", ippTagString(ippGetValueTag(attr)));
	  break;
    }

    if (count > 1)
      cupsFilePuts(outfile, "</array>\n");
  }
  else
  {
    char	buffer[131072];		/* Value buffer */

    if (output == _CUPS_OUTPUT_TEST)
    {
      if (!ippGetName(attr))
      {
        cupsFilePuts(outfile, "        -- separator --\n");
        return;
      }

      cupsFilePrintf(outfile, "        %s (%s%s) = ", ippGetName(attr), ippGetCount(attr) > 1 ? "1setOf " : "", ippTagString(ippGetValueTag(attr)));
    }

    ippAttributeString(attr, buffer, sizeof(buffer));
    cupsFilePrintf(outfile, "%s\n", buffer);
  }
}


/*
 * 'print_csv()' - Print a line of CSV text.
 */

static void
print_csv(
    _cups_testdata_t *data,		/* I - Test data */
    ipp_t            *ipp,		/* I - Response message */
    ipp_attribute_t  *attr,		/* I - First attribute for line */
    int              num_displayed,	/* I - Number of attributes to display */
    char             **displayed,	/* I - Attributes to display */
    size_t           *widths)		/* I - Column widths */
{
  int		i;			/* Looping var */
  size_t	maxlength;		/* Max length of all columns */
  char		*buffer,		/* String buffer */
		*bufptr;		/* Pointer into buffer */
  ipp_attribute_t *current;		/* Current attribute */


 /*
  * Get the maximum string length we have to show and allocate...
  */

  for (i = 1, maxlength = widths[0]; i < num_displayed; i ++)
    if (widths[i] > maxlength)
      maxlength = widths[i];

  maxlength += 2;

  if ((buffer = malloc(maxlength)) == NULL)
    return;

 /*
  * Loop through the attributes to display...
  */

  if (attr)
  {
    for (i = 0; i < num_displayed; i ++)
    {
      if (i)
        cupsFilePutChar(data->outfile, ',');

      buffer[0] = '\0';

      for (current = attr; current; current = ippNextAttribute(ipp))
      {
        if (!ippGetName(current))
          break;
        else if (!strcmp(ippGetName(current), displayed[i]))
        {
          ippAttributeString(current, buffer, maxlength);
          break;
        }
      }

      if (strchr(buffer, ',') != NULL || strchr(buffer, '\"') != NULL ||
	  strchr(buffer, '\\') != NULL)
      {
        cupsFilePutChar(cupsFileStdout(), '\"');
        for (bufptr = buffer; *bufptr; bufptr ++)
        {
          if (*bufptr == '\\' || *bufptr == '\"')
            cupsFilePutChar(cupsFileStdout(), '\\');
          cupsFilePutChar(cupsFileStdout(), *bufptr);
        }
        cupsFilePutChar(cupsFileStdout(), '\"');
      }
      else
        cupsFilePuts(data->outfile, buffer);
    }
    cupsFilePutChar(cupsFileStdout(), '\n');
  }
  else
  {
    for (i = 0; i < num_displayed; i ++)
    {
      if (i)
        cupsFilePutChar(cupsFileStdout(), ',');

      cupsFilePuts(data->outfile, displayed[i]);
    }
    cupsFilePutChar(cupsFileStdout(), '\n');
  }

  free(buffer);
}


/*
 * 'print_fatal_error()' - Print a fatal error message.
 */

static void
print_fatal_error(
    _cups_testdata_t *data,		/* I - Test data */
    const char       *s,		/* I - Printf-style format string */
    ...)				/* I - Additional arguments as needed */
{
  char		buffer[10240];		/* Format buffer */
  va_list	ap;			/* Pointer to arguments */


 /*
  * Format the error message...
  */

  va_start(ap, s);
  vsnprintf(buffer, sizeof(buffer), s, ap);
  va_end(ap);

 /*
  * Then output it...
  */

  if (data->output == _CUPS_OUTPUT_PLIST)
  {
    print_xml_header(data);
    print_xml_trailer(data, 0, buffer);
  }

  _cupsLangPrintf(stderr, "ipptool: %s", buffer);
}


/*
 * 'print_ippserver_attr()' - Print a attribute suitable for use by ippserver.
 */

static void
print_ippserver_attr(
    _cups_testdata_t *data,		/* I - Test data */
    ipp_attribute_t  *attr,		/* I - Attribute to print */
    int              indent)		/* I - Indentation level */
{
  int			i,		/* Looping var */
			count = ippGetCount(attr);
					/* Number of values */
  ipp_attribute_t	*colattr;	/* Collection attribute */


  if (indent == 0)
    cupsFilePrintf(data->outfile, "ATTR %s %s", ippTagString(ippGetValueTag(attr)), ippGetName(attr));
  else
    cupsFilePrintf(data->outfile, "%*sMEMBER %s %s", indent, "", ippTagString(ippGetValueTag(attr)), ippGetName(attr));

  switch (ippGetValueTag(attr))
  {
    case IPP_TAG_INTEGER :
    case IPP_TAG_ENUM :
	for (i = 0; i < count; i ++)
	  cupsFilePrintf(data->outfile, "%s%d", i ? "," : " ", ippGetInteger(attr, i));
	break;

    case IPP_TAG_BOOLEAN :
	cupsFilePuts(data->outfile, ippGetBoolean(attr, 0) ? " true" : " false");

	for (i = 1; i < count; i ++)
	  cupsFilePuts(data->outfile, ippGetBoolean(attr, 1) ? ",true" : ",false");
	break;

    case IPP_TAG_RANGE :
	for (i = 0; i < count; i ++)
	{
	  int upper, lower = ippGetRange(attr, i, &upper);

	  cupsFilePrintf(data->outfile, "%s%d-%d", i ? "," : " ", lower, upper);
	}
	break;

    case IPP_TAG_RESOLUTION :
	for (i = 0; i < count; i ++)
	{
	  ipp_res_t units;
	  int yres, xres = ippGetResolution(attr, i, &yres, &units);

	  cupsFilePrintf(data->outfile, "%s%dx%d%s", i ? "," : " ", xres, yres, units == IPP_RES_PER_INCH ? "dpi" : "dpcm");
	}
	break;

    case IPP_TAG_DATE :
	for (i = 0; i < count; i ++)
	  cupsFilePrintf(data->outfile, "%s%s", i ? "," : " ", iso_date(ippGetDate(attr, i)));
	break;

    case IPP_TAG_STRING :
	for (i = 0; i < count; i ++)
	{
	  int len;
	  const char *s = (const char *)ippGetOctetString(attr, i, &len);

	  cupsFilePuts(data->outfile, i ? "," : " ");
	  print_ippserver_string(data, s, (size_t)len);
	}
	break;

    case IPP_TAG_TEXT :
    case IPP_TAG_TEXTLANG :
    case IPP_TAG_NAME :
    case IPP_TAG_NAMELANG :
    case IPP_TAG_KEYWORD :
    case IPP_TAG_URI :
    case IPP_TAG_URISCHEME :
    case IPP_TAG_CHARSET :
    case IPP_TAG_LANGUAGE :
    case IPP_TAG_MIMETYPE :
	for (i = 0; i < count; i ++)
	{
	  const char *s = ippGetString(attr, i, NULL);

	  cupsFilePuts(data->outfile, i ? "," : " ");
	  print_ippserver_string(data, s, strlen(s));
	}
	break;

    case IPP_TAG_BEGIN_COLLECTION :
	for (i = 0; i < count; i ++)
	{
	  ipp_t *col = ippGetCollection(attr, i);

	  cupsFilePuts(data->outfile, i ? ",{\n" : " {\n");
	  for (colattr = ippFirstAttribute(col); colattr; colattr = ippNextAttribute(col))
	    print_ippserver_attr(data, colattr, indent + 4);
	  cupsFilePrintf(data->outfile, "%*s}", indent, "");
	}
	break;

    default :
        /* Out-of-band value */
	break;
  }

  cupsFilePuts(data->outfile, "\n");
}


/*
 * 'print_ippserver_string()' - Print a string suitable for use by ippserver.
 */

static void
print_ippserver_string(
    _cups_testdata_t *data,		/* I - Test data */
    const char       *s,		/* I - String to print */
    size_t           len)		/* I - Length of string */
{
  cupsFilePutChar(data->outfile, '\"');
  while (len > 0)
  {
    if (*s == '\"')
      cupsFilePutChar(data->outfile, '\\');
    cupsFilePutChar(data->outfile, *s);

    s ++;
    len --;
  }
  cupsFilePutChar(data->outfile, '\"');
}


/*
 * 'print_line()' - Print a line of formatted or CSV text.
 */

static void
print_line(
    _cups_testdata_t *data,		/* I - Test data */
    ipp_t            *ipp,		/* I - Response message */
    ipp_attribute_t  *attr,		/* I - First attribute for line */
    int              num_displayed,	/* I - Number of attributes to display */
    char             **displayed,	/* I - Attributes to display */
    size_t           *widths)		/* I - Column widths */
{
  int		i;			/* Looping var */
  size_t	maxlength;		/* Max length of all columns */
  char		*buffer;		/* String buffer */
  ipp_attribute_t *current;		/* Current attribute */


 /*
  * Get the maximum string length we have to show and allocate...
  */

  for (i = 1, maxlength = widths[0]; i < num_displayed; i ++)
    if (widths[i] > maxlength)
      maxlength = widths[i];

  maxlength += 2;

  if ((buffer = malloc(maxlength)) == NULL)
    return;

 /*
  * Loop through the attributes to display...
  */

  if (attr)
  {
    for (i = 0; i < num_displayed; i ++)
    {
      if (i)
        cupsFilePutChar(cupsFileStdout(), ' ');

      buffer[0] = '\0';

      for (current = attr; current; current = ippNextAttribute(ipp))
      {
        if (!ippGetName(current))
          break;
        else if (!strcmp(ippGetName(current), displayed[i]))
        {
          ippAttributeString(current, buffer, maxlength);
          break;
        }
      }

      cupsFilePrintf(data->outfile, "%*s", (int)-widths[i], buffer);
    }
    cupsFilePutChar(cupsFileStdout(), '\n');
  }
  else
  {
    for (i = 0; i < num_displayed; i ++)
    {
      if (i)
        cupsFilePutChar(cupsFileStdout(), ' ');

      cupsFilePrintf(data->outfile, "%*s", (int)-widths[i], displayed[i]);
    }
    cupsFilePutChar(cupsFileStdout(), '\n');

    for (i = 0; i < num_displayed; i ++)
    {
      if (i)
	cupsFilePutChar(cupsFileStdout(), ' ');

      memset(buffer, '-', widths[i]);
      buffer[widths[i]] = '\0';
      cupsFilePuts(data->outfile, buffer);
    }
    cupsFilePutChar(cupsFileStdout(), '\n');
  }

  free(buffer);
}


/*
 * 'print_xml_header()' - Print a standard XML plist header.
 */

static void
print_xml_header(_cups_testdata_t *data)/* I - Test data */
{
  if (!data->xml_header)
  {
    cupsFilePuts(data->outfile, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    cupsFilePuts(data->outfile, "<!DOCTYPE plist PUBLIC \"-//Apple Computer//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n");
    cupsFilePuts(data->outfile, "<plist version=\"1.0\">\n");
    cupsFilePuts(data->outfile, "<dict>\n");
    cupsFilePuts(data->outfile, "<key>ipptoolVersion</key>\n");
    cupsFilePuts(data->outfile, "<string>" CUPS_SVERSION "</string>\n");
    cupsFilePuts(data->outfile, "<key>Transfer</key>\n");
    cupsFilePrintf(data->outfile, "<string>%s</string>\n", data->transfer == _CUPS_TRANSFER_AUTO ? "auto" : data->transfer == _CUPS_TRANSFER_CHUNKED ? "chunked" : "length");
    cupsFilePuts(data->outfile, "<key>Tests</key>\n");
    cupsFilePuts(data->outfile, "<array>\n");

    data->xml_header = 1;
  }
}


/*
 * 'print_xml_string()' - Print an XML string with escaping.
 */

static void
print_xml_string(cups_file_t *outfile,	/* I - Test data */
		 const char  *element,	/* I - Element name or NULL */
		 const char  *s)	/* I - String to print */
{
  if (element)
    cupsFilePrintf(outfile, "<%s>", element);

  while (*s)
  {
    if (*s == '&')
      cupsFilePuts(outfile, "&amp;");
    else if (*s == '<')
      cupsFilePuts(outfile, "&lt;");
    else if (*s == '>')
      cupsFilePuts(outfile, "&gt;");
    else if ((*s & 0xe0) == 0xc0)
    {
     /*
      * Validate UTF-8 two-byte sequence...
      */

      if ((s[1] & 0xc0) != 0x80)
      {
        cupsFilePutChar(outfile, '?');
        s ++;
      }
      else
      {
        cupsFilePutChar(outfile, *s++);
        cupsFilePutChar(outfile, *s);
      }
    }
    else if ((*s & 0xf0) == 0xe0)
    {
     /*
      * Validate UTF-8 three-byte sequence...
      */

      if ((s[1] & 0xc0) != 0x80 || (s[2] & 0xc0) != 0x80)
      {
        cupsFilePutChar(outfile, '?');
        s += 2;
      }
      else
      {
        cupsFilePutChar(outfile, *s++);
        cupsFilePutChar(outfile, *s++);
        cupsFilePutChar(outfile, *s);
      }
    }
    else if ((*s & 0xf8) == 0xf0)
    {
     /*
      * Validate UTF-8 four-byte sequence...
      */

      if ((s[1] & 0xc0) != 0x80 || (s[2] & 0xc0) != 0x80 ||
          (s[3] & 0xc0) != 0x80)
      {
        cupsFilePutChar(outfile, '?');
        s += 3;
      }
      else
      {
        cupsFilePutChar(outfile, *s++);
        cupsFilePutChar(outfile, *s++);
        cupsFilePutChar(outfile, *s++);
        cupsFilePutChar(outfile, *s);
      }
    }
    else if ((*s & 0x80) || (*s < ' ' && !isspace(*s & 255)))
    {
     /*
      * Invalid control character...
      */

      cupsFilePutChar(outfile, '?');
    }
    else
      cupsFilePutChar(outfile, *s);

    s ++;
  }

  if (element)
    cupsFilePrintf(outfile, "</%s>\n", element);
}


/*
 * 'print_xml_trailer()' - Print the XML trailer with success/fail value.
 */

static void
print_xml_trailer(
    _cups_testdata_t *data,		/* I - Test data */
    int              success,		/* I - 1 on success, 0 on failure */
    const char       *message)		/* I - Error message or NULL */
{
  if (data->xml_header)
  {
    cupsFilePuts(data->outfile, "</array>\n");
    cupsFilePuts(data->outfile, "<key>Successful</key>\n");
    cupsFilePuts(data->outfile, success ? "<true />\n" : "<false />\n");
    if (message)
    {
      cupsFilePuts(data->outfile, "<key>ErrorMessage</key>\n");
      print_xml_string(data->outfile, "string", message);
    }
    cupsFilePuts(data->outfile, "</dict>\n");
    cupsFilePuts(data->outfile, "</plist>\n");

    data->xml_header = 0;
  }
}


#ifndef _WIN32
/*
 * 'sigterm_handler()' - Handle SIGINT and SIGTERM.
 */

static void
sigterm_handler(int sig)		/* I - Signal number (unused) */
{
  (void)sig;

  Cancel = 1;

  signal(SIGINT, SIG_DFL);
  signal(SIGTERM, SIG_DFL);
}
#endif /* !_WIN32 */


/*
 * 'timeout_cb()' - Handle HTTP timeouts.
 */

static int				/* O - 1 to continue, 0 to cancel */
timeout_cb(http_t *http,		/* I - Connection to server */
           void   *user_data)		/* I - User data (unused) */
{
  int		buffered = 0;		/* Bytes buffered but not yet sent */


  (void)user_data;

 /*
  * If the socket still have data waiting to be sent to the printer (as can
  * happen if the printer runs out of paper), continue to wait until the output
  * buffer is empty...
  */

#ifdef SO_NWRITE			/* macOS and some versions of Linux */
  socklen_t len = sizeof(buffered);	/* Size of return value */

  if (getsockopt(httpGetFd(http), SOL_SOCKET, SO_NWRITE, &buffered, &len))
    buffered = 0;

#elif defined(SIOCOUTQ)			/* Others except Windows */
  if (ioctl(httpGetFd(http), SIOCOUTQ, &buffered))
    buffered = 0;

#else					/* Windows (not possible) */
  (void)http;
#endif /* SO_NWRITE */

  return (buffered > 0);
}


/*
 * 'token_cb()' - Parse test file-specific tokens and run tests.
 */

static int				/* O - 1 to continue, 0 to stop */
token_cb(_ipp_file_t      *f,		/* I - IPP file data */
         _ipp_vars_t      *vars,	/* I - IPP variables */
         _cups_testdata_t *data,	/* I - Test data */
         const char       *token)	/* I - Current token */
{
  char	name[1024],			/* Name string */
	temp[1024],			/* Temporary string */
	value[1024],			/* Value string */
	*ptr;				/* Pointer into value */


  if (!token)
  {
   /*
    * Initialize state as needed (nothing for now...)
    */

    return (1);
  }
  else if (f->attrs)
  {
   /*
    * Parse until we see a close brace...
    */

    if (_cups_strcasecmp(token, "COUNT") &&
	_cups_strcasecmp(token, "DEFINE-MATCH") &&
	_cups_strcasecmp(token, "DEFINE-NO-MATCH") &&
	_cups_strcasecmp(token, "DEFINE-VALUE") &&
	_cups_strcasecmp(token, "IF-DEFINED") &&
	_cups_strcasecmp(token, "IF-NOT-DEFINED") &&
	_cups_strcasecmp(token, "IN-GROUP") &&
	_cups_strcasecmp(token, "OF-TYPE") &&
	_cups_strcasecmp(token, "REPEAT-LIMIT") &&
	_cups_strcasecmp(token, "REPEAT-MATCH") &&
	_cups_strcasecmp(token, "REPEAT-NO-MATCH") &&
	_cups_strcasecmp(token, "SAME-COUNT-AS") &&
	_cups_strcasecmp(token, "WITH-ALL-VALUES") &&
	_cups_strcasecmp(token, "WITH-ALL-HOSTNAMES") &&
	_cups_strcasecmp(token, "WITH-ALL-RESOURCES") &&
	_cups_strcasecmp(token, "WITH-ALL-SCHEMES") &&
	_cups_strcasecmp(token, "WITH-HOSTNAME") &&
	_cups_strcasecmp(token, "WITH-RESOURCE") &&
	_cups_strcasecmp(token, "WITH-SCHEME") &&
	_cups_strcasecmp(token, "WITH-VALUE") &&
	_cups_strcasecmp(token, "WITH-VALUE-FROM"))
      data->last_expect = NULL;

    if (_cups_strcasecmp(token, "DEFINE-MATCH") &&
	_cups_strcasecmp(token, "DEFINE-NO-MATCH") &&
	_cups_strcasecmp(token, "IF-DEFINED") &&
	_cups_strcasecmp(token, "IF-NOT-DEFINED") &&
	_cups_strcasecmp(token, "REPEAT-LIMIT") &&
	_cups_strcasecmp(token, "REPEAT-MATCH") &&
	_cups_strcasecmp(token, "REPEAT-NO-MATCH"))
      data->last_status = NULL;

    if (!strcmp(token, "}"))
    {
      return (do_test(f, vars, data));
    }
    else if (!strcmp(token, "COMPRESSION"))
    {
     /*
      * COMPRESSION none
      * COMPRESSION deflate
      * COMPRESSION gzip
      */

      if (_ippFileReadToken(f, temp, sizeof(temp)))
      {
	_ippVarsExpand(vars, data->compression, temp, sizeof(data->compression));
#ifdef HAVE_LIBZ
	if (strcmp(data->compression, "none") && strcmp(data->compression, "deflate") &&
	    strcmp(data->compression, "gzip"))
#else
	if (strcmp(data->compression, "none"))
#endif /* HAVE_LIBZ */
	{
	  print_fatal_error(data, "Unsupported COMPRESSION value \"%s\" on line %d of \"%s\".", data->compression, f->linenum, f->filename);
	  return (0);
	}

	if (!strcmp(data->compression, "none"))
	  data->compression[0] = '\0';
      }
      else
      {
	print_fatal_error(data, "Missing COMPRESSION value on line %d of \"%s\".", f->linenum, f->filename);
	return (0);
      }
    }
    else if (!strcmp(token, "DEFINE"))
    {
     /*
      * DEFINE name value
      */

      if (_ippFileReadToken(f, name, sizeof(name)) && _ippFileReadToken(f, temp, sizeof(temp)))
      {
	_ippVarsExpand(vars, value, temp, sizeof(value));
	_ippVarsSet(vars, name, value);
      }
      else
      {
	print_fatal_error(data, "Missing DEFINE name and/or value on line %d of \"%s\".", f->linenum, f->filename);
	return (0);
      }
    }
    else if (!strcmp(token, "IGNORE-ERRORS"))
    {
     /*
      * IGNORE-ERRORS yes
      * IGNORE-ERRORS no
      */

      if (_ippFileReadToken(f, temp, sizeof(temp)) && (!_cups_strcasecmp(temp, "yes") || !_cups_strcasecmp(temp, "no")))
      {
	data->ignore_errors = !_cups_strcasecmp(temp, "yes");
      }
      else
      {
	print_fatal_error(data, "Missing IGNORE-ERRORS value on line %d of \"%s\".", f->linenum, f->filename);
	return (0);
      }
    }
    else if (!_cups_strcasecmp(token, "NAME"))
    {
     /*
      * Name of test...
      */

      _ippFileReadToken(f, temp, sizeof(temp));
      _ippVarsExpand(vars, data->name, temp, sizeof(data->name));
    }
    else if (!_cups_strcasecmp(token, "PAUSE"))
    {
     /*
      * Pause with a message...
      */

      if (_ippFileReadToken(f, temp, sizeof(temp)))
      {
        pause_message(temp);
      }
      else
      {
	print_fatal_error(data, "Missing PAUSE message on line %d of \"%s\".", f->linenum, f->filename);
	return (0);
      }
    }
    else if (!strcmp(token, "REQUEST-ID"))
    {
     /*
      * REQUEST-ID #
      * REQUEST-ID random
      */

      if (_ippFileReadToken(f, temp, sizeof(temp)))
      {
	if (isdigit(temp[0] & 255))
	{
	  data->request_id = atoi(temp);
	}
	else if (!_cups_strcasecmp(temp, "random"))
	{
	  data->request_id = (CUPS_RAND() % 1000) * 137 + 1;
	}
	else
	{
	  print_fatal_error(data, "Bad REQUEST-ID value \"%s\" on line %d of \"%s\".", temp, f->linenum, f->filename);
	  return (0);
	}
      }
      else
      {
	print_fatal_error(data, "Missing REQUEST-ID value on line %d of \"%s\".", f->linenum, f->filename);
	return (0);
      }
    }
    else if (!strcmp(token, "SKIP-IF-DEFINED"))
    {
     /*
      * SKIP-IF-DEFINED variable
      */

      if (_ippFileReadToken(f, name, sizeof(name)))
      {
	if (_ippVarsGet(vars, name))
	  data->skip_test = 1;
      }
      else
      {
	print_fatal_error(data, "Missing SKIP-IF-DEFINED value on line %d of \"%s\".", f->linenum, f->filename);
	return (0);
      }
    }
    else if (!strcmp(token, "SKIP-IF-MISSING"))
    {
     /*
      * SKIP-IF-MISSING filename
      */

      if (_ippFileReadToken(f, temp, sizeof(temp)))
      {
        char filename[1024];		/* Filename */

	_ippVarsExpand(vars, value, temp, sizeof(value));
	get_filename(f->filename, filename, temp, sizeof(filename));

	if (access(filename, R_OK))
	  data->skip_test = 1;
      }
      else
      {
	print_fatal_error(data, "Missing SKIP-IF-MISSING filename on line %d of \"%s\".", f->linenum, f->filename);
	return (0);
      }
    }
    else if (!strcmp(token, "SKIP-IF-NOT-DEFINED"))
    {
     /*
      * SKIP-IF-NOT-DEFINED variable
      */

      if (_ippFileReadToken(f, name, sizeof(name)))
      {
	if (!_ippVarsGet(vars, name))
	  data->skip_test = 1;
      }
      else
      {
	print_fatal_error(data, "Missing SKIP-IF-NOT-DEFINED value on line %d of \"%s\".", f->linenum, f->filename);
	return (0);
      }
    }
    else if (!strcmp(token, "SKIP-PREVIOUS-ERROR"))
    {
     /*
      * SKIP-PREVIOUS-ERROR yes
      * SKIP-PREVIOUS-ERROR no
      */

      if (_ippFileReadToken(f, temp, sizeof(temp)) && (!_cups_strcasecmp(temp, "yes") || !_cups_strcasecmp(temp, "no")))
      {
	data->skip_previous = !_cups_strcasecmp(temp, "yes");
      }
      else
      {
	print_fatal_error(data, "Missing SKIP-PREVIOUS-ERROR value on line %d of \"%s\".", f->linenum, f->filename);
	return (0);
      }
    }
    else if (!strcmp(token, "TEST-ID"))
    {
     /*
      * TEST-ID "string"
      */

      if (_ippFileReadToken(f, temp, sizeof(temp)))
      {
	_ippVarsExpand(vars, data->test_id, temp, sizeof(data->test_id));
      }
      else
      {
	print_fatal_error(data, "Missing TEST-ID value on line %d of \"%s\".", f->linenum, f->filename);
	return (0);
      }
    }
    else if (!strcmp(token, "TRANSFER"))
    {
     /*
      * TRANSFER auto
      * TRANSFER chunked
      * TRANSFER length
      */

      if (_ippFileReadToken(f, temp, sizeof(temp)))
      {
	if (!strcmp(temp, "auto"))
	{
	  data->transfer = _CUPS_TRANSFER_AUTO;
	}
	else if (!strcmp(temp, "chunked"))
	{
	  data->transfer = _CUPS_TRANSFER_CHUNKED;
	}
	else if (!strcmp(temp, "length"))
	{
	  data->transfer = _CUPS_TRANSFER_LENGTH;
	}
	else
	{
	  print_fatal_error(data, "Bad TRANSFER value \"%s\" on line %d of \"%s\".", temp, f->linenum, f->filename);
	  return (0);
	}
      }
      else
      {
	print_fatal_error(data, "Missing TRANSFER value on line %d of \"%s\".", f->linenum, f->filename);
	return (0);
      }
    }
    else if (!_cups_strcasecmp(token, "VERSION"))
    {
      if (_ippFileReadToken(f, temp, sizeof(temp)))
      {
	if (!strcmp(temp, "0.0"))
	{
	  data->version = 0;
	}
	else if (!strcmp(temp, "1.0"))
	{
	  data->version = 10;
	}
	else if (!strcmp(temp, "1.1"))
	{
	  data->version = 11;
	}
	else if (!strcmp(temp, "2.0"))
	{
	  data->version = 20;
	}
	else if (!strcmp(temp, "2.1"))
	{
	  data->version = 21;
	}
	else if (!strcmp(temp, "2.2"))
	{
	  data->version = 22;
	}
	else
	{
	  print_fatal_error(data, "Bad VERSION \"%s\" on line %d of \"%s\".", temp, f->linenum, f->filename);
	  return (0);
	}
      }
      else
      {
	print_fatal_error(data, "Missing VERSION number on line %d of \"%s\".", f->linenum, f->filename);
	return (0);
      }
    }
    else if (!_cups_strcasecmp(token, "RESOURCE"))
    {
     /*
      * Resource name...
      */

      if (!_ippFileReadToken(f, data->resource, sizeof(data->resource)))
      {
	print_fatal_error(data, "Missing RESOURCE path on line %d of \"%s\".", f->linenum, f->filename);
	return (0);
      }
    }
    else if (!_cups_strcasecmp(token, "OPERATION"))
    {
     /*
      * Operation...
      */

      ipp_op_t	op;			/* Operation code */

      if (!_ippFileReadToken(f, temp, sizeof(temp)))
      {
	print_fatal_error(data, "Missing OPERATION code on line %d of \"%s\".", f->linenum, f->filename);
	return (0);
      }

      _ippVarsExpand(vars, value, temp, sizeof(value));

      if ((op = ippOpValue(value)) == (ipp_op_t)-1 && (op = (ipp_op_t)strtol(value, NULL, 0)) == 0)
      {
	print_fatal_error(data, "Bad OPERATION code \"%s\" on line %d of \"%s\".", temp, f->linenum, f->filename);
	return (0);
      }

      ippSetOperation(f->attrs, op);
    }
    else if (!_cups_strcasecmp(token, "GROUP"))
    {
     /*
      * Attribute group...
      */

      ipp_tag_t	group_tag;		/* Group tag */

      if (!_ippFileReadToken(f, temp, sizeof(temp)))
      {
	print_fatal_error(data, "Missing GROUP tag on line %d of \"%s\".", f->linenum, f->filename);
	return (0);
      }

      if ((group_tag = ippTagValue(temp)) == IPP_TAG_ZERO || group_tag >= IPP_TAG_UNSUPPORTED_VALUE)
      {
	print_fatal_error(data, "Bad GROUP tag \"%s\" on line %d of \"%s\".", temp, f->linenum, f->filename);
	return (0);
      }

      if (group_tag == f->group_tag)
	ippAddSeparator(f->attrs);

      f->group_tag = group_tag;
    }
    else if (!_cups_strcasecmp(token, "DELAY"))
    {
     /*
      * Delay before operation...
      */

      double dval;                    /* Delay value */

      if (!_ippFileReadToken(f, temp, sizeof(temp)))
      {
	print_fatal_error(data, "Missing DELAY value on line %d of \"%s\".", f->linenum, f->filename);
	return (0);
      }

      _ippVarsExpand(vars, value, temp, sizeof(value));

      if ((dval = _cupsStrScand(value, &ptr, localeconv())) < 0.0 || (*ptr && *ptr != ','))
      {
	print_fatal_error(data, "Bad DELAY value \"%s\" on line %d of \"%s\".", value, f->linenum, f->filename);
	return (0);
      }

      data->delay = (useconds_t)(1000000.0 * dval);

      if (*ptr == ',')
      {
	if ((dval = _cupsStrScand(ptr + 1, &ptr, localeconv())) <= 0.0 || *ptr)
	{
	  print_fatal_error(data, "Bad DELAY value \"%s\" on line %d of \"%s\".", value, f->linenum, f->filename);
	  return (0);
	}

	data->repeat_interval = (useconds_t)(1000000.0 * dval);
      }
      else
	data->repeat_interval = data->delay;
    }
    else if (!_cups_strcasecmp(token, "FILE"))
    {
     /*
      * File...
      */

      if (!_ippFileReadToken(f, temp, sizeof(temp)))
      {
	print_fatal_error(data, "Missing FILE filename on line %d of \"%s\".", f->linenum, f->filename);
	return (0);
      }

      _ippVarsExpand(vars, value, temp, sizeof(value));
      get_filename(f->filename, data->file, value, sizeof(data->file));

      if (access(data->file, R_OK))
      {
	print_fatal_error(data, "Filename \"%s\" (mapped to \"%s\") on line %d of \"%s\" cannot be read.", value, data->file, f->linenum, f->filename);
	return (0);
      }
    }
    else if (!_cups_strcasecmp(token, "STATUS"))
    {
     /*
      * Status...
      */

      if (data->num_statuses >= (int)(sizeof(data->statuses) / sizeof(data->statuses[0])))
      {
	print_fatal_error(data, "Too many STATUS's on line %d of \"%s\".", f->linenum, f->filename);
	return (0);
      }

      if (!_ippFileReadToken(f, temp, sizeof(temp)))
      {
	print_fatal_error(data, "Missing STATUS code on line %d of \"%s\".", f->linenum, f->filename);
	return (0);
      }

      if ((data->statuses[data->num_statuses].status = ippErrorValue(temp)) == (ipp_status_t)-1 && (data->statuses[data->num_statuses].status = (ipp_status_t)strtol(temp, NULL, 0)) == 0)
      {
	print_fatal_error(data, "Bad STATUS code \"%s\" on line %d of \"%s\".", temp, f->linenum, f->filename);
	return (0);
      }

      data->last_status = data->statuses + data->num_statuses;
      data->num_statuses ++;

      data->last_status->define_match    = NULL;
      data->last_status->define_no_match = NULL;
      data->last_status->if_defined      = NULL;
      data->last_status->if_not_defined  = NULL;
      data->last_status->repeat_limit    = 1000;
      data->last_status->repeat_match    = 0;
      data->last_status->repeat_no_match = 0;
    }
    else if (!_cups_strcasecmp(token, "EXPECT") || !_cups_strcasecmp(token, "EXPECT-ALL"))
    {
     /*
      * Expected attributes...
      */

      int expect_all = !_cups_strcasecmp(token, "EXPECT-ALL");

      if (data->num_expects >= (int)(sizeof(data->expects) / sizeof(data->expects[0])))
      {
	print_fatal_error(data, "Too many EXPECT's on line %d of \"%s\".", f->linenum, f->filename);
	return (0);
      }

      if (!_ippFileReadToken(f, name, sizeof(name)))
      {
	print_fatal_error(data, "Missing EXPECT name on line %d of \"%s\".", f->linenum, f->filename);
	return (0);
      }

      data->last_expect = data->expects + data->num_expects;
      data->num_expects ++;

      memset(data->last_expect, 0, sizeof(_cups_expect_t));
      data->last_expect->repeat_limit = 1000;
      data->last_expect->expect_all   = expect_all;

      if (name[0] == '!')
      {
	data->last_expect->not_expect = 1;
	data->last_expect->name       = strdup(name + 1);
      }
      else if (name[0] == '?')
      {
	data->last_expect->optional = 1;
	data->last_expect->name     = strdup(name + 1);
      }
      else
	data->last_expect->name = strdup(name);
    }
    else if (!_cups_strcasecmp(token, "COUNT"))
    {
      int	count;			/* Count value */

      if (!_ippFileReadToken(f, temp, sizeof(temp)))
      {
	print_fatal_error(data, "Missing COUNT number on line %d of \"%s\".", f->linenum, f->filename);
	return (0);
      }

      if ((count = atoi(temp)) <= 0)
      {
	print_fatal_error(data, "Bad COUNT \"%s\" on line %d of \"%s\".", temp, f->linenum, f->filename);
	return (0);
      }

      if (data->last_expect)
      {
	data->last_expect->count = count;
      }
      else
      {
	print_fatal_error(data, "COUNT without a preceding EXPECT on line %d of \"%s\".", f->linenum, f->filename);
	return (0);
      }
    }
    else if (!_cups_strcasecmp(token, "DEFINE-MATCH"))
    {
      if (!_ippFileReadToken(f, temp, sizeof(temp)))
      {
	print_fatal_error(data, "Missing DEFINE-MATCH variable on line %d of \"%s\".", f->linenum, f->filename);
	return (0);
      }

      if (data->last_expect)
      {
	data->last_expect->define_match = strdup(temp);
      }
      else if (data->last_status)
      {
	data->last_status->define_match = strdup(temp);
      }
      else
      {
	print_fatal_error(data, "DEFINE-MATCH without a preceding EXPECT or STATUS on line %d of \"%s\".", f->linenum, f->filename);
	return (0);
      }
    }
    else if (!_cups_strcasecmp(token, "DEFINE-NO-MATCH"))
    {
      if (!_ippFileReadToken(f, temp, sizeof(temp)))
      {
	print_fatal_error(data, "Missing DEFINE-NO-MATCH variable on line %d of \"%s\".", f->linenum, f->filename);
	return (0);
      }

      if (data->last_expect)
      {
	data->last_expect->define_no_match = strdup(temp);
      }
      else if (data->last_status)
      {
	data->last_status->define_no_match = strdup(temp);
      }
      else
      {
	print_fatal_error(data, "DEFINE-NO-MATCH without a preceding EXPECT or STATUS on line %d of \"%s\".", f->linenum, f->filename);
	return (0);
      }
    }
    else if (!_cups_strcasecmp(token, "DEFINE-VALUE"))
    {
      if (!_ippFileReadToken(f, temp, sizeof(temp)))
      {
	print_fatal_error(data, "Missing DEFINE-VALUE variable on line %d of \"%s\".", f->linenum, f->filename);
	return (0);
      }

      if (data->last_expect)
      {
	data->last_expect->define_value = strdup(temp);
      }
      else
      {
	print_fatal_error(data, "DEFINE-VALUE without a preceding EXPECT on line %d of \"%s\".", f->linenum, f->filename);
	return (0);
      }
    }
    else if (!_cups_strcasecmp(token, "OF-TYPE"))
    {
      if (!_ippFileReadToken(f, temp, sizeof(temp)))
      {
	print_fatal_error(data, "Missing OF-TYPE value tag(s) on line %d of \"%s\".", f->linenum, f->filename);
	return (0);
      }

      if (data->last_expect)
      {
	data->last_expect->of_type = strdup(temp);
      }
      else
      {
	print_fatal_error(data, "OF-TYPE without a preceding EXPECT on line %d of \"%s\".", f->linenum, f->filename);
	return (0);
      }
    }
    else if (!_cups_strcasecmp(token, "IN-GROUP"))
    {
      ipp_tag_t	in_group;		/* IN-GROUP value */

      if (!_ippFileReadToken(f, temp, sizeof(temp)))
      {
	print_fatal_error(data, "Missing IN-GROUP group tag on line %d of \"%s\".", f->linenum, f->filename);
	return (0);
      }

      if ((in_group = ippTagValue(temp)) == IPP_TAG_ZERO || in_group >= IPP_TAG_UNSUPPORTED_VALUE)
      {
	print_fatal_error(data, "Bad IN-GROUP group tag \"%s\" on line %d of \"%s\".", temp, f->linenum, f->filename);
	return (0);
      }
      else if (data->last_expect)
      {
	data->last_expect->in_group = in_group;
      }
      else
      {
	print_fatal_error(data, "IN-GROUP without a preceding EXPECT on line %d of \"%s\".", f->linenum, f->filename);
	return (0);
      }
    }
    else if (!_cups_strcasecmp(token, "REPEAT-LIMIT"))
    {
      if (!_ippFileReadToken(f, temp, sizeof(temp)))
      {
	print_fatal_error(data, "Missing REPEAT-LIMIT value on line %d of \"%s\".", f->linenum, f->filename);
	return (0);
      }
      else if (atoi(temp) <= 0)
      {
	print_fatal_error(data, "Bad REPEAT-LIMIT value on line %d of \"%s\".", f->linenum, f->filename);
	return (0);
      }

      if (data->last_status)
      {
	data->last_status->repeat_limit = atoi(temp);
      }
      else if (data->last_expect)
      {
	data->last_expect->repeat_limit = atoi(temp);
      }
      else
      {
	print_fatal_error(data, "REPEAT-LIMIT without a preceding EXPECT or STATUS on line %d of \"%s\".", f->linenum, f->filename);
	return (0);
      }
    }
    else if (!_cups_strcasecmp(token, "REPEAT-MATCH"))
    {
      if (data->last_status)
      {
	data->last_status->repeat_match = 1;
      }
      else if (data->last_expect)
      {
	data->last_expect->repeat_match = 1;
      }
      else
      {
	print_fatal_error(data, "REPEAT-MATCH without a preceding EXPECT or STATUS on line %d of \"%s\".", f->linenum, f->filename);
	return (0);
      }
    }
    else if (!_cups_strcasecmp(token, "REPEAT-NO-MATCH"))
    {
      if (data->last_status)
      {
	data->last_status->repeat_no_match = 1;
      }
      else if (data->last_expect)
      {
	data->last_expect->repeat_no_match = 1;
      }
      else
      {
	print_fatal_error(data, "REPEAT-NO-MATCH without a preceding EXPECT or STATUS on line %d of \"%s\".", f->linenum, f->filename);
	return (0);
      }
    }
    else if (!_cups_strcasecmp(token, "SAME-COUNT-AS"))
    {
      if (!_ippFileReadToken(f, temp, sizeof(temp)))
      {
	print_fatal_error(data, "Missing SAME-COUNT-AS name on line %d of \"%s\".", f->linenum, f->filename);
	return (0);
      }

      if (data->last_expect)
      {
	data->last_expect->same_count_as = strdup(temp);
      }
      else
      {
	print_fatal_error(data, "SAME-COUNT-AS without a preceding EXPECT on line %d of \"%s\".", f->linenum, f->filename);
	return (0);
      }
    }
    else if (!_cups_strcasecmp(token, "IF-DEFINED"))
    {
      if (!_ippFileReadToken(f, temp, sizeof(temp)))
      {
	print_fatal_error(data, "Missing IF-DEFINED name on line %d of \"%s\".", f->linenum, f->filename);
	return (0);
      }

      if (data->last_expect)
      {
	data->last_expect->if_defined = strdup(temp);
      }
      else if (data->last_status)
      {
	data->last_status->if_defined = strdup(temp);
      }
      else
      {
	print_fatal_error(data, "IF-DEFINED without a preceding EXPECT or STATUS on line %d of \"%s\".", f->linenum, f->filename);
	return (0);
      }
    }
    else if (!_cups_strcasecmp(token, "IF-NOT-DEFINED"))
    {
      if (!_ippFileReadToken(f, temp, sizeof(temp)))
      {
	print_fatal_error(data, "Missing IF-NOT-DEFINED name on line %d of \"%s\".", f->linenum, f->filename);
	return (0);
      }

      if (data->last_expect)
      {
	data->last_expect->if_not_defined = strdup(temp);
      }
      else if (data->last_status)
      {
	data->last_status->if_not_defined = strdup(temp);
      }
      else
      {
	print_fatal_error(data, "IF-NOT-DEFINED without a preceding EXPECT or STATUS on line %d of \"%s\".", f->linenum, f->filename);
	return (0);
      }
    }
    else if (!_cups_strcasecmp(token, "WITH-ALL-VALUES") ||
	     !_cups_strcasecmp(token, "WITH-ALL-HOSTNAMES") ||
	     !_cups_strcasecmp(token, "WITH-ALL-RESOURCES") ||
	     !_cups_strcasecmp(token, "WITH-ALL-SCHEMES") ||
	     !_cups_strcasecmp(token, "WITH-HOSTNAME") ||
	     !_cups_strcasecmp(token, "WITH-RESOURCE") ||
	     !_cups_strcasecmp(token, "WITH-SCHEME") ||
	     !_cups_strcasecmp(token, "WITH-VALUE"))
    {
      off_t	lastpos;		/* Last file position */
      int	lastline;		/* Last line number */

      if (data->last_expect)
      {
	if (!_cups_strcasecmp(token, "WITH-ALL-HOSTNAMES") || !_cups_strcasecmp(token, "WITH-HOSTNAME"))
	  data->last_expect->with_flags = _CUPS_WITH_HOSTNAME;
	else if (!_cups_strcasecmp(token, "WITH-ALL-RESOURCES") || !_cups_strcasecmp(token, "WITH-RESOURCE"))
	  data->last_expect->with_flags = _CUPS_WITH_RESOURCE;
	else if (!_cups_strcasecmp(token, "WITH-ALL-SCHEMES") || !_cups_strcasecmp(token, "WITH-SCHEME"))
	  data->last_expect->with_flags = _CUPS_WITH_SCHEME;

	if (!_cups_strncasecmp(token, "WITH-ALL-", 9))
	  data->last_expect->with_flags |= _CUPS_WITH_ALL;
      }

      if (!_ippFileReadToken(f, temp, sizeof(temp)))
      {
	print_fatal_error(data, "Missing %s value on line %d of \"%s\".", token, f->linenum, f->filename);
	return (0);
      }

     /*
      * Read additional comma-delimited values - needed since legacy test files
      * will have unquoted WITH-VALUE values with commas...
      */

      ptr = temp + strlen(temp);

      for (;;)
      {
        lastpos  = cupsFileTell(f->fp);
        lastline = f->linenum;
        ptr      += strlen(ptr);

	if (!_ippFileReadToken(f, ptr, (sizeof(temp) - (size_t)(ptr - temp))))
	  break;

        if (!strcmp(ptr, ","))
        {
         /*
          * Append a value...
          */

	  ptr += strlen(ptr);

	  if (!_ippFileReadToken(f, ptr, (sizeof(temp) - (size_t)(ptr - temp))))
	    break;
        }
        else
        {
         /*
          * Not another value, stop here...
          */

          cupsFileSeek(f->fp, lastpos);
          f->linenum = lastline;
          *ptr = '\0';
          break;
	}
      }

      if (data->last_expect)
      {
       /*
	* Expand any variables in the value and then save it.
	*/

	_ippVarsExpand(vars, value, temp, sizeof(value));

	ptr = value + strlen(value) - 1;

	if (value[0] == '/' && ptr > value && *ptr == '/')
	{
	 /*
	  * WITH-VALUE is a POSIX extended regular expression.
	  */

	  data->last_expect->with_value = calloc(1, (size_t)(ptr - value));
	  data->last_expect->with_flags |= _CUPS_WITH_REGEX;

	  if (data->last_expect->with_value)
	    memcpy(data->last_expect->with_value, value + 1, (size_t)(ptr - value - 1));
	}
	else
	{
	 /*
	  * WITH-VALUE is a literal value...
	  */

	  for (ptr = value; *ptr; ptr ++)
	  {
	    if (*ptr == '\\' && ptr[1])
	    {
	     /*
	      * Remove \ from \foo...
	      */

	      _cups_strcpy(ptr, ptr + 1);
	    }
	  }

	  data->last_expect->with_value = strdup(value);
	  data->last_expect->with_flags |= _CUPS_WITH_LITERAL;
	}
      }
      else
      {
	print_fatal_error(data, "%s without a preceding EXPECT on line %d of \"%s\".", token, f->linenum, f->filename);
	return (0);
      }
    }
    else if (!_cups_strcasecmp(token, "WITH-VALUE-FROM"))
    {
      if (!_ippFileReadToken(f, temp, sizeof(temp)))
      {
	print_fatal_error(data, "Missing %s value on line %d of \"%s\".", token, f->linenum, f->filename);
	return (0);
      }

      if (data->last_expect)
      {
       /*
	* Expand any variables in the value and then save it.
	*/

	_ippVarsExpand(vars, value, temp, sizeof(value));

	data->last_expect->with_value_from = strdup(value);
	data->last_expect->with_flags      = _CUPS_WITH_LITERAL;
      }
      else
      {
	print_fatal_error(data, "%s without a preceding EXPECT on line %d of \"%s\".", token, f->linenum, f->filename);
	return (0);
      }
    }
    else if (!_cups_strcasecmp(token, "DISPLAY"))
    {
     /*
      * Display attributes...
      */

      if (data->num_displayed >= (int)(sizeof(data->displayed) / sizeof(data->displayed[0])))
      {
	print_fatal_error(data, "Too many DISPLAY's on line %d of \"%s\".", f->linenum, f->filename);
	return (0);
      }

      if (!_ippFileReadToken(f, temp, sizeof(temp)))
      {
	print_fatal_error(data, "Missing DISPLAY name on line %d of \"%s\".", f->linenum, f->filename);
	return (0);
      }

      data->displayed[data->num_displayed] = strdup(temp);
      data->num_displayed ++;
    }
    else
    {
      print_fatal_error(data, "Unexpected token %s seen on line %d of \"%s\".", token, f->linenum, f->filename);
      return (0);
    }
  }
  else
  {
   /*
    * Scan for the start of a test (open brace)...
    */

    if (!strcmp(token, "{"))
    {
     /*
      * Start new test...
      */

      if (data->show_header)
      {
	if (data->output == _CUPS_OUTPUT_PLIST)
	  print_xml_header(data);

	if (data->output == _CUPS_OUTPUT_TEST || (data->output == _CUPS_OUTPUT_PLIST && data->outfile != cupsFileStdout()))
	  cupsFilePrintf(cupsFileStdout(), "\"%s\":\n", f->filename);

	data->show_header = 0;
      }

      data->compression[0] = '\0';
      data->delay          = 0;
      data->num_expects    = 0;
      data->last_expect    = NULL;
      data->file[0]        = '\0';
      data->ignore_errors  = data->def_ignore_errors;
      strlcpy(data->name, f->filename, sizeof(data->name));
      if ((ptr = strrchr(data->name, '.')) != NULL)
        *ptr = '\0';
      data->repeat_interval = 5000000;
      data->request_id ++;
      strlcpy(data->resource, vars->resource, sizeof(data->resource));
      data->skip_previous = 0;
      data->skip_test     = 0;
      data->num_statuses  = 0;
      data->last_status   = NULL;
      data->test_id[0]    = '\0';
      data->transfer      = data->def_transfer;
      data->version       = data->def_version;

      _ippVarsSet(vars, "date-current", iso_date(ippTimeToDate(time(NULL))));

      f->attrs     = ippNew();
      f->group_tag = IPP_TAG_ZERO;
    }
    else if (!strcmp(token, "DEFINE"))
    {
     /*
      * DEFINE name value
      */

      if (_ippFileReadToken(f, name, sizeof(name)) && _ippFileReadToken(f, temp, sizeof(temp)))
      {
        _ippVarsSet(vars, "date-current", iso_date(ippTimeToDate(time(NULL))));
        _ippVarsExpand(vars, value, temp, sizeof(value));
	_ippVarsSet(vars, name, value);
      }
      else
      {
        print_fatal_error(data, "Missing DEFINE name and/or value on line %d of \"%s\".", f->linenum, f->filename);
	return (0);
      }
    }
    else if (!strcmp(token, "DEFINE-DEFAULT"))
    {
     /*
      * DEFINE-DEFAULT name value
      */

      if (_ippFileReadToken(f, name, sizeof(name)) && _ippFileReadToken(f, temp, sizeof(temp)))
      {
        if (!_ippVarsGet(vars, name))
        {
          _ippVarsSet(vars, "date-current", iso_date(ippTimeToDate(time(NULL))));
	  _ippVarsExpand(vars, value, temp, sizeof(value));
	  _ippVarsSet(vars, name, value);
	}
      }
      else
      {
        print_fatal_error(data, "Missing DEFINE-DEFAULT name and/or value on line %d of \"%s\".", f->linenum, f->filename);
	return (0);
      }
    }
    else if (!strcmp(token, "FILE-ID"))
    {
     /*
      * FILE-ID "string"
      */

      if (_ippFileReadToken(f, temp, sizeof(temp)))
      {
        _ippVarsSet(vars, "date-current", iso_date(ippTimeToDate(time(NULL))));
        _ippVarsExpand(vars, data->file_id, temp, sizeof(data->file_id));
      }
      else
      {
        print_fatal_error(data, "Missing FILE-ID value on line %d of \"%s\".", f->linenum, f->filename);
        return (0);
      }
    }
    else if (!strcmp(token, "IGNORE-ERRORS"))
    {
     /*
      * IGNORE-ERRORS yes
      * IGNORE-ERRORS no
      */

      if (_ippFileReadToken(f, temp, sizeof(temp)) && (!_cups_strcasecmp(temp, "yes") || !_cups_strcasecmp(temp, "no")))
      {
        data->def_ignore_errors = !_cups_strcasecmp(temp, "yes");
      }
      else
      {
        print_fatal_error(data, "Missing IGNORE-ERRORS value on line %d of \"%s\".", f->linenum, f->filename);
        return (0);
      }
    }
    else if (!strcmp(token, "INCLUDE"))
    {
     /*
      * INCLUDE "filename"
      * INCLUDE <filename>
      */

      if (_ippFileReadToken(f, temp, sizeof(temp)))
      {
       /*
        * Map the filename to and then run the tests...
	*/

        _cups_testdata_t inc_data;	/* Data for included file */
        char		filename[1024];	/* Mapped filename */

        memcpy(&inc_data, data, sizeof(inc_data));
        inc_data.http        = NULL;
	inc_data.pass        = 1;
	inc_data.prev_pass   = 1;
	inc_data.show_header = 1;

        if (!do_tests(get_filename(f->filename, filename, temp, sizeof(filename)), vars, &inc_data) && data->stop_after_include_error)
        {
          data->pass = data->prev_pass = 0;
          return (0);
	}
      }
      else
      {
        print_fatal_error(data, "Missing INCLUDE filename on line %d of \"%s\".", f->linenum, f->filename);
        return (0);
      }

      data->show_header = 1;
    }
    else if (!strcmp(token, "INCLUDE-IF-DEFINED"))
    {
     /*
      * INCLUDE-IF-DEFINED name "filename"
      * INCLUDE-IF-DEFINED name <filename>
      */

      if (_ippFileReadToken(f, name, sizeof(name)) && _ippFileReadToken(f, temp, sizeof(temp)))
      {
       /*
        * Map the filename to and then run the tests...
	*/

        _cups_testdata_t inc_data;	/* Data for included file */
        char		filename[1024];	/* Mapped filename */

        memcpy(&inc_data, data, sizeof(inc_data));
        inc_data.http        = NULL;
	inc_data.pass        = 1;
	inc_data.prev_pass   = 1;
	inc_data.show_header = 1;

        if (!do_tests(get_filename(f->filename, filename, temp, sizeof(filename)), vars, &inc_data) && data->stop_after_include_error)
        {
          data->pass = data->prev_pass = 0;
          return (0);
	}
      }
      else
      {
        print_fatal_error(data, "Missing INCLUDE-IF-DEFINED name or filename on line %d of \"%s\".", f->linenum, f->filename);
        return (0);
      }

      data->show_header = 1;
    }
    else if (!strcmp(token, "INCLUDE-IF-NOT-DEFINED"))
    {
     /*
      * INCLUDE-IF-NOT-DEFINED name "filename"
      * INCLUDE-IF-NOT-DEFINED name <filename>
      */

      if (_ippFileReadToken(f, name, sizeof(name)) && _ippFileReadToken(f, temp, sizeof(temp)))
      {
       /*
        * Map the filename to and then run the tests...
	*/

        _cups_testdata_t inc_data;	/* Data for included file */
        char		filename[1024];	/* Mapped filename */

        memcpy(&inc_data, data, sizeof(inc_data));
        inc_data.http        = NULL;
	inc_data.pass        = 1;
	inc_data.prev_pass   = 1;
	inc_data.show_header = 1;

        if (!do_tests(get_filename(f->filename, filename, temp, sizeof(filename)), vars, &inc_data) && data->stop_after_include_error)
        {
          data->pass = data->prev_pass = 0;
          return (0);
	}
      }
      else
      {
        print_fatal_error(data, "Missing INCLUDE-IF-NOT-DEFINED name or filename on line %d of \"%s\".", f->linenum, f->filename);
        return (0);
      }

      data->show_header = 1;
    }
    else if (!strcmp(token, "SKIP-IF-DEFINED"))
    {
     /*
      * SKIP-IF-DEFINED variable
      */

      if (_ippFileReadToken(f, name, sizeof(name)))
      {
        if (_ippVarsGet(vars, name))
          data->skip_test = 1;
      }
      else
      {
        print_fatal_error(data, "Missing SKIP-IF-DEFINED variable on line %d of \"%s\".", f->linenum, f->filename);
        return (0);
      }
    }
    else if (!strcmp(token, "SKIP-IF-NOT-DEFINED"))
    {
     /*
      * SKIP-IF-NOT-DEFINED variable
      */

      if (_ippFileReadToken(f, name, sizeof(name)))
      {
        if (!_ippVarsGet(vars, name))
          data->skip_test = 1;
      }
      else
      {
        print_fatal_error(data, "Missing SKIP-IF-NOT-DEFINED variable on line %d of \"%s\".", f->linenum, f->filename);
        return (0);
      }
    }
    else if (!strcmp(token, "STOP-AFTER-INCLUDE-ERROR"))
    {
     /*
      * STOP-AFTER-INCLUDE-ERROR yes
      * STOP-AFTER-INCLUDE-ERROR no
      */

      if (_ippFileReadToken(f, temp, sizeof(temp)) && (!_cups_strcasecmp(temp, "yes") || !_cups_strcasecmp(temp, "no")))
      {
        data->stop_after_include_error = !_cups_strcasecmp(temp, "yes");
      }
      else
      {
        print_fatal_error(data, "Missing STOP-AFTER-INCLUDE-ERROR value on line %d of \"%s\".", f->linenum, f->filename);
        return (0);
      }
    }
    else if (!strcmp(token, "TRANSFER"))
    {
     /*
      * TRANSFER auto
      * TRANSFER chunked
      * TRANSFER length
      */

      if (_ippFileReadToken(f, temp, sizeof(temp)))
      {
        if (!strcmp(temp, "auto"))
	  data->def_transfer = _CUPS_TRANSFER_AUTO;
	else if (!strcmp(temp, "chunked"))
	  data->def_transfer = _CUPS_TRANSFER_CHUNKED;
	else if (!strcmp(temp, "length"))
	  data->def_transfer = _CUPS_TRANSFER_LENGTH;
	else
	{
	  print_fatal_error(data, "Bad TRANSFER value \"%s\" on line %d of \"%s\".", temp, f->linenum, f->filename);
	  return (0);
	}
      }
      else
      {
        print_fatal_error(data, "Missing TRANSFER value on line %d of \"%s\".", f->linenum, f->filename);
	return (0);
      }
    }
    else if (!strcmp(token, "VERSION"))
    {
      if (_ippFileReadToken(f, temp, sizeof(temp)))
      {
        if (!strcmp(temp, "1.0"))
	  data->def_version = 10;
	else if (!strcmp(temp, "1.1"))
	  data->def_version = 11;
	else if (!strcmp(temp, "2.0"))
	  data->def_version = 20;
	else if (!strcmp(temp, "2.1"))
	  data->def_version = 21;
	else if (!strcmp(temp, "2.2"))
	  data->def_version = 22;
	else
	{
	  print_fatal_error(data, "Bad VERSION \"%s\" on line %d of \"%s\".", temp, f->linenum, f->filename);
	  return (0);
	}
      }
      else
      {
        print_fatal_error(data, "Missing VERSION number on line %d of \"%s\".", f->linenum, f->filename);
        return (0);
      }
    }
    else
    {
      print_fatal_error(data, "Unexpected token %s seen on line %d of \"%s\".", token, f->linenum, f->filename);
      return (0);
    }
  }

  return (1);
}


/*
 * 'usage()' - Show program usage.
 */

static void
usage(void)
{
  _cupsLangPuts(stderr, _("Usage: ipptool [options] URI filename [ ... filenameN ]"));
  _cupsLangPuts(stderr, _("Options:"));
  _cupsLangPuts(stderr, _("--ippserver filename    Produce ippserver attribute file"));
  _cupsLangPuts(stderr, _("--stop-after-include-error\n"
                          "                        Stop tests after a failed INCLUDE"));
  _cupsLangPuts(stderr, _("--version               Show version"));
  _cupsLangPuts(stderr, _("-4                      Connect using IPv4"));
  _cupsLangPuts(stderr, _("-6                      Connect using IPv6"));
  _cupsLangPuts(stderr, _("-C                      Send requests using chunking (default)"));
  _cupsLangPuts(stderr, _("-E                      Test with encryption using HTTP Upgrade to TLS"));
  _cupsLangPuts(stderr, _("-I                      Ignore errors"));
  _cupsLangPuts(stderr, _("-L                      Send requests using content-length"));
  _cupsLangPuts(stderr, _("-P filename.plist       Produce XML plist to a file and test report to standard output"));
  _cupsLangPuts(stderr, _("-S                      Test with encryption using HTTPS"));
  _cupsLangPuts(stderr, _("-T seconds              Set the receive/send timeout in seconds"));
  _cupsLangPuts(stderr, _("-V version              Set default IPP version"));
  _cupsLangPuts(stderr, _("-X                      Produce XML plist instead of plain text"));
  _cupsLangPuts(stderr, _("-c                      Produce CSV output"));
  _cupsLangPuts(stderr, _("-d name=value           Set named variable to value"));
  _cupsLangPuts(stderr, _("-f filename             Set default request filename"));
  _cupsLangPuts(stderr, _("-h                      Validate HTTP response headers"));
  _cupsLangPuts(stderr, _("-i seconds              Repeat the last file with the given time interval"));
  _cupsLangPuts(stderr, _("-l                      Produce plain text output"));
  _cupsLangPuts(stderr, _("-n count                Repeat the last file the given number of times"));
  _cupsLangPuts(stderr, _("-q                      Run silently"));
  _cupsLangPuts(stderr, _("-t                      Produce a test report"));
  _cupsLangPuts(stderr, _("-v                      Be verbose"));

  exit(1);
}


/*
 * 'with_flags_string()' - Return the "WITH-xxx" predicate that corresponds to
                           the flags.
 */

static const char *                     /* O - WITH-xxx string */
with_flags_string(int flags)            /* I - WITH flags */
{
  if (flags & _CUPS_WITH_ALL)
  {
    if (flags & _CUPS_WITH_HOSTNAME)
      return ("WITH-ALL-HOSTNAMES");
    else if (flags & _CUPS_WITH_RESOURCE)
      return ("WITH-ALL-RESOURCES");
    else if (flags & _CUPS_WITH_SCHEME)
      return ("WITH-ALL-SCHEMES");
    else
      return ("WITH-ALL-VALUES");
  }
  else if (flags & _CUPS_WITH_HOSTNAME)
    return ("WITH-HOSTNAME");
  else if (flags & _CUPS_WITH_RESOURCE)
    return ("WITH-RESOURCE");
  else if (flags & _CUPS_WITH_SCHEME)
    return ("WITH-SCHEME");
  else
    return ("WITH-VALUE");
}


/*
 * 'with_value()' - Test a WITH-VALUE predicate.
 */

static int				/* O - 1 on match, 0 on non-match */
with_value(_cups_testdata_t *data,	/* I - Test data */
           cups_array_t     *errors,	/* I - Errors array */
           char             *value,	/* I - Value string */
           int              flags,	/* I - Flags for match */
           ipp_attribute_t  *attr,	/* I - Attribute to compare */
	   char             *matchbuf,	/* I - Buffer to hold matching value */
	   size_t           matchlen)	/* I - Length of match buffer */
{
  int		i,			/* Looping var */
    		count,			/* Number of values */
		match;			/* Match? */
  char		temp[1024],		/* Temporary value string */
		*valptr;		/* Pointer into value */
  const char	*name;			/* Attribute name */


  *matchbuf = '\0';
  match     = (flags & _CUPS_WITH_ALL) ? 1 : 0;

 /*
  * NULL matches everything.
  */

  if (!value || !*value)
    return (1);

 /*
  * Compare the value string to the attribute value.
  */

  name  = ippGetName(attr);
  count = ippGetCount(attr);

  switch (ippGetValueTag(attr))
  {
    case IPP_TAG_INTEGER :
    case IPP_TAG_ENUM :
        for (i = 0; i < count; i ++)
        {
	  char	op,			/* Comparison operator */
	  	*nextptr;		/* Next pointer */
	  int	intvalue,		/* Integer value */
		attrvalue = ippGetInteger(attr, i),
					/* Attribute value */
	  	valmatch = 0;		/* Does the current value match? */

          valptr = value;

	  while (isspace(*valptr & 255) || isdigit(*valptr & 255) ||
		 *valptr == '-' || *valptr == ',' || *valptr == '<' ||
		 *valptr == '=' || *valptr == '>')
	  {
	    op = '=';
	    while (*valptr && !isdigit(*valptr & 255) && *valptr != '-')
	    {
	      if (*valptr == '<' || *valptr == '>' || *valptr == '=')
		op = *valptr;
	      valptr ++;
	    }

            if (!*valptr)
	      break;

	    intvalue = (int)strtol(valptr, &nextptr, 0);
	    if (nextptr == valptr)
	      break;
	    valptr = nextptr;

            if ((op == '=' && attrvalue == intvalue) ||
                (op == '<' && attrvalue < intvalue) ||
                (op == '>' && attrvalue > intvalue))
	    {
	      if (!matchbuf[0])
		snprintf(matchbuf, matchlen, "%d", attrvalue);

	      valmatch = 1;
	      break;
	    }
	  }

          if (flags & _CUPS_WITH_ALL)
          {
            if (!valmatch)
            {
              match = 0;
              break;
            }
          }
          else if (valmatch)
          {
            match = 1;
            break;
          }
        }

        if (!match && errors)
	{
	  for (i = 0; i < count; i ++)
	    add_stringf(data->errors, "GOT: %s=%d", name, ippGetInteger(attr, i));
	}
	break;

    case IPP_TAG_RANGE :
        for (i = 0; i < count; i ++)
        {
	  char	op,			/* Comparison operator */
	  	*nextptr;		/* Next pointer */
	  int	intvalue,		/* Integer value */
	        lower,			/* Lower range */
	        upper,			/* Upper range */
	  	valmatch = 0;		/* Does the current value match? */

	  lower = ippGetRange(attr, i, &upper);
          valptr = value;

	  while (isspace(*valptr & 255) || isdigit(*valptr & 255) ||
		 *valptr == '-' || *valptr == ',' || *valptr == '<' ||
		 *valptr == '=' || *valptr == '>')
	  {
	    op = '=';
	    while (*valptr && !isdigit(*valptr & 255) && *valptr != '-')
	    {
	      if (*valptr == '<' || *valptr == '>' || *valptr == '=')
		op = *valptr;
	      valptr ++;
	    }

            if (!*valptr)
	      break;

	    intvalue = (int)strtol(valptr, &nextptr, 0);
	    if (nextptr == valptr)
	      break;
	    valptr = nextptr;

            if ((op == '=' && (lower == intvalue || upper == intvalue)) ||
		(op == '<' && upper < intvalue) ||
		(op == '>' && upper > intvalue))
	    {
	      if (!matchbuf[0])
		snprintf(matchbuf, matchlen, "%d-%d", lower, upper);

	      valmatch = 1;
	      break;
	    }
	  }

          if (flags & _CUPS_WITH_ALL)
          {
            if (!valmatch)
            {
              match = 0;
              break;
            }
          }
          else if (valmatch)
          {
            match = 1;
            break;
          }
        }

        if (!match && errors)
	{
	  for (i = 0; i < count; i ++)
	  {
	    int lower, upper;		/* Range values */

	    lower = ippGetRange(attr, i, &upper);
	    add_stringf(data->errors, "GOT: %s=%d-%d", name, lower, upper);
	  }
	}
	break;

    case IPP_TAG_BOOLEAN :
	for (i = 0; i < count; i ++)
	{
          if ((!strcmp(value, "true") || !strcmp(value, "1")) == ippGetBoolean(attr, i))
          {
            if (!matchbuf[0])
	      strlcpy(matchbuf, value, matchlen);

	    if (!(flags & _CUPS_WITH_ALL))
	    {
	      match = 1;
	      break;
	    }
	  }
	  else if (flags & _CUPS_WITH_ALL)
	  {
	    match = 0;
	    break;
	  }
	}

	if (!match && errors)
	{
	  for (i = 0; i < count; i ++)
	    add_stringf(data->errors, "GOT: %s=%s", name, ippGetBoolean(attr, i) ? "true" : "false");
	}
	break;

    case IPP_TAG_RESOLUTION :
	for (i = 0; i < count; i ++)
	{
	  int		xres, yres;	/* Resolution values */
	  ipp_res_t	units;		/* Resolution units */

	  xres = ippGetResolution(attr, i, &yres, &units);
	  if (xres == yres)
	    snprintf(temp, sizeof(temp), "%d%s", xres, units == IPP_RES_PER_INCH ? "dpi" : "dpcm");
	  else
	    snprintf(temp, sizeof(temp), "%dx%d%s", xres, yres, units == IPP_RES_PER_INCH ? "dpi" : "dpcm");

          if (!strcmp(value, temp))
          {
            if (!matchbuf[0])
	      strlcpy(matchbuf, value, matchlen);

	    if (!(flags & _CUPS_WITH_ALL))
	    {
	      match = 1;
	      break;
	    }
	  }
	  else if (flags & _CUPS_WITH_ALL)
	  {
	    match = 0;
	    break;
	  }
	}

	if (!match && errors)
	{
	  for (i = 0; i < count; i ++)
	  {
	    int		xres, yres;	/* Resolution values */
	    ipp_res_t	units;		/* Resolution units */

	    xres = ippGetResolution(attr, i, &yres, &units);
	    if (xres == yres)
	      snprintf(temp, sizeof(temp), "%d%s", xres, units == IPP_RES_PER_INCH ? "dpi" : "dpcm");
	    else
	      snprintf(temp, sizeof(temp), "%dx%d%s", xres, yres, units == IPP_RES_PER_INCH ? "dpi" : "dpcm");

            if (strcmp(value, temp))
	      add_stringf(data->errors, "GOT: %s=%s", name, temp);
	  }
	}
	break;

    case IPP_TAG_NOVALUE :
    case IPP_TAG_UNKNOWN :
	return (1);

    case IPP_TAG_CHARSET :
    case IPP_TAG_KEYWORD :
    case IPP_TAG_LANGUAGE :
    case IPP_TAG_MIMETYPE :
    case IPP_TAG_NAME :
    case IPP_TAG_NAMELANG :
    case IPP_TAG_TEXT :
    case IPP_TAG_TEXTLANG :
    case IPP_TAG_URI :
    case IPP_TAG_URISCHEME :
        if (flags & _CUPS_WITH_REGEX)
	{
	 /*
	  * Value is an extended, case-sensitive POSIX regular expression...
	  */

	  regex_t	re;		/* Regular expression */

          if ((i = regcomp(&re, value, REG_EXTENDED | REG_NOSUB)) != 0)
	  {
            regerror(i, &re, temp, sizeof(temp));

	    print_fatal_error(data, "Unable to compile WITH-VALUE regular expression \"%s\" - %s", value, temp);
	    return (0);
	  }

         /*
	  * See if ALL of the values match the given regular expression.
	  */

	  for (i = 0; i < count; i ++)
	  {
	    if (!regexec(&re, get_string(attr, i, flags, temp, sizeof(temp)),
	                 0, NULL, 0))
	    {
	      if (!matchbuf[0])
		strlcpy(matchbuf, get_string(attr, i, flags, temp, sizeof(temp)), matchlen);

	      if (!(flags & _CUPS_WITH_ALL))
	      {
	        match = 1;
	        break;
	      }
	    }
	    else if (flags & _CUPS_WITH_ALL)
	    {
	      match = 0;
	      break;
	    }
	  }

	  regfree(&re);
	}
	else if (ippGetValueTag(attr) == IPP_TAG_URI && !(flags & (_CUPS_WITH_SCHEME | _CUPS_WITH_HOSTNAME | _CUPS_WITH_RESOURCE)))
	{
	 /*
	  * Value is a literal URI string, see if the value(s) match...
	  */

	  for (i = 0; i < count; i ++)
	  {
	    if (!compare_uris(value, get_string(attr, i, flags, temp, sizeof(temp))))
	    {
	      if (!matchbuf[0])
		strlcpy(matchbuf, get_string(attr, i, flags, temp, sizeof(temp)), matchlen);

	      if (!(flags & _CUPS_WITH_ALL))
	      {
	        match = 1;
	        break;
	      }
	    }
	    else if (flags & _CUPS_WITH_ALL)
	    {
	      match = 0;
	      break;
	    }
	  }
	}
	else
	{
	 /*
	  * Value is a literal string, see if the value(s) match...
	  */

	  for (i = 0; i < count; i ++)
	  {
	    int result;

            switch (ippGetValueTag(attr))
            {
              case IPP_TAG_URI :
                 /*
                  * Some URI components are case-sensitive, some not...
                  */

                  if (flags & (_CUPS_WITH_SCHEME | _CUPS_WITH_HOSTNAME))
                    result = _cups_strcasecmp(value, get_string(attr, i, flags, temp, sizeof(temp)));
                  else
                    result = strcmp(value, get_string(attr, i, flags, temp, sizeof(temp)));
                  break;

              case IPP_TAG_MIMETYPE :
              case IPP_TAG_NAME :
              case IPP_TAG_NAMELANG :
              case IPP_TAG_TEXT :
              case IPP_TAG_TEXTLANG :
                 /*
                  * mimeMediaType, nameWithoutLanguage, nameWithLanguage,
                  * textWithoutLanguage, and textWithLanguage are defined to
                  * be case-insensitive strings...
                  */

                  result = _cups_strcasecmp(value, get_string(attr, i, flags, temp, sizeof(temp)));
                  break;

              default :
                 /*
                  * Other string syntaxes are defined as lowercased so we use
                  * case-sensitive comparisons to catch problems...
                  */

                  result = strcmp(value, get_string(attr, i, flags, temp, sizeof(temp)));
                  break;
            }

            if (!result)
	    {
	      if (!matchbuf[0])
		strlcpy(matchbuf, get_string(attr, i, flags, temp, sizeof(temp)), matchlen);

	      if (!(flags & _CUPS_WITH_ALL))
	      {
	        match = 1;
	        break;
	      }
	    }
	    else if (flags & _CUPS_WITH_ALL)
	    {
	      match = 0;
	      break;
	    }
	  }
	}

        if (!match && errors)
        {
	  for (i = 0; i < count; i ++)
	    add_stringf(data->errors, "GOT: %s=\"%s\"", name, ippGetString(attr, i, NULL));
        }
	break;

    case IPP_TAG_STRING :
        if (flags & _CUPS_WITH_REGEX)
	{
	 /*
	  * Value is an extended, case-sensitive POSIX regular expression...
	  */

	  void		*adata;		/* Pointer to octetString data */
	  int		adatalen;	/* Length of octetString */
	  regex_t	re;		/* Regular expression */

          if ((i = regcomp(&re, value, REG_EXTENDED | REG_NOSUB)) != 0)
	  {
            regerror(i, &re, temp, sizeof(temp));

	    print_fatal_error(data, "Unable to compile WITH-VALUE regular expression \"%s\" - %s", value, temp);
	    return (0);
	  }

         /*
	  * See if ALL of the values match the given regular expression.
	  */

	  for (i = 0; i < count; i ++)
	  {
            if ((adata = ippGetOctetString(attr, i, &adatalen)) == NULL || adatalen >= (int)sizeof(temp))
            {
              match = 0;
              break;
            }
            memcpy(temp, adata, (size_t)adatalen);
            temp[adatalen] = '\0';

	    if (!regexec(&re, temp, 0, NULL, 0))
	    {
	      if (!matchbuf[0])
		strlcpy(matchbuf, temp, matchlen);

	      if (!(flags & _CUPS_WITH_ALL))
	      {
	        match = 1;
	        break;
	      }
	    }
	    else if (flags & _CUPS_WITH_ALL)
	    {
	      match = 0;
	      break;
	    }
	  }

	  regfree(&re);

	  if (!match && errors)
	  {
	    for (i = 0; i < count; i ++)
	    {
	      adata = ippGetOctetString(attr, i, &adatalen);
	      copy_hex_string(temp, adata, adatalen, sizeof(temp));
	      add_stringf(data->errors, "GOT: %s=\"%s\"", name, temp);
	    }
	  }
	}
	else
        {
         /*
          * Value is a literal or hex-encoded string...
          */

          unsigned char	withdata[1023],	/* WITH-VALUE data */
			*adata;		/* Pointer to octetString data */
	  int		withlen,	/* Length of WITH-VALUE data */
			adatalen;	/* Length of octetString */

          if (*value == '<')
          {
           /*
            * Grab hex-encoded value...
            */

            if ((withlen = (int)strlen(value)) & 1 || withlen > (int)(2 * (sizeof(withdata) + 1)))
            {
	      print_fatal_error(data, "Bad WITH-VALUE hex value.");
              return (0);
	    }

	    withlen = withlen / 2 - 1;

            for (valptr = value + 1, adata = withdata; *valptr; valptr += 2)
            {
              int ch;			/* Current character/byte */

	      if (isdigit(valptr[0]))
	        ch = (valptr[0] - '0') << 4;
	      else if (isalpha(valptr[0]))
	        ch = (tolower(valptr[0]) - 'a' + 10) << 4;
	      else
	        break;

	      if (isdigit(valptr[1]))
	        ch |= valptr[1] - '0';
	      else if (isalpha(valptr[1]))
	        ch |= tolower(valptr[1]) - 'a' + 10;
	      else
	        break;

	      *adata++ = (unsigned char)ch;
	    }

	    if (*valptr)
	    {
	      print_fatal_error(data, "Bad WITH-VALUE hex value.");
              return (0);
	    }
          }
          else
          {
           /*
            * Copy literal string value...
            */

            withlen = (int)strlen(value);

            memcpy(withdata, value, (size_t)withlen);
	  }

	  for (i = 0; i < count; i ++)
	  {
	    adata = ippGetOctetString(attr, i, &adatalen);

	    if (withlen == adatalen && !memcmp(withdata, adata, (size_t)withlen))
	    {
	      if (!matchbuf[0])
                copy_hex_string(matchbuf, adata, adatalen, matchlen);

	      if (!(flags & _CUPS_WITH_ALL))
	      {
	        match = 1;
	        break;
	      }
	    }
	    else if (flags & _CUPS_WITH_ALL)
	    {
	      match = 0;
	      break;
	    }
	  }

	  if (!match && errors)
	  {
	    for (i = 0; i < count; i ++)
	    {
	      adata = ippGetOctetString(attr, i, &adatalen);
	      copy_hex_string(temp, adata, adatalen, sizeof(temp));
	      add_stringf(data->errors, "GOT: %s=\"%s\"", name, temp);
	    }
	  }
        }
        break;

    default :
        break;
  }

  return (match);
}


/*
 * 'with_value_from()' - Test a WITH-VALUE-FROM predicate.
 */

static int				/* O - 1 on match, 0 on non-match */
with_value_from(
    cups_array_t    *errors,		/* I - Errors array */
    ipp_attribute_t *fromattr,		/* I - "From" attribute */
    ipp_attribute_t *attr,		/* I - Attribute to compare */
    char            *matchbuf,		/* I - Buffer to hold matching value */
    size_t          matchlen)		/* I - Length of match buffer */
{
  int	i, j,				/* Looping vars */
	count = ippGetCount(attr),	/* Number of attribute values */
	match = 1;			/* Match? */


  *matchbuf = '\0';

 /*
  * Compare the from value(s) to the attribute value(s)...
  */

  switch (ippGetValueTag(attr))
  {
    case IPP_TAG_INTEGER :
        if (ippGetValueTag(fromattr) != IPP_TAG_INTEGER && ippGetValueTag(fromattr) != IPP_TAG_RANGE)
	  goto wrong_value_tag;

	for (i = 0; i < count; i ++)
	{
	  int value = ippGetInteger(attr, i);
					/* Current integer value */

	  if (ippContainsInteger(fromattr, value))
	  {
	    if (!matchbuf[0])
	      snprintf(matchbuf, matchlen, "%d", value);
	  }
	  else
	  {
	    add_stringf(errors, "GOT: %s=%d", ippGetName(attr), value);
	    match = 0;
	  }
	}
	break;

    case IPP_TAG_ENUM :
        if (ippGetValueTag(fromattr) != IPP_TAG_ENUM)
	  goto wrong_value_tag;

	for (i = 0; i < count; i ++)
	{
	  int value = ippGetInteger(attr, i);
					/* Current integer value */

	  if (ippContainsInteger(fromattr, value))
	  {
	    if (!matchbuf[0])
	      snprintf(matchbuf, matchlen, "%d", value);
	  }
	  else
	  {
	    add_stringf(errors, "GOT: %s=%d", ippGetName(attr), value);
	    match = 0;
	  }
	}
	break;

    case IPP_TAG_RESOLUTION :
        if (ippGetValueTag(fromattr) != IPP_TAG_RESOLUTION)
	  goto wrong_value_tag;

	for (i = 0; i < count; i ++)
	{
	  int xres, yres;
	  ipp_res_t units;
          int fromcount = ippGetCount(fromattr);
	  int fromxres, fromyres;
	  ipp_res_t fromunits;

	  xres = ippGetResolution(attr, i, &yres, &units);

          for (j = 0; j < fromcount; j ++)
	  {
	    fromxres = ippGetResolution(fromattr, j, &fromyres, &fromunits);
	    if (fromxres == xres && fromyres == yres && fromunits == units)
	      break;
	  }

	  if (j < fromcount)
	  {
	    if (!matchbuf[0])
	    {
	      if (xres == yres)
	        snprintf(matchbuf, matchlen, "%d%s", xres, units == IPP_RES_PER_INCH ? "dpi" : "dpcm");
	      else
	        snprintf(matchbuf, matchlen, "%dx%d%s", xres, yres, units == IPP_RES_PER_INCH ? "dpi" : "dpcm");
	    }
	  }
	  else
	  {
	    if (xres == yres)
	      add_stringf(errors, "GOT: %s=%d%s", ippGetName(attr), xres, units == IPP_RES_PER_INCH ? "dpi" : "dpcm");
	    else
	      add_stringf(errors, "GOT: %s=%dx%d%s", ippGetName(attr), xres, yres, units == IPP_RES_PER_INCH ? "dpi" : "dpcm");

	    match = 0;
	  }
	}
	break;

    case IPP_TAG_NOVALUE :
    case IPP_TAG_UNKNOWN :
	return (1);

    case IPP_TAG_CHARSET :
    case IPP_TAG_KEYWORD :
    case IPP_TAG_LANGUAGE :
    case IPP_TAG_MIMETYPE :
    case IPP_TAG_NAME :
    case IPP_TAG_NAMELANG :
    case IPP_TAG_TEXT :
    case IPP_TAG_TEXTLANG :
    case IPP_TAG_URISCHEME :
	for (i = 0; i < count; i ++)
	{
	  const char *value = ippGetString(attr, i, NULL);
					/* Current string value */

	  if (ippContainsString(fromattr, value))
	  {
	    if (!matchbuf[0])
	      strlcpy(matchbuf, value, matchlen);
	  }
	  else
	  {
	    add_stringf(errors, "GOT: %s='%s'", ippGetName(attr), value);
	    match = 0;
	  }
	}
	break;

    case IPP_TAG_URI :
	for (i = 0; i < count; i ++)
	{
	  const char *value = ippGetString(attr, i, NULL);
					/* Current string value */
          int fromcount = ippGetCount(fromattr);

          for (j = 0; j < fromcount; j ++)
          {
            if (!compare_uris(value, ippGetString(fromattr, j, NULL)))
            {
              if (!matchbuf[0])
                strlcpy(matchbuf, value, matchlen);
              break;
            }
          }

	  if (j >= fromcount)
	  {
	    add_stringf(errors, "GOT: %s='%s'", ippGetName(attr), value);
	    match = 0;
	  }
	}
	break;

    default :
        match = 0;
        break;
  }

  return (match);

  /* value tag mismatch between fromattr and attr */
  wrong_value_tag :

  add_stringf(errors, "GOT: %s OF-TYPE %s", ippGetName(attr), ippTagString(ippGetValueTag(attr)));

  return (0);
}
