/*
 * "$Id: template.c,v 1.22.2.2 2002/01/02 18:04:21 mike Exp $"
 *
 *   CGI template function.
 *
 *   Copyright 1997-2002 by Easy Software Products.
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
 *   cgiCopyTemplateFile() - Copy a template file and replace all the
 *                           '{variable}' strings with the variable value.
 *   cgiCopyTemplateLang() - Copy a template file using a language...
 *   cgi_copy()            - Copy the template file, substituting as needed...
 *   cgi_puts()            - Put a string to the output file, quoting as
 *                           needed...
 */

#include "cgi.h"


/*
 * Local functions...
 */

static void	cgi_copy(FILE *out, FILE *in, int element, char term);
static void	cgi_puts(const char *s, FILE *out);


/*
 * 'cgiCopyTemplateFile()' - Copy a template file and replace all the
 *                           '{variable}' strings with the variable value.
 */

void
cgiCopyTemplateFile(FILE       *out,	/* I - Output file */
                    const char *tmpl)	/* I - Template file to read */
{
  FILE	*in;				/* Input file */


 /*
  * Open the template file...
  */

  if ((in = fopen(tmpl, "r")) == NULL)
    return;

 /*
  * Parse the file to the end...
  */

  cgi_copy(out, in, 0, 0);

 /*
  * Close the template file and return...
  */

  fclose(in);
}


/*
 * 'cgiCopyTemplateLang()' - Copy a template file using a language...
 */

void
cgiCopyTemplateLang(FILE       *out,		/* I - Output file */
                    const char *directory,	/* I - Directory */
                    const char *tmpl,		/* I - Base filename */
		    const char *lang)		/* I - Language */
{
  int	i;					/* Looping var */
  char	filename[1024],				/* Filename */
	locale[16];				/* Locale name */
  FILE	*in;					/* Input file */


 /*
  * Convert the language to a locale name...
  */

  if (lang != NULL)
  {
    for (i = 0; lang[i] && i < 15; i ++)
      if (isalnum(lang[i]))
        locale[i] = tolower(lang[i]);
      else
        locale[i] = '_';

    locale[i] = '\0';
  }
  else
    locale[0] = '\0';

 /*
  * See if we have a template file for this language...
  */

  snprintf(filename, sizeof(filename), "%s/%s/%s", directory, locale, tmpl);
  if (access(filename, 0))
  {
    locale[2] = '\0';

    snprintf(filename, sizeof(filename), "%s/%s/%s", directory, locale, tmpl);
    if (access(filename, 0))
      snprintf(filename, sizeof(filename), "%s/%s", directory, tmpl);
  }

 /*
  * Open the template file...
  */

  if ((in = fopen(filename, "r")) == NULL)
    return;

 /*
  * Parse the file to the end...
  */

  cgi_copy(out, in, 0, 0);

 /*
  * Close the template file and return...
  */

  fclose(in);
}


/*
 * 'cgi_copy()' - Copy the template file, substituting as needed...
 */

static void
cgi_copy(FILE *out,		/* I - Output file */
         FILE *in,		/* I - Input file */
	 int  element,		/* I - Element number (0 to N) */
	 char term)		/* I - Terminating character */
{
  int		ch;		/* Character from file */
  char		op;		/* Operation */
  char		name[255],	/* Name of variable */
		*nameptr,	/* Pointer into name */
		innername[255],	/* Inner comparison name */
		*innerptr,	/* Pointer into inner name */
		*s;		/* String pointer */
  const char	*value;		/* Value of variable */
  const char	*innerval;	/* Inner value */
  const char	*outptr;	/* Output string pointer */
  char		outval[1024],	/* Formatted output string */
		compare[1024];	/* Comparison string */
  int		result;		/* Result of comparison */


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

      for (s = name; (ch = getc(in)) != EOF;)
        if (strchr("}]<>=! \t\n", ch))
          break;
        else if (s > name && ch == '?')
	  break;
	else if (s < (name + sizeof(name) - 1))
          *s++ = ch;

      *s = '\0';

      if (s == name && isspace(ch))
      {
        if (out)
	{
          putc('{', out);
	  putc(ch, out);
        }

	continue;
      }

     /*
      * See if it has a value...
      */

      if (name[0] == '?')
      {
       /*
        * Insert value only if it exists...
	*/

	if ((nameptr = strrchr(name, '-')) != NULL && isdigit(nameptr[1]))
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
        if ((value = cgiGetArray(name + 1, element)) != NULL)
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


        if (isdigit(name[1]))
	  count = atoi(name + 1);
	else
          count = cgiGetSize(name + 1);

	pos = ftell(in);

        if (count > 0)
	{
          for (i = 0; i < count; i ++)
	  {
	    fseek(in, pos, SEEK_SET);
	    cgi_copy(out, in, i, '}');
	  }
        }
	else
	  cgi_copy(NULL, in, 0, '}');

        continue;
      }
      else
      {
       /*
        * Insert variable or variable name (if element is NULL)...
	*/

	if ((nameptr = strrchr(name, '-')) != NULL && isdigit(nameptr[1]))
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
	  cgi_puts(outptr, out);

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

      if (ch == '?')
      {
       /*
        * Test for existance...
	*/

        result = cgiGetArray(name, element) != NULL && outval[0];
      }
      else
      {
       /*
        * Compare to a string...
	*/

        op = ch;

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
	             isdigit(innerptr[1]))
            {
	      *innerptr++ = '\0';
	      if ((innerval = cgiGetArray(innername, atoi(innerptr) - 1)) == NULL)
	        *s = '\0';
	      else
	      {
	        strncpy(s, innerval, sizeof(compare) - (s - compare) - 1);
		compare[sizeof(compare) - 1] = '\0';
	      }
	    }
	    else if (innername[0] == '?')
	    {
	      if ((innerval = cgiGetArray(innername + 1, element)) == NULL)
		*s = '\0';
	      else
	      {
	        strncpy(s, innerval, sizeof(compare) - (s - compare) - 1);
		compare[sizeof(compare) - 1] = '\0';
	      }
            }
	    else if ((innerval = cgiGetArray(innername, element)) == NULL)
	      snprintf(s, sizeof(s), "{%s}", innername);
	    else
	    {
	      strncpy(s, innerval, sizeof(compare) - (s - compare) - 1);
	      compare[sizeof(compare) - 1] = '\0';
	    }

            s += strlen(s);
	  }
          else if (ch == '\\')
	    *s++ = getc(in);
	  else
            *s++ = ch;

        *s = '\0';

        if (ch != '?')
	  return;

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

      if (result)
      {
       /*
	* Comparison true; output first part and ignore second...
	*/

	cgi_copy(out, in, element, ':');
	cgi_copy(NULL, in, element, '}');
      }
      else
      {
       /*
	* Comparison false; ignore first part and output second...
	*/

	cgi_copy(NULL, in, element, ':');
	cgi_copy(out, in, element, '}');
      }
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
cgi_puts(const char *s,
         FILE       *out)
{
  while (*s)
  {
    if (s[0] == '<')
      fputs("&lt;", out);
    else if (s[0] == '>')
      fputs("&gt;", out);
    else if (*s == '\"')
      fputs("&quot;", out);
    else if (s[0] == '&')
      fputs("&amp;", out);
    else
      putc(*s, out);

    s ++;
  }
}


/*
 * End of "$Id: template.c,v 1.22.2.2 2002/01/02 18:04:21 mike Exp $".
 */
