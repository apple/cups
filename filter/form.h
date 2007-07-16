/*
 * "$Id: form.h 6649 2007-07-11 21:46:42Z mike $"
 *
 *   CUPS form header file for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007 by Apple Inc.
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
 * Form elements...
 */

typedef enum
{
  ELEMENT_FILE = -1,		/* Pseudo element, not in file, but above */
  ELEMENT_FRAGMENT,	/* Text fragment */
  ELEMENT_COMMENT,	/* <!-- .... --> */
  ELEMENT_ARC,
  ELEMENT_BOX,
  ELEMENT_BR,
  ELEMENT_B,
  ELEMENT_CUPSFORM,
  ELEMENT_DEFVAR,
  ELEMENT_FONT,
  ELEMENT_H1,
  ELEMENT_H2,
  ELEMENT_H3,
  ELEMENT_H4,
  ELEMENT_H5,
  ELEMENT_H6,
  ELEMENT_HEAD,
  ELEMENT_IMG,
  ELEMENT_I,
  ELEMENT_LINE,
  ELEMENT_PAGE,
  ELEMENT_PIE,
  ELEMENT_POLY,
  ELEMENT_PRE,
  ELEMENT_P,
  ELEMENT_RECT,
  ELEMENT_TEXT,
  ELEMENT_TT,
  ELEMENT_VAR
} element_t;


/*
 * Font styles...
 */

typedef enum
{
  STYLE_NORMAL,
  STYLE_BOLD,
  STYLE_ITALIC,
  STYLE_BOLD_ITALIC
} style_t;


/*
 * Text alignments...
 */

typedef enum
{
  HALIGN_LEFT,
  HALIGN_CENTER,
  HALIGN_RIGHT
} halign_t;

typedef enum
{
  VALIGN_BOTTOM,
  VALIGN_CENTER,
  VALIGN_TOP
} valign_t;


/*
 * Text directions...
 */

typedef enun
{
  DIR_LEFT_TO_RIGHT,
  DIR_RIGHT_TO_LEFT
} dir_t;


/*
 * Attribute structure...
 */

typedef struct
{
  char			*name,		/* Name of attribute */
			*value;		/* Value of attribute */
} attr_t;


/*
 * Form document tree structure...
 */

typedef struct tree_str
{
  struct tree_str	*prev,		/* Previous tree node */
			*next,		/* Next tree node */
			*parent,	/* Parent tree node */
			*child,		/* First child node */
			*last_child;	/* Last child node */
  element_t		element;	/* Element type */
  float			x, y, w, h;	/* Position and size in points */
  float			bg[3], fg[3];	/* Colors of element */
  float			thickness;	/* Thickness of lines */
  int			preformatted;	/* Preformatted text? */
  float			size;		/* Height of text in points */
  char			*typeface;	/* Typeface of text */
  style_t		style;		/* Style of text */
  halign_t		halign;		/* Horizontal alignment */
  valign_t		valign;		/* Vertical alignment */
  dir_t			dir;		/* Direction of text */
  int			num_attrs;	/* Number of attributes */
  attr_t		*attrs;		/* Attributes */
  void			*data;		/* Text fragment data */
} tree_t;


/*
 * Globals...
 */

extern int		NumOptions;	/* Number of command-line options */
extern cups_option_t	*Options;	/* Command-line options */
extern ppd_file_t	*PPD;		/* PPD file */


/*
 * Prototypes...
 */

extern void	formDelete(tree_t *t);
extern char	*formGetAttr(tree_t *t, const char *name);
extern tree_t	*formNew(tree_t *p);
extern tree_t	*formRead(FILE *fp, tree_t *p);
extern void	formSetAttr(tree_t *t, const char *name, const char *value);
extern void	formWrite(tree_t *p);


/*
 * End of "$Id: form.h 6649 2007-07-11 21:46:42Z mike $".
 */
