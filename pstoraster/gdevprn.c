/*
  Copyright 1993-2001 by Easy Software Products.
  Copyright 1990, 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.

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

/*$Id: gdevprn.c,v 1.10 2001/01/22 15:03:55 mike Exp $ */
/* Generic printer driver support */
#include "ctype_.h"
#include "gdevprn.h"
#include "gp.h"
#include "gsparam.h"
#include "gxclio.h"
#include <stdlib.h>

/* ---------------- Standard device procedures ---------------- */

/* Define the standard printer procedure vector. */
const gx_device_procs prn_std_procs =
    prn_procs(gdev_prn_open, gdev_prn_output_page, gdev_prn_close);

/* Forward references */
int gdev_prn_maybe_reallocate_memory(P4(gx_device_printer *pdev, 
					gdev_prn_space_params *old_space,
					int old_width, int old_height));

/* ------ Open/close ------ */

/* Open a generic printer device. */
/* Specific devices may wish to extend this. */
int
gdev_prn_open(gx_device * pdev)
{
    gx_device_printer * const ppdev = (gx_device_printer *)pdev;
    int code;

    ppdev->file = NULL;
    code = gdev_prn_allocate_memory(pdev, NULL, 0, 0);
    if (code < 0)
	return code;
    if (ppdev->OpenOutputFile)
	code = gdev_prn_open_printer(pdev, 1);
    return code;
}

/* Generic closing for the printer device. */
/* Specific devices may wish to extend this. */
int
gdev_prn_close(gx_device * pdev)
{
    gx_device_printer * const ppdev = (gx_device_printer *)pdev;

    gdev_prn_free_memory(pdev);
    if (ppdev->file != NULL) {
	if (ppdev->file != stdout)
	    gp_close_printer(ppdev->file, ppdev->fname);
	ppdev->file = NULL;
    }
    return 0;
}

private int		/* returns 0 ok, else -ve error cde */
gdev_prn_setup_as_command_list(gx_device *pdev, gs_memory_t *buffer_memory,
			       byte **the_memory,
			       const gdev_prn_space_params *space_params,
			       bool bufferSpace_is_exact)
{
    gx_device_printer * const ppdev = (gx_device_printer *)pdev;
    uint space;
    int code;
    gx_device_clist *const pclist_dev = (gx_device_clist *)pdev;
    gx_device_clist_common * const pcldev = &pclist_dev->common;
    bool reallocate = *the_memory != 0;
    byte *base;

    /* Try to allocate based simply on param-requested buffer size */
    for ( space = space_params->BufferSpace; ; ) {
	base = (reallocate ?
		gs_resize_object(buffer_memory, *the_memory, space,
				 "cmd list buffer") :
		gs_alloc_bytes(buffer_memory, space,
			       "cmd list buffer"));
	if (base != 0)
	    break;
	if (bufferSpace_is_exact || (space >>= 1) < PRN_MIN_BUFFER_SPACE)
	    break;
    }
    if (base == 0)
	return_error(gs_error_VMerror);
    *the_memory = base;

    /* Try opening the command list, to see if we allocated */
    /* enough buffer space. */
open_c:
    ppdev->buf = base;
    ppdev->buffer_space = space;
    clist_init_params(pclist_dev, base, space, pdev,
		      ppdev->printer_procs.make_buffer_device,
		      space_params->band, ppdev->is_async_renderer,
		      (ppdev->bandlist_memory == 0 ? &gs_memory_default :
		       ppdev->bandlist_memory),
		      ppdev->free_up_bandlist_memory,
		      ppdev->clist_disable_mask);
    code = (*gs_clist_device_procs.open_device)( (gx_device *)pcldev );
    if (code < 0) {
	/* If there wasn't enough room, and we haven't */
	/* already shrunk the buffer, try enlarging it. */
	if ( code == gs_error_limitcheck &&
	     space >= space_params->BufferSpace &&
	     !bufferSpace_is_exact
	     ) {
	    space <<= 1;
	    if (reallocate) {
		base = gs_resize_object(buffer_memory, 
					*the_memory, space,
					"cmd list buf(retry open)");
		if (base != 0)
		    *the_memory = base;
	    } else {
		gs_free_object(buffer_memory, base,
			       "cmd list buf(retry open)");
		*the_memory = base =
		    gs_alloc_bytes(buffer_memory, space,
				   "cmd list buf(retry open)");
	    }
	    ppdev->buf = *the_memory;
	    if (base != 0)
		goto open_c;
	}
	/* Failure. */
	if (!reallocate) {
	    gs_free_object(buffer_memory, base, "cmd list buf");
	    ppdev->buffer_space = 0;
	    *the_memory = 0;
	}
    }
    return code;
}

private bool	/* ret true if device was cmd list, else false */
gdev_prn_tear_down(gx_device *pdev, byte **the_memory)
{
    gx_device_printer * const ppdev = (gx_device_printer *)pdev;
    gx_device_memory * const pmemdev = (gx_device_memory *)pdev;
    gx_device_clist *const pclist_dev = (gx_device_clist *)pdev;
    gx_device_clist_common * const pcldev = &pclist_dev->common;
    bool is_command_list;

    if (ppdev->buffer_space != 0) {
	/* Close cmd list device & point to the storage */
	(*gs_clist_device_procs.close_device)( (gx_device *)pcldev );
	*the_memory = ppdev->buf;
	ppdev->buf = 0;
	ppdev->buffer_space = 0;
	is_command_list = true;
    } else {
	/* point at the device bitmap, no need to close mem dev */
	*the_memory = pmemdev->base;
	pmemdev->base = 0;
	is_command_list = false;
    }

    /* Reset device proc vector to default */
    if (ppdev->orig_procs.open_device != 0)
	pdev->procs = ppdev->orig_procs;
    ppdev->orig_procs.open_device = 0;	/* prevent uninit'd restore of procs */

    return is_command_list;
}

private int
gdev_prn_allocate(gx_device *pdev, gdev_prn_space_params *new_space_params,
		  int new_width, int new_height, bool reallocate)
{
    gx_device_printer * const ppdev = (gx_device_printer *)pdev;
    gx_device_memory * const pmemdev = (gx_device_memory *)pdev;
    byte *the_memory = 0;
    gdev_prn_space_params save_params;
    int save_width = ppdev->width, save_height = ppdev->height;
    bool is_command_list = false;
    bool save_is_command_list = false;
    int ecode = 0;
    int pass;
    gs_memory_t *buffer_memory =
	(ppdev->buffer_memory == 0 ? &gs_memory_default :
	 ppdev->buffer_memory);

    /* If reallocate, find allocated memory & tear down buffer device */
    if (reallocate)
	save_is_command_list = gdev_prn_tear_down(pdev, &the_memory);

    /* Re/allocate memory */
    ppdev->orig_procs = pdev->procs;
    for ( pass = 1; pass <= (reallocate ? 2 : 1); ++pass ) {
	ulong mem_space;
	byte *base = 0;
	bool bufferSpace_is_default = false;
	gdev_prn_space_params space_params = ppdev->space_params;

	if (reallocate)
	    switch (pass)
	        {
		case 1:
		    /* Setup device to get reallocated */
		    save_params = ppdev->space_params;
		    ppdev->space_params = *new_space_params;
		    save_width = ppdev->width;
		    ppdev->width = new_width;
		    save_height = ppdev->height;
		    ppdev->height = new_height;
		    break;
		case 2:	/* only comes here if reallocate */
		    /* Restore device to previous contents */
		    ppdev->space_params = save_params;
		    ppdev->width = save_width;
		    ppdev->height = save_height;
		    break;
	        }

	/* Init clist/mem device-specific fields */
	memset(ppdev->skip, 0, sizeof(ppdev->skip));
	mem_space = gdev_mem_bitmap_size(pmemdev);

	/* Compute desired space params: never use the space_params as-is. */
	/* Rather, give the dev-specific driver a chance to adjust them. */
	space_params.BufferSpace = 0;
	(*ppdev->printer_procs.get_space_params)(ppdev, &space_params);
	if (ppdev->is_async_renderer && space_params.band.BandBufferSpace != 0)
	    space_params.BufferSpace = space_params.band.BandBufferSpace;
	else if (space_params.BufferSpace == 0) {
	    if (space_params.band.BandBufferSpace > 0)
	        space_params.BufferSpace = space_params.band.BandBufferSpace;
	    else {
		space_params.BufferSpace = ppdev->space_params.BufferSpace;
		bufferSpace_is_default = true;
	    }
	}

	/* Determine if we can use a full bitmap buffer, or have to use banding */
	if (pass > 1)
	    is_command_list = save_is_command_list;
	else {
	    is_command_list = space_params.banding_type == BandingAlways ||
		mem_space >= space_params.MaxBitmap ||
		mem_space != (uint)mem_space;	    /* too big to allocate */
	}
	if (!is_command_list) {
	    /* Try to allocate memory for full memory buffer */
	    base = reallocate
		? gs_resize_object( buffer_memory, the_memory,
				    (uint)mem_space, "printer buffer" )
		: gs_alloc_bytes( buffer_memory, (uint)mem_space,
				  "printer_buffer" );
	    if (base == 0)
		is_command_list = true;
	    else
		the_memory = base;
	}
	if (!is_command_list && pass == 1 && PRN_MIN_MEMORY_LEFT != 0
	    && buffer_memory == &gs_memory_default) {
	    /* before using full memory buffer, ensure enough working mem left */
	    byte * left = gs_alloc_bytes( buffer_memory,
					  PRN_MIN_MEMORY_LEFT, "printer mem left");
	    if (left == 0)
		is_command_list = true;
	    else
		gs_free_object(buffer_memory, left, "printer mem left");
	}

	if (is_command_list) {
	    /* Buffer the image in a command list. */
	    /* Release the buffer if we allocated it. */
	    int code;
	    if (!reallocate) {
		gs_free_object(buffer_memory, the_memory,
			       "printer buffer(open)");
		the_memory = 0;
	    }
	    if (space_params.banding_type == BandingNever) {
		ecode = gs_note_error(gs_error_VMerror);
		continue;
	    }
	    code = gdev_prn_setup_as_command_list(pdev, buffer_memory,
						  &the_memory, &space_params,
						  !bufferSpace_is_default);
	    if (ecode == 0)
		ecode = code;

	    if ( code >= 0 || (reallocate && pass > 1) )
		ppdev->procs = gs_clist_device_procs;
	} else {
	    /* Render entirely in memory. */
	    int code;

	    ppdev->buffer_space = 0;
	    code = (*ppdev->printer_procs.make_buffer_device)
		(pmemdev, pdev, buffer_memory, false);
	    if (code < 0) {	/* Catastrophic. Shouldn't ever happen */
		gs_free_object(buffer_memory, base, "printer buffer");
		pdev->procs = ppdev->orig_procs;
		ppdev->orig_procs.open_device = 0;	/* prevent uninit'd restore of procs */
		return_error(code);
	    }
	    pmemdev->base = base;
	}
	if (ecode == 0)
	    break;
    }

    if (ecode >= 0 || reallocate) {	/* even if realloc failed */
	/* Synthesize the procedure vector. */
	/* Rendering operations come from the memory or clist device, */
	/* non-rendering come from the printer device. */
#define COPY_PROC(p) set_dev_proc(ppdev, p, ppdev->orig_procs.p)
	COPY_PROC(get_initial_matrix);
	COPY_PROC(output_page);
	COPY_PROC(close_device);
	COPY_PROC(map_rgb_color);
	COPY_PROC(map_color_rgb);
	COPY_PROC(get_params);
	COPY_PROC(put_params);
	COPY_PROC(map_cmyk_color);
	COPY_PROC(get_xfont_procs);
	COPY_PROC(get_xfont_device);
	COPY_PROC(map_rgb_alpha_color);
	/* All printers are page devices, even if they didn't use the */
	/* standard macros for generating their procedure vectors. */
	set_dev_proc(ppdev, get_page_device, gx_page_device_get_page_device);
	COPY_PROC(get_alpha_bits);
	COPY_PROC(get_clipping_box);
	COPY_PROC(map_color_rgb_alpha);
	COPY_PROC(get_hardware_params);
#undef COPY_PROC
	/* If using a command list, already opened the device. */
	if (is_command_list)
	    return ecode;
	else
	    return (*dev_proc(pdev, open_device))(pdev);
    } else {
	pdev->procs = ppdev->orig_procs;
	ppdev->orig_procs.open_device = 0;	/* prevent uninit'd restore of procs */
	return ecode;
    }
}

int
gdev_prn_allocate_memory(gx_device *pdev,
			 gdev_prn_space_params *new_space_params,
			 int new_width, int new_height)
{
    return gdev_prn_allocate(pdev, new_space_params,
			     new_width, new_height, false);
}

int
gdev_prn_reallocate_memory(gx_device *pdev,
			 gdev_prn_space_params *new_space_params,
			 int new_width, int new_height)
{
    return gdev_prn_allocate(pdev, new_space_params,
			     new_width, new_height, true);
}

int
gdev_prn_free_memory(gx_device *pdev)
{
    gx_device_printer * const ppdev = (gx_device_printer *)pdev;
    byte *the_memory = 0;
    gs_memory_t *buffer_memory =
	(ppdev->buffer_memory == 0 ? &gs_memory_default :
	 ppdev->buffer_memory);

    gdev_prn_tear_down(pdev, &the_memory);
    gs_free_object(buffer_memory, the_memory, "gdev_prn_free_memory");
    return 0;
}

/* ------------- Stubs related only to async rendering ------- */

int	/* rets 0 ok, -ve error if couldn't start thread */
gx_default_start_render_thread(gdev_prn_start_render_params *params)
{
    return gs_error_unknownerror;
}

/* Open the renderer's copy of a device. */
/* This is overriden in gdevprna.c */
int
gx_default_open_render_device(gx_device_printer *pdev)
{
    return gs_error_unknownerror;
}

/* Close the renderer's copy of a device. */
int
gx_default_close_render_device(gx_device_printer *pdev)
{
    return gdev_prn_close( (gx_device *)pdev );
}

/* ------ Get/put parameters ------ */

/* Get parameters.  Printer devices add several more parameters */
/* to the default set. */
int
gdev_prn_get_params(gx_device * pdev, gs_param_list * plist)
{
    gx_device_printer * const ppdev = (gx_device_printer *)pdev;
    int code = gx_default_get_params(pdev, plist);

    if (code < 0 ||
	(ppdev->Duplex_set >= 0 &&
	 (code = (ppdev->Duplex_set ?
		  param_write_bool(plist, "Duplex", &ppdev->Duplex) :
		  param_write_null(plist, "Duplex"))) < 0)
	)
	return code;

    return 0;
}

/* Put parameters. */
int
gdev_prn_put_params(gx_device * pdev, gs_param_list * plist)
{
    gx_device_printer * const ppdev = (gx_device_printer *)pdev;
    int ecode = 0;
    int code;
    const char *param_name;
    bool is_open = pdev->is_open;
    bool oof = ppdev->OpenOutputFile;
    bool rpp = ppdev->ReopenPerPage;
    bool duplex;
    int duplex_set = -1;
    int width = pdev->width;
    int height = pdev->height;
    gdev_prn_space_params sp, save_sp;
    gs_param_dict mdict;

    sp = ppdev->space_params;
    save_sp = sp;

    switch (code = param_read_bool(plist, (param_name = "OpenOutputFile"), &oof)) {
	default:
	    ecode = code;
	    param_signal_error(plist, param_name, ecode);
	case 0:
	case 1:
	    break;
    }

    switch (code = param_read_bool(plist, (param_name = "ReopenPerPage"), &rpp)) {
	default:
	    ecode = code;
	    param_signal_error(plist, param_name, ecode);
	case 0:
	case 1:
	    break;
    }

    if (ppdev->Duplex_set >= 0)	/* i.e., Duplex is supported */
	switch (code = param_read_bool(plist, (param_name = "Duplex"),
				       &duplex)) {
	    case 0:
		duplex_set = 1;
		break;
	    default:
		if ((code = param_read_null(plist, param_name)) == 0) {
		    duplex_set = 0;
		    break;
		}
		ecode = code;
		param_signal_error(plist, param_name, ecode);
	    case 1:
		;
	}
#define CHECK_PARAM_CASES(member, bad, label)\
    case 0:\
	if ((sp.params_are_read_only ? sp.member != save_sp.member : bad))\
	    ecode = gs_error_rangecheck;\
	else\
	    break;\
	goto label;\
    default:\
	ecode = code;\
label:\
	param_signal_error(plist, param_name, ecode);\
    case 1:\
	break

    /* Read InputAttributes and OutputAttributes just for the type */
    /* check and to indicate that they aren't undefined. */
#define read_media(pname)\
	switch ( code = param_begin_read_dict(plist, (param_name = pname), &mdict, true) )\
	  {\
	  case 0:\
		param_end_read_dict(plist, pname, &mdict);\
		break;\
	  default:\
		ecode = code;\
		param_signal_error(plist, param_name, ecode);\
	  case 1:\
		;\
	  }

    read_media("InputAttributes");
    read_media("OutputAttributes");

    if (ecode < 0)
	return ecode;
    /* Prevent gx_default_put_params from closing the printer. */
    pdev->is_open = false;
    code = gx_default_put_params(pdev, plist);
    pdev->is_open = is_open;
    if (code < 0)
	return code;

    ppdev->OpenOutputFile = oof;
    ppdev->ReopenPerPage = rpp;
    if (duplex_set >= 0) {
	ppdev->Duplex = duplex;
	ppdev->Duplex_set = duplex_set;
    }
    ppdev->space_params = sp;

    /* If necessary, free and reallocate the printer memory. */
    /* Formerly, would not reallocate if device is not open: */
    /* we had to patch this out (see News for 5.50). */
    code = gdev_prn_maybe_reallocate_memory(ppdev, &save_sp, width, height);
    if (code < 0)
	return code;

    return 0;
}

/* ------ Others ------ */

#define TILE_SIZE	256

/* Default routine to override current space_params. */
void
gdev_prn_default_get_space_params(const gx_device_printer *printer_dev,
				  gdev_prn_space_params *space_params)
{
  int	cache_size;			/* Size of tile cache in bytes */
  char	*cache_env,			/* Cache size environment variable */
	cache_units[255];		/* Cache size units */


  if ((cache_env = getenv("RIP_MAX_CACHE")) != NULL)
  {
    switch (sscanf(cache_env, "%d%254s", &cache_size, cache_units))
    {
      case 0 :
          cache_size = 32 * 1024 * 1024;
	  break;
      case 1 :
          cache_size *= 4 * TILE_SIZE * TILE_SIZE;
	  break;
      case 2 :
          if (tolower(cache_units[0]) == 'g')
	    cache_size *= 1024 * 1024 * 1024;
          else if (tolower(cache_units[0]) == 'm')
	    cache_size *= 1024 * 1024;
	  else if (tolower(cache_units[0]) == 'k')
	    cache_size *= 1024;
	  else if (tolower(cache_units[0]) == 't')
	    cache_size *= 4 * TILE_SIZE * TILE_SIZE;
	  break;
    }
  }
  else
    cache_size = 32 * 1024 * 1024;

  space_params->MaxBitmap = cache_size;
}

/* Generic routine to send the page to the printer. */
int	/* 0 ok, -ve error, or 1 if successfully upgraded to buffer_page */
gdev_prn_output_page(gx_device * pdev, int num_copies, int flush)
{
    gx_device_printer * const ppdev = (gx_device_printer *)pdev;
    int outcode = 0, closecode = 0, errcode = 0, endcode;
    bool upgraded_copypage = false;

    if (num_copies > 0 || !flush) {
	int code = gdev_prn_open_printer(pdev, 1);

	if (code < 0)
	    return code;

	/* If copypage request, try to do it using buffer_page */
	if ( !flush &&
	     (*ppdev->printer_procs.buffer_page)
	     (ppdev, ppdev->file, num_copies) >= 0
	     ) {
	    upgraded_copypage = true;
	    flush = true;
	}
	else if (num_copies > 0)
	    /* Print the accumulated page description. */
	    outcode =
		(*ppdev->printer_procs.print_page_copies)(ppdev, ppdev->file,
							  num_copies);
	if (ppdev->file)
	{
	  fflush(ppdev->file);
	  errcode =
	      (ferror(ppdev->file) ? gs_note_error(gs_error_ioerror) : 0);
        }
	else
	  errcode = 0;

	if (!upgraded_copypage)
	    closecode = gdev_prn_close_printer(pdev);
    }
    endcode = (ppdev->buffer_space ? clist_finish_page(pdev, flush) : 0);

    if (outcode < 0)
	return outcode;
    if (errcode < 0)
	return errcode;
    if (closecode < 0)
	return closecode;
    if (endcode < 0)
	return endcode;
    return (upgraded_copypage ? 1 : 0);
}

/* Print multiple copies of a page by calling print_page multiple times. */
int
gx_default_print_page_copies(gx_device_printer * pdev, FILE * prn_stream,
			     int num_copies)
{
    int i = num_copies;
    int code = 0;

    while (code >= 0 && i-- > 0)
	code = (*pdev->printer_procs.print_page) (pdev, prn_stream);
    return code;
}

/*
 * Buffer a (partial) rasterized page & optionally print result multiple times.
 * The default implementation returns error, since the driver needs to override
 * this (in procedure vector) in configurations where this call may occur.
 */
int
gx_default_buffer_page(gx_device_printer *pdev, FILE *prn_stream,
		       int num_copies)
{
    return gs_error_unknownerror;
}

/* ---------------- Driver services ---------------- */

/* Return the number of scan lines that should actually be passed */
/* to the device. */
int
gdev_prn_print_scan_lines(gx_device * pdev)
{
    int height = pdev->height;
    gs_matrix imat;
    float yscale;
    int top, bottom, offset, end;

    (*dev_proc(pdev, get_initial_matrix)) (pdev, &imat);
    yscale = imat.yy * 72.0;	/* Y dpi, may be negative */
    top = (int)(dev_t_margin(pdev) * yscale);
    bottom = (int)(dev_b_margin(pdev) * yscale);
    offset = (int)(dev_y_offset(pdev) * yscale);
    if (yscale < 0) {		/* Y=0 is top of page */
	end = -offset + height + bottom;
    } else {			/* Y=0 is bottom of page */
	end = offset + height - top;
    }
    return min(height, end);
}

/* Open the current page for printing. */
int
gdev_prn_open_printer_positionable(gx_device *pdev, bool binary_mode,
				   bool positionable)
{
    gx_device_printer * const ppdev = (gx_device_printer *)pdev;

    if (ppdev->file != 0) {
	ppdev->file_is_new = false;
	return 0;
    }
    {
	int code = gx_device_open_output_file(pdev, ppdev->fname,
					      binary_mode, positionable,
					      &ppdev->file);
	if (code < 0)
	    return code;
    }
    ppdev->file_is_new = true;
    return 0;
}
int
gdev_prn_open_printer(gx_device *pdev, bool binary_mode)
{
    return gdev_prn_open_printer_positionable(pdev, binary_mode, false);
}

/* Copy a scan line from the buffer to the printer. */
int
gdev_prn_get_bits(gx_device_printer * pdev, int y, byte * str, byte ** actual_data)
{
    int code = (*dev_proc(pdev, get_bits)) ((gx_device *) pdev, y, str, actual_data);
    uint line_size = gdev_prn_raster(pdev);
    int last_bits = -(pdev->width * pdev->color_info.depth) & 7;

    if (code < 0)
	return code;
    if (last_bits != 0) {
	byte *dest = (actual_data != 0 ? *actual_data : str);

	dest[line_size - 1] &= 0xff << last_bits;
    }
    return 0;
}
/* Copy scan lines to a buffer.  Return the number of scan lines, */
/* or <0 if error. */
int
gdev_prn_copy_scan_lines(gx_device_printer * pdev, int y, byte * str, uint size)
{
    uint line_size = gdev_prn_raster(pdev);
    int count = size / line_size;
    int i;
    byte *dest = str;

    count = min(count, pdev->height - y);
    for (i = 0; i < count; i++, dest += line_size) {
	int code = gdev_prn_get_bits(pdev, y + i, dest, NULL);

	if (code < 0)
	    return code;
    }
    return count;
}

/* Like get_bits, but accepts initial raster contents */
int
gdev_prn_get_overlay_bits(gx_device_printer *pdev, int y, int lineCount,
			  byte *data)
{
    if (pdev->buffer_space) {
	/* Command lists have built-in support for this function */
	return clist_get_overlay_bits(pdev, y, lineCount, data);
    } else {
	/* Memory devices cannot support this function. */
	return_error(gs_error_unknownerror);
    }
}

/* Find out where the band buffer for a given line is going to fall on the */
/* next call to get_bits. */
int	/* rets # lines from y till end of buffer, or -ve error code */
gdev_prn_locate_overlay_buffer(gx_device_printer *pdev, int y, byte **data)
{
    gx_device_printer * const ppdev = (gx_device_printer *)pdev;

    if (ppdev->buffer_space) {
	  /* Command lists have built-in support for this function */
	  return clist_locate_overlay_buffer(pdev, y, data);
    } else {
	/* Memory devices cannot support this function. */
	return_error(gs_error_unknownerror);
    }
}

/* Close the current page. */
int
gdev_prn_close_printer(gx_device * pdev)
{
    gx_device_printer * const ppdev = (gx_device_printer *)pdev;

    if (strchr(ppdev->fname, '%') /* file per page */ ||
	ppdev->ReopenPerPage	/* close and reopen for each page */
	) {
	gp_close_printer(ppdev->file, ppdev->fname);
	ppdev->file = NULL;
    }
    return 0;
}

/* If necessary, free and reallocate the printer memory after changing params */
int
gdev_prn_maybe_reallocate_memory(gx_device_printer *prdev, 
				 gdev_prn_space_params *old_sp,
				 int old_width, int old_height)
{
    int code = 0;
    gx_device *const pdev = (gx_device *)prdev;
    gx_device_memory * const mdev = (gx_device_memory *)prdev;
	
    /* The first test here used to be prdev->open.  See News for 5.50. */
    if (mdev->base != 0 &&
	(memcmp(&prdev->space_params, old_sp, sizeof(*old_sp)) != 0 ||
	 prdev->width != old_width || prdev->height != old_height )
	) {
	int new_width = prdev->width;
	int new_height = prdev->height;
	gdev_prn_space_params new_sp = prdev->space_params;

	prdev->width = old_width;
	prdev->height = old_height;
	prdev->space_params = *old_sp;
	code = gdev_prn_reallocate_memory(pdev, &new_sp,
					  new_width, new_height);
	/* If this fails, device should be usable w/old params, but */
	/* band files may not be open. */
    }
    return code;
}
