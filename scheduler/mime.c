/*
 * "$Id: mime.c,v 1.1 2000/01/25 03:50:48 mike Exp $"
 *
 *   MIME database file routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2000 by Easy Software Products, all rights reserved.
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
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 *   mimeDelete()   - Delete (free) a MIME database.
 *   mimeMerge()    - Merge a MIME database from disk with the current one.
 *   mimeNew()      - Create a new, empty MIME database.
 *   load_types()   - Load a xyz.types file...
 *   delete_rules() - Free all memory for the given rule tree.
 *   load_convs()   - Load a xyz.convs file...
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include <cups/string.h>
#include "mime.h"

#if defined(WIN32) || defined(__EMX__)
#  include <windows.h>
#elif HAVE_DIRENT_H
#  include <dirent.h>
typedef struct dirent DIRENT;
#  define NAMLEN(dirent) strlen((dirent)->d_name)
#else
#  if HAVE_SYS_NDIR_H
#    include <sys/ndir.h>
#  endif
#  if HAVE_SYS_DIR_H
#    include <sys/dir.h>
#  endif
#  if HAVE_NDIR_H
#    include <ndir.h>
#  endif
typedef struct direct DIRENT;
#  define NAMLEN(dirent) (dirent)->d_namlen
#endif


/*
 * Local functions...
 */

static void	load_types(mime_t *mime, char *filename);
static void	load_convs(mime_t *mime, char *filename);
static void	delete_rules(mime_magic_t *rules);


/*
 * 'mimeDelete()' - Delete (free) a MIME database.
 */

void
mimeDelete(mime_t *mime)	/* I - MIME database */
{
  int	i;			/* Looping var */


  if (mime == NULL)
    return;

 /*
  * Loop through the file types and delete any rules...
  */

  for (i = 0; i < mime->num_types; i ++)
  {
    delete_rules(mime->types[i]->rules);
    free(mime->types[i]);
  }

 /*
  * Free the types and filters arrays, and then the MIME database structure.
  */

  free(mime->types);
  free(mime->filters);
  free(mime);
}


/*
 * 'mimeMerge()' - Merge a MIME database from disk with the current one.
 */

mime_t *			/* O - Updated MIME database */
mimeMerge(mime_t     *mime,	/* I - MIME database to add to */
          const char *pathname)	/* I - Directory to load */
{
#if defined(WIN32) || defined(__EMX__)
  HANDLE	dir;		/* Directory handle */
  WIN32_FIND_DATA dent;		/* Directory entry */
  char		filename[1024],	/* Full filename of types/converts file */
		*pathsep;	/* Last character in path */


 /*
  * First open the directory specified by pathname...  Return NULL if nothing
  * was read or if the pathname is NULL...
  */

  if (pathname == NULL)
    return (NULL);

  strncpy(filename, pathname, sizeof(filename) - 1);
  filename[sizeof(filename) - 1] = '\0';

  pathsep = filename + strlen(filename);
  if (pathsep == filename ||
      (pathsep[-1] != '/' && pathsep[-1] != '\\'))
  {
    strcpy(pathsep, "/");
    pathsep ++;
  }

  strcpy(pathsep, "*.types");
  
  if ((dir = FindFirstFile(filename, &dent)) == 0)
    return (NULL);

 /*
  * If "mime" is NULL, make a new, blank database...
  */

  if (mime == NULL)
    if ((mime = mimeNew()) == NULL)
      return (NULL);

 /*
  * Read all the .types files...
  */

  do
  {
   /*
    * Load a mime.types file...
    */

    strcpy(pathsep, dent.cFileName);
    load_types(mime, filename);
  }
  while (FindNextFile(dir, &dent));

  FindClose(dir);

 /*
  * Read all the .convs files...
  */

  strcpy(pathsep, "*.convs");
  
  if ((dir = FindFirstFile(filename, &dent)) == 0)
    return (mime);

  do
  {
   /*
    * Load a mime.convs file...
    */

    strcpy(pathsep, dent.cFileName);
    load_convs(mime, filename);
  }
  while (FindNextFile(dir, &dent));

  FindClose(dir);

  return (mime);
#else
  DIR		*dir;		/* Directory */
  DIRENT	*dent;		/* Directory entry */
  char		filename[1024];	/* Full filename of types/converts file */


 /*
  * First open the directory specified by pathname...  Return NULL if nothing
  * was read or if the pathname is NULL...
  */

  if (pathname == NULL)
    return (NULL);

  if ((dir = opendir(pathname)) == NULL)
    return (NULL);

 /*
  * If "mime" is NULL, make a new, blank database...
  */

  if (mime == NULL)
    if ((mime = mimeNew()) == NULL)
      return (NULL);

 /*
  * Read all the .types files...
  */

  while ((dent = readdir(dir)) != NULL)
  {
    if (NAMLEN(dent) > 6 &&
        strcmp(dent->d_name + NAMLEN(dent) - 6, ".types") == 0)
    {
     /*
      * Load a mime.types file...
      */

      snprintf(filename, sizeof(filename), "%s/%s", pathname, dent->d_name);
      load_types(mime, filename);
    }
  }

  rewinddir(dir);

 /*
  * Read all the .convs files...
  */

  while ((dent = readdir(dir)) != NULL)
  {
    if (NAMLEN(dent) > 6 &&
        strcmp(dent->d_name + NAMLEN(dent) - 6, ".convs") == 0)
    {
     /*
      * Load a mime.convs file...
      */

      snprintf(filename, sizeof(filename), "%s/%s", pathname, dent->d_name);
      load_convs(mime, filename);
    }
  }

  closedir(dir);

  return (mime);
#endif /* WIN32 || __EMX__ */
}


/*
 * 'mimeNew()' - Create a new, empty MIME database.
 */

mime_t *			/* O - MIME database */
mimeNew(void)
{
  return ((mime_t *)calloc(1, sizeof(mime_t)));
}


/*
 * 'load_types()' - Load a xyz.types file...
 */

static void
load_types(mime_t *mime,		/* I - MIME database */
           char   *filename)		/* I - Types file to load */
{
  FILE		*fp;			/* Types file */
  int		linelen;		/* Length of line */
  char		line[65536],		/* Input line from file */
		*lineptr,		/* Current position in line */
		super[MIME_MAX_SUPER],	/* Super-type name */
		type[MIME_MAX_TYPE],	/* Type name */
		*temp;			/* Temporary pointer */
  mime_type_t	*typeptr;		/* New MIME type */


 /*
  * First try to open the file...
  */

  if ((fp = fopen(filename, "r")) == NULL)
    return;

 /*
  * Then read each line from the file, skipping any comments in the file...
  */

  while (fgets(line, sizeof(line), fp) != NULL)
  {
    linelen = strlen(line);

   /*
    * While the last character in the line is a backslash, continue on to the
    * next line (and the next, etc.)
    */

    if (line[linelen - 1] == '\n')
    {
      line[linelen - 1] = '\0';
      linelen --;
    }

    while (line[linelen - 1] == '\\')
    {
      linelen --;

      if (fgets(line + linelen, sizeof(line) - linelen, fp) == NULL)
        line[linelen] = '\0';
      else
      {
        linelen += strlen(line + linelen);
	if (line[linelen - 1] == '\n')
	{
	  line[linelen - 1] = '\0';
	  linelen --;
	}
      }
    }

   /*
    * Skip blank lines and lines starting with a #...
    */

    if (line[0] == '\n' || line[0] == '#')
      continue;

   /*
    * Extract the super-type and type names from the beginning of the line.
    */

    lineptr = line;
    temp    = super;

    while (*lineptr != '/' && *lineptr != '\n' && *lineptr != '\0' &&
           (temp - super + 1) < MIME_MAX_SUPER)
      *temp++ = tolower(*lineptr++);

    *temp = '\0';

    if (*lineptr != '/')
      continue;

    lineptr ++;
    temp = type;

    while (*lineptr != ' ' && *lineptr != '\t' && *lineptr != '\n' &&
           *lineptr != '\0' && (temp - type + 1) < MIME_MAX_TYPE)
      *temp++ = tolower(*lineptr++);

    *temp = '\0';

   /*
    * Add the type and rules to the MIME database...
    */

    typeptr = mimeAddType(mime, super, type);
    mimeAddTypeRule(typeptr, lineptr);
  }
}


/*
 * 'load_convs()' - Load a xyz.convs file...
 */

static void
load_convs(mime_t *mime,		/* I - MIME database */
           char   *filename)		/* I - Convs file to load */
{
  int		i;			/* Looping var */
  FILE		*fp;			/* Convs file */
  char		line[1024],		/* Input line from file */
		*lineptr,		/* Current position in line */
		super[MIME_MAX_SUPER],	/* Super-type name */
		type[MIME_MAX_TYPE],	/* Type name */
		*temp,			/* Temporary pointer */
		*filter;		/* Filter program */
  mime_type_t	**temptype,		/* MIME type looping var */
		*dsttype;		/* Destination MIME type */
  int		cost;			/* Cost of filter */


 /*
  * First try to open the file...
  */

  if ((fp = fopen(filename, "r")) == NULL)
    return;

 /*
  * Then read each line from the file, skipping any comments in the file...
  */

  while (fgets(line, sizeof(line), fp) != NULL)
  {
   /*
    * Skip blank lines and lines starting with a #...
    */

    if (line[0] == '\n' || line[0] == '#')
      continue;

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
      *temp++ = tolower(*lineptr++);

    *temp = '\0';

    if (*lineptr != '/')
      continue;

    lineptr ++;
    temp = type;

    while (*lineptr != ' ' && *lineptr != '\t' && *lineptr != '\n' &&
           *lineptr != '\0' && (temp - type + 1) < MIME_MAX_TYPE)
      *temp++ = tolower(*lineptr++);

    *temp = '\0';

    if (*lineptr == '\0' || *lineptr == '\n')
      continue;

    if ((dsttype = mimeType(mime, super, type)) == NULL)
      continue;

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
    if (filter[strlen(filter) - 1] == '\n')
      filter[strlen(filter) - 1] = '\0';

   /*
    * Finally, get the source super-type and type names from the beginning of
    * the line.  We do it here so we can support wildcards...
    */

    lineptr = line;
    temp    = super;

    while (*lineptr != '/' && *lineptr != '\n' && *lineptr != '\0' &&
           (temp - super + 1) < MIME_MAX_SUPER)
      *temp++ = tolower(*lineptr++);

    *temp = '\0';

    if (*lineptr != '/')
      continue;

    lineptr ++;
    temp = type;

    while (*lineptr != ' ' && *lineptr != '\t' && *lineptr != '\n' &&
           *lineptr != '\0' && (temp - type + 1) < MIME_MAX_TYPE)
      *temp++ = tolower(*lineptr++);

    *temp = '\0';

   /*
    * Add the filter to the MIME database, supporting wildcards as needed...
    */

    for (temptype = mime->types, i = 0; i < mime->num_types; i ++, temptype ++)
      if ((super[0] == '*' || strcmp((*temptype)->super, super) == 0) &&
          (type[0] == '*' || strcmp((*temptype)->type, type) == 0))
	mimeAddFilter(mime, *temptype, dsttype, cost, filter);
  }
}


/*
 * 'delete_rules()' - Free all memory for the given rule tree.
 */

static void
delete_rules(mime_magic_t *rules)	/* I - Rules to free */
{
  mime_magic_t	*next;			/* Next rule to free */


 /*
  * Free the rules list, descending recursively to free any child rules.
  */

  while (rules != NULL)
  {
    next = rules->next;

    if (rules->child != NULL)
      delete_rules(rules->child);

    free(rules);
    rules = next;
  }
}


/*
 * End of "$Id: mime.c,v 1.1 2000/01/25 03:50:48 mike Exp $".
 */
