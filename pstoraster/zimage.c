/* Copyright (C) 1989, 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: zimage.c,v 1.1 2000/03/08 23:15:39 mike Exp $ */
/* Image operators */
#include "ghost.h"
#include "oper.h"
#include "estack.h"		/* for image[mask] */
#include "gsstruct.h"
#include "ialloc.h"
#include "igstate.h"
#include "ilevel.h"
#include "store.h"
#include "gscspace.h"
#include "gsmatrix.h"
#include "gsimage.h"
#include "gxiparam.h"
#include "stream.h"
#include "ifilter.h"		/* for stream exception handling */
#include "iimage.h"

/* Forward references */
private int image_setup(P4(gs_image_t * pim, os_ptr op,
			   const gs_color_space * pcs, int npop));
private int image_proc_continue(P1(os_ptr));
private int image_file_continue(P1(os_ptr));
private int image_file_buffered_continue(P1(os_ptr));
private int image_string_process(P3(os_ptr, gs_image_enum *, int));
private int image_cleanup(P1(os_ptr));

/* <width> <height> <bits/sample> <matrix> <datasrc> image - */
int
zimage(register os_ptr op)
{
    return zimage_opaque_setup(op, false, gs_image_alpha_none,
		     gs_cspace_DeviceGray((const gs_imager_state *)igs), 5);
}

/* <width> <height> <paint_1s> <matrix> <datasrc> imagemask - */
int
zimagemask(register os_ptr op)
{
    gs_image_t image;

    check_type(op[-2], t_boolean);
    gs_image_t_init_mask(&image, op[-2].value.boolval);
    return image_setup(&image, op, NULL, 5);
}

/* Common setup for [color|alpha]image. */
/* Fills in format, BitsPerComponent, Alpha. */
int
zimage_opaque_setup(os_ptr op, bool multi, gs_image_alpha_t alpha,
		    const gs_color_space * pcs, int npop)
{
    gs_image_t image;

    check_int_leu(op[-2], (level2_enabled ? 12 : 8));	/* bits/sample */
    gs_image_t_init(&image, pcs);
    image.BitsPerComponent = (int)op[-2].value.intval;
    image.Alpha = alpha;
    image.format =
	(multi ? gs_image_format_component_planar : gs_image_format_chunky);
    return image_setup(&image, op, pcs, npop);
}

/* Common setup for [color|alpha]image and imagemask. */
/* Fills in Width, Height, ImageMatrix, ColorSpace. */
private int
image_setup(gs_image_t * pim, os_ptr op, const gs_color_space * pcs, int npop)
{
    int code;

    check_type(op[-4], t_integer);	/* width */
    check_type(op[-3], t_integer);	/* height */
    if (op[-4].value.intval < 0 || op[-3].value.intval < 0)
	return_error(e_rangecheck);
    if ((code = read_matrix(op - 1, &pim->ImageMatrix)) < 0)
	return code;
    pim->ColorSpace = pcs;
    pim->Width = (int)op[-4].value.intval;
    pim->Height = (int)op[-3].value.intval;
    return zimage_setup((gs_pixel_image_t *) pim, op,
			pim->ImageMask | pim->CombineWithColor, npop);
}

/* Common setup for all Level 1 and 2 images, and ImageType 4 images. */
int
zimage_setup(const gs_pixel_image_t * pim, const ref * sources,
	     bool uses_color, int npop)
{
    gx_image_enum_common_t *pie;
    int code =
	gs_image_begin_typed((const gs_image_common_t *)pim, igs,
			     uses_color, &pie);

    if (code < 0)
	return code;
    return zimage_data_setup((const gs_pixel_image_t *)pim, pie,
			     sources, npop);
}

/* Common setup for all Level 1 and 2 images, and ImageType 3 and 4 images. */
int
zimage_data_setup(const gs_pixel_image_t * pim, gx_image_enum_common_t * pie,
		  const ref * sources, int npop)
{
    int num_sources = pie->num_planes;

#define NUM_PUSH(nsource) ((nsource) * 2 + 4)	/* see below */
    int inumpush = NUM_PUSH(num_sources);
    int code;
    gs_image_enum *penum;
    int px;
    const ref *pp;
    bool must_buffer = false;

    /*
     * We push the following on the estack.
     *      Control mark,
     *      num_sources times (plane N-1 first, plane 0 last):
     *          row buffers (only if must_buffer, otherwise null),
     *          data sources,
     *      current plane index,
     *      current byte in row (only if must_buffer, otherwise 0),
     *      enumeration structure.
     */
    check_estack(inumpush + 2);	/* stuff above, + continuation + proc */
    /*
     * Note that the data sources may be procedures, strings, or (Level
     * 2 only) files.  (The Level 1 reference manual says that Level 1
     * requires procedures, but Adobe Level 1 interpreters also accept
     * strings.)  The sources must all be of the same type.
     *
     * If the sources are files, and two or more are the same file,
     * we must buffer data for each row; otherwise, we can deliver the
     * data directly out of the stream buffers.  This is OK even if
     * some of the sources are filters on the same file, since they
     * have separate buffers.
     */
    for (px = 0, pp = sources; px < num_sources; px++, pp++) {
	switch (r_type(pp)) {
	    case t_file:
		if (!level2_enabled)
		    return_error(e_typecheck);
		/* Check for aliasing. */
		{
		    int pi;

		    for (pi = 0; pi < px; ++pi)
			if (sources[pi].value.pfile == pp->value.pfile)
			    must_buffer = true;
		}
		/* falls through */
	    case t_string:
		if (r_type(pp) != r_type(sources))
		    return_error(e_typecheck);
		check_read(*pp);
		break;
	    default:
		if (!r_is_proc(sources))
		    return_error(e_typecheck);
		check_proc(*pp);
	}
    }
    if ((penum = gs_image_enum_alloc(imemory, "image_setup")) == 0)
	return_error(e_VMerror);
    code = gs_image_common_init(penum, pie, (const gs_data_image_t *)pim,
				imemory, gs_currentdevice(igs));
    if (code != 0) {		/* error, or empty image */
	ifree_object(penum, "image_setup");
	if (code >= 0)		/* empty image */
	    pop(npop);
	return code;
    }
    push_mark_estack(es_other, image_cleanup);
    ++esp;
    for (px = 0, pp = sources + num_sources - 1;
	 px < num_sources; esp += 2, ++px, --pp
	) {
	make_null(esp);		/* buffer */
	esp[1] = *pp;
    }
    esp += 2;
    make_int(esp - 2, 0);	/* current plane */
    make_int(esp - 1, 0);	/* current byte in row */
    make_istruct(esp, 0, penum);
    switch (r_type(sources)) {
	case t_file:
	    if (must_buffer) {	/* Allocate a buffer for each row. */
		for (px = 0; px < num_sources; ++px) {
		    uint size = gs_image_bytes_per_plane_row(penum, px);
		    byte *sbody = ialloc_string(size, "image_setup");

		    if (sbody == 0) {
			esp -= inumpush;
			image_cleanup(osp);
			return_error(e_VMerror);
		    }
		    make_string(esp - 4 - px * 2,
				icurrent_space, size, sbody);
		}
		push_op_estack(image_file_buffered_continue);
	    } else {
		push_op_estack(image_file_continue);
	    }
	    break;
	case t_string:
	    pop(npop);
	    return image_string_process(osp, penum, num_sources);
	default:		/* procedure */
	    push_op_estack(image_proc_continue);
	    *++esp = sources[0];
	    break;
    }
    pop(npop);
    return o_push_estack;
}
/* Pop all the control information off the e-stack. */
private es_ptr
zimage_pop_estack(es_ptr tep)
{
    es_ptr ep = tep - 3;

    while (!r_is_estack_mark(ep))
	ep -= 2;
    return ep - 1;
}
/* Continuation for procedure data source. */
private int
image_proc_continue(register os_ptr op)
{
    gs_image_enum *penum = r_ptr(esp, gs_image_enum);
    uint size, used;
    int code;
    int px;
    const ref *pp;

    if (!r_has_type_attrs(op, t_string, a_read)) {
	check_op(1);
	/* Procedure didn't return a (readable) string.  Quit. */
	esp = zimage_pop_estack(esp);
	image_cleanup(op);
	return_error(!r_has_type(op, t_string) ? e_typecheck : e_invalidaccess);
    }
    size = r_size(op);
    if (size == 0)
	code = 1;
    else
	code = gs_image_next(penum, op->value.bytes, size, &used);
    if (code) {			/* Stop now. */
	esp = zimage_pop_estack(esp);
	pop(1);
	op = osp;
	image_cleanup(op);
	return (code < 0 ? code : o_pop_estack);
    }
    pop(1);
    px = (int)(esp[-2].value.intval) + 1;
    pp = esp - 3 - px * 2;
    if (r_is_estack_mark(pp))
	px = 0, pp = esp - 3;
    esp[-2].value.intval = px;
    push_op_estack(image_proc_continue);
    *++esp = *pp;
    return o_push_estack;
}
/* Continue processing data from an image with file data sources */
/* and no file buffering. */
private int
image_file_continue(os_ptr op)
{
    gs_image_enum *penum = r_ptr(esp, gs_image_enum);
    const ref *pproc = esp - 3;

    for (;;) {
	uint size = max_uint;
	int code;
	int pn, px;
	const ref *pp;

	/*
	 * Do a first pass through the files to ensure that they all
	 * have data available in their buffers, and compute the min
	 * of the available amounts.
	 */

	for (pn = 0, pp = pproc; !r_is_estack_mark(pp);
	     ++pn, pp -= 2
	    ) {
	    stream *s = pp->value.pfile;
	    int min_left = sbuf_min_left(s);
	    uint avail;

	    while ((avail = sbufavailable(s)) <= min_left) {
		int next = sgetc(s);

		if (next >= 0) {
		    sputback(s);
		    if (s->end_status == EOFC || s->end_status == ERRC)
			min_left = 0;
		    continue;
		}
		switch (next) {
		    case EOFC:
			break;	/* with avail = 0 */
		    case INTC:
		    case CALLC:
			return
			    s_handle_read_exception(next, pp,
					      NULL, 0, image_file_continue);
		    default:
			/* case ERRC: */
			return_error(e_ioerror);
		}
		break;		/* for EOFC */
	    }
	    /* Note that in the EOF case, we can get here with */
	    /* avail < min_left. */
	    if (avail >= min_left) {
		avail -= min_left;
		if (avail < size)
		    size = avail;
	    } else
		size = 0;
	}

	/* Now pass the min of the available buffered data to */
	/* the image processor. */

	if (size == 0)
	    code = 1;
	else {
	    int pi;
	    uint used;		/* only set for the last plane */

	    for (px = 0, pp = pproc, code = 0; px < pn && !code;
		 ++px, pp -= 2
		)
		code = gs_image_next(penum, sbufptr(pp->value.pfile),
				     size, &used);
	    /* Now that used has been set, update the streams. */
	    for (pi = 0, pp = pproc; pi < px; ++pi, pp -= 2)
		sbufskip(pp->value.pfile, used);
	}
	if (code) {
	    esp = zimage_pop_estack(esp);
	    image_cleanup(op);
	    return (code < 0 ? code : o_pop_estack);
	}
    }
}
/* Continue processing data from an image with file data sources */
/* and file buffering.  This is similar to the procedure case. */
private int
image_file_buffered_continue(os_ptr op)
{
    gs_image_enum *penum = r_ptr(esp, gs_image_enum);
    const ref *pproc = esp - 3;
    int px = esp[-2].value.intval;
    int dpos = esp[-1].value.intval;
    int code = 0;

    while (!code) {
	const ref *pp;

	/****** 0 IS BOGUS ******/
	uint size = gs_image_bytes_per_plane_row(penum, 0);
	uint avail = size;
	uint used;
	int pi;

	/* Accumulate data until we have a full set of planes. */
	while (!r_is_estack_mark(pp = pproc - px * 2)) {
	    const ref *pb = pp - 1;
	    uint used;
	    int status = sgets(pp->value.pfile, pb->value.bytes,
			       size - dpos, &used);

	    if ((dpos += used) == size)
		dpos = 0, ++px;
	    else
		switch (status) {
		    case EOFC:
			if (dpos < avail)
			    avail = dpos;
			dpos = 0, ++px;
			break;
		    case INTC:
		    case CALLC:
			/* Call out to read from a procedure-based stream. */
			esp[-2].value.intval = px;
			esp[-1].value.intval = dpos;
			return s_handle_read_exception(status, pp,
				     NULL, 0, image_file_buffered_continue);
		    default:
			/*case ERRC: */
			return_error(e_ioerror);
		}
	}
	/* Pass the data to the image processor. */
	if (avail == 0) {
	    code = 1;
	    break;
	}
	for (pi = 0, pp = pproc; pi < px && !code; ++pi, pp -= 2)
	    code = gs_image_next(penum, pp->value.bytes, avail, &used);
	/* Reinitialize for the next row. */
	px = dpos = 0;
    }
    esp = zimage_pop_estack(esp);
    image_cleanup(op);
    return (code < 0 ? code : o_pop_estack);
}
/* Process data from an image with string data sources. */
/* This never requires callbacks, so it's simpler. */
private int
image_string_process(os_ptr op, gs_image_enum * penum, int num_sources)
{
    int px = 0;

    for (;;) {
	const ref *psrc = esp - 3 - px * 2;
	uint size = r_size(psrc);
	uint used;
	int code;

	if (size == 0)
	    code = 1;
	else
	    code = gs_image_next(penum, psrc->value.bytes, size, &used);
	if (code) {		/* Stop now. */
	    esp -= NUM_PUSH(num_sources);
	    image_cleanup(op);
	    return (code < 0 ? code : o_pop_estack);
	}
	if (++px == num_sources)
	    px = 0;
    }
}
/* Clean up after enumerating an image */
private int
image_cleanup(os_ptr op)
{
    gs_image_enum *penum;
    const ref *pb;

    /* Free any row buffers, in LIFO order as usual. */
    for (pb = esp + 2; !r_has_type(pb, t_integer); pb += 2)
	if (r_has_type(pb, t_string))
	    gs_free_string(imemory, pb->value.bytes, r_size(pb),
			   "image_cleanup");
    penum = r_ptr(pb + 2, gs_image_enum);
    gs_image_cleanup(penum);
    ifree_object(penum, "image_cleanup");
    return 0;
}

/* ------ Initialization procedure ------ */

const op_def zimage_op_defs[] =
{
    {"5image", zimage},
    {"5imagemask", zimagemask},
		/* Internal operators */
    {"1%image_proc_continue", image_proc_continue},
    {"0%image_file_continue", image_file_continue},
    op_def_end(0)
};
