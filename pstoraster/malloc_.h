/* Copyright (C) 1989, 1992, 1996, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: malloc_.h,v 1.2 2000/03/08 23:15:18 mike Exp $ */
/* Generic substitute for Unix malloc.h */

#ifndef malloc__INCLUDED
#  define malloc__INCLUDED

/* We must include std.h before any file that includes sys/types.h. */
#include "std.h"

#ifdef __TURBOC__
#  include <alloc.h>
#else
#  if defined(BSD4_2) || defined(apollo) || defined(vax) || defined(sequent) || defined(UTEK)
#    if defined(_POSIX_SOURCE) || (defined(__STDC__) && (!defined(sun) || defined(__svr4__)))	/* >>> */
#      include <stdlib.h>
#    else			/* Ancient breakage */
extern char *malloc();
extern void free();

#    endif
#  else
#    if defined(_HPUX_SOURCE) || defined(__CONVEX__) || defined(__convex__) || defined(__OSF__) || defined(__386BSD__) || defined(_POSIX_SOURCE) || defined(__STDC__) || defined(VMS)
#      include <stdlib.h>
#    else
#      include <malloc.h>
#    endif			/* !_HPUX_SOURCE, ... */
#  endif			/* !BSD4_2, ... */
#endif /* !__TURBOC__ */

/* (At least some versions of) Linux don't have a working realloc.... */
#ifdef linux
#  define malloc__need_realloc
void *gs_realloc(P3(void *, size_t, size_t));

#else
#  define gs_realloc(ptr, old_size, new_size) realloc(ptr, new_size)
#endif

#endif /* malloc__INCLUDED */
