/* Copyright (C) 1989, 1992, 1993, 1994, 1995 Aladdin Enterprises.  All rights reserved.
  
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

/* std.h */
/* Standard definitions for Aladdin Enterprises code */

#ifndef std_INCLUDED
#  define std_INCLUDED

#include "stdpre.h"

/* Include the architecture definitions. */
#include "arch.h"

/* Define integer data type sizes in terms of log2s. */
#define arch_sizeof_short (1 << arch_log2_sizeof_short)
#define arch_sizeof_int (1 << arch_log2_sizeof_int)
#define arch_sizeof_long (1 << arch_log2_sizeof_long)
#define arch_ints_are_short (arch_sizeof_int == arch_sizeof_short)

/* Define whether we are on a large- or small-memory machine. */
/* Currently, we assume small memory and 16-bit ints are synonymous. */
#define arch_small_memory (arch_sizeof_int <= 2)

/* Define unsigned 16- and 32-bit types.  These are needed in */
/* a surprising number of places that do bit manipulation. */
#if arch_sizeof_short == 2	/* no plausible alternative! */
typedef ushort bits16;
#endif
#if arch_sizeof_int == 4
typedef uint bits32;
#else
# if arch_sizeof_long == 4
typedef ulong bits32;
# endif
#endif

/* Minimum and maximum values for the signed types. */
/* Avoid casts, to make them acceptable to strict ANSI compilers. */
#define min_short (-1 << (arch_sizeof_short * 8 - 1))
#define max_short (~min_short)
#define min_int (-1 << (arch_sizeof_int * 8 - 1))
#define max_int (~min_int)
#define min_long (-1L << (arch_sizeof_long * 8 - 1))
#define max_long (~min_long)

/*
 * The maximum values for the unsigned types are defined in arch.h,
 * because so many compilers handle unsigned constants wrong.
 * In particular, most of the DEC VMS compilers incorrectly sign-extend
 * short unsigned constants (but not short unsigned variables) when
 * widening them to longs.  We program around this on a case-by-case basis.
 * Some compilers don't truncate constants when they are cast down.
 * The UTek compiler does special weird things of its own.
 * All the rest (including gcc on all platforms) do the right thing.
 */
#define max_uchar arch_max_uchar
#define max_ushort arch_max_ushort
#define max_uint arch_max_uint
#define max_ulong arch_max_ulong

/* Minimum and maximum values for pointers. */
#if arch_ptrs_are_signed
#  define min_ptr min_long
#  define max_ptr max_long
#else
#  define min_ptr ((ulong)0)
#  define max_ptr max_ulong
#endif

/*
 * Define whether pointers are segmented.  If they are, we assume that
 * the compiler can't do reasonable register assignment for pointers,
 * so sometimes we use in-line casts instead of assignment to
 * a logically redundant pointer of the proper type.
 */
#define arch_ptrs_are_segmented (arch_sizeof_ds_ptr < arch_sizeof_ptr)

/* Define a reliable arithmetic right shift. */
/* Must use arith_rshift_1 for a shift by a literal 1. */
#define arith_rshift_slow(x,n) ((x) < 0 ? ~(~(x) >> (n)) : (x) >> (n))
#if arch_arith_rshift == 2
#  define arith_rshift(x,n) ((x) >> (n))
#  define arith_rshift_1(x) ((x) >> 1)
#else
#if arch_arith_rshift == 1		/* OK except for n=1 */
#  define arith_rshift(x,n) ((x) >> (n))
#  define arith_rshift_1(x) arith_rshift_slow(x,1)
#else
#  define arith_rshift(x,n) arith_rshift_slow(x,n)
#  define arith_rshift_1(x) arith_rshift_slow(x,1)
#endif
#endif

/*
 * Standard error printing macros.
 * Use dprintf for messages that just go to dstderr,
 * eprintf for error messages to estderr that include the program name,
 * lprintf for debugging messages that should include line number info.
 * Since we intercept fprintf to redirect output under MS Windows,
 * we have to define dputc and dputs in terms of fprintf also.
 * We also define eprintf and lprintf in a way that allows us to
 * intercept all calls to them by redefining eprintf_program_name
 * and lprintf_file_and_line.
 */

/* dstderr and estderr may be redefined. */
#define dstderr stderr
#define estderr stderr

#define dputc(chr) dprintf1("%c", chr)
#define dputs(str) dprintf1("%s", str)
#define dprintf(str)\
  fprintf(dstderr, str)
#define dprintf1(str,arg1)\
  fprintf(dstderr, str, arg1)
#define dprintf2(str,arg1,arg2)\
  fprintf(dstderr, str, arg1, arg2)
#define dprintf3(str,arg1,arg2,arg3)\
  fprintf(dstderr, str, arg1, arg2, arg3)
#define dprintf4(str,arg1,arg2,arg3,arg4)\
  fprintf(dstderr, str, arg1, arg2, arg3, arg4)
#define dprintf5(str,arg1,arg2,arg3,arg4,arg5)\
  fprintf(dstderr, str, arg1, arg2, arg3, arg4, arg5)
#define dprintf6(str,arg1,arg2,arg3,arg4,arg5,arg6)\
  fprintf(dstderr, str, arg1, arg2, arg3, arg4, arg5, arg6)
#define dprintf7(str,arg1,arg2,arg3,arg4,arg5,arg6,arg7)\
  fprintf(dstderr, str, arg1, arg2, arg3, arg4, arg5, arg6, arg7)
#define dprintf8(str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8)\
  fprintf(dstderr, str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8)
#define dprintf9(str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9)\
  fprintf(dstderr, str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9)
#define dprintf10(str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9,arg10)\
  fprintf(dstderr, str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10)
#define dprintf11(str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9,arg10,arg11)\
  fprintf(dstderr, str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11)
#define dprintf12(str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9,arg10,arg11,arg12)\
  fprintf(dstderr, str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12)

/* eprintf_program_name may be redefined. */
#define eprintf_program_name(f, program_name)\
  fprintf(f, "%s: ", program_name)
#ifdef PROGRAM_NAME
extern const char *PROGRAM_NAME;
#  define _epn eprintf_program_name(estderr, PROGRAM_NAME),
#else
#  define _epn /* */
#endif

#define eprintf(str)\
  (_epn fprintf(estderr, str))
#define eprintf1(str,arg1)\
  (_epn fprintf(estderr, str, arg1))
#define eprintf2(str,arg1,arg2)\
  (_epn fprintf(estderr, str, arg1, arg2))
#define eprintf3(str,arg1,arg2,arg3)\
  (_epn fprintf(estderr, str, arg1, arg2, arg3))
#define eprintf4(str,arg1,arg2,arg3,arg4)\
  (_epn fprintf(estderr, str, arg1, arg2, arg3, arg4))
#define eprintf5(str,arg1,arg2,arg3,arg4,arg5)\
  (_epn fprintf(estderr, str, arg1, arg2, arg3, arg4, arg5))
#define eprintf6(str,arg1,arg2,arg3,arg4,arg5,arg6)\
  (_epn fprintf(estderr, str, arg1, arg2, arg3, arg4, arg5, arg6))
#define eprintf7(str,arg1,arg2,arg3,arg4,arg5,arg6,arg7)\
  (_epn fprintf(estderr, str, arg1, arg2, arg3, arg4, arg5, arg6, arg7))
#define eprintf8(str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8)\
  (_epn fprintf(estderr, str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8))
#define eprintf9(str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9)\
  (_epn fprintf(estderr, str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9))
#define eprintf10(str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9,arg10)\
  (_epn fprintf(estderr, str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10))

/* lprintf_file_and_line may be redefined. */
#define lprintf_file_and_line(f, file, line)\
  fprintf(f, "%s(%d): ", file, line)
#if __LINE__				/* compiler provides it */
#  define _epl _epn lprintf_file_and_line(estderr, __FILE__, __LINE__),
#else
#  define _epl _epn
#endif

#define lprintf(str)\
  (_epl fprintf(estderr, str))
#define lprintf1(str,arg1)\
  (_epl fprintf(estderr, str, arg1))
#define lprintf2(str,arg1,arg2)\
  (_epl fprintf(estderr, str, arg1, arg2))
#define lprintf3(str,arg1,arg2,arg3)\
  (_epl fprintf(estderr, str, arg1, arg2, arg3))
#define lprintf4(str,arg1,arg2,arg3,arg4)\
  (_epl fprintf(estderr, str, arg1, arg2, arg3, arg4))
#define lprintf5(str,arg1,arg2,arg3,arg4,arg5)\
  (_epl fprintf(estderr, str, arg1, arg2, arg3, arg4, arg5))
#define lprintf6(str,arg1,arg2,arg3,arg4,arg5,arg6)\
  (_epl fprintf(estderr, str, arg1, arg2, arg3, arg4, arg5, arg6))
#define lprintf7(str,arg1,arg2,arg3,arg4,arg5,arg6,arg7)\
  (_epl fprintf(estderr, str, arg1, arg2, arg3, arg4, arg5, arg6, arg7))
#define lprintf8(str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8)\
  (_epl fprintf(estderr, str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8))
#define lprintf9(str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9)\
  (_epl fprintf(estderr, str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9))
#define lprintf10(str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9,arg10)\
  (_epl fprintf(estderr, str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10))

#endif					/* std_INCLUDED */
