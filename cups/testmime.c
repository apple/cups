/*
 * "$Id: testmime.c,v 1.2 1998/08/06 14:38:38 mike Exp $"
 *
 *   MIME library test program for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-1998 by Easy Software Products, all rights reserved.
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
 *       44145 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 *   main() - Main entry for the test program.
 *
 * Revision History:
 *
 *   $Log: testmime.c,v $
 *   Revision 1.2  1998/08/06 14:38:38  mike
 *   Finished coding and testing for CUPS 1.0.
 *
 *   Revision 1.1  1998/08/06 12:46:57  mike
 *   Initial revision
 */

/*
 * Include necessary headers...
 */

#include "mime.h"


/*
 * Local functions...
 */

static void print_rules(mime_magic_t *rules);


/*
 * 'main()' - Main entry for the test program.
 */

int					/* O - Exit code */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  int		i;			/* Looping var */
  char		super[MIME_MAX_SUPER],	/* Super-type name */
		type[MIME_MAX_TYPE];	/* Type name */
  mime_t	*mime;			/* MIME database */
  mime_type_t	*src,			/* Source type */
		*dst,			/* Destination type */
		**types;		/* File type array pointer */
  mime_filter_t	*filters;		/* Filters for the file */
  int		num_filters;		/* Number of filters for the file */


  mime = mimeLoad(".");

  puts("MIME database types:");
  for (i = 0, types = mime->types; i < mime->num_types; i ++, types ++)
  {
    printf("\t%s/%s: ", (*types)->super, (*types)->type);
    print_rules((*types)->rules);
    puts("");
  };

  puts("");

  puts("MIME database filters:");
  for (i = 0, filters = mime->filters; i < mime->num_filters; i ++, filters ++)
    printf("\t%s/%s to %s/%s: %s (%d)\n",
           filters->src->super, filters->src->type,
	   filters->dst->super, filters->dst->type,
	   filters->filter, filters->cost);

  puts("");

  switch (argc)
  {
    default :
        fputs("Usage: testmime source-file [destination-type]\n", stderr);
	return (1);

    case 2 :
        src = mimeFileType(mime, argv[1]);

	if (src != NULL)
	{
	  printf("%s: %s/%s\n", argv[1], src->super, src->type);
	  return (0);
	}
	else
	{
	  printf("%s: unknown\n", argv[1]);
	  return (1);
	};

    case 3 :
        src = mimeFileType(mime, argv[1]);

	sscanf(argv[2], "%[^/]/%s", super, type);
        dst = mimeType(mime, super, type);

        filters = mimeFilter(mime, src, dst, &num_filters);

        if (filters == NULL)
	{
	  printf("No filters to convert from %s to %s.\n", argv[1], argv[2]);
	  return (1);
	}
	else
	{
	  for (i = 0; i < num_filters; i ++)
	    if (i < (num_filters - 1))
	      printf("%s | ", filters[i].filter);
	    else
	      puts(filters[i].filter);

          return (0);
	};
  };
}


/*
 * 'print_rules()' - Print the rules for a file type...
 */

static void
print_rules(mime_magic_t *rules)	/* I - Rules to print */
{
  char	logic;				/* Logic separator */


  if (rules == NULL)
    return;

  if (rules->parent == NULL ||
      rules->parent->op == MIME_MAGIC_OR)
    logic = ',';
  else
    logic = '+';

  while (rules != NULL)
  {
    if (rules->prev != NULL)
      putchar(logic);

    switch (rules->op)
    {
      case MIME_MAGIC_MATCH :
          printf("match(%s)", rules->value.matchv);
	  break;
      case MIME_MAGIC_LOCALE :
          printf("locale(%s)", rules->value.localev);
	  break;
      case MIME_MAGIC_ASCII :
          printf("ascii(%d,%d)", rules->offset, rules->length);
	  break;
      case MIME_MAGIC_PRINTABLE :
          printf("printable(%d,%d)", rules->offset, rules->length);
	  break;
      case MIME_MAGIC_STRING :
          printf("string(%d,%s)", rules->offset, rules->value.stringv);
	  break;
      case MIME_MAGIC_CHAR :
          printf("char(%d,%d)", rules->offset, rules->value.charv);
	  break;
      case MIME_MAGIC_SHORT :
          printf("short(%d,%d)", rules->offset, rules->value.shortv);
	  break;
      case MIME_MAGIC_INT :
          printf("int(%d,%d)", rules->offset, rules->value.intv);
	  break;
      default :
          if (rules->child != NULL)
	  {
	    putchar('(');
	    print_rules(rules->child);
	    putchar(')');
	  };
	  break;
    };

    rules = rules->next;
  };
}



/*
 * End of "$Id: testmime.c,v 1.2 1998/08/06 14:38:38 mike Exp $".
 */
