/* Copyright (C) 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gxidata.c,v 1.1 2000/03/08 23:15:01 mike Exp $ */
/* Generic image enumeration and cleanup */
#include "gx.h"
#include "memory_.h"
#include "gserrors.h"
#include "gxdevice.h"
#include "gxcpath.h"
#include "gximage.h"

/* Process the next piece of an ImageType 1 image. */
int
gx_image1_plane_data(gx_device * dev, gx_image_enum_common_t * info,
		     const gx_image_plane_t * planes, int height)
{
    gx_image_enum *penum = (gx_image_enum *) info;
    int y = penum->y;
    int y_end = min(y + height, penum->rect.h);
    int width_spp = penum->rect.w * penum->spp;
    int num_planes = penum->num_planes;

#define bcount(plane)		/* bytes per data row */\
  (((penum->rect.w + (plane).data_x) * penum->spp / num_planes * penum->bps\
    + 7) >> 3)
    fixed adjust = penum->adjust;
    ulong offsets[gs_image_max_components];
    int ignore_data_x;
    int code;

    if (height == 0)
	return 0;

    /* Set up the clipping and/or RasterOp device if needed. */

    if (penum->clip_dev) {
	gx_device_clip *cdev = penum->clip_dev;

	cdev->target = dev;
	dev = (gx_device *) cdev;
    }
    if (penum->rop_dev) {
	gx_device_rop_texture *rtdev = penum->rop_dev;

	((gx_device_forward *) rtdev)->target = dev;
	dev = (gx_device *) rtdev;
    }
    /* Now render complete rows. */

    memset(offsets, 0, num_planes * sizeof(offsets[0]));
    for (; penum->y < y_end; penum->y++) {
	int px;

	/*
	 * Normally, we unpack the data into the buffer, but if
	 * there is only one plane and we don't need to expand the
	 * input samples, we may use the data directly.
	 */
	int sourcex = planes[0].data_x;
	const byte *buffer =
	(*penum->unpack) (penum->buffer, &sourcex,
			  planes[0].data + offsets[0],
			  planes[0].data_x, bcount(planes[0]),
			  &penum->map[0].table, penum->spread);

	offsets[0] += planes[0].raster;
	for (px = 1; px < num_planes; ++px) {
	    (*penum->unpack) (penum->buffer + (px << penum->log2_xbytes),
			      &ignore_data_x,
			      planes[px].data + offsets[px],
			      planes[px].data_x, bcount(planes[px]),
			      &penum->map[px].table, penum->spread);
	    offsets[px] += planes[px].raster;
	}
#ifdef DEBUG
	if (gs_debug_c('B')) {
	    int i, n = width_spp;

	    dlputs("[B]row:");
	    for (i = 0; i < n; i++)
		dprintf1(" %02x", buffer[i]);
	    dputs("\n");
	}
#endif
	penum->cur.x = dda_current(penum->dda.row.x);
	dda_next(penum->dda.row.x);
	penum->cur.y = dda_current(penum->dda.row.y);
	dda_next(penum->dda.row.y);
	if (!penum->interpolate)
	    switch (penum->posture) {
		case image_portrait:
		    {		/* Precompute integer y and height, */
			/* and check for clipping. */
			fixed yc = penum->cur.y, yn = dda_current(penum->dda.row.y);

			if (yn < yc) {
			    fixed temp = yn;

			    yn = yc;
			    yc = temp;
			}
			yc -= adjust;
			if (yc >= penum->clip_outer.q.y)
			    goto mt;
			yn += adjust;
			if (yn <= penum->clip_outer.p.y)
			    goto mt;
			penum->yci = fixed2int_pixround(yc);
			penum->hci = fixed2int_pixround(yn) - penum->yci;
			if (penum->hci == 0)
			    goto mt;
		    }
		    break;
		case image_landscape:
		    {		/* Check for no pixel centers in x. */
			fixed xc = penum->cur.x, xn = dda_current(penum->dda.row.x);

			if (xn < xc) {
			    fixed temp = xn;

			    xn = xc;
			    xc = temp;
			}
			xc -= adjust;
			if (xc >= penum->clip_outer.q.x)
			    goto mt;
			xn += adjust;
			if (xn <= penum->clip_outer.p.x)
			    goto mt;
			penum->xci = fixed2int_pixround(xc);
			penum->wci = fixed2int_pixround(xn) - penum->xci;
			if (penum->wci == 0)
			    goto mt;
		    }
		    break;
		case image_skewed:
		    ;
	    }
	dda_translate(penum->dda.pixel0.x,
		      penum->cur.x - penum->prev.x);
	dda_translate(penum->dda.pixel0.y,
		      penum->cur.y - penum->prev.y);
	penum->prev = penum->cur;
	code = (*penum->render) (penum, buffer, sourcex, width_spp, 1,
				 dev);
	if (code < 0)
	    goto err;
      mt:;
    }
    if (penum->y < penum->rect.h) {
	code = 0;
	goto out;
    }
    /* End of data.  Render any left-over buffered data. */
    code = gx_image1_flush(info);
    if (code < 0) {
	penum->y--;
	goto err;
    }
    code = 1;
    goto out;
  err:				/* Error or interrupt, restore original state. */
    while (penum->y > y) {
	dda_previous(penum->dda.row.x);
	dda_previous(penum->dda.row.y);
	--(penum->y);
    }
    /* Note that caller must call end_image */
    /* for both error and normal termination. */
  out:return code;
}

/* Flush any buffered data. */
int
gx_image1_flush(gx_image_enum_common_t * info)
{
    gx_image_enum *penum = (gx_image_enum *)info;
    int width_spp = penum->rect.w * penum->spp;
    fixed adjust = penum->adjust;

    penum->cur.x = dda_current(penum->dda.row.x);
    penum->cur.y = dda_current(penum->dda.row.y);
    switch (penum->posture) {
	case image_portrait:
	    {
		fixed yc = penum->cur.y;

		penum->yci = fixed2int_rounded(yc - adjust);
		penum->hci = fixed2int_rounded(yc + adjust) - penum->yci;
	    }
	    break;
	case image_landscape:
	    {
		fixed xc = penum->cur.x;

		penum->xci = fixed2int_rounded(xc - adjust);
		penum->wci = fixed2int_rounded(xc + adjust) - penum->xci;
	    }
	    break;
	case image_skewed:	/* pacify compilers */
	    ;
    }
    dda_translate(penum->dda.pixel0.x, penum->cur.x - penum->prev.x);
    dda_translate(penum->dda.pixel0.y, penum->cur.y - penum->prev.y);
    penum->prev = penum->cur;
    return (*penum->render)(penum, NULL, 0, width_spp, 0, penum->dev);
}

/* Clean up by releasing the buffers. */
/* Currently we ignore draw_last. */
int
gx_image1_end_image(gx_device *ignore_dev, gx_image_enum_common_t * info,
		    bool draw_last)
{
    gx_image_enum *penum = (gx_image_enum *) info;
    gs_memory_t *mem = penum->memory;
    stream_IScale_state *scaler = penum->scaler;

#ifdef DEBUG
    if_debug2('b', "[b]%send_image, y=%d\n",
	      (penum->y < penum->rect.h ? "premature " : ""), penum->y);
#endif
    gs_free_object(mem, penum->rop_dev, "image RasterOp");
    gs_free_object(mem, penum->clip_dev, "image clipper");
    if (scaler != 0) {
	(*s_IScale_template.release) ((stream_state *) scaler);
	gs_free_object(mem, scaler, "image scaler state");
    }
    gs_free_object(mem, penum->line, "image line");
    gs_free_object(mem, penum->buffer, "image buffer");
    gs_free_object(mem, penum, "gx_default_end_image");
    return 0;
}
