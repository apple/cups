/* Copyright (C) 1991, 1992, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gxop1.h,v 1.2 2000/03/08 23:15:02 mike Exp $ */
/* Type 1 state shared between interpreter and compiled fonts. */

#ifndef gxop1_INCLUDED
#  define gxop1_INCLUDED

/*
 * The current point (px,py) in the Type 1 interpreter state is not
 * necessarily the same as the current position in the path being built up.
 * Specifically, (px,py) may not reflect adjustments for hinting,
 * whereas the current path position does reflect those adjustments.
 */

/* Define the shared Type 1 interpreter state. */
#define max_coeff_bits 11	/* max coefficient in char space */
typedef struct gs_op1_state_s {
    struct gx_path_s *ppath;
    struct gs_type1_state_s *pcis;
    fixed_coeff fc;
    gs_fixed_point co;		/* character origin (device space) */
    gs_fixed_point p;		/* current point (device space) */
} gs_op1_state;
typedef gs_op1_state *is_ptr;

/* Define the state used by operator procedures. */
/* These macros refer to a current instance (s) of gs_op1_state. */
#define sppath s.ppath
#define sfc s.fc
#define spt s.p
#define ptx s.p.x
#define pty s.p.y

/* Accumulate relative coordinates */
/****** THESE ARE NOT ACCURATE FOR NON-INTEGER DELTAS. ******/
/* This probably doesn't make any difference in practice. */
#define c_fixed(d, c) m_fixed(d, c, sfc, max_coeff_bits)
#define accum_x(dx)\
  BEGIN\
    ptx += c_fixed(dx, xx);\
    if ( sfc.skewed ) pty += c_fixed(dx, xy);\
  END
#define accum_y(dy)\
  BEGIN\
    pty += c_fixed(dy, yy);\
    if ( sfc.skewed ) ptx += c_fixed(dy, yx);\
  END
void accum_xy_proc(P3(is_ptr ps, fixed dx, fixed dy));

#define accum_xy(dx,dy)\
  accum_xy_proc(&s, dx, dy)

/* Define operator procedures. */
int gs_op1_closepath(P1(is_ptr ps));
int gs_op1_rrcurveto(P7(is_ptr ps, fixed dx1, fixed dy1,
			fixed dx2, fixed dy2, fixed dx3, fixed dy3));

#endif /* gxop1_INCLUDED */
