/* Copyright (C) 1989, 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gconfig.c,v 1.1 2000/03/08 23:14:21 mike Exp $ */
/* Configuration tables */
#include "memory_.h"
#include "gx.h"
#include "gscdefs.h"		/* interface */
#include "gconf.h"		/* for #defines */
#include "gxdevice.h"
#include "gxiodev.h"

/*
 * The makefile generates the file gconfig.h, which consists of
 * lines of the form
 *      device_(gs_xxx_device)
 * or
 *      device2_(gs_xxx_device)
 * for each installed device;
 *      emulator_("emulator", strlen("emulator"))
 * for each known emulator;
 *      init_(gs_xxx_init)
 * for each initialization procedure;
 *      io_device_(gs_iodev_xxx)
 * for each known IODevice;
 *      oper_(xxx_op_defs)
 * for each operator option;
 *      psfile_("gs_xxxx.ps", strlen("gs_xxxx.ps"))
 * for each optional initialization file.
 *
 * We include this file multiple times to generate various different
 * source structures.  (It's a hack, but we haven't come up with anything
 * more satisfactory.)
 */

/* ---------------- Resources (devices, inits, IODevices) ---------------- */

/* Declare devices, init procedures, and IODevices as extern. */
#define device_(dev) extern far_data gx_device dev;
#define device2_(dev) extern const gx_device dev;
#define init_(proc) extern void proc(P1(gs_memory_t *));
#define io_device_(iodev) extern const gx_io_device iodev;
#include "gconf.h"
#undef init_
#undef io_device_
#undef device2_
#undef device_

/* Set up the initialization procedure table. */
extern_gx_init_table();
private void gconf_init(P1(gs_memory_t *));
#define init_(proc) proc,
const gx_init_proc gx_init_table[] = {
#include "gconf.h"
    gconf_init,
    0
};
#undef init_

/* Set up the IODevice table.  The first entry must be %os%, */
/* since it is the default for files with no explicit device specified. */
extern_gx_io_device_table();
extern gx_io_device gs_iodev_os;
#define io_device_(iodev) &iodev,
const gx_io_device *const gx_io_device_table[] = {
    &gs_iodev_os,
#include "gconf.h"
    0
};
#undef io_device_
const uint gx_io_device_table_count = countof(gx_io_device_table) - 1;

/* Set up the device table. */
#define device_(dev) (const gx_device *)&dev,
#define device2_(dev) &dev,
private const gx_device *const gx_device_list[] = {
#include "gconf.h"
	 0
};
#undef device2_
#undef device_

/* Allocate and initialize structure descriptors for the devices. */
private gs_memory_struct_type_t gx_device_st_list[countof(gx_device_list) - 1];
private void
gconf_init(gs_memory_t *mem)
{
    int i;

    for (i = 0; i < countof(gx_device_list) - 1; ++i)
	gx_device_make_struct_type(&gx_device_st_list[i], gx_device_list[i]);
}

/* Return the list of device prototypes, the list of their structure */
/* descriptors, and (as the value) the length of the lists. */
extern_gs_lib_device_list();
int
gs_lib_device_list(const gx_device * const **plist,
		   gs_memory_struct_type_t ** pst)
{
    if (plist != 0)
	*plist = gx_device_list;
    if (pst != 0)
	*pst = gx_device_st_list;
    return countof(gx_device_list) - 1;
}
