/*
 * "$Id: websearch.c 1531 2009-05-22 21:50:50Z msweet $"
 *
 *   Web search program for www.cups.org.
 *
 *   Copyright 2007-2009 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Usage:
 *
 *   websearch directory "search string"
 *
 * Contents:
 *
 *   main()       - Search a directory of help files.
 *   list_nodes() - List matching nodes.
 */

/*
 * Include necessary headers...
 */

#include "cgi.h"


/*
 * Local functions...
 */

static void	list_nodes(help_index_t *hi, const char *title,
		           cups_array_t *nodes);


/*
 * 'main()' - Test the help index code.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  help_index_t	*hi,			/* Help index */
		*search;		/* Search index */
  char		indexname[1024];	/* Name of index file */


  if (argc != 3)
  {
    puts("Usage: websearch directory \"search terms\"");
    return (1);
  }

 /*
  * Load the help index...
  */

  snprintf(indexname, sizeof(indexname), "%s/.index", argv[1]);
  hi = helpLoadIndex(indexname, argv[1]);

 /*
  * Do any searches...
  */

  search = helpSearchIndex(hi, argv[2], NULL, NULL);

  if (search)
    list_nodes(hi, argv[1], search->sorted);

 /*
  * Return with no errors...
  */

  return (0);
}


/*
 * 'list_nodes()' - List nodes in an array...
 */

static void
list_nodes(help_index_t *hi,		/* I - Help index */
           const char   *title,		/* I - Title string */
	   cups_array_t *nodes)		/* I - Nodes */
{
  help_node_t	*node,			/* Current node */
		*file;			/* File node */


  printf("%d\n", cupsArrayCount(nodes));
  for (node = (help_node_t *)cupsArrayFirst(nodes);
       node;
       node = (help_node_t *)cupsArrayNext(nodes))
  {
    if (node->anchor)
    {
      file = helpFindNode(hi, node->filename, NULL);
      printf("%d|%s#%s|%s|%s\n", node->score, node->filename, node->anchor,
             node->text, file ? file->text : node->filename);
    }
    else
      printf("%d|%s|%s|%s\n", node->score, node->filename, node->text,
             node->text);
  }
}


/*
 * End of "$Id: websearch.c 1531 2009-05-22 21:50:50Z msweet $".
 */
