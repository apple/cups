/* Copyright (C) 1989, 1992, 1993, 1994, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gscolor.c */
/* Color and halftone operators for Ghostscript library */
#include "gx.h"
#include "gserrors.h"
#include "gsstruct.h"
#include "gsutil.h"			/* for gs_next_ids */
#include "gsccolor.h"
#include "gxcspace.h"
#include "gxdcconv.h"
#include "gxdevice.h"			/* for gx_color_index */
#include "gxcmap.h"
#include "gzstate.h"

/* Imported from gsht.c */
void gx_set_effective_transfer(P1(gs_state *));

/* Define the standard color space types. */
extern cs_proc_remap_color(gx_remap_DeviceGray);
extern cs_proc_concretize_color(gx_concretize_DeviceGray);
extern cs_proc_remap_concrete_color(gx_remap_concrete_DGray);
extern cs_proc_remap_color(gx_remap_DeviceRGB);
extern cs_proc_concretize_color(gx_concretize_DeviceRGB);
extern cs_proc_remap_concrete_color(gx_remap_concrete_DRGB);
const gs_color_space_type
	gs_color_space_type_DeviceGray =
	 { gs_color_space_index_DeviceGray, 1, true,
	   gx_init_paint_1, gx_same_concrete_space,
	   gx_concretize_DeviceGray, gx_remap_concrete_DGray,
	   gx_remap_DeviceGray, gx_no_install_cspace,
	   gx_no_adjust_cspace_count, gx_no_adjust_color_count,
	   gx_no_cspace_enum_ptrs, gx_no_cspace_reloc_ptrs
	 },
	gs_color_space_type_DeviceRGB =
	 { gs_color_space_index_DeviceRGB, 3, true,
	   gx_init_paint_3, gx_same_concrete_space,
	   gx_concretize_DeviceRGB, gx_remap_concrete_DRGB,
	   gx_remap_DeviceRGB, gx_no_install_cspace,
	   gx_no_adjust_cspace_count, gx_no_adjust_color_count,
	   gx_no_cspace_enum_ptrs, gx_no_cspace_reloc_ptrs
	 };

/* Structure descriptors */
public_st_color_space();
public_st_client_color();

/*
 * Define permanent instances of the 3 device color spaces.
 * The one for CMYK is initialized by gscolor1.c and may be NULL.
 */
const gs_color_space *gs_cs_static_DeviceGray;
private gs_gc_root_t cs_gray_root;
const gs_color_space *gs_cs_static_DeviceRGB;
private gs_gc_root_t cs_rgb_root;
const gs_color_space *gs_cs_static_DeviceCMYK = 0;
private gs_gc_root_t cs_cmyk_root;
void
gs_gscolor_init(gs_memory_t *mem)
{
#define cs_init(cs_var, cs_type, cname)\
  { gs_color_space *pcs =\
      gs_alloc_struct(mem, gs_color_space, &st_color_space, cname);\
    pcs->type = &cs_type;\
    cs_var = pcs;\
  }
	cs_init(gs_cs_static_DeviceGray, gs_color_space_type_DeviceGray,
		"gs_cs_static_DeviceGray");
	gs_register_struct_root(mem, &cs_gray_root,
				(void **)&gs_cs_static_DeviceGray,
				"gs_cs_static_DeviceGray");
	cs_init(gs_cs_static_DeviceRGB, gs_color_space_type_DeviceRGB,
		"gs_cs_static_DeviceRGB");
	gs_register_struct_root(mem, &cs_rgb_root,
				(void **)&gs_cs_static_DeviceRGB,
				"gs_cs_static_DeviceRGB");
	gs_register_struct_root(mem, &cs_cmyk_root,
				(void **)&gs_cs_static_DeviceCMYK,
				"gs_cs_static_DeviceCMYK");
#undef cs_init
}
const gs_color_space *
gs_color_space_DeviceGray(void)
{	return gs_cs_static_DeviceGray;
}
const gs_color_space *
gs_color_space_DeviceRGB(void)
{	return gs_cs_static_DeviceRGB;
}
const gs_color_space *
gs_color_space_DeviceCMYK(void)
{	return gs_cs_static_DeviceCMYK;
}

/* Get the index of a color space. */
gs_color_space_index
gs_color_space_get_index(const gs_color_space *pcs)
{	return pcs->type->index;
}

/* Get the number of components in a color space. */
int
gs_color_space_num_components(const gs_color_space *pcs)
{	return pcs->type->num_components;
}

/* Get the base space of an Indexed color space. */
const gs_color_space *
gs_color_space_indexed_base_space(const gs_color_space *pcs)
{	return (const gs_color_space *)&pcs->params.indexed.base_space;
}

/* Initialize colors with 1, or 3, or 4 paint components. */
/* (These are only used by setcolorspace.) */
void
gx_init_paint_1(gs_client_color *pcc, const gs_color_space *pcs)
{	pcc->paint.values[0] = 0.0;
}
void
gx_init_paint_3(gs_client_color *pcc, const gs_color_space *pcs)
{	pcc->paint.values[2] = 0.0;
	pcc->paint.values[1] = 0.0;
	pcc->paint.values[0] = 0.0;
}
void
gx_init_paint_4(gs_client_color *pcc, const gs_color_space *pcs)
{	/* DeviceCMYK and CIEBasedDEFG spaces initialize to 0,0,0,1. */
	pcc->paint.values[3] = 1.0;
	gx_init_paint_3(pcc, pcs);
}

/* Null color space installation procedure. */
int
gx_no_install_cspace(gs_color_space *pcs, gs_state *pgs)
{	return 0;
}

/* Null reference count adjustment procedures. */
void
gx_no_adjust_cspace_count(const gs_color_space *pcs, gs_state *pgs, int delta)
{
}
void
gx_no_adjust_color_count(const gs_client_color *pcc, const gs_color_space *pcs, gs_state *pgs, int delta)
{
}

/* GC procedures */

#define pcs ((gs_color_space *)vptr)

private ENUM_PTRS_BEGIN_PROC(color_space_enum_ptrs) {
	return (*pcs->type->enum_ptrs)(pcs, size, index, pep);
ENUM_PTRS_END_PROC }
private RELOC_PTRS_BEGIN(color_space_reloc_ptrs) {
	(*pcs->type->reloc_ptrs)(pcs, size, gcst);
} RELOC_PTRS_END

#undef pcs

/* Force a parameter into the range [0.0..1.0]. */
#define force_unit(p) (p < 0.0 ? 0.0 : p > 1.0 ? 1.0 : p)

/* Forward declarations */
void load_transfer_map(P3(gs_state *, gx_transfer_map *, floatp));

/* setgray */
int
gs_setgray(gs_state *pgs, floatp gray)
{	if ( pgs->in_cachedevice ) return_error(gs_error_undefined);
	cs_adjust_counts(pgs, -1);
	pgs->ccolor->paint.values[0] = gray;
	pgs->color_space->type = &gs_color_space_type_DeviceGray;
	gx_unset_dev_color(pgs);
	return 0;
}

/* currentgray */
float
gs_currentgray(const gs_state *pgs)
{	const gs_client_color *pcc = pgs->ccolor;
	switch ( pgs->color_space->type->index )
	{
	case gs_color_space_index_DeviceGray:
		return pcc->paint.values[0];
	case gs_color_space_index_DeviceRGB:
		return frac2float(color_rgb_to_gray(
			float2frac(pcc->paint.values[0]),
			float2frac(pcc->paint.values[1]),
			float2frac(pcc->paint.values[2]),
			pgs));
	case gs_color_space_index_DeviceCMYK:
		return frac2float(color_cmyk_to_gray(
			float2frac(pcc->paint.values[0]),
			float2frac(pcc->paint.values[1]),
			float2frac(pcc->paint.values[2]),
			float2frac(pcc->paint.values[3]),
			pgs));
	default:
		return 0.0;
	}
}

/* setrgbcolor */
int
gs_setrgbcolor(gs_state *pgs, floatp r, floatp g, floatp b)
{	gs_client_color *pcc = pgs->ccolor;
	if ( pgs->in_cachedevice ) return_error(gs_error_undefined);
	cs_adjust_counts(pgs, -1);
	pcc->paint.values[0] = r;
	pcc->paint.values[1] = g;
	pcc->paint.values[2] = b;
	pcc->pattern = 0;			/* for GC */
	pgs->color_space->type = &gs_color_space_type_DeviceRGB;
	gx_unset_dev_color(pgs);
	return 0;
}

/* currentrgbcolor */
int
gs_currentrgbcolor(const gs_state *pgs, float pr3[3])
{	const gs_client_color *pcc = pgs->ccolor;
	switch ( pgs->color_space->type->index )
	{
	case gs_color_space_index_DeviceGray:
		pr3[0] = pr3[1] = pr3[2] = pcc->paint.values[0];
		break;
	case gs_color_space_index_DeviceRGB:
		pr3[0] = pcc->paint.values[0];
		pr3[1] = pcc->paint.values[1];
		pr3[2] = pcc->paint.values[2];
		break;
	case gs_color_space_index_DeviceCMYK:
	{	frac frgb[3];
		color_cmyk_to_rgb(
			float2frac(pcc->paint.values[0]),
			float2frac(pcc->paint.values[1]),
			float2frac(pcc->paint.values[2]),
			float2frac(pcc->paint.values[3]),
			pgs, frgb);
		pr3[0] = frac2float(frgb[0]);
		pr3[1] = frac2float(frgb[1]);
		pr3[2] = frac2float(frgb[2]);
	}	break;
	default:
		pr3[0] = pr3[1] = pr3[2] = 0.0;
	}
	return 0;
}

/* setalpha */
int
gs_setalpha(gs_state *pgs, floatp alpha)
{	pgs->alpha = (gx_color_value)(force_unit(alpha) * gx_max_color_value);
	gx_unset_dev_color(pgs);
	return 0;
}

/* currentalpha */
float
gs_currentalpha(const gs_state *pgs)
{	return (float)pgs->alpha / gx_max_color_value;
}

/* settransfer */
/* Remap=0 is used by the interpreter. */
int
gs_settransfer(gs_state *pgs, gs_mapping_proc tproc)
{	return gs_settransfer_remap(pgs, tproc, true);
}
int
gs_settransfer_remap(gs_state *pgs, gs_mapping_proc tproc, bool remap)
{	gx_transfer_colored *ptran = &pgs->set_transfer.colored;
	/* We can safely decrement the reference counts */
	/* of the non-gray transfer maps, because */
	/* if any of them get freed, the rc_unshare can't fail. */
	rc_decrement(ptran->red, pgs->memory, "gs_settransfer");
	rc_decrement(ptran->green, pgs->memory, "gs_settransfer");
	rc_decrement(ptran->blue, pgs->memory, "gs_settransfer");
	rc_unshare_struct(ptran->gray,
			  gx_transfer_map, &st_transfer_map,
			  pgs->memory, goto fail, "gs_settransfer");
	ptran->gray->proc = tproc;
	ptran->gray->id = gs_next_ids(1);
	ptran->red = ptran->gray;
	ptran->green = ptran->gray;
	ptran->blue = ptran->gray;
	ptran->gray->rc.ref_count += 3;
	if ( remap )
	{	load_transfer_map(pgs, ptran->gray, 0.0);
		gx_set_effective_transfer(pgs);
		gx_unset_dev_color(pgs);
	}
	return 0;
fail:	rc_increment(ptran->red);
	rc_increment(ptran->green);
	rc_increment(ptran->blue);
	return_error(gs_error_VMerror);
}

/* currenttransfer */
gs_mapping_proc
gs_currenttransfer(const gs_state *pgs)
{	return pgs->set_transfer.colored.gray->proc;
}

/* ------ Non-operator routines ------ */

/* Set device color = 1 for writing into the character cache. */
void
gx_set_device_color_1(gs_state *pgs)
{	gx_device_color *pdc = pgs->dev_color;
	gs_client_color *pcc = pgs->ccolor;
	cs_adjust_counts(pgs, -1);
	pcc->paint.values[0] = 0.0;
	pcc->pattern = 0;			/* for GC */
	pgs->color_space->type = &gs_color_space_type_DeviceGray;
	color_set_pure(pdc, 1);
	pgs->log_op = lop_default;
}

/* ------ Internal routines ------ */

/* Load one cached transfer map. */
/* This is exported for gscolor1.c. */
void
load_transfer_map(gs_state *pgs, gx_transfer_map *pmap, floatp min_value)
{	gs_mapping_proc proc = pmap->proc;
	frac *values = pmap->values;
	frac fmin = float2frac(min_value);
	int i;
	for ( i = 0; i < transfer_map_size; i++ )
	{	float fval =
			(*proc)((float)i / (transfer_map_size - 1), pmap);
		values[i] =
			(fval < min_value ? fmin :
			 fval >= 1.0 ? frac_1 :
			 float2frac(fval));
	}
}
