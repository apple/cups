/*
 * "$Id: statbuf.h,v 1.1.2.2 2004/08/23 18:01:56 mike Exp $"
 *
 *   Status buffer definitions for the Common UNIX Printing System (CUPS)
 *   scheduler.
 *
 *   Copyright 1997-2004 by Easy Software Products, all rights reserved.
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

extern cupsd_statbuf_t	*cupsdStatBufNew(int fd, const char *prefix, ...);
extern void		cupsdStatBufDelete(cupsd_statbuf_t *sb);
extern char		*cupsdStatBufUpdate(cupsd_statbuf_t *sb, int *loglevel,
			                    char *line, int linelen);


/*
 * End of "$Id: statbuf.h,v 1.1.2.2 2004/08/23 18:01:56 mike Exp $".
 */
