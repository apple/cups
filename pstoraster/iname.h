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

/* iname.h */
/* Name table interface */

/*
 * This file defines those parts of the name table API that do not depend
 * at all on the implementation.
 */

/* ---------------- Abstract types ---------------- */

typedef struct name_table_s name_table;

/* ---------------- Constant values ---------------- */

extern const uint name_max_string;

/* ---------------- Procedural interface ---------------- */

/* Allocate and initialize a name table. */
name_table *name_init(P2(ulong, gs_memory_t *));

/*
 * The name table machinery is designed so that multiple name tables
 * are possible, but the interpreter relies on there being only one,
 * and many of the procedures below assume this (by virtue of
 * not taking a name_table argument).  Therefore, we provide a procedure
 * to get our hands on that unique table (read-only, however).
 */
const name_table *the_name_table(P0());

/* Get the allocator for the name table. */
gs_memory_t *name_memory(P0());

/*
 * Look up and/or enter a name in the name table.
 * The values of enterflag are:
 *	-1 -- don't enter (return an error) if missing;
 *	 0 -- enter if missing, don't copy the string, which was allocated
 *		statically;
 *	 1 -- enter if missing, copy the string;
 *	 2 -- enter if missing, don't copy the string, which was already
 *		allocated dynamically (using the name_memory allocator).
 * Possible errors: VMerror, limitcheck (if string is too long or if
 * we have assigned all possible name indices).
 */
int name_ref(P4(const byte *ptr, uint size, ref *pnref, int enterflag));
void name_string_ref(P2(const ref *pnref, ref *psref));
/*
 * name_enter_string calls name_ref with a (permanent) C string.
 */
int name_enter_string(P2(const char *str, ref *pnref));
/*
 * name_from_string essentially implements cvn.
 * It always enters the name, and copies the executable attribute.
 */
int name_from_string(P2(const ref *psref, ref *pnref));

/* Compare two names for equality. */
#define name_eq(pnref1, pnref2)\
  ((pnref1)->value.pname == (pnref2)->value.pname)

/* Invalidate the value cache for a name. */
void name_invalidate_value_cache(P1(const ref *));

/* Convert between names and indices. */
uint name_index(P1(const ref *));		/* ref => index */
name *name_index_ptr(P1(uint nidx));		/* index => name */
void name_index_ref(P2(uint nidx, ref *pnref));	/* index => ref */

/* Get the index of the next valid name. */
/* The argument is 0 or a valid index. */
/* Return 0 if there are no more. */
uint name_next_valid_index(P1(uint));

/* Mark a name for the garbage collector. */
/* Return true if this is a new mark. */
bool name_mark_index(P1(uint));

/* Get the object (sub-table) containing a name. */
/* The garbage collector needs this so it can relocate pointers to names. */
void/*obj_header_t*/ *name_ref_sub_table(P1(const ref *));
void/*obj_header_t*/ *name_index_ptr_sub_table(P2(uint, name *));
