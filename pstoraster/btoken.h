/* Copyright (C) 1990, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: btoken.h,v 1.2 2000/03/08 23:14:19 mike Exp $ */
/* Definitions for Level 2 binary tokens */

#ifndef btoken_INCLUDED
#  define btoken_INCLUDED

/* Define accessors for pointers to the system and user name tables. */
extern ref binary_token_names;	/* array of size 2 */

#define system_names_p (binary_token_names.value.refs)
#define user_names_p (binary_token_names.value.refs + 1)

/* Convert an object to its representation in a binary object sequence. */
int encode_binary_token(P4(const ref * obj, long *ref_offset, long *char_offset,
			   byte * str));

#endif /* btoken_INCLUDED */
