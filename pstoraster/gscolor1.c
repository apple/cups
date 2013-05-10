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
/* Level 1 extended color operators for Ghostscript library */
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
#include "gscolor1.h"

/* Imports from gscolor.c */
void load_transfer_map(P3(gs_state *, gx_transfer_map *, floatp));

/* Imported from gsht.c */
void gx_set_effective_transfer(P1(gs_state *));

/* Define the CMYK color space type. */
extern cs_proc_remap_color(gx_remap_DeviceCMYK);
extern cs_proc_concretize_color(gx_concretize_DeviceCMYK);
extern cs_proc_remap_concrete_color(gx_remap_concrete_DCMYK);
const gs_color_space_type
	gs_color_space_type_DeviceCMYK =
	 { gs_color_space_index_DeviceCMYK, 4, true,
	   gx_init_paint_4, gx_same_concrete_space,
	   gx_concretize_DeviceCMYK, gx_remap_concrete_DCMYK,
	   gx_remap_DeviceCMYK, gx_no_install_cspace,
	   gx_no_adjust_cspace_count, gx_no_adjust_color_count,
	   gx_no_cspace_enum_ptrs, gx_no_cspace_reloc_ptrs
	 };

/* Initialize the permanent color space instance (declared in gscolor.c). */
extern const gs_color_space *gs_cs_static_DeviceCMYK;
void
gs_gscolor1_init(gs_memory_t *mem)
{	gs_color_space *pcs =
	  gs_alloc_struct(mem, gs_color_space, &st_color_space,
			  "gs_cs_static_DeviceCMYK");
	pcs->type = &gs_color_space_type_DeviceCMYK;
	gs_cs_static_DeviceCMYK = pcs;
}

/* setcmykcolor */
int
gs_setcmykcolor(gs_state *pgs, floatp c, floatp m, floatp y, floatp k)
{	gs_client_color *pcc = pgs->ccolor;
	if ( pgs->in_cachedevice ) return_error(gs_error_undefined);
	cs_adjust_counts(pgs, -1);
	pcc->paint.values[0] = c;
	pcc->paint.values[1] = m;
	pcc->paint.values[2] = y;
	pcc->paint.values[3] = k;
	pcc->pattern = 0;			/* for GC */
	pgs->color_space->type = &gs_color_space_type_DeviceCMYK;
	gx_unset_dev_color(pgs);
	return 0;
}

/* currentcmykcolor */
int
gs_currentcmykcolor(const gs_state *pgs, float pr4[4])
{	const gs_client_color *pcc = pgs->ccolor;
	switch ( pgs->color_space->type->index )
	{
	case gs_color_space_index_DeviceGray:
		pr4[0] = pr4[1] = pr4[2] = 0.0;
		pr4[3] = 1.0 - pcc->paint.values[0];
		break;
	case gs_color_space_index_DeviceRGB:
	{	frac fcmyk[4];
		color_rgb_to_cmyk(
			float2frac(pcc->paint.values[0]),
			float2frac(pcc->paint.values[1]),
			float2frac(pcc->paint.values[2]),
			pgs, fcmyk);
		pr4[0] = frac2float(fcmyk[0]);
		pr4[1] = frac2float(fcmyk[1]);
		pr4[2] = frac2float(fcmyk[2]);
		pr4[3] = frac2float(fcmyk[3]);
	}	break;
	case gs_color_space_index_DeviceCMYK:
		pr4[0] = pcc->paint.values[0];
		pr4[1] = pcc->paint.values[1];
		pr4[2] = pcc->paint.values[2];
		pr4[3] = pcc->paint.values[3];
		break;
	default:
		pr4[0] = pr4[1] = pr4[2] = 0.0;
		pr4[3] = 1.0;
	}
	return 0;
}

/* setblackgeneration */
/* Remap=0 is used by the interpreter. */
int
gs_setblackgeneration(gs_state *pgs, gs_mapping_proc proc)
{	return gs_setblackgeneration_remap(pgs, proc, true);
}
int
gs_setblackgeneration_remap(gs_state *pgs, gs_mapping_proc proc, bool remap)
{	rc_unshare_struct(pgs->black_generation, gx_transfer_map,
			  &st_transfer_map, pgs->memory,
			  return_error(gs_error_VMerror),
			  "gs_setblackgeneration");
	pgs->black_generation->proc = proc;
	pgs->black_generation->id = gs_next_ids(1);
	if ( remap )
	{	load_transfer_map(pgs, pgs->black_generation, 0.0);
		gx_unset_dev_color(pgs);
	}
	return 0;
}

/* currentblackgeneration */
gs_mapping_proc
gs_currentblackgeneration(const gs_state *pgs)
{	return pgs->black_generation->proc;
}

/* setundercolorremoval */
/* Remap=0 is used by the interpreter. */
int
gs_setundercolorremoval(gs_state *pgs, gs_mapping_proc proc)
{	return gs_setundercolorremoval_remap(pgs, proc, true);
}
int
gs_setundercolorremoval_remap(gs_state *pgs, gs_mapping_proc proc, bool remap)
{	rc_unshare_struct(pgs->undercolor_removal, gx_transfer_map,
			  &st_transfer_map, pgs->memory,
			  return_error(gs_error_VMerror),
			  "gs_setundercolorremoval");
	pgs->undercolor_removal->proc = proc;
	pgs->undercolor_removal->id = gs_next_ids(1);
	if ( remap )
	{	load_transfer_map(pgs, pgs->undercolor_removal, -1.0);
		gx_unset_dev_color(pgs);
	}
	return 0;
}

/* currentundercolorremoval */
gs_mapping_proc
gs_currentundercolorremoval(const gs_state *pgs)
{	return pgs->undercolor_removal->proc;
}

/* setcolortransfer */
/* Remap=0 is used by the interpreter. */
int
gs_setcolortransfer_remap(gs_state *pgs, gs_mapping_proc red_proc,
  gs_mapping_proc green_proc, gs_mapping_proc blue_proc,
  gs_mapping_proc gray_proc, bool remap)
{	gx_transfer_colored *ptran = &pgs->set_transfer.colored;
	gx_transfer_colored old;
	gs_id new_ids = gs_next_ids(4);

	old = *ptran;
	rc_unshare_struct(ptran->gray, gx_transfer_map, &st_transfer_map,
			  pgs->memory, goto fgray, "gs_setcolortransfer");
	rc_unshare_struct(ptran->red, gx_transfer_map, &st_transfer_map,
			  pgs->memory, goto fred, "gs_setcolortransfer");
	rc_unshare_struct(ptran->green, gx_transfer_map, &st_transfer_map,
			  pgs->memory, goto fgreen, "gs_setcolortransfer");
	rc_unshare_struct(ptran->blue, gx_transfer_map, &st_transfer_map,
			  pgs->memory, goto fblue, "gs_setcolortransfer");
	ptran->gray->proc = gray_proc;
	ptran->gray->id = new_ids;
	ptran->red->proc = red_proc;
	ptran->red->id = new_ids + 1;
	ptran->green->proc = green_proc;
	ptran->green->id = new_ids + 2;
	ptran->blue->proc = blue_proc;
	ptran->blue->id = new_ids + 3;
	if ( remap )
	{	load_transfer_map(pgs, ptran->red, 0.0);
		load_transfer_map(pgs, ptran->green, 0.0);
		load_transfer_map(pgs, ptran->blue, 0.0);
		load_transfer_map(pgs, ptran->gray, 0.0);
		gx_set_effective_transfer(pgs);
		gx_unset_dev_color(pgs);
	}
	return 0;
fblue:	rc_assign(ptran->green, old.green, pgs->memory, "setcolortransfer");
fgreen:	rc_assign(ptran->red, old.red, pgs->memory, "setcolortransfer");
fred:	rc_assign(ptran->gray, old.gray, pgs->memory, "setcolortransfer");
fgray:	return_error(gs_error_VMerror);
}
int
gs_setcolortransfer(gs_state *pgs, gs_mapping_proc red_proc,
  gs_mapping_proc green_proc, gs_mapping_proc blue_proc,
  gs_mapping_proc gray_proc)
{	return gs_setcolortransfer_remap(pgs, red_proc, green_proc,
					 blue_proc, gray_proc, true);
}

/* currentcolortransfer */
void
gs_currentcolortransfer(const gs_state *pgs, gs_mapping_proc procs[4])
{	const gx_transfer_colored *ptran = &pgs->set_transfer.colored;
	procs[0] = ptran->red->proc;
	procs[1] = ptran->green->proc;
	procs[2] = ptran->blue->proc;
	procs[3] = ptran->gray->proc;
}
