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

/*$Id: zshade.c,v 1.1 2000/03/08 23:15:42 mike Exp $ */
/* PostScript language interface to shading */
#include "memory_.h"
#include "ghost.h"
#include "oper.h"
#include "gscolor3.h"
#include "gscspace.h"
#include "gscolor2.h"		/* requires gscspace.h */
#include "gsfunc3.h"
#include "gsstruct.h"		/* must precede gsshade.h */
#include "gsshade.h"
#include "gsuid.h"
#include "stream.h"		/* for files.h */
#include "files.h"
#include "ialloc.h"
#include "idict.h"
#include "idparam.h"
#include "ifunc.h"
#include "igstate.h"
#include "store.h"

/* Forward references */
private int shading_param(P2(const_os_ptr op, const gs_shading_t ** ppsh));

/* ---------------- Standard operators ---------------- */

/* - currentsmoothness <smoothness> */
private int
zcurrentsmoothness(register os_ptr op)
{
    push(1);
    make_real(op, gs_currentsmoothness(igs));
    return 0;
}

/* <smoothness> setsmoothness - */
private int
zsetsmoothness(register os_ptr op)
{
    double smoothness;
    int code;

    if (real_param(op, &smoothness) < 0)
	return_op_typecheck(op);
    if ((code = gs_setsmoothness(igs, smoothness)) < 0)
	return code;
    pop(1);
    return 0;
}

/* <shading> .shfill - */
private int
zshfill(register os_ptr op)
{
    const gs_shading_t *psh;
    int code = shading_param(op, &psh);

    if (code < 0 || (code = gs_shfill(igs, psh)) < 0)
	return code;
    pop(1);
    return 0;
}

/* ------ Non-standard operators ------ */

/* <pattern> <matrix> <shading> .buildshadingpattern <pattern> <instance> */
private int
zbuildshadingpattern(os_ptr op)
{
    os_ptr op2 = op - 2;
    const gs_shading_t *psh;
    gs_matrix mat;
    struct {
	gs_uid uid;
    } template;		/****** WRONG ******/
    int code;

    check_type(*op2, t_dictionary);
    check_dict_read(*op2);

    if ((code = shading_param(op, &psh)) < 0 ||
	(code = read_matrix(op - 1, &mat)) < 0 ||
	(code = dict_uid_param(op2, &template.uid, 1, imemory)) != 1
	)
	return_error((code < 0 ? code : e_rangecheck));
    /****** NYI ******/
    return_error(e_undefined);
}

/* ------ Internal procedures ------ */

/* Get a shading parameter. */
private int
shading_param(const_os_ptr op, const gs_shading_t ** ppsh)
{	/*
	 * Since shadings form a subclass hierarchy, we currently have
	 * no way to check whether a structure is actually a shading.
	 */
    if (!r_is_struct(op) ||
	r_has_masked_attrs(op, a_executable | a_execute, a_all)
	)
	return_error(e_typecheck);
    *ppsh = (gs_shading_t *) op->value.pstruct;
    return 0;
}

/* ---------------- Shading dictionaries ---------------- */

/* ------ Common code ------ */

extern_st(st_color_space);

typedef int (*build_shading_proc_t)(P3(const ref *op,
				       const gs_shading_params_t *params,
				       gs_shading_t **ppsh));

/* Operators */

/* Common framework for building shadings. */
private int
build_shading(ref * op, build_shading_proc_t proc)
{
    int code;
    float box[4];
    gs_shading_params_t params;
    gs_shading_t *psh;
    ref *pvalue;

    check_type(*op, t_dictionary);
    params.ColorSpace = 0;
    params.Background = 0;
    /* Collect parameters common to all shading types. */
    {
	const gs_color_space *pcs_orig = gs_currentcolorspace(igs);
	int num_comp = gs_color_space_num_components(pcs_orig);
	gs_color_space *pcs;

	if (num_comp < 0)	/* Pattern color space */
	    return_error(e_rangecheck);
	pcs = ialloc_struct(gs_color_space, &st_color_space,
			    "build_shading");
	if (pcs == 0)
	    return_error(e_VMerror);
	gs_cspace_init_from(pcs, pcs_orig);
	params.ColorSpace = pcs;
	if (dict_find_string(op, "Background", &pvalue) > 0) {
	    gs_client_color *pcc =
		ialloc_struct(gs_client_color, &st_client_color,
			      "build_shading");

	    if (pcc == 0) {
		code = gs_note_error(e_VMerror);
		goto fail;
	    }
	    pcc->pattern = 0;
	    params.Background = pcc;
	    code = dict_float_array_param(op, "Background",
					  countof(pcc->paint.values),
					  pcc->paint.values, NULL);
	    if (code != gs_color_space_num_components(pcs))
		goto fail;
	}
    }
    if (dict_find_string(op, "BBox", &pvalue) <= 0)
	params.have_BBox = false;
    else if ((code = dict_float_array_param(op, "BBox", 4, box, NULL)) == 4) {
	params.BBox.p.x = box[0];
	params.BBox.p.y = box[1];
	params.BBox.q.x = box[2];
	params.BBox.q.y = box[3];
	params.have_BBox = true;
    } else
	goto fail;
    code = dict_bool_param(op, "AntiAlias", false, &params.AntiAlias);
    if (code < 0)
	goto fail;
    /* Finish building the shading. */
    code = (*proc) (op, &params, &psh);
    if (code < 0)
	goto fail;
    make_istruct_new(op, 0, psh);
    return code;
fail:
    ifree_object(params.Background, "Background");
    if (params.ColorSpace) {
	gs_cspace_release(params.ColorSpace);
	ifree_object(params.ColorSpace, "ColorSpace");
    }
    return (code < 0 ? code : gs_note_error(e_rangecheck));
}

/* Collect a Function value. */
private int
build_shading_function(const ref * op, gs_function_t ** ppfn, int num_inputs)
{
    ref *pFunction;

    *ppfn = 0;
    if (dict_find_string(op, "Function", &pFunction) <= 0)
	return 0;
    if (r_is_array(pFunction)) {
	uint size = r_size(pFunction);
	gs_function_t **Functions;
	uint i;
	gs_function_AdOt_params_t params;
	int code;

	check_read(*pFunction);
	if (size == 0)
	    return_error(e_rangecheck);
	code = ialloc_function_array(size, &Functions);
	if (code < 0)
	    return code;
	for (i = 0; i < size; ++i) {
	    ref rsubfn;

	    array_get(op, (long)i, &rsubfn);
	    code = fn_build_function(&rsubfn, &Functions[i]);
	    if (code < 0)
		break;
	}
	params.m = 1;
	params.Domain = 0;
	params.n = size;
	params.Range = 0;
	params.Functions = (const gs_function_t * const *)Functions;
	if (code >= 0)
	    code = gs_function_AdOt_init(ppfn, &params, imemory);
	if (code < 0)
	    gs_function_AdOt_free_params(&params, imemory);
	return code;
    } else
	return fn_build_function(pFunction, ppfn);
}

/* ------ Build shadings ------ */

/* Build a ShadingType 1 (Function-based) shading. */
private int
build_shading_1(const ref * op, const gs_shading_params_t * pcommon,
		gs_shading_t ** ppsh)
{
    gs_shading_Fb_params_t params;
    int code;
    static const float default_Domain[4] = {0, 1, 0, 1};

    *(gs_shading_params_t *)&params = *pcommon;
    gs_make_identity(&params.Matrix);
    params.Function = 0;
    if ((code = dict_float_array_param(op, "Domain", 4, params.Domain,
				       default_Domain)) != 4 ||
	(code = dict_matrix_param(op, "Matrix", &params.Matrix)) < 0 ||
	(code = build_shading_function(op, &params.Function, 2)) < 0 ||
	(code = gs_shading_Fb_init(ppsh, &params, imemory)) < 0
	) {
	ifree_object(params.Function, "Function");
	return (code < 0 ? code : gs_note_error(e_rangecheck));
    }
    return 0;
}
/* <dict> .buildshading1 <shading_struct> */
private int
zbuildshading1(os_ptr op)
{
    return build_shading(op, build_shading_1);
}

/* Collect parameters for an Axial or Radial shading. */
private int
build_directional_shading(const ref * op, float *Coords, int num_Coords,
		float Domain[2], gs_function_t ** pFunction, bool Extend[2])
{
    int code =
	dict_float_array_param(op, "Coords", num_Coords, Coords, NULL);
    static const float default_Domain[2] = {0, 1};
    ref *pExtend;

    *pFunction = 0;
    if (code != num_Coords ||
	(code = dict_float_array_param(op, "Domain", 2, Domain,
				       default_Domain)) != 2 ||
	(code = build_shading_function(op, pFunction, 1)) < 0
	)
	return (code < 0 ? code : gs_note_error(e_rangecheck));
    if (dict_find_string(op, "Extend", &pExtend) <= 0)
	Extend[0] = Extend[1] = false;
    else {
	ref E0, E1;

	if (!r_is_array(pExtend))
	    return_error(e_typecheck);
	else if (r_size(pExtend) != 2)
	    return_error(e_rangecheck);
	else if ((array_get(pExtend, 0L, &E0), !r_has_type(&E0, t_boolean)) ||
		 (array_get(pExtend, 1L, &E1), !r_has_type(&E1, t_boolean))
	    )
	    return_error(e_typecheck);
	Extend[0] = E0.value.boolval, Extend[1] = E1.value.boolval;
    }
    return 0;
}

/* Build a ShadingType 2 (Axial) shading. */
private int
build_shading_2(const ref * op, const gs_shading_params_t * pcommon,
		gs_shading_t ** ppsh)
{
    gs_shading_A_params_t params;
    int code;

    *(gs_shading_params_t *)&params = *pcommon;
    if ((code = build_directional_shading(op, params.Coords, 4,
					  params.Domain, &params.Function,
					  params.Extend)) < 0 ||
	(code = gs_shading_A_init(ppsh, &params, imemory)) < 0
	) {
	ifree_object(params.Function, "Function");
    }
    return code;
}
/* <dict> .buildshading2 <shading_struct> */
private int
zbuildshading2(os_ptr op)
{
    return build_shading(op, build_shading_2);
}

/* Build a ShadingType 3 (Radial) shading. */
private int
build_shading_3(const ref * op, const gs_shading_params_t * pcommon,
		gs_shading_t ** ppsh)
{
    gs_shading_R_params_t params;
    int code;

    *(gs_shading_params_t *)&params = *pcommon;
    if ((code = build_directional_shading(op, params.Coords, 6,
					  params.Domain, &params.Function,
					  params.Extend)) < 0 ||
	(code = gs_shading_R_init(ppsh, &params, imemory)) < 0
	) {
	ifree_object(params.Function, "Function");
    }
    return code;
}
/* <dict> .buildshading3 <shading_struct> */
private int
zbuildshading3(os_ptr op)
{
    return build_shading(op, build_shading_3);
}

/* Collect parameters for a mesh shading. */
private int
build_mesh_shading(const ref * op, gs_shading_mesh_params_t * params,
		   float **pDecode, gs_function_t ** pFunction)
{
    int code;
    ref *pDataSource;

    *pDecode = 0;
    *pFunction = 0;
    if (dict_find_string(op, "DataSource", &pDataSource) <= 0)
	return_error(e_rangecheck);
    if (r_is_array(pDataSource)) {
	uint size = r_size(pDataSource);
	float *data =
	    (float *)ialloc_byte_array(size, sizeof(float),
				       "build_mesh_shading");
	int code;

	if (data == 0)
	    return_error(e_VMerror);
	code = float_params(pDataSource->value.refs + size - 1, size, data);
	if (code < 0) {
	    ifree_object(data, "build_mesh_shading");
	    return code;
	}
	data_source_init_floats(&params->DataSource, data, size);
    } else
	switch (r_type(pDataSource)) {
	    case t_file: {
		stream *s;

		check_read_file(s, pDataSource);
		data_source_init_stream(&params->DataSource, s);
		break;
	    }
	    case t_string:
		check_read(*pDataSource);
		data_source_init_string2(&params->DataSource,
					 pDataSource->value.bytes,
					 r_size(pDataSource));
		break;
	    default:
		return_error(e_typecheck);
	}
    if (data_source_is_array(params->DataSource)) {
	params->BitsPerCoordinate = 0;
	params->BitsPerComponent = 0;
    } else {
	int num_decode =
	    4 + gs_color_space_num_components(params->ColorSpace) * 2;

/****** FREE FLOAT ARRAY DATA ON ERROR ******/
	if ((code = dict_int_param(op, "BitsPerCoordinate", 1, 32, 0,
				   &params->BitsPerCoordinate)) < 0 ||
	    (code = dict_int_param(op, "BitsPerComponent", 1, 16, 0,
				   &params->BitsPerComponent)) < 0
	    )
	    return code;
	*pDecode = (float *)ialloc_byte_array(num_decode, sizeof(float),
					      "build_mesh_shading");

	if (*pDecode == 0)
	    return_error(e_VMerror);
	code = dict_float_array_param(op, "Decode", num_decode, *pDecode,
				      NULL);
	if (code != num_decode) {
	    ifree_object(*pDecode, "build_mesh_shading");
	    *pDecode = 0;
	    return (code < 0 ? code : gs_note_error(e_rangecheck));
	}
    }
    code = build_shading_function(op, pFunction, 1);
    if (code < 0) {
	ifree_object(*pDecode, "build_mesh_shading");
	*pDecode = 0;
    }
    return code;
}

/* Collect the BitsPerFlag parameter, if relevant. */
private int
flag_bits_param(const ref * op, const gs_shading_mesh_params_t * params,
		int *pBitsPerFlag)
{
    if (data_source_is_array(params->DataSource)) {
	*pBitsPerFlag = 0;
	return 0;
    } else {
	return dict_int_param(op, "BitsPerFlag", 2, 8, 0, pBitsPerFlag);
    }
}

/* Build a ShadingType 4 (Free-form Gouraud triangle mesh) shading. */
private int
build_shading_4(const ref * op, const gs_shading_params_t * pcommon,
		gs_shading_t ** ppsh)
{
    gs_shading_FfGt_params_t params;
    int code;

    *(gs_shading_params_t *)&params = *pcommon;
    if ((code =
	 build_mesh_shading(op, (gs_shading_mesh_params_t *)&params,
			    &params.Decode, &params.Function)) < 0 ||
	(code = flag_bits_param(op, (gs_shading_mesh_params_t *)&params,
				&params.BitsPerFlag)) < 0 ||
	(code = gs_shading_FfGt_init(ppsh, &params, imemory)) < 0
	) {
	ifree_object(params.Function, "Function");
	ifree_object(params.Decode, "Decode");
    }
    return code;
}
/* <dict> .buildshading4 <shading_struct> */
private int
zbuildshading4(os_ptr op)
{
    return build_shading(op, build_shading_4);
}

/* Build a ShadingType 5 (Lattice-form Gouraud triangle mesh) shading. */
private int
build_shading_5(const ref * op, const gs_shading_params_t * pcommon,
		gs_shading_t ** ppsh)
{
    gs_shading_LfGt_params_t params;
    int code;

    *(gs_shading_params_t *)&params = *pcommon;
    if ((code =
	 build_mesh_shading(op, (gs_shading_mesh_params_t *)&params,
			    &params.Decode, &params.Function)) < 0 ||
	(code = dict_int_param(op, "VerticesPerRow", 2, max_int, 0,
			       &params.VerticesPerRow)) < 0 ||
	(code = gs_shading_LfGt_init(ppsh, &params, imemory)) < 0
	) {
	ifree_object(params.Function, "Function");
	ifree_object(params.Decode, "Decode");
    }
    return code;
}
/* <dict> .buildshading5 <shading_struct> */
private int
zbuildshading5(os_ptr op)
{
    return build_shading(op, build_shading_5);
}

/* Build a ShadingType 6 (Coons patch mesh) shading. */
private int
build_shading_6(const ref * op, const gs_shading_params_t * pcommon,
		gs_shading_t ** ppsh)
{
    gs_shading_Cp_params_t params;
    int code;

    *(gs_shading_params_t *)&params = *pcommon;
    if ((code =
	 build_mesh_shading(op, (gs_shading_mesh_params_t *)&params,
			    &params.Decode, &params.Function)) < 0 ||
	(code = flag_bits_param(op, (gs_shading_mesh_params_t *)&params,
				&params.BitsPerFlag)) < 0 ||
	(code = gs_shading_Cp_init(ppsh, &params, imemory)) < 0
	) {
	ifree_object(params.Function, "Function");
	ifree_object(params.Decode, "Decode");
    }
    return code;
}
/* <dict> .buildshading6 <shading_struct> */
private int
zbuildshading6(os_ptr op)
{
    return build_shading(op, build_shading_6);
}

/* Build a ShadingType 7 (Tensor product patch mesh) shading. */
private int
build_shading_7(const ref * op, const gs_shading_params_t * pcommon,
		gs_shading_t ** ppsh)
{
    gs_shading_Tpp_params_t params;
    int code;

    *(gs_shading_params_t *)&params = *pcommon;
    if ((code =
	 build_mesh_shading(op, (gs_shading_mesh_params_t *)&params,
			    &params.Decode, &params.Function)) < 0 ||
	(code = flag_bits_param(op, (gs_shading_mesh_params_t *)&params,
				&params.BitsPerFlag)) < 0 ||
	(code = gs_shading_Tpp_init(ppsh, &params, imemory)) < 0
	) {
	ifree_object(params.Function, "Function");
	ifree_object(params.Decode, "Decode");
    }
    return code;
}
/* <dict> .buildshading7 <shading_struct> */
private int
zbuildshading7(os_ptr op)
{
    return build_shading(op, build_shading_7);
}

/* ------ Initialization procedure ------ */

const op_def zshade_op_defs[] =
{
    op_def_begin_ll3(),
    {"0currentsmoothness", zcurrentsmoothness},
    {"1setsmoothness", zsetsmoothness},
    {"1.shfill", zshfill},
    {"1.buildshading1", zbuildshading1},
    {"1.buildshading2", zbuildshading2},
    {"1.buildshading3", zbuildshading3},
    {"1.buildshading4", zbuildshading4},
    {"1.buildshading5", zbuildshading5},
    {"1.buildshading6", zbuildshading6},
    {"1.buildshading7", zbuildshading7},
    {"3.buildshadingpattern", zbuildshadingpattern},
    op_def_end(0)
};
