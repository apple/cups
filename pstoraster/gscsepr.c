/* Copyright (C) 1994, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gscsepr.c,v 1.2 2000/03/08 23:14:38 mike Exp $ */
/* Separation color space and operation definition */
#include "gx.h"
#include "gserrors.h"
#include "gsrefct.h"
#include "gsmatrix.h"		/* for gscolor2.h */
#include "gscsepr.h"
#include "gxcspace.h"
#include "gxfixed.h"		/* for gxcolor2.h */
#include "gxcolor2.h"		/* for gs_indexed_map */
#include "gzstate.h"		/* for pgs->overprint */

/* ---------------- Color space ---------------- */

gs_private_st_composite(st_color_space_Separation, gs_paint_color_space,
			"gs_color_space_Separation",
			cs_Separation_enum_ptrs, cs_Separation_reloc_ptrs);

/* Define the Separation color space type. */
private cs_proc_base_space(gx_alt_space_Separation);
private cs_proc_init_color(gx_init_Separation);
private cs_proc_concrete_space(gx_concrete_space_Separation);
private cs_proc_concretize_color(gx_concretize_Separation);
private cs_proc_remap_concrete_color(gx_remap_concrete_Separation);
private cs_proc_install_cspace(gx_install_Separation);
private cs_proc_adjust_cspace_count(gx_adjust_cspace_Separation);
const gs_color_space_type gs_color_space_type_Separation = {
    gs_color_space_index_Separation, true, false,
    &st_color_space_Separation, gx_num_components_1,
    gx_alt_space_Separation,
    gx_init_Separation, gx_restrict01_paint_1,
    gx_concrete_space_Separation,
    gx_concretize_Separation, gx_remap_concrete_Separation,
    gx_default_remap_color, gx_install_Separation,
    gx_adjust_cspace_Separation, gx_no_adjust_color_count
};

/* GC procedures */

#define pcs ((gs_color_space *)vptr)

private 
ENUM_PTRS_BEGIN(cs_Separation_enum_ptrs)
{
    return ENUM_USING(*pcs->params.separation.alt_space.type->stype,
		      &pcs->params.separation.alt_space,
		      sizeof(pcs->params.separation.alt_space), index - 1);
}
ENUM_PTR(0, gs_color_space, params.separation.map);
ENUM_PTRS_END
private RELOC_PTRS_BEGIN(cs_Separation_reloc_ptrs)
{
    RELOC_PTR(gs_color_space, params.separation.map);
    RELOC_USING(*pcs->params.separation.alt_space.type->stype,
		&pcs->params.separation.alt_space,
		sizeof(gs_base_color_space));
}
RELOC_PTRS_END

#undef pcs

/* Get the alternate space for a Separation space. */
private const gs_color_space *
gx_alt_space_Separation(const gs_color_space * pcs)
{
    return (const gs_color_space *)&(pcs->params.separation.alt_space);
}

/* Get the concrete space for a Separation space. */
/* (We don't support concrete Separation spaces yet.) */
private const gs_color_space *
gx_concrete_space_Separation(const gs_color_space * pcs,
			     const gs_imager_state * pis)
{
    const gs_color_space *pacs =
    (const gs_color_space *)&pcs->params.separation.alt_space;

    return cs_concrete_space(pacs, pis);
}

/* Install a Separation color space. */
private int
gx_install_Separation(gs_color_space * pcs, gs_state * pgs)
{
    return (*pcs->params.separation.alt_space.type->install_cspace)
	((gs_color_space *) & pcs->params.separation.alt_space, pgs);
}

/* Adjust the reference count of a Separation color space. */
private void
gx_adjust_cspace_Separation(const gs_color_space * pcs, int delta)
{
    rc_adjust_const(pcs->params.separation.map, delta,
		    "gx_adjust_Separation");
    (*pcs->params.separation.alt_space.type->adjust_cspace_count)
	((const gs_color_space *)&pcs->params.separation.alt_space, delta);
}

/* ------ Constructors/accessors ------ */

/*
 * The default separation tint transformation function. This will just return
 * the information in the cache or, if the cache is of zero size, set all
 * components in the alternative color space to 0.
 *
 * No special cases are provided for this routine, as the use of separations
 * (particular in this form) is sufficiently rare to not have a significant
 * performance impact.
 */
private int
map_tint_value(const gs_separation_params * pcssepr, floatp in_val,
	       float *out_vals)
{
    int ncomps =
    cs_num_components((const gs_color_space *)&pcssepr->alt_space);
    int nentries = pcssepr->map->num_values / ncomps;
    int indx;
    const float *pv = pcssepr->map->values;
    int i;

    if (nentries == 0) {
	for (i = 0; i < ncomps; i++)
	    out_vals[i] = 0.0;
	return 0;
    }
    if (in_val > 1)
	indx = nentries - 1;
    else if (in_val <= 0)
	indx = 0;
    else
	indx = (int)(in_val * nentries + 0.5);
    pv += indx * ncomps;

    for (i = 0; i < ncomps; i++)
	out_vals[i] = pv[i];
    return 0;
}

/*
 *  Allocate the indexed map required by a separation color space. 
 */
private gs_indexed_map *
alloc_separation_map(const gs_color_space * palt_cspace, int cache_size,
		     gs_memory_t * pmem)
{
    gs_indexed_map *pimap;

    rc_alloc_struct_1(pimap, gs_indexed_map, &st_indexed_map, pmem,
		      return 0,
		      "gs_cspace_build_Separation"
	);
    pimap->rc.free = free_indexed_map;
    pimap->proc.tint_transform = map_tint_value;

    if (cache_size != 0) {
	int num_comps = gs_color_space_num_components(palt_cspace);

	cache_size *= num_comps;
	pimap->num_values = cache_size;
	pimap->values =
	    (float *)gs_alloc_byte_array(pmem, cache_size, sizeof(float),
					 "gs_cspace_build_Separation"
	);

	if (pimap->values == 0)
	    rc_decrement(pimap, "gs_cspace_build_Separation");	/* sets pimap = 0 */

    } else {
	pimap->num_values = 0;
	pimap->values = 0;
    }
    return pimap;
}

/*
 * Build a separation color space.
 *
 * The values array provided with separation color spaces is actually cached
 * information, but filled in by the client. The alternative space is the
 * color space in which the tint procedure will provide alternative colors.
 */
int
gs_cspace_build_Separation(
			      gs_color_space ** ppcspace,
			      gs_separation_name sname,
			      const gs_color_space * palt_cspace,
			      int cache_size,
			      gs_memory_t * pmem
)
{
    gs_color_space *pcspace = 0;
    gs_separation_params *pcssepr = 0;
    int code;

    if (palt_cspace == 0 || !palt_cspace->type->can_be_alt_space)
	return_error(gs_error_rangecheck);

    code = gs_cspace_alloc(&pcspace, &gs_color_space_type_Separation, pmem);
    if (code < 0)
	return code;
    pcssepr = &pcspace->params.separation;
    pcssepr->map = alloc_separation_map(palt_cspace, cache_size, pmem);
    if (pcssepr->map == 0) {
	gs_free_object(pmem, pcspace, "gs_cspace_build_Separation");
	return_error(gs_error_VMerror);
    }
    pcssepr->sname = sname;
    gs_cspace_init_from((gs_color_space *) & pcssepr->alt_space, palt_cspace);
    *ppcspace = pcspace;
    return 0;
}

/*
 * Get the cached value array for a separation color space. This will return
 * a null pointer if the color space is not a separation color space, or if
 * the separation color space has a cache size of 0.
 */
float *
gs_cspace_get_separation_value_array(const gs_color_space * pcspace)
{
    if (gs_color_space_get_index(pcspace) != gs_color_space_index_Separation)
	return 0;
    return pcspace->params.separation.map->values;
}

/*
 * Set the tint transformation procedure used by a Separation color space.
 */
int
gs_cspace_set_tint_transform_proc(gs_color_space * pcspace,
	    int (*proc) (P3(const gs_separation_params *, floatp, float *)))
{
    if (gs_color_space_get_index(pcspace) != gs_color_space_index_Separation)
	return_error(gs_error_rangecheck);
    pcspace->params.separation.map->proc.tint_transform = proc;
    return 0;
}

/* ---------------- Graphics state ---------------- */

/* setoverprint */
void
gs_setoverprint(gs_state * pgs, bool ovp)
{
    pgs->overprint = ovp;
}

/* currentoverprint */
bool
gs_currentoverprint(const gs_state * pgs)
{
    return pgs->overprint;
}

/* ------ Internal procedures ------ */

/* Initialize a Separation color. */

private void
gx_init_Separation(gs_client_color * pcc, const gs_color_space * pcs)
{
    pcc->paint.values[0] = 1.0;
}

/* Remap a Separation color. */

private int
gx_concretize_Separation(const gs_client_color * pc, const gs_color_space * pcs,
			 frac * pconc, const gs_imager_state * pis)
{
    float tint = pc->paint.values[0];
    int code;
    gs_client_color cc;
    const gs_color_space *pacs =
    (const gs_color_space *)&pcs->params.separation.alt_space;

    if (tint < 0)
	tint = 0;
    else if (tint > 1)
	tint = 1;
    /* We always map into the alternate color space. */
    code = (*pcs->params.separation.map->proc.tint_transform) (&pcs->params.separation, tint, &cc.paint.values[0]);
    if (code < 0)
	return code;
    return (*pacs->type->concretize_color) (&cc, pacs, pconc, pis);
}

private int
gx_remap_concrete_Separation(const frac * pconc,
	gx_device_color * pdc, const gs_imager_state * pis, gx_device * dev,
			     gs_color_select_t select)
{				/* We don't support concrete Separation colors yet. */
    return_error(gs_error_rangecheck);
}

/* ---------------- Notes on real Separation colors ---------------- */

typedef ulong gs_separation;	/* BOGUS */

#define gs_no_separation ((gs_separation)(-1L))

#define dev_proc_lookup_separation(proc)\
  gs_separation proc(P4(gx_device *dev, const byte *sname, uint len,\
    gx_color_value *num_levels))

#define dev_proc_map_tint_color(proc)\
  gx_color_index proc(P4(gx_device *dev, gs_separation sepr, bool overprint,\
    gx_color_value tint))

/*
 * In principle, setting a Separation color space, or setting the device
 * when the current color space is a Separation space, calls the
 * lookup_separation device procedure to obtain the separation ID and
 * the number of achievable levels.  Currently, the only hooks for doing
 * this are unsuitable: gx_set_cmap_procs isn't called when the color
 * space changes, and doing it in gx_remap_Separation is inefficient.
 * Probably the best approach is to call gx_set_cmap_procs whenever the
 * color space changes.  In fact, if we do this, we can probably short-cut
 * two levels of procedure call in color remapping (gx_remap_color, by
 * turning it into a macro, and gx_remap_DeviceXXX, by calling the
 * cmap_proc procedure directly).  Some care will be required for the
 * implicit temporary resetting of the color space in [color]image.
 *
 * For actual remapping of Separation colors, we need cmap_separation_direct
 * and cmap_separation_halftoned, just as for the other device color spaces.
 * So we need to break apart gx_render_gray in gxdither.c so it can also
 * do the job for separations.
 */
