/* Copyright (C) 1990, 1993, 1994, 1996 Aladdin Enterprises.  All rights reserved.
  
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

#ifndef gxarith_INCLUDED
#  define gxarith_INCLUDED

/*$Id: gxarith.h,v 1.2 2000/03/08 23:14:50 mike Exp $ */
/* Arithmetic macros for Ghostscript library */

/* Define an in-line abs function, good for any signed numeric type. */
#define any_abs(x) ((x) < 0 ? -(x) : (x))

/* Compute M modulo N.  Requires N > 0; guarantees 0 <= imod(M,N) < N, */
/* regardless of the whims of the % operator for negative operands. */
int imod(P2(int m, int n));

/* Compute the GCD of two integers. */
int igcd(P2(int x, int y));

/* Test whether an integral value fits in a given number of bits. */
/* This works for all integral types. */
#define fits_in_bits(i, n)\
  (sizeof(i) <= sizeof(int) ? fits_in_ubits((i) + (1 << ((n) - 1)), (n) + 1) :\
   fits_in_ubits((i) + (1L << ((n) - 1)), (n) + 1))
#define fits_in_ubits(i, n) (((i) >> (n)) == 0)

/*
 * There are some floating point operations that can be implemented
 * very efficiently on machines that have no floating point hardware,
 * assuming IEEE representation and no range overflows.
 * We define straightforward versions of them here, and alternate versions
 * for no-floating-point machines in gxfarith.h.
 */
/* Test floating point values against constants. */
#define is_fzero(f) ((f) == 0.0)
#define is_fzero2(f1,f2) ((f1) == 0.0 && (f2) == 0.0)
#define is_fneg(f) ((f) < 0.0)
#define is_fge1(f) ((f) >= 1.0)
/* Test whether a floating point value fits in a given number of bits. */
#define f_fits_in_bits(f, n)\
  ((f) >= -2.0 * (1L << ((n) - 2)) && (f) < 2.0 * (1L << ((n) - 2)))
#define f_fits_in_ubits(f, n)\
  ((f) >= 0 && (f) < 4.0 * (1L << ((n) - 2)))

/*
 * Define a macro for computing log2(n), where n=1,2,4,...,128.
 * Because some compilers limit the total size of a statement,
 * this macro must only mention n once.  The macro should really
 * only be used with compile-time constant arguments, but it will work
 * even if n is an expression computed at run-time.
 */
#define small_exact_log2(n)\
 ((uint)(05637042010L >> ((((n) % 11) - 1) * 3)) & 7)

/*
 * The following doesn't give rise to a macro, but is used in several
 * places in Ghostscript.  We observe that if M = 2^n-1 and V < M^2,
 * then the quotient Q and remainder R can be computed as:
 *              Q = V / M = (V + (V >> n) + 1) >> n;
 *              R = V % M = (V + (V / M)) & M = V - (Q << n) + Q.
 */

#endif /* gxarith_INCLUDED */
