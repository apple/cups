/* Copyright (C) 1992 Aladdin Enterprises.  All rights reserved.
  
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

/* scftab.c */
/* Tables for CCITTFaxEncode filter */
#include "std.h"
#include "scommon.h"		/* for scf.h */
#include "scf.h"

/* We make this a separate file so that it can be used by */
/* the program that generates the tables for the CCITTFaxDecode filter. */

/* ------ Run encoding tables ------ */

/* Abbreviate cfe_entry to make the file smaller. */
#define e_(c,len) cfe_entry(c,len)

/* Define the end-of-line code. */
/* Code in scfd.c and scfdgen.c knows that the run value is 1. */
const cfe_run cf_run_eol = e_(run_eol_code_value, run_eol_code_length);

/* Define the 1-D code that signals uncompressed data. */
const cfe_run cf1_run_uncompressed = e_(0xf, 12);

/* Define the 2-D run codes. */
const cfe_run cf2_run_pass =
  e_(cf2_run_pass_value, cf2_run_pass_length);
const cfe_run cf2_run_vertical[7] = {
	e_(0x3, 7),
	e_(0x3, 6),
	e_(0x3, 3),
	e_(0x1, 1),
	e_(0x2, 3),
	e_(0x2, 6),
	e_(0x2, 7)
};
const cfe_run cf2_run_horizontal =
  e_(cf2_run_horizontal_value, cf2_run_horizontal_length);
const cfe_run cf2_run_uncompressed = e_(0xf, 10);

/* EOL codes for Group 3 2-D. */
/* Code in scfd.c knows that these are 0...01x. */
const cfe_run cf2_run_eol_1d =
	e_((run_eol_code_value << 1) + 1, run_eol_code_length + 1);
const cfe_run cf2_run_eol_2d =
	e_((run_eol_code_value << 1) + 0, run_eol_code_length + 1);

/* White run termination codes. */
const cfe_run far_data cf_white_termination[64] = {
	e_(0x35, 8), e_(0x7, 6),  e_(0x7, 4),  e_(0x8, 4),
	e_(0xb, 4),  e_(0xc, 4),  e_(0xe, 4),  e_(0xf, 4),
	e_(0x13, 5), e_(0x14, 5), e_(0x7, 5),  e_(0x8, 5),
	e_(0x8, 6),  e_(0x3, 6),  e_(0x34, 6), e_(0x35, 6),
	e_(0x2a, 6), e_(0x2b, 6), e_(0x27, 7), e_(0xc, 7),
	e_(0x8, 7),  e_(0x17, 7), e_(0x3, 7),  e_(0x4, 7),
	e_(0x28, 7), e_(0x2b, 7), e_(0x13, 7), e_(0x24, 7),
	e_(0x18, 7), e_(0x2, 8),  e_(0x3, 8),  e_(0x1a, 8),
	e_(0x1b, 8), e_(0x12, 8), e_(0x13, 8), e_(0x14, 8),
	e_(0x15, 8), e_(0x16, 8), e_(0x17, 8), e_(0x28, 8),
	e_(0x29, 8), e_(0x2a, 8), e_(0x2b, 8), e_(0x2c, 8),
	e_(0x2d, 8), e_(0x4, 8),  e_(0x5, 8),  e_(0xa, 8),
	e_(0xb, 8),  e_(0x52, 8), e_(0x53, 8), e_(0x54, 8),
	e_(0x55, 8), e_(0x24, 8), e_(0x25, 8), e_(0x58, 8),
	e_(0x59, 8), e_(0x5a, 8), e_(0x5b, 8), e_(0x4a, 8),
	e_(0x4b, 8), e_(0x32, 8), e_(0x33, 8), e_(0x34, 8)
};

/* White run make-up codes. */
const cfe_run far_data cf_white_make_up[41] = {
	e_(0, 0) /* dummy */, e_(0x1b, 5), e_(0x12, 5), e_(0x17, 6),
	e_(0x37, 7), e_(0x36, 8), e_(0x37, 8), e_(0x64, 8),
	e_(0x65, 8), e_(0x68, 8), e_(0x67, 8), e_(0xcc, 9),
	e_(0xcd, 9), e_(0xd2, 9), e_(0xd3, 9), e_(0xd4, 9),
	e_(0xd5, 9), e_(0xd6, 9), e_(0xd7, 9), e_(0xd8, 9),
	e_(0xd9, 9), e_(0xda, 9), e_(0xdb, 9), e_(0x98, 9),
	e_(0x99, 9), e_(0x9a, 9), e_(0x18, 6), e_(0x9b, 9),
	e_(0x8, 11), e_(0xc, 11), e_(0xd, 11), e_(0x12, 12),
	e_(0x13, 12), e_(0x14, 12), e_(0x15, 12), e_(0x16, 12),
	e_(0x17, 12), e_(0x1c, 12), e_(0x1d, 12), e_(0x1e, 12),
	e_(0x1f, 12)
};

/* Black run termination codes. */
const cfe_run far_data cf_black_termination[64] = {
	e_(0x37, 10), e_(0x2, 3),   e_(0x3, 2),   e_(0x2, 2),
	e_(0x3, 3),   e_(0x3, 4),   e_(0x2, 4),   e_(0x3, 5),
	e_(0x5, 6),   e_(0x4, 6),   e_(0x4, 7),   e_(0x5, 7),
	e_(0x7, 7),   e_(0x4, 8),   e_(0x7, 8),   e_(0x18, 9),
	e_(0x17, 10), e_(0x18, 10), e_(0x8, 10),  e_(0x67, 11),
	e_(0x68, 11), e_(0x6c, 11), e_(0x37, 11), e_(0x28, 11),
	e_(0x17, 11), e_(0x18, 11), e_(0xca, 12), e_(0xcb, 12),
	e_(0xcc, 12), e_(0xcd, 12), e_(0x68, 12), e_(0x69, 12),
	e_(0x6a, 12), e_(0x6b, 12), e_(0xd2, 12), e_(0xd3, 12),
	e_(0xd4, 12), e_(0xd5, 12), e_(0xd6, 12), e_(0xd7, 12),
	e_(0x6c, 12), e_(0x6d, 12), e_(0xda, 12), e_(0xdb, 12),
	e_(0x54, 12), e_(0x55, 12), e_(0x56, 12), e_(0x57, 12),
	e_(0x64, 12), e_(0x65, 12), e_(0x52, 12), e_(0x53, 12),
	e_(0x24, 12), e_(0x37, 12), e_(0x38, 12), e_(0x27, 12),
	e_(0x28, 12), e_(0x58, 12), e_(0x59, 12), e_(0x2b, 12),
	e_(0x2c, 12), e_(0x5a, 12), e_(0x66, 12), e_(0x67, 12)
};

/* Black run make-up codes. */
const cfe_run far_data cf_black_make_up[41] = {
	e_(0, 0) /* dummy */, e_(0xf, 10), e_(0xc8, 12), e_(0xc9, 12),
	e_(0x5b, 12), e_(0x33, 12), e_(0x34, 12), e_(0x35, 12),
	e_(0x6c, 13), e_(0x6d, 13), e_(0x4a, 13), e_(0x4b, 13),
	e_(0x4c, 13), e_(0x4d, 13), e_(0x72, 13), e_(0x73, 13),
	e_(0x74, 13), e_(0x75, 13), e_(0x76, 13), e_(0x77, 13),
	e_(0x52, 13), e_(0x53, 13), e_(0x54, 13), e_(0x55, 13),
	e_(0x5a, 13), e_(0x5b, 13), e_(0x64, 13), e_(0x65, 13),
	e_(0x8, 11), e_(0xc, 11), e_(0xd, 11), e_(0x12, 12),
	e_(0x13, 12), e_(0x14, 12), e_(0x15, 12), e_(0x16, 12),
	e_(0x17, 12), e_(0x1c, 12), e_(0x1d, 12), e_(0x1e, 12),
	e_(0x1f, 12)
};

/* Uncompressed codes. */
const cfe_run cf_uncompressed[6] = {
	e_(1, 1),
	e_(1, 2),
	e_(1, 3),
	e_(1, 4),
	e_(1, 5),
	e_(1, 6)
};

/* Uncompressed exit codes. */
const cfe_run cf_uncompressed_exit[10] = {
	e_(2, 8), e_(3, 8),
	e_(2, 9), e_(3, 9),
	e_(2, 10), e_(3, 10),
	e_(2, 11), e_(3, 11),
	e_(2, 12), e_(3, 12)
};

/* Some C compilers insist on having executable code in every file.... */
void
cfe_dummy(void)
{
}
