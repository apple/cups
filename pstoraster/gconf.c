/* Copyright (C) 1989, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gconf.c */
/* Configuration tables */
#include "gx.h"
#include "gscdefs.h"		/* interface */
#include "gconfig.h"		/* for #defines */
/*
 * Since we only declare variables of type gx_device *,
 * it should be sufficient to define struct gx_device_s as
 * an abstract (undefined) structure.  However, the VAX VMS compiler
 * isn't happy with this, so we have to include the full definition.
 */
#include "gxdevice.h"
#include "gxiodev.h"

/*
 * The makefile generates the file gconfig.h, which consists of
 * lines of the form
 *	device_(gs_xxx_device)
 * for each installed device;
 *	emulator_("emulator")
 * for each known emulator;
 *	init_(gs_xxx_init)
 * for each initialization procedure;
 *	io_device_(gs_iodev_xxx)
 * for each known IODevice;
 *	oper_(xxx_op_defs)
 * for each operator option;
 *	psfile_("gs_xxxx.ps")
 * for each optional initialization file.
 *
 * We include this file multiple times to generate various different
 * source structures.  (It's a hack, but we haven't come up with anything
 * more satisfactory.)
 */

/* ---------------- Resources (devices, inits, IODevices) ---------------- */

/* Declare devices, init procedures, and IODevices as extern. */
#define device_(dev) extern far_data gx_device dev;
#define init_(proc) extern void proc(P1(gs_memory_t *));
#define io_device_(iodev) extern gx_io_device iodev;
#include "gconfig.h"
#undef init_
#undef io_device_
#undef device_

/* Set up the initialization procedure table. */
extern_gx_init_table();
#define init_(proc) proc,
void (*gx_init_table[])(P1(gs_memory_t *)) = {
#include "gconfig.h"
	0
};
#undef init_

/* Set up the IODevice table.  The first entry must be %os%, */
/* since it is the default for files with no explicit device specified. */
extern_gx_io_device_table();
extern gx_io_device gs_iodev_os;
#define io_device_(iodev) &iodev,
gx_io_device *gx_io_device_table[] = {
	&gs_iodev_os,
#include "gconfig.h"
	0
};
#undef io_device_
uint gx_io_device_table_count = countof(gx_io_device_table) - 1;

/* Set up the device table. */
#define device_(dev) &dev,
private const gx_device *gx_device_list[] = {
#include "gconfig.h"
	0
};
#undef device_

/*
 * Allocate structure descriptors for the devices.
 * We can't fill in the structure sizes, because we don't know them
 * statically, and we also don't know statically which devices are
 * forwarders; so we fill all of this in when we need to
 * (in gs_copydevice in gsdevice.c).
 */
#define device_(dev) { 0 },
/* Because of a bug in the Borland C++ 4.5 compiler, */
/* we can't declare the following far_data but not static. */
static /*private*/ far_data gs_memory_struct_type_t gx_device_st_list[] = {
#include "gconfig.h"
	{ 0 }
};
#undef device_

/* Return the list of device prototypes, the list of their structure */
/* descriptors, and (as the value) the length of the lists. */
extern_gs_lib_device_list();
int
gs_lib_device_list(const gx_device ***plist, gs_memory_struct_type_t **pst)
{	if ( plist != 0 )
	  *plist = gx_device_list;
	if ( pst != 0 )
	  *pst = gx_device_st_list;
	return countof(gx_device_list) - 1;
}
