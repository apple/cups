/* Copyright (C) 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gdevpsds.h,v 1.1 2000/03/13 19:00:47 mike Exp $ */
/* Image processing stream interface for PostScript and PDF writers */

#ifndef gdevpsds_INCLUDED
#  define gdevpsds_INCLUDED

#include "strimpl.h"

/* Convert between 1/2/4 bits and 8 bits. */
typedef struct stream_1248_state_s {
    stream_state_common;
    /* The following are set at initialization time. */
    uint samples_per_row;	/* >0 */
    int bits_per_sample;	/* 1, 2, 4 */
    /* The following are updated dynamically. */
    uint left;			/* # of samples left in current row */
} stream_1248_state;

/* Expand N (1, 2, 4) bits to 8. */
extern const stream_template s_1_8_template;
extern const stream_template s_2_8_template;
extern const stream_template s_4_8_template;

/* Reduce 8 bits to N (1, 2, 4) */
extern const stream_template s_8_1_template;
extern const stream_template s_8_2_template;
extern const stream_template s_8_4_template;

/* Initialize an expansion or reduction stream. */
#define s_1248_init(ss, Columns, samples_per_pixel)\
  ((ss)->samples_per_row = (Columns) * (samples_per_pixel),\
   (*(ss)->template->init)((stream_state *)(ss)))

/* Convert (8-bit) CMYK to RGB. */
typedef struct stream_C2R_state_s {
    stream_state_common;
    /* The following are set at initialization time. */
    const gs_imager_state *pis;	/* for UCR & BG */
} stream_C2R_state;

#define private_st_C2R_state()	/* in gdevpsds.c */\
  gs_private_st_ptrs1(st_C2R_state, stream_C2R_state, "stream_C2R_state",\
    c2r_enum_ptrs, c2r_reloc_ptrs, pis)
extern const stream_template s_C2R_template;

#define s_C2R_init(ss, pisv)\
  ((ss)->pis = (pisv), 0)

/* Downsample, possibly with anti-aliasing. */
#define stream_Downsample_state_common\
	stream_state_common;\
		/* The client sets the following before initialization. */\
	int Colors;\
	int Columns;		/* # of input columns */\
	int XFactor, YFactor;\
	bool AntiAlias;\
	bool padX, padY;	/* keep excess samples */\
		/* The following are updated dynamically. */\
	int x, y		/* position within input image */
#define s_Downsample_set_defaults_inline(ss)\
  ((ss)->AntiAlias = (ss)->padX = (ss)->padY = false)
typedef struct stream_Downsample_state_s {
    stream_Downsample_state_common;
} stream_Downsample_state;

/* Subsample */
/****** Subsample DOESN'T IMPLEMENT padY YET ******/
typedef struct stream_Subsample_state_s {
    stream_Downsample_state_common;
} stream_Subsample_state;
extern const stream_template s_Subsample_template;

/* Average */
typedef struct stream_Average_state_s {
    stream_Downsample_state_common;
    uint sum_size;
    uint copy_size;
    uint *sums;			/* accumulated sums for average */
} stream_Average_state;

#define private_st_Average_state()	/* in gdevpsds.c */\
  gs_private_st_ptrs1(st_Average_state, stream_Average_state,\
    "stream_Average_state", avg_enum_ptrs, avg_reloc_ptrs, sums)
extern const stream_template s_Average_template;

#endif /* gdevpsds_INCLUDED */
