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

/* sjpeg.h */
/* Definitions for Ghostscript's IJG library interface routines */
/* Requires sdct.h, jpeg/jpeglib.h */

/*
 * Each routine gs_jpeg_xxx is equivalent to the IJG entry point jpeg_xxx,
 * except that
 *   (a) it takes a pointer to stream_DCT_state instead of just the IJG
 *       jpeg_(de)compress_data struct;
 *   (b) it catches any error exit from the IJG code and converts it into
 *       an error return value per Ghostscript custom.  A negative return
 *       value is an error code, except for gs_jpeg_alloc_xxx which return
 *       NULL (indicating e_VMerror).
 */

/* Common to encode/decode */

void gs_jpeg_error_setup (P1(stream_DCT_state *st));
int gs_jpeg_log_error (P1(stream_DCT_state *st));
JQUANT_TBL * gs_jpeg_alloc_quant_table (P1(stream_DCT_state *st));
JHUFF_TBL * gs_jpeg_alloc_huff_table (P1(stream_DCT_state *st));
int gs_jpeg_destroy (P1(stream_DCT_state *st));

/* Encode */

int gs_jpeg_create_compress (P1(stream_DCT_state *st));
int gs_jpeg_set_defaults (P1(stream_DCT_state *st));
int gs_jpeg_set_colorspace (P2(stream_DCT_state *st,
			       J_COLOR_SPACE colorspace));
int gs_jpeg_set_linear_quality (P3(stream_DCT_state *st,
				   int scale_factor,
				   boolean force_baseline));
int gs_jpeg_start_compress (P2(stream_DCT_state *st,
			       boolean write_all_tables));
int gs_jpeg_write_scanlines (P3(stream_DCT_state *st,
				JSAMPARRAY scanlines,
				int num_lines));
int gs_jpeg_finish_compress (P1(stream_DCT_state *st));

/* Decode */

int gs_jpeg_create_decompress (P1(stream_DCT_state *st));
int gs_jpeg_read_header (P2(stream_DCT_state *st,
			    boolean require_image));
int gs_jpeg_start_decompress (P1(stream_DCT_state *st));
int gs_jpeg_read_scanlines (P3(stream_DCT_state *st,
			       JSAMPARRAY scanlines,
			       int max_lines));
int gs_jpeg_finish_decompress (P1(stream_DCT_state *st));
