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

/*$Id: gsrect.h,v 1.1 2000/03/13 18:57:56 mike Exp $ */
/* Rectangle utilities */

#ifndef gsrect_INCLUDED
#  define gsrect_INCLUDED

/* Check whether one rectangle is included entirely within another. */
#define rect_within(inner, outer)\
  (inner.q.y <= outer.q.y && inner.q.x <= outer.q.x &&\
   inner.p.y >= outer.p.y && inner.p.x >= outer.p.x)

/*
 * Intersect two rectangles, replacing the first.  The result may be
 * anomalous (q < p) if the intersection is empty.
 */
#define rect_intersect(to, from)\
  BEGIN\
    if ( from.p.x > to.p.x ) to.p.x = from.p.x;\
    if ( from.q.x < to.q.x ) to.q.x = from.q.x;\
    if ( from.p.y > to.p.y ) to.p.y = from.p.y;\
    if ( from.q.y < to.q.y ) to.q.y = from.q.y;\
  END

/*
 * Calculate the difference of two rectangles, a list of up to 4 rectangles.
 * Return the number of rectangles in the list, and set the first rectangle
 * to the intersection.  The resulting first rectangle is guaranteed not to
 * be anomalous (q < p) iff it was not anomalous originally.
 *
 * Note that unlike the macros above, we need different versions of this
 * depending on the data type of the individual values: we'll only implement
 * the variations that we need.
 */
int int_rect_difference(P3(gs_int_rect * outer, const gs_int_rect * inner,
			   gs_int_rect * diffs /*[4] */ ));

#endif /* gsrect_INCLUDED */
