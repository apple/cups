/* Copyright (C) 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gdevprna.h,v 1.1 2000/03/13 19:00:47 mike Exp $ */
/* Generic asynchronous printer driver support */

/* Initial version 2/1/1998 by John Desrosiers (soho@crl.com) */
/* 7/28/98 ghost@aladdin.com - Updated to Ghostscript coding standards. */

#ifndef gdevprna_INCLUDED
# define gdevprna_INCLUDED

# include "gdevprn.h"
# include "gxsync.h"

/* 
 * General
 * -------
 * Async drivers actually create two separate instances of the device at
 * the same time. The first (the writer instance) is only used in the
 * interpretation operation; it feeds rendering commands into the command 
 * lists. The second device instance is used only for rendering the 
 * commands placed into the command list by the writer.

 * The writer builds a command list for an entire page; the command list 
 * is only queued for rendering once a page's command list is completely 
 * built. The only exception to this rule is when the interpreter runs
 * out of memory, or when no free command list memory is available. In
 * such cases, the interpreter queues a "partial page" consisting of all
 * command list data written so far, plus a command indicating that the
 * page description is not complete. After queuing the partial page, the
 * interpereter waits until the rendering process has freed enough
 * command list memory to enable the interpreter to proceed.

 * To avoid deadlocks when the system runs out of memory, special
 * memory allocation provisions are made on both the writer and 
 * renderer sides. On the writer side, enough "reserve" bandlist
 * memory is set aside at startup time to cover the needs of queuing a
 * partial page to the renderer. The renderer operates out of a fixed
 * memory space; that way, it can always complete rendering pages with
 * the memory it has. To this end, the writer protects the renderer
 * from consuming unbounded amounts of memory by a) never putting
 * complex paths into the command list, b) pre-clipping any output
 * unless the clip path consists of a single rectangle, c) never putting
 * high-level images into the clip path unless the image in question
 * meets some very stringent requirements, such as only being rotated by
 * even multiples of 90 degrees and having source-image data rows which
 * fit into the command buffer in one piece. These restrictions are what
 * dictate the "restricted bandlist format."

 * Note that the renderer's instance of the device driver uses the
 * renderer's memory. That implies that it must also operate in a small, 
 * fixed amount of memory, and must do all memory allocation using the
 * memory allocator pointed to by the render device's ->memory member.

 * Opening the Device
 * ------------------
 * The writer instance is opened first. This occurs when the system
 * calls the "standard" open procedure via the device's procedure
 * vector. The driver must implement the open function, but must call
 * down to gdev_prn_async_write_open instead of calling down to
 * gdev_prn_open. Before calling down to gdev_prn_async_write_open, the
 * driver must:
 *  a - init several procedure vectors, to wit: start_render_thread,
 *      buffer_page, print_page_copies,
 *  b - init space_params.band.BandWidth, space_params.band.BandHeight,
 *      space_params.BufferSpace (see extended comments in gdevasyn.c
 *      for details on computing appropriate values).
 *  c - if it implements those functions, the driver must init the
 *      procedure vectors for: put_params, get_hardware_params,
 *      output_page, open_render_device.
 * Notice that there are two procedure vectors: the usual std_procs, and
 * the printer-specific printer_procs.

 * Since partial page support imposes extra requirements on drivers,
 * such support can be disabled by zeroing out (in the async writer open
 * routine, after calling down to gdev_prn_async_write_open) the 
 * free_up_bandlist_memory member of the driver structure. Doing so
 * will, of course, cause interpretation to fail if memory runs out.

 * Once the driver calls down to gdev_prn_async_write_open, the async
 * support logic will create a second instance of the driver for
 * rendering, but will not open it just yet. Instead, the async logic 
 * will attempt to synchronize the two device instances.

 * Synchrnonizing the instances
 * ----------------------------
 * While still in the gdev_prn_async_write_open routine, the async logic
 * will call printer_procs.start_render_thread (which the driver is
 * required to implement). start_render_thread must somehow either start a new
 * thread or rendez-vous with an existing thread for use in rendering,
 * then return. start_render_thread must also have caused the render thread 
 * to call gdev_prn_async_render_thread, passing it as an argument a magic
 * cookie passed to start_render_thread. start_render_thread will only
 * return once the device has been closed and all renering has been
 * completed.

 * The render device will be opened on the render device's thread, by
 * calling printer_procs.open_render_device.

 * Rendering Operation
 * -------------------
 * During rendering, the device will not see rendering operations -- the
 * first "rendering" operations the driver will see is when the renderer
 * instance's print_page_copies or buffer_page routines get called. In
 * both cases, the appropriate routine must then perform get_bits calls
 * on the async logic in order to retrieve rendered bits, then transmit
 * them to the appropriate device buffers.

 * The complication that is introduced is that which is related to
 * partial pages: A buffer_page call instructs the driver to grab the
 * rendered bits, but to keep the rendered bits available for later
 * instead of marking on media. This implies that a buffer_page call
 * opens a context where subsequent buffer_page's and print_page_copies'
 * must first initialize the rendering buffers with the previous
 * rendering results before calling get_bits. Drivers use the
 * locate_overlay_buffer function to initialize the driver's rendering
 * buffers. The first print_page_copies closes the context that was
 * opened by the initial buffer_page -- the driver must go back to
 * normal rendering until a new buffer_page comes along.
 */

/* -------------- Type declarations --------------- */

/* typedef is in gdevprn.h */
/* typedef struct gdev_prn_start_render_params_s gdev_prn_start_render_params;*/
struct gdev_prn_start_render_params_s {
    gx_device_printer *writer_device;/* writer dev that points to render dev */
    gx_semaphore_t *open_semaphore;	/* signal this once open_code is set */
    int open_code;		/* RETURNS status of open of reader device */
};

/* -------- Macros used to initialize render-specific structures ------ */

#define init_async_render_procs(xpdev, xstart_render_thread,\
				xbuffer_page, xprint_page_copies)\
  BEGIN\
    (xpdev)->printer_procs.start_render_thread = (xstart_render_thread);\
    (xpdev)->printer_procs.buffer_page = (xbuffer_page);\
    (xpdev)->printer_procs.print_page_copies = (xprint_page_copies);\
  END

/* -------------- Global procedure declarations --------- */

/* Open this printer device in ASYNC (overlapped) mode.
 *
 * This routine is always called by the concrete device's xx_open routine 
 * in lieu of gdev_prn_open.
 */
int gdev_prn_async_write_open(P4(gx_device_printer *pdev, int max_raster,
				 int min_band_height, int max_src_image_row));

/* Open the render portion of a printer device in ASYNC (overlapped) mode.
 *
 * This routine is always called by concrete device's xx_open_render_device
 * in lieu of gdev_prn_open.
 */
int gdev_prn_async_render_open(P1(gx_device_printer *prdev));

/*
 * Must be called by async device driver implementation (see
 * gdevprna.h under "Synchronizing the Instances"). This is the
 * rendering loop, which requires its own thread for as long as
 * the device is open. This proc only returns after the device is closed.
 */
int	/* rets 0 ok, -ve error code */
gdev_prn_async_render_thread(P1(gdev_prn_start_render_params *));

#endif				/* gdevprna_INCLUDED */
