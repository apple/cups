/*
 * ipptool command for CUPS.
 *
 * Copyright © 2007-2018 by Apple Inc.
 * Copyright © 1997-2007 by Easy Software Products.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more
 * information.
 */

/*
 * Include necessary headers...
 */

#include <cups/cups-private.h>
#include <cups/file-private.h>
#include <regex.h>
#include <sys/stat.h>
#ifdef WIN32
#  include <windows.h>
#  ifndef R_OK
#    define R_OK 0
#  endif /* !R_OK */
#else
#  include <signal.h>
#  include <termios.h>
#endif /* WIN32 */
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
  int		test_count,		/* Number of tests (total) */
		pass_count,		/* Number of tests that passed */
		fail_count,		/* Number of tests that failed */
		skip_count;		/* Number of tests that were skipped */

  /* Per-Test State */
  cups_array_t	*errors;		/* Errors array */
  int		pass,			/* Did we pass the current test? */
		prev_pass,		/* Did we pass the previous test? */
		skip_previous,		/* Skip on previous test failure? */
		repeat_test;		/* Repeat a test? */
  char		compression[16];	/* COMPRESSION value */
  useconds_t	delay;                  /* Initial delay */
  int		num_displayed;		/* Number of displayed attributes */
  char		*displayed[200];	/* Displayed attributes */
  int		num_expects;		/* Number of expected attributes */
  _cups_expect_t expects[200],		/* Expected attributes */
		*expect,		/* Current expected attribute */
		*last_expect;		/* Last EXPECT (for predicates) */
  char		file_id[1024],		/* File identifier */
		filename[1024];		/* Data filename */
  int		ignore_errors;		/* Ignore test failures? */
  char		name[1024];		/* Test name */
  int	        repeat_count;		/* Repeat count */
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

static void	add_stringf(cups_array_t *a, const char *s, ...) __attribute__ ((__format__ (__printf__, 2, 3)));
static int      compare_uris(const char *a, const char *b);
static int	do_test(_cups_testdata_t *data, _ipp_vars_t *vars, _ipp_file_t *f);
static int	do_tests(_cups_testdata_t *data, _ipp_vars_t *vars, const char *testfile);
static int	error_cb(_ipp_file_t *f, _cups_testdata_t *data, const char *error);
static int      expect_matches(_cups_expect_t *expect, ipp_tag_t value_tag);
static char	*get_filename(const char *testfile, char *dst, const char *src, size_t dstsize);
static const char *get_string(ipp_attribute_t *attr, int element, int flags, char *buffer, size_t bufsize);
static void	init_data(_cups_testdata_t *data);
static char	*iso_date(const ipp_uchar_t *date);
static void	pause_message(const char *message);
static void	print_attr(_cups_testdata_t *data, ipp_attribute_t *attr, ipp_tag_t *group);
static void	print_csv(_cups_testdata_t *data, ipp_attribute_t *attr, int num_displayed, char **displayed, size_t *widths);
static void	print_fatal_error(_cups_testdata_t *data, const char *s, ...) __attribute__ ((__format__ (__printf__, 2, 3)));
static void	print_ippserver_attr(_cups_testdata_t *data, ipp_attribute_t *attr, int indent);
static void	print_ippserver_string(_cups_testdata_t *data, const char *s, size_t len);
static void	print_line(_cups_testdata_t *data, ipp_attribute_t *attr, int num_displayed, char **displayed, size_t *widths);
static void	print_xml_header(_cups_testdata_t *data);
static void	print_xml_string(_cups_testdata_t *data, const char *element, const char *s);
static void	print_xml_trailer(_cups_testdata_t *data, int success, const char *message);
#ifndef WIN32
static void	sigterm_handler(int sig);
#endif /* WIN32 */
static int	timeout_cb(http_t *http, void *user_data);
static int	token_cb(_ipp_file_t *f, _ipp_vars_t *vars, _cups_testdata_t *data, const char *token);
static void	usage(void) __attribute__((noreturn));
static int	validate_attr(cups_file_t *outfile, cups_array_t *errors, ipp_attribute_t *attr);
static const char *with_flags_string(int flags);
static int      with_value(cups_file_t *outfile, cups_array_t *errors, char *value, int flags, ipp_attribute_t *attr, char *matchbuf, size_t matchlen);
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
			testname[1024],	/* Real test filename */
			uri[1024];	/* Copy of printer URI */
  const char		*ext,		/* Extension on filename */
			*testfile;	/* Test file to use */
  int			interval,	/* Test interval in microseconds */
			repeat;		/* Repeat count */
  _cups_testdata_t	data;		/* Test data */
  _ipp_vars_t		vars;		/* Variables */
  http_uri_status_t	uri_status;	/* URI separation status */
  _cups_globals_t	*cg = _cupsGlobals();
					/* Global data */


#ifndef WIN32
 /*
  * Catch SIGINT and SIGTERM...
  */

  signal(SIGINT, sigterm_handler);
  signal(SIGTERM, sigterm_handler);
#endif /* !WIN32 */

 /*
  * Initialize the locale and variables...
  */

  _cupsSetLocale(argv);

  init_data(&data);

  _ippVarsInit(&vars);

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
      StopAfterIncludeError = 1;
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
              Transfer = _CUPS_TRANSFER_CHUNKED;
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
	      IgnoreErrors = 1;
	      break;

          case 'L' : /* Disable HTTP chunking */
              Transfer = _CUPS_TRANSFER_LENGTH;
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
	        Version = 10;
	      else if (!strcmp(argv[i], "1.1"))
	        Version = 11;
	      else if (!strcmp(argv[i], "2.0"))
	        Version = 20;
	      else if (!strcmp(argv[i], "2.1"))
	        Version = 21;
	      else if (!strcmp(argv[i], "2.2"))
	        Version = 22;
	      else
	      {
		_cupsLangPrintf(stderr,
		                _("%s: Bad version %s for \"-V\"."),
				"ipptool", argv[i]);
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

              if (data.filename)
              {
		free(data.filename);
		data.filename = NULL;
	      }

              if (access(argv[i], 0))
              {
               /*
                * Try filename.gz...
                */

		snprintf(filename, sizeof(filename), "%s.gz", argv[i]);
                if (access(filename, 0) && filename[0] != '/'
#ifdef WIN32
                    && (!isalpha(filename[0] & 255) || filename[1] != ':')
#endif /* WIN32 */
                    )
		{
		  snprintf(filename, sizeof(filename), "%s/ipptool/%s",
			   cg->cups_datadir, argv[i]);
		  if (access(filename, 0))
		  {
		    snprintf(filename, sizeof(filename), "%s/ipptool/%s.gz",
			     cg->cups_datadir, argv[i]);
		    if (access(filename, 0))
		      data.filename = strdup(argv[i]);
		    else
		      data.filename = strdup(filename);
		  }
		  else
		    data.filename = strdup(filename);
		}
		else
		  data.filename = strdup(filename);
	      }
              else
		data.filename = strdup(argv[i]);

              if ((ext = strrchr(data.filename, '.')) != NULL)
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
              ValidateHeaders = 1;
              break;

          case 'i' : /* Test every N seconds */
	      i ++;

	      if (i >= argc)
	      {
		_cupsLangPuts(stderr,
		              _("ipptool: Missing seconds for \"-i\"."));
		usage();
              }
	      else
	      {
		interval = (int)(_cupsStrScand(argv[i], NULL, localeconv()) *
		                 1000000.0);
		if (interval <= 0)
		{
		  _cupsLangPuts(stderr,
				_("ipptool: Invalid seconds for \"-i\"."));
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
		_cupsLangPuts(stderr,
		              _("ipptool: Missing count for \"-n\"."));
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
	      Verbosity ++;
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
#ifdef WIN32
          && (!isalpha(argv[i][0] & 255) || argv[i][1] != ':')
#endif /* WIN32 */
          )
      {
        snprintf(testname, sizeof(testname), "%s/ipptool/%s", cg->cups_datadir,
                 argv[i]);
        if (access(testname, 0))
          testfile = argv[i];
        else
          testfile = testname;
      }
      else
        testfile = argv[i];

      if (!do_tests(&data, &vars, testfile))
        status = 1;
    }
  }

  if (!vars.uri || !testfile)
    usage();

 /*
  * Loop if the interval is set...
  */

  if (data.output == _CUPS_OUTPUT_PLIST)
    print_xml_trailer(data->outfile, !status, NULL);
  else if (interval > 0 && repeat > 0)
  {
    while (repeat > 1)
    {
      usleep((useconds_t)interval);
      do_tests(data->outfile, &vars, testfile);
      repeat --;
    }
  }
  else if (interval > 0)
  {
    for (;;)
    {
      usleep((useconds_t)interval);
      do_tests(data->outfile, &vars, testfile);
    }
  }

  if ((Output == _CUPS_OUTPUT_TEST || (Output == _CUPS_OUTPUT_PLIST && outfile)) && TestCount > 1)
  {
   /*
    * Show a summary report if there were multiple tests...
    */

    cupsFilePrintf(cupsFileStdout(), "\nSummary: %d tests, %d passed, %d failed, %d skipped\nScore: %d%%\n", TestCount, PassCount, FailCount, SkipCount, 100 * (PassCount + SkipCount) / TestCount);
  }

  cupsFileClose(data->outfile);

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
 * 'do_test()' - Do a single test from the test file.
 */

static int				/* O - 1 on success, 0 on failure */
do_test(_cups_testdata_t *data,		/* I - Test data */
        _ipp_vars_t      *vars,		/* I - IPP variables */
        _ipp_file_t      *f)		/* I - IPP data file */
{
  return (0);
}


/*
 * 'do_tests()' - Do tests as specified in the test file.
 */

static int				/* O - 1 on success, 0 on failure */
do_tests(_cups_testdata_t *data,	/* I - Test data */
         _cups_vars_t     *vars,	/* I - Variables */
         const char       *testfile)	/* I - Test file to use */
{
  int		i,			/* Looping var */
		linenum,		/* Current line number */
		pass,			/* Did we pass the test? */
		prev_pass = 1,		/* Did we pass the previous test? */
		request_id,		/* Current request ID */
		show_header = 1,	/* Show the test header? */
		ignore_errors,		/* Ignore test failures? */
		skip_previous = 0,	/* Skip on previous test failure? */
		repeat_count,		/* Repeat count */
		repeat_test;		/* Repeat a test? */
  useconds_t	delay,                  /* Initial delay */
		repeat_interval;	/* Repeat interval (delay) */
  http_t	*http = NULL;		/* HTTP connection to server */
  cups_file_t	*fp = NULL;		/* Test file */
  char		resource[512],		/* Resource for request */
		token[1024],		/* Token from file */
		*tokenptr,		/* Pointer into token */
		temp[1024],		/* Temporary string */
		buffer[131072],		/* Copy buffer */
		compression[16];	/* COMPRESSION value */
  ipp_t		*request = NULL,	/* IPP request */
		*response = NULL;	/* IPP response */
  size_t	length;			/* Length of IPP request */
  http_status_t	status;			/* HTTP status */
  cups_file_t	*reqfile;		/* File to send */
  ssize_t	bytes;			/* Bytes read/written */
  char		attr[128];		/* Attribute name */
  ipp_op_t	op;			/* Operation */
  ipp_tag_t	group;			/* Current group */
  ipp_tag_t	value;			/* Current value type */
  ipp_attribute_t *attrptr,		/* Attribute pointer */
		*found,			/* Found attribute */
		*lastcol = NULL;	/* Last collection attribute */
  char		name[1024],		/* Name of test */
		file_id[1024],		/* File identifier */
		test_id[1024];		/* Test identifier */
  char		filename[1024];		/* Filename */
  _cups_transfer_t transfer;		/* To chunk or not to chunk */
  int		version,		/* IPP version number to use */
		skip_test;		/* Skip this test? */
  int		num_statuses = 0;	/* Number of valid status codes */
  _cups_status_t statuses[100],		/* Valid status codes */
		*last_status;		/* Last STATUS (for predicates) */
  int           status_ok,              /* Did we get a matching status? */
	        num_expects = 0;  /* Number of expected attributes */
  _cups_expect_t expects[200],		/* Expected attributes */
		*expect,		/* Current expected attribute */
		*last_expect;		/* Last EXPECT (for predicates) */
  int		num_displayed = 0;	/* Number of displayed attributes */
  char		*displayed[200];	/* Displayed attributes */
  size_t	widths[200];		/* Width of columns */
  cups_array_t	*a,			/* Duplicate attribute array */
		*errors = NULL;		/* Errors array */
  const char	*error;			/* Current error */


 /*
  * Open the test file...
  */

  if ((fp = cupsFileOpen(testfile, "r")) == NULL)
  {
    print_fatal_error(data, "Unable to open test file %s - %s", testfile,
                      strerror(errno));
    pass = 0;
    goto test_exit;
  }

 /*
  * Connect to the server...
  */

  if ((http = httpConnect2(vars->hostname, vars->port, NULL, vars->family,
                           vars->encryption, 1, 30000, NULL)) == NULL)
  {
    print_fatal_error(data, "Unable to connect to %s on port %d - %s", vars->hostname,
                      vars->port, cupsLastErrorString());
    pass = 0;
    goto test_exit;
  }

#ifdef HAVE_LIBZ
  httpSetDefaultField(http, HTTP_FIELD_ACCEPT_ENCODING,
                      "deflate, gzip, identity");
#else
  httpSetDefaultField(http, HTTP_FIELD_ACCEPT_ENCODING, "identity");
#endif /* HAVE_LIBZ */

  if (vars->timeout > 0.0)
    httpSetTimeout(http, vars->timeout, timeout_cb, NULL);

 /*
  * Loop on tests...
  */

  CUPS_SRAND((unsigned)time(NULL));

  errors     = cupsArrayNew3(NULL, NULL, NULL, 0, (cups_acopy_func_t)strdup,
                             (cups_afree_func_t)free);
  file_id[0] = '\0';
  pass       = 1;
  linenum    = 0;
  request_id = (CUPS_RAND() % 1000) * 137 + 1;

  while (!Cancel && get_token(fp, token, sizeof(token), &linenum) != NULL)
  {
   /*
    * Expect an open brace...
    */

    if (!strcmp(token, "DEFINE"))
    {
     /*
      * DEFINE name value
      */

      if (_ippFileReadToken(f, name, sizeof(name)) &&
          _ippFileReadToken(f, temp, sizeof(temp)))
      {
        _ippVarsExpand(vars, token, temp, sizeof(token));
	_ippVarsSet(vars, attr, token);
      }
      else
      {
        print_fatal_error(data, "Missing DEFINE name and/or value on line %d of \"%s\".",
	                  linenum);
	pass = 0;
	goto test_exit;
      }

      continue;
    }
    else if (!strcmp(token, "DEFINE-DEFAULT"))
    {
     /*
      * DEFINE-DEFAULT name value
      */

      if (_ippFileReadToken(f, name, sizeof(name)) &&
          _ippFileReadToken(f, temp, sizeof(temp)))
      {
        _ippVarsExpand(vars, token, temp, sizeof(token));
	if (!_ippVarsGet(vars, attr))
	  _ippVarsSet(vars, attr, token);
      }
      else
      {
        print_fatal_error(data, "Missing DEFINE-DEFAULT name and/or value on line "
	                  "%d.", f->linenum, f->filename);
	pass = 0;
	goto test_exit;
      }

      continue;
    }
    else if (!strcmp(token, "FILE-ID"))
    {
     /*
      * FILE-ID "string"
      */

      if (_ippFileReadToken(f, temp, sizeof(temp)))
      {
        _ippVarsExpand(vars, file_id, temp, sizeof(file_id));
      }
      else
      {
        print_fatal_error(data, "Missing FILE-ID value on line %d of \"%s\".", f->linenum, f->filename);
	pass = 0;
	goto test_exit;
      }

      continue;
    }
    else if (!strcmp(token, "IGNORE-ERRORS"))
    {
     /*
      * IGNORE-ERRORS yes
      * IGNORE-ERRORS no
      */

      if (_ippFileReadToken(f, temp, sizeof(temp)) &&
          (!_cups_strcasecmp(temp, "yes") || !_cups_strcasecmp(temp, "no")))
      {
        IgnoreErrors = !_cups_strcasecmp(temp, "yes");
      }
      else
      {
        print_fatal_error(data, "Missing IGNORE-ERRORS value on line %d of \"%s\".", f->linenum, f->filename);
	pass = 0;
	goto test_exit;
      }

      continue;
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

        if (!do_tests(data->outfile, vars, get_filename(testfile, filename, temp, sizeof(filename))))
	{
	  pass = 0;

	  if (StopAfterIncludeError)
	    goto test_exit;
	}
      }
      else
      {
        print_fatal_error(data, "Missing INCLUDE filename on line %d of \"%s\".", f->linenum, f->filename);
	pass = 0;
	goto test_exit;
      }

      show_header = 1;
      continue;
    }
    else if (!strcmp(token, "INCLUDE-IF-DEFINED"))
    {
     /*
      * INCLUDE-IF-DEFINED name "filename"
      * INCLUDE-IF-DEFINED name <filename>
      */

      if (_ippFileReadToken(f, name, sizeof(name)) &&
          _ippFileReadToken(f, temp, sizeof(temp)))
      {
       /*
        * Map the filename to and then run the tests...
	*/

        if (_ippVarsGet(vars, attr) &&
	    !do_tests(data->outfile, vars, get_filename(testfile, filename, temp, sizeof(filename))))
	{
	  pass = 0;

	  if (StopAfterIncludeError)
	    goto test_exit;
	}
      }
      else
      {
        print_fatal_error(data, "Missing INCLUDE-IF-DEFINED name or filename on line "
	                  "%d.", f->linenum, f->filename);
	pass = 0;
	goto test_exit;
      }

      show_header = 1;
      continue;
    }
    else if (!strcmp(token, "INCLUDE-IF-NOT-DEFINED"))
    {
     /*
      * INCLUDE-IF-NOT-DEFINED name "filename"
      * INCLUDE-IF-NOT-DEFINED name <filename>
      */

      if (_ippFileReadToken(f, name, sizeof(name)) &&
          _ippFileReadToken(f, temp, sizeof(temp)))
      {
       /*
        * Map the filename to and then run the tests...
	*/

        if (!_ippVarsGet(vars, attr) &&
	    !do_tests(data->outfile, vars, get_filename(testfile, filename, temp, sizeof(filename))))
	{
	  pass = 0;

	  if (StopAfterIncludeError)
	    goto test_exit;
	}
      }
      else
      {
        print_fatal_error(data, "Missing INCLUDE-IF-NOT-DEFINED name or filename on "
	                  "line %d of \"%s\".", f->linenum, f->filename);
	pass = 0;
	goto test_exit;
      }

      show_header = 1;
      continue;
    }
    else if (!strcmp(token, "SKIP-IF-DEFINED"))
    {
     /*
      * SKIP-IF-DEFINED variable
      */

      if (_ippFileReadToken(f, temp, sizeof(temp)))
      {
        if (_ippVarsGet(vars, temp))
	  goto test_exit;
      }
      else
      {
        print_fatal_error(data, "Missing SKIP-IF-DEFINED variable on line %d of \"%s\".",
	                  linenum);
	pass = 0;
	goto test_exit;
      }
    }
    else if (!strcmp(token, "SKIP-IF-NOT-DEFINED"))
    {
     /*
      * SKIP-IF-NOT-DEFINED variable
      */

      if (_ippFileReadToken(f, temp, sizeof(temp)))
      {
        if (!_ippVarsGet(vars, temp))
	  goto test_exit;
      }
      else
      {
        print_fatal_error(data, "Missing SKIP-IF-NOT-DEFINED variable on line %d of \"%s\".",
	                  linenum);
	pass = 0;
	goto test_exit;
      }
    }
    else if (!strcmp(token, "STOP-AFTER-INCLUDE-ERROR"))
    {
     /*
      * STOP-AFTER-INCLUDE-ERROR yes
      * STOP-AFTER-INCLUDE-ERROR no
      */

      if (_ippFileReadToken(f, temp, sizeof(temp)) &&
          (!_cups_strcasecmp(temp, "yes") || !_cups_strcasecmp(temp, "no")))
      {
        StopAfterIncludeError = !_cups_strcasecmp(temp, "yes");
      }
      else
      {
        print_fatal_error(data, "Missing STOP-AFTER-INCLUDE-ERROR value on line %d of \"%s\".",
                          linenum);
	pass = 0;
	goto test_exit;
      }

      continue;
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
	  Transfer = _CUPS_TRANSFER_AUTO;
	else if (!strcmp(temp, "chunked"))
	  Transfer = _CUPS_TRANSFER_CHUNKED;
	else if (!strcmp(temp, "length"))
	  Transfer = _CUPS_TRANSFER_LENGTH;
	else
	{
	  print_fatal_error(data, "Bad TRANSFER value \"%s\" on line %d of \"%s\".", temp,
	                    linenum);
	  pass = 0;
	  goto test_exit;
	}
      }
      else
      {
        print_fatal_error(data, "Missing TRANSFER value on line %d of \"%s\".", f->linenum, f->filename);
	pass = 0;
	goto test_exit;
      }

      continue;
    }
    else if (!strcmp(token, "VERSION"))
    {
      if (_ippFileReadToken(f, temp, sizeof(temp)))
      {
        if (!strcmp(temp, "1.0"))
	  Version = 10;
	else if (!strcmp(temp, "1.1"))
	  Version = 11;
	else if (!strcmp(temp, "2.0"))
	  Version = 20;
	else if (!strcmp(temp, "2.1"))
	  Version = 21;
	else if (!strcmp(temp, "2.2"))
	  Version = 22;
	else
	{
	  print_fatal_error(data, "Bad VERSION \"%s\" on line %d of \"%s\".", temp, f->linenum, f->filename);
	  pass = 0;
	  goto test_exit;
	}
      }
      else
      {
        print_fatal_error(data, "Missing VERSION number on line %d of \"%s\".", f->linenum, f->filename);
	pass = 0;
	goto test_exit;
      }

      continue;
    }
    else if (strcmp(token, "{"))
    {
      print_fatal_error(data, "Unexpected token %s seen on line %d of \"%s\".", token, f->linenum, f->filename);
      pass = 0;
      goto test_exit;
    }

   /*
    * Initialize things...
    */

    if (show_header)
    {
      if (Output == _CUPS_OUTPUT_PLIST)
	print_xml_header(data->outfile);
      if (Output == _CUPS_OUTPUT_TEST || (Output == _CUPS_OUTPUT_PLIST && outfile != cupsFileStdout()))
	cupsFilePrintf(cupsFileStdout(), "\"%s\":\n", testfile);

      show_header = 0;
    }

    strlcpy(resource, vars->resource, sizeof(resource));

    request_id ++;
    request         = ippNew();
    op              = (ipp_op_t)0;
    group           = IPP_TAG_ZERO;
    ignore_errors   = IgnoreErrors;
    last_expect     = NULL;
    last_status     = NULL;
    filename[0]     = '\0';
    skip_previous   = 0;
    skip_test       = 0;
    test_id[0]      = '\0';
    version         = Version;
    transfer        = Transfer;
    compression[0]  = '\0';
    delay           = 0;
    repeat_count    = 0;
    repeat_interval = 5000000;

    strlcpy(name, testfile, sizeof(name));
    if (strrchr(name, '.') != NULL)
      *strrchr(name, '.') = '\0';

   /*
    * Parse until we see a close brace...
    */

    while (get_token(fp, token, sizeof(token), &linenum) != NULL)
    {
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
        last_expect = NULL;

      if (_cups_strcasecmp(token, "DEFINE-MATCH") &&
          _cups_strcasecmp(token, "DEFINE-NO-MATCH") &&
	  _cups_strcasecmp(token, "IF-DEFINED") &&
          _cups_strcasecmp(token, "IF-NOT-DEFINED") &&
          _cups_strcasecmp(token, "REPEAT-LIMIT") &&
          _cups_strcasecmp(token, "REPEAT-MATCH") &&
          _cups_strcasecmp(token, "REPEAT-NO-MATCH"))
        last_status = NULL;

      if (!strcmp(token, "}"))
        break;
      else if (!strcmp(token, "{") && lastcol)
      {
       /*
	* Another collection value
	*/

	ipp_t	*col = get_collection(data->outfile, vars, fp, &linenum);
					/* Collection value */

	if (col)
	{
          ippSetCollection(request, &lastcol, ippGetCount(lastcol), col);
        }
	else
	{
	  pass = 0;
	  goto test_exit;
	}
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
	  _ippVarsExpand(vars, compression, temp, sizeof(compression));
#ifdef HAVE_LIBZ
	  if (strcmp(compression, "none") && strcmp(compression, "deflate") &&
	      strcmp(compression, "gzip"))
#else
	  if (strcmp(compression, "none"))
#endif /* HAVE_LIBZ */
          {
	    print_fatal_error(data, "Unsupported COMPRESSION value '%s' on line %d of \"%s\".",
	                      compression, f->linenum, f->filename);
	    pass = 0;
	    goto test_exit;
          }

          if (!strcmp(compression, "none"))
            compression[0] = '\0';
	}
	else
	{
	  print_fatal_error(data, "Missing COMPRESSION value on line %d of \"%s\".", f->linenum, f->filename);
	  pass = 0;
	  goto test_exit;
	}
      }
      else if (!strcmp(token, "DEFINE"))
      {
       /*
	* DEFINE name value
	*/

	if (_ippFileReadToken(f, name, sizeof(name)) &&
	    _ippFileReadToken(f, temp, sizeof(temp)))
	{
	  _ippVarsExpand(vars, token, temp, sizeof(token));
	  _ippVarsSet(vars, attr, token);
	}
	else
	{
	  print_fatal_error(data, "Missing DEFINE name and/or value on line %d of \"%s\".",
			    linenum);
	  pass = 0;
	  goto test_exit;
	}
      }
      else if (!strcmp(token, "IGNORE-ERRORS"))
      {
       /*
	* IGNORE-ERRORS yes
	* IGNORE-ERRORS no
	*/

	if (_ippFileReadToken(f, temp, sizeof(temp)) &&
	    (!_cups_strcasecmp(temp, "yes") || !_cups_strcasecmp(temp, "no")))
	{
	  ignore_errors = !_cups_strcasecmp(temp, "yes");
	}
	else
	{
	  print_fatal_error(data, "Missing IGNORE-ERRORS value on line %d of \"%s\".", f->linenum, f->filename);
	  pass = 0;
	  goto test_exit;
	}

	continue;
      }
      else if (!_cups_strcasecmp(token, "NAME"))
      {
       /*
        * Name of test...
	*/

	_ippFileReadToken(f, temp, sizeof(temp));
	_ippVarsExpand(vars, name, temp, sizeof(name));
      }
      else if (!_cups_strcasecmp(token, "PAUSE"))
      {
       /*
        * Pause with a message...
	*/

	get_token(fp, token, sizeof(token), &linenum);
	pause_message(token);
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
	    request_id = atoi(temp);
	  else if (!_cups_strcasecmp(temp, "random"))
	    request_id = (CUPS_RAND() % 1000) * 137 + 1;
	  else
	  {
	    print_fatal_error(data, "Bad REQUEST-ID value \"%s\" on line %d of \"%s\".", temp,
			      linenum);
	    pass = 0;
	    goto test_exit;
	  }
	}
	else
	{
	  print_fatal_error(data, "Missing REQUEST-ID value on line %d of \"%s\".", f->linenum, f->filename);
	  pass = 0;
	  goto test_exit;
	}
      }
      else if (!strcmp(token, "SKIP-IF-DEFINED"))
      {
       /*
	* SKIP-IF-DEFINED variable
	*/

	if (_ippFileReadToken(f, temp, sizeof(temp)))
	{
	  if (_ippVarsGet(vars, temp))
	    skip_test = 1;
	}
	else
	{
	  print_fatal_error(data, "Missing SKIP-IF-DEFINED value on line %d of \"%s\".",
	                    linenum);
	  pass = 0;
	  goto test_exit;
	}
      }
      else if (!strcmp(token, "SKIP-IF-MISSING"))
      {
       /*
	* SKIP-IF-MISSING filename
	*/

	if (_ippFileReadToken(f, temp, sizeof(temp)))
	{
	  _ippVarsExpand(vars, token, temp, sizeof(token));
	  get_filename(testfile, filename, token, sizeof(filename));

	  if (access(filename, R_OK))
	    skip_test = 1;
	}
	else
	{
	  print_fatal_error(data, "Missing SKIP-IF-MISSING filename on line %d of \"%s\".",
			    linenum);
	  pass = 0;
	  goto test_exit;
	}
      }
      else if (!strcmp(token, "SKIP-IF-NOT-DEFINED"))
      {
       /*
	* SKIP-IF-NOT-DEFINED variable
	*/

	if (_ippFileReadToken(f, temp, sizeof(temp)))
	{
	  if (!_ippVarsGet(vars, temp))
	    skip_test = 1;
	}
	else
	{
	  print_fatal_error(data, "Missing SKIP-IF-NOT-DEFINED value on line %d of \"%s\".",
			    linenum);
	  pass = 0;
	  goto test_exit;
	}
      }
      else if (!strcmp(token, "SKIP-PREVIOUS-ERROR"))
      {
       /*
	* SKIP-PREVIOUS-ERROR yes
	* SKIP-PREVIOUS-ERROR no
	*/

	if (_ippFileReadToken(f, temp, sizeof(temp)) &&
	    (!_cups_strcasecmp(temp, "yes") || !_cups_strcasecmp(temp, "no")))
	{
	  skip_previous = !_cups_strcasecmp(temp, "yes");
	}
	else
	{
	  print_fatal_error(data, "Missing SKIP-PREVIOUS-ERROR value on line %d of \"%s\".", f->linenum, f->filename);
	  pass = 0;
	  goto test_exit;
	}

	continue;
      }
      else if (!strcmp(token, "TEST-ID"))
      {
       /*
	* TEST-ID "string"
	*/

	if (_ippFileReadToken(f, temp, sizeof(temp)))
	{
	  _ippVarsExpand(vars, test_id, temp, sizeof(test_id));
	}
	else
	{
	  print_fatal_error(data, "Missing TEST-ID value on line %d of \"%s\".", f->linenum, f->filename);
	  pass = 0;
	  goto test_exit;
	}

	continue;
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
	    transfer = _CUPS_TRANSFER_AUTO;
	  else if (!strcmp(temp, "chunked"))
	    transfer = _CUPS_TRANSFER_CHUNKED;
	  else if (!strcmp(temp, "length"))
	    transfer = _CUPS_TRANSFER_LENGTH;
	  else
	  {
	    print_fatal_error(data, "Bad TRANSFER value \"%s\" on line %d of \"%s\".", temp,
			      linenum);
	    pass = 0;
	    goto test_exit;
	  }
	}
	else
	{
	  print_fatal_error(data, "Missing TRANSFER value on line %d of \"%s\".", f->linenum, f->filename);
	  pass = 0;
	  goto test_exit;
	}
      }
      else if (!_cups_strcasecmp(token, "VERSION"))
      {
	if (_ippFileReadToken(f, temp, sizeof(temp)))
	{
	  if (!strcmp(temp, "0.0"))
	    version = 0;
	  else if (!strcmp(temp, "1.0"))
	    version = 10;
	  else if (!strcmp(temp, "1.1"))
	    version = 11;
	  else if (!strcmp(temp, "2.0"))
	    version = 20;
	  else if (!strcmp(temp, "2.1"))
	    version = 21;
	  else if (!strcmp(temp, "2.2"))
	    version = 22;
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

	if (!get_token(fp, data->resource, sizeof(data->resource), &linenum))
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

        ipp_op_t	op;		/* Operation code */

	if (!_ippFileReadToken(f, temp, sizeof(temp)))
	{
	  print_fatal_error(data, "Missing OPERATION code on line %d of \"%s\".", f->linenum, f->filename);
	  return (0);
	}

	_ippVarsExpand(vars, value, temp, sizeof(value));

	if ((op = ippOpValue(value)) == (ipp_op_t)-1 && (op = (ipp_op_t)strtol(token, NULL, 0)) == 0)
	{
	  print_fatal_error(data, "Bad OPERATION code \"%s\" on line %d of \"%s\".", token, f->linenum, f->filename);
	  return (0);
	}
      }
      else if (!_cups_strcasecmp(token, "GROUP"))
      {
       /*
        * Attribute group...
	*/

        ipp_tag_t	group_tag;	/* Group tag */

	if (!get_token(fp, temp, sizeof(temp), &linenum))
	{
	  print_fatal_error(data, "Missing GROUP tag on line %d of \"%s\".", f->linenum, f->filename);
	  return (0);
	}

	if ((group_tag = ippTagValue(token)) == IPP_TAG_ZERO || group_tag >= IPP_TAG_UNSUPPORTED_VALUE)
	{
	  print_fatal_error(data, "Bad GROUP tag \"%s\" on line %d of \"%s\".", token, f->linenum, f->filename);
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
	  pass = 0;
	  goto test_exit;
	}

	_ippVarsExpand(vars, token, temp, sizeof(token));

	if ((dval = _cupsStrScand(token, &tokenptr, localeconv())) < 0.0 || (*tokenptr && *tokenptr != ','))
	{
	  print_fatal_error(data, "Bad DELAY value \"%s\" on line %d of \"%s\".", token,
	                    linenum);
	  pass = 0;
	  goto test_exit;
	}

        delay = (useconds_t)(1000000.0 * dval);

        if (*tokenptr == ',')
        {
          if ((dval = _cupsStrScand(tokenptr + 1, &tokenptr, localeconv())) <= 0.0 || *tokenptr)
          {
            print_fatal_error(data, "Bad DELAY value \"%s\" on line %d of \"%s\".", token,
                              linenum);
            pass = 0;
            goto test_exit;
          }

          repeat_interval = (useconds_t)(1000000.0 * dval);
        }
        else
          repeat_interval = delay;
      }
      else if (!_cups_strcasecmp(token, "ATTR"))
      {
       /*
        * Attribute...
	*/

	if (!get_token(fp, token, sizeof(token), &linenum))
	{
	  print_fatal_error(data, "Missing ATTR value tag on line %d of \"%s\".", f->linenum, f->filename);
	  pass = 0;
	  goto test_exit;
	}

	if ((value = ippTagValue(token)) < IPP_TAG_UNSUPPORTED_VALUE)
	{
	  print_fatal_error(data, "Bad ATTR value tag \"%s\" on line %d of \"%s\".", token,
	                    linenum);
	  pass = 0;
	  goto test_exit;
	}

	if (!_ippFileReadToken(f, name, sizeof(name)))
	{
	  print_fatal_error(data, "Missing ATTR name on line %d of \"%s\".", f->linenum, f->filename);
	  pass = 0;
	  goto test_exit;
	}

        if (value < IPP_TAG_INTEGER)
        {
         /*
          * Add out-of-band value - no value string needed...
          */

          token[0] = '\0';
        }
        else if (!_ippFileReadToken(f, temp, sizeof(temp)))
	{
	  print_fatal_error(data, "Missing ATTR value on line %d of \"%s\".", f->linenum, f->filename);
	  pass = 0;
	  goto test_exit;
	}
	else
	{
          _ippVarsExpand(vars, token, temp, sizeof(token));
        }

        attrptr = NULL;

        switch (value)
	{
          default :
              if (value < IPP_TAG_INTEGER)
              {
               /*
                * Add out-of-band value...
                */

                attrptr = ippAddOutOfBand(request, group, value, attr);
              }
              else
              {
                print_fatal_error(data, "Unsupported ATTR value tag %s for \"%s\" on line %d of \"%s\".", ippTagString(value), attr, f->linenum, f->filename);
                pass = 0;
                goto test_exit;
              }
              break;

	  case IPP_TAG_BOOLEAN :
	      if (!_cups_strcasecmp(token, "true"))
		attrptr = ippAddBoolean(request, group, attr, 1);
              else
		attrptr = ippAddBoolean(request, group, attr, (char)atoi(token));
	      break;

	  case IPP_TAG_INTEGER :
	  case IPP_TAG_ENUM :
	      if (!strchr(token, ','))
	      {
	        if (isdigit(token[0] & 255) || token[0] == '-')
		  attrptr = ippAddInteger(request, group, value, attr, (int)strtol(token, &tokenptr, 0));
		else
		{
                  tokenptr = token;
                  if ((i = ippEnumValue(attr, tokenptr)) >= 0)
                  {
                    attrptr  = ippAddInteger(request, group, value, attr, i);
                    tokenptr += strlen(tokenptr);
                  }
		}
	      }
	      else
	      {
	        int	values[100],	/* Values */
			num_values = 1;	/* Number of values */

		if (!isdigit(token[0] & 255) && token[0] != '-' && value == IPP_TAG_ENUM)
		{
		  char *ptr;		/* Pointer to next terminator */

		  if ((ptr = strchr(token, ',')) != NULL)
		    *ptr++ = '\0';
		  else
		    ptr = token + strlen(token);

		  if ((i = ippEnumValue(attr, token)) < 0)
		    tokenptr = NULL;
		  else
		    tokenptr = ptr;
		}
		else
		  i = (int)strtol(token, &tokenptr, 0);

		values[0] = i;

		while (tokenptr && *tokenptr &&
		       num_values < (int)(sizeof(values) / sizeof(values[0])))
		{
		  if (*tokenptr == ',')
		    tokenptr ++;

		  if (!isdigit(*tokenptr & 255) && *tokenptr != '-')
		  {
		    char *ptr;		/* Pointer to next terminator */

		    if (value != IPP_TAG_ENUM)
		      break;

                    if ((ptr = strchr(tokenptr, ',')) != NULL)
                      *ptr++ = '\0';
                    else
                      ptr = tokenptr + strlen(tokenptr);

                    if ((i = ippEnumValue(attr, tokenptr)) < 0)
                      break;

                    tokenptr = ptr;
		  }
		  else
		    i = (int)strtol(tokenptr, &tokenptr, 0);

		  values[num_values ++] = i;
		}

		attrptr = ippAddIntegers(request, group, value, attr, num_values, values);
	      }

	      if ((!token[0] || !tokenptr || *tokenptr) && !skip_test)
	      {
		print_fatal_error(data, "Bad %s value \'%s\' for \"%s\" on line %d of \"%s\".",
				  ippTagString(value), token, attr, f->linenum, f->filename);
		pass = 0;
		goto test_exit;
	      }
	      break;

	  case IPP_TAG_RESOLUTION :
	      {
	        int	xres,		/* X resolution */
			yres;		/* Y resolution */
	        char	*ptr;		/* Pointer into value */

	        xres = yres = (int)strtol(token, (char **)&ptr, 10);
	        if (ptr > token && xres > 0)
	        {
	          if (*ptr == 'x')
		    yres = (int)strtol(ptr + 1, (char **)&ptr, 10);
	        }

	        if (ptr <= token || xres <= 0 || yres <= 0 || !ptr ||
	            (_cups_strcasecmp(ptr, "dpi") &&
	             _cups_strcasecmp(ptr, "dpc") &&
	             _cups_strcasecmp(ptr, "dpcm") &&
	             _cups_strcasecmp(ptr, "other")))
	        {
	          if (skip_test)
	            break;

	          print_fatal_error(data, "Bad resolution value \'%s\' for \"%s\" on line %d of \"%s\".", token, attr, f->linenum, f->filename);
		  pass = 0;
		  goto test_exit;
	        }

	        if (!_cups_strcasecmp(ptr, "dpi"))
	          attrptr = ippAddResolution(request, group, attr, IPP_RES_PER_INCH, xres, yres);
	        else if (!_cups_strcasecmp(ptr, "dpc") ||
	                 !_cups_strcasecmp(ptr, "dpcm"))
	          attrptr = ippAddResolution(request, group, attr, IPP_RES_PER_CM, xres, yres);
	        else
	          attrptr = ippAddResolution(request, group, attr, (ipp_res_t)0, xres, yres);
	      }
	      break;

	  case IPP_TAG_RANGE :
	      {
	        int	lowers[4],	/* Lower value */
			uppers[4],	/* Upper values */
			num_vals;	/* Number of values */


		num_vals = sscanf(token, "%d-%d,%d-%d,%d-%d,%d-%d",
		                  lowers + 0, uppers + 0,
				  lowers + 1, uppers + 1,
				  lowers + 2, uppers + 2,
				  lowers + 3, uppers + 3);

                if ((num_vals & 1) || num_vals == 0)
		{
	          if (skip_test)
	            break;

		  print_fatal_error(data, "Bad rangeOfInteger value \'%s\' for \"%s\" on line %d of \"%s\".", token, attr, f->linenum, f->filename);
		  pass = 0;
		  goto test_exit;
		}

		attrptr = ippAddRanges(request, group, attr, num_vals / 2, lowers,
		                       uppers);
	      }
	      break;

          case IPP_TAG_BEGIN_COLLECTION :
	      if (!strcmp(token, "{"))
	      {
	        ipp_t	*col = get_collection(data->outfile, vars, fp, &linenum);
					/* Collection value */

                if (col)
                {
		  attrptr = lastcol = ippAddCollection(request, group, attr, col);
		  ippDelete(col);
		}
		else
		{
		  pass = 0;
		  goto test_exit;
	        }
              }
              else if (skip_test)
		break;
	      else
	      {
		print_fatal_error(data, "Bad ATTR collection value for \"%s\" on line %d of \"%s\".", attr, f->linenum, f->filename);
		pass = 0;
		goto test_exit;
	      }

	      do
	      {
	        ipp_t	*col;			        /* Collection value */
	        off_t	savepos = cupsFileTell(fp);	/* Save position of file */
	        int	savelinenum = linenum;	        /* Save line number */

		if (!get_token(fp, token, sizeof(token), &linenum))
		  break;

		if (strcmp(token, ","))
		{
		  cupsFileSeek(fp, savepos);
		  linenum = savelinenum;
		  break;
		}

		if (!get_token(fp, token, sizeof(token), &linenum) || strcmp(token, "{"))
		{
		  print_fatal_error(data, "Unexpected \"%s\" on line %d of \"%s\".", token, f->linenum, f->filename);
		  pass = 0;
		  goto test_exit;
		  break;
		}

	        if ((col = get_collection(data->outfile, vars, fp, &linenum)) == NULL)
		  break;

		ippSetCollection(request, &attrptr, ippGetCount(attrptr), col);
		lastcol = attrptr;
	      }
	      while (!strcmp(token, "{"));
	      break;

          case IPP_TAG_STRING :
              attrptr = ippAddOctetString(request, group, attr, token, (int)strlen(token));
	      break;

	  case IPP_TAG_TEXTLANG :
	  case IPP_TAG_NAMELANG :
	  case IPP_TAG_TEXT :
	  case IPP_TAG_NAME :
	  case IPP_TAG_KEYWORD :
	  case IPP_TAG_URI :
	  case IPP_TAG_URISCHEME :
	  case IPP_TAG_CHARSET :
	  case IPP_TAG_LANGUAGE :
	  case IPP_TAG_MIMETYPE :
	      if (!strchr(token, ','))
	        attrptr = ippAddString(request, group, value, attr, NULL, token);
	      else
	      {
	       /*
	        * Multiple string values...
		*/

                int	num_values;	/* Number of values */
                char	*values[100],	/* Values */
			*ptr;		/* Pointer to next value */


                values[0]  = token;
		num_values = 1;

                for (ptr = strchr(token, ','); ptr; ptr = strchr(ptr, ','))
		{
		  if (ptr > token && ptr[-1] == '\\')
		    _cups_strcpy(ptr - 1, ptr);
		  else
		  {
		    *ptr++ = '\0';
		    values[num_values] = ptr;
		    num_values ++;
		  }
		}

	        attrptr = ippAddStrings(request, group, value, attr, num_values,
		                        NULL, (const char **)values);
	      }
	      break;
	}

	if (!attrptr && !skip_test)
	{
	  print_fatal_error(data, "Unable to add attribute \"%s\" on line %d of \"%s\".", attr, f->linenum, f->filename);
	  pass = 0;
	  goto test_exit;
	}
      }
      else if (!_cups_strcasecmp(token, "FILE"))
      {
       /*
        * File...
	*/

	if (!_ippFileReadToken(f, temp, sizeof(temp)))
	{
	  print_fatal_error(data, "Missing FILE filename on line %d of \"%s\".", f->linenum, f->filename);
	  pass = 0;
	  goto test_exit;
	}

        _ippVarsExpand(vars, token, temp, sizeof(token));
	get_filename(testfile, filename, token, sizeof(filename));

        if (access(filename, R_OK))
        {
	  print_fatal_error(data, "Filename \"%s\" on line %d cannot be read.",
	                    temp, f->linenum, f->filename);
	  print_fatal_error(data, "Filename mapped to \"%s\".", filename);
	  pass = 0;
	  goto test_exit;
        }
      }
      else if (!_cups_strcasecmp(token, "STATUS"))
      {
       /*
        * Status...
	*/

        if (num_statuses >= (int)(sizeof(statuses) / sizeof(statuses[0])))
	{
	  print_fatal_error(data, "Too many STATUS's on line %d of \"%s\".", f->linenum, f->filename);
	  pass = 0;
	  goto test_exit;
	}

	if (!get_token(fp, token, sizeof(token), &linenum))
	{
	  print_fatal_error(data, "Missing STATUS code on line %d of \"%s\".", f->linenum, f->filename);
	  pass = 0;
	  goto test_exit;
	}

	if ((statuses[num_statuses].status = ippErrorValue(token))
	        == (ipp_status_t)-1 &&
	    (statuses[num_statuses].status = (ipp_status_t)strtol(token, NULL, 0)) == 0)
	{
	  print_fatal_error(data, "Bad STATUS code \"%s\" on line %d of \"%s\".", token,
	                    linenum);
	  pass = 0;
	  goto test_exit;
	}

        last_status = statuses + num_statuses;
	num_statuses ++;

        last_status->define_match    = NULL;
        last_status->define_no_match = NULL;
	last_status->if_defined      = NULL;
	last_status->if_not_defined  = NULL;
	last_status->repeat_limit    = 1000;
	last_status->repeat_match    = 0;
	last_status->repeat_no_match = 0;
      }
      else if (!_cups_strcasecmp(token, "EXPECT") || !_cups_strcasecmp(token, "EXPECT-ALL"))
      {
       /*
        * Expected attributes...
	*/

	int expect_all = !_cups_strcasecmp(token, "EXPECT-ALL");

        if (num_expects >= (int)(sizeof(expects) / sizeof(expects[0])))
        {
	  print_fatal_error(data, "Too many EXPECT's on line %d of \"%s\".", f->linenum, f->filename);
	  pass = 0;
	  goto test_exit;
        }

	if (!get_token(fp, token, sizeof(token), &linenum))
	{
	  print_fatal_error(data, "Missing EXPECT name on line %d of \"%s\".", f->linenum, f->filename);
	  pass = 0;
	  goto test_exit;
	}

        last_expect = expects + num_expects;
	num_expects ++;

	memset(last_expect, 0, sizeof(_cups_expect_t));
	last_expect->repeat_limit = 1000;
	last_expect->expect_all   = expect_all;

        if (token[0] == '!')
        {
          last_expect->not_expect = 1;
          last_expect->name       = strdup(token + 1);
        }
        else if (token[0] == '?')
        {
          last_expect->optional = 1;
          last_expect->name     = strdup(token + 1);
        }
        else
	  last_expect->name = strdup(token);
      }
      else if (!_cups_strcasecmp(token, "COUNT"))
      {
	if (!get_token(fp, token, sizeof(token), &linenum))
	{
	  print_fatal_error(data, "Missing COUNT number on line %d of \"%s\".", f->linenum, f->filename);
	  pass = 0;
	  goto test_exit;
	}

        if ((i = atoi(token)) <= 0)
	{
	  print_fatal_error(data, "Bad COUNT \"%s\" on line %d of \"%s\".", token, f->linenum, f->filename);
	  pass = 0;
	  goto test_exit;
	}

	if (last_expect)
	  last_expect->count = i;
	else
	{
	  print_fatal_error(data, "COUNT without a preceding EXPECT on line %d of \"%s\".",
	                    linenum);
	  pass = 0;
	  goto test_exit;
	}
      }
      else if (!_cups_strcasecmp(token, "DEFINE-MATCH"))
      {
	if (!get_token(fp, token, sizeof(token), &linenum))
	{
	  print_fatal_error(data, "Missing DEFINE-MATCH variable on line %d of \"%s\".",
	                    linenum);
	  pass = 0;
	  goto test_exit;
	}

	if (last_expect)
	  last_expect->define_match = strdup(token);
	else if (last_status)
	  last_status->define_match = strdup(token);
	else
	{
	  print_fatal_error(data, "DEFINE-MATCH without a preceding EXPECT or STATUS "
	                    "on line %d of \"%s\".", f->linenum, f->filename);
	  pass = 0;
	  goto test_exit;
	}
      }
      else if (!_cups_strcasecmp(token, "DEFINE-NO-MATCH"))
      {
	if (!get_token(fp, token, sizeof(token), &linenum))
	{
	  print_fatal_error(data, "Missing DEFINE-NO-MATCH variable on line %d of \"%s\".",
	                    linenum);
	  pass = 0;
	  goto test_exit;
	}

	if (last_expect)
	  last_expect->define_no_match = strdup(token);
	else if (last_status)
	  last_status->define_no_match = strdup(token);
	else
	{
	  print_fatal_error(data, "DEFINE-NO-MATCH without a preceding EXPECT or "
	                    "STATUS on line %d of \"%s\".", f->linenum, f->filename);
	  pass = 0;
	  goto test_exit;
	}
      }
      else if (!_cups_strcasecmp(token, "DEFINE-VALUE"))
      {
	if (!get_token(fp, token, sizeof(token), &linenum))
	{
	  print_fatal_error(data, "Missing DEFINE-VALUE variable on line %d of \"%s\".",
	                    linenum);
	  pass = 0;
	  goto test_exit;
	}

	if (last_expect)
	  last_expect->define_value = strdup(token);
	else
	{
	  print_fatal_error(data, "DEFINE-VALUE without a preceding EXPECT on "
	                    "line %d of \"%s\".", f->linenum, f->filename);
	  pass = 0;
	  goto test_exit;
	}
      }
      else if (!_cups_strcasecmp(token, "OF-TYPE"))
      {
	if (!get_token(fp, token, sizeof(token), &linenum))
	{
	  print_fatal_error(data, "Missing OF-TYPE value tag(s) on line %d of \"%s\".",
	                    linenum);
	  pass = 0;
	  goto test_exit;
	}

	if (last_expect)
	  last_expect->of_type = strdup(token);
	else
	{
	  print_fatal_error(data, "OF-TYPE without a preceding EXPECT on line %d of \"%s\".",
	                    linenum);
	  pass = 0;
	  goto test_exit;
	}
      }
      else if (!_cups_strcasecmp(token, "IN-GROUP"))
      {
        ipp_tag_t	in_group;	/* IN-GROUP value */


	if (!get_token(fp, token, sizeof(token), &linenum))
	{
	  print_fatal_error(data, "Missing IN-GROUP group tag on line %d of \"%s\".", f->linenum, f->filename);
	  pass = 0;
	  goto test_exit;
	}

        if ((in_group = ippTagValue(token)) == IPP_TAG_ZERO || in_group >= IPP_TAG_UNSUPPORTED_VALUE)
	{
          print_fatal_error(data, "Bad IN-GROUP group tag \"%s\" on line %d of \"%s\".", token, f->linenum, f->filename);
          pass = 0;
          goto test_exit;
	}
	else if (last_expect)
	  last_expect->in_group = in_group;
	else
	{
	  print_fatal_error(data, "IN-GROUP without a preceding EXPECT on line %d of \"%s\".", f->linenum, f->filename);
	  pass = 0;
	  goto test_exit;
	}
      }
      else if (!_cups_strcasecmp(token, "REPEAT-LIMIT"))
      {
	if (!get_token(fp, token, sizeof(token), &linenum))
	{
	  print_fatal_error(data, "Missing REPEAT-LIMIT value on line %d of \"%s\".", f->linenum, f->filename);
	  pass = 0;
	  goto test_exit;
	}
	else if (atoi(token) <= 0)
	{
	  print_fatal_error(data, "Bad REPEAT-LIMIT value on line %d of \"%s\".", f->linenum, f->filename);
	  pass = 0;
	  goto test_exit;
	}

        if (last_status)
          last_status->repeat_limit = atoi(token);
	else if (last_expect)
	  last_expect->repeat_limit = atoi(token);
	else
	{
	  print_fatal_error(data, "REPEAT-LIMIT without a preceding EXPECT or STATUS on line %d of \"%s\".", f->linenum, f->filename);
	  pass = 0;
	  goto test_exit;
	}
      }
      else if (!_cups_strcasecmp(token, "REPEAT-MATCH"))
      {
        if (last_status)
          last_status->repeat_match = 1;
	else if (last_expect)
	  last_expect->repeat_match = 1;
	else
	{
	  print_fatal_error(data, "REPEAT-MATCH without a preceding EXPECT or STATUS on line %d of \"%s\".", f->linenum, f->filename);
	  pass = 0;
	  goto test_exit;
	}
      }
      else if (!_cups_strcasecmp(token, "REPEAT-NO-MATCH"))
      {
	if (last_status)
	  last_status->repeat_no_match = 1;
	else if (last_expect)
	  last_expect->repeat_no_match = 1;
	else
	{
	  print_fatal_error(data, "REPEAT-NO-MATCH without a preceding EXPECT or STATUS on ine %d.", f->linenum, f->filename);
	  pass = 0;
	  goto test_exit;
	}
      }
      else if (!_cups_strcasecmp(token, "SAME-COUNT-AS"))
      {
	if (!get_token(fp, token, sizeof(token), &linenum))
	{
	  print_fatal_error(data, "Missing SAME-COUNT-AS name on line %d of \"%s\".", f->linenum, f->filename);
	  pass = 0;
	  goto test_exit;
	}

	if (last_expect)
	  last_expect->same_count_as = strdup(token);
	else
	{
	  print_fatal_error(data, "SAME-COUNT-AS without a preceding EXPECT on line "
	                    "%d.", f->linenum, f->filename);
	  pass = 0;
	  goto test_exit;
	}
      }
      else if (!_cups_strcasecmp(token, "IF-DEFINED"))
      {
	if (!get_token(fp, token, sizeof(token), &linenum))
	{
	  print_fatal_error(data, "Missing IF-DEFINED name on line %d of \"%s\".", f->linenum, f->filename);
	  pass = 0;
	  goto test_exit;
	}

	if (last_expect)
	  last_expect->if_defined = strdup(token);
	else if (last_status)
	  last_status->if_defined = strdup(token);
	else
	{
	  print_fatal_error(data, "IF-DEFINED without a preceding EXPECT or STATUS on line %d of \"%s\".", f->linenum, f->filename);
	  pass = 0;
	  goto test_exit;
	}
      }
      else if (!_cups_strcasecmp(token, "IF-NOT-DEFINED"))
      {
	if (!get_token(fp, token, sizeof(token), &linenum))
	{
	  print_fatal_error(data, "Missing IF-NOT-DEFINED name on line %d of \"%s\".", f->linenum, f->filename);
	  pass = 0;
	  goto test_exit;
	}

	if (last_expect)
	  last_expect->if_not_defined = strdup(token);
	else if (last_status)
	  last_status->if_not_defined = strdup(token);
	else
	{
	  print_fatal_error(data, "IF-NOT-DEFINED without a preceding EXPECT or STATUS on line %d of \"%s\".", f->linenum, f->filename);
	  pass = 0;
	  goto test_exit;
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
	if (last_expect)
	{
	  if (!_cups_strcasecmp(token, "WITH-ALL-HOSTNAMES") ||
	      !_cups_strcasecmp(token, "WITH-HOSTNAME"))
	    last_expect->with_flags = _CUPS_WITH_HOSTNAME;
	  else if (!_cups_strcasecmp(token, "WITH-ALL-RESOURCES") ||
	      !_cups_strcasecmp(token, "WITH-RESOURCE"))
	    last_expect->with_flags = _CUPS_WITH_RESOURCE;
	  else if (!_cups_strcasecmp(token, "WITH-ALL-SCHEMES") ||
	      !_cups_strcasecmp(token, "WITH-SCHEME"))
	    last_expect->with_flags = _CUPS_WITH_SCHEME;

	  if (!_cups_strncasecmp(token, "WITH-ALL-", 9))
	    last_expect->with_flags |= _CUPS_WITH_ALL;
        }

      	if (!_ippFileReadToken(f, temp, sizeof(temp)))
	{
	  print_fatal_error(data, "Missing %s value on line %d of \"%s\".", token, f->linenum, f->filename);
	  pass = 0;
	  goto test_exit;
	}

        if (last_expect)
	{
	 /*
	  * Expand any variables in the value and then save it.
	  */

	  _ippVarsExpand(vars, token, temp, sizeof(token));

	  tokenptr = token + strlen(token) - 1;

	  if (token[0] == '/' && tokenptr > token && *tokenptr == '/')
	  {
	   /*
	    * WITH-VALUE is a POSIX extended regular expression.
	    */

	    last_expect->with_value = calloc(1, (size_t)(tokenptr - token));
	    last_expect->with_flags |= _CUPS_WITH_REGEX;

	    if (last_expect->with_value)
	      memcpy(last_expect->with_value, token + 1, (size_t)(tokenptr - token - 1));
	  }
	  else
	  {
	   /*
	    * WITH-VALUE is a literal value...
	    */

	    char *ptr;			/* Pointer into value */

            for (ptr = token; *ptr; ptr ++)
            {
	      if (*ptr == '\\' && ptr[1])
	      {
	       /*
	        * Remove \ from \foo...
	        */

		_cups_strcpy(ptr, ptr + 1);
	      }
	    }

	    last_expect->with_value = strdup(token);
	    last_expect->with_flags |= _CUPS_WITH_LITERAL;
	  }
	}
	else
	{
	  print_fatal_error(data, "%s without a preceding EXPECT on line %d of \"%s\".", token, f->linenum, f->filename);
	  pass = 0;
	  goto test_exit;
	}
      }
      else if (!_cups_strcasecmp(token, "WITH-VALUE-FROM"))
      {
      	if (!_ippFileReadToken(f, temp, sizeof(temp)))
	{
	  print_fatal_error(data, "Missing %s value on line %d of \"%s\".", token, f->linenum, f->filename);
	  pass = 0;
	  goto test_exit;
	}

        if (last_expect)
	{
	 /*
	  * Expand any variables in the value and then save it.
	  */

	  _ippVarsExpand(vars, token, temp, sizeof(token));

	  last_expect->with_value_from = strdup(token);
	  last_expect->with_flags      = _CUPS_WITH_LITERAL;
	}
	else
	{
	  print_fatal_error(data, "%s without a preceding EXPECT on line %d of \"%s\".", token, f->linenum, f->filename);
	  pass = 0;
	  goto test_exit;
	}
      }
      else if (!_cups_strcasecmp(token, "DISPLAY"))
      {
       /*
        * Display attributes...
	*/

        if (num_displayed >= (int)(sizeof(displayed) / sizeof(displayed[0])))
	{
	  print_fatal_error(data, "Too many DISPLAY's on line %d", f->linenum, f->filename);
	  pass = 0;
	  goto test_exit;
	}

	if (!get_token(fp, token, sizeof(token), &linenum))
	{
	  print_fatal_error(data, "Missing DISPLAY name on line %d of \"%s\".", f->linenum, f->filename);
	  pass = 0;
	  goto test_exit;
	}

	displayed[num_displayed] = strdup(token);
	num_displayed ++;
      }
      else
      {
	print_fatal_error(data, "Unexpected token %s seen on line %d of \"%s\".", token, f->linenum, f->filename);
	pass = 0;
	goto test_exit;
      }
    }

   /*
    * Submit the IPP request...
    */

    TestCount ++;

    ippSetVersion(request, version / 10, version % 10);
    ippSetOperation(request, op);
    ippSetRequestId(request, request_id);

    if (Output == _CUPS_OUTPUT_PLIST)
    {
      cupsFilePuts(data->outfile, "<dict>\n");
      cupsFilePuts(data->outfile, "<key>Name</key>\n");
      print_xml_string(data->outfile, "string", name);
      if (file_id[0])
      {
	cupsFilePuts(data->outfile, "<key>FileId</key>\n");
	print_xml_string(data->outfile, "string", file_id);
      }
      if (test_id[0])
      {
        cupsFilePuts(data->outfile, "<key>TestId</key>\n");
        print_xml_string(data->outfile, "string", test_id);
      }
      cupsFilePuts(data->outfile, "<key>Version</key>\n");
      cupsFilePrintf(data->outfile, "<string>%d.%d</string>\n", version / 10, version % 10);
      cupsFilePuts(data->outfile, "<key>Operation</key>\n");
      print_xml_string(data->outfile, "string", ippOpString(op));
      cupsFilePuts(data->outfile, "<key>RequestId</key>\n");
      cupsFilePrintf(data->outfile, "<integer>%d</integer>\n", request_id);
      cupsFilePuts(data->outfile, "<key>RequestAttributes</key>\n");
      cupsFilePuts(data->outfile, "<array>\n");
      if (request->attrs)
      {
	cupsFilePuts(data->outfile, "<dict>\n");
	for (attrptr = request->attrs,
	         group = attrptr ? attrptr->group_tag : IPP_TAG_ZERO;
	     attrptr;
	     attrptr = attrptr->next)
	  print_attr(data->outfile, Output, attrptr, &group);
	cupsFilePuts(data->outfile, "</dict>\n");
      }
      cupsFilePuts(data->outfile, "</array>\n");
    }

    if (Output == _CUPS_OUTPUT_TEST || (Output == _CUPS_OUTPUT_PLIST && outfile != cupsFileStdout()))
    {
      if (Verbosity)
      {
	cupsFilePrintf(cupsFileStdout(), "    %s:\n", ippOpString(op));

	for (attrptr = request->attrs; attrptr; attrptr = attrptr->next)
	  print_attr(cupsFileStdout(), _CUPS_OUTPUT_TEST, attrptr, NULL);
      }

      cupsFilePrintf(cupsFileStdout(), "    %-68.68s [", name);
    }

    if ((skip_previous && !prev_pass) || skip_test)
    {
      SkipCount ++;

      ippDelete(request);
      request = NULL;

      if (Output == _CUPS_OUTPUT_PLIST)
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

      if (Output == _CUPS_OUTPUT_TEST || (Output == _CUPS_OUTPUT_PLIST && outfile != cupsFileStdout()))
	cupsFilePuts(cupsFileStdout(), "SKIP]\n");

      goto skip_error;
    }

    PasswordTries   = 0;

    do
    {
      if (delay > 0)
        usleep(delay);

      delay = repeat_interval;
      repeat_count ++;

      status = HTTP_STATUS_OK;

      if (transfer == _CUPS_TRANSFER_CHUNKED ||
	  (transfer == _CUPS_TRANSFER_AUTO && filename[0]))
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

	if (filename[0] && (reqfile = cupsFileOpen(filename, "r")) != NULL)
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

      response    = NULL;
      repeat_test = 0;
      prev_pass   = 1;

      if (status != HTTP_STATUS_ERROR)
      {
	while (!response && !Cancel && prev_pass)
	{
	  status = cupsSendRequest(http, request, resource, length);

#ifdef HAVE_LIBZ
	  if (compression[0])
	    httpSetField(http, HTTP_FIELD_CONTENT_ENCODING, compression);
#endif /* HAVE_LIBZ */

	  if (!Cancel && status == HTTP_STATUS_CONTINUE &&
	      request->state == IPP_DATA && filename[0])
	  {
	    if ((reqfile = cupsFileOpen(filename, "r")) != NULL)
	    {
	      while (!Cancel &&
	             (bytes = cupsFileRead(reqfile, buffer, sizeof(buffer))) > 0)
		if ((status = cupsWriteRequestData(http, buffer, (size_t)bytes)) != HTTP_STATUS_CONTINUE)
		  break;

	      cupsFileClose(reqfile);
	    }
	    else
	    {
	      snprintf(buffer, sizeof(buffer), "%s: %s", filename,
		       strerror(errno));
	      _cupsSetError(IPP_INTERNAL_ERROR, buffer, 0);

	      status = HTTP_STATUS_ERROR;
	    }
	  }

	 /*
	  * Get the server's response...
	  */

	  if (!Cancel && status != HTTP_STATUS_ERROR)
	  {
	    response = cupsGetResponse(http, resource);
	    status   = httpGetStatus(http);
	  }

	  if (!Cancel && status == HTTP_STATUS_ERROR && http->error != EINVAL &&
#ifdef WIN32
	      http->error != WSAETIMEDOUT)
#else
	      http->error != ETIMEDOUT)
#endif /* WIN32 */
	  {
	    if (httpReconnect2(http, 30000, NULL))
	      prev_pass = 0;
	  }
	  else if (status == HTTP_STATUS_ERROR || status == HTTP_STATUS_CUPS_AUTHORIZATION_CANCELED)
	  {
	    prev_pass = 0;
	    break;
	  }
	  else if (status != HTTP_STATUS_OK)
	  {
	    httpFlush(http);

	    if (status == HTTP_STATUS_UNAUTHORIZED)
	      continue;

	    break;
	  }
	}
      }

      if (!Cancel && status == HTTP_STATUS_ERROR && http->error != EINVAL &&
#ifdef WIN32
	  http->error != WSAETIMEDOUT)
#else
	  http->error != ETIMEDOUT)
#endif /* WIN32 */
      {
	if (httpReconnect2(http, 30000, NULL))
	  prev_pass = 0;
      }
      else if (status == HTTP_STATUS_ERROR)
      {
        if (!Cancel)
          httpReconnect2(http, 30000, NULL);

	prev_pass = 0;
      }
      else if (status != HTTP_STATUS_OK)
      {
        httpFlush(http);
        prev_pass = 0;
      }

     /*
      * Check results of request...
      */

      cupsArrayClear(errors);

      if (http->version != HTTP_1_1)
	add_stringf(errors, "Bad HTTP version (%d.%d)", http->version / 100,
		    http->version % 100);

      if (ValidateHeaders)
      {
        const char *header;               /* HTTP header value */

        if ((header = httpGetField(http, HTTP_FIELD_CONTENT_TYPE)) == NULL || _cups_strcasecmp(header, "application/ipp"))
          add_stringf(errors, "Bad HTTP Content-Type in response (%s)", header && *header ? header : "<missing>");

        if ((header = httpGetField(http, HTTP_FIELD_DATE)) != NULL && *header && httpGetDateTime(header) == 0)
          add_stringf(errors, "Bad HTTP Date in response (%s)", header);
      }

      if (!response)
      {
       /*
        * No response, log error...
        */

	add_stringf(errors, "IPP request failed with status %s (%s)",
		    ippErrorString(cupsLastError()),
		    cupsLastErrorString());
      }
      else
      {
       /*
        * Collect common attribute values...
        */

	if ((attrptr = ippFindAttribute(response, "job-id",
					IPP_TAG_INTEGER)) != NULL)
	{
	  snprintf(temp, sizeof(temp), "%d", attrptr->values[0].integer);
	  _ippVarsSet(vars, "job-id", temp);
	}

	if ((attrptr = ippFindAttribute(response, "job-uri",
					IPP_TAG_URI)) != NULL)
	  _ippVarsSet(vars, "job-uri", attrptr->values[0].string.text);

	if ((attrptr = ippFindAttribute(response, "notify-subscription-id",
					IPP_TAG_INTEGER)) != NULL)
	{
	  snprintf(temp, sizeof(temp), "%d", attrptr->values[0].integer);
	  _ippVarsSet(vars, "notify-subscription-id", temp);
	}

       /*
        * Check response, validating groups and attributes and logging errors
        * as needed...
        */

	if (response->state != IPP_DATA)
	  add_stringf(errors,
	              "Missing end-of-attributes-tag in response "
		      "(RFC 2910 section 3.5.1)");

	if (version &&
	    (response->request.status.version[0] != (version / 10) ||
	     response->request.status.version[1] != (version % 10)))
          add_stringf(errors,
                      "Bad version %d.%d in response - expected %d.%d "
		      "(RFC 2911 section 3.1.8).",
		      response->request.status.version[0],
		      response->request.status.version[1],
		      version / 10, version % 10);

	if (response->request.status.request_id != request_id)
	  add_stringf(errors,
	              "Bad request ID %d in response - expected %d "
		      "(RFC 2911 section 3.1.1)",
		      response->request.status.request_id, request_id);

	attrptr = response->attrs;
	if (!attrptr)
	  add_stringf(errors,
	              "Missing first attribute \"attributes-charset "
		      "(charset)\" in group operation-attributes-tag "
		      "(RFC 2911 section 3.1.4).");
	else
	{
	  if (!attrptr->name ||
	      attrptr->value_tag != IPP_TAG_CHARSET ||
	      attrptr->group_tag != IPP_TAG_OPERATION ||
	      attrptr->num_values != 1 ||
	      strcmp(attrptr->name, "attributes-charset"))
	    add_stringf(errors,
	                "Bad first attribute \"%s (%s%s)\" in group %s, "
			"expected \"attributes-charset (charset)\" in "
			"group operation-attributes-tag (RFC 2911 section "
			"3.1.4).",
			attrptr->name ? attrptr->name : "(null)",
			attrptr->num_values > 1 ? "1setOf " : "",
			ippTagString(attrptr->value_tag),
			ippTagString(attrptr->group_tag));

	  attrptr = attrptr->next;
	  if (!attrptr)
	    add_stringf(errors,
	                "Missing second attribute \"attributes-natural-"
			"language (naturalLanguage)\" in group "
			"operation-attributes-tag (RFC 2911 section "
			"3.1.4).");
	  else if (!attrptr->name ||
	           attrptr->value_tag != IPP_TAG_LANGUAGE ||
	           attrptr->group_tag != IPP_TAG_OPERATION ||
	           attrptr->num_values != 1 ||
	           strcmp(attrptr->name, "attributes-natural-language"))
	    add_stringf(errors,
	                "Bad first attribute \"%s (%s%s)\" in group %s, "
			"expected \"attributes-natural-language "
			"(naturalLanguage)\" in group "
			"operation-attributes-tag (RFC 2911 section "
			"3.1.4).",
			attrptr->name ? attrptr->name : "(null)",
			attrptr->num_values > 1 ? "1setOf " : "",
			ippTagString(attrptr->value_tag),
			ippTagString(attrptr->group_tag));
        }

	if ((attrptr = ippFindAttribute(response, "status-message",
					 IPP_TAG_ZERO)) != NULL)
	{
	  if (attrptr->value_tag != IPP_TAG_TEXT)
	    add_stringf(errors,
	                "status-message (text(255)) has wrong value tag "
			"%s (RFC 2911 section 3.1.6.2).",
			ippTagString(attrptr->value_tag));
	  if (attrptr->group_tag != IPP_TAG_OPERATION)
	    add_stringf(errors,
	                "status-message (text(255)) has wrong group tag "
			"%s (RFC 2911 section 3.1.6.2).",
			ippTagString(attrptr->group_tag));
	  if (attrptr->num_values != 1)
	    add_stringf(errors,
	                "status-message (text(255)) has %d values "
			"(RFC 2911 section 3.1.6.2).",
			attrptr->num_values);
	  if (attrptr->value_tag == IPP_TAG_TEXT &&
	      strlen(attrptr->values[0].string.text) > 255)
	    add_stringf(errors,
	                "status-message (text(255)) has bad length %d"
			" (RFC 2911 section 3.1.6.2).",
			(int)strlen(attrptr->values[0].string.text));
        }

	if ((attrptr = ippFindAttribute(response, "detailed-status-message",
					 IPP_TAG_ZERO)) != NULL)
	{
	  if (attrptr->value_tag != IPP_TAG_TEXT)
	    add_stringf(errors,
	                "detailed-status-message (text(MAX)) has wrong "
			"value tag %s (RFC 2911 section 3.1.6.3).",
			ippTagString(attrptr->value_tag));
	  if (attrptr->group_tag != IPP_TAG_OPERATION)
	    add_stringf(errors,
	                "detailed-status-message (text(MAX)) has wrong "
			"group tag %s (RFC 2911 section 3.1.6.3).",
			ippTagString(attrptr->group_tag));
	  if (attrptr->num_values != 1)
	    add_stringf(errors,
	                "detailed-status-message (text(MAX)) has %d values"
			" (RFC 2911 section 3.1.6.3).",
			attrptr->num_values);
	  if (attrptr->value_tag == IPP_TAG_TEXT &&
	      strlen(attrptr->values[0].string.text) > 1023)
	    add_stringf(errors,
	                "detailed-status-message (text(MAX)) has bad "
			"length %d (RFC 2911 section 3.1.6.3).",
			(int)strlen(attrptr->values[0].string.text));
        }

	a = cupsArrayNew((cups_array_func_t)strcmp, NULL);

	for (attrptr = response->attrs,
	         group = attrptr ? attrptr->group_tag : IPP_TAG_ZERO;
	     attrptr;
	     attrptr = attrptr->next)
	{
	  if (attrptr->group_tag != group)
	  {
	    int out_of_order = 0;	/* Are attribute groups out-of-order? */
	    cupsArrayClear(a);

            switch (attrptr->group_tag)
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
                  if (group != IPP_TAG_OPERATION &&
                      group != IPP_TAG_UNSUPPORTED_GROUP)
		    out_of_order = 1;
                  break;

              case IPP_TAG_SUBSCRIPTION :
                  if (group > attrptr->group_tag &&
                      group != IPP_TAG_DOCUMENT)
		    out_of_order = 1;
                  break;

              default :
                  if (group > attrptr->group_tag)
		    out_of_order = 1;
                  break;
            }

            if (out_of_order)
	      add_stringf(errors, "Attribute groups out of order (%s < %s)",
			  ippTagString(attrptr->group_tag),
			  ippTagString(group));

	    if (attrptr->group_tag != IPP_TAG_ZERO)
	      group = attrptr->group_tag;
	  }

	  validate_attr(data->outfile, errors, attrptr);

          if (attrptr->name)
          {
            if (cupsArrayFind(a, attrptr->name))
              add_stringf(errors, "Duplicate \"%s\" attribute in %s group",
			  attrptr->name, ippTagString(group));

            cupsArrayAdd(a, attrptr->name);
          }
	}

        cupsArrayDelete(a);

       /*
        * Now check the test-defined expected status-code and attribute
        * values...
        */

	for (i = 0, status_ok = 0; i < num_statuses; i ++)
	{
	  if (statuses[i].if_defined &&
	      !_ippVarsGet(vars, statuses[i].if_defined))
	    continue;

	  if (statuses[i].if_not_defined &&
	      _ippVarsGet(vars, statuses[i].if_not_defined))
	    continue;

	  if (ippGetStatusCode(response) == statuses[i].status)
	  {
            status_ok = 1;

            if (statuses[i].repeat_match && repeat_count < statuses[i].repeat_limit)
              repeat_test = 1;

            if (statuses[i].define_match)
              _ippVarsSet(vars, statuses[i].define_match, "1");
	  }
	  else
	  {
	    if (statuses[i].repeat_no_match && repeat_count < statuses[i].repeat_limit)
              repeat_test = 1;

            if (statuses[i].define_no_match)
            {
              _ippVarsSet(vars, statuses[i].define_no_match, "1");
              status_ok = 1;
            }
          }
	}

	if (!status_ok && num_statuses > 0)
	{
	  for (i = 0; i < num_statuses; i ++)
	  {
	    if (statuses[i].if_defined &&
		!_ippVarsGet(vars, statuses[i].if_defined))
	      continue;

	    if (statuses[i].if_not_defined &&
		_ippVarsGet(vars, statuses[i].if_not_defined))
	      continue;

            if (!statuses[i].repeat_match || repeat_count >= statuses[i].repeat_limit)
	      add_stringf(errors, "EXPECTED: STATUS %s (got %s)",
			  ippErrorString(statuses[i].status),
			  ippErrorString(cupsLastError()));
	  }

	  if ((attrptr = ippFindAttribute(response, "status-message",
					  IPP_TAG_TEXT)) != NULL)
	    add_stringf(errors, "status-message=\"%s\"",
			attrptr->values[0].string.text);
        }

	for (i = num_expects, expect = expects; i > 0; i --, expect ++)
	{
	  if (expect->if_defined && !_ippVarsGet(vars, expect->if_defined))
	    continue;

	  if (expect->if_not_defined &&
	      _ippVarsGet(vars, expect->if_not_defined))
	    continue;

          found = ippFindAttribute(response, expect->name, IPP_TAG_ZERO);

          do
          {
	    if ((found && expect->not_expect) ||
		(!found && !(expect->not_expect || expect->optional)) ||
		(found && !expect_matches(expect, found->value_tag)) ||
		(found && expect->in_group &&
		 found->group_tag != expect->in_group))
	    {
	      if (expect->define_no_match)
		_ippVarsSet(vars, expect->define_no_match, "1");
	      else if (!expect->define_match && !expect->define_value)
	      {
		if (found && expect->not_expect)
		  add_stringf(errors, "NOT EXPECTED: %s", expect->name);
		else if (!found && !(expect->not_expect || expect->optional))
		  add_stringf(errors, "EXPECTED: %s", expect->name);
		else if (found)
		{
		  if (!expect_matches(expect, found->value_tag))
		    add_stringf(errors, "EXPECTED: %s OF-TYPE %s (got %s)",
				expect->name, expect->of_type,
				ippTagString(found->value_tag));

		  if (expect->in_group && found->group_tag != expect->in_group)
		    add_stringf(errors, "EXPECTED: %s IN-GROUP %s (got %s).",
				expect->name, ippTagString(expect->in_group),
				ippTagString(found->group_tag));
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
		add_stringf(errors, "EXPECTED: %s WITH-VALUES-FROM %s", expect->name, expect->with_value_from);

		with_value_from(errors, ippFindAttribute(response, expect->with_value_from, IPP_TAG_ZERO), found, buffer, sizeof(buffer));
	      }

	      if (expect->repeat_no_match && repeat_count < expect->repeat_limit)
		repeat_test = 1;

	      break;
	    }
	    else if (found && !with_value(data->outfile, NULL, expect->with_value, expect->with_flags, found, buffer, sizeof(buffer)))
	    {
	      if (expect->define_no_match)
		_ippVarsSet(vars, expect->define_no_match, "1");
	      else if (!expect->define_match && !expect->define_value &&
		       !expect->repeat_match && (!expect->repeat_no_match || repeat_count >= expect->repeat_limit))
	      {
		if (expect->with_flags & _CUPS_WITH_REGEX)
		  add_stringf(errors, "EXPECTED: %s %s /%s/", expect->name, with_flags_string(expect->with_flags), expect->with_value);
		else
		  add_stringf(errors, "EXPECTED: %s %s \"%s\"", expect->name, with_flags_string(expect->with_flags), expect->with_value);

		with_value(data->outfile, errors, expect->with_value, expect->with_flags, found, buffer, sizeof(buffer));
	      }

	      if (expect->repeat_no_match &&
		  repeat_count < expect->repeat_limit)
		repeat_test = 1;

	      break;
	    }

	    if (found && expect->count > 0 &&
		found->num_values != expect->count)
	    {
	      if (expect->define_no_match)
		_ippVarsSet(vars, expect->define_no_match, "1");
	      else if (!expect->define_match && !expect->define_value)
	      {
		add_stringf(errors, "EXPECTED: %s COUNT %d (got %d)", expect->name,
			    expect->count, found->num_values);
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

	      if (!attrptr || attrptr->num_values != found->num_values)
	      {
		if (expect->define_no_match)
		  _ippVarsSet(vars, expect->define_no_match, "1");
		else if (!expect->define_match && !expect->define_value)
		{
		  if (!attrptr)
		    add_stringf(errors,
				"EXPECTED: %s (%d values) SAME-COUNT-AS %s "
				"(not returned)", expect->name,
				found->num_values, expect->same_count_as);
		  else if (attrptr->num_values != found->num_values)
		    add_stringf(errors,
				"EXPECTED: %s (%d values) SAME-COUNT-AS %s "
				"(%d values)", expect->name, found->num_values,
				expect->same_count_as, attrptr->num_values);
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
	if (Output == _CUPS_OUTPUT_TEST || (Output == _CUPS_OUTPUT_PLIST && outfile != cupsFileStdout()))
        {
          cupsFilePrintf(cupsFileStdout(), "%04d]\n", repeat_count);
\
	  if (num_displayed > 0)
	  {
	    for (attrptr = ippFirstAttribute(response); attrptr; attrptr = ippNextAttribute(response))
	    {
	      const char *attrname = ippGetName(attrptr);
	      if (attrname)
	      {
		for (i = 0; i < num_displayed; i ++)
		{
		  if (!strcmp(displayed[i], attrname))
		  {
		    print_attr(cupsFileStdout(), _CUPS_OUTPUT_TEST, attrptr, NULL);
		    break;
		  }
		}
	      }
	    }
	  }
        }

	if (Output == _CUPS_OUTPUT_TEST || (Output == _CUPS_OUTPUT_PLIST && outfile != cupsFileStdout()))
	{
	  cupsFilePrintf(cupsFileStdout(), "    %-68.68s [", name);
	}

        ippDelete(response);
        response = NULL;
      }
    }
    while (repeat_test);

    ippDelete(request);

    request = NULL;

    if (cupsArrayCount(errors) > 0)
      prev_pass = pass = 0;

    if (prev_pass)
      PassCount ++;
    else
      FailCount ++;

    if (Output == _CUPS_OUTPUT_PLIST)
    {
      cupsFilePuts(data->outfile, "<key>Successful</key>\n");
      cupsFilePuts(data->outfile, prev_pass ? "<true />\n" : "<false />\n");
      cupsFilePuts(data->outfile, "<key>StatusCode</key>\n");
      print_xml_string(data->outfile, "string", ippErrorString(cupsLastError()));
      cupsFilePuts(data->outfile, "<key>ResponseAttributes</key>\n");
      cupsFilePuts(data->outfile, "<array>\n");
      cupsFilePuts(data->outfile, "<dict>\n");
      for (attrptr = response ? response->attrs : NULL,
               group = attrptr ? attrptr->group_tag : IPP_TAG_ZERO;
	   attrptr;
	   attrptr = attrptr->next)
	print_attr(data->outfile, Output, attrptr, &group);
      cupsFilePuts(data->outfile, "</dict>\n");
      cupsFilePuts(data->outfile, "</array>\n");
    }
    else if (Output == _CUPS_OUTPUT_IPPSERVER && response)
    {
      for (attrptr = ippFirstAttribute(response), group = IPP_TAG_ZERO; attrptr; attrptr = ippNextAttribute(response))
      {
        if (!ippGetName(attrptr) || ippGetGroupTag(attrptr) != IPP_TAG_PRINTER)
          continue;

        print_ippserver_attr(data->outfile, attrptr, 0);
      }
    }

    if (Output == _CUPS_OUTPUT_TEST || (Output == _CUPS_OUTPUT_PLIST && outfile != cupsFileStdout()))
    {
      cupsFilePuts(cupsFileStdout(), prev_pass ? "PASS]\n" : "FAIL]\n");

      if (!prev_pass || (Verbosity && response))
      {
	cupsFilePrintf(cupsFileStdout(), "        RECEIVED: %lu bytes in response\n", (unsigned long)ippLength(response));
	cupsFilePrintf(cupsFileStdout(), "        status-code = %s (%s)\n", ippErrorString(cupsLastError()), cupsLastErrorString());

        if (Verbosity && response)
        {
	  for (attrptr = response->attrs;
	       attrptr != NULL;
	       attrptr = attrptr->next)
	    print_attr(cupsFileStdout(), _CUPS_OUTPUT_TEST, attrptr, NULL);
	}
      }
    }
    else if (!prev_pass && Output != _CUPS_OUTPUT_QUIET)
      fprintf(stderr, "%s\n", cupsLastErrorString());

    if (prev_pass && Output >= _CUPS_OUTPUT_LIST && !Verbosity &&
        num_displayed > 0)
    {
      size_t	width;			/* Length of value */

      for (i = 0; i < num_displayed; i ++)
      {
	widths[i] = strlen(displayed[i]);

	for (attrptr = ippFindAttribute(response, displayed[i], IPP_TAG_ZERO);
	     attrptr;
	     attrptr = ippFindNextAttribute(response, displayed[i],
					    IPP_TAG_ZERO))
	{
	  width = ippAttributeString(attrptr, NULL, 0);
	  if (width > widths[i])
	    widths[i] = width;
	}
      }

      if (Output == _CUPS_OUTPUT_CSV)
	print_csv(data->outfile, NULL, num_displayed, displayed, widths);
      else
	print_line(data->outfile, NULL, num_displayed, displayed, widths);

      attrptr = response->attrs;

      while (attrptr)
      {
	while (attrptr && attrptr->group_tag <= IPP_TAG_OPERATION)
	  attrptr = attrptr->next;

	if (attrptr)
	{
	  if (Output == _CUPS_OUTPUT_CSV)
	    print_csv(data->outfile, attrptr, num_displayed, displayed, widths);
	  else
	    print_line(data->outfile, attrptr, num_displayed, displayed, widths);

	  while (attrptr && attrptr->group_tag > IPP_TAG_OPERATION)
	    attrptr = attrptr->next;
	}
      }
    }
    else if (!prev_pass)
    {
      if (Output == _CUPS_OUTPUT_PLIST)
      {
	cupsFilePuts(data->outfile, "<key>Errors</key>\n");
	cupsFilePuts(data->outfile, "<array>\n");

	for (error = (char *)cupsArrayFirst(errors);
	     error;
	     error = (char *)cupsArrayNext(errors))
	  print_xml_string(data->outfile, "string", error);

	cupsFilePuts(data->outfile, "</array>\n");
      }

      if (Output == _CUPS_OUTPUT_TEST || (Output == _CUPS_OUTPUT_PLIST && outfile != cupsFileStdout()))
      {
	for (error = (char *)cupsArrayFirst(errors);
	     error;
	     error = (char *)cupsArrayNext(errors))
	  cupsFilePrintf(cupsFileStdout(), "        %s\n", error);
      }
    }

    if (num_displayed > 0 && !Verbosity && response && (Output == _CUPS_OUTPUT_TEST || (Output == _CUPS_OUTPUT_PLIST && outfile != cupsFileStdout())))
    {
      for (attrptr = response->attrs;
	   attrptr != NULL;
	   attrptr = attrptr->next)
      {
	if (attrptr->name)
	{
	  for (i = 0; i < num_displayed; i ++)
	  {
	    if (!strcmp(displayed[i], attrptr->name))
	    {
	      print_attr(data->outfile, Output, attrptr, NULL);
	      break;
	    }
	  }
	}
      }
    }

    skip_error:

    if (Output == _CUPS_OUTPUT_PLIST)
      cupsFilePuts(data->outfile, "</dict>\n");

    ippDelete(response);
    response = NULL;

    for (i = 0; i < num_statuses; i ++)
    {
      if (statuses[i].if_defined)
        free(statuses[i].if_defined);
      if (statuses[i].if_not_defined)
        free(statuses[i].if_not_defined);
      if (statuses[i].define_match)
        free(statuses[i].define_match);
      if (statuses[i].define_no_match)
        free(statuses[i].define_no_match);
    }
    num_statuses = 0;

    for (i = num_expects, expect = expects; i > 0; i --, expect ++)
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
    num_expects = 0;

    for (i = 0; i < num_displayed; i ++)
      free(displayed[i]);
    num_displayed = 0;

    if (!ignore_errors && !prev_pass)
      break;
  }

  test_exit:

  cupsArrayDelete(errors);

  if (fp)
    cupsFileClose(fp);

  httpClose(http);
  ippDelete(request);
  ippDelete(response);

  for (i = 0; i < num_statuses; i ++)
  {
    if (statuses[i].if_defined)
      free(statuses[i].if_defined);
    if (statuses[i].if_not_defined)
      free(statuses[i].if_not_defined);
    if (statuses[i].define_match)
      free(statuses[i].define_match);
    if (statuses[i].define_no_match)
      free(statuses[i].define_no_match);
  }

  for (i = num_expects, expect = expects; i > 0; i --, expect ++)
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

  for (i = 0; i < num_displayed; i ++)
    free(displayed[i]);

  return (pass);
}


/*
 * 'error_cb()' - Print/add an error message.
 */

static int				/* O - 1 to continue, 0 to stop */
error_cb(_ipp_file_t      *f,		/* I - IPP file data */
         _cups_testdata_t *data,	/* I - Test data */
         const char       *error)	/* I - Error message */
{
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
 * 'get_collection()' - Get a collection value from the current test file.
 */

static ipp_t *				/* O  - Collection value */
get_collection(cups_file_t  *outfile,	/* I  - Output file */
               _cups_vars_t *vars,	/* I  - Variables */
               cups_file_t  *fp,	/* I  - File to read from */
	       int          *linenum)	/* IO - Line number */
{
  char		token[1024],		/* Token from file */
		temp[1024],		/* Temporary string */
		attr[128];		/* Attribute name */
  ipp_tag_t	value;			/* Current value type */
  ipp_t		*col = ippNew();	/* Collection value */
  ipp_attribute_t *lastcol = NULL;	/* Last collection attribute */


  while (get_token(fp, token, sizeof(token), f->linenum, f->filename) != NULL)
  {
    if (!strcmp(token, "}"))
      break;
    else if (!strcmp(token, "{") && lastcol)
    {
     /*
      * Another collection value
      */

      ipp_t	*subcol = get_collection(data->outfile, vars, fp, f->linenum, f->filename);
					/* Collection value */

      if (subcol)
        ippSetCollection(col, &lastcol, ippGetCount(lastcol), subcol);
      else
	goto col_error;
    }
    else if (!_cups_strcasecmp(token, "MEMBER"))
    {
     /*
      * Attribute...
      */

      lastcol = NULL;

      if (!get_token(fp, token, sizeof(token), f->linenum, f->filename))
      {
	print_fatal_error(data, "Missing MEMBER value tag on line %d of \"%s\".", *linenum);
	goto col_error;
      }

      if ((value = ippTagValue(token)) < IPP_TAG_UNSUPPORTED_VALUE)
      {
	print_fatal_error(data, "Bad MEMBER value tag \"%s\" on line %d of \"%s\".", token,
			  *linenum);
	goto col_error;
      }

      if (!get_token(fp, attr, sizeof(attr), f->linenum, f->filename))
      {
	print_fatal_error(data, "Missing MEMBER name on line %d of \"%s\".", *linenum);
	goto col_error;
      }

      if (value < IPP_TAG_INTEGER)
      {
       /*
        * Out-of-band member attributes have no value...
        */

        token[0] = '\0';
      }
      else if (!get_token(fp, temp, sizeof(temp), f->linenum, f->filename))
      {
	print_fatal_error(data, "Missing MEMBER value on line %d of \"%s\".", *linenum);
	goto col_error;
      }
      else
      {
        _ippVarsExpand(vars, token, temp, sizeof(token));
      }

      switch (value)
      {
        default :
              if (value < IPP_TAG_INTEGER)
              {
               /*
                * Add out-of-band value...
                */

                ippAddOutOfBand(col, IPP_TAG_ZERO, value, attr);
              }
              else
              {
                print_fatal_error(data, "Unsupported MEMBER value tag %s for \"%s\" on line %d of \"%s\".", ippTagString(value), attr, *linenum);
                goto col_error;
              }
              break;

        case IPP_TAG_BOOLEAN :
	    if (!_cups_strcasecmp(token, "true"))
	      ippAddBoolean(col, IPP_TAG_ZERO, attr, 1);
	    else
	      ippAddBoolean(col, IPP_TAG_ZERO, attr, (char)atoi(token));
	    break;

	case IPP_TAG_INTEGER :
	case IPP_TAG_ENUM :
	    ippAddInteger(col, IPP_TAG_ZERO, value, attr, atoi(token));
	    break;

	case IPP_TAG_RESOLUTION :
	    {
	      int	xres,		/* X resolution */
			yres;		/* Y resolution */
	      char	units[6];	/* Units */

	      if (sscanf(token, "%dx%d%5s", &xres, &yres, units) != 3 ||
		  (_cups_strcasecmp(units, "dpi") &&
		   _cups_strcasecmp(units, "dpc") &&
		   _cups_strcasecmp(units, "dpcm") &&
		   _cups_strcasecmp(units, "other")))
	      {
		print_fatal_error(data, "Bad resolution value \"%s\" on line %d of \"%s\".",
				  token, *linenum);
		goto col_error;
	      }

	      if (!_cups_strcasecmp(units, "dpi"))
		ippAddResolution(col, IPP_TAG_ZERO, attr, IPP_RES_PER_INCH, xres, yres);
	      else if (!_cups_strcasecmp(units, "dpc") ||
	               !_cups_strcasecmp(units, "dpcm"))
		ippAddResolution(col, IPP_TAG_ZERO, attr, IPP_RES_PER_CM, xres, yres);
	      else
		ippAddResolution(col, IPP_TAG_ZERO, attr, (ipp_res_t)0, xres, yres);
	    }
	    break;

	case IPP_TAG_RANGE :
	    {
	      int	lowers[4],	/* Lower value */
			uppers[4],	/* Upper values */
			num_vals;	/* Number of values */


	      num_vals = sscanf(token, "%d-%d,%d-%d,%d-%d,%d-%d",
				lowers + 0, uppers + 0,
				lowers + 1, uppers + 1,
				lowers + 2, uppers + 2,
				lowers + 3, uppers + 3);

	      if ((num_vals & 1) || num_vals == 0)
	      {
		print_fatal_error(data, "Bad rangeOfInteger value \"%s\" on line %d of \"%s\".",
		                  token, *linenum);
		goto col_error;
	      }

	      ippAddRanges(col, IPP_TAG_ZERO, attr, num_vals / 2, lowers,
			   uppers);
	    }
	    break;

	case IPP_TAG_BEGIN_COLLECTION :
	    if (!strcmp(token, "{"))
	    {
	      ipp_t	*subcol = get_collection(data->outfile, vars, fp, f->linenum, f->filename);
				      /* Collection value */

	      if (subcol)
	      {
		lastcol = ippAddCollection(col, IPP_TAG_ZERO, attr, subcol);
		ippDelete(subcol);
	      }
	      else
		goto col_error;
	    }
	    else
	    {
	      print_fatal_error(data, "Bad collection value on line %d of \"%s\".", *linenum);
	      goto col_error;
	    }
	    break;

        case IPP_TAG_STRING :
	    ippAddOctetString(col, IPP_TAG_ZERO, attr, token, (int)strlen(token));
	    break;

        case IPP_TAG_TEXTLANG :
        case IPP_TAG_NAMELANG :
        case IPP_TAG_TEXT :
        case IPP_TAG_NAME :
        case IPP_TAG_KEYWORD :
        case IPP_TAG_URI :
        case IPP_TAG_URISCHEME :
        case IPP_TAG_CHARSET :
        case IPP_TAG_LANGUAGE :
        case IPP_TAG_MIMETYPE :
	    if (!strchr(token, ','))
	      ippAddString(col, IPP_TAG_ZERO, value, attr, NULL, token);
	    else
	    {
	     /*
	      * Multiple string values...
	      */

	      int	num_values;	/* Number of values */
	      char	*values[100],	/* Values */
			*ptr;		/* Pointer to next value */


	      values[0]  = token;
	      num_values = 1;

	      for (ptr = strchr(token, ','); ptr; ptr = strchr(ptr, ','))
	      {
                if (ptr > token && ptr[-1] == '\\')
                  _cups_strcpy(ptr - 1, ptr);
                else
                {
                  *ptr++ = '\0';
                  values[num_values] = ptr;
                  num_values ++;
                }
	      }

	      ippAddStrings(col, IPP_TAG_ZERO, value, attr, num_values,
			    NULL, (const char **)values);
	    }
	    break;
      }
    }
    else
    {
      print_fatal_error(data, "Unexpected token %s seen on line %d of \"%s\".", token, *linenum);
      goto col_error;
    }
  }

  return (col);

 /*
  * If we get here there was a parse error; free memory and return.
  */

  col_error:

  ippDelete(col);

  return (NULL);
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
  else if (*src == '/' || !strchr(testfile, '/')
#ifdef WIN32
           || (isalpha(*src & 255) && src[1] == ':')
#endif /* WIN32 */
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
 * 'get_token()' - Get a token from a file.
 */

static char *				/* O  - Token from file or NULL on EOF */
get_token(cups_file_t *fp,		/* I  - File to read from */
          char        *buf,		/* I  - Buffer to read into */
	  int         buflen,		/* I  - Length of buffer */
	  int         *linenum)		/* IO - Current line number */
{
  int	ch,				/* Character from file */
	quote;				/* Quoting character */
  char	*bufptr,			/* Pointer into buffer */
	*bufend;			/* End of buffer */


  for (;;)
  {
   /*
    * Skip whitespace...
    */

    while (isspace(ch = cupsFileGetChar(fp)))
    {
      if (ch == '\n')
        (*linenum) ++;
    }

   /*
    * Read a token...
    */

    if (ch == EOF)
      return (NULL);
    else if (ch == '\'' || ch == '\"')
    {
     /*
      * Quoted text or regular expression...
      */

      quote  = ch;
      bufptr = buf;
      bufend = buf + buflen - 1;

      while ((ch = cupsFileGetChar(fp)) != EOF)
      {
        if (ch == '\\')
	{
	 /*
	  * Escape next character...
	  */

	  if (bufptr < bufend)
	    *bufptr++ = (char)ch;

	  if ((ch = cupsFileGetChar(fp)) != EOF && bufptr < bufend)
	    *bufptr++ = (char)ch;
	}
	else if (ch == quote)
          break;
	else if (bufptr < bufend)
          *bufptr++ = (char)ch;
      }

      *bufptr = '\0';

      return (buf);
    }
    else if (ch == '#')
    {
     /*
      * Comment...
      */

      while ((ch = cupsFileGetChar(fp)) != EOF)
	if (ch == '\n')
          break;

      (*linenum) ++;
    }
    else if (ch == '{' || ch == '}' || ch == ',')
    {
      buf[0] = (char)ch;
      buf[1] = '\0';

      return (buf);
    }
    else
    {
     /*
      * Whitespace delimited text...
      */

      cupsFileSeek(fp, cupsFileTell(fp) - 1);

      bufptr = buf;
      bufend = buf + buflen - 1;

      while ((ch = cupsFileGetChar(fp)) != EOF)
	if (isspace(ch) || ch == '#')
          break;
	else if (bufptr < bufend)
          *bufptr++ = (char)ch;

      if (ch == '#')
        cupsFileSeek(fp, cupsFileTell(fp) - 1);
      else if (ch == '\n')
        (*linenum) ++;

      *bufptr = '\0';

      return (buf);
    }
  }
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
  data->prev_pass    = 1;
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
  struct tm	*utcdate;		/* UTC date/time */
  static char	buffer[255];		/* String buffer */


  utctime = ippDateToTime(date);
  utcdate = gmtime(&utctime);

  snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%02dZ",
	   utcdate->tm_year + 1900, utcdate->tm_mon + 1, utcdate->tm_mday,
	   utcdate->tm_hour, utcdate->tm_min, utcdate->tm_sec);

  return (buffer);
}


/*
 * 'pause_message()' - Display the message and pause until the user presses a key.
 */

static void
pause_message(const char *message)	/* I - Message */
{
#ifdef WIN32
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
#endif /* WIN32 */

 /*
  * Display the prompt...
  */

  cupsFilePrintf(cupsFileStdout(), "%s\n---- PRESS ANY KEY ----", message);

#ifdef WIN32
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
#endif /* WIN32 */

 /*
  * Erase the "press any key" prompt...
  */

  cupsFilePuts(cupsFileStdout(), "\r                       \r");
}


/*
 * 'print_attr()' - Print an attribute on the screen.
 */

static void
print_attr(_cups_testdata_t *data,	/* I  - Test data */
           ipp_attribute_t *attr,	/* I  - Attribute to print */
           ipp_tag_t       *group)	/* IO - Current group */
{
  int			i;		/* Looping var */
  ipp_attribute_t	*colattr;	/* Collection attribute */


  if (format == _CUPS_OUTPUT_PLIST)
  {
    if (!attr->name || (group && *group != attr->group_tag))
    {
      if (attr->group_tag != IPP_TAG_ZERO)
      {
	cupsFilePuts(data->outfile, "</dict>\n");
	cupsFilePuts(data->outfile, "<dict>\n");
      }

      if (group)
        *group = attr->group_tag;
    }

    if (!attr->name)
      return;

    print_xml_string(data->outfile, "key", attr->name);
    if (attr->num_values > 1)
      cupsFilePuts(data->outfile, "<array>\n");

    switch (attr->value_tag)
    {
      case IPP_TAG_INTEGER :
      case IPP_TAG_ENUM :
	  for (i = 0; i < attr->num_values; i ++)
	    cupsFilePrintf(data->outfile, "<integer>%d</integer>\n", attr->values[i].integer);
	  break;

      case IPP_TAG_BOOLEAN :
	  for (i = 0; i < attr->num_values; i ++)
	    cupsFilePuts(data->outfile, attr->values[i].boolean ? "<true />\n" : "<false />\n");
	  break;

      case IPP_TAG_RANGE :
	  for (i = 0; i < attr->num_values; i ++)
	    cupsFilePrintf(data->outfile, "<dict><key>lower</key><integer>%d</integer>"
			     "<key>upper</key><integer>%d</integer></dict>\n",
		    attr->values[i].range.lower, attr->values[i].range.upper);
	  break;

      case IPP_TAG_RESOLUTION :
	  for (i = 0; i < attr->num_values; i ++)
	    cupsFilePrintf(data->outfile, "<dict><key>xres</key><integer>%d</integer>"
			     "<key>yres</key><integer>%d</integer>"
			     "<key>units</key><string>%s</string></dict>\n",
		   attr->values[i].resolution.xres,
		   attr->values[i].resolution.yres,
		   attr->values[i].resolution.units == IPP_RES_PER_INCH ?
		       "dpi" : "dpcm");
	  break;

      case IPP_TAG_DATE :
	  for (i = 0; i < attr->num_values; i ++)
	    cupsFilePrintf(data->outfile, "<date>%s</date>\n", iso_date(attr->values[i].date));
	  break;

      case IPP_TAG_STRING :
          for (i = 0; i < attr->num_values; i ++)
          {
	    char	buffer[IPP_MAX_LENGTH * 5 / 4 + 1];
					/* Output buffer */

	    cupsFilePrintf(data->outfile, "<data>%s</data>\n",
		    httpEncode64_2(buffer, sizeof(buffer),
				   attr->values[i].unknown.data,
				   attr->values[i].unknown.length));
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
	  for (i = 0; i < attr->num_values; i ++)
	    print_xml_string(data->outfile, "string", attr->values[i].string.text);
	  break;

      case IPP_TAG_TEXTLANG :
      case IPP_TAG_NAMELANG :
	  for (i = 0; i < attr->num_values; i ++)
	  {
	    cupsFilePuts(data->outfile, "<dict><key>language</key><string>");
	    print_xml_string(data->outfile, NULL, attr->values[i].string.language);
	    cupsFilePuts(data->outfile, "</string><key>string</key><string>");
	    print_xml_string(data->outfile, NULL, attr->values[i].string.text);
	    cupsFilePuts(data->outfile, "</string></dict>\n");
	  }
	  break;

      case IPP_TAG_BEGIN_COLLECTION :
	  for (i = 0; i < attr->num_values; i ++)
	  {
	    cupsFilePuts(data->outfile, "<dict>\n");
	    for (colattr = attr->values[i].collection->attrs;
		 colattr;
		 colattr = colattr->next)
	      print_attr(data->outfile, format, colattr, NULL);
	    cupsFilePuts(data->outfile, "</dict>\n");
	  }
	  break;

      default :
	  cupsFilePrintf(data->outfile, "<string>&lt;&lt;%s&gt;&gt;</string>\n", ippTagString(attr->value_tag));
	  break;
    }

    if (attr->num_values > 1)
      cupsFilePuts(data->outfile, "</array>\n");
  }
  else
  {
    char	buffer[131072];		/* Value buffer */

    if (format == _CUPS_OUTPUT_TEST)
    {
      if (!attr->name)
      {
        cupsFilePuts(data->outfile, "        -- separator --\n");
        return;
      }

      cupsFilePrintf(data->outfile, "        %s (%s%s) = ", attr->name, attr->num_values > 1 ? "1setOf " : "", ippTagString(attr->value_tag));
    }

    ippAttributeString(attr, buffer, sizeof(buffer));
    cupsFilePrintf(data->outfile, "%s\n", buffer);
  }
}


/*
 * 'print_csv()' - Print a line of CSV text.
 */

static void
print_csv(
    _cups_testdata_t *data,		/* I - Test data */
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

      for (current = attr; current; current = current->next)
      {
        if (!current->name)
          break;
        else if (!strcmp(current->name, displayed[i]))
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

  if (Output == _CUPS_OUTPUT_PLIST)
  {
    print_xml_header(data->outfile);
    print_xml_trailer(data->outfile, 0, buffer);
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
	  print_ippserver_string(data->outfile, s, (size_t)len);
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
	  print_ippserver_string(data->outfile, s, strlen(s));
	}
	break;

    case IPP_TAG_BEGIN_COLLECTION :
	for (i = 0; i < attr->num_values; i ++)
	{
	  ipp_t *col = ippGetCollection(attr, i);

	  cupsFilePuts(data->outfile, i ? ",{\n" : " {\n");
	  for (colattr = ippFirstAttribute(col); colattr; colattr = ippNextAttribute(col))
	    print_ippserver_attr(data->outfile, colattr, indent + 4);
	  cupsFilePrintf(data->outfile, "%*s}", indent, "");
	}
	break;

    default :
	cupsFilePuts(data->outfile, " \"\"");
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

      for (current = attr; current; current = current->next)
      {
        if (!current->name)
          break;
        else if (!strcmp(current->name, displayed[i]))
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
    cupsFilePuts(data->outfile, "<!DOCTYPE plist PUBLIC \"-//Apple Computer//DTD PLIST 1.0//EN\" "
         "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n");
    cupsFilePuts(data->outfile, "<plist version=\"1.0\">\n");
    cupsFilePuts(data->outfile, "<dict>\n");
    cupsFilePuts(data->outfile, "<key>ipptoolVersion</key>\n");
    cupsFilePuts(data->outfile, "<string>" CUPS_SVERSION "</string>\n");
    cupsFilePuts(data->outfile, "<key>Transfer</key>\n");
    cupsFilePrintf(data->outfile, "<string>%s</string>\n",
	    Transfer == _CUPS_TRANSFER_AUTO ? "auto" :
		Transfer == _CUPS_TRANSFER_CHUNKED ? "chunked" : "length");
    cupsFilePuts(data->outfile, "<key>Tests</key>\n");
    cupsFilePuts(data->outfile, "<array>\n");

    data->xml_header = 1;
  }
}


/*
 * 'print_xml_string()' - Print an XML string with escaping.
 */

static void
print_xml_string(
    _cups_testdata_t *data,		/* I - Test data */
    const char       *element,		/* I - Element name or NULL */
    const char       *s)		/* I - String to print */
{
  if (element)
    cupsFilePrintf(data->outfile, "<%s>", element);

  while (*s)
  {
    if (*s == '&')
      cupsFilePuts(data->outfile, "&amp;");
    else if (*s == '<')
      cupsFilePuts(data->outfile, "&lt;");
    else if (*s == '>')
      cupsFilePuts(data->outfile, "&gt;");
    else if ((*s & 0xe0) == 0xc0)
    {
     /*
      * Validate UTF-8 two-byte sequence...
      */

      if ((s[1] & 0xc0) != 0x80)
      {
        cupsFilePutChar(data->outfile, '?');
        s ++;
      }
      else
      {
        cupsFilePutChar(data->outfile, *s++);
        cupsFilePutChar(data->outfile, *s);
      }
    }
    else if ((*s & 0xf0) == 0xe0)
    {
     /*
      * Validate UTF-8 three-byte sequence...
      */

      if ((s[1] & 0xc0) != 0x80 || (s[2] & 0xc0) != 0x80)
      {
        cupsFilePutChar(data->outfile, '?');
        s += 2;
      }
      else
      {
        cupsFilePutChar(data->outfile, *s++);
        cupsFilePutChar(data->outfile, *s++);
        cupsFilePutChar(data->outfile, *s);
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
        cupsFilePutChar(data->outfile, '?');
        s += 3;
      }
      else
      {
        cupsFilePutChar(data->outfile, *s++);
        cupsFilePutChar(data->outfile, *s++);
        cupsFilePutChar(data->outfile, *s++);
        cupsFilePutChar(data->outfile, *s);
      }
    }
    else if ((*s & 0x80) || (*s < ' ' && !isspace(*s & 255)))
    {
     /*
      * Invalid control character...
      */

      cupsFilePutChar(data->outfile, '?');
    }
    else
      cupsFilePutChar(data->outfile, *s);

    s ++;
  }

  if (element)
    cupsFilePrintf(data->outfile, "</%s>\n", element);
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


#ifndef WIN32
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
#endif /* !WIN32 */


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
	value[1024];			/* Value string */


  if (f->attrs)
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
     /*
      * TODO: Call do_test and return...
      */

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

	if (!strcmp(compression, "none"))
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
	pass = 0;
	goto test_exit;
      }
    }
    else if (!_cups_strcasecmp(token, "RESOURCE"))
    {
     /*
      * Resource name...
      */

      if (!get_token(fp, resource, sizeof(resource), &linenum))
      {
	print_fatal_error(data, "Missing RESOURCE path on line %d of \"%s\".", f->linenum, f->filename);
	pass = 0;
	goto test_exit;
      }
    }
    else if (!_cups_strcasecmp(token, "OPERATION"))
    {
     /*
      * Operation...
      */

      if (!_ippFileReadToken(f, temp, sizeof(temp)))
      {
	print_fatal_error(data, "Missing OPERATION code on line %d of \"%s\".", f->linenum, f->filename);
	pass = 0;
	goto test_exit;
      }

      _ippVarsExpand(vars, token, temp, sizeof(token));

      if ((op = ippOpValue(token)) == (ipp_op_t)-1 &&
	  (op = (ipp_op_t)strtol(token, NULL, 0)) == 0)
      {
	print_fatal_error(data, "Bad OPERATION code \"%s\" on line %d of \"%s\".", token,
			  linenum);
	pass = 0;
	goto test_exit;
      }
    }
    else if (!_cups_strcasecmp(token, "GROUP"))
    {
     /*
      * Attribute group...
      */

      if (!get_token(fp, token, sizeof(token), &linenum))
      {
	print_fatal_error(data, "Missing GROUP tag on line %d of \"%s\".", f->linenum, f->filename);
	pass = 0;
	goto test_exit;
      }

      if ((value = ippTagValue(token)) == IPP_TAG_ZERO || value >= IPP_TAG_UNSUPPORTED_VALUE)
      {
	print_fatal_error(data, "Bad GROUP tag \"%s\" on line %d of \"%s\".", token, f->linenum, f->filename);
	pass = 0;
	goto test_exit;
      }

      if (value == group)
	ippAddSeparator(request);

      group = value;
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
	pass = 0;
	goto test_exit;
      }

      _ippVarsExpand(vars, token, temp, sizeof(token));

      if ((dval = _cupsStrScand(token, &tokenptr, localeconv())) < 0.0 || (*tokenptr && *tokenptr != ','))
      {
	print_fatal_error(data, "Bad DELAY value \"%s\" on line %d of \"%s\".", token,
			  linenum);
	pass = 0;
	goto test_exit;
      }

      delay = (useconds_t)(1000000.0 * dval);

      if (*tokenptr == ',')
      {
	if ((dval = _cupsStrScand(tokenptr + 1, &tokenptr, localeconv())) <= 0.0 || *tokenptr)
	{
	  print_fatal_error(data, "Bad DELAY value \"%s\" on line %d of \"%s\".", token,
			    linenum);
	  pass = 0;
	  goto test_exit;
	}

	repeat_interval = (useconds_t)(1000000.0 * dval);
      }
      else
	repeat_interval = delay;
    }
    else if (!_cups_strcasecmp(token, "FILE"))
    {
     /*
      * File...
      */

      if (!_ippFileReadToken(f, temp, sizeof(temp)))
      {
	print_fatal_error(data, "Missing FILE filename on line %d of \"%s\".", f->linenum, f->filename);
	pass = 0;
	goto test_exit;
      }

      _ippVarsExpand(vars, token, temp, sizeof(token));
      get_filename(testfile, filename, token, sizeof(filename));

      if (access(filename, R_OK))
      {
	print_fatal_error(data, "Filename \"%s\" on line %d cannot be read.",
			  temp, f->linenum, f->filename);
	print_fatal_error(data, "Filename mapped to \"%s\".", filename);
	pass = 0;
	goto test_exit;
      }
    }
    else if (!_cups_strcasecmp(token, "STATUS"))
    {
     /*
      * Status...
      */

      if (num_statuses >= (int)(sizeof(statuses) / sizeof(statuses[0])))
      {
	print_fatal_error(data, "Too many STATUS's on line %d of \"%s\".", f->linenum, f->filename);
	pass = 0;
	goto test_exit;
      }

      if (!get_token(fp, token, sizeof(token), &linenum))
      {
	print_fatal_error(data, "Missing STATUS code on line %d of \"%s\".", f->linenum, f->filename);
	pass = 0;
	goto test_exit;
      }

      if ((statuses[num_statuses].status = ippErrorValue(token))
	      == (ipp_status_t)-1 &&
	  (statuses[num_statuses].status = (ipp_status_t)strtol(token, NULL, 0)) == 0)
      {
	print_fatal_error(data, "Bad STATUS code \"%s\" on line %d of \"%s\".", token,
			  linenum);
	pass = 0;
	goto test_exit;
      }

      last_status = statuses + num_statuses;
      num_statuses ++;

      last_status->define_match    = NULL;
      last_status->define_no_match = NULL;
      last_status->if_defined      = NULL;
      last_status->if_not_defined  = NULL;
      last_status->repeat_limit    = 1000;
      last_status->repeat_match    = 0;
      last_status->repeat_no_match = 0;
    }
    else if (!_cups_strcasecmp(token, "EXPECT") || !_cups_strcasecmp(token, "EXPECT-ALL"))
    {
     /*
      * Expected attributes...
      */

      int expect_all = !_cups_strcasecmp(token, "EXPECT-ALL");

      if (num_expects >= (int)(sizeof(expects) / sizeof(expects[0])))
      {
	print_fatal_error(data, "Too many EXPECT's on line %d of \"%s\".", f->linenum, f->filename);
	pass = 0;
	goto test_exit;
      }

      if (!get_token(fp, token, sizeof(token), &linenum))
      {
	print_fatal_error(data, "Missing EXPECT name on line %d of \"%s\".", f->linenum, f->filename);
	pass = 0;
	goto test_exit;
      }

      last_expect = expects + num_expects;
      num_expects ++;

      memset(last_expect, 0, sizeof(_cups_expect_t));
      last_expect->repeat_limit = 1000;
      last_expect->expect_all   = expect_all;

      if (token[0] == '!')
      {
	last_expect->not_expect = 1;
	last_expect->name       = strdup(token + 1);
      }
      else if (token[0] == '?')
      {
	last_expect->optional = 1;
	last_expect->name     = strdup(token + 1);
      }
      else
	last_expect->name = strdup(token);
    }
    else if (!_cups_strcasecmp(token, "COUNT"))
    {
      if (!get_token(fp, token, sizeof(token), &linenum))
      {
	print_fatal_error(data, "Missing COUNT number on line %d of \"%s\".", f->linenum, f->filename);
	pass = 0;
	goto test_exit;
      }

      if ((i = atoi(token)) <= 0)
      {
	print_fatal_error(data, "Bad COUNT \"%s\" on line %d of \"%s\".", token, f->linenum, f->filename);
	pass = 0;
	goto test_exit;
      }

      if (last_expect)
	last_expect->count = i;
      else
      {
	print_fatal_error(data, "COUNT without a preceding EXPECT on line %d of \"%s\".",
			  linenum);
	pass = 0;
	goto test_exit;
      }
    }
    else if (!_cups_strcasecmp(token, "DEFINE-MATCH"))
    {
      if (!get_token(fp, token, sizeof(token), &linenum))
      {
	print_fatal_error(data, "Missing DEFINE-MATCH variable on line %d of \"%s\".",
			  linenum);
	pass = 0;
	goto test_exit;
      }

      if (last_expect)
	last_expect->define_match = strdup(token);
      else if (last_status)
	last_status->define_match = strdup(token);
      else
      {
	print_fatal_error(data, "DEFINE-MATCH without a preceding EXPECT or STATUS "
			  "on line %d of \"%s\".", f->linenum, f->filename);
	pass = 0;
	goto test_exit;
      }
    }
    else if (!_cups_strcasecmp(token, "DEFINE-NO-MATCH"))
    {
      if (!get_token(fp, token, sizeof(token), &linenum))
      {
	print_fatal_error(data, "Missing DEFINE-NO-MATCH variable on line %d of \"%s\".",
			  linenum);
	pass = 0;
	goto test_exit;
      }

      if (last_expect)
	last_expect->define_no_match = strdup(token);
      else if (last_status)
	last_status->define_no_match = strdup(token);
      else
      {
	print_fatal_error(data, "DEFINE-NO-MATCH without a preceding EXPECT or "
			  "STATUS on line %d of \"%s\".", f->linenum, f->filename);
	pass = 0;
	goto test_exit;
      }
    }
    else if (!_cups_strcasecmp(token, "DEFINE-VALUE"))
    {
      if (!get_token(fp, token, sizeof(token), &linenum))
      {
	print_fatal_error(data, "Missing DEFINE-VALUE variable on line %d of \"%s\".",
			  linenum);
	pass = 0;
	goto test_exit;
      }

      if (last_expect)
	last_expect->define_value = strdup(token);
      else
      {
	print_fatal_error(data, "DEFINE-VALUE without a preceding EXPECT on "
			  "line %d of \"%s\".", f->linenum, f->filename);
	pass = 0;
	goto test_exit;
      }
    }
    else if (!_cups_strcasecmp(token, "OF-TYPE"))
    {
      if (!get_token(fp, token, sizeof(token), &linenum))
      {
	print_fatal_error(data, "Missing OF-TYPE value tag(s) on line %d of \"%s\".",
			  linenum);
	pass = 0;
	goto test_exit;
      }

      if (last_expect)
	last_expect->of_type = strdup(token);
      else
      {
	print_fatal_error(data, "OF-TYPE without a preceding EXPECT on line %d of \"%s\".",
			  linenum);
	pass = 0;
	goto test_exit;
      }
    }
    else if (!_cups_strcasecmp(token, "IN-GROUP"))
    {
      ipp_tag_t	in_group;	/* IN-GROUP value */


      if (!get_token(fp, token, sizeof(token), &linenum))
      {
	print_fatal_error(data, "Missing IN-GROUP group tag on line %d of \"%s\".", f->linenum, f->filename);
	pass = 0;
	goto test_exit;
      }

      if ((in_group = ippTagValue(token)) == IPP_TAG_ZERO || in_group >= IPP_TAG_UNSUPPORTED_VALUE)
      {
	print_fatal_error(data, "Bad IN-GROUP group tag \"%s\" on line %d of \"%s\".", token, f->linenum, f->filename);
	pass = 0;
	goto test_exit;
      }
      else if (last_expect)
	last_expect->in_group = in_group;
      else
      {
	print_fatal_error(data, "IN-GROUP without a preceding EXPECT on line %d of \"%s\".", f->linenum, f->filename);
	pass = 0;
	goto test_exit;
      }
    }
    else if (!_cups_strcasecmp(token, "REPEAT-LIMIT"))
    {
      if (!get_token(fp, token, sizeof(token), &linenum))
      {
	print_fatal_error(data, "Missing REPEAT-LIMIT value on line %d of \"%s\".", f->linenum, f->filename);
	pass = 0;
	goto test_exit;
      }
      else if (atoi(token) <= 0)
      {
	print_fatal_error(data, "Bad REPEAT-LIMIT value on line %d of \"%s\".", f->linenum, f->filename);
	pass = 0;
	goto test_exit;
      }

      if (last_status)
	last_status->repeat_limit = atoi(token);
      else if (last_expect)
	last_expect->repeat_limit = atoi(token);
      else
      {
	print_fatal_error(data, "REPEAT-LIMIT without a preceding EXPECT or STATUS on line %d of \"%s\".", f->linenum, f->filename);
	pass = 0;
	goto test_exit;
      }
    }
    else if (!_cups_strcasecmp(token, "REPEAT-MATCH"))
    {
      if (last_status)
	last_status->repeat_match = 1;
      else if (last_expect)
	last_expect->repeat_match = 1;
      else
      {
	print_fatal_error(data, "REPEAT-MATCH without a preceding EXPECT or STATUS on line %d of \"%s\".", f->linenum, f->filename);
	pass = 0;
	goto test_exit;
      }
    }
    else if (!_cups_strcasecmp(token, "REPEAT-NO-MATCH"))
    {
      if (last_status)
	last_status->repeat_no_match = 1;
      else if (last_expect)
	last_expect->repeat_no_match = 1;
      else
      {
	print_fatal_error(data, "REPEAT-NO-MATCH without a preceding EXPECT or STATUS on ine %d.", f->linenum, f->filename);
	pass = 0;
	goto test_exit;
      }
    }
    else if (!_cups_strcasecmp(token, "SAME-COUNT-AS"))
    {
      if (!get_token(fp, token, sizeof(token), &linenum))
      {
	print_fatal_error(data, "Missing SAME-COUNT-AS name on line %d of \"%s\".", f->linenum, f->filename);
	pass = 0;
	goto test_exit;
      }

      if (last_expect)
	last_expect->same_count_as = strdup(token);
      else
      {
	print_fatal_error(data, "SAME-COUNT-AS without a preceding EXPECT on line "
			  "%d.", f->linenum, f->filename);
	pass = 0;
	goto test_exit;
      }
    }
    else if (!_cups_strcasecmp(token, "IF-DEFINED"))
    {
      if (!get_token(fp, token, sizeof(token), &linenum))
      {
	print_fatal_error(data, "Missing IF-DEFINED name on line %d of \"%s\".", f->linenum, f->filename);
	pass = 0;
	goto test_exit;
      }

      if (last_expect)
	last_expect->if_defined = strdup(token);
      else if (last_status)
	last_status->if_defined = strdup(token);
      else
      {
	print_fatal_error(data, "IF-DEFINED without a preceding EXPECT or STATUS on line %d of \"%s\".", f->linenum, f->filename);
	pass = 0;
	goto test_exit;
      }
    }
    else if (!_cups_strcasecmp(token, "IF-NOT-DEFINED"))
    {
      if (!get_token(fp, token, sizeof(token), &linenum))
      {
	print_fatal_error(data, "Missing IF-NOT-DEFINED name on line %d of \"%s\".", f->linenum, f->filename);
	pass = 0;
	goto test_exit;
      }

      if (last_expect)
	last_expect->if_not_defined = strdup(token);
      else if (last_status)
	last_status->if_not_defined = strdup(token);
      else
      {
	print_fatal_error(data, "IF-NOT-DEFINED without a preceding EXPECT or STATUS on line %d of \"%s\".", f->linenum, f->filename);
	pass = 0;
	goto test_exit;
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
      if (last_expect)
      {
	if (!_cups_strcasecmp(token, "WITH-ALL-HOSTNAMES") ||
	    !_cups_strcasecmp(token, "WITH-HOSTNAME"))
	  last_expect->with_flags = _CUPS_WITH_HOSTNAME;
	else if (!_cups_strcasecmp(token, "WITH-ALL-RESOURCES") ||
	    !_cups_strcasecmp(token, "WITH-RESOURCE"))
	  last_expect->with_flags = _CUPS_WITH_RESOURCE;
	else if (!_cups_strcasecmp(token, "WITH-ALL-SCHEMES") ||
	    !_cups_strcasecmp(token, "WITH-SCHEME"))
	  last_expect->with_flags = _CUPS_WITH_SCHEME;

	if (!_cups_strncasecmp(token, "WITH-ALL-", 9))
	  last_expect->with_flags |= _CUPS_WITH_ALL;
      }

      if (!_ippFileReadToken(f, temp, sizeof(temp)))
      {
	print_fatal_error(data, "Missing %s value on line %d of \"%s\".", token, f->linenum, f->filename);
	pass = 0;
	goto test_exit;
      }

      if (last_expect)
      {
       /*
	* Expand any variables in the value and then save it.
	*/

	_ippVarsExpand(vars, token, temp, sizeof(token));

	tokenptr = token + strlen(token) - 1;

	if (token[0] == '/' && tokenptr > token && *tokenptr == '/')
	{
	 /*
	  * WITH-VALUE is a POSIX extended regular expression.
	  */

	  last_expect->with_value = calloc(1, (size_t)(tokenptr - token));
	  last_expect->with_flags |= _CUPS_WITH_REGEX;

	  if (last_expect->with_value)
	    memcpy(last_expect->with_value, token + 1, (size_t)(tokenptr - token - 1));
	}
	else
	{
	 /*
	  * WITH-VALUE is a literal value...
	  */

	  char *ptr;			/* Pointer into value */

	  for (ptr = token; *ptr; ptr ++)
	  {
	    if (*ptr == '\\' && ptr[1])
	    {
	     /*
	      * Remove \ from \foo...
	      */

	      _cups_strcpy(ptr, ptr + 1);
	    }
	  }

	  last_expect->with_value = strdup(token);
	  last_expect->with_flags |= _CUPS_WITH_LITERAL;
	}
      }
      else
      {
	print_fatal_error(data, "%s without a preceding EXPECT on line %d of \"%s\".", token, f->linenum, f->filename);
	pass = 0;
	goto test_exit;
      }
    }
    else if (!_cups_strcasecmp(token, "WITH-VALUE-FROM"))
    {
      if (!_ippFileReadToken(f, temp, sizeof(temp)))
      {
	print_fatal_error(data, "Missing %s value on line %d of \"%s\".", token, f->linenum, f->filename);
	pass = 0;
	goto test_exit;
      }

      if (last_expect)
      {
       /*
	* Expand any variables in the value and then save it.
	*/

	_ippVarsExpand(vars, token, temp, sizeof(token));

	last_expect->with_value_from = strdup(token);
	last_expect->with_flags      = _CUPS_WITH_LITERAL;
      }
      else
      {
	print_fatal_error(data, "%s without a preceding EXPECT on line %d of \"%s\".", token, f->linenum, f->filename);
	pass = 0;
	goto test_exit;
      }
    }
    else if (!_cups_strcasecmp(token, "DISPLAY"))
    {
     /*
      * Display attributes...
      */

      if (num_displayed >= (int)(sizeof(displayed) / sizeof(displayed[0])))
      {
	print_fatal_error(data, "Too many DISPLAY's on line %d", f->linenum, f->filename);
	pass = 0;
	goto test_exit;
      }

      if (!get_token(fp, token, sizeof(token), &linenum))
      {
	print_fatal_error(data, "Missing DISPLAY name on line %d of \"%s\".", f->linenum, f->filename);
	pass = 0;
	goto test_exit;
      }

      displayed[num_displayed] = strdup(token);
      num_displayed ++;
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

      char *ptr;			/* Pointer into string */

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
      data->num_expect     = 0;
      data->last_expect    = NULL;
      data->filename[0]    = '\0';
      data->ignore_errors  = data->def_ignore_errors;
      strlcpy(data->name, f->filename, sizeof(data->name));
      if ((ptr = strrchr(data->name, '.')) != NULL)
        *ptr = '\0';
      data->repeat_count    = 0;
      data->repeat_interval = 5000000;
      data->request_id ++;
      strlcpy(data->resource, vars->resource, sizeof(data->resource));
      data->skip_previous = 0;
      data->skip_test     = 0;
      data->num_status    = 0;
      data->last_status   = NULL;
      data->test_id[0]    = '\0';
      data->transfer      = data->def_transfer;
      data->version       = data->def_version;

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

        char	filename[1024],		/* Mapped filename */
        	save_file_id[1024];	/* Saved FILE-ID value */

        strlcpy(save_file_id, data->file_id, sizeof(save_file_id));
        data->file_id[0] = '\0';

        if (!do_tests(data, vars, get_filename(f->filename, filename, temp, sizeof(filename))) && data->stop_after_include_error)
          return (0);

        strlcpy(data->file_id, save_file_id, sizeof(data->file_id));
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

        char	filename[1024],		/* Mapped filename */
        	save_file_id[1024];	/* Saved FILE-ID value */

        strlcpy(save_file_id, data->file_id, sizeof(save_file_id));
        data->file_id[0] = '\0';

        if (_ippVarsGet(vars, name) && !do_tests(data, vars, get_filename(f->filename, filename, temp, sizeof(filename))) && data->stop_after_include_error)
          return (0);

        strlcpy(data->file_id, save_file_id, sizeof(data->file_id));
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

        char	filename[1024],		/* Mapped filename */
        	save_file_id[1024];	/* Saved FILE-ID value */

        strlcpy(save_file_id, data->file_id, sizeof(save_file_id));
        data->file_id[0] = '\0';

        if (!_ippVarsGet(vars, name) && !do_tests(data, vars, get_filename(f->filename, filename, temp, sizeof(filename))) && data->stop_after_include_error)
          return (0);

        strlcpy(data->file_id, save_file_id, sizeof(data->file_id));
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
  _cupsLangPuts(stderr, _("  --help                  Show help."));
  _cupsLangPuts(stderr, _("  --ippserver filename    Produce ippserver attribute file."));
  _cupsLangPuts(stderr, _("  --stop-after-include-error\n"
                          "                          Stop tests after a failed INCLUDE."));
  _cupsLangPuts(stderr, _("  --version               Show version."));
  _cupsLangPuts(stderr, _("  -4                      Connect using IPv4."));
  _cupsLangPuts(stderr, _("  -6                      Connect using IPv6."));
  _cupsLangPuts(stderr, _("  -C                      Send requests using "
                          "chunking (default)."));
  _cupsLangPuts(stderr, _("  -E                      Test with encryption using HTTP Upgrade to TLS."));
  _cupsLangPuts(stderr, _("  -I                      Ignore errors."));
  _cupsLangPuts(stderr, _("  -L                      Send requests using content-length."));
  _cupsLangPuts(stderr, _("  -P filename.plist       Produce XML plist to a file and test report to standard output."));
  _cupsLangPuts(stderr, _("  -S                      Test with encryption using HTTPS."));
  _cupsLangPuts(stderr, _("  -T seconds              Set the receive/send timeout in seconds."));
  _cupsLangPuts(stderr, _("  -V version              Set default IPP version."));
  _cupsLangPuts(stderr, _("  -X                      Produce XML plist instead of plain text."));
  _cupsLangPuts(stderr, _("  -c                      Produce CSV output."));
  _cupsLangPuts(stderr, _("  -d name=value           Set named variable to value."));
  _cupsLangPuts(stderr, _("  -f filename             Set default request filename."));
  _cupsLangPuts(stderr, _("  -h                      Validate HTTP response headers."));
  _cupsLangPuts(stderr, _("  -i seconds              Repeat the last file with the given time interval."));
  _cupsLangPuts(stderr, _("  -l                      Produce plain text output."));
  _cupsLangPuts(stderr, _("  -n count                Repeat the last file the given number of times."));
  _cupsLangPuts(stderr, _("  -q                      Run silently."));
  _cupsLangPuts(stderr, _("  -t                      Produce a test report."));
  _cupsLangPuts(stderr, _("  -v                      Be verbose."));

  exit(1);
}


/*
 * 'validate_attr()' - Determine whether an attribute is valid.
 */

static int				/* O - 1 if valid, 0 otherwise */
validate_attr(cups_file_t     *outfile,	/* I - Output file */
              cups_array_t    *errors,	/* I - Errors array */
              ipp_attribute_t *attr)	/* I - Attribute to validate */
{
  int		i;			/* Looping var */
  char		scheme[64],		/* Scheme from URI */
		userpass[256],		/* Username/password from URI */
		hostname[256],		/* Hostname from URI */
		resource[1024];		/* Resource from URI */
  int		port,			/* Port number from URI */
		uri_status,		/* URI separation status */
		valid = 1;		/* Is the attribute valid? */
  const char	*ptr;			/* Pointer into string */
  ipp_attribute_t *colattr;		/* Collection attribute */
  regex_t	re;			/* Regular expression */
  ipp_uchar_t	*date;			/* Current date value */


 /*
  * Skip separators.
  */

  if (!attr->name)
    return (1);

 /*
  * Validate the attribute name.
  */

  for (ptr = attr->name; *ptr; ptr ++)
    if (!isalnum(*ptr & 255) && *ptr != '-' && *ptr != '.' && *ptr != '_')
      break;

  if (*ptr || ptr == attr->name)
  {
    valid = 0;

    add_stringf(errors,
		"\"%s\": Bad attribute name - invalid character "
		"(RFC 2911 section 4.1.3).", attr->name);
  }

  if ((ptr - attr->name) > 255)
  {
    valid = 0;

    add_stringf(errors,
		"\"%s\": Bad attribute name - bad length "
		"(RFC 2911 section 4.1.3).", attr->name);
  }

  switch (attr->value_tag)
  {
    case IPP_TAG_INTEGER :
        break;

    case IPP_TAG_BOOLEAN :
        for (i = 0; i < attr->num_values; i ++)
	{
	  if (attr->values[i].boolean != 0 &&
	      attr->values[i].boolean != 1)
	  {
	    valid = 0;

	    add_stringf(errors,
			"\"%s\": Bad boolen value %d "
			"(RFC 2911 section 4.1.11).", attr->name,
			attr->values[i].boolean);
	  }
	}
        break;

    case IPP_TAG_ENUM :
        for (i = 0; i < attr->num_values; i ++)
	{
	  if (attr->values[i].integer < 1)
	  {
	    valid = 0;

	    add_stringf(errors,
			"\"%s\": Bad enum value %d - out of range "
			"(RFC 2911 section 4.1.4).", attr->name,
			attr->values[i].integer);
	  }
	}
        break;

    case IPP_TAG_STRING :
        for (i = 0; i < attr->num_values; i ++)
	{
	  if (attr->values[i].unknown.length > IPP_MAX_OCTETSTRING)
	  {
	    valid = 0;

	    add_stringf(errors,
			"\"%s\": Bad octetString value - bad length %d "
			"(RFC 2911 section 4.1.10).", attr->name,
			attr->values[i].unknown.length);
	  }
	}
        break;

    case IPP_TAG_DATE :
        for (i = 0; i < attr->num_values; i ++)
	{
	  date = attr->values[i].date;

          if (date[2] < 1 || date[2] > 12)
	  {
	    valid = 0;

	    add_stringf(errors,
			"\"%s\": Bad dateTime month %u "
			"(RFC 2911 section 4.1.14).", attr->name, date[2]);
	  }

          if (date[3] < 1 || date[3] > 31)
	  {
	    valid = 0;

	    add_stringf(errors,
			"\"%s\": Bad dateTime day %u "
			"(RFC 2911 section 4.1.14).", attr->name, date[3]);
	  }

          if (date[4] > 23)
	  {
	    valid = 0;

	    add_stringf(errors,
			"\"%s\": Bad dateTime hours %u "
			"(RFC 2911 section 4.1.14).", attr->name, date[4]);
	  }

          if (date[5] > 59)
	  {
	    valid = 0;

	    add_stringf(errors,
			"\"%s\": Bad dateTime minutes %u "
			"(RFC 2911 section 4.1.14).", attr->name, date[5]);
	  }

          if (date[6] > 60)
	  {
	    valid = 0;

	    add_stringf(errors,
			"\"%s\": Bad dateTime seconds %u "
			"(RFC 2911 section 4.1.14).", attr->name, date[6]);
	  }

          if (date[7] > 9)
	  {
	    valid = 0;

	    add_stringf(errors,
			"\"%s\": Bad dateTime deciseconds %u "
			"(RFC 2911 section 4.1.14).", attr->name, date[7]);
	  }

          if (date[8] != '-' && date[8] != '+')
	  {
	    valid = 0;

	    add_stringf(errors,
			"\"%s\": Bad dateTime UTC sign '%c' "
			"(RFC 2911 section 4.1.14).", attr->name, date[8]);
	  }

          if (date[9] > 11)
	  {
	    valid = 0;

	    add_stringf(errors,
			"\"%s\": Bad dateTime UTC hours %u "
			"(RFC 2911 section 4.1.14).", attr->name, date[9]);
	  }

          if (date[10] > 59)
	  {
	    valid = 0;

	    add_stringf(errors,
			"\"%s\": Bad dateTime UTC minutes %u "
			"(RFC 2911 section 4.1.14).", attr->name, date[10]);
	  }
	}
        break;

    case IPP_TAG_RESOLUTION :
        for (i = 0; i < attr->num_values; i ++)
	{
	  if (attr->values[i].resolution.xres <= 0)
	  {
	    valid = 0;

	    add_stringf(errors,
			"\"%s\": Bad resolution value %dx%d%s - cross "
			"feed resolution must be positive "
			"(RFC 2911 section 4.1.15).", attr->name,
			attr->values[i].resolution.xres,
			attr->values[i].resolution.yres,
			attr->values[i].resolution.units ==
			    IPP_RES_PER_INCH ? "dpi" :
			    attr->values[i].resolution.units ==
				IPP_RES_PER_CM ? "dpcm" : "unknown");
	  }

	  if (attr->values[i].resolution.yres <= 0)
	  {
	    valid = 0;

	    add_stringf(errors,
			"\"%s\": Bad resolution value %dx%d%s - feed "
			"resolution must be positive "
			"(RFC 2911 section 4.1.15).", attr->name,
			attr->values[i].resolution.xres,
			attr->values[i].resolution.yres,
			attr->values[i].resolution.units ==
			    IPP_RES_PER_INCH ? "dpi" :
			    attr->values[i].resolution.units ==
				IPP_RES_PER_CM ? "dpcm" : "unknown");
	  }

	  if (attr->values[i].resolution.units != IPP_RES_PER_INCH &&
	      attr->values[i].resolution.units != IPP_RES_PER_CM)
	  {
	    valid = 0;

	    add_stringf(errors,
			"\"%s\": Bad resolution value %dx%d%s - bad "
			"units value (RFC 2911 section 4.1.15).",
			attr->name, attr->values[i].resolution.xres,
			attr->values[i].resolution.yres,
			attr->values[i].resolution.units ==
			    IPP_RES_PER_INCH ? "dpi" :
			    attr->values[i].resolution.units ==
				IPP_RES_PER_CM ? "dpcm" : "unknown");
	  }
	}
        break;

    case IPP_TAG_RANGE :
        for (i = 0; i < attr->num_values; i ++)
	{
	  if (attr->values[i].range.lower > attr->values[i].range.upper)
	  {
	    valid = 0;

	    add_stringf(errors,
			"\"%s\": Bad rangeOfInteger value %d-%d - lower "
			"greater than upper (RFC 2911 section 4.1.13).",
			attr->name, attr->values[i].range.lower,
			attr->values[i].range.upper);
	  }
	}
        break;

    case IPP_TAG_BEGIN_COLLECTION :
        for (i = 0; i < attr->num_values; i ++)
	{
	  for (colattr = attr->values[i].collection->attrs;
	       colattr;
	       colattr = colattr->next)
	  {
	    if (!validate_attr(data->outfile, NULL, colattr))
	    {
	      valid = 0;
	      break;
	    }
	  }

	  if (colattr && errors)
	  {
            add_stringf(errors, "\"%s\": Bad collection value.", attr->name);

	    while (colattr)
	    {
	      validate_attr(data->outfile, errors, colattr);
	      colattr = colattr->next;
	    }
	  }
	}
        break;

    case IPP_TAG_TEXT :
    case IPP_TAG_TEXTLANG :
        for (i = 0; i < attr->num_values; i ++)
	{
	  for (ptr = attr->values[i].string.text; *ptr; ptr ++)
	  {
	    if ((*ptr & 0xe0) == 0xc0)
	    {
	      ptr ++;
	      if ((*ptr & 0xc0) != 0x80)
	        break;
	    }
	    else if ((*ptr & 0xf0) == 0xe0)
	    {
	      ptr ++;
	      if ((*ptr & 0xc0) != 0x80)
	        break;
	      ptr ++;
	      if ((*ptr & 0xc0) != 0x80)
	        break;
	    }
	    else if ((*ptr & 0xf8) == 0xf0)
	    {
	      ptr ++;
	      if ((*ptr & 0xc0) != 0x80)
	        break;
	      ptr ++;
	      if ((*ptr & 0xc0) != 0x80)
	        break;
	      ptr ++;
	      if ((*ptr & 0xc0) != 0x80)
	        break;
	    }
	    else if (*ptr & 0x80)
	      break;
	  }

	  if (*ptr)
	  {
	    valid = 0;

	    add_stringf(errors,
			"\"%s\": Bad text value \"%s\" - bad UTF-8 "
			"sequence (RFC 2911 section 4.1.1).", attr->name,
			attr->values[i].string.text);
	  }

	  if ((ptr - attr->values[i].string.text) > (IPP_MAX_TEXT - 1))
	  {
	    valid = 0;

	    add_stringf(errors,
			"\"%s\": Bad text value \"%s\" - bad length %d "
			"(RFC 2911 section 4.1.1).", attr->name,
			attr->values[i].string.text,
			(int)strlen(attr->values[i].string.text));
	  }
	}
        break;

    case IPP_TAG_NAME :
    case IPP_TAG_NAMELANG :
        for (i = 0; i < attr->num_values; i ++)
	{
	  for (ptr = attr->values[i].string.text; *ptr; ptr ++)
	  {
	    if ((*ptr & 0xe0) == 0xc0)
	    {
	      ptr ++;
	      if ((*ptr & 0xc0) != 0x80)
	        break;
	    }
	    else if ((*ptr & 0xf0) == 0xe0)
	    {
	      ptr ++;
	      if ((*ptr & 0xc0) != 0x80)
	        break;
	      ptr ++;
	      if ((*ptr & 0xc0) != 0x80)
	        break;
	    }
	    else if ((*ptr & 0xf8) == 0xf0)
	    {
	      ptr ++;
	      if ((*ptr & 0xc0) != 0x80)
	        break;
	      ptr ++;
	      if ((*ptr & 0xc0) != 0x80)
	        break;
	      ptr ++;
	      if ((*ptr & 0xc0) != 0x80)
	        break;
	    }
	    else if (*ptr & 0x80)
	      break;
	  }

	  if (*ptr)
	  {
	    valid = 0;

	    add_stringf(errors,
			"\"%s\": Bad name value \"%s\" - bad UTF-8 "
			"sequence (RFC 2911 section 4.1.2).", attr->name,
			attr->values[i].string.text);
	  }

	  if ((ptr - attr->values[i].string.text) > (IPP_MAX_NAME - 1))
	  {
	    valid = 0;

	    add_stringf(errors,
			"\"%s\": Bad name value \"%s\" - bad length %d "
			"(RFC 2911 section 4.1.2).", attr->name,
			attr->values[i].string.text,
			(int)strlen(attr->values[i].string.text));
	  }
	}
        break;

    case IPP_TAG_KEYWORD :
        for (i = 0; i < attr->num_values; i ++)
	{
	  for (ptr = attr->values[i].string.text; *ptr; ptr ++)
	    if (!isalnum(*ptr & 255) && *ptr != '-' && *ptr != '.' &&
	        *ptr != '_')
	      break;

	  if (*ptr || ptr == attr->values[i].string.text)
	  {
	    valid = 0;

	    add_stringf(errors,
			"\"%s\": Bad keyword value \"%s\" - invalid "
			"character (RFC 2911 section 4.1.3).",
			attr->name, attr->values[i].string.text);
	  }

	  if ((ptr - attr->values[i].string.text) > (IPP_MAX_KEYWORD - 1))
	  {
	    valid = 0;

	    add_stringf(errors,
			"\"%s\": Bad keyword value \"%s\" - bad "
			"length %d (RFC 2911 section 4.1.3).",
			attr->name, attr->values[i].string.text,
			(int)strlen(attr->values[i].string.text));
	  }
	}
        break;

    case IPP_TAG_URI :
        for (i = 0; i < attr->num_values; i ++)
	{
	  uri_status = httpSeparateURI(HTTP_URI_CODING_ALL,
	                               attr->values[i].string.text,
				       scheme, sizeof(scheme),
				       userpass, sizeof(userpass),
				       hostname, sizeof(hostname),
				       &port, resource, sizeof(resource));

	  if (uri_status < HTTP_URI_OK)
	  {
	    valid = 0;

	    add_stringf(errors,
			"\"%s\": Bad URI value \"%s\" - %s "
			"(RFC 2911 section 4.1.5).", attr->name,
			attr->values[i].string.text,
			httpURIStatusString(uri_status));
	  }

	  if (strlen(attr->values[i].string.text) > (IPP_MAX_URI - 1))
	  {
	    valid = 0;

	    add_stringf(errors,
			"\"%s\": Bad URI value \"%s\" - bad length %d "
			"(RFC 2911 section 4.1.5).", attr->name,
			attr->values[i].string.text,
			(int)strlen(attr->values[i].string.text));
	  }
	}
        break;

    case IPP_TAG_URISCHEME :
        for (i = 0; i < attr->num_values; i ++)
	{
	  ptr = attr->values[i].string.text;
	  if (islower(*ptr & 255))
	  {
	    for (ptr ++; *ptr; ptr ++)
	      if (!islower(*ptr & 255) && !isdigit(*ptr & 255) &&
	          *ptr != '+' && *ptr != '-' && *ptr != '.')
                break;
	  }

	  if (*ptr || ptr == attr->values[i].string.text)
	  {
	    valid = 0;

	    add_stringf(errors,
			"\"%s\": Bad uriScheme value \"%s\" - bad "
			"characters (RFC 2911 section 4.1.6).",
			attr->name, attr->values[i].string.text);
	  }

	  if ((ptr - attr->values[i].string.text) > (IPP_MAX_URISCHEME - 1))
	  {
	    valid = 0;

	    add_stringf(errors,
			"\"%s\": Bad uriScheme value \"%s\" - bad "
			"length %d (RFC 2911 section 4.1.6).",
			attr->name, attr->values[i].string.text,
			(int)strlen(attr->values[i].string.text));
	  }
	}
        break;

    case IPP_TAG_CHARSET :
        for (i = 0; i < attr->num_values; i ++)
	{
	  for (ptr = attr->values[i].string.text; *ptr; ptr ++)
	    if (!isprint(*ptr & 255) || isupper(*ptr & 255) ||
	        isspace(*ptr & 255))
	      break;

	  if (*ptr || ptr == attr->values[i].string.text)
	  {
	    valid = 0;

	    add_stringf(errors,
			"\"%s\": Bad charset value \"%s\" - bad "
			"characters (RFC 2911 section 4.1.7).",
			attr->name, attr->values[i].string.text);
	  }

	  if ((ptr - attr->values[i].string.text) > (IPP_MAX_CHARSET - 1))
	  {
	    valid = 0;

	    add_stringf(errors,
			"\"%s\": Bad charset value \"%s\" - bad "
			"length %d (RFC 2911 section 4.1.7).",
			attr->name, attr->values[i].string.text,
			(int)strlen(attr->values[i].string.text));
	  }
	}
        break;

    case IPP_TAG_LANGUAGE :
       /*
        * The following regular expression is derived from the ABNF for
	* language tags in RFC 4646.  All I can say is that this is the
	* easiest way to check the values...
	*/

        if ((i = regcomp(&re,
			 "^("
			 "(([a-z]{2,3}(-[a-z][a-z][a-z]){0,3})|[a-z]{4,8})"
								/* language */
			 "(-[a-z][a-z][a-z][a-z]){0,1}"		/* script */
			 "(-([a-z][a-z]|[0-9][0-9][0-9])){0,1}"	/* region */
			 "(-([a-z]{5,8}|[0-9][0-9][0-9]))*"	/* variant */
			 "(-[a-wy-z](-[a-z0-9]{2,8})+)*"	/* extension */
			 "(-x(-[a-z0-9]{1,8})+)*"		/* privateuse */
			 "|"
			 "x(-[a-z0-9]{1,8})+"			/* privateuse */
			 "|"
			 "[a-z]{1,3}(-[a-z][0-9]{2,8}){1,2}"	/* grandfathered */
			 ")$",
			 REG_NOSUB | REG_EXTENDED)) != 0)
        {
          char	temp[256];		/* Temporary error string */

          regerror(i, &re, temp, sizeof(temp));
	  print_fatal_error(data, "Unable to compile naturalLanguage regular "
	                    "expression: %s.", temp);
	  break;
        }

        for (i = 0; i < attr->num_values; i ++)
	{
	  if (regexec(&re, attr->values[i].string.text, 0, NULL, 0))
	  {
	    valid = 0;

	    add_stringf(errors,
			"\"%s\": Bad naturalLanguage value \"%s\" - bad "
			"characters (RFC 2911 section 4.1.8).",
			attr->name, attr->values[i].string.text);
	  }

	  if (strlen(attr->values[i].string.text) > (IPP_MAX_LANGUAGE - 1))
	  {
	    valid = 0;

	    add_stringf(errors,
			"\"%s\": Bad naturalLanguage value \"%s\" - bad "
			"length %d (RFC 2911 section 4.1.8).",
			attr->name, attr->values[i].string.text,
			(int)strlen(attr->values[i].string.text));
	  }
	}

	regfree(&re);
        break;

    case IPP_TAG_MIMETYPE :
       /*
        * The following regular expression is derived from the ABNF for
	* language tags in RFC 2045 and 4288.  All I can say is that this is
	* the easiest way to check the values...
	*/

        if ((i = regcomp(&re,
			 "^"
			 "[-a-zA-Z0-9!#$&.+^_]{1,127}"		/* type-name */
			 "/"
			 "[-a-zA-Z0-9!#$&.+^_]{1,127}"		/* subtype-name */
			 "(;[-a-zA-Z0-9!#$&.+^_]{1,127}="	/* parameter= */
			 "([-a-zA-Z0-9!#$&.+^_]{1,127}|\"[^\"]*\"))*"
			 					/* value */
			 "$",
			 REG_NOSUB | REG_EXTENDED)) != 0)
        {
          char	temp[256];		/* Temporary error string */

          regerror(i, &re, temp, sizeof(temp));
	  print_fatal_error(data, "Unable to compile mimeMediaType regular "
	                    "expression: %s.", temp);
	  break;
        }

        for (i = 0; i < attr->num_values; i ++)
	{
	  if (regexec(&re, attr->values[i].string.text, 0, NULL, 0))
	  {
	    valid = 0;

	    add_stringf(errors,
			"\"%s\": Bad mimeMediaType value \"%s\" - bad "
			"characters (RFC 2911 section 4.1.9).",
			attr->name, attr->values[i].string.text);
	  }

	  if (strlen(attr->values[i].string.text) > (IPP_MAX_MIMETYPE - 1))
	  {
	    valid = 0;

	    add_stringf(errors,
			"\"%s\": Bad mimeMediaType value \"%s\" - bad "
			"length %d (RFC 2911 section 4.1.9).",
			attr->name, attr->values[i].string.text,
			(int)strlen(attr->values[i].string.text));
	  }
	}

	regfree(&re);
        break;

    default :
        break;
  }

  return (valid);
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
with_value(cups_file_t     *outfile,	/* I - Output file */
           cups_array_t    *errors,	/* I - Errors array */
           char            *value,	/* I - Value string */
           int             flags,	/* I - Flags for match */
           ipp_attribute_t *attr,	/* I - Attribute to compare */
	   char            *matchbuf,	/* I - Buffer to hold matching value */
	   size_t          matchlen)	/* I - Length of match buffer */
{
  int	i,				/* Looping var */
	match;				/* Match? */
  char	temp[1024],			/* Temporary value string */
	*valptr;			/* Pointer into value */


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

  switch (attr->value_tag)
  {
    case IPP_TAG_INTEGER :
    case IPP_TAG_ENUM :
        for (i = 0; i < attr->num_values; i ++)
        {
	  char	op,			/* Comparison operator */
	  	*nextptr;		/* Next pointer */
	  int	intvalue,		/* Integer value */
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

            if ((op == '=' && attr->values[i].integer == intvalue) ||
                (op == '<' && attr->values[i].integer < intvalue) ||
                (op == '>' && attr->values[i].integer > intvalue))
	    {
	      if (!matchbuf[0])
		snprintf(matchbuf, matchlen, "%d", attr->values[i].integer);

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
	  for (i = 0; i < attr->num_values; i ++)
	    add_stringf(errors, "GOT: %s=%d", attr->name,
	                attr->values[i].integer);
	}
	break;

    case IPP_TAG_RANGE :
        for (i = 0; i < attr->num_values; i ++)
        {
	  char	op,			/* Comparison operator */
	  	*nextptr;		/* Next pointer */
	  int	intvalue,		/* Integer value */
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

            if ((op == '=' && (attr->values[i].range.lower == intvalue ||
			       attr->values[i].range.upper == intvalue)) ||
		(op == '<' && attr->values[i].range.upper < intvalue) ||
		(op == '>' && attr->values[i].range.upper > intvalue))
	    {
	      if (!matchbuf[0])
		snprintf(matchbuf, matchlen, "%d-%d",
			 attr->values[0].range.lower,
			 attr->values[0].range.upper);

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
	  for (i = 0; i < attr->num_values; i ++)
	    add_stringf(errors, "GOT: %s=%d-%d", attr->name,
	                     attr->values[i].range.lower,
	                     attr->values[i].range.upper);
	}
	break;

    case IPP_TAG_BOOLEAN :
	for (i = 0; i < attr->num_values; i ++)
	{
          if ((!strcmp(value, "true")) == attr->values[i].boolean)
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
	  for (i = 0; i < attr->num_values; i ++)
	    add_stringf(errors, "GOT: %s=%s", attr->name,
			     attr->values[i].boolean ? "true" : "false");
	}
	break;

    case IPP_TAG_RESOLUTION :
	for (i = 0; i < attr->num_values; i ++)
	{
	  if (attr->values[i].resolution.xres ==
	          attr->values[i].resolution.yres)
	    snprintf(temp, sizeof(temp), "%d%s",
	             attr->values[i].resolution.xres,
	             attr->values[i].resolution.units == IPP_RES_PER_INCH ?
	                 "dpi" : "dpcm");
	  else
	    snprintf(temp, sizeof(temp), "%dx%d%s",
	             attr->values[i].resolution.xres,
	             attr->values[i].resolution.yres,
	             attr->values[i].resolution.units == IPP_RES_PER_INCH ?
	                 "dpi" : "dpcm");

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
	  for (i = 0; i < attr->num_values; i ++)
	  {
	    if (attr->values[i].resolution.xres ==
		    attr->values[i].resolution.yres)
	      snprintf(temp, sizeof(temp), "%d%s",
		       attr->values[i].resolution.xres,
		       attr->values[i].resolution.units == IPP_RES_PER_INCH ?
			   "dpi" : "dpcm");
	    else
	      snprintf(temp, sizeof(temp), "%dx%d%s",
		       attr->values[i].resolution.xres,
		       attr->values[i].resolution.yres,
		       attr->values[i].resolution.units == IPP_RES_PER_INCH ?
			   "dpi" : "dpcm");

            if (strcmp(value, temp))
	      add_stringf(errors, "GOT: %s=%s", attr->name, temp);
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

	    print_fatal_error(data, "Unable to compile WITH-VALUE regular expression "
	                      "\"%s\" - %s", value, temp);
	    return (0);
	  }

         /*
	  * See if ALL of the values match the given regular expression.
	  */

	  for (i = 0; i < attr->num_values; i ++)
	  {
	    if (!regexec(&re, get_string(attr, i, flags, temp, sizeof(temp)),
	                 0, NULL, 0))
	    {
	      if (!matchbuf[0])
		strlcpy(matchbuf,
		        get_string(attr, i, flags, temp, sizeof(temp)),
		        matchlen);

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

	  for (i = 0; i < attr->num_values; i ++)
	  {
	    if (!compare_uris(value, get_string(attr, i, flags, temp, sizeof(temp))))
	    {
	      if (!matchbuf[0])
		strlcpy(matchbuf,
		        get_string(attr, i, flags, temp, sizeof(temp)),
		        matchlen);

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

	  for (i = 0; i < attr->num_values; i ++)
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
		strlcpy(matchbuf,
		        get_string(attr, i, flags, temp, sizeof(temp)),
		        matchlen);

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
	  for (i = 0; i < attr->num_values; i ++)
	    add_stringf(errors, "GOT: %s=\"%s\"", attr->name,
			attr->values[i].string.text);
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
