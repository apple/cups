/* Copyright (C) 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gsmdebug.h */
/* Debugging definitions for memory manager */
/* Requires gdebug.h (for gs_debug) */

/* Define the fill patterns used for debugging the allocator. */
extern byte
  gs_alloc_fill_alloc,		/* allocated but not initialized */
  gs_alloc_fill_block,		/* locally allocated block */
  gs_alloc_fill_collected,	/* garbage collected */
  gs_alloc_fill_deleted,	/* locally deleted block */
  gs_alloc_fill_free;		/* freed */

/* Define an alias for a specialized debugging flag */
/* that used to be a separate variable. */
#define gs_alloc_debug gs_debug['@']

/* Conditionally fill unoccupied blocks with a pattern. */
extern void gs_alloc_memset(P3(void *, int/*byte*/, ulong));
#ifdef DEBUG
/* The following peculiar syntax avoids incorrect capture of an 'else'. */
#  define gs_alloc_fill(ptr, fill, len)\
     do { if ( gs_alloc_debug ) gs_alloc_memset(ptr, fill, (ulong)(len));\
     } while ( 0 )
#else
#  define gs_alloc_fill(ptr, fill, len)\
     DO_NOTHING
#endif
