/* Copyright (C) 1989, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gsdevice.c */
/* Device operators for Ghostscript library */
#include "memory_.h"			/* for memcpy */
#include "gx.h"
#include "gscdefs.h"			/* for gs_lib_device_list */
#include "gserrors.h"
#include "gsstruct.h"
#include "gspath.h"			/* gs_initclip prototype */
#include "gspaint.h"			/* gs_erasepage prototype */
#include "gsmatrix.h"			/* for gscoord.h */
#include "gscoord.h"			/* for gs_initmatrix */
#include "gzstate.h"
#include "gxcmap.h"
#include "gxdevice.h"
#include "gxdevmem.h"

/* Include the extern for the device list. */
extern_gs_lib_device_list();

/* Finalization for devices: close the device if it is open. */
void
gx_device_finalize(void *vptr)
{	discard(gs_closedevice((gx_device *)vptr));
}

/* GC procedures */
#define fdev ((gx_device_forward *)vptr)
private ENUM_PTRS_BEGIN(device_forward_enum_ptrs) return 0;
	case 0:
	  *pep = gx_device_enum_ptr(fdev->target);
	  break;
ENUM_PTRS_END
private RELOC_PTRS_BEGIN(device_forward_reloc_ptrs) {
	fdev->target = gx_device_reloc_ptr(fdev->target, gcst);
} RELOC_PTRS_END
#undef fdev

/* Structure descriptors.  These must follow the procedures, because */
/* we can't conveniently forward-declare the procedures. */
/* (See gxdevice.h for details.) */
public_st_device();
public_st_device_forward();
public_st_device_null();
/* A fake descriptor for devices whose descriptor we can't find. */
gs_private_st_complex_only(st_device_unknown, byte, "gx_device(unknown)",
  0, 0, 0, gx_device_finalize);

/* GC utilities */
/* Enumerate or relocate a device pointer for a client. */
gx_device *
gx_device_enum_ptr(gx_device *dev)
{	if ( dev == 0 || dev->memory == 0 )
	  return 0;
	return dev;
}
gx_device *
gx_device_reloc_ptr(gx_device *dev, gc_state_t *gcst)
{	if ( dev == 0 || dev->memory == 0 )
	  return dev;
	return gs_reloc_struct_ptr(dev, gcst);
}

/* Set up the device procedures in the device structure. */
/* Also copy old fields to new ones. */
void
gx_device_set_procs(gx_device *dev)
{	if ( dev->static_procs != 0 )		/* 0 if already populated */
	{	dev->std_procs = *dev->static_procs;
		dev->static_procs = 0;
	}
}

/* Initialize a device just after allocation. */
int
gdev_initialize(gx_device *dev)
{	*dev = *(gx_device *)&gs_null_device;
	return 0;
}

/* Flush buffered output to the device */
int
gs_flushpage(gs_state *pgs)
{	gx_device *dev = gs_currentdevice(pgs);
	return (*dev_proc(dev, sync_output))(dev);
}

/* Make the device output the accumulated page description */
int
gs_copypage(gs_state *pgs)
{	return gs_output_page(pgs, 1, 0);
}
int
gs_output_page(gs_state *pgs, int num_copies, int flush)
{	gx_device *dev = gs_currentdevice(pgs);
	int code;

	if ( dev->IgnoreNumCopies )
	  num_copies = 1;
	code = (*dev_proc(dev, output_page))(dev, num_copies, flush);
	if ( code >= 0 )
	{	dev->PageCount += num_copies;
		if ( flush )
		  dev->ShowpageCount++;
	}
	return code;
}

/* Copy scan lines from an image device */
int
gs_copyscanlines(gx_device *dev, int start_y, byte *data, uint size,
  int *plines_copied, uint *pbytes_copied)
{	uint line_size = gx_device_raster(dev, 0);
	uint count = size / line_size;
	uint i;
	byte *dest = data;
	for ( i = 0; i < count; i++, dest += line_size )
	{	int code = (*dev_proc(dev, get_bits))(dev, start_y + i, dest, NULL);
		if ( code < 0 )
		{	/* Might just be an overrun. */
			if ( start_y + i == dev->height ) break;
			return_error(code);
		}
	}
	if ( plines_copied != NULL )
	  *plines_copied = i;
	if ( pbytes_copied != NULL )
	  *pbytes_copied = i * line_size;
	return 0;
}

/* Get the current device from the graphics state. */
gx_device *
gs_currentdevice(const gs_state *pgs)
{	return pgs->device;
}

/* Get the name of a device. */
const char *
gs_devicename(const gx_device *dev)
{	return dev->dname;
}

/* Get the initial matrix of a device. */
void
gs_deviceinitialmatrix(gx_device *dev, gs_matrix *pmat)
{	fill_dev_proc(dev, get_initial_matrix, gx_default_get_initial_matrix);
	(*dev_proc(dev, get_initial_matrix))(dev, pmat);
}

/* Get the N'th device from the known device list */
const gx_device *
gs_getdevice(int index)
{	const gx_device **list;
	int count = gs_lib_device_list(&list, NULL);

	if ( index < 0 || index >= count )
	  return 0;			/* index out of range */
	return list[index];
}

/* Clone an existing device. */
int
gs_copydevice(gx_device **pnew_dev, const gx_device *dev, gs_memory_t *mem)
{	gx_device *new_dev;
	const gs_memory_struct_type_t *std = dev->stype;

	/*
	 * Because command list devices have complicated internal pointer
	 * structures, we allocate all device instances as immovable.
	 */
	if ( std == 0 )
	  {	/*
		 * This is the statically allocated prototype.  Find its
		 * structure descriptor, and fill it in if this is the first
		 * time we've needed it.  (Right now we always fill it in,
		 * for simplicity.)
		 */
		const gx_device **list;
		gs_memory_struct_type_t *st;
		int count = gs_lib_device_list(&list, &st);
		int i;
		bool forward = false;
		const gx_device_procs *procs = dev->static_procs;

		for ( i = 0; list[i] != dev; ++i )
		  if ( i == count )
		  {	/* We can't find a structure descriptor for */
			/* this device.  Allocate it as bytes and */
			/* hope for the best. */
			std = &st_device_unknown;
			new_dev = gs_alloc_struct_array_immovable(mem,
				dev->params_size, gx_device, st,
				"gs_copydevice(unknown)");
			goto out;
		  }
		st += i;
		/*
		 * Try to figure out if this is a forwarding device.
		 * All printer devices, and no other devices, have
		 * a null fill_rectangle procedure; for other devices,
		 * we look for a likely forwarding procedure in the vector.
		 * The algorithm isn't foolproof, but it's all we've got.
		 */
		if ( procs == 0 )
		  procs = &dev->std_procs;
		if ( procs->fill_rectangle == 0 ||
		     procs->get_xfont_procs == gx_forward_get_xfont_procs
		   )
		  forward = true;
		if ( forward )
		  *st = st_device_forward;
		else
		  *st = st_device;
		st->ssize = dev->params_size;
		std = st;
	  }
	new_dev = gs_alloc_struct_immovable(mem, gx_device, std,
					    "gs_copydevice");
out:	if ( new_dev == 0 )
	  return_error(gs_error_VMerror);
	memcpy(new_dev, dev, dev->params_size);
	new_dev->memory = mem;
	new_dev->stype = std;
	new_dev->is_open = false;
	*pnew_dev = new_dev;
	return 0;
}

/* Set the device in the graphics state */
int
gs_setdevice(gs_state *pgs, gx_device *dev)
{	int code = gs_setdevice_no_erase(pgs, dev);
	if ( code == 1 )
	  code = gs_erasepage(pgs);
	return code;
}
int
gs_setdevice_no_erase(gs_state *pgs, gx_device *dev)
{	bool was_open = dev->is_open;
	int code;
	/* Initialize the device */
	if ( !was_open )
	{	gx_device_fill_in_procs(dev);
		if ( gs_device_is_memory(dev) )
		{	/* Set the target to the current device. */
			gx_device *odev = gs_currentdevice_inline(pgs);
			while ( odev != 0 && gs_device_is_memory(odev) )
				odev = ((gx_device_memory *)odev)->target;
			((gx_device_memory *)dev)->target = odev;
		}
		code = (*dev_proc(dev, open_device))(dev);
		if ( code < 0 ) return_error(code);
		dev->is_open = true;
	}
	pgs->device = dev;
	gx_set_cmap_procs(pgs);
	pgs->ctm_default_set = false;
	if (	(code = gs_initmatrix(pgs)) < 0 ||
		(code = gs_initclip(pgs)) < 0
	   )
		return code;
	gx_unset_dev_color(pgs);
	/* If we were in a charpath or a setcachedevice, */
	/* we aren't any longer. */
	pgs->in_cachedevice = 0;
	pgs->in_charpath = 0;
	return (was_open ? 0 : 1);
}

/* Make a null device. */
void
gs_make_null_device(gx_device_null *dev, gs_memory_t *mem)
{	*dev = gs_null_device;
	dev->memory = mem;
}

/* Select the null device.  This is just a convenience. */
void
gs_nulldevice(gs_state *pgs)
{	gs_setdevice(pgs, (gx_device *)&gs_null_device);
}

/* Close a device.  The client is responsible for ensuring that */
/* this device is not current in any graphics state. */
int
gs_closedevice(gx_device *dev)
{	int code = 0;
	if ( dev->is_open )
	   {	code = (*dev_proc(dev, close_device))(dev);
		if ( code < 0 ) return_error(code);
		dev->is_open = false;
	   }
	return code;
}

/* Install enough of a null device to suppress the page device check */
/* during the execution of a restore/grestore/setgstate. */
void
gx_device_no_output(gs_state *pgs)
{	pgs->device = (gx_device *)&gs_null_device;
}

/* Just set the device without reinitializing. */
/* (For internal use only.) */
void
gx_set_device_only(gs_state *pgs, gx_device *dev)
{	pgs->device = dev;
}

/* Compute the size of one scan line for a device, */
/* with or without padding to a word boundary. */
uint
gx_device_raster(const gx_device *dev, bool pad)
{	ulong bits = (ulong)dev->width * dev->color_info.depth;
	return (pad ? bitmap_raster(bits) : (uint)((bits + 7) >> 3));
}

/* Adjust the resolution for devices that only have a fixed set of */
/* geometries, so that the apparent size in inches remains constant. */
/* If fit=1, the resolution is adjusted so that the entire image fits; */
/* if fit=0, one dimension fits, but the other one is clipped. */
int
gx_device_adjust_resolution(gx_device *dev,
  int actual_width, int actual_height, int fit)
{	double width_ratio = (double)actual_width / dev->width ;
	double height_ratio = (double)actual_height / dev->height ;
	double ratio =
		(fit ? min(width_ratio, height_ratio) :
		 max(width_ratio, height_ratio));
	dev->x_pixels_per_inch *= ratio;
	dev->y_pixels_per_inch *= ratio;
	gx_device_set_width_height(dev, actual_width, actual_height);
	return 0;
}

/* Set the HWMargins to values defined in inches. */
/* If move_origin is true, also reset the Margins. */
/* Note that this assumes a printer-type device (Y axis inverted). */
void
gx_device_set_margins(gx_device *dev, const float *margins /*[4]*/,
  bool move_origin)
{	int i;
	for ( i = 0; i < 4; ++i )
		dev->HWMargins[i] = margins[i] * 72.0;
	if ( move_origin )
	{	dev->Margins[0] = -margins[0] * dev->MarginsHWResolution[0];
		dev->Margins[1] = -margins[3] * dev->MarginsHWResolution[1];
	}
}

/* Set the width and height, updating MediaSize to remain consistent. */
void
gx_device_set_width_height(gx_device *dev, int width, int height)
{	dev->width = width - dev->x_pixels_per_inch *
                     (dev->HWMargins[0] - dev->HWMargins[2]) / 72.0;
	dev->height = height - dev->y_pixels_per_inch *
	              (dev->HWMargins[1] - dev->HWMargins[3]) / 72.0;
	dev->MediaSize[0] = width * 72.0 / dev->x_pixels_per_inch;
	dev->MediaSize[1] = height * 72.0 / dev->y_pixels_per_inch;
}

/* Set the resolution, updating width and height to remain consistent. */
void
gx_device_set_resolution(gx_device *dev, floatp x_dpi, floatp y_dpi)
{
	dev->x_pixels_per_inch = x_dpi;
	dev->y_pixels_per_inch = y_dpi;

	dev->width = (dev->MediaSize[0] - dev->HWMargins[0] - dev->HWMargins[2]) *
	             dev->x_pixels_per_inch / 72.0 + 0.5;
	dev->height = (dev->MediaSize[1] - dev->HWMargins[1] - dev->HWMargins[3]) *
	              dev->y_pixels_per_inch / 72.0 + 0.5;
}

/* Set the MediaSize, updating width and height to remain consistent. */
void
gx_device_set_media_size(gx_device *dev, floatp media_width, floatp media_height)
{	dev->MediaSize[0] = media_width;
	dev->MediaSize[1] = media_height;
	dev->width = (dev->MediaSize[0] - dev->HWMargins[0] - dev->HWMargins[2]) *
	             dev->x_pixels_per_inch / 72.0 + 0.5;
	dev->height = (dev->MediaSize[1] - dev->HWMargins[1] - dev->HWMargins[3]) *
	              dev->y_pixels_per_inch / 72.0 + 0.5;
}
