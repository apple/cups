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

/* gxstate.h */
/* Internal graphics state API */

#ifndef gxstate_INCLUDED
#  define gxstate_INCLUDED

/* Opaque type for a graphics state */
#ifndef gs_state_DEFINED
#  define gs_state_DEFINED
typedef struct gs_state_s gs_state;
#endif

/*
 * The interfaces in this file are for internal use only, primarily by the
 * interpreter.  They are not guaranteed to remain stable from one release
 * to another.
 */

/* Memory and save/restore management */
gs_memory_t *gs_state_memory(P1(const gs_state *));
gs_state *gs_state_saved(P1(const gs_state *));
gs_state *gs_state_swap_saved(P2(gs_state *, gs_state *));
gs_memory_t *gs_state_swap_memory(P2(gs_state *, gs_memory_t *));

/* "Client data" interface for graphics states. */
typedef void *(*gs_state_alloc_proc_t)(P1(gs_memory_t *mem));
typedef int (*gs_state_copy_proc_t)(P2(void *to, const void *from));
typedef void (*gs_state_free_proc_t)(P2(void *old, gs_memory_t *mem));
typedef struct gs_state_client_procs_s {
	gs_state_alloc_proc_t alloc;
	gs_state_copy_proc_t copy;
	gs_state_free_proc_t free;
} gs_state_client_procs;
void	gs_state_set_client(P3(gs_state *, void *, const gs_state_client_procs *));
/* gzstate.h redefines the following: */
#ifndef gs_state_client_data
void *gs_state_client_data(P1(const gs_state *));
#endif

#endif					/* gxstate_INCLUDED */
