/*
 * "$Id$"
 *
 *   On-line help index definitions for the Common UNIX Printing System (CUPS).
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
 */

#ifndef _CUPS_HELP_INDEX_H_
#  define _CUPS_HELP_INDEX_H_

/*
 * Include necessary headers...
 */

#  include <stdio.h>
#  include <stdlib.h>
#  include <sys/stat.h>
#  include <cups/cups.h>
#  include <cups/string.h>


/*
 * Data structures...
 */

typedef struct				/**** Help node structure... ****/
{
  char		*filename;		/* Filename, relative to help dir */
  char		*anchor;		/* Anchor name (NULL if none) */
  char		*text;			/* Text in anchor */
  time_t	mtime;			/* Last modification time */
  off_t		offset;			/* Offset in file */
  size_t	length;			/* Length in bytes */
  int		score;			/* Search score */
} help_node_t;

typedef struct				/**** Help index structure ****/
{
  int		search,			/* 1 = search index, 0 = normal */
		num_nodes,		/* Number of nodes */
		alloc_nodes;		/* Allocated nodes */
  help_node_t	**nodes;		/* Nodes sorted by filename */
  help_node_t	**sorted;		/* Nodes sorted by score + text */
} help_index_t;


/*
 * Functions...
 */

extern void		helpDeleteIndex(help_index_t *hi);
extern help_node_t	*helpFindNode(help_index_t *hi, const char *filename,
			              const char *anchor);
extern help_index_t	*helpLoadIndex(const char *hifile, const char *directory);
extern int		helpSaveIndex(help_index_t *hi, const char *hifile);
extern help_index_t	*helpSearchIndex(help_index_t *hi, const char *query);


#endif /* !_CUPS_HELP_INDEX_H_ */

/*
 * End of "$Id$".
 */
