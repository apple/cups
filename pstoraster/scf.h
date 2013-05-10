/* Copyright (C) 1992, 1995 Aladdin Enterprises.  All rights reserved.
  
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

/* scf.h */
/* Common definitions for CCITTFax encoding and decoding filters */
#include "shc.h"

/*
 * The CCITT Group 3 (T.4) and Group 4 (T.6) fax specifications map
 * run lengths to Huffman codes.  White and black have different mappings.
 * If the run length is 64 or greater, two or more codes are needed:
 *	- One or more 'make-up' codes for 2560 pixels;
 *	- A 'make-up' code that encodes the multiple of 64;
 *	- A 'termination' code for the remainder.
 * For runs of 63 or less, only the 'termination' code is needed.
 */

/* ------ Encoding tables ------ */

/* Define the maximum length of a scan line that we can encode. */
/* Even though the natural values for this are 2560 * N + 63, */
/* we don't want to impose a limit significantly smaller than max_int. */
#define cfe_max_width 32000
#define cfe_max_makeups (cfe_max_width / 2560)
#define cfe_max_code_bytes (cfe_max_makeups * 2 + 2) /* conservative */

typedef hce_code cfe_run;
#define cfe_entry(c, len) hce_entry(c, len)

/* Codes common to 1-D and 2-D encoding. */
/* The decoding algorithms know that EOL is 0....01. */
#define run_eol_code_length 12
#define run_eol_code_value 1
extern const cfe_run cf_run_eol;
extern const cfe_run far_data cf_white_termination[64];
extern const cfe_run far_data cf_white_make_up[41];
extern const cfe_run far_data cf_black_termination[64];
extern const cfe_run far_data cf_black_make_up[41];
extern const cfe_run cf_uncompressed[6];
extern const cfe_run cf_uncompressed_exit[10];	/* indexed by 2 x length of */
			/* white run + (1 if next run black, 0 if white) */
/* 1-D encoding. */
extern const cfe_run cf1_run_uncompressed;
/* 2-D encoding. */
extern const cfe_run cf2_run_pass;
#define cf2_run_pass_length 4
#define cf2_run_pass_value 0x1
#define cf2_run_vertical_offset 3
extern const cfe_run cf2_run_vertical[7];	/* indexed by b1 - a1 + offset */
extern const cfe_run cf2_run_horizontal;
#define cf2_run_horizontal_value 1
#define cf2_run_horizontal_length 3
extern const cfe_run cf2_run_uncompressed;
/* 2-D Group 3 encoding. */
extern const cfe_run cf2_run_eol_1d;
extern const cfe_run cf2_run_eol_2d;

/* ------ Decoding tables ------ */

typedef hcd_code cfd_node;
#define run_length value

/*
 * The value in the decoding tables is either a white or black run length,
 * or a (negative) exceptional value.
 */
#define run_error (-1)
#define run_zeros (-2)	/* EOL follows, possibly with more padding first */
#define run_uncompressed (-3)
/* 2-D codes */
#define run2_pass (-4)
#define run2_horizontal (-5)

#define cfd_white_initial_bits 8
extern const cfd_node far_data cf_white_decode[];
#define cfd_black_initial_bits 7
extern const cfd_node far_data cf_black_decode[];
#define cfd_2d_initial_bits 7
extern const cfd_node far_data cf_2d_decode[];
#define cfd_uncompressed_initial_bits 6		/* must be 6 */
extern const cfd_node far_data cf_uncompressed_decode[];

/* ------ Run detection macros ------ */

/*
 * For the run detection macros:
 *   white_byte is 0 or 0xff for BlackIs1 or !BlackIs1 respectively;
 *   data holds p[-1], inverted if !BlackIs1;
 *   count is the number of valid bits remaining in the scan line.
 */

/* Aliases for bit processing tables. */
#define cf_byte_run_length byte_bit_run_length_neg
#define cf_byte_run_length_0 byte_bit_run_length_0
	  
/* Skip over white pixels to find the next black pixel in the input. */
/* Store the run length in rlen, and update data, p, and count. */
/* There are many more white pixels in typical input than black pixels, */
/* and the runs of white pixels tend to be much longer, so we use */
/* substantially different loops for the two cases. */

#define skip_white_pixels(data, p, count, white_byte, rlen)\
{ if ( (rlen = cf_byte_run_length[count & 7][data ^ 0xff]) >= 8 )\
    { if ( white_byte == 0 )\
	{ if ( p[0] ) { data = p[0]; p += 1; rlen -= 8; }\
	  else if ( p[1] ) { data = p[1]; p += 2; }\
	  else\
	    { while ( !(p[2] | p[3] | p[4] | p[5]) ) p += 4, rlen += 32;\
	      if ( p[2] ) { data = p[2]; p += 3; rlen += 8; }\
	      else if ( p[3] ) { data = p[3]; p += 4; rlen += 16; }\
	      else if ( p[4] ) { data = p[4]; p += 5; rlen += 24; }\
	      else /* p[5] */ { data = p[5]; p += 6; rlen += 32; }\
	    }\
	}\
      else\
	{ if ( p[0] != 0xff ) { data = (byte)~p[0]; p += 1; rlen -= 8; }\
	  else if ( p[1] != 0xff ) { data = (byte)~p[1]; p += 2; }\
	  else\
	    { while ( (p[2] & p[3] & p[4] & p[5]) == 0xff ) p += 4, rlen += 32;\
	      if ( p[2] != 0xff ) { data = (byte)~p[2]; p += 3; rlen += 8; }\
	      else if ( p[3] != 0xff ) { data = (byte)~p[3]; p += 4; rlen += 16; }\
	      else if ( p[4] != 0xff ) { data = (byte)~p[4]; p += 5; rlen += 24; }\
	      else /* p[5] != 0xff */ { data = (byte)~p[5]; p += 6; rlen += 32; }\
	    }\
	}\
     rlen += cf_byte_run_length_0[data ^ 0xff];\
   }\
  count -= rlen;\
}

/* Skip over black pixels to find the next white pixel in the input. */
/* Store the run length in rlen, and update data, p, and count. */

#define skip_black_pixels(data, p, count, white_byte, rlen)\
{ if ( (rlen = cf_byte_run_length[count & 7][data]) >= 8 )\
   { if ( white_byte == 0 )\
      for ( ; ; p += 4, rlen += 32 )\
      { if ( p[0] != 0xff ) { data = p[0]; p += 1; rlen -= 8; break; }\
	if ( p[1] != 0xff ) { data = p[1]; p += 2; break; }\
	if ( p[2] != 0xff ) { data = p[2]; p += 3; rlen += 8; break; }\
	if ( p[3] != 0xff ) { data = p[3]; p += 4; rlen += 16; break; }\
      }\
     else\
      for ( ; ; p += 4, rlen += 32 )\
      { if ( p[0] ) { data = (byte)~p[0]; p += 1; rlen -= 8; break; }\
	if ( p[1] ) { data = (byte)~p[1]; p += 2; break; }\
	if ( p[2] ) { data = (byte)~p[2]; p += 3; rlen += 8; break; }\
	if ( p[3] ) { data = (byte)~p[3]; p += 4; rlen += 16; break; }\
      }\
     rlen += cf_byte_run_length_0[data];\
   }\
  count -= rlen;\
}
