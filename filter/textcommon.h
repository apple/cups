/*
 * "$Id: textcommon.h,v 1.1 1999/03/23 20:10:20 mike Exp $"
 *
 *   Common text filter definitions for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-1999 by Easy Software Products.
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
 */

/*
 * Include necessary headers...
 */

#include "common.h"


/*
 * Constants...
 */

#define ATTR_BOLD	0x01
#define ATTR_UNDERLINE	0x02
#define ATTR_RAISED	0x04
#define ATTR_LOWERED	0x08
#define ATTR_ITALIC	0x10
#define ATTR_RED	0x20
#define ATTR_GREEN	0x40
#define ATTR_BLUE	0x80


/*
 * Structures...
 */

typedef struct			/**** Character/attribute structure... ****/
{
  unsigned short ch,		/* Character */
		attr;		/* Any attributes */
} lchar_t;


/*
 * Globals...
 */

extern int	WrapLines,	/* Wrap text in lines */
		SizeLines,	/* Number of lines on a page */
		SizeColumns,	/* Number of columns on a line */
		PageColumns,	/* Number of columns on a page */
		ColumnGutter,	/* Number of characters between text columns */
		ColumnWidth,	/* Width of each column */
		PrettyPrint;	/* Do pretty code formatting */
extern lchar_t	**Page;		/* Page characters */
extern int	NumPages;	/* Number of pages in document */
extern int	CharsPerInch,	/* Number of character columns per inch */
		LinesPerInch,	/* Number of lines per inch */
		UTF8;		/* Use UTF-8 encoding? */
extern char	*Keywords[];	/* List of known keywords... */


/*
 * Required functions...
 */

extern int	TextMain(char *name, int argc, char *argv[]);
extern void	WriteEpilogue(void);
extern void	WritePage(void);
extern void	WriteProlog(char *title, char *user);


/*
 * End of "$Id: textcommon.h,v 1.1 1999/03/23 20:10:20 mike Exp $".
 */
