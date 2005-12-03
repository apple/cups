/*
 * "$Id$"
 *
 *   On-line help CGI for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2005 by Easy Software Products.
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
  help_node_t	**n;			/* Current help node */
  int		i, j;			/* Looping vars */
  const char	*query;			/* Search query */
  const char	*server_root;		/* CUPS_SERVERROOT environment variable */
  const char	*docroot;		/* CUPS_DOCROOT environment variable */
  const char	*helpfile;		/* Current help file */
  const char	*topic;			/* Current topic */
  const char	*section;		/* Current section */
  char		filename[1024],		/* Filename */
		directory[1024];	/* Directory */
  cups_file_t	*fp;			/* Help file */
  char		line[1024];		/* Line from file */


 /*
  * Get any form variables...
  */

  cgiInitialize();

 /*
  * Set the web interface section...
  */

  cgiSetVariable("SECTION", "help");

 /*
  * Load the help index...
  */

  if ((server_root = getenv("CUPS_SERVERROOT")) == NULL)
    server_root = CUPS_SERVERROOT;

  snprintf(filename, sizeof(filename), "%s/help.index", server_root);

  if ((docroot = getenv("CUPS_DOCROOT")) == NULL)
    docroot = CUPS_DOCROOT;

  snprintf(directory, sizeof(directory), "%s/help", docroot);

  fprintf(stderr, "DEBUG: helpLoadIndex(filename=\"%s\", directory=\"%s\")\n",
          filename, directory);

  hi = helpLoadIndex(filename, directory);
  if (!hi)
  {
    perror(filename);

    cgiStartHTML("Help");
    cgiSetVariable("ERROR", "Unable to load help index!");
    cgiCopyTemplateLang(stdout, cgiGetTemplateDir(), "error.tmpl", getenv("LANG"));
    cgiEndHTML();

    return (1);
  }

  fprintf(stderr, "hi->num_nodes=%d\n", hi->num_nodes);

 /*
  * See if we are viewing a file...
  */

  for (i = 0; i < argc; i ++)
    fprintf(stderr, "argv[%d]=\"%s\"\n", i, argv[i]);

  if ((helpfile = getenv("PATH_INFO")) != NULL)
    helpfile ++;
  else if (strstr(argv[0], "help.cgi"))
    helpfile = NULL;
  else
    helpfile = argv[0];

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

      cgiStartHTML("Help");
      cgiSetVariable("ERROR", "Unable to access help file!");
      cgiCopyTemplateLang(stdout, cgiGetTemplateDir(), "error.tmpl", getenv("LANG"));
      cgiEndHTML();

      return (1);
    }

    if ((n = helpFindNode(hi, helpfile, NULL)) == NULL)
    {
      cgiStartHTML("Help");
      cgiSetVariable("ERROR", "Help file not in index!");
      cgiCopyTemplateLang(stdout, cgiGetTemplateDir(), "error.tmpl", getenv("LANG"));
      cgiEndHTML();

      return (1);
    }

   /*
    * Set the page title and save the help file...
    */

    cgiSetVariable("HELPFILE", helpfile);
    cgiSetVariable("HELPTITLE", n[0]->text);

   /*
    * Send a standard page header...
    */

    cgiStartHTML(n[0]->text);
  }
  else
  {
   /*
    * Send a standard page header...
    */

    cgiStartHTML("Help");
  }

 /*
  * Do a search as needed...
  */

  query = cgiGetVariable("QUERY");
  topic = cgiGetVariable("TOPIC");
  si    = helpSearchIndex(hi, query, topic, helpfile);

  if (si)
  {
    help_node_t	**nn;			/* Parent node */


    fprintf(stderr, "si=%p, si->num_nodes=%d, si->sorted=%p\n", si,
            si->num_nodes, si->sorted);

    for (i = 0, n = si->sorted; i < si->num_nodes; i ++, n ++)
    {
      if (helpfile && n[0]->anchor)
        snprintf(line, sizeof(line), "#%s", n[0]->anchor);
      else if (n[0]->anchor)
        snprintf(line, sizeof(line), "/help/%s?QUERY=%s#%s", n[0]->filename,
	         query ? query : "", n[0]->anchor);
      else
        snprintf(line, sizeof(line), "/help/%s?QUERY=%s", n[0]->filename,
	         query ? query : "");

      cgiSetArray("QTEXT", i, n[0]->text);
      cgiSetArray("QLINK", i, line);

      if (!helpfile && n[0]->anchor)
      {
        nn = helpFindNode(hi, n[0]->filename, NULL);

        snprintf(line, sizeof(line), "/help/%s?QUERY=%s", nn[0]->filename,
	         query ? query : "");

        cgiSetArray("QPTEXT", i, nn[0]->text);
	cgiSetArray("QPLINK", i, line);
      }
      else
      {
        cgiSetArray("QPTEXT", i, "");
	cgiSetArray("QPLINK", i, "");
      }

      fprintf(stderr, "DEBUG: [%d] = \"%s\" @ \"%s\"\n", i, n[0]->text, line);
    }

    helpDeleteIndex(si);
  }

 /*
  * OK, now list the bookmarks within the index...
  */

  for (i = hi->num_nodes, j = 0, n = hi->sorted, section = NULL;
       i > 0;
       i --, n ++)
  {
    if (n[0]->anchor)
      continue;

   /*
    * Add a section link as needed...
    */

    if (n[0]->section &&
        (!section || strcmp(n[0]->section, section)))
    {
     /*
      * Add a link for this node...
      */

      snprintf(line, sizeof(line), "/help/?TOPIC=%s&QUERY=%s",
               n[0]->section, query ? query : "");
      cgiSetArray("BMLINK", j, line);
      cgiSetArray("BMTEXT", j, n[0]->section);
      cgiSetArray("BMINDENT", j, "0");

      j ++;
      section = n[0]->section;
    }

    if (topic && strcmp(n[0]->section, topic))
      continue;

   /*
    * Add a link for this node...
    */

    snprintf(line, sizeof(line), "/help/%s?TOPIC=%s&QUERY=%s", n[0]->filename,
             n[0]->section, query ? query : "");
    cgiSetArray("BMLINK", j, line);
    cgiSetArray("BMTEXT", j, n[0]->text);
    cgiSetArray("BMINDENT", j, "1");

    j ++;

    if (helpfile && !strcmp(helpfile, n[0]->filename))
    {
      int		ii;		/* Looping var */
      help_node_t	**nn;		/* Pointer to sub-node */


      for (ii = hi->num_nodes, nn = hi->sorted; ii > 0; ii --, nn ++)
        if (nn[0]->anchor && !strcmp(helpfile, nn[0]->filename))
	{
	 /*
	  * Add a link for this node...
	  */

	  snprintf(line, sizeof(line), "#%s", nn[0]->anchor);
	  cgiSetArray("BMLINK", j, line);
	  cgiSetArray("BMTEXT", j, nn[0]->text);
	  cgiSetArray("BMINDENT", j, "2");

	  j ++;
	}
    }
  }

 /*
  * Show the search and bookmark content...
  */

  cgiCopyTemplateLang(stdout, cgiGetTemplateDir(), "help-header.tmpl",
                      getenv("LANG"));

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
      cgiCopyTemplateLang(stdout, cgiGetTemplateDir(), "error.tmpl", getenv("LANG"));
    }
  }

 /*
  * Send a standard trailer...
  */

  cgiEndHTML();

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
