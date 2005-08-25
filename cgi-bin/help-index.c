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
#include <dirent.h>


/*
 * Local functions...
 */

static void		help_create_sorted(help_index_t *hi);
static void		help_delete_node(help_node_t *n);
static help_node_t	**help_find_node(help_index_t *hi,
			                 const char *filename,
			                 const char *anchor);
static int		help_load_directory(help_index_t *hi,
			                    const char *directory,
					    const char *relative);
static int		help_load_file(help_index_t *hi,
			               const char *filename,
				       const char *relative,
				       time_t     mtime);
static help_node_t	*help_new_node(help_index_t *hi, const char *filename,
			               const char *anchor, const char *text,
				       time_t mtime, off_t offset,
				       size_t length);
static int		help_sort_nodes_by_name(const void *n1, const void *n2);
static int		help_sort_nodes_by_score(const void *n1, const void *n2);


/*
 * 'helpDeleteIndex()' - Delete an index, freeing all memory used.
 */

void
helpDeleteIndex(help_index_t *hi)
{
  int	i;				/* Looping var */


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

      if ((node = help_new_node(hi,
                                strdup(filename),
				anchor ? strdup(anchor) : anchor,
				strdup(text),
				mtime, offset, length)) == NULL)
        break;

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

help_index_t *
helpSearchIndex(help_index_t *hi, const char *query)
{
}


/*
 * 'help_create_sorted()' - Create the sorted node array.
 */

static void
help_create_sorted(help_index_t *hi)	/* I - Index */
{
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

  qsort(hi->sorted, hi->num_nodes, sizeof(help_node_t *),
        help_sort_by_score);
}


/*
 * 'help_delete_node()' - Free all memory used by a node.
 */

static void
help_delete_node(help_node_t *n)	/* I - Node */
{
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


  key.filename = filename;
  key.anchor   = anchor;
  ptr          = &key;

  return ((help_node_t **)bsearch(&ptr, hi->nodes, hi->num_nodes,
                                  sizeof(help_node_t *), help_sort_by_name));
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

        update = 1;

	help_load_file(hi, filename, relname, fileinfo.st_mtime);
      }
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


  if ((fp = cupsFileOpen(filename, "r")) == NULL)
    return (-1);

  node   = NULL;
  offset = 0;

  while (cupsFileGets(fp, line, sizeof(line)))
  {
   /*
    * Look for a <TITLE> or <A NAME prefix...
    */

    for (ptr = line; (ptr = strchr(ptr, '<')) != NULL);)
    {
      ptr ++;

      if (!strncasecmp(ptr, "TITLE>", 6))
      {
       /*
        * Found the title...
	*/

	break;
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

        text = ptr + 1;
	break;
      }
    }

   /*
    * Get the offset of the next line...
    */

    offset = cupsFileTell(fp);
  }

  if (node)
    node->length = offset - node->offset;

  return (0);
}


/*
 * 'help_new_node()' - Create a new node and add it to an index.
 */

static help_node_t *			/* O - Node pointer or NULL on error */
help_new_node(help_index_t *hi,		/* I - Index */
              const char   *filename,	/* I - Filename */
              const char   *anchor,	/* I - Anchor */
	      const char   *text,	/* I - Text */
              off_t        offset,	/* I - Offset in file */
	      size_t       length)	/* I - Length in bytes */
{
}


/*
 * 'help_sort_nodes_by_name()' - Sort nodes by filename and anchor.
 */

static int
help_sort_nodes_by_name(const void *n1,
                        const void *n2)
{
}


/*
 * 'help_sort_nodes_by_score()' - Sort nodes by score and text.
 */

static int
help_sort_nodes_by_score(const void *n1,
                         const void *n2)
{
}


/*
 * End of "$Id$".
 */
