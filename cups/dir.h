/*
 * "$Id$"
 *
 *   Public directory definitions for the Common UNIX Printing System (CUPS).
 *
 *   This set of APIs abstracts enumeration of directory entries.
 *
 *   Copyright 1997-2005 by Easy Software Products, all rights reserved.
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

#ifndef _CUPS_DIR_H_
#  define _CUPS_DIR_H_


/*
 * Include necessary headers...
 */

#  include <sys/stat.h>


/*
 * C++ magic...
 */

#  ifdef _cplusplus
extern "C" {
#  endif /* _cplusplus */


/*
 * Data types...
 */

typedef struct cups_dir_s cups_dir_t;	/**** Directory type ****/

typedef struct cups_dentry_s		/**** Directory entry type ****/
{
  char		filename[260];		/* File name */
  struct stat	fileinfo;		/* File information */
} cups_dentry_t;


/*
 * Prototypes...
 */

extern void		cupsDirClose(cups_dir_t *dp);
extern cups_dir_t	*cupsDirOpen(const char *directory);
extern cups_dentry_t	*cupsDirRead(cups_dir_t *dp);
extern void		cupsDirRewind(cups_dir_t *dp);


#  ifdef _cplusplus
}
#  endif /* _cplusplus */
#endif /* !_CUPS_DIR_H_ */

/*
 * End of "$Id$".
 */
