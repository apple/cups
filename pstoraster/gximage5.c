/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gximage5.c */
/* Interpolated image procedures */
#include "gx.h"
#include "memory_.h"
#include "gpcheck.h"
#include "gserrors.h"
#include "gxfixed.h"
#include "gxfrac.h"
#include "gxarith.h"
#include "gxmatrix.h"
#include "gsccolor.h"
#include "gspaint.h"
#include "gzstate.h"
#include "gxcmap.h"
#include "gzpath.h"
#include "gxdevice.h"
#include "gxdevmem.h"
#include "gxcpath.h"
#include "gximage.h"

/* ------ Rendering for interpolated images ------ */

int
image_render_interpolate(gx_image_enum *penum, byte *buffer,
  uint iw, int h, gx_device *dev)
{	stream_IScale_state *pss = penum->scaler;
	const gs_state *pgs = penum->pgs;
	const gs_imager_state *pis = penum->pis;
	const gs_color_space *pcs = penum->pcs;
	int c = pss->Colors;
	stream_cursor_read r;
	stream_cursor_write w;

	if ( h != 0 )
	  {	/* Convert the unpacked data to concrete values in */
		/* the source buffer. */
		uint row_size = pss->WidthIn * c * pss->sizeofPixelIn;

		if ( pss->sizeofPixelIn == 1 )
		  {	/* Easy case: 8-bit device color values. */
			r.ptr = buffer - 1;
		  }
		else
		  {	/* Messy case: concretize each sample. */
			int bps = penum->bps;
			int dc = penum->spp;
			byte *pdata = buffer;
			frac *psrc = (frac *)penum->line;
			gs_client_color cc;
			int i;

			r.ptr = (byte *)psrc - 1;
			for ( i = 0; i < pss->WidthIn; i++, psrc += c )
			  {	int j;
				if ( bps <= 8 )
				  for ( j = 0; j < dc; ++pdata, ++j  )
				    { decode_sample(*pdata, cc, j);
				    }
				else		/* bps == 12 */
				  for ( j = 0; j < dc; pdata += sizeof(frac), ++j  )
				    { decode_frac(*(frac *)pdata, cc, j);
				    }
				(*pcs->type->concretize_color)(&cc, pcs, psrc,
							       pgs);
			  }
		  }
		r.limit = r.ptr + row_size;
	  }
	else			/* h == 0 */
	  r.ptr = 0, r.limit = 0;

	/*
	 * Process input and/or collect output.
	 * By construction, the pixels are 1-for-1 with the device,
	 * but the Y coordinate might be inverted.
	 */

	{	int xo = fixed2int_pixround(penum->mtx);
		int yo = fixed2int_pixround(penum->mty);
		int width = pss->WidthOut;
		int dy;
		const gs_color_space *pconcs = cs_concrete_space(pcs, pgs);
		gs_logical_operation_t lop = pis->log_op;
		int bpp = dev->color_info.depth;
		uint raster = bitmap_raster(width * bpp);

		if ( dda_current(penum->next_y) > penum->mty )
		  dy = 1;
		else
		  dy = -1, yo--;
		for ( ; ; )
		{	int ry = yo + penum->line_xy * dy;
			int x;
			const frac *psrc;
			gx_device_color devc;
			int code;
			declare_line_accum(penum->line, bpp, xo);
		
			w.limit = penum->line + width * c *
			  sizeof(gx_color_index) - 1;
			w.ptr = w.limit - width * c *
			  (sizeof(gx_color_index) - pss->sizeofPixelOut);
			psrc = (const frac *)(w.ptr + 1);
			code = (*s_IScale_template.process)
			  ((stream_state *)pss, &r, &w, false);
			if ( code < 0 && code != EOFC )
			  return_error(gs_error_ioerror);
			if ( w.ptr == w.limit )
			  { for ( x = xo; x < xo + width; x++, psrc += c )
			      { (*pconcs->type->remap_concrete_color)
				  (psrc, &devc, pgs);
				if ( color_is_pure(&devc) )
				  { /* Just pack colors into a scan line. */
				    gx_color_index color = devc.colors.pure;
				    line_accum(color, bpp);
				  }
				else
				  { if ( bpp < 8 )
				      { if ( (l_shift -= bpp) < 0 )
					  *l_dst++ = (byte)l_bits, l_bits = 0,
					  l_shift += 8;
				      }
				  else
				    l_dst += bpp >> 3;
				    line_accum_copy(dev, penum->line, bpp,
						    xo, x, raster, ry);
				    code = gx_fill_rectangle_device_rop(x, ry,
						1, 1, &devc, dev, lop);
				    if ( code < 0 )
				      return code;
				    l_xprev = x + 1;
				  }
			      }
			    line_accum_copy(dev, penum->line, bpp,
					    xo, x, raster, ry);
			    penum->line_xy++;
			    continue;
			  }
			if ( r.ptr == r.limit || code == EOFC )
			  break;
		}
	}

	return (h == 0 ? 0 : 1);
}
