/* Copyright (C) 1994, 1995 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: spdiffx.h,v 1.2 2000/03/08 23:15:27 mike Exp $ */
/* Requires strimpl.h */

#ifndef spdiffx_INCLUDED
#  define spdiffx_INCLUDED

/* PixelDifferenceDecode / PixelDifferenceEncode */
typedef struct stream_PDiff_state_s {
    stream_state_common;
    /* The client sets the following before initialization. */
    int Colors;			/* # of colors, 1..4 */
    int BitsPerComponent;	/* 1, 2, 4, 8 */
    int Columns;
    /* The init procedure computes the following. */
    uint row_count;		/* # of bytes per row */
    byte end_mask;		/* mask for left-over bits in last byte */
    int case_index;		/* switch index for case dispatch */
    /* The following are updated dynamically. */
    uint row_left;		/* # of bytes left in row */
    byte s0, s1, s2, s3;	/* previous sample */
} stream_PDiff_state;

#define private_st_PDiff_state()	/* in spdiff.c */\
  gs_private_st_simple(st_PDiff_state, stream_PDiff_state,\
    "PixelDifferenceEncode/Decode state")
#define s_PDiff_set_defaults_inline(ss)\
  ((ss)->Colors = 1, (ss)->BitsPerComponent = 8, (ss)->Columns = 1)
extern const stream_template s_PDiffD_template;
extern const stream_template s_PDiffE_template;

#endif /* spdiffx_INCLUDED */
