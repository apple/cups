/* Copyright (C) 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gdevpstr.h,v 1.1 2000/03/13 19:00:47 mike Exp $ */
/* Stream output for PostScript- and PDF-writing drivers. */

#ifndef gdevpstr_INCLUDED
#  define gdevpstr_INCLUDED

/* Define an opaque type for streams. */
#ifndef stream_DEFINED
#  define stream_DEFINED
typedef struct stream_s stream;

#endif

/* Put a character on a stream. */
#define pputc(s, c) spputc(s, c)

/* Put a byte array on a stream. */
int pwrite(P3(stream * s, const void *ptr, uint count));

/* Put a string on a stream. */
int pputs(P2(stream * s, const char *str));

/*
 * Print (a) floating point number(s) using a format.  This is needed
 * because %f format always prints a fixed number of digits after the
 * decimal point, and %g format may use %e format, which PDF disallows.
 * These functions return a pointer to the next %-element of the format, or
 * to the terminating 0.
 */
const char *pprintg1(P3(stream * s, const char *format, floatp v));
const char *pprintg2(P4(stream * s, const char *format, floatp v1, floatp v2));
const char *pprintg3(P5(stream * s, const char *format,
			floatp v1, floatp v2, floatp v3));
const char *pprintg4(P6(stream * s, const char *format,
			floatp v1, floatp v2, floatp v3, floatp v4));
const char *pprintg6(P8(stream * s, const char *format,
			floatp v1, floatp v2, floatp v3, floatp v4,
			floatp v5, floatp v6));

/*
 * The rest of these printing functions exist solely because the ANSI C
 * "standard" for functions with a variable number of arguments is not
 * implemented properly or consistently across compilers.
 */
/* Print (an) int value(s) using a format. */
const char *pprintd1(P3(stream * s, const char *format, int v));
const char *pprintd2(P4(stream * s, const char *format, int v1, int v2));
const char *pprintd3(P5(stream * s, const char *format,
			int v1, int v2, int v3));
const char *pprintd4(P6(stream * s, const char *format,
			int v1, int v2, int v3, int v4));

/* Print a long value using a format. */
const char *pprintld1(P3(stream * s, const char *format, long v));
const char *pprintld2(P4(stream * s, const char *format, long v1, long v2));
const char *pprintld3(P5(stream * s, const char *format,
			 long v1, long v2, long v3));

/* Print (a) string(s) using a format. */
const char *pprints1(P3(stream * s, const char *format, const char *str));
const char *pprints2(P4(stream * s, const char *format,
			const char *str1, const char *str2));
const char *pprints3(P5(stream * s, const char *format,
		     const char *str1, const char *str2, const char *str3));

#endif /* gdevpstr_INCLUDED */
