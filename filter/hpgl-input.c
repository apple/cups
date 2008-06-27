/*
 * "$Id: hpgl-input.c 7219 2008-01-14 22:00:02Z mike $"
 *
 *   HP-GL/2 input processing for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007-2008 by Apple Inc.
 *   Copyright 1993-2006 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   This file is subject to the Apple OS-Developed Software exception.
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
#include <cups/i18n.h>

#define MAX_PARAMS 16384


/*
 * 'ParseCommand()' - Parse an HPGL/2 command.
 *
 * Returns the number of parameters seen or -1 on EOF.
 */

int				/* O - -1 on EOF, # params otherwise */
ParseCommand(FILE    *fp,	/* I - File to read from */
             char    *name,	/* O - Name of command */
             param_t **params)	/* O - Parameter list */
{
  int		num_params,	/* Number of parameters seen */
		ch,		/* Current char */
		done,		/* Non-zero when the current command is read */
		i;		/* Looping var */
  char		buf[262144],	/* String buffer */
		*bufptr;	/* Pointer into buffer */
  float		temp;		/* Temporary parameter value */
  static param_t p[MAX_PARAMS];	/* Parameter buffer */


  num_params = 0;
  done       = 0;

  do
  {
    while ((ch = getc(fp)) != EOF)
      if (strchr(" \t\r\n,;", ch) == NULL)
        break;

    if (ch == EOF)
    {
      return (-1);
    }

    if (ch == 0x1b)
      switch (getc(fp))
      {
        case '.' : /* HP-GL/2 job control */
            i = getc(fp);

            if (strchr(")Z", i) != NULL)
            {
             /*
              * 'Printer Off' command - look for next 'Printer On' command...
              */

              for (;;)
              {
                while ((i = getc(fp)) != EOF && i != 0x1b);

                if (i == EOF)
                  return (-1);

                if (getc(fp) != '.')
                  continue;

                if ((i = getc(fp)) == '(' ||
                    i == 'Y')
                  break;
              }
            }
            else if (strchr("@HIMNTI\003", i) != NULL)
            {
              while ((i = getc(fp)) != EOF && i != ':');
            }
            break;

        case '%' : /* PJL command? */
            if ((i = getc(fp)) == '-')
	      if ((i = getc(fp)) == '1')
	        if ((i = getc(fp)) == '2')
		{
		 /*
		  * Yes, dump everything up to the "ENTER LANGUAGE" line...
		  */

        	  while (fgets(buf, sizeof(buf), fp) != NULL)
	            if (strstr(buf, "ENTER") && strstr(buf, "LANGUAGE"))
		      break;
		  break;
		}

            ungetc(i, fp);

        default : /* HP RTL/PCL control */
            while ((i = getc(fp)) != EOF && !isupper(i & 255));

	    if (i == EOF)
	      return (-1);
            break;
      }
  } while (ch < ' ');

  name[0] = ch;
  name[1] = getc(fp);
  name[2] = '\0';

  if (name[1] < ' ')
  {
   /*
    * If we get here, then more than likely we are faced with a raw PCL
    * file which we can't handle - abort!
    */

    fputs(_("ERROR: Invalid HP-GL/2 command seen, unable to print file!\n"),
          stderr);
    return (-1);
  }

  if (!strcasecmp(name, "LB"))
  {
    bufptr = buf;
    while ((ch = getc(fp)) != StringTerminator)
      if (bufptr < (buf + sizeof(buf) - 1))
        *bufptr++ = ch;
    *bufptr = '\0';

    p[num_params].type         = PARAM_STRING;
    p[num_params].value.string = strdup(buf);
    num_params ++;
  }
  else if (!strcasecmp(name, "SM"))
  {
    buf[0] = getc(fp);
    buf[1] = '\0';
    p[num_params].type         = PARAM_STRING;
    p[num_params].value.string = strdup(buf);
    num_params ++;
  }
  else if (!strcasecmp(name, "DT"))
  {
    if ((buf[0] = getc(fp)) != ';')
    {
      buf[1] = '\0';
      p[num_params].type         = PARAM_STRING;
      p[num_params].value.string = strdup(buf);
      num_params ++;
    }
  }
  else if (!strcasecmp(name, "PE"))
  {
    bufptr = buf;
    while ((ch = getc(fp)) != ';')
      if (ch == EOF)
        break;
      else if (bufptr < (buf + sizeof(buf) - 1))
        *bufptr++ = ch;
    *bufptr = '\0';

    p[num_params].type         = PARAM_STRING;
    p[num_params].value.string = strdup(buf);
    num_params ++;
  }

  while (!done)
    switch (ch = getc(fp))
    {
      case EOF :
          done = 1;
          break;

      case ',' :
      case ' ' :
      case '\n' :
      case '\r' :
      case '\t' :
          break;

      case '\"' :
          fscanf(fp, "%262143[^\"]\"", buf);
          if (num_params < MAX_PARAMS)
          {
            p[num_params].type         = PARAM_STRING;
            p[num_params].value.string = strdup(buf);
            num_params ++;
          };
          break;

      case '-' :
      case '+' :
          ungetc(ch, fp);
          if (fscanf(fp, "%f", &temp) == 1 && num_params < MAX_PARAMS)
          {
            p[num_params].type         = PARAM_RELATIVE;
            p[num_params].value.number = temp;
            num_params ++;
          }
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
          ungetc(ch, fp);
          if (fscanf(fp, "%f", &temp) == 1 && num_params < MAX_PARAMS)
          {
            p[num_params].type         = PARAM_ABSOLUTE;
            p[num_params].value.number = temp;
            num_params ++;
          }
          break;
      default :
          ungetc(ch, fp);
          done = 1;
          break;
    }

  *params = p;
  return (num_params);
}


/*
 * 'FreeParameters()' - Free all string parameter values.
 */

void
FreeParameters(int     num_params,	/* I - Number of parameters */
               param_t *params)		/* I - Parameter values */
{
  int	i;				/* Looping var */


  for (i = 0; i < num_params; i ++)
    if (params[i].type == PARAM_STRING)
      free(params[i].value.string);
}


/*
 * End of "$Id: hpgl-input.c 7219 2008-01-14 22:00:02Z mike $".
 */
