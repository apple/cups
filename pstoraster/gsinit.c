/* Copyright (C) 1989, 1995, 1996, 1997 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gsinit.c,v 1.2 2000/03/08 23:14:43 mike Exp $ */
/* Initialization for the imager */
#include "stdio_.h"
#include "memory_.h"
#include "gdebug.h"
#include "gscdefs.h"
#include "gsmemory.h"
#include "gsmalloc.h"
#include "gp.h"
#include "gslib.h"		/* interface definition */

/* Imported from gsmisc.c */
extern FILE *gs_debug_out;

/* Configuration information from gconfig.c. */
extern_gx_init_table();

/* Initialization to be done before anything else. */
void
gs_lib_init(FILE * debug_out)
{
    gs_lib_init1(gs_lib_init0(debug_out));
}
gs_memory_t *
gs_lib_init0(FILE * debug_out)
{
    gs_memory_t *mem;

    gs_debug_out = debug_out;
    mem = (gs_memory_t *) gs_malloc_init();
    /* Reset debugging flags */
    memset(gs_debug, 0, 128);
    gs_log_errors = 0;
    return mem;
}
void
gs_lib_init1(gs_memory_t * mem)
{				/* Run configuration-specific initialization procedures. */
    {
	void (*const *ipp) (P1(gs_memory_t *));

	for (ipp = gx_init_table; *ipp != 0; ++ipp)
	    (**ipp) (mem);
    }
}

/* Clean up after execution. */
void
gs_lib_finit(int exit_status, int code)
{
    fflush(stderr);		/* in case of error exit */
    /* Do platform-specific cleanup. */
    gp_exit(exit_status, code);
    gs_malloc_release();
}
