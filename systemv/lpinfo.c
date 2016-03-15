/*
 * "$Id: lpinfo.c 10996 2013-05-29 11:51:34Z msweet $"
 *
 *   "lpinfo" command for CUPS.
 *
 *   Copyright 2007-2010 by Apple Inc.
 *   Copyright 1997-2006 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
 *   main()         - Parse options and show information.
 *   device_cb      - Device callback.
 *   show_devices() - Show available devices.
 *   show_models()  - Show available PPDs.
 */

/*
 * Include necessary headers...
 */

#include <cups/cups-private.h>


/*
 * Local functions...
 */

static void	device_cb(const char *device_clas, const char *device_id,
		          const char *device_info,
			  const char *device_make_and_model,
			  const char *device_uri, const char *device_location,
			  void *user_data);
static int	show_devices(int long_status, int timeout,
			     const char *include_schemes,
			     const char *exclude_schemes);
static int	show_models(int long_status,
			    const char *device_id, const char *language,
			    const char *make_model, const char *product,
			    const char *include_schemes,
			    const char *exclude_schemes);


/*
 * 'main()' - Parse options and show status information.
 */

int
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  int		i;			/* Looping var */
  int		long_status;		/* Long listing? */
  const char	*device_id,		/* 1284 device ID */
		*language,		/* Language */
		*make_model,		/* Make and model */
		*product,		/* Product */
		*include_schemes,	/* Schemes to include */
		*exclude_schemes;	/* Schemes to exclude */
  int		timeout;		/* Device timeout */


  _cupsSetLocale(argv);

  long_status     = 0;
  device_id       = NULL;
  language        = NULL;
  make_model      = NULL;
  product         = NULL;
  include_schemes = CUPS_INCLUDE_ALL;
  exclude_schemes = CUPS_EXCLUDE_NONE;
  timeout         = CUPS_TIMEOUT_DEFAULT;

  for (i = 1; i < argc; i ++)
    if (argv[i][0] == '-')
      switch (argv[i][1])
      {
        case 'E' : /* Encrypt */
#ifdef HAVE_SSL
	    cupsSetEncryption(HTTP_ENCRYPT_REQUIRED);
#else
            _cupsLangPrintf(stderr,
		            _("%s: Sorry, no encryption support."),
	                    argv[0]);
#endif /* HAVE_SSL */
	    break;

        case 'h' : /* Connect to host */
	    if (argv[i][2] != '\0')
	      cupsSetServer(argv[i] + 2);
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
	        _cupsLangPuts(stderr,
		              _("Error: need hostname after \"-h\" option."));
		return (1);
              }

	      cupsSetServer(argv[i]);
	    }
	    break;

        case 'l' : /* Show long listing */
	    long_status = 1;
	    break;

        case 'm' : /* Show models */
            if (show_models(long_status, device_id, language, make_model,
	                    product, include_schemes, exclude_schemes))
	      return (1);
	    break;
	    
        case 'v' : /* Show available devices */
            if (show_devices(long_status, timeout, include_schemes,
	                     exclude_schemes))
	      return (1);
	    break;

        case '-' : /* --something */
            if (!strcmp(argv[i], "--device-id"))
	    {
	      i ++;

	      if (i < argc)
		device_id = argv[i];
	      else
	      {
		_cupsLangPuts(stderr,
			      _("lpinfo: Expected 1284 device ID string "
				"after \"--device-id\"."));
		return (1);
	      }
	    }
	    else if (!strncmp(argv[i], "--device-id=", 12) && argv[i][12])
	    {
	      device_id = argv[i] + 12;
	    }
            else if (!strcmp(argv[i], "--exclude-schemes"))
	    {
	      i ++;

	      if (i < argc)
		exclude_schemes = argv[i];
	      else
	      {
		_cupsLangPuts(stderr,
			      _("lpinfo: Expected scheme list after "
				"\"--exclude-schemes\"."));
		return (1);
	      }
	    }
	    else if (!strncmp(argv[i], "--exclude-schemes=", 18) && argv[i][18])
	    {
	      exclude_schemes = argv[i] + 18;
	    }
            else if (!strcmp(argv[i], "--include-schemes"))
	    {
	      i ++;

	      if (i < argc)
		include_schemes = argv[i];
	      else
	      {
		_cupsLangPuts(stderr,
			      _("lpinfo: Expected scheme list after "
				"\"--include-schemes\"."));
		return (1);
	      }
	    }
	    else if (!strncmp(argv[i], "--include-schemes=", 18) && argv[i][18])
	    {
	      include_schemes = argv[i] + 18;
	    }
            else if (!strcmp(argv[i], "--language"))
	    {
	      i ++;
	      if (i < argc)
		language = argv[i];
	      else
	      {
		_cupsLangPuts(stderr,
			      _("lpinfo: Expected language after "
				"\"--language\"."));
		return (1);
	      }
	    }
	    else if (!strncmp(argv[i], "--language=", 11) && argv[i][11])
	    {
	      language = argv[i] + 11;
	    }
            else if (!strcmp(argv[i], "--make-and-model"))
	    {
	      i ++;
	      if (i < argc)
		make_model= argv[i];
	      else
	      {
		_cupsLangPuts(stderr,
			      _("lpinfo: Expected make and model after "
				"\"--make-and-model\"."));
		return (1);
	      }
	    }
	    else if (!strncmp(argv[i], "--make-and-model=", 17) && argv[i][17])
	    {
	      make_model = argv[i] + 17;
	    }
            else if (!strcmp(argv[i], "--product"))
	    {
	      i ++;
	      if (i < argc)
		product = argv[i];
	      else
	      {
		_cupsLangPuts(stderr,
			      _("lpinfo: Expected product string after "
				"\"--product\"."));
		return (1);
	      }
	    }
	    else if (!strncmp(argv[i], "--product=", 10) && argv[i][10])
	    {
	      product = argv[i] + 10;
	    }
            else if (!strcmp(argv[i], "--timeout"))
	    {
	      i ++;
	      if (i < argc)
		timeout = atoi(argv[i]);
	      else
	      {
		_cupsLangPuts(stderr,
			      _("lpinfo: Expected timeout after "
			        "\"--timeout\"."));
		return (1);
	      }
	    }
	    else if (!strncmp(argv[i], "--timeout=", 10) && argv[i][10])
	    {
	      timeout = atoi(argv[i] + 10);
	    }
	    else
	    {
	      _cupsLangPrintf(stderr, _("lpinfo: Unknown option \"%s\"."),
			      argv[i]);
	      return (1);
	    }
	    break;

	default :
	    _cupsLangPrintf(stderr, _("lpinfo: Unknown option \"%c\"."),
	                    argv[i][1]);
	    return (1);
      }
    else
    {
      _cupsLangPrintf(stderr, _("lpinfo: Unknown argument \"%s\"."),
                      argv[i]);
      return (1);
    }

  return (0);
}


/*
 * 'device_cb()' - Device callback.
 */

static void
device_cb(
    const char *device_class,		/* I - device-class string */
    const char *device_id,		/* I - device-id string */
    const char *device_info,		/* I - device-info string */
    const char *device_make_and_model,	/* I - device-make-and-model string */
    const char *device_uri,		/* I - device-uri string */
    const char *device_location,	/* I - device-location string */
    void       *user_data)		/* I - User data */
{
  int	*long_status;			/* Show verbose info? */


 /*
  * Display the device...
  */

  long_status = (int *)user_data;

  if (*long_status)
  {
    _cupsLangPrintf(stdout,
		    _("Device: uri = %s\n"
		      "        class = %s\n"
		      "        info = %s\n"
		      "        make-and-model = %s\n"
		      "        device-id = %s\n"
		      "        location = %s"),
		    device_uri, device_class, device_info,
		    device_make_and_model, device_id, device_location);
  }
  else
    _cupsLangPrintf(stdout, "%s %s", device_class, device_uri);
}


/*
 * 'show_devices()' - Show available devices.
 */

static int				/* O - 0 on success, 1 on failure */
show_devices(
    int        long_status,		/* I - Long status report? */
    int        timeout,			/* I - Timeout */
    const char *include_schemes,	/* I - List of schemes to include */
    const char *exclude_schemes)	/* I - List of schemes to exclude */
{
  if (cupsGetDevices(CUPS_HTTP_DEFAULT, timeout, include_schemes,
                     exclude_schemes, device_cb, &long_status) != IPP_OK)
  {
    _cupsLangPrintf(stderr, "lpinfo: %s", cupsLastErrorString());
    return (1);
  }

  return (0);
}


/*
 * 'show_models()' - Show available PPDs.
 */

static int				/* O - 0 on success, 1 on failure */
show_models(
    int        long_status,		/* I - Long status report? */
    const char *device_id,		/* I - 1284 device ID */
    const char *language,		/* I - Language */
    const char *make_model,		/* I - Make and model */
    const char *product,		/* I - Product */
    const char *include_schemes,	/* I - List of schemes to include */
    const char *exclude_schemes)	/* I - List of schemes to exclude */
{
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  ipp_attribute_t *attr;		/* Current attribute */
  const char	*ppd_device_id,		/* Pointer to ppd-device-id */
		*ppd_language,		/* Pointer to ppd-natural-language */
		*ppd_make_model,	/* Pointer to ppd-make-and-model */
		*ppd_name;		/* Pointer to ppd-name */
  cups_option_t	option;			/* in/exclude-schemes option */


 /*
  * Build a CUPS_GET_PPDS request...
  */

  request = ippNewRequest(CUPS_GET_PPDS);

  if (device_id)
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_TEXT, "ppd-device-id",
                 NULL, device_id);
  if (language)
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE, "ppd-language",
                 NULL, language);
  if (make_model)
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_TEXT, "ppd-make-and-model",
                 NULL, make_model);
  if (product)
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_TEXT, "ppd-product",
                 NULL, product);

  if (include_schemes)
  {
    option.name  = "include-schemes";
    option.value = (char *)include_schemes;

    cupsEncodeOptions2(request, 1, &option, IPP_TAG_OPERATION);
  }

  if (exclude_schemes)
  {
    option.name  = "exclude-schemes";
    option.value = (char *)exclude_schemes;

    cupsEncodeOptions2(request, 1, &option, IPP_TAG_OPERATION);
  }

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(CUPS_HTTP_DEFAULT, request, "/")) != NULL)
  {
   /*
    * Loop through the device list and display them...
    */

    if (response->request.status.status_code > IPP_OK_CONFLICT)
    {
      _cupsLangPrintf(stderr, "lpinfo: %s", cupsLastErrorString());
      ippDelete(response);
      return (1);
    }

    for (attr = response->attrs; attr != NULL; attr = attr->next)
    {
     /*
      * Skip leading attributes until we hit a PPD...
      */

      while (attr != NULL && attr->group_tag != IPP_TAG_PRINTER)
        attr = attr->next;

      if (attr == NULL)
        break;

     /*
      * Pull the needed attributes from this PPD...
      */

      ppd_device_id  = "NONE";
      ppd_language   = NULL;
      ppd_make_model = NULL;
      ppd_name       = NULL;

      while (attr != NULL && attr->group_tag == IPP_TAG_PRINTER)
      {
        if (!strcmp(attr->name, "ppd-device-id") &&
	    attr->value_tag == IPP_TAG_TEXT)
	  ppd_device_id = attr->values[0].string.text;
        else if (!strcmp(attr->name, "ppd-natural-language") &&
	         attr->value_tag == IPP_TAG_LANGUAGE)
	  ppd_language = attr->values[0].string.text;
        else if (!strcmp(attr->name, "ppd-make-and-model") &&
	         attr->value_tag == IPP_TAG_TEXT)
	  ppd_make_model = attr->values[0].string.text;
        else if (!strcmp(attr->name, "ppd-name") &&
	         attr->value_tag == IPP_TAG_NAME)
	  ppd_name = attr->values[0].string.text;

        attr = attr->next;
      }

     /*
      * See if we have everything needed...
      */

      if (ppd_language == NULL || ppd_make_model == NULL || ppd_name == NULL)
      {
        if (attr == NULL)
	  break;
	else
          continue;
      }

     /*
      * Display the device...
      */

      if (long_status)
      {
	_cupsLangPrintf(stdout,
	                _("Model:  name = %s\n"
			  "        natural_language = %s\n"
			  "        make-and-model = %s\n"
			  "        device-id = %s"),
			ppd_name, ppd_language, ppd_make_model, ppd_device_id);
      }
      else
        _cupsLangPrintf(stdout, "%s %s", ppd_name, ppd_make_model);

      if (attr == NULL)
        break;
    }

    ippDelete(response);
  }
  else
  {
    _cupsLangPrintf(stderr, "lpinfo: %s", cupsLastErrorString());

    return (1);
  }

  return (0);
}


/*
 * End of "$Id: lpinfo.c 10996 2013-05-29 11:51:34Z msweet $".
 */
