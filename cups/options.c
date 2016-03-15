/*
 * "$Id: options.c 11558 2014-02-06 18:33:34Z msweet $"
 *
 * Option routines for CUPS.
 *
 * Copyright 2007-2014 by Apple Inc.
 * Copyright 1997-2007 by Easy Software Products.
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

#include "cups-private.h"


/*
 * Local functions...
 */

static int	cups_compare_options(cups_option_t *a, cups_option_t *b);
static int	cups_find_option(const char *name, int num_options,
	                         cups_option_t *option, int prev, int *rdiff);


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
  cups_option_t	*temp;			/* Pointer to new option */
  int		insert,			/* Insertion point */
		diff;			/* Result of search */


  DEBUG_printf(("2cupsAddOption(name=\"%s\", value=\"%s\", num_options=%d, "
                "options=%p)", name, value, num_options, options));

  if (!name || !name[0] || !value || !options || num_options < 0)
  {
    DEBUG_printf(("3cupsAddOption: Returning %d", num_options));
    return (num_options);
  }

 /*
  * Look for an existing option with the same name...
  */

  if (num_options == 0)
  {
    insert = 0;
    diff   = 1;
  }
  else
  {
    insert = cups_find_option(name, num_options, *options, num_options - 1,
                              &diff);

    if (diff > 0)
      insert ++;
  }

  if (diff)
  {
   /*
    * No matching option name...
    */

    DEBUG_printf(("4cupsAddOption: New option inserted at index %d...",
                  insert));

    if (num_options == 0)
      temp = (cups_option_t *)malloc(sizeof(cups_option_t));
    else
      temp = (cups_option_t *)realloc(*options, sizeof(cups_option_t) * (size_t)(num_options + 1));

    if (!temp)
    {
      DEBUG_puts("3cupsAddOption: Unable to expand option array, returning 0");
      return (0);
    }

    *options = temp;

    if (insert < num_options)
    {
      DEBUG_printf(("4cupsAddOption: Shifting %d options...",
                    (int)(num_options - insert)));
      memmove(temp + insert + 1, temp + insert, (size_t)(num_options - insert) * sizeof(cups_option_t));
    }

    temp        += insert;
    temp->name  = _cupsStrAlloc(name);
    num_options ++;
  }
  else
  {
   /*
    * Match found; free the old value...
    */

    DEBUG_printf(("4cupsAddOption: Option already exists at index %d...",
                  insert));

    temp = *options + insert;
    _cupsStrFree(temp->value);
  }

  temp->value = _cupsStrAlloc(value);

  DEBUG_printf(("3cupsAddOption: Returning %d", num_options));

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


  DEBUG_printf(("cupsFreeOptions(num_options=%d, options=%p)", num_options,
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
  int	diff,				/* Result of comparison */
	match;				/* Matching index */


  DEBUG_printf(("2cupsGetOption(name=\"%s\", num_options=%d, options=%p)",
                name, num_options, options));

  if (!name || num_options <= 0 || !options)
  {
    DEBUG_puts("3cupsGetOption: Returning NULL");
    return (NULL);
  }

  match = cups_find_option(name, num_options, options, -1, &diff);

  if (!diff)
  {
    DEBUG_printf(("3cupsGetOption: Returning \"%s\"", options[match].value));
    return (options[match].value);
  }

  DEBUG_puts("3cupsGetOption: Returning NULL");
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
	sep,				/* Separator character */
	quote;				/* Quote character */


  DEBUG_printf(("cupsParseOptions(arg=\"%s\", num_options=%d, options=%p)",
                arg, num_options, options));

 /*
  * Range check input...
  */

  if (!arg)
  {
    DEBUG_printf(("1cupsParseOptions: Returning %d", num_options));
    return (num_options);
  }

  if (!options || num_options < 0)
  {
    DEBUG_puts("1cupsParseOptions: Returning 0");
    return (0);
  }

 /*
  * Make a copy of the argument string and then divide it up...
  */

  if ((copyarg = strdup(arg)) == NULL)
  {
    DEBUG_puts("1cupsParseOptions: Unable to copy arg string");
    DEBUG_printf(("1cupsParseOptions: Returning %d", num_options));
    return (num_options);
  }

  if (*copyarg == '{')
  {
   /*
    * Remove surrounding {} so we can parse "{name=value ... name=value}"...
    */

    if ((ptr = copyarg + strlen(copyarg) - 1) > copyarg && *ptr == '}')
    {
      *ptr = '\0';
      ptr  = copyarg + 1;
    }
    else
      ptr = copyarg;
  }
  else
    ptr = copyarg;

 /*
  * Skip leading spaces...
  */

  while (_cups_isspace(*ptr))
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
    while (!strchr("\f\n\r\t\v =", *ptr) && *ptr)
      ptr ++;

   /*
    * Avoid an empty name...
    */

    if (ptr == name)
      break;

   /*
    * Skip trailing spaces...
    */

    while (_cups_isspace(*ptr))
      *ptr++ = '\0';

    if ((sep = *ptr) == '=')
      *ptr++ = '\0';

    DEBUG_printf(("2cupsParseOptions: name=\"%s\"", name));

    if (sep != '=')
    {
     /*
      * Boolean option...
      */

      if (!_cups_strncasecmp(name, "no", 2))
        num_options = cupsAddOption(name + 2, "false", num_options,
	                            options);
      else
        num_options = cupsAddOption(name, "true", num_options, options);

      continue;
    }

   /*
    * Remove = and parse the value...
    */

    value = ptr;

    while (*ptr && !_cups_isspace(*ptr))
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

	while (*ptr && !_cups_isspace(*ptr))
	{
	  if (*ptr == '\\' && ptr[1])
	    _cups_strcpy(ptr, ptr + 1);

	  ptr ++;
	}
      }
    }

    if (*ptr != '\0')
      *ptr++ = '\0';

    DEBUG_printf(("2cupsParseOptions: value=\"%s\"", value));

   /*
    * Skip trailing whitespace...
    */

    while (_cups_isspace(*ptr))
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

  DEBUG_printf(("1cupsParseOptions: Returning %d", num_options));

  return (num_options);
}


/*
 * 'cupsRemoveOption()' - Remove an option from an option array.
 *
 * @since CUPS 1.2/OS X 10.5@
 */

int					/* O  - New number of options */
cupsRemoveOption(
    const char    *name,		/* I  - Option name */
    int           num_options,		/* I  - Current number of options */
    cups_option_t **options)		/* IO - Options */
{
  int		i;			/* Looping var */
  cups_option_t	*option;		/* Current option */


  DEBUG_printf(("2cupsRemoveOption(name=\"%s\", num_options=%d, options=%p)",
                name, num_options, options));

 /*
  * Range check input...
  */

  if (!name || num_options < 1 || !options)
  {
    DEBUG_printf(("3cupsRemoveOption: Returning %d", num_options));
    return (num_options);
  }

 /*
  * Loop for the option...
  */

  for (i = num_options, option = *options; i > 0; i --, option ++)
    if (!_cups_strcasecmp(name, option->name))
      break;

  if (i)
  {
   /*
    * Remove this option from the array...
    */

    DEBUG_puts("4cupsRemoveOption: Found option, removing it...");

    num_options --;
    i --;

    _cupsStrFree(option->name);
    _cupsStrFree(option->value);

    if (i > 0)
      memmove(option, option + 1, (size_t)i * sizeof(cups_option_t));
  }

 /*
  * Return the new number of options...
  */

  DEBUG_printf(("3cupsRemoveOption: Returning %d", num_options));
  return (num_options);
}


/*
 * '_cupsGet1284Values()' - Get 1284 device ID keys and values.
 *
 * The returned dictionary is a CUPS option array that can be queried with
 * cupsGetOption and freed with cupsFreeOptions.
 */

int					/* O - Number of key/value pairs */
_cupsGet1284Values(
    const char *device_id,		/* I - IEEE-1284 device ID string */
    cups_option_t **values)		/* O - Array of key/value pairs */
{
  int		num_values;		/* Number of values */
  char		key[256],		/* Key string */
		value[256],		/* Value string */
		*ptr;			/* Pointer into key/value */


 /*
  * Range check input...
  */

  if (values)
    *values = NULL;

  if (!device_id || !values)
    return (0);

 /*
  * Parse the 1284 device ID value into keys and values.  The format is
  * repeating sequences of:
  *
  *   [whitespace]key:value[whitespace];
  */

  num_values = 0;
  while (*device_id)
  {
    while (_cups_isspace(*device_id))
      device_id ++;

    if (!*device_id)
      break;

    for (ptr = key; *device_id && *device_id != ':'; device_id ++)
      if (ptr < (key + sizeof(key) - 1))
        *ptr++ = *device_id;

    if (!*device_id)
      break;

    while (ptr > key && _cups_isspace(ptr[-1]))
      ptr --;

    *ptr = '\0';
    device_id ++;

    while (_cups_isspace(*device_id))
      device_id ++;

    if (!*device_id)
      break;

    for (ptr = value; *device_id && *device_id != ';'; device_id ++)
      if (ptr < (value + sizeof(value) - 1))
        *ptr++ = *device_id;

    if (!*device_id)
      break;

    while (ptr > value && _cups_isspace(ptr[-1]))
      ptr --;

    *ptr = '\0';
    device_id ++;

    num_values = cupsAddOption(key, value, num_values, values);
  }

  return (num_values);
}


/*
 * 'cups_compare_options()' - Compare two options.
 */

static int				/* O - Result of comparison */
cups_compare_options(cups_option_t *a,	/* I - First option */
		     cups_option_t *b)	/* I - Second option */
{
  return (_cups_strcasecmp(a->name, b->name));
}


/*
 * 'cups_find_option()' - Find an option using a binary search.
 */

static int				/* O - Index of match */
cups_find_option(
    const char    *name,		/* I - Option name */
    int           num_options,		/* I - Number of options */
    cups_option_t *options,		/* I - Options */
    int           prev,			/* I - Previous index */
    int           *rdiff)		/* O - Difference of match */
{
  int		left,			/* Low mark for binary search */
		right,			/* High mark for binary search */
		current,		/* Current index */
		diff;			/* Result of comparison */
  cups_option_t	key;			/* Search key */


  DEBUG_printf(("7cups_find_option(name=\"%s\", num_options=%d, options=%p, "
	        "prev=%d, rdiff=%p)", name, num_options, options, prev,
		rdiff));

#ifdef DEBUG
  for (left = 0; left < num_options; left ++)
    DEBUG_printf(("9cups_find_option: options[%d].name=\"%s\", .value=\"%s\"",
                  left, options[left].name, options[left].value));
#endif /* DEBUG */

  key.name = (char *)name;

  if (prev >= 0)
  {
   /*
    * Start search on either side of previous...
    */

    if ((diff = cups_compare_options(&key, options + prev)) == 0 ||
        (diff < 0 && prev == 0) ||
	(diff > 0 && prev == (num_options - 1)))
    {
      *rdiff = diff;
      return (prev);
    }
    else if (diff < 0)
    {
     /*
      * Start with previous on right side...
      */

      left  = 0;
      right = prev;
    }
    else
    {
     /*
      * Start wih previous on left side...
      */

      left  = prev;
      right = num_options - 1;
    }
  }
  else
  {
   /*
    * Start search in the middle...
    */

    left  = 0;
    right = num_options - 1;
  }

  do
  {
    current = (left + right) / 2;
    diff    = cups_compare_options(&key, options + current);

    if (diff == 0)
      break;
    else if (diff < 0)
      right = current;
    else
      left = current;
  }
  while ((right - left) > 1);

  if (diff != 0)
  {
   /*
    * Check the last 1 or 2 elements...
    */

    if ((diff = cups_compare_options(&key, options + left)) <= 0)
      current = left;
    else
    {
      diff    = cups_compare_options(&key, options + right);
      current = right;
    }
  }

 /*
  * Return the closest destination and the difference...
  */

  *rdiff = diff;

  return (current);
}


/*
 * End of "$Id: options.c 11558 2014-02-06 18:33:34Z msweet $".
 */
