/*
 * "$Id: testmime.c 11558 2014-02-06 18:33:34Z msweet $"
 *
 * MIME test program for CUPS.
 *
 * Copyright 2007-2014 by Apple Inc.
 * Copyright 1997-2006 by Easy Software Products, all rights reserved.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * file is missing or damaged, see the license at "http://www.cups.org/".
 */

/*
 * Include necessary headers...
 */

#include <cups/string-private.h>
#include <cups/dir.h>
#include <cups/debug-private.h>
#include <cups/ppd-private.h>
#include "mime.h"


/*
 * Local functions...
 */

static void	add_ppd_filter(mime_t *mime, mime_type_t *filtertype,
		               const char *filter);
static void	add_ppd_filters(mime_t *mime, ppd_file_t *ppd);
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
  struct stat	srcinfo;		/* Source information */
  ppd_file_t	*ppd;			/* PPD file */
  cups_array_t	*filters;		/* Filters for the file */
  mime_filter_t	*filter;		/* Current filter */


  mime        = NULL;
  src         = NULL;
  dst         = NULL;
  ppd         = NULL;
  filter_path = "../filter:" CUPS_SERVERBIN "/filter";

  srcinfo.st_size = 0;

  for (i = 1; i < argc; i ++)
    if (!strcmp(argv[i], "-d"))
    {
      i ++;

      if (i < argc)
      {
        mime = mimeLoad(argv[i], filter_path);

	if (ppd)
	  add_ppd_filters(mime, ppd);
      }
    }
    else if (!strcmp(argv[i], "-f"))
    {
      i ++;

      if (i < argc)
        filter_path = argv[i];
    }
    else if (!strcmp(argv[i], "-p"))
    {
      i ++;

      if (i < argc)
      {
        ppd = ppdOpenFile(argv[i]);

	if (mime)
	  add_ppd_filters(mime, ppd);
      }
    }
    else if (!src)
    {
      if (!mime)
	mime = mimeLoad("../conf", filter_path);

      if (ppd)
        add_ppd_filters(mime, ppd);

      src = mimeFileType(mime, argv[i], NULL, &compression);
      stat(argv[i], &srcinfo);

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
      sscanf(argv[i], "%15[^/]/%255s", super, type);
      dst = mimeType(mime, super, type);

      filters = mimeFilter2(mime, src, (size_t)srcinfo.st_size, dst, &cost);

      if (!filters)
      {
	printf("No filters to convert from %s/%s to %s.\n", src->super,
	       src->type, argv[i]);
      }
      else
      {
        int first = 1;			/* First filter shown? */

        printf("Filter cost = %d\n", cost);

        for (filter = (mime_filter_t *)cupsArrayFirst(filters);
	     filter;
	     filter = (mime_filter_t *)cupsArrayNext(filters))
	{
	  if (!strcmp(filter->filter, "-"))
	    continue;

          if (first)
	  {
	    first = 0;
	    fputs(filter->filter, stdout);
	  }
	  else
	    printf(" | %s", filter->filter);
	}

        putchar('\n');

        cupsArrayDelete(filters);
      }
    }

  if (!mime)
  {
    mime = mimeLoad("../conf", filter_path);
    if (ppd)
      add_ppd_filters(mime, ppd);
  }

  if (!src)
  {
    puts("MIME database types:");
    for (src = mimeFirstType(mime); src; src = mimeNextType(mime))
    {
      printf("\t%s/%s (%d):\n", src->super, src->type, src->priority);
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
  }

  return (0);
}


/*
 * 'add_printer_filter()' - Add a printer filter from a PPD.
 */

static void
add_ppd_filter(mime_t      *mime,	/* I - MIME database */
               mime_type_t *filtertype,	/* I - Filter or prefilter MIME type */
	       const char  *filter)	/* I - Filter to add */
{
  char		super[MIME_MAX_SUPER],	/* Super-type for filter */
		type[MIME_MAX_TYPE],	/* Type for filter */
		dsuper[MIME_MAX_SUPER],	/* Destination super-type for filter */
		dtype[MIME_MAX_TYPE],	/* Destination type for filter */
		dest[MIME_MAX_SUPER + MIME_MAX_TYPE + 2],
					/* Destination super/type */
		program[1024];		/* Program/filter name */
  int		cost;			/* Cost of filter */
  size_t	maxsize = 0;		/* Maximum supported file size */
  mime_type_t	*temptype,		/* MIME type looping var */
		*desttype;		/* Destination MIME type */
  mime_filter_t	*filterptr;		/* MIME filter */


  DEBUG_printf(("add_ppd_filter(mime=%p, filtertype=%p(%s/%s), filter=\"%s\")",
                mime, filtertype, filtertype->super, filtertype->type, filter));

 /*
  * Parse the filter string; it should be in one of the following formats:
  *
  *     source/type cost program
  *     source/type cost maxsize(nnnn) program
  *     source/type dest/type cost program
  *     source/type dest/type cost maxsize(nnnn) program
  */

  if (sscanf(filter, "%15[^/]/%255s%*[ \t]%15[^/]/%255s%d%*[ \t]%1023[^\n]",
             super, type, dsuper, dtype, &cost, program) == 6)
  {
    snprintf(dest, sizeof(dest), "test/%s/%s", dsuper, dtype);

    if ((desttype = mimeType(mime, "printer", dest)) == NULL)
      desttype = mimeAddType(mime, "printer", dest);
  }
  else
  {
    if (sscanf(filter, "%15[^/]/%255s%d%*[ \t]%1023[^\n]", super, type, &cost,
               program) == 4)
    {
      desttype = filtertype;
    }
    else
    {
      printf("testmime: Invalid filter string \"%s\".\n", filter);
      return;
    }
  }

  if (!strncmp(program, "maxsize(", 8))
  {
    char	*ptr;			/* Pointer into maxsize(nnnn) program */

    maxsize = (size_t)strtoll(program + 8, &ptr, 10);

    if (*ptr != ')')
    {
      printf("testmime: Invalid filter string \"%s\".\n", filter);
      return;
    }

    ptr ++;
    while (_cups_isspace(*ptr))
      ptr ++;

    _cups_strcpy(program, ptr);
  }

 /*
  * Add the filter to the MIME database, supporting wildcards as needed...
  */

  for (temptype = mimeFirstType(mime);
       temptype;
       temptype = mimeNextType(mime))
    if (((super[0] == '*' && _cups_strcasecmp(temptype->super, "printer")) ||
         !_cups_strcasecmp(temptype->super, super)) &&
        (type[0] == '*' || !_cups_strcasecmp(temptype->type, type)))
    {
      if (desttype != filtertype)
      {
        DEBUG_printf(("add_ppd_filter: Adding filter %s/%s %s/%s %d %s",
		      temptype->super, temptype->type, desttype->super,
		      desttype->type, cost, program));
        filterptr = mimeAddFilter(mime, temptype, desttype, cost, program);

        if (!mimeFilterLookup(mime, desttype, filtertype))
        {
          DEBUG_printf(("add_printer_filter: Adding filter %s/%s %s/%s 0 -",
	                desttype->super, desttype->type, filtertype->super,
	                filtertype->type));
          mimeAddFilter(mime, desttype, filtertype, 0, "-");
        }
      }
      else
      {
        DEBUG_printf(("add_printer_filter: Adding filter %s/%s %s/%s %d %s",
		      temptype->super, temptype->type, filtertype->super,
		      filtertype->type, cost, program));
        filterptr = mimeAddFilter(mime, temptype, filtertype, cost, program);
      }

      if (filterptr)
	filterptr->maxsize = maxsize;
    }
}


/*
 * 'add_ppd_filters()' - Add all filters from a PPD.
 */

static void
add_ppd_filters(mime_t     *mime,	/* I - MIME database */
                ppd_file_t *ppd)	/* I - PPD file */
{
  _ppd_cache_t	*pc;			/* Cache data for PPD */
  const char	*value;			/* Filter definition value */
  mime_type_t	*filter,		/* Filter type */
		*prefilter;		/* Pre-filter type */


  pc = _ppdCacheCreateWithPPD(ppd);
  if (!pc)
    return;

  filter = mimeAddType(mime, "printer", "test");

  if (pc->filters)
  {
    for (value = (const char *)cupsArrayFirst(pc->filters);
         value;
         value = (const char *)cupsArrayNext(pc->filters))
      add_ppd_filter(mime, filter, value);
  }
  else
  {
    add_ppd_filter(mime, filter, "application/vnd.cups-raw 0 -");
    add_ppd_filter(mime, filter, "application/vnd.cups-postscript 0 -");
  }

  if (pc->prefilters)
  {
    prefilter = mimeAddType(mime, "prefilter", "test");

    for (value = (const char *)cupsArrayFirst(pc->prefilters);
         value;
         value = (const char *)cupsArrayNext(pc->prefilters))
      add_ppd_filter(mime, prefilter, value);
  }
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
 * End of "$Id: testmime.c 11558 2014-02-06 18:33:34Z msweet $".
 */
