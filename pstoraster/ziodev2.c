/* Copyright (C) 1993, 1994, 1996, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: ziodev2.c,v 1.2 2000/03/08 23:15:39 mike Exp $ */
/* (Level 2) IODevice operators */
#include "string_.h"
#include "ghost.h"
#include "gp.h"
#include "oper.h"
#include "stream.h"
#include "gxiodev.h"
#include "dstack.h"		/* for systemdict */
#include "files.h"		/* for file_open_stream */
#include "iparam.h"
#include "iutil2.h"
#include "store.h"

/* ------ %null% ------ */

/* This represents the null output file. */
private iodev_proc_open_device(null_open);
const gx_io_device gs_iodev_null = {
    "%null%", "FileSystem",
    {
	iodev_no_init, null_open, iodev_no_open_file,
	iodev_os_fopen, iodev_os_fclose,
	iodev_no_delete_file, iodev_no_rename_file, iodev_no_file_status,
	iodev_no_enumerate_files, NULL, NULL,
	iodev_no_get_params, iodev_no_put_params
    }
};

private int
null_open(gx_io_device * iodev, const char *access, stream ** ps,
	  gs_memory_t * mem)
{
    if (!streq1(access, 'w'))
	return_error(e_invalidfileaccess);
    return file_open_stream(gp_null_file_name,
			    strlen(gp_null_file_name),
			    access, 256 /* arbitrary */ , ps,
			    iodev->procs.fopen);
}

/* ------ %ram% ------ */

/* This is an IODevice with no interesting parameters for the moment. */
const gx_io_device gs_iodev_ram = {
    "%ram%", "Special",
    {
	iodev_no_init, iodev_no_open_device, iodev_no_open_file,
	iodev_no_fopen, iodev_no_fclose,
	iodev_no_delete_file, iodev_no_rename_file, iodev_no_file_status,
	iodev_no_enumerate_files, NULL, NULL,
	iodev_no_get_params, iodev_no_put_params
    }
};

/* ------ Operators ------ */

/* <iodevice> .getdevparams <mark> <name> <value> ... */
private int
zgetdevparams(os_ptr op)
{
    gx_io_device *iodev;
    stack_param_list list;
    gs_param_list *const plist = (gs_param_list *) & list;
    int code;
    ref *pmark;

    check_read_type(*op, t_string);
    iodev = gs_findiodevice(op->value.bytes, r_size(op));
    if (iodev == 0)
	return_error(e_undefinedfilename);
    stack_param_list_write(&list, &o_stack, NULL);
    if ((code = gs_getdevparams(iodev, plist)) < 0) {
	ref_stack_pop(&o_stack, list.count * 2);
	return code;
    }
    pmark = ref_stack_index(&o_stack, list.count * 2);
    make_mark(pmark);
    return 0;
}

/* <mark> <name> <value> ... <iodevice> .putdevparams */
private int
zputdevparams(os_ptr op)
{
    gx_io_device *iodev;
    stack_param_list list;
    gs_param_list *const plist = (gs_param_list *) & list;
    int code;
    password system_params_password;

    check_read_type(*op, t_string);
    iodev = gs_findiodevice(op->value.bytes, r_size(op));
    if (iodev == 0)
	return_error(e_undefinedfilename);
    code = stack_param_list_read(&list, &o_stack, 1, NULL, false);
    if (code < 0)
	return code;
    code = dict_read_password(&system_params_password, systemdict,
			      "SystemParamsPassword");
    if (code < 0)
	return code;
    code = param_check_password(plist, &system_params_password);
    if (code != 0) {
	iparam_list_release(&list);
	return_error(code < 0 ? code : e_invalidaccess);
    }
    code = gs_putdevparams(iodev, plist);
    iparam_list_release(&list);
    if (code < 0)
	return code;
    ref_stack_pop(&o_stack, list.count * 2 + 2);
    return 0;
}

/* ------ Initialization procedure ------ */

const op_def ziodev2_l2_op_defs[] =
{
    op_def_begin_level2(),
    {"1.getdevparams", zgetdevparams},
    {"2.putdevparams", zputdevparams},
    op_def_end(0)
};
