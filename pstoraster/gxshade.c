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

/*$Id: gxshade.c,v 1.1 2000/03/08 23:15:05 mike Exp $ */
/* Shading rendering support */
#include "math_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsrect.h"
#include "gxcspace.h"
#include "gscie.h"		/* requires gscspace.h */
#include "gxdevcli.h"
#include "gxistate.h"
#include "gxdht.h"		/* for computing # of different colors */
#include "gxpaint.h"
#include "gxshade.h"

/* ================ Packed coordinate streams ================ */

/* Forward references */
private int cs_next_packed_value(P3(shade_coord_stream_t *, int, uint *));
private int cs_next_array_value(P3(shade_coord_stream_t *, int, uint *));
private int cs_next_packed_decoded(P4(shade_coord_stream_t *, int,
				      const float[2], float *));
private int cs_next_array_decoded(P4(shade_coord_stream_t *, int,
				     const float[2], float *));

/* Initialize a packed value stream. */
void
shade_next_init(shade_coord_stream_t * cs,
		const gs_shading_mesh_params_t * params,
		const gs_imager_state * pis)
{
    cs->params = params;
    cs->pctm = &pis->ctm;
    if (data_source_is_stream(params->DataSource)) {
	cs->s = params->DataSource.data.strm;
    } else {
	sread_string(&cs->ds, params->DataSource.data.str.data,
		     params->DataSource.data.str.size);
	cs->s = &cs->ds;
    }
    if (data_source_is_array(params->DataSource)) {
	cs->get_value = cs_next_array_value;
	cs->get_decoded = cs_next_array_decoded;
    } else {
	cs->get_value = cs_next_packed_value;
	cs->get_decoded = cs_next_packed_decoded;
    }
    cs->left = 0;
}

/* Get the next (integer) value from a packed value stream. */
/* 1 <= num_bits <= sizeof(uint) * 8. */
private int
cs_next_packed_value(shade_coord_stream_t * cs, int num_bits, uint * pvalue)
{
    uint bits = cs->bits;
    int left = cs->left;

    if (left >= num_bits) {
	/* We can satisfy this request with the current buffered bits. */
	cs->left = left -= num_bits;
	*pvalue = (bits >> left) & ((1 << num_bits) - 1);
    } else {
	/* We need more bits. */
	int needed = num_bits - left;
	uint value = bits & ((1 << left) - 1);	/* all the remaining bits */

	for (; needed >= 8; needed -= 8) {
	    int b = sgetc(cs->s);

	    if (b < 0)
		return_error(gs_error_rangecheck);
	    value = (value << 8) + b;
	}
	if (needed == 0) {
	    cs->left = 0;
	    *pvalue = value;
	} else {
	    int b = sgetc(cs->s);

	    if (b < 0)
		return_error(gs_error_rangecheck);
	    cs->bits = b;
	    cs->left = left = 8 - needed;
	    *pvalue = (value << needed) + (b >> left);
	}
    }
    return 0;
}

/* Get the next (integer) value from an unpacked array. */
private int
cs_next_array_value(shade_coord_stream_t * cs, int num_bits, uint * pvalue)
{
    float value;
    uint read;

    if (sgets(cs->s, (byte *)&value, sizeof(float), &read) < 0 ||
	read != sizeof(float) || value < 0 || value >= (1 << num_bits) ||
	value != (int)value
	)
	return_error(gs_error_rangecheck);
    *pvalue = (uint) value;
    return 0;
}

/* Get the next decoded floating point value. */
private int
cs_next_packed_decoded(shade_coord_stream_t * cs, int num_bits,
		       const float decode[2], float *pvalue)
{
    uint value;
    int code = cs->get_value(cs, num_bits, &value);
    double max_value = (double)(uint) ((1 << num_bits) - 1);

    if (code < 0)
	return code;
    *pvalue =
	(decode == 0 ? value / max_value :
	 decode[0] + value * (decode[1] - decode[0]) / max_value);
    return 0;
}

/* Get the next floating point value from an array, without decoding. */
private int
cs_next_array_decoded(shade_coord_stream_t * cs, int num_bits,
		      const float decode[2], float *pvalue)
{
    float value;
    uint read;

    if (sgets(cs->s, (byte *)&value, sizeof(float), &read) < 0 ||
	read != sizeof(float)
    )
	return_error(gs_error_rangecheck);
    *pvalue = value;
    return 0;
}

/* Get the next flag value. */
/* Note that this always starts a new data byte. */
int
shade_next_flag(shade_coord_stream_t * cs, int BitsPerFlag)
{
    uint flag;
    int code;

    cs->left = 0;		/* start a new byte if packed */
    code = cs->get_value(cs, BitsPerFlag, &flag);
    return (code < 0 ? code : flag);
}

/* Get one or more coordinate pairs. */
int
shade_next_coords(shade_coord_stream_t * cs, gs_fixed_point * ppt,
		  int num_points)
{
    int num_bits = cs->params->BitsPerCoordinate;
    const float *decode = cs->params->Decode;
    int code = 0;
    int i;

    for (i = 0; i < num_points; ++i) {
	float x, y;

	if ((code = cs->get_decoded(cs, num_bits, decode, &x)) < 0 ||
	    (code = cs->get_decoded(cs, num_bits, decode, &y)) < 0 ||
	    (code = gs_point_transform2fixed(cs->pctm, x, y, &ppt[i])) < 0
	    )
	    break;
    }
    return code;
}

/* Get a color.  Currently all this does is look up Indexed colors. */
int
shade_next_color(shade_coord_stream_t * cs, float *pc)
{
    const float *decode = cs->params->Decode + 4;	/* skip coord decode */
    const gs_color_space *pcs = cs->params->ColorSpace;
    gs_color_space_index index = gs_color_space_get_index(pcs);
    int num_bits = cs->params->BitsPerComponent;

    if (index == gs_color_space_index_Indexed) {
	uint i;
	int code = cs->get_value(cs, num_bits, &i);

	if (code < 0)
	    return code;
	/****** DO INDEXED LOOKUP TO pc[] ******/
    } else {
	int i, code;
	int ncomp = gs_color_space_num_components(pcs);

	for (i = 0; i < ncomp; ++i)
	    if ((code = cs->get_decoded(cs, num_bits, decode + i * 2, &pc[i])) < 0)
		return code;
    }
    return 0;
}

/* Get the next vertex for a mesh element. */
int
shade_next_vertex(shade_coord_stream_t * cs, mesh_vertex_t * vertex)
{
    int code = shade_next_coords(cs, &vertex->p, 1);

    if (code >= 0)
	code = shade_next_color(cs, vertex->cc);
    return code;
}

/* ================ Shading rendering ================ */

/* Initialize the common parts of the recursion state. */
void
shade_init_fill_state(shading_fill_state_t * pfs, const gs_shading_t * psh,
		      gx_device * dev, gs_imager_state * pis)
{
    const gs_color_space *pcs = psh->params.ColorSpace;
    float max_error = pis->smoothness;
    /*
     * There's no point in trying to achieve smoothness beyond what
     * the device can implement, i.e., the number of representable
     * colors times the number of halftone levels.
     */
    long num_colors =
	max(dev->color_info.max_gray, dev->color_info.max_color) + 1;
    const gs_range *ranges = 0;
    int ci;

    pfs->dev = dev;
    pfs->pis = pis;
    pfs->num_components = gs_color_space_num_components(pcs);
top:
    switch ( gs_color_space_get_index(pcs) )
	{
	case gs_color_space_index_Indexed:
	    pcs = gs_cspace_base_space(pcs);
	    goto top;
	case gs_color_space_index_CIEDEFG:
	    ranges = pcs->params.defg->RangeDEFG.ranges;
	    break;
	case gs_color_space_index_CIEDEF:
	    ranges = pcs->params.def->RangeDEF.ranges;
	    break;
	case gs_color_space_index_CIEABC:
	    ranges = pcs->params.abc->RangeABC.ranges;
	    break;
	case gs_color_space_index_CIEA:
	    ranges = &pcs->params.a->RangeA;
	    break;
	default:
	    break;
	}
    if (num_colors <= 32) {
	/****** WRONG FOR MULTI-PLANE HALFTONES ******/
	num_colors *= pis->dev_ht->order.num_levels;
    }
    if (max_error < 1.0 / num_colors)
	max_error = 1.0 / num_colors;
    for (ci = 0; ci < pfs->num_components; ++ci)
	pfs->cc_max_error[ci] =
	    (ranges == 0 ? max_error :
	     max_error * (ranges[ci].rmax - ranges[ci].rmin));
}

/* Transform a bounding box into device space. */
int
shade_bbox_transform2fixed(const gs_rect * rect, const gs_imager_state * pis,
			   gs_fixed_rect * rfixed)
{
    gs_rect dev_rect;
    int code = gs_bbox_transform(rect, &ctm_only(pis), &dev_rect);

    if (code >= 0) {
	rfixed->p.x = float2fixed(dev_rect.p.x);
	rfixed->p.y = float2fixed(dev_rect.p.y);
	rfixed->q.x = float2fixed(dev_rect.q.x);
	rfixed->q.y = float2fixed(dev_rect.q.y);
    }
    return code;
}

/* Check whether 4 colors fall within the smoothness criterion. */
bool
shade_colors4_converge(const gs_client_color cc[4],
		       const shading_fill_state_t * pfs)
{
    int ci;

    for (ci = 0; ci < pfs->num_components; ++ci) {
	float
	      c0 = cc[0].paint.values[ci], c1 = cc[1].paint.values[ci],
	      c2 = cc[2].paint.values[ci], c3 = cc[3].paint.values[ci];
	float min01, max01, min23, max23;

	if (c0 < c1)
	    min01 = c0, max01 = c1;
	else
	    min01 = c1, max01 = c0;
	if (c2 < c3)
	    min23 = c2, max23 = c3;
	else
	    min23 = c3, max23 = c2;
	if (max(max01, max23) - min(min01, min23) > pfs->cc_max_error[ci])
	    return false;
    }
    return true;
}

/* Fill one piece of a shading. */
int
shade_fill_path(const shading_fill_state_t * pfs, gx_path * ppath,
		gx_device_color * pdevc)
{
    gx_fill_params params;

    params.rule = -1;		/* irrelevant */
    params.adjust = pfs->pis->fill_adjust;
    params.flatness = 0;	/* irrelevant */
    params.fill_zero_width = false;
    return (*dev_proc(pfs->dev, fill_path)) (pfs->dev, pfs->pis, ppath,
					     &params, pdevc, NULL);
}
