/*
 * "$Id$"
 *
 *   Man page to HTML conversion program.
 *
 *   Copyright 2004-2006 by Easy Software Products.
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
 *   main()        - Convert a man page to HTML.
 *   putc_entity() - Put a single character, using entities as needed.
 *   strmove()     - Move characters within a string.
 */

/*
 * Include necessary headers.
 */

#include <cups/string.h>
#include <stdlib.h>


/*
 * Local functions...
 */

static void	putc_entity(int ch, FILE *fp);
static void	strmove(char *d, const char *s);


/*
 * 'main()' - Convert a man page to HTML.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  FILE		*infile,		/* Input file */
		*outfile;		/* Output file */
  char		line[1024],		/* Line from file */
		*lineptr,		/* Pointer into line */
		*endptr,		/* Pointer to end of current */
		endchar,		/* End character */
		*paren,			/* Pointer to parenthesis */
		name[1024];		/* Man page name */
  int		section,		/* Man page section */
		pre,			/* Preformatted */
		font,			/* Current font */
		blist,			/* In a bullet list? */
		list,			/* In a list? */
		linenum;		/* Current line number */
  const char 	*post;			/* Text to add after the current line */
  static const char			/* Start/end tags for fonts */
	* const start_fonts[] = { "", "<b>", "<i>" },
	* const end_fonts[] = { "", "</b>", "</i>" };

 /*
  * Check arguments...
  */

  if (argc > 3)
  {
    fputs("Usage: mantohtml [filename.man [filename.html]]\n", stderr);
    return (1);
  }

 /*
  * Open files as needed...
  */

  if (argc > 1)
  {
    if ((infile = fopen(argv[1], "r")) == NULL)
    {
      perror(argv[1]);
      return (1);
    }
  }
  else
    infile = stdin;

  if (argc > 2)
  {
    if ((outfile = fopen(argv[2], "w")) == NULL)
    {
      perror(argv[2]);
      fclose(infile);
      return (1);
    }
  }
  else
    outfile = stdout;

 /*
  * Read from input and write the output...
  */

  fputs("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\" "
        "\"http://www.w3.org/TR/REC-html40/loose.dtd\">\n"
        "<html>\n"
	"<!-- SECTION: Man Pages -->\n"
	"<head>\n"
	"\t<style type='text/css'><!--\n"
	"\th1, h2, h3, p { font-family: sans-serif; text-align: justify; }\n"
	"\ttt, pre a:link, pre a:visited, tt a:link, tt a:visited { font-weight: bold; color: #7f0000; }\n"
	"\tpre { font-weight: bold; color: #7f0000; margin-left: 2em; }\n"
	"\th1.title, h2.title, h3.title { border-bottom: solid 2px #000000; }\n"
	"\t--></style>\n", outfile);

  blist   = 0;
  font    = 0;
  list    = 0;
  linenum = 0;
  pre     = 0;
  post    = NULL;
  section = -1;

  while (fgets(line, sizeof(line), infile))
  {
    linenum ++;

    if (line[0] == '.')
    {
     /*
      * Strip leading whitespace...
      */

      while (line[1] == ' ' || line[1] == '\t')
        strmove(line + 1, line + 2);

     /*
      * Process man page commands...
      */

      if (!strncmp(line, ".TH ", 4) && section < 0)
      {
       /*
        * Grab man page title...
	*/

        sscanf(line + 4, "%s%d", name, &section);

        fprintf(outfile,
	        "\t<title>%s(%d)</title>\n"
	        "</head>\n"
	        "<body>\n"
		"%s",
	        name, section, start_fonts[font]);
      }
      else if (section < 0)
        continue;
      else if (!strncmp(line, ".SH ", 4) || !strncmp(line, ".Sh ", 4))
      {
       /*
        * Grab heading...
	*/

        int first = 1;

	fputs(end_fonts[font], outfile);

        if (blist)
	{
	  fputs("</li>\n</ul>\n", outfile);
	  blist = 0;
	}

        if (list)
	{
	  if (list == 1)
	    fputs("</dt>\n", outfile);
	  else if (list)
	    fputs("</dd>\n", outfile);

	  fputs("</dl>\n", outfile);
	  list = 0;
	}

        line[strlen(line) - 1] = '\0';	/* Strip LF */

        if (line[2] == 'H')
	  fputs("<h2><a name='", outfile);
	else
	  fputs("<h3><a name='", outfile);

        for (lineptr = line + 4; *lineptr; lineptr ++)
	  if (*lineptr == '\"')
	    continue;
	  else if (*lineptr == ' ')
	    putc_entity('_', outfile);
	  else
	    putc_entity(*lineptr, outfile);

	fputs("'>", outfile);

        for (lineptr = line + 4; *lineptr; lineptr ++)
	  if (*lineptr == '\"')
	    continue;
	  else if (*lineptr == ' ')
	  {
	    putc_entity(' ', outfile);

            first = 1;
	  }
	  else
	  {
	    if (first)
	      putc_entity(*lineptr, outfile);
	    else
	      putc_entity(tolower(*lineptr), outfile);

            first = 0;
          }

        if (line[2] == 'H')
	  fprintf(outfile, "</a></h2>\n%s", start_fonts[font]);
	else
	  fprintf(outfile, "</a></h3>\n%s", start_fonts[font]);
      }
      else if (!strncmp(line, ".LP", 3) || !strncmp(line, ".PP", 3))
      {
       /*
        * New paragraph...
	*/

	fputs(end_fonts[font], outfile);

        if (blist)
	{
	  fputs("</li>\n</ul>\n", outfile);
	  blist = 0;
	}

        if (list)
	{
	  if (list == 1)
	    fputs("</dt>\n", outfile);
	  else if (list)
	    fputs("</dd>\n", outfile);

	  fputs("</dl>\n", outfile);
	  list = 0;
	}

	fputs("<p>", outfile);
	font = 0;
      }
      else if (!strncmp(line, ".TP ", 4))
      {
       /*
        * Grab list...
	*/

	fputs(end_fonts[font], outfile);

        if (blist)
	{
	  fputs("</li>\n</ul>\n", outfile);
	  blist = 0;
	}

        if (!list)
	  fputs("<dl>\n", outfile);
	else if (list == 1)
	  fputs("</dt>\n", outfile);
	else if (list)
	  fputs("</dd>\n", outfile);

	fputs("<dt>", outfile);
	list = 1;
	font = 0;
      }
      else if (!strncmp(line, ".br", 3))
      {
       /*
        * Grab line break...
	*/

        if (list == 1)
	{
	  fputs("</dt>\n<dd>", outfile);
	  list = 2;
	}
        else if (list)
	  fputs("</dd>\n<dd>", outfile);
	else
	  fputs("<br>\n", outfile);
      }
      else if (!strncmp(line, ".de ", 4))
      {
       /*
        * Define macro - ignore...
	*/

        while (fgets(line, sizeof(line), infile))
	{
	  linenum ++;

	  if (!strncmp(line, "..", 2))
	    break;
	}
      }
      else if (!strncmp(line, ".RS", 3))
      {
       /*
        * Indent...
	*/

        fputs("<div style='margin-left: 3em;'>\n", outfile);
      }
      else if (!strncmp(line, ".RE", 3))
      {
       /*
        * Unindent...
	*/

        fputs("</div>\n", outfile);
      }
      else if (!strncmp(line, ".ds ", 4) || !strncmp(line, ".rm ", 4) ||
               !strncmp(line, ".tr ", 4) || !strncmp(line, ".hy ", 4) ||
               !strncmp(line, ".IX ", 4) || !strncmp(line, ".PD", 3) ||
	       !strncmp(line, ".Sp", 3))
      {
       /*
        * Ignore unused commands...
	*/
      }
      else if (!strncmp(line, ".Vb", 3) || !strncmp(line, ".nf", 3))
      {
       /*
        * Start preformatted...
	*/

        pre = 1;
	fputs("<pre>\n", outfile);
      }
      else if (!strncmp(line, ".Ve", 3) || !strncmp(line, ".fi", 3))
      {
       /*
        * End preformatted...
	*/

        if (pre)
	{
          pre = 0;
	  fputs("</pre>\n", outfile);
	}
      }
      else if (!strncmp(line, ".IP \\(bu", 8))
      {
       /*
        * Bullet list...
	*/

        if (blist)
	  fputs("</li>\n", outfile);
	else
	{
	  fputs("<ul>\n", outfile);
	  blist = 1;
	}

	fputs("<li>", outfile);
      }
      else if (!strncmp(line, ".IP ", 4))
      {
       /*
        * Indented paragraph...
	*/

        if (blist)
	{
	  fputs("</li>\n</ul>\n", outfile);
	  blist = 0;
	}

	fputs("<p style='margin-left: 3em;'>", outfile);

        for (lineptr = line + 4; isspace(*lineptr); lineptr ++);

        if (*lineptr == '\"')
	{
	  strmove(line, lineptr + 1);

	  if ((lineptr = strchr(line, '\"')) != NULL)
	    *lineptr = '\0';
        }
	else
	{
	  strmove(line, lineptr);

	  if ((lineptr = strchr(line, ' ')) != NULL)
	    *lineptr = '\0';
        }

       /*
        * Process the text as if it was in-line...
	*/

        post = "\n<br />\n<br />";
        goto process_text;
      }
      else if (!strncmp(line, ".\\}", 3))
      {
       /*
        * Ignore close block...
	*/
      }
      else if (!strncmp(line, ".ie", 3) || !strncmp(line, ".if", 3) ||
               !strncmp(line, ".el", 3))
      {
       /*
        * If/else - ignore...
	*/

        if (strchr(line, '{') != NULL)
	{
	 /*
	  * Skip whole block...
	  */

          while (fgets(line, sizeof(line), infile))
	  {
	    linenum ++;

	    if (strchr(line, '}') != NULL)
	      break;
          }
	}
      }
#if 0
      else if (!strncmp(line, ". ", 4))
      {
       /*
        * Grab ...
	*/
      }
#endif /* 0 */
      else if (!strncmp(line, ".B ", 3))
      {
       /*
        * Grab bold text...
	*/

	fprintf(outfile, "%s<b>%s</b>%s", end_fonts[font], line + 3,
	        start_fonts[font]);
      }
      else if (!strncmp(line, ".I ", 3))
      {
       /*
        * Grab italic text...
	*/

	fprintf(outfile, "%s<i>%s</i>%s", end_fonts[font], line + 3,
	        start_fonts[font]);
      }
      else if (strncmp(line, ".\\\"", 3))
      {
       /*
        * Unknown...
	*/

        if ((lineptr = strchr(line, ' ')) != NULL)
	  *lineptr = '\0';
	else if ((lineptr = strchr(line, '\n')) != NULL)
	  *lineptr = '\0';

        fprintf(stderr, "mantohtml: Unknown man page command \'%s\' on line %d!\n",
	        line, linenum);
      }

     /*
      * Skip continuation lines...
      */

      lineptr = line + strlen(line) - 2;
      if (lineptr >= line && *lineptr == '\\')
      {
        while (fgets(line, sizeof(line), infile))
	{
	  linenum ++;
	  lineptr = line + strlen(line) - 2;

	  if (lineptr < line || *lineptr != '\\')
	    break;
	}
      }
    }
    else
    {
     /*
      * Process man page text...
      */

process_text:

      for (lineptr = line; *lineptr; lineptr ++)
      {
        if (!strncmp(lineptr, "http://", 7))
	{
	 /*
	  * Embed URL...
	  */

          for (endptr = lineptr + 7;
	       *endptr && !isspace(*endptr & 255);
	       endptr ++);

          endchar = *endptr;
	  *endptr = '\0';

          fprintf(outfile, "<a href='%s'>%s</a>", lineptr, lineptr);
	  *endptr = endchar;
	  lineptr = endptr - 1;
	}
	else if (!strncmp(lineptr, "\\fI", 3) &&
	         (endptr = strstr(lineptr, "\\fR")) != NULL &&
		 (paren = strchr(lineptr, '(')) != NULL &&
		 paren < endptr)
        {
	 /*
	  * Link to man page?
	  */

          char	manfile[1024],		/* Man page filename */
		manurl[1024];		/* Man page URL */


         /*
	  * See if the man file is available locally...
	  */

          lineptr += 3;
	  endchar = *paren;
	  *paren  = '\0';

	  snprintf(manfile, sizeof(manfile), "%s.man", lineptr);
	  snprintf(manurl, sizeof(manurl), "man-%s.html?TOPIC=Man+Pages",
	           lineptr);

	  *paren  = endchar;
	  endchar = *endptr;
	  *endptr = '\0';

	  if (access(manfile, 0))
	  {
	   /*
	    * Not a local man page, just do it italic...
	    */

	    fputs("<i>", outfile);
	    while (*lineptr)
	      putc_entity(*lineptr++, outfile);
	    fputs("</i>", outfile);
	  }
	  else
	  {
	   /*
	    * Local man page, do a link...
	    */

	    fprintf(outfile, "<a href='%s'>", manurl);
	    while (*lineptr)
	      putc_entity(*lineptr++, outfile);
	    fputs("</a>", outfile);
	  }

          *endptr = endchar;
	  lineptr = endptr + 2;
	}
        else if (*lineptr == '\\')
	{
	  lineptr ++;
	  if (!*lineptr)
	    break;
	  else if (isdigit(lineptr[0]) && isdigit(lineptr[1]) &&
	           isdigit(lineptr[2]))
	  {
	    fprintf(outfile, "&#%d;", ((lineptr[0] - '0') * 8 +
	                               lineptr[1] - '0') * 8 +
				      lineptr[2] - '0');
	    lineptr += 2;
	  }
	  else if (*lineptr == '&')
	    continue;
	  else if (*lineptr == 's')
	  {
	    while (lineptr[1] == '-' || isdigit(lineptr[1]))
	      lineptr ++;
	  }
	  else if (*lineptr == '*')
	  {
	    lineptr += 2;
	  }
	  else if (*lineptr != 'f')
	    putc_entity(*lineptr, outfile);
	  else
	  {
	    lineptr ++;
	    if (!*lineptr)
	      break;
	    else
	    {
	      fputs(end_fonts[font], outfile);

	      switch (*lineptr)
	      {
	        default : /* Regular */
		    font = 0;
		    break;
	        case 'B' : /* Bold */
		case 'b' :
		    font = 1;
		    break;
	        case 'I' : /* Italic */
		case 'i' :
		    font = 2;
		    break;
	      }

	      fputs(start_fonts[font], outfile);
	    }
	  }
	}
	else
	  putc_entity(*lineptr, outfile);
      }

      if (post)
      {
        fputs(post, outfile);
	post = NULL;
      }
    }
  }

  fprintf(outfile, "%s\n", end_fonts[font]);

  if (blist)
  {
    fputs("</li>\n</ul>\n", outfile);
    blist = 0;
  }

  if (list)
  {
    if (list == 1)
      fputs("</dt>\n", outfile);
    else if (list)
      fputs("</dd>\n", outfile);

    fputs("</dl>\n", outfile);
    list = 0;
  }

  fputs("</body>\n"
        "</html>\n", outfile);

 /*
  * Close files...
  */

  if (infile != stdin)
    fclose(infile);

  if (outfile != stdout)
    fclose(outfile);

 /*
  * Return with no errors...
  */

  return (0);
}


/*
 * 'putc_entity()' - Put a single character, using entities as needed.
 */

static void
putc_entity(int  ch,			/* I - Character */
            FILE *fp)			/* I - File */
{
  if (ch == '&')
    fputs("&amp;", fp);
  else if (ch == '<')
    fputs("&lt;", fp);
  else
    putc(ch, fp);
}


/*
 * 'strmove()' - Move characters within a string.
 */

static void
strmove(char       *d,			/* I - Destination */
        const char *s)			/* I - Source */
{
  while (*s)
    *d++ = *s++;

  *d = '\0';
}


/*
 * End of "$Id$".
 */
