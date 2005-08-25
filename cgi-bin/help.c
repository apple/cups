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
 */

/*
 * Include necessary headers...
 */

#include "ipp-var.h"


/*
 * Data structures...
 */

typedef struct				/**** Help node structure... ****/
{
  char		*filename;		/* Filename, relative to help dir */
  char		*anchor;		/* Anchor name (NULL if none) */
  char		*text;			/* Text in anchor */
  time_t	mtime;			/* Last modification time */
  long		offset,			/* Offset in file */
		length;			/* Length in bytes */
  int		score;			/* 
} help_node_t;

typedef struct				/**** Help index structure ****/
{
  int		num_nodes,		/* Number of nodes */
		alloc_nodes;		/* Allocated nodes */
  help_index_t	**nodes;		/* Nodes */
} help_index_t;


/*
 * Functions...
 */

help_node_t	*help_find_node(help_index_t *hi, const char *filename,
		                const char *anchor);
void		help_free_index(help_index_t *hi);
help_index_t	*help_load_index(void);
int		help_save_index(help_index_t *hi);
help_index_t	*help_search_index(help_index_t *hi, const char *query);
int		help_sort_nodes_by_name(const void *n1, const void *n2);
int		help_sort_nodes_by_text(const void *n1, const void *n2);
int		help_sort_nodes_by_score(const void *n1, const void *n2);



/*
 * Globals...
 */

int		NumHelpIndex = 0,	/* Number of help indices */
		AllocHelpIndex = 0;	/* Allocated help indices */
help_index_t	**HelpIndex = NULL;	/* Help indices */


/*
 * 'main()' - Main entry for CGI.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{


 /*
  * Get any form variables...
  */

  cgiInitialize();

 /*
  * Set the web interface section...
  */

  cgiSetVariable("SECTION", "help");

 /*
  * Send a standard header...
  */

  cgiStartHTML("Help");

 /*
  * Send a standard trailer...
  */

  cgiEndHTML();

 /*
  * Return with no errors...
  */

  return (0);
}


/*
 * End of "$Id$".
 */
