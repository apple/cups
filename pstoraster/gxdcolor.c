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

/* gxdcolor.c */
/* Pure and null device color implementation */
#include "gx.h"
#include "gxdcolor.h"
#include "gxdevice.h"

/* Define the standard device color types. */
private dev_color_proc_load(gx_dc_no_load);
private dev_color_proc_fill_rectangle(gx_dc_no_fill_rectangle);
private dev_color_proc_load(gx_dc_pure_load);
private dev_color_proc_fill_rectangle(gx_dc_pure_fill_rectangle);
const gx_device_color_procs
  gx_dc_procs_none =
    { gx_dc_no_load, gx_dc_no_fill_rectangle, 0, 0 },
  gx_dc_procs_pure =
    { gx_dc_pure_load, gx_dc_pure_fill_rectangle, 0, 0 };
#undef gx_dc_type_none
const gx_device_color_procs _ds *gx_dc_type_none = &gx_dc_procs_none;
#define gx_dc_type_none (&gx_dc_procs_none)
#undef gx_dc_type_pure
const gx_device_color_procs _ds *gx_dc_type_pure = &gx_dc_procs_pure;
#define gx_dc_type_pure (&gx_dc_procs_pure)

/* Define a null RasterOp source. */
const gx_rop_source_t gx_rop_no_source = { gx_rop_no_source_body };

/* Null color */
private int
gx_dc_no_load(gx_device_color *pdevc, const gs_state *ignore_pgs)
{	return 0;
}
private int
gx_dc_no_fill_rectangle(const gx_device_color *pdevc, int x, int y,
  int w, int h, gx_device *dev, gs_logical_operation_t lop,
  const gx_rop_source_t *source)
{	return 0;
}

/* Pure color */
private int
gx_dc_pure_load(gx_device_color *pdevc, const gs_state *ignore_pgs)
{	return 0;
}
/* Fill a rectangle with a pure color. */
/* Note that we treat this as "texture" for RasterOp. */
private int
gx_dc_pure_fill_rectangle(const gx_device_color *pdevc, int x, int y,
  int w, int h, gx_device *dev, gs_logical_operation_t lop,
  const gx_rop_source_t *source)
{	if ( source == NULL && lop_no_S_is_T(lop) )
	  return (*dev_proc(dev, fill_rectangle))(dev, x, y, w, h,
						  pdevc->colors.pure);
	{ gx_color_index colors[2];

	  colors[0] = colors[1] = pdevc->colors.pure;
	  if ( source == NULL )
	    source = &gx_rop_no_source;
	  return (*dev_proc(dev, strip_copy_rop))(dev, source->sdata,
					    source->sourcex, source->sraster,
					    source->id,
					    (source->use_scolors ?
					     source->scolors : NULL),
					    NULL /*arbitrary*/, colors,
					    x, y, w, h, 0, 0, lop);
	}
}
