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

/*$Id: gscrd.c,v 1.1 2000/03/08 23:14:38 mike Exp $ */
/* CIE color rendering dictionary creation */
#include "math_.h"
#include "memory_.h"
#include "string_.h"
#include "gx.h"
#include "gscdefs.h"		/* for gs_lib_device_list */
#include "gsdevice.h"
#include "gserrors.h"
#include "gsmatrix.h"		/* for gscolor2.h */
#include "gsparam.h"
#include "gxcspace.h"
#include "gscolor2.h"		/* for gs_set/currentcolorrendering */
#include "gscrd.h"

/* Import gs_lib_device_list() */
extern_gs_lib_device_list();

/* Allocator structure type */
public_st_cie_render1();
#define pcrd ((gs_cie_render *)vptr)
private 
ENUM_PTRS_BEGIN(cie_render1_enum_ptrs) return 0;

case 0:
return ENUM_OBJ(pcrd->client_data);
case 1:
return ENUM_OBJ(pcrd->RenderTable.lookup.table);
case 2:
return (pcrd->RenderTable.lookup.table ?
	ENUM_CONST_STRING(&pcrd->TransformPQR.proc_data) :
	0);
ENUM_PTRS_END
private RELOC_PTRS_BEGIN(cie_render1_reloc_ptrs);
RELOC_OBJ_VAR(pcrd->client_data);
if (pcrd->RenderTable.lookup.table)
{
RELOC_OBJ_VAR(pcrd->RenderTable.lookup.table);
RELOC_CONST_STRING_VAR(pcrd->TransformPQR.proc_data);
}
RELOC_PTRS_END
#undef pcrd

/* Default CRD procedures. */

private int
tpqr_identity(int index, floatp in, const gs_cie_wbsd * pwbsd,
	      gs_cie_render * pcrd, float *out)
{
    *out = in;
    return 0;
}

private float
render_identity(floatp in, const gs_cie_render * pcrd)
{
    return in;
}
private frac
render_table_identity(byte in, const gs_cie_render * pcrd)
{
    return byte2frac(in);
}

/* Transformation procedures that just consult the cache. */

#define clamp_index(index)\
  index = (index < 0 ? 0 :\
	   index >= gx_cie_cache_size ? gx_cie_cache_size - 1 : index)

private float
EncodeABC_cached(floatp in, const gs_cie_render * pcrd, int i)
{
    const gx_cie_scalar_cache *pcache = &pcrd->caches.EncodeABC[i];

    if (pcrd->RenderTable.lookup.table == 0) {
	int index = (in - pcache->fracs.params.base) *
	pcache->fracs.params.factor;

	clamp_index(index);
	return frac2float(pcache->fracs.values[index]);
    } else {
	/****** WRONG IF INTERPOLATING ******/
	int index = (in - pcache->ints.params.base) *
	pcache->ints.params.factor;
	const gs_range *prange = &pcrd->RangeABC.ranges[i];
	int m = pcrd->RenderTable.lookup.m;
	int k = (i == 0 ? 1 : i == 1 ?
		 m * pcrd->RenderTable.lookup.dims[2] : m);

	clamp_index(index);
	return (double)(pcache->ints.values[index]) / k *
	    (prange->rmax - prange->rmin) / (gx_cie_cache_size - 1) +
	    prange->rmin;
    }
}
private float
EncodeABC_cached_A(floatp in, const gs_cie_render * pcrd)
{
    return EncodeABC_cached(in, pcrd, 0);
}
private float
EncodeABC_cached_B(floatp in, const gs_cie_render * pcrd)
{
    return EncodeABC_cached(in, pcrd, 1);
}
private float
EncodeABC_cached_C(floatp in, const gs_cie_render * pcrd)
{
    return EncodeABC_cached(in, pcrd, 2);
}
private float
EncodeLMN_cached(floatp in, const gs_cie_render * pcrd, int i)
{
    const gx_cie_vector_cache *pcache = &pcrd->caches.EncodeLMN[i];
    int index = (in - pcache->floats.params.base) *
    pcache->floats.params.factor;

    clamp_index(index);
    /* Pick any one of u, v, w.  We should probably pick the one */
    /* with the largest coefficient.... */
    return cie_cached2float(pcache->vecs.values[index].u) /
	(i == 0 ? pcrd->MatrixABCEncode.cu.u :
	 i == 1 ? pcrd->MatrixABCEncode.cv.u :
	 pcrd->MatrixABCEncode.cw.u);
}
private float
EncodeLMN_cached_L(floatp in, const gs_cie_render * pcrd)
{
    return EncodeLMN_cached(in, pcrd, 0);
}
private float
EncodeLMN_cached_M(floatp in, const gs_cie_render * pcrd)
{
    return EncodeLMN_cached(in, pcrd, 1);
}
private float
EncodeLMN_cached_N(floatp in, const gs_cie_render * pcrd)
{
    return EncodeLMN_cached(in, pcrd, 2);
}

private frac
RTT_cached(byte in, const gs_cie_render * pcrd, int i)
{
    /****** NYI ******/
    return byte2frac(in);
}
private frac
RTT_cached_0(byte in, const gs_cie_render * pcrd)
{
    return RTT_cached(in, pcrd, 0);
}
private frac
RTT_cached_1(byte in, const gs_cie_render * pcrd)
{
    return RTT_cached(in, pcrd, 1);
}
private frac
RTT_cached_2(byte in, const gs_cie_render * pcrd)
{
    return RTT_cached(in, pcrd, 2);
}
private frac
RTT_cached_3(byte in, const gs_cie_render * pcrd)
{
    return RTT_cached(in, pcrd, 3);
}

#undef clamp_index

/* Define the TransformPQR trampoline procedure that looks up proc_name. */

private int
tpqr_do_lookup(gs_cie_render *pcrd, const gx_device *dev_proto)
{
    gx_device *dev;
    gs_memory_t *mem = pcrd->rc.memory;
    gs_c_param_list list;
    gs_param_string proc_addr;
    int code;

    /* Device prototypes are const, so we must create a copy. */
    code = gs_copydevice(&dev, dev_proto, mem);
    if (code < 0)
	return code;
    gs_c_param_list_write(&list, mem);
    code = param_request((gs_param_list *)&list,
			 pcrd->TransformPQR.proc_name);
    if (code >= 0) {
	code = gs_getdeviceparams(dev, (gs_param_list *)&list);
	if (code >= 0) {
	    gs_c_param_list_read(&list);
	    code = param_read_string((gs_param_list *)&list,
				     pcrd->TransformPQR.proc_name,
				     &proc_addr);
	    if (code == 0 && proc_addr.size == sizeof(gs_cie_transform_proc)) {
		memcpy(&pcrd->TransformPQR.proc, proc_addr.data,
		       sizeof(gs_cie_transform_proc));
	    } else
		code = gs_note_error(gs_error_rangecheck);
	}
    }
    gs_c_param_list_release(&list);
    gs_free_object(mem, dev, "tpqr_do_lookup(device)");
    return code;
}
private int
tpqr_lookup(int index, floatp in, const gs_cie_wbsd * pwbsd,
	    gs_cie_render * pcrd, float *out)
{
    const gx_device *const *dev_list;
    int count = gs_lib_device_list(&dev_list, NULL);
    int i;
    int code;

    for (i = 0; i < count; ++i)
	if (!strcmp(gs_devicename(dev_list[i]),
		    pcrd->TransformPQR.driver_name))
	    break;
    if (i < count)
	code = tpqr_do_lookup(pcrd, dev_list[i]);
    else
	code = gs_note_error(gs_error_undefined);
    if (code < 0)
	return code;
    return pcrd->TransformPQR.proc(index, in, pwbsd, pcrd, out);
}


/* Default vectors. */
const gs_cie_transform_proc3 TransformPQR_default = {
    tpqr_identity,
    0,				/* proc_name */
    {0, 0},			/* proc_data */
    0				/* driver_name */
};
const gs_cie_transform_proc TransformPQR_lookup_proc_name = tpqr_lookup;
const gs_cie_render_proc3 Encode_default = {
    {render_identity, render_identity, render_identity}
};
const gs_cie_render_proc3 EncodeLMN_from_cache = {
    {EncodeLMN_cached_L, EncodeLMN_cached_M, EncodeLMN_cached_N}
};
const gs_cie_render_proc3 EncodeABC_from_cache = {
    {EncodeABC_cached_A, EncodeABC_cached_B, EncodeABC_cached_C}
};
const gs_cie_render_table_procs RenderTableT_default = {
    {render_table_identity, render_table_identity, render_table_identity,
     render_table_identity
    }
};
const gs_cie_render_table_procs RenderTableT_from_cache = {
    {RTT_cached_0, RTT_cached_1, RTT_cached_2, RTT_cached_3}
};

/*
 * Allocate and minimally initialize a CRD.  Note that this procedure sets
 * the reference count of the structure to 1, not 0.  gs_setcolorrendering
 * will increment the reference count again, so unless you want the
 * structure to stay allocated permanently (or until a garbage collection),
 * you should call rc_decrement(pcrd, "client name") *after* calling
 * gs_setcolorrendering.
 */
int
gs_cie_render1_build(gs_cie_render ** ppcrd, gs_memory_t * mem,
		     client_name_t cname)
{
    gs_cie_render *pcrd;

    rc_alloc_struct_1(pcrd, gs_cie_render, &st_cie_render1, mem,
		      return_error(gs_error_VMerror), cname);
    /* Initialize pointers for the GC. */
    pcrd->client_data = 0;
    pcrd->RenderTable.lookup.table = 0;
    pcrd->status = CIE_RENDER_STATUS_BUILT;
    *ppcrd = pcrd;
    return 0;
}

/*
 * Initialize a CRD given all of the relevant parameters.
 * Any of the pointers except WhitePoint may be zero, meaning
 * use the default values.
 *
 * The actual point, matrix, range, and procedure values are copied into the
 * CRD, but only the pointer to the color lookup table is copied.
 */
int
gs_cie_render1_initialize(gs_cie_render * pcrd, void *client_data,
			  const gs_vector3 * WhitePoint,
			  const gs_vector3 * BlackPoint,
			  const gs_matrix3 * MatrixPQR,
			  const gs_range3 * RangePQR,
			  const gs_cie_transform_proc3 * TransformPQR,
			  const gs_matrix3 * MatrixLMN,
			  const gs_cie_render_proc3 * EncodeLMN,
			  const gs_range3 * RangeLMN,
			  const gs_matrix3 * MatrixABC,
			  const gs_cie_render_proc3 * EncodeABC,
			  const gs_range3 * RangeABC,
			  const gs_cie_render_table_t * RenderTable)
{
    pcrd->client_data = client_data;
    pcrd->points.WhitePoint = *WhitePoint;
    pcrd->points.BlackPoint =
	*(BlackPoint ? BlackPoint : &BlackPoint_default);
    pcrd->MatrixPQR = *(MatrixPQR ? MatrixPQR : &Matrix3_default);
    pcrd->RangePQR = *(RangePQR ? RangePQR : &Range3_default);
    pcrd->TransformPQR =
	*(TransformPQR ? TransformPQR : &TransformPQR_default);
    pcrd->MatrixLMN = *(MatrixLMN ? MatrixLMN : &Matrix3_default);
    pcrd->EncodeLMN = *(EncodeLMN ? EncodeLMN : &Encode_default);
    pcrd->RangeLMN = *(RangeLMN ? RangeLMN : &Range3_default);
    pcrd->MatrixABC = *(MatrixABC ? MatrixABC : &Matrix3_default);
    pcrd->EncodeABC = *(EncodeABC ? EncodeABC : &Encode_default);
    pcrd->RangeABC = *(RangeABC ? RangeABC : &Range3_default);
    if (RenderTable)
	pcrd->RenderTable = *RenderTable;
    else {
	pcrd->RenderTable.lookup.table = 0;
	pcrd->RenderTable.T = RenderTableT_default;
    }
    pcrd->status = CIE_RENDER_STATUS_BUILT;
    return 0;
}
