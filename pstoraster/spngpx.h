/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* spngpx.h */
/* PNG pixel prediction filter state definition */
/* Requires strimpl.h */

/* PNGPredictorDecode / PNGPredictorEncode */
typedef struct stream_PNGP_state_s {
	stream_state_common;
		/* The client sets the following before initialization. */
	int Colors;		/* # of colors, 1..16 */
	int BitsPerComponent;	/* 1, 2, 4, 8, 16 */
	uint Columns;		/* >0 */
	int Predictor;		/* 10-15, only relevant for Encode */
		/* The init procedure computes the following. */
	uint row_count;		/* # of bytes per row */
	byte end_mask;		/* mask for left-over bits in last byte */
	int bpp;		/* bytes per pixel */
	byte *prev_row;		/* previous row */
	int case_index;		/* switch index for case dispatch, */
				/* set dynamically when decoding */
		/* The following are updated dynamically. */
	long row_left;		/* # of bytes left in row */
	byte prev[32];		/* previous samples */
} stream_PNGP_state;
#define private_st_PNGP_state()	/* in sPNGP.c */\
  gs_private_st_ptrs1(st_PNGP_state, stream_PNGP_state,\
    "PNGPredictorEncode/Decode state", pngp_enum_ptrs, pngp_reloc_ptrs,\
    prev_row)
#define s_PNGP_set_defaults_inline(ss)\
  ((ss)->Colors = 1, (ss)->BitsPerComponent = 8, (ss)->Columns = 1,\
   (ss)->Predictor = 15)
extern const stream_template s_PNGPD_template;
extern const stream_template s_PNGPE_template;
