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

/*$Id: gconf.h,v 1.1 2000/03/13 18:57:14 mike Exp $ */
/* Wrapper for gconfig.h or a substitute. */

/*
 * NOTA BENE: This file, unlike all other header files, must *not* have
 * double-inclusion protection, since it is used in peculiar ways.
 */

/*
 * Since not all C preprocessors implement #include with a non-quoted
 * argument, we arrange things so that we can still compile with such
 * compilers as long as GCONFIG_H isn't defined.
 */

#ifndef GCONFIG_H
#  include "gconfig.h"
#else
#  include GCONFIG_H
#endif
