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
#include <config.h>
#ifdef HAVE_LIBJPEG
/* sjpege.c */
/* Interface routines for IJG encoding code. */
#include "stdio_.h"
#include "string_.h"
#include "jpeglib.h"
#include "jerror.h"
#include "gx.h"
#include "gserrors.h"
#include "strimpl.h"
#include "sdct.h"
#include "sjpeg.h"

/*
 * Interface routines.  This layer of routines exists solely to limit
 * side-effects from using setjmp.
 */

int
gs_jpeg_create_compress (stream_DCT_state *st)
{	/* Initialize error handling */
	gs_jpeg_error_setup(st);
	/* Establish the setjmp return context for gs_jpeg_error_exit to use. */
	if (setjmp(st->data.common->exit_jmpbuf))
		return_error(gs_jpeg_log_error(st));

	jpeg_create_compress(&st->data.compress->cinfo);
	return 0;
}

int
gs_jpeg_set_defaults (stream_DCT_state *st)
{	if (setjmp(st->data.common->exit_jmpbuf))
		return_error(gs_jpeg_log_error(st));
	jpeg_set_defaults(&st->data.compress->cinfo);
	return 0;
}

int
gs_jpeg_set_colorspace (stream_DCT_state *st,
			J_COLOR_SPACE colorspace)
{	if (setjmp(st->data.common->exit_jmpbuf))
		return_error(gs_jpeg_log_error(st));
	jpeg_set_colorspace(&st->data.compress->cinfo, colorspace);
	return 0;
}

int
gs_jpeg_set_linear_quality (stream_DCT_state *st,
			    int scale_factor, boolean force_baseline)
{	if (setjmp(st->data.common->exit_jmpbuf))
		return_error(gs_jpeg_log_error(st));
	jpeg_set_linear_quality(&st->data.compress->cinfo,
				scale_factor, force_baseline);
	return 0;
}

int
gs_jpeg_start_compress (stream_DCT_state *st,
			boolean write_all_tables)
{	if (setjmp(st->data.common->exit_jmpbuf))
		return_error(gs_jpeg_log_error(st));
	jpeg_start_compress(&st->data.compress->cinfo, write_all_tables);
	return 0;
}

int
gs_jpeg_write_scanlines (stream_DCT_state *st,
			 JSAMPARRAY scanlines,
			 int num_lines)
{	if (setjmp(st->data.common->exit_jmpbuf))
		return_error(gs_jpeg_log_error(st));
	return (int) jpeg_write_scanlines(&st->data.compress->cinfo,
					  scanlines, (JDIMENSION) num_lines);
}

int
gs_jpeg_finish_compress (stream_DCT_state *st)
{	if (setjmp(st->data.common->exit_jmpbuf))
		return_error(gs_jpeg_log_error(st));
	jpeg_finish_compress(&st->data.compress->cinfo);
	return 0;
}
#endif
