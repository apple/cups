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

/*$Id: gxshade4.c,v 1.1 2000/03/08 23:15:05 mike Exp $ */
/* Rendering for Gouraud triangle shadings */
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsmatrix.h"		/* for gscoord.h */
#include "gscoord.h"
#include "gxcspace.h"
#include "gxdcolor.h"
#include "gxdevcli.h"
#include "gxistate.h"
#include "gxpath.h"
#include "gxshade.h"
#include "gxshade4.h"

/* ---------------- Triangle mesh filling ---------------- */

/* Initialize the fill state for triangle shading. */
void
mesh_init_fill_state(mesh_fill_state_t * pfs, const gs_shading_mesh_t * psh,
		     const gs_rect * rect, gx_device * dev,
		     gs_imager_state * pis)
{
    shade_init_fill_state((shading_fill_state_t *) pfs,
			  (const gs_shading_t *)psh, dev, pis);
    pfs->pshm = psh;
    shade_bbox_transform2fixed(rect, pis, &pfs->rect);
}

#define SET_MIN_MAX_3(vmin, vmax, a, b, c)\
  if ( a < b ) vmin = a, vmax = b; else vmin = b, vmax = a;\
  if ( c < vmin ) vmin = c; else if ( c > vmax ) vmax = c

int
mesh_fill_triangle(const mesh_fill_state_t * pfs, const mesh_vertex_t *va,
		   const mesh_vertex_t *vb, const mesh_vertex_t *vc,
		   bool check)
{
    const gs_shading_mesh_t *psh = pfs->pshm;
    int ci;

    /*
     * Fill the triangle with vertices at va->p, vb->p, and vc->p
     * with color va->cc.
     * If check is true, check for whether the triangle is entirely
     * inside the rectangle, entirely outside, or partly inside;
     * if check is false, assume the triangle is entirely inside.
     */
    if (check) {
	fixed xmin, ymin, xmax, ymax;

	SET_MIN_MAX_3(xmin, xmax, va->p.x, vb->p.x, vc->p.x);
	SET_MIN_MAX_3(ymin, ymax, va->p.y, vb->p.y, vc->p.y);
	if (xmin >= pfs->rect.p.x && xmax <= pfs->rect.q.x &&
	    ymin >= pfs->rect.p.y && ymax <= pfs->rect.q.y
	    ) {
	    /* The triangle is entirely inside the rectangle. */
	    check = false;
	} else if (xmin >= pfs->rect.q.x || xmax <= pfs->rect.p.x ||
		   ymin >= pfs->rect.q.y || ymax <= pfs->rect.p.y
	    ) {
	    /* The triangle is entirely outside the rectangle. */
	    return 0;
	}
    }
    /* Check whether the colors fall within the smoothness criterion. */
    for (ci = 0; ci < pfs->num_components; ++ci) {
	float c0 = va->cc[ci], c1 = vb->cc[ci], c2 = vc->cc[ci];
	float cmin, cmax;

	SET_MIN_MAX_3(cmin, cmax, c0, c1, c2);
	if (cmax - cmin > pfs->cc_max_error[ci])
	    goto recur;
    }
    /* Fill the triangle with the color. */
    {
	gx_device_color dev_color;
	const gs_color_space *pcs = psh->params.ColorSpace;
	gs_imager_state *pis = pfs->pis;
	gs_client_color fcc;
	int code;

	memcpy(&fcc.paint, va->cc, sizeof(fcc.paint));
	(*pcs->type->restrict_color)(&fcc, pcs);
	(*pcs->type->remap_color)(&fcc, pcs, &dev_color, pis,
				  pfs->dev, gs_color_select_texture);
/****** SHOULD ADD adjust ON ANY OUTSIDE EDGES ******/
#if 0
	{
	    gx_path *ppath = gx_path_alloc(pis->memory, "Gt_fill");

	    gx_path_add_point(ppath, va->p.x, va->p.y);
	    gx_path_add_line(ppath, vb->p.x, vb->p.y);
	    gx_path_add_line(ppath, vc->p.x, vc->p.y);
	    code = shade_fill_path((const shading_fill_state_t *)pfs,
				   ppath, &dev_color);
	    gx_path_free(ppath, "Gt_fill");
	}
#else
	code = (*dev_proc(pfs->dev, fill_triangle))
	    (pfs->dev, va->p.x, va->p.y,
	     vb->p.x - va->p.x, vb->p.y - va->p.y,
	     vc->p.x - va->p.x, vc->p.y - va->p.y,
	     &dev_color, pis->log_op);
#endif
	return code;
    }
    /*
     * Subdivide the triangle and recur.  The only subdivision method
     * that doesn't seem to create anomalous shapes divides the
     * triangle in 4, using the midpoints of each side.
     */
recur:
    {
	mesh_vertex_t vab, vac, vbc;
	int i;
	int code;

#define MIDPOINT_FAST(a,b) arith_rshift_1((a) + (b) + 1)
	vab.p.x = MIDPOINT_FAST(va->p.x, vb->p.x);
	vab.p.y = MIDPOINT_FAST(va->p.y, vb->p.y);
	vac.p.x = MIDPOINT_FAST(va->p.x, vc->p.x);
	vac.p.y = MIDPOINT_FAST(va->p.y, vc->p.y);
	vbc.p.x = MIDPOINT_FAST(vb->p.x, vc->p.x);
	vbc.p.y = MIDPOINT_FAST(vb->p.y, vc->p.y);
#undef MIDPOINT_FAST
	for (i = 0; i < pfs->num_components; ++i) {
	    float ta = va->cc[i], tb = vb->cc[i], tc = vc->cc[i];

	    vab.cc[i] = (ta + tb) * 0.5;
	    vac.cc[i] = (ta + tc) * 0.5;
	    vbc.cc[i] = (tb + tc) * 0.5;
	}
	/* Do the "A" triangle. */
	code = mesh_fill_triangle(pfs, va, &vab, &vac, check);
	if (code < 0)
	    return code;
	/* Do the central triangle. */
	code = mesh_fill_triangle(pfs, &vab, &vac, &vbc, check);
	if (code < 0)
	    return code;
	/* Do the "C" triangle. */
	code = mesh_fill_triangle(pfs, &vac, &vbc, vc, check);
	if (code < 0)
	    return code;
	/* Do the "B" triangle. */
	return mesh_fill_triangle(pfs, &vab, vb, &vbc, check);
    }
}

/* ---------------- Gouraud triangle shadings ---------------- */

private int
Gt_next_vertex(const gs_shading_mesh_t * psh, shade_coord_stream_t * cs,
	       mesh_vertex_t * vertex)
{
    int code = shade_next_vertex(cs, vertex);

    if (code >= 0 && psh->params.Function) {
	/* Decode the color with the function. */
	code = gs_function_evaluate(psh->params.Function, vertex->cc,
				    vertex->cc);
    }
    return code;
}

inline private int
Gt_fill_triangle(const mesh_fill_state_t * pfs, const mesh_vertex_t * va,
		 const mesh_vertex_t * vb, const mesh_vertex_t * vc)
{
    return mesh_fill_triangle(pfs, va, vb, vc, true);
}

int
gs_shading_FfGt_fill_rectangle(const gs_shading_t * psh0, const gs_rect * rect,
			       gx_device * dev, gs_imager_state * pis)
{
    const gs_shading_FfGt_t * const psh = (const gs_shading_FfGt_t *)psh0;
    mesh_fill_state_t state;
    shade_coord_stream_t cs;
    int num_bits = psh->params.BitsPerFlag;
    int flag;
    mesh_vertex_t va, vb, vc;

    mesh_init_fill_state(&state, (const gs_shading_mesh_t *)psh, rect,
			 dev, pis);
    shade_next_init(&cs, (const gs_shading_mesh_params_t *)&psh->params,
		    pis);
    while ((flag = shade_next_flag(&cs, num_bits)) >= 0) {
	int code;

	switch (flag) {
	    default:
		return_error(gs_error_rangecheck);
	    case 0:
		if ((code = Gt_next_vertex(state.pshm, &cs, &va)) < 0 ||
		    (code = shade_next_flag(&cs, num_bits)) < 0 ||
		    (code = Gt_next_vertex(state.pshm, &cs, &vb)) < 0 ||
		    (code = shade_next_flag(&cs, num_bits)) < 0
		    )
		    return code;
		goto v2;
	    case 1:
		va = vb;
	    case 2:
		vb = vc;
v2:		if ((code = Gt_next_vertex(state.pshm, &cs, &vc)) < 0 ||
		    (code = Gt_fill_triangle(&state, &va, &vb, &vc)) < 0
		    )
		    return code;
	}
    }
    return 0;
}

int
gs_shading_LfGt_fill_rectangle(const gs_shading_t * psh0, const gs_rect * rect,
			       gx_device * dev, gs_imager_state * pis)
{
    const gs_shading_LfGt_t * const psh = (const gs_shading_LfGt_t *)psh0;
    mesh_fill_state_t state;
    shade_coord_stream_t cs;
    mesh_vertex_t *vertex;
    mesh_vertex_t next;
    int per_row = psh->params.VerticesPerRow;
    int i, code = 0;

    mesh_init_fill_state(&state, (const gs_shading_mesh_t *)psh, rect,
			 dev, pis);
    shade_next_init(&cs, (const gs_shading_mesh_params_t *)&psh->params,
		    pis);
    vertex = (mesh_vertex_t *)
	gs_alloc_byte_array(pis->memory, per_row, sizeof(*vertex),
			    "gs_shading_LfGt_render");
    if (vertex == 0)
	return_error(gs_error_VMerror);
    for (i = 0; i < per_row; ++i)
	if ((code = Gt_next_vertex(state.pshm, &cs, &vertex[i])) < 0)
	    goto out;
    while (!seofp(cs.s)) {
	code = Gt_next_vertex(state.pshm, &cs, &next);
	if (code < 0)
	    goto out;
	for (i = 1; i < per_row; ++i) {
	    code = Gt_fill_triangle(&state, &vertex[i - 1], &vertex[i], &next);
	    if (code < 0)
		goto out;
	    vertex[i - 1] = next;
	    code = Gt_next_vertex(state.pshm, &cs, &next);
	    if (code < 0)
		goto out;
	    code = Gt_fill_triangle(&state, &vertex[i], &vertex[i - 1], &next);
	    if (code < 0)
		goto out;
	}
	vertex[per_row - 1] = next;
    }
out:
    gs_free_object(pis->memory, vertex, "gs_shading_LfGt_render");
    return code;
}
