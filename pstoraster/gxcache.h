/* Copyright (C) 1993 Aladdin Enterprises.  All rights reserved.
  
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

/* gxcache.h */
/* General-purpose cache schema for Ghostscript */

#ifndef gxcache_INCLUDED
#  define gxcache_INCLUDED

/*
 * Ghostscript caches a wide variety of information internally:
 * *font/matrix pairs, *scaled fonts, rendered characters,
 * binary halftones, *colored halftones, *Patterns,
 * and the results of many procedures (transfer functions, undercolor removal,
 * black generation, CIE color transformations).
 * The caches marked with * all use a similar structure: a chained hash
 * table with a maximum number of entries allocated in a single block,
 * and a roving pointer for purging.
 */
#define cache_members(entry_type, hash_size)\
	uint csize, cmax;\
	uint cnext;\
	uint esize;		/* for generic operations */\
	entry_type *entries;\
	entry_type *hash[hash_size];
#define cache_init(pcache)\
  (memset((char *)(pcache)->hash, 0, sizeof((pcache)->hash)),\
   (pcache)->esize = sizeof(*(pcache)->entries),\
   (pcache)->entries = 0,\
   (pcache)->csize = (pcache)->cmax = (pcache)->cnext = 0)
/*
 * The following operations should be generic, but can't be because
 * C doesn't provide templates: allocate, look up, add, purge at 'restore'.
 */

#endif					/* gxcache_INCLUDED */
