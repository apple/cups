/* Copyright (C) 1989, 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: iutil.c,v 1.2 2000/03/08 23:15:17 mike Exp $ */
/* Utilities for Ghostscript interpreter */
#include "math_.h"		/* for fabs */
#include "memory_.h"
#include "string_.h"
#include "ghost.h"
#include "errors.h"
#include "idict.h"
#include "imemory.h"
#include "iname.h"
#include "ipacked.h"		/* for array_get */
#include "iutil.h"		/* for checking prototypes */
#include "ivmspace.h"
#include "oper.h"
#include "store.h"
#include "gsccode.h"		/* for gxfont.h */
#include "gsmatrix.h"
#include "gsutil.h"
#include "gxfont.h"

/* ------ Object utilities ------ */

/* Define the table of ref type properties. */
const byte ref_type_properties[] =
{
    ref_type_properties_data
};

/* Copy refs from one place to another. */
int
refcpy_to_old(ref * aref, uint index, const ref * from,
	      uint size, client_name_t cname)
{
    ref *to = aref->value.refs + index;
    int code = refs_check_space(from, size, r_space(aref));

    if (code < 0)
	return code;
    /* We have to worry about aliasing.... */
    if (to <= from || from + size <= to)
	while (size--)
	    ref_assign_old(aref, to, from, cname), to++, from++;
    else
	for (from += size, to += size; size--;)
	    from--, to--, ref_assign_old(aref, to, from, cname);
    return 0;
}
void
refcpy_to_new(ref * to, const ref * from, uint size)
{
    while (size--)
	ref_assign_new(to, from), to++, from++;
}

/* Fill a new object with nulls. */
void
refset_null(ref * to, uint size)
{
    while (size--)
	make_null_new(to), to++;
}

/* Compare two objects for equality. */
bool
obj_eq(const ref * pref1, const ref * pref2)
{
    ref nref;

    if (r_type(pref1) != r_type(pref2)) {
	/*
	 * Only a few cases need be considered here:
	 * integer/real (and vice versa), name/string (and vice versa),
	 * and extended operators.
	 */
	switch (r_type(pref1)) {
	    case t_integer:
		return (r_has_type(pref2, t_real) &&
			pref2->value.realval == pref1->value.intval);
	    case t_real:
		return (r_has_type(pref2, t_integer) &&
			pref2->value.intval == pref1->value.realval);
	    case t_name:
		if (!r_has_type(pref2, t_string))
		    return false;
		name_string_ref(pref1, &nref);
		pref1 = &nref;
		break;
	    case t_string:
		if (!r_has_type(pref2, t_name))
		    return false;
		name_string_ref(pref2, &nref);
		pref2 = &nref;
		break;
	    default:
		if (r_btype(pref1) != r_btype(pref2))
		    return false;
	}
    }
    /*
     * Now do a type-dependent comparison.  This would be very simple if we
     * always filled in all the bytes of a ref, but we currently don't.
     */
    switch (r_btype(pref1)) {
	case t_array:
	    return (pref1->value.refs == pref2->value.refs &&
		    r_size(pref1) == r_size(pref2));
	case t_mixedarray:
	case t_shortarray:
	    return (pref1->value.packed == pref2->value.packed &&
		    r_size(pref1) == r_size(pref2));
	case t_boolean:
	    return (pref1->value.boolval == pref2->value.boolval);
	case t_dictionary:
	    return (pref1->value.pdict == pref2->value.pdict);
	case t_file:
	    return (pref1->value.pfile == pref2->value.pfile &&
		    r_size(pref1) == r_size(pref2));
	case t_integer:
	    return (pref1->value.intval == pref2->value.intval);
	case t_mark:
	case t_null:
	    return true;
	case t_name:
	    return (pref1->value.pname == pref2->value.pname);
	case t_oparray:
	case t_operator:
	    return (op_index(pref1) == op_index(pref2));
	case t_real:
	    return (pref1->value.realval == pref2->value.realval);
	case t_save:
	    return (pref2->value.saveid == pref1->value.saveid);
	case t_string:
	    return (!bytes_compare(pref1->value.bytes, r_size(pref1),
				   pref2->value.bytes, r_size(pref2)));
	case t_device:
	    return (pref1->value.pdevice == pref2->value.pdevice);
	case t_struct:
	case t_astruct:
	    return (pref1->value.pstruct == pref2->value.pstruct);
	case t_fontID:
	    {	/*
		 * In the Adobe implementations, different scalings of a
		 * font have "equal" FIDs, so we do the same.
		 */
		const gs_font *pfont1 = r_ptr(pref1, gs_font);
		const gs_font *pfont2 = r_ptr(pref2, gs_font);

		while (pfont1->base != pfont1)
		    pfont1 = pfont1->base;
		while (pfont2->base != pfont2)
		    pfont2 = pfont2->base;
		return (pfont1 == pfont2);
	    }
    }
    return false;		/* shouldn't happen! */
}

/* Compare two objects for identity. */
bool
obj_ident_eq(const ref * pref1, const ref * pref2)
{
    if (r_type(pref1) != r_type(pref2))
	return false;
    if (r_has_type(pref1, t_string))
	return (pref1->value.bytes == pref2->value.bytes &&
		r_size(pref1) == r_size(pref2));
    return obj_eq(pref1, pref2);
}

/*
 * Create a printable representation of an object, a la cvs (full_print =
 * false) or == (full_print = true).  Return 0 if OK, <0 if the destination
 * wasn't large enough or the object's contents weren't readable.
 * If the object was a string or name, store a pointer to its characters
 * even if it was too large.
 */
private void ensure_dot(P1(char *));
int
obj_cvp(const ref * op, byte * str, uint len, uint * prlen,
	const byte ** pchars, bool full_print)
{
    if (full_print)
	switch (r_btype(op)) {
	    case t_boolean:
	    case t_integer:
		break;
	    case t_real:
	    /*
	     * To get fully accurate output results for IEEE single-
	     * precision floats (24 bits of mantissa), the ANSI
	     * %g default of 6 digits is not enough; 9 are needed.
	     * Unfortunately, using %.9g for floats (as opposed to
	     * doubles) produces unfortunate artifacts such as 0.01 5 mul
	     * printing as 0.049999997.  Therefore, we print using %g,
	     * and if the result isn't accurate enough, print again
	     * using %.9g.  Unfortunately, a few PostScript programs
	     * 'know' that the printed representation of floats fits
	     * into 6 digits (e.g., with cvs).  We resolve this by letting
	     * cvs, cvrs, and = do what the Adobe interpreters appear
	     * to do (use %g), and only produce accurate output for ==,
	     * for which there is no analogue of cvs.  What a hack!
	     */
		if (!full_print)
		    break;
		{
		    char buf[30];  /* big enough for any float or double */
		    float value = op->value.realval;
		    float scanned;
		    uint plen;

		    sprintf(buf, "%g", value);
		    sscanf(buf, "%f", &scanned);
		    if (scanned != value)
			sprintf(buf, "%.9g", value);
		    ensure_dot(buf);
		    *prlen = plen = strlen(buf);
		    if (plen > len)
			return_error(e_rangecheck);
		    memcpy(str, buf, plen);
		    return 0;
		}
	    default:
		return_error(e_typecheck);
	}
    return obj_cvs(op, str, len, prlen, pchars);
}
int
obj_cvs(const ref * op, byte * str, uint len, uint * prlen,
	const byte ** pchars)
{
    char buf[30];		/* big enough for any float or double */
    const byte *pstr = (const byte *)buf;
    uint plen;
    ref nref;

    switch (r_btype(op)) {
	case t_boolean:
	    pstr = (const byte *)(op->value.boolval ? "true" : "false");
	    break;
	case t_integer:
	    sprintf(buf, "%ld", op->value.intval);
	    break;
	case t_name:
	    name_string_ref(op, &nref);		/* name string */
cvname:	    pstr = nref.value.bytes;
	    plen = r_size(&nref);
	    if (pchars != 0)
		*pchars = pstr;
	    goto nl;
	case t_oparray:
	    {
		uint index = op_index(op);
		const op_array_table *opt = op_index_op_array_table(index);

		name_index_ref(opt->nx_table[index - opt->base_index], &nref);
	    }
	    name_string_ref(&nref, &nref);
	    goto cvname;
	case t_operator:
	    {
		/* Recover the name from the initialization table. */
		uint index = op_index(op);

		/*
		 * Check the validity of the index.  (An out-of-bounds index
		 * is only possible when examining an invalid object using
		 * the debugger.)
		 */
		if (index > 0 && index < op_def_count) {
		    pstr = (const byte *)(op_def_table[index]->oname + 1);
		    break;
		}
	    }
	    /* Internal operator, no name. */
	    sprintf(buf, "@0x%lx", (ulong) op->value.opproc);
	    break;
	case t_real:
	    sprintf(buf, "%g", op->value.realval);
	    ensure_dot(buf);
	    break;
	case t_string:
	    check_read(*op);
	    pstr = op->value.bytes;
	    plen = r_size(op);
	    if (pchars != 0)
		*pchars = pstr;
	    goto nl;
	default:
	    pstr = (const byte *)"--nostringval--";
    }
    plen = strlen((const char *)pstr);
nl: *prlen = plen;
    if (plen > len)
	return_error(e_rangecheck);
    memcpy(str, pstr, plen);
    return 0;
}
/*
 * Make sure the converted form of a real number has a decimal point.  This
 * is needed for compatibility with Adobe (and other) interpreters.
 */
private void
ensure_dot(char *buf)
{
    if (strchr(buf, '.') == NULL) {
	char *ept = strchr(buf, 'e');

	if (ept == NULL)
	    strcat(buf, ".0");
	else {
	    /* Insert the .0 before the exponent.  What a nuisance! */
	    char buf1[30];

	    strcpy(buf1, ept);
	    strcpy(ept, ".0");
	    strcat(buf, buf1);
	}
    }
}

/* Find the index of an operator that doesn't have one stored in it. */
ushort
op_find_index(const ref * pref /* t_operator */ )
{
    op_proc_p proc = real_opproc(pref);
    const op_def *const *opp = op_def_table;
    const op_def *const *opend = opp + op_def_count;

    for (; ++opp < opend;) {
	if ((*opp)->proc == proc)
	    return opp - op_def_table;
    }
    /* Lookup failed!  This isn't possible.... */
    return 0;
}

/*
 * Convert an operator index to an operator or oparray ref.
 * This is only used for debugging and for 'get' from packed arrays,
 * so it doesn't have to be very fast.
 */
void
op_index_ref(uint index, ref * pref)
{
    const op_array_table *opt;

    if (op_index_is_operator(index)) {
	make_oper(pref, index, op_index_proc(index));
	return;
    }
    opt = op_index_op_array_table(index);
    make_tasv(pref, t_oparray, opt->attrs, index,
	      const_refs, (opt->table.value.const_refs
			   + index - opt->base_index));
}

/* Get an element from an array of some kind. */
/* This is also used to index into Encoding vectors, */
/* the error name vector, etc. */
int
array_get(const ref * aref, long index_long, ref * pref)
{
    if ((ulong)index_long >= r_size(aref))
	return_error(e_rangecheck);
    switch (r_type(aref)) {
	case t_array:
	    {
		const ref *pvalue = aref->value.refs + index_long;

		ref_assign(pref, pvalue);
	    }
	    break;
	case t_mixedarray:
	    {
		const ref_packed *packed = aref->value.packed;
		uint index = (uint)index_long;

		for (; index--;)
		    packed = packed_next(packed);
		packed_get(packed, pref);
	    }
	    break;
	case t_shortarray:
	    {
		const ref_packed *packed = aref->value.packed + index_long;

		packed_get(packed, pref);
	    }
	    break;
	default:
	    return_error(e_typecheck);
    }
    return 0;
}

/* Get an element from a packed array. */
/* (This works for ordinary arrays too.) */
/* Source and destination are allowed to overlap if the source is packed, */
/* or if they are identical. */
void
packed_get(const ref_packed * packed, ref * pref)
{
    const ref_packed elt = *packed;
    uint value = elt & packed_value_mask;

    switch (elt >> r_packed_type_shift) {
	default:		/* (shouldn't happen) */
	    make_null(pref);
	    break;
	case pt_executable_operator:
	    op_index_ref(value, pref);
	    break;
	case pt_integer:
	    make_int(pref, (int)value + packed_min_intval);
	    break;
	case pt_literal_name:
	    name_index_ref(value, pref);
	    break;
	case pt_executable_name:
	    name_index_ref(value, pref);
	    r_set_attrs(pref, a_executable);
	    break;
	case pt_full_ref:
	case pt_full_ref + 1:
	    ref_assign(pref, (const ref *)packed);
    }
}

/* Check to make sure an interval contains no object references */
/* to a space younger than a given one. */
/* Return 0 or e_invalidaccess. */
int
refs_check_space(const ref * bot, uint size, uint space)
{
    for (; size--; bot++)
	store_check_space(space, bot);
    return 0;
}

/* ------ String utilities ------ */

/* Convert a C string to a Ghostscript string */
int
string_to_ref(const char *cstr, ref * pref, gs_ref_memory_t * mem,
	      client_name_t cname)
{
    uint size = strlen(cstr);
    int code = gs_alloc_string_ref(mem, pref, a_all, size, cname);

    if (code < 0)
	return code;
    memcpy(pref->value.bytes, cstr, size);
    return 0;
}

/* Convert a Ghostscript string to a C string. */
/* Return 0 iff the buffer can't be allocated. */
char *
ref_to_string(const ref * pref, gs_memory_t * mem, client_name_t cname)
{
    uint size = r_size(pref);
    char *str = (char *)gs_alloc_string(mem, size + 1, cname);

    if (str == 0)
	return 0;
    memcpy(str, (const char *)pref->value.bytes, size);
    str[size] = 0;
    return str;
}

/* ------ Operand utilities ------ */

/* Get N numeric operands from the stack or an array. */
/* Return a bit-mask indicating which ones are integers, */
/* or a (negative) error indication. */
/* The 1-bit in the bit-mask refers to the first operand. */
/* Store float versions of the operands at pval. */
/* The stack underflow check (check for t__invalid) is harmless */
/* if the operands come from somewhere other than the stack. */
int
num_params(const ref * op, int count, double *pval)
{
    int mask = 0;

    pval += count;
    while (--count >= 0) {
	mask <<= 1;
	switch (r_type(op)) {
	    case t_real:
		*--pval = op->value.realval;
		break;
	    case t_integer:
		*--pval = op->value.intval;
		mask++;
		break;
	    case t__invalid:
		return_error(e_stackunderflow);
	    default:
		return_error(e_typecheck);
	}
	op--;
    }
    /* If count is very large, mask might overflow. */
    /* In this case we clearly don't care about the value of mask. */
    return (mask < 0 ? 0 : mask);
}
/* float_params doesn't bother to keep track of the mask. */
int
float_params(const ref * op, int count, float *pval)
{
    for (pval += count; --count >= 0; --op)
	switch (r_type(op)) {
	    case t_real:
		*--pval = op->value.realval;
		break;
	    case t_integer:
		*--pval = op->value.intval;
		break;
	    case t__invalid:
		return_error(e_stackunderflow);
	    default:
		return_error(e_typecheck);
	}
    return 0;
}

/* Get a single real parameter. */
/* The only possible error is e_typecheck. */
/* If an error is returned, the return value is not updated. */
int
real_param(const ref * op, double *pparam)
{
    switch (r_type(op)) {
	case t_integer:
	    *pparam = op->value.intval;
	    break;
	case t_real:
	    *pparam = op->value.realval;
	    break;
	default:
	    return_error(e_typecheck);
    }
    return 0;
}
int
float_param(const ref * op, float *pparam)
{
    double dval;
    int code = real_param(op, &dval);

    if (code >= 0)
	*pparam = (float)dval;	/* can't overflow */
    return code;
}

/* Get an integer parameter in a given range. */
int
int_param(const ref * op, int max_value, int *pparam)
{
    check_int_leu(*op, max_value);
    *pparam = (int)op->value.intval;
    return 0;
}

/* Make real values on the operand stack. */
int
make_reals(ref * op, const double *pval, int count)
{
    /* This should return e_limitcheck if any real is too large */
    /* to fit into a float on the stack. */
    for (; count--; op++, pval++)
	make_real(op, *pval);
    return 0;
}
int
make_floats(ref * op, const float *pval, int count)
{
    /* This should return e_undefinedresult for infinities. */
    for (; count--; op++, pval++)
	make_real(op, *pval);
    return 0;
}

/* Compute the error code when check_proc fails. */
/* Note that the client, not this procedure, uses return_error. */
/* The stack underflow check is harmless in the off-stack case. */
int
check_proc_failed(const ref * pref)
{
    return (r_is_array(pref) ? e_invalidaccess :
	    r_has_type(pref, t__invalid) ? e_stackunderflow :
	    e_typecheck);
}

/* Compute the error code when a type check on the stack fails. */
/* Note that the client, not this procedure, uses return_error. */
int
check_type_failed(const ref * op)
{
    return (r_has_type(op, t__invalid) ? e_stackunderflow : e_typecheck);
}

/* ------ Matrix utilities ------ */

/* Read a matrix operand. */
/* Return 0 if OK, error code if not. */
int
read_matrix(const ref * op, gs_matrix * pmat)
{
    int code;
    ref values[6];
    const ref *pvalues;

    if (r_has_type(op, t_array))
	pvalues = op->value.refs;
    else {
	int i;

	for (i = 0; i < 6; ++i) {
	    code = array_get(op, (long)i, &values[i]);
	    if (code < 0)
		return code;
	}
	pvalues = values;
    }
    check_read(*op);
    if (r_size(op) != 6)
	return_error(e_rangecheck);
    code = float_params(pvalues + 5, 6, (float *)pmat);
    return (code < 0 ? code : 0);
}

/* Write a matrix operand. */
/* Return 0 if OK, error code if not. */
int
write_matrix(ref * op, const gs_matrix * pmat)
{
    ref *aptr;
    const float *pel;
    int i;

    check_write_type(*op, t_array);
    if (r_size(op) != 6)
	return_error(e_rangecheck);
    aptr = op->value.refs;
    pel = (const float *)pmat;
    for (i = 5; i >= 0; i--, aptr++, pel++) {
	ref_save(op, aptr, "write_matrix");
	make_real_new(aptr, *pel);
    }
    return 0;
}
