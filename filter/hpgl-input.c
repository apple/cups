/*
 * "$Id: hpgl-input.c,v 1.5 1999/03/21 02:10:11 mike Exp $"
 *
 *   HP-GL/2 input processing for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1993-1999 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Easy Software Products and are protected by Federal
 *   copyright law.  Distribution and use rights are outlined in the file
 *   "LICENSE.txt" which should have been included with this file.  If this
 *   file is missing or damaged please contact Easy Software Products
 *   at:
 *
 *       Attn: CUPS Licensing Information
 *       Easy Software Products
 *       44141 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 *   ParseCommand()   - Parse an HPGL/2 command.
 *   FreeParameters() - Free all string parameter values.
 */

/*
 * Include necessary headers...
 */

#include "hpgltops.h"
#include <ctype.h>

#define MAX_PARAMS 16384


/*
 * 'ParseCommand()' - Parse an HPGL/2 command.
 *
 * Returns the number of parameters seen.
 */

int
ParseCommand(char    *name,	/* O - Name of command */
             param_t **params)	/* O - Parameter list */
{
  int	num_params,	/* Number of parameters seen */
	ch,		/* Current char */
	done;		/* Non-zero when the current command is read */
  int	i;		/* Looping var */
  char	buf[262144];	/* String buffer */
  static param_t	p[MAX_PARAMS];
  			/* Parameter buffer */


  num_params = 0;
  done       = 0;

  do
  {
    while ((ch = getc(InputFile)) != EOF)
      if (strchr(" \t\r\n,;", ch) == NULL)
        break;

    if (ch == EOF)
      return (-1);

    if (ch == 0x1b)
      switch (getc(InputFile))
      {
        case '.' : /* HPGL/2 job control */
            i = getc(InputFile);
            if (strchr(")Z", i) != NULL)
            {
             /*
              * 'Printer Off' command - look for next 'Printer On' command...
              */
              while (1)
              {
                while ((i = getc(InputFile)) != EOF &&
                       i != 0x1b);
                if (i == EOF)
                  return (-1);

                if (getc(InputFile) != '.')
                  continue;
                if ((i = getc(InputFile)) == '(' ||
                    i == 'Y')
                  break;
              };
            }
            else if (strchr("@HIMNTI\003", i) != NULL)
            {
              while ((i = getc(InputFile)) != EOF &&
                     i != ':');
            };
            break;

        default : /* HP RTL/PCL control */
            while ((i = getc(InputFile)) != EOF &&
                   !isupper(i));
            break;
      };
  } while (ch == 0x1b);

  name[0] = ch;
  name[1] = getc(InputFile);
  name[2] = '\0';

  if (strcasecmp(name, "LB") == 0)
  {
    for (i = 0; (ch = getc(InputFile)) != StringTerminator; i ++)
      buf[i] = ch;
    buf[i] = '\0';
    p[num_params].type = PARAM_STRING;
    p[num_params].value.string = strdup(buf);
    num_params ++;
  }
  else if (strcasecmp(name, "SM") == 0)
  {
    buf[0] = getc(InputFile);
    buf[1] = '\0';
    p[num_params].type = PARAM_STRING;
    p[num_params].value.string = strdup(buf);
    num_params ++;
  }
  else if (strcasecmp(name, "DT") == 0)
  {
    if ((buf[0] = getc(InputFile)) != ';')
    {
      buf[1] = '\0';
      p[num_params].type = PARAM_STRING;
      p[num_params].value.string = strdup(buf);
      num_params ++;
    };
  }
  else if (strcasecmp(name, "PE") == 0)
  {
    for (i = 0; i < (sizeof(buf) - 1); i ++)
      if ((buf[i] = getc(InputFile)) == ';')
        break;

    buf[i] = '\0';
    p[num_params].type = PARAM_STRING;
    p[num_params].value.string = strdup(buf);
    num_params ++;
  };

  while (!done)
    switch (ch = getc(InputFile))
    {
      case ',' :
      case ' ' :
      case '\n' :
      case '\r' :
      case '\t' :
          break;

      case '\"' :
          fscanf(InputFile, "%[^\"]\"", buf);
          if (num_params < MAX_PARAMS)
          {
            p[num_params].type = PARAM_STRING;
            p[num_params].value.string = strdup(buf);
            num_params ++;
          };
          break;

      case '-' :
      case '+' :
          ungetc(ch, InputFile);
          fscanf(InputFile, "%f", &(p[num_params].value.number));
          if (num_params < MAX_PARAMS)
          {
            p[num_params].type = PARAM_RELATIVE;
            num_params ++;
          };
          break;
      case '0' :
      case '1' :
      case '2' :
      case '3' :
      case '4' :
      case '5' :
      case '6' :
      case '7' :
      case '8' :
      case '9' :
      case '.' :
          ungetc(ch, InputFile);
          fscanf(InputFile, "%f", &(p[num_params].value.number));
          if (num_params < MAX_PARAMS)
          {
            p[num_params].type = PARAM_ABSOLUTE;
            num_params ++;
          };
          break;
      default :
          ungetc(ch, InputFile);
          done = 1;
          break;
    };

  *params = p;
  return (num_params);
}


/*
 * 'FreeParameters()' - Free all string parameter values.
 */

void
FreeParameters(int     num_params, /* I - Number of parameters */
               param_t *params)    /* I - Parameter values */
{
  int i;	/* Looping var */


  for (i = 0; i < num_params; i ++)
    if (params[i].type == PARAM_STRING)
      free(params[i].value.string);
}


/*
 * End of "$Id: hpgl-input.c,v 1.5 1999/03/21 02:10:11 mike Exp $".
 */
