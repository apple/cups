/* Copyright (C) 1998 Aladdin Enterprises.  All rights reserved.
  
  This file is part of GNU Ghostscript.
  
  GNU Ghostscript is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY.  No author or distributor accepts responsibility
  to anyone for the consequences of using it or for whether it serves any
  particular purpose or works at all, unless he says so in writing.  Refer
  to the GNU General Public License for full details.
  
  Everyone is granted permission to copy, modify and redistribute GNU
  Ghostscript, but only under the conditions described in the GNU General
  Public License.  A copy of this license is supposed to have been given
  to you along with GNU Ghostscript so you can know your rights and
  responsibilities.  It should be in a file named COPYING.  Among other
  things, the copyright notice and this notice must be preserved on all
  copies.
  
  Aladdin Enterprises supports the work of the GNU Project, but is not
  affiliated with the Free Software Foundation or the GNU Project.  GNU
  Ghostscript, as distributed by Aladdin Enterprises, does not require any
  GNU software to build or run it.
*/

/*$Id: gstext.h,v 1.1 2000/03/13 18:57:56 mike Exp $ */
/* Driver interface for text */

#ifndef gstext_INCLUDED
#  define gstext_INCLUDED

#include "gsccode.h"
#include "gsrefct.h"

/* EVERYTHING IN THIS FILE IS SUBJECT TO CHANGE WITHOUT NOTICE. */

/*
 * Note that like get_params and get_hardware_params, but unlike all other
 * driver procedures, text display must return information to the generic
 * code:
 *      *show except [x][y]show: the string escapement a.k.a. "width").
 *      charpath, .glyphpath: the entire character description.
 *      .charboxpath: the character bounding box.
 */

/*
 * Define the set of possible text operations.  While we define this as
 * a bit mask for convenience in testing, only certain combinations are
 * meaningful.  Specifically, the following are errors:
 *      - No FROM or DO.
 * The following are undefined:
 *      - More than one FROM or DO.
 *      - Both ADD_TO and REPLACE.
 */
#define TEXT_HAS_MORE_THAN_ONE_(op, any_)\
  ( ((op) & any_) & (((op) & any_) - 1) )
#define TEXT_OPERATION_IS_INVALID(op)\
  (!((op) & TEXT_FROM_ANY_) ||\
   !((op) & TEXT_DO_ANY_) ||\
   TEXT_HAS_MORE_THAN_ONE_(op, TEXT_FROM_ANY_) ||\
   TEXT_HAS_MORE_THAN_ONE_(op, TEXT_DO_ANY_) ||\
   (((op) & TEXT_ADD_ANY_) && ((op) & TEXT_REPLACE_ANY_))\
   )

	/* Define the representation of the text itself. */
#define TEXT_FROM_STRING          0x00001
#define TEXT_FROM_BYTES           0x00002
#define TEXT_FROM_CHARS           0x00004
#define TEXT_FROM_GLYPHS          0x00008
#define TEXT_FROM_ANY_ /* internal use only, see above */\
  (TEXT_FROM_STRING | TEXT_FROM_BYTES | TEXT_FROM_CHARS | TEXT_FROM_GLYPHS)
	/* Define how to compute escapements. */
#define TEXT_ADD_TO_ALL_WIDTHS    0x00010
#define TEXT_ADD_TO_SPACE_WIDTH   0x00020
#define TEXT_ADD_ANY_ /* internal use only, see above */\
  (TEXT_ADD_TO_ALL_WIDTHS | TEXT_ADD_TO_SPACE_WIDTH)
#define TEXT_REPLACE_X_WIDTHS     0x00040
#define TEXT_REPLACE_Y_WIDTHS     0x00080
#define TEXT_REPLACE_ANY_ /* internal use only, see above */\
  (TEXT_REPLACE_X_WIDTHS | TEXT_REPLACE_Y_WIDTHS)
	/* Define what result should be produced. */
#define TEXT_DO_NONE              0x00100	/* stringwidth or cshow only */
#define TEXT_DO_DRAW              0x00200
#define TEXT_DO_FALSE_CHARPATH    0x00400
#define TEXT_DO_TRUE_CHARPATH     0x00800
#define TEXT_DO_FALSE_CHARBOXPATH 0x01000
#define TEXT_DO_TRUE_CHARBOXPATH  0x02000
#define TEXT_DO_ANY_CHARPATH\
  (TEXT_DO_FALSE_CHARPATH | TEXT_DO_TRUE_CHARPATH |\
   TEXT_DO_FALSE_CHARBOXPATH | TEXT_DO_TRUE_CHARBOXPATH)
#define TEXT_DO_ANY_ /* internal use only, see above */\
  (TEXT_DO_NONE | TEXT_DO_DRAW | TEXT_DO_ANY_CHARPATH)
	/* Define whether the client intervenes between characters. */
#define TEXT_INTERVENE            0x10000
	/* Define whether to return the width. */
#define TEXT_RETURN_WIDTH         0x20000

/*
 * Define the structure of parameters passed in for text display.
 * Note that the implementation does not modify any of these; the client
 * must not modify them after initialization.
 */
typedef struct gs_text_params_s {
    /* The client must set the following in all cases. */
    uint operation;		/* TEXT_xxx mask */
    union sd_ {
	const byte *bytes;	/* FROM_STRING, FROM_BYTES */
	const gs_char *chars;	/* FROM_CHARS */
	const gs_glyph *glyphs;	/* FROM_GLYPHS */
    } data;
    uint size;			/* number of data elements */
    /* The following are used only in the indicated cases. */
    gs_point delta_all;		/* ADD_TO_ALL_WIDTHS */
    gs_point delta_space;	/* ADD_TO_SPACE_WIDTH */
    union s_ {
	gs_char s_char;		/* ADD_TO_SPACE_WIDTH & !FROM_GLYPHS */
	gs_glyph s_glyph;	/* ADD_TO_SPACE_WIDTH & FROM_GLYPHS */
    } space;
    /* If x_widths == y_widths, widths are taken in pairs. */
    /* Either one may be NULL, meaning widths = 0. */
    const float *x_widths;	/* REPLACE_X_WIDTHS */
    const float *y_widths;	/* REPLACE_Y_WIDTHS */
    /* The following are for internal use only, not by clients. */
    gs_const_string gc_string;	/* for use only during GC */
} gs_text_params_t;

#define st_gs_text_params_max_ptrs 3
/*extern_st(st_gs_text_params); */
#define public_st_gs_text_params() /* in gstext.c */\
  gs_public_st_composite(st_gs_text_params, gs_text_params_t,\
    "gs_text_params", text_params_enum_ptrs, text_params_reloc_ptrs)

/* Define the abstract type for the object procedures. */
typedef struct gs_text_enum_procs_s gs_text_enum_procs_t;

/*
 * Define the common part of the structure that tracks the state of text
 * display.  All implementations of text_begin must allocate one of these
 * using rc_alloc_struct_1; implementations may subclass and extend it.
 * Note that it includes a copy of the text parameters.
 */
#ifndef gx_device_DEFINED
#  define gx_device_DEFINED
typedef struct gx_device_s gx_device;

#endif
#define gs_text_enum_common\
	/* The following are set at initialization, and const thereafter. */\
	gs_text_params_t text;\
	const gs_text_enum_procs_t *procs;\
	gx_device *dev;\
	/* The following change dynamically. */\
	rc_header rc;\
	uint index		/* index within string */
typedef struct gs_text_enum_s {
    gs_text_enum_common;
} gs_text_enum_t;

#define st_gs_text_enum_max_ptrs st_gs_text_params_max_ptrs
/*extern_st(st_gs_text_enum); */
#define public_st_gs_text_enum()	/* in gstext.c */\
  gs_public_st_composite(st_gs_text_enum, gs_text_enum_t, "gs_text_enum_t",\
    text_enum_enum_ptrs, text_enum_reloc_ptrs)

/* Begin processing text. */
/* Note that these take a graphics state argument. */
#ifndef gs_state_DEFINED
#  define gs_state_DEFINED
typedef struct gs_state_s gs_state;

#endif
int gs_text_begin(P4(gs_state * pgs, const gs_text_params_t * text,
		     gs_memory_t * mem, gs_text_enum_t ** ppenum));

/* Begin the PostScript-equivalent text operators. */
int
    gs_show_begin(P5(gs_state *, const byte *, uint,
		     gs_memory_t *, gs_text_enum_t **)), gs_ashow_begin(P7(gs_state *, floatp, floatp, const byte *, uint,
					 gs_memory_t *, gs_text_enum_t **)),
      gs_widthshow_begin(P8(gs_state *, floatp, floatp, gs_char,
			    const byte *, uint,
			    gs_memory_t *, gs_text_enum_t **)), gs_awidthshow_begin(P10(gs_state *, floatp, floatp, gs_char,
					 floatp, floatp, const byte *, uint,
					 gs_memory_t *, gs_text_enum_t **)),
      gs_kshow_begin(P5(gs_state *, const byte *, uint,
			gs_memory_t *, gs_text_enum_t **)), gs_xyshow_begin(P7(gs_state *, const byte *, uint,
					       const float *, const float *,
					 gs_memory_t *, gs_text_enum_t **)),
      gs_glyphshow_begin(P4(gs_state *, gs_glyph,
			    gs_memory_t *, gs_text_enum_t **)), gs_cshow_begin(P5(gs_state *, const byte *, uint,
					 gs_memory_t *, gs_text_enum_t **)),
      gs_stringwidth_begin(P5(gs_state *, const byte *, uint,
			      gs_memory_t *, gs_text_enum_t **)), gs_charpath_begin(P6(gs_state *, const byte *, uint, bool,
					 gs_memory_t *, gs_text_enum_t **)),
      gs_glyphpath_begin(P5(gs_state *, gs_glyph, bool,
			    gs_memory_t *, gs_text_enum_t **)), gs_charboxpath_begin(P6(gs_state *, const byte *, uint, bool,
					 gs_memory_t *, gs_text_enum_t **));

/*
 * Define the possible return values from gs_text_process.  The client
 * should call text_process until it returns 0 (successful completion) or a
 * negative (error) value.
 */

	/*
	 * The client must render a character: obtain the code from
	 * gs_show_current_char, do whatever is necessary, and then
	 * call gs_text_process again.
	 */
#define TEXT_PROCESS_RENDER 1

	/*
	 * The client has asked to intervene between characters.
	 * Obtain the previous and next codes from gs_show_previous_char
	 * and gs_kshow_next_char, do whatever is necessary, and then
	 * call gs_text_process again.
	 */
#define TEXT_PROCESS_INTERVENE 2

/* Process text after 'begin'. */
int gs_text_process(P1(gs_text_enum_t * penum));

/* Set text metrics and optionally enable caching. */
int
    gs_text_setcharwidth(P2(gs_text_enum_t * penum, const double wxy[2])),
       gs_text_setcachedevice(P2(gs_text_enum_t * penum, const double wbox[6])),
       gs_text_setcachedevice2(P2(gs_text_enum_t * penum, const double wbox2[10]));

/* Release the text processing structures. */
void gs_text_release(P2(gs_text_enum_t * penum, client_name_t cname));

#endif /* gstext_INCLUDED */
