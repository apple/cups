/* Copyright (C) 1989, 1992, 1993, 1994 Aladdin Enterprises.  All rights reserved.
  
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

/* memory_.h */
/* Generic substitute for Unix memory.h */

/* We must include std.h before any file that includes sys/types.h. */
#include "std.h"

/****** Note: the System V bcmp routine only returns zero or non-zero, ******/
/****** unlike memcmp which returns -1, 0, or 1. ******/

#ifdef __TURBOC__
/* Define inline functions */
#  ifdef __WIN32__
#    define memcmp_inline(b1,b2,len) memcmp(b1,b2,len)
#  else
#    define memcmp_inline(b1,b2,len) __memcmp__(b1,b2,len)
#  endif
/* The Turbo C implementation of memset swaps the arguments and calls */
/* the non-standard routine setmem.  We may as well do it in advance. */
#  undef memset			/* just in case */
#  include <mem.h>
#  ifndef memset		/* Borland C++ can inline this */
#    define memset(dest,chr,cnt) setmem(dest,cnt,chr)
#  endif
#else
	/* Not Turbo C, no inline functions */
#  define memcmp_inline(b1,b2,len) memcmp(b1,b2,len)
	/* Apparently the newer VMS compilers include prototypes */
	/* for the mem... routines in <string.h>.  Unfortunately, */
	/* gcc lies on Sun systems: it defines __STDC__ even if */
	/* the header files in /usr/include are broken. */
	/* However, Solaris systems, which define __svr4__, do have */
	/* correct header files. */
#  if defined(VMS) || defined(_POSIX_SOURCE) || (defined(__STDC__) && (!defined(sun) || defined(__svr4__))) || defined(_HPUX_SOURCE) || defined(__WATCOMC__) || defined(THINK_C) || defined(bsdi)
#    include <string.h>
#  else
#    if defined(BSD4_2) || defined(UTEK)
	 extern bcopy(), bcmp(), bzero();
#	 define memcpy(dest,src,len) bcopy(src,dest,len)
#	 define memcmp(b1,b2,len) bcmp(b1,b2,len)
	 /* Define our own versions of missing routines (in gsmisc.c). */
#	 define memory__need_memmove
#        define memmove(dest,src,len) gs_memmove(dest,src,len)
#        include <sys/types.h>   /* for size_t */
	 void *gs_memmove(P3(void *, const void *, size_t));
#	 define memory__need_memset
#        define memset(dest,ch,len) gs_memset(dest,ch,len)
	 void *gs_memset(P3(void *, int, size_t));
#	 if defined(UTEK)
#          define memory__need_memchr
#          define memchr(ptr,ch,len) gs_memchr(ptr,ch,len)
	   const char *gs_memchr(P3(const void *, int, size_t));
#        endif				/* UTEK */
#    else
#      include <memory.h>
#      if defined(__SVR3) || defined(sun)	/* Not sure this is right.... */
#	 define memory__need_memmove
#        define memmove(dest,src,len) gs_memmove(dest,src,len)
#        include <sys/types.h>   /* for size_t */
	 void *gs_memmove(P3(void *, const void *, size_t));
#      endif				/* __SVR3 or sun */
#    endif				/* BSD4_2 or UTEK */
#  endif				/* VMS, POSIX, ... */
#endif					/* !__TURBOC__ */
