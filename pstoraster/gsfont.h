/* Copyright (C) 1989, 1993, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gsfont.h */
/* Client interface to font operations */
/* Requires gsmatrix.h */

/* A 'font directory' object (to avoid making fonts global). */
/* 'directory' is something of a misnomer: this structure */
/* just keeps track of the defined fonts, and the scaled font and */
/* rendered character caches. */
#ifndef gs_font_dir_DEFINED
#  define gs_font_dir_DEFINED	
typedef struct gs_font_dir_s gs_font_dir;
#endif

/* Font objects */
#ifndef gs_font_DEFINED
#  define gs_font_DEFINED
typedef struct gs_font_s gs_font;
#endif

/* Initialization */
/* These procedures return 0 if they fail. */
gs_font_dir	*gs_font_dir_alloc(P1(gs_memory_t *));
gs_font_dir	*gs_font_dir_alloc_limits(P6(gs_memory_t *,
			uint /*smax*/, uint /*bmax*/, uint /*mmax*/,
			uint /*cmax*/, uint /*upper*/));

/* Font manipulations */
/* Use gs_definefont only with original (unscaled) fonts! */
int gs_definefont(P2(gs_font_dir *, gs_font *));
/* gs_scalefont and gs_makefont return 0 if the scaled font */
/* was already in the cache, 1 if a new font was created. */
int	gs_scalefont(P4(gs_font_dir *, const gs_font *, floatp, gs_font **));
int	gs_makefont(P4(gs_font_dir *, const gs_font *, const gs_matrix *, gs_font **));
int	gs_setfont(P2(gs_state *, gs_font *));
gs_font *	gs_currentfont(P1(const gs_state *));
gs_font *	gs_rootfont(P1(const gs_state *));
void	gs_purge_font(P1(gs_font *));

/* Font cache parameter operations */
void	gs_cachestatus(P2(const gs_font_dir *, uint [7]));
#define gs_setcachelimit(pdir,limit) gs_setcacheupper(pdir,limit)
uint	gs_currentcachesize(P1(const gs_font_dir *));
int	gs_setcachesize(P2(gs_font_dir *, uint));
uint	gs_currentcachelower(P1(const gs_font_dir *));
int	gs_setcachelower(P2(gs_font_dir *, uint));
uint	gs_currentcacheupper(P1(const gs_font_dir *));
int	gs_setcacheupper(P2(gs_font_dir *, uint));
