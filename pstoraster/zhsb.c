/* Copyright (C) 1994, 1997 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: zhsb.c,v 1.2 2000/03/08 23:15:38 mike Exp $ */
/* HSB color operators */
#include "ghost.h"
#include "oper.h"
#include "igstate.h"
#include "store.h"
#include "gshsb.h"

/* - currenthsbcolor <hue> <saturation> <brightness> */
private int
zcurrenthsbcolor(register os_ptr op)
{
    float par[3];

    gs_currenthsbcolor(igs, par);
    push(3);
    make_floats(op - 2, par, 3);
    return 0;
}

/* <hue> <saturation> <brightness> sethsbcolor - */
private int
zsethsbcolor(register os_ptr op)
{
    double par[3];
    int code;

    if ((code = num_params(op, 3, par)) < 0 ||
	(code = gs_sethsbcolor(igs, par[0], par[1], par[2])) < 0
	)
	return code;
    make_null(&istate->colorspace.array);
    pop(3);
    return 0;
}

/* ------ Initialization procedure ------ */

const op_def zhsb_op_defs[] =
{
    {"0currenthsbcolor", zcurrenthsbcolor},
    {"3sethsbcolor", zsethsbcolor},
    op_def_end(0)
};
