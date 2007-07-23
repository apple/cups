/*
 * "$Id$"
 *
 *   CGI template function.
 *
 *   Copyright 2007 by Apple Inc.
 *   Copyright 1997-2006 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
 *   cgiCopyTemplateFile() - Copy a template file and replace all the
 *                           '{variable}' strings with the variable value.
 *   cgiCopyTemplateLang() - Copy a template file using a language...
 *   cgiGetTemplateDir()   - Get the templates directory...
 *   cgiSetServerVersion() - Set the server name and CUPS version...
 *   cgi_copy()            - Copy the template file, substituting as needed...
 *   cgi_puts()            - Put a string to the output file, quoting as
 *                           needed...
 */

#include "cgi-private.h"
#include <errno.h>


/*
 * Local functions...
 */

static void	cgi_copy(FILE *out, FILE *in, int element, char term,
		         int indent);
static void	cgi_puts(const char *s, FILE *out);
static void	cgi_puturi(const char *s, FILE *out);


/*
 * 'cgiCopyTemplateFile()' - Copy a template file and replace all the
 *                           '{variable}' strings with the variable value.
 */

void
cgiCopyTemplateFile(FILE       *out,	/* I - Output file */
                    const char *tmpl)	/* I - Template file to read */
{
  FILE	*in;				/* Input file */


  fprintf(stderr, "DEBUG2: cgiCopyTemplateFile(out=%p, tmpl=\"%s\")\n", out,
          tmpl ? tmpl : "(null)");

 /*
  * Open the template file...
  */

  if ((in = fopen(tmpl, "r")) == NULL)
  {
    fprintf(stderr, "ERROR: Unable to open template file \"%s\" - %s\n",
            tmpl ? tmpl : "(null)", strerror(errno));
    return;
  }

 /*
  * Parse the file to the end...
  */

  cgi_copy(out, in, 0, 0, 0);

 /*
  * Close the template file and return...
  */

  fclose(in);
}


/*
 * 'cgiCopyTemplateLang()' - Copy a template file using a language...
 */

void
cgiCopyTemplateLang(const char *tmpl)	/* I - Base filename */
{
  int		i;			/* Looping var */
  char		filename[1024],		/* Filename */
		locale[16];		/* Locale name */
  const char	*directory,		/* Directory for templates */
		*lang;			/* Language */
  FILE		*in;			/* Input file */


  fprintf(stderr, "DEBUG2: cgiCopyTemplateLang(tmpl=\"%s\")\n",
          tmpl ? tmpl : "(null)");

 /*
  * Convert the language to a locale name...
  */

  if ((lang = getenv("LANG")) != NULL)
  {
    for (i = 0; lang[i] && i < 15; i ++)
      if (isalnum(lang[i] & 255) || lang[i] == '_')
        locale[i] = tolower(lang[i]);
      else if (lang[i] == '-')
        locale[i] = '_';
      else
        break;

    locale[i] = '\0';
  }
  else
    locale[0] = '\0';

  fprintf(stderr, "DEBUG2: locale=\"%s\"...\n", locale);

 /*
  * See if we have a template file for this language...
  */

  directory = cgiGetTemplateDir();

  snprintf(filename, sizeof(filename), "%s/%s/%s", directory, locale, tmpl);
  if (access(filename, 0))
  {
    locale[2] = '\0';

    snprintf(filename, sizeof(filename), "%s/%s/%s", directory, locale, tmpl);
    if (access(filename, 0))
      snprintf(filename, sizeof(filename), "%s/%s", directory, tmpl);
  }

  fprintf(stderr, "DEBUG2: Template file is \"%s\"...\n", filename);

 /*
  * Open the template file...
  */

  if ((in = fopen(filename, "r")) == NULL)
  {
    fprintf(stderr, "ERROR: Unable to open template file \"%s\" - %s\n",
            filename, strerror(errno));
    return;
  }

 /*
  * Parse the file to the end...
  */

  cgi_copy(stdout, in, 0, 0, 0);

 /*
  * Close the template file and return...
  */

  fclose(in);
}


/*
 * 'cgiGetTemplateDir()' - Get the templates directory...
 */

char *					/* O - Template directory */
cgiGetTemplateDir(void)
{
  const char	*datadir;		/* CUPS_DATADIR env var */
  static char	templates[1024] = "";	/* Template directory */


  if (!templates[0])
  {
   /*
    * Build the template directory pathname...
    */

    if ((datadir = getenv("CUPS_DATADIR")) == NULL)
      datadir = CUPS_DATADIR;

    snprintf(templates, sizeof(templates), "%s/templates", datadir);
  }

  return (templates);
}


/*
 * 'cgiSetServerVersion()' - Set the server name and CUPS version...
 */

void
cgiSetServerVersion(void)
{
  cgiSetVariable("SERVER_NAME", getenv("SERVER_NAME"));
  cgiSetVariable("REMOTE_USER", getenv("REMOTE_USER"));
  cgiSetVariable("CUPS_VERSION", CUPS_SVERSION);

#ifdef LC_TIME
  setlocale(LC_TIME, "");
#endif /* LC_TIME */
}


/*
 * 'cgi_copy()' - Copy the template file, substituting as needed...
 */

static void
cgi_copy(FILE *out,			/* I - Output file */
         FILE *in,			/* I - Input file */
	 int  element,			/* I - Element number (0 to N) */
	 char term,			/* I - Terminating character */
	 int  indent)			/* I - Debug info indentation */
{
  int		ch;			/* Character from file */
  char		op;			/* Operation */
  char		name[255],		/* Name of variable */
		*nameptr,		/* Pointer into name */
		innername[255],		/* Inner comparison name */
		*innerptr,		/* Pointer into inner name */
		*s;			/* String pointer */
  const char	*value;			/* Value of variable */
  const char	*innerval;		/* Inner value */
  const char	*outptr;		/* Output string pointer */
  char		outval[1024],		/* Formatted output string */
		compare[1024];		/* Comparison string */
  int		result;			/* Result of comparison */
  int		uriencode;		/* Encode as URI */


  fprintf(stderr, "DEBUG2: %*sStarting at file position %ld...\n", indent, "",
          ftell(in));

 /*
  * Parse the file to the end...
  */

  while ((ch = getc(in)) != EOF)
    if (ch == term)
      break;
    else if (ch == '{')
    {
     /*
      * Get a variable name...
      */

      uriencode = 0;

      for (s = name; (ch = getc(in)) != EOF;)
        if (strchr("}]<>=! \t\n", ch))
          break;
	else if (s == name && ch == '%')
	  uriencode = 1;
        else if (s > name && ch == '?')
	  break;
	else if (s < (name + sizeof(name) - 1))
          *s++ = ch;

      *s = '\0';

      if (s == name && isspace(ch & 255))
      {
        fprintf(stderr, "DEBUG2: %*sLone { at %ld...\n", indent, "", ftell(in));

        if (out)
	{
          putc('{', out);
	  putc(ch, out);
        }

	continue;
      }

      if (ch == '}')
	fprintf(stderr, "DEBUG2: %*s\"{%s}\" at %ld...\n", indent, "", name,
        	ftell(in));

     /*
      * See if it has a value...
      */

      if (name[0] == '?')
      {
       /*
        * Insert value only if it exists...
	*/

	if ((nameptr = strrchr(name, '-')) != NULL && isdigit(nameptr[1] & 255))
	{
	  *nameptr++ = '\0';

	  if ((value = cgiGetArray(name + 1, atoi(nameptr) - 1)) != NULL)
	    outptr = value;
	  else
	  {
	    outval[0] = '\0';
	    outptr    = outval;
	  }
	}
        else if ((value = cgiGetArray(name + 1, element)) != NULL)
	  outptr = value;
	else
	{
	  outval[0] = '\0';
	  outptr    = outval;
	}
      }
      else if (name[0] == '#')
      {
       /*
        * Insert count...
	*/

        if (name[1])
          sprintf(outval, "%d", cgiGetSize(name + 1));
	else
	  sprintf(outval, "%d", element + 1);

        outptr = outval;
      }
      else if (name[0] == '[')
      {
       /*
        * Loop for # of elements...
	*/

	int  i;		/* Looping var */
        long pos;	/* File position */
	int  count;	/* Number of elements */


        if (isdigit(name[1] & 255))
	  count = atoi(name + 1);
	else
          count = cgiGetSize(name + 1);

	pos = ftell(in);

        fprintf(stderr, "DEBUG2: %*sLooping on \"%s\" at %ld, count=%d...\n",
	        indent, "", name + 1, pos, count);

        if (count > 0)
	{
          for (i = 0; i < count; i ++)
	  {
	    if (i)
	      fseek(in, pos, SEEK_SET);

	    cgi_copy(out, in, i, '}', indent + 2);
	  }
        }
	else
	  cgi_copy(NULL, in, 0, '}', indent + 2);

        fprintf(stderr, "DEBUG2: %*sFinished looping on \"%s\"...\n", indent,
	        "", name + 1);

        continue;
      }
      else
      {
       /*
        * Insert variable or variable name (if element is NULL)...
	*/

	if ((nameptr = strrchr(name, '-')) != NULL && isdigit(nameptr[1] & 255))
	{
	  *nameptr++ = '\0';
	  if ((value = cgiGetArray(name, atoi(nameptr) - 1)) == NULL)
          {
	    snprintf(outval, sizeof(outval), "{%s}", name);
	    outptr = outval;
	  }
	  else
	    outptr = value;
	}
	else if ((value = cgiGetArray(name, element)) == NULL)
        {
	  snprintf(outval, sizeof(outval), "{%s}", name);
	  outptr = outval;
	}
	else
	  outptr = value;
      }

     /*
      * See if the terminating character requires another test...
      */

      if (ch == '}')
      {
       /*
        * End of substitution...
        */

	if (out)
	{
	  if (uriencode)
	    cgi_puturi(outptr, out);
	  else
	    cgi_puts(outptr, out);
        }

        continue;
      }

     /*
      * OK, process one of the following checks:
      *
      *   {name?exist:not-exist}     Exists?
      *   {name=value?true:false}    Equal
      *   {name<value?true:false}    Less than
      *   {name>value?true:false}    Greater than
      *   {name!value?true:false}    Not equal
      */

      op = ch;

      if (ch == '?')
      {
       /*
        * Test for existance...
	*/

        result     = cgiGetArray(name, element) != NULL && outptr[0];
	compare[0] = '\0';
      }
      else
      {
       /*
        * Compare to a string...
	*/

	for (s = compare; (ch = getc(in)) != EOF;)
          if (ch == '?')
            break;
	  else if (s >= (compare + sizeof(compare) - 1))
	    continue;
	  else if (ch == '#')
	  {
	    sprintf(s, "%d", element + 1);
	    s += strlen(s);
	  }
	  else if (ch == '{')
	  {
	   /*
	    * Grab the value of a variable...
	    */

	    innerptr = innername;
	    while ((ch = getc(in)) != EOF && ch != '}')
	      if (innerptr < (innername + sizeof(innername) - 1))
	        *innerptr++ = ch;
	    *innerptr = '\0';

            if (innername[0] == '#')
	      sprintf(s, "%d", cgiGetSize(innername + 1));
	    else if ((innerptr = strrchr(innername, '-')) != NULL &&
	             isdigit(innerptr[1] & 255))
            {
	      *innerptr++ = '\0';
	      if ((innerval = cgiGetArray(innername, atoi(innerptr) - 1)) == NULL)
	        *s = '\0';
	      else
	        strlcpy(s, innerval, sizeof(compare) - (s - compare));
	    }
	    else if (innername[0] == '?')
	    {
	      if ((innerval = cgiGetArray(innername + 1, element)) == NULL)
		*s = '\0';
	      else
	        strlcpy(s, innerval, sizeof(compare) - (s - compare));
            }
	    else if ((innerval = cgiGetArray(innername, element)) == NULL)
	      snprintf(s, sizeof(compare) - (s - compare), "{%s}", innername);
	    else
	      strlcpy(s, innerval, sizeof(compare) - (s - compare));

            s += strlen(s);
	  }
          else if (ch == '\\')
	    *s++ = getc(in);
	  else
            *s++ = ch;

        *s = '\0';

        if (ch != '?')
	{
	  fprintf(stderr,
	          "DEBUG2: %*sBad terminator '%c' at file position %ld...\n",
	          indent, "", ch, ftell(in));
	  return;
	}

       /*
        * Do the comparison...
	*/

        switch (op)
	{
	  case '<' :
	      result = strcasecmp(outptr, compare) < 0;
	      break;
	  case '>' :
	      result = strcasecmp(outptr, compare) > 0;
	      break;
	  case '=' :
	      result = strcasecmp(outptr, compare) == 0;
	      break;
	  case '!' :
	      result = strcasecmp(outptr, compare) != 0;
	      break;
	  default :
	      result = 1;
	      break;
	}
      }

      fprintf(stderr,
              "DEBUG2: %*sStarting \"{%s%c%s\" at %ld, result=%d...\n",
	      indent, "", name, op, compare, ftell(in), result);

      if (result)
      {
       /*
	* Comparison true; output first part and ignore second...
	*/

        fprintf(stderr, "DEBUG2: %*sOutput first part...\n", indent, "");
	cgi_copy(out, in, element, ':', indent + 2);

        fprintf(stderr, "DEBUG2: %*sSkip second part...\n", indent, "");
	cgi_copy(NULL, in, element, '}', indent + 2);
      }
      else
      {
       /*
	* Comparison false; ignore first part and output second...
	*/

        fprintf(stderr, "DEBUG2: %*sSkip first part...\n", indent, "");
	cgi_copy(NULL, in, element, ':', indent + 2);

        fprintf(stderr, "DEBUG2: %*sOutput second part...\n", indent, "");
	cgi_copy(out, in, element, '}', indent + 2);
      }

      fprintf(stderr, "DEBUG2: %*sFinished \"{%s%c%s\", out=%p...\n", indent, "",
              name, op, compare, out);
    }
    else if (ch == '\\')	/* Quoted char */
    {
      if (out)
        putc(getc(in), out);
      else
        getc(in);
    }
    else if (out)
      putc(ch, out);

  if (ch == EOF)
    fprintf(stderr, "DEBUG2: %*sReturning at file position %ld on EOF...\n",
	    indent, "", ftell(in));
  else
    fprintf(stderr,
            "DEBUG2: %*sReturning at file position %ld on character '%c'...\n",
	    indent, "", ftell(in), ch);

  if (ch == EOF && term)
    fprintf(stderr, "ERROR: %*sSaw EOF, expected '%c'!\n", indent, "", term);

 /*
  * Flush any pending output...
  */

  if (out)
    fflush(out);
}


/*
 * 'cgi_puts()' - Put a string to the output file, quoting as needed...
 */

static void
cgi_puts(const char *s,			/* I - String to output */
         FILE       *out)		/* I - Output file */
{
  while (*s)
  {
    if (*s == '<')
    {
     /*
      * Pass <A HREF="url"> and </A>, otherwise quote it...
      */

      if (!strncasecmp(s, "<A HREF=\"", 9))
      {
        fputs("<A HREF=\"", out);
	s += 9;

	while (*s && *s != '\"')
	{
          if (*s == '&')
            fputs("&amp;", out);
	  else
	    putc(*s, out);

	  s ++;
	}

        if (*s)
	  s ++;

	fputs("\">", out);
      }
      else if (!strncasecmp(s, "</A>", 4))
      {
        fputs("</A>", out);
	s += 3;
      }
      else
        fputs("&lt;", out);
    }
    else if (*s == '>')
      fputs("&gt;", out);
    else if (*s == '\"')
      fputs("&quot;", out);
    else if (*s == '&')
      fputs("&amp;", out);
    else
      putc(*s, out);

    s ++;
  }
}


/*
 * 'cgi_puturi()' - Put a URI string to the output file, quoting as needed...
 */

static void
cgi_puturi(const char *s,		/* I - String to output */
           FILE       *out)		/* I - Output file */
{
  while (*s)
  {
    if (strchr("%&+ <>#=", *s) || *s & 128)
      fprintf(out, "%%%02X", *s & 255);
    else
      putc(*s, out);

    s ++;
  }
}


/*
 * End of "$Id$".
 */
