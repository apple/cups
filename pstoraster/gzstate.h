/* Copyright (C) 1989, 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gzstate.h,v 1.2 2000/03/08 23:15:07 mike Exp $ */
/* Private graphics state definition for Ghostscript library */

#ifndef gzstate_INCLUDED
#  define gzstate_INCLUDED

#include "gscpm.h"
#include "gsrefct.h"
#include "gxdcolor.h"
#include "gxistate.h"
#include "gsstate.h"
#include "gxstate.h"

/* Opaque types referenced by the graphics state. */
#ifndef gx_path_DEFINED
#  define gx_path_DEFINED
typedef struct gx_path_s gx_path;

#endif
#ifndef gx_clip_path_DEFINED
#  define gx_clip_path_DEFINED
typedef struct gx_clip_path_s gx_clip_path;

#endif
#ifndef gs_color_space_DEFINED
#  define gs_color_space_DEFINED
typedef struct gs_color_space_s gs_color_space;

#endif
#ifndef gs_client_color_DEFINED
#  define gs_client_color_DEFINED
typedef struct gs_client_color_s gs_client_color;

#endif
#ifndef gs_font_DEFINED
#  define gs_font_DEFINED
typedef struct gs_font_s gs_font;

#endif

/* Graphics state structure. */

struct gs_state_s {
    gs_imager_state_common;	/* imager state, must be first */
    gs_state *saved;		/* previous state from gsave */

    /* Transformation: */

    gs_matrix ctm_inverse;
    bool ctm_inverse_valid;	/* true if ctm_inverse = ctm^-1 */
    gs_matrix ctm_default;
    bool ctm_default_set;	/* if true, use ctm_default; */
    /* if false, ask device */

    /* Paths: */

    gx_path *path;
    gx_clip_path *clip_path;
    gx_clip_path *view_clip;	/* (may be 0, or have rule = 0) */
    bool clamp_coordinates;	/* if true, clamp out-of-range */
    /* coordinates; if false, */
    /* report a limitcheck */
    /* Effective clip path cache */
    gs_id effective_clip_id;	/* (key) clip path id */
    gs_id effective_view_clip_id;	/* (key) view clip path id */
    gx_clip_path *effective_clip_path;	/* (value) effective clip path, */
    /* possibly = clip_path or view_clip */
    bool effective_clip_shared;	/* true iff e.c.p. = c.p. or v.c. */

    /* Color (device-independent): */

    gs_color_space *color_space;
    gs_client_color *ccolor;

    /* Color caches: */

    gx_device_color *dev_color;

    /* Font: */

    gs_font *font;
    gs_font *root_font;
    gs_matrix_fixed char_tm;	/* font matrix * ctm */
#define char_tm_only(pgs) *(gs_matrix *)&(pgs)->char_tm
    bool char_tm_valid;		/* true if char_tm is valid */
    byte in_cachedevice;	/* 0 if not in setcachedevice, */
    /* 1 if in setcachedevice but not */
    /* actually caching, */
    /* 2 if in setcachedevice and */
    /* actually caching */
    gs_char_path_mode in_charpath;	/* (see gscpm.h) */
    gs_state *show_gstate;	/* gstate when show was invoked */
    /* (so charpath can append to path) */

    /* Other stuff: */

    int level;			/* incremented by 1 per gsave */
    gx_device *device;
#undef gs_currentdevice_inline
#define gs_currentdevice_inline(pgs) ((pgs)->device)

    /* Client data: */

    void *client_data;
#define gs_state_client_data(pgs) ((pgs)->client_data)
    gs_state_client_procs client_procs;
};

#define private_st_gs_state()	/* in gsstate.c */\
  gs_private_st_composite(st_gs_state, gs_state, "gs_state",\
    gs_state_enum_ptrs, gs_state_reloc_ptrs)

#endif /* gzstate_INCLUDED */
