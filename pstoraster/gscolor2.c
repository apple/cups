/* Copyright (C) 1992, 1994, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gscolor2.c,v 1.2 2000/03/08 23:14:37 mike Exp $ */
/* Level 2 color operators for Ghostscript library */
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gxarith.h"
#include "gxfixed.h"		/* ditto */
#include "gxmatrix.h"		/* for gzstate.h */
#include "gxcspace.h"		/* for gscolor2.h */
#include "gxcolor2.h"
#include "gzstate.h"

/* ---------------- General colors and color spaces ---------------- */

/* setcolorspace */
int
gs_setcolorspace(gs_state * pgs, gs_color_space * pcs)
{
    int code;
    gs_color_space cs_old;
    gs_client_color cc_old;

    if (pgs->in_cachedevice)
	return_error(gs_error_undefined);
    cs_old = *pgs->color_space;
    cc_old = *pgs->ccolor;
    (*pcs->type->adjust_cspace_count)(pcs, 1);
    *pgs->color_space = *pcs;
    if ((code = (*pcs->type->install_cspace)(pcs, pgs)) < 0)
	goto rcs;
    cs_full_init_color(pgs->ccolor, pcs);
    (*cs_old.type->adjust_color_count)(&cc_old, &cs_old, -1);
    (*cs_old.type->adjust_cspace_count)(&cs_old, -1);
    gx_unset_dev_color(pgs);
    return code;
    /* Restore the color space if installation failed. */
rcs:*pgs->color_space = cs_old;
    (*pcs->type->adjust_cspace_count)(pcs, -1);
    return code;
}

/* currentcolorspace */
const gs_color_space *
gs_currentcolorspace(const gs_state * pgs)
{
    return pgs->color_space;
}

/* setcolor */
int
gs_setcolor(gs_state * pgs, const gs_client_color * pcc)
{
    gs_color_space *pcs = pgs->color_space;

    if (pgs->in_cachedevice)
	return_error(gs_error_undefined);
    (*pcs->type->adjust_color_count)(pcc, pcs, 1);
    (*pcs->type->adjust_color_count)(pgs->ccolor, pcs, -1);
    *pgs->ccolor = *pcc;
    (*pcs->type->restrict_color)(pgs->ccolor, pcs);
    gx_unset_dev_color(pgs);
    return 0;
}

/* currentcolor */
const gs_client_color *
gs_currentcolor(const gs_state * pgs)
{
    return pgs->ccolor;
}

/* ------ Internal procedures ------ */

/* GC descriptors */
public_st_indexed_map();

/* Free an indexed map and its values when the reference count goes to 0. */
void
free_indexed_map(gs_memory_t * pmem, void *pmap, client_name_t cname)
{
    gs_free_object(pmem, ((gs_indexed_map *) pmap)->values, cname);
    gs_free_object(pmem, pmap, cname);
}

/*
 * Allocate an indexed map for an Indexed or Separation color space.
 */
int
alloc_indexed_map(gs_indexed_map ** ppmap, int nvals, gs_memory_t * pmem,
		  client_name_t cname)
{
    gs_indexed_map *pimap;

    rc_alloc_struct_1(pimap, gs_indexed_map, &st_indexed_map, pmem,
		      return_error(gs_error_VMerror), cname);
    pimap->values =
	(float *)gs_alloc_byte_array(pmem, nvals, sizeof(float), cname);

    if (pimap->values == 0) {
	gs_free_object(pmem, pimap, cname);
	return_error(gs_error_VMerror);
    }
    pimap->rc.free = free_indexed_map;
    pimap->num_values = nvals;
    *ppmap = pimap;
    return 0;
}

/* ---------------- Indexed color spaces ---------------- */

gs_private_st_composite(st_color_space_Indexed, gs_paint_color_space,
     "gs_color_space_Indexed", cs_Indexed_enum_ptrs, cs_Indexed_reloc_ptrs);

/* ------ Color space ------ */

/* Define the Indexed color space type. */
private cs_proc_base_space(gx_base_space_Indexed);
private cs_proc_restrict_color(gx_restrict_Indexed);
private cs_proc_concrete_space(gx_concrete_space_Indexed);
private cs_proc_concretize_color(gx_concretize_Indexed);
private cs_proc_install_cspace(gx_install_Indexed);
private cs_proc_adjust_cspace_count(gx_adjust_cspace_Indexed);
const gs_color_space_type gs_color_space_type_Indexed = {
    gs_color_space_index_Indexed, false, false,
    &st_color_space_Indexed, gx_num_components_1,
    gx_base_space_Indexed,
    gx_init_paint_1, gx_restrict_Indexed,
    gx_concrete_space_Indexed,
    gx_concretize_Indexed, NULL,
    gx_default_remap_color, gx_install_Indexed,
    gx_adjust_cspace_Indexed, gx_no_adjust_color_count
};

/* GC procedures. */

#define pcs ((gs_color_space *)vptr)

private 
ENUM_PTRS_BEGIN(cs_Indexed_enum_ptrs)
{
    return ENUM_USING(*pcs->params.indexed.base_space.type->stype,
		      &pcs->params.indexed.base_space,
		      sizeof(pcs->params.indexed.base_space), index - 1);
}
case 0:
if (pcs->params.indexed.use_proc)
    ENUM_RETURN((void *)pcs->params.indexed.lookup.map);
else {
    pcs->params.indexed.lookup.table.size =
	(pcs->params.indexed.hival + 1) *
	cs_num_components((const gs_color_space *)
			  &pcs->params.indexed.base_space);
    ENUM_RETURN_CONST_STRING_PTR(gs_color_space,
				 params.indexed.lookup.table);
}
ENUM_PTRS_END
private RELOC_PTRS_BEGIN(cs_Indexed_reloc_ptrs)
{
    RELOC_USING(*pcs->params.indexed.base_space.type->stype,
		&pcs->params.indexed.base_space,
		sizeof(gs_base_color_space));
    if (pcs->params.indexed.use_proc)
	RELOC_PTR(gs_color_space, params.indexed.lookup.map);
    else
	RELOC_CONST_STRING_PTR(gs_color_space, params.indexed.lookup.table);
}
RELOC_PTRS_END

#undef pcs

/* Return the base space of an Indexed color space. */
private const gs_color_space *
gx_base_space_Indexed(const gs_color_space * pcs)
{
    return (const gs_color_space *)&(pcs->params.indexed.base_space);
}

/* Color space installation ditto. */

private int
gx_install_Indexed(gs_color_space * pcs, gs_state * pgs)
{
    return (*pcs->params.indexed.base_space.type->install_cspace)
	((gs_color_space *) & pcs->params.indexed.base_space, pgs);
}

/* Color space reference count adjustment ditto. */

private void
gx_adjust_cspace_Indexed(const gs_color_space * pcs, int delta)
{
    if (pcs->params.indexed.use_proc) {
	rc_adjust_const(pcs->params.indexed.lookup.map, delta,
			"gx_adjust_Indexed");
    }
    (*pcs->params.indexed.base_space.type->adjust_cspace_count)
	((const gs_color_space *)&pcs->params.indexed.base_space, delta);
}

/*
 * Default palette mapping functions for indexed color maps. These just
 * return the values already in the palette.
 *
 * For performance reasons, we provide four functions: special cases for 1,
 * 3, and 4 entry palettes, and a general case. Note that these procedures
 * do not range-check their input values.
 */
private int
map_palette_entry_1(const gs_indexed_params * params, int indx, float *values)
{
    values[0] = params->lookup.map->values[indx];
    return 0;
}

private int
map_palette_entry_3(const gs_indexed_params * params, int indx, float *values)
{
    const float *pv = &(params->lookup.map->values[3 * indx]);

    values[0] = pv[0];
    values[1] = pv[1];
    values[2] = pv[2];
    return 0;
}

private int
map_palette_entry_4(const gs_indexed_params * params, int indx, float *values)
{
    const float *pv = &(params->lookup.map->values[4 * indx]);

    values[0] = pv[0];
    values[1] = pv[1];
    values[2] = pv[2];
    values[3] = pv[3];
    return 0;
}

private int
map_palette_entry_n(const gs_indexed_params * params, int indx, float *values)
{
    int m = cs_num_components((const gs_color_space *)&params->base_space);

    memcpy((void *)values,
	   (const void *)(params->lookup.map->values + indx * m),
	   m * sizeof(float)
    );

    return 0;
}

/*
 * Allocate an indexed map to be used as a palette for indexed color space.
 */
private gs_indexed_map *
alloc_indexed_palette(
			 const gs_color_space * pbase_cspace,
			 int nvals,
			 gs_memory_t * pmem
)
{
    int num_comps = gs_color_space_num_components(pbase_cspace);
    gs_indexed_map *pimap;
    int code =
    alloc_indexed_map(&pimap, nvals * num_comps, pmem,
		      "alloc_indexed_palette");

    if (code < 0)
	return 0;
    if (num_comps == 1)
	pimap->proc.lookup_index = map_palette_entry_1;
    else if (num_comps == 3)
	pimap->proc.lookup_index = map_palette_entry_3;
    else if (num_comps == 4)
	pimap->proc.lookup_index = map_palette_entry_4;
    else
	pimap->proc.lookup_index = map_palette_entry_n;
    return pimap;
}

/*
 * Build an indexed color space.
 */
int
gs_cspace_build_Indexed(
			   gs_color_space ** ppcspace,
			   const gs_color_space * pbase_cspace,
			   uint num_entries,
			   const gs_const_string * ptbl,
			   gs_memory_t * pmem
)
{
    gs_color_space *pcspace = 0;
    gs_indexed_params *pindexed = 0;
    int code;

    if ((pbase_cspace == 0) || !pbase_cspace->type->can_be_base_space)
	return_error(gs_error_rangecheck);

    code = gs_cspace_alloc(&pcspace, &gs_color_space_type_Indexed, pmem);
    if (code < 0)
	return code;
    pindexed = &(pcspace->params.indexed);
    if (ptbl == 0) {
	pindexed->lookup.map =
	    alloc_indexed_palette(pbase_cspace, num_entries, pmem);
	if (pindexed->lookup.map == 0) {
	    gs_free_object(pmem, pcspace, "gs_cspace_build_Indexed");
	    return_error(gs_error_VMerror);
	}
	pindexed->use_proc = true;
    } else {
	pindexed->lookup.table = *ptbl;
	pindexed->use_proc = false;
    }
    gs_cspace_init_from((gs_color_space *) & (pindexed->base_space),
			pbase_cspace);
    pindexed->hival = num_entries - 1;
    *ppcspace = pcspace;
    return 0;
}

/*
 * Return the number of entries in an indexed color space.
 */
int
gs_cspace_indexed_num_entries(const gs_color_space * pcspace)
{
    if (gs_color_space_get_index(pcspace) != gs_color_space_index_Indexed)
	return 0;
    return pcspace->params.indexed.hival + 1;
}

/*
 * Get the palette for an indexed color space. This will return a null
 * pointer if the color space is not an indexed color space or if the
 * color space does not use the mapped index palette.
 */
float *
gs_cspace_indexed_value_array(const gs_color_space * pcspace)
{
    if ((gs_color_space_get_index(pcspace) != gs_color_space_index_Indexed) ||
	pcspace->params.indexed.use_proc
	)
	return 0;
    return pcspace->params.indexed.lookup.map->values;
}

/*
 * Set the lookup procedure to be used with an indexed color space.
 */
int
gs_cspace_indexed_set_proc(
			      gs_color_space * pcspace,
		   int (*proc) (P3(const gs_indexed_params *, int, float *))
)
{
    if ((gs_color_space_get_index(pcspace) != gs_color_space_index_Indexed) ||
	!pcspace->params.indexed.use_proc
	)
	return_error(gs_error_rangecheck);
    pcspace->params.indexed.lookup.map->proc.lookup_index = proc;
    return 0;
}

/* ------ Colors ------ */

/* Force an Indexed color into legal range. */

private void
gx_restrict_Indexed(gs_client_color * pcc, const gs_color_space * pcs)
{
    float value = pcc->paint.values[0];

    pcc->paint.values[0] =
	(is_fneg(value) ? 0 :
	 value >= pcs->params.indexed.hival ? pcs->params.indexed.hival :
	 value);
}

/* Color remapping for Indexed color spaces. */

private const gs_color_space *
gx_concrete_space_Indexed(const gs_color_space * pcs,
			  const gs_imager_state * pis)
{
    const gs_color_space *pbcs =
    (const gs_color_space *)&pcs->params.indexed.base_space;

    return cs_concrete_space(pbcs, pis);
}

private int
gx_concretize_Indexed(const gs_client_color * pc, const gs_color_space * pcs,
		      frac * pconc, const gs_imager_state * pis)
{
    float value = pc->paint.values[0];
    int index =
    (is_fneg(value) ? 0 :
     value >= pcs->params.indexed.hival ? pcs->params.indexed.hival :
     (int)value);
    gs_client_color cc;
    const gs_color_space *pbcs =
    (const gs_color_space *)&pcs->params.indexed.base_space;

    if (pcs->params.indexed.use_proc) {
	int code =
	(*pcs->params.indexed.lookup.map->proc.lookup_index)
	(&pcs->params.indexed, index, &cc.paint.values[0]);

	if (code < 0)
	    return code;
    } else {
	int m = cs_num_components((const gs_color_space *)
				  &pcs->params.indexed.base_space);
	const byte *pcomp =
	pcs->params.indexed.lookup.table.data + m * index;

	switch (m) {
	    default:
		return_error(gs_error_rangecheck);
	    case 4:
		cc.paint.values[3] = pcomp[3] * (1.0 / 255.0);
	    case 3:
		cc.paint.values[2] = pcomp[2] * (1.0 / 255.0);
		cc.paint.values[1] = pcomp[1] * (1.0 / 255.0);
	    case 1:
		cc.paint.values[0] = pcomp[0] * (1.0 / 255.0);
	}
    }
    return (*pbcs->type->concretize_color) (&cc, pbcs, pconc, pis);
}
