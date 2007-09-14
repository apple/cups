/*
 * "$Id$"
 *
 *   On-line help CGI for the Common UNIX Printing System (CUPS).
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
 *   main() - Main entry for CGI.
 */

/*
 * Include necessary headers...
 */

#include "cgi-private.h"


/*
 * 'main()' - Main entry for CGI.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  help_index_t	*hi,			/* Help index */
		*si;			/* Search index */
  help_node_t	*n;			/* Current help node */
  int		i;			/* Looping var */
  const char	*query;			/* Search query */
  const char	*cache_dir;		/* CUPS_CACHEDIR environment variable */
  const char	*docroot;		/* CUPS_DOCROOT environment variable */
  const char	*helpfile;		/* Current help file */
  const char	*topic;			/* Current topic */
  char		topic_data[1024];	/* Topic form data */
  const char	*section;		/* Current section */
  char		filename[1024],		/* Filename */
		directory[1024];	/* Directory */
  cups_file_t	*fp;			/* Help file */
  char		line[1024];		/* Line from file */
  int		printable;		/* Show printable version? */


 /*
  * Get any form variables...
  */

  cgiInitialize();

  printable = cgiGetVariable("PRINTABLE") != NULL;

 /*
  * Set the web interface section...
  */

  cgiSetVariable("SECTION", "help");

 /*
  * Load the help index...
  */

  if ((cache_dir = getenv("CUPS_CACHEDIR")) == NULL)
    cache_dir = CUPS_CACHEDIR;

  snprintf(filename, sizeof(filename), "%s/help.index", cache_dir);

  if ((docroot = getenv("CUPS_DOCROOT")) == NULL)
    docroot = CUPS_DOCROOT;

  snprintf(directory, sizeof(directory), "%s/help", docroot);

  fprintf(stderr, "DEBUG: helpLoadIndex(filename=\"%s\", directory=\"%s\")\n",
          filename, directory);

  hi = helpLoadIndex(filename, directory);
  if (!hi)
  {
    perror(filename);

    cgiStartHTML(cgiText(_("Help")));
    cgiSetVariable("ERROR", "Unable to load help index!");
    cgiCopyTemplateLang("error.tmpl");
    cgiEndHTML();

    return (1);
  }

  fprintf(stderr, "DEBUG: %d nodes in help index...\n",
          cupsArrayCount(hi->nodes));

 /*
  * See if we are viewing a file...
  */

  for (i = 0; i < argc; i ++)
    fprintf(stderr, "argv[%d]=\"%s\"\n", i, argv[i]);

  if ((helpfile = getenv("PATH_INFO")) != NULL)
  {
    helpfile ++;

    if (!*helpfile)
      helpfile = NULL;
  }

  if (helpfile)
  {
   /*
    * Verify that the help file exists and is part of the index...
    */

    snprintf(filename, sizeof(filename), "%s/help/%s", docroot, helpfile);

    fprintf(stderr, "DEBUG: helpfile=\"%s\", filename=\"%s\"\n",
            helpfile, filename);

    if (access(filename, R_OK))
    {
      perror(filename);

      cgiStartHTML(cgiText(_("Help")));
      cgiSetVariable("ERROR", "Unable to access help file!");
      cgiCopyTemplateLang("error.tmpl");
      cgiEndHTML();

      return (1);
    }

    if ((n = helpFindNode(hi, helpfile, NULL)) == NULL)
    {
      cgiStartHTML(cgiText(_("Help")));
      cgiSetVariable("ERROR", "Help file not in index!");
      cgiCopyTemplateLang("error.tmpl");
      cgiEndHTML();

      return (1);
    }

   /*
    * Set the page title and save the help file...
    */

    cgiSetVariable("HELPFILE", helpfile);
    cgiSetVariable("HELPTITLE", n->text);
    cgiSetVariable("TOPIC", n->section);

   /*
    * Send a standard page header...
    */

    if (printable)
      puts("Content-Type: text/html;charset=utf-8\n");
    else
      cgiStartHTML(n->text);
  }
  else
  {
   /*
    * Send a standard page header...
    */

    cgiStartHTML(cgiText(_("Help")));
  }

 /*
  * Do a search as needed...
  */

  query = cgiGetVariable("QUERY");
  topic = cgiGetVariable("TOPIC");
  si    = helpSearchIndex(hi, query, topic, helpfile);

  fprintf(stderr, "DEBUG: query=\"%s\", topic=\"%s\"\n",
          query ? query : "(null)", topic ? topic : "(null)");

  if (si)
  {
    help_node_t	*nn;			/* Parent node */


    fprintf(stderr,
            "DEBUG: si=%p, si->sorted=%p, cupsArrayCount(si->sorted)=%d\n", si,
            si->sorted, cupsArrayCount(si->sorted));

    for (i = 0, n = (help_node_t *)cupsArrayFirst(si->sorted);
         n;
	 i ++, n = (help_node_t *)cupsArrayNext(si->sorted))
    {
      if (helpfile && n->anchor)
        snprintf(line, sizeof(line), "#%s", n->anchor);
      else if (n->anchor)
        snprintf(line, sizeof(line), "/help/%s?QUERY=%s#%s", n->filename,
	         query ? query : "", n->anchor);
      else
        snprintf(line, sizeof(line), "/help/%s?QUERY=%s", n->filename,
	         query ? query : "");

      cgiSetArray("QTEXT", i, n->text);
      cgiSetArray("QLINK", i, line);

      if (!helpfile && n->anchor)
      {
        nn = helpFindNode(hi, n->filename, NULL);

        snprintf(line, sizeof(line), "/help/%s?QUERY=%s", nn->filename,
	         query ? query : "");

        cgiSetArray("QPTEXT", i, nn->text);
	cgiSetArray("QPLINK", i, line);
      }
      else
      {
        cgiSetArray("QPTEXT", i, "");
	cgiSetArray("QPLINK", i, "");
      }

      fprintf(stderr, "DEBUG: [%d] = \"%s\" @ \"%s\"\n", i, n->text, line);
    }

    helpDeleteIndex(si);
  }

 /*
  * OK, now list the bookmarks within the index...
  */

  for (i = 0, section = NULL, n = (help_node_t *)cupsArrayFirst(hi->sorted);
       n;
       n = (help_node_t *)cupsArrayNext(hi->sorted))
  {
    if (n->anchor)
      continue;

   /*
    * Add a section link as needed...
    */

    if (n->section &&
        (!section || strcmp(n->section, section)))
    {
     /*
      * Add a link for this node...
      */

      snprintf(line, sizeof(line), "/help/?TOPIC=%s&QUERY=%s",
               cgiFormEncode(topic_data, n->section, sizeof(topic_data)),
	       query ? query : "");
      cgiSetArray("BMLINK", i, line);
      cgiSetArray("BMTEXT", i, n->section);
      cgiSetArray("BMINDENT", i, "0");

      i ++;
      section = n->section;
    }

    if (!topic || strcmp(n->section, topic))
      continue;

   /*
    * Add a link for this node...
    */

    snprintf(line, sizeof(line), "/help/%s?TOPIC=%s&QUERY=%s", n->filename,
             cgiFormEncode(topic_data, n->section, sizeof(topic_data)),
	     query ? query : "");
    cgiSetArray("BMLINK", i, line);
    cgiSetArray("BMTEXT", i, n->text);
    cgiSetArray("BMINDENT", i, "1");

    i ++;

    if (helpfile && !strcmp(helpfile, n->filename))
    {
      help_node_t	*nn;		/* Pointer to sub-node */


      cupsArraySave(hi->sorted);

      for (nn = (help_node_t *)cupsArrayFirst(hi->sorted);
           nn;
	   nn = (help_node_t *)cupsArrayNext(hi->sorted))
        if (nn->anchor && !strcmp(helpfile, nn->filename))
	{
	 /*
	  * Add a link for this node...
	  */

	  snprintf(line, sizeof(line), "#%s", nn->anchor);
	  cgiSetArray("BMLINK", i, line);
	  cgiSetArray("BMTEXT", i, nn->text);
	  cgiSetArray("BMINDENT", i, "2");

	  i ++;
	}

      cupsArrayRestore(hi->sorted);
    }
  }

 /*
  * Show the search and bookmark content...
  */

  if (!helpfile || !printable)
    cgiCopyTemplateLang("help-header.tmpl");
  else
    cgiCopyTemplateLang("help-printable.tmpl");

 /*
  * If we are viewing a file, copy it in now...
  */

  if (helpfile)
  {
    if ((fp = cupsFileOpen(filename, "r")) != NULL)
    {
      int	inbody;			/* Are we inside the body? */


      inbody = 0;

      while (cupsFileGets(fp, line, sizeof(line)))
      {
        if (inbody)
	{
	  if (!strncasecmp(line, "</BODY>", 7))
	    break;

	  printf("%s\n", line);
        }
	else if (!strncasecmp(line, "<BODY", 5))
	  inbody = 1;
      }

      cupsFileClose(fp);
    }
    else
    {
      perror(filename);
      cgiSetVariable("ERROR", "Unable to open help file.");
      cgiCopyTemplateLang("error.tmpl");
    }
  }

 /*
  * Send a standard trailer...
  */

  if (!printable)
    cgiEndHTML();
  else
    puts("</BODY>\n</HTML>");

 /*
  * Delete the index...
  */

  helpDeleteIndex(hi);

 /*
  * Return with no errors...
  */

  return (0);
}


/*
 * End of "$Id$".
 */
