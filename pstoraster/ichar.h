/* Copyright (C) 1994, 1996, 1997 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: ichar.h,v 1.2 2000/03/08 23:15:08 mike Exp $ */
/* Requires gxchar.h */

#ifndef ichar_INCLUDED
#  define ichar_INCLUDED

/*
 * All the character rendering operators use the execution stack
 * for loop control -- see estack.h for details.
 * The information pushed by these operators is as follows:
 *      the enumerator (t_struct, a gs_show_enum);
 *      a slot for the procedure for kshow or cshow (probably t_array) or
 *              the string or array for [x][y]show (t_string or t_array);
 *      a slot for the string/array index for [x][y]show (t_integer);
 *      a slot for the saved o-stack depth for cshow or stringwidth,
 *              and for error recovery (t_integer);
 *      a slot for the saved d-stack depth ditto (t_integer);
 *      a slot for the saved gstate level ditto (t_integer);
 *      the procedure to be called at the end of the enumeration
 *              (t_operator, but called directly, not by the interpreter);
 *      the usual e-stack mark (t_null).
 */
#define snumpush 8
#define esenum(ep) r_ptr(ep, gs_show_enum)
#define senum esenum(esp)
#define esslot(ep) ((ep)[-1])
#define sslot esslot(esp)
#define essindex(ep) ((ep)[-2])
#define ssindex essindex(esp)
#define esodepth(ep) ((ep)[-3])
#define sodepth esodepth(esp)
#define esddepth(ep) ((ep)[-4])
#define sddepth esddepth(esp)
#define esgslevel(ep) ((ep)[-5])
#define sgslevel esgslevel(esp)
#define eseproc(ep) ((ep)[-6])
#define seproc eseproc(esp)

/* Procedures exported by zchar.c for zchar1.c, zchar2.c, and/or zcharcid.c. */
gs_show_enum *op_show_find(P0());
int op_show_setup(P2(os_ptr, gs_show_enum **));
int op_show_enum_setup(P2(os_ptr, gs_show_enum **));
void op_show_finish_setup(P3(gs_show_enum *, int, op_proc_p));
int op_show_continue(P1(os_ptr));
int op_show_continue_dispatch(P2(os_ptr, int));
int op_show_free(P1(int));

#endif /* ichar_INCLUDED */
