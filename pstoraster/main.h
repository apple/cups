/* Copyright (C) 1992, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: main.h,v 1.2 2000/03/08 23:15:17 mike Exp $ */
/* Backward-compatible interface to gsmain.c */

#ifndef main_INCLUDED
#  define main_INCLUDED

#include "imain.h"
#include "iminst.h"

/*
 * This file adds to imain.h some backward-compatible procedures and
 * data elements that assume there is only a single instance of
 * the interpreter.
 */

/* ================ Data elements ================ */

/* Clients should never access these directly. */

#define gs_user_errors (gs_main_instance_default()->user_errors)
#define gs_lib_path (gs_main_instance_default()->lib_path)
/* gs_lib_paths removed in release 3.65 */
/* gs_lib_env_path removed in release 3.65 */

/* ================ Exported procedures from gsmain.c ================ */

/* ---------------- Initialization ---------------- */

#define gs_init0(in, out, err, mlp)\
  gs_main_init0(gs_main_instance_default(), in, out, err, mlp)

#define gs_init1()\
  gs_main_init1(gs_main_instance_default())

#define gs_init2()\
  gs_main_init2(gs_main_instance_default())

#define gs_add_lib_path(path)\
  gs_main_add_lib_path(gs_main_instance_default(), path)

#define gs_set_lib_paths()\
  gs_main_set_lib_paths(gs_main_instance_default())

#define gs_lib_open(fname, pfile)\
  gs_main_lib_open(gs_main_instance_default(), fname, pfile)

/* ---------------- Execution ---------------- */

#define gs_run_file(fn, ue, pec, peo)\
  gs_main_run_file(gs_main_instance_default(), fn, ue, pec, peo)

#define gs_run_string(str, ue, pec, peo)\
  gs_main_run_string(gs_main_instance_default(), str, ue, pec, peo)

#define gs_run_string_with_length(str, len, ue, pec, peo)\
  gs_main_run_string_with_length(gs_main_instance_default(),\
				 str, len, ue, pec, peo)

#define gs_run_file_open(fn, pfref)\
  gs_main_run_file_open(gs_main_instance_default(), fn, pfref)

#define gs_run_string_begin(ue, pec, peo)\
  gs_main_run_string_begin(gs_main_instance_default(), ue, pec, peo)

#define gs_run_string_continue(str, len, ue, pec, peo)\
  gs_main_run_string_continue(gs_main_instance_default(),\
			      str, len, ue, pec, peo)

#define gs_run_string_end(ue, pec, peo)\
  gs_main_run_string_end(gs_main_instance_default(), ue, pec, peo)

/* ---------------- Termination ---------------- */

#define gs_finit(status, code)\
  gs_main_finit(gs_main_instance_default(), status, code)

#endif /* main_INCLUDED */
