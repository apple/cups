/*
 * "$Id: statbuf.h 7674 2008-06-18 23:18:32Z mike $"
 *
 *   Status buffer definitions for the Common UNIX Printing System (CUPS)
 *   scheduler.
 *
 *   Copyright 2007-2008 by Apple Inc.
 *   Copyright 1997-2005 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 */


/*
 * Constants...
 */

#define CUPSD_SB_BUFFER_SIZE	1024	/* Bytes or job status buffer */


/*
 * Types and structures...
 */

typedef struct				/**** Status buffer */
{
  int	fd;				/* File descriptor to read from */
  char	prefix[64];			/* Prefix for log messages */
  int	bufused;			/* How much is used in buffer */
  char	buffer[CUPSD_SB_BUFFER_SIZE];	/* Buffer */
} cupsd_statbuf_t;


/*
 * Prototypes...
 */

extern void		cupsdStatBufDelete(cupsd_statbuf_t *sb);
extern cupsd_statbuf_t	*cupsdStatBufNew(int fd, const char *prefix, ...);
extern char		*cupsdStatBufUpdate(cupsd_statbuf_t *sb, int *loglevel,
			                    char *line, int linelen);


/*
 * End of "$Id: statbuf.h 7674 2008-06-18 23:18:32Z mike $".
 */
