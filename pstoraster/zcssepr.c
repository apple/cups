/* Copyright (C) 1994, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* zcssepr.c */
/* Separation color space support */
#include "ghost.h"
#include "errors.h"
#include "oper.h"
#include "gsstruct.h"
#include "gscolor.h"
#include "gsmatrix.h"		/* for gxcolor2.h */
#include "gxcspace.h"
#include "gxfixed.h"		/* ditto */
#include "gxcolor2.h"
#include "estack.h"
#include "ialloc.h"
#include "icsmap.h"
#include "igstate.h"
#include "ivmspace.h"
#include "store.h"

/* Imported from gscsepr.c */
extern const gs_color_space_type gs_color_space_type_Separation;

/* Forward references */
private int separation_map1(P1(os_ptr));

/* Define the separation cache size.  This makes many useful tint values */
/* map to integer cache indices. */
#define separation_cache_size 360

/* Tint transform procedure that just consults the cache. */
private int
lookup_tint(const gs_separation_params *params, floatp tint, float *values)
{	int m = params->alt_space.type->num_components;
	const gs_indexed_map *map = params->map;
	int value_index =
	  (tint < 0 ? 0 : tint > 1 ? map->num_values - m :
	   (int)(tint * separation_cache_size + 0.5) * m);
	const float *pv = &map->values[value_index];
	switch ( m )
	{
	default: return_error(e_rangecheck);
	case 4: values[3] = pv[3];
	case 3: values[2] = pv[2];
		values[1] = pv[1];
	case 1: values[0] = pv[0];
	}
	return 0;
}

/* <array> .setseparationspace - */
/* The current color space is the alternate space for the separation space. */
private int
zsetseparationspace(register os_ptr op)
{	const ref *pcsa;
	gs_color_space cs;
	ref_colorspace cspace_old;
	uint edepth = ref_stack_count(&e_stack);
	gs_indexed_map *map;
	int code;

	check_read_type(*op, t_array);
	if ( r_size(op) != 4 )
		return_error(e_rangecheck);
	pcsa = op->value.const_refs + 1;
	switch ( r_type(pcsa) )
	{
	default:
		return_error(e_typecheck);
	case t_string:
	case t_name:
		;
	}
	check_proc(pcsa[2]);
	cs = *gs_currentcolorspace(igs);
	if ( !cs.type->can_be_base_space )
		return_error(e_rangecheck);
	code = zcs_begin_map(&map, &pcsa[2], separation_cache_size + 1,
			     (const gs_base_color_space *)&cs,
			     separation_map1);
	if ( code < 0 )
	  return code;
	map->proc.tint_transform = lookup_tint;
	cs.params.separation.alt_space = *(gs_base_color_space *)&cs;
	cs.params.separation.map = map;
	cspace_old = istate->colorspace;
	istate->colorspace.procs.special.separation.layer_name = pcsa[0];
	istate->colorspace.procs.special.separation.tint_transform = pcsa[2];
	cs.type = &gs_color_space_type_Separation;
	code = gs_setcolorspace(igs, &cs);
	if ( code < 0 )
	{	istate->colorspace = cspace_old;
		ref_stack_pop_to(&e_stack, edepth);
		return code;
	}
	pop(1);
	return (ref_stack_count(&e_stack) == edepth ? 0 : o_push_estack);  /* installation will load the caches */
}

/* Continuation procedure for saving transformed tint values. */
private int
separation_map1(os_ptr op)
{	es_ptr ep = esp;
	int i = (int)ep[csme_index].value.intval;
	if ( i >= 0 )		/* i.e., not first time */
	{	int m = (int)ep[csme_num_components].value.intval;
		int code = num_params(op, m, &r_ptr(&ep[csme_map], gs_indexed_map)->values[i * m]);
		if ( code < 0 )
			return code;
		pop(m);  op -= m;
		if ( i == (int)ep[csme_hival].value.intval )
		{	/* All done. */
			esp -= num_csme;
			return o_pop_estack;
		}
	}
	push(1);
	ep[csme_index].value.intval = ++i;
	make_real(op, i / (float)separation_cache_size);
	make_op_estack(ep + 1, separation_map1);
	ep[2] = ep[csme_proc];		/* tint_transform */
	esp = ep + 2;
	return o_push_estack;
}

/* ------ Initialization procedure ------ */

BEGIN_OP_DEFS(zcssepr_l2_op_defs) {
		op_def_begin_level2(),
	{"1.setseparationspace", zsetseparationspace},
		/* Internal operators */
	{"1%separation_map1", separation_map1},
END_OP_DEFS(0) }
