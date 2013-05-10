/* Copyright (C) 1993, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gspcolor.c */
/* Pattern color operators and procedures for Ghostscript library */
#include "math_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsstruct.h"
#include "gsutil.h"			/* for gs_next_ids */
#include "gxarith.h"
#include "gxfixed.h"
#include "gxmatrix.h"
#include "gxcoord.h"			/* for gs_concat, gx_tr'_to_fixed */
#include "gxcspace.h"			/* for gscolor2.h */
#include "gxcolor2.h"
#include "gxdcolor.h"
#include "gxdevice.h"
#include "gxdevmem.h"
#include "gxclip2.h"
#include "gspath.h"
#include "gxpath.h"
#include "gxpcolor.h"
#include "gzstate.h"
#include "gsimage.h"

private_st_client_pattern();
public_st_pattern_instance();

/* Define the Pattern color space. */
extern cs_proc_remap_color(gx_remap_Pattern);
private cs_proc_install_cspace(gx_install_Pattern);
private cs_proc_adjust_cspace_count(gx_adjust_cspace_Pattern);
private cs_proc_init_color(gx_init_Pattern);
private cs_proc_adjust_color_count(gx_adjust_color_Pattern);
private struct_proc_enum_ptrs(gx_enum_ptrs_Pattern);
private struct_proc_reloc_ptrs(gx_reloc_ptrs_Pattern);
const gs_color_space_type
	gs_color_space_type_Pattern =
	 { gs_color_space_index_Pattern, -1, false,
	   gx_init_Pattern, gx_no_concrete_space,
	   gx_no_concretize_color, NULL,
	   gx_remap_Pattern, gx_install_Pattern,
	   gx_adjust_cspace_Pattern, gx_adjust_color_Pattern,
	   gx_enum_ptrs_Pattern, gx_reloc_ptrs_Pattern
	 };

/* makepattern */
private int compute_inst_matrix(P3(gs_pattern_instance *pinst,
				   const gs_state *saved,
				   gs_rect *pbbox));
int
gs_makepattern(gs_client_color *pcc, const gs_client_pattern *pcp,
  const gs_matrix *pmat, gs_state *pgs, gs_memory_t *mem)
{	gs_memory_t *save_mem = pgs->memory;
	gs_pattern_instance inst;
	gs_pattern_instance *pinst;
	gs_state *saved;
	gs_rect bbox;
	gs_fixed_rect cbox;
	int code;

	if ( mem == 0 )
	  mem = save_mem;
	rc_alloc_struct_1(pinst, gs_pattern_instance, &st_pattern_instance,
			  mem, return_error(gs_error_VMerror),
			  "gs_makepattern");
	inst.rc = pinst->rc;
	pgs->memory = mem;
	saved = gs_gstate(pgs);
	pgs->memory = save_mem;
	if ( saved == 0 )
	  return_error(gs_error_VMerror);
	gs_concat(saved, pmat);
	gs_newpath(saved);
	inst.template = *pcp;
	inst.saved = saved;
	code = compute_inst_matrix(&inst, saved, &bbox);
	if ( code < 0 )
	  {	gs_state_free(saved);
		return code;
	  }
#define mat inst.matrix
	if_debug6('t', "[t]matrix=[%g %g %g %g %g %g]\n",
		  mat.xx, mat.xy, mat.yx, mat.yy, mat.tx, mat.ty);
	/* Check for singular tiling matrix. */
	if ( fabs(mat.xx * mat.yy - mat.xy * mat.yx) < 1.0e-6 )
	  {	gs_state_free(saved);
		return_error(gs_error_rangecheck);
	  }
	if_debug8('t', "[t]bbox=(%g,%g),(%g,%g) ibbox=(%g,%g),(%g,%g)\n",
		  bbox.p.x, bbox.p.y, bbox.q.x, bbox.q.y,
		  inst.bbox.p.x, inst.bbox.p.y, inst.bbox.q.x, inst.bbox.q.y);
	{	float bbw = bbox.q.x - bbox.p.x;
		float bbh = bbox.q.y - bbox.p.y;
		/* If the step and the size agree to within 1/2 pixel, */
		/* make them the same. */
		inst.size.x = (int)(bbw + 0.8);	/* 0.8 is arbitrary */
		inst.size.y = (int)(bbh + 0.8);
		if ( mat.xy == 0 && mat.yx == 0 &&
		     fabs(fabs(mat.xx) - bbw) < 0.5 &&
		     fabs(fabs(mat.yy) - bbh) < 0.5
		   )
		  {	gs_scale(saved, fabs(inst.size.x / mat.xx),
				 fabs(inst.size.y / mat.yy));
			code = compute_inst_matrix(&inst, saved, &bbox);
			if ( code < 0 )
			  {	gs_state_free(saved);
				return code;
			  }
			if_debug2('t',
				"[t]adjusted XStep & YStep to size=(%d,%d)\n",
				inst.size.x, inst.size.y);
		  }
	}
	inst.offset.x = bbox.p.x - mat.tx;
	inst.offset.y = bbox.p.y - mat.ty;
	gx_translate_to_fixed(saved, -float2fixed(inst.offset.x),
			      -float2fixed(inst.offset.y));
#undef mat
	cbox.p.x = fixed_0;
	cbox.p.y = fixed_0;
	cbox.q.x = int2fixed(inst.size.x);
	cbox.q.y = int2fixed(inst.size.y);
	code = gx_clip_to_rectangle(saved, &cbox);
	if ( code < 0 )
	  { gs_state_free(saved);
	    return code;
	  }
	inst.id = gs_next_ids(1);
	*pinst = inst;
	pcc->pattern = pinst;
	return 0;
#undef mat
}
/* Compute the instance matrix and bounding box from the step values */
/* and the saved matrix. */
private int
compute_inst_matrix(gs_pattern_instance *pinst, const gs_state *saved,
  gs_rect *pbbox)
{	int code;
	pinst->matrix.xx = pinst->template.XStep * saved->ctm.xx;
	pinst->matrix.xy = pinst->template.XStep * saved->ctm.xy;
	pinst->matrix.yx = pinst->template.YStep * saved->ctm.yx;
	pinst->matrix.yy = pinst->template.YStep * saved->ctm.yy;
	pinst->matrix.tx = saved->ctm.tx;
	pinst->matrix.ty = saved->ctm.ty;
	if ( (code = gs_bbox_transform(&pinst->template.BBox, &ctm_only(saved),
				       pbbox)) < 0 ||
	     (code = gs_bbox_transform_inverse(pbbox, &pinst->matrix,
					       &pinst->bbox)) < 0
	   )
	  return code;
	return 0;
}

/* setpattern */
int
gs_setpattern(gs_state *pgs, const gs_client_color *pcc)
{	int code = gs_setpatternspace(pgs);
	if ( code < 0 )
		return code;
	return gs_setcolor(pgs, pcc);
}

/* setpatternspace */
/* This does all the work of setpattern except for the final setcolor. */
int
gs_setpatternspace(gs_state *pgs)
{	int code = 0;
	if ( pgs->color_space->type->index != gs_color_space_index_Pattern )
	{	gs_color_space cs;
		cs.params.pattern.base_space =
			*(gs_paint_color_space *)pgs->color_space;
		cs.params.pattern.has_base_space = true;
		cs.type = &gs_color_space_type_Pattern;
		code = gs_setcolorspace(pgs, &cs);
	}
	return code;
}

/* getpattern */
/* This is only intended for the benefit of pattern PaintProcs. */
const gs_client_pattern *
gs_getpattern(const gs_client_color *pcc)
{	return &pcc->pattern->template;
}

/* makebitmappattern */
private int bitmap_PaintProc(P2(const gs_client_color *, gs_state *));
int
gs_makebitmappattern(gs_client_color *pcc, const gx_tile_bitmap *tile,
  bool mask, gs_state *pgs, gs_memory_t *mem)
{	gs_client_pattern pat;
	gs_matrix mat, smat;

	if ( tile->raster != bitmap_raster(tile->rep_width) )
	  return_error(gs_error_rangecheck);
	uid_set_UniqueID(&pat.uid, gs_next_ids(1));
	pat.PaintType = (mask ? 2 : 1);
	pat.TilingType = 1;
	pat.BBox.p.x = 0;
	pat.BBox.p.y = 0;
	pat.BBox.q.x = tile->rep_width;
	pat.BBox.q.y = tile->rep_height;
	pat.XStep = tile->rep_width;
	pat.YStep = tile->rep_height;
	pat.PaintProc = bitmap_PaintProc;
	pat.client_data = tile->data;
	gs_make_identity(&mat);
	gs_currentmatrix(pgs, &smat);
	if ( smat.yy > 0 )
	  mat.yy = -1;
	gs_setmatrix(pgs, &mat);
	gs_makepattern(pcc, &pat, &mat, pgs, mem);
	gs_setmatrix(pgs, &smat);
	return 0;
}
private int
bitmap_PaintProc(const gs_client_color *pcolor, gs_state *pgs)
{	gs_image_enum *pen = gs_image_enum_alloc(gs_state_memory(pgs), "tppp");
	const gs_client_pattern *ppat = gs_getpattern(pcolor);
	const byte *dp = ppat->client_data;
	gs_image_t image;
	int n;
	uint nbytes, raster, used;

	if ( ppat->PaintType == 2 )
	  gs_image_t_init_mask(&image, true);
	else
	  { gs_image_t_init_gray(&image);
	    image.Decode[0] = 1.0;
	    image.Decode[1] = 0.0;
	  }
	image.Width = (int)ppat->XStep;
	image.Height = (int)ppat->YStep;
	raster = bitmap_raster(image.Width);
	nbytes = (image.Width + 7) >> 3;
	gs_image_init(pen, &image, false, pgs);
	for ( n = image.Height; n > 0; dp += raster, --n )
	  gs_image_next(pen, dp, nbytes, &used);
	gs_image_cleanup(pen);
	gs_free_object(gs_state_memory(pgs), pen, "tppp");
	return 0;
}

/* ------ Color space implementation ------ */

/* Pattern device color types. */
/* We need a masked analogue of each of the non-pattern types, */
/* to handle uncolored patterns. */
/* We use 'masked_fill_rect' instead of 'masked_fill_rectangle' */
/* in order to limit identifier lengths to 32 characters. */
private dev_color_proc_load(gx_dc_pattern_load);
private dev_color_proc_fill_rectangle(gx_dc_pattern_fill_rectangle);
private struct_proc_enum_ptrs(dc_pattern_enum_ptrs);
private struct_proc_reloc_ptrs(dc_pattern_reloc_ptrs);
private dev_color_proc_load(gx_dc_pure_masked_load);
private dev_color_proc_fill_rectangle(gx_dc_pure_masked_fill_rect);
private struct_proc_enum_ptrs(dc_masked_enum_ptrs);
private struct_proc_reloc_ptrs(dc_masked_reloc_ptrs);
private dev_color_proc_load(gx_dc_binary_masked_load);
private dev_color_proc_fill_rectangle(gx_dc_binary_masked_fill_rect);
private struct_proc_enum_ptrs(dc_binary_masked_enum_ptrs);
private struct_proc_reloc_ptrs(dc_binary_masked_reloc_ptrs);
private dev_color_proc_load(gx_dc_colored_masked_load);
private dev_color_proc_fill_rectangle(gx_dc_colored_masked_fill_rect);
/* The device color types are exported for gxpcmap.c. */
const gx_device_color_procs
  gx_dc_pattern =
    { gx_dc_pattern_load, gx_dc_pattern_fill_rectangle,
      dc_pattern_enum_ptrs, dc_pattern_reloc_ptrs
    },
  gx_dc_pure_masked =
    { gx_dc_pure_masked_load, gx_dc_pure_masked_fill_rect,
      dc_masked_enum_ptrs, dc_masked_reloc_ptrs
    },
  gx_dc_binary_masked =
    { gx_dc_binary_masked_load, gx_dc_binary_masked_fill_rect,
      dc_binary_masked_enum_ptrs, dc_binary_masked_reloc_ptrs
    },
  gx_dc_colored_masked =
    { gx_dc_colored_masked_load, gx_dc_colored_masked_fill_rect,
      dc_masked_enum_ptrs, dc_masked_reloc_ptrs
    };
#undef gx_dc_type_pattern
const gx_device_color_procs _ds *gx_dc_type_pattern = &gx_dc_pattern;
#define gx_dc_type_pattern (&gx_dc_pattern)
/* GC procedures */
#define cptr ((gx_device_color *)vptr)
private ENUM_PTRS_BEGIN(dc_pattern_enum_ptrs) {
	return dc_masked_enum_ptrs(vptr, size, index - 1, pep);
	}
	case 0:
	{	gx_color_tile *tile = cptr->colors.pattern.p_tile;
		ENUM_RETURN((tile == 0 ? tile : tile - tile->index));
	}
ENUM_PTRS_END
private RELOC_PTRS_BEGIN(dc_pattern_reloc_ptrs) {
	gx_color_tile *tile = cptr->colors.pattern.p_tile;
	if ( tile != 0 )
	  {	uint index = tile->index;
		RELOC_TYPED_OFFSET_PTR(gx_device_color, colors.pattern.p_tile, index);
	  }
	dc_masked_reloc_ptrs(vptr, size, gcst);
} RELOC_PTRS_END
private ENUM_PTRS_BEGIN(dc_masked_enum_ptrs) return 0;
	case 0:
	{	gx_color_tile *mask = cptr->mask;
		ENUM_RETURN((mask == 0 ? mask : mask - mask->index));
	}
ENUM_PTRS_END
private RELOC_PTRS_BEGIN(dc_masked_reloc_ptrs) {
	gx_color_tile *mask = cptr->mask;
	if ( mask != 0 )
	  {	uint index = mask->index;
		RELOC_TYPED_OFFSET_PTR(gx_device_color, mask, index);
	  }
} RELOC_PTRS_END
private ENUM_PTRS_BEGIN(dc_binary_masked_enum_ptrs) {
	return (*gx_dc_procs_ht_binary.enum_ptrs)(vptr, size, index - 1, pep);
	}
	case 0:
	  return dc_masked_enum_ptrs(vptr, size, index, pep);
ENUM_PTRS_END
private RELOC_PTRS_BEGIN(dc_binary_masked_reloc_ptrs) {
	dc_masked_reloc_ptrs(vptr, size, gcst);
	(*gx_dc_procs_ht_binary.reloc_ptrs)(vptr, size, gcst);
} RELOC_PTRS_END
#undef cptr

/* Macros for pattern loading */
private int near pattern_load(P2(gx_device_color *, const gs_state *));
#define FINISH_PATTERN_LOAD\
	while ( !gx_pattern_cache_lookup(pdevc, pgs) )\
	 { code = pattern_load(pdevc, pgs);\
	   if ( code < 0 ) break;\
	 }\
	return code;

/* Ensure that a colored Pattern is loaded in the cache. */
private int
gx_dc_pattern_load(gx_device_color *pdevc, const gs_state *pgs)
{	int code = 0;
	FINISH_PATTERN_LOAD
}
/* Ensure that an uncolored Pattern is loaded in the cache. */
private int
gx_dc_pure_masked_load(gx_device_color *pdevc, const gs_state *pgs)
{	int code = (*gx_dc_procs_pure.load)(pdevc, pgs);
	if ( code < 0 )
	  return code;
	FINISH_PATTERN_LOAD
}
private int
gx_dc_binary_masked_load(gx_device_color *pdevc, const gs_state *pgs)
{	int code = (*gx_dc_procs_ht_binary.load)(pdevc, pgs);
	if ( code < 0 )
	  return code;
	FINISH_PATTERN_LOAD
}
private int
gx_dc_colored_masked_load(gx_device_color *pdevc, const gs_state *pgs)
{	int code = (*gx_dc_procs_ht_colored.load)(pdevc, pgs);
	if ( code < 0 )
	  return code;
	FINISH_PATTERN_LOAD
}

/* Look up a pattern color in the cache. */
bool
gx_pattern_cache_lookup(gx_device_color *pdevc, const gs_state *pgs)
{	gx_pattern_cache *pcache = pgs->pattern_cache;
	gx_bitmap_id id = pdevc->id;
	if ( id == gx_no_bitmap_id )
	  {	color_set_null_pattern(pdevc);
		return true;
	  }
	if ( pcache != 0 )
	  { gx_color_tile *ctile = &pcache->tiles[id % pcache->num_tiles];
	    if ( ctile->id == id &&
		(pdevc->type != &gx_dc_pattern ||
		 ctile->depth == gs_currentdevice_inline(pgs)->color_info.depth)
	       )
	      { if ( pdevc->type == &gx_dc_pattern ) /* colored */
		  { pdevc->colors.pattern.p_tile = ctile;
		    color_set_phase_mod(pdevc, pgs->ht_phase.x,
					pgs->ht_phase.y,
					ctile->tbits.rep_width,
					ctile->tbits.rep_height);
		  }
		pdevc->mask =
		  (ctile->tmask.data == 0 ? (gx_color_tile *)0 :
		   ctile);
		return true;
	      }
	  }
	return false;
}
/* Remap the current (Pattern) color before trying the cache again. */
private int near
pattern_load(gx_device_color *pdevc, const gs_state *pgs)
{	const gs_color_space *pcs = pgs->color_space;
	/****** pgs->ccolor IS WRONG ******/
	return (*pcs->type->remap_color)(pgs->ccolor, pcs, pdevc, pgs);
}

#undef FINISH_PATTERN_LOAD

/* Macros for filling with a possibly masked pattern. */
/****** PHASE IS WRONG HERE ******/
#define BEGIN_PATTERN_FILL\
	  {	gx_device_tile_clip cdev;\
		gx_device *pcdev;\
		int code;\
		if ( pdevc->mask == 0 )	/* no clipping */\
		  {	code = 0;\
			pcdev = dev;\
		  }\
		else\
		  {	code = tile_clip_initialize(&cdev,\
					  &pdevc->mask->tmask, dev,\
					  0, 0);\
			if ( code < 0 )\
			  return code;\
			pcdev = (gx_device *)&cdev;\
		  }
#define CLIPPING_FILL (pcdev == (gx_device *)&cdev)
#define END_PATTERN_FILL\
		return code;\
	  }

/* Macros for filling with non-standard X and Y stepping. */
/* Free variables: x, y, w, h, ptile. */
/* tbits_or_tmask is whichever of tbits and tmask is supplying */
/* the tile size. */
/* This implementation could be sped up considerably! */
#define BEGIN_STEPS(tbits_or_tmask)\
	  {	gs_rect bbox, ibbox;\
		gs_point offset;\
		int x0 = x, x1 = x + w, y0 = y, y1 = y + h;\
		int w0 = w, h0 = h;\
		int i0, i1, j0, j1, i, j;\
\
		bbox.p.x = x0, bbox.p.y = y0;\
		bbox.q.x = x1, bbox.q.y = y1;\
		gs_bbox_transform_inverse(&bbox, &ptile->matrix, &ibbox);\
		if_debug10('T',\
			  "[T]x,y=(%d,%d) w,h=(%d,%d) => (%g,%g),(%g,%g), offset=(%g,%g)\n",\
			  x, y, w, h,\
			  ibbox.p.x, ibbox.p.y, ibbox.q.x, ibbox.q.y,\
			  ptile->offset.x, ptile->offset.y);\
		offset.x = ptile->matrix.tx + ptile->offset.x;\
		offset.y = ptile->matrix.ty + ptile->offset.y;\
		i0 = (int)(ibbox.p.x - ptile->bbox.q.x);\
		i1 = (int)ceil(ibbox.q.x - ptile->bbox.p.x);\
		j0 = (int)(ibbox.p.y - ptile->bbox.q.y);\
		j1 = (int)ceil(ibbox.q.y - ptile->bbox.p.y);\
		if_debug4('T', "[T]i=(%d,%d) j=(%d,%d)\n", i0, i1, j0, j1);\
		for ( i = i0; i < i1; i++ )\
		  for ( j = j0; j < j1; j++ )\
		  {	int xoff, yoff;\
			x = (int)(ptile->matrix.xx * i +\
				  ptile->matrix.yx * j + offset.x);\
			y = (int)(ptile->matrix.xy * i +\
				  ptile->matrix.yy * j + offset.y);\
			if_debug4('T', "[T]i=%d j=%d x,y=(%d,%d)", i, j, x, y);\
			w = ptile->tbits_or_tmask.size.x;\
			h = ptile->tbits_or_tmask.size.y;\
			if ( x < x0 ) xoff = x0 - x, x = x0, w -= xoff;\
			else xoff = 0;\
			if ( y < y0 ) yoff = y0 - y, y = y0, h -= yoff;\
			else yoff = 0;\
			if ( x + w > x1 ) w = x1 - x;\
			if ( y + h > y1 ) h = y1 - y;\
			if_debug4('T', "=>(%d,%d) w,h=(%d,%d)\n",\
				  x, y, w, h);\
			if ( w > 0 && h > 0 )\
			  {	if ( CLIPPING_FILL )\
				  tile_clip_set_phase(&cdev, xoff-x, yoff-y)
#define SOURCE_STEP(src)\
				(src).sdata = source->sdata + (y - y0) * source->sraster;\
				(src).sourcex = source->sourcex + (x - x0);\
				(src).sraster = source->sraster;\
				(src).id = (w == w0 && h == h0 ?\
					    source->id : gx_no_bitmap_id);\
				(src).scolors[0] = source->scolors[0];\
				(src).scolors[1] = source->scolors[1];\
				(src).use_scolors = source->use_scolors
#define END_STEPS\
			  }\
		  }\
	  }

/* Fill a rectangle with a colored Pattern. */
/* Note that we treat this as "texture" for RasterOp. */
private int
gx_dc_pattern_fill_rectangle(const gx_device_color *pdevc, int x, int y,
  int w, int h, gx_device *dev, gs_logical_operation_t lop,
  const gx_rop_source_t *source)
{	gx_color_tile *ptile = pdevc->colors.pattern.p_tile;
	const gx_rop_source_t *rop_source =
	  (source == NULL ? &gx_rop_no_source : source);
	gx_strip_bitmap *bits;

	if ( ptile == 0 )	/* null pattern */
	  return 0;
	bits = &ptile->tbits;
	/****** PHASE IS WRONG HERE ******/
	BEGIN_PATTERN_FILL
	if ( ptile->is_simple )
	  {	if ( source == NULL && lop_no_S_is_T(lop) )
		  code = (*dev_proc(pcdev, strip_tile_rectangle))(pcdev, bits,
			x, y, w, h,
			gx_no_color_index, gx_no_color_index,
			pdevc->phase.x, pdevc->phase.y);
		else
		  code = (*dev_proc(dev, strip_copy_rop))(dev,
			rop_source->sdata, rop_source->sourcex,
			rop_source->sraster, rop_source->id,
			(rop_source->use_scolors ? rop_source->scolors : NULL),
			bits, NULL,
			x, y, w, h, pdevc->phase.x, pdevc->phase.y, lop);
	  }
	else
	  {	BEGIN_STEPS(tbits);
		  { const byte *data = bits->data + bits->raster * yoff;
		    bool full_transfer = (w == w0 && h == h0);
		    gx_bitmap_id id =
		      (full_transfer ? bits->id : gx_no_bitmap_id);
		      
		    if ( source == NULL && lop_no_S_is_T(lop) )
		      code = (*dev_proc(pcdev, copy_color))(pcdev,
				data, xoff, bits->raster, id,
				x, y, w, h);
		    else
		      { /* We pass the partial pattern as the 'texture' */
			/* even though it won't get repeated. */
			gx_strip_bitmap data_tile;

			data_tile.data = (byte *)data;	/* actually const */
			data_tile.raster = bits->raster;
			data_tile.size.x = data_tile.rep_width = xoff + w;
			data_tile.size.y = data_tile.rep_height = h;
			data_tile.id = id;
			/****** FOLLOWING IS WRONG ******/
			data_tile.shift = data_tile.rep_shift = 0;
			code = (*dev_proc(dev, strip_copy_rop))(dev,
				rop_source->sdata + (y - y0) * rop_source->sraster,
				rop_source->sourcex + (x - x0),
				rop_source->sraster,
				(full_transfer ? rop_source->id :
				 gx_no_bitmap_id),
				(rop_source->use_scolors ?
				 rop_source->scolors : NULL),
				&data_tile, NULL,
				x, y, w, h, xoff, 0, lop);
		      }
		    if ( code < 0 )
		      return code;
		  }
		END_STEPS
	  }
	END_PATTERN_FILL
}
/* Fill a rectangle with an uncolored Pattern. */
/* Note that we treat this as "texture" for RasterOp. */
private int
gx_dc_pure_masked_fill_rect(const gx_device_color *pdevc, int x, int y,
  int w, int h, gx_device *dev, gs_logical_operation_t lop,
  const gx_rop_source_t *source)
{	gx_color_tile *ptile = pdevc->mask;

	BEGIN_PATTERN_FILL
	if ( ptile == 0 || ptile->is_simple )
		code = (*gx_dc_procs_pure.fill_rectangle)(pdevc, x, y, w, h,
					pcdev, lop, source);
	else
	  {	BEGIN_STEPS(tmask);
		if ( source == NULL )
		  code = (*gx_dc_procs_pure.fill_rectangle)(pdevc, x, y, w, h,
					pcdev, lop, source);
		else
		  { gx_rop_source_t step_source;
		    SOURCE_STEP(step_source);
		    code = (*gx_dc_procs_pure.fill_rectangle)(pdevc,
					x, y, w, h,
					pcdev, lop, &step_source);
		  }
		if ( code < 0 )
		  return code;
		END_STEPS
	  }
	END_PATTERN_FILL
}
private int
gx_dc_binary_masked_fill_rect(const gx_device_color *pdevc, int x, int y,
  int w, int h, gx_device *dev, gs_logical_operation_t lop,
  const gx_rop_source_t *source)
{	gx_color_tile *ptile = pdevc->mask;

	BEGIN_PATTERN_FILL
	if ( ptile == 0 || ptile->is_simple )
		code = (*gx_dc_procs_ht_binary.fill_rectangle)(pdevc,
					x, y, w, h,
					pcdev, lop, source);
	else
	  {	BEGIN_STEPS(tmask);
		if ( source == NULL )
		  code = (*gx_dc_procs_ht_binary.fill_rectangle)(pdevc,
					x, y, w, h,
					pcdev, lop, source);
		else
		  { gx_rop_source_t step_source;
		    SOURCE_STEP(step_source);
		    code = (*gx_dc_procs_ht_binary.fill_rectangle)(pdevc,
					x, y, w, h,
					pcdev, lop, &step_source);
		  }
		if ( code < 0 )
		  return code;
		END_STEPS
	  }
	END_PATTERN_FILL
}
private int
gx_dc_colored_masked_fill_rect(const gx_device_color *pdevc, int x, int y,
  int w, int h, gx_device *dev, gs_logical_operation_t lop,
  const gx_rop_source_t *source)
{	gx_color_tile *ptile = pdevc->mask;

	BEGIN_PATTERN_FILL
	if ( ptile == 0 || ptile->is_simple )
		code = (*gx_dc_procs_ht_colored.fill_rectangle)(pdevc,
					x, y, w, h,
					pcdev, lop, source);
	else
	  {	BEGIN_STEPS(tmask);
		if ( source == NULL )
		  code = (*gx_dc_procs_ht_colored.fill_rectangle)(pdevc,
					x, y, w, h,
					pcdev, lop, source);
		else
		  { gx_rop_source_t step_source;
		    SOURCE_STEP(step_source);
		    code = (*gx_dc_procs_ht_colored.fill_rectangle)(pdevc,
					x, y, w, h,
					pcdev, lop, &step_source);
		  }
		if ( code < 0 )
		  return code;
		END_STEPS
	  }
	END_PATTERN_FILL
}

#undef BEGIN_PATTERN_FILL
#undef END_PATTERN_FILL

/* Initialize a Pattern color. */
private void
gx_init_Pattern(gs_client_color *pcc, const gs_color_space *pcs)
{	if ( pcs->params.pattern.has_base_space )
	{	const gs_color_space *pbcs =
		  (const gs_color_space *)&pcs->params.pattern.base_space;
		cs_init_color(pcc, pbcs);
	}
	/*pcc->pattern = 0;*/		/* cs_full_init_color handles this */
}

/* Install a Pattern color space. */
private int
gx_install_Pattern(gs_color_space *pcs, gs_state *pgs)
{	if ( !pcs->params.pattern.has_base_space )
		return 0;
	return (*pcs->params.pattern.base_space.type->install_cspace)
		((gs_color_space *)&pcs->params.pattern.base_space, pgs);
}

/* Adjust the reference counts for Pattern color spaces or colors. */
private void
gx_adjust_cspace_Pattern(const gs_color_space *pcs, gs_state *pgs, int delta)
{	if ( pcs->params.pattern.has_base_space )
		(*pcs->params.pattern.base_space.type->adjust_cspace_count)
		 ((const gs_color_space *)&pcs->params.pattern.base_space, pgs, delta);
}

private void
gx_adjust_color_Pattern(const gs_client_color *pcc, const gs_color_space *pcs,
  gs_state *pgs, int delta)
{	gs_pattern_instance *pinst = pcc->pattern;
	if ( pinst != 0 && (pinst->rc.ref_count += delta) == 0 )
	{	/* Release all the storage associated with the instance. */
		gs_state *saved = pinst->saved;
		gs_state_free(saved);
		gs_free_object(saved->memory, pinst,
			       "gx_adjust_color_Pattern");
	}
	if ( pcs->params.pattern.has_base_space )
		(*pcs->params.pattern.base_space.type->adjust_color_count)
		 (pcc, (const gs_color_space *)&pcs->params.pattern.base_space,
		  pgs, delta);
}

/* GC procedures */

#define pcs ((gs_color_space *)vptr)

private ENUM_PTRS_BEGIN_PROC(gx_enum_ptrs_Pattern) {
	if ( !pcs->params.pattern.has_base_space )
	  return 0;
	return (*pcs->params.pattern.base_space.type->enum_ptrs)
		 (&pcs->params.pattern.base_space,
		  sizeof(pcs->params.pattern.base_space), index, pep);
} ENUM_PTRS_END_PROC
private RELOC_PTRS_BEGIN(gx_reloc_ptrs_Pattern) {
	if ( !pcs->params.pattern.has_base_space )
	  return;
	(*pcs->params.pattern.base_space.type->reloc_ptrs)
	  (&pcs->params.pattern.base_space, sizeof(gs_paint_color_space), gcst);
} RELOC_PTRS_END

#undef pcs
