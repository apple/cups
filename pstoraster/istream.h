/* Copyright (C) 1994, 1995 Aladdin Enterprises.  All rights reserved.
  
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

/* istream.h */
/* Interpreter-level stream procedures */
/* Requires scommon.h, ostack.h */

/* Procedures exported by zfproc.c */
	/* for zfilter.c - procedure stream initialization */
int sread_proc(P2(ref *, stream **));
int swrite_proc(P2(ref *, stream **));
	/* for interp.c, zfileio.c, zpaint.c - handle a procedure */
	/* callback or an interrupt */
int s_handle_read_exception(P5(int, const ref *, const ref *, int,
  int (*)(P1(os_ptr))));
int s_handle_write_exception(P5(int, const ref *, const ref *, int,
  int (*)(P1(os_ptr))));
