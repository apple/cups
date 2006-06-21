/*
 * "$Id$"
 *
 *   MIME test program for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2006 by Easy Software Products, all rights reserved.
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
 *   main()        - Main entry for the test program.
 *   print_rules() - Print the rules for a file type...
 *   type_dir()    - Show the MIME types for a given directory.
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <cups/string.h>
#include "mime.h"
#include <cups/dir.h>


/*
 * Local functions...
 */

static void	print_rules(mime_magic_t *rules);
static void	type_dir(mime_t *mime, const char *dirname);


/*
 * 'main()' - Main entry for the test program.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  int		i;			/* Looping vars */
  const char	*filter_path;		/* Filter path */
  char		super[MIME_MAX_SUPER],	/* Super-type name */
		type[MIME_MAX_TYPE];	/* Type name */
  int		compression;		/* Compression of file */
  int		cost;			/* Cost of filters */
  mime_t	*mime;			/* MIME database */
  mime_type_t	*src,			/* Source type */
		*dst;			/* Destination type */
  cups_array_t	*filters;		/* Filters for the file */
  mime_filter_t	*filter;		/* Current filter */


  mime        = NULL;
  src         = NULL;
  dst         = NULL;
  filter_path = "../filter:../pdftops";

  for (i = 1; i < argc; i ++)
    if (!strcmp(argv[i], "-d"))
    {
      i ++;

      if (i < argc)
        mime = mimeLoad(argv[i], filter_path);
    }
    else if (!strcmp(argv[i], "-f"))
    {
      i ++;

      if (i < argc)
        filter_path = argv[i];
    }
    else if (!src)
    {
      if (!mime)
	mime = mimeLoad("../conf", filter_path);

      src = mimeFileType(mime, argv[i], NULL, &compression);

      if (src)
	printf("%s: %s/%s%s\n", argv[i], src->super, src->type,
	       compression ? " (gzipped)" : "");
      else if ((src = mimeType(mime, "application", "octet-stream")) != NULL)
	printf("%s: application/octet-stream\n", argv[i]);
      else
      {
	printf("%s: unknown\n", argv[i]);
	if (mime)
	  mimeDelete(mime);
	return (1);
      }
    }
    else
    {
      sscanf(argv[i], "%15[^/]/%31s", super, type);
      dst = mimeType(mime, super, type);

      filters = mimeFilter(mime, src, dst, &cost);

      if (!filters)
      {
	printf("No filters to convert from %s/%s to %s.\n", src->super,
	       src->type, argv[i]);
      }
      else
      {
        printf("Filter cost = %d\n", cost);

        filter = (mime_filter_t *)cupsArrayFirst(filters);
	fputs(filter->filter, stdout);

	for (filter = (mime_filter_t *)cupsArrayNext(filters);
	     filter;
	     filter = (mime_filter_t *)cupsArrayNext(filters))
	  printf(" | %s", filter->filter);

        putchar('\n');

        cupsArrayDelete(filters);
      }
    }

  if (!mime)
    mime = mimeLoad("../conf", filter_path);

  if (!src)
  {
    puts("MIME database types:");
    for (src = mimeFirstType(mime); src; src = mimeNextType(mime))
    {
      printf("\t%s/%s:\n", src->super, src->type);
      print_rules(src->rules);
      puts("");
    }

    puts("");

    puts("MIME database filters:");
    for (filter = mimeFirstFilter(mime); filter; filter = mimeNextFilter(mime))
      printf("\t%s/%s to %s/%s: %s (%d)\n",
             filter->src->super, filter->src->type,
	     filter->dst->super, filter->dst->type,
	     filter->filter, filter->cost);

    type_dir(mime, "../doc");
    type_dir(mime, "../man");
  }

  return (0);
}


/*
 * 'print_rules()' - Print the rules for a file type...
 */

static void
print_rules(mime_magic_t *rules)	/* I - Rules to print */
{
  int	i;				/* Looping var */
  static char	indent[255] = "\t";	/* Indentation for rules */


  if (rules == NULL)
    return;

  while (rules != NULL)
  {
    printf("%s[%p] ", indent, rules);

    if (rules->invert)
      printf("NOT ");

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
          printf("string(%d,", rules->offset);
	  for (i = 0; i < rules->length; i ++)
	    if (rules->value.stringv[i] < ' ' ||
	        rules->value.stringv[i] > 126)
	      printf("<%02X>", rules->value.stringv[i]);
	    else
	      putchar(rules->value.stringv[i]);
          putchar(')');
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
      case MIME_MAGIC_CONTAINS :
          printf("contains(%d,%d,", rules->offset, rules->region);
	  for (i = 0; i < rules->length; i ++)
	    if (rules->value.stringv[i] < ' ' ||
	        rules->value.stringv[i] > 126)
	      printf("<%02X>", rules->value.stringv[i]);
	    else
	      putchar(rules->value.stringv[i]);
          putchar(')');
	  break;
      default :
	  break;
    }

    if (rules->child != NULL)
    {
      if (rules->op == MIME_MAGIC_OR)
	puts("OR (");
      else
	puts("AND (");

      strcat(indent, "\t");
      print_rules(rules->child);
      indent[strlen(indent) - 1] = '\0';
      printf("%s)\n", indent);
    }
    else
      putchar('\n');

    rules = rules->next;
  }
}


/*
 * 'type_dir()' - Show the MIME types for a given directory.
 */

static void
type_dir(mime_t     *mime,		/* I - MIME database */
         const char *dirname)		/* I - Directory */
{
  cups_dir_t	*dir;			/* Directory */
  cups_dentry_t	*dent;			/* Directory entry */
  char		filename[1024];		/* File to type */
  mime_type_t	*filetype;		/* File type */
  int		compression;		/* Compressed file? */
  mime_type_t	*pstype;		/* application/vnd.cups-postscript */
  cups_array_t	*filters;		/* Filters to pstype */
  mime_filter_t	*filter;		/* Current filter */
  int		cost;			/* Filter cost */


  dir = cupsDirOpen(dirname);
  if (!dir)
    return;

  pstype = mimeType(mime, "application", "vnd.cups-postscript");

  while ((dent = cupsDirRead(dir)) != NULL)
  {
    if (dent->filename[0] == '.')
      continue;

    snprintf(filename, sizeof(filename), "%s/%s", dirname, dent->filename);

    if (S_ISDIR(dent->fileinfo.st_mode))
      type_dir(mime, filename);

    if (!S_ISREG(dent->fileinfo.st_mode))
      continue;

    filetype = mimeFileType(mime, filename, NULL, &compression);

    if (filetype)
    {
      printf("%s: %s/%s%s\n", filename, filetype->super, filetype->type,
             compression ? " (compressed)" : "");

      filters = mimeFilter(mime, filetype, pstype, &cost);

      if (!filters)
	puts("    No filters to convert application/vnd.cups-postscript.");
      else
      {
        printf("    Filter cost = %d\n", cost);

        filter = (mime_filter_t *)cupsArrayFirst(filters);
	printf("    %s", filter->filter);

	for (filter = (mime_filter_t *)cupsArrayNext(filters);
	     filter;
	     filter = (mime_filter_t *)cupsArrayNext(filters))
	  printf(" | %s", filter->filter);

        putchar('\n');

        cupsArrayDelete(filters);
      }
    }
    else
      printf("%s: unknown%s\n", filename, compression ? " (compressed)" : "");
  }

  cupsDirClose(dir);
}


/*
 * End of "$Id$".
 */
