/*
 * "$Id$"
 *
 *   Common PostScript text definitions for CUPS.
 *
 *   Copyright 2008-2010 by Apple Inc.
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
#include <cups/transcode.h>


/*
 * Constants...
 */

#define PS_NORMAL	0	/* Normal text */
#define PS_BOLD		1	/* Bold text */
#define PS_ITALIC	2	/* Italic text */
#define PS_BOLDITALIC	3	/* Bold italic text */

#define PS_LEFT		1	/* Left-justified text */
#define PS_CENTER	0	/* Center-justified text */
#define PS_RIGHT	-1	/* Right-justified text */


/*
 * Structures...
 */

typedef struct ps_text_s	/**** PostScript font data ****/
{
  char		*glyphs[65536];	/* PostScript glyphs for Unicode */
  int		num_fonts;	/* Number of fonts to use */
  char		*fonts[256][4];	/* Fonts to use */
  cups_array_t	*unique;	/* Unique fonts */
  unsigned short chars[65536],	/* 0xffcc (ff = font, cc = char) */
		codes[65536];	/* Unicode glyph mapping to fonts */
  int		widths[256],	/* Widths of each font */
		directions[256];/* Text directions for each font */
  float		size;		/* Current text size */
  int		style;		/* Current text style */
} ps_text_t;


/*
 * Functions...
 */

extern void		psTextEmbedFonts(ps_text_t *fonts);
extern void		psTextListFonts(ps_text_t *fonts);
extern ps_text_t	*psTextInitialize(void);
extern void		psTextUTF8(ps_text_t *fonts, float size, int style,
			           int align, const char *text);
extern void		psTextUTF32(ps_text_t *fonts, float size, int style,
			            int align, const cups_utf32_t *text,
				    int textlen);


/*
 * End of "$Id$".
 */
