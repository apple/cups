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

/*$Id: gsfunc0.c,v 1.1 2000/03/08 23:14:41 mike Exp $ */
/* Implementation of FunctionType 0 (Sampled) Functions */
#include "math_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsfunc0.h"
#include "gxfarith.h"
#include "gxfunc.h"

typedef struct gs_function_Sd_s {
    gs_function_head_t head;
    gs_function_Sd_params_t params;
} gs_function_Sd_t;

private_st_function_Sd();

/* Define the maximum plausible number of inputs and outputs */
/* for a Sampled function. */
#define max_Sd_m 16
#define max_Sd_n 16

/* Get one set of sample values. */
#define SETUP_SAMPLES(bps, nbytes)\
	int n = pfn->params.n;\
	byte buf[max_Sd_n * ((bps + 7) >> 3)];\
	const byte *p;\
	int i;\
\
	data_source_access(&pfn->params.DataSource, offset >> 3,\
			   nbytes, buf, &p)

private int
fn_gets_1(const gs_function_Sd_t * pfn, ulong offset, uint * samples)
{
    SETUP_SAMPLES(1, ((offset & 7) + n + 7) >> 3);
    for (i = 0; i < n; ++i) {
	samples[i] = (*p >> (~offset & 7)) & 1;
	if (!(++offset & 7))
	    p++;
    }
    return 0;
}
private int
fn_gets_2(const gs_function_Sd_t * pfn, ulong offset, uint * samples)
{
    SETUP_SAMPLES(2, (((offset & 7) >> 1) + n + 3) >> 2);
    for (i = 0; i < n; ++i) {
	samples[i] = (*p >> (6 - (offset & 7))) & 3;
	if (!((offset += 2) & 7))
	    p++;
    }
    return 0;
}
private int
fn_gets_4(const gs_function_Sd_t * pfn, ulong offset, uint * samples)
{
    SETUP_SAMPLES(4, (((offset & 7) >> 2) + n + 1) >> 1);
    for (i = 0; i < n; ++i) {
	samples[i] = (offset & 4 ? *p++ & 0xf : *p >> 4);
    }
    return 0;
}
private int
fn_gets_8(const gs_function_Sd_t * pfn, ulong offset, uint * samples)
{
    SETUP_SAMPLES(8, n);
    for (i = 0; i < n; ++i) {
	samples[i] = *p++;
    }
    return 0;
}
private int
fn_gets_12(const gs_function_Sd_t * pfn, ulong offset, uint * samples)
{
    SETUP_SAMPLES(12, (((offset & 7) >> 2) + 3 * n + 1) >> 1);
    for (i = 0; i < n; ++i) {
	if (offset & 4)
	    samples[i] = ((*p & 0xf) << 8) + p[1], p += 2;
	else
	    samples[i] = (*p << 4) + (p[1] >> 4), p++;
	offset ^= 4;
    }
    return 0;
}
private int
fn_gets_16(const gs_function_Sd_t * pfn, ulong offset, uint * samples)
{
    SETUP_SAMPLES(16, n * 2);
    for (i = 0; i < n; ++i) {
	samples[i] = (*p << 8) + p[1];
	p += 2;
    }
    return 0;
}
private int
fn_gets_24(const gs_function_Sd_t * pfn, ulong offset, uint * samples)
{
    SETUP_SAMPLES(24, n * 3);
    for (i = 0; i < n; ++i) {
	samples[i] = (*p << 16) + (p[1] << 8) + p[2];
	p += 3;
    }
    return 0;
}
private int
fn_gets_32(const gs_function_Sd_t * pfn, ulong offset, uint * samples)
{
    SETUP_SAMPLES(32, n * 4);
    for (i = 0; i < n; ++i) {
	samples[i] = (*p << 24) + (p[1] << 16) + (p[2] << 8) + p[3];
	p += 4;
    }
    return 0;
}

private int (*const fn_get_samples[]) (P3(const gs_function_Sd_t * pfn,
					  ulong offset, uint * samples)) =
{
    0, fn_gets_1, fn_gets_2, 0, fn_gets_4, 0, 0, 0,
	fn_gets_8, 0, 0, 0, fn_gets_12, 0, 0, 0,
	fn_gets_16, 0, 0, 0, 0, 0, 0, 0,
	fn_gets_24, 0, 0, 0, 0, 0, 0, 0,
	fn_gets_32
};

/* Calculate a result by multilinear interpolation. */
private void
fn_interpolate_linear(const gs_function_Sd_t *pfn, const float *fparts,
		 const ulong *factors, float *samples, ulong offset, int m)
{
    int j;

top:
    if (m == 0) {
	uint sdata[max_Sd_n];

	(*fn_get_samples[pfn->params.BitsPerSample])(pfn, offset, sdata);
	for (j = pfn->params.n - 1; j >= 0; --j)
	    samples[j] = sdata[j];
    } else {
	float fpart = *fparts++;
	float samples1[max_Sd_n];

	if (is_fzero(fpart)) {
	    ++factors;
	    --m;
	    goto top;
	}
	fn_interpolate_linear(pfn, fparts, factors + 1, samples,
			      offset, m - 1);
	fn_interpolate_linear(pfn, fparts, factors + 1, samples1,
			      offset + *factors, m - 1);
	for (j = pfn->params.n - 1; j >= 0; --j)
	    samples[j] += (samples1[j] - samples[j]) * fpart;
    }
}

/* Evaluate a Sampled function. */
private int
fn_Sd_evaluate(const gs_function_t * pfn_common, const float *in, float *out)
{
    const gs_function_Sd_t *pfn = (const gs_function_Sd_t *)pfn_common;
    int bps = pfn->params.BitsPerSample;
    ulong offset = 0;
    int i;
    float encoded[max_Sd_m];
    ulong factors[max_Sd_m];
    float samples[max_Sd_n];

    /* Encode the input values. */

    for (i = 0; i < pfn->params.m; ++i) {
	float d0 = pfn->params.Domain[2 * i],
	    d1 = pfn->params.Domain[2 * i + 1];
	float arg = in[i], enc;

	if (arg < d0)
	    arg = d0;
	else if (arg > d1)
	    arg = d1;
	if (pfn->params.Encode) {
	    float e0 = pfn->params.Encode[2 * i];
	    float e1 = pfn->params.Encode[2 * i + 1];

	    enc = (arg - d0) * (e1 - e0) / (d1 - d0) + e0;
	    if (enc < 0)
		encoded[i] = 0;
	    else if (enc >= pfn->params.Size[i] - 1)
		encoded[i] = pfn->params.Size[i] - 1;
	    else
		encoded[i] = enc;
	} else {
	    /* arg is guaranteed to be in bounds, ergo so is enc */
	    encoded[i] = (arg - d0) * (pfn->params.Size[i] - 1) / (d1 - d0);
	}
    }

    /* Look up and interpolate the output values. */

    {
	ulong factor = bps * pfn->params.n;

	for (i = 0; i < pfn->params.m; factor *= pfn->params.Size[i++]) {
	    int ipart = (int)encoded[i];

	    offset += (factors[i] = factor) * ipart;
	    encoded[i] -= ipart;
	}
    }
    /****** LINEAR INTERPOLATION ONLY ******/
    fn_interpolate_linear(pfn, encoded, factors, samples, offset,
			  pfn->params.m);

    /* Encode the output values. */

    for (i = 0; i < pfn->params.n; offset += bps, ++i) {
	float d0, d1, r0, r1, value;

	if (pfn->params.Range)
	    r0 = pfn->params.Range[2 * i], r1 = pfn->params.Range[2 * i + 1];
	else
	    r0 = 0, r1 = (1 << bps) - 1;
	if (pfn->params.Decode)
	    d0 = pfn->params.Decode[2 * i], d1 = pfn->params.Decode[2 * i + 1];
	else
	    d0 = r0, d1 = r1;

	value = samples[i] * (d1 - d0) / ((1 << bps) - 1) + d0;
	if (value < r0)
	    out[i] = r0;
	else if (value > r1)
	    out[i] = r1;
	else
	    out[i] = value;
    }

    return 0;
}

/* Test whether a Sampled function is monotonic. */
/* Since this can be very time-consuming, we only do it if necessary. */
private int
fn_Sd_is_monotonic(const gs_function_t * pfn_common,
		   const float *lower, const float *upper, bool must_know)
{
    if (!must_know)
	return gs_error_undefined;	/* don't know */
/****** NYI ******/
    return gs_error_undefined;
}

/* Free the parameters of a Sampled function. */
void
gs_function_Sd_free_params(gs_function_Sd_params_t * params, gs_memory_t * mem)
{
    gs_free_object(mem, (void *)params->Size, "Size");	/* break const */
    gs_free_object(mem, (void *)params->Decode, "Decode");	/* break const */
    gs_free_object(mem, (void *)params->Encode, "Encode");	/* break const */
    fn_common_free_params((gs_function_params_t *) params, mem);
}

/* Allocate and initialize a Sampled function. */
int
gs_function_Sd_init(gs_function_t ** ppfn,
		  const gs_function_Sd_params_t * params, gs_memory_t * mem)
{
    static const gs_function_head_t function_Sd_head =
    {
	function_type_Sampled,
	(fn_evaluate_proc_t) fn_Sd_evaluate,
	(fn_is_monotonic_proc_t) fn_Sd_is_monotonic,
	(fn_free_params_proc_t) gs_function_Sd_free_params,
	fn_common_free
    };
    int code;
    int i;

    *ppfn = 0;			/* in case of error */
    code = fn_check_mnDR((const gs_function_params_t *)params,
			 params->m, params->n);
    if (code < 0)
	return code;
    if (params->m > max_Sd_m)
	return_error(gs_error_limitcheck);
    switch (params->Order) {
	case 0:		/* use default */
	case 1:
	case 3:
	    break;
	default:
	    return_error(gs_error_rangecheck);
    }
    switch (params->BitsPerSample) {
	case 1:
	case 2:
	case 4:
	case 8:
	case 12:
	case 16:
	case 24:
	case 32:
	    break;
	default:
	    return_error(gs_error_rangecheck);
    }
    for (i = 0; i < params->m; ++i)
	if (params->Size[i] <= 0)
	    return_error(gs_error_rangecheck);
    {
	gs_function_Sd_t *pfn =
	gs_alloc_struct(mem, gs_function_Sd_t, &st_function_Sd,
			"gs_function_Sd_init");

	if (pfn == 0)
	    return_error(gs_error_VMerror);
	pfn->params = *params;
	if (params->Order == 0)
	    pfn->params.Order = 1;	/* default */
	pfn->head = function_Sd_head;
	*ppfn = (gs_function_t *) pfn;
    }
    return 0;
}
