/*
 * "$Id: mime.c 11558 2014-02-06 18:33:34Z msweet $"
 *
 * MIME database file routines for CUPS.
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
#include <cups/debug-private.h>
#include <cups/dir.h>
#include "mime-private.h"


/*
 * Local types...
 */

typedef struct _mime_fcache_s		/**** Filter cache structure ****/
{
  char	*name,				/* Filter name */
	*path;				/* Full path to filter if available */
} _mime_fcache_t;


/*
 * Local functions...
 */

static const char *mime_add_fcache(cups_array_t *filtercache, const char *name,
		                   const char *filterpath);
static int	mime_compare_fcache(_mime_fcache_t *a, _mime_fcache_t *b);
static void	mime_delete_fcache(cups_array_t *filtercache);
static void	mime_delete_rules(mime_magic_t *rules);
static void	mime_load_convs(mime_t *mime, const char *filename,
		                const char *filterpath,
			        cups_array_t *filtercache);
static void	mime_load_types(mime_t *mime, const char *filename);


/*
 * 'mimeDelete()' - Delete (free) a MIME database.
 */

void
mimeDelete(mime_t *mime)		/* I - MIME database */
{
  mime_type_t	*type;			/* Current type */
  mime_filter_t	*filter;		/* Current filter */


  DEBUG_printf(("mimeDelete(mime=%p)", mime));

  if (!mime)
    return;

 /*
  * Loop through filters and free them...
  */

  for (filter = (mime_filter_t *)cupsArrayFirst(mime->filters);
       filter;
       filter = (mime_filter_t *)cupsArrayNext(mime->filters))
    mimeDeleteFilter(mime, filter);

 /*
  * Loop through the file types and delete any rules...
  */

  for (type = (mime_type_t *)cupsArrayFirst(mime->types);
       type;
       type = (mime_type_t *)cupsArrayNext(mime->types))
    mimeDeleteType(mime, type);

 /*
  * Free the types and filters arrays, and then the MIME database structure.
  */

  cupsArrayDelete(mime->types);
  cupsArrayDelete(mime->filters);
  cupsArrayDelete(mime->srcs);
  free(mime);
}


/*
 * 'mimeDeleteFilter()' - Delete a filter from the MIME database.
 */

void
mimeDeleteFilter(mime_t        *mime,	/* I - MIME database */
		 mime_filter_t *filter)	/* I - Filter */
{
  DEBUG_printf(("mimeDeleteFilter(mime=%p, filter=%p(%s/%s->%s/%s, cost=%d, "
                "maxsize=" CUPS_LLFMT "))", mime, filter,
		filter ? filter->src->super : "???",
		filter ? filter->src->type : "???",
		filter ? filter->dst->super : "???",
		filter ? filter->dst->super : "???",
		filter ? filter->cost : -1,
		filter ? CUPS_LLCAST filter->maxsize : CUPS_LLCAST -1));

  if (!mime || !filter)
    return;

#ifdef DEBUG
  if (!cupsArrayFind(mime->filters, filter))
    DEBUG_puts("1mimeDeleteFilter: Filter not in MIME database.");
#endif /* DEBUG */

  cupsArrayRemove(mime->filters, filter);
  free(filter);

 /*
  * Deleting a filter invalidates the source lookup cache used by
  * mimeFilter()...
  */

  if (mime->srcs)
  {
    DEBUG_puts("1mimeDeleteFilter: Deleting source lookup cache.");
    cupsArrayDelete(mime->srcs);
    mime->srcs = NULL;
  }
}


/*
 * 'mimeDeleteType()' - Delete a type from the MIME database.
 */

void
mimeDeleteType(mime_t      *mime,	/* I - MIME database */
	       mime_type_t *mt)		/* I - Type */
{
  DEBUG_printf(("mimeDeleteType(mime=%p, mt=%p(%s/%s))", mime, mt,
                mt ? mt->super : "???", mt ? mt->type : "???"));

  if (!mime || !mt)
    return;

#ifdef DEBUG
  if (!cupsArrayFind(mime->types, mt))
    DEBUG_puts("1mimeDeleteFilter: Type not in MIME database.");
#endif /* DEBUG */

  cupsArrayRemove(mime->types, mt);

  mime_delete_rules(mt->rules);
  free(mt);
}


/*
 * '_mimeError()' - Show an error message.
 */

void
_mimeError(mime_t     *mime,		/* I - MIME database */
           const char *message,		/* I - Printf-style message string */
	   ...)				/* I - Additional arguments as needed */
{
  va_list	ap;			/* Argument pointer */
  char		buffer[8192];		/* Message buffer */


  if (mime->error_cb)
  {
    va_start(ap, message);
    vsnprintf(buffer, sizeof(buffer), message, ap);
    va_end(ap);

    (*mime->error_cb)(mime->error_ctx, buffer);
  }
}


/*
 * 'mimeFirstFilter()' - Get the first filter in the MIME database.
 */

mime_filter_t *				/* O - Filter or NULL */
mimeFirstFilter(mime_t *mime)		/* I - MIME database */
{
  DEBUG_printf(("6mimeFirstFilter(mime=%p)", mime));

  if (!mime)
  {
    DEBUG_puts("7mimeFirstFilter: Returning NULL.");
    return (NULL);
  }
  else
  {
    mime_filter_t *first = (mime_filter_t *)cupsArrayFirst(mime->filters);
					/* First filter */

    DEBUG_printf(("7mimeFirstFilter: Returning %p.", first));
    return (first);
  }
}


/*
 * 'mimeFirstType()' - Get the first type in the MIME database.
 */

mime_type_t *				/* O - Type or NULL */
mimeFirstType(mime_t *mime)		/* I - MIME database */
{
  DEBUG_printf(("6mimeFirstType(mime=%p)", mime));

  if (!mime)
  {
    DEBUG_puts("7mimeFirstType: Returning NULL.");
    return (NULL);
  }
  else
  {
    mime_type_t *first = (mime_type_t *)cupsArrayFirst(mime->types);
					/* First type */

    DEBUG_printf(("7mimeFirstType: Returning %p.", first));
    return (first);
  }
}


/*
 * 'mimeLoad()' - Create a new MIME database from disk.
 *
 * This function uses @link mimeLoadFilters@ and @link mimeLoadTypes@ to
 * create a MIME database from a single directory.
 */

mime_t *				/* O - New MIME database */
mimeLoad(const char *pathname,		/* I - Directory to load */
         const char *filterpath)	/* I - Directory to load */
{
  mime_t *mime;				/* New MIME database */

  DEBUG_printf(("mimeLoad(pathname=\"%s\", filterpath=\"%s\")", pathname,
                filterpath));

  mime = mimeLoadFilters(mimeLoadTypes(NULL, pathname), pathname, filterpath);
  DEBUG_printf(("1mimeLoad: Returning %p.", mime));

  return (mime);
}


/*
 * 'mimeLoadFilters()' - Load filter definitions from disk.
 *
 * This function loads all of the .convs files from the specified directory.
 * Use @link mimeLoadTypes@ to load all types before you load the filters.
 */

mime_t *				/* O - MIME database */
mimeLoadFilters(mime_t     *mime,	/* I - MIME database */
                const char *pathname,	/* I - Directory to load from */
                const char *filterpath)	/* I - Default filter program directory */
{
  cups_dir_t	*dir;			/* Directory */
  cups_dentry_t	*dent;			/* Directory entry */
  char		filename[1024];		/* Full filename of .convs file */
  cups_array_t	*filtercache;		/* Filter cache */


  DEBUG_printf(("mimeLoadFilters(mime=%p, pathname=\"%s\", filterpath=\"%s\")",
		mime, pathname, filterpath));

 /*
  * Range check input...
  */

  if (!mime || !pathname || !filterpath)
  {
    DEBUG_puts("1mimeLoadFilters: Bad arguments.");
    return (mime);
  }

 /*
  * Then open the directory specified by pathname...
  */

  if ((dir = cupsDirOpen(pathname)) == NULL)
  {
    DEBUG_printf(("1mimeLoadFilters: Unable to open \"%s\": %s", pathname,
                  strerror(errno)));
    _mimeError(mime, "Unable to open \"%s\": %s", pathname, strerror(errno));
    return (mime);
  }

 /*
  * Read all the .convs files...
  */

  filtercache = cupsArrayNew((cups_array_func_t)mime_compare_fcache, NULL);

  while ((dent = cupsDirRead(dir)) != NULL)
  {
    if (strlen(dent->filename) > 6 &&
        !strcmp(dent->filename + strlen(dent->filename) - 6, ".convs"))
    {
     /*
      * Load a mime.convs file...
      */

      snprintf(filename, sizeof(filename), "%s/%s", pathname, dent->filename);
      DEBUG_printf(("1mimeLoadFilters: Loading \"%s\".", filename));
      mime_load_convs(mime, filename, filterpath, filtercache);
    }
  }

  mime_delete_fcache(filtercache);

  cupsDirClose(dir);

  return (mime);
}


/*
 * 'mimeLoadTypes()' - Load type definitions from disk.
 *
 * This function loads all of the .types files from the specified directory.
 * Use @link mimeLoadFilters@ to load all filters after you load the types.
 */

mime_t *				/* O - MIME database */
mimeLoadTypes(mime_t     *mime,		/* I - MIME database or @code NULL@ to create a new one */
              const char *pathname)	/* I - Directory to load from */
{
  cups_dir_t	*dir;			/* Directory */
  cups_dentry_t	*dent;			/* Directory entry */
  char		filename[1024];		/* Full filename of .types file */


  DEBUG_printf(("mimeLoadTypes(mime=%p, pathname=\"%s\")", mime, pathname));

 /*
  * First open the directory specified by pathname...
  */

  if ((dir = cupsDirOpen(pathname)) == NULL)
  {
    DEBUG_printf(("1mimeLoadTypes: Unable to open \"%s\": %s", pathname,
                  strerror(errno)));
    DEBUG_printf(("1mimeLoadTypes: Returning %p.", mime));
    _mimeError(mime, "Unable to open \"%s\": %s", pathname, strerror(errno));
    return (mime);
  }

 /*
  * If "mime" is NULL, make a new, empty database...
  */

  if (!mime)
    mime = mimeNew();

  if (!mime)
  {
    cupsDirClose(dir);
    DEBUG_puts("1mimeLoadTypes: Returning NULL.");
    return (NULL);
  }

 /*
  * Read all the .types files...
  */

  while ((dent = cupsDirRead(dir)) != NULL)
  {
    if (strlen(dent->filename) > 6 &&
        !strcmp(dent->filename + strlen(dent->filename) - 6, ".types"))
    {
     /*
      * Load a mime.types file...
      */

      snprintf(filename, sizeof(filename), "%s/%s", pathname, dent->filename);
      DEBUG_printf(("1mimeLoadTypes: Loading \"%s\".", filename));
      mime_load_types(mime, filename);
    }
  }

  cupsDirClose(dir);

  DEBUG_printf(("1mimeLoadTypes: Returning %p.", mime));

  return (mime);
}


/*
 * 'mimeNew()' - Create a new, empty MIME database.
 */

mime_t *				/* O - MIME database */
mimeNew(void)
{
  return ((mime_t *)calloc(1, sizeof(mime_t)));
}


/*
 * 'mimeNextFilter()' - Get the next filter in the MIME database.
 */

mime_filter_t *				/* O - Filter or NULL */
mimeNextFilter(mime_t *mime)		/* I - MIME database */
{
  DEBUG_printf(("6mimeNextFilter(mime=%p)", mime));

  if (!mime)
  {
    DEBUG_puts("7mimeNextFilter: Returning NULL.");
    return (NULL);
  }
  else
  {
    mime_filter_t *next = (mime_filter_t *)cupsArrayNext(mime->filters);
					/* Next filter */

    DEBUG_printf(("7mimeNextFilter: Returning %p.", next));
    return (next);
  }
}


/*
 * 'mimeNextType()' - Get the next type in the MIME database.
 */

mime_type_t *				/* O - Type or NULL */
mimeNextType(mime_t *mime)		/* I - MIME database */
{
  DEBUG_printf(("6mimeNextType(mime=%p)", mime));

  if (!mime)
  {
    DEBUG_puts("7mimeNextType: Returning NULL.");
    return (NULL);
  }
  else
  {
    mime_type_t *next = (mime_type_t *)cupsArrayNext(mime->types);
					/* Next type */

    DEBUG_printf(("7mimeNextType: Returning %p.", next));
    return (next);
  }
}


/*
 * 'mimeNumFilters()' - Get the number of filters in a MIME database.
 */

int
mimeNumFilters(mime_t *mime)		/* I - MIME database */
{
  DEBUG_printf(("mimeNumFilters(mime=%p)", mime));

  if (!mime)
  {
    DEBUG_puts("1mimeNumFilters: Returning 0.");
    return (0);
  }
  else
  {
    DEBUG_printf(("1mimeNumFilters: Returning %d.",
                  cupsArrayCount(mime->filters)));
    return (cupsArrayCount(mime->filters));
  }
}


/*
 * 'mimeNumTypes()' - Get the number of types in a MIME database.
 */

int
mimeNumTypes(mime_t *mime)		/* I - MIME database */
{
  DEBUG_printf(("mimeNumTypes(mime=%p)", mime));

  if (!mime)
  {
    DEBUG_puts("1mimeNumTypes: Returning 0.");
    return (0);
  }
  else
  {
    DEBUG_printf(("1mimeNumTypes: Returning %d.",
                  cupsArrayCount(mime->types)));
    return (cupsArrayCount(mime->types));
  }
}


/*
 * 'mimeSetErrorCallback()' - Set the callback for error messages.
 */

void
mimeSetErrorCallback(
    mime_t          *mime,		/* I - MIME database */
    mime_error_cb_t cb,			/* I - Callback function */
    void            *ctx)		/* I - Context pointer for callback */
{
  if (mime)
  {
    mime->error_cb  = cb;
    mime->error_ctx = ctx;
  }
}


/*
 * 'mime_add_fcache()' - Add a filter to the filter cache.
 */

static const char *			/* O - Full path to filter or NULL */
mime_add_fcache(
    cups_array_t *filtercache,		/* I - Filter cache */
    const char   *name,			/* I - Filter name */
    const char   *filterpath)		/* I - Filter path */
{
  _mime_fcache_t	key,		/* Search key */
			*temp;		/* New filter cache */
  char			path[1024];	/* Full path to filter */


  DEBUG_printf(("2mime_add_fcache(filtercache=%p, name=\"%s\", "
                "filterpath=\"%s\")", filtercache, name, filterpath));

  key.name = (char *)name;
  if ((temp = (_mime_fcache_t *)cupsArrayFind(filtercache, &key)) != NULL)
  {
    DEBUG_printf(("3mime_add_fcache: Returning \"%s\".", temp->path));
    return (temp->path);
  }

  if ((temp = calloc(1, sizeof(_mime_fcache_t))) == NULL)
  {
    DEBUG_puts("3mime_add_fcache: Returning NULL.");
    return (NULL);
  }

  temp->name = strdup(name);

  if (cupsFileFind(name, filterpath, 1, path, sizeof(path)))
    temp->path = strdup(path);

  cupsArrayAdd(filtercache, temp);

  DEBUG_printf(("3mime_add_fcache: Returning \"%s\".", temp->path));
  return (temp->path);
}


/*
 * 'mime_compare_fcache()' - Compare two filter cache entries.
 */

static int				/* O - Result of comparison */
mime_compare_fcache(_mime_fcache_t *a,	/* I - First entry */
               _mime_fcache_t *b)	/* I - Second entry */
{
  return (strcmp(a->name, b->name));
}


/*
 * 'mime_delete_fcache()' - Free all memory used by the filter cache.
 */

static void
mime_delete_fcache(
    cups_array_t *filtercache)		/* I - Filter cache */
{
  _mime_fcache_t	*current;	/* Current cache entry */


  DEBUG_printf(("2mime_delete_fcache(filtercache=%p)", filtercache));

  for (current = (_mime_fcache_t *)cupsArrayFirst(filtercache);
       current;
       current = (_mime_fcache_t *)cupsArrayNext(filtercache))
  {
    free(current->name);

    if (current->path)
      free(current->path);

    free(current);
  }

  cupsArrayDelete(filtercache);
}


/*
 * 'mime_delete_rules()' - Free all memory for the given rule tree.
 */

static void
mime_delete_rules(mime_magic_t *rules)	/* I - Rules to free */
{
  mime_magic_t	*next;			/* Next rule to free */


  DEBUG_printf(("2mime_delete_rules(rules=%p)", rules));

 /*
  * Free the rules list, descending recursively to free any child rules.
  */

  while (rules != NULL)
  {
    next = rules->next;

    if (rules->child != NULL)
      mime_delete_rules(rules->child);

    if (rules->op == MIME_MAGIC_REGEX)
      regfree(&(rules->value.rev));

    free(rules);
    rules = next;
  }
}


/*
 * 'mime_load_convs()' - Load a xyz.convs file.
 */

static void
mime_load_convs(
    mime_t       *mime,			/* I - MIME database */
    const char   *filename,		/* I - Convs file to load */
    const char   *filterpath,		/* I - Path for filters */
    cups_array_t *filtercache)		/* I - Filter program cache */
{
  cups_file_t	*fp;			/* Convs file */
  char		line[1024],		/* Input line from file */
		*lineptr,		/* Current position in line */
		super[MIME_MAX_SUPER],	/* Super-type name */
		type[MIME_MAX_TYPE],	/* Type name */
		*temp,			/* Temporary pointer */
		*filter;		/* Filter program */
  mime_type_t	*temptype,		/* MIME type looping var */
		*dsttype;		/* Destination MIME type */
  int		cost;			/* Cost of filter */


  DEBUG_printf(("2mime_load_convs(mime=%p, filename=\"%s\", filterpath=\"%s\", "
                "filtercache=%p)", mime, filename, filterpath, filtercache));

 /*
  * First try to open the file...
  */

  if ((fp = cupsFileOpen(filename, "r")) == NULL)
  {
    DEBUG_printf(("3mime_load_convs: Unable to open \"%s\": %s", filename,
                  strerror(errno)));
    _mimeError(mime, "Unable to open \"%s\": %s", filename, strerror(errno));
    return;
  }

 /*
  * Then read each line from the file, skipping any comments in the file...
  */

  while (cupsFileGets(fp, line, sizeof(line)) != NULL)
  {
   /*
    * Skip blank lines and lines starting with a #...
    */

    if (!line[0] || line[0] == '#')
      continue;

   /*
    * Strip trailing whitespace...
    */

    for (lineptr = line + strlen(line) - 1;
         lineptr >= line && isspace(*lineptr & 255);
	 lineptr --)
      *lineptr = '\0';

   /*
    * Extract the destination super-type and type names from the middle of
    * the line.
    */

    lineptr = line;
    while (*lineptr != ' ' && *lineptr != '\t' && *lineptr != '\0')
      lineptr ++;

    while (*lineptr == ' ' || *lineptr == '\t')
      lineptr ++;

    temp = super;

    while (*lineptr != '/' && *lineptr != '\n' && *lineptr != '\0' &&
           (temp - super + 1) < MIME_MAX_SUPER)
      *temp++ = (char)tolower(*lineptr++ & 255);

    *temp = '\0';

    if (*lineptr != '/')
      continue;

    lineptr ++;
    temp = type;

    while (*lineptr != ' ' && *lineptr != '\t' && *lineptr != '\n' &&
           *lineptr != '\0' && (temp - type + 1) < MIME_MAX_TYPE)
      *temp++ = (char)tolower(*lineptr++ & 255);

    *temp = '\0';

    if (*lineptr == '\0' || *lineptr == '\n')
      continue;

    if ((dsttype = mimeType(mime, super, type)) == NULL)
    {
      DEBUG_printf(("3mime_load_convs: Destination type %s/%s not found.",
                    super, type));
      continue;
    }

   /*
    * Then get the cost and filter program...
    */

    while (*lineptr == ' ' || *lineptr == '\t')
      lineptr ++;

    if (*lineptr < '0' || *lineptr > '9')
      continue;

    cost = atoi(lineptr);

    while (*lineptr != ' ' && *lineptr != '\t' && *lineptr != '\0')
      lineptr ++;
    while (*lineptr == ' ' || *lineptr == '\t')
      lineptr ++;

    if (*lineptr == '\0' || *lineptr == '\n')
      continue;

    filter = lineptr;

    if (strcmp(filter, "-"))
    {
     /*
      * Verify that the filter exists and is executable...
      */

      if (!mime_add_fcache(filtercache, filter, filterpath))
      {
        DEBUG_printf(("mime_load_convs: Filter %s not found in %s.", filter,
	              filterpath));
        _mimeError(mime, "Filter \"%s\" not found.", filter);
        continue;
      }
    }

   /*
    * Finally, get the source super-type and type names from the beginning of
    * the line.  We do it here so we can support wildcards...
    */

    lineptr = line;
    temp    = super;

    while (*lineptr != '/' && *lineptr != '\n' && *lineptr != '\0' &&
           (temp - super + 1) < MIME_MAX_SUPER)
      *temp++ = (char)tolower(*lineptr++ & 255);

    *temp = '\0';

    if (*lineptr != '/')
      continue;

    lineptr ++;
    temp = type;

    while (*lineptr != ' ' && *lineptr != '\t' && *lineptr != '\n' &&
           *lineptr != '\0' && (temp - type + 1) < MIME_MAX_TYPE)
      *temp++ = (char)tolower(*lineptr++ & 255);

    *temp = '\0';

    if (!strcmp(super, "*") && !strcmp(type, "*"))
    {
     /*
      * Force * / * to be "application/octet-stream"...
      */

      strlcpy(super, "application", sizeof(super));
      strlcpy(type, "octet-stream", sizeof(type));
    }

   /*
    * Add the filter to the MIME database, supporting wildcards as needed...
    */

    for (temptype = (mime_type_t *)cupsArrayFirst(mime->types);
         temptype;
	 temptype = (mime_type_t *)cupsArrayNext(mime->types))
      if ((super[0] == '*' || !strcmp(temptype->super, super)) &&
          (type[0] == '*' || !strcmp(temptype->type, type)))
	mimeAddFilter(mime, temptype, dsttype, cost, filter);
  }

  cupsFileClose(fp);
}


/*
 * 'mime_load_types()' - Load a xyz.types file.
 */

static void
mime_load_types(mime_t     *mime,	/* I - MIME database */
                const char *filename)	/* I - Types file to load */
{
  cups_file_t	*fp;			/* Types file */
  size_t	linelen;		/* Length of line */
  char		line[32768],		/* Input line from file */
		*lineptr,		/* Current position in line */
		super[MIME_MAX_SUPER],	/* Super-type name */
		type[MIME_MAX_TYPE],	/* Type name */
		*temp;			/* Temporary pointer */
  mime_type_t	*typeptr;		/* New MIME type */


  DEBUG_printf(("2mime_load_types(mime=%p, filename=\"%s\")", mime, filename));

 /*
  * First try to open the file...
  */

  if ((fp = cupsFileOpen(filename, "r")) == NULL)
  {
    DEBUG_printf(("3mime_load_types: Unable to open \"%s\": %s", filename,
                  strerror(errno)));
    _mimeError(mime, "Unable to open \"%s\": %s", filename, strerror(errno));
    return;
  }

 /*
  * Then read each line from the file, skipping any comments in the file...
  */

  while (cupsFileGets(fp, line, sizeof(line)) != NULL)
  {
   /*
    * Skip blank lines and lines starting with a #...
    */

    if (!line[0] || line[0] == '#')
      continue;

   /*
    * While the last character in the line is a backslash, continue on to the
    * next line (and the next, etc.)
    */

    linelen = strlen(line);

    while (line[linelen - 1] == '\\')
    {
      linelen --;

      if (cupsFileGets(fp, line + linelen, sizeof(line) - linelen) == NULL)
        line[linelen] = '\0';
      else
        linelen += strlen(line + linelen);
    }

   /*
    * Extract the super-type and type names from the beginning of the line.
    */

    lineptr = line;
    temp    = super;

    while (*lineptr != '/' && *lineptr != '\n' && *lineptr != '\0' &&
           (temp - super + 1) < MIME_MAX_SUPER)
      *temp++ = (char)tolower(*lineptr++ & 255);

    *temp = '\0';

    if (*lineptr != '/')
      continue;

    lineptr ++;
    temp = type;

    while (*lineptr != ' ' && *lineptr != '\t' && *lineptr != '\n' &&
           *lineptr != '\0' && (temp - type + 1) < MIME_MAX_TYPE)
      *temp++ = (char)tolower(*lineptr++ & 255);

    *temp = '\0';

   /*
    * Add the type and rules to the MIME database...
    */

    typeptr = mimeAddType(mime, super, type);
    mimeAddTypeRule(typeptr, lineptr);
  }

  cupsFileClose(fp);
}


/*
 * End of "$Id: mime.c 11558 2014-02-06 18:33:34Z msweet $".
 */
