/*
 * "$Id$"
 *
 *   RFC file to HTML conversion program.
 *
 *   Copyright 2006 by Easy Software Products.
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
 *       Hollywood, Maryland 20636 USA
 *
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 *   main()       - Convert a man page to HTML.
 *   put_entity() - Put a single character, using entities as needed.
 *   put_line()   - Put a whole string for a line.
 */

/*
 * Include necessary headers.
 */

#include <cups/string.h>
#include <stdlib.h>
#include <cups/file.h>


/*
 * Local functions...
 */

void	put_entity(cups_file_t *fp, int ch);
void	put_line(cups_file_t *fp, const char *line);


/*
 * 'main()' - Convert a man page to HTML.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  cups_file_t	*infile,		/* Input file */
		*outfile;		/* Output file */
  char		line[1024],		/* Line from file */
		*lineptr,		/* Pointer into line */
		name[1024],		/* Heading anchor name */
		*nameptr;		/* Pointer into anchor name */
  int		rfc,			/* RFC # */
		inheading,		/* Inside a heading? */
		inpre,			/* Inside preformatted text? */
		intoc,			/* Inside table-of-contents? */
		toclevel,		/* Current table-of-contents level */
		linenum;		/* Current line on page */


 /*
  * Check arguments...
  */

  if (argc > 3)
  {
    fputs("Usage: rfctohtml [rfcNNNN.txt [rfcNNNN.html]]\n", stderr);
    return (1);
  }

 /*
  * Open files as needed...
  */

  if (argc > 1)
  {
    if ((infile = cupsFileOpen(argv[1], "r")) == NULL)
    {
      perror(argv[1]);
      return (1);
    }
  }
  else
    infile = cupsFileOpenFd(0, "r");

  if (argc > 2)
  {
    if ((outfile = cupsFileOpen(argv[2], "w")) == NULL)
    {
      perror(argv[2]);
      cupsFileClose(infile);
      return (1);
    }
  }
  else
    outfile = cupsFileOpenFd(1, "w");

 /*
  * Read from input and write the output...
  */

  cupsFilePuts(outfile,
	       "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\" "
	       "\"http://www.w3.org/TR/REC-html40/loose.dtd\">\n"
	       "<html>\n"
	       "<!-- SECTION: Specifications -->\n"
	       "<head>\n"
	       "\t<style type='text/css'><!--\n"
	       "\th1, h2, h3 { font-family: sans-serif; }\n"
	       "\tp, pre { font-family: monospace; }\n"
	       "\th2.title, h3.title, h3.title { border-bottom: solid "
	       "2px #000000; }\n"
	       "\t--></style>\n");

 /*
  * Skip the initial header stuff (working group ID, RFC #, authors, and
  * copyright...
  */

  linenum = 0;
  rfc     = 0;

  while (cupsFileGets(infile, line, sizeof(line)))
  {
    linenum ++;

    if (line[0])
      break;
  }

  while (cupsFileGets(infile, line, sizeof(line)))
  {
    linenum ++;

    if (!line[0])
      break;
    else if (!strncasecmp(line, "Request for Comments:", 21))
      rfc = atoi(line + 21);
  }

 /*
  * Read the document title...
  */

  while (cupsFileGets(infile, line, sizeof(line)))
  {
    linenum ++;

    if (line[0])
      break;
  }

  for (lineptr = line; isspace(*lineptr & 255); lineptr ++);

  cupsFilePrintf(outfile, "<title>RFC %d: %s", rfc, lineptr);

  while (cupsFileGets(infile, line, sizeof(line)))
  {
    linenum ++;

    if (!line[0])
      break;
    else
    {
      for (lineptr = line; isspace(*lineptr & 255); lineptr ++);
      cupsFilePrintf(outfile, " %s", lineptr);
    }
  }

  cupsFilePuts(outfile, "</title>\n"
		        "</head>\n"
		        "<body>\n");

 /*
  * Read the rest of the file...
  */

  inheading = 0;
  inpre     = 0;
  intoc     = 0;
  toclevel  = 0;

  while (cupsFileGets(infile, line, sizeof(line)))
  {
    linenum ++;

    if (!line[0])
    {
      if (linenum > 50)
        continue;

      if (inpre)
      {
	cupsFilePuts(outfile, "</pre>\n");
	inpre = 0;
      }

      if (inheading)
      {
	if (inheading < 0)
	  cupsFilePuts(outfile, "</h1>\n");
	else
	  cupsFilePrintf(outfile, "</a></h%d>\n", inheading);

        inheading = 0;
      }
    }
    else if ((line[0] == ' ' ||
              (!isupper(line[0] & 255) && !isdigit(line[0] & 255) &&
	       !strstr(line, "[Page "))) && !inheading)
    {
      if (inheading)
      {
	if (inheading < 0)
	  cupsFilePuts(outfile, "</h1>\n");
	else
	  cupsFilePrintf(outfile, "</a></h%d>\n", inheading);

        inheading = 0;
      }

      for (lineptr = line; *lineptr == ' '; lineptr ++);

      if (intoc)
      {
        char	*temp;			/* Temporary pointer into line */
	int	level;			/* Heading level */


        if (isdigit(*lineptr & 255))
	{
	  strlcpy(name, lineptr, sizeof(name));

	  for (nameptr = name, level = -1; *nameptr;)
	    if (isdigit(*nameptr & 255))
	    {
	      while (isdigit(*nameptr & 255))
		nameptr ++;

	      level ++;
	    }
	    else if (*nameptr == ' ')
	    {
	      *nameptr = '\0';
	      break;
	    }
	    else
	      nameptr ++;

	  while (toclevel > level)
	  {
	    cupsFilePuts(outfile, "\n</ul>");
	    toclevel --;
	  }

	  while (toclevel < level)
	  {
	    cupsFilePuts(outfile, "\n<ul style=\"list-style-type: none;\">\n");
	    toclevel ++;
	  }

	  cupsFilePrintf(outfile, "\n<%s><a href=\"#s%s\">", toclevel ? "li" : "p",
	                 name);
	}

        temp = lineptr + strlen(lineptr) - 1;

	while (temp > lineptr)
	  if (*temp == ' ' || !isdigit(*temp & 255))
	    break;
	  else
	    temp --;

        if (*temp == ' ')
	{
	  while (temp > lineptr)
	    if (*temp != ' ' && *temp != '.')
	      break;
	    else
	      *temp-- = '\0';
	}
	else
	  temp = NULL;

        if (isdigit(*lineptr & 255))
          put_line(outfile, lineptr);
	else
	  put_line(outfile, lineptr - 1);

        if (temp)
	  cupsFilePuts(outfile, "</a>");
      }
      else if (!inpre)
      {
	cupsFilePuts(outfile, "\n<pre>");
	put_line(outfile, line);
	inpre = 1;
      }
      else
      {
	cupsFilePutChar(outfile, '\n');
	put_line(outfile, line);
      }
    }
    else if (strstr(line, "[Page "))
    {
     /*
      * Skip page footer and header...
      */

      cupsFileGets(infile, line, sizeof(line));
      cupsFileGets(infile, line, sizeof(line));
      cupsFileGets(infile, line, sizeof(line));
      cupsFileGets(infile, line, sizeof(line));
      linenum = 3;
    }
    else if (isdigit(line[0] & 255) && !inheading)
    {
      int level;			/* Heading level */


      if (intoc)
      {
        while (toclevel > 0)
	{
	  cupsFilePuts(outfile, "\n</ul>");
	  toclevel --;
	}

	cupsFilePutChar(outfile, '\n');
	intoc = 0;
      }

      if (inpre)
      {
	cupsFilePuts(outfile, "</pre>\n");
	inpre = 0;
      }

      strlcpy(name, line, sizeof(name));
      for (nameptr = name, level = 1; *nameptr;)
        if (isdigit(*nameptr & 255))
	{
	  while (isdigit(*nameptr & 255))
	    nameptr ++;

          level ++;
	}
	else if (*nameptr == ' ')
	{
	  *nameptr = '\0';
	  break;
	}
	else
	  nameptr ++;

      cupsFilePrintf(outfile, "\n<h%d class='title'><a name='s%s'>", level,
                     name);
      put_line(outfile, line);

      intoc     = 0;
      inheading = level;
    }
    else
    {
      if (intoc)
      {
        while (toclevel > 0)
	{
	  cupsFilePuts(outfile, "\n</ul>");
	  toclevel --;
	}

	cupsFilePutChar(outfile, '\n');
	intoc = 0;
      }

      if (!inheading)
      {
        cupsFilePuts(outfile, "\n<h2 class='title'>");
        inheading = -1;
      }

      put_line(outfile, line);

      intoc    = !strcasecmp(line, "Table of Contents");
      toclevel = 0;
    }
  }

  if (inpre)
    cupsFilePuts(outfile, "</pre>\n");

  if (inheading)
  {
    if (inheading < 0)
      cupsFilePuts(outfile, "</h2>\n");
    else
      cupsFilePrintf(outfile, "</a></h%d>\n", inheading);
  }

  cupsFilePuts(outfile, "</body>\n"
                        "</html>\n");

 /*
  * Close files...
  */

  cupsFileClose(infile);
  cupsFileClose(outfile);

 /*
  * Return with no errors...
  */

  return (0);
}


/*
 * 'put_entity()' - Put a single character, using entities as needed.
 */

void
put_entity(cups_file_t *fp,		/* I - File */
           int         ch)		/* I - Character */
{
  if (ch == '&')
    cupsFilePuts(fp, "&amp;");
  else if (ch == '<')
    cupsFilePuts(fp, "&lt;");
  else
    cupsFilePutChar(fp, ch);
}


/*
 * 'put_line()' - Put a whole string for a line.
 */

void
put_line(cups_file_t *fp,		/* I - File */
         const char  *s)		/* I - String */
{
  int	whitespace,			/* Saw whitespace */
	i,				/* Looping var */
	len;				/* Length of keyword */
  static const char * const keywords[] =/* Special keywords to boldface */
  {
    "MAY",
    "MUST",
    "NOT",
    "SHALL",
    "SHOULD"
  };


  whitespace = 1;

  while (*s)
  {
    if (*s == ' ')
      whitespace = 1;

    if (whitespace && isupper(*s & 255))
    {
      whitespace = 0;

      for (i = 0; i < (int)(sizeof(keywords) / sizeof(sizeof(keywords[0]))); i ++)
      {
        len = strlen(keywords[i]);
	if (!strncmp(s, keywords[i], len) && (isspace(s[len] & 255) || !*s))
	{
	  cupsFilePrintf(fp, "<b>%s</b>", keywords[i]);
	  s += len;
	  break;
	}
      }

      if (i >= (int)(sizeof(keywords) / sizeof(sizeof(keywords[0]))))
        put_entity(fp, *s++);
    }
    else
    {
      if (*s != ' ')
        whitespace = 0;

      put_entity(fp, *s++);
    }
  }
}


/*
 * End of "$Id$".
 */
