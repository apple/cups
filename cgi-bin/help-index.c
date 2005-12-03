/*
 * "$Id$"
 *
 *   On-line help index routines for the Common UNIX Printing System (CUPS).
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
 *   helpDeleteIndex()          - Delete an index, freeing all memory used.
 *   helpFindNode()             - Find a node in an index.
 *   helpLoadIndex()            - Load a help index from disk.
 *   helpSaveIndex()            - Save a help index to disk.
 *   helpSearchIndex()          - Search an index.
 *   help_compile_search()      - Convert a search string into a regular expression.
 *   help_create_sorted()       - Create the sorted node array.
 *   help_delete_node()         - Free all memory used by a node.
 *   help_insert_node()         - Insert a node in an index.
 *   help_load_directory()      - Load a directory of files into an index.
 *   help_load_file()           - Load a HTML files into an index.
 *   help_new_node()            - Create a new node and add it to an index.
 *   help_sort_nodes_by_name()  - Sort nodes by section, filename, and anchor.
 *   help_sort_nodes_by_score() - Sort nodes by score and text.
 */

/*
 * Include necessary headers...
 */

#include "cgi-private.h"
#include <cups/dir.h>


/*
 * Local functions...
 */

static void		help_create_sorted(help_index_t *hi);
static void		help_delete_node(help_node_t *n);
static void		help_insert_node(help_index_t *hi, help_node_t *n);
static int		help_load_directory(help_index_t *hi,
			                    const char *directory,
					    const char *relative);
static int		help_load_file(help_index_t *hi,
			               const char *filename,
				       const char *relative,
				       time_t     mtime);
static help_node_t	*help_new_node(const char *filename, const char *anchor,
			               const char *section, const char *text,
				       time_t mtime, off_t offset,
				       size_t length);
static int		help_sort_by_name(const void *p1, const void *p2);
static int		help_sort_by_score(const void *p1, const void *p2);


/*
 * 'helpDeleteIndex()' - Delete an index, freeing all memory used.
 */

void
helpDeleteIndex(help_index_t *hi)
{
  int	i;				/* Looping var */


  DEBUG_printf(("helpDeleteIndex(hi=%p)\n", hi));

  if (!hi)
    return;

  if (!hi->search)
  {
    for (i = 0; i < hi->num_nodes; i ++)
      help_delete_node(hi->nodes[i]);
  }

  free(hi->nodes);

  if (hi->sorted)
    free(hi->sorted);

  free(hi);
}


/*
 * 'helpFindNode()' - Find a node in an index.
 */

help_node_t **				/* O - Node pointer or NULL */
helpFindNode(help_index_t *hi,		/* I - Index */
             const char   *filename,	/* I - Filename */
             const char   *anchor)	/* I - Anchor */
{
  help_node_t	key,			/* Search key */
		*ptr;			/* Pointer to key */


  DEBUG_printf(("helpFindNode(hi=%p, filename=\"%s\", anchor=\"%s\")\n",
                hi, filename ? filename : "(nil)", anchor ? anchor : "(nil)"));

 /*
  * Range check input...
  */

  if (!hi || !filename)
    return (NULL);

 /*
  * Initialize the search key...
  */

  key.filename = (char *)filename;
  key.anchor   = (char *)anchor;
  ptr          = &key;

 /*
  * Return any match...
  */

  return ((help_node_t **)bsearch(&ptr, hi->nodes, hi->num_nodes,
                                  sizeof(help_node_t *), help_sort_by_name));
}


/*
 * 'helpLoadIndex()' - Load a help index from disk.
 */

help_index_t *				/* O - Index pointer or NULL */
helpLoadIndex(const char *hifile,	/* I - Index filename */
              const char *directory)	/* I - Directory that is indexed */
{
  help_index_t	*hi;			/* Help index */
  cups_file_t	*fp;			/* Current file */
  char		line[2048],		/* Line from file */
		*ptr,			/* Pointer into line */
		*filename,		/* Filename in line */
		*anchor,		/* Anchor in line */
		*sectptr,		/* Section pointer in line */
		section[1024],		/* Section name */
		*text;			/* Text in line */
  time_t	mtime;			/* Modification time */
  off_t		offset;			/* Offset into file */
  size_t	length;			/* Length in bytes */
  int		update;			/* Update? */
  int		i;			/* Looping var */
  help_node_t	*node;			/* Current node */


  DEBUG_printf(("helpLoadIndex(hifile=\"%s\", directory=\"%s\")\n",
                hifile, directory));

 /*
  * Create a new, empty index.
  */

  hi = (help_index_t *)calloc(1, sizeof(help_index_t));

 /*
  * Try loading the existing index file...
  */

  if ((fp = cupsFileOpen(hifile, "r")) != NULL)
  {
   /*
    * Lock the file and then read the first line...
    */

    cupsFileLock(fp, 1);

    if (cupsFileGets(fp, line, sizeof(line)) && !strcmp(line, "HELPV1"))
    {
     /*
      * Got a valid header line, now read the data lines...
      */

      while (cupsFileGets(fp, line, sizeof(line)))
      {
       /*
	* Each line looks like one of the following:
	*
	*     filename mtime offset length "section" "text"
	*     filename#anchor offset length "text"
	*/

	filename = line;

	if ((ptr = strchr(line, ' ')) == NULL)
          break;

	while (isspace(*ptr & 255))
          *ptr++ = '\0';

	if ((anchor = strrchr(filename, '#')) != NULL)
	{
          *anchor++ = '\0';
	  mtime = 0;
	}
	else
	  mtime = strtol(ptr, &ptr, 10);

	offset = strtoll(ptr, &ptr, 10);
	length = strtoll(ptr, &ptr, 10);

	while (isspace(*ptr & 255))
          ptr ++;

        if (!anchor)
	{
	 /*
	  * Get section...
	  */

          if (*ptr != '\"')
	    break;

          ptr ++;
	  sectptr = ptr;

          while (*ptr && *ptr != '\"')
	    ptr ++;

          if (*ptr != '\"')
	    break;

          *ptr++ = '\0';

          strlcpy(section, sectptr, sizeof(section));

	  while (isspace(*ptr & 255))
            ptr ++;
        }

        if (*ptr != '\"')
	  break;

        ptr ++;
	text = ptr;

        while (*ptr && *ptr != '\"')
	  ptr ++;

        if (*ptr != '\"')
	  break;

        *ptr++ = '\0';

	if ((node = help_new_node(filename, anchor, section, text,
				  mtime, offset, length)) == NULL)
          break;

	help_insert_node(hi, node);

	node->score = -1;
      }
    }

    cupsFileClose(fp);
  }

 /*
  * Scan for new/updated files...
  */

  update = help_load_directory(hi, directory, NULL);

 /*
  * Remove any files that are no longer installed...
  */

  for (i = 0; i < hi->num_nodes;)
  {
    if (hi->nodes[i]->score < 0)
    {
     /*
      * Delete this node...
      */

      help_delete_node(hi->nodes[i]);

      hi->num_nodes --;
      if (i < hi->num_nodes)
        memmove(hi->nodes + i, hi->nodes + i + 1,
	        (hi->num_nodes - i) * sizeof(help_node_t *));

      update = 1;
    }
    else
    {
     /*
      * Keep this node...
      */

      i ++;
    }
  }

 /*
  * Save the index if we updated it...
  */

  if (update)
    helpSaveIndex(hi, hifile);

 /*
  * Create the sorted array...
  */

  help_create_sorted(hi);

 /*
  * Return the index...
  */

  return (hi);
}


/*
 * 'helpSaveIndex()' - Save a help index to disk.
 */

int					/* O - 0 on success, -1 on error */
helpSaveIndex(help_index_t *hi,		/* I - Index */
              const char   *hifile)	/* I - Index filename */
{
  cups_file_t	*fp;			/* Index file */
  int		i;			/* Looping var */
  help_node_t	*node;			/* Current node */


  DEBUG_printf(("helpSaveIndex(hi=%p, hifile=\"%s\")\n", hi, hifile));

 /*
  * Try creating a new index file...
  */

  if ((fp = cupsFileOpen(hifile, "w9")) == NULL)
    return (-1);

 /*
  * Lock the file while we write it...
  */

  cupsFileLock(fp, 1);

  cupsFilePuts(fp, "HELPV1\n");

  for (i = 0; i < hi->num_nodes; i ++)
  {
   /*
    * Write the current node with/without the anchor...
    */

    node = hi->nodes[i];

    if (node->anchor)
    {
      if (cupsFilePrintf(fp, "%s#%s " CUPS_LLFMT " " CUPS_LLFMT " \"%s\"\n",
                         node->filename, node->anchor,
                         CUPS_LLCAST node->offset, CUPS_LLCAST node->length,
			 node->text) < 0)
        break;
    }
    else
    {
      if (cupsFilePrintf(fp, "%s %d " CUPS_LLFMT " " CUPS_LLFMT " \"%s\" \"%s\"\n",
                         node->filename, node->mtime,
                         CUPS_LLCAST node->offset, CUPS_LLCAST node->length,
			 node->section ? node->section : "", node->text) < 0)
        break;
    }
  }

  if (cupsFileClose(fp) < 0)
    return (-1);
  else if (i < hi->num_nodes)
    return (-1);
  else
    return (0);
}


/*
 * 'helpSearchIndex()' - Search an index.
 */

help_index_t *				/* O - Search index */
helpSearchIndex(help_index_t *hi,	/* I - Index */
                const char   *query,	/* I - Query string */
		const char   *section,	/* I - Limit search to this section */
		const char   *filename)	/* I - Limit search to this file */
{
  int		i;			/* Looping var */
  help_index_t	*search;		/* Search index */
  help_node_t	**n;			/* Current node */
  void		*sc;			/* Search context */
  int		matches;		/* Number of matches */


  DEBUG_printf(("helpSearchIndex(hi=%p, query=\"%s\", filename=\"%s\")\n",
                hi, query ? query : "(nil)",
		filename ? filename : "(nil)"));

 /*
  * Range check...
  */

  if (!hi || !query)
    return (NULL);

  for (i = 0, n = hi->nodes; i < hi->num_nodes; i ++, n ++)
    n[0]->score = 0;

  if (filename)
  {
    n = helpFindNode(hi, filename, NULL);
    if (!n)
      return (NULL);
  }
  else
    n = hi->nodes;

 /*
  * Convert the query into a regular expression...
  */

  sc = cgiCompileSearch(query);
  if (!sc)
    return (NULL);

 /*
  * Allocate a search index...
  */

  search = calloc(1, sizeof(help_index_t));
  if (!search)
  {
    cgiFreeSearch(sc);
    return (NULL);
  }

  search->search = 1;

 /*
  * Check each node in the index, adding matching nodes to the
  * search index...
  */

  for (i = n - hi->nodes; i < hi->num_nodes; i ++, n ++)
    if (section && strcmp(n[0]->section, section))
      continue;
    else if (filename && strcmp(n[0]->filename, filename))
      continue;
    else if ((matches = cgiDoSearch(sc, n[0]->text)) > 0)
    {
     /*
      * Found a match, add the node to the search index...
      */

      help_insert_node(search, *n);

      n[0]->score = matches;
    }

 /*
  * Free the search context...
  */

  cgiFreeSearch(sc);

 /*
  * Sort the results...
  */

  help_create_sorted(search);

 /*
  * Return the results...
  */

  return (search);
}


/*
 * 'help_create_sorted()' - Create the sorted node array.
 */

static void
help_create_sorted(help_index_t *hi)	/* I - Index */
{
  DEBUG_printf(("help_create_sorted(hi=%p)\n", hi));

 /*
  * Free any existing sorted array...
  */

  if (hi->sorted)
    free(hi->sorted);

 /*
  * Create a new sorted array...
  */

  hi->sorted = calloc(hi->num_nodes, sizeof(help_node_t *));

  if (!hi->sorted)
    return;

 /*
  * Copy the nodes to the new array...
  */

  memcpy(hi->sorted, hi->nodes, hi->num_nodes * sizeof(help_node_t *));

 /*
  * Sort the new array by score and text.
  */

  if (hi->num_nodes > 1)
    qsort(hi->sorted, hi->num_nodes, sizeof(help_node_t *),
          help_sort_by_score);
}


/*
 * 'help_delete_node()' - Free all memory used by a node.
 */

static void
help_delete_node(help_node_t *n)	/* I - Node */
{
  DEBUG_printf(("help_delete_node(n=%p)\n", n));

  if (!n)
    return;

  if (n->filename)
    free(n->filename);

  if (n->anchor)
    free(n->anchor);

  if (n->section)
    free(n->section);

  if (n->text)
    free(n->text);

  free(n);
}


/*
 * 'help_insert_node()' - Insert a node in an index.
 */

static void
help_insert_node(help_index_t *hi,	/* I - Index */
                 help_node_t  *n)	/* I - Node */
{
  int		current,		/* Current node */
		left,			/* Left side */
		right,			/* Right side */
		diff;			/* Difference between nodes */
  help_node_t	**temp;			/* Temporary node pointer */


  DEBUG_printf(("help_insert_node(hi=%p, n=%p)\n", hi, n));

 /*
  * Allocate memory as needed...
  */

  if (hi->num_nodes >= hi->alloc_nodes)
  {
   /*
    * Expand the array in 128 node increments...
    */

    hi->alloc_nodes += 128;
    if (hi->alloc_nodes == 128)
      temp = (help_node_t **)malloc(hi->alloc_nodes * sizeof(help_node_t *));
    else
      temp = (help_node_t **)realloc(hi->nodes,
                                     hi->alloc_nodes * sizeof(help_node_t *));

    if (!temp)
      return;

    hi->nodes = temp;
  }

 /*
  * Find the insertion point...
  */

  if (hi->num_nodes == 0 ||
      help_sort_by_name(&n, hi->nodes + hi->num_nodes - 1) > 0)
  {
   /*
    * Last node...
    */

    hi->nodes[hi->num_nodes] = n;
    hi->num_nodes ++;
    return;
  }
  else if (help_sort_by_name(&n, hi->nodes) < 0)
  {
   /*
    * First node...
    */

    memmove(hi->nodes + 1, hi->nodes, hi->num_nodes * sizeof(help_node_t *));
    hi->nodes[0] = n;
    hi->num_nodes ++;
    return;
  }

 /*
  * Otherwise, do a binary insertion...
  */

  left    = 0;
  right   = hi->num_nodes - 1;

  do
  {
    current = (left + right) / 2;
    diff    = help_sort_by_name(&n, hi->nodes + current);

    if (!diff)
      break;
    else if (diff < 0)
      right = current;
    else
      left = current;
  }
  while ((right - left) > 1);

  if (diff > 0)
    current ++;

  memmove(hi->nodes + current + 1, hi->nodes + current,
          (hi->num_nodes - current) * sizeof(help_node_t *));
  hi->nodes[current] = n;
  hi->num_nodes ++;
}


/*
 * 'help_load_directory()' - Load a directory of files into an index.
 */

static int				/* O - 0 = success, -1 = error, 1 = updated */
help_load_directory(
    help_index_t *hi,			/* I - Index */
    const char   *directory,		/* I - Directory */
    const char   *relative)		/* I - Relative path */
{
  int		i;			/* Looping var */
  cups_dir_t	*dir;			/* Directory file */
  cups_dentry_t	*dent;			/* Directory entry */
  char		*ext,			/* Pointer to extension */
		filename[1024],		/* Full filename */
		relname[1024];		/* Relative filename */
  int		update;			/* Updated? */
  help_node_t	**node;			/* Current node */


  DEBUG_printf(("help_load_directory(hi=%p, directory=\"%s\", relative=\"%s\")\n",
                hi, directory ? directory : "(nil)", relative ? relative : "(nil)"));

 /*
  * Open the directory and scan it...
  */

  if ((dir = cupsDirOpen(directory)) == NULL)
    return (0);

  update = 0;

  while ((dent = cupsDirRead(dir)) != NULL)
  {
   /*
    * Get absolute and relative filenames...
    */

    snprintf(filename, sizeof(filename), "%s/%s", directory, dent->filename);
    if (relative)
      snprintf(relname, sizeof(relname), "%s/%s", relative, dent->filename);
    else
      strlcpy(relname, dent->filename, sizeof(relname));

   /*
    * Check if we have a HTML file...
    */

    if ((ext = strstr(dent->filename, ".html")) != NULL &&
        (!ext[5] || !strcmp(ext + 5, ".gz")))
    {
     /*
      * HTML file, see if we have already indexed the file...
      */

      if ((node = helpFindNode(hi, relname, NULL)) != NULL)
      {
       /*
        * File already indexed - check dates to confirm that the
	* index is up-to-date...
	*/

        if (node[0]->mtime == dent->fileinfo.st_mtime)
	{
	 /*
	  * Same modification time, so mark all of the nodes
	  * for this file as up-to-date...
	  */

          for (i = node - hi->nodes; i < hi->num_nodes; i ++, node ++)
	    if (!strcmp(node[0]->filename, relname))
	      node[0]->score = 0;
	    else
	      break;

          continue;
	}
      }

      update = 1;

      help_load_file(hi, filename, relname, dent->fileinfo.st_mtime);
    }
    else if (S_ISDIR(dent->fileinfo.st_mode))
    {
     /*
      * Process sub-directory...
      */

      if (help_load_directory(hi, filename, relname) == 1)
        update = 1;
    }
  }

  cupsDirClose(dir);

  return (update);
}


/*
 * 'help_load_file()' - Load a HTML files into an index.
 */

static int				/* O - 0 = success, -1 = error */
help_load_file(
    help_index_t *hi,			/* I - Index */
    const char   *filename,		/* I - Filename */
    const char   *relative,		/* I - Relative path */
    time_t       mtime)			/* I - Modification time */
{
  cups_file_t	*fp;			/* HTML file */
  help_node_t	*node,			/* Current node */
		**n;			/* Node pointer */
  char		line[1024],		/* Line from file */
                section[1024],		/* Section */
		*ptr,			/* Pointer into line */
		*anchor,		/* Anchor name */
		*text;			/* Text for anchor */
  off_t		offset;			/* File offset */
  char		quote;			/* Quote character */


  DEBUG_printf(("help_load_file(hi=%p, filename=\"%s\", relative=\"%s\", mtime=%ld)\n",
                hi, filename ? filename : "(nil)",
		relative ? relative : "(nil)", mtime));

  if ((fp = cupsFileOpen(filename, "r")) == NULL)
    return (-1);

  node   = NULL;
  offset = 0;

  strcpy(section, "Other");

  while (cupsFileGets(fp, line, sizeof(line)))
  {
   /*
    * Look for "<TITLE>", "<A NAME", or "<!-- SECTION:" prefix...
    */

    if (!strncasecmp(line, "<!-- SECTION:", 13))
    {
     /*
      * Got section line, copy it!
      */

      for (ptr = line + 13; isspace(*ptr & 255); ptr ++);

      strlcpy(section, ptr, sizeof(section));
      if ((ptr = strstr(section, "-->")) != NULL)
      {
       /*
        * Strip comment stuff from end of line...
	*/

        for (*ptr-- = '\0'; ptr > line && isspace(*ptr & 255); *ptr-- = '\0');

	if (isspace(*ptr & 255))
	  *ptr = '\0';
      }
      continue;
    }

    for (ptr = line; (ptr = strchr(ptr, '<')) != NULL;)
    {
      ptr ++;

      if (!strncasecmp(ptr, "TITLE>", 6))
      {
       /*
        * Found the title...
	*/

	anchor = NULL;
	ptr += 6;
      }
      else if (!strncasecmp(ptr, "A NAME=", 7))
      {
       /*
        * Found an anchor...
	*/

        ptr += 7;

	if (*ptr == '\"' || *ptr == '\'')
	{
	 /*
	  * Get quoted anchor...
	  */

	  quote  = *ptr;
          anchor = ptr + 1;
	  if ((ptr = strchr(anchor, quote)) != NULL)
	    *ptr++ = '\0';
	  else
	    break;
	}
	else
	{
	 /*
	  * Get unquoted anchor...
	  */

          anchor = ptr + 1;

	  for (ptr = anchor; *ptr && *ptr != '>' && !isspace(*ptr & 255); ptr ++);

	  if (*ptr)
	    *ptr++ = '\0';
	  else
	    break;
	}

       /*
        * Got the anchor, now lets find the end...
	*/

        while (*ptr && *ptr != '>')
	  ptr ++;

        if (*ptr != '>')
	  break;

        ptr ++;
      }
      else
        continue;

     /*
      * Now collect text for the link...
      */

      text = ptr;
      while ((ptr = strchr(text, '<')) == NULL)
      {
	ptr = text + strlen(text);
	if (ptr >= (line + sizeof(line) - 2))
	  break;

        *ptr++ = ' ';

        if (!cupsFileGets(fp, ptr, sizeof(line) - (ptr - line) - 1))
	  break;
      }

      *ptr = '\0';

      if (node)
	node->length = offset - node->offset;

      if (!*text)
      {
        node = NULL;
        break;
      }

      if ((n = helpFindNode(hi, relative, anchor)) != NULL)
      {
       /*
	* Node already in the index, so replace the text and other
	* data...
	*/

        node = n[0];

        if (node->section)
	  free(node->section);

	if (node->text)
	  free(node->text);

	node->section = section[0] ? strdup(section) : NULL;
	node->text    = strdup(text);
	node->mtime   = mtime;
	node->offset  = offset;
	node->score   = 0;
      }
      else
      {
       /*
	* New node...
	*/

        node = help_new_node(relative, anchor, section, text, mtime, offset, 0);
	help_insert_node(hi, node);
      }

     /*
      * Go through the text value and replace tabs and newlines with
      * whitespace and eliminate extra whitespace...
      */

      for (ptr = node->text, text = node->text; *ptr;)
	if (isspace(*ptr & 255))
	{
	  while (isspace(*ptr & 255))
	    *ptr ++;

	  *text++ = ' ';
        }
	else if (text != ptr)
	  *text++ = *ptr++;
	else
	{
	  text ++;
	  ptr ++;
	}

      *text = '\0';

      break;
    }

   /*
    * Get the offset of the next line...
    */

    offset = cupsFileTell(fp);
  }

  cupsFileClose(fp);

  if (node)
    node->length = offset - node->offset;

  return (0);
}


/*
 * 'help_new_node()' - Create a new node and add it to an index.
 */

static help_node_t *			/* O - Node pointer or NULL on error */
help_new_node(const char   *filename,	/* I - Filename */
              const char   *anchor,	/* I - Anchor */
	      const char   *section,	/* I - Section */
	      const char   *text,	/* I - Text */
	      time_t       mtime,	/* I - Modification time */
              off_t        offset,	/* I - Offset in file */
	      size_t       length)	/* I - Length in bytes */
{
  help_node_t	*n;			/* Node */


  DEBUG_printf(("help_new_node(filename=\"%s\", anchor=\"%s\", text=\"%s\", mtime=%ld, offset=%ld, length=%ld)\n",
                filename ? filename : "(nil)", anchor ? anchor : "(nil)",
		text ? text : "(nil)", mtime, offset, length));

  n = (help_node_t *)calloc(1, sizeof(help_node_t));
  if (!n)
    return (NULL);

  n->filename = strdup(filename);
  n->anchor   = anchor ? strdup(anchor) : NULL;
  n->section  = (section && *section) ? strdup(section) : NULL;
  n->text     = strdup(text);
  n->mtime    = mtime;
  n->offset   = offset;
  n->length   = length;

  return (n);
}


/*
 * 'help_sort_nodes_by_name()' - Sort nodes by section, filename, and anchor.
 */

static int				/* O - Difference */
help_sort_by_name(const void *p1,	/* I - First node */
                  const void *p2)	/* I - Second node */
{
  help_node_t	**n1,			/* First node */
		**n2;			/* Second node */
  int		diff;			/* Difference */


  DEBUG_printf(("help_sort_by_name(p1=%p, p2=%p)\n", p1, p2));

  n1 = (help_node_t **)p1;
  n2 = (help_node_t **)p2;

  if ((diff = strcmp(n1[0]->filename, n2[0]->filename)) != 0)
    return (diff);

  if (!n1[0]->anchor && !n2[0]->anchor)
    return (0);
  else if (!n1[0]->anchor)
    return (-1);
  else if (!n2[0]->anchor)
    return (1);
  else
    return (strcmp(n1[0]->anchor, n2[0]->anchor));
}


/*
 * 'help_sort_nodes_by_score()' - Sort nodes by score and text.
 */

static int				/* O - Difference */
help_sort_by_score(const void *p1,	/* I - First node */
                   const void *p2)	/* I - Second node */
{
  help_node_t	**n1,			/* First node */
		**n2;			/* Second node */
  int		diff;			/* Difference */


  DEBUG_printf(("help_sort_by_score(p1=%p, p2=%p)\n", p1, p2));

  n1 = (help_node_t **)p1;
  n2 = (help_node_t **)p2;

  if (n1[0]->score != n2[0]->score)
    return (n1[0]->score - n2[0]->score);

  if (n1[0]->section && !n2[0]->section)
    return (1);
  else if (!n1[0]->section && n2[0]->section)
    return (-1);
  else if (n1[0]->section && n2[0]->section &&
           (diff = strcmp(n1[0]->section, n2[0]->section)) != 0)
    return (diff);

  return (strcasecmp(n1[0]->text, n2[0]->text));
}


/*
 * End of "$Id$".
 */
