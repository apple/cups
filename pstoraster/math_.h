/* Copyright (C) 1989, 1995 Aladdin Enterprises.  All rights reserved.
  
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

/* math_.h */
/* Generic substitute for math.h */

/* We must include std.h before any file that includes sys/types.h. */
#include "std.h"

#if defined(VMS) && defined(__GNUC__)
/*  DEC VAX/VMS C comes with a math.h file, but GNU VAX/VMS C does not. */
#  include "vmsmath.h"
#else
#  include <math.h>
#endif

/* math.h is different for Turbo and Unix.... */
#ifndef M_PI
#  ifdef PI
#    define M_PI PI
#  else
#    define M_PI 3.14159265358979324
#  endif
#endif

/* Factors for converting between degrees and radians */
#define degrees_to_radians (M_PI / 180.0)
#define radians_to_degrees (180.0 / M_PI)

/* Define the maximum value of a single-precision float. */
/* This doesn't seem to be defined in any standard place, */
/* and we need an exact value for it. */
#undef MAX_FLOAT		/* just in case */
#if defined(vax) || defined(VAX) || defined(__vax) || defined(__VAX)
/* Maximum exponent is +127, 23 bits of fraction. */
#  define MAX_FLOAT\
     ((0x800000 - 1.0) * 0x1000000 * 0x1000000 * 0x10000000 * 0x10000000)
#else
/* IEEE, maximum exponent is +127, 23 bits of fraction + an implied '1'. */
#  define MAX_FLOAT\
     ((0x1000000 - 1.0) * 0x1000000 * 0x1000000 * 0x10000000 * 0x10000000)
#endif

/* Define the hypot procedure on those few systems that don't provide it. */
#ifdef _IBMR2
/* The RS/6000 has hypot, but math.h doesn't declare it! */
extern double hypot(double, double);
#else
#  if !defined(__TURBOC__) && !defined(BSD4_2) && !defined(VMS)
#    define hypot(x,y) sqrt((x)*(x)+(y)*(y))
#  endif
#endif

#ifdef OSK
/* OSK has atan2 and ldexp, but math.h doesn't declare them! */
extern double atan2(), ldexp();
#endif

#ifdef DEBUG

/* Intercept calls on sqrt for debugging. */
extern double gs_sqrt(P3(double, const char *, int));
#undef sqrt
#define sqrt(x) gs_sqrt(x, __FILE__, __LINE__)

#endif				/* DEBUG */
