/* Copyright (C) 1992, 1993, 1994, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: idparam.h,v 1.2 2000/03/08 23:15:10 mike Exp $ */
/* Interface to idparam.c */

#ifndef idparam_INCLUDED
#  define idparam_INCLUDED

#ifndef gs_matrix_DEFINED
#  define gs_matrix_DEFINED
typedef struct gs_matrix_s gs_matrix;

#endif

#ifndef gs_uid_DEFINED
#  define gs_uid_DEFINED
typedef struct gs_uid_s gs_uid;

#endif

/*
 * Unless otherwise noted, all the following routines return 0 for
 * a valid parameter, 1 for a defaulted parameter, or <0 on error.
 *
 * Note that all the dictionary parameter routines take a C string,
 * not a t_name ref *.  Even though this is slower, it means that
 * the GC doesn't have to worry about finding statically declared
 * name refs, and we have that many fewer static variables.
 *
 * All these routines allow pdict == NULL, which they treat the same as
 * pdict referring to an empty dictionary.  Routines with "null" in their
 * name return 2 if the parameter is null, without setting *pvalue.
 */
int dict_bool_param(P4(const ref * pdict, const char *kstr,
		       bool defaultval, bool * pvalue));
int dict_int_param(P6(const ref * pdict, const char *kstr,
		      int minval, int maxval, int defaultval, int *pvalue));
int dict_int_null_param(P6(const ref * pdict, const char *kstr,
			   int minval, int maxval, int defaultval,
			   int *pvalue));
int dict_uint_param(P6(const ref * pdict, const char *kstr,
		       uint minval, uint maxval, uint defaultval,
		       uint * pvalue));
int dict_float_param(P4(const ref * pdict, const char *kstr,
			floatp defaultval, float *pvalue));
int dict_int_array_param(P4(const ref * pdict, const char *kstr,
			    uint maxlen, int *ivec));
int dict_float_array_param(P5(const ref * pdict, const char *kstr,
			      uint maxlen, float *fvec,
			      const float *defaultvec));

/*
 * For dict_proc_param,
 *      defaultval = false means substitute t__invalid;
 *      defaultval = true means substitute an empty procedure.
 * In either case, return 1.
 */
int dict_proc_param(P4(const ref * pdict, const char *kstr, ref * pproc,
		       bool defaultval));
int dict_matrix_param(P3(const ref * pdict, const char *kstr,
			 gs_matrix * pmat));
int dict_uid_param(P4(const ref * pdict, gs_uid * puid, int defaultval,
		      gs_memory_t * mem));

/* Check that a UID in a dictionary is equal to an existing, valid UID. */
bool dict_check_uid_param(P2(const ref * pdict, const gs_uid * puid));

#endif /* idparam_INCLUDED */
