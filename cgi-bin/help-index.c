/*
 * "$Id: help-index.c 7717 2008-07-04 02:35:33Z mike $"
 *
 *   Online help index routines for CUPS.
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
 *   helpDeleteIndex()          - Delete an index, freeing all memory used.
 *   helpFindNode()             - Find a node in an index.
 *   helpLoadIndex()            - Load a help index from disk.
 *   helpSaveIndex()            - Save a help index to disk.
 *   helpSearchIndex()          - Search an index.
 *   help_add_word()            - Add a word to a node.
 *   help_compile_search()      - Convert a search string into a regular expression.
 *   help_delete_node()         - Free all memory used by a node.
 *   help_delete_word()         - Free all memory used by a word.
 *   help_load_directory()      - Load a directory of files into an index.
 *   help_load_file()           - Load a HTML files into an index.
 *   help_new_node()            - Create a new node and add it to an index.
 *   help_sort_nodes_by_name()  - Sort nodes by section, filename, and anchor.
 *   help_sort_nodes_by_score() - Sort nodes by score and text.
 *   help_sort_words()          - Sort words alphabetically.
 */

/*
 * Include necessary headers...
 */

#include "cgi-private.h"
#include <cups/dir.h>


/*
 * List of common English words that should not be indexed...
 */

static char		help_common_words[][6] =
			{
			  "about",
			  "all",
			  "an",
			  "and",
			  "are",
			  "as",
			  "at",
			  "be",
			  "been",
			  "but",
			  "by",
			  "call",
			  "can",
			  "come",
			  "could",
			  "day",
			  "did",
			  "do",
			  "down",
			  "each",
			  "find",
			  "first",
			  "for",
			  "from",
			  "go",
			  "had",
			  "has",
			  "have",
			  "he",
			  "her",
			  "him",
			  "his",
			  "hot",
			  "how",
			  "if",
			  "in",
			  "is",
			  "it",
			  "know",
			  "like",
			  "long",
			  "look",
			  "make",
			  "many",
			  "may",
			  "more",
			  "most",
			  "my",
			  "no",
			  "now",
			  "of",
			  "on",
			  "one",
			  "or",
			  "other",
			  "out",
			  "over",
			  "said",
			  "see",
			  "she",
			  "side",
			  "so",
			  "some",
			  "sound",
			  "than",
			  "that",
			  "the",
			  "their",
			  "them",
			  "then",
			  "there",
			  "these",
			  "they",
			  "thing",
			  "this",
			  "time",
			  "to",
			  "two",
			  "up",
			  "use",
			  "was",
			  "water",
			  "way",
			  "we",
			  "were",
			  "what",
			  "when",
			  "which",
			  "who",
			  "will",
			  "with",
			  "word",
			  "would",
			  "write",
			  "you",
			  "your"
			};


/*
 * Local functions...
 */

static help_word_t	*help_add_word(help_node_t *n, const char *text);
static void		help_delete_node(help_node_t *n);
static void		help_delete_word(help_word_t *w);
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
static int		help_sort_by_name(help_node_t *p1, help_node_t *p2);
static int		help_sort_by_score(help_node_t *p1, help_node_t *p2);
static int		help_sort_words(help_word_t *w1, help_word_t *w2);


/*
 * 'helpDeleteIndex()' - Delete an index, freeing all memory used.
 */

void
helpDeleteIndex(help_index_t *hi)	/* I - Help index */
{
  help_node_t	*node;			/* Current node */


  DEBUG_printf(("helpDeleteIndex(hi=%p)\n", hi));

  if (!hi)
    return;

  for (node = (help_node_t *)cupsArrayFirst(hi->nodes);
       node;
       node = (help_node_t *)cupsArrayNext(hi->nodes))
  {
    if (!hi->search)
      help_delete_node(node);
  }

  cupsArrayDelete(hi->nodes);
  cupsArrayDelete(hi->sorted);

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
  help_node_t	key;			/* Search key */


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

 /*
  * Return any match...
  */

  return ((help_node_t *)cupsArrayFind(hi->nodes, &key));
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
  help_node_t	*node;			/* Current node */
  help_word_t	*word;			/* Current word */


  DEBUG_printf(("helpLoadIndex(hifile=\"%s\", directory=\"%s\")\n",
                hifile, directory));

 /*
  * Create a new, empty index.
  */

  if ((hi = (help_index_t *)calloc(1, sizeof(help_index_t))) == NULL)
    return (NULL);

  hi->nodes  = cupsArrayNew((cups_array_func_t)help_sort_by_name, NULL);
  hi->sorted = cupsArrayNew((cups_array_func_t)help_sort_by_score, NULL);

  if (!hi->nodes || !hi->sorted)
  {
    cupsArrayDelete(hi->nodes);
    cupsArrayDelete(hi->sorted);
    free(hi);
    return (NULL);
  }

 /*
  * Try loading the existing index file...
  */

  if ((fp = cupsFileOpen(hifile, "r")) != NULL)
  {
   /*
    * Lock the file and then read the first line...
    */

    cupsFileLock(fp, 1);

    if (cupsFileGets(fp, line, sizeof(line)) && !strcmp(line, "HELPV2"))
    {
     /*
      * Got a valid header line, now read the data lines...
      */

      node = NULL;

      while (cupsFileGets(fp, line, sizeof(line)))
      {
       /*
	* Each line looks like one of the following:
	*
	*     filename mtime offset length "section" "text"
	*     filename#anchor offset length "text"
	*     SP count word
	*/

        if (line[0] == ' ')
	{
	 /*
	  * Read a word in the current node...
	  */

          if (!node || (ptr = strrchr(line, ' ')) == NULL)
	    continue;

          if ((word = help_add_word(node, ptr + 1)) != NULL)
	    word->count = atoi(line + 1);
        }
	else
	{
	 /*
	  * Add a node...
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

	  node->score = -1;

	  cupsArrayAdd(hi->nodes, node);
        }
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

  for (node = (help_node_t *)cupsArrayFirst(hi->nodes);
       node;
       node = (help_node_t *)cupsArrayNext(hi->nodes))
    if (node->score < 0)
    {
     /*
      * Delete this node...
      */

      cupsArrayRemove(hi->nodes, node);
      help_delete_node(node);
    }

 /*
  * Add nodes to the sorted array...
  */

  for (node = (help_node_t *)cupsArrayFirst(hi->nodes);
       node;
       node = (help_node_t *)cupsArrayNext(hi->nodes))
    cupsArrayAdd(hi->sorted, node);

 /*
  * Save the index if we updated it...
  */

  if (update)
    helpSaveIndex(hi, hifile);

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
  help_node_t	*node;			/* Current node */
  help_word_t	*word;			/* Current word */


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

  cupsFilePuts(fp, "HELPV2\n");

  for (node = (help_node_t *)cupsArrayFirst(hi->nodes);
       node;
       node = (help_node_t *)cupsArrayNext(hi->nodes))
  {
   /*
    * Write the current node with/without the anchor...
    */

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
                         node->filename, (int)node->mtime,
                         CUPS_LLCAST node->offset, CUPS_LLCAST node->length,
			 node->section ? node->section : "", node->text) < 0)
        break;
    }

   /*
    * Then write the words associated with the node...
    */

    for (word = (help_word_t *)cupsArrayFirst(node->words);
         word;
	 word = (help_word_t *)cupsArrayNext(node->words))
      if (cupsFilePrintf(fp, " %d %s\n", word->count, word->text) < 0)
        break;
  }

  cupsFileFlush(fp);

  if (cupsFileClose(fp) < 0)
    return (-1);
  else if (node)
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
  help_index_t	*search;		/* Search index */
  help_node_t	*node;			/* Current node */
  help_word_t	*word;			/* Current word */
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

 /*
  * Reset the scores of all nodes to 0...
  */

  for (node = (help_node_t *)cupsArrayFirst(hi->nodes);
       node;
       node = (help_node_t *)cupsArrayNext(hi->nodes))
    node->score = 0;

 /*
  * Find the first node to search in...
  */

  if (filename)
  {
    node = helpFindNode(hi, filename, NULL);
    if (!node)
      return (NULL);
  }
  else
    node = (help_node_t *)cupsArrayFirst(hi->nodes);

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

  search->nodes  = cupsArrayNew((cups_array_func_t)help_sort_by_name, NULL);
  search->sorted = cupsArrayNew((cups_array_func_t)help_sort_by_score, NULL);

  if (!search->nodes || !search->sorted)
  {
    cupsArrayDelete(search->nodes);
    cupsArrayDelete(search->sorted);
    free(search);
    cgiFreeSearch(sc);
    return (NULL);
  }

  search->search = 1;

 /*
  * Check each node in the index, adding matching nodes to the
  * search index...
  */

  for (; node; node = (help_node_t *)cupsArrayNext(hi->nodes))
    if (section && strcmp(node->section, section))
      continue;
    else if (filename && strcmp(node->filename, filename))
      continue;
    else
    {
      matches = cgiDoSearch(sc, node->text);

      for (word = (help_word_t *)cupsArrayFirst(node->words);
           word;
	   word = (help_word_t *)cupsArrayNext(node->words))
        if (cgiDoSearch(sc, word->text) > 0)
          matches += word->count;

      if (matches > 0)
      {
       /*
	* Found a match, add the node to the search index...
	*/

	node->score = matches;

	cupsArrayAdd(search->nodes, node);
	cupsArrayAdd(search->sorted, node);
      }
    }

 /*
  * Free the search context...
  */

  cgiFreeSearch(sc);

 /*
  * Return the results...
  */

  return (search);
}


/*
 * 'help_add_word()' - Add a word to a node.
 */

static help_word_t *			/* O - New word */
help_add_word(help_node_t *n,		/* I - Node */
              const char  *text)	/* I - Word text */
{
  help_word_t	*w,			/* New word */
		key;			/* Search key */


  DEBUG_printf(("help_add_word(n=%p, text=\"%s\")\n", n, text));

 /*
  * Create the words array as needed...
  */

  if (!n->words)
    n->words = cupsArrayNew((cups_array_func_t)help_sort_words, NULL);

 /*
  * See if the word is already added...
  */

  key.text = (char *)text;

  if ((w = (help_word_t *)cupsArrayFind(n->words, &key)) == NULL)
  {
   /*
    * Create a new word...
    */

    if ((w = calloc(1, sizeof(help_word_t))) == NULL)
      return (NULL);

    if ((w->text = strdup(text)) == NULL)
    {
      free(w);
      return (NULL);
    }

    cupsArrayAdd(n->words, w);
  }

 /*
  * Bump the counter for this word and return it...
  */

  w->count ++;

  return (w);
}


/*
 * 'help_delete_node()' - Free all memory used by a node.
 */

static void
help_delete_node(help_node_t *n)	/* I - Node */
{
  help_word_t	*w;			/* Current word */


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

  for (w = (help_word_t *)cupsArrayFirst(n->words);
       w;
       w = (help_word_t *)cupsArrayNext(n->words))
    help_delete_word(w);

  cupsArrayDelete(n->words);

  free(n);
}


/*
 * 'help_delete_word()' - Free all memory used by a word.
 */

static void
help_delete_word(help_word_t *w)	/* I - Word */
{
  DEBUG_printf(("help_delete_word(w=%p)\n", w));

  if (!w)
    return;

  if (w->text)
    free(w->text);

  free(w);
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
  cups_dir_t	*dir;			/* Directory file */
  cups_dentry_t	*dent;			/* Directory entry */
  char		*ext,			/* Pointer to extension */
		filename[1024],		/* Full filename */
		relname[1024];		/* Relative filename */
  int		update;			/* Updated? */
  help_node_t	*node;			/* Current node */


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
    * Skip "." files...
    */

    if (dent->filename[0] == '.')
      continue;

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

        if (node->mtime == dent->fileinfo.st_mtime)
	{
	 /*
	  * Same modification time, so mark all of the nodes
	  * for this file as up-to-date...
	  */

          for (; node; node = (help_node_t *)cupsArrayNext(hi->nodes))
	    if (!strcmp(node->filename, relname))
	      node->score = 0;
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
  help_node_t	*node;			/* Current node */
  char		line[1024],		/* Line from file */
		temp[1024],		/* Temporary word */
                section[1024],		/* Section */
		*ptr,			/* Pointer into line */
		*anchor,		/* Anchor name */
		*text;			/* Text for anchor */
  off_t		offset;			/* File offset */
  char		quote;			/* Quote character */
  help_word_t	*word;			/* Current word */
  int		wordlen;		/* Length of word */


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

    if (!_cups_strncasecmp(line, "<!-- SECTION:", 13))
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

      if (!_cups_strncasecmp(ptr, "TITLE>", 6))
      {
       /*
        * Found the title...
	*/

	anchor = NULL;
	ptr += 6;
      }
      else if (!_cups_strncasecmp(ptr, "A NAME=", 7))
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

      if ((node = helpFindNode(hi, relative, anchor)) != NULL)
      {
       /*
	* Node already in the index, so replace the text and other
	* data...
	*/

        cupsArrayRemove(hi->nodes, node);

        if (node->section)
	  free(node->section);

	if (node->text)
	  free(node->text);

        if (node->words)
	{
	  for (word = (help_word_t *)cupsArrayFirst(node->words);
	       word;
	       word = (help_word_t *)cupsArrayNext(node->words))
	    help_delete_word(word);

	  cupsArrayDelete(node->words);
	  node->words = NULL;
	}

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
      }

     /*
      * Go through the text value and replace tabs and newlines with
      * whitespace and eliminate extra whitespace...
      */

      for (ptr = node->text, text = node->text; *ptr;)
	if (isspace(*ptr & 255))
	{
	  while (isspace(*ptr & 255))
	    ptr ++;

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

     /*
      * (Re)add the node to the array...
      */

      cupsArrayAdd(hi->nodes, node);

      if (!anchor)
        node = NULL;
      break;
    }

    if (node)
    {
     /*
      * Scan this line for words...
      */

      for (ptr = line; *ptr; ptr ++)
      {
       /*
	* Skip HTML stuff...
	*/

	if (*ptr == '<')
	{
          if (!strncmp(ptr, "<!--", 4))
	  {
	   /*
	    * Skip HTML comment...
	    */

            if ((text = strstr(ptr + 4, "-->")) == NULL)
	      ptr += strlen(ptr) - 1;
	    else
	      ptr = text + 2;
	  }
	  else
	  {
	   /*
            * Skip HTML element...
	    */

            for (ptr ++; *ptr && *ptr != '>'; ptr ++)
	    {
	      if (*ptr == '\"' || *ptr == '\'')
	      {
		for (quote = *ptr++; *ptr && *ptr != quote; ptr ++);

		if (!*ptr)
		  ptr --;
	      }
	    }

	    if (!*ptr)
	      ptr --;
          }

          continue;
	}
	else if (*ptr == '&')
	{
	 /*
	  * Skip HTML entity...
	  */

	  for (ptr ++; *ptr && *ptr != ';'; ptr ++);

	  if (!*ptr)
	    ptr --;

	  continue;
	}
	else if (!isalnum(*ptr & 255))
          continue;

       /*
	* Found the start of a word, search until we find the end...
	*/

	for (text = ptr, ptr ++; *ptr && isalnum(*ptr & 255); ptr ++);

	wordlen = ptr - text;

        memcpy(temp, text, wordlen);
	temp[wordlen] = '\0';

        ptr --;

	if (wordlen > 1 && !bsearch(temp, help_common_words,
	                            (sizeof(help_common_words) /
				     sizeof(help_common_words[0])),
				    sizeof(help_common_words[0]),
				    (int (*)(const void *, const void *))
				        _cups_strcasecmp))
          help_add_word(node, temp);
      }
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


  DEBUG_printf(("help_new_node(filename=\"%s\", anchor=\"%s\", text=\"%s\", "
                "mtime=%ld, offset=%ld, length=%ld)\n",
                filename ? filename : "(nil)", anchor ? anchor : "(nil)",
		text ? text : "(nil)", (long)mtime, (long)offset,
		(long)length));

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
help_sort_by_name(help_node_t *n1,	/* I - First node */
                  help_node_t *n2)	/* I - Second node */
{
  int		diff;			/* Difference */


  DEBUG_printf(("help_sort_by_name(n1=%p(%s#%s), n2=%p(%s#%s)\n",
                n1, n1->filename, n1->anchor ? n1->anchor : "",
		n2, n2->filename, n2->anchor ? n2->anchor : ""));

  if ((diff = strcmp(n1->filename, n2->filename)) != 0)
    return (diff);

  if (!n1->anchor && !n2->anchor)
    return (0);
  else if (!n1->anchor)
    return (-1);
  else if (!n2->anchor)
    return (1);
  else
    return (strcmp(n1->anchor, n2->anchor));
}


/*
 * 'help_sort_nodes_by_score()' - Sort nodes by score and text.
 */

static int				/* O - Difference */
help_sort_by_score(help_node_t *n1,	/* I - First node */
                   help_node_t *n2)	/* I - Second node */
{
  int		diff;			/* Difference */


  DEBUG_printf(("help_sort_by_score(n1=%p(%d \"%s\" \"%s\"), "
                "n2=%p(%d \"%s\" \"%s\")\n",
                n1, n1->score, n1->section ? n1->section : "", n1->text,
                n2, n2->score, n2->section ? n2->section : "", n2->text));

  if (n1->score != n2->score)
    return (n2->score - n1->score);

  if (n1->section && !n2->section)
    return (1);
  else if (!n1->section && n2->section)
    return (-1);
  else if (n1->section && n2->section &&
           (diff = strcmp(n1->section, n2->section)) != 0)
    return (diff);

  return (_cups_strcasecmp(n1->text, n2->text));
}


/*
 * 'help_sort_words()' - Sort words alphabetically.
 */

static int				/* O - Difference */
help_sort_words(help_word_t *w1,	/* I - Second word */
                help_word_t *w2)	/* I - Second word */
{
  DEBUG_printf(("help_sort_words(w1=%p(\"%s\"), w2=%p(\"%s\"))\n",
                w1, w1->text, w2, w2->text));

  return (_cups_strcasecmp(w1->text, w2->text));
}


/*
 * End of "$Id: help-index.c 7717 2008-07-04 02:35:33Z mike $".
 */
