/*
 * "$Id: textcommon.h 6649 2007-07-11 21:46:42Z mike $"
 *
 *   Common text filter definitions for CUPS.
 *
 *   Copyright 2007-2010 by Apple Inc.
 *   Copyright 1997-2005 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 */

/*
 * Include necessary headers...
 */

#include "common.h"


/*
 * C++ magic...
 */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/*
 * Constants...
 */

#define ATTR_NORMAL	0x00
#define ATTR_BOLD	0x01
#define ATTR_ITALIC	0x02
#define ATTR_BOLDITALIC	0x03
#define ATTR_FONT	0x03

#define ATTR_UNDERLINE	0x04
#define ATTR_RAISED	0x08
#define ATTR_LOWERED	0x10
#define ATTR_RED	0x20
#define ATTR_GREEN	0x40
#define ATTR_BLUE	0x80

#define PRETTY_OFF	0
#define PRETTY_PLAIN	1
#define PRETTY_CODE	2
#define PRETTY_SHELL	3
#define PRETTY_PERL	4
#define PRETTY_HTML	5


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
		PrettyPrint,	/* Do pretty code formatting? */
		Copies;		/* Number of copies to produce */
extern lchar_t	**Page;		/* Page characters */
extern int	NumPages;	/* Number of pages in document */
extern float	CharsPerInch,	/* Number of character columns per inch */
		LinesPerInch;	/* Number of lines per inch */
extern int	UTF8,		/* Use UTF-8 encoding? */
		NumKeywords;	/* Number of known keywords */
extern char	**Keywords;	/* List of known keywords... */


/*
 * Required functions...
 */

extern int	TextMain(const char *name, int argc, char *argv[]);
extern void	WriteEpilogue(void);
extern void	WritePage(void);
extern void	WriteProlog(const char *title, const char *user,
		            const char *classification, const char *label,
			    ppd_file_t *ppd);


/*
 * C++ magic...
 */

#ifdef __cplusplus
}
#endif /* __cplusplus */


/*
 * End of "$Id: textcommon.h 6649 2007-07-11 21:46:42Z mike $".
 */
