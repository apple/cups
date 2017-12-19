/*
 * Help index test program for CUPS.
 *
 * Copyright 2007-2017 by Apple Inc.
 * Copyright 1997-2007 by Easy Software Products.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more information.
 */

/*
 * Include necessary headers...
 */

#include "cgi.h"


/*
 * Local functions...
 */

static void	list_nodes(const char *title, cups_array_t *nodes);
static int	usage(void);


/*
 * 'main()' - Test the help index code.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  int		i;			/* Looping var */
  help_index_t	*hi,			/* Help index */
		*search;		/* Search index */
  const char	*opt,			/* Current option character */
		*dir = ".",		/* Directory to index */
		*q = NULL,		/* Query string */
		*section = NULL,	/* Section string */
		*filename = NULL;	/* Filename string */


 /*
  * Parse the command-line...
  */

  for (i = 1; i < argc; i ++)
  {
    if (argv[i][0] == '-')
    {
      if (!strcmp(argv[i], "--help"))
      {
        usage();
        return (0);
      }

      for (opt = argv[i] + 1; *opt; opt ++)
      {
        switch (*opt)
        {
          case 'd' : /* -d directory */
              i ++;
              if (i < argc)
              {
                dir = argv[i];
              }
              else
              {
                fputs("testhi: Missing directory for \"-d\" option.\n", stderr);
                return (usage());
              }
              break;

          case 's' : /* -s section */
              i ++;
              if (i < argc)
              {
                section = argv[i];
              }
              else
              {
                fputs("testhi: Missing section name for \"-s\" option.\n", stderr);
                return (usage());
              }
              break;

          default :
	      fprintf(stderr, "testhi: Unknown option \"-%c\".\n", *opt);
	      return (usage());
        }
      }
    }
    else if (!q)
      q = argv[i];
    else if (!filename)
      filename = argv[i];
    else
    {
      fprintf(stderr, "testhi: Unknown argument \"%s\".\n", argv[i]);
      return (usage());
    }
  }

 /*
  * Load the help index...
  */

  hi = helpLoadIndex("testhi.index", dir);

  list_nodes("nodes", hi->nodes);
  list_nodes("sorted", hi->sorted);

 /*
  * Do any searches...
  */

  if (q)
  {
    search = helpSearchIndex(hi, q, section, filename);

    if (search)
    {
      list_nodes(argv[1], search->sorted);
      helpDeleteIndex(search);
    }
    else
      printf("%s (0 nodes)\n", q);
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
 * 'usage()' - Show program usage.
 */

static int				/* O - Exit status */
usage(void)
{
  puts("Usage: ./testhi [options] [\"query\"] [filename]");
  puts("Options:");
  puts("-d directory      Specify index directory.");
  puts("-s section        Specify search section.");

  return (1);
}
