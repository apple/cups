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

/*$Id: gxtext.h,v 1.1 2000/03/13 18:58:46 mike Exp $ */
/* Driver text interface implementation support */

#ifndef gxtext_INCLUDED
#  define gxtext_INCLUDED

#include "gstext.h"

/* EVERYTHING IN THIS FILE IS SUBJECT TO CHANGE WITHOUT NOTICE. */

/*
 * Define the control parameter for setting text metrics.
 */
typedef enum {
    TEXT_SET_CHAR_WIDTH,
    TEXT_SET_CACHE_DEVICE,
    TEXT_SET_CACHE_DEVICE2
} gs_text_cache_control_t;

/*
 * Define the procedures associated with text display.
 */
struct gs_text_enum_procs_s {

    /*
     * Process the text.  Then client should call this repeatedly until
     * it returns <= 0.  (> 0 means the client must intervene.)
     */

#define text_enum_proc_process(proc)\
  int proc(P1(gs_text_enum_t *penum))

    text_enum_proc_process((*process));

    /*
     * Set the character width and optionally bounding box,
     * and enable caching.
     */

#define text_enum_proc_set_cache(proc)\
  int proc(P3(gs_text_enum_t *penum, const double *values,\
    gs_text_cache_control_t control))

    text_enum_proc_set_cache((*set_cache));

};

/* Abstract types */
#ifndef gs_imager_state_DEFINED
#  define gs_imager_state_DEFINED
typedef struct gs_imager_state_s gs_imager_state;

#endif
#ifndef gx_device_color_DEFINED
#  define gx_device_color_DEFINED
typedef struct gx_device_color_s gx_device_color;

#endif
#ifndef gs_font_DEFINED
#  define gs_font_DEFINED
typedef struct gs_font_s gs_font;

#endif
#ifndef gx_path_DEFINED
#  define gx_path_DEFINED
typedef struct gx_path_s gx_path;

#endif
#ifndef gx_clip_path_DEFINED
#  define gx_clip_path_DEFINED
typedef struct gx_clip_path_s gx_clip_path;

#endif

/*
 * Define the driver procedure for text.
 */
#define dev_t_proc_text_begin(proc, dev_t)\
  int proc(P9(dev_t *dev,\
    gs_imager_state *pis,\
    const gs_text_params_t *text,\
    const gs_font *font,\
    gx_path *path,			/* unless DO_NONE & !RETURN_WIDTH */\
    const gx_device_color *pdcolor,	/* DO_DRAW */\
    const gx_clip_path *pcpath,		/* DO_DRAW */\
    gs_memory_t *memory,\
    gs_text_enum_t **ppenum))
#define dev_proc_text_begin(proc)\
  dev_t_proc_text_begin(proc, gx_device)

/*
 * Begin processing text.  This calls the device procedure, and also
 * initializes the common parts of the enumerator.
 */
dev_proc_text_begin(gx_device_text_begin);

#endif /* gxtext_INCLUDED */
