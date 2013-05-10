/* Copyright (C) 1994 Aladdin Enterprises.  All rights reserved.
  
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

/* zfdcte.c */
/* DCTEncode filter creation */
#include "memory_.h"
#include "stdio_.h"			/* for jpeglib.h */
#include "jpeglib.h"
#include "ghost.h"
#include "errors.h"
#include "oper.h"
#include "idict.h"
#include "idparam.h"
#include "strimpl.h"
#include "sdct.h"
#include "sjpeg.h"
#include "ifilter.h"

/* Import the common setup routines from zfdctc.c */
int zfdct_setup(P2(const ref *op, stream_DCT_state *pdct));
int zfdct_setup_quantization_tables(P3(const ref *op, stream_DCT_state *pdct,
				       bool is_encode));
int zfdct_setup_huffman_tables(P3(const ref *op, stream_DCT_state *pdct,
				  bool is_encode));
int zfdct_byte_params(P4(const ref *op, int start, int count, UINT8 *pvals));

/* Collect encode-only parameters. */
private int
dct_setup_samples(const ref *op, const char _ds *kstr, int num_colors,
		  jpeg_compress_data *jcdp, bool is_vert)
{	int code;
	int i;
	ref *pdval;
	jpeg_component_info * comp_info = jcdp->cinfo.comp_info;
	UINT8 samples[4];
	/* Adobe default is all sampling factors = 1,
	 * which is NOT the IJG default, so we must always assign values.
	 */
	if ( op != 0 && dict_find_string(op, kstr, &pdval) > 0 )
	{	if ( r_size(pdval) < num_colors )
			return_error(e_rangecheck);
		if ( (code = zfdct_byte_params(pdval, 0, num_colors, samples)) < 0 )
			return code;
	}
	else
	{	samples[0] = samples[1] = samples[2] = samples[3] = 1;
	}
	for ( i = 0; i < num_colors; i++ )
	{	if ( samples[i] < 1 || samples[i] > 4 )
			return_error(e_rangecheck);
		if ( is_vert )
			comp_info[i].v_samp_factor = samples[i];
		else
			comp_info[i].h_samp_factor = samples[i];
	}
	return 0;
}

private int
zfdcte_setup(const ref *op, stream_DCT_state *pdct)
{	jpeg_compress_data *jcdp = pdct->data.compress;
	uint Columns, Rows, Resync;
	int num_colors;
	int Blend;
	ref *mstr;
	int i;
	int code;

	/* Required parameters for DCTEncode.
	 * (DCTDecode gets the equivalent info from the SOF marker.)
	 */
	if ( (code = dict_uint_param(op, "Columns", 1, 0xffff, 0,
				     &Columns)) < 0 ||
	     (code = dict_uint_param(op, "Rows", 1, 0xffff, 0,
				     &Rows)) < 0 ||
	     (code = dict_int_param(op, "Colors", 1, 4, -1,
				    &num_colors)) < 0
	   )
		return code;
	/* Set up minimal image description & call set_defaults */
	jcdp->cinfo.image_width = Columns;
	jcdp->cinfo.image_height = Rows;
	jcdp->cinfo.input_components = num_colors;
	switch ( num_colors )
	{
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
	if ( (code = gs_jpeg_set_defaults(pdct)) < 0 )
		return code;
	/* Change IJG colorspace defaults as needed;
	 * set ColorTransform to what will go in the Adobe marker.
	 */
	switch ( num_colors )
	{
	case 3:
		if ( pdct->ColorTransform < 0 )
			pdct->ColorTransform = 1; /* default */
		if ( pdct->ColorTransform == 0 )
		{
			if ( (code = gs_jpeg_set_colorspace(pdct, JCS_RGB)) < 0 )
				return code;
		}
		else
			pdct->ColorTransform = 1; /* flag YCC xform */
		break;
	case 4:
		if ( pdct->ColorTransform < 0 )
			pdct->ColorTransform = 0; /* default */
		if ( pdct->ColorTransform != 0 )
		{	if ( (code = gs_jpeg_set_colorspace(pdct, JCS_YCCK)) < 0 )
				return code;
			pdct->ColorTransform = 2; /* flag YCCK xform */
		}
		else
		{	if ( (code = gs_jpeg_set_colorspace(pdct, JCS_CMYK)) < 0 )
				return code;
		}
		break;
	default:
		pdct->ColorTransform = 0; /* no transform otherwise */
		break;
	}
	/* Optional encoding-only parameters */
	if ( dict_find_string(op, "Markers", &mstr) > 0 )
	{	check_read_type(*mstr, t_string);
		pdct->Markers.data = mstr->value.const_bytes;
		pdct->Markers.size = r_size(mstr);
	}
	if ( (code = dict_bool_param(op, "NoMarker", false,
				     &pdct->NoMarker)) < 0 ||
	     (code = dict_uint_param(op, "Resync", 0, 0xffff, 0,
				     &Resync)) < 0 ||
	     (code = dict_int_param(op, "Blend", 0, 1, 0,
				    &Blend)) < 0 ||
	     (code = dct_setup_samples(op, "HSamples", num_colors,
				       jcdp, false)) < 0 ||
	     (code = dct_setup_samples(op, "VSamples", num_colors,
				       jcdp, true)) < 0
	    )
		return code;
	jcdp->cinfo.write_JFIF_header = FALSE;
	jcdp->cinfo.write_Adobe_marker = FALSE;	/* must do it myself */
	jcdp->cinfo.restart_interval = Resync;
	/* What to do with Blend ??? */
	if ( pdct->data.common->Relax == 0 )
	{	jpeg_component_info *comp_info = jcdp->cinfo.comp_info;
		int num_samples;
		for ( i = 0, num_samples = 0; i < num_colors; i++ )
			num_samples += comp_info[i].h_samp_factor *
				       comp_info[i].v_samp_factor;
		if ( num_samples > 10 )
			return_error(e_rangecheck);
		/* Note: by default the IJG software does not allow
		 * num_samples to exceed 10, Relax or no.  For full
		 * compatibility with Adobe's non-JPEG-compliant
		 * software, set MAX_BLOCKS_IN_MCU to 64 in jpeglib.h.
		 */
	}
	return 0;
}

/* <target> <dict> DCTEncode/filter <file> */
private int
zDCTE(os_ptr op)
{	stream_DCT_state state;
	jpeg_compress_data *jcdp;
	int code;
	int npop;
	const ref *dop;
	uint dspace;
	ref *pdval;

	/* First allocate space for IJG parameters. */
	jcdp = gs_malloc(1, sizeof(*jcdp), "zDCTE");
	if ( jcdp == 0 )
		return_error(e_VMerror);
	state.data.compress = jcdp;
	if ( (code = gs_jpeg_create_compress(&state)) < 0 )
		goto fail;	/* correct to do jpeg_destroy here */
	/* Read parameters from dictionary */
	if ( (code = zfdct_setup(op, &state)) < 0 )
		goto fail;
	npop = code;
	if ( npop == 0 )
	  dop = 0, dspace = 0;
	else
	  dop = op, dspace = r_space(op);
	if ( (code = zfdcte_setup(dop, &state)) < 0 )
		goto fail;
	/* Check for QFactor without QuantTables. */
	if ( dop == 0 || dict_find_string(dop, "QuantTables", &pdval) <= 0 )
	  {	/* No QuantTables, but maybe a QFactor to apply to default. */
		if ( state.QFactor != 1.0 )
		{	code = gs_jpeg_set_linear_quality(&state,
					(int) (min(state.QFactor, 100.0)
					       * 100.0 + 0.5),
					TRUE);
			if ( code < 0 )
				return code;
		}

	  }
	if ( (code = zfdct_setup_huffman_tables(dop, &state, true)) < 0 ||
	     (code = zfdct_setup_quantization_tables(dop, &state, true)) < 0
	   )
		goto fail;
	/* Create the filter. */
	jcdp->template = s_DCTE_template;
	/* Make sure we get at least a full scan line of input. */
	state.scan_line_size = jcdp->cinfo.input_components *
			       jcdp->cinfo.image_width;
	jcdp->template.min_in_size =
		max(s_DCTE_template.min_in_size, state.scan_line_size);
	/* Make sure we can write the user markers in a single go. */
	jcdp->template.min_out_size =
		max(s_DCTE_template.min_out_size, state.Markers.size);
	code = filter_write(op, npop, &jcdp->template,
			    (stream_state *)&state, dspace);
	if ( code >= 0 )		/* Success! */
		return code;
	/* We assume that if filter_write fails, the stream has not been
	 * registered for closing, so s_DCTE_release will never be called.
	 * Therefore we free the allocated memory before failing.
	 */

fail:
	gs_jpeg_destroy(&state);
	gs_free(jcdp, 1, sizeof(*jcdp), "zDCTE fail");
	return code;
}

/* ------ Initialization procedure ------ */

BEGIN_OP_DEFS(zfdcte_op_defs) {
		op_def_begin_filter(),
	{"2DCTEncode", zDCTE},
END_OP_DEFS(0) }
