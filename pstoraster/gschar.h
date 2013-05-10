/* Copyright (C) 1989, 1995 Aladdin Enterprises.  All rights reserved.
  
  This file is part of GNU Ghostscript.
  
  GNU Ghostscript is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY.  No author or distributor accepts responsibility to
  anyone for the consequences of using it or for whether it serves any
  particular purpose or works at all, unless he says so in writing.  Refer to
  the GNU General Public License for full details.
  
  Everyone is granted permission to copy, modify and redistribute GNU
  Ghostscript, but only under the conditions described in the GNU General
  Public License.  A copy of this license is supposed to have been given to
  you along with GNU Ghostscript so you can know your rights and
  responsibilities.  It should be in a file named COPYING.  Among other
  things, the copyright notice and this notice must be preserved on all
  copies.
  
  Aladdin Enterprises is not affiliated with the Free Software Foundation or
  the GNU Project.  GNU Ghostscript, as distributed by Aladdin Enterprises,
  does not depend on any other GNU software.
*/

/* gschar.h */
/* Client interface to character operations */
#include "gsccode.h"
#include "gscpm.h"

/* String display, like image display, uses an enumeration structure */
/* to keep track of what's going on (aka 'poor man's coroutine'). */
#ifndef gs_show_enum_DEFINED
#  define gs_show_enum_DEFINED
typedef struct gs_show_enum_s gs_show_enum;
#endif

/* Define an opaque type for fonts if necessary. */
#ifndef gs_font_DEFINED
#  define gs_font_DEFINED
typedef struct gs_font_s gs_font;
#endif

/* Allocate an enumerator. */
gs_show_enum *gs_show_enum_alloc(P3(gs_memory_t *, gs_state *, client_name_t));
/* Release the contents of an enumerator. */
/* (This happens automatically if the enumeration finishes normally.) */
/* If the second argument is not NULL, also free the enumerator. */
void gs_show_enum_release(P2(gs_show_enum *, gs_memory_t *));

/* The initialization routines all come in two versions, */
/* one that uses the C convention of null-terminated strings, */
/* and one that supplies a length. */
int	gs_show_init(P3(gs_show_enum *, gs_state *, const char *)),
	gs_show_n_init(P4(gs_show_enum *, gs_state *, const char *, uint)),
	gs_ashow_init(P5(gs_show_enum *, gs_state *, floatp, floatp, const char *)),
	gs_ashow_n_init(P6(gs_show_enum *, gs_state *, floatp, floatp, const char *, uint)),
	gs_widthshow_init(P6(gs_show_enum *, gs_state *, floatp, floatp, gs_char, const char *)),
	gs_widthshow_n_init(P7(gs_show_enum *, gs_state *, floatp, floatp, gs_char, const char *, uint)),
	gs_awidthshow_init(P8(gs_show_enum *, gs_state *, floatp, floatp, gs_char, floatp, floatp, const char *)),
	gs_awidthshow_n_init(P9(gs_show_enum *, gs_state *, floatp, floatp, gs_char, floatp, floatp, const char *, uint)),
	gs_kshow_init(P3(gs_show_enum *, gs_state *, const char *)),
	gs_kshow_n_init(P4(gs_show_enum *, gs_state *, const char *, uint)),
	gs_xyshow_init(P3(gs_show_enum *, gs_state *, const char *)),
	gs_xyshow_n_init(P4(gs_show_enum *, gs_state *, const char *, uint)),
	gs_glyphshow_init(P3(gs_show_enum *, gs_state *, gs_glyph)),
	gs_cshow_init(P3(gs_show_enum *, gs_state *, const char *)),
	gs_cshow_n_init(P4(gs_show_enum *, gs_state *, const char *, uint)),
	gs_stringwidth_init(P3(gs_show_enum *, gs_state *, const char *)),
	gs_stringwidth_n_init(P4(gs_show_enum *, gs_state *, const char *, uint)),
	gs_charpath_init(P4(gs_show_enum *, gs_state *, const char *, bool)),
	gs_charpath_n_init(P5(gs_show_enum *, gs_state *, const char *, uint, bool)),
	gs_charboxpath_init(P4(gs_show_enum *, gs_state *, const char *, bool)),
	gs_charboxpath_n_init(P5(gs_show_enum *, gs_state *, const char *, uint, bool));

/* After setting up the enumeration, all the string-related routines */
/* work the same way.  The client calls gs_show_next until it returns */
/* a zero (successful completion) or negative (error) value. */
/* Other values indicate the following situations: */

	/* The client must render a character: obtain the code from */
	/* gs_show_current_char, do whatever is necessary, and then */
	/* call gs_show_next again. */
#define gs_show_render 1

	/* The client has asked to intervene between characters (kshow). */
	/* Obtain the previous and next codes from gs_kshow_previous_char */
	/* and gs_kshow_next_char, do whatever is necessary, and then */
	/* call gs_show_next again. */
#define gs_show_kern 2

	/* The client has asked to handle characters individually */
	/* (xshow, yshow, xyshow, cshow).  Obtain the current code */
	/* from gs_show_current_char, do whatever is necessary, and then */
	/* call gs_show_next again. */
#define gs_show_move 3

int	gs_show_next(P1(gs_show_enum *));
gs_char
	gs_show_current_char(P1(const gs_show_enum *)),
	gs_kshow_previous_char(P1(const gs_show_enum *)),
	gs_kshow_next_char(P1(const gs_show_enum *));
gs_font *
	gs_show_current_font(P1(const gs_show_enum *));
gs_glyph
	gs_show_current_glyph(P1(const gs_show_enum *));
int	gs_show_current_width(P2(const gs_show_enum *, gs_point *));
void	gs_show_width(P2(const gs_show_enum *, gs_point *));	/* cumulative width */
gs_char_path_mode
	gs_show_in_charpath(P1(const gs_show_enum *));	/* return charpath flag */

/* Character cache and metrics operators. */
/* gs_setcachedevice[2] return 1 iff the cache device was just installed. */
int	gs_setcachedevice(P3(gs_show_enum *, gs_state *, const float * /*[6]*/));
int	gs_setcachedevice2(P3(gs_show_enum *, gs_state *, const float * /*[10]*/));
int	gs_setcharwidth(P4(gs_show_enum *, gs_state *, floatp, floatp));
/* Return true if we only need the width from the rasterizer */
/* and can short-circuit the full rendering of the character, */
/* false if we need the actual character bits. */
/* This is only meaningful just before calling gs_setcharwidth or */
/* gs_setcachedevice[2]. */
bool	gs_show_width_only(P1(const gs_show_enum *));
