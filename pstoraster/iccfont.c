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

/* iccfont.c */
/* Initialization support for compiled fonts */
#include "string_.h"
#include "ghost.h"
#include "gsstruct.h"		/* for iscan.h */
#include "ccfont.h"
#include "errors.h"
#include "ialloc.h"
#include "idict.h"
#include "ifont.h"
#include "iname.h"
#include "isave.h"		/* for ialloc_ref_array */
#include "iutil.h"
#include "oper.h"
#include "ostack.h"		/* for iscan.h */
#include "store.h"
#include "stream.h"		/* for iscan.h */
#include "strimpl.h"		/* for sfilter.h for picky compilers */
#include "sfilter.h"		/* for iscan.h */
#include "iscan.h"

/* ------ Private code ------ */

/* Forward references */
private int huge cfont_ref_from_string(P3(ref *, const char *, uint));

typedef struct {
	const char *str_array;
	ref next;
} str_enum;
#define init_str_enum(sep, ksa)\
  (sep)->str_array = ksa
typedef struct {
	cfont_dict_keys keys;
	str_enum strings;
} key_enum;
#define init_key_enum(kep, kp, ksa)\
  (kep)->keys = *kp, init_str_enum(&(kep)->strings, ksa)

/* Check for reaching the end of the keys. */
#define more_keys(kep) ((kep)->keys.num_enc_keys | (kep)->keys.num_str_keys)

/* Get the next string from a string array. */
/* Return 1 if it was a string, 0 if it was something else, */
/* or an error code. */
private int near
cfont_next_string(str_enum _ss *pse)
{	const byte *str = pse->str_array;
	uint len = (str[0] << 8) + str[1];

	if ( len == 0xffff )
	  {	make_null(&pse->next);
		pse->str_array = str + 2;
		return 0;
	  }
	else if ( len >= 0xff00 )
	  {	int code;
		len = ((len & 0xff) << 8) + str[2];
		code = cfont_ref_from_string(&pse->next,
					     (const char *)str + 3, len);
		if ( code < 0 )
		  return code;
		pse->str_array = str + 3 + len;
		return 0;
	  }
	make_const_string(&pse->next, avm_foreign, len, str + 2);
	pse->str_array = str + 2 + len;
	return 1;
}

/* Put the next entry into a dictionary. */
/* We know that more_keys(kp) is true. */
private int near
cfont_put_next(ref *pdict, key_enum _ss *kep, const ref *pvalue)
{	ref kname;
	int code;

#define kp (&kep->keys)
	if ( pdict->value.pdict == 0 )
	{	/* First time, create the dictionary. */
		code = dict_create(kp->num_enc_keys + kp->num_str_keys + kp->extra_slots, pdict);
		if ( code < 0 )
			return code;
	}
	if ( kp->num_enc_keys )
	{	const charindex *skp = kp->enc_keys++;
		code = array_get(&registered_Encoding(skp->encx), (long)(skp->charx), &kname);
		kp->num_enc_keys--;
	}
	else		/* must have kp->num_str_keys != 0 */
	{	code = cfont_next_string(&kep->strings);
		if ( code != 1 )
		  return (code < 0 ? code : gs_note_error(e_Fatal));
		code = name_ref(kep->strings.next.value.const_bytes,
				r_size(&kep->strings.next), &kname, 0);
		kp->num_str_keys--;
	}
	if ( code < 0 )
	  return code;
	return dict_put(pdict, &kname, pvalue);
#undef kp
}

/* ------ Routines called from compiled font initialization ------ */

/* Create a dictionary with general ref values. */
private int huge
cfont_ref_dict_create(ref *pdict, const cfont_dict_keys *kp,
  cfont_string_array ksa, const ref *values)
{	key_enum kenum;
	const ref *vp = values;
	init_key_enum(&kenum, kp, ksa);
	pdict->value.pdict = 0;
	while ( more_keys(&kenum) )
	{	const ref *pvalue = vp++;
		int code = cfont_put_next(pdict, &kenum, pvalue);
		if ( code < 0 ) return code;
	}
	return 0;
}

/* Create a dictionary with string/null values. */
private int huge
cfont_string_dict_create(ref *pdict, const cfont_dict_keys *kp,
  cfont_string_array ksa, cfont_string_array kva)
{	key_enum kenum;
	str_enum senum;
	uint attrs = kp->value_attrs;
	init_key_enum(&kenum, kp, ksa);
	init_str_enum(&senum, kva);
	pdict->value.pdict = 0;
	while ( more_keys(&kenum) )
	{	int code =  cfont_next_string(&senum);
		switch ( code )
		  {
		  default:	/* error */
		    return code;
		  case 1:	/* string */
		    r_set_attrs(&senum.next, attrs);
		  case 0:	/* other */
		    ;
		  }
		code = cfont_put_next(pdict, &kenum, &senum.next);
		if ( code < 0 ) return code;
	}
	return 0;
}

/* Create a dictionary with number values. */
private int huge
cfont_num_dict_create(ref *pdict, const cfont_dict_keys *kp,
  cfont_string_array ksa, const ref *values, const char *lengths)
{	key_enum kenum;
	const ref *vp = values;
	const char *lp = lengths;
	ref vnum;

	init_key_enum(&kenum, kp, ksa);
	pdict->value.pdict = 0;
	while ( more_keys(&kenum) )
	{	int len = (lp == 0 ? 0 : *lp++);
		int code;

		if ( len == 0 )
		  vnum = *vp++;
		else
		  {	--len;
			make_const_array(&vnum, avm_foreign | a_readonly, len, vp);
			vp += len;
		  }
		code = cfont_put_next(pdict, &kenum, &vnum);
		if ( code < 0 ) return code;
	}
	return 0;
}

/* Create an array with name values. */
private int huge
cfont_name_array_create(ref *parray, cfont_string_array ksa, int size)
{	int code = ialloc_ref_array(parray, a_readonly, size,
				    "cfont_name_array_create");
	ref *aptr = parray->value.refs;
	int i;
	str_enum senum;

	if ( code < 0 ) return code;
	init_str_enum(&senum, ksa);
	for ( i = 0; i < size; i++, aptr++ )
	{	ref nref;
		int code = cfont_next_string(&senum);
		if ( code != 1 )
		  return (code < 0 ? code : gs_note_error(e_Fatal));
		code = name_ref(senum.next.value.const_bytes,
				r_size(&senum.next), &nref, 0);
		if ( code < 0 )
		  return code;
		ref_assign_new(aptr, &nref);
	}
	return 0;
}

/* Create an array with string/null values. */
private int huge
cfont_string_array_create(ref *parray, cfont_string_array ksa,
  int size, uint attrs)
{	int code = ialloc_ref_array(parray, a_readonly, size,
				    "cfont_string_array_create");
	ref *aptr = parray->value.refs;
	int i;
	str_enum senum;

	if ( code < 0 ) return code;
	init_str_enum(&senum, ksa);
	for ( i = 0; i < size; i++, aptr++ )
	{	int code =  cfont_next_string(&senum);
		switch ( code )
		  {
		  default:	/* error */
		    return code;
		  case 1:	/* string */
		    r_set_attrs(&senum.next, attrs);
		  case 0:	/* other */
		    ;
		  }
		ref_mark_new(&senum.next);
		*aptr = senum.next;
	}
	return 0;
}

/* Create a name. */
private int huge
cfont_name_create(ref *pnref, const char *str)
{	return name_ref((const byte *)str, strlen(str), pnref, 0);
}

/* Create an object by parsing a string. */
private int huge
cfont_ref_from_string(ref *pref, const char *str, uint len)
{	scanner_state sstate;
	stream s;
	int code;

	scanner_state_init(&sstate, false);
	sread_string(&s, (const byte *)str, len);
	code = scan_token(&s, pref, &sstate);
	return (code <= 0 ? code : gs_note_error(e_Fatal));
}

/* ------ Initialization ------ */

/* Procedure vector passed to font initialization procedures. */
private const cfont_procs ccfont_procs = {
	cfont_ref_dict_create,
	cfont_string_dict_create,
	cfont_num_dict_create,
	cfont_name_array_create,
	cfont_string_array_create,
	cfont_name_create,
	cfont_ref_from_string
};

/* null    .getccfont    <number-of-fonts> */
/* <int>   .getccfont    <font-object> */
private int
zgetccfont(register os_ptr op)
{
  int code;
  ccfont_fproc **fprocs;
  int nfonts;
  int index;

  code = ccfont_fprocs (&nfonts, &fprocs);
  if ( code != ccfont_version )
    return_error(e_invalidfont);

  if ( r_has_type(op, t_null) )
  {
    make_int(op, nfonts);
    return 0;
  }

  check_type(*op, t_integer);
  index = op->value.intval;
  if ( index < 0 || index >= nfonts )
    return_error(e_rangecheck);

  return (*fprocs[index])(&ccfont_procs, op);
}

/* Operator table initialization */

BEGIN_OP_DEFS(ccfonts_op_defs) {
  {"0.getccfont", zgetccfont},
END_OP_DEFS(0) }
