/*
 * "$Id: template.c,v 1.4 1999/09/10 13:38:33 mike Exp $"
 *
 *   CGI template function.
 *
 *   Copyright 1997-1999 by Easy Software Products, All Rights Reserved.
 *
 * Contents:
 *
 *   cgiCopyTemplateFile() - Copy a template file and replace all the
 *                           '{variable}' strings with the variable value.
 *   cgi_copy()            - Copy the template file, substituting as needed...
 */

#include "cgi.h"


/*
 * Local functions...
 */

static void	cgi_copy(FILE *out, FILE *in, int element);


/*
 * 'cgiCopyTemplateFile()' - Copy a template file and replace all the
 *                           '{variable}' strings with the variable value.
 */

void
cgiCopyTemplateFile(FILE       *out,		/* I - Output file */
                    const char *template)	/* I - Template file to read */
{
  FILE		*in;			/* Input file */
  int		ch;			/* Character from file */
  char		name[255],		/* Name of variable */
		*s;			/* String pointer */
  const char	*value;			/* Value of variable */


 /*
  * Open the template file...
  */

  if ((in = fopen(template, "r")) == NULL)
    return;

 /*
  * Parse the file to the end...
  */

  cgi_copy(out, in, 0);

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
	 int  element)		/* I - Element number (0 to N) */
{
  int		ch;			/* Character from file */
  char		name[255],		/* Name of variable */
		*s;			/* String pointer */
  const char	*value;			/* Value of variable */


 /*
  * Parse the file to the end...
  */

  while ((ch = getc(in)) != EOF)
    if (ch == '}')
      break;
    else if (ch == '{')
    {
     /*
      * Get a variable name...
      */

      for (s = name; (ch = getc(in)) != EOF; s ++)
        if (ch == '}' || ch == ']')
          break;
        else
          *s = ch;
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
          fputs(value, out);
      }
      else if (name[0] == '#')
      {
       /*
        * Insert count...
	*/

        fprintf(out, "%d", cgiGetSize(name + 1));
      }
      else if (name[0] == '[')
      {
       /*
        * Loop for # of elements...
	*/

	int  i;		/* Looping var */
        long pos;	/* File position */
	int  count;	/* Number of elements */


        count = cgiGetSize(name + 1);
	pos   = ftell(in);

        for (i = 0; i < count; i ++)
	{
	  fseek(in, pos, SEEK_SET);
	  cgi_copy(out, in, i);
	}
      }
      else
      {
       /*
        * Insert variable or variable name (if element is NULL)...
	*/

	if ((value = cgiGetArray(name, element)) == NULL)
          fprintf(out, "{%s}", name);
	else
          fputs(value, out);
      }
    }
    else if (ch == '\\')	/* Quoted char */
      putc(getc(in), out);
    else
      putc(ch, out);
}


/*
 * End of "$Id: template.c,v 1.4 1999/09/10 13:38:33 mike Exp $".
 */
