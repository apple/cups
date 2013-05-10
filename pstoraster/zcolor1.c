/* Copyright (C) 1993, 1994, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* zcolor1.c */
/* Level 1 extended color operators */
#include "ghost.h"
#include "errors.h"
#include "oper.h"
#include "estack.h"
#include "ialloc.h"
#include "igstate.h"
#include "iutil.h"
#include "store.h"
#include "gxfixed.h"
#include "gxmatrix.h"
#include "gzstate.h"
#include "gxdevice.h"
#include "gxcmap.h"
#include "gscolor1.h"
#include "gxcspace.h"
#include "icolor.h"
#include "iimage.h"

/* - currentblackgeneration <proc> */
private int
zcurrentblackgeneration(register os_ptr op)
{	push(1);
	*op = istate->black_generation;
	return 0;
}

/* - currentcmykcolor <cyan> <magenta> <yellow> <black> */
private int
zcurrentcmykcolor(register os_ptr op)
{	float par[4];
	gs_currentcmykcolor(igs, par);
	push(4);
	make_reals(op - 3, par, 4);
	return 0;
}

/* - currentcolortransfer <redproc> <greenproc> <blueproc> <grayproc> */
private int
zcurrentcolortransfer(register os_ptr op)
{	push(4);
	op[-3] = istate->transfer_procs.colored.red;
	op[-2] = istate->transfer_procs.colored.green;
	op[-1] = istate->transfer_procs.colored.blue;
	*op = istate->transfer_procs.colored.gray;
	return 0;
}

/* - currentundercolorremoval <proc> */
private int
zcurrentundercolorremoval(register os_ptr op)
{	push(1);
	*op = istate->undercolor_removal;
	return 0;
}

/* <proc> setblackgeneration - */
private int
zsetblackgeneration(register os_ptr op)
{	int code;
	check_proc(*op);
	check_ostack(zcolor_remap_one_ostack - 1);
	check_estack(1 + zcolor_remap_one_estack);
	code = gs_setblackgeneration_remap(igs, gs_mapped_transfer, false);
	if ( code < 0 )
		return code;
	istate->black_generation = *op;
	pop(1);  op--;
	push_op_estack(zcolor_remap_color);
	return zcolor_remap_one(&istate->black_generation, op,
				igs->black_generation, igs,
				zcolor_remap_one_finish);
}

/* <cyan> <magenta> <yellow> <black> setcmykcolor - */
private int
zsetcmykcolor(register os_ptr op)
{	float par[4];
	int code;
	if ( (code = num_params(op, 4, par)) < 0 ||
	     (code = gs_setcmykcolor(igs, par[0], par[1], par[2], par[3])) < 0
	   )
	  return code;
	make_null(&istate->colorspace.array);
	pop(4);
	return 0;
}

/* <redproc> <greenproc> <blueproc> <grayproc> setcolortransfer - */
private int
zsetcolortransfer(register os_ptr op)
{	int code;
	check_proc(op[-3]);
	check_proc(op[-2]);
	check_proc(op[-1]);
	check_proc(*op);
	check_ostack(zcolor_remap_one_ostack * 4 - 4);
	check_estack(1 + zcolor_remap_one_estack * 4);
	istate->transfer_procs.colored.red = op[-3];
	istate->transfer_procs.colored.green = op[-2];
	istate->transfer_procs.colored.blue = op[-1];
	istate->transfer_procs.colored.gray = *op;
	if ( (code = gs_setcolortransfer_remap(igs,
			gs_mapped_transfer, gs_mapped_transfer,
			gs_mapped_transfer, gs_mapped_transfer,
			false)) < 0
	   )
		return code;
	/* Use osp rather than op here, because zcolor_remap_one pushes. */
	pop(4);  op -= 4;
	push_op_estack(zcolor_reset_transfer);
	if ( (code = zcolor_remap_one(&istate->transfer_procs.colored.red,
				osp, igs->set_transfer.colored.red, igs,
				zcolor_remap_one_finish)) < 0 ||
	     (code = zcolor_remap_one(&istate->transfer_procs.colored.green,
				osp, igs->set_transfer.colored.green, igs,
				zcolor_remap_one_finish)) < 0 ||
	     (code = zcolor_remap_one(&istate->transfer_procs.colored.blue,
				osp, igs->set_transfer.colored.blue, igs,
				zcolor_remap_one_finish)) < 0
	   )
	  return code;
	return zcolor_remap_one(&istate->transfer_procs.colored.gray,
				osp, igs->set_transfer.colored.gray, igs,
				zcolor_remap_one_finish);
}

/* <proc> setundercolorremoval - */
private int
zsetundercolorremoval(register os_ptr op)
{	int code;
	check_proc(*op);
	check_ostack(zcolor_remap_one_ostack - 1);
	check_estack(1 + zcolor_remap_one_estack);
	code = gs_setundercolorremoval_remap(igs, gs_mapped_transfer, false);
	if ( code < 0 )
		return code;
	istate->undercolor_removal = *op;
	pop(1);  op--;
	push_op_estack(zcolor_remap_color);
	return zcolor_remap_one(&istate->undercolor_removal, op,
				igs->undercolor_removal, igs,
				zcolor_remap_one_signed_finish);
}

/* <width> <height> <bits/comp> <matrix> */
/*	true <datasrc_0> ... <datasrc_ncomp-1> <ncomp> colorimage - */
/*	false <datasrc> <ncomp> colorimage - */
private int
zcolorimage(register os_ptr op)
{	int spp;			/* samples per pixel */
	int npop = 7;
	os_ptr procp = op - 2;
	const gs_color_space *pcs;

	bool multi = false;
	check_int_leu(*op, 4);		/* ncolors */
	check_type(op[-1], t_boolean);	/* multiproc */
	switch ( (spp = (int)(op->value.intval)) )
	{
	case 1:
		pcs = gs_color_space_DeviceGray();
		break;
	case 3:
		pcs = gs_color_space_DeviceRGB();
		goto color;
	case 4:
		pcs = gs_color_space_DeviceCMYK();
color:		if ( op[-1].value.boolval )	/* planar format */
		  { npop += spp - 1;
		    procp -= spp - 1,
		    multi = true;
		  }
		break;
	default:
		return_error(e_rangecheck);
	}
	return zimage_opaque_setup(procp, multi, pcs, npop);
}

/* ------ Initialization procedure ------ */

BEGIN_OP_DEFS(zcolor1_op_defs) {
	{"0currentblackgeneration", zcurrentblackgeneration},
	{"0currentcmykcolor", zcurrentcmykcolor},
	{"0currentcolortransfer", zcurrentcolortransfer},
	{"0currentundercolorremoval", zcurrentundercolorremoval},
	{"1setblackgeneration", zsetblackgeneration},
	{"4setcmykcolor", zsetcmykcolor},
	{"4setcolortransfer", zsetcolortransfer},
	{"1setundercolorremoval", zsetundercolorremoval},
	{"7colorimage", zcolorimage},
END_OP_DEFS(0) }
