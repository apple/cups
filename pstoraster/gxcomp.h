/* Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gxcomp.h,v 1.1 2000/03/13 18:58:45 mike Exp $ */
/* Definitions for implementing compositing functions */

#ifndef gxcomp_INCLUDED
#  define gxcomp_INCLUDED

#include "gscompt.h"
#include "gsrefct.h"
#include "gxbitfmt.h"

/*
 * Define the abstract superclass for all compositing function types.
 */
						   /*typedef struct gs_composite_s gs_composite_t; *//* in gscompt.h */

#ifndef gs_imager_state_DEFINED
#  define gs_imager_state_DEFINED
typedef struct gs_imager_state_s gs_imager_state;

#endif

#ifndef gx_device_DEFINED
#  define gx_device_DEFINED
typedef struct gx_device_s gx_device;

#endif

typedef struct gs_composite_type_procs_s {

    /*
     * Create the default compositor for a compositing function.
     */
#define composite_create_default_compositor_proc(proc)\
  int proc(P5(const gs_composite_t *pcte, gx_device **pcdev,\
    gx_device *dev, const gs_imager_state *pis, gs_memory_t *mem))
    composite_create_default_compositor_proc((*create_default_compositor));

    /*
     * Test whether this function is equal to another one.
     */
#define composite_equal_proc(proc)\
  bool proc(P2(const gs_composite_t *pcte, const gs_composite_t *pcte2))
    composite_equal_proc((*equal));

    /*
     * Convert the representation of this function to a string
     * for writing in a command list.  *psize is the amount of space
     * available.  If it is large enough, the procedure sets *psize
     * to the amount used and returns 0; if it is not large enough,
     * the procedure sets *psize to the amount needed and returns a
     * rangecheck error; in the case of any other error, *psize is
     * not changed.
     */
#define composite_write_proc(proc)\
  int proc(P3(const gs_composite_t *pcte, byte *data, uint *psize))
    composite_write_proc((*write));

    /*
     * Convert the string representation of a function back to
     * a structure, allocating the structure.
     */
#define composite_read_proc(proc)\
  int proc(P4(gs_composite_t **ppcte, const byte *data, uint size,\
    gs_memory_t *mem))
    composite_read_proc((*read));

} gs_composite_type_procs_t;
typedef struct gs_composite_type_s {
    gs_composite_type_procs_t procs;
} gs_composite_type_t;

/*
 * Compositing objects are reference-counted, because graphics states will
 * eventually reference them.  Note that the common part has no
 * garbage-collectible pointers and is never actually instantiated, so no
 * structure type is needed for it.
 */
#define gs_composite_common\
	const gs_composite_type_t *type;\
	gs_id id;		/* see gscompt.h */\
	rc_header rc
struct gs_composite_s {
    gs_composite_common;
};

/* Replace a procedure with a macro. */
#define gs_composite_id(pcte) ((pcte)->id)

#endif /* gxcomp_INCLUDED */
