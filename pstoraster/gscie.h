/* Copyright (C) 1992, 1995, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gscie.h,v 1.2 2000/03/08 23:14:35 mike Exp $ */
/* Structures for CIE color algorithms */
/* (requires gscspace.h, gscolor2.h) */

#ifndef gscie_INCLUDED
#  define gscie_INCLUDED

#include "gsrefct.h"
#include "gsstruct.h"		/* for extern_st */
#include "gxctable.h"

/* ---------------- Configuration parameters ---------------- */

/* Define the size of the Encode/Decode/Transform procedure value caches. */
/* With the current design, these caches must all have the same size. */
#ifndef CIE_LOG2_CACHE_SIZE
#  define CIE_LOG2_CACHE_SIZE 9
#endif

/* Define whether to use fixed- or floating-point values in the caches. */
/*#define CIE_CACHE_USE_FIXED */

/* If we are using fixed-point values, define the number of fraction bits. */
#define CIE_FIXED_FRACTION_BITS 12

/* Define whether to interpolate between cached values. */
#define CIE_CACHE_INTERPOLATE

/* Define whether to interpolate at all intermediate lookup steps. */
/* This is computationally expensive and doesn't seem to improve */
/* the accuracy of the result. */
/*#define CIE_INTERPOLATE_INTERMEDIATE */

/* Define whether to interpolate in the RenderTable. */
/* This is computationally very expensive, so it is normally disabled. */
#define CIE_RENDER_TABLE_INTERPOLATE

/* ------ Derived values ------ */

/* from CIE_LOG2_CACHE_SIZE */
#define gx_cie_log2_cache_size CIE_LOG2_CACHE_SIZE
#define gx_cie_cache_size (1 << gx_cie_log2_cache_size)

/* From CIE_FIXED_FRACTION_BITS 12 */
#ifndef CIE_FIXED_FRACTION_BITS
/* Take as many bits as we can without having to multiply in two pieces. */
#  define CIE_FIXED_FRACTION_BITS\
     ((arch_sizeof_long * 8 - gx_cie_log2_cache_size) / 2 - 1)
#endif

/* From CIE_RENDER_TABLE_INTERPOLATE */
#ifdef CIE_RENDER_TABLE_INTERPOLATE
#  define CIE_CACHE_INTERPOLATE
#endif

#define float_lshift(v, nb) ((v) * (1L << (nb)))
#define float_rshift(v, nb) ((v) * (1.0 / (1L << (nb))))

#ifdef CIE_CACHE_INTERPOLATE
/* We have to have room for both a cache index and the interpolation bits */
/* in a positive int (i.e., leaving 1 bit for the sign), plus a little slop. */
/* The values for interpolation are cie_cached_values by default. */
#  define _cie_interpolate_bits\
     min(arch_sizeof_int * 8 - gx_cie_log2_cache_size - 2, 10)
#  define _cix(i) ((i) >> _cie_interpolate_bits)
#  define _cif(i) ((int)(i) & ((1 << _cie_interpolate_bits) - 1))
#  define cie_interpolate_between(v0, v1, i)\
     ((v0) + cie_cached_rshift(((v1) - (v0)) * _cif(i) +\
			        (1 << (_cie_interpolate_bits - 1)),\
			       _cie_interpolate_bits))
#  define cie_interpolate(p, i)\
     cie_interpolate_between((p)[_cix(i)], (p)[_cix(i) + 1], i)
#  define cie_interpolate_fracs(p, i)\
     ((p)[_cix(i)] + (frac)arith_rshift((long)((p)[_cix(i) + 1] - (p)[_cix(i)]) * _cif(i), _cie_interpolate_bits))
#else
#  define _cie_interpolate_bits 0
#  define cie_interpolate_between(v0, v1, i) (v0)
#  define cie_interpolate(p, i) ((p)[i])
#  define cie_interpolate_fracs(p, i) ((p)[i])
#endif

#ifdef CIE_CACHE_USE_FIXED
typedef long cie_cached_value;

#  define _cie_fixed_shift CIE_FIXED_FRACTION_BITS
#  define float2cie_cached(v)\
     ((cie_cached_value)float_lshift(v, _cie_fixed_shift))
#  define cie_cached2float(v)\
     float_rshift(v, _cie_fixed_shift)
#  define cie_cached2int(v, fbits)\
     arith_rshift(v, _cie_fixed_shift - (fbits))
/* We are multiplying two cie_cached_values to produce a result that */
/* lies between 0 and gx_cie_cache_size - 1.  If the intermediate result */
/* might overflow, compute it in pieces (being a little sloppy). */
#  define _cie_product_excess_bits\
     (_cie_fixed_shift * 2 + gx_cie_log2_cache_size - (arch_sizeof_long * 8 - 1))
#  define cie_cached_product2int(v, factor, fbits)\
     (_cie_product_excess_bits > 0 ?\
      arith_rshift( (v) * arith_rshift(factor, _cie_product_excess_bits) +\
		    arith_rshift(v, _cie_product_excess_bits) *\
		     ((factor) & ((1 << _cie_product_excess_bits) - 1)),\
		    _cie_fixed_shift * 2 - _cie_product_excess_bits - (fbits)) :\
      arith_rshift((v) * (factor), _cie_fixed_shift * 2 - (fbits)))
#  define cie_cached_rshift(v, n) arith_rshift(v, n)
#else
typedef float cie_cached_value;
#  define float2cie_cached(v) (v)
#  define cie_cached2float(v) (v)
#  define cie_cached2int(v, fbits)\
     ((int)float_lshift(v, fbits))
#  define cie_cached_product2int(v, factor, fbits)\
     ((int)float_lshift((v) * (factor), fbits))
#  define cie_cached_rshift(v, n) float_rshift(v, n)
#endif

/* ---------------- Structures ---------------- */

#ifndef gs_cie_render_DEFINED
#  define gs_cie_render_DEFINED
typedef struct gs_cie_render_s gs_cie_render;
#endif

/* ------ Common definitions ------ */

/*
 * For the purposes of the CIE routines, we consider that all the vectors
 * are column vectors, that the matrices are specified in column order
 * (e.g., the matrix
 *      [ A B C ]
 *      [ D E F ]
 *      [ G H I ]
 * is represented as [A D G B E H C F I]), and that to transform a vector
 * V by a matrix M, we compute M * V to produce another column vector.
 * Note in particular that in order to produce a matrix M that is
 * equivalent to transforming by M1 and then by M2, we must compute
 * M = M2 * M1.
 */

/* A 3-element vector. */
typedef struct gs_vector3_s {
    float u, v, w;
} gs_vector3;

/* A 3x3 matrix, stored in column order. */
typedef struct gs_matrix3_s {
    gs_vector3 cu, cv, cw;
    bool is_identity;
} gs_matrix3;

/* 3- and 4-element vectors of ranges. */
typedef struct gs_range_s {
    float rmin, rmax;
} gs_range;
typedef struct gs_range3_s {
    gs_range ranges[3];
} gs_range3;
typedef struct gs_range4_s {
    gs_range ranges[4];
} gs_range4;

/* Client-supplied transformation procedures. */
typedef struct gs_cie_common_s gs_cie_common;
typedef struct gs_cie_wbsd_s gs_cie_wbsd;

typedef float (*gs_cie_a_proc) (P2(floatp, const gs_cie_a *));

typedef float (*gs_cie_abc_proc) (P2(floatp, const gs_cie_abc *));
typedef struct gs_cie_abc_proc3_s {
    gs_cie_abc_proc procs[3];
} gs_cie_abc_proc3;

typedef float (*gs_cie_def_proc) (P2(floatp, const gs_cie_def *));
typedef struct gs_cie_def_proc3_s {
    gs_cie_def_proc procs[3];
} gs_cie_def_proc3;

typedef float (*gs_cie_defg_proc) (P2(floatp, const gs_cie_defg *));
typedef struct gs_cie_defg_proc4_s {
    gs_cie_defg_proc procs[4];
} gs_cie_defg_proc4;

typedef float (*gs_cie_common_proc) (P2(floatp, const gs_cie_common *));
typedef struct gs_cie_common_proc3_s {
    gs_cie_common_proc procs[3];
} gs_cie_common_proc3;

typedef float (*gs_cie_render_proc) (P2(floatp, const gs_cie_render *));
typedef struct gs_cie_render_proc3_s {
    gs_cie_render_proc procs[3];
} gs_cie_render_proc3;

/*
 * The TransformPQR procedure depends on both the color space and the
 * CRD, so we can't simply pass it through the band list as a table of
 * sampled values, even though such a table exists as part of an
 * internal cache.  Instead, we use two different approaches.  The
 * graphics library knows that the cache must be reloaded whenever the
 * color space or CRD changes, so we can simply transmit the cached
 * values through the band list whenever this occurs.  However, this
 * still leaves the issue of how to represent the procedure in the CRD
 * per se: such a representation is required in order for
 * currentcolorrendering and setcolorrendering to work.  For this
 * purpose, we provide a procedure name and procedure data, which
 * drivers can supply with their default CRDs; the driver must also be
 * prepared to map the procedure name back to an actual set of
 * procedures.
 *
 * To simplify the driver-provided CRD machinery, we define TransformPQR as
 * a single procedure taking an integer that specifies the component number,
 * rather than an array of procedures.  Note that if proc_name != 0,
 * proc is irrelevant -- the driver will provide it by looking up proc_name.
 * For this reason, the last argument of TransformPQR must be writable.
 * Note also that since TransformPQR can fail (if the driver doesn't
 * recognize the proc_name), it must return a failure code.
 */
typedef int (*gs_cie_transform_proc)(P5(int, floatp, const gs_cie_wbsd *,
					gs_cie_render *, float *));
typedef struct gs_cie_transform_proc3_s {
    gs_cie_transform_proc proc;
    const char *proc_name;
    gs_const_string proc_data;
    const char *driver_name;	/* for mapping proc_name back to procs */
} gs_cie_transform_proc3;

typedef frac(*gs_cie_render_table_proc) (P2(byte, const gs_cie_render *));
typedef struct gs_cie_render_table_procs_s {
    gs_cie_render_table_proc procs[4];
} gs_cie_render_table_procs;

/* CIE white and black points. */
typedef struct gs_cie_wb_s {
    gs_vector3 WhitePoint;
    gs_vector3 BlackPoint;
} gs_cie_wb;

/* ------ Caches ------ */

/*
 * Given that all the client-supplied procedures involved in CIE color
 * mapping and rendering are monotonic, and given that we can determine
 * the minimum and maximum input values for them, we can cache their values.
 * This takes quite a lot of space, but eliminates the need for callbacks
 * deep in the graphics code (particularly the image operator).
 *
 * The procedures, and how we determine their domains, are as follows:

 Stage          Name            Domain determination
 -----          ----            --------------------
 pre-decode     DecodeDEF       RangeDEF
 pre-decode     DecodeDEFG      RangeDEFG
 color space    DecodeA         RangeA
 color space    DecodeABC       RangeABC
 color space    DecodeLMN       RangeLMN
 rendering      TransformPQR    RangePQR
 (but depends on color space White/BlackPoints)
 rendering      EncodeLMN       RangePQR transformed by the inverse of
				MatrixPQR and then by MatrixLMN
 rendering      EncodeABC       RangeLMN transformed by MatrixABC
 rendering      RenderTable.T   [0..1]*m

 * Note that we can mostly cache the results of the color space procedures
 * without knowing the color rendering parameters, and vice versa,
 * because of the range parameters supplied in the dictionaries.
 * Unfortunately, TransformPQR is an exception.
 */
/*
 * The index into a cache is (value - base) * factor, where
 * factor is computed as (cie_cache_size - 1) / (rmax - rmin).
 */
/*
 * We have two kinds of caches: ordinary caches, where each value is
 * a scalar, and vector caches, where each value is a gs_cached_vector3.
 * The latter allow us to pre-multiply the values by one column of
 * a gs_matrix3, avoiding multiplications at lookup time.
 * Since we sometimes alias the two types of caches for access to
 * the floats, values must come last.
 */
typedef struct cie_cache_params_s {
    bool is_identity;		/* must come first */
    float base, factor;
} cie_cache_params;
typedef struct cie_cache_floats_s {
    cie_cache_params params;
    float values[gx_cie_cache_size];
} cie_cache_floats;
typedef struct cie_cache_fracs_s {
    cie_cache_params params;
    frac values[gx_cie_cache_size];
} cie_cache_fracs;
typedef struct cie_cache_ints_s {
    cie_cache_params params;
    int values[gx_cie_cache_size];
} cie_cache_ints;
typedef union gx_cie_scalar_cache_s {
    cie_cache_floats floats;
    cie_cache_fracs fracs;
    cie_cache_ints ints;
} gx_cie_scalar_cache;

typedef struct cie_cached_vector3_s {
    cie_cached_value u, v, w;
} cie_cached_vector3;
typedef struct cie_vector_cache_params_s {
    bool is_identity;		/* must come first */
    cie_cached_value base, factor, limit;
} cie_vector_cache_params;
typedef struct cie_cache_vectors_s {
    cie_vector_cache_params params;	/* must come first for is_identity */
    cie_cached_vector3 values[gx_cie_cache_size];
} cie_cache_vectors;
typedef union gx_cie_vector_cache_s {
    cie_cache_floats floats;
    cie_cache_vectors vecs;
} gx_cie_vector_cache;

/* ------ Color space dictionaries ------ */

/* Elements common to all CIE color space dictionaries. */
struct gs_cie_common_s {
    int (*install_cspace) (P2(gs_color_space *, gs_state *));
    void *client_data;
    gs_range3 RangeLMN;
    gs_cie_common_proc3 DecodeLMN;
    gs_matrix3 MatrixLMN;
    gs_cie_wb points;
    /* Following are computed when structure is initialized. */
    struct {
	gx_cie_scalar_cache DecodeLMN[3];
    } caches;
};

#define private_st_cie_common()     /* in gscscie.c */\
  gs_private_st_ptrs1(st_cie_common, gs_cie_common, "gs_cie_common",\
		      cie_common_enum_ptrs, cie_common_reloc_ptrs, client_data)

#define gs_cie_common_elements\
	gs_cie_common common;		/* must be first */\
	rc_header rc
typedef struct gs_cie_common_elements_s {
    gs_cie_common_elements;
} gs_cie_common_elements_t;

#define private_st_cie_common_elements() /* in gscscie.c */ \
  gs_private_st_suffix_add0_local(st_cie_common_elements_t,\
				  gs_cie_common_elements_t,\
				  "gs_cie_common_elements_t",\
				  cie_common_enum_ptrs,\
				  cie_common_reloc_ptrs,\
				  st_cie_common)

/* A CIEBasedA dictionary. */
struct gs_cie_a_s {
    gs_cie_common_elements;	/* must be first */
    gs_range RangeA;
    gs_cie_a_proc DecodeA;
    gs_vector3 MatrixA;
    /* Following are computed when structure is initialized. */
    struct {
	gx_cie_vector_cache DecodeA;	/* mult. by MatrixA */
    } caches;
};

#define private_st_cie_a()	/* in gscscie.c */\
  gs_private_st_suffix_add0_local(st_cie_a, gs_cie_a, "gs_cie_a",\
				  cie_common_enum_ptrs,\
				  cie_common_reloc_ptrs,\
				  st_cie_common_elements_t)

/* Common elements for CIEBasedABC, DEF, and DEFG dictionaries. */
#define gs_cie_abc_elements\
	gs_cie_common_elements;		/* must be first */\
	gs_range3 RangeABC;\
	gs_cie_abc_proc3 DecodeABC;\
	gs_matrix3 MatrixABC;\
		/* Following are computed when structure is initialized. */\
	struct {\
		bool skipABC;\
		gx_cie_vector_cache DecodeABC[3];  /* mult. by MatrixABC */\
	} caches

/* A CIEBasedABC dictionary. */
struct gs_cie_abc_s {
    gs_cie_abc_elements;
};

#define private_st_cie_abc()	/* in gscscie.c */\
  gs_private_st_suffix_add0_local(st_cie_abc, gs_cie_abc, "gs_cie_abc",\
				  cie_common_enum_ptrs, cie_common_reloc_ptrs,\
				  st_cie_common_elements_t)

/* A CIEBasedDEF dictionary. */
struct gs_cie_def_s {
    gs_cie_abc_elements;	/* must be first */
    gs_range3 RangeDEF;
    gs_cie_def_proc3 DecodeDEF;
    gs_range3 RangeHIJ;
    gx_color_lookup_table Table;	/* [NH][NI * NJ * 3] */
    struct {
	gx_cie_scalar_cache DecodeDEF[3];
    } caches_def;
};

#define private_st_cie_def()	/* in gscscie.c */\
  gs_private_st_suffix_add1(st_cie_def, gs_cie_def, "gs_cie_def",\
                            cie_def_enum_ptrs, cie_def_reloc_ptrs,\
                            st_cie_abc, Table.table)

/* A CIEBasedDEFG dictionary. */
struct gs_cie_defg_s {
    gs_cie_abc_elements;
    gs_range4 RangeDEFG;
    gs_cie_defg_proc4 DecodeDEFG;
    gs_range4 RangeHIJK;
    gx_color_lookup_table Table;	/* [NH * NI][NJ * NK * 3] */
    struct {
	gx_cie_scalar_cache DecodeDEFG[4];
    } caches_defg;
};

#define private_st_cie_defg()	/* in gscscie.c */\
  gs_private_st_suffix_add1(st_cie_defg, gs_cie_defg, "gs_cie_defg",\
			    cie_defg_enum_ptrs, cie_defg_reloc_ptrs,\
			    st_cie_abc, Table.table)

/*
 * Default values for components.  Note that for some components, there are
 * two sets of default procedures: _default (identity procedures) and
 * _from_cache (procedures that just return the cached values).  Currently
 * we only provide the latter for the Encode elements of the CRD.
 */
extern const gs_range3 Range3_default;
extern const gs_range4 Range4_default;
extern const gs_cie_defg_proc4 DecodeDEFG_default;
extern const gs_cie_def_proc3 DecodeDEF_default;
extern const gs_cie_abc_proc3 DecodeABC_default;
extern const gs_cie_common_proc3 DecodeLMN_default;
extern const gs_matrix3 Matrix3_default;
extern const gs_range RangeA_default;
extern const gs_cie_a_proc DecodeA_default;
extern const gs_vector3 MatrixA_default;
extern const gs_vector3 BlackPoint_default;
extern const gs_cie_render_proc3 Encode_default;
extern const gs_cie_render_proc3 EncodeLMN_from_cache;
extern const gs_cie_render_proc3 EncodeABC_from_cache;
extern const gs_cie_transform_proc3 TransformPQR_default;
extern const gs_cie_transform_proc TransformPQR_lookup_proc_name;
extern const gs_cie_render_table_procs RenderTableT_default;
extern const gs_cie_render_table_procs RenderTableT_from_cache;

/* ------ Rendering dictionaries ------ */

struct gs_cie_wbsd_s {
    struct {
	gs_vector3 xyz, pqr;
    } ws, bs, wd, bd;
};
typedef struct gs_cie_render_table_s {
    /*
     * If lookup.table == 0, the other members (of both lookup and T) are
     * not set.  If not 0, lookup.table points to an array of
     * st_const_string_elements.
     */
    gx_color_lookup_table lookup;
    gs_cie_render_table_procs T;
} gs_cie_render_table_t;
typedef enum {
    CIE_RENDER_STATUS_BUILT,
    CIE_RENDER_STATUS_INITED,
    CIE_RENDER_STATUS_SAMPLED,
    CIE_RENDER_STATUS_COMPLETED
} cie_render_status_t;

/* The main dictionary */
struct gs_cie_render_s {
    cie_render_status_t status;
    rc_header rc;
    void *client_data;
    gs_cie_wb points;
    gs_matrix3 MatrixPQR;
    gs_range3 RangePQR;
    gs_cie_transform_proc3 TransformPQR;
    gs_matrix3 MatrixLMN;
    gs_cie_render_proc3 EncodeLMN;
    gs_range3 RangeLMN;
    gs_matrix3 MatrixABC;
    gs_cie_render_proc3 EncodeABC;
    gs_range3 RangeABC;
    gs_cie_render_table_t RenderTable;
    /* Following are computed when structure is initialized. */
    gs_range3 DomainLMN;
    gs_range3 DomainABC;
    gs_matrix3 MatrixABCEncode;
    cie_cached_value EncodeABC_base[3];
    gs_matrix3 MatrixPQR_inverse_LMN;
    gs_vector3 wdpqr, bdpqr;
    struct {
	gx_cie_vector_cache EncodeLMN[3];	/* mult. by M'ABCEncode */
	gx_cie_scalar_cache EncodeABC[3];
	gx_cie_scalar_cache RenderTableT[4];
	bool RenderTableT_is_identity;
    } caches;
};

/* The CRD type is public only for a type test in zcrd.c. */
extern_st(st_cie_render1);
#define public_st_cie_render1()	/* in gscrd.c */\
  gs_public_st_composite(st_cie_render1, gs_cie_render, "gs_cie_render",\
			 cie_render1_enum_ptrs, cie_render1_reloc_ptrs)

/* ------ Joint caches ------ */

/* This cache depends on both the color space and the rendering */
/* dictionary -- see above. */

typedef struct gx_cie_joint_caches_s {
    rc_header rc;
    bool skipLMN;
    gx_cie_vector_cache DecodeLMN[3];	/* mult. by dLMN_PQR */
    gs_cie_wbsd points_sd;
    gs_matrix3 MatrixLMN_PQR;
    bool skipPQR;
    gx_cie_vector_cache TransformPQR[3];	/* mult. by PQR_inverse_LMN */
} gx_cie_joint_caches;

#define private_st_joint_caches() /* in gscie.c */\
  gs_private_st_simple(st_joint_caches, gx_cie_joint_caches,\
    "gx_cie_joint_caches")

/* ------ Internal procedures ------ */

typedef struct gs_for_loop_params_s {
    float init, step, limit;
} gs_for_loop_params;
void gs_cie_cache_init(P4(cie_cache_params *, gs_for_loop_params *,
			  const gs_range *, client_name_t));
void gs_cie_cache_to_fracs(P1(gx_cie_scalar_cache *));
void gs_cie_defg_complete(P1(gs_cie_defg *));
void gs_cie_def_complete(P1(gs_cie_def *));
void gs_cie_abc_complete(P1(gs_cie_abc *));
void gs_cie_a_complete(P1(gs_cie_a *));
gx_cie_joint_caches *gx_currentciecaches(P1(gs_state *));
const gs_cie_common *gs_cie_cs_common(P1(gs_state *));
int gs_cie_cs_complete(P2(gs_state *, bool));

/*
 * Compute the source and destination WhitePoint and BlackPoint for
 * the TransformPQR procedure.
 */
int gs_cie_compute_wbsd(P4(gs_cie_wbsd * pwbsd,
			   const gs_vector3 * cs_WhitePoint,
			   const gs_vector3 * cs_BlackPoint,
			   const gs_cie_render * pcrd));

/*
 * Compute the derived values in a CRD that don't involve the cached
 * procedure values, moving the CRD from "built" to "inited" status.
 * If the CRD is already in "inited" or a later status, do nothing.
 */
int gs_cie_render_init(P1(gs_cie_render *));

/*
 * Sample the EncodeLMN, EncodeABC, and RenderTableT CRD procedures, and
 * load the caches, moving the CRD from "inited" to "sampled" status.
 * If the CRD is already in "sampled" or a later status, do nothing;
 * otherwise, if the CRD is not in "inited" status, return an error.
 */
int gs_cie_render_sample(P1(gs_cie_render *));

/*
 * Finish preparing a CRD for installation, by restricting and/or
 * transforming the cached procedure values, moving the CRD from "sampled"
 * to "completed" status.  If the CRD is already in "completed" status, do
 * nothing; otherwise, if the CRD is not in "sampled" status, return an
 * error.
 */
int gs_cie_render_complete(P1(gs_cie_render *));

/* ---------------- Procedures ---------------- */

/* ------ Constructors ------ */

/*
 * Note that these procedures take a client_data pointer as an operand. The
 * client is responsible for allocating and deleting this object; the
 * color space machinery does not take ownership of it.
 *
 * Note that these procedures set the reference count of the (large)
 * parameter structures to 1, not 0.  gs_setcolorspace will increment
 * the reference count again, so unless you want the parameter structures
 * to stay allocated permanently (or until a garbage collection),
 * you should call cs_adjust_count(pcspace, -1).  THIS IS A BUG IN THE API.
 */
extern int
    gs_cspace_build_CIEA(P3(gs_color_space ** ppcspace, void *client_data,
			    gs_memory_t * pmem)),
    gs_cspace_build_CIEABC(P3(gs_color_space ** ppcspace, void *client_data,
			      gs_memory_t * pmem)),
    gs_cspace_build_CIEDEF(P3(gs_color_space ** ppcspace, void *client_data,
			      gs_memory_t * pmem)),
    gs_cspace_build_CIEDEFG(P3(gs_color_space ** ppcspace, void *client_data,
			       gs_memory_t * pmem));

/* ------ Accessors ------ */

/*
 * Note that the accessors depend heavily on "puns" between the variants
 * of pcspace->params.{a,abc,def,defg}.
 */

/* Generic CIE based color space parameters */
#define gs_cie_RangeLMN(pcspace)  (&(pcspace)->params.a->common.RangeLMN)
#define gs_cie_DecodeLMN(pcspace) (&(pcspace)->params.a->common.DecodeLMN)
#define gs_cie_MatrixLMN(pcspace) (&(pcspace)->params.a->common.MatrixLMN)
#define gs_cie_WhitePoint(pcspace)\
  ((pcspace)->params.a->common.points.WhitePoint)
#define gs_cie_BlackPoint(pcspace)\
  ((pcspace)->params.a->common.points.BlackPoint)

/* CIEBasedA color space */
#define gs_cie_a_RangeA(pcspace)      (&(pcspace)->params.a->RangeA)
#define gs_cie_a_DecodeA(pcspace)     (&(pcspace)->params.a->DecodeA)
#define gs_cie_a_MatrixA(pcspace)     (&(pcspace)->params.a->MatrixA)
#define gs_cie_a_RangeA(pcspace)      (&(pcspace)->params.a->RangeA)

/* CIEBasedABC color space */
/* Note that these also work for CIEBasedDEF[G] spaces. */
#define gs_cie_abc_RangeABC(pcspace)    (&(pcspace)->params.abc->RangeABC)
#define gs_cie_abc_DecodeABC(pcspace)   (&(pcspace)->params.abc->DecodeABC)
#define gs_cie_abc_MatrixABC(pcspace)   (&(pcspace)->params.abc->MatrixABC)

/* CIDBasedDEF color space */
#define gs_cie_def_RangeDEF(pcspace)    (&(pcspace)->params.def->RangeDEF)
#define gs_cie_def_DecodeDEF(pcspace)   (&(pcspace)->params.def->DecodeDEF)
#define gs_cie_def_RangeHIJ(pcspace)    (&(pcspace)->params.def->RangeHIJ)

/* CIDBasedDEFG color space */
#define gs_cie_defg_RangeDEFG(pcspace)  (&(pcspace)->params.defg->RangeDEFG)
#define gs_cie_defg_DecodeDEFG(pcspace) (&(pcspace)->params.defg->DecodeDEFG)
#define gs_cie_defg_RangeHIJK(pcspace)  (&(pcspace)->params.defg->RangeHIJK)

/*
 * The following routine is provided so as to avoid explicitly exporting the
 * CIEBasedDEF[G] color lookup table structure. It is doubtful any
 * high-level clients will ever need to get this information.
 *
 * The caller must make sure the number of dimensions and strings provided
 * are the number expected given the number of components in the color space.
 * The procedure gs_color_space_num_components is available for this purpose.
 *
 * For a 3 component color space (CIEBasedDEF), ptable points to an array of
 * pdims[0] gs_const_string structures, each of which is of length
 * 3 * pdims[1] * pdims[2].
 *
 * For a 4 component color space (CIEBasedDEFG), ptable points to an array of
 * pdims[0] * pdims[1] strings, each of which is of length 
 * 3 * pdims[2] * pdims[3].
 *
 * NB: the caller is responsible for deallocating the color table data
 *     when no longer needed.  */
extern int
    gs_cie_defx_set_lookup_table(P3(gs_color_space * pcspace, int *pdims,
				    const gs_const_string * ptable));

#endif /* gscie_INCLUDED */
