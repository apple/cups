/* Copyright (C) 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gsshade.c,v 1.1 2000/03/08 23:14:47 mike Exp $ */
/* Constructors for shadings */
#include "gx.h"
#include "gscspace.h"
#include "gserrors.h"
#include "gsstruct.h"		/* for extern_st */
#include "gxdevcli.h"
#include "gxcpath.h"
#include "gxistate.h"
#include "gxpath.h"
#include "gxshade.h"

/* ================ Initialize shadings ================ */

/* ---------------- Generic services ---------------- */

/* GC descriptors */
private_st_shading();
private_st_shading_mesh();

/* Check ColorSpace, BBox, and Function (if present). */
/* Free variables: params. */
private int
check_CBFD(const gs_shading_params_t * params,
	   const gs_function_t * function, const float *domain, int m)
{
    int ncomp = gs_color_space_num_components(params->ColorSpace);

    if (ncomp < 0 ||
	(params->have_BBox &&
	 (params->BBox.p.x > params->BBox.q.x ||
	  params->BBox.p.y > params->BBox.q.y))
	)
	return_error(gs_error_rangecheck);
    if (function != 0) {
	if (function->params.m != m || function->params.n != ncomp)
	    return_error(gs_error_rangecheck);
	/*
	 * The Adobe documentation says that the function's domain must
	 * be a superset of the domain defined in the shading dictionary.
	 * However, Adobe implementations apparently don't necessarily
	 * check this ahead of time; therefore, we do the same.
	 */
#if 0				/*************** */
	{
	    int i;

	    for (i = 0; i < m; ++i)
		if (function->params.Domain[2 * i] > domain[2 * i] ||
		    function->params.Domain[2 * i + 1] < domain[2 * i + 1]
		    )
		    return_error(gs_error_rangecheck);
	}
#endif /*************** */
    }
    return 0;
}

/* Check parameters for a mesh shading. */
private int
check_mesh(const gs_shading_mesh_params_t * params)
{
    if (!data_source_is_array(params->DataSource)) {
	int code = check_CBFD((const gs_shading_params_t *)params,
			      params->Function, params->Decode, 1);

	if (code < 0)
	    return code;
	switch (params->BitsPerCoordinate) {
	    case  1: case  2: case  4: case  8:
	    case 12: case 16: case 24: case 32:
		break;
	    default:
		return_error(gs_error_rangecheck);
	}
	switch (params->BitsPerComponent) {
	    case  1: case  2: case  4: case  8:
	    case 12: case 16:
		break;
	    default:
		return_error(gs_error_rangecheck);
	}
    }
    return 0;
}

/* Check the BitsPerFlag value.  Return the value or an error code. */
private int
check_BPF(const gs_data_source_t *pds, int bpf)
{
    if (data_source_is_array(*pds))
	return 2;
    switch (bpf) {
    case 2: case 4: case 8:
	return bpf;
    default:
	return_error(gs_error_rangecheck);
    }
}

/* Initialize common shading parameters. */
private void
shading_params_init(gs_shading_params_t *params)
{
    params->ColorSpace = 0;	/* must be set by client */
    params->Background = 0;
    params->have_BBox = false;
    params->AntiAlias = false;
}

/* Initialize common mesh shading parameters. */
private void
mesh_shading_params_init(gs_shading_mesh_params_t *params)
{
    shading_params_init((gs_shading_params_t *)params);
    data_source_init_floats(&params->DataSource, NULL, 0);/* client must set */
    /* Client must set BitsPerCoordinate and BitsPerComponent */
    /* if DataSource is not an array. */
    params->Decode = 0;
    params->Function = 0;
}

/* Allocate and initialize a shading. */
/* Free variables: mem, params, ppsh, psh. */
#define ALLOC_SHADING(sttype, stype, sfrproc, cname)\
  BEGIN\
    psh = gs_alloc_struct(mem, void, sttype, cname);\
    if ( psh == 0 )\
      return_error(gs_error_VMerror);\
    psh->head.type = stype;\
    psh->head.fill_rectangle = sfrproc;\
    psh->params = *params;\
    *ppsh = (gs_shading_t *)psh;\
  END

/* ---------------- Function-based shading ---------------- */

private_st_shading_Fb();

/* Initialize parameters for a Function-based shading. */
void
gs_shading_Fb_params_init(gs_shading_Fb_params_t * params)
{
    shading_params_init((gs_shading_params_t *)params);
    params->Domain[0] = params->Domain[2] = 0;
    params->Domain[1] = params->Domain[3] = 1;
    gs_make_identity(&params->Matrix);
    params->Function = 0;	/* must be set by client */
}

/* Allocate and initialize a Function-based shading. */
int
gs_shading_Fb_init(gs_shading_t ** ppsh,
		   const gs_shading_Fb_params_t * params, gs_memory_t * mem)
{
    gs_shading_Fb_t *psh;
    gs_matrix imat;
    int code = check_CBFD((const gs_shading_params_t *)params,
			  params->Function, params->Domain, 2);

    if (code < 0 ||
	(code = gs_matrix_invert(&params->Matrix, &imat)) < 0
	)
	return code;
    ALLOC_SHADING(&st_shading_Fb, shading_type_Function_based,
		  gs_shading_Fb_fill_rectangle, "gs_shading_Fb_init");
    return 0;
}

/* ---------------- Axial shading ---------------- */

private_st_shading_A();

/* Initialize parameters for an Axial shading. */
void
gs_shading_A_params_init(gs_shading_A_params_t * params)
{
    shading_params_init((gs_shading_params_t *)params);
    /* Coords must be set by client */
    params->Domain[0] = 0;
    params->Domain[1] = 1;
    params->Function = 0;	/* must be set by client */
    params->Extend[0] = params->Extend[1] = false;
}

/* Allocate and initialize an Axial shading. */
int
gs_shading_A_init(gs_shading_t ** ppsh,
		  const gs_shading_A_params_t * params, gs_memory_t * mem)
{
    gs_shading_A_t *psh;
    int code = check_CBFD((const gs_shading_params_t *)params,
			  params->Function, params->Domain, 1);

    if (code < 0)
	return code;
    ALLOC_SHADING(&st_shading_A, shading_type_Axial,
		  gs_shading_A_fill_rectangle, "gs_shading_A_init");
    return 0;
}

/* ---------------- Radial shading ---------------- */

private_st_shading_R();

/* Initialize parameters for a Radial shading. */
void
gs_shading_R_params_init(gs_shading_R_params_t * params)
{
    shading_params_init((gs_shading_params_t *)params);
    /* Coords must be set by client */
    params->Domain[0] = 0;
    params->Domain[1] = 1;
    params->Function = 0;	/* must be set by client */
    params->Extend[0] = params->Extend[1] = false;
}

/* Allocate and initialize a Radial shading. */
int
gs_shading_R_init(gs_shading_t ** ppsh,
		  const gs_shading_R_params_t * params, gs_memory_t * mem)
{
    gs_shading_R_t *psh;
    int code = check_CBFD((const gs_shading_params_t *)params,
			  params->Function, params->Domain, 1);

    if (code < 0)
	return code;
    if ((params->Domain != 0 && params->Domain[0] == params->Domain[1]) ||
	params->Coords[2] < 0 || params->Coords[5] < 0
	)
	return_error(gs_error_rangecheck);
    ALLOC_SHADING(&st_shading_R, shading_type_Radial,
		  gs_shading_R_fill_rectangle, "gs_shading_R_init");
    return 0;
}

/* ---------------- Free-form Gouraud triangle mesh shading ---------------- */

private_st_shading_FfGt();

/* Initialize parameters for a Free-form Gouraud triangle mesh shading. */
void
gs_shading_FfGt_params_init(gs_shading_FfGt_params_t * params)
{
    mesh_shading_params_init((gs_shading_mesh_params_t *)params);
    /* Client must set BitsPerFlag if DataSource is not an array. */
}

/* Allocate and initialize a Free-form Gouraud triangle mesh shading. */
int
gs_shading_FfGt_init(gs_shading_t ** ppsh,
		     const gs_shading_FfGt_params_t * params,
		     gs_memory_t * mem)
{
    gs_shading_FfGt_t *psh;
    int code = check_mesh((const gs_shading_mesh_params_t *)params);
    int bpf = check_BPF(&params->DataSource, params->BitsPerFlag);

    if (code < 0)
	return code;
    if (bpf < 0)
	return bpf;
    if (params->Decode != 0 && params->Decode[0] == params->Decode[1])
	return_error(gs_error_rangecheck);
    ALLOC_SHADING(&st_shading_FfGt, shading_type_Free_form_Gouraud_triangle,
		  gs_shading_FfGt_fill_rectangle, "gs_shading_FfGt_init");
    psh->params.BitsPerFlag = bpf;
    return 0;
}

/* -------------- Lattice-form Gouraud triangle mesh shading -------------- */

private_st_shading_LfGt();

/* Initialize parameters for a Lattice-form Gouraud triangle mesh shading. */
void
gs_shading_LfGt_params_init(gs_shading_LfGt_params_t * params)
{
    mesh_shading_params_init((gs_shading_mesh_params_t *)params);
    /* Client must set VerticesPerRow. */
}

/* Allocate and initialize a Lattice-form Gouraud triangle mesh shading. */
int
gs_shading_LfGt_init(gs_shading_t ** ppsh,
		 const gs_shading_LfGt_params_t * params, gs_memory_t * mem)
{
    gs_shading_LfGt_t *psh;
    int code = check_mesh((const gs_shading_mesh_params_t *)params);

    if (code < 0)
	return code;
    if (params->VerticesPerRow < 2)
	return_error(gs_error_rangecheck);
    ALLOC_SHADING(&st_shading_LfGt, shading_type_Lattice_form_Gouraud_triangle,
		  gs_shading_LfGt_fill_rectangle, "gs_shading_LfGt_init");
    return 0;
}

/* ---------------- Coons patch mesh shading ---------------- */

private_st_shading_Cp();

/* Initialize parameters for a Coons patch mesh shading. */
void
gs_shading_Cp_params_init(gs_shading_Cp_params_t * params)
{
    mesh_shading_params_init((gs_shading_mesh_params_t *)params);
    /* Client must set BitsPerFlag if DataSource is not an array. */
}

/* Allocate and initialize a Coons patch mesh shading. */
int
gs_shading_Cp_init(gs_shading_t ** ppsh,
		   const gs_shading_Cp_params_t * params, gs_memory_t * mem)
{
    gs_shading_Cp_t *psh;
    int code = check_mesh((const gs_shading_mesh_params_t *)params);
    int bpf = check_BPF(&params->DataSource, params->BitsPerFlag);

    if (code < 0)
	return code;
    if (bpf < 0)
	return bpf;
    ALLOC_SHADING(&st_shading_Cp, shading_type_Coons_patch,
		  gs_shading_Cp_fill_rectangle, "gs_shading_Cp_init");
    psh->params.BitsPerFlag = bpf;
    return 0;
}

/* ---------------- Tensor product patch mesh shading ---------------- */

private_st_shading_Tpp();

/* Initialize parameters for a Tensor product patch mesh shading. */
void
gs_shading_Tpp_params_init(gs_shading_Tpp_params_t * params)
{
    mesh_shading_params_init((gs_shading_mesh_params_t *)params);
    /* Client must set BitsPerFlag if DataSource is not an array. */
}

/* Allocate and initialize a Tensor product patch mesh shading. */
int
gs_shading_Tpp_init(gs_shading_t ** ppsh,
		  const gs_shading_Tpp_params_t * params, gs_memory_t * mem)
{
    gs_shading_Tpp_t *psh;
    int code = check_mesh((const gs_shading_mesh_params_t *)params);
    int bpf = check_BPF(&params->DataSource, params->BitsPerFlag);

    if (code < 0)
	return code;
    if (bpf < 0)
	return bpf;
    ALLOC_SHADING(&st_shading_Tpp, shading_type_Tensor_product_patch,
		  gs_shading_Tpp_fill_rectangle, "gs_shading_Tpp_init");
    psh->params.BitsPerFlag = bpf;
    return 0;
}

/* ================ Shading rendering ================ */

/* Fill a path with a shading. */
int
gs_shading_fill_path(const gs_shading_t *psh, const gx_path *ppath,
		     gx_device *orig_dev, gs_imager_state *pis)
{
    gs_memory_t *mem = pis->memory;
    gx_device *dev = orig_dev;
    gs_fixed_rect path_box;
    gs_rect rect;
    gx_clip_path *box_clip = 0;
    gx_clip_path *path_clip = 0;
    gx_device_clip box_dev, path_dev;
    int code;

#if 0				/****** NOT IMPLEMENTED YET *****/
    if (psh->params.have_BBox) {
	box_clip = gx_cpath_alloc(mem, "shading_fill_path(box_clip)");
	if (box_clip == 0)
	    return_error(gs_error_VMerror);
	/****** APPEND TRANSFORMED BOX ******/
	gx_make_clip_device(&box_dev, &box_dev, box_clip->list);
	box_dev.target = dev;
	dev = &box_dev;
	dev_proc(dev, open_device)(dev);
    }
#endif
    dev_proc(dev, get_clipping_box)(dev, &path_box);
#if 0				/****** NOT IMPLEMENTED YET *****/
    if (ppath) {
	if (psh->params.Background) {
	    /****** FILL BOX WITH BACKGROUND ******/
	}
	path_clip = gx_cpath_alloc(mem, "shading_fill_path(path_clip)");
	if (path_clip == 0) {
	    code = gs_note_error(gs_error_VMerror);
	    goto out;
	}
	/****** SET CLIP PATH ******/
	gx_make_clip_device(&path_dev, &path_dev, path_clip->list);
	path_dev.target = dev;
	dev = &path_dev;
	dev_proc(dev, open_device)(dev);
	dev_proc(dev, get_clipping_box)(dev, &path_box);
    }
#endif
    {
	gs_rect path_rect;
	const gs_matrix *pmat = &ctm_only(pis);

	path_rect.p.x = fixed2float(path_box.p.x);
	path_rect.p.y = fixed2float(path_box.p.y);
	path_rect.q.x = fixed2float(path_box.q.x);
	path_rect.q.y = fixed2float(path_box.q.y);
	gs_bbox_transform_inverse(&path_rect, pmat, &rect);
    }
    code = psh->head.fill_rectangle(psh, &rect, dev, pis);
out:
    if (path_clip)
	gx_cpath_free(path_clip, "shading_fill_path(path_clip)");
    if (box_clip)
	gx_cpath_free(box_clip, "shading_fill_path(box_clip)");
    return code;
}
