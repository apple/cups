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

/* gsimage.c */
/* Image setup procedures for Ghostscript library */
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsstruct.h"
#include "gscspace.h"
#include "gsmatrix.h"		/* for gsiparam.h */
#include "gsimage.h"
#include "gxarith.h"		/* for igcd */
#include "gxdevice.h"
#include "gzstate.h"

/* Define the enumeration state for this interface layer. */
/*typedef struct gs_image_enum_s gs_image_enum;*/	/* in gsimage.h */
struct gs_image_enum_s {
		/* The following are set at initialization time. */
	gs_memory_t *memory;
	gx_device *dev;
	void *info;		/* driver bookkeeping structure */
	int num_components;
	bool multi;
	int num_planes;
	int width, height;
	int bpp;		/* bits per pixel (per plane, if multi) */
	int bytes_mod;	/* minimum # of bytes for an integral # of pixels */
	uint raster;		/* bytes per row (per plane), no padding */
		/* The following are updated dynamically. */
	int plane_index;	/* index of next plane of data */
	int x, y;
	uint pos;		/* byte position within the scan line */
	gs_const_string planes[4];
	byte saved[4][6];	/* partial bytes_mod bytes */
	int num_saved;		/* # of saved bytes (per plane) */
	bool error;
};
gs_private_st_composite(st_gs_image_enum, gs_image_enum, "gs_image_enum",
  gs_image_enum_enum_ptrs, gs_image_enum_reloc_ptrs);
#define gs_image_enum_num_ptrs 2

/* GC procedures */
#define eptr ((gs_image_enum *)vptr)
private ENUM_PTRS_BEGIN(gs_image_enum_enum_ptrs) {
	/* Enumerate the data planes. */
	index -= gs_image_enum_num_ptrs;
	if ( index < eptr->plane_index )
	  { *pep = (void *)&eptr->planes[index];
	    return ptr_string_type;
	  }
	return 0;
	}
	ENUM_PTR(0, gs_image_enum, dev);
	ENUM_PTR(1, gs_image_enum, info);
ENUM_PTRS_END
private RELOC_PTRS_BEGIN(gs_image_enum_reloc_ptrs) {
	int i;
	RELOC_PTR(gs_image_enum, dev);
	RELOC_PTR(gs_image_enum, info);
	for ( i = 0; i < eptr->plane_index; i++ )
	  RELOC_CONST_STRING_PTR(gs_image_enum, planes[i]);
} RELOC_PTRS_END
#undef eptr

/* Allocate an image enumerator. */
gs_image_enum *
gs_image_enum_alloc(gs_memory_t *mem, client_name_t cname)
{	gs_image_enum *pie =
	  gs_alloc_struct(mem, gs_image_enum, &st_gs_image_enum, cname);

	if ( pie != 0 )
	  { int i;
	    pie->memory = mem;
	    pie->info = 0;
	    /* Clean pointers for GC. */
	    pie->dev = 0;
	    for ( i = 0; i < countof(pie->planes); ++i )
	      pie->planes[i].data = 0, pie->planes[i].size = 0;
	  }
	return pie;
}

/* Start processing an image. */
int
gs_image_init(gs_image_enum *pie, const gs_image_t *pim, bool multi,
  gs_state *pgs)
{	gx_device *dev = gs_currentdevice_inline(pgs);
	gs_image_t image;
	ulong samples_per_row = pim->Width;
	int code;

	if ( pim->Width == 0 || pim->Height == 0 )
	  return 1;
	image = *pim;
	if ( image.ImageMask )
	  { image.ColorSpace = NULL;
	    if ( pgs->in_cachedevice <= 1 )
	      image.adjust = false;
	    pie->num_components = pie->num_planes = 1;
	  }
	else
	  { if ( pgs->in_cachedevice )
	      return_error(gs_error_undefined);
	    if ( image.ColorSpace == NULL )
	      image.ColorSpace = gs_color_space_DeviceGray();
	    pie->num_components =
	      gs_color_space_num_components(image.ColorSpace);
	    if ( multi )
	      pie->num_planes = pie->num_components;
	    else
	      { pie->num_planes = 1;
		samples_per_row *= pie->num_components;
	      }
	  }
	if ( image.ImageMask | image.CombineWithColor )
	  gx_set_dev_color(pgs);
	code = (*dev_proc(dev, begin_image))
	  (dev, (const gs_imager_state *)pgs, &image,
	   (multi ? gs_image_format_component_planar : gs_image_format_chunky),
	   gs_image_shape_rows | gs_image_shape_split_row,
	   pgs->dev_color, pgs->clip_path, pie->memory, &pie->info);
	if ( code < 0 )
	  return code;
	pie->dev = dev;
	pie->multi = multi;
	pie->bpp =
	  image.BitsPerComponent * pie->num_components / pie->num_planes;
	pie->width = image.Width;
	pie->height = image.Height;
	pie->bytes_mod = pie->bpp / igcd(pie->bpp, 8);
	pie->raster = (samples_per_row * image.BitsPerComponent + 7) >> 3;
	/* Initialize the dynamic part of the state. */
	pie->plane_index = 0;
	pie->x = pie->y = 0;
	pie->pos = 0;
	pie->num_saved = 0;
	pie->error = false;
	return 0;
}

/*
 * Return the number of bytes of data per row
 * (per plane, if MultipleDataSources is true).
 */
uint
gs_image_bytes_per_row(const gs_image_enum *pie)
{	return pie->raster;
}

/* Process the next piece of an image. */
private int near
copy_planes(gx_device *dev, gs_image_enum *pie, const byte **planes, int w, int h)
{	int code = (*dev_proc(dev, image_data))(dev, pie->info, planes,
						pie->raster, pie->x, pie->y,
						w, h);
	if ( code < 0 )
	  pie->error = true;
	return code;
}
int
gs_image_next(gs_image_enum *pie, const byte *dbytes, uint dsize,
  uint *pused)
{	gx_device *dev = pie->dev;
	uint left;
	const byte *planes[4];
	int i;
	int code;

	/*
	 * Handle the following differences between gs_image_next and
	 * the device image_data procedure:
	 *
	 *	- image_data requires an array of planes; gs_image_next
	 *	expects planes in successive calls.
	 *
	 *	- image_data requires that each call pass an integral
	 *	number of pixels, and not cross scan line boundaries
	 *	unless all scan lines have the same amount of data;
	 *	gs_image_next allows arbitrary amounts of data.
	 */
	if ( pie->plane_index != 0 )
	  if ( dsize != pie->planes[0].size )
	    return_error(gs_error_rangecheck);
	pie->planes[pie->plane_index].data = dbytes;
	pie->planes[pie->plane_index].size = dsize;
	if ( ++(pie->plane_index) != pie->num_planes )
	  return 0;
	/* We have a full set of planes. */
	for ( i = 0; i < pie->num_planes; ++i )
	  planes[i] = pie->planes[i].data;
	left = dsize;
	/*
	 * We might have some left-over bytes from the previous set of planes.
	 * Check for this now.
	 */
	if ( pie->num_saved != 0 )
	  { int copy =
	      min(pie->bytes_mod, pie->raster - pie->pos) - pie->num_saved;
	    if ( dsize >= copy )
	      { int w = pie->bytes_mod * 8 / pie->bpp;
		const byte *mod_planes[4];

		for ( i = 0; i < pie->num_planes; ++i )
		  { mod_planes[i] = pie->saved[i];
		    memcpy(pie->saved[i] + pie->num_saved, planes[i], copy);
		    planes[i] += copy;
		  }
		pie->num_saved = 0;
		left -= copy;
		if ( pie->x + w >= pie->width )
		  { w = pie->width - pie->x;
		    code = copy_planes(dev, pie, mod_planes, w, 1);
		    if ( code < 0 )
		      return code;
		    pie->x = 0;
		    pie->y++;
		    pie->pos = 0;
		    if ( pie->y == pie->height )
		      { *pused = dsize - left;
			return 1;
		      }
		  }
		else
		  { code = copy_planes(dev, pie, mod_planes, w, 1);
		    if ( code < 0 )
		      return code;
		    pie->x += w;
		    pie->pos += pie->bytes_mod;
		  }
	      }
	  }
	/*
	 * Pass data by rows.
	 */
	{ uint row_left;
	  while ( left >= (row_left = pie->raster - pie->pos) )
	    { int w = pie->width - pie->x;
	      int h = (pie->x == 0 ? left / row_left : 1);

	      if ( h > pie->height - pie->y )
		h = pie->height - pie->y;
	      code = copy_planes(dev, pie, planes, w, h);
	      if ( code < 0 )
		return code;
	      row_left *= h;
	      for ( i = 0; i < pie->num_planes; ++i )
		planes[i] += row_left;
	      left -= row_left;
	      pie->x = 0;
	      pie->y += h;
	      pie->pos = 0;
	      if ( pie->y == pie->height )
		{ *pused = dsize - left;
		  return 1;
		}
	    }
	  if ( (row_left = left - left % pie->bytes_mod) != 0 )
	    { int w = row_left * 8 / pie->bpp;

	      code = copy_planes(dev, pie, planes, w, 1);
	      if ( code < 0 )
		return code;
	      for ( i = 0; i < pie->num_planes; ++i )
		planes[i] += row_left;
	      left -= row_left;
	      pie->x += w;
	      pie->pos += row_left;
	    }
	}
	/*
	 * Save any left-over bytes.
	 */
	if ( left != 0 )
	  { for ( i = 0; i < pie->num_planes; ++i )
	      memcpy(pie->saved[i] + pie->num_saved, planes[i], left);
	    pie->num_saved += left;
	  }
	pie->plane_index = 0;
	*pused = dsize;
	return 0;
}

/* Clean up after processing an image. */
void
gs_image_cleanup(gs_image_enum *pie)
{	gx_device *dev = pie->dev;

	(*dev_proc(dev, end_image))(dev, pie->info, !pie->error);
	/* Don't free the local enumerator -- the client does that. */
}
