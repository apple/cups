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

/*$Id: gsmemlok.h,v 1.1 2000/03/13 18:57:56 mike Exp $ */
/* Interface to monitor-locked heap memory allocator */

/* Initial version 2/1/98 by John Desrosiers (soho@crl.com) */

#if !defined(gsmemlok_INCLUDED)
 #define gsmemlok_INCLUDED

#include "gsmemory.h"
#include "gxsync.h"

/*
 * This allocator encapsulates another allocator with a mutex.
 * Note that it does not keep track of memory that it acquires:
 * thus free_all with FREE_ALL_DATA is a no-op.
 */

typedef struct gs_memory_locked_s {
    gs_memory_common;		/* interface outside world sees */
    gs_memory_t *target;	/* allocator to front */
    gx_monitor_t *monitor;	/* monitor to serialize access to functions */
} gs_memory_locked_t;

/* ---------- Public constructors/destructors ---------- */

/* Initialize a locked memory manager. */
int gs_memory_locked_init(P2(
			     gs_memory_locked_t * lmem,	/* allocator to init */
			     gs_memory_t * target	/* allocator to monitor lock */
			     ));

/* Release a locked memory manager. */
/* Note that this has no effect on the target. */
#define gs_memory_locked_release(lmem)\
  gs_memory_free_all(lmem, FREE_STRUCTURES, "gs_memory_locked_release")

/* Get the target of a locked memory manager. */
gs_memory_t * gs_memory_locked_target(P1(const gs_memory_locked_t *lmem));

#endif /*!defined(gsmemlok_INCLUDED) */
