/* Copyright (C) 1993, 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: zcsindex.c,v 1.2 2000/03/08 23:15:33 mike Exp $ */
/* Indexed color space support */
#include "memory_.h"
#include "ghost.h"
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

/* Imported from gscolor2.c */
extern const gs_color_space_type gs_color_space_type_Indexed;

/* Forward references. */
private int indexed_map1(P1(os_ptr));

/* Free the map when freeing the gs_indexed_map structure. */
private void
rc_free_indexed_map(gs_memory_t * mem, void *data, client_name_t cname)
{	/*
	 * A bug in the SGI Irix 4.05 compiler requires the following:
	 */
    char *cdata = (char *)data;

    gs_free_object(mem, ((gs_indexed_map *)cdata)->values, cname);
    gs_free_object(mem, cdata, cname);
}

/* Indexed lookup procedure that just consults the cache. */
private int
lookup_indexed(const gs_indexed_params * params, int index, float *values)
{
    int m = cs_num_components((const gs_color_space *)&params->base_space);
    const float *pv = &params->lookup.map->values[index * m];

    switch (m) {
	default:
	    return_error(e_rangecheck);
	case 4:
	    values[3] = pv[3];
	case 3:
	    values[2] = pv[2];
	    values[1] = pv[1];
	case 1:
	    values[0] = pv[0];
    }
    return 0;
}

/* <array> .setindexedspace - */
/* The current color space is the base space for the indexed space. */
private int
zsetindexedspace(register os_ptr op)
{
    ref *pproc = &istate->colorspace.procs.special.index_proc;
    const ref *pcsa;
    gs_color_space cs;
    gs_direct_color_space cs_base;
    ref_colorspace cspace_old;
    uint edepth = ref_stack_count(&e_stack);
    int num_entries;
    int code;

    check_read_type(*op, t_array);
    if (r_size(op) != 4)
	return_error(e_rangecheck);
    pcsa = op->value.const_refs + 1;
    check_type_only(pcsa[1], t_integer);
    if (pcsa[1].value.intval < 0 || pcsa[1].value.intval > 4095)
	return_error(e_rangecheck);
    num_entries = (int)pcsa[1].value.intval + 1;
    cs = *gs_currentcolorspace(igs);
    if (!cs.type->can_be_base_space)
	return_error(e_rangecheck);
    cspace_old = istate->colorspace;
    /*
     * We can't count on C compilers to recognize the aliasing
     * that would be involved in a direct assignment.
     * Formerly, we used the following code:
	 cs_base = *(gs_direct_color_space *)&cs;
	 cs.params.indexed.base_space = cs_base;
     * But the Watcom C 10.0 compiler is too smart: it turns this into
     * a direct assignment (and compiles incorrect code for it),
     * defeating our purpose.  Instead, we have to do it by brute force:
     */
    memcpy(&cs_base, &cs, sizeof(cs_base));
    cs.params.indexed.base_space = cs_base;
    if (r_has_type(&pcsa[2], t_string)) {
	int num_values = num_entries * cs_num_components(&cs);

	check_read(pcsa[2]);
	if (r_size(&pcsa[2]) != num_values)
	    return_error(e_rangecheck);
	cs.params.indexed.lookup.table.data =
	    pcsa[2].value.const_bytes;
	cs.params.indexed.use_proc = 0;
	make_null(pproc);
	code = 0;
    } else {
	gs_indexed_map *map;

	check_proc(pcsa[2]);
	code = zcs_begin_map(&map, &pcsa[2], num_entries,
			     (const gs_base_color_space *)&cs,
			     indexed_map1);
	if (code < 0)
	    return code;
	cs.params.indexed.use_proc = 1;
	*pproc = pcsa[2];
	map->proc.lookup_index = lookup_indexed;
	cs.params.indexed.lookup.map = map;
    }
    cs.params.indexed.hival = num_entries - 1;
    cs.type = &gs_color_space_type_Indexed;
    code = gs_setcolorspace(igs, &cs);
    if (code < 0) {
	istate->colorspace = cspace_old;
	ref_stack_pop_to(&e_stack, edepth);
	return code;
    }
    pop(1);
    return (ref_stack_count(&e_stack) == edepth ? 0 : o_push_estack);	/* installation will load the caches */
}

/* Continuation procedure for saving mapped Indexed color values. */
private int
indexed_map1(os_ptr op)
{
    es_ptr ep = esp;
    int i = (int)ep[csme_index].value.intval;

    if (i >= 0) {		/* i.e., not first time */
	int m = (int)ep[csme_num_components].value.intval;
	int code = float_params(op, m, &r_ptr(&ep[csme_map], gs_indexed_map)->values[i * m]);

	if (code < 0)
	    return code;
	pop(m);
	op -= m;
	if (i == (int)ep[csme_hival].value.intval) {	/* All done. */
	    esp -= num_csme;
	    return o_pop_estack;
	}
    }
    push(1);
    ep[csme_index].value.intval = ++i;
    make_int(op, i);
    make_op_estack(ep + 1, indexed_map1);
    ep[2] = ep[csme_proc];	/* lookup proc */
    esp = ep + 2;
    return o_push_estack;
}

/* ------ Initialization procedure ------ */

const op_def zcsindex_l2_op_defs[] =
{
    op_def_begin_level2(),
    {"1.setindexedspace", zsetindexedspace},
		/* Internal operators */
    {"1%indexed_map1", indexed_map1},
    op_def_end(0)
};

/* ------ Internal routines ------ */

/* Allocate, and prepare to load, the index or tint map. */
int
zcs_begin_map(gs_indexed_map ** pmap, const ref * pproc, int num_entries,
	   const gs_base_color_space * base_space, int (*map1) (P1(os_ptr)))
{
    gs_memory_t *mem = gs_state_memory(igs);
    int num_components =
    cs_num_components((const gs_color_space *)base_space);
    int num_values = num_entries * num_components;
    gs_indexed_map *map;
    es_ptr ep;
    float *values;

    rc_alloc_struct_0(map, gs_indexed_map, &st_indexed_map,
		      mem, return_error(e_VMerror),
		      "setcolorspace(mapped)");
    values =
	(float *)gs_alloc_byte_array(mem, num_values, sizeof(float),
				     "setcolorspace(mapped)");

    if (values == 0) {
	gs_free_object(mem, map, "setcolorspace(mapped)");
	return_error(e_VMerror);
    }
    map->rc.free = rc_free_indexed_map;
    map->num_values = num_values;
    map->values = values;
    *pmap = map;
    /* Map the entire set of color indices.  Since the */
    /* o-stack may not be able to hold 4*4096 values, we have */
    /* to load them into the cache as they are generated. */
    check_estack(num_csme + 1);	/* 1 extra for map1 proc */
    ep = esp += num_csme;
    make_int(ep + csme_num_components, num_components);
    make_struct(ep + csme_map, imemory_space((gs_ref_memory_t *) mem), map);
    ep[csme_proc] = *pproc;
    make_int(ep + csme_hival, num_entries - 1);
    make_int(ep + csme_index, -1);
    push_op_estack(map1);
    return o_push_estack;
}
