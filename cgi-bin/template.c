/*
 * "$Id: template.c,v 1.1 1997/05/08 19:55:53 mike Exp $"
 *
 *   CGI template function.
 *
 * Revision History:
 *
 *   $Log: template.c,v $
 *   Revision 1.1  1997/05/08 19:55:53  mike
 *   Initial revision
 *
 */

#include "cgi.h"


/*
 * 'CGI_CopyTemplateFile()' - Copy a template file and replace all the
 *                            '{variable}' strings with the variable value.
 */

void
CGI_CopyTemplateFile(FILE *out,		/* I - Output file */
                     char *template)	/* I - Template file to read */
{
  FILE	*in;		/* Input file */
  int	ch;		/* Character from file */
  char	name[255],	/* Name of variable */
	*s,		/* String pointer */
	*value;		/* Value of variable */


 /*
  * Open the template file...
  */

  in = fopen(template, "r");
  if (in == NULL)
    return;

 /*
  * Parse the file to the end...
  */

  while ((ch = getc(in)) != EOF)
    if (ch == '{')
    {
     /*
      * Get a variable name...
      */

      for (s = name; (ch = getc(in)) != EOF; s ++)
        if (ch == '}')
          break;
        else
          *s = ch;
      *s = '\0';

     /*
      * See if it has a value...
      */

      value = CGI_GetVariable(name);
      if (value == NULL)
        fprintf(out, "{%s}", name);
      else
        fprintf(out, "%s", value);
    }
    else if (ch == '\\')	/* Quoted char */
      putc(getc(in), out);
    else
      putc(ch, out);

 /*
  * Close the template file and return...
  */

  fclose(in);
}


/*
 * End of "$Id: template.c,v 1.1 1997/05/08 19:55:53 mike Exp $".
 */
