/* Copyright (C) 1989, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gximage.c */
/* Image setup procedures for Ghostscript library */
#include "gx.h"
#include "math_.h"
#include "memory_.h"
#include "gpcheck.h"
#include "gserrors.h"
#include "gsstruct.h"
#include "gxfixed.h"
#include "gxfrac.h"
#include "gxarith.h"
#include "gxmatrix.h"
#include "gsccolor.h"
#include "gspaint.h"
#include "gzstate.h"
#include "gxdevice.h"
#include "gzpath.h"
#include "gzcpath.h"
#include "gxdevmem.h"
#include "gximage.h"
#include "gdevmrop.h"

/****************************************************************
 * NOTE: This file assumes that the gx_imager_state passed to
 * gx_default_begin_image is actually a gs_state *.
 * This is clearly incorrect and will be fixed in the future.
 * (It is, however, correct for direct calls from the PostScript
 * interpreter.)  Meanwhile, use at your own risk.
 ****************************************************************/

/* Structure descriptor */
private_st_gx_image_enum();

/* Define the procedures for initializing gs_image_ts to default values. */
private void
image_t_init(gs_image_t *pim, bool mask)
{	pim->Width = pim->Height = 0;
	gs_make_identity(&pim->ImageMatrix);
	pim->BitsPerComponent = 1;
	/* Doesn't fill in ColorSpace. */
	/* Doesn't fill in Decode. */
	pim->Interpolate = false;
	pim->ImageMask = pim->adjust = mask;
	pim->CombineWithColor = false;
}
void
gs_image_t_init_mask(gs_image_t *pim, bool write_1s)
{	image_t_init(pim, true);
	pim->ColorSpace = NULL;
	if ( write_1s )
	  pim->Decode[0] = 1, pim->Decode[1] = 0;
	else
	  pim->Decode[0] = 0, pim->Decode[1] = 1;
}
void
gs_image_t_init_gray(gs_image_t *pim)
{	image_t_init(pim, false);
	pim->ColorSpace = gs_color_space_DeviceGray();
	pim->Decode[0] = 0;
	pim->Decode[1] = 1;
}
void
gs_image_t_init_color(gs_image_t *pim)
{	gs_image_t_init_gray(pim);
	pim->ColorSpace = gs_color_space_DeviceRGB();
	pim->Decode[2] = pim->Decode[4] = pim->Decode[6] = 0;
	pim->Decode[3] = pim->Decode[5] = pim->Decode[7] = 1;
}

/* Declare the 1-for-1 unpacking procedure so we can test for it */
/* in the GC procedures. */
extern iunpack_proc(image_unpack_copy);

/* GC procedures */
#define eptr ((gx_image_enum *)vptr)
private ENUM_PTRS_BEGIN(image_enum_enum_ptrs) {
	int bps;
	gs_ptr_type_t ret;
	/* Enumerate the used members of clues.dev_color. */
	index -= gx_image_enum_num_ptrs;
	bps = eptr->unpack_bps;
	if ( eptr->spp != 1 )
	  bps = 8;
	else if ( bps > 8 || eptr->unpack == image_unpack_copy )
	  bps = 1;
	if ( index >= (1 << bps) * st_device_color_max_ptrs ) /* done */
	  return 0;
	ret = (*st_device_color.enum_ptrs)
	  (&eptr->clues[(index / st_device_color_max_ptrs) *
			(255 / ((1 << bps) - 1))].dev_color,
	   sizeof(eptr->clues[0].dev_color),
	   index % st_device_color_max_ptrs, pep);
	if ( ret == 0 )		/* don't stop early */
	  { *pep = 0;
	    break;
	  }
	return ret;
	}
#define e1(i,elt) ENUM_PTR(i,gx_image_enum,elt);
	gx_image_enum_do_ptrs(e1)
#undef e1
ENUM_PTRS_END
private RELOC_PTRS_BEGIN(image_enum_reloc_ptrs) {
	int i;
#define r1(i,elt) RELOC_PTR(gx_image_enum,elt);
	gx_image_enum_do_ptrs(r1)
#undef r1
	{ int bps = eptr->unpack_bps;
	  if ( eptr->spp != 1 )
	    bps = 8;
	  else if ( bps > 8 || eptr->unpack == image_unpack_copy )
	    bps = 1;
	  for ( i = 0; i <= 255; i += 255 / ((1 << bps) - 1) )
	    (*st_device_color.reloc_ptrs)
	      (&eptr->clues[i].dev_color, sizeof(gx_device_color), gcst);
	}
} RELOC_PTRS_END
#undef eptr

/* Forward declarations */
private void image_init_map(P3(byte *map, int map_size, const float *decode));
private void image_init_colors(P7(gx_image_enum *penum, const gs_image_t *pim,
				  bool multi, gs_state *pgs, int spp,
				  const gs_color_space *pcs, bool *pdcb));

/* Procedures for unpacking the input data into bytes or fracs. */
/*extern iunpack_proc(image_unpack_copy);*/	/* declared above */
extern iunpack_proc(image_unpack_1);
extern iunpack_proc(image_unpack_1_spread);
extern iunpack_proc(image_unpack_2);
extern iunpack_proc(image_unpack_2_spread);
extern iunpack_proc(image_unpack_4);
extern iunpack_proc(image_unpack_8);
extern iunpack_proc(image_unpack_8_spread);
extern iunpack_proc(image_unpack_12);

/* The image_render procedures work on fully expanded, complete rows. */
/* These take a height argument, which is an integer >= 0; */
/* they return a negative code, or the number of */
/* rows actually processed (which may be less than the height). */
/* height = 0 is a special call to indicated that the image has been */
/* fully processed; this is necessary because the last scan lines of */
/* the source data may not produce any output. */
extern irender_proc(image_render_skip);
extern irender_proc(image_render_simple);
extern irender_proc(image_render_landscape);
extern irender_proc(image_render_mono);
extern irender_proc(image_render_color);
extern irender_proc(image_render_frac);
extern irender_proc(image_render_interpolate);

/* Define 'strategy' procedures for selecting imaging methods. */
/* Strategies are called in a known order, so each one may assume */
/* that all the previous ones failed. */
/* If a strategy succeeds, it may update the enumerator structure */
/* as well as returning the rendering procedure. */
typedef irender_proc((*irender_proc_t));
#define image_strategy_proc(proc)\
  irender_proc_t proc(P1(gx_image_enum *penum))
private image_strategy_proc(image_strategy_skip);
private image_strategy_proc(image_strategy_interpolate);
private image_strategy_proc(image_strategy_simple);
private image_strategy_proc(image_strategy_frac);
private image_strategy_proc(image_strategy_mono);

/* Standard mask tables for spreading input data. */
/* Note that the mask tables depend on the end-orientation of the CPU. */
/* We can't simply define them as byte arrays, because */
/* they might not wind up properly long- or short-aligned. */
#define map4tox(z,a,b,c,d)\
	z, z^a, z^b, z^(a+b),\
	z^c, z^(a+c), z^(b+c), z^(a+b+c),\
	z^d, z^(a+d), z^(b+d), z^(a+b+d),\
	z^(c+d), z^(a+c+d), z^(b+c+d), z^(a+b+c+d)
#if arch_is_big_endian
private const bits32 map_4x1_to_32[16] =
   {	map4tox(0L, 0xffL, 0xff00L, 0xff0000L, 0xff000000L)	};
private const bits32 map_4x1_to_32_invert[16] =
   {	map4tox(0xffffffffL, 0xffL, 0xff00L, 0xff0000L, 0xff000000L)	};
#else					/* !arch_is_big_endian */
private const bits32 map_4x1_to_32[16] =
   {	map4tox(0L, 0xff000000L, 0xff0000L, 0xff00L, 0xffL)	};
private const bits32 map_4x1_to_32_invert[16] =
   {	map4tox(0xffffffffL, 0xff000000L, 0xff0000L, 0xff00L, 0xffL)	};
#endif

/* Start processing an image. */
int
gx_default_begin_image(gx_device *dev,
  const gs_imager_state *pis, const gs_image_t *pim,
  gs_image_format_t format, gs_image_shape_t shape,
  const gx_drawing_color *pdcolor, const gx_clip_path *pcpath,
  gs_memory_t *mem, void **pinfo)
{	gx_image_enum *penum;
	int width = pim->Width;
	int height = pim->Height;
	int bps = pim->BitsPerComponent;
	bool multi;
	int index_bps;
	const gs_color_space *pcs = pim->ColorSpace;
	gs_state *pgs = (gs_state *)pis;	/****** SEE ABOVE ******/
	int code;
	gs_matrix mat;
	int log2_xbytes = (bps <= 8 ? 0 : arch_log2_sizeof_frac);
	int spp, nplanes, spread;
	uint bsize;
	byte *buffer;
	fixed mtx, mty;
	gs_fixed_point row_extent, col_extent;
	bool device_color;
	gs_fixed_rect obox, cbox;
	fixed adjust;

	if ( width < 0 || height < 0 ||
	     (shape & (gs_image_shape_clip_left |
		       gs_image_shape_clip_right |
		       gs_image_shape_varying_width)) != 0
	   )
	  return_error(gs_error_rangecheck);
	switch ( format )
	  { case gs_image_format_chunky: multi = false; break;
	    case gs_image_format_component_planar: multi = true; break;
	    default: return_error(gs_error_rangecheck);
	  }
	switch ( bps )
	   {
	   case 1: index_bps = 0; break;
	   case 2: index_bps = 1; break;
	   case 4: index_bps = 2; break;
	   case 8: index_bps = 3; break;
	   case 12: index_bps = 4; break;
	   default: return_error(gs_error_rangecheck);
	   }
	if ( (code = gs_matrix_invert(&pim->ImageMatrix, &mat)) < 0 ||
	     (code = gs_matrix_multiply(&mat, &ctm_only(pgs), &mat)) < 0 ||
	     (code =
	      gs_distance_transform2fixed((const gs_matrix_fixed *)&mat,
					  (floatp)width, (floatp)0,
					  &row_extent)) < 0 ||
	     (code =
	      gs_distance_transform2fixed((const gs_matrix_fixed *)&mat,
					  (floatp)0, (floatp)height,
					  &col_extent)) < 0
	   )
	  return code;
	penum = gs_alloc_struct(mem, gx_image_enum, &st_gx_image_enum,
				"gx_default_begin_image");
	if ( (penum->masked = pim->ImageMask) )
	  {	/* This is imagemask. */
		const float *decode = pim->Decode;	/* [2] */

		if ( pim->BitsPerComponent != 1 || multi || pcs != NULL ||
		     !((decode[0] == 0.0 && decode[1] == 1.0) ||
		       (decode[0] == 1.0 && decode[1] == 0.0))
		   )
		  return_error(gs_error_rangecheck);
		/* Initialize color entries 0 and 255. */
		color_set_pure(&penum->icolor0, gx_no_color_index);
		penum->icolor1 = *pdcolor;
		memcpy(&penum->map[0].table.lookup4x1to32[0],
		       (decode[0] == 0 ? map_4x1_to_32_invert :
			map_4x1_to_32),
		       16 * 4);
		penum->map[0].decoding = sd_none;
		spp = 1;
		adjust = (pim->adjust ? float2fixed(0.25) : fixed_0);
	  }
	else
	  {	/* This is image, not imagemask. */
		const gs_color_space_type _ds *pcst = pcs->type;

		spp = pcst->num_components;
		if ( spp < 0 )		/* Pattern not allowed */
		  return_error(gs_error_rangecheck);
		device_color = (*pcst->concrete_space)(pcs, pgs) == pcs;
		image_init_colors(penum, pim, multi, pgs, spp, pcs,
				  &device_color);
		adjust = fixed_0;
	  }
	penum->device_color = device_color;
	bsize = (width + 8) * spp;	/* round up, +1 for end-of-run byte */
	buffer = gs_alloc_bytes(mem, bsize, "image buffer");
	if ( buffer == 0 )
	  return_error(gs_error_VMerror);
	penum->width = width;
	penum->height = height;
	penum->bps = bps;
	penum->unpack_bps = bps;
	penum->log2_xbytes = log2_xbytes;
	penum->spp = spp;
	nplanes = (multi ? spp : 1);
	penum->num_planes = nplanes;
	spread = nplanes << log2_xbytes;
	penum->spread = spread;
	penum->matrix = mat;
	penum->row_extent = row_extent;
	penum->posture =
	  ((row_extent.y | col_extent.x) == 0 ? image_portrait :
	   (row_extent.x | col_extent.y) == 0 ? image_landscape :
	   image_skewed);
	mtx = float2fixed(mat.tx);
	mty = float2fixed(mat.ty);
	penum->pgs = pgs;
	penum->pis = pis;
	penum->pcs = pcs;
	penum->memory = mem;
	penum->dev = pgs->device;
	penum->buffer = buffer;
	penum->buffer_size = bsize;
	penum->line = 0;
	penum->line_size = 0;
	penum->bytes_per_row =
	  (uint)(((ulong)width * (bps * spp) / nplanes + 7) >> 3);
	penum->interpolate = pim->Interpolate;
	penum->use_rop = pim->CombineWithColor && !pim->ImageMask;
	penum->slow_loop = 0;
	penum->clip_image =
	  (pcpath == 0 ?
	   (obox.p.x = obox.p.y = min_fixed, obox.q.x = obox.q.y = max_fixed,
	    cbox.p.x = cbox.p.y = cbox.q.x = cbox.q.y = 0, 0) :
	   gx_cpath_outer_box(pcpath, &obox) |	/* not || */
	   gx_cpath_inner_box(pcpath, &cbox) ?
	   0 : image_clip_region);
	penum->clip_outer = obox;
	penum->clip_inner = cbox;
	penum->log_op = (penum->use_rop ? rop3_T : pis->log_op);
	penum->clip_dev = 0;		/* in case we bail out */
	penum->rop_dev = 0;		/* ditto */
	penum->scaler = 0;		/* ditto */
	/*
	 * If all four extrema of the image fall within the clipping
	 * rectangle, clipping is never required.  When making this check,
	 * we must carefully take into account the fact that we only care
	 * about pixel centers.
	 */
	   {	int hwx, hwy;
		fixed
		  epx = min(row_extent.x, 0) + min(col_extent.x, 0),
		  eqx = max(row_extent.x, 0) + max(col_extent.x, 0),
		  epy = min(row_extent.y, 0) + min(col_extent.y, 0),
		  eqy = max(row_extent.y, 0) + max(col_extent.y, 0);

		switch ( penum->posture )
		  {
		  case image_portrait:
		    hwx = width, hwy = height;
		    break;
		  case image_landscape:
		    hwx = height, hwy = width;
		    break;
		  default:
		    hwx = hwy = 0;
		  }
		{	/*
			 * If the image is only 1 sample wide or high,
			 * and is less than 1 device pixel wide or high,
			 * move it slightly so that it covers pixel centers.
			 * This is a hack to work around a bug in some old
			 * versions of TeX/dvips, which use 1-bit-high images
			 * to draw horizontal and vertical lines without
			 * positioning them properly.
			 */
			fixed diff;
			if ( hwx == 1 && eqx - epx < fixed_1 )
			  {	diff =
				  arith_rshift_1(row_extent.x + col_extent.x);
				mtx = (((mtx + diff) | fixed_half)
					& -fixed_half) - diff;
			  }
			if ( hwy == 1 && eqy - epy < fixed_1 )
			  {	diff =
				  arith_rshift_1(row_extent.y + col_extent.y);
				mty = (((mty + diff) | fixed_half)
					& -fixed_half) - diff;
			  }
		}
		if ( !penum->clip_image )	/* i.e., not clip region */
		  penum->clip_image =
		    (fixed_pixround(mtx + epx) < fixed_pixround(cbox.p.x) ?
		     image_clip_xmin : 0) +
		    (fixed_pixround(mtx + eqx) >= fixed_pixround(cbox.q.x) ?
		     image_clip_xmax : 0) +
		    (fixed_pixround(mty + epy) < fixed_pixround(cbox.p.y) ?
		     image_clip_ymin : 0) +
		    (fixed_pixround(mty + eqy) >= fixed_pixround(cbox.q.y) ?
		     image_clip_ymax : 0);
	   }
	if_debug11('b',
		   "[b]Image: cbox=(%g,%g),(%g,%g), obox=(%g,%g),(%g,%g)\n	mt=(%g,%g) clip_image=0x%x\n",
		   fixed2float(cbox.p.x), fixed2float(cbox.p.y),
		   fixed2float(cbox.q.x), fixed2float(cbox.q.y),
		   fixed2float(obox.p.x), fixed2float(obox.p.y),
		   fixed2float(obox.q.x), fixed2float(obox.q.y),
		   fixed2float(mtx), fixed2float(mty), penum->clip_image);
	penum->byte_in_row = 0;
	penum->xcur = penum->mtx = mtx;
	dda_init(penum->next_x, mtx, col_extent.x, height);
	penum->ycur = penum->mty = mty;
	dda_init(penum->next_y, mty, col_extent.y, height);
	penum->x = 0;
	penum->y = 0;
	penum->adjust = adjust;
	   {	static iunpack_proc((*procs[5])) = {
			image_unpack_1, image_unpack_2,
			image_unpack_4, image_unpack_8, image_unpack_12
		   };
		static iunpack_proc((*spread_procs[5])) = {
			image_unpack_1_spread, image_unpack_2_spread,
			image_unpack_4, image_unpack_8_spread,
			image_unpack_12
		   };
		if ( nplanes != 1 )
		  {	penum->unpack = spread_procs[index_bps];
			if_debug1('b', "[b]unpack=spread %d\n", bps);
		  }
		else
		  {	penum->unpack = procs[index_bps];
			if_debug1('b', "[b]unpack=%d\n", bps);
		  }
		/* Use slow loop for imagemask with a halftone, */
		/* or for a non-default logical operation. */
		penum->slow_loop |=
		  (penum->masked &&
		   !color_is_pure(pdcolor)) ||
		  penum->use_rop ||
		  !lop_no_T_is_S(pis->log_op);
		if ( (penum->render = image_strategy_skip(penum)) == 0 &&
		     (penum->render = image_strategy_interpolate(penum)) == 0 &&
		     (penum->render = image_strategy_simple(penum)) == 0 &&
		     (penum->render = image_strategy_frac(penum)) == 0 &&
		     (penum->render = image_strategy_mono(penum)) == 0
		   )
		  {	/* Use default logic. */
			penum->render = image_render_color;
		  }
	   }
	if ( penum->clip_image && pcpath )
	  {	/* Set up the clipping device. */
		gx_device_clip *cdev =
		  gs_alloc_struct(mem, gx_device_clip,
				  &st_device_clip, "image clipper");
		if ( cdev == 0 )
		  {	gx_default_end_image(dev, penum, false);
			return_error(gs_error_VMerror);
		  }
		gx_make_clip_device(cdev, cdev, &pcpath->list);
		penum->clip_dev = cdev;
		cdev->target = gs_currentdevice(pgs);
		(*dev_proc(cdev, open_device))((gx_device *)cdev);
	  }
	if ( penum->use_rop )
	  {	/* Set up the RasterOp source device. */
		gx_device_rop_texture *rtdev =
		  gs_alloc_struct(mem, gx_device_rop_texture,
				  &st_device_rop_texture, "image RasterOp");
		if ( rtdev == 0 )
		  {	gx_default_end_image(dev, penum, false);
			return_error(gs_error_VMerror);
		  }
		gx_make_rop_texture_device(rtdev,
					  (penum->clip_dev != 0 ?
					   (gx_device *)penum->clip_dev :
					   dev), pis->log_op, pdcolor);
		penum->rop_dev = rtdev;
	  }
	if_debug8('b', "[b]Image: w=%d h=%d [%g %g %g %g %g %g]\n",
		 width, height,
		 mat.xx, mat.xy, mat.yx, mat.yy, mat.tx, mat.ty);
	*pinfo = penum;
	return 0;
}

/* Initialize the color mapping tables for a non-mask image. */
private void
image_init_colors(gx_image_enum *penum, const gs_image_t *pim, bool multi,
  gs_state *pgs, int spp, const gs_color_space *pcs, bool *pdcb)
{	int bps = pim->BitsPerComponent;
	const float *decode = pim->Decode;	/* [spp*2] */
	int ci;
	static const float default_decode[8] =
	  { 0.0, 1.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0 };

	/* Initialize the color table */

#define ictype(i)\
  penum->clues[i].dev_color.type
	switch ( (spp == 1 ? bps : 8) )
	{
	case 8:			/* includes all color images */
	{	register gx_image_clue *pcht = &penum->clues[0];
		register int n = 64;
		do
		{	pcht[0].dev_color.type =
			  pcht[1].dev_color.type =
			  pcht[2].dev_color.type =
			  pcht[3].dev_color.type =
			    gx_dc_type_none;
			pcht[0].key = pcht[1].key =
			  pcht[2].key = pcht[3].key = 0;
			pcht += 4;
		}
		while ( --n > 0 );
		penum->clues[0].key = 1;	/* guarantee no hit */
		break;
	}
	case 4:
		ictype(17) = ictype(2*17) = ictype(3*17) =
		  ictype(4*17) = ictype(6*17) = ictype(7*17) =
		  ictype(8*17) = ictype(9*17) = ictype(11*17) =
		  ictype(12*17) = ictype(13*17) = ictype(14*17) =
		    gx_dc_type_none;
		/* falls through */
	case 2:
		ictype(5*17) = ictype(10*17) = gx_dc_type_none;
#undef ictype
	}

	/* Initialize the maps from samples to intensities. */

	for ( ci = 0; ci < spp; ci++ )
	{	sample_map *pmap = &penum->map[ci];

		/* If the decoding is [0 1] or [1 0], we can fold it */
		/* into the expansion of the sample values; */
		/* otherwise, we have to use the floating point method. */

		const float *this_decode = &decode[ci * 2];
		const float *map_decode;	/* decoding used to */
				/* construct the expansion map */

		const float *real_decode;	/* decoding for */
				/* expanded samples */

		bool no_decode;

		map_decode = real_decode = this_decode;
		if ( map_decode[0] == 0.0 && map_decode[1] == 1.0 )
			no_decode = true;
		else if ( map_decode[0] == 1.0 && map_decode[1] == 0.0 )
			no_decode = true,
			real_decode = default_decode;
		else
			no_decode = false,
			*pdcb = false,
			map_decode = default_decode;
		if ( bps > 2 || multi )
		{	if ( bps <= 8 )
			  image_init_map(&pmap->table.lookup8[0], 1 << bps,
					 map_decode);
		}
		else
		{	/* The map index encompasses more than one pixel. */
			byte map[4];
			register int i;
			image_init_map(&map[0], 1 << bps, map_decode);
			switch ( bps )
			{
			case 1:
			{	register bits32 *p = &pmap->table.lookup4x1to32[0];
				if ( map[0] == 0 && map[1] == 0xff )
					memcpy((byte *)p, map_4x1_to_32, 16 * 4);
				else if ( map[0] == 0xff && map[1] == 0 )
					memcpy((byte *)p, map_4x1_to_32_invert, 16 * 4);
				else
				  for ( i = 0; i < 16; i++, p++ )
					((byte *)p)[0] = map[i >> 3],
					((byte *)p)[1] = map[(i >> 2) & 1],
					((byte *)p)[2] = map[(i >> 1) & 1],
					((byte *)p)[3] = map[i & 1];
			}	break;
			case 2:
			{	register bits16 *p = &pmap->table.lookup2x2to16[0];
				for ( i = 0; i < 16; i++, p++ )
					((byte *)p)[0] = map[i >> 2],
					((byte *)p)[1] = map[i & 3];
			}	break;
			}
		}
		pmap->decode_base /* = decode_lookup[0] */ = real_decode[0];
		pmap->decode_factor =
		  (real_decode[1] - real_decode[0]) /
		    (bps <= 8 ? 255.0 : (float)frac_1);
		pmap->decode_max /* = decode_lookup[15] */ = real_decode[1];
		if ( no_decode )
			pmap->decoding = sd_none;
		else if ( bps <= 4 )
		{	int step = 15 / ((1 << bps) - 1);
			int i;

			pmap->decoding = sd_lookup;
			for ( i = 15 - step; i > 0; i -= step )
			  pmap->decode_lookup[i] = pmap->decode_base +
			    i * (255.0 / 15) * pmap->decode_factor;
		}
		else
			pmap->decoding = sd_compute;
		if ( spp == 1 )		/* and ci == 0 */
		{	/* Pre-map entries 0 and 255. */
			gs_client_color cc;
			cc.paint.values[0] = real_decode[0];
			(*pcs->type->remap_color)(&cc, pcs, &penum->icolor0, pgs);
			cc.paint.values[0] = real_decode[1];
			(*pcs->type->remap_color)(&cc, pcs, &penum->icolor1, pgs);
		}
	}

}
/* Construct a mapping table for sample values. */
/* map_size is 2, 4, 16, or 256.  Note that 255 % (map_size - 1) == 0. */
private void
image_init_map(byte *map, int map_size, const float *decode)
{	float min_v = decode[0], max_v = decode[1];
	byte *limit = map + map_size;
	uint value = min_v * 0xffffL;
	/* The division in the next statement is exact, */
	/* see the comment above. */
	uint diff = (max_v - min_v) * (0xffffL / (map_size - 1));
	for ( ; map != limit; map++, value += diff )
		*map = value >> 8;
}

/* Strategy procedures */

/* If we're in a charpath, don't image anything. */
private irender_proc_t
image_strategy_skip(gx_image_enum *penum)
{	if ( !penum->pgs->in_charpath )
	  return 0;
	if_debug0('b', "[b]render=skip\n");
	return image_render_skip;
}

/* If we're interpolating, use special logic. */
private irender_proc_t
image_strategy_interpolate(gx_image_enum *penum)
{	gs_state *pgs = penum->pgs;
	gs_memory_t *mem = penum->memory;
	stream_IScale_state iss;
	stream_IScale_state *pss;
	byte *line;
	const gs_color_space *pcs = penum->pcs;
	gs_point dst_xy;

	if ( !penum->interpolate )
	  return 0;
	if ( penum->posture != image_portrait || penum->masked )
	  {	/* We can't handle these cases yet.  Punt. */
		penum->interpolate = false;
		return 0;
	  }
	iss.memory = mem;
	/* Non-ANSI compilers require the following casts: */
	gs_distance_transform((float)penum->width, (float)penum->height,
			      &penum->matrix, &dst_xy);
	if ( penum->bps <= 8 && penum->device_color )
	  iss.BitsPerComponentIn = 8,
	  iss.MaxValueIn = 0xff;
	else
	  iss.BitsPerComponentIn = sizeof(frac) * 8,
	  iss.MaxValueIn = frac_1;
	iss.BitsPerComponentOut = sizeof(frac) * 8;
	iss.MaxValueOut = frac_1;
	iss.WidthOut = (int)ceil(fabs(dst_xy.x));
	iss.HeightOut = (int)ceil(fabs(dst_xy.y));
	iss.WidthIn = penum->width;
	iss.HeightIn = penum->height;
	iss.Colors = cs_concrete_space(pcs, pgs)->type->num_components;
	/* Allocate a buffer for one source/destination line. */
	{ uint in_size =
	    iss.WidthIn * iss.Colors * (iss.BitsPerComponentIn / 8);
	  uint out_size =
	    iss.WidthOut * iss.Colors *
	      max(iss.BitsPerComponentOut / 8, sizeof(gx_color_index));
	  line = gs_alloc_bytes(mem, max(in_size, out_size),
				"image scale src line");
	}
	pss = gs_alloc_struct(mem, stream_IScale_state,
			      &st_IScale_state, "image scale state");
	if ( line == 0 || pss == 0 ||
	     (*pss = iss,
	      (*s_IScale_template.init)((stream_state *)pss) < 0)
	   )
	  { gs_free_object(mem, pss, "image scale state");
	    gs_free_object(mem, line, "image scale src line");
	    /* Try again without interpolation. */
	    penum->interpolate = false;
	    return 0;
	  }
	penum->line = line;
	penum->scaler = pss;
	penum->line_xy = 0;
	if_debug0('b', "[b]render=interpolate\n");
	return image_render_interpolate;
}

/* Use special fast logic for portrait or landscape black-and-white images. */
private irender_proc_t
image_strategy_simple(gx_image_enum *penum)
{	irender_proc_t rproc;
	if ( !(penum->spp == 1 && penum->bps == 1 && !penum->slow_loop &&
	       (penum->masked ||
		(color_is_pure(&penum->icolor0) &&
		 color_is_pure(&penum->icolor1))))
	   )
	  return 0;
	switch ( penum->posture )
	  {
	  case image_portrait:
	  {	/* Use fast portrait algorithm. */
		long dev_width =
		  fixed2long_rounded(penum->mtx + penum->row_extent.x) -
		    fixed2long_rounded(penum->mtx);

		if ( dev_width != penum->width )
		  {	/* Add an extra align_bitmap_mod of padding so that */
			/* we can align scaled rows with the device. */
			long line_size =
			  bitmap_raster(any_abs(dev_width)) + align_bitmap_mod;
			if ( penum->adjust != 0 || line_size > max_uint )
			  return 0;
			/* Must buffer a scan line. */
			penum->line_width = any_abs(dev_width);
			penum->line_size = (uint)line_size;
			penum->line = gs_alloc_bytes(penum->memory,
					     penum->line_size, "image line");
			if ( penum->line == 0 )
			  {	gx_default_end_image(penum->dev, penum, false);
				return 0;
			  }
		  }
		if_debug2('b', "[b]render=simple, unpack=copy; width=%d, dev_width=%ld\n",
			  penum->width, dev_width);
		rproc = image_render_simple;
		break;
	  }
	  case image_landscape:
	  {	/* Use fast landscape algorithm. */
		long dev_width =
		  fixed2long_rounded(penum->mty + penum->row_extent.y) -
		    fixed2long_rounded(penum->mty);
		long line_size =
		  (dev_width = any_abs(dev_width),
		   bitmap_raster(dev_width) * 8 +
		   round_up(dev_width, 8) * align_bitmap_mod);

		if ( (dev_width != penum->width && penum->adjust != 0) ||
		     line_size > max_uint
		   )
		  return 0;
		/* Must buffer a group of 8N scan lines. */
		penum->line_width = dev_width;
		penum->line_size = (uint)line_size;
		penum->line = gs_alloc_bytes(penum->memory,
				     penum->line_size, "image line");
		if ( penum->line == 0 )
		  {	gx_default_end_image(penum->dev, penum, false);
			return 0;
		  }
		penum->line_xy = fixed2int_var_rounded(penum->xcur);
		if_debug3('b', "[b]render=landscape, unpack=copy; width=%d, dev_width=%ld, line_size=%ld\n",
			  penum->width, dev_width, line_size);
		rproc = image_render_landscape;
		break;
	  }
	  default:
	    return 0;
	  }
	/* We don't want to spread the samples, */
	/* but we have to reset unpack_bps to prevent the buffer */
	/* pointer from being incremented by 8 bytes */
	/* per input byte. */
	penum->unpack = image_unpack_copy;
	penum->unpack_bps = 8;
	return rproc;
}

/* We can bypass X clipping for portrait monochrome images. */
private irender_proc_t
image_strategy_mono(gx_image_enum *penum)
{	if ( penum->spp == 1 )
	  { if ( !(penum->slow_loop || penum->posture != image_portrait) )
	      penum->clip_image &= ~(image_clip_xmin | image_clip_xmax);
	    if_debug0('b', "[b]render=mono\n");
	    return image_render_mono;
	  }
	return 0;
}

/* Use special (slow) logic for 12-bit source values. */
private irender_proc_t
image_strategy_frac(gx_image_enum *penum)
{	if ( penum->bps > 8 )
	  {	if_debug0('b', "[b]render=frac\n");
		return image_render_frac;
	  }
	return 0;
}
