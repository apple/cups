/* Copyright (C) 1989, 1993, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gserrors.h,v 1.2 2000/03/08 23:14:40 mike Exp $ */
/* Error code definitions */

#ifndef gserrors_INCLUDED
#  define gserrors_INCLUDED

/* A procedure that may return an error always returns */
/* a non-negative value (zero, unless otherwise noted) for success, */
/* or negative for failure. */
/* We use ints rather than an enum to avoid a lot of casting. */

#define gs_error_unknownerror (-1)	/* unknown error */
#define gs_error_interrupt (-6)
#define gs_error_invalidaccess (-7)
#define gs_error_invalidfileaccess (-9)
#define gs_error_invalidfont (-10)
#define gs_error_ioerror (-12)
#define gs_error_limitcheck (-13)
#define gs_error_nocurrentpoint (-14)
#define gs_error_rangecheck (-15)
#define gs_error_typecheck (-20)
#define gs_error_undefined (-21)
#define gs_error_undefinedfilename (-22)
#define gs_error_undefinedresult (-23)
#define gs_error_VMerror (-25)
#define gs_error_unregistered (-29)

#define gs_error_hit_detected (-99)

#define gs_error_Fatal (-100)

#endif /* gserrors_INCLUDED */
