/*
 * "$Id$"
 *
 *   Option routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007-2008 by Apple Inc.
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
 *   cupsAddOption()    - Add an option to an option array.
 *   cupsFreeOptions()  - Free all memory used by options.
 *   cupsGetOption()    - Get an option value.
 *   cupsParseOptions() - Parse options from a command-line argument.
 *   cupsRemoveOption() - Remove an option from an option array.
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
 *
 * New option arrays can be initialized simply by passing 0 for the
 * "num_options" parameter.
 */

int					/* O  - Number of options */
cupsAddOption(const char    *name,	/* I  - Name of option */
              const char    *value,	/* I  - Value of option */
	      int           num_options,/* I  - Number of options */
              cups_option_t **options)	/* IO - Pointer to options */
{
  int		i;			/* Looping var */
  cups_option_t	*temp;			/* Pointer to new option */


  DEBUG_printf(("cupsAddOption(name=\"%s\", value=\"%s\", num_options=%d, "
                "options=%p)\n", name, value, num_options, options));
 
  if (!name || !name[0] || !value || !options || num_options < 0)
  {
    DEBUG_printf(("cupsAddOption: Returning %d\n", num_options));
    return (num_options);
  }

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

    DEBUG_puts("cupsAddOption: New option...");

    if (num_options == 0)
      temp = (cups_option_t *)malloc(sizeof(cups_option_t));
    else
      temp = (cups_option_t *)realloc(*options, sizeof(cups_option_t) *
                                        	(num_options + 1));

    if (temp == NULL)
    {
      DEBUG_puts("cupsAddOption: Unable to expand option array, returning 0");
      return (0);
    }

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

    DEBUG_puts("cupsAddOption: Option already exists...");
    _cupsStrFree(temp->value);
  }

  temp->value = _cupsStrAlloc(value);

  DEBUG_printf(("cupsAddOption: Returning %d\n", num_options));

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


  DEBUG_printf(("cupsFreeOptions(num_options=%d, options=%p)\n", num_options,
                options));

  if (num_options <= 0 || !options)
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

const char *				/* O - Option value or @code NULL@ */
cupsGetOption(const char    *name,	/* I - Name of option */
              int           num_options,/* I - Number of options */
              cups_option_t *options)	/* I - Options */
{
  int	i;				/* Looping var */


  DEBUG_printf(("cupsGetOption(name=\"%s\", num_options=%d, options=%p)\n",
                name, num_options, options));

  if (!name || num_options <= 0 || !options)
  {
    DEBUG_puts("cupsGetOption: Returning NULL");
    return (NULL);
  }

  for (i = 0; i < num_options; i ++)
    if (!strcasecmp(options[i].name, name))
    {
      DEBUG_printf(("cupsGetOption: Returning \"%s\"\n", options[i].value));
      return (options[i].value);
    }

  DEBUG_puts("cupsGetOption: Returning NULL");
  return (NULL);
}


/*
 * 'cupsParseOptions()' - Parse options from a command-line argument.
 *
 * This function converts space-delimited name/value pairs according
 * to the PAPI text option ABNF specification. Collection values
 * ("name={a=... b=... c=...}") are stored with the curley brackets
 * intact - use @code cupsParseOptions@ on the value to extract the
 * collection attributes.
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
	*value,				/* Pointer to value */
	quote;				/* Quote character */


  DEBUG_printf(("cupsParseOptions(arg=\"%s\", num_options=%d, options=%p)\n",
                arg, num_options, options));

 /*
  * Range check input...
  */

  if (!arg)
  {
    DEBUG_printf(("cupsParseOptions: Returning %d\n", num_options));
    return (num_options);
  }

  if (!options || num_options < 0)
  {
    DEBUG_puts("cupsParseOptions: Returning 0");
    return (0);
  }

 /*
  * Make a copy of the argument string and then divide it up...
  */

  if ((copyarg = strdup(arg)) == NULL)
  {
    DEBUG_puts("cupsParseOptions: Unable to copy arg string");
    DEBUG_printf(("cupsParseOptions: Returning %d\n", num_options));
    return (num_options);
  }

  ptr = copyarg;

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
    while (!isspace(*ptr & 255) && *ptr != '=' && *ptr)
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

    DEBUG_printf(("cupsParseOptions: name=\"%s\"\n", name));

    if (*ptr != '=')
    {
     /*
      * Boolean option...
      */

      if (!strncasecmp(name, "no", 2))
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
    value  = ptr;

    while (*ptr && !isspace(*ptr & 255))
    {
      if (*ptr == ',')
        ptr ++;
      else if (*ptr == '\'' || *ptr == '\"')
      {
       /*
	* Quoted string constant...
	*/

	quote = *ptr;
	_cups_strcpy(ptr, ptr + 1);

	while (*ptr != quote && *ptr)
	{
	  if (*ptr == '\\' && ptr[1])
	    _cups_strcpy(ptr, ptr + 1);

	  ptr ++;
	}

	if (*ptr)
	  _cups_strcpy(ptr, ptr + 1);
      }
      else if (*ptr == '{')
      {
       /*
	* Collection value...
	*/

	int depth;

	for (depth = 0; *ptr; ptr ++)
	{
	  if (*ptr == '{')
	    depth ++;
	  else if (*ptr == '}')
	  {
	    depth --;
	    if (!depth)
	    {
	      ptr ++;
	      break;
	    }
	  }
	  else if (*ptr == '\\' && ptr[1])
	    _cups_strcpy(ptr, ptr + 1);
	}
      }
      else
      {
       /*
	* Normal space-delimited string...
	*/

	while (!isspace(*ptr & 255) && *ptr)
	{
	  if (*ptr == '\\' && ptr[1])
	    _cups_strcpy(ptr, ptr + 1);

	  ptr ++;
	}
      }
    }

    if (*ptr != '\0')
      *ptr++ = '\0';

    DEBUG_printf(("cupsParseOptions: value=\"%s\"\n", value));

   /*
    * Skip trailing whitespace...
    */

    while (isspace(*ptr & 255))
      ptr ++;

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

  DEBUG_printf(("cupsParseOptions: Returning %d\n", num_options));

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


  DEBUG_printf(("cupsRemoveOption(name=\"%s\", num_options=%d, options=%p)\n",
                name, num_options, options));

 /*
  * Range check input...
  */

  if (!name || num_options < 1 || !options)
  {
    DEBUG_printf(("cupsRemoveOption: Returning %d\n", num_options));
    return (num_options);
  }

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

    DEBUG_puts("cupsRemoveOption: Found option, removing it...");

    num_options --;
    i --;

    _cupsStrFree(option->name);
    _cupsStrFree(option->value);

    if (i > 0)
      memmove(option, option + 1, i * sizeof(cups_option_t));
  }

 /*
  * Return the new number of options...
  */

  DEBUG_printf(("cupsRemoveOption: Returning %d\n", num_options));
  return (num_options);
}


/*
 * End of "$Id$".
 */
