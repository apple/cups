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
 */

/*
 * Include necessary headers...
 */

#include "help-index.h"
#include <cups/debug.h>
#include <dirent.h>
#include <regex.h>


/*
 * Local functions...
 */

static regex_t		*help_compile_search(const char *query);
static void		help_create_sorted(help_index_t *hi);
static void		help_delete_node(help_node_t *n);
static help_node_t	**help_find_node(help_index_t *hi,
			                 const char *filename,
			                 const char *anchor);
static void		help_insert_node(help_index_t *hi, help_node_t *n);
static int		help_load_directory(help_index_t *hi,
			                    const char *directory,
					    const char *relative);
static int		help_load_file(help_index_t *hi,
			               const char *filename,
				       const char *relative,
				       time_t     mtime);
static help_node_t	*help_new_node(const char *filename, const char *anchor,
			               const char *text, time_t mtime,
				       off_t offset, size_t length);
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

help_node_t *				/* O - Node pointer or NULL */
helpFindNode(help_index_t *hi,		/* I - Index */
             const char   *filename,	/* I - Filename */
             const char   *anchor)	/* I - Anchor */
{
  help_node_t	**match;		/* Match */


  DEBUG_printf(("helpFindNode(hi=%p, filename=\"%s\", anchor=\"%s\")\n",
                hi, filename ? filename : "(nil)", anchor ? anchor : "(nil)"));

  if (!hi || !filename)
    return (NULL);

  if ((match = help_find_node(hi, filename, anchor)) != NULL)
    return (*match);

  return (NULL);
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
    cupsFileLock(fp, 1);

    while (cupsFileGets(fp, line, sizeof(line)))
    {
     /*
      * Each line looks like one of the following:
      *
      *     filename mtime offset length text
      *     filename#anchor offset length text
      */

      filename = line;

      if ((ptr = strchr(line, ' ')) == NULL)
        break;

      while (isspace(*ptr & 255))
        *ptr++ = '\0';

      if ((anchor = strrchr(filename, '#')) != NULL)
        *anchor++ = '\0';

      mtime  = strtol(ptr, &ptr, 10);
     /* TODO: Use strtoll for 64-bit file support */
      offset = strtol(ptr, &ptr, 10);
      length = strtol(ptr, &ptr, 10);

      while (isspace(*ptr & 255))
        ptr ++;

      text = ptr;

      if ((node = help_new_node(filename, anchor, text,
				mtime, offset, length)) == NULL)
        break;

      help_insert_node(hi, node);

      node->score = -1;
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

  for (i = 0; i < hi->num_nodes; i ++)
  {
   /*
    * Write the current node with/without the anchor...
    */

    node = hi->nodes[i];

   /* TODO: %lld for 64-bit file support */
    if (node->anchor)
    {
      if (cupsFilePrintf(fp, "%s#%s %ld %ld %s\n", node->filename, node->anchor,
                         node->offset, node->length, node->text) < 0)
        break;
    }
    else
    {
      if (cupsFilePrintf(fp, "%s %ld %ld %ld %s\n", node->filename, node->mtime,
                         node->offset, node->length, node->text) < 0)
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
		const char   *filename)	/* I - Limit search to this file */
{
  int		i, j;			/* Looping vars */
  help_index_t	*search;		/* Search index */
  help_node_t	**n;			/* Current node */
  regex_t	*re;			/* Regular expression */
  int		num_matches;		/* Number of matches */
  regmatch_t	matches[100];		/* Matches */


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
    n = help_find_node(hi, filename, NULL);
    if (!n)
      return (NULL);
  }
  else
    n = hi->nodes;

 /*
  * Convert the query into a regular expression...
  */

  re = help_compile_search(query);
  if (!re)
    return (NULL);

 /*
  * Allocate a search index...
  */

  search = calloc(1, sizeof(help_index_t));
  if (!search)
  {
    regfree(re);
    return (NULL);
  }

  search->search = 1;

 /*
  * Check each node in the index, adding matching nodes to the
  * search index...
  */

  for (i = n - hi->nodes; i < hi->num_nodes; i ++, n ++)
    if (!regexec(re, n[0]->text, sizeof(matches) / sizeof(matches[0]),
                 matches, 0))
    {
     /*
      * Found a match, add the node to the search index...
      */

      help_insert_node(search, *n);

     /*
      * Figure out the number of matches in the string...
      */

      for (j = 0; j < (int)(sizeof(matches) / sizeof(matches[0])); j ++)
        if (matches[j].rm_so < 0)
	  break;

      n[0]->score = j;
    }

 /*
  * Free the regular expression...
  */

  regfree(re);

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
 * 'help_compile_search()' - Convert a search string into a regular expression.
 */

static regex_t *			/* O - New regular expression */
help_compile_search(const char *query)	/* I - Query string */
{
  regex_t	*re;			/* Regular expression */
  char		*s,			/* Regular expression string */
		*sptr;			/* Pointer into RE string */
  int		slen;			/* Allocated size of RE string */
  const char	*qptr,			/* Pointer into query string */
		*qend;			/* End of current word */
  const char	*prefix;		/* Prefix to add to next word */
  int		quoted;			/* Word is quoted */
  int		wlen;			/* Word length */


  DEBUG_printf(("help_compile_search(query=\"%s\")\n", query ? query : "(nil)"));

 /*
  * Allocate a regular expression storage structure...
  */

  re = (regex_t *)calloc(1, sizeof(regex_t));

 /*
  * Allocate a buffer to hold the regular expression string, starting
  * at 1024 bytes or 3 times the length of the query string, whichever
  * is greater.  We'll expand the string as needed...
  */

  slen = strlen(query) * 3;
  if (slen < 1024)
    slen = 1024;

  s = (char *)malloc(slen);

 /*
  * Copy the query string to the regular expression, handling basic
  * AND and OR logic...
  */

  prefix = ".*";
  qptr   = query;
  sptr   = s;

  while (*qptr)
  {
   /*
    * Skip leading whitespace...
    */

    while (isspace(*qptr & 255))
      qptr ++;

    if (!*qptr)
      break;

   /*
    * Find the end of the current word...
    */

    if (*qptr == '\"' || *qptr == '\'')
    {
     /*
      * Scan quoted string...
      */

      quoted = *qptr ++;
      for (qend = qptr; *qend && *qend != quoted; qend ++);

      if (!*qend)
      {
       /*
        * No closing quote, error out!
	*/

	free(s);
	free(re);

	return (NULL);
      }
    }
    else
    {
     /*
      * Scan whitespace-delimited string...
      */

      quoted = 0;
      for (qend = qptr + 1; *qend && !isspace(*qend); qend ++);
    }

    wlen = qend - qptr;

   /*
    * Look for logic words: AND, OR
    */

    if (wlen == 3 && !strncasecmp(qptr, "AND", 3))
    {
     /*
      * Logical AND with the following text...
      */

      if (sptr > s)
        prefix = ".*";

      qptr = qend;
    }
    else if (wlen == 2 && !strncasecmp(qptr, "OR", 2))
    {
     /*
      * Logical OR with the following text...
      */

      if (sptr > s)
        prefix = ".*|.*";

      qptr = qend;
    }
    else
    {
     /*
      * Add a search word, making sure we have enough room for the
      * string + RE overhead...
      */

      wlen = (sptr - s) + 2 * wlen + strlen(prefix) + 4;

      if (wlen > slen)
      {
       /*
        * Expand the RE string buffer...
	*/

        char *temp;			/* Temporary string pointer */


	slen = wlen + 128;
        temp = (char *)realloc(s, slen);
	if (!temp)
	{
	  free(s);
	  free(re);

	  return (NULL);
	}

        sptr = temp + (sptr - s);
	s    = temp;
      }

     /*
      * Add the prefix string...
      */

      strcpy(sptr, prefix);
      sptr += strlen(sptr);

     /*
      * Then quote the remaining word characters as needed for the
      * RE...
      */

      while (qptr < qend)
      {
       /*
        * Quote: ^ . [ $ ( ) | * + ? { \
	*/

        if (strchr("^.[$()|*+?{\\", *qptr))
	  *sptr++ = '\\';

	*sptr++ = *qptr++;
      }

      prefix = ".*|.*";
    }

   /*
    * Advance to the next string...
    */

    if (quoted)
      qptr ++;
  }

  if (sptr > s)
    strcpy(sptr, ".*");
  else
  {
   /*
    * No query data, return NULL...
    */

    free(s);
    free(re);

    return (NULL);
  }

 /*
  * Compile the regular expression...
  */

  DEBUG_printf(("    s=\"%s\"\n", s));

  if (regcomp(re, s, REG_ICASE))
  {
    free(re);
    free(s);

    return (NULL);
  }

 /*
  * Free the RE string and return the new regular expression we compiled...
  */

  free(s);

  return (re);
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

  if (n->text)
    free(n->text);

  free(n);
}


/*
 * 'help_find_node()' - Find a node in an index.
 */

help_node_t **				/* O - Node array pointer or NULL */
help_find_node(help_index_t *hi,	/* I - Index */
               const char   *filename,	/* I - Filename */
               const char   *anchor)	/* I - Anchor */
{
  help_node_t	key,			/* Search key */
		*ptr;			/* Pointer to key */


  DEBUG_printf(("help_find_node(hi=%p, filename=\"%s\", anchor=\"%s\")\n",
                hi, filename ? filename : "(nil)", anchor ? anchor : "(nil)"));

  key.filename = (char *)filename;
  key.anchor   = (char *)anchor;
  ptr          = &key;

  return ((help_node_t **)bsearch(&ptr, hi->nodes, hi->num_nodes,
                                  sizeof(help_node_t *), help_sort_by_name));
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
  DIR		*dir;			/* Directory file */
  struct dirent	*dent;			/* Directory entry */
  char		*ext,			/* Pointer to extension */
		filename[1024],		/* Full filename */
		relname[1024];		/* Relative filename */
  struct stat	fileinfo;		/* File information */
  int		update;			/* Updated? */
  help_node_t	**node;			/* Current node */


  DEBUG_printf(("help_load_directory(hi=%p, directory=\"%s\", relative=\"%s\")\n",
                hi, directory ? directory : "(nil)", relative ? relative : "(nil)"));

 /*
  * Open the directory and scan it...
  */

  if ((dir = opendir(directory)) == NULL)
    return (0);

  while ((dent = readdir(dir)) != NULL)
  {
   /*
    * Skip dot files...
    */

    if (dent->d_name[0] == '.')
      continue;

   /*
    * Get file information...
    */

    snprintf(filename, sizeof(filename), "%s/%s", directory, dent->d_name);
    if (relative)
      snprintf(relname, sizeof(relname), "%s/%s", relative, dent->d_name);
    else
      strlcpy(relname, dent->d_name, sizeof(relname));

    if (stat(filename, &fileinfo))
      continue;

   /*
    * Check if we have a HTML file...
    */

    if ((ext = strstr(dent->d_name, ".html")) != NULL &&
        (!ext[5] || !strcmp(ext + 5, ".gz")))
    {
     /*
      * HTML file, see if we have already indexed the file...
      */

      if ((node = help_find_node(hi, relname, NULL)) != NULL)
      {
       /*
        * File already indexed - check dates to confirm that the
	* index is up-to-date...
	*/

        if (node[0]->mtime == fileinfo.st_mtime)
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

      help_load_file(hi, filename, relname, fileinfo.st_mtime);
    }
    else if (S_ISDIR(fileinfo.st_mode))
    {
     /*
      * Process sub-directory...
      */

      if (help_load_directory(hi, filename, relname) == 1)
        update = 1;
    }
  }

  closedir(dir);

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
  help_node_t	*node;			/* Current node */
  char		line[1024],		/* Line from file */
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

  while (cupsFileGets(fp, line, sizeof(line)))
  {
   /*
    * Look for a <TITLE> or <A NAME prefix...
    */

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

      if ((node = helpFindNode(hi, filename, anchor)) != NULL)
      {
       /*
	* Node already in the index, so replace the text and other
	* data...
	*/

	if (node->text)
	  free(node->text);

	node->text   = strdup(text);
	node->mtime  = mtime;
	node->offset = offset;
	node->score  = 0;
      }
      else
      {
       /*
	* New node...
	*/

        node = help_new_node(filename, anchor, text, mtime, offset, 0);
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
  n->text     = strdup(text);
  n->mtime    = mtime;
  n->offset   = offset;
  n->length   = length;

  return (n);
}


/*
 * 'help_sort_nodes_by_name()' - Sort nodes by filename and anchor.
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


  DEBUG_printf(("help_sort_by_score(p1=%p, p2=%p)\n", p1, p2));

  n1 = (help_node_t **)p1;
  n2 = (help_node_t **)p2;

  if (n1[0]->score != n2[0]->score)
    return (n1[0]->score - n2[0]->score);
  else
    return (strcasecmp(n1[0]->text, n2[0]->text));
}


/*
 * End of "$Id$".
 */
