/* Copyright (C) 1994 Aladdin Enterprises.  All rights reserved.
  
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

/* gdev8bcm.h */
/* Dynamic color mapping for 8-bit displays */
/* Requires gxdevice.h (for gx_color_value) */

/*
 * The MS-DOS, MS Windows, and X Windows drivers all use (at least on
 * some platforms) an 8-bit color map in which some fraction is reserved
 * for a pre-allocated cube and some or all of the remainder is
 * allocated dynamically.  Since looking up colors in this map can be
 * a major performance bottleneck, we provide an efficient implementation
 * that can be shared among drivers.
 *
 * As a performance compromise, we only look up the top 5 bits of the
 * RGB value in the color map.  This compromises color quality very little,
 * and allows substantial optimizations.
 */

#define gx_8bit_map_size 323
#define gx_8bit_map_spreader 123	/* approx. 323 - (1.618 * 323) */
typedef struct gx_8bit_map_entry_s {
	ushort rgb;			/* key = 0rrrrrgggggbbbbb */
#define gx_8bit_no_rgb ((ushort)0xffff)
#define gx_8bit_rgb_key(r, g, b)\
  (((r >> (gx_color_value_bits - 5)) << 10) +\
   ((g >> (gx_color_value_bits - 5)) << 5) +\
   (b >> (gx_color_value_bits - 5)))
	short index;			/* value */
} gx_8bit_map_entry;
typedef struct gx_8bit_color_map_s {
	int count;			/* # of occupied entries */
	int max_count;			/* max # of occupied entries */
	gx_8bit_map_entry map[gx_8bit_map_size + 1];
} gx_8bit_color_map;

/* Initialize an 8-bit color map. */
void gx_8bit_map_init(P2(gx_8bit_color_map *, int));

/* Look up a color in an 8-bit color map. */
/* Return -1 if not found. */
int gx_8bit_map_rgb_color(P4(const gx_8bit_color_map *, gx_color_value,
			     gx_color_value, gx_color_value));

/* Test whether an 8-bit color map has room for more entries. */
#define gx_8bit_map_is_full(pcm)\
  ((pcm)->count == (pcm)->max_count)

/* Add a color to an 8-bit color map. */
/* Return -1 if the map is full. */
int gx_8bit_add_rgb_color(P4(gx_8bit_color_map *, gx_color_value,
			     gx_color_value, gx_color_value));
