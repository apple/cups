/* Copyright (C) 1995, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: zdevcal.c,v 1.2 2000/03/08 23:15:33 mike Exp $ */
/* %Calendar% IODevice */
#include "time_.h"
#include "ghost.h"
#include "gxiodev.h"
#include "istack.h"
#include "iparam.h"

/* ------ %Calendar% ------ */

private iodev_proc_get_params(calendar_get_params);
const gx_io_device gs_iodev_calendar = {
    "%Calendar%", "Special",
    { iodev_no_init, iodev_no_open_device, iodev_no_open_file,
      iodev_no_fopen, iodev_no_fclose,
      iodev_no_delete_file, iodev_no_rename_file, iodev_no_file_status,
      iodev_no_enumerate_files, NULL, NULL,
      calendar_get_params, iodev_no_put_params
    }
};

/* Get the date and time. */
private int
calendar_get_params(gx_io_device * iodev, gs_param_list * plist)
{
    int code;
    time_t t;
    struct tm *pltime;
    struct tm ltime;
    static const gs_param_item_t items[] = {
	{"Year", gs_param_type_int, offset_of(struct tm, tm_year)},
	{"Month", gs_param_type_int, offset_of(struct tm, tm_mon)},
	{"Day", gs_param_type_int, offset_of(struct tm, tm_mday)},
	{"Weekday", gs_param_type_int, offset_of(struct tm, tm_wday)},
	{"Hour", gs_param_type_int, offset_of(struct tm, tm_hour)},
	{"Minute", gs_param_type_int, offset_of(struct tm, tm_min)},
	{"Second", gs_param_type_int, offset_of(struct tm, tm_sec)},
	gs_param_item_end
    };
    bool running;

    if (time(&t) == -1 || (pltime = localtime(&t)) == 0) {
	ltime.tm_sec = ltime.tm_min = ltime.tm_hour =
	    ltime.tm_mday = ltime.tm_mon = ltime.tm_year = 0;
	running = false;
    } else {
	ltime = *pltime;
	ltime.tm_year += 1900;
	ltime.tm_mon++;		/* 1-origin */
	running = true;
    }
    if ((code = gs_param_write_items(plist, &ltime, NULL, items)) < 0)
	return code;
    return param_write_bool(plist, "Running", &running);
}
