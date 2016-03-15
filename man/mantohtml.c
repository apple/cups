/*
 * "$Id: mantohtml.c 12362 2014-12-12 19:50:49Z msweet $"
 *
 * Man page to HTML conversion program.
 *
 * Copyright 2007-2010, 2014 by Apple Inc.
 * Copyright 2004-2006 by Easy Software Products.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * file is missing or damaged, see the license at "http://www.cups.org/".
 */

/*
 * Include necessary headers.
 */

#include <cups/string-private.h>
#include <cups/array-private.h>
#include <unistd.h>


/*
 * Local globals...
 */

static const char			/* Start/end tags for fonts */
	* const start_fonts[] = { "", "<b>", "<i>" },
	* const end_fonts[] = { "", "</b>", "</i>" };


/*
 * Local functions...
 */

static void	html_alternate(const char *s, const char *first, const char *second, FILE *fp);
static void	html_fputs(const char *s, int *font, FILE *fp);
static void	html_putc(int ch, FILE *fp);
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
		anchor[1024],		/* Anchor */
		name[1024],		/* Man page name */
		ddpost[256];		/* Tagged list post markup */
  int		section = -1,		/* Man page section */
		pre = 0,		/* Preformatted */
		font = 0,		/* Current font */
		linenum = 0;		/* Current line number */
  float		list_indent = 0.0f,	/* Current list indentation */
		nested_indent = 0.0f;	/* Nested list indentation, if any */
  const char	*list = NULL,		/* Current list, if any */
		*nested = NULL;		/* Nested list, if any */
  const char 	*post = NULL;		/* Text to add after the current line */


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

  fputs("<!DOCTYPE HTML>\n"
        "<html>\n"
	"<!-- SECTION: Man Pages -->\n"
	"<head>\n"
	"\t<link rel=\"stylesheet\" type=\"text/css\" "
	"href=\"../cups-printable.css\">\n", outfile);

  anchor[0] = '\0';

  while (fgets(line, sizeof(line), infile))
  {
    size_t linelen = strlen(line);	/* Length of line */

    if (linelen > 0 && line[linelen - 1] == '\n')
      line[linelen - 1] = '\0';

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
		"<h1 class=\"title\">%s(%d)</h1>\n"
		"%s",
	        name, section, name, section, start_fonts[font]);
      }
      else if (section < 0)
        continue;
      else if (!strncmp(line, ".SH ", 4) || !strncmp(line, ".SS ", 4))
      {
       /*
        * Grab heading...
	*/

        int first = 1;

	fputs(end_fonts[font], outfile);
	font = 0;

        if (list)
	{
	  fprintf(outfile, "</%s>\n", list);
	  list = NULL;
	}

        if (line[2] == 'H')
	  fputs("<h2 class=\"title\"><a name=\"", outfile);
	else
	  fputs("<h3><a name=\"", outfile);

        if (anchor[0])
        {
          fputs(anchor, outfile);
          anchor[0] = '\0';
        }
        else
        {
	  for (lineptr = line + 4; *lineptr; lineptr ++)
	    if (*lineptr  == '\"')
	      continue;
	    else if (isalnum(*lineptr & 255))
	      html_putc(*lineptr, outfile);
	    else
	      html_putc('_', outfile);
        }

	fputs("\">", outfile);

        for (lineptr = line + 4; *lineptr; lineptr ++)
        {
	  if (*lineptr == '\"')
	    continue;
	  else if (*lineptr == ' ')
	  {
	    html_putc(' ', outfile);

            first = 1;
	  }
	  else
	  {
	    if (first)
	      html_putc(*lineptr, outfile);
	    else
	      html_putc(tolower(*lineptr & 255), outfile);

            first = 0;
          }
        }

        if (line[2] == 'H')
	  fputs("</a></h2>\n", outfile);
	else
	  fputs("</a></h3>\n", outfile);
      }
      else if (!strncmp(line, ".B ", 3))
      {
       /*
        * Grab bold text...
	*/

	fputs(end_fonts[font], outfile);
	font = 0;

        if (anchor[0])
          fprintf(outfile, "<a name=\"%s\">", anchor);

        html_alternate(line + 3, "b", "b", outfile);

        if (anchor[0])
        {
          fputs("</a>", outfile);
          anchor[0] = '\0';
        }

	if (post)
	{
	  fputs(post, outfile);
	  post = NULL;
	}
      }
      else if (!strncmp(line, ".I ", 3))
      {
       /*
        * Grab italic text...
	*/

	fputs(end_fonts[font], outfile);
	font = 0;

        if (anchor[0])
          fprintf(outfile, "<a name=\"%s\">", anchor);

        html_alternate(line + 3, "i", "i", outfile);

        if (anchor[0])
        {
          fputs("</a>", outfile);
          anchor[0] = '\0';
        }

	if (post)
	{
	  fputs(post, outfile);
	  post = NULL;
	}
      }
      else if (!strncmp(line, ".BI ", 4))
      {
       /*
        * Alternating bold and italic text...
        */

	fputs(end_fonts[font], outfile);
	font = 0;

        if (anchor[0])
          fprintf(outfile, "<a name=\"%s\">", anchor);

        html_alternate(line + 4, "b", "i", outfile);

        if (anchor[0])
        {
          fputs("</a>", outfile);
          anchor[0] = '\0';
        }

	if (post)
	{
	  fputs(post, outfile);
	  post = NULL;
	}
      }
      else if (!strncmp(line, ".BR ", 4))
      {
       /*
        * Alternating bold and roman (plain) text...
        */

	fputs(end_fonts[font], outfile);
	font = 0;

        if (anchor[0])
          fprintf(outfile, "<a name=\"%s\">", anchor);

        html_alternate(line + 4, "b", NULL, outfile);

        if (anchor[0])
        {
          fputs("</a>", outfile);
          anchor[0] = '\0';
        }

	if (post)
	{
	  fputs(post, outfile);
	  post = NULL;
	}
      }
      else if (!strncmp(line, ".IB ", 4))
      {
       /*
        * Alternating italic and bold text...
        */

	fputs(end_fonts[font], outfile);
	font = 0;

        if (anchor[0])
          fprintf(outfile, "<a name=\"%s\">", anchor);

        html_alternate(line + 4, "i", "b", outfile);

        if (anchor[0])
        {
          fputs("</a>", outfile);
          anchor[0] = '\0';
        }

	if (post)
	{
	  fputs(post, outfile);
	  post = NULL;
	}
      }
      else if (!strncmp(line, ".IR ", 4))
      {
       /*
        * Alternating italic and roman (plain) text...
        */

	fputs(end_fonts[font], outfile);
	font = 0;

        if (anchor[0])
          fprintf(outfile, "<a name=\"%s\">", anchor);

        html_alternate(line + 4, "i", NULL, outfile);

        if (anchor[0])
        {
          fputs("</a>", outfile);
          anchor[0] = '\0';
        }

	if (post)
	{
	  fputs(post, outfile);
	  post = NULL;
	}
      }
      else if (!strncmp(line, ".RB ", 4))
      {
       /*
        * Alternating roman (plain) and bold text...
        */

	fputs(end_fonts[font], outfile);
	font = 0;

        if (anchor[0])
          fprintf(outfile, "<a name=\"%s\">", anchor);

        html_alternate(line + 4, NULL, "b", outfile);

        if (anchor[0])
        {
          fputs("</a>", outfile);
          anchor[0] = '\0';
        }

	if (post)
	{
	  fputs(post, outfile);
	  post = NULL;
	}
      }
      else if (!strncmp(line, ".RI ", 4))
      {
       /*
        * Alternating roman (plain) and italic text...
        */

	fputs(end_fonts[font], outfile);
	font = 0;

        if (anchor[0])
          fprintf(outfile, "<a name=\"%s\">", anchor);

        html_alternate(line + 4, NULL, "i", outfile);

        if (anchor[0])
        {
          fputs("</a>", outfile);
          anchor[0] = '\0';
        }

	if (post)
	{
	  fputs(post, outfile);
	  post = NULL;
	}
      }
      else if (!strncmp(line, ".SB ", 4))
      {
       /*
        * Alternating small and bold text...
        */

	fputs(end_fonts[font], outfile);
	font = 0;

        if (anchor[0])
          fprintf(outfile, "<a name=\"%s\">", anchor);

        html_alternate(line + 4, "small", "b", outfile);

        if (anchor[0])
        {
          fputs("</a>", outfile);
          anchor[0] = '\0';
        }

	if (post)
	{
	  fputs(post, outfile);
	  post = NULL;
	}
      }
      else if (!strncmp(line, ".SM ", 4))
      {
       /*
        * Small text...
        */

	fputs(end_fonts[font], outfile);
	font = 0;

        if (anchor[0])
          fprintf(outfile, "<a name=\"%s\">", anchor);

        html_alternate(line + 4, "small", "small", outfile);

        if (anchor[0])
        {
          fputs("</a>", outfile);
          anchor[0] = '\0';
        }

	if (post)
	{
	  fputs(post, outfile);
	  post = NULL;
	}
      }
      else if (!strcmp(line, ".LP") || !strcmp(line, ".PP") || !strcmp(line, ".P"))
      {
       /*
        * New paragraph...
	*/

	fputs(end_fonts[font], outfile);
	font = 0;

        if (list)
        {
          fprintf(outfile, "</%s>\n", list);
          list = NULL;
        }

	fputs("<p>", outfile);

        if (anchor[0])
        {
          fprintf(outfile, "<a name=\"%s\"></a>", anchor);
          anchor[0] = '\0';
        }
      }
      else if (!strcmp(line, ".RS") || !strncmp(line, ".RS ", 4))
      {
       /*
        * Indent...
	*/

	float amount = 3.0;		/* Indentation */

        if (line[3])
          amount = atof(line + 4);

	fputs(end_fonts[font], outfile);
	font = 0;

        if (list)
        {
          nested        = list;
          list          = NULL;
          nested_indent = list_indent;
          list_indent   = 0.0f;
        }

        fprintf(outfile, "<div style=\"margin-left: %.1fem;\">\n", amount - nested_indent);
      }
      else if (!strcmp(line, ".RE"))
      {
       /*
        * Unindent...
	*/

	fputs(end_fonts[font], outfile);
	font = 0;

        fputs("</div>\n", outfile);

        if (nested)
        {
          list   = nested;
          nested = NULL;

          list_indent   = nested_indent;
          nested_indent = 0.0f;
        }
      }
      else if (!strcmp(line, ".HP") || !strncmp(line, ".HP ", 4))
      {
       /*
        * Hanging paragraph...
        *
        * .HP i
	*/

	float amount = 3.0;		/* Indentation */

        if (line[3])
          amount = atof(line + 4);

	fputs(end_fonts[font], outfile);
	font = 0;

        if (list)
        {
          fprintf(outfile, "</%s>\n", list);
          list = NULL;
        }

        fprintf(outfile, "<p style=\"margin-left: %.1fem; text-indent: %.1fem\">", amount, -amount);

        if (anchor[0])
        {
          fprintf(outfile, "<a name=\"%s\"></a>", anchor);
          anchor[0] = '\0';
        }

        if (line[1] == 'T')
          post = "<br>\n";
      }
      else if (!strcmp(line, ".TP") || !strncmp(line, ".TP ", 4))
      {
       /*
        * Tagged list...
        *
        * .TP i
	*/

	float amount = 3.0;		/* Indentation */

        if (line[3])
          amount = atof(line + 4);

	fputs(end_fonts[font], outfile);
	font = 0;

        if (list && strcmp(list, "dl"))
        {
          fprintf(outfile, "</%s>\n", list);
          list = NULL;
        }

        if (!list)
        {
          fputs("<dl class=\"man\">\n", outfile);
          list        = "dl";
          list_indent = amount;
        }

        fputs("<dt>", outfile);
        snprintf(ddpost, sizeof(ddpost), "<dd style=\"margin-left: %.1fem\">", amount);
	post = ddpost;

        if (anchor[0])
        {
          fprintf(outfile, "<a name=\"%s\"></a>", anchor);
          anchor[0] = '\0';
        }
      }
      else if (!strncmp(line, ".IP ", 4))
      {
       /*
        * Indented paragraph...
        *
        * .IP x i
	*/

        float amount = 3.0;		/* Indentation */
        const char *newlist = NULL;	/* New list style */
        const char *newtype = NULL;	/* New list numbering type */

	fputs(end_fonts[font], outfile);
	font = 0;

        lineptr = line + 4;
        while (isspace(*lineptr & 255))
          lineptr ++;

        if (!strncmp(lineptr, "\\(bu", 4) || !strncmp(lineptr, "\\(em", 4))
	{
	 /*
	  * Bullet list...
	  */

          newlist = "ul";
	}
	else if (isdigit(*lineptr & 255))
	{
	 /*
	  * Numbered list...
	  */

          newlist = "ol";
	}
	else if (islower(*lineptr & 255))
	{
	 /*
	  * Lowercase alpha list...
	  */

          newlist = "ol";
          newtype = "a";
	}
	else if (isupper(*lineptr & 255))
	{
	 /*
	  * Lowercase alpha list...
	  */

          newlist = "ol";
          newtype = "A";
	}

        while (!isspace(*lineptr & 255))
          lineptr ++;
        while (isspace(*lineptr & 255))
          lineptr ++;

        if (isdigit(*lineptr & 255))
          amount = atof(lineptr);

        if (newlist && list && strcmp(newlist, list))
        {
          fprintf(outfile, "</%s>\n", list);
          list = NULL;
        }

        if (newlist && !list)
        {
          if (newtype)
            fprintf(outfile, "<%s type=\"%s\">\n", newlist, newtype);
          else
            fprintf(outfile, "<%s>\n", newlist);

          list = newlist;
        }

        if (list)
          fprintf(outfile, "<li style=\"margin-left: %.1fem;\">", amount);
        else
          fprintf(outfile, "<p style=\"margin-left: %.1fem;\">", amount);

        if (anchor[0])
        {
          fprintf(outfile, "<a name=\"%s\"></a>", anchor);
          anchor[0] = '\0';
        }
      }
      else if (!strncmp(line, ".br", 3))
      {
       /*
        * Grab line break...
	*/

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
      else if (!strncmp(line, ".ds ", 4) || !strncmp(line, ".rm ", 4) ||
               !strncmp(line, ".tr ", 4) || !strncmp(line, ".hy ", 4) ||
               !strncmp(line, ".IX ", 4) || !strncmp(line, ".PD", 3) ||
	       !strncmp(line, ".Sp", 3))
      {
       /*
        * Ignore unused commands...
	*/
      }
      else if (!strncmp(line, ".Vb", 3) || !strncmp(line, ".nf", 3) || !strncmp(line, ".EX", 3))
      {
       /*
        * Start preformatted...
	*/

	fputs(end_fonts[font], outfile);
	font = 0;

//        if (list)
//	{
//	  fprintf(outfile, "</%s>\n", list);
//	  list = NULL;
//	}

        pre = 1;
	fputs("<pre class=\"man\">\n", outfile);
      }
      else if (!strncmp(line, ".Ve", 3) || !strncmp(line, ".fi", 3) || !strncmp(line, ".EE", 3))
      {
       /*
        * End preformatted...
	*/

	fputs(end_fonts[font], outfile);
	font = 0;

        if (pre)
	{
          pre = 0;
	  fputs("</pre>\n", outfile);
	}
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
      else if (!strncmp(line, ".\\\"#", 4))
      {
       /*
        * Anchor for HTML output...
        */

        strlcpy(anchor, line + 4, sizeof(anchor));
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

        fprintf(stderr, "mantohtml: Unknown man page command \'%s\' on line %d.\n",  line, linenum);
      }

     /*
      * Skip continuation lines...
      */

      lineptr = line + strlen(line) - 1;
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

      html_fputs(line, &font, outfile);
      putc('\n', outfile);

      if (post)
      {
        fputs(post, outfile);
	post = NULL;
      }
    }
  }

  fprintf(outfile, "%s\n", end_fonts[font]);
  font = 0;

  if (list)
  {
    fprintf(outfile, "</%s>\n", list);
    list = NULL;
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
 * 'html_alternate()' - Alternate words between two styles of text.
 */

static void
html_alternate(const char *s,		/* I - String */
               const char *first,	/* I - First style or NULL */
               const char *second,	/* I - Second style of NULL */
               FILE       *fp)		/* I - File */
{
  int		i = 0;			/* Which style */
  int		quote = 0;		/* Saw quote? */
  int		dolinks,		/* Do hyperlinks to other man pages? */
		link = 0;		/* Doing a link now? */


 /*
  * Skip leading whitespace...
  */

  while (isspace(*s & 255))
    s ++;

  dolinks = first && !strcmp(first, "b") && !second;

  while (*s)
  {
    if (!i && dolinks)
    {
     /*
      * See if we need to make a link to a man page...
      */

      const char *end;			/* End of current word */
      const char *next;			/* Start of next word */

      for (end = s; *end && !isspace(*end & 255); end ++);
      for (next = end; isspace(*next & 255); next ++);

      if (isalnum(*s & 255) && *next == '(')
      {
       /*
	* See if the man file is available locally...
	*/

	char	name[1024],		/* Name */
		manfile[1024],		/* Man page filename */
		manurl[1024];		/* Man page URL */

        strlcpy(name, s, sizeof(name));
        if ((size_t)(end - s) < sizeof(name))
          name[end - s] = '\0';

        snprintf(manfile, sizeof(manfile), "%s.man", name);
	snprintf(manurl, sizeof(manurl), "man-%s.html?TOPIC=Man+Pages", name);

	if (!access(manfile, 0))
	{
	 /*
	  * Local man page, do a link...
	  */

	  fprintf(fp, "<a href=\"%s\">", manurl);
	  link = 1;
	}
      }
    }

    if (!i && first)
      fprintf(fp, "<%s>", first);
    else if (i && second)
      fprintf(fp, "<%s>", second);

    while ((!isspace(*s & 255) || quote) && *s)
    {
      if (*s == '\"')
        quote = !quote;
      else if (*s == '\\' && s[1])
      {
        s ++;
        html_putc(*s++, fp);
      }
      else
        html_putc(*s++, fp);
    }

    if (!i && first)
      fprintf(fp, "</%s>", first);
    else if (i && second)
      fprintf(fp, "</%s>", second);

    if (i && link)
    {
      fputs("</a>", fp);
      link = 0;
    }

    i = 1 - i;

   /*
    * Skip trailing whitespace...
    */

    while (isspace(*s & 255))
      s ++;
  }

  putc('\n', fp);
}

/*
 * 'html_fputs()' - Output a string, quoting as needed HTML entities.
 */

static void
html_fputs(const char *s,		/* I  - String */
           int        *font,		/* IO - Font */
           FILE       *fp)		/* I  - File */
{
  while (*s)
  {
    if (*s == '\\')
    {
      s ++;
      if (!*s)
	break;

      if (*s == 'f')
      {
        int	newfont;		/* New font */

        s ++;
        if (!*s)
          break;

        if (!font)
        {
          s ++;
          continue;
        }

        switch (*s++)
        {
          case 'R' :
          case 'P' :
              newfont = 0;
              break;

          case 'b' :
          case 'B' :
              newfont = 1;
              break;

          case 'i' :
          case 'I' :
              newfont = 2;
              break;

          default :
              fprintf(stderr, "mantohtml: Unknown font \"\\f%c\" ignored.\n", s[-1]);
              newfont = *font;
              break;
        }

        if (newfont != *font)
        {
	  fputs(end_fonts[*font], fp);
	  *font = newfont;
	  fputs(start_fonts[*font], fp);
	}
      }
      else if (*s == '*')
      {
       /*
        * Substitute macro...
        */

        s ++;
        if (!*s)
          break;

        switch (*s++)
        {
          case 'R' :
              fputs("&reg;", fp);
              break;

          case '(' :
	      if (!strncmp(s, "lq", 2))
		fputs("&ldquo;", fp);
	      else if (!strncmp(s, "rq", 2))
		fputs("&rdquo;", fp);
              else if (!strncmp(s, "Tm", 2))
                fputs("<sup>TM</sup>", fp);
              else
                fprintf(stderr, "mantohtml: Unknown macro \"\\*(%2s\" ignored.\n", s);

              if (*s)
                s ++;
              if (*s)
                s ++;
              break;

          default :
              fprintf(stderr, "mantohtml: Unknown macro \"\\*%c\" ignored.\n", s[-1]);
              break;
        }
      }
      else if (*s == '(')
      {
        if (!strncmp(s, "(em", 3))
        {
          fputs("&mdash;", fp);
          s += 3;
        }
        else if (!strncmp(s, "(en", 3))
        {
          fputs("&ndash;", fp);
          s += 3;
        }
        else
        {
          putc(*s, fp);
          s ++;
        }
      }
      else if (*s == '[')
      {
       /*
        * Substitute escaped character...
        */

        s ++;
	if (!strncmp(s, "co]", 3))
	  fputs("&copy;", fp);
	else if (!strncmp(s, "de]", 3))
	  fputs("&deg;", fp);
        else if (!strncmp(s, "rg]", 3))
	  fputs("&reg;", fp);
	else if (!strncmp(s, "tm]", 3))
	  fputs("<sup>TM</sup>", fp);

	if (*s)
	  s ++;
	if (*s)
	  s ++;
	if (*s)
	  s ++;
      }
      else if (isdigit(s[0]) && isdigit(s[1]) &&
	       isdigit(s[2]))
      {
	fprintf(fp, "&#%d;", ((s[0] - '0') * 8 + s[1] - '0') * 8 + s[2] - '0');
	s += 3;
      }
      else
      {
        if (*s != '\\' && *s == '\"' && *s == '\'' && *s == '-')
          fprintf(stderr, "mantohtml: Unrecognized escape \"\\%c\" ignored.\n", *s);

        html_putc(*s++, fp);
      }
    }
    else if (!strncmp(s, "http://", 7) || !strncmp(s, "https://", 8) || !strncmp(s, "ftp://", 6))
    {
     /*
      * Embed URL...
      */

      char temp[1024];			/* Temporary string */
      const char *end = s + 6;		/* End of URL */

      while (*end && !isspace(*end & 255))
	end ++;

      if (end[-1] == ',' || end[-1] == '.' || end[-1] == ')')
        end --;

      strlcpy(temp, s, sizeof(temp));
      if ((size_t)(end -s) < sizeof(temp))
        temp[end - s] = '\0';

      fprintf(fp, "<a href=\"%s\">%s</a>", temp, temp);
      s = end;
    }
    else
      html_putc(*s++ & 255, fp);
  }
}


/*
 * 'html_putc()' - Put a single character, using entities as needed.
 */

static void
html_putc(int  ch,			/* I - Character */
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
 * End of "$Id: mantohtml.c 12362 2014-12-12 19:50:49Z msweet $".
 */
