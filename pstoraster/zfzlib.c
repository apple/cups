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

/* zfzlib.c */
/* zlib and Flate filter creation */
#include "ghost.h"
#include "errors.h"
#include "oper.h"
#include "idict.h"
#include "strimpl.h"
#include "spdiffx.h"
#include "spngpx.h"
#include "szlibx.h"
#include "ifilter.h"

/* Import the Predictor machinery from zfdecode.c and zfilter2.c. */
int filter_read_predictor(P4(os_ptr op, int npop,
  const stream_template *template, stream_state *st));
int filter_write_predictor(P4(os_ptr op, int npop,
  const stream_template *template, stream_state *st));

/* <source> zlibEncode/filter <file> */
/* <source> <dict_ignored> zlibEncode/filter <file> */
private int
zzlibE(os_ptr op)
{	return filter_write_simple(op, &s_zlibE_template);
}

/* <target> zlibDecode/filter <file> */
/* <target> <dict_ignored> zlibDecode/filter <file> */
private int
zzlibD(os_ptr op)
{	return filter_read_simple(op, &s_zlibD_template);
}

/* <source> FlateEncode/filter <file> */
/* <source> <dict> FlateEncode/filter <file> */
private int
zFlateE(os_ptr op)
{	stream_zlib_state zls;
	int npop;

	if ( r_has_type(op, t_dictionary) )
	  { check_dict_read(*op);
	  npop = 1;
	  }
	else
	  npop = 0;
	return filter_write_predictor(op, npop, &s_zlibE_template,
				      (stream_state *)&zls);
}

/* <target> FlateDecode/filter <file> */
/* <target> <dict> FlateDecode/filter <file> */
private int
zFlateD(os_ptr op)
{	stream_zlib_state zls;
	int npop;

	if ( r_has_type(op, t_dictionary) )
	  { check_dict_read(*op);
	  npop = 1;
	  }
	else
	  npop = 0;
	return filter_read_predictor(op, npop, &s_zlibD_template,
				     (stream_state *)&zls);
}

/* ------ Initialization procedure ------ */

BEGIN_OP_DEFS(zfzlib_op_defs) {
		op_def_begin_filter(),
	{"1zlibEncode", zzlibE},
	{"1zlibDecode", zzlibD},
	{"1FlateEncode", zFlateE},
	{"1FlateDecode", zFlateD},
END_OP_DEFS(0) }
