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

/* iname.c */
/* Name lookup for Ghostscript interpreter */
#include "memory_.h"
#include "string_.h"
#include "ghost.h"
#include "gsstruct.h"
#include "errors.h"
#include "inamedef.h"
#include "imemory.h"			/* for isave.h */
#include "isave.h"
#include "store.h"

/* Public values */
const uint name_max_string = max_name_string;

/* In the code below, we use the hashing method described in */
/* "Fast Hashing of Variable-Length Text Strings" by Peter K. Pearson, */
/* pp. 677-680, CACM 33(6), June 1990. */

/* Define a pseudo-random permutation of the integers 0..255. */
/* Pearson's article claims this permutation gave good results. */
private const far_data byte hash_permutation[256] = {
  1,  87,  49,  12, 176, 178, 102, 166, 121, 193,   6,  84, 249, 230,  44, 163,
 14, 197, 213, 181, 161,  85, 218,  80,  64, 239,  24, 226, 236, 142,  38, 200,
110, 177, 104, 103, 141, 253, 255,  50,  77, 101,  81,  18,  45,  96,  31, 222,
 25, 107, 190,  70,  86, 237, 240,  34,  72, 242,  20, 214, 244, 227, 149, 235,
 97, 234,  57,  22,  60, 250,  82, 175, 208,   5, 127, 199, 111,  62, 135, 248,
174, 169, 211,  58,  66, 154, 106, 195, 245, 171,  17, 187, 182, 179,   0, 243,
132,  56, 148,  75, 128, 133, 158, 100, 130, 126,  91,  13, 153, 246, 216, 219,
119,  68, 223,  78,  83,  88, 201,  99, 122,  11,  92,  32, 136, 114,  52,  10,
138,  30,  48, 183, 156,  35,  61,  26, 143,  74, 251,  94, 129, 162,  63, 152,
170,   7, 115, 167, 241, 206,   3, 150,  55,  59, 151, 220,  90,  53,  23, 131,
125, 173,  15, 238,  79,  95,  89,  16, 105, 137, 225, 224, 217, 160,  37, 123,
118,  73,   2, 157,  46, 116,   9, 145, 134, 228, 207, 212, 202, 215,  69, 229,
 27, 188,  67, 124, 168, 252,  42,   4,  29, 108,  21, 247,  19, 205,  39, 203,
233,  40, 186, 147, 198, 192, 155,  33, 164, 191,  98, 204, 165, 180, 117,  76,
140,  36, 210, 172,  41,  54, 159,   8, 185, 232, 113, 196, 231,  47, 146, 120,
 51,  65,  28, 144, 254, 221,  93, 189, 194, 139, 112,  43,  71, 109, 184, 209
};

/*
 * Definitions and structure for the name table.
 * Entry 0 is left unused.
 * The entry with count = 1 is the entry for the 0-length name.
 * The next nt_1char_size entries (in count order) are 1-character names.
 */
#define nt_1char_size 128
#define nt_1char_first 2
private const far_data byte nt_1char_names[128] = {
#define q8(n) n,n+1,n+2,n+3,n+4,n+5,n+6,n+7
#define q32(n) q8(n),q8(n+8),q8(n+16),q8(n+24)
	q32(0), q32(32), q32(64), q32(96)
#undef q8
};

gs_private_st_simple(st_name_sub_table, name_sub_table, "name_sub_table");
gs_private_st_composite(st_name_table, name_table,
  "name_table", name_table_enum_ptrs, name_table_reloc_ptrs);

/* The one and only name table (for now). */
private name_table *the_nt;
private gs_gc_root_t the_nt_root;

/* Forward references */
private int name_alloc_sub(P1(name_table *));
private void name_scan_sub(P3(name_table *, uint, bool));

/* Debugging printout */
#ifdef DEBUG
private void
name_print(const char *msg, name *pname, uint nidx, const int *pflag)
{	const byte *ptr = pname->string_bytes;
	dprintf1("[n]%s", msg);
	if ( pflag )
	  dprintf1("(%d)", *pflag);
	dprintf2(" (0x%lx#%u)", (ulong)pname, nidx);
	debug_print_string(ptr, pname->string_size);
	dprintf2("(0x%lx,%u)\n", (ulong)ptr, pname->string_size);
}
#  define if_debug_name(msg, pname, nidx, pflag)\
     if ( gs_debug_c('n') ) name_print(msg, pname, nidx, pflag)
#else
#  define if_debug_name(msg, pname, nidx, pflag) DO_NOTHING
#endif

/* Initialize the name table */
name_table *
name_init(ulong count, gs_memory_t *mem)
{	register int i;
	name_table *nt;

	if ( count == 0 )
	  count = max_name_count + 1L;
	else if ( count - 1 > max_name_count )
	  return 0;
	nt =
	  gs_alloc_struct(mem, name_table, &st_name_table, "name_init(nt)");
	the_nt = nt;
	memset(nt, 0, sizeof(name_table));
	nt->max_sub_count =
	  ((count - 1) | nt_sub_index_mask) >> nt_log2_sub_size;
	nt->memory = mem;
	/* Initialize the one-character names. */
	/* Start by creating the necessary sub-tables. */
	for ( i = 0; i < nt_1char_first + nt_1char_size; i += nt_sub_size )
	  name_alloc_sub(nt);
	for ( i = -1; i < nt_1char_size; i++ )
	{	uint ncnt = nt_1char_first + i;
		uint nidx = name_count_to_index(ncnt);
		register name *pname = name_index_ptr_inline(nt, nidx);
		if ( i < 0 )
		  pname->string_bytes = nt_1char_names,
		  pname->string_size = 0;
		else
		  pname->string_bytes = nt_1char_names + i,
		  pname->string_size = 1;
		pname->foreign_string = 1;
		pname->mark = 1;
		pname->pvalue = pv_no_defn;
	}
	/* Reconstruct the free list. */
	nt->free = 0;
	name_gc_cleanup(NULL);
	/* Register the name table root. */
	gs_register_struct_root(mem, &the_nt_root, (void **)&the_nt,
				"name table");
	return nt;
}

/* Return the one and only table. */
const name_table *
the_name_table(void)
{	return the_nt;
}

/* Get the allocator for the name table. */
gs_memory_t *
name_memory(void)
{	return the_nt->memory;
}

/* Look up or enter a name in the table. */
/* Return 0 or an error code. */
/* The return may overlap the characters of the string! */
/* See iname.h for the meaning of enterflag. */
int
name_ref(const byte *ptr, uint size, ref *pref, int enterflag)
{	name_table *nt = the_nt;
	register name *pname;
	uint nidx;
	uint *phash;

	/* Compute a hash for the string. */
	{	uint hash;
		const byte *p = ptr;
		uint n = size;
		/* Make a special check for 1-character names. */
		switch ( size )
		  {
		  case 0:
		  	nidx = name_count_to_index(1);
		  	pname = name_index_ptr_inline(nt, nidx);
			goto mkn;
		  case 1:
			if ( *p < nt_1char_size )
			  {	hash = *p + nt_1char_first;
				nidx = name_count_to_index(hash);
				pname = name_index_ptr_inline(nt, nidx);
				goto mkn;
			  }
			/* falls through */
		  default:
			hash = hash_permutation[*p++];
			while ( --n > 0 )
			  hash = (hash << 8) |
			    hash_permutation[(byte)hash ^ *p++];
		  }
		phash = nt->hash + (hash & (nt_hash_size - 1));
	}

	for ( nidx = *phash; nidx != 0;
	      nidx = name_next_index(nidx, pname)
	    )
	{	pname = name_index_ptr_inline(nt, nidx);
		if ( pname->string_size == size &&
		     !memcmp_inline(ptr, pname->string_bytes, size)
		   )
		  goto mkn;
	}
	/* Name was not in the table.  Make a new entry. */
	if ( enterflag < 0 )
	  return_error(e_undefined);
	if ( size > max_name_string )
	  return_error(e_limitcheck);
	nidx = nt->free;
	if ( nidx == 0 )
	   {	int code = name_alloc_sub(nt);
		if ( code < 0 ) return code;
		nidx = nt->free;
	   }
	pname = name_index_ptr_inline(nt, nidx);
	if ( enterflag == 1 )
	{	byte *cptr = (byte *)gs_alloc_string(nt->memory, size,
						     "name_ref(string)");
		if ( cptr == 0 )
		  return_error(e_VMerror);
		memcpy(cptr, ptr, size);
		pname->string_bytes = cptr;
		pname->foreign_string = 0;
	}
	else
	{	pname->string_bytes = ptr;
		pname->foreign_string = (enterflag == 0 ? 1 : 0);
	}
	pname->string_size = size;
	pname->pvalue = pv_no_defn;
	nt->free = name_next_index(nidx, pname);
	set_name_next_index(nidx, pname, *phash);
	*phash = nidx;
	if_debug_name("new name", pname, nidx, &enterflag);
mkn:	make_name(pref, nidx, pname);
	return 0;
}

/* Get the string for a name. */
void
name_string_ref(const ref *pnref /* t_name */,
  ref *psref /* result, t_string */)
{	name *pname = pnref->value.pname;
	const name_table *nt = the_nt;
	make_const_string(psref,
			  (pname->foreign_string ? avm_foreign :
			   imemory_space((gs_ref_memory_t *)nt->memory))
			   | a_readonly,
			  pname->string_size,
			  (const byte *)pname->string_bytes);
}

/* Convert a t_string object to a name. */
/* Copy the executable attribute. */
int
name_from_string(const ref *psref, ref *pnref)
{	int exec = r_has_attr(psref, a_executable);
	int code = name_ref(psref->value.bytes, r_size(psref), pnref, 1);
	if ( code < 0 )
	  return code;
	if ( exec )
	  r_set_attrs(pnref, a_executable);
	return code;
}

/* Enter a (permanently allocated) C string as a name. */
int
name_enter_string(const char *str, ref *pref)
{	return name_ref((const byte *)str, strlen(str), pref, 0);
}

/* Invalidate the value cache for a name. */
void
name_invalidate_value_cache(const ref *pnref)
{	pnref->value.pname->pvalue = pv_other;
}

/* Convert between names and indices. */
#undef name_index
uint
name_index(const ref *pnref)
{	return name_index_inline(pnref);
}
void
name_index_ref(uint index, ref *pnref)
{	name_index_ref_inline(the_nt, index, pnref);
}
name *
name_index_ptr(uint index)
{	return name_index_ptr_inline(the_nt, index);
}

/* Get the index of the next valid name. */
/* The argument is 0 or a valid index. */
/* Return 0 if there are no more. */
uint
name_next_valid_index(uint nidx)
{	name_table *nt = the_nt;
	name_sub_table *sub = nt->sub_tables[nidx >> nt_log2_sub_size];
	name *pname;
	do
	  { ++nidx;
	    if ( (nidx & nt_sub_index_mask) == 0 )
	      for ( ; ; nidx += nt_sub_size )
		{ if ( (nidx >> nt_log2_sub_size) >= nt->sub_count )
		    return 0;
		  sub = nt->sub_tables[nidx >> nt_log2_sub_size];
		  if ( sub != 0 )
		    break;
		}
	    pname = &sub->names[nidx & nt_sub_index_mask];
	  }
	while ( pname->string_bytes == 0 );
	return nidx;
}

/* ------ Garbage collection ------ */

/* Unmark all names, except for 1-character permanent names, */
/* before a garbage collection. */
void
name_unmark_all(void)
{	name_table *nt = the_nt;
	uint si;
	name_sub_table *sub;
	for ( si = 0; si < nt->sub_count; ++si )
	  if ( (sub = nt->sub_tables[si]) != 0 )
	    { uint i;
	      for ( i = 0; i < nt_sub_size; ++i )
		sub->names[i].mark = 0;
	    }
	{ uint ncnt;
	  for ( ncnt = 1; ncnt <= nt_1char_size; ++ncnt )
	    name_index_ptr(name_count_to_index(ncnt))->mark = 1;
	}
}

/* Mark a name.  Return true if new mark.  We export this so we can mark */
/* character names in the character cache. */
bool
name_mark_index(uint nidx)
{	name *pname = name_index_ptr(nidx);
	if ( pname->mark )
	  return false;
	pname->mark = 1;
	return true;
}

/* Get the object (sub-table) containing a name. */
/* The garbage collector needs this so it can relocate pointers to names. */
void/*obj_header_t*/ *
name_ref_sub_table(const ref *pnref)
{	/* When this procedure is called, the pointers from the name table */
	/* to the sub-tables may or may not have been relocated already, */
	/* so we can't use them.  Instead, we have to work backwards from */
	/* the name pointer itself. */
	return pnref->value.pname - (name_index_inline(pnref) & nt_sub_index_mask);
}
void/*obj_header_t*/ *
name_index_ptr_sub_table(uint index, name *pname)
{	return pname - (index & nt_sub_index_mask);
}

/* Clean up the name table after a garbage collection, by removing */
/* names that aren't marked, and relocating name string pointers */
/* (the latter only if gcst != 0). */
void
name_gc_cleanup(gc_state_t *gcst)
{	name_table *nt = the_nt;
	uint *phash = &nt->hash[0];
	register uint i;
	for ( i = 0; i < nt_hash_size; phash++, i++ )
	   {	uint prev = 0;
		name *pnprev;
		uint nidx = *phash;
		while ( nidx != 0 )
		{	name *pname = name_index_ptr_inline(nt, nidx);
			uint next = name_next_index(nidx, pname);
			if ( pname->mark )
			{	if ( !pname->foreign_string && gcst != NULL)
				  {	gs_const_string nstr;
					nstr.data = pname->string_bytes;
					nstr.size = pname->string_size;
					gs_reloc_const_string(&nstr, gcst);
					pname->string_bytes = nstr.data;
				  }
				prev = nidx;
				pnprev = pname;
			}
			else
			{	if_debug_name("GC remove name", pname, nidx,
					      NULL);
				/* Zero out the string data for the GC. */
				pname->string_bytes = 0;
				pname->string_size = 0;
				if ( prev == 0 )
				  *phash = next;
				else
				  set_name_next_index(prev, pnprev, next);
			}
			nidx = next;
		}
	}
	/* Reconstruct the free list. */
	nt->free = 0;
	for ( i = nt->sub_count - 1; ; --i )
	  { name_scan_sub(nt, i, true);
	    if ( i == 0 )
	      break;
	  }
	nt->sub_next = 0;
}

/* ------ Save/restore ------ */

/* Clean up the name table before a restore. */
/* Currently, this is never called, because the name table is allocated */
/* in system VM.  However, for a Level 1 system, we might choose to */
/* allocate the name table in global VM; in this case, this routine */
/* would be called before doing the global part of a top-level restore. */
/* Currently we don't make any attempt to optimize this. */
void
name_restore(alloc_save_t *save)
{	name_table *nt = the_nt;
	/* We simply mark all names older than the save, */
	/* and let name_gc_cleanup sort everything out. */
	uint si;
	for ( si = 0; si < nt->sub_count; ++si )
	  if ( nt->sub_tables[si] != 0 )
	    { uint i;
	      for ( i = 0; i < nt_sub_size; ++i )
		{ name *pname =
		    name_index_ptr_inline(nt, (si << nt_log2_sub_size) + i);
		  if ( pname->string_bytes == 0 )
		    pname->mark = 0;
		  else if ( pname->foreign_string )
		    pname->mark = 1;
		  else
		    pname->mark =
		      !alloc_is_since_save(pname->string_bytes, save);
		}
	    }
	name_gc_cleanup(NULL);
}

/* ------ Internal procedures ------ */

/* Allocate the next sub-table. */
private int
name_alloc_sub(name_table *nt)
{	uint sub_index = nt->sub_next;
	name_sub_table *sub;

	for ( ; ; ++sub_index )
	  {	if ( sub_index > nt->max_sub_count )
		  return_error(e_limitcheck);
		if ( nt->sub_tables[sub_index] == 0 )
		  break;
	  }
	nt->sub_next = sub_index + 1;
	if ( nt->sub_next > nt->sub_count )
	  nt->sub_count = nt->sub_next;
	sub = gs_alloc_struct(nt->memory, name_sub_table, &st_name_sub_table,
			      "name_alloc_sub");
	if ( sub == 0 )
	  return_error(e_VMerror);
	memset(sub, 0, sizeof(name_sub_table));
	/* The following code is only used if EXTEND_NAMES is non-zero. */
	if ( sub_index >= 0x10000L >> nt_log2_sub_size )
	{	/* Fill in my_extension in all the newly created names. */
		uint extn = sub_index >> (16 - nt_log2_sub_size);
		int i;
		for ( i = 0; i < nt_sub_size; ++i )
		  set_name_extension(&sub->names[i], extn);
	}
	nt->sub_tables[sub_index] = sub;
	/* Add the newly allocated entries to the free list. */
	/* Note that the free list will only be properly sorted if */
	/* it was empty initially. */
	name_scan_sub(nt, sub_index, false);
#ifdef DEBUG
	if ( gs_debug_c('n') )
	  {	/* Print the lengths of the hash chains. */
		int i0;
		for ( i0 = 0; i0 < nt_hash_size; i0 += 16 )
		  {	int i;
			dprintf1("[n]chain %d:", i0);
			for ( i = i0; i < i0 + 16; i++ )
			  {	int n = 0;
				uint nidx;
				for ( nidx = nt->hash[i]; nidx != 0;
				      nidx = name_next_index(nidx, name_index_ptr_inline(nt, nidx))
				    )
				  n++;
				dprintf1(" %d", n);
			  }
			dputc('\n');
		  }
	  }
#endif
	return 0;
}

/* Scan a sub-table and add unmarked entries to the free list. */
/* We add the entries in decreasing count order, so the free list */
/* will stay sorted.  If all entries are unmarked and free_empty is true, */
/* free the sub-table. */
private void
name_scan_sub(name_table *nt, uint sub_index, bool free_empty)
{	name_sub_table *sub = nt->sub_tables[sub_index];
	uint free = nt->free;
	uint nbase = sub_index << nt_log2_sub_size;
	uint ncnt = nbase + (nt_sub_size - 1);
	bool keep = !free_empty;

	if ( sub == 0 )
	  return;
	if ( nbase == 0 )
	  nbase = 1, keep = true;		/* don't free name 0 */
	for ( ; ; --ncnt )
	  {	uint nidx = name_count_to_index(ncnt);
		name *pname = &sub->names[nidx & nt_sub_index_mask];
		if ( pname->mark )
		  keep = true;
		else
		  { set_name_next_index(nidx, pname, free);
		    free = nidx;
		  }
		if ( ncnt == nbase )
		  break;
	  }
	if ( keep )
	  nt->free = free;
	else
	  {	/* No marked entries, free the sub-table. */
		gs_free_object(nt->memory, sub, "name_scan_sub");
		nt->sub_tables[sub_index] = 0;
		if ( sub_index == nt->sub_count - 1 )
		  {	nt->sub_count = sub_index;
			if ( nt->sub_next == sub_index )
			  nt->sub_next--;
		  }
	  }
}

/* Garbage collector enumeration and relocation procedures. */
#define ntptr ((name_table *)vptr)
private ENUM_PTRS_BEGIN_PROC(name_table_enum_ptrs) {
	if ( index >= ntptr->sub_count )
	  return 0;
	*pep = ntptr->sub_tables[index];
	return ptr_struct_type;
} ENUM_PTRS_END_PROC
private RELOC_PTRS_BEGIN(name_table_reloc_ptrs) {
	name_sub_table **sub = ntptr->sub_tables;
	uint sub_count = ntptr->sub_count;
	uint i;
	/* Now we can relocate the sub-table pointers. */
	for ( i = 0; i < sub_count; i++, sub++ )
	  *sub = gs_reloc_struct_ptr(*sub, gcst);
	/*
	 * We also need to relocate the cached value pointers.
	 * We don't do this here, but in a separate scan over the
	 * permanent dictionaries, at the very end of garbage collection.
	 */
} RELOC_PTRS_END
