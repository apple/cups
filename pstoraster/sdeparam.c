/*
  Copyright 1993-2001 by Easy Software Products.
  Copyright 1998 Aladdin Enterprises.  All rights reserved.
  
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

#include <config.h>
#ifdef HAVE_LIBJPEG
/*$Id: sdeparam.c,v 1.3 2001/01/22 15:03:56 mike Exp $ */
/* DCTEncode filter parameter setting and reading */
#include "memory_.h"
#include "jpeglib.h"
#include "gserror.h"
#include "gserrors.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gsparam.h"
#include "strimpl.h"		/* sdct.h requires this */
#include "sdct.h"
#include "sdcparam.h"
#include "sjpeg.h"

/* Define a structure for the DCTEncode scalar parameters. */
typedef struct dcte_scalars_s {
    int Columns;
    int Rows;
    int Colors;
    gs_param_string Markers;
    bool NoMarker;
    int Resync;
    int Blend;
} dcte_scalars_t;
private const dcte_scalars_t dcte_scalars_default =
{
    0, 0, -1,
    {0, 0}, 0 /*false */ , 0, 0
};
private const gs_param_item_t s_DCTE_param_items[] =
{
#define dctp(key, type, memb) { key, type, offset_of(dcte_scalars_t, memb) }
    dctp("Columns", gs_param_type_int, Columns),
    dctp("Rows", gs_param_type_int, Rows),
    dctp("Colors", gs_param_type_int, Colors),
    dctp("Marker", gs_param_type_string, Markers),
    dctp("NoMarker", gs_param_type_bool, NoMarker),
    dctp("Resync", gs_param_type_int, Resync),
    dctp("Blend", gs_param_type_int, Blend),
#undef dctp
    gs_param_item_end
};

/* ================ Get parameters ================ */

stream_state_proc_get_params(s_DCTE_get_params, stream_DCT_state);	/* check */

/* Get a set of sampling values. */
private int
dcte_get_samples(gs_param_list * plist, gs_param_name key, int num_colors,
 const jpeg_compress_data * jcdp, gs_memory_t * mem, bool is_vert, bool all)
{
    const jpeg_component_info *comp_info = jcdp->cinfo.comp_info;
    int samples[4];
    bool write = all;
    int i;

    for (i = 0; i < num_colors; ++i)
	write |= (samples[i] = (is_vert ? comp_info[i].v_samp_factor :
				comp_info[i].h_samp_factor)) != 1;
    if (write) {
	int *data = (int *)gs_alloc_byte_array(mem, num_colors, sizeof(int),
					       "dcte_get_samples");
	gs_param_int_array sa;

	if (data == 0)
	    return_error(gs_error_VMerror);
	sa.data = data;
	sa.size = num_colors;
	sa.persistent = true;
	memcpy(data, samples, num_colors * sizeof(samples[0]));
	return param_write_int_array(plist, key, &sa);
    }
    return 0;
}

int
s_DCTE_get_params(gs_param_list * plist, const stream_DCT_state * ss, bool all)
{
    gs_memory_t *mem = ss->memory;
    stream_DCT_state dcts_defaults;
    const stream_DCT_state *defaults = 0;
    dcte_scalars_t params;
    const jpeg_compress_data *jcdp = ss->data.compress;
    int code;

    if (!all) {
	jpeg_compress_data *jcdp_default =
	(jpeg_compress_data *)
	gs_alloc_bytes_immovable(mem, sizeof(*jcdp), "s_DCTE_get_params");

	if (jcdp_default == 0)
	    return_error(gs_error_VMerror);
	defaults = &dcts_defaults;
	(*s_DCTE_template.set_defaults) ((stream_state *) & dcts_defaults);
	dcts_defaults.data.compress = jcdp_default;
	jcdp_default->memory = dcts_defaults.jpeg_memory = mem;
	if ((code = gs_jpeg_create_compress(&dcts_defaults)) < 0)
	    goto fail;		/* correct to do jpeg_destroy here */
/****** SET DEFAULTS HERE ******/
	dcts_defaults.data.common->Picky = 0;
	dcts_defaults.data.common->Relax = 0;
    }
    params.Columns = jcdp->cinfo.image_width;
    params.Rows = jcdp->cinfo.image_height;
    params.Colors = jcdp->cinfo.input_components;
    params.Markers.data = ss->Markers.data;
    params.Markers.size = ss->Markers.size;
    params.Markers.persistent = false;
    params.NoMarker = ss->NoMarker;
    params.Resync = jcdp->cinfo.restart_interval;
    /* What about Blend?? */
    if ((code = s_DCT_get_params(plist, ss, defaults)) < 0 ||
	(code = gs_param_write_items(plist, &params,
				     &dcte_scalars_default,
				     s_DCTE_param_items)) < 0 ||
	(code = dcte_get_samples(plist, "HSamples", params.Colors,
				 jcdp, mem, false, all)) < 0 ||
	(code = dcte_get_samples(plist, "VSamples", params.Colors,
				 jcdp, mem, true, all)) < 0 ||
    (code = s_DCT_get_quantization_tables(plist, ss, defaults, true)) < 0 ||
	(code = s_DCT_get_huffman_tables(plist, ss, defaults, true)) < 0
	)
	DO_NOTHING;
/****** NYI ******/
  fail:if (defaults) {
	gs_jpeg_destroy(&dcts_defaults);
	gs_free_object(mem, dcts_defaults.data.compress,
		       "s_DCTE_get_params");
    }
    return code;
}

/* ================ Put parameters ================ */

stream_state_proc_put_params(s_DCTE_put_params, stream_DCT_state);	/* check */

/* Put a set of sampling values. */
private int
dcte_put_samples(gs_param_list * plist, gs_param_name key, int num_colors,
		 jpeg_compress_data * jcdp, bool is_vert)
{
    int code;
    int i;
    jpeg_component_info *comp_info = jcdp->cinfo.comp_info;
    UINT8 samples[4];

    /*
     * Adobe default is all sampling factors = 1,
     * which is NOT the IJG default, so we must always assign values.
     */
    switch ((code = s_DCT_byte_params(plist, key, 0, num_colors,
				      samples))
	) {
	default:		/* error */
	    return code;
	case 0:
	    break;
	case 1:
	    samples[0] = samples[1] = samples[2] = samples[3] = 1;
    }
    for (i = 0; i < num_colors; i++) {
	if (samples[i] < 1 || samples[i] > 4)
	    return_error(gs_error_rangecheck);
	if (is_vert)
	    comp_info[i].v_samp_factor = samples[i];
	else
	    comp_info[i].h_samp_factor = samples[i];
    }
    return 0;
}

/* Main procedure */
int
s_DCTE_put_params(gs_param_list * plist, stream_DCT_state * pdct)
{
    jpeg_compress_data *jcdp = pdct->data.compress;
    dcte_scalars_t params;
    int i;
    int code;

    params = dcte_scalars_default;
    /*
     * Required parameters for DCTEncode.
     * (DCTDecode gets the equivalent info from the SOF marker.)
     */
    code = gs_param_read_items(plist, &params, s_DCTE_param_items);
    if (code < 0)
	return code;
    if (params.Columns <= 0 || params.Columns > 0xffff ||
	params.Rows <= 0 || params.Rows > 0xffff ||
	params.Colors <= 0 || params.Colors == 2 || params.Colors > 4 ||
	params.Resync < 0 || params.Resync > 0xffff ||
	params.Blend < 0 || params.Blend > 1
	)
	return_error(gs_error_rangecheck);
/****** HACK: SET DEFAULTS HERE ******/
    jcdp->Picky = 0;
    jcdp->Relax = 0;
    if ((code = s_DCT_put_params(plist, pdct)) < 0 ||
	(code = s_DCT_put_huffman_tables(plist, pdct, false)) < 0
	)
	return code;
    switch ((code = s_DCT_put_quantization_tables(plist, pdct, false))) {
	case 0:
	    break;
	default:
	    return code;
	case 1:
	    /* No QuantTables, but maybe a QFactor to apply to default. */
	    if (pdct->QFactor != 1.0) {
		code = gs_jpeg_set_linear_quality(pdct,
					     (int)(min(pdct->QFactor, 100.0)
						   * 100.0 + 0.5),
						  TRUE);
		if (code < 0)
		    return code;
	    }
    }
    /* Set up minimal image description & call set_defaults */
    jcdp->cinfo.image_width = params.Columns;
    jcdp->cinfo.image_height = params.Rows;
    jcdp->cinfo.input_components = params.Colors;
    switch (params.Colors) {
	case 1:
	    jcdp->cinfo.in_color_space = JCS_GRAYSCALE;
	    break;
	case 3:
	    jcdp->cinfo.in_color_space = JCS_RGB;
	    break;
	case 4:
	    jcdp->cinfo.in_color_space = JCS_CMYK;
	    break;
	default:
	    jcdp->cinfo.in_color_space = JCS_UNKNOWN;
    }
    if ((code = gs_jpeg_set_defaults(pdct)) < 0)
	return code;
    /* Change IJG colorspace defaults as needed;
     * set ColorTransform to what will go in the Adobe marker.
     */
    switch (params.Colors) {
	case 3:
	    if (pdct->ColorTransform < 0)
		pdct->ColorTransform = 1;	/* default */
	    if (pdct->ColorTransform == 0) {
		if ((code = gs_jpeg_set_colorspace(pdct, JCS_RGB)) < 0)
		    return code;
	    } else
		pdct->ColorTransform = 1;	/* flag YCC xform */
	    break;
	case 4:
	    if (pdct->ColorTransform < 0)
		pdct->ColorTransform = 0;	/* default */
	    if (pdct->ColorTransform != 0) {
		if ((code = gs_jpeg_set_colorspace(pdct, JCS_YCCK)) < 0)
		    return code;
		pdct->ColorTransform = 2;	/* flag YCCK xform */
	    } else {
		if ((code = gs_jpeg_set_colorspace(pdct, JCS_CMYK)) < 0)
		    return code;
	    }
	    break;
	default:
	    pdct->ColorTransform = 0;	/* no transform otherwise */
	    break;
    }
    /* Optional encoding-only parameters */
    pdct->Markers.data = params.Markers.data;
    pdct->Markers.size = params.Markers.size;
    pdct->NoMarker = params.NoMarker;
    if ((code = dcte_put_samples(plist, "HSamples", params.Colors,
				 jcdp, false)) < 0 ||
	(code = dcte_put_samples(plist, "VSamples", params.Colors,
				 jcdp, true)) < 0
	)
	return code;
    jcdp->cinfo.write_JFIF_header = FALSE;
    jcdp->cinfo.write_Adobe_marker = FALSE;	/* must do it myself */
    jcdp->cinfo.restart_interval = params.Resync;
    /* What to do with Blend ??? */
    if (pdct->data.common->Relax == 0) {
	jpeg_component_info *comp_info = jcdp->cinfo.comp_info;
	int num_samples;

	for (i = 0, num_samples = 0; i < params.Colors; i++)
	    num_samples += comp_info[i].h_samp_factor *
		comp_info[i].v_samp_factor;
	if (num_samples > 10)
	    return_error(gs_error_rangecheck);
	/*
	 * Note: by default the IJG software does not allow
	 * num_samples to exceed 10, Relax or no.  For full
	 * compatibility with Adobe's non-JPEG-compliant
	 * software, set MAX_BLOCKS_IN_MCU to 64 in jpeglib.h.
	 */
    }
    return 0;
}
#endif /* HAVE_LIBJPEG */
