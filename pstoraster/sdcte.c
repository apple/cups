/*
  Copyright 1993-2001 by Easy Software Products.
  Copyright 1994, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: sdcte.c,v 1.6 2001/01/22 15:03:56 mike Exp $ */
/* DCT encoding filter stream */
#include "memory_.h"
#include "stdio_.h"
#include "jpeglib.h"
#include "jerror.h"
#include "gdebug.h"
#include "gsmemory.h"		/* for gsmalloc.h */
#include "gsmalloc.h"		/* for gs_memory_default */
#include "strimpl.h"
#include "sdct.h"
#include "sjpeg.h"

/* ------ DCTEncode ------ */

/* JPEG destination manager procedures */
private void
dcte_init_destination(j_compress_ptr cinfo)
{
}
private boolean
dcte_empty_output_buffer(j_compress_ptr cinfo)
{
    return FALSE;
}
private void
dcte_term_destination(j_compress_ptr cinfo)
{
}

/* Set the defaults for the DCTEncode filter. */
private void
s_DCTE_set_defaults(stream_state * st)
{
    stream_DCT_state *const ss = (stream_DCT_state *) st;

    s_DCT_set_defaults(st);
    ss->QFactor = 1.0;
    ss->ColorTransform = 0;
    ss->Markers.data = 0;
    ss->Markers.size = 0;
    ss->NoMarker = true;
}

/* Initialize DCTEncode filter */
private int
s_DCTE_init(stream_state * st)
{
    stream_DCT_state *const ss = (stream_DCT_state *) st;
    struct jpeg_destination_mgr *dest = &ss->data.compress->destination;

    dest->init_destination = dcte_init_destination;
    dest->empty_output_buffer = dcte_empty_output_buffer;
    dest->term_destination = dcte_term_destination;
    ss->data.common->memory = ss->jpeg_memory;
    ss->data.compress->cinfo.dest = dest;
    ss->phase = 0;
    return 0;
}

/* Process a buffer */
private int
s_DCTE_process(stream_state * st, stream_cursor_read * pr,
	       stream_cursor_write * pw, bool last)
{
    stream_DCT_state *const ss = (stream_DCT_state *) st;
    jpeg_compress_data *jcdp = ss->data.compress;
    struct jpeg_destination_mgr *dest = jcdp->cinfo.dest;

    dest->next_output_byte = pw->ptr + 1;
    dest->free_in_buffer = pw->limit - pw->ptr;
    switch (ss->phase) {
	case 0:		/* not initialized yet */
	    if (gs_jpeg_start_compress(ss, TRUE) < 0)
		return ERRC;
	    pw->ptr = dest->next_output_byte - 1;
	    ss->phase = 1;
	    /* falls through */
	case 1:		/* initialized, Markers not written */
	    if (pw->limit - pw->ptr < ss->Markers.size)
		return 1;
	    memcpy(pw->ptr + 1, ss->Markers.data, ss->Markers.size);
	    pw->ptr += ss->Markers.size;
	    ss->phase = 2;
	    /* falls through */
	case 2:		/* still need to write Adobe marker */
	    if (!ss->NoMarker) {
		static const byte Adobe[] =
		{
		    0xFF, JPEG_APP0 + 14, 0, 14,	/* parameter length */
		    'A', 'd', 'o', 'b', 'e',
		    0, 100,	/* Version */
		    0, 0,	/* Flags0 */
		    0, 0,	/* Flags1 */
		    0		/* ColorTransform */
		};

#define ADOBE_MARKER_LEN sizeof(Adobe)
		if (pw->limit - pw->ptr < ADOBE_MARKER_LEN)
		    return 1;
		memcpy(pw->ptr + 1, Adobe, ADOBE_MARKER_LEN);
		pw->ptr += ADOBE_MARKER_LEN;
		*pw->ptr = ss->ColorTransform;
#undef ADOBE_MARKER_LEN
	    }
	    dest->next_output_byte = pw->ptr + 1;
	    dest->free_in_buffer = pw->limit - pw->ptr;
	    ss->phase = 3;
	    /* falls through */
	case 3:		/* markers written, processing data */
	    while (jcdp->cinfo.image_height > jcdp->cinfo.next_scanline) {
		int written;

		/*
		 * The data argument for jpeg_write_scanlines is
		 * declared as a JSAMPARRAY.  There is no corresponding
		 * const type, so we must remove const from the
		 * argument that we are passing here.  (Tom Lane of IJG
		 * judges that providing const analogues of the
		 * interface types wouldn't be worth the trouble.)
		 */
		/*const */ byte *samples = (byte *) (pr->ptr + 1);

		if ((uint) (pr->limit - pr->ptr) < ss->scan_line_size) {
		    if (last)
			return ERRC;	/* premature EOD */
		    return 0;	/* need more data */
		}
		written = gs_jpeg_write_scanlines(ss, &samples, 1);
		if (written < 0)
		    return ERRC;
		pw->ptr = dest->next_output_byte - 1;
		if (!written)
		    return 1;	/* output full */
		pr->ptr += ss->scan_line_size;
	    }
	    ss->phase = 4;
	    /* falls through */
	case 4:		/* all data processed, finishing */
	    /* jpeg_finish_compress can't suspend, so make sure
	     * it has plenty of room to write the last few bytes.
	     */
	    if (pw->limit - pw->ptr < 100)
		return 1;
	    if (gs_jpeg_finish_compress(ss) < 0)
		return ERRC;
	    pw->ptr = dest->next_output_byte - 1;
	    ss->phase = 5;
	    /* falls through */
	case 5:		/* we are DONE */
	    return EOFC;
    }
    /* Default case can't happen.... */
    return ERRC;
}

/* Release the stream */
private void
s_DCTE_release(stream_state * st)
{
    stream_DCT_state *const ss = (stream_DCT_state *) st;

    gs_jpeg_destroy(ss);
    gs_free_object(ss->data.common->memory, ss->data.compress,
		   "s_DCTE_release");
    /* Switch the template pointer back in case we still need it. */
    st->template = &s_DCTE_template;
}

/* Stream template */
const stream_template s_DCTE_template =
{&st_DCT_state, s_DCTE_init, s_DCTE_process, 1000, 4000, s_DCTE_release,
 s_DCTE_set_defaults
};
#endif /* HAVE_LIBJPEG */
