/* Copyright (C) 1989, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* zpaint.c */
/* Painting operators */
#include "ghost.h"
#include "errors.h"
#include "oper.h"
#include "estack.h"			/* for image[mask] */
#include "gsstruct.h"
#include "ialloc.h"
#include "igstate.h"
#include "ilevel.h"
#include "store.h"
#include "gscspace.h"
#include "gsmatrix.h"
#include "gsimage.h"
#include "gspaint.h"
#include "stream.h"
#include "ifilter.h"		/* for stream exception handling */
#include "iimage.h"

/* Forward references */
private int image_setup(P5(gs_image_t *pim, bool multi, os_ptr op, const gs_color_space *pcs, int npop));
private int image_proc_continue(P1(os_ptr));
private int image_file_continue(P1(os_ptr));
private int image_file_buffered_continue(P1(os_ptr));
private int image_string_process(P3(os_ptr, gs_image_enum *, int));
private int image_cleanup(P1(os_ptr));

/* - fill - */
private int
zfill(register os_ptr op)
{	return gs_fill(igs);
}

/* - .fillpage - */
private int
zfillpage(register os_ptr op)
{	return gs_fillpage(igs);
}

/* - eofill - */
private int
zeofill(register os_ptr op)
{	return gs_eofill(igs);
}

/* - stroke - */
private int
zstroke(register os_ptr op)
{	return gs_stroke(igs);
}

/* <width> <height> <bits/sample> <matrix> <datasrc> image - */
int
zimage(register os_ptr op)
{	return zimage_opaque_setup(op, false, NULL, 5);
}

/* <width> <height> <paint_1s> <matrix> <datasrc> imagemask - */
int
zimagemask(register os_ptr op)
{	gs_image_t image;

	check_type(op[-2], t_boolean);
	gs_image_t_init_mask(&image, op[-2].value.boolval);
	return image_setup(&image, false, op, NULL, 5);
}

/* Common setup for image and colorimage. */
/* Fills in MultipleDataSources, BitsPerComponent. */
int
zimage_opaque_setup(register os_ptr op, bool multi,
  const gs_color_space *pcs, int npop)
{	gs_image_t image;

	check_int_leu(op[-2], (level2_enabled ? 12 : 8));  /* bits/sample */
	gs_image_t_init_color(&image);
	image.BitsPerComponent = (int)op[-2].value.intval;
	return image_setup(&image, multi, op, pcs, npop);
}

/* Common setup for [color]image and imagemask. */
/* Fills in Width, Height, ImageMatrix, ColorSpace. */
private int
image_setup(gs_image_t *pim, bool multi, register os_ptr op,
  const gs_color_space *pcs, int npop)
{	int code;

	check_type(op[-4], t_integer);	/* width */
	check_type(op[-3], t_integer);	/* height */
	if ( op[-4].value.intval < 0 || op[-3].value.intval < 0 )
	  return_error(e_rangecheck);
	if ( (code = read_matrix(op - 1, &pim->ImageMatrix)) < 0 )
	  return code;
	pim->ColorSpace = pcs;
	pim->Width = (int)op[-4].value.intval;
	pim->Height = (int)op[-3].value.intval;
	return zimage_setup(pim, multi, op, npop);
}

/* Common setup for Level 1 image/imagemask/colorimage and */
/* the Level 2 dictionary form of image/imagemask. */
int
zimage_setup(const gs_image_t *pim, bool multi, const ref *sources, int npop)
{	int code;
	gs_image_enum *penum;
	int px;
	const ref *pp;
	int num_sources =
	  (multi ? gs_color_space_num_components(pim->ColorSpace) : 1);
	bool must_buffer = false;

	/*
	 * We push the following on the estack.  "Optional" values are
	 * set to null if not used, so the offsets will be constant.
	 *	Control mark,
	 *	4 data sources (1-4 actually used),
	 *	4 row buffers (only if must_buffer),
	 *	current plane index,
	 *	current byte in row (only if must_buffer, otherwise 0),
	 *	enumeration structure.
	 */
#define inumpush 12
	check_estack(inumpush + 2);  /* stuff above, + continuation + proc */
	/*
	 * Note that the data sources may be procedures, strings, or (Level
	 * 2 only) files.  (The Level 1 reference manual says that Level 1
	 * requires procedures, but Adobe Level 1 interpreters also accept
	 * strings.)  The sources must all be of the same type.
	 *
	 * If the sources are files, and two or more are the same file,
	 * we must buffer data for each row; otherwise, we can deliver the
	 * data directly out of the stream buffers.
	 */
	for ( px = 0, pp = sources; px < num_sources; px++, pp++ )
	{	switch ( r_type(pp) )
		{
		case t_file:
			if ( !level2_enabled )
			  return_error(e_typecheck);
			/* Check for aliasing. */
			{ int pi;
			  for ( pi = 0; pi < px; ++pi )
			    if ( sources[pi].value.pfile == pp->value.pfile )
			      must_buffer = true;
			}
			/* falls through */
		case t_string:
			if ( r_type(pp) != r_type(sources) )
			  return_error(e_typecheck);
			check_read(*pp);
			break;
		default:
			if ( !r_is_proc(sources) )
			  return_error(e_typecheck);
			check_proc(*pp);
		}
	}
	if ( (penum = gs_image_enum_alloc(imemory, "image_setup")) == 0 )
	  return_error(e_VMerror);
	code = gs_image_init(penum, pim, multi, igs);
	if ( code != 0 )	/* error, or empty image */
	{	ifree_object(penum, "image_setup");
		if ( code >= 0 )	/* empty image */
		  pop(npop);
		return code;
	}
	push_mark_estack(es_other, image_cleanup);
	++esp;
	for ( px = 0, pp = sources; px < 4; esp++, px++, pp++ )
	  { if ( px < num_sources )
	      *esp = *pp;
	    else
	      make_null(esp);
	    make_null(esp + 4);		/* buffer */
	  }
	esp += 6;
	make_int(esp - 2, 0);		/* current plane */
	make_int(esp - 1, 0);		/* current byte in row */
	make_istruct(esp, 0, penum);
	switch ( r_type(sources) )
	  {
	  case t_file:
		if ( must_buffer )
		  { /* Allocate a buffer for each row. */
		    uint size = gs_image_bytes_per_row(penum);
		    for ( px = 0; px < num_sources; ++px )
		      { byte *sbody = ialloc_string(size, "image_setup");
			if ( sbody == 0 )
			  { esp -= inumpush;
			    image_cleanup(osp);
			    return_error(e_VMerror);
			  }
			make_string(esp - 6 + px, icurrent_space, size, sbody);
		      }
		    push_op_estack(image_file_buffered_continue);
		  }
		else
		  { push_op_estack(image_file_continue);
		  }
		break;
	  case t_string:
		pop(npop);
		return image_string_process(osp, penum, num_sources);
	  default:			/* procedure */
		push_op_estack(image_proc_continue);
		*++esp = sources[0];
		break;
	  }
	pop(npop);
	return o_push_estack;
}
/* Continuation for procedure data source. */
private int
image_proc_continue(register os_ptr op)
{	gs_image_enum *penum = r_ptr(esp, gs_image_enum);
	uint size, used;
	int code;
	int px;
	const ref *pproc;

	if ( !r_has_type_attrs(op, t_string, a_read) )
	{	check_op(1);
		/* Procedure didn't return a (readable) string.  Quit. */
		esp -= inumpush;
		image_cleanup(op);
		return_error(!r_has_type(op, t_string) ? e_typecheck : e_invalidaccess);
	}
	size = r_size(op);
	if ( size == 0 )
	  code = 1;
	else
	  code = gs_image_next(penum, op->value.bytes, size, &used);
	if ( code )
	  {	/* Stop now. */
		esp -= inumpush;
		pop(1);  op = osp;
		image_cleanup(op);
		return (code < 0 ? code : o_pop_estack);
	  }
	pop(1);
	px = (int)++(esp[-2].value.intval);
	pproc = esp - (inumpush - 2);
	if ( px == 4 || r_has_type(pproc + px, t_null) )
	  esp[-2].value.intval = px = 0;
	push_op_estack(image_proc_continue);
	*++esp = pproc[px];
	return o_push_estack;
}
/* Continue processing data from an image with file data sources */
/* and no file buffering. */
private int
image_file_continue(os_ptr op)
{	gs_image_enum *penum = r_ptr(esp, gs_image_enum);
	const ref *pproc = esp - (inumpush - 2);

	for ( ; ; )
	  {	uint size = max_uint;
		int code;
		int pn, px;
		const ref *pp;

		/*
		 * Do a first pass through the files to ensure that they all
		 * have data available in their buffers, and compute the min
		 * of the available amounts.
		 */

		for ( pn = 0, pp = pproc; pn < 4 && !r_has_type(pp, t_null);
		      ++pn, ++pp
		    )
		  {	stream *s = pp->value.pfile;
			int min_left = sbuf_min_left(s);
			uint avail;

			while ( (avail = sbufavailable(s)) <= min_left )
			  {	int next = sgetc(s);

				if ( next >= 0 )
				  { sputback(s);
				    if ( s->end_status == EOFC || s->end_status == ERRC )
				      min_left = 0;
				    continue;
				  }
				switch ( next )
				  {
				  case EOFC:
				    break;		/* with avail = 0 */
				  case INTC:
				  case CALLC:
				    return
				      s_handle_read_exception(next, pp,
						NULL, 0, image_file_continue);
				  default:
				  /* case ERRC: */
				    return_error(e_ioerror);
				  }
				break;			/* for EOFC */
			  }
			avail -= min_left;
			if ( avail < size )
			  size = avail;
		  }

		/* Now pass the min of the available buffered data to */
		/* the image processor. */

		if ( size == 0 )
		  code = 1;
		else
		  { int pi;
		    uint used;		/* only set for the last plane */

		    for ( px = 0, pp = pproc, code = 0; px < pn && !code;
			  ++px, ++pp
			)
		      code = gs_image_next(penum, sbufptr(pp->value.pfile),
					   size, &used);
		    /* Now that used has been set, update the streams. */
		    for ( pi = 0, pp = pproc; pi < px; ++pi, ++pp )
		      sbufskip(pp->value.pfile, used);
		  }
		if ( code )
		  {	esp -= inumpush;
			image_cleanup(op);
			return (code < 0 ? code : o_pop_estack);
		  }
	  }
}
/* Continue processing data from an image with file data sources */
/* and file buffering.  This is similar to the procedure case. */
private int
image_file_buffered_continue(os_ptr op)
{	gs_image_enum *penum = r_ptr(esp, gs_image_enum);
	const ref *pproc = esp - (inumpush - 2);
	int px = esp[-2].value.intval;
	int dpos = esp[-1].value.intval;
	uint size = gs_image_bytes_per_row(penum);
	int code = 0;

	while ( !code )
	  {	const ref *pp;
		uint avail = size;
		uint used;
		int pi;

		/* Accumulate data until we have a full set of planes. */
		while ( px < 4 && !r_has_type((pp = pproc + px), t_null) )
		  { const ref *pb = pp + 4;
		    uint used;
		    int status = sgets(pp->value.pfile, pb->value.bytes,
				       size - dpos, &used);

		    if ( (dpos += used) == size )
		      dpos = 0, ++px;
		    else switch ( status )
		      {
		      case EOFC:
			if ( dpos < avail )
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
		      /*case ERRC:*/
			return_error(e_ioerror);
		      }
		  }
		/* Pass the data to the image processor. */
		if ( avail == 0 )
		  { code = 1;
		    break;
		  }
		for ( pi = 0, pp = pproc + 4; pi < px && !code; ++pi, ++pp )
		  code = gs_image_next(penum, pp->value.bytes, avail, &used);
		/* Reinitialize for the next row. */
		px = dpos = 0;
	  }
	esp -= inumpush;
	image_cleanup(op);
	return (code < 0 ? code : o_pop_estack);
}
/* Process data from an image with string data sources. */
/* This never requires callbacks, so it's simpler. */
private int
image_string_process(os_ptr op, gs_image_enum *penum, int num_sources)
{	int px = 0;

	for ( ; ; )
	  {	const ref *psrc = esp - (inumpush - 2) + px;
		uint size = r_size(psrc);
		uint used;
		int code;

		if ( size == 0 )
		  code = 1;
		else
		  code = gs_image_next(penum, psrc->value.bytes, size, &used);
		if ( code )
		  {	/* Stop now. */
			esp -= inumpush;
			image_cleanup(op);
			return (code < 0 ? code : o_pop_estack);
		  }
		if ( ++px == num_sources )
		  px = 0;
	  }
}
/* Clean up after enumerating an image */
private int
image_cleanup(os_ptr op)
{	gs_image_enum *penum = r_ptr(esp + inumpush, gs_image_enum);
	const ref *pb;

	/* Free any row buffers, in LIFO order as usual. */
	for ( pb = esp + 9; pb >= esp + 6; --pb )
	  if ( r_has_type(pb, t_string) )
	    gs_free_string(imemory, pb->value.bytes, r_size(pb),
			   "image_cleanup");
	gs_image_cleanup(penum);
	ifree_object(penum, "image_cleanup");
	return 0;
}

/* ------ Non-standard operators ------ */

/* <width> <height> <data> .imagepath - */
private int
zimagepath(register os_ptr op)
{	int code;
	check_type(op[-2], t_integer);
	check_type(op[-1], t_integer);
	check_read_type(*op, t_string);
	if ( r_size(op) < ((op[-2].value.intval + 7) >> 3) * op[-1].value.intval )
		return_error(e_rangecheck);
	code = gs_imagepath(igs,
		(int)op[-2].value.intval, (int)op[-1].value.intval,
		op->value.const_bytes);
	if ( code == 0 ) pop(3);
	return code;
}

/* ------ Initialization procedure ------ */

BEGIN_OP_DEFS(zpaint_op_defs) {
	{"0eofill", zeofill},
	{"0fill", zfill},
	{"0.fillpage", zfillpage},
	{"5image", zimage},
	{"5imagemask", zimagemask},
	{"3.imagepath", zimagepath},
	{"0stroke", zstroke},
		/* Internal operators */
	{"1%image_proc_continue", image_proc_continue},
	{"0%image_file_continue", image_file_continue},
END_OP_DEFS(0) }
