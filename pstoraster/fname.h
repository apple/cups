/* Copyright (C) 1993 Aladdin Enterprises.  All rights reserved.
  
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

/* fname.h */
/* File name parsing interface */
/* Requires gxiodev.h */

/* Parsed file name type.  Note that the file name may be either a */
/* gs_string (no terminator) or a C string (null terminator). */
typedef struct parsed_file_name_s {
	gx_io_device *iodev;
	const char *fname;
	uint len;
} parsed_file_name;
int parse_file_name(P2(const ref *, parsed_file_name *));
int parse_real_file_name(P3(const ref *, parsed_file_name *, client_name_t));
int terminate_file_name(P2(parsed_file_name *, client_name_t));
void free_file_name(P2(parsed_file_name *, client_name_t));
