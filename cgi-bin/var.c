/*
 * "$Id: var.c,v 1.20.2.3 2003/01/07 18:26:20 mike Exp $"
 *
 *   CGI form variable and array functions.
 *
 *   Copyright 1997-2003 by Easy Software Products.
 *
 *   This program is free software; you can redistribute it and/or modify it
 *   under the terms of the GNU General Public License as published by the Free
 *   Software Foundation; either version 2 of the License, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 *   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *   for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Contents:
 *
 *   cgiInitialize()         - Initialize the CGI variable "database"...
 *   cgiCheckVariables()     - Check for the presence of "required" variables.
 *   cgiGetArray()           - Get an element from a form array...
 *   cgiGetSize()            - Get the size of a form array value.
 *   cgiGetVariable()        - Get a CGI variable from the database...
 *   cgiSetArray()           - Set array element N to the specified string.
 *   cgiSetVariable()        - Set a CGI variable in the database...
 *   cgi_add_variable()      - Add a form variable.
 *   cgi_compare_variables() - Compare two variables.
 *   cgi_find_variable()     - Find a variable...
 *   cgi_initialize_get()    - Initialize form variables using the GET method.
 *   cgi_initialize_post()   - Initialize variables using the POST method.
 *   cgi_initialize_string() - Initialize form variables from a string.
 *   cgi_sort_variables()    - Sort all form variables for faster lookup.
 */

/*#define DEBUG*/
#include "cgi.h"
#include <errno.h>
#include <syslog.h>


/*
 * Data structure to hold all the CGI form variables and arrays...
 */

typedef struct
{
  const char	*name;		/* Name of variable */
  int		nvalues,	/* Number of values */
		avalues;	/* Number of values allocated */
  const char	**values;	/* Value(s) of variable */
} var_t;


/*
 * Local globals...
 */

static int	form_count = 0,		/* Form variable count */
		form_alloc = 0;		/* Number of variables allocated */
static var_t	*form_vars = NULL;	/* Form variables */


/*
 * Local functions...
 */

static void	cgi_add_variable(const char *name, int element,
		                 const char *value);
static int	cgi_compare_variables(const var_t *v1, const var_t *v2);
static var_t	*cgi_find_variable(const char *name);
static int	cgi_initialize_get(void);
static int	cgi_initialize_post(void);
static int	cgi_initialize_string(const char *data);
static void	cgi_sort_variables(void);


/*
 * 'cgiInitialize()' - Initialize the CGI variable "database"...
 */

int			/* O - Non-zero if there was form data */
cgiInitialize(void)
{
  char	*method;	/* Form posting method */
 

#ifdef DEBUG
  setbuf(stdout, NULL);
  puts("Content-type: text/plain\n");
#endif /* DEBUG */

  method = getenv("REQUEST_METHOD");

  if (method == NULL)
    return (0);

  if (strcasecmp(method, "GET") == 0)
    return (cgi_initialize_get());
  else if (strcasecmp(method, "POST") == 0)
    return (cgi_initialize_post());
  else
    return (0);
}


/*
 * 'cgiCheckVariables()' - Check for the presence of "required" variables.
 *
 * Names may be separated by spaces and/or commas.
 */

int					/* O - 1 if all variables present, 0 otherwise */
cgiCheckVariables(const char *names)	/* I - Variables to look for */
{
  char		name[255],		/* Current variable name */
		*s;			/* Pointer in string */
  const char	*val;			/* Value of variable */
  int		element;		/* Array element number */


  if (names == NULL)
    return (1);

  while (*names != '\0')
  {
    while (*names == ' ' || *names == ',')
      names ++;

    for (s = name; *names != '\0' && *names != ' ' && *names != ','; s ++, names ++)
      *s = *names;

    *s = 0;
    if (name[0] == '\0')
      break;

    if ((s = strrchr(name, '-')) != NULL)
    {
      *s      = '\0';
      element = atoi(s + 1) - 1;
      val     = cgiGetArray(name, element);
    }
    else
      val = cgiGetVariable(name);

    if (val == NULL)
      return (0);

    if (*val == '\0')
      return (0);	/* Can't be blank, either! */
  }

  return (1);
}


/*
 * 'cgiGetArray()' - Get an element from a form array...
 */

const char *			/* O - Element value or NULL */
cgiGetArray(const char *name,	/* I - Name of array variable */
            int        element)	/* I - Element number (0 to N) */
{
  var_t	*var;			/* Pointer to variable */


  if ((var = cgi_find_variable(name)) == NULL)
    return (NULL);

  if (var->nvalues == 1)
    return (var->values[0]);

  if (element < 0 || element >= var->nvalues)
    return (NULL);

  return (var->values[element]);
}


/*
 * 'cgiGetSize()' - Get the size of a form array value.
 */

int				/* O - Number of elements */
cgiGetSize(const char *name)	/* I - Name of variable */
{
  var_t	*var;			/* Pointer to variable */


  if ((var = cgi_find_variable(name)) == NULL)
    return (0);

  return (var->nvalues);
}


/*
 * 'cgiGetVariable()' - Get a CGI variable from the database...
 *
 * Returns NULL if the variable doesn't exist...  If the variable is an
 * array of values, returns the last element...
 */

const char *			/* O - Value of variable */
cgiGetVariable(const char *name)/* I - Name of variable */
{
  const var_t	*var;		/* Returned variable */


  var = cgi_find_variable(name);

#ifdef DEBUG
  if (var == NULL)
    printf("cgiGetVariable(\"%s\") is returning NULL...\n", name);
  else
    printf("cgiGetVariable(\"%s\") is returning \"%s\"...\n", name,
           var->values[var->nvalues - 1]);
#endif /* DEBUG */

  return ((var == NULL) ? NULL : var->values[var->nvalues - 1]);
}


/*
 * 'cgiSetArray()' - Set array element N to the specified string.
 *
 * If the variable array is smaller than (element + 1), the intervening
 * elements are set to NULL.
 */

void
cgiSetArray(const char *name,	/* I - Name of variable */
            int        element,	/* I - Element number (0 to N) */
            const char *value)	/* I - Value of variable */
{
  int	i;	/* Looping var */
  var_t	*var;	/* Returned variable */


  if (name == NULL || value == NULL || element < 0 || element > 100000)
    return;

  if ((var = cgi_find_variable(name)) == NULL)
  {
    cgi_add_variable(name, element, value);
    cgi_sort_variables();
  }
  else
  {
    if (element >= var->avalues)
    {
      var->avalues = element + 16;
      var->values  = (const char **)realloc((void *)(var->values),
                                            sizeof(char *) * var->avalues);
    }

    if (element >= var->nvalues)
    {
      for (i = var->nvalues; i < element; i ++)
	var->values[i] = NULL;

      var->nvalues = element + 1;
    }
    else if (var->values[element])
      free((char *)var->values[element]);

    var->values[element] = strdup(value);
  }
}


/*
 * 'cgiSetSize()' - Set the array size.
 */

void
cgiSetSize(const char *name,	/* I - Name of variable */
           int        size)	/* I - Number of elements (0 to N) */
{
  int	i;	/* Looping var */
  var_t	*var;	/* Returned variable */


  if (name == NULL || size < 0 || size > 100000)
    return;

  if ((var = cgi_find_variable(name)) == NULL)
    return;

  if (size >= var->avalues)
  {
    var->avalues = size + 16;
    var->values  = (const char **)realloc((void *)(var->values),
                                          sizeof(char *) * var->avalues);
  }

  if (size > var->nvalues)
  {
    for (i = var->nvalues; i < size; i ++)
      var->values[i] = NULL;
  }
  else if (size < var->nvalues)
  {
    for (i = size; i < var->nvalues; i ++)
      if (var->values[i])
        free((void *)(var->values[i]));
  }

  var->nvalues = size;
}


/*
 * 'cgiSetVariable()' - Set a CGI variable in the database...
 *
 * If the variable is an array, this truncates the array to a single element.
 */

void
cgiSetVariable(const char *name,	/* I - Name of variable */
               const char *value)	/* I - Value of variable */
{
  int	i;				/* Looping var */
  var_t	*var;				/* Returned variable */


  if (name == NULL || value == NULL)
    return;

  if ((var = cgi_find_variable(name)) == NULL)
  {
    cgi_add_variable(name, 0, value);
    cgi_sort_variables();
  }
  else
  {
    for (i = 0; i < var->nvalues; i ++)
      if (var->values[i])
        free((char *)var->values[i]);

    var->values[0] = strdup(value);
    var->nvalues   = 1;
  }
}


/*
 * 'cgi_add_variable()' - Add a form variable.
 */

static void
cgi_add_variable(const char *name,	/* I - Variable name */
		 int        element,	/* I - Array element number */
                 const char *value)	/* I - Variable value */
{
  var_t	*var;				/* New variable */


  if (name == NULL || value == NULL || element < 0 || element > 100000)
    return;

#ifdef DEBUG
  printf("Adding variable \'%s\' with value \'%s\'...\n", name, value);
#endif /* DEBUG */

  if (form_count >= form_alloc)
  {
    if (form_alloc == 0)
      form_vars = malloc(sizeof(var_t) * 16);
    else
      form_vars = realloc(form_vars, (form_alloc + 16) * sizeof(var_t));

    form_alloc += 16;
  }

  var                  = form_vars + form_count;
  var->name            = strdup(name);
  var->nvalues         = element + 1;
  var->avalues         = element + 1;
  var->values          = calloc(element + 1, sizeof(char *));
  var->values[element] = strdup(value);

  form_count ++;
}


/*
 * 'cgi_compare_variables()' - Compare two variables.
 */

static int				/* O - Result of comparison */
cgi_compare_variables(const var_t *v1,	/* I - First variable */
                      const var_t *v2)	/* I - Second variable */
{
  return (strcasecmp(v1->name, v2->name));
}


/*
 * 'cgi_find_variable()' - Find a variable...
 */

static var_t *				/* O - Variable pointer or NULL */
cgi_find_variable(const char *name)	/* I - Name of variable */
{
  var_t	key;				/* Search key */


  if (form_count < 1 || name == NULL)
    return (NULL);

  key.name = name;

  return ((var_t *)bsearch(&key, form_vars, form_count, sizeof(var_t),
                           (int (*)(const void *, const void *))cgi_compare_variables));
}


/*
 * 'cgi_initialize_get()' - Initialize form variables using the GET method.
 */

static int		/* O - 1 if form data read */
cgi_initialize_get(void)
{
  char	*data;		/* Pointer to form data string */


#ifdef DEBUG
  puts("Initializing variables using GET method...");
#endif /* DEBUG */

 /*
  * Check to see if there is anything for us to read...
  */

  data = getenv("QUERY_STRING");
  if (data == NULL || strlen(data) == 0)
    return (0);

 /*
  * Parse it out and return...
  */

  return (cgi_initialize_string(data));
}


/*
 * 'cgi_initialize_post()' - Initialize variables using the POST method.
 */

static int			/* O - 1 if form data was read */
cgi_initialize_post(void)
{
  char	*content_length,	/* Length of input data (string) */
	*data;			/* Pointer to form data string */
  int	length,			/* Length of input data */
	nbytes,			/* Number of bytes read this read() */
	tbytes,			/* Total number of bytes read */
	status;			/* Return status */


#ifdef DEBUG
  puts("Initializing variables using POST method...");
#endif /* DEBUG */

 /*
  * Check to see if there is anything for us to read...
  */

  content_length = getenv("CONTENT_LENGTH");
  if (content_length == NULL || atoi(content_length) <= 0)
    return (0);

 /*
  * Get the length of the input stream and allocate a buffer for it...
  */

  length = atoi(content_length);
  data   = malloc(length + 1);

  if (data == NULL)
    return (0);

 /*
  * Read the data into the buffer...
  */

  for (tbytes = 0; tbytes < length; tbytes += nbytes)
    if ((nbytes = read(0, data + tbytes, length - tbytes)) < 0)
      if (errno != EAGAIN)
      {
        free(data);
        return (0);
      }

  data[length] = '\0';

 /*
  * Parse it out...
  */

  status = cgi_initialize_string(data);

 /*
  * Free the data and return...
  */

  free(data);

  return (status);
}


/*
 * 'cgi_initialize_string()' - Initialize form variables from a string.
 */

static int
cgi_initialize_string(const char *data)	/* I - Form data string */
{
  int	done;		/* True if we're done reading a form variable */
  char	*s,		/* Pointer to current form string */
	ch,		/* Temporary character */
	name[255],	/* Name of form variable */
	value[65536];	/* Variable value... */


 /*
  * Check input...
  */

  if (data == NULL)
    return (0);

 /*
  * Loop until we've read all the form data...
  */

  while (*data != '\0')
  {
   /*
    * Get the variable name...
    */

    for (s = name; *data != '\0'; data ++)
      if (*data == '=')
        break;
      else if (*data >= ' ' && s < (name + sizeof(name) - 1))
        *s++ = *data;

    *s = '\0';
    if (*data == '=')
      data ++;
    else
      return (0);

   /*
    * Read the variable value...
    */

    for (s = value, done = 0; !done && *data != '\0'; data ++)
      switch (*data)
      {
	case '&' :	/* End of data... */
            done = 1;
            break;

	case '+' :	/* Escaped space character */
            if (s < (value + sizeof(value) - 1))
              *s++ = ' ';
            break;

	case '%' :	/* Escaped control character */
	   /*
	    * Read the hex code...
	    */

            if (s < (value + sizeof(value) - 1))
	    {
              data ++;
              ch = *data - '0';
              if (ch > 9)
        	ch -= 7;
              *s = ch << 4;

              data ++;
              ch = *data - '0';
              if (ch > 9)
        	ch -= 7;
              *s++ |= ch;
            }
	    else
	      data += 2;
            break;

	default :	/* Other characters come straight through */
	    if (*data >= ' ' && s < (value + sizeof(value) - 1))
              *s++ = *data;
            break;
      }

    *s = '\0';		/* nul terminate the string */

   /*
    * Remove trailing whitespace...
    */

    if (s > value)
      s --;

    while (s >= value && *s == ' ')
      *s-- = '\0';

   /*
    * Add the string to the variable "database"...
    */

    if ((s = strrchr(name, '-')) != NULL && isdigit(s[1]))
    {
      *s++ = '\0';
      if (value[0])
        cgiSetArray(name, atoi(s) - 1, value);
    }
    else if (cgiGetVariable(name) != NULL)
      cgiSetArray(name, cgiGetSize(name), value);
    else
      cgiSetVariable(name, value);
  }

  return (1);
}


/*
 * 'cgi_sort_variables()' - Sort all form variables for faster lookup.
 */

static void
cgi_sort_variables(void)
{
#ifdef DEBUG
  int	i;


  puts("Sorting variables...");
#endif /* DEBUG */

  if (form_count < 2)
    return;

  qsort(form_vars, form_count, sizeof(var_t),
        (int (*)(const void *, const void *))cgi_compare_variables);

#ifdef DEBUG
  puts("Sorted variable list is:");
  for (i = 0; i < form_count; i ++)
    printf("%d: %s (%d) = \"%s\" ...\n", i, form_vars[i].name,
           form_vars[i].nvalues, form_vars[i].values[0]);
#endif /* DEBUG */
}


/*
 * End of "$Id: var.c,v 1.20.2.3 2003/01/07 18:26:20 mike Exp $".
 */
