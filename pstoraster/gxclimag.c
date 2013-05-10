/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gxclimag.c */
/* Higher-level image operations for band lists */
#include "math_.h"
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gscspace.h"
#include "gxarith.h"
#include "gxdevice.h"
#include "gxdevmem.h"			/* must precede gxcldev.h */
#include "gxcldev.h"
#include "gxclpath.h"

/* Define whether we can handle high-level images yet. */
/*#define USE_HL_IMAGES*/

#define cdev cwdev

extern int igcd(P2(int, int));

/* Driver procedures */
private dev_proc_fill_mask(clist_fill_mask);
private dev_proc_begin_image(clist_begin_image);
private dev_proc_image_data(clist_image_data);
private dev_proc_end_image(clist_end_image);

/* Initialize the extensions to the command set. */
void
gs_climag_init(gs_memory_t *mem)
{
	gs_clist_device_procs.fill_mask = clist_fill_mask;
	gs_clist_device_procs.begin_image = clist_begin_image;
	gs_clist_device_procs.image_data = clist_image_data;
	gs_clist_device_procs.end_image = clist_end_image;
}

/* ------ Driver procedures ------ */

private int
clist_fill_mask(gx_device *dev,
  const byte *data, int data_x, int raster, gx_bitmap_id id,
  int x, int y, int width, int height,
  const gx_drawing_color *pdcolor, int depth,
  gs_logical_operation_t lop, const gx_clip_path *pcpath)
{	int log2_depth = depth >> 1;	/* works for 1,2,4 */
	int y0;
	int data_x_bit;
	gx_bitmap_id orig_id = id;
	byte copy_op =
	  (depth > 1 ? cmd_op_copy_color_alpha :
	   gx_dc_is_pure(pdcolor) ? cmd_op_copy_mono :
	   cmd_op_copy_mono + cmd_copy_ht_color);

	fit_copy(dev, data, data_x, raster, id, x, y, width, height);
	if ( cmd_check_clip_path(cdev, pcpath) )
	  cmd_clear_known(cdev, clip_path_known);
	y0 = y;
	data_x_bit = data_x << log2_depth;
	BEGIN_RECT
	int dx = (data_x_bit & 7) >> log2_depth;
	int w1 = dx + width;
	const byte *row = data + (y - y0) * raster + (data_x_bit >> 3);
	int code;

	if ( lop == lop_default )
	  { cmd_disable_lop(cdev, pcls);
	  }
	else
	  { if ( lop != pcls->lop )
	      { code = cmd_set_lop(cdev, pcls, lop);
		if ( code < 0 )
		  return code;
	      }
	    cmd_enable_lop(cdev, pcls);
	  }
	if ( depth > 1 && !pcls->color_is_alpha )
	  { byte *dp;
	    set_cmd_put_op(dp, cdev, pcls, cmd_opv_set_copy_alpha, 1);
	    pcls->color_is_alpha = 1;
	  }
	cmd_do_write_unknown(cdev, pcls, clip_path_known);
	cmd_do_enable_clip(cdev, pcls, pcpath != NULL);
	code = cmd_put_drawing_color(cdev, pcls, pdcolor);
	if ( code < 0 )
	  return code;
	/*
	 * Unfortunately, painting a character with a halftone requires the
	 * use of two bitmaps, a situation that we can neither represent in
	 * the band list nor guarantee will both be present in the tile
	 * cache; in this case, we always write the bits of the character.
	 */
	if ( id != gx_no_bitmap_id && gx_dc_is_pure(pdcolor) )
	  {	/* This is a character.  ****** WRONG IF HALFTONE CELL. ******/
		/* Put it in the cache if possible. */
		ulong offset_temp;
		if ( !cls_has_tile_id(cdev, pcls, id, offset_temp) )
		  { gx_strip_bitmap tile;
		    tile.data = (byte *)data;		/* actually const */
		    tile.raster = raster;
		    tile.size.x = tile.rep_width = width;
		    tile.size.y = tile.rep_height = yend - y0;	/* full height */
		    tile.rep_shift = tile.shift = 0;
		    tile.id = id;
		    if ( clist_change_bits(cdev, pcls, &tile, depth) < 0 )
		      { /* Something went wrong; just copy the bits. */
			goto copy;
		      }
		  }
		{ gx_cmd_rect rect;
		  int rsize;
		  byte op = copy_op + cmd_copy_use_tile;
		  byte *dp;

		  /* Output a command to copy the entire character. */
		  /* It will be truncated properly per band. */
		  rect.x = x, rect.y = y0;
		  rect.width = w1, rect.height = yend - y0;
		  rsize = 1 + cmd_sizexy(rect);
		  if ( dx )
		    { set_cmd_put_op(dp, cdev, pcls, cmd_opv_next_data_x, 2);
		      dp[1] = dx;
		    }
		  set_cmd_put_op(dp, cdev, pcls, op, rsize);
		  dp++;
		  cmd_putxy(rect, dp);
		  pcls->rect = rect;
		  goto end;
		}
	  }
copy:	{	gx_cmd_rect rect;
		int rsize;
		byte *dp;
		uint csize;
		int code;

		rect.x = x, rect.y = y;
		rect.width = w1, rect.height = height;
		rsize = (dx ? 3 : 1) + (depth > 1 ? 1 : 0 ) +
		  cmd_size_rect(&rect);
		code = cmd_put_bits(cdev, pcls, row, w1 << log2_depth,
				    height, raster, rsize,
				    (orig_id == gx_no_bitmap_id || depth > 1 ?
				     1 << cmd_compress_rle :
				     cmd_mask_compress_any),
				    &dp, &csize);
		if ( code < 0 )
		  { if ( code != gs_error_limitcheck )
		      return code;
		    /* The bitmap was too large; split up the transfer. */
		    if ( height > 1 )
		      {	/* Split the transfer by reducing the */
			/* height. See the comment above BEGIN_RECT. */
			height >>= 1;
			goto copy;
		      }
		    {	/* Split a single (very long) row. */
			int w2 = w1 >> 1;
			code = clist_fill_mask(dev, row, dx + w2,
				raster, gx_no_bitmap_id, x + w2, y,
				w1 - w2, 1, pdcolor, depth, lop, pcpath);
			if ( code < 0 )
			  return code;
			w1 = w2;
			goto copy;
		    }
		  }
		if ( dx )
		  { *dp++ = cmd_count_op(cmd_opv_next_data_x, 2);
		    *dp++ = dx;
		  }
		*dp++ = cmd_count_op(copy_op + code, csize);
		if ( depth > 1 )
		  *dp++ = depth;
		cmd_put2w(x, y, dp);
		cmd_put2w(w1, height, dp);
		pcls->rect = rect;
	   }
end:	;
	END_RECT
	return 0;
}

/* ------ Bitmap image driver procedures ------ */

/* Define the structure for keeping track of progress through an image. */
typedef struct clist_image_enum_s {
		/* Arguments of begin_image */
	gs_memory_t *memory;
	gs_image_t image;
	gx_drawing_color dcolor;
	const gs_imager_state *pis;
	const gx_clip_path *pcpath;
		/* Set at creation time */
	void *default_info;
	bool multi;
	int num_planes;
	int bits_per_pixel;	/* bits per pixel (per plane) */
	uint bytes_mod;		/* min # of bytes (per plane) for integral */
				/* number of pixels */
	gs_matrix matrix;	/* image space -> device space */
	byte color_space;
	int ymin, ymax;
		/* Updated dynamically */
	int xywh[4];		/* most recent image_data parameters */
} clist_image_enum;
/* We can disregard the pointers in the writer by allocating */
/* the image enumerator as immovable.  This is a hack, of course. */
gs_private_st_ptrs1(st_clist_image_enum, clist_image_enum, "clist_image_enum",
  clist_image_enum_enum_ptrs, clist_image_enum_reloc_ptrs, default_info);

/* Forward declarations */
private int cmd_image_data(P10(gx_device_clist_writer *cldev,
			       gx_clist_state *pcls, clist_image_enum *pie,
			       const byte *data, uint raster, uint nbytes,
			       int x, int y, int w, int h));

/* Start processing an image. */
private int
clist_begin_image(gx_device *dev,
  const gs_imager_state *pis, const gs_image_t *pim,
  gs_image_format_t format, gs_image_shape_t shape,
  const gx_drawing_color *pdcolor, const gx_clip_path *pcpath,
  gs_memory_t *mem, void **pinfo)
{	clist_image_enum *pie;
	int base_index;
	bool indexed;
	int num_components;
	gs_matrix mat;
	int code;

	/* See above for why we allocate the enumerator as immovable. */
	pie = gs_alloc_struct_immovable(mem, clist_image_enum,
					&st_clist_image_enum,
					"clist_begin_image");
	if ( pie == 0 )
	  return_error(gs_error_VMerror);
	pie->memory = mem;
	*pinfo = pie;
	if ( pim->ImageMask )
	  { base_index = gs_color_space_index_DeviceGray;  /* arbitrary */
	    indexed = false;
	    num_components = 1;
	  }
	else
	  { const gs_color_space *pcs = pim->ColorSpace;
	    base_index = gs_color_space_get_index(pcs);
	    if ( base_index == gs_color_space_index_Indexed )
	      { const gs_color_space *pbcs =
		  gs_color_space_indexed_base_space(pcs);
		indexed = true;
		base_index = gs_color_space_get_index(pbcs);
		num_components = 1;
	      }
	    else
	      { indexed = false;
		num_components = gs_color_space_num_components(pcs);
	      }
	  }
	if ( /****** CAN'T HANDLE CIE COLOR YET ******/
	     base_index > gs_color_space_index_DeviceCMYK ||
	     (code = gs_matrix_invert(&pim->ImageMatrix, &mat)) < 0 ||
	     (code = gs_matrix_multiply(&mat, &ctm_only(pis), &mat)) < 0
	     /****** CAN'T HANDLE NON-PURE-COLOR MASKS YET ******/
	     || (pim->ImageMask && !gx_dc_is_pure(pdcolor))
	     /****** CAN'T HANDLE MULTI-PLANE IMAGES YET ******/
	     /****** (requires flipping in cmd_image_data) ******/
	     || format != gs_image_format_chunky
#ifndef USE_HL_IMAGES
	     || 1		/* Always use the default. */
#endif
	   )
	    { int code = gx_default_begin_image(dev, pis, pim, format, shape,
						pdcolor, pcpath, mem,
						&pie->default_info);
	      if ( code < 0 )
		gs_free_object(mem, pie, "clist_begin_image");
	      return code;
	    }
	pie->default_info = 0;
	pie->image = *pim;
	pie->dcolor = *pdcolor;
	pie->pis = pis;
	pie->pcpath = pcpath;
	pie->multi = (format != gs_image_format_chunky);
	pie->num_planes = (pie->multi ? num_components : 1);
	pie->bits_per_pixel =
	  pim->BitsPerComponent * num_components / pie->num_planes;
	pie->bytes_mod = pie->bits_per_pixel / igcd(pie->bits_per_pixel, 8);
	pie->matrix = mat;
	pie->color_space = (base_index << 4) | (indexed ? 8 : 0);
	/* Write out the begin_image command. */
	{ gs_rect sbox, dbox;
	  int y, height;
	  byte cbuf[2 + 14 * sizeof(float)];
	  byte *cp = cbuf;
	  byte b;
	  uint len, total_len;

	  if ( indexed )
	    b = 0;
	  else switch ( pim->BitsPerComponent )
	    {
	    case 1: b = 1 << 5; break;
	    case 2: b = 2 << 5; break;
	    case 4: b = 3 << 5; break;
	    case 8: b = 4 << 5; break;
	    case 12: b = 5 << 5; break;
	    default: return_error(gs_error_rangecheck);
	    }
	  if ( pim->Interpolate )
	    b |= 1 << 4;
	  if ( !(pim->ImageMatrix.xx == pim->Width &&
		 pim->ImageMatrix.xy == 0 &&
		 pim->ImageMatrix.yx == 0 &&
		 pim->ImageMatrix.yy == -pim->Height &&
		 pim->ImageMatrix.tx == 0 &&
		 pim->ImageMatrix.ty == pim->Height
		)
	     )
	    { b |= 1 << 3;
	      cp = cmd_for_matrix(cp, &pim->ImageMatrix);
	    }
	  { static const float base_decode[8] = {0, 1, 0, 1, 0, 1, 0, 1};
	    float indexed_decode[2];
	    const float *default_decode = base_decode;
	    int num_decode = num_components * 2;
	    int i;

	    if ( indexed )
	      { indexed_decode[0] = 0;
		indexed_decode[1] = (1 << pim->BitsPerComponent) - 1;
		default_decode = indexed_decode;
	      }
	    for ( i = 0; i < num_decode; ++i )
	      if ( pim->Decode[i] != default_decode[i] )
		break;
	    if ( i != num_decode )
	      { byte *pdb = cp++;
		byte dflags = 0;

		b |= 1 << 2;
		for ( i = 0; i < num_decode; i += 2 )
		  { float u = pim->Decode[i], v = pim->Decode[i+1];
		    dflags <<= 2;
		    if ( u == 0 && v == default_decode[i+1] )
		      ;
		    else if ( u == default_decode[i+1] && v == 0 )
		      dflags += 1;
		    else
		      { if ( u != 0 )
			  { dflags++;
			    memcpy(cp, &u, sizeof(float));
			    cp += sizeof(float);
			  }
			dflags += 2;
			memcpy(cp, &v, sizeof(float));
			cp += sizeof(float);
		      }
		  }
		*pdb = dflags;
	      }
	  }
	  if ( (indexed ? pim->adjust : pim->CombineWithColor) )
	    b |= 1 << 1;
	  len = cp - cbuf;
	  total_len = 3 + len + cmd_size2w(pim->Width, pim->Height);
	  sbox.p.x = 0;
	  sbox.p.y = 0;
	  sbox.q.x = pim->Width;
	  sbox.q.y = pim->Height;
	  gs_bbox_transform(&sbox, &pie->matrix, &dbox);
	  y = pie->ymin = (int)floor(dbox.p.y);
	  height = (pie->ymax = (int)ceil(dbox.q.y)) - y;
	  BEGIN_RECT
	  byte *dp;

	  set_cmd_put_op(dp, cdev, pcls, cmd_opv_begin_image, total_len);
	  dp[1] = b;
	  /****** ADD split_row IF NECESSARY ******/
	  dp[2] = shape |
	    gs_image_shape_clip_top | gs_image_shape_clip_bottom |
	      gs_image_shape_rows;
	  dp += 3;
	  cmd_put2w(pim->Width, pim->Height, dp);
	  memcpy(dp, cbuf, len);
	  END_RECT
	}
	pie->xywh[0] = 0;		/* x */
	pie->xywh[1] = 0;		/* y */
	pie->xywh[2] = pim->Width;	/* w */
	pie->xywh[3] = 1;		/* h */
	return 0;
}

/* Process the next piece of an image. */
/* We rely on the caller to provide the data in the right order & size. */
private int
clist_image_data(gx_device *dev,
  void *info, const byte **planes, uint raster,
  int bx, int by, int bwidth, int bheight)
{	clist_image_enum *pie = info;
	const gs_imager_state *pis = pie->pis;
	const gx_clip_path *pcpath = pie->pcpath;
	gs_logical_operation_t lop;
	gs_rect sbox, dbox;
	int y, height;		/* for BEGIN/END_RECT */
	uint unknown = 0;

	if ( pie->default_info )
	  return gx_default_image_data(dev, pie->default_info, planes, raster,
				       bx, by, bwidth, bheight);
	lop = pis->log_op;
	if ( state_neq(ctm.xx) || state_neq(ctm.xy) ||
	     state_neq(ctm.yx) || state_neq(ctm.yy) ||
	     state_neq(ctm.tx) || state_neq(ctm.ty)
	   )
	  { unknown |= ctm_known;
	    state_update(ctm);
	  }
	if ( cmd_check_clip_path(cdev, pcpath) )
	  unknown |= clip_path_known;
	if ( cdev->color_space != pie->color_space ||
	     ((cdev->color_space & 8) != 0 &&
	      cdev->indexed_hival !=
	       pie->image.ColorSpace->params.indexed.hival)
	   )
	  { unknown |= color_space_known;
	    cdev->color_space = pie->color_space;
	    if ( cdev->color_space & 8 )
	      cdev->indexed_hival =
		pie->image.ColorSpace->params.indexed.hival;
	  }
	if ( unknown )
	  cmd_clear_known(cdev, unknown);
	sbox.p.x = bx;
	sbox.p.y = by;
	sbox.q.x = bx + bwidth;
	sbox.q.y = by + bheight;
	gs_bbox_transform(&sbox, &pie->matrix, &dbox);
	y = (int)floor(dbox.p.y);
	height = (int)ceil(dbox.q.y) - y;
	BEGIN_RECT
	int code;

	cmd_do_write_unknown(cdev, pcls,
			     ctm_known | clip_path_known | color_space_known);
	cmd_do_enable_clip(cdev, pcls, pie->pcpath != NULL);
	if ( lop == lop_default )
	  { cmd_disable_lop(cdev, pcls);
	  }
	else
	  { if ( lop != pcls->lop )
	      { byte *dp;
		set_cmd_put_op(dp, cdev, pcls,
			       cmd_opv_set_lop, 1 + cmd_size_w(lop));
		++dp;
		cmd_put_w(lop, dp);
		pcls->lop = lop;
	      }
	    cmd_enable_lop(cdev, pcls);
	  }
	if ( pie->image.ImageMask )
	  { code = cmd_put_drawing_color(cdev, pcls, &pie->dcolor);
	    if ( code < 0 )
	      return code;
	  }
	/*
	 * Currently we only clip unrotated images; for others, we transmit
	 * the entire block of data in every band it may overlap.
	 * (Since blocks tend to be small, we don't transmit much twice.)
	 */
	{ int iy, ih;
	  uint bytes_per_row;
	  long offset;

#if 0
	  if ( pie->matrix.xy == 0 && pie->matrix.yx == 0 )
	    { /* Unrotated image, clip the data. */
	      /****** NOT IMPLEMENTED YET ******/
	    }
	  else
#endif
	    { iy = by;
	      ih = bheight;
	      offset = 0;
	    }
	  bytes_per_row = (bwidth * pie->bits_per_pixel + 7) >> 3;
	  if ( bytes_per_row <= cbuf_size - cmd_largest_size )
	    { /* Transmit multiple complete rows. */
	      uint rows_per_cmd =
		(cbuf_size - cmd_largest_size) / bytes_per_row;
	      int nrows;

	      for ( ; ih > 0;
		    iy += nrows, ih -= nrows, offset += raster * nrows
		  )
		{ nrows = min(ih, rows_per_cmd);
		  code = cmd_image_data(cdev, pcls, pie,
					planes[0] + offset,
					raster, bytes_per_row,
					bx, iy, bwidth, nrows);
		  if ( code < 0 )
		    return code;
		}
	    }
	  else
	    { /* Transmit partial rows. */
	      uint bytes_per_cmd =
		((cbuf_size - cmd_largest_size) / pie->bytes_mod) *
		  pie->bytes_mod;
	      int width_per_cmd = bytes_per_cmd * 8 / pie->bits_per_pixel;

	      for ( ; ih > 0; ++iy, --ih, offset += raster )
		{ uint xoff = 0, xbytes;
		  int dx = 0, w;

		  for ( ; xoff < bytes_per_row; xoff += xbytes, dx += w )
		    { xbytes = min(bytes_per_cmd, bytes_per_row - xoff);
		      w = min(width_per_cmd, bwidth - dx);
		      code = cmd_image_data(cdev, pcls, pie,
					    planes[0] + offset + xoff,
					    raster, xbytes,
					    bx + dx, w, iy, 1);
		      if ( code < 0 )
			return code;
		    }
		}
	    }
	}


	END_RECT
	return 0;
}

/* Clean up by releasing the buffers. */
private int
clist_end_image(gx_device *dev, void *info, bool draw_last)
{	clist_image_enum *pie = info;
	int code;

	if ( pie->default_info )
	  code = gx_default_end_image(dev, pie->default_info, draw_last);
	else
	  { int y = pie->ymin;
	    int height = pie->ymax - y;
	    BEGIN_RECT
	      byte *dp;
	      set_cmd_put_op(dp, cdev, pcls, cmd_opv_image_data, 2);
	      dp[1] = 0;		/* EOD */
	    END_RECT
	    code = 0;
	  }
	gs_free_object(pie->memory, pie, "clist_end_image");
	return code;
}

/* ------ Utilities ------ */

/* Write data for a partial image. */
private int
cmd_image_data(gx_device_clist_writer *cldev, gx_clist_state *pcls,
  clist_image_enum *pie, const byte *data, uint raster, uint nbytes,
  int x, int y, int w, int h)
{	byte b = 0;
	uint len = 2 + cmd_sizew(nbytes) + h * nbytes;
	int xywh[4];
	int i;
	byte *dp;

	xywh[0] = x, xywh[1] = y, xywh[2] = w, xywh[3] = h;
	for ( i = 0; i < 4; ++i )
	  { int diff = xywh[i] - pie->xywh[i];
	    b <<= 2;
	    if ( diff == 0 )
	      ;
	    else if ( diff == 1 )
	      b += 1;
	    else if ( diff > 0 )
	      { b += 2;
		len += cmd_sizew(diff);
	      }
	    else
	      { b += 3;
		len += cmd_sizew(-diff);
	      }
	    pie->xywh[i] += diff;
	    xywh[i] = diff;
	  }
	set_cmd_put_op(dp, cldev, pcls, cmd_opv_image_data, len);
	cmd_putw(nbytes, dp);
	*dp++ = b;
	for ( i = 0; i < 4; ++i )
	  { int diff = xywh[i];
	    if ( diff & ~1 )
	      dp = cmd_putw((diff < 0 ? -diff : diff), dp);
	  }
	for ( i = 0; i < h; ++i )
	  { memcpy(dp, data + i * raster, nbytes);
	    dp += nbytes;
	  }
	return 0;
}
