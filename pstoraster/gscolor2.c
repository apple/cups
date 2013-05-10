/* Copyright (C) 1992, 1994, 1994 Aladdin Enterprises.  All rights reserved.
  
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

/* gscolor2.c */
/* Level 2 color operators for Ghostscript library */
#include "gx.h"
#include "gserrors.h"
#include "gxarith.h"
#include "gxfixed.h"			/* ditto */
#include "gxmatrix.h"			/* for gzstate.h */
#include "gxcspace.h"			/* for gscolor2.h */
#include "gxcolor2.h"
#include "gzstate.h"

/* Define the Indexed color space type. */
cs_declare_procs(private, gx_concretize_Indexed, gx_install_Indexed,
  gx_adjust_cspace_Indexed,
  gx_enum_ptrs_Indexed, gx_reloc_ptrs_Indexed);
private cs_proc_concrete_space(gx_concrete_space_Indexed);
const gs_color_space_type
	gs_color_space_type_Indexed =
	 { gs_color_space_index_Indexed, 1, false,
	   gx_init_paint_1, gx_concrete_space_Indexed,
	   gx_concretize_Indexed, NULL,
	   gx_default_remap_color, gx_install_Indexed,
	   gx_adjust_cspace_Indexed, gx_no_adjust_color_count,
	   gx_enum_ptrs_Indexed, gx_reloc_ptrs_Indexed
	 };

/* setcolorspace */
int
gs_setcolorspace(gs_state *pgs, gs_color_space *pcs)
{	int code;
	gs_color_space cs_old;
	gs_client_color cc_old;
	if ( pgs->in_cachedevice )
		return_error(gs_error_undefined);
	cs_old = *pgs->color_space;
	cc_old = *pgs->ccolor;
	(*pcs->type->adjust_cspace_count)(pcs, pgs, 1);
	*pgs->color_space = *pcs;
	if ( (code = (*pcs->type->install_cspace)(pcs, pgs)) < 0 )
		goto rcs;
	cs_full_init_color(pgs->ccolor, pcs);
	(*cs_old.type->adjust_color_count)(&cc_old, &cs_old, pgs, -1);
	(*cs_old.type->adjust_cspace_count)(&cs_old, pgs, -1);
	gx_unset_dev_color(pgs);
	return code;
	/* Restore the color space if installation failed. */
rcs:	*pgs->color_space = cs_old;
	(*pcs->type->adjust_cspace_count)(pcs, pgs, -1);
	return code;
}

/* currentcolorspace */
const gs_color_space *
gs_currentcolorspace(const gs_state *pgs)
{	return pgs->color_space;
}

/* setcolor */
int
gs_setcolor(gs_state *pgs, const gs_client_color *pcc)
{	gs_color_space *pcs = pgs->color_space;
	if ( pgs->in_cachedevice )
		return_error(gs_error_undefined);
	(*pcs->type->adjust_color_count)(pcc, pcs, pgs, 1);
	(*pcs->type->adjust_color_count)(pgs->ccolor, pcs, pgs, -1);
	*pgs->ccolor = *pcc;
	gx_unset_dev_color(pgs);
	return 0;
}

/* currentcolor */
const gs_client_color *
gs_currentcolor(const gs_state *pgs)
{	return pgs->ccolor;
}

/* setoverprint */
void
gs_setoverprint(gs_state *pgs, bool ovp)
{	pgs->overprint = ovp;
}

/* currentoverprint */
bool
gs_currentoverprint(const gs_state *pgs)
{	return pgs->overprint;
}

/* ------ Internal routines ------ */

/* Color remapping for Indexed color spaces. */

private const gs_color_space *
gx_concrete_space_Indexed(const gs_color_space *pcs, const gs_state *pgs)
{	const gs_color_space *pbcs =
		(const gs_color_space *)&pcs->params.indexed.base_space;
	return cs_concrete_space(pbcs, pgs);
}

private int
gx_concretize_Indexed(const gs_client_color *pc, const gs_color_space *pcs,
  frac *pconc, const gs_state *pgs)
{	float value = pc->paint.values[0];
	int index =
		(is_fneg(value) ? 0 :
		 value >= pcs->params.indexed.hival ?
		   pcs->params.indexed.hival :
		 (int)value);
	gs_client_color cc;
	const gs_color_space *pbcs =
		(const gs_color_space *)&pcs->params.indexed.base_space;
	if ( pcs->params.indexed.use_proc )
	{	int code = (*pcs->params.indexed.lookup.map->proc.lookup_index)(&pcs->params.indexed, index, &cc.paint.values[0]);
		if ( code < 0 ) return code;
	}
	else
	{	int m = pcs->params.indexed.base_space.type->num_components;
		const byte *pcomp =
		  pcs->params.indexed.lookup.table.data + m * index;
		switch ( m )
		{
		default: return_error(gs_error_rangecheck);
		case 4: cc.paint.values[3] = pcomp[3] * (1.0 / 255.0);
		case 3: cc.paint.values[2] = pcomp[2] * (1.0 / 255.0);
			cc.paint.values[1] = pcomp[1] * (1.0 / 255.0);
		case 1: cc.paint.values[0] = pcomp[0] * (1.0 / 255.0);
		}
	}
	return (*pbcs->type->concretize_color)(&cc, pbcs, pconc, pgs);
}

/* Color space installation ditto. */

private int
gx_install_Indexed(gs_color_space *pcs, gs_state *pgs)
{	return (*pcs->params.indexed.base_space.type->install_cspace)
		((gs_color_space *)&pcs->params.indexed.base_space, pgs);
}

/* Color space reference count adjustment ditto. */

private void
gx_adjust_cspace_Indexed(const gs_color_space *pcs, gs_state *pgs, int delta)
{	if ( pcs->params.indexed.use_proc )
	{	rc_adjust_const(pcs->params.indexed.lookup.map, delta,
				pgs->memory, "gx_adjust_Indexed");
	}
	(*pcs->params.indexed.base_space.type->adjust_cspace_count)
	 ((const gs_color_space *)&pcs->params.indexed.base_space, pgs, delta);
}

/* GC procedures ditto. */

#define pcs ((gs_color_space *)vptr)

private ENUM_PTRS_BEGIN(gx_enum_ptrs_Indexed) {
	return (*pcs->params.indexed.base_space.type->enum_ptrs)
		 (&pcs->params.indexed.base_space,
		  sizeof(pcs->params.indexed.base_space), index-1, pep);
	}
	case 0:
		if ( pcs->params.indexed.use_proc )
		  *pep = (void *)pcs->params.indexed.lookup.map;
		else
		  {	pcs->params.indexed.lookup.table.size =
			  (pcs->params.indexed.hival + 1) *
			    pcs->params.indexed.base_space.type->num_components;
			*pep = &pcs->params.indexed.lookup.table;
			return ptr_const_string_type;
		  }
		break;
ENUM_PTRS_END
private RELOC_PTRS_BEGIN(gx_reloc_ptrs_Indexed) {
	(*pcs->params.indexed.base_space.type->reloc_ptrs)
	  (&pcs->params.indexed.base_space, sizeof(gs_base_color_space), gcst);
	if ( pcs->params.indexed.use_proc )
	  RELOC_PTR(gs_color_space, params.indexed.lookup.map);
	else
	  RELOC_CONST_STRING_PTR(gs_color_space, params.indexed.lookup.table);
} RELOC_PTRS_END

#undef pcs
