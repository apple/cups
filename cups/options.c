/*
 * "$Id: options.c,v 1.21.2.4 2002/01/29 02:29:46 mike Exp $"
 *
 *   Option routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2002 by Easy Software Products.
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
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 *   cupsAddOption()    - Add an option to an option array.
 *   cupsFreeOptions()  - Free all memory used by options.
 *   cupsGetOption()    - Get an option value.
 *   cupsParseOptions() - Parse options from a command-line argument.
 *   cupsMarkOptions()  - Mark command-line options in a PPD file.
 */

/*
 * Include necessary headers...
 */

#include "cups.h"
#include <stdlib.h>
#include <ctype.h>
#include "string.h"
#include "debug.h"


/*
 * 'cupsAddOption()' - Add an option to an option array.
 */

int						/* O - Number of options */
cupsAddOption(const char    *name,		/* I - Name of option */
              const char    *value,		/* I - Value of option */
	      int           num_options,	/* I - Number of options */
              cups_option_t **options)		/* IO - Pointer to options */
{
  int		i;				/* Looping var */
  cups_option_t	*temp;				/* Pointer to new option */


  if (name == NULL || !name[0] || value == NULL ||
      options == NULL || num_options < 0)
    return (num_options);

 /*
  * Look for an existing option with the same name...
  */

  for (i = 0, temp = *options; i < num_options; i ++, temp ++)
    if (strcasecmp(temp->name, name) == 0)
      break;

  if (i >= num_options)
  {
   /*
    * No matching option name...
    */

    if (num_options == 0)
      temp = (cups_option_t *)malloc(sizeof(cups_option_t));
    else
      temp = (cups_option_t *)realloc(*options, sizeof(cups_option_t) *
                                        	(num_options + 1));

    if (temp == NULL)
      return (0);

    *options    = temp;
    temp        += num_options;
    temp->name  = strdup(name);
    num_options ++;
  }
  else
  {
   /*
    * Match found; free the old value...
    */

    free(temp->value);
  }

  temp->value = strdup(value);

  return (num_options);
}


/*
 * 'cupsFreeOptions()' - Free all memory used by options.
 */

void
cupsFreeOptions(int           num_options,	/* I - Number of options */
                cups_option_t *options)		/* I - Pointer to options */
{
  int	i;					/* Looping var */


  if (num_options <= 0 || options == NULL)
    return;

  for (i = 0; i < num_options; i ++)
  {
    free(options[i].name);
    free(options[i].value);
  }

  free(options);
}


/*
 * 'cupsGetOption()' - Get an option value.
 */

const char *				/* O - Option value or NULL */
cupsGetOption(const char    *name,	/* I - Name of option */
              int           num_options,/* I - Number of options */
              cups_option_t *options)	/* I - Options */
{
  int	i;				/* Looping var */


  if (name == NULL || num_options <= 0 || options == NULL)
    return (NULL);

  for (i = 0; i < num_options; i ++)
    if (strcasecmp(options[i].name, name) == 0)
      return (options[i].value);

  return (NULL);
}


/*
 * 'cupsParseOptions()' - Parse options from a command-line argument.
 */

int						/* O - Number of options found */
cupsParseOptions(const char    *arg,		/* I - Argument to parse */
                 int           num_options,	/* I - Number of options */
                 cups_option_t **options)	/* O - Options found */
{
  char	*copyarg,				/* Copy of input string */
	*ptr,					/* Pointer into string */
	*name,					/* Pointer to name */
	*value;					/* Pointer to value */


  if (arg == NULL || options == NULL || num_options < 0)
    return (0);

 /*
  * Make a copy of the argument string and then divide it up...
  */

  copyarg     = strdup(arg);
  ptr         = copyarg;

 /*
  * Skip leading spaces...
  */

  while (isspace(*ptr))
    ptr ++;

 /*
  * Loop through the string...
  */

  while (*ptr != '\0')
  {
   /*
    * Get the name up to a SPACE, =, or end-of-string...
    */

    name = ptr;
    while (!isspace(*ptr) && *ptr != '=' && *ptr != '\0')
      ptr ++;

   /*
    * Avoid an empty name...
    */

    if (ptr == name)
      break;

   /*
    * Skip trailing spaces...
    */

    while (isspace(*ptr))
      *ptr++ = '\0';

    if (*ptr != '=')
    {
     /*
      * Start of another option...
      */

      if (strncasecmp(name, "no", 2) == 0)
        num_options = cupsAddOption(name + 2, "false", num_options,
	                            options);
      else
        num_options = cupsAddOption(name, "true", num_options, options);

      continue;
    }

   /*
    * Remove = and parse the value...
    */

    *ptr++ = '\0';

    if (*ptr == '\'')
    {
     /*
      * Quoted string constant...
      */

      ptr ++;
      value = ptr;

      while (*ptr != '\'' && *ptr != '\0')
        ptr ++;

      if (*ptr != '\0')
        *ptr++ = '\0';
    }
    else if (*ptr == '\"')
    {
     /*
      * Double-quoted string constant...
      */

      ptr ++;
      value = ptr;

      while (*ptr != '\"' && *ptr != '\0')
        ptr ++;

      if (*ptr != '\0')
        *ptr++ = '\0';
    }
    else
    {
     /*
      * Normal space-delimited string...
      */

      value = ptr;

      while (!isspace(*ptr) && *ptr != '\0')
	ptr ++;
    }

   /*
    * Skip trailing whitespace...
    */

    while (isspace(*ptr))
      *ptr++ = '\0';

   /*
    * Add the string value...
    */

    num_options = cupsAddOption(name, value, num_options, options);
  }

 /*
  * Free the copy of the argument we made and return the number of options
  * found.
  */

  free(copyarg);

  return (num_options);
}


/*
 * 'cupsMarkOptions()' - Mark command-line options in a PPD file.
 */

int						/* O - 1 if conflicting */
cupsMarkOptions(ppd_file_t    *ppd,		/* I - PPD file */
                int           num_options,	/* I - Number of options */
                cups_option_t *options)		/* I - Options */
{
  int	i;					/* Looping var */
  int	conflict;				/* Option conflicts */
  char	*val,					/* Pointer into value */
	*ptr,					/* Pointer into string */
	s[255];					/* Temporary string */


 /*
  * Check arguments...
  */

  if (ppd == NULL || num_options <= 0 || options == NULL)
    return (0);

 /*
  * Mark options...
  */

  conflict = 0;

  for (i = num_options; i > 0; i --, options ++)
    if (strcasecmp(options->name, "media") == 0)
    {
     /*
      * Loop through the option string, separating it at commas and
      * marking each individual option.
      */

      for (val = options->value; *val;)
      {
       /*
        * Extract the sub-option from the string...
	*/

        for (ptr = s; *val && *val != ',' && (ptr - s) < (sizeof(s) - 1);)
	  *ptr++ = *val++;
	*ptr++ = '\0';

	if (*val == ',')
	  val ++;

       /*
        * Mark it...
	*/

	if (ppdMarkOption(ppd, "PageSize", s))
          conflict = 1;
	if (ppdMarkOption(ppd, "InputSlot", s))
          conflict = 1;
	if (ppdMarkOption(ppd, "MediaType", s))
          conflict = 1;
	if (ppdMarkOption(ppd, "EFMediaQualityMode", s))	/* EFI */
          conflict = 1;
	if (strcasecmp(s, "manual") == 0)
          if (ppdMarkOption(ppd, "ManualFeed", "True"))
	    conflict = 1;
      }
    }
    else if (strcasecmp(options->name, "sides") == 0)
    {
      if (strcasecmp(options->value, "one-sided") == 0)
      {
        if (ppdMarkOption(ppd, "Duplex", "None"))
	  conflict = 1;
        if (ppdMarkOption(ppd, "JCLDuplex", "None"))	/* Samsung */
	  conflict = 1;
        if (ppdMarkOption(ppd, "EFDuplex", "None"))	/* EFI */
	  conflict = 1;
        if (ppdMarkOption(ppd, "KD03Duplex", "None"))	/* Kodak */
	  conflict = 1;
      }
      else if (strcasecmp(options->value, "two-sided-long-edge") == 0)
      {
        if (ppdMarkOption(ppd, "Duplex", "DuplexNoTumble"))
	  conflict = 1;
        if (ppdMarkOption(ppd, "JCLDuplex", "DuplexNoTumble"))	/* Samsung */
	  conflict = 1;
        if (ppdMarkOption(ppd, "EFDuplex", "DuplexNoTumble"))	/* EFI */
	  conflict = 1;
        if (ppdMarkOption(ppd, "KD03Duplex", "DuplexNoTumble"))	/* Kodak */
	  conflict = 1;
      }
      else if (strcasecmp(options->value, "two-sided-short-edge") == 0)
      {
        if (ppdMarkOption(ppd, "Duplex", "DuplexTumble"))
	  conflict = 1;
        if (ppdMarkOption(ppd, "JCLDuplex", "DuplexTumble"))	/* Samsung */
	  conflict = 1;
        if (ppdMarkOption(ppd, "EFDuplex", "DuplexTumble"))	/* EFI */
	  conflict = 1;
        if (ppdMarkOption(ppd, "KD03Duplex", "DuplexTumble"))	/* Kodak */
	  conflict = 1;
      }
    }
    else if (strcasecmp(options->name, "resolution") == 0 ||
             strcasecmp(options->name, "printer-resolution") == 0)
    {
      if (ppdMarkOption(ppd, "Resolution", options->value))
        conflict = 1;
      if (ppdMarkOption(ppd, "SetResolution", options->value))
      	/* Calcomp, Linotype, QMS, Summagraphics, Tektronix, Varityper */
        conflict = 1;
      if (ppdMarkOption(ppd, "JCLResolution", options->value))	/* HP */
        conflict = 1;
      if (ppdMarkOption(ppd, "CNRes_PGP", options->value))	/* Canon */
        conflict = 1;
    }
    else if (strcasecmp(options->name, "output-bin") == 0)
    {
      if (ppdMarkOption(ppd, "OutputBin", options->value))
        conflict = 1;
    }
    else if (ppdMarkOption(ppd, options->name, options->value))
      conflict = 1;

  return (conflict);
}


/*
 * End of "$Id: options.c,v 1.21.2.4 2002/01/29 02:29:46 mike Exp $".
 */
