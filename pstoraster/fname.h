/* Copyright (C) 1993, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: fname.h,v 1.2 2000/03/08 23:14:21 mike Exp $ */
/* Requires gxiodev.h */

#ifndef fname_INCLUDED
#  define fname_INCLUDED

/*
 * Define a structure for representing a parsed file name, consisting of
 * an IODevice name in %'s, a file name, or both.  Note that the file name
 * may be either a gs_string (no terminator) or a C string (null terminator).
 */
typedef struct parsed_file_name_s {
    gx_io_device *iodev;
    const char *fname;
    uint len;
} parsed_file_name;

/* Parse a file name into device and individual name. */
int parse_file_name(P2(const ref *, parsed_file_name *));

/* Parse a real (non-device) file name and convert to a C string. */
int parse_real_file_name(P3(const ref *, parsed_file_name *, client_name_t));

/* Convert a file name to a C string by adding a null terminator. */
int terminate_file_name(P2(parsed_file_name *, client_name_t));

/* Free a file name that was copied to a C string. */
void free_file_name(P2(parsed_file_name *, client_name_t));

#endif /* fname_INCLUDED */
