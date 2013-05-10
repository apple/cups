/*
  Copyright 1993-1999 by Easy Software Products.
  Copyright (C) 1990, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gdevprn.c */
/* Generic printer driver support */
#include "ctype_.h"
#include "gdevprn.h"
#include "gp.h"
#include "gsparam.h"
#include "gxclio.h"
#include <stdlib.h>

/* ---------------- Standard device procedures ---------------- */

/* Macros for casting the pdev argument */
#define ppdev ((gx_device_printer *)pdev)
#define pmemdev ((gx_device_memory *)pdev)
#define pcldev (&((gx_device_clist *)pdev)->common)

/* Define the standard printer procedure vector. */
gx_device_procs prn_std_procs =
  prn_procs(gdev_prn_open, gdev_prn_output_page, gdev_prn_close);

/* ------ Open/close ------ */

/* Forward references */
private int gdev_prn_alloc(P1(gx_device *));
private int gdev_prn_free(P1(gx_device *));

/* Open a generic printer device. */
/* Specific devices may wish to extend this. */
int
gdev_prn_open(gx_device *pdev)
{	int code;
	ppdev->file = NULL;
	code = gdev_prn_alloc(pdev);
	if ( code < 0 )
	  return code;
	if ( ppdev->OpenOutputFile )
	  code = gdev_prn_open_printer(pdev, 1);
	return code;
}

/* Allocate buffer space and initialize the device. */
/* We break this out as a separate procedure so that resetting */
/* the page dimensions doesn't have to close and reopen the device. */
private int
gdev_prn_alloc(gx_device *pdev)
{	ulong mem_space;
	byte *base = 0;
	void *left = 0;
  int	cache_size;			/* Size of tile cache in bytes */
  char	*cache_env,			/* Cache size environment variable */
	cache_units[255];		/* Cache size units */


	memset(ppdev->skip, 0, sizeof(ppdev->skip));
	ppdev->orig_procs = pdev->std_procs;
	ppdev->ccfile = ppdev->cbfile = NULL;

       /*
	* MRS - reset max_bitmap to the value of RIP_MAX_CACHE as necessary...
	*/

	if ((cache_env = getenv("RIP_MAX_CACHE")) != NULL)
	{
	  switch (sscanf(cache_env, "%d%s", &cache_size, cache_units))
	  {
	    case 0 :
        	cache_size = 32 * 1024 * 1024;
		break;
	    case 1 :
                cache_size *= 4 * 256 * 256;
		break;
	    case 2 :
        	if (tolower(cache_units[0]) == 'g')		/* Gigabytes */
		  cache_size *= 1024 * 1024 * 1024;
        	else if (tolower(cache_units[0]) == 'm')	/* Megabytes */
		  cache_size *= 1024 * 1024;
		else if (tolower(cache_units[0]) == 'k')	/* Kilobytes */
		  cache_size *= 1024;
		else if (tolower(cache_units[0]) == 't')	/* Tiles */
		  cache_size *= 4 * 256 * 256;
		break;
	  }
	}
	else
          cache_size = 32 * 1024 * 1024;

        ppdev->max_bitmap       = cache_size;
	ppdev->use_buffer_space = cache_size / 8;

	mem_space = gdev_mem_bitmap_size(pmemdev);

	if ( mem_space >= ppdev->max_bitmap ||
	     mem_space != (uint)mem_space ||	/* too big to allocate */
	     (base = (byte *)gs_malloc((uint)mem_space, 1, "printer buffer")) == 0 ||	/* can't allocate */
	     (PRN_MIN_MEMORY_LEFT != 0 &&
	      (left = gs_malloc(PRN_MIN_MEMORY_LEFT, 1, "printer memory left")) == 0)	/* not enough left */
	   )
	{	/* Buffer the image in a command list. */
		uint space;
		int code, bcode = 0, ccode = 0;

		/* Release the buffer if we allocated it. */
		gs_free(base, (uint)mem_space, 1, "printer buffer(open)");
		for ( space = ppdev->use_buffer_space; ; )
		{	base = (byte *)gs_malloc(space, 1,
						 "command list buffer");
			if ( base != 0 ) break;
			if ( (space >>= 1) < PRN_MIN_BUFFER_SPACE )
			  return_error(gs_error_VMerror);	/* no hope */
		}
open_c:		pcldev->data = base;
		pcldev->data_size = space;
		pcldev->target = pdev;
		pcldev->make_buffer_device =
		  ppdev->printer_procs.make_buffer_device;
		ppdev->buf = base;
		ppdev->buffer_space = space;

		/* Try opening the command list, to see if we allocated */
		/* enough buffer space. */
		code = (*gs_clist_device_procs.open_device)((gx_device *)pcldev);
		if ( code < 0 )
		{	/* If there wasn't enough room, and we haven't */
			/* already shrunk the buffer, try enlarging it. */
			if ( code == gs_error_limitcheck &&
			     space >= ppdev->use_buffer_space
			   )
			{	gs_free(base, space, 1,
					"command list buffer(retry open)");
				space <<= 1;
				base = (byte *)gs_malloc(space, 1,
					 "command list buffer(retry open)");
				ppdev->buf = base;
				if ( base != 0 )
				  goto open_c;
			}
			/* Fall through with code < 0 */
		}
		if ( code < 0 ||
		     (ccode = clist_open_scratch(ppdev->ccfname, &ppdev->ccfile, &gs_memory_default, true)) < 0 ||
		     (bcode = clist_open_scratch(ppdev->cbfname, &ppdev->cbfile, &gs_memory_default, false)) < 0
		   )
		{	/* Clean up before exiting */
			eprintf3("Problem opening clist: code=%d, bcode=%d, ccode=%d\n",
				 code, bcode, ccode);
			gdev_prn_free(pdev);
			return (code < 0 ? code : ccode < 0 ? ccode : bcode);
		}
		pcldev->cfile = ppdev->ccfile;
		pcldev->bfile = ppdev->cbfile;
		pcldev->bfile_end_pos = 0;
		ppdev->std_procs = gs_clist_device_procs;
	}
	else
	{	/* Render entirely in memory. */
		int code;
		/* Release the leftover memory. */
		gs_free(left, PRN_MIN_MEMORY_LEFT, 1,
			"printer memory left");
		ppdev->buffer_space = 0;
		code = (*ppdev->printer_procs.make_buffer_device)
		  (pmemdev, pdev, pdev->memory, false);
		if ( code < 0 )
		  { gs_free(base, space, 1, "command list buffer");
		    return code;
		  }
		pmemdev->base = base;
	}

	/* Synthesize the procedure vector. */
	/* Rendering operations come from the memory or clist device, */
	/* non-rendering come from the printer device. */
#define copy_proc(p) set_dev_proc(ppdev, p, ppdev->orig_procs.p)
	copy_proc(get_initial_matrix);
	copy_proc(output_page);
	copy_proc(close_device);
	copy_proc(map_rgb_color);
	copy_proc(map_color_rgb);
	copy_proc(get_params);
	copy_proc(put_params);
	copy_proc(map_cmyk_color);
	copy_proc(get_xfont_procs);
	copy_proc(get_xfont_device);
	copy_proc(map_rgb_alpha_color);
	/* All printers are page devices, even if they didn't use the */
	/* standard macros for generating their procedure vectors. */
	set_dev_proc(ppdev, get_page_device, gx_page_device_get_page_device);
	copy_proc(get_alpha_bits);
#undef copy_proc
	return (*dev_proc(pdev, open_device))(pdev);
}

/* Generic closing for the printer device. */
/* Specific devices may wish to extend this. */
int
gdev_prn_close(gx_device *pdev)
{	gdev_prn_free(pdev);
	if ( ppdev->file != NULL )
	{	if ( ppdev->file != stdout )
		  gp_close_printer(ppdev->file, ppdev->fname);
		ppdev->file = NULL;
	}
	return 0;
}

/* Free buffer space and related objects, the inverse of alloc. */
/* Again, we break this out for resetting page dimensions. */
private int
gdev_prn_free(gx_device *pdev)
{	if ( ppdev->ccfile != NULL )
	{	clist_fclose_and_unlink(ppdev->ccfile, ppdev->ccfname);
		ppdev->ccfile = NULL;
	}
	if ( ppdev->cbfile != NULL )
	{	clist_fclose_and_unlink(ppdev->cbfile, ppdev->cbfname);
		ppdev->cbfile = NULL;
	}
	if ( ppdev->buffer_space != 0 )
	{	/* Free the buffer */
		gs_free(ppdev->buf, (uint)ppdev->buffer_space, 1,
			"command list buffer(close)");
		ppdev->buf = 0;
		ppdev->buffer_space = 0;
	}
	else
	{	/* Free the memory device bitmap */
		gs_free(pmemdev->base, (uint)gdev_mem_bitmap_size(pmemdev), 1,
			"printer buffer(close)");
		pmemdev->base = 0;
	}
	pdev->std_procs = ppdev->orig_procs;
	return 0;
}

/* ------ Get/put parameters ------ */

/* Get parameters.  Printer devices add several more parameters */
/* to the default set. */
int
gdev_prn_get_params(gx_device *pdev, gs_param_list *plist)
{	int code = gx_default_get_params(pdev, plist);
	gs_param_string ofns;

	if ( code < 0 ) return code;

	code = (ppdev->NumCopies_set ?
		param_write_int(plist, "NumCopies", &ppdev->NumCopies) :
		param_write_null(plist, "NumCopies"));
	if ( code < 0 ) return code;

	code = param_write_bool(plist, "OpenOutputFile", &ppdev->OpenOutputFile);
	if ( code < 0 ) return code;

	if ( ppdev->Duplex_set >= 0 )
	  { code =
	      (ppdev->Duplex_set ? param_write_bool(plist, "Duplex", &ppdev->Duplex) :
	       param_write_null(plist, "Duplex"));
	    if ( code < 0 ) return code;
	  }

	code = param_write_long(plist, "BufferSpace", &ppdev->use_buffer_space);
	if ( code < 0 ) return code;

	code = param_write_long(plist, "MaxBitmap", &ppdev->max_bitmap);
	if ( code < 0 ) return code;

	ofns.data = (const byte *)ppdev->fname,
	  ofns.size = strlen(ppdev->fname),
	  ofns.persistent = false;
	return param_write_string(plist, "OutputFile", &ofns);
}

/* Put parameters. */
int
gdev_prn_put_params(gx_device *pdev, gs_param_list *plist)
{	int ecode = 0;
	int code;
	const char _ds *param_name;
	bool is_open = pdev->is_open;
	int nci = ppdev->NumCopies;
	bool ncset = ppdev->NumCopies_set;
	bool oof = ppdev->OpenOutputFile;
	bool duplex;
	  int duplex_set = -1;
	int width = pdev->width;
	int height = pdev->height;
	long buffer_space = ppdev->use_buffer_space;
	long max_bitmap = ppdev->max_bitmap;
	long bsl = buffer_space;
	long mbl = max_bitmap;
	gs_param_string ofs;
	gs_param_dict mdict;

	switch ( code = param_read_int(plist, (param_name = "NumCopies"), &nci) )
	{
	case 0:
		if ( nci < 0 )
		  ecode = gs_error_rangecheck;
		else
		  { ncset = true;
		    break;
		  }
		goto nce;
	default:
		if ( (code = param_read_null(plist, param_name)) == 0 )
		  {	ncset = false;
			break;
		  }
		ecode = code;	/* can't be 1 */
nce:		param_signal_error(plist, param_name, ecode);
	case 1:
		break;
	}

	switch ( code = param_read_bool(plist, (param_name = "OpenOutputFile"), &oof) )
	{
	default:
		ecode = code;
		param_signal_error(plist, param_name, ecode);
	case 0:
	case 1:
		break;
	}

	if ( ppdev->Duplex_set >= 0 )	/* i.e., Duplex is supported */
	  switch ( code = param_read_bool(plist, (param_name = "Duplex"),
					  &duplex) )
	    {
	    case 0:
		duplex_set = 1;
		break;
	    default:
		if ( (code = param_read_null(plist, param_name)) == 0 )
		{	duplex_set = 0;
			break;
		}
		ecode = code;
		param_signal_error(plist, param_name, ecode);
	    case 1:
		;
	    }

	switch ( code = param_read_long(plist, (param_name = "BufferSpace"), &bsl) )
	{
	case 0:
		if ( bsl < 10000 )
		  ecode = gs_error_rangecheck;
		else
		  break;
		goto bse;
	default:
		ecode = code;
bse:		param_signal_error(plist, param_name, ecode);
	case 1:
		break;
	}

	switch ( code = param_read_long(plist, (param_name = "MaxBitmap"), &mbl) )
	{
	case 0:
		if ( mbl < 10000 )
		  ecode = gs_error_rangecheck;
		else
		  break;
		goto mbe;
	default:
		ecode = code;
mbe:		param_signal_error(plist, param_name, ecode);
	case 1:
		break;
	}

	switch ( code = param_read_string(plist, (param_name = "OutputFile"), &ofs) )
	{
	case 0:
		if ( ofs.size >= prn_fname_sizeof )
		  ecode = gs_error_limitcheck;
		else
		  {	/* Check the validity of any % formats. */
			uint i;
			bool pagenum = false;
			for ( i = 0; i < ofs.size; ++i )
			  if ( ofs.data[i] == '%' )
			    { if ( i+1 < ofs.size && ofs.data[i+1] == '%' )
				continue;
			      if ( pagenum )		/* more than one %, */
				i = ofs.size - 1;	/* force error */
			      pagenum = true;
sw:			      if ( ++i == ofs.size )
				{ ecode = gs_error_rangecheck;
				  goto ofe;
				}
			      switch ( ofs.data[i] )
				{
				case ' ': case '#': case '+': case '-':
				case '0': case '1': case '2': case '3':
				case '4': case '5': case '6': case '7':
				case '8': case '9': case 'l':
				  goto sw;
				case 'd': case 'i': case 'u': case 'o':
				case 'x': case 'X':
				  continue;
				default:
				  ecode = gs_error_rangecheck;
				  goto ofe;
				}
			    }
			break;
		  }
		goto ofe;
	default:
		ecode = code;
ofe:		param_signal_error(plist, param_name, ecode);
	case 1:
		ofs.data = 0;
		break;
	}

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

	if ( ecode < 0 )
	  return ecode;
	/* Prevent gx_default_put_params from closing the printer. */
	pdev->is_open = false;
	code = gx_default_put_params(pdev, plist);
	pdev->is_open = is_open;
	if ( code < 0 )
	  return code;

	ppdev->NumCopies = nci;
	ppdev->NumCopies_set = ncset;
	ppdev->OpenOutputFile = oof;
	if ( duplex_set >= 0 )
	  { ppdev->Duplex = duplex;
	    ppdev->Duplex_set = duplex_set;
	  }
	/* If necessary, free and reallocate the printer memory. */
	if ( bsl != buffer_space || mbl != max_bitmap ||
	     pdev->width != width || pdev->height != height
	   )
	  {	if ( is_open )
		  gdev_prn_free(pdev);
		ppdev->use_buffer_space = bsl;
		ppdev->max_bitmap = mbl;
		if ( is_open )
		  {	ecode = code = gdev_prn_alloc(pdev);
			if ( code < 0 )
			  {	/* Try to put things back as they were. */
				ppdev->use_buffer_space = buffer_space;
				ppdev->max_bitmap = max_bitmap;
				gx_device_set_width_height(pdev,
							   width, height);
				code = gdev_prn_alloc(pdev);
				if ( code < 0 )
				  {	/* Disaster!  We can't get back. */
					pdev->is_open = false;
					return code;
				  }
				return ecode;
			  }
		  }
	  }
	if ( ofs.data != 0 &&
	     bytes_compare(ofs.data, ofs.size,
			   (const byte *)ppdev->fname, strlen(ppdev->fname))
	   )
	  {	/* Close the file if it's open. */
		if ( ppdev->file != NULL && ppdev->file != stdout )
		  gp_close_printer(ppdev->file, ppdev->fname);
		ppdev->file = NULL;
		memcpy(ppdev->fname, ofs.data, ofs.size);
		ppdev->fname[ofs.size] = 0;
	  }

	/* If the device is open and OpenOutputFile is true, */
	/* open the OutputFile now.  (If the device isn't open, */
	/* this will happen when it is opened.) */
	if ( pdev->is_open && oof )
	  {	code = gdev_prn_open_printer(pdev, 1);
		if ( code < 0 )
		  return code;
	  }

	return 0;
}

/* Put InputAttributes and OutputAttributes. */
int
gdev_prn_input_page_size(int index, gs_param_dict *pdict,
  floatp width_points, floatp height_points)
{	input_media media;
	media.PageSize[0] = width_points;
	media.PageSize[1] = height_points;
	media.MediaColor = 0;
	media.MediaWeight = 0;
	media.MediaType = 0;
	return gdev_prn_input_media(index, pdict, &media);
}
private int
finish_media(gs_param_list *mlist, gs_param_name key, const char *media_type)
{	int code = 0;
	if ( media_type != 0 )
	  {	gs_param_string as;
		param_string_from_string(as, media_type);
		code = param_write_string(mlist, key, &as);
	  }
	return code;
}
int
gdev_prn_input_media(int index, gs_param_dict *pdict, const input_media *pim)
{	char key[25];
	gs_param_dict mdict;
	int code;
	gs_param_string as;

	sprintf(key, "%d", index);
	mdict.size = 4;
	code = param_begin_write_dict(pdict->list, key, &mdict, false);
	if ( code < 0 )
	  return code;
	if ( pim->PageSize[0] != 0 && pim->PageSize[1] != 0 )
	  {	gs_param_float_array psa;
		psa.data = pim->PageSize, psa.size = 2, psa.persistent = false;
		code = param_write_float_array(mdict.list, "PageSize",
					       &psa);
		if ( code < 0 )
		  return code;
	  }
	if ( pim->MediaColor != 0 )
	  {	param_string_from_string(as, pim->MediaColor);
		code = param_write_string(mdict.list, "MediaColor",
					  &as);
		if ( code < 0 )
		  return code;
	  }
	if ( pim->MediaWeight != 0 )
	  {	/* We do the following silly thing in order to avoid */
		/* having to work around the 'const' in the arg list. */
		float weight = pim->MediaWeight;
		code = param_write_float(mdict.list, "MediaWeight",
					 &weight);
		if ( code < 0 )
		  return code;
	  }
	code = finish_media(mdict.list, "MediaType", pim->MediaType);
	if ( code < 0 )
	  return code;
	return param_end_write_dict(pdict->list, key, &mdict);
}
int
gdev_prn_output_media(int index, gs_param_dict *pdict, const output_media *pom)
{	char key[25];
	gs_param_dict mdict;
	int code;

	sprintf(key, "%d", index);
	mdict.size = 4;
	code = param_begin_write_dict(pdict->list, key, &mdict, false);
	if ( code < 0 )
	  return code;
	code = finish_media(mdict.list, "OutputType", pom->OutputType);
	if ( code < 0 )
	  return code;
	return param_end_write_dict(pdict->list, key, &mdict);
}

/* ------ Others ------ */

/* Generic routine to send the page to the printer. */
int
gdev_prn_output_page(gx_device *pdev, int num_copies, int flush)
{	int code, outcode, closecode, errcode;

	if ( num_copies > 0 )
	  {	code = gdev_prn_open_printer(pdev, 1);
		if ( code < 0 )
		  return code;

		/* Print the accumulated page description. */
		outcode =
		  (*ppdev->printer_procs.print_page_copies)(ppdev,
							    ppdev->file,
							    num_copies);
                if (ppdev->file != NULL)
		  errcode = (ferror(ppdev->file) ? gs_error_ioerror : 0);
		else
		  errcode = 0;

		closecode = gdev_prn_close_printer(pdev);
	  }
	else
	  code = outcode = closecode = errcode = 0;

	if ( ppdev->buffer_space ) /* reinitialize clist for writing */
	  code = (*gs_clist_device_procs.output_page)(pdev, num_copies, flush);

	if ( outcode < 0 )
	  return outcode;
	if ( errcode < 0 )
	  return_error(errcode);
	if ( closecode < 0 )
	  return closecode;
	return code;
}

/* Print multiple copies of a page by calling print_page multiple times. */
int
gx_default_print_page_copies(gx_device_printer *pdev, FILE *prn_stream,
  int num_copies)
{	int i = num_copies;
	int code = 0;
	while ( code >= 0 && i-- > 0 )
	  code = (*pdev->printer_procs.print_page)(pdev, prn_stream);
	return code;
}

/* ---------------- Driver services ---------------- */

/* Return the number of scan lines that should actually be passed */
/* to the device. */
int
gdev_prn_print_scan_lines(gx_device *pdev)
{	int height = pdev->height;
	gs_matrix imat;
	float yscale;
	int top, bottom, offset, end;

	(*dev_proc(pdev, get_initial_matrix))(pdev, &imat);
	yscale = imat.yy * 72.0;		/* Y dpi, may be negative */
	top = (int)(dev_t_margin(pdev) * yscale);
	bottom = (int)(dev_b_margin(pdev) * yscale);
	offset = (int)(dev_y_offset(pdev) * yscale);
	if ( yscale < 0 )
	  {	/* Y=0 is top of page */
		end = -offset + height + bottom;
	  }
	else
	  {	/* Y=0 is bottom of page */
		end = offset + height - top;
	  }
	return min(height, end);
}

/* Open the current page for printing. */
int
gdev_prn_open_printer(gx_device *pdev, int binary_mode)
{	char *fname = ppdev->fname;
	char pfname[prn_fname_sizeof + 20];
	char *fchar = fname;
	long count1 = ppdev->PageCount + 1;

	while ( (fchar = strchr(fchar, '%')) != NULL )
	{	if ( *++fchar == '%' )
		  {	++fchar;
			continue;
		  }
		while ( !isalpha(*fchar) )
		  ++fchar;
		sprintf(pfname, fname,
			(*fchar == 'l' ? count1 : (int)count1));
		fname = pfname;
		break;
	}
	if ( ppdev->file == NULL )
	{	if ( !strcmp(fname, "-") )
			ppdev->file = stdout;
		else
		{	ppdev->file = gp_open_printer(fname, binary_mode);
			if ( ppdev->file == NULL )
				return_error(gs_error_invalidfileaccess);
		}
		ppdev->file_is_new = true;
	}
	else
		ppdev->file_is_new = false;
	return 0;
}

/* Copy a scan line from the buffer to the printer. */
int
gdev_prn_get_bits(gx_device_printer *pdev, int y, byte *str, byte **actual_data)
{	int code = (*dev_proc(pdev, get_bits))((gx_device *)pdev, y, str, actual_data);
	uint line_size = gdev_prn_raster(pdev);
	int last_bits = -(pdev->width * pdev->color_info.depth) & 7;
	if ( code < 0 ) return code;
	if ( last_bits != 0 )
	{	byte *dest = (actual_data != 0 ? *actual_data : str);
		dest[line_size - 1] &= 0xff << last_bits;
	}
	return 0;
}
/* Copy scan lines to a buffer.  Return the number of scan lines, */
/* or <0 if error. */
int
gdev_prn_copy_scan_lines(gx_device_printer *pdev, int y, byte *str, uint size)
{	uint line_size = gdev_prn_raster(pdev);
	int count = size / line_size;
	int i;
	byte *dest = str;
	count = min(count, pdev->height - y);
	for ( i = 0; i < count; i++, dest += line_size )
	{	int code = gdev_prn_get_bits(pdev, y + i, dest, NULL);
		if ( code < 0 ) return code;
	}
	return count;
}

/* Close the current page. */
int
gdev_prn_close_printer(gx_device *pdev)
{	if ( strchr(ppdev->fname, '%') )	/* file per page */
	   {	gp_close_printer(ppdev->file, ppdev->fname);
		ppdev->file = NULL;
	   }
	return 0;
}
