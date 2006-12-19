/*
 * "$Id$"
 *
 *   PPD test program for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2006 by Easy Software Products.
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
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   main() - Main entry.
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <cups/string.h>
#include <errno.h>
#include "ppd.h"
#ifdef WIN32
#  include <io.h>
#else
#  include <unistd.h>
#  include <fcntl.h>
#endif /* WIN32 */


/*
 * Test data...
 */

static const char	*default_code =
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
			"%%BeginFeature: *IntOption None\n"
			"%%EndFeature\n"
			"} stopped cleartomark\n"
			"[{\n"
			"%%BeginFeature: *StringOption None\n"
			"%%EndFeature\n"
			"} stopped cleartomark\n";

static const char	*custom_code =
			"[{\n"
			"%%BeginFeature: *CustomPageSize True\n"
			"400\n"
			"500\n"
			"0\n"
			"0\n"
			"0\n"
			"PageSize=Custom\n"
			"%%EndFeature\n"
			"} stopped cleartomark\n"
			"[{\n"
			"%%BeginFeature: *InputSlot Tray\n"
			"InputSlot=Tray\n"
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
  ppd_file_t	*ppd;			/* PPD file loaded from disk */
  int		status;			/* Status of tests (0 = success, 1 = fail) */
  int		conflicts;		/* Number of conflicts */
  char		*s;			/* String */


  status = 0;

  if (argc == 1)
  {
    fputs("ppdOpenFile: ", stdout);

    if ((ppd = ppdOpenFile("test.ppd")) != NULL)
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
	!strcmp(s, default_code))
      puts("PASS");
    else
    {
      printf("FAIL (%d bytes instead of %d)\n", s ? (int)strlen(s) : 0,
	     (int)strlen(default_code));

      if (s)
	puts(s);
    }

    if (s)
      free(s);

    fputs("ppdEmitString (custom size): ", stdout);
    ppdMarkOption(ppd, "PageSize", "Custom.400x500");

    if ((s = ppdEmitString(ppd, PPD_ORDER_ANY, 0.0)) != NULL &&
	!strcmp(s, custom_code))
      puts("PASS");
    else
    {
      printf("FAIL (%d bytes instead of %d)\n", s ? (int)strlen(s) : 0,
	     (int)strlen(custom_code));

      if (s)
	puts(s);
    }

    if (s)
      free(s);

    ppdClose(ppd);
  }
  else
  {
    if ((ppd = ppdOpenFile(argv[1])) == NULL)
    {
      ppd_status_t	err;		/* Last error in file */
      int		line;		/* Line number in file */


      status ++;
      err = ppdLastError(&line);

      printf("%s: %s on line %d\n", argv[1], ppdErrorString(err), line);
    }
    else
    {
      int		i, j, k;	/* Looping vars */
      ppd_attr_t	*attr;		/* Current attribute */
      ppd_group_t	*group;		/* Option group */
      ppd_option_t	*option;	/* Option */
      ppd_coption_t	*coption;	/* Custom option */
      ppd_cparam_t	*cparam;	/* Custom parameter */
      char		lang[255];	/* LANG environment variable */


      if (argc > 2)
      {
        snprintf(lang, sizeof(lang), "LANG=%s", argv[2]);
	putenv(lang);
      }

      ppdLocalize(ppd);

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
	    printf("        - %s (%s)\n", option->choices[k].choice,
		   option->choices[k].text);

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

      puts("Attributes:");

      for (attr = (ppd_attr_t *)cupsArrayFirst(ppd->sorted_attrs);
           attr;
	   attr = (ppd_attr_t *)cupsArrayNext(ppd->sorted_attrs))
        printf("    *%s %s/%s: \"%s\"\n", attr->name, attr->spec,
	       attr->text, attr->value ? attr->value : "");
    }
  }

  return (status);
}


/*
 * End of "$Id$".
 */
