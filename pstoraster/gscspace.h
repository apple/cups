/* Copyright (C) 1991, 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gscspace.h,v 1.2 2000/03/08 23:14:39 mike Exp $ */
/* Client interface to color spaces */

#ifndef gscspace_INCLUDED
#  define gscspace_INCLUDED

#include "gsmemory.h"

/*
 * The handling of color spaces in the graphic library is somewhat
 * awkward because of historical artifacts.
 *
 * The PostScript Level 1 (DeviceGray/RGB) color spaces, and the "Level
 * 1 1/2" DeviceCMYK space, have no associated parameters.  Therefore,
 * they can be represented as simple objects (just a pointer to a type
 * vector containing procedures and parameters).  This was the original
 * design.
 *
 * PostScript Level 2 and LanguageLevel 3 add two new kinds of color spaces:
 * color spaces with parameters (CIEBased and DevicePixel spaces), and
 * compound color spaces (Indexed, Separation, Pattern, and DeviceN spaces),
 * which parameters that include specifying an alternate or underlying color
 * space.  To handle these spaces, we extended the original design to store
 * scalar parameters (i.e., parameters other than the complex color
 * transformation data for CIEBased spaces, the lookup table for Indexed
 * spaces, and the list of component names for DeviceN spaces) in-line in
 * the color space object.  For compound spaces, this requires storing a
 * color space in-line inside another color space, which is clearly
 * impossible.  Therefore, we defined a generality hierarchy for color
 * spaces:
 *
 *      - Base spaces (DeviceGray/RGB/CMYK/Pixel and CIEBased),
 *      whose parameters (if any) don't include other color spaces.
 *
 *	- Direct spaces (base spaces + Separation and DeviceN), which
 *	may have a base space as an alternative space.
 *
 *      - Paint spaces (direct spaces + Indexed), which may have a
 *      direct space as the underlying space.
 *
 *      - General spaces (paint spaces + Pattern), which may have a
 *      paint space as the underlying space.
 *
 * With this approach, a general space can include a paint space stored
 * in-line; a paint space (either in its own right or as the underlying
 * space of a Pattern space) can include a direct space in-line; and a
 * direct space can include a base space in-line.
 *
 * Note that because general, paint, direct, and base spaces are
 * (necessarily) of different sizes, assigning (copying the top object of)
 * color spaces must take into account the actual size of the color space
 * being assigned.  In principle, this also requires checking that the
 * source object will actually fit into the destination.  Currently we rely
 * on the caller to ensure that this is the case; in fact, the current API
 * (gs_cspace_init and gs_cspace_assign) doesn't even provide enough
 * information to make the check.
 *
 * In retrospect, we might have gotten a simpler design without significant
 * performance loss by always referencing underlying and alternate spaces
 * through a pointer; however, at this point, the cost and risk of changing
 * to such a design are too high.
 *
 * There are two aspects to memory management for color spaces: managing
 * the color space objects themselves, and managing the non-scalar
 * parameters that they reference (if any).
 *
 *      - Color space objects per se have no special management properties:
 *      they can be allocated on the stack or on the heap, and freed by
 *      scope exit, explicit deallocation, or garbage collection.
 *
 *      - Separately allocated (non-scalar) color space parameters are
 *      managed by reference counting.  Currently we do this for the
 *      CIEBased spaces, and for the first-level parameters of Indexed and
 *      Separation spaces: clients must deal with deallocating the other
 *      parameter structures mentioned above, including the Indexed lookup
 *      table if any. This is clearly not a good situation, but we don't
 *      envision fixing it any time soon.
 *
 * Here is the information associated with the various color space
 * structures.  Note that DevicePixel, DeviceN, and the ability to use
 * Separation or DeviceN spaces as the base space of an Indexed space
 * are LanguageLevel 3 additions.  Unfortunately, the terminology for the
 * different levels of generality is confusing and inconsistent for
 * historical reasons.
 *
 * For base spaces:
 *
 * Space        Space parameters                Color parameters
 * -----        ----------------                ----------------
 * DeviceGray   (none)                          1 real [0-1]
 * DeviceRGB    (none)                          3 reals [0-1]
 * DeviceCMYK   (none)                          4 reals [0-1]
 * DevicePixel  depth                           1 int [up to depth bits]
 * CIEBasedDEFG dictionary                      4 reals
 * CIEBasedDEF  dictionary                      3 reals
 * CIEBasedABC  dictionary                      3 reals
 * CIEBasedA    dictionary                      1 real
 *
 * For non-base direct spaces:
 *
 * Space        Space parameters                Color parameters
 * -----        ----------------                ----------------
 *
 * Separation   name, alt_space, tint_xform     1 real [0-1]
 * DeviceN      names, alt_space, tint_xform    N reals
 *
 * For non-direct paint spaces:
 *
 * Space        Space parameters                Color parameters
 * -----        ----------------                ----------------
 * Indexed      base_space, hival, lookup       1 int [0-hival]
 *
 * For non-paint spaces:
 *
 * Space        Space parameters                Color parameters
 * -----        ----------------                ----------------
 * Pattern      colored: (none)                 dictionary
 *              uncolored: base_space dictionary + base space params */

/* Color space type indices */
typedef enum {

    /* Supported in all configurations */
    gs_color_space_index_DeviceGray = 0,
    gs_color_space_index_DeviceRGB,

    /* Supported in extended Level 1, and in Level 2 and above */
    gs_color_space_index_DeviceCMYK,

    /* Supported in LanguageLevel 3 only */
    gs_color_space_index_DevicePixel,
    gs_color_space_index_DeviceN,

    /* Supported in Level 2 and above only */
    /* DEC C truncates identifiers at 32 characters, so.... */
    gs_color_space_index_CIEDEFG,
    gs_color_space_index_CIEDEF,
    gs_color_space_index_CIEABC,
    gs_color_space_index_CIEA,
    gs_color_space_index_Separation,
    gs_color_space_index_Indexed,
    gs_color_space_index_Pattern

} gs_color_space_index;

/* Define an abstract type for color space types (method structures). */
typedef struct gs_color_space_type_s gs_color_space_type;

/*
 * The common part of all color spaces. This structure now includes a memory
 * structure pointer, so that it may be released without providing this
 * information separately. (type is a pointer to the structure of methods.)
 *
 * Note that all color space structures consist of the basic information and
 * a union containing some additional information. The macro operand is that
 * union.
 */
#define gs_cspace_common(param_union)   \
    const gs_color_space_type * type;   \
    gs_memory_t *               pmem;   \
    union {                             \
	param_union;                    \
    }                           params

/*
 * Parameters for base color spaces. Of the base color spaces, only
 * DevicePixel and CIE spaces have parameters: see gscie.h for the structure
 * definitions for CIE space parameters.
 */
typedef struct gs_device_pixel_params_s {
    int depth;
} gs_device_pixel_params;
typedef struct gs_cie_a_s gs_cie_a;
typedef struct gs_cie_abc_s gs_cie_abc;
typedef struct gs_cie_def_s gs_cie_def;
typedef struct gs_cie_defg_s gs_cie_defg;

#define gs_base_cspace_params           \
    gs_device_pixel_params   pixel;     \
    gs_cie_defg *            defg;      \
    gs_cie_def *             def;       \
    gs_cie_abc *             abc;       \
    gs_cie_a *               a

typedef struct gs_base_color_space_s {
    gs_cspace_common(gs_base_cspace_params);
} gs_base_color_space;

#define gs_base_color_space_size sizeof(gs_base_color_space)

/*
 * Non-base direct color spaces: Separation and DeviceN spaces.
 * These include a base alternative color space.
 */
typedef ulong gs_separation_name;	/* BOGUS */
typedef struct gs_indexed_map_s gs_indexed_map;

typedef struct gs_separation_params_s {
    gs_separation_name sname;
    gs_base_color_space alt_space;
    gs_indexed_map *map;
} gs_separation_params;

typedef struct gs_device_n_params_s gs_device_n_params;
struct gs_device_n_params_s {
    gs_separation_name *names;
    uint num_components;
    gs_base_color_space alt_space;
    int (*tint_transform)
        (P4(const gs_device_n_params * params, const float *in, float *out,
	    void *data));
    void *tint_transform_data;
};

#define gs_direct_cspace_params         \
    gs_base_cspace_params;              \
    gs_separation_params separation;    \
    gs_device_n_params device_n

typedef struct gs_direct_color_space_s {
    gs_cspace_common(gs_direct_cspace_params);
} gs_direct_color_space;

#define gs_direct_color_space_size sizeof(gs_direct_color_space)

/*
 * Non-direct paint space: Indexed space.
 *
 * Note that for indexed color spaces, hival is the highest support index,
 * which is one less than the number of entries in the palette (as defined
 * in PostScript).
 */

typedef struct gs_indexed_params_s {
    gs_direct_color_space base_space;
    int hival;			/* num_entries - 1 */
    union {
	gs_const_string table;	/* size is implicit */
	gs_indexed_map *map;
    } lookup;
    bool use_proc;		/* 0 = use table, 1 = use proc & map */
} gs_indexed_params;

#define gs_paint_cspace_params          \
    gs_direct_cspace_params;            \
    gs_indexed_params indexed

typedef struct gs_paint_color_space_s {
    gs_cspace_common(gs_paint_cspace_params);
} gs_paint_color_space;

#define gs_paint_color_space_size sizeof(gs_paint_color_space)

/*
 * Pattern parameter set. This may contain an instances of a paintable
 * color space. The boolean indicates if this is the case.
 */
typedef struct gs_pattern_params_s {
    bool has_base_space;
    gs_paint_color_space base_space;
} gs_pattern_params;

/*
 * Fully general color spaces.
 */
struct gs_color_space_s {
    gs_cspace_common(
	gs_paint_cspace_params;
	gs_pattern_params pattern
    );
};

#define gs_pattern_color_space_size sizeof(gs_color_space)

/*
 * Define the abstract type for color space objects.
 */
#ifndef gs_color_space_DEFINED
#  define gs_color_space_DEFINED
typedef struct gs_color_space_s gs_color_space;
#endif

					/*extern_st(st_color_space); *//* in gxcspace.h */
#define public_st_color_space()	/* in gscspace.c */  \
    gs_public_st_composite( st_color_space,         \
                            gs_color_space,         \
                            "gs_color_space",       \
                            color_space_enum_ptrs,  \
                            color_space_reloc_ptrs  \
                            )

#define st_color_space_max_ptrs 2	/* 1 base + 1 indexed */

/* ---------------- Procedures ---------------- */

/* ------ Create/copy/destroy ------ */

/*
 * Note that many of the constructors take no parameters, and the
 * remainder take only a few (CIE color spaces constructures take a
 * client data pointer as an operand, the composite color space (Separation,
 * Indexed, and Pattern) constructurs take the base space as an operand,
 * and the Indexed color space constructors have a few additiona operands).
 * This is done to conserve memory. If initialization values for all the
 * color space parameters were provided to the constructors, these values
 * would need to have some fairly generic format. Different clients gather
 * this data in different forms, so they would need to allocate memory to
 * convert it to the generic form, only to immediately release this memory
 * once the construction is complete.
 *
 * The alternative approach employed here is to provide a number of access
 * methods (macros) that return pointers to individual fields of the
 * various color space structures. This requires exporting only fairly simple
 * data structures, and so does not violate modularity too severely.
 *
 * NB: Of necessity, the macros provide access to modifiable structures. If
 *     these structures are modified after the color space object is first
 *     initialized, unpredictable and, most likely, undesirable results will
 *     occur.
 *
 * The constructors will return an integer, which is 0 on success and an
 * error code on failure (gs_error_VMerror or gs_error_rangecheck).
 */

extern int
    gs_cspace_build_DeviceGray(P2(gs_color_space ** ppcspace,
				  gs_memory_t * pmem)),
    gs_cspace_build_DeviceRGB(P2(gs_color_space ** ppcspace,
				 gs_memory_t * pmem)),
    gs_cspace_build_DeviceCMYK(P2(gs_color_space ** ppcspace,
				  gs_memory_t * pmem));

/*
 * We preallocate instances of the 3 device color spaces, and provide
 * procedures that return them.  Note that gs_cspace_DeviceCMYK() is
 * defined even if CMYK color support is not included in this configuration.
 */
#ifndef gs_imager_state_DEFINED
#  define gs_imager_state_DEFINED
typedef struct gs_imager_state_s gs_imager_state;
#endif

const gs_color_space * gs_cspace_DeviceGray(P1(const gs_imager_state * pis));
const gs_color_space * gs_cspace_DeviceRGB(P1(const gs_imager_state * pis));
const gs_color_space * gs_cspace_DeviceCMYK(P1(const gs_imager_state * pis));

/* Copy a color space into one newly allocated by the caller. */
void gs_cspace_init_from(P2(gs_color_space * pcsto,
			    const gs_color_space * pcsfrom));

/* Assign a color space into a previously initialized one. */
void cs_cspace_assign(P2(gs_color_space * pdest, const gs_color_space * psrc));

/* Prepare to free a color space. */
void gs_cspace_release(P1(gs_color_space * pcs));

/* ------ Accessors ------ */

/* Get the index of a color space. */
gs_color_space_index gs_color_space_get_index(P1(const gs_color_space *));

/* Get the number of components in a color space. */
int gs_color_space_num_components(P1(const gs_color_space *));

/*
 * Get the base space of an Indexed or uncolored Pattern color space, or the
 * alternate space of a Separation or DeviceN space.  Return NULL if the
 * color space does not have a base/alternative color space.
 */
const gs_color_space *gs_cspace_base_space(P1(const gs_color_space * pcspace));

/* backwards compatibility */
#define gs_color_space_indexed_base_space(pcspace)\
    gs_cspace_base_space(pcspace)

#endif /* gscspace_INCLUDED */
