/* Copyright (C) 1992, 1994 Aladdin Enterprises.  All rights reserved.
  
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

/* gdevpcl.h */
/* Interface to PCL utilities for printer drivers */
/* Requires gdevprn.h */

/* Define the PCL paper size codes. */
#define PAPER_SIZE_LETTER 2
#define PAPER_SIZE_LEGAL 3
#define PAPER_SIZE_A4 26
#define PAPER_SIZE_A3 27
#define PAPER_SIZE_A2 28
#define PAPER_SIZE_A1 29
#define PAPER_SIZE_A0 30

/* Get the paper size code, based on width and height. */
int	gdev_pcl_paper_size(P1(gx_device *));

/* Color mapping procedures for 3-bit-per-pixel RGB printers */
dev_proc_map_rgb_color(gdev_pcl_3bit_map_rgb_color);
dev_proc_map_color_rgb(gdev_pcl_3bit_map_color_rgb);

/* Row compression routines */
typedef ulong word;
int	gdev_pcl_mode2compress(P3(const word *row, const word *end_row, byte *compressed)),
	gdev_pcl_mode3compress(P4(int bytecount, const byte *current, byte *previous, byte *compressed));
