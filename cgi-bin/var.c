/*
 * CGI form variable and array functions for CUPS.
 *
 * Copyright © 2007-2019 by Apple Inc.
 * Copyright © 1997-2005 by Easy Software Products.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * missing or damaged, see the license at "http://www.cups.org/".
 */

/*
 * Include necessary headers...
 */

/*#define DEBUG*/
#include "cgi-private.h"
#include <cups/http.h>


/*
 * Session ID name
 */

#define CUPS_SID	"org.cups.sid"


/*
 * Data structure to hold all the CGI form variables and arrays...
 */

typedef struct				/**** Form variable structure ****/
{
  char		*name;			/* Name of variable */
  int		nvalues,		/* Number of values */
		avalues;		/* Number of values allocated */
  char		**values;		/* Value(s) of variable */
} _cgi_var_t;


/*
 * Local globals...
 */

static int		num_cookies = 0;/* Number of cookies */
static cups_option_t	*cookies = NULL;/* Cookies */
static int		form_count = 0,	/* Form variable count */
			form_alloc = 0;	/* Number of variables allocated */
static _cgi_var_t	*form_vars = NULL;
					/* Form variables */
static cgi_file_t	*form_file = NULL;
					/* Uploaded file */


/*
 * Local functions...
 */

static void		cgi_add_variable(const char *name, int element,
			                 const char *value);
static int		cgi_compare_variables(const _cgi_var_t *v1,
			                      const _cgi_var_t *v2);
static _cgi_var_t	*cgi_find_variable(const char *name);
static void		cgi_initialize_cookies(void);
static int		cgi_initialize_get(void);
static int		cgi_initialize_multipart(const char *boundary);
static int		cgi_initialize_post(void);
static int		cgi_initialize_string(const char *data);
static const char	*cgi_passwd(const char *prompt);
static const char	*cgi_set_sid(void);
static void		cgi_sort_variables(void);
static void		cgi_unlink_file(void);


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
 * 'cgiClearVariables()' - Clear all form variables.
 */

void
cgiClearVariables(void)
{
  int		i, j;			/* Looping vars */
  _cgi_var_t	*v;			/* Current variable */


  fputs("DEBUG: cgiClearVariables called.\n", stderr);

  for (v = form_vars, i = form_count; i > 0; v ++, i --)
  {
    free(v->name);
    for (j = 0; j < v->nvalues; j ++)
      if (v->values[j])
        free(v->values[j]);
  }

  form_count = 0;

  cgi_unlink_file();
}


/*
 * 'cgiGetArray()' - Get an element from a form array.
 */

const char *				/* O - Element value or NULL */
cgiGetArray(const char *name,		/* I - Name of array variable */
            int        element)		/* I - Element number (0 to N) */
{
  _cgi_var_t	*var;			/* Pointer to variable */


  if ((var = cgi_find_variable(name)) == NULL)
    return (NULL);

  if (element < 0 || element >= var->nvalues)
    return (NULL);

  return (strdup(var->values[element]));
}


/*
 * 'cgiGetCookie()' - Get a cookie value.
 */

const char *				/* O - Value or NULL */
cgiGetCookie(const char *name)		/* I - Name of cookie */
{
  return (cupsGetOption(name, num_cookies, cookies));
}


/*
 * 'cgiGetFile()' - Get the file (if any) that was submitted in the form.
 */

const cgi_file_t *			/* O - Attached file or NULL */
cgiGetFile(void)
{
  return (form_file);
}


/*
 * 'cgiGetSize()' - Get the size of a form array value.
 */

int					/* O - Number of elements */
cgiGetSize(const char *name)		/* I - Name of variable */
{
  _cgi_var_t	*var;			/* Pointer to variable */


  if ((var = cgi_find_variable(name)) == NULL)
    return (0);

  return (var->nvalues);
}


/*
 * 'cgiGetVariable()' - Get a CGI variable from the database.
 *
 * Returns NULL if the variable doesn't exist.  If the variable is an
 * array of values, returns the last element.
 */

const char *				/* O - Value of variable */
cgiGetVariable(const char *name)	/* I - Name of variable */
{
  const _cgi_var_t	*var;		/* Returned variable */


  var = cgi_find_variable(name);

#ifdef DEBUG
  if (var == NULL)
    DEBUG_printf(("cgiGetVariable(\"%s\") is returning NULL...\n", name));
  else
    DEBUG_printf(("cgiGetVariable(\"%s\") is returning \"%s\"...\n", name,
		  var->values[var->nvalues - 1]));
#endif /* DEBUG */

  return ((var == NULL) ? NULL : strdup(var->values[var->nvalues - 1]));
}


/*
 * 'cgiInitialize()' - Initialize the CGI variable "database".
 */

int					/* O - Non-zero if there was form data */
cgiInitialize(void)
{
  const char	*method,		/* Form posting method */
		*content_type,		/* Content-Type of post data */
		*cups_sid_cookie,	/* SID cookie */
		*cups_sid_form;		/* SID form variable */


 /*
  * Setup a password callback for authentication...
  */

  cupsSetPasswordCB(cgi_passwd);

 /*
  * Set the locale so that times, etc. are formatted properly...
  */

  setlocale(LC_ALL, "");

#ifdef DEBUG
 /*
  * Disable output buffering to find bugs...
  */

  setbuf(stdout, NULL);
#endif /* DEBUG */

 /*
  * Get cookies...
  */

  cgi_initialize_cookies();

  if ((cups_sid_cookie = cgiGetCookie(CUPS_SID)) == NULL)
  {
    fputs("DEBUG: " CUPS_SID " cookie not found, initializing!\n", stderr);
    cups_sid_cookie = cgi_set_sid();
  }

  fprintf(stderr, "DEBUG: " CUPS_SID " cookie is \"%s\"\n", cups_sid_cookie);

 /*
  * Get the request method (GET or POST)...
  */

  method       = getenv("REQUEST_METHOD");
  content_type = getenv("CONTENT_TYPE");
  if (!method)
    return (0);

 /*
  * Grab form data from the corresponding location...
  */

  if (!_cups_strcasecmp(method, "GET"))
    return (cgi_initialize_get());
  else if (!_cups_strcasecmp(method, "POST") && content_type)
  {
    const char *boundary = strstr(content_type, "boundary=");

    if (boundary)
      boundary += 9;

    if (content_type && !strncmp(content_type, "multipart/form-data; ", 21))
    {
      if (!cgi_initialize_multipart(boundary))
        return (0);
    }
    else if (!cgi_initialize_post())
      return (0);

    if ((cups_sid_form = cgiGetVariable(CUPS_SID)) == NULL ||
	strcmp(cups_sid_cookie, cups_sid_form))
    {
      if (cups_sid_form)
	fprintf(stderr, "DEBUG: " CUPS_SID " form variable is \"%s\"\n",
	        cups_sid_form);
      else
	fputs("DEBUG: " CUPS_SID " form variable is not present.\n", stderr);

      cgiClearVariables();
      return (0);
    }
    else
      return (1);
  }
  else
    return (0);
}


/*
 * 'cgiIsPOST()' - Determine whether this page was POSTed.
 */

int					/* O - 1 if POST, 0 if GET */
cgiIsPOST(void)
{
  const char	*method;		/* REQUEST_METHOD environment variable */


  if ((method = getenv("REQUEST_METHOD")) == NULL)
    return (0);
  else
    return (!strcmp(method, "POST"));
}


/*
 * 'cgiSetArray()' - Set array element N to the specified string.
 *
 * If the variable array is smaller than (element + 1), the intervening
 * elements are set to NULL.
 */

void
cgiSetArray(const char *name,		/* I - Name of variable */
            int        element,		/* I - Element number (0 to N) */
            const char *value)		/* I - Value of variable */
{
  int		i;			/* Looping var */
  _cgi_var_t	*var;			/* Returned variable */


  if (name == NULL || value == NULL || element < 0 || element > 100000)
    return;

  fprintf(stderr, "DEBUG: cgiSetArray: %s[%d]=\"%s\"\n", name, element, value);

  if ((var = cgi_find_variable(name)) == NULL)
  {
    cgi_add_variable(name, element, value);
    cgi_sort_variables();
  }
  else
  {
    if (element >= var->avalues)
    {
      char **temp;			/* Temporary pointer */

      temp = (char **)realloc((void *)(var->values), sizeof(char *) * (size_t)(element + 16));
      if (!temp)
        return;

      var->avalues = element + 16;
      var->values  = temp;
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
 * 'cgiSetCookie()' - Set a cookie value.
 */

void
cgiSetCookie(const char *name,		/* I - Name */
             const char *value,		/* I - Value */
             const char *path,		/* I - Path (typically "/") */
	     const char *domain,	/* I - Domain name */
	     time_t     expires,	/* I - Expiration date (0 for session) */
	     int        secure)		/* I - Require SSL */
{
  num_cookies = cupsAddOption(name, value, num_cookies, &cookies);

  printf("Set-Cookie: %s=%s;", name, value);
  if (path)
    printf(" path=%s;", path);
  if (domain)
    printf(" domain=%s;", domain);
  if (expires)
  {
    char	date[256];		/* Date string */

    printf(" expires=%s;", httpGetDateString2(expires, date, sizeof(date)));
  }
  if (secure)
    puts(" httponly; secure;");
  else
    puts(" httponly;");
}


/*
 * 'cgiSetSize()' - Set the array size.
 */

void
cgiSetSize(const char *name,		/* I - Name of variable */
           int        size)		/* I - Number of elements (0 to N) */
{
  int		i;			/* Looping var */
  _cgi_var_t	*var;			/* Returned variable */


  if (name == NULL || size < 0 || size > 100000)
    return;

  if ((var = cgi_find_variable(name)) == NULL)
    return;

  if (size >= var->avalues)
  {
    char **temp;			/* Temporary pointer */

    temp = (char **)realloc((void *)(var->values), sizeof(char *) * (size_t)(size + 16));
    if (!temp)
      return;

    var->avalues = size + 16;
    var->values  = temp;
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
 * 'cgiSetVariable()' - Set a CGI variable in the database.
 *
 * If the variable is an array, this truncates the array to a single element.
 */

void
cgiSetVariable(const char *name,	/* I - Name of variable */
               const char *value)	/* I - Value of variable */
{
  int		i;			/* Looping var */
  _cgi_var_t	*var;			/* Returned variable */


  if (name == NULL || value == NULL)
    return;

  fprintf(stderr, "cgiSetVariable: %s=\"%s\"\n", name, value);

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
  _cgi_var_t	*var;			/* New variable */


  if (name == NULL || value == NULL || element < 0 || element > 100000)
    return;

  DEBUG_printf(("cgi_add_variable: Adding variable \'%s\' with value "
                "\'%s\'...\n", name, value));

  if (form_count >= form_alloc)
  {
    _cgi_var_t	*temp_vars;		/* Temporary form pointer */


    if (form_alloc == 0)
      temp_vars = malloc(sizeof(_cgi_var_t) * 16);
    else
      temp_vars = realloc(form_vars, (size_t)(form_alloc + 16) * sizeof(_cgi_var_t));

    if (!temp_vars)
      return;

    form_vars  = temp_vars;
    form_alloc += 16;
  }

  var = form_vars + form_count;

  if ((var->values = calloc((size_t)element + 1, sizeof(char *))) == NULL)
    return;

  var->name            = strdup(name);
  var->nvalues         = element + 1;
  var->avalues         = element + 1;
  var->values[element] = strdup(value);

  form_count ++;
}


/*
 * 'cgi_compare_variables()' - Compare two variables.
 */

static int				/* O - Result of comparison */
cgi_compare_variables(
    const _cgi_var_t *v1,		/* I - First variable */
    const _cgi_var_t *v2)		/* I - Second variable */
{
  return (_cups_strcasecmp(v1->name, v2->name));
}


/*
 * 'cgi_find_variable()' - Find a variable.
 */

static _cgi_var_t *			/* O - Variable pointer or NULL */
cgi_find_variable(const char *name)	/* I - Name of variable */
{
  _cgi_var_t	key;			/* Search key */


  if (form_count < 1 || name == NULL)
    return (NULL);

  key.name = (char *)name;

  return ((_cgi_var_t *)bsearch(&key, form_vars, (size_t)form_count, sizeof(_cgi_var_t),
                           (int (*)(const void *, const void *))cgi_compare_variables));
}


/*
 * 'cgi_initialize_cookies()' - Initialize cookies.
 */

static void
cgi_initialize_cookies(void)
{
  const char	*cookie;		/* HTTP_COOKIE environment variable */
  char		name[128],		/* Name string */
		value[512],		/* Value string */
		*ptr;			/* Pointer into name/value */


  if ((cookie = getenv("HTTP_COOKIE")) == NULL)
    return;

  while (*cookie)
  {
    int	skip = 0;			/* Skip this cookie? */

   /*
    * Skip leading whitespace...
    */

    while (isspace(*cookie & 255))
      cookie ++;
    if (!*cookie)
      break;

   /*
    * Copy the name...
    */

    for (ptr = name; *cookie && *cookie != '=';)
      if (ptr < (name + sizeof(name) - 1))
      {
        *ptr++ = *cookie++;
      }
      else
      {
        skip = 1;
	cookie ++;
      }

    if (*cookie != '=')
      break;

    *ptr = '\0';
    cookie ++;

   /*
    * Then the value...
    */

    if (*cookie == '\"')
    {
      for (cookie ++, ptr = value; *cookie && *cookie != '\"';)
        if (ptr < (value + sizeof(value) - 1))
	{
	  *ptr++ = *cookie++;
	}
	else
	{
	  skip = 1;
	  cookie ++;
	}

      if (*cookie == '\"')
        cookie ++;
      else
        skip = 1;
    }
    else
    {
      for (ptr = value; *cookie && *cookie != ';';)
        if (ptr < (value + sizeof(value) - 1))
	{
	  *ptr++ = *cookie++;
	}
	else
	{
	  skip = 1;
	  cookie ++;
	}
    }

    if (*cookie == ';')
      cookie ++;
    else if (*cookie)
      skip = 1;

    *ptr = '\0';

   /*
    * Then add the cookie to an array as long as the name doesn't start with
    * "$"...
    */

    if (name[0] != '$' && !skip)
      num_cookies = cupsAddOption(name, value, num_cookies, &cookies);
  }
}


/*
 * 'cgi_initialize_get()' - Initialize form variables using the GET method.
 */

static int				/* O - 1 if form data read */
cgi_initialize_get(void)
{
  char	*data;				/* Pointer to form data string */


  DEBUG_puts("cgi_initialize_get: Initializing variables using GET method...");

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
 * 'cgi_initialize_multipart()' - Initialize variables and file using the POST
 *                                method.
 *
 * TODO: Update to support files > 2GB.
 */

static int				/* O - 1 if form data was read */
cgi_initialize_multipart(
    const char *boundary)		/* I - Boundary string */
{
  char		line[10240],		/* MIME header line */
		name[1024],		/* Form variable name */
		filename[1024],		/* Form filename */
		mimetype[1024],		/* MIME media type */
		bstring[256],		/* Boundary string to look for */
		*ptr,			/* Pointer into name/filename */
		*end;			/* End of buffer */
  int		ch,			/* Character from file */
		fd;			/* Temporary file descriptor */
  size_t	blen;			/* Length of boundary string */


  DEBUG_printf(("cgi_initialize_multipart(boundary=\"%s\")\n", boundary));

 /*
  * Read multipart form data until we run out...
  */

  name[0]     = '\0';
  filename[0] = '\0';
  mimetype[0] = '\0';

  snprintf(bstring, sizeof(bstring), "\r\n--%s", boundary);
  blen = strlen(bstring);

  while (fgets(line, sizeof(line), stdin))
  {
    if (!strcmp(line, "\r\n"))
    {
     /*
      * End of headers, grab value...
      */

      if (filename[0])
      {
       /*
        * Read an embedded file...
	*/

        if (form_file)
	{
	 /*
	  * Remove previous file...
	  */

	  cgi_unlink_file();
	}

       /*
        * Allocate memory for the new file...
	*/

	if ((form_file = calloc(1, sizeof(cgi_file_t))) == NULL)
	  return (0);

        form_file->name     = strdup(name);
	form_file->filename = strdup(filename);
	form_file->mimetype = strdup(mimetype);

        fd = cupsTempFd(form_file->tempfile, sizeof(form_file->tempfile));

        if (fd < 0)
	  return (0);

        atexit(cgi_unlink_file);

       /*
        * Copy file data to the temp file...
	*/

        ptr = line;

	while ((ch = getchar()) != EOF)
	{
	  *ptr++ = (char)ch;

          if ((size_t)(ptr - line) >= blen && !memcmp(ptr - blen, bstring, blen))
	  {
	    ptr -= blen;
	    break;
	  }

          if ((ptr - line - (int)blen) >= 8192)
	  {
	   /*
	    * Write out the first 8k of the buffer...
	    */

	    write(fd, line, 8192);
	    memmove(line, line + 8192, (size_t)(ptr - line - 8192));
	    ptr -= 8192;
	  }
	}

       /*
        * Write the rest of the data and close the temp file...
	*/

	if (ptr > line)
          write(fd, line, (size_t)(ptr - line));

	close(fd);
      }
      else
      {
       /*
        * Just get a form variable; the current code only handles
	* form values up to 10k in size...
	*/

        ptr = line;
	end = line + sizeof(line) - 1;

	while ((ch = getchar()) != EOF)
	{
	  if (ptr < end)
	    *ptr++ = (char)ch;

          if ((size_t)(ptr - line) >= blen && !memcmp(ptr - blen, bstring, blen))
	  {
	    ptr -= blen;
	    break;
	  }
	}

	*ptr = '\0';

       /*
        * Set the form variable...
	*/

	if ((ptr = strrchr(name, '-')) != NULL && isdigit(ptr[1] & 255))
	{
	 /*
	  * Set a specific index in the array...
	  */

	  *ptr++ = '\0';
	  if (line[0])
            cgiSetArray(name, atoi(ptr) - 1, line);
	}
	else if (cgiGetVariable(name))
	{
	 /*
	  * Add another element in the array...
	  */

	  cgiSetArray(name, cgiGetSize(name), line);
	}
	else
	{
	 /*
	  * Just set the line...
	  */

	  cgiSetVariable(name, line);
	}
      }

     /*
      * Read the rest of the current line...
      */

      fgets(line, sizeof(line), stdin);

     /*
      * Clear the state vars...
      */

      name[0]     = '\0';
      filename[0] = '\0';
      mimetype[0] = '\0';
    }
    else if (!_cups_strncasecmp(line, "Content-Disposition:", 20))
    {
      if ((ptr = strstr(line + 20, " name=\"")) != NULL)
      {
        strlcpy(name, ptr + 7, sizeof(name));

	if ((ptr = strchr(name, '\"')) != NULL)
	  *ptr = '\0';
      }

      if ((ptr = strstr(line + 20, " filename=\"")) != NULL)
      {
        strlcpy(filename, ptr + 11, sizeof(filename));

	if ((ptr = strchr(filename, '\"')) != NULL)
	  *ptr = '\0';
      }
    }
    else if (!_cups_strncasecmp(line, "Content-Type:", 13))
    {
      for (ptr = line + 13; isspace(*ptr & 255); ptr ++);

      strlcpy(mimetype, ptr, sizeof(mimetype));

      for (ptr = mimetype + strlen(mimetype) - 1;
           ptr > mimetype && isspace(*ptr & 255);
	   *ptr-- = '\0');
    }
  }

 /*
  * Return 1 for "form data found"...
  */

  return (1);
}


/*
 * 'cgi_initialize_post()' - Initialize variables using the POST method.
 */

static int				/* O - 1 if form data was read */
cgi_initialize_post(void)
{
  char		*content_length,	/* Length of input data (string) */
		*data;			/* Pointer to form data string */
  size_t	length,			/* Length of input data */
		tbytes;			/* Total number of bytes read */
  ssize_t	nbytes;			/* Number of bytes read this read() */
  int		status;			/* Return status */


  DEBUG_puts("cgi_initialize_post: Initializing variables using POST method...");

 /*
  * Check to see if there is anything for us to read...
  */

  content_length = getenv("CONTENT_LENGTH");
  if (content_length == NULL || atoi(content_length) <= 0)
    return (0);

 /*
  * Get the length of the input stream and allocate a buffer for it...
  */

  length = (size_t)strtol(content_length, NULL, 10);
  data   = malloc(length + 1);

  if (data == NULL)
    return (0);

 /*
  * Read the data into the buffer...
  */

  for (tbytes = 0; tbytes < length; tbytes += (size_t)nbytes)
    if ((nbytes = read(0, data + tbytes, (size_t)(length - tbytes))) < 0)
    {
      if (errno != EAGAIN)
      {
        free(data);
        return (0);
      }
      else
        nbytes = 0;
    }
    else if (nbytes == 0)
    {
     /*
      * CUPS STR #3176: OpenBSD: Early end-of-file on POST data causes 100% CPU
      *
      * This should never happen, but does on OpenBSD.  If we see early end-of-
      * file, treat this as an error and process no data.
      */

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

static int				/* O - 1 if form data was processed */
cgi_initialize_string(const char *data)	/* I - Form data string */
{
  int	done;				/* True if we're done reading a form variable */
  char	*s,				/* Pointer to current form string */
	ch,				/* Temporary character */
	name[255],			/* Name of form variable */
	value[65536];			/* Variable value */


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

            if (!isxdigit(data[1] & 255) || !isxdigit(data[2] & 255))
	      return (0);

            if (s < (value + sizeof(value) - 1))
	    {
              data ++;
              ch = *data - '0';
              if (ch > 9)
        	ch -= 7;
              *s = (char)(ch << 4);

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

    while (s >= value && isspace(*s & 255))
      *s-- = '\0';

   /*
    * Add the string to the variable "database"...
    */

    if ((s = strrchr(name, '-')) != NULL && isdigit(s[1] & 255))
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
 * 'cgi_passwd()' - Catch authentication requests and notify the server.
 *
 * This function sends a Status header and exits, forcing authentication
 * for this request.
 */

static const char *			/* O - NULL (no return) */
cgi_passwd(const char *prompt)		/* I - Prompt (not used) */
{
  (void)prompt;

  fprintf(stderr, "DEBUG: cgi_passwd(prompt=\"%s\") called!\n",
          prompt ? prompt : "(null)");

 /*
  * Send a 401 (unauthorized) status to the server, so it can notify
  * the client that authentication is required.
  */

  puts("Status: 401\n");
  exit(0);

 /*
  * This code is never executed, but is present to satisfy the compiler.
  */

  return (NULL);
}


/*
 * 'cgi_set_sid()' - Set the CUPS session ID.
 */

static const char *			/* O - New session ID */
cgi_set_sid(void)
{
  char			buffer[512],	/* SID data */
			sid[33];	/* SID string */
  unsigned char		sum[16];	/* MD5 sum */
  const char		*remote_addr,	/* REMOTE_ADDR */
			*server_name,	/* SERVER_NAME */
			*server_port;	/* SERVER_PORT */
  struct timeval	curtime;	/* Current time */


  if ((remote_addr = getenv("REMOTE_ADDR")) == NULL)
    remote_addr = "REMOTE_ADDR";
  if ((server_name = getenv("SERVER_NAME")) == NULL)
    server_name = "SERVER_NAME";
  if ((server_port = getenv("SERVER_PORT")) == NULL)
    server_port = "SERVER_PORT";

  gettimeofday(&curtime, NULL);
  CUPS_SRAND(curtime.tv_sec + curtime.tv_usec);
  snprintf(buffer, sizeof(buffer), "%s:%s:%s:%02X%02X%02X%02X%02X%02X%02X%02X",
           remote_addr, server_name, server_port,
	   (unsigned)CUPS_RAND() & 255, (unsigned)CUPS_RAND() & 255,
	   (unsigned)CUPS_RAND() & 255, (unsigned)CUPS_RAND() & 255,
	   (unsigned)CUPS_RAND() & 255, (unsigned)CUPS_RAND() & 255,
	   (unsigned)CUPS_RAND() & 255, (unsigned)CUPS_RAND() & 255);
  cupsHashData("md5", (unsigned char *)buffer, strlen(buffer), sum, sizeof(sum));

  cgiSetCookie(CUPS_SID, cupsHashString(sum, sizeof(sum), sid, sizeof(sid)), "/", NULL, 0, 0);

  return (cupsGetOption(CUPS_SID, num_cookies, cookies));
}


/*
 * 'cgi_sort_variables()' - Sort all form variables for faster lookup.
 */

static void
cgi_sort_variables(void)
{
#ifdef DEBUG
  int	i;


  DEBUG_puts("cgi_sort_variables: Sorting variables...");
#endif /* DEBUG */

  if (form_count < 2)
    return;

  qsort(form_vars, (size_t)form_count, sizeof(_cgi_var_t),
        (int (*)(const void *, const void *))cgi_compare_variables);

#ifdef DEBUG
  DEBUG_puts("cgi_sort_variables: Sorted variable list is:");
  for (i = 0; i < form_count; i ++)
    DEBUG_printf(("cgi_sort_variables: %d: %s (%d) = \"%s\" ...\n", i,
                  form_vars[i].name, form_vars[i].nvalues,
		  form_vars[i].values[0]));
#endif /* DEBUG */
}


/*
 * 'cgi_unlink_file()' - Remove the uploaded form.
 */

static void
cgi_unlink_file(void)
{
  if (form_file)
  {
   /*
    * Remove the temporary file...
    */

    unlink(form_file->tempfile);

   /*
    * Free memory used...
    */

    free(form_file->name);
    free(form_file->filename);
    free(form_file->mimetype);
    free(form_file);

    form_file = NULL;
  }
}
