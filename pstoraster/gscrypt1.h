/* Copyright (C) 1990, 1992, 1997 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gscrypt1.h,v 1.2 2000/03/08 23:14:38 mike Exp $ */
/* Interface to Adobe Type 1 encryption/decryption. */

#ifndef gscrypt1_INCLUDED
#  define gscrypt1_INCLUDED

/* Normal public interface */
typedef ushort crypt_state;
int gs_type1_encrypt(P4(byte * dest, const byte * src, uint len,
			crypt_state * pstate));
int gs_type1_decrypt(P4(byte * dest, const byte * src, uint len,
			crypt_state * pstate));

/* Define the encryption parameters and procedures. */
#define crypt_c1 ((ushort)52845)
#define crypt_c2 ((ushort)22719)
/* c1 * c1' == 1 mod 2^16. */
#define crypt_c1_inverse ((ushort)27493)
#define encrypt_next(ch, state, chvar)\
  (chvar = ((ch) ^ (state >> 8)),\
   state = (chvar + state) * crypt_c1 + crypt_c2)
#define decrypt_this(ch, state)\
  ((ch) ^ (state >> 8))
#define decrypt_next(ch, state, chvar)\
  (chvar = decrypt_this(ch, state),\
   decrypt_skip_next(ch, state))
#define decrypt_skip_next(ch, state)\
  (state = ((ch) + state) * crypt_c1 + crypt_c2)
#define decrypt_skip_previous(ch, state)\
  (state = (state - crypt_c2) * crypt_c1_inverse - (ch))

#endif /* gscrypt1_INCLUDED */
