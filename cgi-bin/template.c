/*
 * "$Id: template.c,v 1.9 1999/10/29 15:51:22 mike Exp $"
 *
 *   CGI template function.
 *
 *   Copyright 1997-1999 by Easy Software Products.
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
cgiCopyTemplateFile(FILE       *out,		/* I - Output file */
                    const char *template)	/* I - Template file to read */
{
  FILE	*in;					/* Input file */


 /*
  * Open the template file...
  */

  if ((in = fopen(template, "r")) == NULL)
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
		*s;		/* String pointer */
  const char	*value;		/* Value of variable */
  char		outval[1024],	/* Output string */
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
        if (strchr("}]<>=!", ch))
          break;
        else if (s > name && ch == '?')
	  break;
	else
          *s++ = ch;

      *s = '\0';

     /*
      * See if it has a value...
      */

      if (name[0] == '?')
      {
       /*
        * Insert value only if it exists...
	*/

        if ((value = cgiGetArray(name + 1, element)) != NULL)
	  strcpy(outval, value);
	else
	  outval[0] = '\0';
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

	if ((value = cgiGetArray(name, element)) == NULL)
          sprintf(outval, "{%s}", name);
	else
          strcpy(outval, value);
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
	  cgi_puts(outval, out);

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

        result = cgiGetVariable(name) != NULL;
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
	  else if (ch == '#')
	  {
	    sprintf(s, "%d", element + 1);
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
	      result = strcasecmp(outval, compare) < 0;
	      break;
	  case '>' :
	      result = strcasecmp(outval, compare) > 0;
	      break;
	  case '=' :
	      result = strcasecmp(outval, compare) == 0;
	      break;
	  case '!' :
	      result = strcasecmp(outval, compare) != 0;
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
    if (s[0] == '<' && s[1] != '/' && !isalpha(s[1]))
      fputs("&lt;", out);
    else
      putc(*s, out);

    s ++;
  }
}


/*
 * End of "$Id: template.c,v 1.9 1999/10/29 15:51:22 mike Exp $".
 */
