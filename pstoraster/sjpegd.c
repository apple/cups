/*
  Copyright 1993-2001 by Easy Software Products.
  Copyright (C) 1994, 1998 Aladdin Enterprises.  All rights reserved.
  
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
/*$Id: sjpegd.c,v 1.4 2001/01/22 15:03:56 mike Exp $ */
/* Interface routines for IJG decoding code. */
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
gs_jpeg_create_decompress(stream_DCT_state * st)
{				/* Initialize error handling */
    gs_jpeg_error_setup(st);
    /* Establish the setjmp return context for gs_jpeg_error_exit to use. */
    if (setjmp(st->data.common->exit_jmpbuf))
	return_error(gs_jpeg_log_error(st));

    jpeg_create_decompress(&st->data.decompress->dinfo);
    jpeg_stream_data_common_init(st->data.decompress);
    return 0;
}

int
gs_jpeg_read_header(stream_DCT_state * st,
		    boolean require_image)
{
    if (setjmp(st->data.common->exit_jmpbuf))
	return_error(gs_jpeg_log_error(st));
    return jpeg_read_header(&st->data.decompress->dinfo, require_image);
}

int
gs_jpeg_start_decompress(stream_DCT_state * st)
{
    if (setjmp(st->data.common->exit_jmpbuf))
	return_error(gs_jpeg_log_error(st));
#if JPEG_LIB_VERSION > 55
    return (int)jpeg_start_decompress(&st->data.decompress->dinfo);
#else
    /* in IJG version 5, jpeg_start_decompress had no return value */
    jpeg_start_decompress(&st->data.decompress->dinfo);
    return 1;
#endif
}

int
gs_jpeg_read_scanlines(stream_DCT_state * st,
		       JSAMPARRAY scanlines,
		       int max_lines)
{
    if (setjmp(st->data.common->exit_jmpbuf))
	return_error(gs_jpeg_log_error(st));
    return (int)jpeg_read_scanlines(&st->data.decompress->dinfo,
				    scanlines, (JDIMENSION) max_lines);
}

int
gs_jpeg_finish_decompress(stream_DCT_state * st)
{
    if (setjmp(st->data.common->exit_jmpbuf))
	return_error(gs_jpeg_log_error(st));
    return (int)jpeg_finish_decompress(&st->data.decompress->dinfo);
}
#endif /* HAVE_LIBJPEG */
