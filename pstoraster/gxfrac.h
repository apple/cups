/* Copyright (C) 1992, 1993 Aladdin Enterprises.  All rights reserved.
  
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

/* gxfrac.h */
/* Fraction representation for Ghostscript */

#ifndef gxfrac_INCLUDED
#  define gxfrac_INCLUDED

/*
 * Represent a fraction in [0.0..1.0].
 * Note that the 1.0 endpoint is included.
 * Since undercolor removal requires a signed frac,
 * we limit fracs to 15 bits rather than 16.
 */
typedef short frac;
typedef short signed_frac;
#define arch_log2_sizeof_frac arch_log2_sizeof_short
#define arch_sizeof_frac arch_sizeof_short
#define frac_bits 15
#define frac_0 ((frac)0)
/* The following definition of frac_1 allows exact representation of */
/* almost all common fractions (e.g., N/360 for 0<=N<=360). */
#define frac_1_0bits 3
#define frac_1 ((frac)0x7ff8)
#define frac_1_long ((long)frac_1)
#define frac_1_float ((float)frac_1)
/* Conversion between fracs and floats. */
#define frac2float(fr) ((fr) / frac_1_float)
#define float2frac(fl) ((frac)(((fl) + 0.5 / frac_1_float) * frac_1_float))

/*
 * Conversion between unsigned fracs and bytes (or, in general,
 * shorter integers) representing fractions. This is highly dependent
 * on the definition of frac_1 above.
 */
#define _frac2s(fr)\
  (((fr) >> (frac_bits - frac_1_0bits)) + (fr))
#define frac2bits(fr, nb)\
  ((uint)(_frac2s(fr) >> (frac_bits - (nb))))
#define frac2byte(fr) ((byte)frac2bits(fr, 8))
/* bits2frac requires frac_bits / 2 <= nb <= frac_bits. */
#define bits2frac(v, nb) ((frac)(\
  ((frac)(v) << (frac_bits - (nb))) +\
   ((v) >> ((nb) * 2 - frac_bits)) -\
   ((v) >> ((nb) - frac_1_0bits)) ))
#define byte2frac(b) bits2frac(b, 8)
/* Produce a result that is guaranteed to convert back to a frac */
/* not exceeding the original value fr. */
#define frac2bits_floor(fr, nb)\
  ((uint)((_frac2s(fr) - (_frac2s(fr) >> (nb))) >> (frac_bits - (nb))))
/*
 * Conversion between fracs and unsigned shorts.
 */
#define ushort_bits (arch_sizeof_short * 8)
#define frac2ushort(fr) ((ushort)(\
  ((fr) << (ushort_bits - frac_bits)) +\
  ((fr) >> (frac_bits * 2 - ushort_bits - frac_1_0bits)) ))
#define ushort2frac(us) ((frac)(\
  ((us) >> (ushort_bits - frac_bits)) -\
  ((us) >> (ushort_bits - frac_1_0bits)) ))
/*
 * Compute the quotient Q = floor(P / frac_1),
 * where P is the (ulong) product of a uint or ushort V and a frac F.
 * See gxarith.h for the underlying algorithm.
 */
#define frac_1_quo(p)\
  ( (((p) >> frac_1_0bits) + ((p) >> frac_bits) + 1) >> (frac_bits - frac_1_0bits) )
/*
 * Compute the remainder similarly, having already computed the quotient.
 * This is, of course, P - Q * frac_1.
 */
#define frac_1_rem(p, q)\
  ((frac)( (uint)(p) - ((q) << frac_bits) + ((q) << frac_1_0bits) ))

#endif					/* gxfrac_INCLUDED */
