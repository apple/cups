/* Copyright (C) 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gscspace.c,v 1.1 2000/03/08 23:14:39 mike Exp $ */
/* Color space operators and support */
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsstruct.h"
#include "gsccolor.h"
#include "gxcspace.h"
#include "gxistate.h"

/* Define the standard color space types. */
extern cs_proc_remap_color(gx_remap_DeviceGray);
extern cs_proc_concretize_color(gx_concretize_DeviceGray);
extern cs_proc_remap_concrete_color(gx_remap_concrete_DGray);
extern cs_proc_remap_color(gx_remap_DeviceRGB);
extern cs_proc_concretize_color(gx_concretize_DeviceRGB);
extern cs_proc_remap_concrete_color(gx_remap_concrete_DRGB);
const gs_color_space_type gs_color_space_type_DeviceGray = {
    gs_color_space_index_DeviceGray, true, true,
    &st_base_color_space, gx_num_components_1,
    gx_no_base_space,
    gx_init_paint_1, gx_restrict01_paint_1,
    gx_same_concrete_space,
    gx_concretize_DeviceGray, gx_remap_concrete_DGray,
    gx_remap_DeviceGray, gx_no_install_cspace,
    gx_no_adjust_cspace_count, gx_no_adjust_color_count
};
const gs_color_space_type gs_color_space_type_DeviceRGB = {
    gs_color_space_index_DeviceRGB, true, true,
    &st_base_color_space, gx_num_components_3,
    gx_no_base_space,
    gx_init_paint_3, gx_restrict01_paint_3,
    gx_same_concrete_space,
    gx_concretize_DeviceRGB, gx_remap_concrete_DRGB,
    gx_remap_DeviceRGB, gx_no_install_cspace,
    gx_no_adjust_cspace_count, gx_no_adjust_color_count
};

/* Structure descriptors */
public_st_color_space();
public_st_base_color_space();

/* Return the shared instances of the color spaces. */
const gs_color_space *
gs_cspace_DeviceGray(const gs_imager_state * pis)
{
    return gs_imager_state_shared(pis, cs_DeviceGray);
}
const gs_color_space *
gs_cspace_DeviceRGB(const gs_imager_state * pis)
{
    return gs_imager_state_shared(pis, cs_DeviceRGB);
}
const gs_color_space *
gs_cspace_DeviceCMYK(const gs_imager_state * pis)
{
    return gs_imager_state_shared(pis, cs_DeviceCMYK);
}

/* ------ Create/copy/destroy ------ */

int
gs_cspace_alloc(gs_color_space ** ppcspace,
		const gs_color_space_type * pcstype,
		gs_memory_t * mem)
{
    gs_color_space *pcspace =
	gs_alloc_struct(mem, gs_color_space, &st_color_space,
			"gs_cspace_alloc");

    if (pcspace == 0)
	return_error(gs_error_VMerror);
    pcspace->pmem = mem;
    pcspace->type = pcstype;
    *ppcspace = pcspace;
    return 0;
}

int
gs_cspace_build_DeviceGray(gs_color_space ** ppcspace, gs_memory_t * pmem)
{
    return gs_cspace_alloc(ppcspace, &gs_color_space_type_DeviceGray, pmem);
}
int
gs_cspace_build_DeviceRGB(gs_color_space ** ppcspace, gs_memory_t * pmem)
{
    return gs_cspace_alloc(ppcspace, &gs_color_space_type_DeviceRGB, pmem);
}
int
gs_cspace_build_DeviceCMYK(gs_color_space ** ppcspace, gs_memory_t * pmem)
{
    return gs_cspace_alloc(ppcspace, &gs_color_space_type_DeviceCMYK, pmem);
}

/*
 * Copy just enough of a color space object.  This will do the right thing
 * for copying color spaces into the base or alternate color space of a
 * compound color space when legal, but it can't check that the operation is
 * actually legal.
 */
inline private void
cs_copy(gs_color_space *pcsto, const gs_color_space *pcsfrom)
{
    memcpy(pcsto, pcsfrom, pcsfrom->type->stype->ssize);
}

/* Copy a color space into one newly allocated by the caller. */
void
gs_cspace_init_from(gs_color_space * pcsto, const gs_color_space * pcsfrom)
{
    cs_copy(pcsto, pcsfrom);
    (*pcsto->type->adjust_cspace_count)(pcsto, 1);
}

/* Assign a color space into a previously initialized one. */
void
gs_cspace_assign(gs_color_space * pdest, const gs_color_space * psrc)
{
    /* check for a = a */
    if (pdest == psrc)
	return;
    (*psrc->type->adjust_cspace_count)(psrc, 1);
    (*pdest->type->adjust_cspace_count)(pdest, -1);
    cs_copy(pdest, psrc);
}


/* Prepare to free a color space. */
void
gs_cspace_release(gs_color_space * pcs)
{
    (*pcs->type->adjust_cspace_count)(pcs, -1);
}

/* ------ Accessors ------ */

/* Get the index of a color space. */
gs_color_space_index
gs_color_space_get_index(const gs_color_space * pcs)
{
    return pcs->type->index;
}

/* Get the number of components in a color space. */
int
gs_color_space_num_components(const gs_color_space * pcs)
{
    return cs_num_components(pcs);
}

int
gx_num_components_1(const gs_color_space * pcs)
{
    return 1;
}
int
gx_num_components_3(const gs_color_space * pcs)
{
    return 3;
}
int
gx_num_components_4(const gs_color_space * pcs)
{
    return 4;
}

/*
 * For color spaces that have a base or alternative color space, return that
 * color space. Otherwise return null.
 */
const gs_color_space *
gs_cspace_base_space(const gs_color_space * pcspace)
{
    return cs_base_space(pcspace);
}

const gs_color_space *
gx_no_base_space(const gs_color_space * pcspace)
{
    return NULL;
}

/* ------ Other implementation procedures ------ */

/* Null color space installation procedure. */
int
gx_no_install_cspace(gs_color_space * pcs, gs_state * pgs)
{
    return 0;
}

/* Null reference count adjustment procedure. */
void
gx_no_adjust_cspace_count(const gs_color_space * pcs, int delta)
{
}

/* GC procedures */

#define pcs ((gs_color_space *)vptr)
private 
ENUM_PTRS_BEGIN_PROC(color_space_enum_ptrs)
{
    return ENUM_USING(*pcs->type->stype, vptr, size, index);
    ENUM_PTRS_END_PROC
}
private 
RELOC_PTRS_BEGIN(color_space_reloc_ptrs)
{
    RELOC_USING(*pcs->type->stype, vptr, size);
}
RELOC_PTRS_END
#undef pcs
