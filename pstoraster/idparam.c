/* Copyright (C) 1992, 1995, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: idparam.c,v 1.3 2000/03/08 23:15:10 mike Exp $ */
/* Utilities for getting parameters out of dictionaries. */
#include "memory_.h"
#include "string_.h"		/* for strlen */
#include "ghost.h"
#include "errors.h"
#include "gsmatrix.h"		/* for dict_matrix_param */
#include "gsuid.h"
#include "idict.h"
#include "idparam.h"		/* interface definition */
#include "ilevel.h"
#include "imemory.h"		/* for iutil.h */
#include "iname.h"
#include "iutil.h"
#include "oper.h"		/* for check_proc */
#include "store.h"		/* for making empty proc */

/* Get a Boolean parameter from a dictionary. */
/* Return 0 if found, 1 if defaulted, <0 if wrong type. */
int
dict_bool_param(const ref * pdict, const char *kstr,
		bool defaultval, bool * pvalue)
{
    ref *pdval;

    if (pdict == 0 || dict_find_string(pdict, kstr, &pdval) <= 0) {
	*pvalue = defaultval;
	return 1;
    }
    if (!r_has_type(pdval, t_boolean))
	return_error(e_typecheck);
    *pvalue = pdval->value.boolval;
    return 0;
}

/* Get an integer or null parameter from a dictionary. */
/* Return 0 if found, 1 if defaulted, <0 if invalid. */
/* If the parameter is null, return 2 without setting *pvalue. */
/* Note that the default value may be out of range, in which case */
/* a missing value will return e_rangecheck rather than 1. */
int
dict_int_null_param(const ref * pdict, const char *kstr, int minval,
		    int maxval, int defaultval, int *pvalue)
{
    ref *pdval;
    int code;
    long ival;

    if (pdict == 0 || dict_find_string(pdict, kstr, &pdval) <= 0) {
	ival = defaultval;
	code = 1;
    } else {
	switch (r_type(pdval)) {
	    case t_integer:
		ival = pdval->value.intval;
		break;
	    case t_real:
		/* Allow an integral real, because Fontographer */
		/* (which violates the Adobe specs in other ways */
		/* as well) sometimes generates output that */
		/* needs this. */
		if (pdval->value.realval < minval || pdval->value.realval > maxval)
		    return_error(e_rangecheck);
		ival = (long)pdval->value.realval;
		if (ival != pdval->value.realval)
		    return_error(e_rangecheck);
		break;
	    case t_null:
		return 2;
	    default:
		return_error(e_typecheck);
	}
	code = 0;
    }
    if (ival < minval || ival > maxval)
	return_error(e_rangecheck);
    *pvalue = (int)ival;
    return code;
}
/* Get an integer parameter from a dictionary. */
/* Return like dict_int_null_param, but return e_typecheck for null. */
int
dict_int_param(const ref * pdict, const char *kstr, int minval, int maxval,
	       int defaultval, int *pvalue)
{
    int code = dict_int_null_param(pdict, kstr, minval, maxval,
				   defaultval, pvalue);

    return (code == 2 ? gs_note_error(e_typecheck) : code);
}

/* Get an unsigned integer parameter from a dictionary. */
/* Return 0 if found, 1 if defaulted, <0 if invalid. */
/* Note that the default value may be out of range, in which case */
/* a missing value will return e_rangecheck rather than 1. */
int
dict_uint_param(const ref * pdict, const char *kstr,
		uint minval, uint maxval, uint defaultval, uint * pvalue)
{
    ref *pdval;
    int code;
    uint ival;

    if (pdict == 0 || dict_find_string(pdict, kstr, &pdval) <= 0) {
	ival = defaultval;
	code = 1;
    } else {
	check_type_only(*pdval, t_integer);
	if (pdval->value.intval != (uint) pdval->value.intval)
	    return_error(e_rangecheck);
	ival = (uint) pdval->value.intval;
	code = 0;
    }
    if (ival < minval || ival > maxval)
	return_error(e_rangecheck);
    *pvalue = ival;
    return code;
}

/* Get a float parameter from a dictionary. */
/* Return 0 if found, 1 if defaulted, <0 if wrong type. */
int
dict_float_param(const ref * pdict, const char *kstr,
		 floatp defaultval, float *pvalue)
{
    ref *pdval;

    if (pdict == 0 || dict_find_string(pdict, kstr, &pdval) <= 0) {
	*pvalue = defaultval;
	return 1;
    }
    switch (r_type(pdval)) {
	case t_integer:
	    *pvalue = pdval->value.intval;
	    return 0;
	case t_real:
	    *pvalue = pdval->value.realval;
	    return 0;
    }
    return_error(e_typecheck);
}

/* Get an integer array from a dictionary. */
/* Return the element count if OK, 0 if missing, <0 if invalid. */
int
dict_int_array_param(const ref * pdict, const char *kstr,
		     uint maxlen, int *ivec)
{
    ref *pdval;
    const ref *pa;
    int *pi = ivec;
    uint size;
    int i;

    if (pdict == 0 || dict_find_string(pdict, kstr, &pdval) <= 0)
	return 0;
    if (!r_has_type(pdval, t_array))
	return_error(e_typecheck);
    size = r_size(pdval);
    if (size > maxlen)
	return_error(e_limitcheck);
    pa = pdval->value.const_refs;
    for (i = 0; i < size; i++, pa++, pi++) {	/* See dict_int_param above for why we allow reals here. */
	switch (r_type(pa)) {
	    case t_integer:
		if (pa->value.intval != (int)pa->value.intval)
		    return_error(e_rangecheck);
		*pi = (int)pa->value.intval;
		break;
	    case t_real:
		if (pa->value.realval < min_int ||
		    pa->value.realval > max_int ||
		    pa->value.realval != (int)pa->value.realval
		    )
		    return_error(e_rangecheck);
		*pi = (int)pa->value.realval;
		break;
	    default:
		return_error(e_typecheck);
	}
    }
    return size;
}

/* Get a float array from a dictionary. */
/* Return the element count if OK, <0 if invalid. */
/* If the parameter is missing, then if defaultvec is NULL, return 0; */
/* if defaultvec is not NULL, copy it into fvec (maxlen elements) */
/* and return maxlen. */
int
dict_float_array_param(const ref * pdict, const char *kstr,
		       uint maxlen, float *fvec, const float *defaultvec)
{
    ref *pdval;
    uint size;
    int code;

    if (pdict == 0 || dict_find_string(pdict, kstr, &pdval) <= 0) {
	if (defaultvec == NULL)
	    return 0;
	memcpy(fvec, defaultvec, maxlen * sizeof(float));

	return maxlen;
    }
    if (!r_has_type(pdval, t_array))
	return_error(e_typecheck);
    size = r_size(pdval);
    if (size > maxlen)
	return_error(e_limitcheck);
    code = float_params(pdval->value.refs + size - 1, size, fvec);
    return (code >= 0 ? size : code);
}

/*
 * Get a procedure from a dictionary.  If the key is missing,
 *      defaultval = false means substitute t__invalid;
 *      defaultval = true means substitute an empty procedure.
 * In either case, return 1.
 */
int
dict_proc_param(const ref * pdict, const char *kstr, ref * pproc,
		bool defaultval)
{
    ref *pdval;

    if (pdict == 0 || dict_find_string(pdict, kstr, &pdval) <= 0) {
	if (defaultval)
	    make_empty_const_array(pproc, a_readonly + a_executable);
	else
	    make_t(pproc, t__invalid);
	return 1;
    }
    check_proc(*pdval);
    *pproc = *pdval;
    return 0;
}

/* Get a matrix from a dictionary. */
int
dict_matrix_param(const ref * pdict, const char *kstr, gs_matrix * pmat)
{
    ref *pdval;

    if (pdict == 0 || dict_find_string(pdict, kstr, &pdval) <= 0)
	return_error(e_typecheck);
    return read_matrix(pdval, pmat);
}

/* Get a UniqueID or XUID from a dictionary. */
/* Return 0 if UniqueID, 1 if XUID, <0 if error. */
/* If there is no uid, return default. */
int
dict_uid_param(const ref * pdict, gs_uid * puid, int defaultval,
	       gs_memory_t * mem)
{
    ref *puniqueid;

    if (pdict == 0) {
	uid_set_invalid(puid);
	return defaultval;
    }
    /* In a Level 2 environment, check for XUID first. */
    if (level2_enabled &&
	dict_find_string(pdict, "XUID", &puniqueid) > 0
	) {
	long *xvalues;
	uint size, i;

	if (!r_has_type(puniqueid, t_array))
	    return_error(e_typecheck);
	size = r_size(puniqueid);
	if (size == 0)
	    return_error(e_rangecheck);
	xvalues = (long *)gs_alloc_byte_array(mem, size, sizeof(long),
					      "get XUID");

	if (xvalues == 0)
	    return_error(e_VMerror);
	/* Get the values from the XUID array. */
	for (i = 0; i < size; i++) {
	    const ref *pvalue = puniqueid->value.const_refs + i;

	    if (!r_has_type(pvalue, t_integer)) {
		gs_free_object(mem, xvalues, "get XUID");
		return_error(e_typecheck);
	    }
	    xvalues[i] = pvalue->value.intval;
	}
	uid_set_XUID(puid, xvalues, size);
	return 1;
    }
    /* If no UniqueID entry, set the UID to invalid, */
    /* because UniqueID need not be present in all fonts, */
    /* and if it is, the legal range is 0 to 2^24-1. */
    if (dict_find_string(pdict, "UniqueID", &puniqueid) <= 0) {
	uid_set_invalid(puid);
	return defaultval;
    } else {
	if (!r_has_type(puniqueid, t_integer) ||
	    puniqueid->value.intval < 0 ||
	    puniqueid->value.intval > 0xffffffL
	    )
	    return_error(e_rangecheck);
	/* Apparently fonts created by Fontographer often have */
	/* a UniqueID of 0, contrary to Adobe's specifications. */
	/* Treat 0 as equivalent to -1 (no UniqueID). */
	if (puniqueid->value.intval == 0) {
	    uid_set_invalid(puid);
	    return defaultval;
	} else
	    uid_set_UniqueID(puid, puniqueid->value.intval);
    }
    return 0;
}

/* Check that a UID in a dictionary is equal to an existing, valid UID. */
bool
dict_check_uid_param(const ref * pdict, const gs_uid * puid)
{
    ref *puniqueid;

    if (uid_is_XUID(puid)) {
	uint size = uid_XUID_size(puid);
	uint i;

	if (dict_find_string(pdict, "XUID", &puniqueid) <= 0)
	    return false;
	if (!r_has_type(puniqueid, t_array) ||
	    r_size(puniqueid) != size
	    )
	    return false;
	for (i = 0; i < size; i++) {
	    const ref *pvalue = puniqueid->value.const_refs + i;

	    if (!r_has_type(pvalue, t_integer))
		return false;
	    if (pvalue->value.intval != uid_XUID_values(puid)[i])
		return false;
	}
	return true;
    } else {
	if (dict_find_string(pdict, "UniqueID", &puniqueid) <= 0)
	    return false;
	return (r_has_type(puniqueid, t_integer) &&
		puniqueid->value.intval == puid->id);
    }
}
