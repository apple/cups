/*
  Copyright 1993-2001 by Easy Software Products.
  Copyright 1998 Aladdin Enterprises.  All rights reserved.
  
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
#ifdef HAVE_LIBJPEG
/*$Id: sddparam.c,v 1.3 2001/01/22 15:03:56 mike Exp $ */
/* DCTDecode filter parameter setting and reading */
#include "std.h"
#include "jpeglib.h"
#include "gserror.h"
#include "gserrors.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gsparam.h"
#include "strimpl.h"		/* sdct.h requires this */
#include "sdct.h"
#include "sdcparam.h"
#include "sjpeg.h"

/* ================ Get parameters ================ */

stream_state_proc_get_params(s_DCTD_get_params, stream_DCT_state);	/* check */

int
s_DCTD_get_params(gs_param_list * plist, const stream_DCT_state * ss, bool all)
{
    stream_DCT_state dcts_defaults;
    const stream_DCT_state *defaults;

    if (all)
	defaults = 0;
    else {
	(*s_DCTE_template.set_defaults) ((stream_state *) & dcts_defaults);
	defaults = &dcts_defaults;
    }
/****** NYI ******/
    return s_DCT_get_params(plist, ss, defaults);
}

/* ================ Put parameters ================ */

stream_state_proc_put_params(s_DCTD_put_params, stream_DCT_state);	/* check */

int
s_DCTD_put_params(gs_param_list * plist, stream_DCT_state * pdct)
{
    int code;

    if ((code = s_DCT_put_params(plist, pdct)) < 0 ||
    /*
     * DCTDecode accepts quantization and huffman tables
     * in case these tables have been omitted from the datastream.
     */
	(code = s_DCT_put_huffman_tables(plist, pdct, false)) < 0 ||
	(code = s_DCT_put_quantization_tables(plist, pdct, false)) < 0
	)
	DO_NOTHING;
    return code;
}
#endif /* HAVE_LIBJPEG */
