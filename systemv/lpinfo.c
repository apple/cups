/*
 * "$Id: lpinfo.c,v 1.3.2.8 2004/06/29 13:15:11 mike Exp $"
 *
 *   "lpinfo" command for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2004 by Easy Software Products.
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
 *       Hollywood, Maryland 20636-3142 USA
 *
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 *   main()         - Parse options and show information.
 *   show_devices() - Show available devices.
 *   show_models()  - Show available PPDs.
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <cups/cups.h>
#include <cups/language.h>
#include <cups/debug.h>
#include <cups/string.h>


/*
 * Local functions...
 */

static void	show_devices(http_t *, int);
static void	show_models(http_t *, int);


/*
 * 'main()' - Parse options and show status information.
 */

int
main(int  argc,			/* I - Number of command-line arguments */
     char *argv[])		/* I - Command-line arguments */
{
  int		i;		/* Looping var */
  http_t	*http;		/* Connection to server */
  int		long_status;	/* Long listing? */


  http        = NULL;
  long_status = 0;

  for (i = 1; i < argc; i ++)
    if (argv[i][0] == '-')
      switch (argv[i][1])
      {
        case 'E' : /* Encrypt */
#ifdef HAVE_SSL
	    cupsSetEncryption(HTTP_ENCRYPT_REQUIRED);

	    if (http)
	      httpEncryption(http, HTTP_ENCRYPT_REQUIRED);
#else
            fprintf(stderr, "%s: Sorry, no encryption support compiled in!\n",
	            argv[0]);
#endif /* HAVE_SSL */
	    break;

        case 'l' : /* Show long listing */
	    long_status = 1;
	    break;

        case 'm' : /* Show models */
	    if (!http)
	    {
              http = httpConnectEncrypt(cupsServer(), ippPort(),
	                                cupsEncryption());

	      if (http == NULL)
	      {
		perror("lpinfo: Unable to connect to server");
		return (1);
	      }
            }

            show_models(http, long_status);
	    break;
	    
        case 'v' : /* Show available devices */
	    if (!http)
	    {
              http = httpConnectEncrypt(cupsServer(), ippPort(),
	                                cupsEncryption());

	      if (http == NULL)
	      {
		perror("lpinfo: Unable to connect to server");
		return (1);
	      }
            }

            show_devices(http, long_status);
	    break;

        case 'h' : /* Connect to host */
	    if (http)
	    {
	      httpClose(http);
	      http = NULL;
	    }

	    if (argv[i][2] != '\0')
	      cupsSetServer(argv[i] + 2);
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
	        fputs("Error: need hostname after \'-h\' option!\n", stderr);
		return (1);
              }

	      cupsSetServer(argv[i]);
	    }
	    break;

	default :
	    fprintf(stderr, "lpinfo: Unknown option \'%c\'!\n", argv[i][1]);
	    return (1);
      }
    else
    {
      fprintf(stderr, "lpinfo: Unknown argument \'%s\'!\n", argv[i]);
      return (1);
    }

  return (0);
}


/*
 * 'show_devices()' - Show available devices.
 */

static void
show_devices(http_t *http,	/* I - HTTP connection to server */
             int    long_status)/* I - Long status report? */
{
  ipp_t		*request,	/* IPP Request */
		*response;	/* IPP Response */
  ipp_attribute_t *attr;	/* Current attribute */
  cups_lang_t	*language;	/* Default language */
  const char	*device_class,	/* Pointer into device-class */
		*device_info,	/* Pointer into device-info */
		*device_make,	/* Pointer into device-make-and-model */
		*device_uri;	/* Pointer into device-uri */


  if (http == NULL)
    return;

 /*
  * Build a CUPS_GET_DEVICES request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  */

  request = ippNew();

  request->request.op.operation_id = CUPS_GET_DEVICES;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
               NULL, "ipp://localhost/printers/");

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/")) != NULL)
  {
   /*
    * Loop through the device list and display them...
    */

    if (response->request.status.status_code > IPP_OK_CONFLICT)
    {
      fprintf(stderr, "lpinfo: cups-get-devices failed: %s\n",
              ippErrorString(response->request.status.status_code));
      ippDelete(response);
      return;
    }

    for (attr = response->attrs; attr != NULL; attr = attr->next)
    {
     /*
      * Skip leading attributes until we hit a device...
      */

      while (attr != NULL && attr->group_tag != IPP_TAG_PRINTER)
        attr = attr->next;

      if (attr == NULL)
        break;

     /*
      * Pull the needed attributes from this device...
      */

      device_class = NULL;
      device_info  = NULL;
      device_make  = NULL;
      device_uri   = NULL;

      while (attr != NULL && attr->group_tag == IPP_TAG_PRINTER)
      {
        if (strcmp(attr->name, "device-class") == 0 &&
	    attr->value_tag == IPP_TAG_KEYWORD)
	  device_class = attr->values[0].string.text;

        if (strcmp(attr->name, "device-info") == 0 &&
	    attr->value_tag == IPP_TAG_TEXT)
	  device_info = attr->values[0].string.text;

        if (strcmp(attr->name, "device-make-and-model") == 0 &&
	    attr->value_tag == IPP_TAG_TEXT)
	  device_make = attr->values[0].string.text;

        if (strcmp(attr->name, "device-uri") == 0 &&
	    attr->value_tag == IPP_TAG_URI)
	  device_uri = attr->values[0].string.text;

        attr = attr->next;
      }

     /*
      * See if we have everything needed...
      */

      if (device_class == NULL || device_info == NULL ||
          device_make == NULL || device_uri == NULL)
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
	printf("Device: uri = %s\n", device_uri);
	printf("        class = %s\n", device_class);
	printf("        info = %s\n", device_info);
	printf("        make-and-model = %s\n", device_make);
      }
      else
        printf("%s %s\n", device_class, device_uri);

      if (attr == NULL)
        break;
    }

    ippDelete(response);
  }
  else
    fprintf(stderr, "lpinfo: cups-get-devices failed: %s\n",
            ippErrorString(cupsLastError()));
}


/*
 * 'show_models()' - Show available PPDs.
 */

static void
show_models(http_t *http,	/* I - HTTP connection to server */
            int    long_status)	/* I - Long status report? */
{
  ipp_t		*request,	/* IPP Request */
		*response;	/* IPP Response */
  ipp_attribute_t *attr;	/* Current attribute */
  cups_lang_t	*language;	/* Default language */
  const char	*ppd_language,	/* Pointer into ppd-natural-language */
		*ppd_make,	/* Pointer into ppd-make-and-model */
		*ppd_name;	/* Pointer into ppd-name */


  if (http == NULL)
    return;

 /*
  * Build a CUPS_GET_PPDS request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  */

  request = ippNew();

  request->request.op.operation_id = CUPS_GET_PPDS;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
               NULL, "ipp://localhost/printers/");

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/")) != NULL)
  {
   /*
    * Loop through the device list and display them...
    */

    if (response->request.status.status_code > IPP_OK_CONFLICT)
    {
      fprintf(stderr, "lpinfo: cups-get-ppds failed: %s\n",
              ippErrorString(response->request.status.status_code));
      ippDelete(response);
      return;
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

      ppd_language = NULL;
      ppd_make     = NULL;
      ppd_name     = NULL;

      while (attr != NULL && attr->group_tag == IPP_TAG_PRINTER)
      {
        if (strcmp(attr->name, "ppd-natural-language") == 0 &&
	    attr->value_tag == IPP_TAG_LANGUAGE)
	  ppd_language = attr->values[0].string.text;

        if (strcmp(attr->name, "ppd-make-and-model") == 0 &&
	    attr->value_tag == IPP_TAG_TEXT)
	  ppd_make = attr->values[0].string.text;

        if (strcmp(attr->name, "ppd-name") == 0 &&
	    attr->value_tag == IPP_TAG_NAME)
	  ppd_name = attr->values[0].string.text;

        attr = attr->next;
      }

     /*
      * See if we have everything needed...
      */

      if (ppd_language == NULL || ppd_make == NULL || ppd_name == NULL)
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
	printf("Model:  name = %s\n", ppd_name);
	printf("        natural_language = %s\n", ppd_language);
	printf("        make-and-model = %s\n", ppd_make);
      }
      else
        printf("%s %s\n", ppd_name, ppd_make);

      if (attr == NULL)
        break;
    }

    ippDelete(response);
  }
  else
    fprintf(stderr, "lpinfo: cups-get-ppds failed: %s\n",
            ippErrorString(cupsLastError()));
}


/*
 * End of "$Id: lpinfo.c,v 1.3.2.8 2004/06/29 13:15:11 mike Exp $".
 */
