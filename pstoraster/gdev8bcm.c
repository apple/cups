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

/* gdev8bcm.c */
/* Dynamic color mapping for 8-bit displays */
#include "gx.h"
#include "gxdevice.h"
#include "gdev8bcm.h"

/* Initialize an 8-bit color map. */
void
gx_8bit_map_init(gx_8bit_color_map *pcm, int max_count)
{	int i;
	pcm->count = 0;
	pcm->max_count = max_count;
	for ( i = 0; i < gx_8bit_map_size; i++ )
	  pcm->map[i].rgb = gx_8bit_no_rgb;
}

/* Look up a color in an 8-bit color map. */
/* Return <0 if not found. */
int
gx_8bit_map_rgb_color(const gx_8bit_color_map *pcm, gx_color_value r,
  gx_color_value g, gx_color_value b)
{	ushort rgb = gx_8bit_rgb_key(r, g, b);
	const gx_8bit_map_entry *pme =
	  &pcm->map[(rgb * gx_8bit_map_spreader) % gx_8bit_map_size];
	for ( ; ; pme++ )
	  {	if ( pme->rgb == rgb )
		  return pme->index;
		else if ( pme->rgb == gx_8bit_no_rgb )
		  break;
	  }
	if ( pme != &pcm->map[gx_8bit_map_size] )
	  return pme - &pcm->map[gx_8bit_map_size];
	/* We ran off the end; wrap around and continue. */
	pme = &pcm->map[0];
	for ( ; ; pme++ )
	  {	if ( pme->rgb == rgb )
		  return pme->index;
		else if ( pme->rgb == gx_8bit_no_rgb )
		  return pme - &pcm->map[gx_8bit_map_size];
	  }
}

/* Add a color to an 8-bit color map after an unsuccessful lookup, */
/* and return its index.  Return <0 if the map is full. */
int
gx_8bit_add_rgb_color(gx_8bit_color_map *pcm, gx_color_value r,
  gx_color_value g, gx_color_value b)
{	int index;
	gx_8bit_map_entry *pme;
	if ( gx_8bit_map_is_full(pcm) )
	  return -1;
	index = gx_8bit_map_rgb_color(pcm, r, g, b);
	if ( index >= 0 )			/* shouldn't happen */
	  return index;
	pme = &pcm->map[-index];
	pme->rgb = gx_8bit_rgb_key(r, g, b);
	return (pme->index = pcm->count++);
}
