/* Copyright (C) 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* iconf.c */
/* Configuration-dependent tables and initialization for interpreter */
#include "stdio_.h"			/* stdio for stream.h */
#include "gsmemory.h"			/* for gscdefs.h */
#include "gscdefs.h"
#include "iref.h"
#include "ivmspace.h"
#include "opdef.h"
#include "stream.h"			/* for files.h */
#include "files.h"
#include "imain.h"

/* Set up the .ps file name string array. */
/* We fill in the lengths at initialization time. */
#define ref_(t) struct { struct tas_s tas; t value; }
#define string_(s)\
 { { (t_string<<r_type_shift) + a_readonly + avm_foreign, 0 }, s },
#define psfile_(fns) string_(fns)
ref_(const char *) gs_init_file_array[] = {
#include "gconfig.h"
	string_(0)
};
#undef psfile_

/* Set up the emulator name string array similarly. */
#define emulator_(ems) string_(ems)
ref_(const char *) gs_emulator_name_array[] = {
#include "gconfig.h"
	string_(0)
};
#undef emulator_

/* Initialize the operators. */
extern op_def_ptr
	/* Initialization operators */
#define oper_(defs) defs(P0()),
#include "gconfig.h"
#undef oper_
	/* Interpreter operators */
  interp_op_defs(P0());
op_def_ptr (*(op_defs_all[]))(P0()) = {
	/* Initialization operators */
#define oper_(defs) defs,
#include "gconfig.h"
#undef oper_
	/* Interpreter operators */
  interp_op_defs,
	/* end marker */
  0
};
