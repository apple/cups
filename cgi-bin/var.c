/*
 * "$Id: var.c,v 1.8 1999/07/11 12:56:42 mike Exp $"
 *
 *   CGI form variable functions.
 *
 *   Copyright 1997 by Easy Software Products, All Rights Reserved.
 *
 * Contents:
 *
 *   cgiInitialize()         - Initialize the CGI variable "database"...
 *   cgiCheckVariables()     - Check for the presence of "required" variables.
 *   cgiGetVariable()        - Get a CGI variable from the database...
 *   cgiSetVariable()        - Set a CGI variable in the database...
 *   cgi_sort_variables()    - Sort all form variables for faster lookup.
 *   cgi_compare_variables() - Compare two variables.
 *   cgi_add_variable()      - Add a form variable.
 *   cgi_initialize_string() - Initialize form variables from a string.
 *   cgi_initialize_get()    - Initialize form variables using the GET method.
 *   cgi_initialize_post()   - Initialize variables using the POST method.
 *
 * Revision History:
 *
 *   $Log: var.c,v $
 *   Revision 1.8  1999/07/11 12:56:42  mike
 *   Checkin for CVS.
 *
 *   Revision 1.7  1997/09/02  18:46:51  mike
 *   OK, now we check for errors in the form data.
 *   cgiInitialize() returns 0 if there was no form data or an error occured
 *   and 1 if form data is present.
 *
 *   Revision 1.6  1997/08/26  16:49:24  mike
 *   Updated cgiInitialize() to always return FALSE if no parameter data
 *   is present.
 *
 *   Revision 1.5  1997/05/13  16:38:07  mike
 *   Fixed a bug in the cgi_initialize_string() code - was not sorting vars!
 *
 *   Revision 1.4  1997/05/13  15:16:30  mike
 *   Updated cgiCheckVariables() to see if the variable is blank.
 *
 *   Revision 1.3  1997/05/13  14:56:37  mike
 *   Added cgiCheckVariables() function to check for required variables.
 *
 *   Revision 1.2  1997/05/08  20:14:19  mike
 *   Renamed CGI_Name functions to cgiName functions.
 *   Updated documentation.
 *
 *   Revision 1.1  1997/05/08  19:55:53  mike
 *   Initial revision
 */

/*#define DEBUG*/
#include "cgi.h"


/*
 * Data structure to hold all the CGI form variables...
 */

typedef struct
{
  char	*name,	/* Name of variable */
	*value;	/* Value of variable */
} var_t;


/*
 * Local globals...
 */

static int	form_count = 0;		/* Form variable count */
static var_t	*form_vars = NULL;	/* Form variables */


/*
 * Local functions...
 */

static void	cgi_sort_variables(void);
static int	cgi_compare_variables(var_t *v1, var_t *v2);
static void	cgi_add_variable(char *name, char *value);
static int	cgi_initialize_string(char *data);
static int	cgi_initialize_get(int need_content);
static int	cgi_initialize_post(int need_content);


/*
 * 'cgiInitialize()' - Initialize the CGI variable "database"...
 */

int
cgiInitialize(int need_content)	/* I - True if input is required */
{
  char	*method;		/* Form posting method */
 

#ifdef DEBUG
  setbuf(stdout, NULL);
  puts("Content-type: text/plain\n");
#endif /* DEBUG */

  method = getenv("REQUEST_METHOD");

  if (method == NULL)
    return (0);

  if (strcasecmp(method, "GET") == 0)
    return (cgi_initialize_get(need_content));
  else if (strcasecmp(method, "POST") == 0)
    return (cgi_initialize_post(need_content));
  else
    return (0);
}


/*
 * 'cgiCheckVariables()' - Check for the presence of "required" variables.
 *
 * Returns 1 if all variables are present, 0 otherwise.  Name may be separated
 * by spaces and/or commas.
 */

int
cgiCheckVariables(char *names)	/* I - Variables to look for */
{
  char	name[255],	/* Current variable name */
	*s,		/* Pointer in string */
	*val;		/* Value of variable */


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

    if ((val = cgiGetVariable(name)) == NULL)
      return (0);

    if (*val == '\0')
      return (0);	/* Can't be blank, either! */
  };

  return (1);
}


/*
 * 'cgiGetVariable()' - Get a CGI variable from the database...
 *
 * Returns NULL if the variable doesn't exist...
 */

char *
cgiGetVariable(char *name)	/* I - Name of variable */
{
  var_t	key,	/* Search key */
	*var;	/* Returned variable */


  if (form_count < 1)
    return (NULL);

  key.name = name;

  var = bsearch(&key, form_vars, form_count, sizeof(var_t),
                (int (*)(const void *, const void *))cgi_compare_variables);

#ifdef DEBUG
  if (var == NULL)
    printf("cgiGetVariable(\"%s\") is returning NULL...\n", name);
  else
    printf("cgiGetVariable(\"%s\") is returning \"%s\"...\n", name, var->value);
#endif /* DEBUG */

  return ((var == NULL) ? NULL : var->value);
}


/*
 * 'cgiSetVariable()' - Set a CGI variable in the database...
 */

void
cgiSetVariable(char *name,	/* I - Name of variable */
               char *value)	/* I - Value of variable */
{
  var_t	key,	/* Search key */
	*var;	/* Returned variable */


  if (name == NULL || value == NULL)
    return;

  if (form_count > 0)
  {
    key.name = name;

    var = bsearch(&key, form_vars, form_count, sizeof(var_t),
                  (int (*)(const void *, const void *))cgi_compare_variables);
  }
  else
    var = NULL;

  if (var == NULL)
  {
    cgi_add_variable(name, value);
    cgi_sort_variables();
  }
  else
  {
    free(var->value);
    var->value = strdup(value);
  };
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
  puts("New variable list is:");
  for (i = 0; i < form_count; i ++)
    printf("%s = %s\n", form_vars[i].name, form_vars[i].value);
#endif /* DEBUG */
}


/*
 * 'cgi_compare_variables()' - Compare two variables.
 */

static int
cgi_compare_variables(var_t *v1,	/* I - First variable */
                      var_t *v2)	/* I - Second variable */
{
  return (strcasecmp(v1->name, v2->name));
}


/*
 * 'cgi_add_variable()' - Add a form variable.
 */

static void
cgi_add_variable(char *name,		/* I - Variable name */
                 char *value)		/* I - Variable value */
{
  var_t	*var;


  if (name == NULL || value == NULL)
    return;

#ifdef DEBUG
  printf("Adding variable \'%s\' with value \'%s\'...\n", name, value);
#endif /* DEBUG */

  if (form_count == 0)
    form_vars = malloc(sizeof(var_t));
  else
    form_vars = realloc(form_vars, (form_count + 1) * sizeof(var_t));

  var        = form_vars + form_count;
  var->name  = strdup(name);
  var->value = strdup(value);
  form_count ++;
}


/*
 * 'cgi_initialize_string()' - Initialize form variables from a string.
 */

static int
cgi_initialize_string(char *data)	/* I - Form data string */
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

    for (s = name; *data != '\0'; data ++, s ++)
      if (*data == '=')
        break;
      else
        *s = *data;

    *s = '\0';
    if (*data == '=')
      data ++;
    else
      return (0);

   /*
    * Read the variable value...
    */

    for (s = value, done = 0; !done && *data != '\0'; data ++, s ++)
      switch (*data)
      {
	case '&' :	/* End of data... */
            done = 1;
            s --;
            break;

	case '+' :	/* Escaped space character */
            *s = ' ';
            break;

	case '%' :	/* Escaped control character */
	   /*
	    * Read the hex code from stdin...
	    */

            data ++;
            ch = *data - '0';
            if (ch > 9)
              ch -= 7;
            *s = ch << 4;

            data ++;
            ch = *data - '0';
            if (ch > 9)
              ch -= 7;
            *s |= ch;
            break;

	default :	/* Other characters come straight through */
            *s = *data;
            break;
      };

    *s = '\0';		/* nul terminate the string */

   /*
    * Remove trailing whitespace...
    */

    s --;
    while (s >= value && *s == ' ')
      *s-- = '\0';

   /*
    * Add the string to the variable "database"...
    */

    cgi_add_variable(name, value);
  };

  cgi_sort_variables();

  return (1);
}


/*
 * 'cgi_initialize_get()' - Initialize form variables using the GET method.
 */

static int
cgi_initialize_get(int need_content)	/* I - True if input is required */
{
  char	*data;	/* Pointer to form data string */


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

static int
cgi_initialize_post(int need_content)	/* I - True if input is required */
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
  if (content_length == NULL || atoi(content_length) == 0)
    return (0);

 /*
  * Get the length of the input stream and allocate a buffer for it...
  */

  length = atoi(content_length);
  data   = malloc(length + 1);

 /*
  * Read the data into the buffer...
  */

  for (tbytes = 0; tbytes < length; tbytes += nbytes)
    if ((nbytes = read(0, data + tbytes, length - tbytes)) < 0)
    {
      free(data);
      return (0);
    };

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
 * End of "$Id: var.c,v 1.8 1999/07/11 12:56:42 mike Exp $".
 */
