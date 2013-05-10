/* Copyright (C) 1992, 1995 Aladdin Enterprises.  All rights reserved.
  
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

/* gscie.h */
/* Structures for CIE color algorithms */
/* (requires gscspace.h, gscolor2.h) */
#include "gsrefct.h"
#include "gxctable.h"

/* Define the size of the Encode/Decode/Transform procedure value caches. */
/* With the current design, these caches must all have the same size. */
#ifndef CIE_LOG2_CACHE_SIZE
#  define CIE_LOG2_CACHE_SIZE 9
#endif
#define gx_cie_log2_cache_size CIE_LOG2_CACHE_SIZE
#define gx_cie_cache_size (1 << gx_cie_log2_cache_size)

/* Define whether to use fixed- or floating-point values in the caches. */
/*#define CIE_CACHE_USE_FIXED*/

/* If we are using fixed-point values, define the number of fraction bits. */
#define CIE_FIXED_FRACTION_BITS 12
#ifndef CIE_FIXED_FRACTION_BITS
/* Take as many bits as we can without having to multiply in two pieces. */
#  define CIE_FIXED_FRACTION_BITS\
     ((arch_sizeof_long * 8 - gx_cie_log2_cache_size) / 2 - 1)
#endif

/* Define whether to interpolate between cached values. */
#define CIE_CACHE_INTERPOLATE

/* Define whether to interpolate at all intermediate lookup steps. */
/* This is computationally expensive and doesn't seem to improve */
/* the accuracy of the result. */
/*#define CIE_INTERPOLATE_INTERMEDIATE*/

/* Define whether to interpolate in the RenderTable. */
/* This is computationally very expensive, so it is normally disabled. */
#define CIE_RENDER_TABLE_INTERPOLATE
#ifdef CIE_RENDER_TABLE_INTERPOLATE
#  define CIE_CACHE_INTERPOLATE
#endif

/* Mark code intended for later use. */
/****** NOTE: this is also used in zcie.c. ******/
/*#define NEW_CIE*/

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

/* ------ Common definitions ------ */

/*
 * For the purposes of the CIE routines, we consider that all the vectors
 * are column vectors, that the matrices are specified in column order
 * (e.g., the matrix
 *	[ A B C ]
 *	[ D E F ]
 *	[ G H I ]
 * is represented as [A D G B E H C F I]), and that to transform a vector
 * V by a matrix M, we compute M * V to produce another column vector.
 * Note in particular that in order to produce a matrix M that is
 * equivalent to transforming by M1 and then by M2, we must compute
 * M = M2 * M1.  This probably isn't the most intuitive way to specify
 * these things, but that's how the code turned out, and it isn't worth
 * changing at this point.
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
#ifdef NEW_CIE
typedef struct gs_cie_abc_common_s gs_cie_abc_common;
#else
typedef struct gs_cie_abc_s gs_cie_abc_common;
#endif
typedef struct gs_cie_wbsd_s gs_cie_wbsd;

typedef float (*gs_cie_a_proc)(P2(floatp, const gs_cie_a *));

typedef float (*gs_cie_abc_proc)(P2(floatp, const gs_cie_abc *));
typedef struct gs_cie_abc_proc3_s {
  gs_cie_abc_proc procs[3];
} gs_cie_abc_proc3;

typedef float (*gs_cie_def_proc)(P2(floatp, const gs_cie_def *));
typedef struct gs_cie_def_proc3_s {
  gs_cie_def_proc procs[3];
} gs_cie_def_proc3;

typedef float (*gs_cie_defg_proc)(P2(floatp, const gs_cie_defg *));
typedef struct gs_cie_defg_proc4_s {
  gs_cie_defg_proc procs[4];
} gs_cie_defg_proc4;

typedef float (*gs_cie_common_proc)(P2(floatp, const gs_cie_common *));
typedef struct gs_cie_common_proc3_s {
  gs_cie_common_proc procs[3];
} gs_cie_common_proc3;

typedef float (*gs_cie_render_proc)(P2(floatp, const gs_cie_render *));
typedef struct gs_cie_render_proc3_s {
  gs_cie_render_proc procs[3];
} gs_cie_render_proc3;

typedef float (*gs_cie_transform_proc)(P3(floatp, const gs_cie_wbsd *,
  const gs_cie_render *));
typedef struct gs_cie_transform_proc3_s {
  gs_cie_transform_proc procs[3];
} gs_cie_transform_proc3;

typedef frac (*gs_cie_render_table_proc)(P2(byte, const gs_cie_render *));
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

Stage		Name		Domain determination
-----		----		--------------------
pre-decode	DecodeDEF	RangeDEF
pre-decode	DecodeDEFG	RangeDEFG
color space	DecodeA		RangeA
color space	DecodeABC	RangeABC
color space	DecodeLMN	RangeLMN
rendering	TransformPQR	RangePQR
  (but depends on color space White/BlackPoints)
rendering	EncodeLMN	RangePQR transformed by the inverse of
				  MatrixPQR and then by MatrixLMN
rendering	EncodeABC	RangeLMN transformed by MatrixABC
rendering	RenderTable.T	[0..1]*m

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
#define cie_cache_struct(sname, vtype)\
  struct sname {\
	cie_cache_params params;\
	vtype values[gx_cie_cache_size];\
  }
typedef cie_cache_struct(gx_cie_cache_s, float) cie_cache_floats;
typedef union gx_cie_scalar_cache_s {
	cie_cache_floats floats;
	cie_cache_struct(_scf, frac) fracs;
	cie_cache_struct(_sci, int) ints;
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

/* Elements common to all CIE dictionaries. */
struct gs_cie_common_s {
	gs_range3 RangeLMN;
	gs_cie_common_proc3 DecodeLMN;
	gs_matrix3 MatrixLMN;
	gs_cie_wb points;
		/* Following are computed when structure is initialized. */
	struct {
		gx_cie_scalar_cache DecodeLMN[3];
	} caches;
};

/* A CIEBasedA dictionary. */
struct gs_cie_a_s {
	gs_cie_common common;		/* must be first */
	rc_header rc;
	gs_range RangeA;
	gs_cie_a_proc DecodeA;
	gs_vector3 MatrixA;
		/* Following are computed when structure is initialized. */
	struct {
		gx_cie_vector_cache DecodeA;  /* mult. by MatrixA */
	} caches;
};
#define private_st_cie_a()	/* in zcie.c */\
  gs_private_st_simple(st_cie_a, gs_cie_a, "gs_cie_a")

/* A CIEBasedABC dictionary. */
#ifdef NEW_CIE
struct gs_cie_abc_common_s {
	gs_cie_common common;		/* must be first */
#else
struct gs_cie_abc_s {
	gs_cie_common common;		/* must be first */
	rc_header rc;
#endif
	gs_range3 RangeABC;
	gs_cie_abc_proc3 DecodeABC;
	gs_matrix3 MatrixABC;
		/* Following are computed when structure is initialized. */
	struct {
		bool skipABC;
		gx_cie_vector_cache DecodeABC[3];  /* mult. by MatrixABC */
	} caches;
};
#ifdef NEW_CIE
/* A CIEBasedABC dictionary. */
struct gs_cie_abc_s {
	gs_cie_abc_common abc;		/* must be first */
	rc_header rc;
};
#endif
#define private_st_cie_abc()	/* in zcie.c */\
  gs_private_st_simple(st_cie_abc, gs_cie_abc, "gs_cie_abc")

/* A CIEBasedDEF dictionary. */
/****** NOT IMPLEMENTED YET ******/
struct gs_cie_def_s {
	gs_cie_abc_common abc;			/* must be first */
#ifndef NEW_CIE
	rc_header rc;
#endif
	gs_range3 RangeDEF;
	gs_cie_def_proc3 DecodeDEF;
	gs_range3 RangeHIJ;
	gx_color_lookup_table Table;		/* [NH][NI * NJ * 3] */
	struct {
		gx_cie_scalar_cache DecodeDEF[3];
	} caches;
};
#define private_st_cie_def()	/* in zcie.c */\
  gs_private_st_ptrs1(st_cie_def, gs_cie_def, "gs_cie_def",\
    cie_def_enum_ptrs, cie_def_reloc_ptrs, Table.table)

/* A CIEBasedDEFG dictionary. */
/****** NOT IMPLEMENTED YET ******/
struct gs_cie_defg_s {
	gs_cie_abc_common abc;			/* must be first */
#ifndef NEW_CIE
	rc_header rc;
#endif
	gs_range4 RangeDEFG;
	gs_cie_defg_proc4 DecodeDEFG;
	gs_range4 RangeHIJK;
	gx_color_lookup_table Table;		/* [NH * NI][NJ * NK * 3] */
	struct {
		gx_cie_scalar_cache DecodeDEFG[4];
	} caches;
};
#define private_st_cie_defg()	/* in zcie.c */\
  gs_private_st_ptrs1(st_cie_defg, gs_cie_defg, "gs_cie_defg",\
    cie_defg_enum_ptrs, cie_defg_reloc_ptrs, Table.table)

/* Default values for components */
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
extern const gs_cie_transform_proc3 TransformPQR_default;
extern const gs_cie_render_table_procs RenderTableT_default;

/* ------ Rendering dictionaries ------ */

struct gs_cie_wbsd_s {
	struct { gs_vector3 xyz, pqr; } ws, bs, wd, bd;
};
/* The main dictionary */
struct gs_cie_render_s {
	rc_header rc;
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
	struct {
		gx_color_lookup_table lookup;	/* if table is 0, other */
						/* members are not set */
		gs_cie_render_table_procs T;
	} RenderTable;
		/* Following are computed when structure is initialized. */
	gs_range3 DomainLMN;
	gs_range3 DomainABC;
	gs_matrix3 MatrixABCEncode;
	cie_cached_value EncodeABC_base[3];
	gs_matrix3 MatrixPQR_inverse_LMN;
	gs_vector3 wdpqr, bdpqr;
	struct {
		gx_cie_vector_cache EncodeLMN[3];  /* mult. by M'ABCEncode */
		gx_cie_scalar_cache EncodeABC[3];
		gx_cie_scalar_cache RenderTableT[4];
		bool RenderTableT_is_identity;
	} caches;
};
#define private_st_cie_render()	/* in zcrd.c */\
  gs_private_st_ptrs1(st_cie_render, gs_cie_render, "gs_cie_render",\
    cie_render_enum_ptrs, cie_render_reloc_ptrs, RenderTable.lookup.table)
/* RenderTable.lookup.table points to an array of st_const_string_elements. */
#define private_st_const_string()	/* in gscie.c */\
  gs_private_st_composite(st_const_string, gs_const_string, "gs_const_string",\
    const_string_enum_ptrs, const_string_reloc_ptrs)
extern_st(st_const_string_element);
#define public_st_const_string_element()	/* in gscie.c */\
  gs_public_st_element(st_const_string_element, gs_const_string,\
    "gs_const_string[]", const_string_elt_enum_ptrs,\
    const_string_elt_reloc_ptrs, st_const_string)

/* ------ Joint caches ------ */

/* This cache depends on both the color space and the rendering */
/* dictionary -- see above. */

typedef struct gx_cie_joint_caches_s {
	rc_header rc;
	bool skipLMN;
	gx_cie_vector_cache DecodeLMN[3];  /* mult. by dLMN_PQR */
	gs_cie_wbsd points_sd;
	gs_matrix3 MatrixLMN_PQR;
	bool skipPQR;
	gx_cie_vector_cache TransformPQR[3];  /* mult. by PQR_inverse_eLMN */
} gx_cie_joint_caches;
#define private_st_joint_caches() /* in gscie.c */\
  gs_private_st_simple(st_joint_caches, gx_cie_joint_caches,\
    "gx_cie_joint_caches")

/* Internal routines */
typedef struct gs_for_loop_params_s {
	float init, step, limit;
} gs_for_loop_params;
void gs_cie_cache_init(P4(cie_cache_params *, gs_for_loop_params *,
  const gs_range *, client_name_t));
void gs_cie_cache_to_fracs(P1(gx_cie_scalar_cache *));
void gs_cie_abc_complete(P1(gs_cie_abc *));
void gs_cie_a_complete(P1(gs_cie_a *));
int gs_cie_render_init(P1(gs_cie_render *));
int gs_cie_render_complete(P1(gs_cie_render *));
gx_cie_joint_caches *gx_currentciecaches(P1(gs_state *));
const gs_cie_common *gs_cie_cs_common(P1(gs_state *));
void gs_cie_cs_complete(P2(gs_state *, bool));
