/* Copyright (C) 1994, 1995 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: iscannum.h,v 1.2 2000/03/08 23:15:16 mike Exp $ */
/* Interface to Ghostscript number scanner */

#ifndef iscannum_INCLUDED
#  define iscannum_INCLUDED

/* Scan a number.  If the number consumes the entire string, return 0; */
/* if not, set *psp to the first character beyond the number and return 1. */
int scan_number(P5(const byte * sp, const byte * end, int sign, ref * pref,
		   const byte ** psp));

#endif /* iscannum_INCLUDED */
