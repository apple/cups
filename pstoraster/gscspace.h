/* Copyright (C) 1991, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gscspace.h */
/* Client interface to color spaces */

#ifndef gscspace_INCLUDED
#  define gscspace_INCLUDED

/* Color space type indices */
typedef enum {
		/* Supported in all configurations */
	gs_color_space_index_DeviceGray = 0,
	gs_color_space_index_DeviceRGB,
		/* Supported in extended Level 1, and in Level 2 */
	gs_color_space_index_DeviceCMYK,
		/* Supported in Level 2 only */
		/* DEC C truncates identifiers at 32 characters, so.... */
	gs_color_space_index_CIEDEFG,
	gs_color_space_index_CIEDEF,
	gs_color_space_index_CIEABC,
	gs_color_space_index_CIEA,
	gs_color_space_index_Separation,
	gs_color_space_index_Indexed,
	gs_color_space_index_Pattern
} gs_color_space_index;

/* Define the abstract type for color space objects. */
#ifndef gs_color_space_DEFINED
#  define gs_color_space_DEFINED
typedef struct gs_color_space_s gs_color_space;
#endif

/*
 * We preallocate instances of the 3 device color spaces, and provide
 * procedures that return them (to avoid upsetting compilers that don't
 * allow extern pointers to abstract types).  Note that
 * gs_color_space_DeviceCMYK() may return NULL if CMYK color support is not
 * included in this configuration.
 */
extern const gs_color_space *gs_color_space_DeviceGray(P0());
extern const gs_color_space *gs_color_space_DeviceRGB(P0());
extern const gs_color_space *gs_color_space_DeviceCMYK(P0());

/*
 * Color spaces are complicated because different spaces involve
 * different kinds of parameters, as follows:

Space		Space parameters		Color parameters
-----		----------------		----------------
DeviceGray	(none)				1 real [0-1]
DeviceRGB	(none)				3 reals [0-1]
DeviceCMYK	(none)				4 reals [0-1]
CIEBasedDEFG	dictionary			4 reals
CIEBasedDEF	dictionary			3 reals
CIEBasedABC	dictionary			3 reals
CIEBasedA	dictionary			1 real
Separation	name, alt_space, tint_xform	1 real [0-1]
Indexed		base_space, hival, lookup	1 int [0-hival]
Pattern		colored: (none)			dictionary
		uncolored: base_space		dictionary + base space params

Space		Underlying or alternate space
-----		-----------------------------
Separation	Device, CIE
Indexed		Device, CIE
Pattern		Device, CIE, Separation, Indexed

 * Logically speaking, each color space type should be a different
 * structure type at the allocator level.  This would potentially require
 * either reference counting or garbage collector for color spaces, but that
 * is probably better than the current design, which uses fixed-size color
 * space objects and a second level of type discrimination.
 */

/* Define abstract structure types for color space parameters. */
typedef struct gs_cie_defg_s gs_cie_defg;
typedef struct gs_cie_def_s gs_cie_def;
typedef struct gs_cie_abc_s gs_cie_abc;
typedef struct gs_cie_a_s gs_cie_a;
typedef struct gs_separation_params_s gs_separation_params;
typedef struct gs_indexed_params_s gs_indexed_params;

/* Define an abstract type for color space types. */
typedef struct gs_color_space_type_s gs_color_space_type;

/* ---------------- Color spaces per se ---------------- */

	/* Base color spaces (Device and CIE) */

/*typedef struct gs_cie_defg_s gs_cie_defg;*/
/*typedef struct gs_cie_def_s gs_cie_def;*/
/*typedef struct gs_cie_abc_s gs_cie_abc;*/
/*typedef struct gs_cie_a_s gs_cie_a;*/
#define gs_base_cspace_params\
	gs_cie_defg *defg;\
	gs_cie_def *def;\
	gs_cie_abc *abc;\
	gs_cie_a *a
typedef struct gs_base_color_space_s {
	const gs_color_space_type _ds *type;
	union {
		gs_base_cspace_params;
	} params;
} gs_base_color_space;

	/* Paint (non-pattern) color spaces (base + Separation + Indexed) */

typedef ulong gs_separation_name;		/* BOGUS */

typedef struct gs_indexed_map_s gs_indexed_map;
/*typedef struct gs_separation_params_s gs_separation_params;*/
struct gs_separation_params_s {
	gs_separation_name sname;
	gs_base_color_space alt_space;
	gs_indexed_map *map;
};
/*typedef struct gs_indexed_params_s gs_indexed_params;*/
struct gs_indexed_params_s {
	gs_base_color_space base_space;
	int hival;
	union {
		gs_const_string table;	/* size is implicit */
		gs_indexed_map *map;
	} lookup;
	bool use_proc;		/* 0 = use table, 1 = use proc & map */
};
#define gs_paint_cspace_params\
	gs_base_cspace_params;\
	gs_separation_params separation;\
	gs_indexed_params indexed
typedef struct gs_paint_color_space_s {
	const gs_color_space_type _ds *type;
	union {
		gs_paint_cspace_params;
	} params;
} gs_paint_color_space;

	/* General color spaces (including patterns) */

typedef struct gs_pattern_params_s {
	bool has_base_space;
	gs_paint_color_space base_space;
} gs_pattern_params;
struct gs_color_space_s {
	const gs_color_space_type _ds *type;
	union {
		gs_paint_cspace_params;
		gs_pattern_params pattern;
	} params;
};
/*extern_st(st_color_space);*/		/* in gxcspace.h */
#define public_st_color_space()	/* in gscolor.c */\
  gs_public_st_composite(st_color_space, gs_color_space,\
    "gs_color_space", color_space_enum_ptrs, color_space_reloc_ptrs)
#define st_color_space_max_ptrs 2 /* 1 base + 1 indexed */

/* ---------------- Procedures ---------------- */

/* Get the index of a color space. */
gs_color_space_index gs_color_space_get_index(P1(const gs_color_space *));

/* Get the number of components in a color space. */
int gs_color_space_num_components(P1(const gs_color_space *));

/* Get the base space of an Indexed color space. */
/* We have to redefine this because the VAX VMS C compiler (but not */
/* the preprocessor!) limits identifiers to 31 characters. */
#define gs_color_space_indexed_base_space(pcs)\
  gs_color_space_indexed_base(pcs)
const gs_color_space *
  gs_color_space_indexed_base_space(P1(const gs_color_space *));

#endif					/* gscspace_INCLUDED */
