/* Copyright (C) 1995, 1996, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gsropt.h,v 1.2 2000/03/08 23:14:47 mike Exp $ */
/* RasterOp / transparency type definitions */

#ifndef gsropt_INCLUDED
#  define gsropt_INCLUDED

/*
 * This file defines the types for some library extensions that are
 * motivated by PCL5 and also made available for PostScript:
 * RasterOp, source and pattern white-pixel transparency, and
 * per-pixel "render algorithm" information.
 */

/*
 * By the magic of Boolean algebra, we can operate on the rop codes using
 * Boolean operators and get the right result.  E.g., the value of
 * (rop3_S & rop3_D) is the rop3 code for S & D.  We just have to remember
 * to mask results with rop2_1 or rop3_1 if necessary.
 */

/* 2-input RasterOp */
typedef enum {
    rop2_0 = 0,
    rop2_S = 0xc,		/* source */
#define rop2_S_shift 2
    rop2_D = 0xa,		/* destination */
#define rop2_D_shift 1
    rop2_1 = 0xf,
#define rop2_operand(shift, d, s)\
  ((shift) == 2 ? (s) : (d))
    rop2_default = rop2_S
} gs_rop2_t;

/*
 * For the 3-input case, we follow H-P's inconsistent terminology:
 * the transparency mode is called pattern transparency, but the third
 * RasterOp operand is called texture, not pattern.
 */

/* 3-input RasterOp */
typedef enum {
    rop3_0 = 0,
    rop3_T = 0xf0,		/* texture */
#define rop3_T_shift 4
    rop3_S = 0xcc,		/* source */
#define rop3_S_shift 2
    rop3_D = 0xaa,		/* destination */
#define rop3_D_shift 1
    rop3_1 = 0xff,
    rop3_default = rop3_T | rop3_S
} gs_rop3_t;

/* All the transformations on rop3s are designed so that */
/* they can also be used on lops.  The only place this costs anything */
/* is in rop3_invert. */

/*
 * Invert an operand.
 */
#define rop3_invert_(op, mask, shift)\
  ( (((op) & mask) >> shift) | (((op) & (rop3_1 - mask)) << shift) |\
    ((op) & ~rop3_1) )
#define rop3_invert_D(op) rop3_invert_(op, rop3_D, rop3_D_shift)
#define rop3_invert_S(op) rop3_invert_(op, rop3_S, rop3_S_shift)
#define rop3_invert_T(op) rop3_invert_(op, rop3_T, rop3_T_shift)
/*
 * Pin an operand to 0.
 */
#define rop3_know_0_(op, mask, shift)\
  ( (((op) & (rop3_1 - mask)) << shift) | ((op) & ~mask) )
#define rop3_know_D_0(op) rop3_know_0_(op, rop3_D, rop3_D_shift)
#define rop3_know_S_0(op) rop3_know_0_(op, rop3_S, rop3_S_shift)
#define rop3_know_T_0(op) rop3_know_0_(op, rop3_T, rop3_T_shift)
/*
 * Pin an operand to 1.
 */
#define rop3_know_1_(op, mask, shift)\
  ( (((op) & mask) >> shift) | ((op) & ~(rop3_1 - mask)) )
#define rop3_know_D_1(op) rop3_know_1_(op, rop3_D, rop3_D_shift)
#define rop3_know_S_1(op) rop3_know_1_(op, rop3_S, rop3_S_shift)
#define rop3_know_T_1(op) rop3_know_1_(op, rop3_T, rop3_T_shift)
/*
 * Swap S and T.
 */
#define rop3_swap_S_T(op)\
  ( (((op) & rop3_S & ~rop3_T) << (rop3_T_shift - rop3_S_shift)) |\
    (((op) & ~rop3_S & rop3_T) >> (rop3_T_shift - rop3_S_shift)) |\
    ((op) & (~rop3_1 | (rop3_S ^ rop3_T))) )
/*
 * Account for transparency.
 */
#define rop3_use_D_when_0_(op, mask)\
  (((op) & ~(rop3_1 - mask)) | (rop3_D & ~mask))
#define rop3_use_D_when_1_(op, mask)\
  (((op) & ~mask) | (rop3_D & mask))
#define rop3_use_D_when_S_0(op) rop3_use_D_when_0_(op, rop3_S)
#define rop3_use_D_when_S_1(op) rop3_use_D_when_1_(op, rop3_S)
#define rop3_use_D_when_T_0(op) rop3_use_D_when_0_(op, rop3_T)
#define rop3_use_D_when_T_1(op) rop3_use_D_when_1_(op, rop3_T)
/*
 * Invert the result.
 */
#define rop3_not(op) ((op) ^ rop3_1)
/*
 * Test whether an operand is used.
 */
#define rop3_uses_(op, mask, shift)\
  ( ((((op) << shift) ^ (op)) & mask) != 0 )
#define rop3_uses_D(op) rop3_uses_(op, rop3_D, rop3_D_shift)
#define rop3_uses_S(op) rop3_uses_(op, rop3_S, rop3_S_shift)
#define rop3_uses_T(op) rop3_uses_(op, rop3_T, rop3_T_shift)
/*
 * Test whether an operation is idempotent, i.e., whether
 * f(D, S, T) = f(f(D, S, T), S, T).  This is equivalent to the condition that
 * for all values s and t, !( f(0,s,t) == 1 && f(1,s,t) == 0 ).
 */
#define rop3_is_idempotent(op)\
  !( (op) & ~((op) << rop3_D_shift) & rop3_D )

/* Transparency */
#define source_transparent_default false
#define pattern_transparent_default false

/*
 * We define a logical operation as a RasterOp, transparency flags,
 * and render algorithm all packed into a single integer.
 * In principle, we should use a structure, but most C implementations
 * implement structure values very inefficiently.
 */
#define lop_rop(lop) ((gs_rop3_t)((lop) & 0xff))	/* must be low-order bits */
#define lop_S_transparent 0x100
#define lop_T_transparent 0x200
#define lop_ral_shift 10
#define lop_ral_mask 0xf
typedef uint gs_logical_operation_t;

#define lop_default\
  (rop3_default |\
   (source_transparent_default ? lop_S_transparent : 0) |\
   (pattern_transparent_default ? lop_T_transparent : 0))

     /* Test whether a logical operation uses S or T. */
#define lop_uses_S(lop)\
  (rop3_uses_S(lop) || ((lop) & lop_S_transparent))
#define lop_uses_T(lop)\
  (rop3_uses_T(lop) || ((lop) & lop_T_transparent))
/* Test whether a logical operation just sets D = x if y = 0. */
#define lop_no_T_is_S(lop)\
  (((lop) & (lop_S_transparent | (rop3_1 - rop3_T))) == (rop3_S & ~rop3_T))
#define lop_no_S_is_T(lop)\
  (((lop) & (lop_T_transparent | (rop3_1 - rop3_S))) == (rop3_T & ~rop3_S))
/* Test whether a logical operation is idempotent. */
#define lop_is_idempotent(lop) rop3_is_idempotent(lop)

/* Define the interface to the table of 256 RasterOp procedures. */
typedef unsigned rop_operand;
typedef rop_operand(*rop_proc) (P3(rop_operand D, rop_operand S, rop_operand T));

/* Define the table of operand usage by the 256 RasterOp operations. */
typedef enum {
    rop_usage_none = 0,
    rop_usage_D = 1,
    rop_usage_S = 2,
    rop_usage_DS = 3,
    rop_usage_T = 4,
    rop_usage_DT = 5,
    rop_usage_ST = 6,
    rop_usage_DST = 7
} rop_usage_t;

#endif /* gsropt_INCLUDED */
