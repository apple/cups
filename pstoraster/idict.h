/* Copyright (C) 1989, 1995 Aladdin Enterprises.  All rights reserved.
  
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

/* idict.h */
/* Interfaces for Ghostscript dictionary package */

/*
 * Contrary to our usual practice, we expose the (first-level)
 * representation of a dictionary in the interface file,
 * because it is so important that access checking go fast.
 * The access attributes for the dictionary are stored in
 * the values ref.
 */
struct dict_s {
	ref values;		/* t_array, values */
	ref keys;		/* t_shortarray or t_array, keys */
	ref count;		/* t_integer, count of occupied entries */
				/* (length) */
	ref maxlength;		/* t_integer, maxlength as seen by client. */
};

/*
 * Define the maximum size of a dictionary.
 */
extern const uint dict_max_size;

/*
 * Define whether dictionaries expand automatically when full.  Note that
 * if dict_auto_expand is true, dict_put, dict_copy, dict_resize, and
 * dict_grow cannot return e_dictfull; however, they can return e_VMerror.
 * (dict_find can return e_dictfull even if dict_auto_expand is true.)
 */
extern bool dict_auto_expand;

/*
 * Create a dictionary.
 */
int dict_create(P2(uint maxlength, ref *pdref));

/*
 * Return a pointer to a ref that holds the access attributes
 * for a dictionary.
 */
#define dict_access_ref(pdref) (&(pdref)->value.pdict->values)
/*
 * Check a dictionary for read or write permission.
 * Note: this does NOT check the type of its operand!
 */
#define check_dict_read(dref) check_read(*dict_access_ref(&dref))
#define check_dict_write(dref) check_write(*dict_access_ref(&dref))

/*
 * Look up a key in a dictionary.  Store a pointer to the value slot
 * where found, or to the (value) slot for inserting.
 * The caller is responsible for checking that the dictionary is readable.
 * Return 1 if found, 0 if not and there is room for a new entry,
 * Failure returns:
 *	e_typecheck if the key is null;
 *	e_invalidaccess if the key is a string lacking read access;
 *	e_VMerror or e_limitcheck if the key is a string and the corresponding
 *	  error occurs from attempting to convert it to a name;
 *	e_dictfull if the dictionary is full and the key is missing.
 */
int dict_find(P3(const ref *pdref, const ref *key, ref **ppvalue));

/*
 * Look up a (constant) C string in a dictionary.
 * Return 1 if found, <= 0 if not.
 */
int dict_find_string(P3(const ref *pdref, const char _ds *kstr,
			ref **ppvalue));

/*
 * Enter a key-value pair in a dictionary.
 * The caller is responsible for checking that the dictionary is writable.
 * Return 1 if this was a new entry, 0 if this replaced an existing entry.
 * Failure returns are as for dict_find, except that e_dictfull doesn't
 * occur if the dictionary is full but expandable, plus:
 *	e_invalidaccess for an attempt to store a younger key or value into
 *	  an older dictionary;
 *	e_VMerror if a VMerror occurred while trying to expand the
 *	  dictionary.
 */
int dict_put(P3(ref *pdref, const ref *key, const ref *pvalue));

/*
 * Enter a key-value pair where the key is a (constant) C string.
 */
int dict_put_string(P3(ref *pdref, const char *kstr, const ref *pvalue));

/*
 * Remove a key-value pair from a dictionary.
 * Return 0 or e_undefined.
 */
int dict_undef(P2(ref *pdref, const ref *key));

/*
 * Return the number of elements in a dictionary.
 */
uint dict_length(P1(const ref *pdref));

/*
 * Return the capacity of a dictionary.
 */
uint dict_maxlength(P1(const ref *pdref));

/*
 * Copy one dictionary into another.
 * Return 0 or e_dictfull.
 * If new_only is true, only copy entries whose keys
 * aren't already present in the destination.
 */
int dict_copy_entries(P3(const ref *dfrom, ref *dto, bool new_only));
#define dict_copy(dfrom, dto) dict_copy_entries(dfrom, dto, false)
#define dict_copy_new(dfrom, dto) dict_copy_entries(dfrom, dto, true)

/*
 * Grow or shrink a dictionary.
 * Return 0, e_dictfull, or e_VMerror.
 */
int dict_resize(P2(ref *pdref, uint newmaxlength));

/*
 * Grow a dictionary in the same way as dict_put does.
 * We export this for some special-case code in zop_def.
 */
int dict_grow(P1(ref *pdref));

/*
 * Ensure that a dictionary uses the unpacked representation for keys.
 * (This is of no interest to ordinary clients.)
 */
int dict_unpack(P1(ref *pdref));

/*
 * Prepare to enumerate a dictionary.
 * Return an integer suitable for the first call to dict_next.
 */
int dict_first(P1(const ref *pdref));

/*
 * Enumerate the next element of a dictionary.
 * index is initially the result of a call on dict_first.
 * Either store a key and value at eltp[0] and eltp[1]
 * and return an updated index, or return -1
 * to signal that there are no more elements in the dictionary.
 */
int dict_next(P3(const ref *pdref, int index, ref *eltp));

/*
 * Given a value pointer return by dict_find, return an index that
 * identifies the entry within the dictionary. (This may, but need not,
 * be the same as the index returned by dict_next.)
 * The index is in the range [0..maxlength-1].
 */
int dict_value_index(P2(const ref *pdref, const ref *pvalue));

/*
 * Given an index in [0..maxlength-1], as returned by dict_value_index,
 * return the key and value, as returned by dict_next.
 * If the index designates an unoccupied entry, return e_undefined.
 */
int dict_index_entry(P3(const ref *pdref, int index, ref *eltp));
