/* Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gscompt.h,v 1.1 2000/03/13 18:57:55 mike Exp $ */
/* Abstract types for compositing objects */

#ifndef gscompt_INCLUDED
#  define gscompt_INCLUDED

/*
 * Compositing is the next-to-last step in the rendering pipeline.
 * It occurs after color correction but before halftoning (if needed).
 *
 * gs_composite_t is the abstract superclass for compositing functions such
 * as RasterOp functions or alpha-based compositing.  Concrete subclasses
 * must provide a default implementation (presumably based on
 * get_bits_rectangle and copy_color) for devices that provide no optimized
 * implementation of their own.
 *
 * A client that wants to produce composited output asks the target device
 * to create an appropriate compositing device based on the target device
 * and the gs_composite_t (and possibly other elements of the imager state).
 * If the target device doesn't have its own implementation for the
 * requested function, format, and state, it passes the buck to the
 * gs_composite_t, which may make further tests for special cases before
 * creating and returning a compositing device that uses the default
 * implementation.
 */
typedef struct gs_composite_s gs_composite_t;

/*
 * To enable fast cache lookup and equality testing, compositing functions,
 * like halftones, black generation functions, etc., carry a unique ID (time
 * stamp).
 */
gs_id gs_composite_id(P1(const gs_composite_t * pcte));

#endif /* gscompt_INCLUDED */
