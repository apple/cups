/*
  Copyright 1993-2000 by Easy Software Products
  Copyright 1995, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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
#include <config.h>
#ifdef HAVE_LIBZ

/*$Id: szlibc.c,v 1.5 2000/06/22 20:33:31 mike Exp $ */
/* Code common to zlib encoding and decoding streams */
#include "std.h"
#include "gserror.h"
#include "gserrors.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gsmalloc.h"
#include "gsstruct.h"
#include "strimpl.h"
#include "szlibxx.h"
#include "zconf.h"

private_st_zlib_block();
private_st_zlib_dynamic_state();
public_st_zlib_state();

/* Set defaults for stream parameters. */
void
s_zlib_set_defaults(stream_state * st)
{
    stream_zlib_state *const ss = (stream_zlib_state *)st;

    ss->windowBits = MAX_WBITS;
    ss->no_wrapper = false;
    ss->level = Z_DEFAULT_COMPRESSION;
    ss->method = Z_DEFLATED;
    /* DEF_MEM_LEVEL should be in zlib.h or zconf.h, but it isn't. */
    ss->memLevel = min(MAX_MEM_LEVEL, 8);
    ss->strategy = Z_DEFAULT_STRATEGY;
}

/* Allocate the dynamic state. */
int
s_zlib_alloc_dynamic_state(stream_zlib_state *ss)
{
    gs_memory_t *mem = (ss->memory ? ss->memory : &gs_memory_default);
    zlib_dynamic_state_t *zds =
	gs_alloc_struct_immovable(mem, zlib_dynamic_state_t,
				  &st_zlib_dynamic_state,
				  "s_zlib_alloc_dynamic_state");

    ss->dynamic = zds;
    if (zds == 0)
	return_error(gs_error_VMerror);
    zds->blocks = 0;
    zds->memory = mem;
    zds->zstate.zalloc = (alloc_func)s_zlib_alloc;
    zds->zstate.zfree = (free_func)s_zlib_free;
    zds->zstate.opaque = (voidpf)zds;
    return 0;
}

/* Free the dynamic state. */
void
s_zlib_free_dynamic_state(stream_zlib_state *ss)
{
    if (ss->dynamic)
	gs_free_object(ss->dynamic->memory, ss->dynamic,
		       "s_zlib_free_dynamic_state");
}

/* Provide zlib-compatible allocation and freeing functions. */
void *
s_zlib_alloc(void *zmem, uint items, uint size)
{
    zlib_dynamic_state_t *const zds = zmem;
    gs_memory_t *mem = zds->memory;
    zlib_block_t *block =
	gs_alloc_struct(mem, zlib_block_t, &st_zlib_block,
			"s_zlib_alloc(block)");
    void *data =
	gs_alloc_byte_array_immovable(mem, items, size, "s_zlib_alloc(data)");

    if (block == 0 || data == 0) {
	gs_free_object(mem, data, "s_zlib_alloc(data)");
	gs_free_object(mem, block, "s_zlib_alloc(block)");
	return Z_NULL;
    }
    block->data = data;
    block->next = zds->blocks;
    block->prev = 0;
    if (zds->blocks)
	zds->blocks->prev = block;
    zds->blocks = block;
    return data;
}
void
s_zlib_free(void *zmem, void *data)
{
    zlib_dynamic_state_t *const zds = zmem;
    gs_memory_t *mem = zds->memory;
    zlib_block_t *block = zds->blocks;

    gs_free_object(mem, data, "s_zlib_free(data)");
    for (; ; block = block->next) {
	if (block == 0) {
	    lprintf1("Freeing unrecorded data 0x%lx!\n", (ulong)data);
	    return;
	}
	if (block->data == data)
	    break;
    }
    if (block->next)
	block->next->prev = block->prev;
    if (block->prev)
	block->prev->next = block->next;
    else
	zds->blocks = block->next;
    gs_free_object(mem, block, "s_zlib_free(block)");
}
#endif
