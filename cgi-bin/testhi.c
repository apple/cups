/*
 * "$Id: testhi.c 6649 2007-07-11 21:46:42Z mike $"
 *
 *   Help index test program for CUPS.
 *
 *   Copyright 2007-2011 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
 *   main()       - Test the help index code.
 *   list_nodes() - List nodes in an array...
 */

/*
 * Include necessary headers...
 */

#include "cgi.h"


/*
 * Local functions...
 */

static void	list_nodes(const char *title, cups_array_t *nodes);


/*
 * 'main()' - Test the help index code.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  help_index_t	*hi,			/* Help index */
		*search;		/* Search index */


 /*
  * Load the help index...
  */

  hi = helpLoadIndex("testhi.index", ".");

  list_nodes("nodes", hi->nodes);
  list_nodes("sorted", hi->sorted);

 /*
  * Do any searches...
  */

  if (argc > 1)
  {
    search = helpSearchIndex(hi, argv[1], NULL, argv[2]);

    if (search)
    {
      list_nodes(argv[1], search->sorted);
      helpDeleteIndex(search);
    }
    else
      printf("%s (0 nodes)\n", argv[1]);
  }

  helpDeleteIndex(hi);

 /*
  * Return with no errors...
  */

  return (0);
}


/*
 * 'list_nodes()' - List nodes in an array...
 */

static void
list_nodes(const char   *title,		/* I - Title string */
	   cups_array_t *nodes)		/* I - Nodes */
{
  int		i;			/* Looping var */
  help_node_t	*node;			/* Current node */


  printf("%s (%d nodes):\n", title, cupsArrayCount(nodes));
  for (i = 1, node = (help_node_t *)cupsArrayFirst(nodes);
       node;
       i ++, node = (help_node_t *)cupsArrayNext(nodes))
  {
    if (node->anchor)
      printf("    %d: %s#%s \"%s\"", i, node->filename, node->anchor,
             node->text);
    else
      printf("    %d: %s \"%s\"", i, node->filename, node->text);

    printf(" (%d words)\n", cupsArrayCount(node->words));
  }
}


/*
 * End of "$Id: testhi.c 6649 2007-07-11 21:46:42Z mike $".
 */
