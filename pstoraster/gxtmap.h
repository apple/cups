/* Copyright (C) 1994, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gxtmap.h,v 1.2 2000/03/08 23:15:06 mike Exp $ */
/* Definition of transfer mapping function */
/* (also used for black generation and undercolor removal) */

#ifndef gxtmap_INCLUDED
#  define gxtmap_INCLUDED

/* Common definition for mapping procedures. */
/* These are used for transfer functions, black generation, */
/* and undercolor removal. */
/* gx_transfer_map should probably be renamed gx_mapping_cache.... */

/* Define an abstract type for a transfer map. */
typedef struct gx_transfer_map_s gx_transfer_map;

/*
 * Define the type of a mapping procedure.  There are two forms of this.
 * The original form passed only the transfer map itself as an argument:
 */
typedef float (*gs_mapping_proc) (P2(floatp, const gx_transfer_map *));

/*
 * Later, we recognized that this procedure should really be a general
 * closure:
 */
typedef float (*gs_mapping_closure_proc_t) (P3(floatp value,
					       const gx_transfer_map * pmap,
					       const void *proc_data));
typedef struct gs_mapping_closure_s {
    gs_mapping_closure_proc_t proc;
    const void *data;
} gs_mapping_closure_t;

#endif /* gxtmap_INCLUDED */
