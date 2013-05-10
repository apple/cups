/* Copyright (C) 1995 Aladdin Enterprises.  All rights reserved.
  
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

/* gslparam.h */
/* Line parameter definitions */

#ifndef gslparam_INCLUDED
#  define gslparam_INCLUDED

/* Line cap values */
typedef enum {
	gs_cap_butt = 0,
	gs_cap_round = 1,
	gs_cap_square = 2,
	gs_cap_triangle = 3	/* not supported by PostScript */
} gs_line_cap;

/* Line join values */
typedef enum {
	gs_join_miter = 0,
	gs_join_round = 1,
	gs_join_bevel = 2,
	gs_join_none = 3,	/* not supported by PostScript */
	gs_join_triangle = 4	/* not supported by PostScript */
} gs_line_join;

#endif					/* gslparam_INCLUDED */
