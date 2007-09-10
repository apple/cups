/*
 * "$Id$"
 *
 *   Option routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   cupsAddOption()     - Add an option to an option array.
 *   cupsFreeOptions()   - Free all memory used by options.
 *   cupsGetOption()     - Get an option value.
 *   cupsMarkOptions()   - Mark command-line options in a PPD file.
 *   cupsParseOptions()  - Parse options from a command-line argument.
 *   cupsRemoveOptions() - Remove an option from an option array.
 *   debug_marked()      - Output the marked array to stdout...
 *   ppd_mark_choices()  - Mark one or more option choices from a string.
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
 * Local functions...
 */

#ifdef DEBUG
static void	debug_marked(ppd_file_t *ppd, const char *title);
#else
#  define debug_marked(ppd,title)
#endif /* DEBUG */
static int	ppd_mark_choices(ppd_file_t *ppd, const char *options);


/*
 * 'cupsAddOption()' - Add an option to an option array.
 */

int					/* O - Number of options */
cupsAddOption(const char    *name,	/* I - Name of option */
              const char    *value,	/* I - Value of option */
	      int           num_options,/* I - Number of options */
              cups_option_t **options)	/* IO - Pointer to options */
{
  int		i;			/* Looping var */
  cups_option_t	*temp;			/* Pointer to new option */


  if (name == NULL || !name[0] || value == NULL ||
      options == NULL || num_options < 0)
    return (num_options);

 /*
  * Look for an existing option with the same name...
  */

  for (i = 0, temp = *options; i < num_options; i ++, temp ++)
    if (!strcasecmp(temp->name, name))
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
    temp->name  = _cupsStrAlloc(name);
    num_options ++;
  }
  else
  {
   /*
    * Match found; free the old value...
    */

    _cupsStrFree(temp->value);
  }

  temp->value = _cupsStrAlloc(value);

  return (num_options);
}


/*
 * 'cupsFreeOptions()' - Free all memory used by options.
 */

void
cupsFreeOptions(
    int           num_options,		/* I - Number of options */
    cups_option_t *options)		/* I - Pointer to options */
{
  int	i;				/* Looping var */


  if (num_options <= 0 || options == NULL)
    return;

  for (i = 0; i < num_options; i ++)
  {
    _cupsStrFree(options[i].name);
    _cupsStrFree(options[i].value);
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
 * 'cupsMarkOptions()' - Mark command-line options in a PPD file.
 */

int					/* O - 1 if conflicting */
cupsMarkOptions(
    ppd_file_t    *ppd,			/* I - PPD file */
    int           num_options,		/* I - Number of options */
    cups_option_t *options)		/* I - Options */
{
  int		i, j, k;		/* Looping vars */
  int		conflict;		/* Option conflicts */
  char		*val,			/* Pointer into value */
		*ptr,			/* Pointer into string */
		s[255];			/* Temporary string */
  const char	*page_size;		/* PageSize option */
  cups_option_t	*optptr;		/* Current option */
  ppd_option_t	*option;		/* PPD option */
  ppd_attr_t	*attr;			/* PPD attribute */
  static const char * const duplex_options[] =
		{			/* Duplex option names */
		  "Duplex",		/* Adobe */
		  "EFDuplex",		/* EFI */
		  "EFDuplexing",	/* EFI */
		  "KD03Duplex",		/* Kodak */
		  "JCLDuplex"		/* Samsung */
		};
  static const char * const duplex_one[] =
		{			/* one-sided names */
		  "None",
		  "False"
		};
  static const char * const duplex_two_long[] =
		{			/* two-sided-long-edge names */
		  "DuplexNoTumble",	/* Adobe */
		  "LongEdge",		/* EFI */
		  "Top"			/* EFI */
		};
  static const char * const duplex_two_short[] =
		{			/* two-sided-long-edge names */
		  "DuplexTumble",	/* Adobe */
		  "ShortEdge",		/* EFI */
		  "Bottom"		/* EFI */
		};


 /*
  * Check arguments...
  */

  if (ppd == NULL || num_options <= 0 || options == NULL)
    return (0);

  debug_marked(ppd, "Before...");

 /*
  * Mark options...
  */

  conflict  = 0;

  for (i = num_options, optptr = options; i > 0; i --, optptr ++)
    if (!strcasecmp(optptr->name, "media"))
    {
     /*
      * Loop through the option string, separating it at commas and
      * marking each individual option as long as the corresponding
      * PPD option (PageSize, InputSlot, etc.) is not also set.
      *
      * For PageSize, we also check for an empty option value since
      * some versions of MacOS X use it to specify auto-selection
      * of the media based solely on the size.
      */

      page_size = cupsGetOption("PageSize", num_options, options);

      for (val = optptr->value; *val;)
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

        if (!page_size || !page_size[0])
	  if (ppdMarkOption(ppd, "PageSize", s))
            conflict = 1;

        if (cupsGetOption("InputSlot", num_options, options) == NULL)
	  if (ppdMarkOption(ppd, "InputSlot", s))
            conflict = 1;

        if (cupsGetOption("MediaType", num_options, options) == NULL)
	  if (ppdMarkOption(ppd, "MediaType", s))
            conflict = 1;

        if (cupsGetOption("EFMediaType", num_options, options) == NULL)
	  if (ppdMarkOption(ppd, "EFMediaType", s))		/* EFI */
            conflict = 1;

        if (cupsGetOption("EFMediaQualityMode", num_options, options) == NULL)
	  if (ppdMarkOption(ppd, "EFMediaQualityMode", s))	/* EFI */
            conflict = 1;

	if (strcasecmp(s, "manual") == 0 &&
	    cupsGetOption("ManualFeed", num_options, options) == NULL)
          if (ppdMarkOption(ppd, "ManualFeed", "True"))
	    conflict = 1;
      }
    }
    else if (!strcasecmp(optptr->name, "sides"))
    {
      for (j = 0; j < (int)(sizeof(duplex_options) / sizeof(duplex_options[0])); j ++)
        if (cupsGetOption(duplex_options[j], num_options, options) != NULL)
	  break;

      if (j < (int)(sizeof(duplex_options) / sizeof(duplex_options[0])))
      {
       /*
        * Don't override the PPD option with the IPP attribute...
	*/

        continue;
      }

      if (!strcasecmp(optptr->value, "one-sided"))
      {
       /*
        * Mark the appropriate duplex option for one-sided output...
	*/

        for (j = 0; j < (int)(sizeof(duplex_options) / sizeof(duplex_options[0])); j ++)
	  if ((option = ppdFindOption(ppd, duplex_options[j])) != NULL)
	    break;

	if (j < (int)(sizeof(duplex_options) / sizeof(duplex_options[0])))
	{
          for (k = 0; k < (int)(sizeof(duplex_one) / sizeof(duplex_one[0])); k ++)
            if (ppdFindChoice(option, duplex_one[k]))
	    {
	      if (ppdMarkOption(ppd, duplex_options[j], duplex_one[k]))
		conflict = 1;

	      break;
            }
        }
      }
      else if (!strcasecmp(optptr->value, "two-sided-long-edge"))
      {
       /*
        * Mark the appropriate duplex option for two-sided-long-edge output...
	*/

        for (j = 0; j < (int)(sizeof(duplex_options) / sizeof(duplex_options[0])); j ++)
	  if ((option = ppdFindOption(ppd, duplex_options[j])) != NULL)
	    break;

	if (j < (int)(sizeof(duplex_options) / sizeof(duplex_options[0])))
	{
          for (k = 0; k < (int)(sizeof(duplex_two_long) / sizeof(duplex_two_long[0])); k ++)
            if (ppdFindChoice(option, duplex_two_long[k]))
	    {
	      if (ppdMarkOption(ppd, duplex_options[j], duplex_two_long[k]))
		conflict = 1;

	      break;
            }
        }
      }
      else if (!strcasecmp(optptr->value, "two-sided-short-edge"))
      {
       /*
        * Mark the appropriate duplex option for two-sided-short-edge output...
	*/

        for (j = 0; j < (int)(sizeof(duplex_options) / sizeof(duplex_options[0])); j ++)
	  if ((option = ppdFindOption(ppd, duplex_options[j])) != NULL)
	    break;

	if (j < (int)(sizeof(duplex_options) / sizeof(duplex_options[0])))
	{
          for (k = 0; k < (int)(sizeof(duplex_two_short) / sizeof(duplex_two_short[0])); k ++)
            if (ppdFindChoice(option, duplex_two_short[k]))
	    {
	      if (ppdMarkOption(ppd, duplex_options[j], duplex_two_short[k]))
		conflict = 1;

	      break;
            }
        }
      }
    }
    else if (!strcasecmp(optptr->name, "resolution") ||
             !strcasecmp(optptr->name, "printer-resolution"))
    {
      if (ppdMarkOption(ppd, "Resolution", optptr->value))
        conflict = 1;
      if (ppdMarkOption(ppd, "SetResolution", optptr->value))
      	/* Calcomp, Linotype, QMS, Summagraphics, Tektronix, Varityper */
        conflict = 1;
      if (ppdMarkOption(ppd, "JCLResolution", optptr->value))	/* HP */
        conflict = 1;
      if (ppdMarkOption(ppd, "CNRes_PGP", optptr->value))	/* Canon */
        conflict = 1;
    }
    else if (!strcasecmp(optptr->name, "output-bin"))
    {
      if (!cupsGetOption("OutputBin", num_options, options))
        if (ppdMarkOption(ppd, "OutputBin", optptr->value))
          conflict = 1;
    }
    else if (!strcasecmp(optptr->name, "multiple-document-handling"))
    {
      if (!cupsGetOption("Collate", num_options, options) &&
          ppdFindOption(ppd, "Collate"))
      {
        if (strcasecmp(optptr->value, "separate-documents-uncollated-copies"))
	{
	  if (ppdMarkOption(ppd, "Collate", "True"))
            conflict = 1;
        }
	else
	{
	  if (ppdMarkOption(ppd, "Collate", "False"))
            conflict = 1;
        }
      }
    }
    else if (!strcasecmp(optptr->name, "finishings"))
    {
     /*
      * Lookup cupsIPPFinishings attributes for each value...
      */

      for (ptr = optptr->value; *ptr;)
      {
       /*
        * Get the next finishings number...
	*/

        if (!isdigit(*ptr & 255))
	  break;

        if ((j = strtol(ptr, &ptr, 10)) < 3)
	  break;

       /*
        * Skip separator as needed...
	*/

        if (*ptr == ',')
	  ptr ++;

       /*
        * Look it up in the PPD file...
	*/

	sprintf(s, "%d", j);

        if ((attr = ppdFindAttr(ppd, "cupsIPPFinishings", s)) == NULL)
	  continue;

       /*
        * Apply "*Option Choice" settings from the attribute value...
	*/

        if (ppd_mark_choices(ppd, attr->value))
	  conflict = 1;
      }
    }
    else if (!strcasecmp(optptr->name, "mirror"))
    {
      if (ppdMarkOption(ppd, "MirrorPrint", optptr->value))
	conflict = 1;
    }
    else if (ppdMarkOption(ppd, optptr->name, optptr->value))
      conflict = 1;

  debug_marked(ppd, "After...");

  return (conflict);
}


/*
 * 'cupsParseOptions()' - Parse options from a command-line argument.
 *
 * This function converts space-delimited name/value pairs according
 * to the PAPI text option ABNF specification. Collection values
 * ("name={a=... b=... c=...}") are stored with the curley brackets
 * intact - use cupsParseOptions() on the value to extract the collection
 * attributes.
 */

int					/* O - Number of options found */
cupsParseOptions(
    const char    *arg,			/* I - Argument to parse */
    int           num_options,		/* I - Number of options */
    cups_option_t **options)		/* O - Options found */
{
  char	*copyarg,			/* Copy of input string */
	*ptr,				/* Pointer into string */
	*name,				/* Pointer to name */
	*value;				/* Pointer to value */


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

  while (isspace(*ptr & 255))
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
    while (!isspace(*ptr & 255) && *ptr != '=' && *ptr != '\0')
      ptr ++;

   /*
    * Avoid an empty name...
    */

    if (ptr == name)
      break;

   /*
    * Skip trailing spaces...
    */

    while (isspace(*ptr & 255))
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
      {
        if (*ptr == '\\')
	  _cups_strcpy(ptr, ptr + 1);

        ptr ++;
      }

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
      {
        if (*ptr == '\\')
	  _cups_strcpy(ptr, ptr + 1);

        ptr ++;
      }

      if (*ptr != '\0')
        *ptr++ = '\0';
    }
    else if (*ptr == '{')
    {
     /*
      * Collection value...
      */

      int depth;

      value = ptr;

      for (depth = 1; *ptr; ptr ++)
        if (*ptr == '{')
	  depth ++;
	else if (*ptr == '}')
	{
	  depth --;
	  if (!depth)
	  {
	    ptr ++;

	    if (*ptr != ',')
	      break;
	  }
        }
        else if (*ptr == '\\')
	  _cups_strcpy(ptr, ptr + 1);

      if (*ptr != '\0')
        *ptr++ = '\0';
    }
    else
    {
     /*
      * Normal space-delimited string...
      */

      value = ptr;

      while (!isspace(*ptr & 255) && *ptr != '\0')
      {
        if (*ptr == '\\')
	  _cups_strcpy(ptr, ptr + 1);

        ptr ++;
      }
    }

   /*
    * Skip trailing whitespace...
    */

    while (isspace(*ptr & 255))
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
 * 'cupsRemoveOption()' - Remove an option from an option array.
 *
 * @since CUPS 1.2@
 */

int					/* O  - New number of options */
cupsRemoveOption(
    const char    *name,		/* I  - Option name */
    int           num_options,		/* I  - Current number of options */
    cups_option_t **options)		/* IO - Options */
{
  int		i;			/* Looping var */
  cups_option_t	*option;		/* Current option */


 /*
  * Range check input...
  */

  if (!name || num_options < 1 || !options)
    return (num_options);

 /*
  * Loop for the option...
  */

  for (i = num_options, option = *options; i > 0; i --, option ++)
    if (!strcasecmp(name, option->name))
      break;

  if (i)
  {
   /*
    * Remove this option from the array...
    */

    num_options --;
    i --;

    _cupsStrFree(option->name);
    if (option->value)
      _cupsStrFree(option->value);

    if (i > 0)
      memmove(option, option + 1, i * sizeof(cups_option_t));
  }

 /*
  * Return the new number of options...
  */

  return (num_options);
}


#ifdef DEBUG
/*
 * 'debug_marked()' - Output the marked array to stdout...
 */

static void
debug_marked(ppd_file_t *ppd,		/* I - PPD file data */
             const char *title)		/* I - Title for list */
{
  ppd_choice_t	*c;			/* Current choice */


  printf("cupsMarkOptions: %s\n", title);

  for (c = (ppd_choice_t *)cupsArrayFirst(ppd->marked);
       c;
       c = (ppd_choice_t *)cupsArrayNext(ppd->marked))
    printf("cupsMarkOptions: %s=%s\n", c->option->keyword, c->choice);
}
#endif /* DEBUG */


/*
 * 'ppd_mark_choices()' - Mark one or more option choices from a string.
 */

static int				/* O - 1 if there are conflicts, 0 otherwise */
ppd_mark_choices(ppd_file_t *ppd,	/* I - PPD file */
                 const char *options)	/* I - "*Option Choice ..." string */
{
  char	option[PPD_MAX_NAME],		/* Current option */
	choice[PPD_MAX_NAME],		/* Current choice */
	*ptr;				/* Pointer into option or choice */
  int	conflict = 0;			/* Do we have a conflict? */


  if (!options)
    return (0);

 /*
  * Read all of the "*Option Choice" pairs from the string, marking PPD
  * options as we go...
  */

  while (*options)
  {
   /*
    * Skip leading whitespace...
    */

    while (isspace(*options & 255))
      options ++;

    if (*options != '*')
      break;

   /*
    * Get the option name...
    */

    options ++;
    ptr = option;
    while (*options && !isspace(*options & 255) &&
	       ptr < (option + sizeof(option) - 1))
      *ptr++ = *options++;

    if (ptr == option)
      break;

    *ptr = '\0';

   /*
    * Get the choice...
    */

    while (isspace(*options & 255))
      options ++;

    if (!*options)
      break;

    ptr = choice;
    while (*options && !isspace(*options & 255) &&
	       ptr < (choice + sizeof(choice) - 1))
      *ptr++ = *options++;

    *ptr = '\0';

   /*
    * Mark the option...
    */

    if (ppdMarkOption(ppd, option, choice))
      conflict = 1;
  }

 /*
  * Return whether we had any conflicts...
  */

  return (conflict);
}


/*
 * End of "$Id$".
 */
