/*
  Copyright 1993-2000 by Easy Software Products
  Copyright 1995, 1996, 1998 Aladdin Enterprises.  All rights reserved.
  
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

#include <config.h>
#ifdef HAVE_LIBZ
/*$Id: zfzlib.c,v 1.4 2000/03/14 13:52:36 mike Exp $ */
/* zlib and Flate filter creation */
#include "ghost.h"
#include "oper.h"
#include "idict.h"
#include "strimpl.h"
#include "spdiffx.h"
#include "spngpx.h"
#include "szlibx.h"
#include "ifilter.h"

/* Import the Predictor machinery from zfdecode.c and zfilter2.c. */
int filter_read_predictor(P4(os_ptr op, int npop,
			     const stream_template * template,
			     stream_state * st));
int filter_write_predictor(P4(os_ptr op, int npop,
			      const stream_template * template,
			      stream_state * st));

/* <source> zlibEncode/filter <file> */
/* <source> <dict> zlibEncode/filter <file> */
private int
zzlibE(os_ptr op)
{
    stream_zlib_state zls;

    (*s_zlibE_template.set_defaults)((stream_state *)&zls);
    return filter_write(op, 0, &s_zlibE_template, (stream_state *)&zls, 0);
}

/* <target> zlibDecode/filter <file> */
/* <target> <dict> zlibDecode/filter <file> */
private int
zzlibD(os_ptr op)
{
    stream_zlib_state zls;

    (*s_zlibD_template.set_defaults)((stream_state *)&zls);
    return filter_read(op, 0, &s_zlibD_template, (stream_state *)&zls, 0);
}

/* <source> FlateEncode/filter <file> */
/* <source> <dict> FlateEncode/filter <file> */
private int
zFlateE(os_ptr op)
{
    stream_zlib_state zls;

    (*s_zlibE_template.set_defaults)((stream_state *)&zls);
    return filter_write_predictor(op, 0, &s_zlibE_template,
				  (stream_state *)&zls);
}

/* <target> FlateDecode/filter <file> */
/* <target> <dict> FlateDecode/filter <file> */
private int
zFlateD(os_ptr op)
{
    stream_zlib_state zls;

    (*s_zlibD_template.set_defaults)((stream_state *)&zls);
    return filter_read_predictor(op, 0, &s_zlibD_template,
				 (stream_state *)&zls);
}

/* ------ Initialization procedure ------ */

const op_def zfzlib_op_defs[] =
{
    op_def_begin_filter(),
    {"1zlibEncode", zzlibE},
    {"1zlibDecode", zzlibD},
    {"1FlateEncode", zFlateE},
    {"1FlateDecode", zFlateD},
    op_def_end(0)
};
#endif /* HAVE_LIBZ */
