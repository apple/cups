/* Copyright (C) 1993, 1994 Aladdin Enterprises.  All rights reserved.
  
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

/* gsht.h */
/* Public interface to halftone functionality */

#ifndef gsht_INCLUDED
#  define gsht_INCLUDED

/* Client definition of (Type 1) halftones */
typedef struct gs_screen_halftone_s {
	float frequency;
	float angle;
	float (*spot_function)(P2(floatp, floatp));
		/* setscreen or sethalftone sets these: */
		/* (a Level 2 feature, but we include them in Level 1) */
	float actual_frequency;
	float actual_angle;
} gs_screen_halftone;
#define st_screen_halftone_max_ptrs 0

/* Client definition of color (Type 2) halftones */
typedef struct gs_colorscreen_halftone_s {
	union _css {
		gs_screen_halftone indexed[4];
		struct _csc {
			gs_screen_halftone red, green, blue, gray;
		} colored;
	} screens;
} gs_colorscreen_halftone;
#define st_colorscreen_halftone_max_ptrs 0

/* Opaque definition of general halftones */
#ifndef gs_halftone_DEFINED
#  define gs_halftone_DEFINED
typedef struct gs_halftone_s gs_halftone;
#endif

/* Procedural interface */
int gs_setscreen(P2(gs_state *, gs_screen_halftone *));
int gs_currentscreen(P2(const gs_state *, gs_screen_halftone *));
int gs_currentscreenlevels(P1(const gs_state *));

/*
 * Enumeration-style definition of a single screen.  The client must:
 *	- probably, call gs_screen_enum_alloc;
 *	- call gs_screen_init;
 *	- in a loop,
 *		- call gs_screen_currentpoint; if it returns 1, exit;
 *		- call gs_screen_next;
 *	- if desired, call gs_screen_install to install the screen.
 */
typedef struct gs_screen_enum_s gs_screen_enum;
gs_screen_enum *gs_screen_enum_alloc(P2(gs_memory_t *, client_name_t));
int gs_screen_init(P3(gs_screen_enum *, gs_state *,
		      gs_screen_halftone *));
int gs_screen_currentpoint(P2(gs_screen_enum *, gs_point *));
int gs_screen_next(P2(gs_screen_enum *, floatp));
int gs_screen_install(P1(gs_screen_enum *));

#endif					/* gsht_INCLUDED */
