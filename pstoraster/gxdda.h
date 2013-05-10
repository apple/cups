/* Copyright (C) 1994, 1995 Aladdin Enterprises.  All rights reserved.
  
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

/* gxdda.h */
/* DDA definitions for Ghostscript line drawing */
/* Requires gxfixed.h */

/*
 * We use the familiar Bresenham DDA algorithm for several purposes:
 *	- tracking the edges when filling trapezoids;
 *	- tracking the current pixel corner coordinates when rasterizing
 *	skewed or rotated images;
 *	- converting curves to sequences of lines (this is a 3rd-order
 *	DDA, the others are 1st-order);
 *	- perhaps someday for drawing single-pixel lines.
 * In the case of trapezoids, lines, and curves, we need to use
 * the DDA to find the integer X values at integer+0.5 values of Y;
 * in the case of images, we use DDAs to compute the (fixed)
 * X and Y values at (integer) source pixel corners.
 *
 * The purpose of the DDA is to compute the exact values Q(i) = floor(i*D/N)
 * for increasing integers i, 0 <= i <= N.  D is considered to be an
 * integer, although it may actually be a fixed.  For the algorithm,
 * we maintain i*D/N as Q + R/N where Q and R are integers, 0 <= R < N,
 * with the following auxiliary values:
 *	dQ = floor(D/N)
 *	dR = D mod N
 *	NdR = N - dR
 * Then at each iteration we do:
 *	Q += dQ;
 *	if ( R < dR ) ++Q, R += NdR; else R -= dR;
 * These formulas work regardless of the sign of D, and never let R go
 * out of range.
 */
#define dda_state_struct(sname, dtype, ntype)\
  struct sname { dtype Q; ntype R; }
#define dda_step_struct(sname, dtype, ntype)\
  struct sname { dtype dQ; ntype dR, NdR; }
/* DDA with fixed Q and (unsigned) integer N */
typedef dda_state_struct(_a, fixed, uint) gx_dda_state_fixed;
typedef dda_step_struct(_e, fixed, uint) gx_dda_step_fixed;
typedef struct gx_dda_fixed_s {
  gx_dda_state_fixed state;
  gx_dda_step_fixed step;
} gx_dda_fixed;
/*
 * Initialize a DDA.  The sign test is needed only because C doesn't
 * provide reliable definitions of / and % for integers (!!!).
 */
#define dda_init_state(dstate, init, N)\
  (dstate).Q = (init), (dstate).R = (N)
#define dda_init_step(dstep, D, N)\
  if ( (N) == 0 )\
    (dstep).dQ = 0, (dstep).dR = 0;\
  else if ( (D) < 0 )\
   { (dstep).dQ = -(-(D) / (N));\
     if ( ((dstep).dR = -(D) % (N)) != 0 )\
       --(dstep).dQ, (dstep).dR = (N) - (dstep).dR;\
   }\
  else\
   { (dstep).dQ = (D) / (N); (dstep).dR = (D) % (N); }\
  (dstep).NdR = (N) - (dstep).dR
#define dda_init(dda, init, D, N)\
  dda_init_state((dda).state, init, N);\
  dda_init_step((dda).step, D, N)
/*
 * Compute the sum of two DDA steps with the same D and N.
 */
#define dda_step_add(tostep, fromstep)\
  (tostep).dQ +=\
    ((tostep).dR < (fromstep).NdR ?\
     ((tostep).dR += (fromstep).dR, (fromstep).dQ) :\
     ((tostep).dR -= (fromstep).NdR, (fromstep).dQ + 1))
/*
 * Return the current value in a DDA.
 */
#define dda_state_current(dstate) (dstate).Q
#define dda_current(dda) dda_state_current((dda).state)
/*
 * Increment a DDA to the next point.
 * Returns the updated current value.
 */
#define dda_state_next(dstate, dstep)\
  (dstate).Q +=\
    ((dstate).R >= (dstep).dR ?\
     ((dstate).R -= (dstep).dR, (dstep).dQ) :\
     ((dstate).R += (dstep).NdR, (dstep).dQ + 1))
#define dda_next(dda) dda_state_next((dda).state, (dda).step)
/*
 * Back up a DDA to the previous point.
 * Returns the updated current value.
 */
#define dda_state_previous(dstate, dstep)\
  (dstate).Q -=\
    ((dstate).R < (dstep).NdR ?\
     ((dstate).R += (dstep).dR, (dstep).dQ) :\
     ((dstate).R -= (dstep).NdR, (dstep).dQ + 1))
#define dda_previous(dda) dda_state_previous((dda).state, (dda).step)
