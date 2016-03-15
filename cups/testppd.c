/*
 * "$Id: testppd.c 12604 2015-05-06 01:43:05Z msweet $"
 *
 * PPD test program for CUPS.
 *
 * Copyright 2007-2014 by Apple Inc.
 * Copyright 1997-2006 by Easy Software Products.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * This file is subject to the Apple OS-Developed Software exception.
 */

/*
 * Include necessary headers...
 */

#undef _CUPS_NO_DEPRECATED
#include "cups-private.h"
#include <sys/stat.h>
#ifdef WIN32
#  include <io.h>
#else
#  include <unistd.h>
#  include <fcntl.h>
#endif /* WIN32 */
#include <math.h>


/*
 * Test data...
 */

static const char	*default_code =
			"[{\n"
			"%%BeginFeature: *InstalledDuplexer False\n"
			"%%EndFeature\n"
			"} stopped cleartomark\n"
			"[{\n"
			"%%BeginFeature: *PageRegion Letter\n"
			"PageRegion=Letter\n"
			"%%EndFeature\n"
			"} stopped cleartomark\n"
			"[{\n"
			"%%BeginFeature: *InputSlot Tray\n"
			"InputSlot=Tray\n"
			"%%EndFeature\n"
			"} stopped cleartomark\n"
			"[{\n"
			"%%BeginFeature: *MediaType Plain\n"
			"MediaType=Plain\n"
			"%%EndFeature\n"
			"} stopped cleartomark\n"
			"[{\n"
			"%%BeginFeature: *IntOption None\n"
			"%%EndFeature\n"
			"} stopped cleartomark\n"
			"[{\n"
			"%%BeginFeature: *StringOption None\n"
			"%%EndFeature\n"
			"} stopped cleartomark\n";

static const char	*custom_code =
			"[{\n"
			"%%BeginFeature: *InstalledDuplexer False\n"
			"%%EndFeature\n"
			"} stopped cleartomark\n"
			"[{\n"
			"%%BeginFeature: *InputSlot Tray\n"
			"InputSlot=Tray\n"
			"%%EndFeature\n"
			"} stopped cleartomark\n"
			"[{\n"
			"%%BeginFeature: *MediaType Plain\n"
			"MediaType=Plain\n"
			"%%EndFeature\n"
			"} stopped cleartomark\n"
			"[{\n"
			"%%BeginFeature: *IntOption None\n"
			"%%EndFeature\n"
			"} stopped cleartomark\n"
			"[{\n"
			"%%BeginFeature: *CustomStringOption True\n"
			"(value\\0502\\051)\n"
			"(value 1)\n"
			"StringOption=Custom\n"
			"%%EndFeature\n"
			"} stopped cleartomark\n"
			"[{\n"
			"%%BeginFeature: *CustomPageSize True\n"
			"400\n"
			"500\n"
			"0\n"
			"0\n"
			"0\n"
			"PageSize=Custom\n"
			"%%EndFeature\n"
			"} stopped cleartomark\n";

static const char	*default2_code =
			"[{\n"
			"%%BeginFeature: *InstalledDuplexer False\n"
			"%%EndFeature\n"
			"} stopped cleartomark\n"
			"[{\n"
			"%%BeginFeature: *InputSlot Tray\n"
			"InputSlot=Tray\n"
			"%%EndFeature\n"
			"} stopped cleartomark\n"
			"[{\n"
			"%%BeginFeature: *Quality Normal\n"
			"Quality=Normal\n"
			"%%EndFeature\n"
			"} stopped cleartomark\n"
			"[{\n"
			"%%BeginFeature: *IntOption None\n"
			"%%EndFeature\n"
			"} stopped cleartomark\n"
			"[{\n"
			"%%BeginFeature: *StringOption None\n"
			"%%EndFeature\n"
			"} stopped cleartomark\n";


/*
 * 'main()' - Main entry.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  int		i;			/* Looping var */
  ppd_file_t	*ppd;			/* PPD file loaded from disk */
  int		status;			/* Status of tests (0 = success, 1 = fail) */
  int		conflicts;		/* Number of conflicts */
  char		*s;			/* String */
  char		buffer[8192];		/* String buffer */
  const char	*text,			/* Localized text */
		*val;			/* Option value */
  int		num_options;		/* Number of options */
  cups_option_t	*options;		/* Options */
  ppd_size_t	minsize,		/* Minimum size */
		maxsize,		/* Maximum size */
		*size;			/* Current size */
  ppd_attr_t	*attr;			/* Current attribute */
  _ppd_cache_t	*pc;			/* PPD cache */


  status = 0;

  if (argc == 1)
  {
   /*
    * Setup directories for locale stuff...
    */

    if (access("locale", 0))
    {
      mkdir("locale", 0777);
      mkdir("locale/fr", 0777);
      symlink("../../../locale/cups_fr.po", "locale/fr/cups_fr.po");
      mkdir("locale/zh_TW", 0777);
      symlink("../../../locale/cups_zh_TW.po", "locale/zh_TW/cups_zh_TW.po");
    }

    putenv("LOCALEDIR=locale");
    putenv("SOFTWARE=CUPS");

   /*
    * Do tests with test.ppd...
    */

    fputs("ppdOpenFile(test.ppd): ", stdout);

    if ((ppd = _ppdOpenFile("test.ppd", _PPD_LOCALIZATION_ALL)) != NULL)
      puts("PASS");
    else
    {
      ppd_status_t	err;		/* Last error in file */
      int		line;		/* Line number in file */


      status ++;
      err = ppdLastError(&line);

      printf("FAIL (%s on line %d)\n", ppdErrorString(err), line);
    }

    fputs("ppdFindAttr(wildcard): ", stdout);
    if ((attr = ppdFindAttr(ppd, "cupsTest", NULL)) == NULL)
    {
      status ++;
      puts("FAIL (not found)");
    }
    else if (strcmp(attr->name, "cupsTest") || strcmp(attr->spec, "Foo"))
    {
      status ++;
      printf("FAIL (got \"%s %s\")\n", attr->name, attr->spec);
    }
    else
      puts("PASS");

    fputs("ppdFindNextAttr(wildcard): ", stdout);
    if ((attr = ppdFindNextAttr(ppd, "cupsTest", NULL)) == NULL)
    {
      status ++;
      puts("FAIL (not found)");
    }
    else if (strcmp(attr->name, "cupsTest") || strcmp(attr->spec, "Bar"))
    {
      status ++;
      printf("FAIL (got \"%s %s\")\n", attr->name, attr->spec);
    }
    else
      puts("PASS");

    fputs("ppdFindAttr(Foo): ", stdout);
    if ((attr = ppdFindAttr(ppd, "cupsTest", "Foo")) == NULL)
    {
      status ++;
      puts("FAIL (not found)");
    }
    else if (strcmp(attr->name, "cupsTest") || strcmp(attr->spec, "Foo"))
    {
      status ++;
      printf("FAIL (got \"%s %s\")\n", attr->name, attr->spec);
    }
    else
      puts("PASS");

    fputs("ppdFindNextAttr(Foo): ", stdout);
    if ((attr = ppdFindNextAttr(ppd, "cupsTest", "Foo")) != NULL)
    {
      status ++;
      printf("FAIL (got \"%s %s\")\n", attr->name, attr->spec);
    }
    else
      puts("PASS");

    fputs("ppdMarkDefaults: ", stdout);
    ppdMarkDefaults(ppd);

    if ((conflicts = ppdConflicts(ppd)) == 0)
      puts("PASS");
    else
    {
      status ++;
      printf("FAIL (%d conflicts)\n", conflicts);
    }

    fputs("ppdEmitString (defaults): ", stdout);
    if ((s = ppdEmitString(ppd, PPD_ORDER_ANY, 0.0)) != NULL &&
	!strcmp(s, default_code))
      puts("PASS");
    else
    {
      status ++;
      printf("FAIL (%d bytes instead of %d)\n", s ? (int)strlen(s) : 0,
	     (int)strlen(default_code));

      if (s)
	puts(s);
    }

    if (s)
      free(s);

    fputs("ppdEmitString (custom size and string): ", stdout);
    ppdMarkOption(ppd, "PageSize", "Custom.400x500");
    ppdMarkOption(ppd, "StringOption", "{String1=\"value 1\" String2=value(2)}");

    if ((s = ppdEmitString(ppd, PPD_ORDER_ANY, 0.0)) != NULL &&
	!strcmp(s, custom_code))
      puts("PASS");
    else
    {
      status ++;
      printf("FAIL (%d bytes instead of %d)\n", s ? (int)strlen(s) : 0,
	     (int)strlen(custom_code));

      if (s)
	puts(s);
    }

    if (s)
      free(s);

   /*
    * Test constraints...
    */

    fputs("cupsGetConflicts(InputSlot=Envelope): ", stdout);
    ppdMarkOption(ppd, "PageSize", "Letter");

    num_options = cupsGetConflicts(ppd, "InputSlot", "Envelope", &options);
    if (num_options != 2 ||
        (val = cupsGetOption("PageRegion", num_options, options)) == NULL ||
	_cups_strcasecmp(val, "Letter") ||
	(val = cupsGetOption("PageSize", num_options, options)) == NULL ||
	_cups_strcasecmp(val, "Letter"))
    {
      printf("FAIL (%d options:", num_options);
      for (i = 0; i < num_options; i ++)
        printf(" %s=%s", options[i].name, options[i].value);
      puts(")");
      status ++;
    }
    else
      puts("PASS");

    fputs("ppdConflicts(): ", stdout);
    ppdMarkOption(ppd, "InputSlot", "Envelope");

    if ((conflicts = ppdConflicts(ppd)) == 2)
      puts("PASS (2)");
    else
    {
      printf("FAIL (%d)\n", conflicts);
      status ++;
    }

    fputs("cupsResolveConflicts(InputSlot=Envelope): ", stdout);
    num_options = 0;
    options     = NULL;
    if (!cupsResolveConflicts(ppd, "InputSlot", "Envelope", &num_options,
                             &options))
    {
      puts("FAIL (Unable to resolve)");
      status ++;
    }
    else if (num_options != 2 ||
             !cupsGetOption("PageSize", num_options, options))
    {
      printf("FAIL (%d options:", num_options);
      for (i = 0; i < num_options; i ++)
        printf(" %s=%s", options[i].name, options[i].value);
      puts(")");
      status ++;
    }
    else
      puts("PASS (Resolved by changing PageSize)");

    cupsFreeOptions(num_options, options);

    fputs("cupsResolveConflicts(No option/choice): ", stdout);
    num_options = 0;
    options     = NULL;
    if (cupsResolveConflicts(ppd, NULL, NULL, &num_options, &options) &&
        num_options == 1 && !_cups_strcasecmp(options[0].name, "InputSlot") &&
	!_cups_strcasecmp(options[0].value, "Tray"))
      puts("PASS (Resolved by changing InputSlot)");
    else if (num_options > 0)
    {
      printf("FAIL (%d options:", num_options);
      for (i = 0; i < num_options; i ++)
        printf(" %s=%s", options[i].name, options[i].value);
      puts(")");
      status ++;
    }
    else
    {
      puts("FAIL (Unable to resolve)");
      status ++;
    }
    cupsFreeOptions(num_options, options);

    fputs("ppdInstallableConflict(): ", stdout);
    if (ppdInstallableConflict(ppd, "Duplex", "DuplexNoTumble") &&
        !ppdInstallableConflict(ppd, "Duplex", "None"))
      puts("PASS");
    else if (!ppdInstallableConflict(ppd, "Duplex", "DuplexNoTumble"))
    {
      puts("FAIL (Duplex=DuplexNoTumble did not conflict)");
      status ++;
    }
    else
    {
      puts("FAIL (Duplex=None conflicted)");
      status ++;
    }

   /*
    * ppdPageSizeLimits
    */

    fputs("ppdPageSizeLimits: ", stdout);
    if (ppdPageSizeLimits(ppd, &minsize, &maxsize))
    {
      if (fabs(minsize.width - 36.0) > 0.001 || fabs(minsize.length - 36.0) > 0.001 ||
          fabs(maxsize.width - 1080.0) > 0.001 || fabs(maxsize.length - 86400.0) > 0.001)
      {
        printf("FAIL (got min=%.3fx%.3f, max=%.3fx%.3f, "
	       "expected min=36x36, max=1080x86400)\n", minsize.width,
	       minsize.length, maxsize.width, maxsize.length);
        status ++;
      }
      else
        puts("PASS");
    }
    else
    {
      puts("FAIL (returned 0)");
      status ++;
    }

   /*
    * cupsMarkOptions with PWG and IPP size names.
    */

    fputs("cupsMarkOptions(media=iso-a4): ", stdout);
    num_options = cupsAddOption("media", "iso-a4", 0, &options);
    cupsMarkOptions(ppd, num_options, options);
    cupsFreeOptions(num_options, options);

    size = ppdPageSize(ppd, NULL);
    if (!size || strcmp(size->name, "A4"))
    {
      printf("FAIL (%s)\n", size ? size->name : "unknown");
      status ++;
    }
    else
      puts("PASS");

    fputs("cupsMarkOptions(media=na_letter_8.5x11in): ", stdout);
    num_options = cupsAddOption("media", "na_letter_8.5x11in", 0, &options);
    cupsMarkOptions(ppd, num_options, options);
    cupsFreeOptions(num_options, options);

    size = ppdPageSize(ppd, NULL);
    if (!size || strcmp(size->name, "Letter"))
    {
      printf("FAIL (%s)\n", size ? size->name : "unknown");
      status ++;
    }
    else
      puts("PASS");

    fputs("cupsMarkOptions(media=oe_letter-fullbleed_8.5x11in): ", stdout);
    num_options = cupsAddOption("media", "oe_letter-fullbleed_8.5x11in", 0,
                                &options);
    cupsMarkOptions(ppd, num_options, options);
    cupsFreeOptions(num_options, options);

    size = ppdPageSize(ppd, NULL);
    if (!size || strcmp(size->name, "Letter.Fullbleed"))
    {
      printf("FAIL (%s)\n", size ? size->name : "unknown");
      status ++;
    }
    else
      puts("PASS");

    fputs("cupsMarkOptions(media=A4): ", stdout);
    num_options = cupsAddOption("media", "A4", 0, &options);
    cupsMarkOptions(ppd, num_options, options);
    cupsFreeOptions(num_options, options);

    size = ppdPageSize(ppd, NULL);
    if (!size || strcmp(size->name, "A4"))
    {
      printf("FAIL (%s)\n", size ? size->name : "unknown");
      status ++;
    }
    else
      puts("PASS");

   /*
    * Custom sizes...
    */

    fputs("cupsMarkOptions(media=Custom.8x10in): ", stdout);
    num_options = cupsAddOption("media", "Custom.8x10in", 0, &options);
    cupsMarkOptions(ppd, num_options, options);
    cupsFreeOptions(num_options, options);

    size = ppdPageSize(ppd, NULL);
    if (!size || strcmp(size->name, "Custom") ||
        fabs(size->width - 576.0) > 0.001 ||
        fabs(size->length - 720.0) > 0.001)
    {
      printf("FAIL (%s - %gx%g)\n", size ? size->name : "unknown",
             size ? size->width : 0.0, size ? size->length : 0.0);
      status ++;
    }
    else
      puts("PASS");

   /*
    * Test localization...
    */

    fputs("ppdLocalizeIPPReason(text): ", stdout);
    if (ppdLocalizeIPPReason(ppd, "foo", NULL, buffer, sizeof(buffer)) &&
        !strcmp(buffer, "Foo Reason"))
      puts("PASS");
    else
    {
      status ++;
      printf("FAIL (\"%s\" instead of \"Foo Reason\")\n", buffer);
    }

    fputs("ppdLocalizeIPPReason(http): ", stdout);
    if (ppdLocalizeIPPReason(ppd, "foo", "http", buffer, sizeof(buffer)) &&
        !strcmp(buffer, "http://foo/bar.html"))
      puts("PASS");
    else
    {
      status ++;
      printf("FAIL (\"%s\" instead of \"http://foo/bar.html\")\n", buffer);
    }

    fputs("ppdLocalizeIPPReason(help): ", stdout);
    if (ppdLocalizeIPPReason(ppd, "foo", "help", buffer, sizeof(buffer)) &&
        !strcmp(buffer, "help:anchor='foo'%20bookID=Vendor%20Help"))
      puts("PASS");
    else
    {
      status ++;
      printf("FAIL (\"%s\" instead of \"help:anchor='foo'%%20bookID=Vendor%%20Help\")\n", buffer);
    }

    fputs("ppdLocalizeIPPReason(file): ", stdout);
    if (ppdLocalizeIPPReason(ppd, "foo", "file", buffer, sizeof(buffer)) &&
        !strcmp(buffer, "/help/foo/bar.html"))
      puts("PASS");
    else
    {
      status ++;
      printf("FAIL (\"%s\" instead of \"/help/foo/bar.html\")\n", buffer);
    }

    putenv("LANG=fr");
    putenv("LC_ALL=fr");
    putenv("LC_CTYPE=fr");
    putenv("LC_MESSAGES=fr");

    fputs("ppdLocalizeIPPReason(fr text): ", stdout);
    if (ppdLocalizeIPPReason(ppd, "foo", NULL, buffer, sizeof(buffer)) &&
        !strcmp(buffer, "La Long Foo Reason"))
      puts("PASS");
    else
    {
      status ++;
      printf("FAIL (\"%s\" instead of \"La Long Foo Reason\")\n", buffer);
    }

    putenv("LANG=zh_TW");
    putenv("LC_ALL=zh_TW");
    putenv("LC_CTYPE=zh_TW");
    putenv("LC_MESSAGES=zh_TW");

    fputs("ppdLocalizeIPPReason(zh_TW text): ", stdout);
    if (ppdLocalizeIPPReason(ppd, "foo", NULL, buffer, sizeof(buffer)) &&
        !strcmp(buffer, "Number 1 Foo Reason"))
      puts("PASS");
    else
    {
      status ++;
      printf("FAIL (\"%s\" instead of \"Number 1 Foo Reason\")\n", buffer);
    }

   /*
    * cupsMarkerName localization...
    */

    putenv("LANG=en");
    putenv("LC_ALL=en");
    putenv("LC_CTYPE=en");
    putenv("LC_MESSAGES=en");

    fputs("ppdLocalizeMarkerName(bogus): ", stdout);

    if ((text = ppdLocalizeMarkerName(ppd, "bogus")) != NULL)
    {
      status ++;
      printf("FAIL (\"%s\" instead of NULL)\n", text);
    }
    else
      puts("PASS");

    fputs("ppdLocalizeMarkerName(cyan): ", stdout);

    if ((text = ppdLocalizeMarkerName(ppd, "cyan")) != NULL &&
        !strcmp(text, "Cyan Toner"))
      puts("PASS");
    else
    {
      status ++;
      printf("FAIL (\"%s\" instead of \"Cyan Toner\")\n",
             text ? text : "(null)");
    }

    putenv("LANG=fr");
    putenv("LC_ALL=fr");
    putenv("LC_CTYPE=fr");
    putenv("LC_MESSAGES=fr");

    fputs("ppdLocalizeMarkerName(fr cyan): ", stdout);
    if ((text = ppdLocalizeMarkerName(ppd, "cyan")) != NULL &&
        !strcmp(text, "La Toner Cyan"))
      puts("PASS");
    else
    {
      status ++;
      printf("FAIL (\"%s\" instead of \"La Toner Cyan\")\n",
             text ? text : "(null)");
    }

    putenv("LANG=zh_TW");
    putenv("LC_ALL=zh_TW");
    putenv("LC_CTYPE=zh_TW");
    putenv("LC_MESSAGES=zh_TW");

    fputs("ppdLocalizeMarkerName(zh_TW cyan): ", stdout);
    if ((text = ppdLocalizeMarkerName(ppd, "cyan")) != NULL &&
        !strcmp(text, "Number 1 Cyan Toner"))
      puts("PASS");
    else
    {
      status ++;
      printf("FAIL (\"%s\" instead of \"Number 1 Cyan Toner\")\n",
             text ? text : "(null)");
    }

    ppdClose(ppd);

   /*
    * Test new constraints...
    */

    fputs("ppdOpenFile(test2.ppd): ", stdout);

    if ((ppd = ppdOpenFile("test2.ppd")) != NULL)
      puts("PASS");
    else
    {
      ppd_status_t	err;		/* Last error in file */
      int		line;		/* Line number in file */


      status ++;
      err = ppdLastError(&line);

      printf("FAIL (%s on line %d)\n", ppdErrorString(err), line);
    }

    fputs("ppdMarkDefaults: ", stdout);
    ppdMarkDefaults(ppd);

    if ((conflicts = ppdConflicts(ppd)) == 0)
      puts("PASS");
    else
    {
      status ++;
      printf("FAIL (%d conflicts)\n", conflicts);
    }

    fputs("ppdEmitString (defaults): ", stdout);
    if ((s = ppdEmitString(ppd, PPD_ORDER_ANY, 0.0)) != NULL &&
	!strcmp(s, default2_code))
      puts("PASS");
    else
    {
      status ++;
      printf("FAIL (%d bytes instead of %d)\n", s ? (int)strlen(s) : 0,
	     (int)strlen(default2_code));

      if (s)
	puts(s);
    }

    if (s)
      free(s);

    fputs("ppdConflicts(): ", stdout);
    ppdMarkOption(ppd, "PageSize", "Env10");
    ppdMarkOption(ppd, "InputSlot", "Envelope");
    ppdMarkOption(ppd, "Quality", "Photo");

    if ((conflicts = ppdConflicts(ppd)) == 1)
      puts("PASS (1)");
    else
    {
      printf("FAIL (%d)\n", conflicts);
      status ++;
    }

    fputs("cupsResolveConflicts(Quality=Photo): ", stdout);
    num_options = 0;
    options     = NULL;
    if (cupsResolveConflicts(ppd, "Quality", "Photo", &num_options,
                             &options))
    {
      printf("FAIL (%d options:", num_options);
      for (i = 0; i < num_options; i ++)
        printf(" %s=%s", options[i].name, options[i].value);
      puts(")");
      status ++;
    }
    else
      puts("PASS (Unable to resolve)");
    cupsFreeOptions(num_options, options);

    fputs("cupsResolveConflicts(No option/choice): ", stdout);
    num_options = 0;
    options     = NULL;
    if (cupsResolveConflicts(ppd, NULL, NULL, &num_options, &options) &&
        num_options == 1 && !_cups_strcasecmp(options->name, "Quality") &&
	!_cups_strcasecmp(options->value, "Normal"))
      puts("PASS");
    else if (num_options > 0)
    {
      printf("FAIL (%d options:", num_options);
      for (i = 0; i < num_options; i ++)
        printf(" %s=%s", options[i].name, options[i].value);
      puts(")");
      status ++;
    }
    else
    {
      puts("FAIL (Unable to resolve!)");
      status ++;
    }
    cupsFreeOptions(num_options, options);

    fputs("cupsResolveConflicts(loop test): ", stdout);
    ppdMarkOption(ppd, "PageSize", "A4");
    ppdMarkOption(ppd, "InputSlot", "Tray");
    ppdMarkOption(ppd, "Quality", "Photo");
    num_options = 0;
    options     = NULL;
    if (!cupsResolveConflicts(ppd, NULL, NULL, &num_options, &options))
      puts("PASS");
    else if (num_options > 0)
    {
      printf("FAIL (%d options:", num_options);
      for (i = 0; i < num_options; i ++)
        printf(" %s=%s", options[i].name, options[i].value);
      puts(")");
    }
    else
      puts("FAIL (No conflicts!)");

    fputs("ppdInstallableConflict(): ", stdout);
    if (ppdInstallableConflict(ppd, "Duplex", "DuplexNoTumble") &&
        !ppdInstallableConflict(ppd, "Duplex", "None"))
      puts("PASS");
    else if (!ppdInstallableConflict(ppd, "Duplex", "DuplexNoTumble"))
    {
      puts("FAIL (Duplex=DuplexNoTumble did not conflict)");
      status ++;
    }
    else
    {
      puts("FAIL (Duplex=None conflicted)");
      status ++;
    }

   /*
    * ppdPageSizeLimits
    */

    ppdMarkDefaults(ppd);

    fputs("ppdPageSizeLimits(default): ", stdout);
    if (ppdPageSizeLimits(ppd, &minsize, &maxsize))
    {
      if (fabs(minsize.width - 36.0) > 0.001 || fabs(minsize.length - 36.0) > 0.001 ||
          fabs(maxsize.width - 1080.0) > 0.001 || fabs(maxsize.length - 86400.0) > 0.001)
      {
        printf("FAIL (got min=%.0fx%.0f, max=%.0fx%.0f, "
	       "expected min=36x36, max=1080x86400)\n", minsize.width,
	       minsize.length, maxsize.width, maxsize.length);
        status ++;
      }
      else
        puts("PASS");
    }
    else
    {
      puts("FAIL (returned 0)");
      status ++;
    }

    ppdMarkOption(ppd, "InputSlot", "Manual");

    fputs("ppdPageSizeLimits(InputSlot=Manual): ", stdout);
    if (ppdPageSizeLimits(ppd, &minsize, &maxsize))
    {
      if (fabs(minsize.width - 100.0) > 0.001 || fabs(minsize.length - 100.0) > 0.001 ||
          fabs(maxsize.width - 1000.0) > 0.001 || fabs(maxsize.length - 1000.0) > 0.001)
      {
        printf("FAIL (got min=%.0fx%.0f, max=%.0fx%.0f, "
	       "expected min=100x100, max=1000x1000)\n", minsize.width,
	       minsize.length, maxsize.width, maxsize.length);
        status ++;
      }
      else
        puts("PASS");
    }
    else
    {
      puts("FAIL (returned 0)");
      status ++;
    }

    ppdMarkOption(ppd, "Quality", "Photo");

    fputs("ppdPageSizeLimits(Quality=Photo): ", stdout);
    if (ppdPageSizeLimits(ppd, &minsize, &maxsize))
    {
      if (fabs(minsize.width - 200.0) > 0.001 || fabs(minsize.length - 200.0) > 0.001 ||
          fabs(maxsize.width - 1000.0) > 0.001 || fabs(maxsize.length - 1000.0) > 0.001)
      {
        printf("FAIL (got min=%.0fx%.0f, max=%.0fx%.0f, "
	       "expected min=200x200, max=1000x1000)\n", minsize.width,
	       minsize.length, maxsize.width, maxsize.length);
        status ++;
      }
      else
        puts("PASS");
    }
    else
    {
      puts("FAIL (returned 0)");
      status ++;
    }

    ppdMarkOption(ppd, "InputSlot", "Tray");

    fputs("ppdPageSizeLimits(Quality=Photo): ", stdout);
    if (ppdPageSizeLimits(ppd, &minsize, &maxsize))
    {
      if (fabs(minsize.width - 300.0) > 0.001 || fabs(minsize.length - 300.0) > 0.001 ||
          fabs(maxsize.width - 1080.0) > 0.001 || fabs(maxsize.length - 86400.0) > 0.001)
      {
        printf("FAIL (got min=%.0fx%.0f, max=%.0fx%.0f, "
	       "expected min=300x300, max=1080x86400)\n", minsize.width,
	       minsize.length, maxsize.width, maxsize.length);
        status ++;
      }
      else
        puts("PASS");
    }
    else
    {
      puts("FAIL (returned 0)");
      status ++;
    }
  }
  else
  {
    const char	*filename;		/* PPD filename */
    struct stat	fileinfo;		/* File information */


    if (strchr(argv[1], ':'))
    {
     /*
      * Server PPD...
      */

      if ((filename = cupsGetServerPPD(CUPS_HTTP_DEFAULT, argv[1])) == NULL)
      {
        printf("%s: %s\n", argv[1], cupsLastErrorString());
        return (1);
      }
    }
    else if (!strncmp(argv[1], "-d", 2))
    {
      const char *printer;		/* Printer name */

      if (argv[1][2])
	printer = argv[1] + 2;
      else if (argv[2])
	printer = argv[2];
      else
      {
        puts("Usage: ./testppd -d printer");
	return (1);
      }

      filename = cupsGetPPD(printer);

      if (!filename)
      {
        printf("%s: %s\n", printer, cupsLastErrorString());
        return (1);
      }
    }
    else
      filename = argv[1];

    if (lstat(filename, &fileinfo))
    {
      printf("%s: %s\n", filename, strerror(errno));
      return (1);
    }

    if (S_ISLNK(fileinfo.st_mode))
    {
      char	realfile[1024];		/* Real file path */
      ssize_t	realsize;		/* Size of real file path */


      if ((realsize = readlink(filename, realfile, sizeof(realfile) - 1)) < 0)
        strlcpy(realfile, "Unknown", sizeof(realfile));
      else
        realfile[realsize] = '\0';

      if (stat(realfile, &fileinfo))
	printf("%s: symlink to \"%s\", %s\n", filename, realfile,
	       strerror(errno));
      else
	printf("%s: symlink to \"%s\", %ld bytes\n", filename, realfile,
	       (long)fileinfo.st_size);
    }
    else
      printf("%s: regular file, %ld bytes\n", filename, (long)fileinfo.st_size);

    if ((ppd = ppdOpenFile(filename)) == NULL)
    {
      ppd_status_t	err;		/* Last error in file */
      int		line;		/* Line number in file */


      status ++;
      err = ppdLastError(&line);

      printf("%s: %s on line %d\n", argv[1], ppdErrorString(err), line);
    }
    else
    {
      int		j, k;		/* Looping vars */
      ppd_group_t	*group;		/* Option group */
      ppd_option_t	*option;	/* Option */
      ppd_coption_t	*coption;	/* Custom option */
      ppd_cparam_t	*cparam;	/* Custom parameter */
      ppd_const_t	*c;		/* UIConstraints */
      char		lang[255],	/* LANG environment variable */
			lc_all[255],	/* LC_ALL environment variable */
			lc_ctype[255],	/* LC_CTYPE environment variable */
			lc_messages[255];/* LC_MESSAGES environment variable */


      if (argc > 2)
      {
        snprintf(lang, sizeof(lang), "LANG=%s", argv[2]);
	putenv(lang);
        snprintf(lc_all, sizeof(lc_all), "LC_ALL=%s", argv[2]);
	putenv(lc_all);
        snprintf(lc_ctype, sizeof(lc_ctype), "LC_CTYPE=%s", argv[2]);
	putenv(lc_ctype);
        snprintf(lc_messages, sizeof(lc_messages), "LC_MESSAGES=%s", argv[2]);
	putenv(lc_messages);
      }

      ppdLocalize(ppd);
      ppdMarkDefaults(ppd);

      if (argc > 3)
      {
        text = ppdLocalizeIPPReason(ppd, argv[3], NULL, buffer, sizeof(buffer));
	printf("ppdLocalizeIPPReason(%s)=%s\n", argv[3],
	       text ? text : "(null)");
	return (text == NULL);
      }

      for (i = ppd->num_groups, group = ppd->groups;
	   i > 0;
	   i --, group ++)
      {
	printf("%s (%s):\n", group->name, group->text);

	for (j = group->num_options, option = group->options;
	     j > 0;
	     j --, option ++)
	{
	  printf("    %s (%s):\n", option->keyword, option->text);

	  for (k = 0; k < option->num_choices; k ++)
	    printf("        - %s%s (%s)\n",
	           option->choices[k].marked ? "*" : "",
		   option->choices[k].choice, option->choices[k].text);

          if ((coption = ppdFindCustomOption(ppd, option->keyword)) != NULL)
	  {
	    for (cparam = (ppd_cparam_t *)cupsArrayFirst(coption->params);
	         cparam;
		 cparam = (ppd_cparam_t *)cupsArrayNext(coption->params))
            {
	      switch (cparam->type)
	      {
	        case PPD_CUSTOM_CURVE :
		    printf("              %s(%s): PPD_CUSTOM_CURVE (%g to %g)\n",
		           cparam->name, cparam->text,
			   cparam->minimum.custom_curve,
			   cparam->maximum.custom_curve);
		    break;

	        case PPD_CUSTOM_INT :
		    printf("              %s(%s): PPD_CUSTOM_INT (%d to %d)\n",
		           cparam->name, cparam->text,
			   cparam->minimum.custom_int,
			   cparam->maximum.custom_int);
		    break;

	        case PPD_CUSTOM_INVCURVE :
		    printf("              %s(%s): PPD_CUSTOM_INVCURVE (%g to %g)\n",
		           cparam->name, cparam->text,
			   cparam->minimum.custom_invcurve,
			   cparam->maximum.custom_invcurve);
		    break;

	        case PPD_CUSTOM_PASSCODE :
		    printf("              %s(%s): PPD_CUSTOM_PASSCODE (%d to %d)\n",
		           cparam->name, cparam->text,
			   cparam->minimum.custom_passcode,
			   cparam->maximum.custom_passcode);
		    break;

	        case PPD_CUSTOM_PASSWORD :
		    printf("              %s(%s): PPD_CUSTOM_PASSWORD (%d to %d)\n",
		           cparam->name, cparam->text,
			   cparam->minimum.custom_password,
			   cparam->maximum.custom_password);
		    break;

	        case PPD_CUSTOM_POINTS :
		    printf("              %s(%s): PPD_CUSTOM_POINTS (%g to %g)\n",
		           cparam->name, cparam->text,
			   cparam->minimum.custom_points,
			   cparam->maximum.custom_points);
		    break;

	        case PPD_CUSTOM_REAL :
		    printf("              %s(%s): PPD_CUSTOM_REAL (%g to %g)\n",
		           cparam->name, cparam->text,
			   cparam->minimum.custom_real,
			   cparam->maximum.custom_real);
		    break;

	        case PPD_CUSTOM_STRING :
		    printf("              %s(%s): PPD_CUSTOM_STRING (%d to %d)\n",
		           cparam->name, cparam->text,
			   cparam->minimum.custom_string,
			   cparam->maximum.custom_string);
		    break;
	      }
	    }
	  }
	}
      }

      puts("\nSizes:");
      for (i = ppd->num_sizes, size = ppd->sizes; i > 0; i --, size ++)
        printf("    %s = %gx%g, [%g %g %g %g]\n", size->name, size->width,
	       size->length, size->left, size->bottom, size->right, size->top);

      puts("\nConstraints:");

      for (i = ppd->num_consts, c = ppd->consts; i > 0; i --, c ++)
        printf("    *UIConstraints: *%s %s *%s %s\n", c->option1, c->choice1,
	       c->option2, c->choice2);
      if (ppd->num_consts == 0)
        puts("    NO CONSTRAINTS");

      puts("\nFilters:");

      for (i = 0; i < ppd->num_filters; i ++)
        printf("    %s\n", ppd->filters[i]);

      if (ppd->num_filters == 0)
        puts("    NO FILTERS");

      puts("\nAttributes:");

      for (attr = (ppd_attr_t *)cupsArrayFirst(ppd->sorted_attrs);
           attr;
	   attr = (ppd_attr_t *)cupsArrayNext(ppd->sorted_attrs))
        printf("    *%s %s/%s: \"%s\"\n", attr->name, attr->spec,
	       attr->text, attr->value ? attr->value : "");

      puts("\nPPD Cache:");
      if ((pc = _ppdCacheCreateWithPPD(ppd)) == NULL)
        printf("    Unable to create: %s\n", cupsLastErrorString());
      else
      {
        _ppdCacheWriteFile(pc, "t.cache", NULL);
        puts("    Wrote t.cache.");
      }
    }

    if (!strncmp(argv[1], "-d", 2))
      unlink(filename);
  }

#ifdef __APPLE__
  if (getenv("MallocStackLogging") && getenv("MallocStackLoggingNoCompact"))
  {
    char	command[1024];		/* malloc_history command */

    snprintf(command, sizeof(command), "malloc_history %d -all_by_size",
	     getpid());
    fflush(stdout);
    system(command);
  }
#endif /* __APPLE__ */

  ppdClose(ppd);

  return (status);
}


/*
 * End of "$Id: testppd.c 12604 2015-05-06 01:43:05Z msweet $".
 */
