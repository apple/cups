/*
 * Status buffer definitions for the CUPS scheduler.
 *
 * Copyright 2007-2010 by Apple Inc.
 * Copyright 1997-2005 by Easy Software Products, all rights reserved.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more information.
 */


/*
 * Constants...
 */

#define CUPSD_SB_BUFFER_SIZE	2048	/* Bytes for job status buffer */


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
