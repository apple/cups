/* Copyright (C) 1989, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* zdevice.c */
/* Device-related operators */
#include "ghost.h"
#include "errors.h"
#include "oper.h"
#include "ialloc.h"
#include "idict.h"
#include "igstate.h"
#include "iname.h"
#include "interp.h"
#include "iparam.h"
#include "ivmspace.h"
#include "gsmatrix.h"
#include "gsstate.h"
#include "gxdevice.h"
#include "store.h"

/* <device> copydevice <newdevice> */
private int
zcopydevice(register os_ptr op)
{	gx_device *new_dev;
	int code;

	check_read_type(*op, t_device);
	code = gs_copydevice(&new_dev, op->value.pdevice, imemory);
	if ( code < 0 )
	  return code;
	new_dev->memory = imemory;
	make_tav(op, t_device, icurrent_space | a_all, pdevice, new_dev);
	return 0;
}

/* <device> <y> <string> copyscanlines <substring> */
private int
zcopyscanlines(register os_ptr op)
{	os_ptr op1 = op - 1;
	os_ptr op2 = op - 2;
	gx_device *dev;
	int code;
	uint bytes_copied;

	check_read_type(*op2, t_device);
	dev = op2->value.pdevice;
	check_type(*op1, t_integer);
	if ( op1->value.intval < 0 || op1->value.intval > dev->height )
	  return_error(e_rangecheck);
	check_write_type(*op, t_string);
	code = gs_copyscanlines(dev, (int)op1->value.intval,
				op->value.bytes, r_size(op), NULL,
				&bytes_copied);
	if ( code < 0 )
	  return_error(e_rangecheck);	/* not a memory device */
	*op2 = *op;
	r_set_size(op2, bytes_copied);
	pop(2);
	return 0;
}

/* - currentdevice <device> */
int
zcurrentdevice(register os_ptr op)
{	gx_device *dev = gs_currentdevice(igs);
	gs_ref_memory_t *mem = (gs_ref_memory_t *)dev->memory;

	push(1);
	make_tav(op, t_device,
		 (mem == 0 ? avm_foreign : imemory_space(mem)) | a_all,
		 pdevice, dev);
	return 0;
}

/* - flushpage - */
int
zflushpage(register os_ptr op)
{	return gs_flushpage(igs);
}

/* <int> .getdevice <device> */
private int
zgetdevice(register os_ptr op)
{	const gx_device *dev;

	check_type(*op, t_integer);
	if ( op->value.intval != (int)(op->value.intval) )
	  return_error(e_rangecheck);	/* won't fit in an int */
	dev = gs_getdevice((int)(op->value.intval));
	if ( dev == 0 )		/* index out of range */
	  return_error(e_rangecheck);
	/* Device prototypes are read-only; */
	/* the cast is logically unnecessary. */
	make_tav(op, t_device, avm_foreign | a_readonly, pdevice,
		 (gx_device *)dev);
	return 0;
}

/* <device> <key_dict|null> .getdeviceparams <mark> <name> <value> ... */
private int
zgetdeviceparams(os_ptr op)
{	ref rkeys;
	gx_device *dev;
	stack_param_list list;
	int code;
	ref *pmark;

	check_read_type(op[-1], t_device);
	rkeys = *op;
	dev = op[-1].value.pdevice;
	pop(1);
	stack_param_list_write(&list, &o_stack, &rkeys);
	code = gs_getdeviceparams(dev, (gs_param_list *)&list);
	if ( code < 0 )
	  {	/* We have to put back the top argument. */
		if ( list.count > 0 )
		  ref_stack_pop(&o_stack, list.count * 2 - 1);
		else
		  ref_stack_push(&o_stack, 1);
		*osp = rkeys;
		return code;
	  }
	pmark = ref_stack_index(&o_stack, list.count * 2);
	make_mark(pmark);
	return 0;
}

/* <matrix> <width> <height> <palette> <word?> makewordimagedevice <device> */
private int
zmakewordimagedevice(register os_ptr op)
{	os_ptr op1 = op - 1;
	gs_matrix imat;
	gx_device *new_dev;
	const byte *colors;
	int colors_size;
	int code;

	check_int_leu(op[-3], max_uint >> 1);	/* width */
	check_int_leu(op[-2], max_uint >> 1);	/* height */
	check_type(*op, t_boolean);
	if ( r_has_type(op1, t_null) )	/* true color */
	   {	colors = 0;
		colors_size = -24;	/* 24-bit true color */
	   }
	else if ( r_has_type(op1, t_integer) )
	  {	switch ( op1->value.intval )
		  {
		  case 16: case 24: case 32: break;
		  default: return_error(e_rangecheck);
		  }
		colors = 0;
		colors_size = -op1->value.intval;
	  }
	else
	   {	check_type(*op1, t_string);	/* palette */
		if ( r_size(op1) > 3*256 )
		  return_error(e_rangecheck);
		colors = op1->value.bytes;
		colors_size = r_size(op1);
	   }
	if ( (code = read_matrix(op - 4, &imat)) < 0 )
		return code;
	/* Everything OK, create device */
	code = gs_makewordimagedevice(&new_dev, &imat,
				      (int)op[-3].value.intval,
				      (int)op[-2].value.intval,
				      colors, colors_size,
				      op->value.boolval, true, imemory);
	if ( code == 0 )
	{	new_dev->memory = imemory;
		make_tav(op - 4, t_device, imemory_space(iimemory) | a_all,
			 pdevice, new_dev);
		pop(4);
	}
	return code;
}

/* - nulldevice - */
/* Note that nulldevice clears the current pagedevice. */
private int
znulldevice(register os_ptr op)
{	gs_nulldevice(igs);
	clear_pagedevice(istate);
	return 0;
}

/* <num_copies> <flush_bool> .outputpage - */
private int
zoutputpage(register os_ptr op)
{	int code;

	check_type(op[-1], t_integer);
	check_type(*op, t_boolean);
	code = gs_output_page(igs, (int)op[-1].value.intval,
			      op->value.boolval);
	if ( code < 0 )
	  return code;
	pop(2);
	return 0;
}

/* <device> <policy_dict|null> <require_all> <mark> <name> <value> ... */
/*	.putdeviceparams */
/*   (on success) <device> <eraseflag> */
/*   (on failure) <device> <policy_dict|null> <require_all> <mark> */
/*	 <name1> <error1> ... */
/* For a key that simply was not recognized, if require_all is true, */
/* the result will be an /undefined error; if require_all is false, */
/* the key will be ignored. */
/* Note that .putdeviceparams clears the current pagedevice. */
private int
zputdeviceparams(os_ptr op)
{	uint count = ref_stack_counttomark(&o_stack);
	ref *prequire_all;
	ref *ppolicy;
	ref *pdev;
	gx_device *dev;
	stack_param_list list;
	int code;
	int old_width, old_height;
	int i, dest;

	if ( count == 0 )
	  return_error(e_unmatchedmark);
	prequire_all = ref_stack_index(&o_stack, count);
	ppolicy = ref_stack_index(&o_stack, count + 1);
	pdev = ref_stack_index(&o_stack, count + 2);
	if ( pdev == 0 )
	  return_error(e_stackunderflow);
	check_type_only(*prequire_all, t_boolean);
	check_write_type_only(*pdev, t_device);
	dev = pdev->value.pdevice;
	code = stack_param_list_read(&list, &o_stack, 0, ppolicy,
				     prequire_all->value.boolval);
	if ( code < 0 )
	  return code;
	old_width = dev->width;
	old_height = dev->height;
	code = gs_putdeviceparams(dev, (gs_param_list *)&list);
	/* Check for names that were undefined or caused errors. */
	for ( dest = count - 2, i = 0; i < count >> 1; i++ )
	  if ( list.results[i] < 0 )
	    {	*ref_stack_index(&o_stack, dest) =
		  *ref_stack_index(&o_stack, count - (i << 1) - 2);
		gs_errorname(list.results[i],
			     ref_stack_index(&o_stack, dest - 1));
		dest -= 2;
	    }
	if ( code < 0 )
	  {	/* There were errors reported. */
		ref_stack_pop(&o_stack, dest + 1);
		return 0;
	  }
	if ( code > 0 || (code == 0 && (dev->width != old_width || dev->height != old_height)) )
	{	/* The device was open and is now closed, */
		/* or its dimensions have changed. */
		/* If it was the current device, */
		/* call setdevice to reinstall it and erase the page. */
		/****** DOESN'T FIND ALL THE GSTATES THAT REFERENCE THE DEVICE. ******/
		if ( gs_currentdevice(igs) == dev )
		{	bool was_open = dev->is_open;
			code = gs_setdevice_no_erase(igs, dev);
			/* If the device wasn't closed, */
			/* setdevice won't erase the page. */
			if ( was_open && code >= 0 )
			  code = 1;
		}
	}
	if ( code < 0 )
	  return code;
	ref_stack_pop(&o_stack, count + 1);
	make_bool(osp, code);
	clear_pagedevice(istate);
	return 0;
}

/* <device> .setdevice <eraseflag> */
/* Note that .setdevice clears the current pagedevice. */
int
zsetdevice(register os_ptr op)
{	int code;

	check_write_type(*op, t_device);
	code = gs_setdevice_no_erase(igs, op->value.pdevice);
	if ( code < 0 )
	  return code;
	make_bool(op, 1);
	clear_pagedevice(istate);
	return code;
}

/* ------ Initialization procedure ------ */

BEGIN_OP_DEFS(zdevice_op_defs) {
	{"1copydevice", zcopydevice},
	{"3copyscanlines", zcopyscanlines},
	{"0currentdevice", zcurrentdevice},
	{"0flushpage", zflushpage},
	{"1.getdevice", zgetdevice},
	{"2.getdeviceparams", zgetdeviceparams},
	{"5makewordimagedevice", zmakewordimagedevice},
	{"0nulldevice", znulldevice},
	{"2.outputpage", zoutputpage},
	{"3.putdeviceparams", zputdeviceparams},
	{"1.setdevice", zsetdevice},
END_OP_DEFS(0) }
