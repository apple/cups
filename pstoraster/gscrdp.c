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

/*$Id: gscrdp.c,v 1.2 2000/10/13 01:04:41 mike Exp $ */
/* CIE color rendering dictionary creation */
#include "math_.h"
#include "memory_.h"
#include "gx.h"
#include "gsdevice.h"
#include "gserrors.h"
#include "gsmatrix.h"		/* for gscolor2.h */
#include "gxcspace.h"
#include "gscolor2.h"		/* for gs_set/currentcolorrendering */
#include "gscrdp.h"
#include "gxarith.h"

/* Define the CRD type that we use here. */
#define CRD_TYPE 101

/* ---------------- Writing ---------------- */

/* Internal procedures for writing parameter values. */
private void
store_vector3(float *p, const gs_vector3 * pvec)
{
    p[0] = pvec->u, p[1] = pvec->v, p[2] = pvec->w;
}
private int
write_floats(gs_param_list * plist, gs_param_name key,
	     const float *values, int size, gs_memory_t * mem)
{
    float *p = (float *)
	gs_alloc_byte_array(mem, size, sizeof(float), "write_floats");
    gs_param_float_array fa;

    if (p == 0)
	return_error(gs_error_VMerror);
    memcpy(p, values, size * sizeof(float));

    fa.data = p;
    fa.size = size;
    fa.persistent = true;
    return param_write_float_array(plist, key, &fa);
}
private int
write_vector3(gs_param_list * plist, gs_param_name key,
	      const gs_vector3 * pvec, gs_memory_t * mem)
{
    float values[3];

    store_vector3(values, pvec);
    return write_floats(plist, key, values, 3, mem);
}
private int
write_matrix3(gs_param_list * plist, gs_param_name key,
	      const gs_matrix3 * pmat, gs_memory_t * mem)
{
    float values[9];

    if (!memcmp(pmat, &Matrix3_default, sizeof(*pmat)))
	return 0;
    store_vector3(values, &pmat->cu);
    store_vector3(values + 3, &pmat->cv);
    store_vector3(values + 6, &pmat->cw);
    return write_floats(plist, key, values, 9, mem);
}
private int
write_range3(gs_param_list * plist, gs_param_name key,
	     const gs_range3 * prange, gs_memory_t * mem)
{
    float values[6];

    if (!memcmp(prange, &Range3_default, sizeof(*prange)))
	return 0;
    values[0] = prange->ranges[0].rmin, values[1] = prange->ranges[0].rmax;
    values[2] = prange->ranges[1].rmin, values[3] = prange->ranges[1].rmax;
    values[4] = prange->ranges[2].rmin, values[5] = prange->ranges[2].rmax;
    return write_floats(plist, key, values, 6, mem);
}
private int
write_proc3(gs_param_list * plist, gs_param_name key,
	    const gs_cie_render * pcrd, const gs_cie_render_proc3 * procs,
	    const gs_range3 * domain, gs_memory_t * mem)
{
    float *values;
    uint size = gx_cie_cache_size;
    gs_param_float_array fa;
    int i;

    if (!memcmp(procs, &Encode_default, sizeof(*procs)))
	return 0;
    values = (float *)gs_alloc_byte_array(mem, size * 3, sizeof(float),
					  "write_proc3");

    if (values == 0)
	return_error(gs_error_VMerror);
    for (i = 0; i < 3; ++i) {
	double base = domain->ranges[i].rmin;
	double scale = (domain->ranges[i].rmax - base) / (size - 1);
	int j;

	for (j = 0; j < size; ++j)
	    values[i * size + j] =
		(*procs->procs[i]) (j * scale + base, pcrd);
    }
    fa.data = values;
    fa.size = size * 3;
    fa.persistent = true;
    return param_write_float_array(plist, key, &fa);
}

/* Write a CRD as a device parameter. */
int
param_write_cie_render1(gs_param_list * plist, gs_param_name key,
			const gs_cie_render * pcrd, gs_memory_t * mem)
{
    gs_param_dict dict;
    int code, dcode;

    dict.size = 20;
    if ((code = param_begin_write_dict(plist, key, &dict, false)) < 0)
	return code;
    code = param_put_cie_render1(dict.list, pcrd, mem);
    dcode = param_end_write_dict(plist, key, &dict);
    return (code < 0 ? code : dcode);
}

/* Write a CRD directly to a parameter list. */
int
param_put_cie_render1(gs_param_list * plist, const gs_cie_render * pcrd,
		      gs_memory_t * mem)
{
    int crd_type = CRD_TYPE;
    int code;

    if (pcrd->TransformPQR.proc_name) {
	gs_param_string pn, pd;

	param_string_from_string(pn, pcrd->TransformPQR.proc_name);
	pn.size++;		/* include terminating null */
	pd.data = pcrd->TransformPQR.proc_data.data;
	pd.size = pcrd->TransformPQR.proc_data.size;
	pd.persistent = true;  /****** WRONG ******/
	if ((code = param_write_name(plist, "TransformPQRName", &pn)) < 0 ||
	    (code = param_write_string(plist, "TransformPQRData", &pd)) < 0
	    )
	    return code;
    }
    else if (pcrd->TransformPQR.proc != TransformPQR_default.proc) {
	/* We have no way to represent the procedure, so return an error. */
	return_error(gs_error_rangecheck);
    }
    if ((code = param_write_int(plist, "ColorRenderingType", &crd_type)) < 0 ||
	(code = write_vector3(plist, "WhitePoint", &pcrd->points.WhitePoint, mem)) < 0
	)
	return code;
    if (memcmp(&pcrd->points.BlackPoint, &BlackPoint_default,
	       sizeof(pcrd->points.BlackPoint))) {
	if ((code = write_vector3(plist, "BlackPoint", &pcrd->points.BlackPoint, mem)) < 0)
	    return code;
    }
    if ((code = write_matrix3(plist, "MatrixPQR", &pcrd->MatrixPQR, mem)) < 0 ||
	(code = write_range3(plist, "RangePQR", &pcrd->RangePQR, mem)) < 0 ||
    /* TransformPQR is handled separately */
    (code = write_matrix3(plist, "MatrixLMN", &pcrd->MatrixLMN, mem)) < 0 ||
	(code = write_proc3(plist, "EncodeLMNValues", pcrd,
			    &pcrd->EncodeLMN, &pcrd->DomainLMN, mem)) < 0 ||
	(code = write_range3(plist, "RangeLMN", &pcrd->RangeLMN, mem)) < 0 ||
    (code = write_matrix3(plist, "MatrixABC", &pcrd->MatrixABC, mem)) < 0 ||
	(code = write_proc3(plist, "EncodeABCValues", pcrd,
			    &pcrd->EncodeABC, &pcrd->DomainABC, mem)) < 0 ||
	(code = write_range3(plist, "RangeABC", &pcrd->RangeABC, mem)) < 0
	)
	return code;
    if (pcrd->RenderTable.lookup.table) {
	int n = pcrd->RenderTable.lookup.n;
	int m = pcrd->RenderTable.lookup.m;
	int na = pcrd->RenderTable.lookup.dims[0];
	int *size = (int *)
	    gs_alloc_byte_array(mem, n + 1, sizeof(int), "RenderTableSize");

	/*
	 * In principle, we should use gs_alloc_struct_array with a
	 * type descriptor for gs_param_string.  However, it is widely
	 * assumed that parameter lists are transient, and don't require
	 * accurate GC information; so we can get away with allocating
	 * the string table as bytes.
	 */
	gs_param_string *table =
	    (gs_param_string *)
	    gs_alloc_byte_array(mem, na, sizeof(gs_param_string),
				"RenderTableTable");
	gs_param_int_array ia;

	if (size == 0 || table == 0)
	    code = gs_note_error(gs_error_VMerror);
	else {
	    memcpy(size, pcrd->RenderTable.lookup.dims, sizeof(int) * n);

	    size[n] = m;
	    ia.data = size;
	    ia.size = n + 1;
	    ia.persistent = true;
	    code = param_write_int_array(plist, "RenderTableSize", &ia);
	}
	if (code >= 0) {
	    gs_param_string_array sa;
	    int a;

	    for (a = 0; a < na; ++a)
		table[a].data = pcrd->RenderTable.lookup.table[a].data,
		    table[a].size = pcrd->RenderTable.lookup.table[a].size,
		    table[a].persistent = true;
	    sa.data = table;
	    sa.size = na;
	    sa.persistent = true;
	    code = param_write_string_array(plist, "RenderTableTable", &sa);
	    if (code >= 0 && !pcrd->caches.RenderTableT_is_identity) {
		/****** WRITE RenderTableTValues LIKE write_proc3 ******/
		uint size = gx_cie_cache_size;
		float *values =
		    (float *)gs_alloc_byte_array(mem, size * m,
						 sizeof(float),
						 "write_proc3");
		gs_param_float_array fa;
		int i;

		if (values == 0)
		    return_error(gs_error_VMerror);
		for (i = 0; i < m; ++i) {
		    double scale = 255.0 / (size - 1);
		    int j;

		    for (j = 0; j < size; ++j)
			values[i * size + j] =
			    frac2float((*pcrd->RenderTable.T.procs[i])
				       (j * scale, pcrd));
		}
		fa.data = values;
		fa.size = size * m;
		fa.persistent = true;
		code = param_write_float_array(plist, "RenderTableTValues",
					       &fa);
	    }
	}
	if (code < 0) {
	    gs_free_object(mem, table, "RenderTableTable");
	    gs_free_object(mem, size, "RenderTableSize");
	    return code;
	}
    }
    return code;
}

/* ---------------- Reading ---------------- */

/* Internal procedures for reading parameter values. */
private void
load_vector3(gs_vector3 * pvec, const float *p)
{
    pvec->u = p[0], pvec->v = p[1], pvec->w = p[2];
}
private int
read_floats(gs_param_list * plist, gs_param_name key, float *values, int count)
{
    gs_param_float_array fa;
    int code = param_read_float_array(plist, key, &fa);

    if (code)
	return code;
    if (fa.size != count)
	return_error(gs_error_rangecheck);
    memcpy(values, fa.data, sizeof(float) * count);

    return 0;
}
private int
read_vector3(gs_param_list * plist, gs_param_name key,
	     gs_vector3 * pvec, const gs_vector3 * dflt)
{
    float values[3];
    int code = read_floats(plist, key, values, 3);

    switch (code) {
	case 1:		/* not defined */
	    if (dflt)
		*pvec = *dflt;
	    break;
	case 0:
	    load_vector3(pvec, values);
	default:		/* error */
	    break;
    }
    return code;
}
private int
read_matrix3(gs_param_list * plist, gs_param_name key, gs_matrix3 * pmat)
{
    float values[9];
    int code = read_floats(plist, key, values, 9);

    switch (code) {
	case 1:		/* not defined */
	    *pmat = Matrix3_default;
	    break;
	case 0:
	    load_vector3(&pmat->cu, values);
	    load_vector3(&pmat->cv, values + 3);
	    load_vector3(&pmat->cw, values + 6);
	default:		/* error */
	    break;
    }
    return code;
}
private int
read_range3(gs_param_list * plist, gs_param_name key, gs_range3 * prange)
{
    float values[6];
    int code = read_floats(plist, key, values, 6);

    switch (code) {
	case 1:		/* not defined */
	    *prange = Range3_default;
	    break;
	case 0:
	    prange->ranges[0].rmin = values[0];
	    prange->ranges[0].rmax = values[1];
	    prange->ranges[1].rmin = values[2];
	    prange->ranges[1].rmax = values[3];
	    prange->ranges[2].rmin = values[4];
	    prange->ranges[2].rmax = values[5];
	default:		/* error */
	    break;
    }
    return code;
}
private int
read_proc3(gs_param_list * plist, gs_param_name key,
	   float values[gx_cie_cache_size * 3])
{
    return read_floats(plist, key, values, gx_cie_cache_size * 3);
}

/* Read a CRD from a device parameter. */
int
gs_cie_render1_param_initialize(gs_cie_render * pcrd, gs_param_list * plist,
				gs_param_name key, gx_device * dev)
{
    gs_param_dict dict;
    int code = param_begin_read_dict(plist, key, &dict, false);
    int dcode;

    if (code < 0)
	return code;
    code = param_get_cie_render1(pcrd, dict.list, dev);
    dcode = param_end_read_dict(plist, key, &dict);
    if (code < 0)
	return code;
    if (dcode < 0)
	return dcode;
    gs_cie_render_init(pcrd);
    gs_cie_render_sample(pcrd);
    return gs_cie_render_complete(pcrd);
}

/* Define the structure for passing Encode values as "client data". */
typedef struct encode_data_s {
    float lmn[gx_cie_cache_size * 3]; /* EncodeLMN */
    float abc[gx_cie_cache_size * 3]; /* EncodeABC */
    float t[gx_cie_cache_size * 4]; /* RenderTable.T */
} encode_data_t;

/* Define procedures that retrieve the Encode values read from the list. */
private float
encode_from_data(floatp v, const float values[gx_cie_cache_size],
		 const gs_range * range)
{
    return (v <= range->rmin ? values[0] :
	    v >= range->rmax ? values[gx_cie_cache_size - 1] :
	    values[(int)((v - range->rmin) / (range->rmax - range->rmin) *
			 (gx_cie_cache_size - 1) + 0.5)]);
}
/*
 * The repetitive boilerplate in the next 10 procedures really sticks in
 * my craw, but I've got a mandate not to use macros....
 */
private float
encode_lmn_0_from_data(floatp v, const gs_cie_render * pcrd)
{
    const encode_data_t *data = pcrd->client_data;

    return encode_from_data(v, &data->lmn[0],
			    &pcrd->DomainLMN.ranges[0]);
}
private float
encode_lmn_1_from_data(floatp v, const gs_cie_render * pcrd)
{
    const encode_data_t *data = pcrd->client_data;

    return encode_from_data(v, &data->lmn[gx_cie_cache_size],
			    &pcrd->DomainLMN.ranges[1]);
}
private float
encode_lmn_2_from_data(floatp v, const gs_cie_render * pcrd)
{
    const encode_data_t *data = pcrd->client_data;

    return encode_from_data(v, &data->lmn[gx_cie_cache_size * 2],
			    &pcrd->DomainLMN.ranges[2]);
}
private float
encode_abc_0_from_data(floatp v, const gs_cie_render * pcrd)
{
    const encode_data_t *data = pcrd->client_data;

    return encode_from_data(v, &data->abc[0],
			    &pcrd->DomainABC.ranges[0]);
}
private float
encode_abc_1_from_data(floatp v, const gs_cie_render * pcrd)
{
    const encode_data_t *data = pcrd->client_data;

    return encode_from_data(v, &data->abc[gx_cie_cache_size],
			    &pcrd->DomainABC.ranges[1]);
}
private float
encode_abc_2_from_data(floatp v, const gs_cie_render * pcrd)
{
    const encode_data_t *data = pcrd->client_data;

    return encode_from_data(v, &data->abc[gx_cie_cache_size * 2],
			    &pcrd->DomainABC.ranges[2]);
}
private frac
render_table_t_0_from_data(byte v, const gs_cie_render * pcrd)
{
    const encode_data_t *data = pcrd->client_data;

    return float2frac(encode_from_data(v / 255.0,
				       &data->t[0],
				       &Range3_default.ranges[0]));
}
private frac
render_table_t_1_from_data(byte v, const gs_cie_render * pcrd)
{
    const encode_data_t *data = pcrd->client_data;

    return float2frac(encode_from_data(v / 255.0,
				       &data->t[gx_cie_cache_size],
				       &Range3_default.ranges[0]));
}
private frac
render_table_t_2_from_data(byte v, const gs_cie_render * pcrd)
{
    const encode_data_t *data = pcrd->client_data;

    return float2frac(encode_from_data(v / 255.0,
				       &data->t[gx_cie_cache_size * 2],
				       &Range3_default.ranges[0]));
}
private frac
render_table_t_3_from_data(byte v, const gs_cie_render * pcrd)
{
    const encode_data_t *data = pcrd->client_data;

    return float2frac(encode_from_data(v / 255.0,
				       &data->t[gx_cie_cache_size * 3],
				       &Range3_default.ranges[0]));
}
private const gs_cie_render_proc3 EncodeLMN_from_data = {
    {encode_lmn_0_from_data, encode_lmn_1_from_data, encode_lmn_2_from_data}
};
private const gs_cie_render_proc3 EncodeABC_from_data = {
    {encode_abc_0_from_data, encode_abc_1_from_data, encode_abc_2_from_data}
};
private const gs_cie_render_table_procs RenderTableT_from_data = {
    {render_table_t_0_from_data, render_table_t_1_from_data,
     render_table_t_2_from_data, render_table_t_3_from_data
    }
};

/* Read a CRD directly from a parameter list. */
int
param_get_cie_render1(gs_cie_render * pcrd, gs_param_list * plist,
		      gx_device * dev)
{
    encode_data_t data;
    gs_param_int_array rt_size;
    int crd_type;
    int code, code_lmn, code_abc, code_rt, code_t;
    gs_param_string pname, pdata;

    if ((code = param_read_int(plist, "ColorRenderingType", &crd_type)) < 0 ||
	crd_type != CRD_TYPE ||
	(code = read_vector3(plist, "WhitePoint", &pcrd->points.WhitePoint,
			     NULL)) < 0 ||
	(code = read_vector3(plist, "BlackPoint", &pcrd->points.BlackPoint,
			     &BlackPoint_default)) < 0 ||
	(code = read_matrix3(plist, "MatrixPQR", &pcrd->MatrixPQR)) < 0 ||
	(code = read_range3(plist, "RangePQR", &pcrd->RangePQR)) < 0 ||
	/* TransformPQR is handled specially below. */
	(code = read_matrix3(plist, "MatrixLMN", &pcrd->MatrixLMN)) < 0 ||
	(code_lmn = code =
	 read_proc3(plist, "EncodeLMNValues", data.lmn)) < 0 ||
	(code = read_range3(plist, "RangeLMN", &pcrd->RangeLMN)) < 0 ||
	(code = read_matrix3(plist, "MatrixABC", &pcrd->MatrixABC)) < 0 ||
	(code_abc = code =
	 read_proc3(plist, "EncodeABCValues", data.abc)) < 0 ||
	(code = read_range3(plist, "RangeABC", &pcrd->RangeABC)) < 0
	)
	return code;
    /* Handle the sampled functions. */
    switch (code = param_read_string(plist, "TransformPQRName", &pname)) {
	default:		/* error */
	    return code;
	case 1:			/* missing */
	    pcrd->TransformPQR = TransformPQR_default;
	    break;
	case 0:			/* specified */
	    /* The procedure name must be null-terminated: */
	    /* see param_put_cie_render1 above. */
	    if (pname.size < 1 || pname.data[pname.size - 1] != 0)
		return_error(gs_error_rangecheck);
	    pcrd->TransformPQR.proc = TransformPQR_lookup_proc_name;
	    pcrd->TransformPQR.proc_name = (char *)pname.data;
	    switch (code = param_read_string(plist, "TransformPQRData", &pdata)) {
		default:	/* error */
		    return code;
		case 1:		/* missing */
		    pcrd->TransformPQR.proc_data.data = 0;
		    pcrd->TransformPQR.proc_data.size = 0;
		    break;
		case 0:
		    pcrd->TransformPQR.proc_data.data = pdata.data;
		    pcrd->TransformPQR.proc_data.size = pdata.size;
	    }
	    pcrd->TransformPQR.driver_name = gs_devicename(dev);
	    break;
    }
    pcrd->client_data = &data;
    if (code_lmn > 0)
	pcrd->EncodeLMN = Encode_default;
    else
	pcrd->EncodeLMN = EncodeLMN_from_data;
    if (code_abc > 0)
	pcrd->EncodeABC = Encode_default;
    else
	pcrd->EncodeABC = EncodeABC_from_data;
    code_rt = code = param_read_int_array(plist, "RenderTableSize", &rt_size);
    if (code == 1) {
	if (pcrd->RenderTable.lookup.table) {
	    gs_free_object(pcrd->rc.memory,
		(void *)pcrd->RenderTable.lookup.table, /* break const */
		"param_get_cie_render1(RenderTable)");
	    pcrd->RenderTable.lookup.table = 0;
	}
	pcrd->RenderTable.T = RenderTableT_default;
	code_t = 1;
    } else if (code < 0)
	return code;
    else if (rt_size.size != 4)
	return_error(gs_error_rangecheck);
    else {
	gs_param_string_array rt_values;
	gs_const_string *table;
	int n, m, j;

	code = param_read_string_array(plist, "RenderTableTable", &rt_values);
	if (code < 0)
	    return code;
	else if (code > 0 ||
		 rt_values.size != rt_size.data[3] *
		 rt_size.data[1] * rt_size.data[2])
	    return_error(gs_error_rangecheck);
	pcrd->RenderTable.lookup.n = n = rt_size.size - 1;
	pcrd->RenderTable.lookup.m = m = rt_size.data[n];
	if (n > 4 || m > 4)
	    return_error(gs_error_rangecheck);
	memcpy(pcrd->RenderTable.lookup.dims, rt_size.data, n * sizeof(int));
	table = (gs_const_string *)gs_malloc(pcrd->RenderTable.lookup.dims[0],
	                                     sizeof(gs_const_string *),
					     "param_get_cie_render1");
	for (j = 0; j < pcrd->RenderTable.lookup.dims[0]; ++j) {
	    table[j].data = rt_values.data[j].data;
	    table[j].size = rt_values.data[j].size;
	}
	pcrd->RenderTable.lookup.table = table;
	pcrd->RenderTable.T = RenderTableT_from_data;
	code_t = code = read_floats(plist, "RenderTableTValues", data.t,
				    gx_cie_cache_size * m);
	if (code > 0)
	    pcrd->RenderTable.T = RenderTableT_default;
	else if (code == 0)
	    pcrd->RenderTable.T = RenderTableT_from_data;
    }
    if ((code = gs_cie_render_init(pcrd)) >= 0 &&
	(code = gs_cie_render_sample(pcrd)) >= 0
	)
	code = gs_cie_render_complete(pcrd);
    /* Clean up before exiting. */
    pcrd->client_data = 0;
    if (code_lmn == 0)
	pcrd->EncodeLMN = EncodeLMN_from_cache;
    if (code_abc == 0)
	pcrd->EncodeABC = EncodeABC_from_cache;
    if (code_t == 0)
	pcrd->RenderTable.T = RenderTableT_from_cache;
    return code;
}
