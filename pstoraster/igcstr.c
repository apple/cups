/* Copyright (C) 1994, 1995, 1996, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: igcstr.c,v 1.2 2000/03/08 23:15:11 mike Exp $ */
/* String GC routines for Ghostscript */
#include "memory_.h"
#include "ghost.h"
#include "gsmdebug.h"
#include "gsstruct.h"
#include "iastate.h"
#include "igcstr.h"

/* Forward references */
private bool gc_mark_string(P4(const byte *, uint, bool, const chunk_t *));

/* (Un)mark the strings in a chunk. */
void
gc_strings_set_marks(chunk_t * cp, bool mark)
{
    if (cp->smark != 0) {
	if_debug3('6', "[6]clearing string marks 0x%lx[%u] to %d\n",
		  (ulong) cp->smark, cp->smark_size, (int)mark);
	memset(cp->smark, 0, cp->smark_size);
	if (mark)
	    gc_mark_string(cp->sbase, cp->climit - cp->sbase, true, cp);
    }
}

/* We mark strings a word at a time. */
typedef string_mark_unit bword;

#define bword_log2_bytes log2_sizeof_string_mark_unit
#define bword_bytes (1 << bword_log2_bytes)
#define bword_log2_bits (bword_log2_bytes + 3)
#define bword_bits (1 << bword_log2_bits)
#define bword_1s (~(bword)0)
/* Compensate for byte order reversal if necessary. */
#if arch_is_big_endian
#  if bword_bytes == 2
#    define bword_swap_bytes(m) m = (m << 8) | (m >> 8)
#  else				/* bword_bytes == 4 */
#    define bword_swap_bytes(m)\
	m = (m << 24) | ((m & 0xff00) << 8) | ((m >> 8) & 0xff00) | (m >> 24)
#  endif
#else
#  define bword_swap_bytes(m) DO_NOTHING
#endif

/* (Un)mark a string in a known chunk.  Return true iff any new marks. */
private bool
gc_mark_string(const byte * ptr, uint size, bool set, const chunk_t * cp)
{
    uint offset = ptr - cp->sbase;
    bword *bp = (bword *) (cp->smark + ((offset & -bword_bits) >> 3));
    uint bn = offset & (bword_bits - 1);
    bword m = bword_1s << bn;
    uint left = size;
    bword marks = 0;

    bword_swap_bytes(m);
    if (set) {
	if (left + bn >= bword_bits) {
	    marks |= ~*bp & m;
	    *bp |= m;
	    m = bword_1s, left -= bword_bits - bn, bp++;
	    while (left >= bword_bits) {
		marks |= ~*bp;
		*bp = bword_1s;
		left -= bword_bits, bp++;
	    }
	}
	if (left) {
	    bword_swap_bytes(m);
	    m -= m << left;
	    bword_swap_bytes(m);
	    marks |= ~*bp & m;
	    *bp |= m;
	}
    } else {
	if (left + bn >= bword_bits) {
	    *bp &= ~m;
	    m = bword_1s, left -= bword_bits - bn, bp++;
	    if (left >= bword_bits * 5) {
		memset(bp, 0, (left & -bword_bits) >> 3);
		bp += left >> bword_log2_bits;
		left &= bword_bits - 1;
	    } else
		while (left >= bword_bits) {
		    *bp = 0;
		    left -= bword_bits, bp++;
		}
	}
	if (left) {
	    bword_swap_bytes(m);
	    m -= m << left;
	    bword_swap_bytes(m);
	    *bp &= ~m;
	}
    }
    return marks != 0;
}

/* Mark a string.  Return true if any new marks. */
bool
gc_string_mark(const byte * ptr, uint size, bool set, gc_state_t * gcst)
{
    const chunk_t *cp;
    bool marks;

    if (size == 0)
	return false;
#define dprintstr()\
  dputc('('); fwrite(ptr, 1, min(size, 20), dstderr);\
  dputs((size <= 20 ? ")" : "...)"))
    if (!(cp = gc_locate(ptr, gcst))) {		/* not in a chunk */
#ifdef DEBUG
	if (gs_debug_c('5')) {
	    dlprintf2("[5]0x%lx[%u]", (ulong) ptr, size);
	    dprintstr();
	    dputs(" not in a chunk\n");
	}
#endif
	return false;
    }
    if (cp->smark == 0)		/* not marking strings */
	return false;
#ifdef DEBUG
    if (ptr < cp->ctop) {
	lprintf4("String pointer 0x%lx[%u] outside [0x%lx..0x%lx)\n",
		 (ulong) ptr, size, (ulong) cp->ctop, (ulong) cp->climit);
	return false;
    } else if (ptr + size > cp->climit) {	/*
						 * If this is the bottommost string in a chunk that has
						 * an inner chunk, the string's starting address is both
						 * cp->ctop of the outer chunk and cp->climit of the inner;
						 * gc_locate may incorrectly attribute the string to the
						 * inner chunk because of this.  This doesn't affect
						 * marking or relocation, since the machinery for these
						 * is all associated with the outermost chunk,
						 * but it can cause the validity check to fail.
						 * Check for this case now.
						 */
	const chunk_t *scp = cp;

	while (ptr == scp->climit && scp->outer != 0)
	    scp = scp->outer;
	if (ptr + size > scp->climit) {
	    lprintf4("String pointer 0x%lx[%u] outside [0x%lx..0x%lx)\n",
		     (ulong) ptr, size,
		     (ulong) scp->ctop, (ulong) scp->climit);
	    return false;
	}
    }
#endif
    marks = gc_mark_string(ptr, size, set, cp);
#ifdef DEBUG
    if (gs_debug_c('5')) {
	dlprintf4("[5]%s%smarked 0x%lx[%u]",
		  (marks ? "" : "already "), (set ? "" : "un"),
		  (ulong) ptr, size);
	dprintstr();
	dputc('\n');
    }
#endif
    return marks;
}

/* Clear the relocation for strings. */
/* This requires setting the marks. */
void
gc_strings_clear_reloc(chunk_t * cp)
{
    if (cp->sreloc != 0) {
	gc_strings_set_marks(cp, true);
	if_debug1('6', "[6]clearing string reloc 0x%lx\n",
		  (ulong) cp->sreloc);
	gc_strings_set_reloc(cp);
    }
}

/* Count the 0-bits in a byte. */
private const byte count_zero_bits_table[256] =
{
#define o4(n) n,n-1,n-1,n-2
#define o16(n) o4(n),o4(n-1),o4(n-1),o4(n-2)
#define o64(n) o16(n),o16(n-1),o16(n-1),o16(n-2)
    o64(8), o64(7), o64(7), o64(6)
};

#define byte_count_zero_bits(byt)\
  (uint)(count_zero_bits_table[byt])
#define byte_count_one_bits(byt)\
  (uint)(8 - count_zero_bits_table[byt])

/* Set the relocation for the strings in a chunk. */
/* The sreloc table stores the relocated offset from climit for */
/* the beginning of each block of string_data_quantum characters. */
void
gc_strings_set_reloc(chunk_t * cp)
{
    if (cp->sreloc != 0 && cp->smark != 0) {
	byte *bot = cp->ctop;
	byte *top = cp->climit;
	uint count =
	(top - bot + (string_data_quantum - 1)) >>
	log2_string_data_quantum;
	string_reloc_offset *relp =
	cp->sreloc +
	(cp->smark_size >> (log2_string_data_quantum - 3));
	register byte *bitp = cp->smark + cp->smark_size;
	register string_reloc_offset reloc = 0;

	while (count--) {
	    bitp -= string_data_quantum / 8;
	    reloc += string_data_quantum -
		byte_count_zero_bits(bitp[0]);
	    reloc -= byte_count_zero_bits(bitp[1]);
	    reloc -= byte_count_zero_bits(bitp[2]);
	    reloc -= byte_count_zero_bits(bitp[3]);
#if log2_string_data_quantum > 5
	    reloc -= byte_count_zero_bits(bitp[4]);
	    reloc -= byte_count_zero_bits(bitp[5]);
	    reloc -= byte_count_zero_bits(bitp[6]);
	    reloc -= byte_count_zero_bits(bitp[7]);
#endif
	    *--relp = reloc;
	}
    }
    cp->sdest = cp->climit;
}

/* Relocate a string pointer. */
void
igc_reloc_string(gs_string * sptr, gc_state_t * gcst)
{
    byte *ptr;
    const chunk_t *cp;
    uint offset;
    uint reloc;
    const byte *bitp;
    byte byt;

    if (sptr->size == 0) {
	sptr->data = 0;
	return;
    }
    ptr = sptr->data;
    if (!(cp = gc_locate(ptr, gcst)))	/* not in a chunk */
	return;
    if (cp->sreloc == 0 || cp->smark == 0)	/* not marking strings */
	return;
    offset = ptr - cp->sbase;
    reloc = cp->sreloc[offset >> log2_string_data_quantum];
    bitp = &cp->smark[offset >> 3];
    switch (offset & (string_data_quantum - 8)) {
#if log2_string_data_quantum > 5
	case 56:
	    reloc -= byte_count_one_bits(bitp[-7]);
	case 48:
	    reloc -= byte_count_one_bits(bitp[-6]);
	case 40:
	    reloc -= byte_count_one_bits(bitp[-5]);
	case 32:
	    reloc -= byte_count_one_bits(bitp[-4]);
#endif
	case 24:
	    reloc -= byte_count_one_bits(bitp[-3]);
	case 16:
	    reloc -= byte_count_one_bits(bitp[-2]);
	case 8:
	    reloc -= byte_count_one_bits(bitp[-1]);
    }
    byt = *bitp & (0xff >> (8 - (offset & 7)));
    reloc -= byte_count_one_bits(byt);
    if_debug2('5', "[5]relocate string 0x%lx to 0x%lx\n",
	      (ulong) ptr, (ulong) (cp->sdest - reloc));
    sptr->data = cp->sdest - reloc;
}
void
igc_reloc_const_string(gs_const_string * sptr, gc_state_t * gcst)
{				/* We assume the representation of byte * and const byte * is */
    /* the same.... */
    igc_reloc_string((gs_string *) sptr, gcst);
}

/* Compact the strings in a chunk. */
void
gc_strings_compact(chunk_t * cp)
{
    if (cp->smark != 0) {
	byte *hi = cp->climit;
	byte *lo = cp->ctop;
	register const byte *from = hi;
	register byte *to = hi;
	register const byte *bp = cp->smark + cp->smark_size;

#ifdef DEBUG
	if (gs_debug_c('4') || gs_debug_c('5')) {
	    byte *base = cp->sbase;
	    uint i = (lo - base) & -string_data_quantum;
	    uint n = round_up(hi - base, string_data_quantum);

#define R 16
	    for (; i < n; i += R) {
		uint j;

		dlprintf1("[4]0x%lx: ", (ulong) (base + i));
		for (j = i; j < i + R; j++) {
		    byte ch = base[j];

		    if (ch <= 31) {
			dputc('^');
			dputc(ch + 0100);
		    } else
			dputc(ch);
		}
		dputc(' ');
		for (j = i; j < i + R; j++)
		    dputc((cp->smark[j >> 3] & (1 << (j & 7)) ?
			   '+' : '.'));
#undef R
		if (!(i & (string_data_quantum - 1)))
		    dprintf1(" %u", cp->sreloc[i >> log2_string_data_quantum]);
		dputc('\n');
	    }
	}
#endif
	while (from > lo) {
	    register byte b = *--bp;

	    from -= 8;
	    switch (b) {
		case 0xff:
		    to -= 8;
		    if (to != from) {
			to[7] = from[7];
			to[6] = from[6];
			to[5] = from[5];
			to[4] = from[4];
			to[3] = from[3];
			to[2] = from[2];
			to[1] = from[1];
			to[0] = from[0];
		    }
		    break;
		default:
		    if (b & 0x80)
			*--to = from[7];
		    if (b & 0x40)
			*--to = from[6];
		    if (b & 0x20)
			*--to = from[5];
		    if (b & 0x10)
			*--to = from[4];
		    if (b & 8)
			*--to = from[3];
		    if (b & 4)
			*--to = from[2];
		    if (b & 2)
			*--to = from[1];
		    if (b & 1)
			*--to = from[0];
		    /* falls through */
		case 0:
		    ;
	    }
	}
	gs_alloc_fill(cp->ctop, gs_alloc_fill_collected,
		      to - cp->ctop);
	cp->ctop = to;
    }
}
