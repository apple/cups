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

/*$Id: scfparam.c,v 1.1 2000/03/08 23:15:22 mike Exp $ */
/* CCITTFax filter parameter setting and reading */
#include "std.h"
#include "gserror.h"
#include "gserrors.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gsparam.h"
#include "scommon.h"
#include "scf.h"		/* for cfe_max_width */
#include "scfx.h"

/* Define the CCITTFax parameters. */
private const gs_param_item_t s_CF_param_items[] =
{
#define cfp(key, type, memb) { key, type, offset_of(stream_CF_state, memb) }
    cfp("Uncompressed", gs_param_type_bool, Uncompressed),
    cfp("K", gs_param_type_int, K),
    cfp("EndOfLine", gs_param_type_bool, EndOfLine),
    cfp("EncodedByteAlign", gs_param_type_bool, EncodedByteAlign),
    cfp("Columns", gs_param_type_int, Columns),
    cfp("Rows", gs_param_type_int, Rows),
    cfp("EndOfBlock", gs_param_type_bool, EndOfBlock),
    cfp("BlackIs1", gs_param_type_bool, BlackIs1),
    cfp("DamagedRowsBeforeError", gs_param_type_int, DamagedRowsBeforeError),
    cfp("FirstBitLowOrder", gs_param_type_bool, FirstBitLowOrder),
    cfp("DecodedByteAlign", gs_param_type_int, DecodedByteAlign),
#undef cfp
    gs_param_item_end
};

/* Define a limit on the Rows parameter, close to max_int. */
#define cf_max_height 32000

/* Get non-default CCITTFax filter parameters. */
stream_state_proc_get_params(s_CF_get_params, stream_CF_state);		/* check */
int
s_CF_get_params(gs_param_list * plist, const stream_CF_state * ss, bool all)
{
    stream_CF_state cfs_defaults;
    const stream_CF_state *defaults;

    if (all)
	defaults = 0;
    else {
	s_CF_set_defaults_inline(&cfs_defaults);
	defaults = &cfs_defaults;
    }
    return gs_param_write_items(plist, ss, defaults, s_CF_param_items);
}

/* Put CCITTFax filter parameters. */
stream_state_proc_put_params(s_CF_put_params, stream_CF_state);		/* check */
int
s_CF_put_params(gs_param_list * plist, stream_CF_state * ss)
{
    stream_CF_state state;
    int code;

    state = *ss;
    code = gs_param_read_items(plist, (void *)&state, s_CF_param_items);
    if (code >= 0 &&
	(state.K < -cf_max_height || state.K > cf_max_height ||
	 state.Columns < 0 || state.Columns > cfe_max_width ||
	 state.Rows < 0 || state.Rows > cf_max_height ||
	 state.DamagedRowsBeforeError < 0 ||
	 state.DamagedRowsBeforeError > cf_max_height ||
	 state.DecodedByteAlign < 1 || state.DecodedByteAlign > 16 ||
	 (state.DecodedByteAlign & (state.DecodedByteAlign - 1)) != 0)
	)
	code = gs_note_error(gs_error_rangecheck);
    if (code >= 0)
	*ss = state;
    return code;
}
