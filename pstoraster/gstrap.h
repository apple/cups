/* Copyright (C) 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gstrap.h,v 1.1 2000/03/13 18:57:56 mike Exp $ */
/* Definitions for trapping parameters and zones */

#ifndef gstrap_INCLUDED
#  define gstrap_INCLUDED

#include "gsparam.h"

/* ---------------- Types and structures ---------------- */

/* Opaque type for a path */
#ifndef gx_path_DEFINED
#  define gx_path_DEFINED
typedef struct gx_path_s gx_path;

#endif

/* Define the placement of image traps. */
typedef enum {
    tp_Center,
    tp_Choke,
    tp_Spread,
    tp_Normal
} gs_trap_placement_t;

#define gs_trap_placement_names\
  "Center", "Choke", "Spread", "Normal"

/* Define a trapping parameter set. */
typedef struct gs_trap_params_s {
    float BlackColorLimit;	/* 0-1 */
    float BlackDensityLimit;	/* > 0 */
    float BlackWidth;		/* > 0 */
    /* ColorantZoneDetails; */
    bool Enabled;
    /* HalftoneName; */
    bool ImageInternalTrapping;
    int ImageResolution;
    bool ImageToObjectTrapping;
    gs_trap_placement_t ImageTrapPlacement;
    float SlidingTrapLimit;	/* 0-1 */
    float StepLimit;		/* 0-1 */
    float TrapColorScaling;	/* 0-1 */
    float TrapWidth;		/* > 0 */
} gs_trap_params_t;

/* Define a trapping zone.  ****** SUBJECT TO CHANGE ****** */
typedef struct gs_trap_zone_s {
    gs_trap_params_t params;
    gx_path *zone;
} gs_trap_zone_t;

/* ---------------- Procedures ---------------- */

int gs_settrapparams(P2(gs_trap_params_t * params, gs_param_list * list));

#endif /* gstrap_INCLUDED */
