/* Copyright (C) 1992, 1993 Aladdin Enterprises.  All rights reserved.
  
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

/* gsutil.h */
/* Prototypes for procedures in gsutil.c */

#ifndef gsutil_INCLUDED
#  define gsutil_INCLUDED

/* ------ Unique IDs ------ */

/* Generate a block of unique IDs. */
gs_id gs_next_ids(P1(uint count));

/* ------ Memory utilities ------ */

/* Transpose an 8 x 8 block of bits. */
/* line_size is the raster of the input data; */
/* dist is the distance between output bytes. */
/* Dot matrix printers need this. */
/* Note that with a negative dist value, */
/* this will rotate an 8 x 8 block 90 degrees counter-clockwise. */
void memflip8x8(P4(const byte *inp, int line_size, byte *outp, int dist));

/* ------ String utilities ------ */

/* Compare two strings, returning -1 if the first is less, */
/* 0 if they are equal, and 1 if first is greater. */
/* We can't use memcmp, because we always use unsigned characters. */
int bytes_compare(P4(const byte *str1, uint len1,
		     const byte *str2, uint len2));

/* Test whether a string matches a pattern with wildcards. */
/* If psmp == NULL, use standard parameters: '*' = any substring, */
/* '?' = any character, '\\' quotes next character, don't ignore case. */
typedef struct string_match_params_s {
	int any_substring;		/* '*' */
	int any_char;			/* '?' */
	int quote_next;			/* '\\' */
	bool ignore_case;
} string_match_params;
bool string_match(P5(const byte *str, uint len,
		     const byte *pstr, uint plen,
		     const string_match_params *psmp));

#endif					/* gsutil_INCLUDED */
