/* Copyright (C) 1991, 1995 Aladdin Enterprises.  All rights reserved.
  
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

/* time_.h */
/* Generic substitute for Unix sys/time.h */

/* We must include std.h before any file that includes sys/types.h. */
#include "std.h"

/* The location (or existence) of certain system headers is */
/* environment-dependent. We detect this in the makefile */
/* and conditionally define switches in gconfig_.h. */
#include "gconfig_.h"

/* Some System V environments don't include sys/time.h. */
/* The SYSTIME_H switch in gconfig_.h reflects this. */
#ifdef SYSTIME_H
#  include <sys/time.h>
#  if defined(M_UNIX) || defined(_IBMR2)	/* SCO and AIX need both time.h and sys/time.h! */
#    include <time.h>
#  endif
#else
#  include <time.h>
#  ifndef __DECC
struct timeval {
	long tv_sec, tv_usec;
};
#  endif
struct timezone {
	int tz_minuteswest, tz_dsttime;
};
#endif

#if defined(ultrix) && defined(mips)
/* Apparently some versions of Ultrix for the DECstation include */
/* time_t in sys/time.h, and some don't.  If you get errors */
/* compiling gp_unix.c, uncomment the next line. */
/*	typedef	int	time_t;	*/
#endif

/* In SVR4.0 (but not other System V implementations), */
/* gettimeofday doesn't take a timezone argument. */
#ifdef SVR4_0
#  define gettimeofday_no_timezone 1
#else
#  define gettimeofday_no_timezone 0
#endif

/* Some System V environments, and Posix environments, need <sys/times.h>. */
#if defined(SYSV) || defined(SVR4)
#  include <sys/times.h>
#  define use_times_for_usertime 1
		/* Posix 1003.1b-1993 section 4.8.1.5 says that
		   CLK_TCK is obsolescent and that sysconf(_SC_CLK_TCK)
		   should be used instead, but this requires including
		   <unistd.h>, which is too painful to configure.  */
#  ifndef CLK_TCK
#    define CLK_TCK 100		/* guess for older hosts */
#  endif
#else
#  define use_times_for_usertime 0
#endif
