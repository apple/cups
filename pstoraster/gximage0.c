/* Copyright (C) 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gximage0.c */
/* Generic image enumeration and cleanup */
#include "gx.h"
#include "memory_.h"
#include "gserrors.h"
#include "gxdevice.h"
#include "gxcpath.h"
#include "gximage.h"

/* Process the next piece of an image */
int
gx_default_image_data(gx_device *dev,
  void *info, const byte **planes, uint raster,
  int x, int y, int dwidth, int dheight)
{	gx_image_enum *penum = info;
	int x_end = x + dwidth;
	int y_end = y + dheight;
	uint rsize = penum->bytes_per_row;
	uint pos = penum->byte_in_row;
	int width = penum->width;
	int nplanes = penum->num_planes;
	uint bcount =			/* bytes per data row */
	  (dwidth * penum->bps * (penum->spp / penum->num_planes) + 7) >> 3;
	uint dpos = 0;
	fixed adjust = penum->adjust;
	int code;

	if ( dwidth == 0 || dheight == 0 )
	  return 0;
	if ( penum->x != x || penum->y != y )
	  return_error(gs_error_rangecheck);
	if ( x_end < width )
	  --y_end;
	else
	  x_end = 0;

	/* We've accumulated an entire set of planes. */
	/* Set up the clipping and/or RasterOp device if needed. */

	if ( penum->clip_dev )
	   {	gx_device_clip *cdev = penum->clip_dev;
		cdev->target = dev;
		dev = (gx_device *)cdev;
	   }
	if ( penum->rop_dev )
	  {	gx_device_rop_texture *rtdev = penum->rop_dev;
		((gx_device_forward *)rtdev)->target = dev;
		dev = (gx_device *)rtdev;
	  }

	/* Now render complete rows. */

	while ( penum->x != x_end || penum->y < y_end )
	   {	/* Fill up a row, then display it. */
		int px;

		for ( px = 0; px < nplanes; px++ )
		  (*penum->unpack)(penum->buffer + (px << penum->log2_xbytes),
				   planes[px] + dpos, bcount,
				   &penum->map[px], penum->spread, pos);
		pos += bcount;
		dpos += bcount;
		if ( pos == rsize )	/* filled an entire row */
		  {
#ifdef DEBUG
			if ( gs_debug_c('B') )
			  { int i, n = width * penum->spp;
			    dputs("[B]row:");
			    for ( i = 0; i < n; i++ )
			      dprintf1(" %02x", penum->buffer[i]);
			    dputs("\n");
			  }
#endif
			penum->xcur = dda_current(penum->next_x);
			dda_next(penum->next_x);
			penum->ycur = dda_current(penum->next_y);
			dda_next(penum->next_y);
			if ( !penum->interpolate )
			  switch ( penum->posture )
			  {
			  case image_portrait:
			    {	/* Precompute integer y and height, */
				/* and check for clipping. */
				fixed yc = penum->ycur,
				  yn = dda_current(penum->next_y);

				if ( yn < yc )
				  { fixed temp = yn; yn = yc; yc = temp; }
				yc -= adjust;
				if ( yc >= penum->clip_outer.q.y ) goto mt;
				yn += adjust;
				if ( yn <= penum->clip_outer.p.y ) goto mt;
				penum->yci = fixed2int_pixround(yc);
				penum->hci =
				  fixed2int_pixround(yn) - penum->yci;
				if ( penum->hci == 0 ) goto mt;
			    }	break;
			  case image_landscape:
			    {	/* Check for no pixel centers in x. */
				fixed xc = penum->xcur,
				  xn = dda_current(penum->next_x);

				xc -= adjust;
				if ( xn < xc )
				  { fixed temp = xn; xn = xc; xc = temp; }
				if ( xc >= penum->clip_outer.q.x ) goto mt;
				xn += adjust;
				if ( xn <= penum->clip_outer.p.x ) goto mt;
				penum->xci = fixed2int_pixround(xc);
				penum->wci =
				  fixed2int_pixround(xn) - penum->xci;
				if ( penum->wci == 0 ) goto mt;
			    }	break;
			  case image_skewed:
			    ;
			  }
			code = (*penum->render)(penum, penum->buffer,
						width * penum->spp, 1, dev);
			if ( code < 0 )
			  goto err;
mt:			penum->x = 0;
			if ( ++(penum->y) == penum->height )
			  goto end;
			pos = 0;
		   }
		else
		  penum->x = x_end;
	   }
	penum->byte_in_row = pos;
	code = 0;
	goto out;
end:	/* End of data.  Render any left-over buffered data. */
	switch ( penum->posture )
	  {
	  case image_portrait:
	    {	fixed yc = dda_current(penum->next_y);
		penum->yci = fixed2int_rounded(yc - adjust);
		penum->hci = fixed2int_rounded(yc + adjust) - penum->yci;
	    }	break;
	  case image_landscape:
	    {	fixed xc = dda_current(penum->next_x);
		penum->xci = fixed2int_rounded(xc - adjust);
		penum->wci = fixed2int_rounded(xc + adjust) - penum->xci;
	    }	break;
	  case image_skewed:		/* pacify compilers */
	    ;
	  }
	code = (*penum->render)(penum, NULL, width * penum->spp, 0, dev);
	if ( code < 0 )
	  { penum->y--;
	    goto err;
	  }
	code = 1;
	goto out;
err:	/* Error or interrupt, restore original state. */
	penum->x = x;
	while ( penum->y > y )
	  { dda_previous(penum->next_x);
	    dda_previous(penum->next_y);
	    --(penum->y);
	  }
	/* Note that caller must call end_image */
	/* for both error and normal termination. */
out:	return code;
}

/* Clean up by releasing the buffers. */
/* Currently we ignore draw_last. */
int
gx_default_end_image(gx_device *dev, void *info, bool draw_last)
{	gx_image_enum *penum = info;
	gs_memory_t *mem = penum->memory;
	stream_IScale_state *scaler = penum->scaler;

	gs_free_object(mem, penum->rop_dev, "image RasterOp");
	gs_free_object(mem, penum->clip_dev, "image clipper");
	if ( scaler != 0 )
	  { (*s_IScale_template.release)((stream_state *)scaler);
	    gs_free_object(mem, scaler, "image scaler state");
	  }
	gs_free_object(mem, penum->line, "image line");
	gs_free_object(mem, penum->buffer, "image buffer");
	gs_free_object(mem, penum, "gx_default_end_image");
	return 0;
}

/* ------ Unpacking procedures ------ */

void
image_unpack_copy(byte *bptr, const byte *data, uint dsize,
  const sample_map *pmap, int spread, uint inpos)
{	register byte *bufp = bptr + inpos;
	if ( data != bufp )
	  memcpy(bufp, data, dsize);
}

void
image_unpack_1(byte *bptr, register const byte *data, uint dsize,
  const sample_map *pmap, int spread, uint inpos)
{	register bits32 *bufp = (bits32 *)(bptr + (inpos << 3));
	int left = dsize;
	register const bits32 *map = &pmap->table.lookup4x1to32[0];
	register uint b;
	if ( left & 1 )
	   {	b = data[0];
		bufp[0] = map[b >> 4];
		bufp[1] = map[b & 0xf];
		data++, bufp += 2;
	   }
	left >>= 1;
	while ( left-- )
	   {	b = data[0];
		bufp[0] = map[b >> 4];
		bufp[1] = map[b & 0xf];
		b = data[1];
		bufp[2] = map[b >> 4];
		bufp[3] = map[b & 0xf];
		data += 2, bufp += 4;
	   }
}

void
image_unpack_2(byte *bptr, register const byte *data, uint dsize,
  const sample_map *pmap, int spread, uint inpos)
{	register bits16 *bufp = (bits16 *)(bptr + (inpos << 2));
	int left = dsize;
	register const bits16 *map = &pmap->table.lookup2x2to16[0];
	while ( left-- )
	   {	register unsigned b = *data++;
		*bufp++ = map[b >> 4];
		*bufp++ = map[b & 0xf];
	   }
}

void
image_unpack_4(byte *bptr, register const byte *data, uint dsize,
  const sample_map *pmap, register int spread, uint inpos)
{	register byte *bufp = bptr + (inpos << 1) * spread;
	int left = dsize;
	register const byte *map = &pmap->table.lookup8[0];
	while ( left-- )
	   {	register unsigned b = *data++;
		*bufp = map[b >> 4]; bufp += spread;
		*bufp = map[b & 0xf]; bufp += spread;
	   }
}

void
image_unpack_8(byte *bptr, const byte *data, uint dsize,
  const sample_map *pmap, int spread, uint inpos)
{	register byte *bufp = bptr + inpos;
	if ( pmap->table.lookup8[0] != 0 || pmap->table.lookup8[255] != 255 )
	  {	register uint left = dsize;
		register const byte *map = &pmap->table.lookup8[0];
		while ( left-- )
		  *bufp++ = map[*data++];
	  }
	else if ( data != bufp )
	  memcpy(bufp, data, dsize);
}
