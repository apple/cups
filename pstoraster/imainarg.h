/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: imainarg.h,v 1.3 2000/03/13 19:00:48 mike Exp $ */
/* argv/argc interface to imainarg.c */

#ifndef imainarg_INCLUDED
#  define imainarg_INCLUDED

/* Define an opaque type for an interpreter instance.  See imain.h. */
#ifndef gs_main_instance_DEFINED
#  define gs_main_instance_DEFINED
typedef struct gs_main_instance_s gs_main_instance;

#endif

/*
 * As a shortcut for very high-level clients, we define a single call
 * that does the equivalent of command line invocation, passing argc
 * and argv.  This call includes calling init0 through init2.
 * argv should really be const char *[], but ANSI C requires writable
 * strings (which, however, it forbids the callee to modify!).
 */
int gs_main_init_with_args(P3(gs_main_instance * minst, int argc,
			      char *argv[]));

/*
 * Run the 'start' procedure (after processing the command line).
 * Note that this procedure exits rather than returning.
 */
void gs_main_run_start(P1(gs_main_instance * minst));

#endif /* imainarg_INCLUDED */
