/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gsflip.h,v 1.3 2000/03/13 18:57:55 mike Exp $ */
/* Interface to routines for "flipping" image data */

#ifndef gsflip_INCLUDED
#  define gsflip_INCLUDED

/*
 * Convert line-based (MultipleDataSource) input to the chunky format
 * used everywhere else.
 *
 * We store the output at buffer.
 * Each row of input must consist of an integral number of pixels.
 * In particular, for 12-bit input, nbytes must be 0 mod 3.
 * offset is the amount to be added to each plane pointer.
 * num_planes must be 3 or 4; bits_per_sample must be 1, 2, 4, 8, or 12.
 * Returns -1 if num_planes or bits_per_sample is invalid, otherwise 0.
 */
extern int image_flip_planes(P6(byte * buffer, const byte ** planes,
				uint offset, uint nbytes,
				int num_planes, int bits_per_sample));

#endif /* gsflip_INCLUDED */
