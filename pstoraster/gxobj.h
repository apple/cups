/* Copyright (C) 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gxobj.h,v 1.2 2000/03/08 23:15:02 mike Exp $ */
/* Memory manager implementation structures for Ghostscript */

#ifndef gxobj_INCLUDED
#  define gxobj_INCLUDED

#include "gxbitmap.h"

/* ================ Objects ================ */

/*
 * Object headers come in a number of different varieties.
 * All arise from the same basic form, which is
 -l- -lmsize/mark/back-
 -size-
 -type/reloc-
 * l (large) is a single bit.  The size of lmsize/mark/back, size, and type
 * varies according to the environment.  On machines with N:16 segmented
 * addressing, 16-bit ints, and no alignment requirement more severe than
 * 2 bytes, we can fit an object header into 8 bytes by making the first
 * two fields only 16 bits wide.  On all other machines, we let the
 * lmsize/mark/back field be 1 bit shorter than a uint, and round the header
 * size up to the next multiple of the most severe alignment restriction
 * (4 or 8 bytes).  Miraculously, we can do all this without any case testing.
 *
 * The mark/back field is used for the mark during the marking phase of
 * garbage collection, and for a back pointer value during the compaction
 * phase.  Since we want to be able to collect local VM independently of
 * global VM, we need two different distinguished mark values:
 *      - For local objects that have not been traced and should be freed
 *      (compacted out), we use 1...11 in the mark field (o_unmarked).
 *      - For global objects that have not been traced but should be kept,
 *      we use 1...10 in the mark field (o_untraced).
 * Note that neither of these values is a possible real relocation value.
 *
 * The lmsize field of large objects overlaps mark and back, so we must
 * handle these functions for large objects in some other way.
 * Since large objects cannot be moved or relocated, we don't need the
 * back field for them; we allocate 2 bits for the 3 mark values.
 */
/*
 * The back pointer's meaning depends on whether the object is
 * free (unmarked) or in use (marked):
 *      - In free objects, the back pointer is an offset from the object
 * header back to a chunk_head_t structure that contains the location
 * to which all the data in this chunk will get moved; the reloc field
 * contains the amount by which the following run of useful objects
 * will be relocated downwards.
 *      - In useful objects, the back pointer is an offset from the object
 * back to the previous free object; the reloc field is not used (it
 * overlays the type field).
 * These two cases can be distinguished when scanning a chunk linearly,
 * but when simply examining an object via a pointer, the chunk pointer
 * is also needed.
 */
#define obj_flag_bits 1
#define obj_mb_bits (arch_sizeof_int * 8 - obj_flag_bits)
#define obj_ls_bits (obj_mb_bits - 2)
#define o_unmarked (((uint)1 << obj_mb_bits) - 1)
#define o_l_unmarked (o_unmarked & 3)
#define o_set_unmarked_large(pp) (pp)->o_lmark = o_l_unmarked
#define o_set_unmarked(pp)\
  if ( (pp)->o_large ) o_set_unmarked_large(pp);\
  else (pp)->o_smark = o_unmarked
#define o_is_unmarked_large(pp) ((pp)->o_lmark == o_l_unmarked)
#define o_is_unmarked(pp)\
 ((pp)->o_large ? o_is_unmarked_large(pp) :\
  ((pp)->o_smark == o_unmarked))
#define o_untraced (((uint)1 << obj_mb_bits) - 2)
#define o_l_untraced (o_untraced & 3)
#define o_set_untraced(pp)\
  if ( (pp)->o_large ) (pp)->o_lmark = o_l_untraced;\
  else (pp)->o_smark = o_untraced
#define o_is_untraced(pp)\
 ((pp)->o_large ? (pp)->o_lmark == o_l_untraced :\
  ((pp)->o_smark == o_untraced))
#define o_marked 0
#define o_mark_large(pp) (pp)->o_lmark = o_marked
#define o_mark(pp)\
  if ( (pp)->o_large ) o_mark_large(pp);\
  else (pp)->o_smark = o_marked
#define obj_back_shift obj_flag_bits
#define obj_back_scale (1 << obj_back_shift)
typedef struct obj_header_data_s {
    union _f {
	struct _h {
	    unsigned large:1;
	} h;
	struct _l {
	    unsigned _:1, lmark:2, lsize:obj_ls_bits;
	} l;
	struct _m {
	    unsigned _:1, smark:obj_mb_bits;
	} m;
	struct _b {
	    unsigned _:1, back:obj_mb_bits;
	} b;
    } f;
    uint size;
    union _t {
	gs_memory_type_ptr_t type;
	uint reloc;
    } t;
} obj_header_data_t;

/*
 * Define the alignment modulus for aligned objects.  We assume all
 * alignment values are powers of 2; we can avoid nested 'max'es that way.
 * The final | is because back pointer values are divided by obj_back_scale,
 * so objects must be aligned at least 0 mod obj_back_scale.
 */
#define obj_align_mod\
  (((arch_align_long_mod - 1) | (arch_align_ptr_mod - 1) |\
    (arch_align_double_mod - 1) | (align_bitmap_mod - 1) |\
    (obj_back_scale - 1)) + 1)
/* The only possible values for obj_align_mod are 4, 8, or 16.... */
#if obj_align_mod == 4
#  define log2_obj_align_mod 2
#else
#if obj_align_mod == 8
#  define log2_obj_align_mod 3
#else
#if obj_align_mod == 16
#  define log2_obj_align_mod 4
#endif
#endif
#endif
#define obj_align_mask (obj_align_mod-1)
#define obj_align_round(siz)\
  (uint)(((siz) + obj_align_mask) & -obj_align_mod)
#define obj_size_round(siz)\
  obj_align_round((siz) + sizeof(obj_header_t))

/* Define the real object header type, taking alignment into account. */
struct obj_header_s {		/* must be a struct because of forward reference */
    union _d {
	obj_header_data_t o;
	byte _pad[round_up(sizeof(obj_header_data_t), obj_align_mod)];
    }
    d;
};

/* Define some reasonable abbreviations for the fields. */
#define o_large d.o.f.h.large
#define o_lsize d.o.f.l.lsize
#define o_lmark d.o.f.l.lmark
#define o_back d.o.f.b.back
#define o_smark d.o.f.m.smark
#define o_size d.o.size
#define o_type d.o.t.type
#define o_nreloc d.o.t.reloc

/*
 * The macros for getting the sizes of objects all take pointers to
 * the object header, for use when scanning storage linearly.
 */
#define pre_obj_small_size(pp)\
  ((pp)->o_size)

#if arch_sizeof_long > arch_sizeof_int

	/* Large objects need to use o_lsize. */

#define pre_obj_large_size(pp)\
  (((ulong)(pp)->o_lsize << (arch_sizeof_int * 8)) + (pp)->o_size)
#define pre_obj_set_large_size(pp, lsize)\
  ((pp)->o_lsize = (lsize) >> (arch_sizeof_int * 8),\
   (pp)->o_size = (uint)(lsize))
#define pre_obj_contents_size(pp)\
  ((pp)->o_large ? pre_obj_large_size(pp) : pre_obj_small_size(pp))

#else

	/* Large objects don't need to use o_lsize. */

#define pre_obj_large_size(pp)\
  pre_obj_small_size(pp)
#define pre_obj_set_large_size(pp, lsize)\
  ((pp)->o_lsize = 0,\
   (pp)->o_size = (lsize))
#define pre_obj_contents_size(pp)\
  pre_obj_small_size(pp)

#endif

#define pre_obj_rounded_size(pp)\
  obj_size_round(pre_obj_contents_size(pp))
#define pre_obj_next(pp)\
  ((obj_header_t *)((byte *)(pp) + obj_align_round(\
    pre_obj_contents_size(pp) + sizeof(obj_header_t) )))

/*
 * Define the header that free objects point back to when relocating.
 * Every chunk, including inner chunks, has one of these.
 */
typedef struct chunk_head_s {
    byte *dest;			/* destination for objects */
#if obj_align_mod > arch_sizeof_ptr
    byte *_pad[obj_align_mod / arch_sizeof_ptr - 1];
#endif
    obj_header_t free;		/* header for a free object, */
    /* in case the first real object */
    /* is in use */
} chunk_head_t;

#endif /* gxobj_INCLUDED */
