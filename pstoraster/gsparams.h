/* Copyright (C) 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gsparams.h,v 1.1 2000/03/13 18:57:56 mike Exp $ */
/* Serializer/expander for gs_parm_list's */

#ifndef gsparams_INCLUDED
#  define gsparams_INCLUDED

/* Initial version 2/1/98 by John Desrosiers (soho@crl.com) */
/* 8/8/98 L. Peter Deutsch (ghost@aladdin.com) Completely redesigned
   to use stream rather than buffer API (but retained former API for
   compatibility as well). */

#include "stream.h"
#include "gsparam.h"

#if 0	/****************/

/* ------ Future interface, implemented in gsparam2.c ------ */

/*
 * Serialize the contents of a gs_param_list, including sub-collections,
 * onto a stream.  The list must be in READ mode.
 */
int gs_param_list_puts(P2(stream *dest, gs_param_list *list));

/*
 * Unserialize a parameter list, including sub-collections, from a stream.
 * The list must be in WRITE mode.
 */
int gs_param_list_gets(P3(stream *src, gs_param_list *list, gs_memory_t *mem));

#else	/****************/

/* ------ Present interface, implemented in gsparams.c ------ */

/*
 * Serialize a parameter list into a buffer.  Return the actual number
 * of bytes required to store the list, or a negative error code.
 * The list was stored successfully iff the return value is positive and
 * less than or equal to the buffer size.  Note that the buffer may be
 * NULL, in which case nothing is stored (but the size is still returned).
 */
int gs_param_list_serialize(P3(gs_param_list *list, byte *buf, int buf_size));

/*
 * Unserialize a parameter list from a buffer.  Return the actual number
 * of bytes occupied by the list, or a negative error code.  The buffer
 * must be void * aligned.
 */
int gs_param_list_unserialize(P2(gs_param_list *list, const byte *buf));

#endif	/****************/

#endif /* gsparams_INCLUDED */
