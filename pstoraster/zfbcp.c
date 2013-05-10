/* Copyright (C) 1994, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* zfbcp.c */
/* (T)BCP filter creation */
#include "memory_.h"
#include "ghost.h"
#include "errors.h"
#include "oper.h"
#include "gsstruct.h"
#include "ialloc.h"
#include "stream.h"
#include "strimpl.h"
#include "sfilter.h"
#include "ifilter.h"

/* Define null handlers for the BCP out-of-band signals. */
private int
no_bcp_signal_interrupt(stream_state *st)
{	return 0;
}
private int
no_bcp_request_status(stream_state *st)
{	return 0;
}

/* <source> BCPEncode/filter <file> */
/* <source> <dict_ignored> BCPEncode/filter <file> */
private int
zBCPE(os_ptr op)
{	return filter_write_simple(op, &s_BCPE_template);
}

/* <target> BCPDecode/filter <file> */
/* <target> <dict_ignored> BCPDecode/filter <file> */
private int
zBCPD(os_ptr op)
{	stream_BCPD_state state;
	state.signal_interrupt = no_bcp_signal_interrupt;
	state.request_status = no_bcp_request_status;
	return filter_read(op, (r_has_type(op, t_dictionary) ? 1 : 0),
			   &s_BCPD_template, (stream_state *)&state, 0);
}

/* <source> TBCPEncode/filter <file> */
/* <source> <dict_ignored> TBCPEncode/filter <file> */
private int
zTBCPE(os_ptr op)
{	return filter_write_simple(op, &s_TBCPE_template);
}

/* <target> TBCPDecode/filter <file> */
/* <target> <dict_ignored> TBCPDecode/filter <file> */
private int
zTBCPD(os_ptr op)
{	stream_BCPD_state state;
	state.signal_interrupt = no_bcp_signal_interrupt;
	state.request_status = no_bcp_request_status;
	return filter_read(op, (r_has_type(op, t_dictionary) ? 1 : 0),
			   &s_TBCPD_template, (stream_state *)&state, 0);
}

/* ------ Initialization procedure ------ */

BEGIN_OP_DEFS(zfbcp_op_defs) {
		op_def_begin_filter(),
	{"1BCPEncode", zBCPE},
	{"1BCPDecode", zBCPD},
	{"1TBCPEncode", zTBCPE},
	{"1TBCPDecode", zTBCPD},
END_OP_DEFS(0) }
