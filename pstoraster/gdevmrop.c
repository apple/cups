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

/* gdevmrop.c */
/* RasterOp / transparency / render algorithm implementation for */
/* memory devices */
#include "memory_.h"
#include "gx.h"
#include "gsbittab.h"
#include "gserrors.h"
#include "gsropt.h"
#include "gxdcolor.h"
#include "gxdevice.h"
#include "gxdevmem.h"
#include "gxdevrop.h"
#include "gdevmrop.h"

#define mdev ((gx_device_memory *)dev)

/**************** NOTE: ****************
 * The 2- and 4-bit cases don't handle transparency right.
 * The 8- and 24-bit cases haven't been tested.
 * The 16- and 32-bit cases aren't implemented.
 ***************** ****************/

/* Forward references */
private gs_rop3_t gs_transparent_rop(P3(gs_rop3_t rop, bool source_transparent,
  bool pattern_transparent));

#define chunk byte

/* Calculate the X offset for a given Y value, */
/* taking shift into account if necessary. */
#define x_offset(px, ty, textures)\
  ((textures)->shift == 0 ? (px) :\
   (px) + (ty) / (textures)->rep_height * (textures)->rep_shift)

/* ---------------- Initialization ---------------- */

void
gs_roplib_init(gs_memory_t *mem)
{	/* Replace the default and forwarding copy_rop procedures. */
	gx_default_copy_rop_proc = gx_real_default_copy_rop;
	gx_forward_copy_rop_proc = gx_forward_copy_rop;
	gx_default_strip_copy_rop_proc = gx_real_default_strip_copy_rop;
	gx_forward_strip_copy_rop_proc = gx_forward_strip_copy_rop;
}

/* ---------------- Debugging aids ---------------- */

#ifdef DEBUG

private void
trace_copy_rop(const char *cname, gx_device *dev,
  const byte *sdata, int sourcex, uint sraster, gx_bitmap_id id,
  const gx_color_index *scolors,
  const gx_strip_bitmap *textures, const gx_color_index *tcolors,
  int x, int y, int width, int height,
  int phase_x, int phase_y, gs_logical_operation_t lop)
{	dprintf4("%s: dev=0x%lx(%s) depth=%d\n",
		 cname, (ulong)dev, dev->dname, dev->color_info.depth);
	dprintf4("  source data=0x%lx x=%d raster=%u id=%lu colors=",
		 (ulong)sdata, sourcex, sraster, (ulong)id);
	if ( scolors )
	  dprintf2("(%lu,%lu);\n", scolors[0], scolors[1]);
	else
	  dputs("none;\n");
	if ( textures )
	  dprintf8("  textures=0x%lx size=%dx%d(%dx%d) raster=%u shift=%d(%d)",
		   (ulong)textures, textures->size.x, textures->size.y,
		   textures->rep_width, textures->rep_height, textures->raster,
		   textures->shift, textures->rep_shift);
	else
	  dputs("  textures=none");
	if ( tcolors )
	  dprintf2(" colors=(%lu,%lu)\n", tcolors[0], tcolors[1]);
	else
	  dputs(" colors=none\n");
	dprintf7("  rect=(%d,%d),(%d,%d) phase=(%d,%d) op=0x%x\n",
		 x, y, x + width, y + height, phase_x, phase_y,
		 (uint)lop);
	if ( gs_debug_c('B') )
	{ if ( sdata )
	    debug_dump_bitmap(sdata, sraster, height, "source bits");
	  if ( textures && textures->data )
	    debug_dump_bitmap(textures->data, textures->raster,
			      textures->size.y, "textures bits");
	}
}

#endif

/* ---------------- Monobit RasterOp ---------------- */

int
mem_mono_strip_copy_rop(gx_device *dev,
  const byte *sdata, int sourcex, uint sraster, gx_bitmap_id id,
  const gx_color_index *scolors,
  const gx_strip_bitmap *textures, const gx_color_index *tcolors,
  int x, int y, int width, int height,
  int phase_x, int phase_y, gs_logical_operation_t lop)
{	gs_rop3_t rop = (gs_rop3_t)(lop & lop_rop_mask);
	gx_strip_bitmap no_texture;
	bool invert;
	uint draster = mdev->raster;
	uint traster;
	int line_count;
	byte *drow;
	const byte *srow;
	int ty;

	/* If map_rgb_color isn't the default one for monobit memory */
	/* devices, palette might not be set; set it now if needed. */
	if ( mdev->palette.data == 0 )
	  gdev_mem_mono_set_inverted(mdev,
				     (*dev_proc(dev, map_rgb_color))
				       (dev, (gx_color_value)0,
				       (gx_color_value)0, (gx_color_value)0)
				      != 0);
	invert = mdev->palette.data[0] != 0;

#ifdef DEBUG
	if ( gs_debug_c('b') )
	  trace_copy_rop("mem_mono_strip_copy_rop",
			 dev, sdata, sourcex, sraster,
			 id, scolors, textures, tcolors,
			 x, y, width, height, phase_x, phase_y, lop);
	if ( gs_debug_c('B') )
	  debug_dump_bitmap(scan_line_base(mdev, y), mdev->raster,
			    height, "initial dest bits");
#endif

	/* Handle source and destination transparency. */
	rop = gs_transparent_rop(rop, lop & lop_S_transparent,
				 lop & lop_T_transparent);

	/*
	 * RasterOp is defined as operating in RGB space; in the monobit
	 * case, this means black = 0, white = 1.  However, most monobit
	 * devices use the opposite convention.  To make this work,
	 * we must precondition the Boolean operation by swapping the
	 * order of bits end-for-end and then inverting.
	 */

	if ( invert )
	  rop = (gs_rop3_t)(byte_reverse_bits[rop] ^ 0xff);

	/* Modify the raster operation according to the source palette. */
	if ( scolors != 0 )
	{	/* Source with palette. */
		switch ( (int)((scolors[1] << 1) + scolors[0]) )
		{
		case 0: rop = (gs_rop3_t)rop3_know_S_0(rop); break;
		case 1: rop = (gs_rop3_t)rop3_invert_S(rop); break;
		case 2: break;
		case 3: rop = (gs_rop3_t)rop3_know_S_1(rop); break;
		}
	}

	/* Modify the raster operation according to the texture palette. */
	if ( tcolors != 0 )
	{	/* Texture with palette. */
		switch ( (int)((tcolors[1] << 1) + tcolors[0]) )
		{
		case 0: rop = (gs_rop3_t)rop3_know_T_0(rop); break;
		case 1: rop = (gs_rop3_t)rop3_invert_T(rop); break;
		case 2: break;
		case 3: rop = (gs_rop3_t)rop3_know_T_1(rop); break;
		}
	}

	/* Handle constant source and/or texture. */
	if ( rop3_uses_S(rop) )
	{	fit_copy(dev, sdata, sourcex, sraster, id,
			 x, y, width, height);
	}
	else
	{	/* Source is not used; sdata et al may be garbage. */
		sdata = mdev->base;	/* arbitrary, as long as all */
					/* accesses are valid */
		sourcex = x;		/* guarantee no source skew */
		sraster = 0;
		fit_fill(dev, x, y, width, height);
	}
	if ( !rop3_uses_T(rop) )
	{	/* Texture is not used; texture may be garbage. */
		no_texture.data = mdev->base;	/* arbitrary */
		no_texture.raster = 0;
		no_texture.size.x = width;
		no_texture.size.y = height;
		no_texture.rep_width = no_texture.rep_height = 1;
		no_texture.rep_shift = no_texture.shift = 0;
		textures = &no_texture;
	}

#ifdef DEBUG
	if_debug1('b', "final rop=0x%x\n", rop);
#endif

	/* Set up transfer parameters. */
	line_count = height;
	srow = sdata;
	drow = scan_line_base(mdev, y);
	traster = textures->raster;
	ty = y + phase_y;

	/* Loop over scan lines. */
	for ( ; line_count-- > 0; drow += draster, srow += sraster, ++ty )
	{	/* Loop over copies of the tile. */
		int sx = sourcex;
		int dx = x;
		int w = width;
		const byte *trow =
		  textures->data + (ty % textures->size.y) * traster;
		int xoff = x_offset(phase_x, ty, textures);
		int nw;

		for ( ; w > 0; sx += nw, dx += nw, w -= nw )
		{	int dbit = dx & 7;
			int sbit = sx & 7;
			int sskew = sbit - dbit;
			int tx = (dx + xoff) % textures->rep_width;
			int tbit = tx & 7;
			int tskew = tbit - dbit;
			int left = nw = min(w, textures->size.x - tx);
			byte lmask = 0xff >> dbit;
			byte rmask = 0xff << (~(dbit + nw - 1) & 7);
			byte mask = lmask;
			int nx = 8 - dbit;
			byte *dptr = drow + (dx >> 3);
			const byte *sptr = srow + (sx >> 3);
			const byte *tptr = trow + (tx >> 3);

			if ( sskew < 0 )
			  --sptr, sskew += 8;
			if ( tskew < 0 )
			  --tptr, tskew += 8;
			for ( ; left > 0;
			      left -= nx, mask = 0xff, nx = 8,
			        ++dptr, ++sptr, ++tptr
			    )
			    {	byte dbyte = *dptr;
#define fetch1(ptr, skew)\
  (skew ? (ptr[0] << skew) + (ptr[1] >> (8 - skew)) : *ptr)
				byte sbyte = fetch1(sptr, sskew);
				byte tbyte = fetch1(tptr, tskew);
#undef fetch1
				byte result =
				  (*rop_proc_tab[rop])(dbyte, sbyte, tbyte);
				if ( left <= nx )
				  mask &= rmask;
				*dptr = (mask == 0xff ? result :
					 (result & mask) | (dbyte & ~mask));
			 }
		}
	}
#ifdef DEBUG
	if ( gs_debug_c('B') )
	  debug_dump_bitmap(scan_line_base(mdev, y), mdev->raster,
			    height, "final dest bits");
#endif
	return 0;
}

/* ---------------- Fake RasterOp for 2- and 4-bit devices ---------------- */

int
mem_gray_strip_copy_rop(gx_device *dev,
  const byte *sdata, int sourcex, uint sraster, gx_bitmap_id id,
  const gx_color_index *scolors,
  const gx_strip_bitmap *textures, const gx_color_index *tcolors,
  int x, int y, int width, int height,
  int phase_x, int phase_y, gs_logical_operation_t lop)
{	gx_color_index scolors2[2];
	const gx_color_index *real_scolors = scolors;
	gx_color_index tcolors2[2];
	const gx_color_index *real_tcolors = tcolors;
	gx_strip_bitmap texture2;
	const gx_strip_bitmap *real_texture = textures;
	long tdata;
	int depth = dev->color_info.depth;
	int log2_depth = depth >> 1;		/* works for 2, 4 */
	gx_color_index max_pixel = (1 << depth) - 1;
	int code;

#ifdef DEBUG
	if ( gs_debug_c('b') )
	  trace_copy_rop("mem_gray_strip_copy_rop",
			 dev, sdata, sourcex, sraster,
			 id, scolors, textures, tcolors,
			 x, y, width, height, phase_x, phase_y, lop);
#endif
	if ( scolors )
	{	/* We can't handle "real" source colors. */
		if ( (scolors[0] | scolors[1]) & ~max_pixel )
		  return_error(gs_error_rangecheck);
		scolors2[0] = scolors[0] & 1;
		scolors2[1] = scolors[1] & 1;
		real_scolors = scolors2;
	}
	if ( textures )
	{	texture2 = *textures;
		texture2.size.x <<= log2_depth;
		texture2.rep_width <<= log2_depth;
		texture2.shift <<= log2_depth;
		texture2.rep_shift <<= log2_depth;
		real_texture = &texture2;
	}
	if ( tcolors )
	{	/* We can't handle monobit textures. */
		if ( tcolors[0] != tcolors[1] )
		  return_error(gs_error_rangecheck);
		/* For polybit textures with colors other than */
		/* all 0s or all 1s, fabricate the data. */
		if ( tcolors[0] != 0 && tcolors[0] != max_pixel )
		{	real_tcolors = 0;
			*(byte *)&tdata = (byte)tcolors[0] << (8 - depth);
			texture2.data = (byte *)&tdata;
			texture2.raster = align_bitmap_mod;
			texture2.size.x = texture2.rep_width = depth;
			texture2.size.y = texture2.rep_height = 1;
			texture2.id = gx_no_bitmap_id;
			texture2.shift = texture2.rep_shift = 0;
			real_texture = &texture2;
		}
		else
		{	tcolors2[0] = tcolors2[1] = tcolors[0] & 1;
			real_tcolors = tcolors2;
		}
	}
	dev->width <<= log2_depth;
	code = mem_mono_strip_copy_rop(dev, sdata,
		(real_scolors == NULL ? sourcex << log2_depth : sourcex),
		sraster, id, real_scolors, real_texture, real_tcolors,
		x << log2_depth, y, width << log2_depth, height,
		phase_x << log2_depth, phase_y, lop);
	dev->width >>= log2_depth;
	return code;
}

/* ---------------- RasterOp with 8-bit gray / 24-bit RGB ---------------- */

int
mem_gray8_rgb24_strip_copy_rop(gx_device *dev,
  const byte *sdata, int sourcex, uint sraster, gx_bitmap_id id,
  const gx_color_index *scolors,
  const gx_strip_bitmap *textures, const gx_color_index *tcolors,
  int x, int y, int width, int height,
  int phase_x, int phase_y, gs_logical_operation_t lop)
{	gs_rop3_t rop = (gs_rop3_t)(lop & lop_rop_mask);
	gx_color_index const_source = gx_no_color_index;
	gx_color_index const_texture = gx_no_color_index;
	uint draster = mdev->raster;
	int line_count;
	byte *drow;
	int depth = dev->color_info.depth;
	int bpp = depth >> 3;		/* bytes per pixel, 1 or 3 */
	gx_color_index all_ones = ((gx_color_index)1 << depth) - 1;
	gx_color_index strans =
	  (lop & lop_S_transparent ? all_ones : gx_no_color_index);
	gx_color_index ttrans =
	  (lop & lop_T_transparent ? all_ones : gx_no_color_index);

	/* Check for constant source. */
	if ( scolors != 0 && scolors[0] == scolors[1] )
	{	/* Constant source */
		const_source = scolors[0];
		if ( const_source == 0 )
		  rop = (gs_rop3_t)rop3_know_S_0(rop);
		else if ( const_source == all_ones )
		  rop = (gs_rop3_t)rop3_know_S_1(rop);
	}
	else if ( !rop3_uses_S(rop) )
	  const_source = 0;		/* arbitrary */

	/* Check for constant texture. */
	if ( tcolors != 0 && tcolors[0] == tcolors[1] )
	{	/* Constant texture */
		const_texture = tcolors[0];
		if ( const_texture == 0 )
		  rop = (gs_rop3_t)rop3_know_T_0(rop);
		else if ( const_texture == all_ones )
		  rop = (gs_rop3_t)rop3_know_T_1(rop);
	}
	else if ( !rop3_uses_T(rop) )
	  const_texture = 0;		/* arbitrary */

	/* Adjust coordinates to be in bounds. */
	if ( const_source == gx_no_color_index )
	{	fit_copy(dev, sdata, sourcex, sraster, id,
			 x, y, width, height);
	}
	else
	{	fit_fill(dev, x, y, width, height);
	}

	/* Set up transfer parameters. */
	line_count = height;
	drow = scan_line_base(mdev, y) + x * bpp;

	/*
	 * There are 18 cases depending on whether each of the source and
	 * texture is constant, 1-bit, or multi-bit, and on whether the
	 * depth is 8 or 24 bits.  We divide first according to constant
	 * vs. non-constant, and then according to 1- vs. multi-bit, and
	 * finally according to pixel depth.  This minimizes source code,
	 * but not necessarily time, since we do some of the divisions
	 * within 1 or 2 levels of loop.
	 */

#define dbit(base, i) ((base)[(i) >> 3] & (0x80 >> ((i) & 7)))
/* 8-bit */
#define cbit8(base, i, colors)\
  (dbit(base, i) ? (byte)colors[1] : (byte)colors[0])
#define rop_body_8()\
  if ( s_pixel == strans ||	/* So = 0, s_tr = 1 */\
       t_pixel == ttrans	/* Po = 0, p_tr = 1 */\
     )\
    continue;\
  *dptr = (*rop_proc_tab[rop])(*dptr, s_pixel, t_pixel)
/* 24-bit */
#define get24(ptr)\
  (((gx_color_index)(ptr)[0] << 16) | ((gx_color_index)(ptr)[1] << 8) | (ptr)[2])
#define put24(ptr, pixel)\
  (ptr)[0] = (byte)((pixel) >> 16),\
  (ptr)[1] = (byte)((uint)(pixel) >> 8),\
  (ptr)[2] = (byte)(pixel)
#define cbit24(base, i, colors)\
  (dbit(base, i) ? colors[1] : colors[0])
#define rop_body_24()\
  if ( s_pixel == strans ||	/* So = 0, s_tr = 1 */\
       t_pixel == ttrans	/* Po = 0, p_tr = 1 */\
     )\
    continue;\
  { gx_color_index d_pixel = get24(dptr);\
    d_pixel = (*rop_proc_tab[rop])(d_pixel, s_pixel, t_pixel);\
    put24(dptr, d_pixel);\
  }

	if ( const_texture != gx_no_color_index )	/**** Constant texture ****/
	  {
	    if ( const_source != gx_no_color_index )	/**** Constant source & texture ****/
	      {
		for ( ; line_count-- > 0; drow += draster )
		  { byte *dptr = drow;
		    int left = width;
		    if ( bpp == 1 )	/**** 8-bit destination ****/
#define s_pixel (byte)const_source
#define t_pixel (byte)const_texture
		      for ( ; left > 0; ++dptr, --left )
			{ rop_body_8();
			}
#undef s_pixel
#undef t_pixel
		    else		/**** 24-bit destination ****/
#define s_pixel const_source
#define t_pixel const_texture
		      for ( ; left > 0; dptr += 3, --left )
			{ rop_body_24();
			}
#undef s_pixel
#undef t_pixel
		  }
	      }
	    else			/**** Data source, const texture ****/
	      { const byte *srow = sdata;
		for ( ; line_count-- > 0; drow += draster, srow += sraster )
		  { byte *dptr = drow;
		    int left = width;
		    if ( scolors )	/**** 1-bit source ****/
		      {	int sx = sourcex;
			if ( bpp == 1 )	/**** 8-bit destination ****/
#define t_pixel (byte)const_texture
			  for ( ; left > 0; ++dptr, ++sx, --left )
			    { byte s_pixel = cbit8(srow, sx, scolors);
			      rop_body_8();
			    }
#undef t_pixel
			else		/**** 24-bit destination ****/
#define t_pixel const_texture
			  for ( ; left > 0; dptr += 3, ++sx, --left )
			    { bits32 s_pixel = cbit24(srow, sx, scolors);
			      rop_body_24();
			    }
#undef t_pixel
		      }
		    else if ( bpp == 1)	/**** 8-bit source & dest ****/
		      {	const byte *sptr = srow + sourcex;
#define t_pixel (byte)const_texture
			for ( ; left > 0; ++dptr, ++sptr, --left )
			  { byte s_pixel = *sptr;
			    rop_body_8();
			  }
#undef t_pixel
		      }
		    else		/**** 24-bit source & dest ****/
		      {	const byte *sptr = srow + sourcex * 3;
#define t_pixel const_texture
			for ( ; left > 0; dptr += 3, sptr += 3, --left )
			  { bits32 s_pixel = get24(sptr);
			    rop_body_24();
			  }
#undef t_pixel
		      }
		  }
	      }
	  }
	else if ( const_source != gx_no_color_index )	/**** Const source, data texture ****/
	  {	uint traster = textures->raster;
		int ty = y + phase_y;

		for ( ; line_count-- > 0; drow += draster, ++ty )
		  { /* Loop over copies of the tile. */
		    int dx = x, w = width, nw;
		    byte *dptr = drow;
		    const byte *trow =
		      textures->data + (ty % textures->size.y) * traster;
		    int xoff = x_offset(phase_x, ty, textures);

		    for ( ; w > 0; dx += nw, w -= nw )
		      { int tx = (dx + xoff) % textures->rep_width;
			int left = nw = min(w, textures->size.x - tx);
			const byte *tptr = trow;

			if ( tcolors )	/**** 1-bit texture ****/
			  { if ( bpp == 1 )	/**** 8-bit dest ****/
#define s_pixel (byte)const_source
			      for ( ; left > 0; ++dptr, ++tx, --left )
				{ byte t_pixel = cbit8(tptr, tx, tcolors);
				  rop_body_8();
				}
#undef s_pixel
			    else		/**** 24-bit dest ****/
#define s_pixel const_source
			      for ( ; left > 0; dptr += 3, ++tx, --left )
				{ bits32 t_pixel = cbit24(tptr, tx, tcolors);
				  rop_body_24();
				}
#undef s_pixel
			  }
			else if ( bpp == 1 )	/**** 8-bit T & D ****/
			  { tptr += tx;
#define s_pixel (byte)const_source
			    for ( ; left > 0; ++dptr, ++tptr, --left )
			      {	byte t_pixel = *tptr;
				rop_body_8();
			      }
#undef s_pixel
			  }
			else			/**** 24-bit T & D ****/
			  { tptr += tx * 3;
#define s_pixel const_source
			    for ( ; left > 0; dptr += 3, tptr += 3, --left )
			      {	bits32 t_pixel = get24(tptr);
				rop_body_24();
			      }
#undef s_pixel
			  }
		      }
		  }
	  }
	else				/**** Data source & texture ****/
	  {
	uint traster = textures->raster;
	int ty = y + phase_y;
	const byte *srow = sdata;

	/* Loop over scan lines. */
	for ( ; line_count-- > 0; drow += draster, srow += sraster, ++ty )
	{	/* Loop over copies of the tile. */
		int sx = sourcex;
		int dx = x;
		int w = width;
		int nw;
		byte *dptr = drow;
		const byte *trow =
		  textures->data + (ty % textures->size.y) * traster;
		int xoff = x_offset(phase_x, ty, textures);

		for ( ; w > 0; dx += nw, w -= nw )
		{	/* Loop over individual pixels. */
			int tx = (dx + xoff) % textures->rep_width;
			int left = nw = min(w, textures->size.x - tx);
			const byte *tptr = trow;

			/*
			 * For maximum speed, we should split this loop
			 * into 7 cases depending on source & texture
			 * depth: (1,1), (1,8), (1,24), (8,1), (8,8),
			 * (24,1), (24,24).  But since we expect these
			 * cases to be relatively uncommon, we just
			 * divide on the destination depth.
			 */
			if ( bpp == 1 )		/**** 8-bit destination ****/
			  { const byte *sptr = srow + sx;
			    tptr += tx;
			    for ( ; left > 0; ++dptr, ++sptr, ++tptr, ++sx, ++tx, --left )
			      { byte s_pixel =
				 (scolors ? cbit8(srow, sx, scolors) : *sptr);
				byte t_pixel =
				 (tcolors ? cbit8(tptr, tx, tcolors) : *tptr);
				rop_body_8();
			      }
			  }
			else			/**** 24-bit destination ****/
			  { const byte *sptr = srow + sx * 3;
			    tptr += tx * 3;
			    for ( ; left > 0; dptr += 3, sptr += 3, tptr += 3, ++sx, ++tx, --left )
			      { bits32 s_pixel =
				  (scolors ? cbit24(srow, sx, scolors) :
				   get24(sptr));
				bits32 t_pixel =
				  (tcolors ? cbit24(tptr, tx, tcolors) :
				   get24(tptr));
				rop_body_24();
			      }
			  }
		}
	}
	}
#undef rop_body_8
#undef rop_body_24
#undef dbit
#undef cbit8
#undef cbit24
	return 0;
}

/* ---------------- Default copy_rop implementations ---------------- */

#undef mdev

int
gx_real_default_copy_rop(gx_device *dev,
  const byte *sdata, int sourcex, uint sraster, gx_bitmap_id id,
  const gx_color_index *scolors,
  const gx_tile_bitmap *texture, const gx_color_index *tcolors,
  int x, int y, int width, int height,
  int phase_x, int phase_y, gs_logical_operation_t lop)
{	const gx_strip_bitmap *textures;
	gx_strip_bitmap tiles;

	if ( texture == 0 )
	  textures = 0;
	else
	  { *(gx_tile_bitmap *)&tiles = *texture;
	    tiles.rep_shift = tiles.shift = 0;
	    textures = &tiles;
	  }
	return (*dev_proc(dev, strip_copy_rop))
	  (dev, sdata, sourcex, sraster, id, scolors, textures, tcolors,
	   x, y, width, height, phase_x, phase_y, lop);
}

int
gx_real_default_strip_copy_rop(gx_device *dev,
  const byte *sdata, int sourcex, uint sraster, gx_bitmap_id id,
  const gx_color_index *scolors,
  const gx_strip_bitmap *textures, const gx_color_index *tcolors,
  int x, int y, int width, int height,
  int phase_x, int phase_y, gs_logical_operation_t lop)
{	/*
	 * The default implementation uses get_bits to read out the
	 * pixels, the memory device implementation to do the operation,
	 * and copy_color to write the pixels back.
	 */
	gs_rop3_t rop = (gs_rop3_t)(lop & lop_rop_mask);
	int depth = dev->color_info.depth;
	const gx_device_memory *mdproto = gdev_mem_device_for_bits(depth);
	gx_device_memory mdev;
	uint draster = gx_device_raster(dev, true);
	bool uses_d = rop3_uses_D(rop);				  
	byte *row;
	int code;
	int py;

#ifdef DEBUG
	if ( gs_debug_c('b') )
	  trace_copy_rop("gx_default_strip_copy_rop",
			 dev, sdata, sourcex, sraster,
			 id, scolors, textures, tcolors,
			 x, y, width, height, phase_x, phase_y, lop);
#endif
	if ( mdproto == 0 )
	  return_error(gs_error_rangecheck);
	gs_make_mem_device(&mdev, mdproto, 0, -1, dev);
	mdev.width = width;
	mdev.height = 1;
	mdev.bitmap_memory = &gs_memory_default;
	code = (*dev_proc(&mdev, open_device))((gx_device *)&mdev);
	if ( code < 0 )
	  return code;
	row = gs_malloc(1, draster, "copy_rop buffer");
	if ( row == 0 )
	  { (*dev_proc(&mdev, close_device))((gx_device *)&mdev);
	    return_error(gs_error_VMerror);
	  }
	for ( py = y; py < y + height; ++py )
	  { byte *data;

	    if ( uses_d )
	      { code = (*dev_proc(dev, get_bits))(dev, py, row, &data);
		if ( code < 0 )
		  break;
		code = (*dev_proc(&mdev, copy_color))((gx_device *)&mdev,
			data, x, draster, gx_no_bitmap_id,
			0, 0, width, 1);
		if ( code < 0 )
		  return code;
	      }
	    code = (*dev_proc(&mdev, strip_copy_rop))((gx_device *)&mdev,
			sdata + (py - y) * sraster, sourcex, sraster,
			gx_no_bitmap_id, scolors, textures, tcolors,
			0, 0, width, 1, phase_x + x, phase_y + py,
			lop);
	    if ( code < 0 )
	      break;
	    code = (*dev_proc(&mdev, get_bits))((gx_device *)&mdev, 0, row, &data);
	    if ( code < 0 )
	      break;
	    code = (*dev_proc(dev, copy_color))(dev,
			data, 0, draster, gx_no_bitmap_id,
			x, py, width, 1);
	    if ( code < 0 )
	      break;
	  }
	gs_free(row, 1, draster, "copy_rop buffer");
	(*dev_proc(&mdev, close_device))((gx_device *)&mdev);
	return code;
}

int
gx_forward_copy_rop(gx_device *dev,
  const byte *sdata, int sourcex, uint sraster, gx_bitmap_id id,
  const gx_color_index *scolors,
  const gx_tile_bitmap *texture, const gx_color_index *tcolors,
  int x, int y, int width, int height,
  int phase_x, int phase_y, gs_logical_operation_t lop)
{	gx_device *tdev = ((gx_device_forward *)dev)->target;
	dev_proc_copy_rop((*proc));

	if ( tdev == 0 )
	  tdev = dev, proc = gx_default_copy_rop;
	else
	  proc = dev_proc(tdev, copy_rop);
	return (*proc)(tdev, sdata, sourcex, sraster, id, scolors,
		       texture, tcolors, x, y, width, height,
		       phase_x, phase_y, lop);
}

int
gx_forward_strip_copy_rop(gx_device *dev,
  const byte *sdata, int sourcex, uint sraster, gx_bitmap_id id,
  const gx_color_index *scolors,
  const gx_strip_bitmap *textures, const gx_color_index *tcolors,
  int x, int y, int width, int height,
  int phase_x, int phase_y, gs_logical_operation_t lop)
{	gx_device *tdev = ((gx_device_forward *)dev)->target;
	dev_proc_strip_copy_rop((*proc));

	if ( tdev == 0 )
	  tdev = dev, proc = gx_default_strip_copy_rop;
	else
	  proc = dev_proc(tdev, strip_copy_rop);
	return (*proc)(tdev, sdata, sourcex, sraster, id, scolors,
		       textures, tcolors, x, y, width, height,
		       phase_x, phase_y, lop);
}

int
gx_copy_rop_unaligned(gx_device *dev,
  const byte *sdata, int sourcex, uint sraster, gx_bitmap_id id,
  const gx_color_index *scolors,
  const gx_tile_bitmap *texture, const gx_color_index *tcolors,
  int x, int y, int width, int height,
  int phase_x, int phase_y, gs_logical_operation_t lop)
{	const gx_strip_bitmap *textures;
	gx_strip_bitmap tiles;

	if ( texture == 0 )
	  textures = 0;
	else
	  { *(gx_tile_bitmap *)&tiles = *texture;
	    tiles.rep_shift = tiles.shift = 0;
	    textures = &tiles;
	  }
	return gx_strip_copy_rop_unaligned
	  (dev, sdata, sourcex, sraster, id, scolors, textures, tcolors,
	   x, y, width, height, phase_x, phase_y, lop);
}

int
gx_strip_copy_rop_unaligned(gx_device *dev,
  const byte *sdata, int sourcex, uint sraster, gx_bitmap_id id,
  const gx_color_index *scolors,
  const gx_strip_bitmap *textures, const gx_color_index *tcolors,
  int x, int y, int width, int height,
  int phase_x, int phase_y, gs_logical_operation_t lop)
{	dev_proc_strip_copy_rop((*copy_rop)) = dev_proc(dev, strip_copy_rop);
	int depth = (scolors == 0 ? dev->color_info.depth : 1);
	int step = sraster & (align_bitmap_mod - 1);

	/* Adjust the origin. */
	if ( sdata != 0 )
	  { uint offset =
	      (uint)(sdata - (const byte *)0) & (align_bitmap_mod - 1);
	    /* See copy_color above re the following statement. */
	    if ( depth == 24 )
	      offset += (offset % 3) *
		(align_bitmap_mod * (3 - (align_bitmap_mod % 3)));
	    sdata -= offset;
	    sourcex += (offset << 3) / depth;
	  }

	/* Adjust the raster. */
	if ( !step || sdata == 0 ||
	     (scolors != 0 && scolors[0] == scolors[1])
	   )
	  { /* No adjustment needed. */
	    return (*copy_rop)(dev, sdata, sourcex, sraster, id, scolors,
			       textures, tcolors, x, y, width, height,
			       phase_x, phase_y, lop);
	  }

	/* Do the transfer one scan line at a time. */
	{ const byte *p = sdata;
	  int d = sourcex;
	  int dstep = (step << 3) / depth;
	  int code = 0;
	  int i;

	  for ( i = 0; i < height && code >= 0;
	        ++i, p += sraster - step, d += dstep
	      )
	    code = (*copy_rop)(dev, p, d, sraster, gx_no_bitmap_id, scolors,
			       textures, tcolors, x, y + i, width, 1,
			       phase_x, phase_y, lop);
	  return code;
	}
}

/* ---------------- RasterOp texture device ---------------- */

public_st_device_rop_texture();

/* Device for clipping with a region. */
private dev_proc_fill_rectangle(rop_texture_fill_rectangle);
private dev_proc_copy_mono(rop_texture_copy_mono);
private dev_proc_copy_color(rop_texture_copy_color);

/* The device descriptor. */
private const gx_device_rop_texture far_data gs_rop_texture_device =
{	std_device_std_body(gx_device_rop_texture, 0, "rop source",
	  0, 0, 1, 1),
	{	gx_default_open_device,
		NULL,		/* get_initial_matrix */
		gx_default_sync_output,
		gx_default_output_page,
		gx_default_close_device,
		NULL,		/* map_rgb_color */
		NULL,		/* map_color_rgb */
		rop_texture_fill_rectangle,
		gx_default_tile_rectangle,
		rop_texture_copy_mono,
		rop_texture_copy_color,
		gx_default_draw_line,
		NULL,		/* get_bits */
		NULL,		/* get_params */
		NULL,		/* put_params */
		NULL,		/* map_cmyk_color */
		NULL,		/* get_xfont_procs */
		NULL,		/* get_xfont_device */
		NULL,		/* map_rgb_alpha_color */
		NULL,		/* get_page_device */
		gx_default_get_alpha_bits,	/* (no alpha) */
		gx_no_copy_alpha,		/* shouldn't be called */
		NULL,		/* get_band */
		gx_no_copy_rop			/* shouldn't be called */
	},
	0,			/* target */
	lop_default,		/* log_op */
	NULL			/* texture */
};
#define rtdev ((gx_device_rop_texture *)dev)

/* Initialize a RasterOp source device. */
void
gx_make_rop_texture_device(gx_device_rop_texture *dev, gx_device *target,
  gs_logical_operation_t log_op, const gx_device_color *texture)
{	*dev = gs_rop_texture_device;
	gx_device_forward_fill_in_procs((gx_device_forward *)dev);
	dev->color_info = target->color_info;
	dev->target = target;
	dev->log_op = log_op;
	dev->texture = texture;
}

/* Fill a rectangle */
private int
rop_texture_fill_rectangle(gx_device *dev, int x, int y, int w, int h,
  gx_color_index color)
{	gx_rop_source_t source;

	source.sdata = NULL;
	source.sourcex = 0;
	source.sraster = 0;
	source.id = gx_no_bitmap_id;
	source.scolors[0] = source.scolors[1] = color;
	return gx_device_color_fill_rectangle(rtdev->texture,
				x, y, w, h, rtdev->target,
				rtdev->log_op, &source);
}

/* Copy a monochrome rectangle */
private int
rop_texture_copy_mono(gx_device *dev,
  const byte *data, int sourcex, int raster, gx_bitmap_id id,
  int x, int y, int w, int h,
  gx_color_index color0, gx_color_index color1)
{	gx_rop_source_t source;
	gs_logical_operation_t lop = rtdev->log_op;

	source.sdata = data;
	source.sourcex = sourcex;
	source.sraster = raster;
	source.id = id;
	source.scolors[0] = color0;
	source.scolors[1] = color1;
	/* Adjust the logical operation per transparent colors. */
	if ( color0 == gx_no_color_index )
	  lop = rop3_use_D_when_S_0(lop);
	else if ( color1 == gx_no_color_index )
	  lop = rop3_use_D_when_S_1(lop);
	return gx_device_color_fill_rectangle(rtdev->texture,
				x, y, w, h, rtdev->target,
				lop, &source);
}

/* Copy a color rectangle */
private int
rop_texture_copy_color(gx_device *dev,
  const byte *data, int sourcex, int raster, gx_bitmap_id id,
  int x, int y, int w, int h)
{	gx_rop_source_t source;

	source.sdata = data;
	source.sourcex = sourcex;
	source.sraster = raster;
	source.id = id;
	source.scolors[0] = source.scolors[1] = gx_no_color_index;
	return gx_device_color_fill_rectangle(rtdev->texture,
				x, y, w, h, rtdev->target,
				rtdev->log_op, &source);
}

/* ---------------- Internal routines ---------------- */

/* Compute the effective RasterOp for the 1-bit case, */
/* taking transparency into account. */
private gs_rop3_t
gs_transparent_rop(gs_rop3_t rop, bool source_transparent,
  bool pattern_transparent)
{	/*
	 * The algorithm for computing an effective RasterOp is presented,
	 * albeit obfuscated, in the H-P PCL5 technical documentation.
	 * One applies the original RasterOp to compute an intermediate
	 * result R, and then computes the final result as
	 * (R & M) | (D & ~M) where M depends on transparencies as follows:
	 *	s_tr	p_tr	M
	 *	 0	 0	1
	 *	 0	 1	~So | Po (? Po ?)
	 *	 1	 0	So
	 *	 1	 1	So & Po
	 * or equivalently
	 *	So	Po	M
	 *	 0	 0	~s_tr (? ~s_tr & ~p_tr ?)
	 *	 0	 1	~s_tr
	 *	 1	 0	~p_tr
	 *	 1	 1	1
	 * The s_tr = 0, p_tr = 1 case seems wrong, but it's clearly
	 * specified that way in the "PCL 5 Color Technical Reference
	 * Manual."  So and Po are "source opaque" and "pattern opaque";
	 * in the uninverted 1-bit case with black = 0, these are
	 * equivalent to ~S and ~P.
	 */
#define So rop3_not(rop3_S)
#define Po rop3_not(rop3_T)
	gs_rop3_t mask =
	  (gs_rop3_t)(source_transparent ?
		      (pattern_transparent ? So & Po : So) :
		      (pattern_transparent ? ~So | Po : rop3_1));
	return (gs_rop3_t)((rop & mask) | (rop3_D & ~mask));
}
