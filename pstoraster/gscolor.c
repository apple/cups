/* Copyright (C) 1989, 1992, 1993, 1994, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
  This file is part of GNU Ghostscript.
  
  GNU Ghostscript is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY.  No author or distributor accepts responsibility
  to anyone for the consequences of using it or for whether it serves any
  particular purpose or works at all, unless he says so in writing.  Refer
  to the GNU General Public License for full details.
  
  Everyone is granted permission to copy, modify and redistribute GNU
  Ghostscript, but only under the conditions described in the GNU General
  Public License.  A copy of this license is supposed to have been given
  to you along with GNU Ghostscript so you can know your rights and
  responsibilities.  It should be in a file named COPYING.  Among other
  things, the copyright notice and this notice must be preserved on all
  copies.
  
  Aladdin Enterprises supports the work of the GNU Project, but is not
  affiliated with the Free Software Foundation or the GNU Project.  GNU
  Ghostscript, as distributed by Aladdin Enterprises, does not require any
  GNU software to build or run it.
*/

/*$Id: gscolor.c,v 1.2 2000/03/08 23:14:36 mike Exp $ */
/* Color and halftone operators for Ghostscript library */
#include "gx.h"
#include "gserrors.h"
#include "gsstruct.h"
#include "gsutil.h"		/* for gs_next_ids */
#include "gsccolor.h"
#include "gxcspace.h"
#include "gxdcconv.h"
#include "gxdevice.h"		/* for gx_color_index */
#include "gxcmap.h"
#include "gzstate.h"

/* Imported from gsht.c */
void gx_set_effective_transfer(P1(gs_state *));

/* Structure descriptors */
public_st_client_color();
public_st_transfer_map();

/* GC procedures */
#define mptr ((gx_transfer_map *)vptr)
private 
ENUM_PTRS_BEGIN(transfer_map_enum_ptrs) return 0;

case 0:
ENUM_RETURN((mptr->proc == 0 ? mptr->closure.data : 0));
ENUM_PTRS_END
private RELOC_PTRS_BEGIN(transfer_map_reloc_ptrs)
{
    if (mptr->proc == 0)
	RELOC_PTR(gx_transfer_map, closure.data);
}
RELOC_PTRS_END
#undef mptr

/* Initialize colors with 1, or 3, or 4 paint components. */
/* (These are only used by setcolorspace.) */
void
gx_init_paint_1(gs_client_color * pcc, const gs_color_space * pcs)
{
    pcc->paint.values[0] = 0.0;
}
void
gx_init_paint_3(gs_client_color * pcc, const gs_color_space * pcs)
{
    pcc->paint.values[2] = 0.0;
    pcc->paint.values[1] = 0.0;
    pcc->paint.values[0] = 0.0;
}
void
gx_init_paint_4(gs_client_color * pcc, const gs_color_space * pcs)
{
    /* DeviceCMYK and CIEBasedDEFG spaces initialize to 0,0,0,1. */
    pcc->paint.values[3] = 1.0;
    gx_init_paint_3(pcc, pcs);
}

/* Force a value into the range [0.0..1.0]. */
#define FORCE_UNIT(p) (p <= 0.0 ? 0.0 : p >= 1.0 ? 1.0 : p)

/* Restrict colors with 1, 3, or 4 components to the range (0,1). */
void
gx_restrict01_paint_1(gs_client_color * pcc, const gs_color_space * pcs)
{
    pcc->paint.values[0] = FORCE_UNIT(pcc->paint.values[0]);
}
void
gx_restrict01_paint_3(gs_client_color * pcc, const gs_color_space * pcs)
{
    pcc->paint.values[2] = FORCE_UNIT(pcc->paint.values[2]);
    pcc->paint.values[1] = FORCE_UNIT(pcc->paint.values[1]);
    pcc->paint.values[0] = FORCE_UNIT(pcc->paint.values[0]);
}
void
gx_restrict01_paint_4(gs_client_color * pcc, const gs_color_space * pcs)
{
    pcc->paint.values[3] = FORCE_UNIT(pcc->paint.values[3]);
    gx_restrict01_paint_3(pcc, pcs);
}

/* Null reference count adjustment procedure. */
void
gx_no_adjust_color_count(const gs_client_color * pcc,
			 const gs_color_space * pcs, int delta)
{
}

/* Forward declarations */
void load_transfer_map(P3(gs_state *, gx_transfer_map *, floatp));

/* setgray */
int
gs_setgray(gs_state * pgs, floatp gray)
{
    if (pgs->in_cachedevice)
	return_error(gs_error_undefined);
    cs_adjust_counts(pgs, -1);
    pgs->ccolor->paint.values[0] = FORCE_UNIT(gray);
    pgs->color_space->type = &gs_color_space_type_DeviceGray;
    gx_unset_dev_color(pgs);
    return 0;
}

/* currentgray */
float
gs_currentgray(const gs_state * pgs)
{
    const gs_client_color *pcc = pgs->ccolor;
    const gs_imager_state *const pis = (const gs_imager_state *)pgs;

    switch (pgs->color_space->type->index) {
	case gs_color_space_index_DeviceGray:
	    return pcc->paint.values[0];
	case gs_color_space_index_DeviceRGB:
	    return frac2float(
				 color_rgb_to_gray(
					   float2frac(pcc->paint.values[0]),
					   float2frac(pcc->paint.values[1]),
					   float2frac(pcc->paint.values[2]),
						      pis));
	case gs_color_space_index_DeviceCMYK:
	    return frac2float(
				 color_cmyk_to_gray(
					   float2frac(pcc->paint.values[0]),
					   float2frac(pcc->paint.values[1]),
					   float2frac(pcc->paint.values[2]),
					   float2frac(pcc->paint.values[3]),
						       pis));
	default:
	    /*
	     * Might be another convertible color space, but this is rare,
	     * so we don't care about speed or (to some extent) accuracy.
	     */
	    {
		float rgb[3];

		gs_currentrgbcolor(pgs, rgb);
		return frac2float(
				     color_rgb_to_gray(
		 float2frac(rgb[0]), float2frac(rgb[1]), float2frac(rgb[2]),
							  pis));
	    }
    }
}

/* setrgbcolor */
int
gs_setrgbcolor(gs_state * pgs, floatp r, floatp g, floatp b)
{
    gs_client_color *pcc = pgs->ccolor;

    if (pgs->in_cachedevice)
	return_error(gs_error_undefined);
    cs_adjust_counts(pgs, -1);
    pcc->paint.values[0] = FORCE_UNIT(r);
    pcc->paint.values[1] = FORCE_UNIT(g);
    pcc->paint.values[2] = FORCE_UNIT(b);
    pcc->pattern = 0;		/* for GC */
    pgs->color_space->type = &gs_color_space_type_DeviceRGB;
    gx_unset_dev_color(pgs);
    return 0;
}

/* currentrgbcolor */
int
gs_currentrgbcolor(const gs_state * pgs, float pr3[3])
{
    const gs_client_color *pcc = pgs->ccolor;
    const gs_color_space *pcs = pgs->color_space;
    const gs_color_space *pbcs = pcs;
    const gs_imager_state *const pis = (const gs_imager_state *)pgs;
    frac fcc[4];
    gs_client_color cc;

  sw:switch (pbcs->type->index) {
	case gs_color_space_index_DeviceGray:
	    pr3[0] = pr3[1] = pr3[2] = pcc->paint.values[0];
	    return 0;
	case gs_color_space_index_DeviceRGB:
	    pr3[0] = pcc->paint.values[0];
	    pr3[1] = pcc->paint.values[1];
	    pr3[2] = pcc->paint.values[2];
	    return 0;
	case gs_color_space_index_DeviceCMYK:
	    color_cmyk_to_rgb(
				 float2frac(pcc->paint.values[0]),
				 float2frac(pcc->paint.values[1]),
				 float2frac(pcc->paint.values[2]),
				 float2frac(pcc->paint.values[3]),
				 pis, fcc);
	    pr3[0] = frac2float(fcc[0]);
	    pr3[1] = frac2float(fcc[1]);
	    pr3[2] = frac2float(fcc[2]);
	    return 0;
	case gs_color_space_index_DeviceN:
	case gs_color_space_index_Separation:
	  ds:if (cs_concrete_space(pbcs, pis) == pbcs)
		break;		/* not using alternative space */
	    /* (falls through) */
	case gs_color_space_index_Indexed:
	    pbcs = gs_cspace_base_space(pbcs);
	    switch (pbcs->type->index) {
		case gs_color_space_index_DeviceN:
		case gs_color_space_index_Separation:
		    goto ds;
		default:	/* outer switch will catch undefined cases */
		    break;
	    }
	    if (cs_concretize_color(pcc, pcs, fcc, pis) < 0)
		break;
	    cc.paint.values[0] = frac2float(fcc[0]);
	    cc.paint.values[1] = frac2float(fcc[1]);
	    cc.paint.values[2] = frac2float(fcc[2]);
	    cc.paint.values[3] = frac2float(fcc[3]);
	    pcc = &cc;
	    pcs = pbcs;
	    goto sw;
	default:
	    break;
    }
    pr3[0] = pr3[1] = pr3[2] = 0.0;
    return 0;
}

/* setnullcolor */
int
gs_setnullcolor(gs_state * pgs)
{
    if (pgs->in_cachedevice)
	return_error(gs_error_undefined);
    gs_setgray(pgs, 0.0);	/* set color space to something harmless */
    color_set_null(pgs->dev_color);
    return 0;
}

/* settransfer */
/* Remap=0 is used by the interpreter. */
int
gs_settransfer(gs_state * pgs, gs_mapping_proc tproc)
{
    return gs_settransfer_remap(pgs, tproc, true);
}
int
gs_settransfer_remap(gs_state * pgs, gs_mapping_proc tproc, bool remap)
{
    gx_transfer_colored *ptran = &pgs->set_transfer.colored;

    /*
     * We can safely decrement the reference counts
     * of the non-gray transfer maps, because
     * if any of them get freed, the rc_unshare can't fail.
     */
    rc_decrement(ptran->red, "gs_settransfer");
    rc_decrement(ptran->green, "gs_settransfer");
    rc_decrement(ptran->blue, "gs_settransfer");
    rc_unshare_struct(ptran->gray, gx_transfer_map, &st_transfer_map,
		      pgs->memory, goto fail, "gs_settransfer");
    ptran->gray->proc = tproc;
    ptran->gray->id = gs_next_ids(1);
    ptran->red = ptran->gray;
    ptran->green = ptran->gray;
    ptran->blue = ptran->gray;
    ptran->gray->rc.ref_count += 3;
    if (remap) {
	load_transfer_map(pgs, ptran->gray, 0.0);
	gx_set_effective_transfer(pgs);
	gx_unset_dev_color(pgs);
    }
    return 0;
  fail:
    rc_increment(ptran->red);
    rc_increment(ptran->green);
    rc_increment(ptran->blue);
    return_error(gs_error_VMerror);
}

/* currenttransfer */
gs_mapping_proc
gs_currenttransfer(const gs_state * pgs)
{
    return pgs->set_transfer.colored.gray->proc;
}

/* ------ Non-operator routines ------ */

/* Set device color = 1 for writing into the character cache. */
void
gx_set_device_color_1(gs_state * pgs)
{
    gx_device_color *pdc = pgs->dev_color;
    gs_client_color *pcc = pgs->ccolor;

    cs_adjust_counts(pgs, -1);
    pcc->paint.values[0] = 0.0;
    pcc->pattern = 0;		/* for GC */
    pgs->color_space->type = &gs_color_space_type_DeviceGray;
    color_set_pure(pdc, 1);
    pgs->log_op = lop_default;
}

/* ------ Internal routines ------ */

/*
 * Load one cached transfer map.  We export this for gscolor1.c.
 * Note that we must deal with both old (proc) and new (closure) maps.
 */
private float
transfer_use_proc(floatp value, const gx_transfer_map * pmap,
		  const void *ignore_proc_data)
{
    return (*pmap->proc) (value, pmap);
}
void
load_transfer_map(gs_state * pgs, gx_transfer_map * pmap, floatp min_value)
{
    gs_mapping_closure_proc_t proc =
    (pmap->proc == 0 ? pmap->closure.proc : transfer_use_proc);
    const void *proc_data = pmap->closure.data;
    frac *values = pmap->values;
    frac fmin = float2frac(min_value);
    int i;

    for (i = 0; i < transfer_map_size; i++) {
	float fval =
	(*proc) ((float)i / (transfer_map_size - 1), pmap, proc_data);

	values[i] =
	    (fval < min_value ? fmin :
	     fval >= 1.0 ? frac_1 :
	     float2frac(fval));
    }
}
