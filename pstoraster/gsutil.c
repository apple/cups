/* Copyright (C) 1992, 1993, 1994, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gsutil.c,v 1.2 2000/03/08 23:14:49 mike Exp $ */
/* Utilities for Ghostscript library */
#include "string_.h"
#include "memory_.h"
#include "gstypes.h"
#include "gconfigv.h"		/* for USE_ASM */
#include "gsmemory.h"		/* for init procedure */
#include "gsrect.h"		/* for prototypes */
#include "gsuid.h"
#include "gsutil.h"		/* for prototypes */

/* ------ Unique IDs ------ */

/* Generate a block of unique IDs. */
static ulong gs_next_id;

void
gs_gsutil_init(gs_memory_t *mem)
{
    gs_next_id = 1;
}

ulong
gs_next_ids(uint count)
{
    ulong id = gs_next_id;

    gs_next_id += count;
    return id;
}

/* ------ Memory utilities ------ */

/* Transpose an 8 x 8 block of bits.  line_size is the raster of */
/* the input data.  dist is the distance between output bytes. */
/* This routine may be supplanted by assembly code. */
#if !USE_ASM

void
memflip8x8(const byte * inp, int line_size, byte * outp, int dist)
{
    register uint ae, bf, cg, dh;

    {
	const byte *ptr4 = inp + (line_size << 2);

	ae = ((uint) * inp << 8) + *ptr4;
	inp += line_size, ptr4 += line_size;
	bf = ((uint) * inp << 8) + *ptr4;
	inp += line_size, ptr4 += line_size;
	cg = ((uint) * inp << 8) + *ptr4;
	inp += line_size, ptr4 += line_size;
	dh = ((uint) * inp << 8) + *ptr4;
    }

    /* Check for all 8 bytes being the same. */
    /* This is especially worth doing for the case where all are zero. */
    if (ae == bf && ae == cg && ae == dh && (ae >> 8) == (ae & 0xff)) {
	if (ae == 0)
	    goto store;
	*outp = -((ae >> 7) & 1);
	outp += dist;
	*outp = -((ae >> 6) & 1);
	outp += dist;
	*outp = -((ae >> 5) & 1);
	outp += dist;
	*outp = -((ae >> 4) & 1);
	outp += dist;
	*outp = -((ae >> 3) & 1);
	outp += dist;
	*outp = -((ae >> 2) & 1);
	outp += dist;
	*outp = -((ae >> 1) & 1);
	outp += dist;
	*outp = -(ae & 1);
	return;
    } {
	register uint temp;

/* Transpose a block of bits between registers. */
#define transpose(r,s,mask,shift)\
  r ^= (temp = ((s >> shift) ^ r) & mask);\
  s ^= temp << shift

/* Transpose blocks of 4 x 4 */
#define transpose4(r) transpose(r,r,0x00f0,4)
	transpose4(ae);
	transpose4(bf);
	transpose4(cg);
	transpose4(dh);

/* Transpose blocks of 2 x 2 */
	transpose(ae, cg, 0x3333, 2);
	transpose(bf, dh, 0x3333, 2);

/* Transpose blocks of 1 x 1 */
	transpose(ae, bf, 0x5555, 1);
	transpose(cg, dh, 0x5555, 1);

    }

  store:*outp = ae >> 8;
    outp += dist;
    *outp = bf >> 8;
    outp += dist;
    *outp = cg >> 8;
    outp += dist;
    *outp = dh >> 8;
    outp += dist;
    *outp = (byte) ae;
    outp += dist;
    *outp = (byte) bf;
    outp += dist;
    *outp = (byte) cg;
    outp += dist;
    *outp = (byte) dh;
}

#endif /* !USE_ASM */

/* ------ String utilities ------ */

/* Compare two strings, returning -1 if the first is less, */
/* 0 if they are equal, and 1 if first is greater. */
/* We can't use memcmp, because we always use unsigned characters. */
int
bytes_compare(const byte * s1, uint len1, const byte * s2, uint len2)
{
    register uint len = len1;

    if (len2 < len)
	len = len2;
    {
	register const byte *p1 = s1;
	register const byte *p2 = s2;

	while (len--)
	    if (*p1++ != *p2++)
		return (p1[-1] < p2[-1] ? -1 : 1);
    }
    /* Now check for differing lengths */
    return (len1 == len2 ? 0 : len1 < len2 ? -1 : 1);
}

/* Test whether a string matches a pattern with wildcards. */
/* '*' = any substring, '?' = any character, '\' quotes next character. */
private const string_match_params smp_default =
{'*', '?', '\\', false};

bool
string_match(const byte * str, uint len, const byte * pstr, uint plen,
	     register const string_match_params * psmp)
{
    const byte *pback = 0;
    const byte *spback = 0;	/* initialized only to pacify gcc */
    const byte *p = pstr, *pend = pstr + plen;
    const byte *sp = str, *spend = str + len;

    if (psmp == 0)
	psmp = &smp_default;
  again:while (p < pend) {
	register byte ch = *p;

	if (ch == psmp->any_substring) {
	    pback = ++p, spback = sp;
	    continue;
	} else if (ch == psmp->any_char) {
	    if (sp == spend)
		return false;	/* str too short */
	    p++, sp++;
	    continue;
	} else if (ch == psmp->quote_next) {
	    if (++p == pend)
		return true;	/* bad pattern */
	    ch = *p;
	}
	if (sp == spend)
	    return false;	/* str too short */
	if (*sp == ch ||
	    (psmp->ignore_case && (*sp ^ ch) == 0x20 &&
	     (ch &= ~0x20) >= 0x41 && ch <= 0x5a)
	    )
	    p++, sp++;
	else if (pback == 0)
	    return false;	/* no * to back up to */
	else {
	    sp = ++spback;
	    p = pback;
	}
    }
    if (sp < spend) {		/* We got a match, but there are chars left over. */
	/* If we can back up, back up to the only place that */
	/* could produce a complete match, otherwise fail. */
	if (pback == 0)
	    return false;
	p = pback;
	pback = 0;
	sp = spend - (pend - p);
	goto again;
    }
    return true;
}

/* ------ UID utilities ------ */

/* Compare two UIDs for equality. */
/* We know that at least one of them is valid. */
bool
uid_equal(register const gs_uid * puid1, register const gs_uid * puid2)
{
    if (puid1->id != puid2->id)
	return false;
    if (puid1->id >= 0)
	return true;		/* UniqueID */
    return
	!memcmp((const char *)puid1->xvalues,
		(const char *)puid2->xvalues,
		(uint) - (puid1->id) * sizeof(long));
}

/* ------ Rectangle utilities ------ */

/*
 * Calculate the difference of two rectangles, a list of up to 4 rectangles.
 * Return the number of rectangles in the list, and set the first rectangle
 * to the intersection.
 */
int
int_rect_difference(gs_int_rect * outer, const gs_int_rect * inner,
		    gs_int_rect * diffs /*[4] */ )
{
    int x0 = outer->p.x, y0 = outer->p.y;
    int x1 = outer->q.x, y1 = outer->q.y;
    int count = 0;

    if (y0 < inner->p.y) {
	diffs[0].p.x = x0, diffs[0].p.y = y0;
	diffs[0].q.x = x1, diffs[0].q.y = min(y1, inner->p.y);
	outer->p.y = y0 = diffs[0].q.y;
	++count;
    }
    if (y1 > inner->q.y) {
	diffs[count].p.x = x0, diffs[count].p.y = max(y0, inner->q.y);
	diffs[count].q.x = x1, diffs[count].q.y = y1;
	outer->q.y = y1 = diffs[count].p.y;
	++count;
    }
    if (x0 < inner->p.x) {
	diffs[0].p.x = x0, diffs[0].p.y = y0;
	diffs[0].q.x = min(x1, inner->p.x), diffs[0].q.y = y1;
	outer->p.x = x0 = diffs[count].q.x;
	++count;
    }
    if (x1 > inner->q.x) {
	diffs[count].p.x = max(x0, inner->q.x), diffs[count].p.y = y0;
	diffs[count].q.x = x1, diffs[count].q.y = y1;
	outer->q.x = x1 = diffs[count].p.x;
	++count;
    }
    return count;
}
