/* Copyright (C) 1989, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* idict.c */
/* Dictionaries for Ghostscript */
#include "string_.h"			/* for strlen */
#include "ghost.h"
#include "errors.h"
#include "ialloc.h"
#include "idebug.h"			/* for debug_print_name */
#include "inamedef.h"
#include "ipacked.h"
#include "isave.h"			/* for value cache in names */
#include "store.h"
#include "idict.h"			/* interface definition */
#include "dstack.h"			/* interface & some implementation */
#include "iutil.h"
#include "ivmspace.h"			/* for store check */

/*
 * A dictionary of capacity M is a structure of four elements (refs):
 *
 *	keys - a t_shortarray or t_array of M+1 elements, containing
 *	the keys.
 *
 *	values - a t_array of M+1 elements, containing the values.
 *
 *	count - a t_integer whose value tells how many entries are
 *	occupied (N).
 *
 *	maxlength - a t_integer whose value gives the client's view of
 *	the capacity (C).  C may be less than M (see below).
 *
 * C < M is possible because on large-memory systems, we round up M so that
 * M is a power of 2; this allows us to use masking rather than division
 * for computing the initial hash probe.  However, C is always the
 * maxlength specified by the client, so clients get a consistent story.
 *
 * As noted above, the keys may be either in packed or unpacked form.
 * The markers for unused and deleted entries are different in the two forms.
 * In the packed form:
 *	unused entries contain packed_key_empty;
 *	deleted entries contain packed_key_deleted.
 * In the unpacked form:
 *	unused entries contain a literal null;
 *	deleted entries contain an executable null.
 *
 * The first entry is always marked deleted, to reduce the cost of the
 * wrap-around check.
 *
 * Note that if the keys slot in the dictionary is new,
 * all the key slots are new (more recent than the last save).
 * We use this fact to avoid saving stores into packed keys
 * for newly created dictionaries.
 *
 * Note that name keys with indices above packed_name_max_index require using
 * the unpacked form.
 */
#define dict_is_packed(dct) r_has_type(&(dct)->keys, t_shortarray)
#define packed_key_empty (pt_tag(pt_integer) + 0)
#define packed_key_deleted (pt_tag(pt_integer) + 1)
#define packed_key_impossible pt_tag(pt_full_ref)	/* never matches */
#define packed_name_key(nidx)\
  ((nidx) <= packed_name_max_index ? pt_tag(pt_literal_name) + (nidx) :\
   packed_key_impossible)
/*
 * Using a special mark for deleted entries causes lookup time to degrade
 * as entries are inserted and deleted.  This is not a problem, because
 * entries are almost never deleted.
 */
#define d_maxlength(dct) ((uint)((dct)->maxlength.value.intval))
#define d_set_maxlength(dct,siz) ((dct)->maxlength.value.intval = (siz))
#define nslots(dct) r_size(&(dct)->values)
#define npairs(dct) (nslots(dct) - 1)
#define d_length(dct) ((uint)((dct)->count.value.intval))

/*
 * Define the size of the largest valid dictionary.
 * This is limited by the size field of the keys and values refs,
 * and by the enumeration interface, which requires the size to
 * fit in an int.  As it happens, max_array_size will always be
 * smaller than max_int.
 */
const uint dict_max_size = max_array_size - 1;

/* Define whether dictionaries expand automatically when full. */
bool dict_auto_expand = false;

/* Define whether dictionaries are packed by default. */
bool dict_default_pack = true;

/* Cached values from the top element of the dictionary stack. */
/* See dstack.h for details. */
int dsspace;				/* see dstack.h */
const ref_packed *dtop_keys;
uint dtop_npairs;
ref *dtop_values;

/* Forward references */
private int dict_create_contents(P3(uint size, const ref *pdref, bool pack));

/* Debugging statistics */
#ifdef DEBUG
long dn_lookups;		/* total lookups */
long dn_1probe;			/* successful lookups on only 1 probe */
long dn_2probe;			/* successful lookups on 2 probes */
/* Wrappers for dict_find and dict_find_name_by_index */
int real_dict_find(P3(const ref *pdref, const ref *key, ref **ppvalue));
int
dict_find(const ref *pdref, const ref *pkey, ref **ppvalue)
{	dict *pdict = pdref->value.pdict;
	int code = real_dict_find(pdref, pkey, ppvalue);

	dn_lookups++;
	if ( r_has_type(pkey, t_name) && dict_is_packed(pdict) )
	{	uint nidx = name_index(pkey);
		uint hash =
		  hash_mod(dict_name_index_hash(nidx), npairs(pdict)) + 1;
		if (  pdict->keys.value.packed[hash] ==
		        pt_tag(pt_literal_name) + nidx
		   )
		  dn_1probe++;
		else if (  pdict->keys.value.packed[hash - 1] ==
			     pt_tag(pt_literal_name) + nidx
			)
		  dn_2probe++;
	}
	/* Do the cheap flag test before the expensive remainder test. */
	if ( gs_debug_c('d') && !(dn_lookups % 1000) )
	  dprintf3("[d]lookups=%ld 1probe=%ld 2probe=%ld\n",
		   dn_lookups, dn_1probe, dn_2probe);
	return code;
}
#define dict_find real_dict_find
ref *real_dict_find_name_by_index(P1(uint nidx));
ref *
dict_find_name_by_index(uint nidx)
{	ref *pvalue = real_dict_find_name_by_index(nidx);
	dict *pdict = dsp->value.pdict;

	dn_lookups++;
	if ( dict_is_packed(pdict) )
	{	uint hash =
		  hash_mod(dict_name_index_hash(nidx), npairs(pdict)) + 1;
		if (  pdict->keys.value.packed[hash] ==
		        pt_tag(pt_literal_name) + nidx
		   )
		  dn_1probe++;
		else if (  pdict->keys.value.packed[hash - 1] ==
			     pt_tag(pt_literal_name) + nidx
			)
		  dn_2probe++;
	}
	/* Do the cheap flag test before the expensive remainder test. */
	if ( gs_debug_c('d') && !(dn_lookups % 1000) )
	  dprintf3("[d]lookups=%ld 1probe=%ld 2probe=%ld\n",
		   dn_lookups, dn_1probe, dn_2probe);
	return pvalue;
}
#define dict_find_name_by_index real_dict_find_name_by_index
#endif

/* Create a dictionary in the current VM space. */
int
dict_create(uint size, ref *pdref)
{	ref arr;
	int code = ialloc_ref_array(&arr, a_all, sizeof(dict) / sizeof(ref),
				    "dict_create");
	ref dref;

	if ( code < 0 )
	  return code;
	make_tav_new(&dref, t_dictionary, r_space(&arr) | a_all,
		     pdict, (dict *)arr.value.refs);
	code = dict_create_contents(size, &dref, dict_default_pack);
	if ( code < 0 )
	  return code;
	*pdref = dref;
	return 0;
}
/* Create unpacked keys for a dictionary. */
/* The keys are allocated in the same VM space as the dictionary. */
private int
dict_create_unpacked_keys(uint asize, const ref *pdref)
{	dict *pdict = pdref->value.pdict;
	uint space = ialloc_space(idmemory);
	int code;

	ialloc_set_space(idmemory, r_space(pdref));
	code = ialloc_ref_array(&pdict->keys, a_all, asize, "dict create unpacked keys");
	if ( code >= 0 )
	  { ref *kp = pdict->keys.value.refs;
	    ref_mark_new(&pdict->keys);
	    refset_null(kp, asize);
	    r_set_attrs(kp, a_executable);	/* wraparound entry */
	  }
	ialloc_set_space(idmemory, space);
	return code;
}
/* Create the contents (keys and values) of a newly allocated dictionary. */
/* Allocate in the current VM space, which is assumed to be the same as */
/* the VM space where the dictionary is allocated. */
private int
dict_create_contents(uint size, const ref *pdref, bool pack)
{	dict *pdict = pdref->value.pdict;
	uint asize = (size == 0 ? 1 : size);
	int code;
	uint i;

	/* If appropriate, round up the actual allocated size to the next */
	/* higher power of 2, so we can use & instead of %. */
	dict_round_size(asize);
	asize++;		/* allow room for wraparound entry */
	code = ialloc_ref_array(&pdict->values, a_all, asize,
				"dict_create(values)");
	if ( code < 0 )
	  return code;
	ref_mark_new(&pdict->values);
	refset_null(pdict->values.value.refs, asize);
	if ( pack )
	   {	uint ksize = (asize + packed_per_ref - 1) / packed_per_ref;
		ref arr;
		ref_packed *pkp;
		ref_packed *pzp;
		code = ialloc_ref_array(&arr, a_all, ksize,
					"dict_create(packed keys)");
		if ( code < 0 )
		  return code;
		pkp = (ref_packed *)arr.value.refs;
		make_tasv_new(&pdict->keys, t_shortarray,
			      r_space(&arr) | a_all,
			      asize, packed, pkp);
                /* MRS - unrolled loop to avoid SGI compiler bug */
		for (pzp = pkp, i = 0; i < asize; i++ )
		  *pzp++ = packed_key_empty;
		for (; i % packed_per_ref; i++ )
		  *pzp++ = packed_key_empty;
		*pkp = packed_key_deleted;	/* wraparound entry */
	   }
	else				/* not packed */
	   {	int code = dict_create_unpacked_keys(asize, pdref);
		if ( code < 0 ) return code;
	   }
	make_int_new(&pdict->count, 0);
	make_int_new(&pdict->maxlength, size);
	return 0;
}

/*
 * Ensure that a dictionary uses the unpacked representation for keys.
 * We can't just use dict_resize, because the values slots mustn't move.
 */
int
dict_unpack(ref *pdref)
{	dict *pdict = pdref->value.pdict;
	if ( !dict_is_packed(pdict) )
	  return 0;			/* nothing to do */
	{	uint count = nslots(pdict);
		const ref_packed *okp = pdict->keys.value.packed;
		ref old_keys;
		int code;
		ref *nkp;
		old_keys = pdict->keys;
		if ( ref_must_save(&old_keys) )
		  ref_do_save(pdref, &pdict->keys, "dict_unpack(keys)");
		code = dict_create_unpacked_keys(count, pdref);
		if ( code < 0 )
		  return code;
		for ( nkp = pdict->keys.value.refs; count--; okp++, nkp++ )
		  if ( r_packed_is_name(okp) )
		    { packed_get(okp, nkp);
		      ref_mark_new(nkp);
		    }
		if ( !ref_must_save(&old_keys) )
		  ifree_ref_array(&old_keys, "dict_unpack(old keys)");
		dict_set_top();	/* just in case */
	}
	return 0;
}

/*
 * Define a macro for searching a packed dictionary.  Free variables:
 *	ref_packed kpack - holds the packed key.
 *	uint hash - holds the hash of the name.
 *	dict *pdict - points to the dictionary.
 *	uint size - holds npairs(pdict).
 * Note that the macro is *not* enclosed in {}, so that we can access
 * the values of kbot and kp after leaving the loop.
 *
 * We break the macro into two to avoid overflowing some preprocessors.
 */
/* packed_search_body also uses kp and kbot as free variables. */
#define packed_search_body(del,pre,post,miss)\
    { if_debug2('D', "[D]probe 0x%lx: 0x%x\n", (ulong)kp, *kp);\
      if ( *kp == kpack )\
       { pre (pdict->values.value.refs + (kp - kbot));\
	 post;\
       }\
      else if ( !r_packed_is_name(kp) )\
       { /* Empty, deleted, or wraparound. Figure out which. */\
	 if ( *kp == packed_key_empty ) miss;\
	 if ( kp == kbot ) break;	/* wrap */\
	 else { del; }\
       }\
    }
#define packed_search_1(del,pre,post,miss)\
   const ref_packed *kbot = pdict->keys.value.packed;\
   register const ref_packed *kp;\
   for ( kp = kbot + hash_mod(hash, size) + 1; ; kp-- )\
     packed_search_body(del,pre,post,miss)
#define packed_search_2(del,pre,post,miss)\
   for ( kp += size; ; kp-- )\
     packed_search_body(del,pre,post,miss)

/*
 * Look up a key in a dictionary.  Store a pointer to the value slot
 * where found, or to the (value) slot for inserting.
 * Return 1 if found, 0 if not and there is room for a new entry,
 * or e_dictfull if the dictionary is full and the key is missing.
 * The caller is responsible for ensuring key is not a null.
 */
int
dict_find(const ref *pdref, const ref *pkey,
  ref **ppvalue	/* result is stored here */)
{	dict *pdict = pdref->value.pdict;
	uint size = npairs(pdict);
	register int etype;
	uint nidx;
	ref_packed kpack;
	uint hash;
	int ktype;
	/* Compute hash.  The only types we bother with are strings, */
	/* names, and (unlikely, but worth checking for) integers. */
	switch ( r_type(pkey) )
	{
	case t_name:
		nidx = name_index(pkey);
nh:		hash = dict_name_index_hash(nidx);
		kpack = packed_name_key(nidx);
		ktype = t_name;
		break;
	case t_string:			/* convert to a name first */
	{	ref nref;
		int code;
		if ( !r_has_attr(pkey, a_read) )
		  return_error(e_invalidaccess);
		code = name_ref(pkey->value.bytes, r_size(pkey), &nref, 1);
		if ( code < 0 )
		  return code;
		nidx = name_index(&nref);
	}	goto nh;
	case t_integer:
		hash = (uint)pkey->value.intval * 30503;
		kpack = packed_key_impossible;
		ktype = -1;
		nidx = 0;		/* only to pacify gcc */
		break;
	case t_null:			/* not allowed as a key */
		return_error(e_typecheck);
	default:
		hash = r_btype(pkey) * 99;	/* yech */
		kpack = packed_key_impossible;
		ktype = -1;
		nidx = 0;		/* only to pacify gcc */
	}
	/* Search the dictionary */
	if ( dict_is_packed(pdict) )
	{	const ref_packed *pslot = 0;
		packed_search_1(if ( pslot == 0 ) pslot = kp,
				*ppvalue =, return 1, goto miss);
		packed_search_2(if ( pslot == 0 ) pslot = kp,
				*ppvalue =, return 1, goto miss);
		/*
		 * Double wraparound, dict is full.
		 * Note that even if there was an empty slot (pslot != 0),
		 * we must return dictfull if length = maxlength.
		 */
		if ( pslot == 0 || d_length(pdict) == d_maxlength(pdict) )
		  return(e_dictfull);
		*ppvalue = pdict->values.value.refs + (pslot - kbot);
		return 0;
miss:		/* Key is missing, not double wrap.  See above re dictfull. */
		if ( d_length(pdict) == d_maxlength(pdict) )
		  return(e_dictfull);
		if ( pslot == 0 )
		  pslot = kp;
		*ppvalue = pdict->values.value.refs + (pslot - kbot);
		return 0;
	}
	else
	{	ref *kbot = pdict->keys.value.refs;
		register ref *kp;
		ref *pslot = 0;
		int wrap = 0;
		for ( kp = kbot + hash_mod(hash, size) + 2; ; )
		{	--kp;
			if ( (etype = r_type(kp)) == ktype )
			{	/* Fast comparison if both keys are names */
				if ( name_index(kp) == nidx )
				{	*ppvalue = pdict->values.value.refs + (kp - kbot);
					return 1;
				}
			}
			else if ( etype == t_null )
			{	/* Empty, deleted, or wraparound. */
				/* Figure out which. */
				if ( kp == kbot )	/* wrap */
				{	if ( wrap++ )	/* wrapped twice */
					{	if ( pslot == 0 )
						  return(e_dictfull);
						break;
					}
					kp += size + 1;
				   }
				else if ( r_has_attr(kp, a_executable) )
				{	/* Deleted entry, save the slot. */
					if ( pslot == 0 )
						pslot = kp;
				}
				else	/* key not found */
					break;
			}
			else
			{	if ( obj_eq(kp, pkey) )
				{	*ppvalue = pdict->values.value.refs + (kp - kbot);
					return 1;
				}
			}
		}
		if ( d_length(pdict) == d_maxlength(pdict) )
		  return(e_dictfull);
		*ppvalue = pdict->values.value.refs +
			  ((pslot != 0 ? pslot : kp) - kbot);
		return 0;
	}
}

/*
 * Look up a (constant) C string in a dictionary.
 * Return 1 if found, <= 0 if not.
 */
int
dict_find_string(const ref *pdref, const char _ds *kstr, ref **ppvalue)
{	int code;
	ref kname;
	if ( (code = name_ref((const byte *)kstr, strlen(kstr), &kname, -1)) < 0 )
	  return code;
	return dict_find(pdref, &kname, ppvalue);
}

/* Check whether a dictionary is one of the permanent ones on the d-stack. */
bool
dict_is_permanent_on_dstack(const ref *pdref)
{	dict *pdict = pdref->value.pdict;
	int i;
	if ( d_stack.extension_size == 0 )
	  {	/* Only one block of d-stack. */
		for ( i = 0; i < min_dstack_size; ++i )
		  if ( dsbot[i].value.pdict == pdict )
		    return true;
	  }
	else
	  {	/* More than one block of d-stack. */
		uint count = ref_stack_count(&d_stack);
		for ( i = count - min_dstack_size; i < count; ++i )
		  if ( ref_stack_index(&d_stack, i)->value.pdict == pdict )
		    return true;
	  }
	return false;
}

/*
 * Look up a name on the dictionary stack.
 * Return the pointer to the value if found, 0 if not.
 */
ref *
dict_find_name_by_index(uint nidx)
{	ds_ptr pdref = dsp;
/* Since we know the hash function is the identity function, */
/* there's no point in allocating a separate variable for it. */
#define hash dict_name_index_hash(nidx)
	ref_packed kpack = packed_name_key(nidx);
	do
	   {	dict *pdict = pdref->value.pdict;
		uint size = npairs(pdict);
#ifdef DEBUG
		if ( gs_debug_c('D') )
		{	ref dnref;
			name_index_ref(nidx, &dnref);
			dputs("[D]lookup ");
			debug_print_name(&dnref);
			dprintf3(" in 0x%lx(%u/%u)\n",
				 (ulong)pdict, dict_length(pdref),
				 dict_maxlength(pdref));
		}
#endif
		if ( dict_is_packed(pdict) )
		   {	packed_search_1(DO_NOTHING, return,
					DO_NOTHING, goto miss);
			packed_search_2(DO_NOTHING, return,
					DO_NOTHING, break);
 miss:			;
		   }
		else
		   {	ref *kbot = pdict->keys.value.refs;
			register ref *kp;
			int wrap = 0;
			/* Search the dictionary */
			for ( kp = kbot + hash_mod(hash, size) + 2; ; )
			   {	--kp;
				if ( r_has_type(kp, t_name) )
				   {	if ( name_index(kp) == nidx )
					  return pdict->values.value.refs +
					    (kp - kbot);
				   }
				else if ( r_has_type(kp, t_null) )
				   {	/* Empty, deleted, or wraparound. */
					/* Figure out which. */
					if ( !r_has_attr(kp, a_executable) )
					  break;
					if ( kp == kbot )	/* wrap */
					   {	if ( wrap++ )
						  break;	/* 2 wraps */
						kp += size + 1;
					   }
				   }
			   }
		   }
	   }
	while ( pdref-- > dsbot );
	/* The name isn't in the top dictionary block. */
	/* If there are other blocks, search them now (more slowly). */
	if ( !d_stack.extension_size )		/* no more blocks */
	  return (ref *)0;
	{	/* We could use the STACK_LOOP macros, but for now, */
		/* we'll do things the simplest way. */
		ref key;
		uint i = dsp + 1 - dsbot;
		uint size = ref_stack_count(&d_stack);
		ref *pvalue;
		name_index_ref(nidx, &key);
		for ( ; i < size; i++ )
		  {	if ( dict_find(ref_stack_index(&d_stack, i),
				       &key, &pvalue) > 0
			   )
			  return pvalue;
		  }
	}
	return (ref *)0;
#undef hash
}

/*
 * Enter a key-value pair in a dictionary.
 * See idict.h for the possible return values.
 */
int
dict_put(ref *pdref /* t_dictionary */, const ref *pkey, const ref *pvalue)
{	int rcode = 0;
	int code;
	ref *pvslot;
	/* Check the value. */
	store_check_dest(pdref, pvalue);
top:	if ( (code = dict_find(pdref, pkey, &pvslot)) <= 0 )	/* not found */
	   {	/* Check for overflow */
		dict *pdict = pdref->value.pdict;
		ref kname;
		uint index;
		switch ( code )
		  {
		  case 0:
			break;
		  case e_dictfull:
			if ( !dict_auto_expand )
			  return_error(e_dictfull);
			code = dict_grow(pdref);
			if ( code < 0 )
			  return code;
			goto top;	/* keep things simple */
		  default:	/* e_typecheck */
			return code;
		  }
		index = pvslot - pdict->values.value.refs;
		/* If the key is a string, convert it to a name. */
		if ( r_has_type(pkey, t_string) )
		  {	int code;
			if ( !r_has_attr(pkey, a_read) )
			  return_error(e_invalidaccess);
			code = name_from_string(pkey, &kname);
			if ( code < 0 )
			  return code;
			pkey = &kname;
		   }
		if ( dict_is_packed(pdict) )
		   {	ref_packed *kp;
			if ( !r_has_type(pkey, t_name) ||
			     name_index(pkey) > packed_name_max_index
			   )
			   {	/* Change to unpacked representation. */
				int code = dict_unpack(pdref);
				if ( code < 0 )
				  return code;
				goto top;
			   }
			kp = (ref_packed *)(pdict->keys.value.packed + index);
			if ( ref_must_save(&pdict->keys) )
			   {	/* See initial comment for why it is safe */
				/* not to save the change if the keys */
				/* array itself is new. */
				ref_do_save(&pdict->keys, kp, "dict_put(key)");
			   }
			*kp = pt_tag(pt_literal_name) + name_index(pkey);
		   }
		else
		   {	ref *kp = pdict->keys.value.refs + index;
			if_debug2('d', "[d]0x%lx: fill key at 0x%lx\n",
				  (ulong)pdict, (ulong)kp);
			store_check_dest(pdref, pkey);
			ref_assign_old(&pdict->keys, kp, pkey,
				       "dict_put(key)");	/* set key of pair */
		   }
		ref_save(pdref, &pdict->count, "dict_put(count)");
		pdict->count.value.intval++;
		/* If the key is a name, update its 1-element cache. */
		if ( r_has_type(pkey, t_name) )
		   {	name *pname = pkey->value.pname;
			if ( pname->pvalue == pv_no_defn &&
				(pdict == systemdict->value.pdict ||
				 dict_is_permanent_on_dstack(pdref)) &&
				/* Only set the cache if we aren't inside */
				/* a save.  This way, we never have to */
				/* undo setting the cache. */
				alloc_save_level(idmemory) == 0
			   )
			   {	/* Set the cache. */
				if_debug0('d', "[d]set cache\n");
				pname->pvalue = pvslot;
			   }
			else
			  {	/* The cache can't be used. */
				if_debug0('d', "[d]no cache\n");
				pname->pvalue = pv_other;
			  }
		   }
		rcode = 1;
	   }
	if_debug8('d', "[d]0x%lx: put key 0x%lx 0x%lx\n  value at 0x%lx: old 0x%lx 0x%lx, new 0x%lx 0x%lx\n",
		  (ulong)pdref->value.pdict,
		  ((const ulong *)pkey)[0], ((const ulong *)pkey)[1],
		  (ulong)pvslot,
		  ((const ulong *)pvslot)[0], ((const ulong *)pvslot)[1],
		  ((const ulong *)pvalue)[0], ((const ulong *)pvalue)[1]);
	ref_assign_old(&pdref->value.pdict->values, pvslot, pvalue,
		       "dict_put(value)");
	return rcode;
}

/*
 * Enter a key-value pair where the key is a (constant) C string.
 */
int
dict_put_string(ref *pdref, const char *kstr, const ref *pvalue)
{	int code;
	ref kname;
	if ( (code = name_ref((const byte *)kstr, strlen(kstr), &kname, 0)) < 0 )
	  return code;
	return dict_put(pdref, &kname, pvalue);
}

/* Remove an element from a dictionary. */
int
dict_undef(ref *pdref, const ref *pkey)
{	ref *pvslot;
	dict *pdict;
	uint index;
	if ( dict_find(pdref, pkey, &pvslot) <= 0 )
	  return(e_undefined);
	/* Remove the entry from the dictionary. */
	pdict = pdref->value.pdict;
	index = pvslot - pdict->values.value.refs;
	if ( dict_is_packed(pdict) )
	   {	ref_packed *pkp =
		   (ref_packed *)(pdict->keys.value.packed + index);
		/* See the initial comment for why it is safe not to save */
		/* the change if the keys array itself is new. */
		if ( ref_must_save(&pdict->keys) )
		  ref_do_save(&pdict->keys, pkp, "dict_undef(key)");
		/* Accumulating deleted entries slows down lookup. */
		/* Detect the easy case where we can use an empty entry */
		/* rather than a deleted one, namely, when the next entry */
		/* in the probe order is empty. */
		if ( pkp[-1] == packed_key_empty )
		  *pkp = packed_key_empty;
		else
		  *pkp = packed_key_deleted;
	   }
	else				/* not packed */
	   {	ref *kp = pdict->keys.value.refs + index;
		make_null_old(&pdict->keys, kp, "dict_undef(key)");
		/* Accumulating deleted entries slows down lookup. */
		/* Detect the easy case where we can use an empty entry */
		/* rather than a deleted one, namely, when the next entry */
		/* in the probe order is empty. */
		if ( !r_has_type(kp - 1, t_null) ||	/* full entry */
		     r_has_attr(kp - 1, a_executable)	/* deleted or wraparound */
		    )
		  r_set_attrs(kp, a_executable);	/* mark as deleted */
	   }
	ref_save(pdref, &pdict->count, "dict_undef(count)");
	pdict->count.value.intval--;
	/* If the key is a name, update its 1-element cache. */
	if ( r_has_type(pkey, t_name) )
	  {	name *pname = pkey->value.pname;
		if ( pv_valid(pname->pvalue) )
		  {
#ifdef DEBUG
			/* Check the the cache is correct. */
			if ( !dict_is_permanent_on_dstack(pdref) )
			  lprintf1("dict_undef: cached name value pointer 0x%lx is incorrect!\n",
				   (ulong)pname->pvalue);
#endif
			/* Clear the cache */
			pname->pvalue = pv_no_defn;
		  }
	  }
	make_null_old(&pdict->values, pvslot, "dict_undef(value)");
	return 0;
}

/* Return the number of elements in a dictionary. */
uint
dict_length(const ref *pdref /* t_dictionary */)
{	return d_length(pdref->value.pdict);
}

/* Return the capacity of a dictionary. */
uint
dict_maxlength(const ref *pdref	/* t_dictionary */)
{	return d_maxlength(pdref->value.pdict);
}

/* Copy one dictionary into another. */
/* If new_only is true, only copy entries whose keys */
/* aren't already present in the destination. */
int
dict_copy_entries(const ref *pdrfrom /* t_dictionary */,
  ref *pdrto /* t_dictionary */, bool new_only)
{	int space = r_space(pdrto);
	int index;
	ref elt[2];
	ref *pvslot;
	int code;
	if ( space != avm_max )
	  {	/* Do the store check before starting the copy. */
		index = dict_first(pdrfrom);
		while ( (index = dict_next(pdrfrom, index, elt)) >= 0 )
		  if ( !new_only || dict_find(pdrto, &elt[0], &pvslot) <= 0 )
		  {	store_check_space(space, &elt[0]);
			store_check_space(space, &elt[1]);
		  }
	  }
	/* Now copy the contents. */
	index = dict_first(pdrfrom);
	while ( (index = dict_next(pdrfrom, index, elt)) >= 0 )
	  {	if ( new_only && dict_find(pdrto, &elt[0], &pvslot) > 0 )
		  continue;
		if ( (code = dict_put(pdrto, &elt[0], &elt[1])) < 0 )
		  return code;
	  }
	return 0;
}

/* Set the cached values computed from the top entry on the dstack. */
/* See dstack.h for details. */
private const ref_packed no_packed_keys[2] =
	{ packed_key_deleted, packed_key_empty };
void
dict_set_top(void)
{	dict *pdict = dsp->value.pdict;
	if_debug3('d', "[d]dsp = 0x%lx -> 0x%lx, key array type = %d\n",
		  (ulong)dsp, (ulong)pdict, r_type(&pdict->keys));
	if ( dict_is_packed(pdict) &&
	     r_has_attr(dict_access_ref(dsp), a_read)
	   )
	{	dtop_keys = pdict->keys.value.packed;
		dtop_npairs = npairs(pdict);
		dtop_values = pdict->values.value.refs;
	}
	else
	{	dtop_keys = no_packed_keys;
		dtop_npairs = 1;
	}
	if ( !r_has_attr(dict_access_ref(dsp), a_write) )
		dsspace = -1;
	else
		dsspace = r_space(dsp);
}

/* Resize a dictionary. */
int
dict_resize(ref *pdref, uint new_size)
{	dict *pdict = pdref->value.pdict;
	dict dnew;
	ref drto;
	int code;
	uint space;
	if ( new_size < d_length(pdict) )
	{	if ( !dict_auto_expand )
		  return_error(e_dictfull);
		new_size = d_length(pdict);
	}
	space = ialloc_space(idmemory);
	ialloc_set_space(idmemory, r_space(pdref));
	make_tav_new(&drto, t_dictionary, r_space(pdref) | a_all,
		     pdict, &dnew);
	if ( (code = dict_create_contents(new_size, &drto, dict_is_packed(pdict))) < 0 )
	{	ialloc_set_space(idmemory, space);
		return code;
	}
	/* We must suppress the store check, in case we are expanding */
	/* systemdict or another global dictionary that is allowed */
	/* to reference local objects. */
	r_set_space(&drto, avm_local);
	dict_copy(pdref, &drto);	/* can't fail */
	/* Save or free the old dictionary. */
	if ( ref_must_save(&pdict->values) )
	  ref_do_save(pdref, &pdict->values, "dict_resize(values)");
	else
	  ifree_ref_array(&pdict->values, "dict_resize(old values)");
	if ( ref_must_save(&pdict->keys) )
	  ref_do_save(pdref, &pdict->keys, "dict_resize(keys)");
	else
	  ifree_ref_array(&pdict->keys, "dict_resize(old keys)");
	ref_assign(&pdict->keys, &dnew.keys);
	ref_assign(&pdict->values, &dnew.values);
	ref_save(pdref, &pdict->maxlength, "dict_resize(maxlength)");
	d_set_maxlength(pdict, new_size);
	ialloc_set_space(idmemory, space);
	dict_set_top();		/* just in case this is the top dict */
	return 0;
}

/* Grow a dictionary for dict_put. */
int
dict_grow(ref *pdref)
{	dict *pdict = pdref->value.pdict;
	/* We might have maxlength < npairs, if */
	/* dict_round_size is true. */
	ulong new_size = d_maxlength(pdict) * 3 / 2 + 2;
	if ( new_size > dict_max_size )
	{	if ( d_maxlength(pdict) == dict_max_size )
		  return_error(e_dictfull);
		new_size = dict_max_size;
	}
	if ( new_size > npairs(pdict) )
	{	int code = dict_resize(pdref, (uint)new_size);
		if ( code < 0 )
		  return code;
	}
	else
	{	/* maxlength < npairs, we can grow in place */
		ref_save(pdref, &pdict->maxlength, "dict_put(maxlength)");
		d_set_maxlength(pdict, new_size);
	}
	return 0;
}

/* Prepare to enumerate a dictionary. */
int
dict_first(const ref *pdref)
{	return (int)nslots(pdref->value.pdict);
}

/* Enumerate the next element of a dictionary. */
int
dict_next(const ref *pdref, int index, ref *eltp /* ref eltp[2] */)
{	dict *pdict = pdref->value.pdict;
	ref *vp = pdict->values.value.refs + index;
	while ( vp--, --index >= 0 )
	   {	array_get(&pdict->keys, (long)index, eltp);
		/* Make sure this is a valid entry. */
		if ( r_has_type(eltp, t_name) ||
		     (!dict_is_packed(pdict) && !r_has_type(eltp, t_null))
		   )
		   {	eltp[1] = *vp;
			if_debug6('d', "[d]0x%lx: index %d: %lx %lx, %lx %lx\n",
				(ulong)pdict, index,
				((ulong *)eltp)[0], ((ulong *)eltp)[1],
				((ulong *)vp)[0], ((ulong *)vp)[1]);
			return index;
		   }
	   }
	return -1;			/* no more elements */
}

/* Return the index of a value within a dictionary. */
int
dict_value_index(const ref *pdref, const ref *pvalue)
{	return (int)(pvalue - pdref->value.pdict->values.value.refs - 1);
}

/* Return the entry at a given index within a dictionary. */
/* If the index designates an unoccupied entry, return e_undefined. */
int
dict_index_entry(const ref *pdref, int index, ref *eltp /* ref eltp[2] */)
{	const dict *pdict = pdref->value.pdict;
	array_get(&pdict->keys, (long)(index + 1), eltp);
	if ( r_has_type(eltp, t_name) ||
	     (!dict_is_packed(pdict) && !r_has_type(eltp, t_null))
	   )
	  {	eltp[1] = pdict->values.value.refs[index + 1];
		return 0;
	  }
	return e_undefined;
}

/* After a garbage collection, scan the permanent dictionaries and */
/* update the cached value pointers in names. */
void
dstack_gc_cleanup(void)
{	uint count = ref_stack_count(&d_stack);
	uint dsi;
	for ( dsi = min_dstack_size; dsi > 0; --dsi )
	  {	const dict *pdict =
		  ref_stack_index(&d_stack, count - dsi)->value.pdict;
		uint size = nslots(pdict);
		ref *pvalue = pdict->values.value.refs;
		uint i;
		for ( i = 0; i < size; ++i, ++pvalue )
		  {	ref key;
			ref *old_pvalue;
			array_get(&pdict->keys, (long)i, &key);
			if ( r_has_type(&key, t_name) &&
			     pv_valid(old_pvalue = key.value.pname->pvalue)
			   )
			  {	/*
				 * The name only has a single definition,
				 * so it must be this one.  Check to see if
				 * no relocation is actually needed; if so,
				 * we can skip the entire dictionary.
				 */
				if ( old_pvalue == pvalue )
				  {	if_debug1('d', "[d]skipping dstack entry %d\n",
						  dsi - 1);
					break;
				  }
				/* Update the value pointer. */
				key.value.pname->pvalue = pvalue;
			  }
		  }
	  }
}
