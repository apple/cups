/* Copyright (C) 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gpgetenv.h,v 1.1 2000/03/13 19:00:47 mike Exp $ */
/* Interface to platform-specific getenv routine */

#ifndef gpgetenv_INCLUDED
#  define gpgetenv_INCLUDED

/*
 * Get a value from the environment (getenv).
 *
 * If the key is missing, set *ptr = 0 (if *plen > 0), set *plen = 1,
 * and return 1.
 *
 * If the key is present and the length len of the value (not counting
 * the terminating \0) is less than *plen, copy the value to ptr, set
 * *plen = len + 1, and return 0.
 *
 * If the key is present and len >= *plen, set *plen = len + 1,
 * don't store anything at ptr, and return -1.
 *
 * Note that *plen is the size of the buffer, not the length of the string:
 * because of the terminating \0, the maximum string length is 1 less than
 * the size of the buffer.
 */
int gp_getenv(P3(const char *key, char *ptr, int *plen));

#endif /* gpgetenv_INCLUDED */
