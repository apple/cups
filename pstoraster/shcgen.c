/* Copyright (C) 1994, 1996, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: shcgen.c,v 1.2 2000/03/08 23:15:25 mike Exp $ */
/* Generate (bounded) Huffman code definitions from frequencies, */
/* and tables from definitions. */
#include "memory_.h"
#include "stdio_.h"
#include <stdlib.h>		/* for qsort */
#include "gdebug.h"
#include "gserror.h"
#include "gserrors.h"
#include "gsmemory.h"
#include "scommon.h"
#include "shc.h"
#include "shcgen.h"

/* ------ Frequency -> definition procedure ------ */

/* Define a node for the Huffman code tree. */
typedef struct count_node_s count_node;
struct count_node_s {
    long freq;			/* frequency of value */
    uint value;			/* data value being encoded */
    uint code_length;		/* length of Huffman code */
    count_node *next;		/* next node in freq-sorted list */
    count_node *left;		/* left child in tree (smaller code_length) */
    count_node *right;		/* right child in tree (greater code_length) */
};

#ifdef DEBUG
#  define debug_print_nodes(nodes, n, tag, lengths)\
     if ( gs_debug_c('W') ) print_nodes_proc(nodes, n, tag, lengths);
private void
print_nodes_proc(const count_node * nodes, int n, const char *tag, int lengths)
{
    int i;

    dlprintf1("[w]---------------- %s ----------------\n", tag);
    for (i = 0; i < n; ++i)
	dlprintf7("[w]node %d: f=%ld v=%d len=%d N=%d L=%d R=%d\n",
		  i, nodes[i].freq, nodes[i].value, nodes[i].code_length,
		  (nodes[i].next == 0 ? -1 : (int)(nodes[i].next - nodes)),
		  (nodes[i].left == 0 ? -1 : (int)(nodes[i].left - nodes)),
		(nodes[i].right == 0 ? -1 : (int)(nodes[i].right - nodes)));
    for (i = lengths; i > 0;) {
	int j = i;
	int len = nodes[--j].code_length;

	while (j > 0 && nodes[j - 1].code_length == len)
	    --j;
	dlprintf2("[w]%d codes of length %d\n", i - j, len);
	i = j;
    }
}
#else
#  define debug_print_nodes(nodes, n, tag, lengths) DO_NOTHING
#endif

/* Node comparison procedures for sorting. */
#define pn1 ((const count_node *)p1)
#define pn2 ((const count_node *)p2)
/* Sort by decreasing frequency. */
private int
compare_freqs(const void *p1, const void *p2)
{
    long diff = pn2->freq - pn1->freq;

    return (diff < 0 ? -1 : diff > 0 ? 1 : 0);
}
/* Sort by increasing code length, and secondarily by decreasing frequency. */
private int
compare_code_lengths(const void *p1, const void *p2)
{
    int diff = pn1->code_length - pn2->code_length;

    return (diff < 0 ? -1 : diff > 0 ? 1 : compare_freqs(p1, p2));
}
/* Sort by increasing code value. */
private int
compare_values(const void *p1, const void *p2)
{
    return (pn1->value < pn2->value ? -1 :
	    pn1->value > pn2->value ? 1 : 0);
}
#undef pn1
#undef pn2

/* Adjust code lengths so that none of them exceeds max_length. */
/* We break this out just to help organize the code; it's only called */
/* from one place in hc_compute. */
private void
hc_limit_code_lengths(count_node * nodes, uint num_values, int max_length)
{
    int needed;			/* # of max_length codes we need to free up */
    count_node *longest = nodes + num_values;

    {				/* Compute the number of additional max_length codes */
	/* we need to make available. */
	int length = longest[-1].code_length;
	int next_length;
	int avail = 0;

	while ((next_length = longest[-1].code_length) > max_length) {
	    avail >>= length - next_length;
	    length = next_length;
	    (--longest)->code_length = max_length;
	    ++avail;
	}
	needed = (nodes + num_values - longest) -
	    (avail >>= (length - max_length));
	if_debug2('W', "[w]avail=%d, needed=%d\n",
		  avail, needed);
    }
    /* Skip over all max_length codes. */
    while (longest[-1].code_length == max_length)
	--longest;

    /*
     * To make available a code of length N, suppose that the next
     * shortest used code is of length M.
     * We take the lowest-frequency code of length M and change it
     * to M+1; we then have to compensate by reducing the length of
     * some of the highest-frequency codes of length N, as follows:
     *       M      new lengths for codes of length N
     *      ---     -----------
     *      N-1     (none)
     *      N-2     N-1
     *      <N-2    M+2, M+2, N-1
     * In the present situation, N = max_length.
     */
    for (; needed > 0; --needed) {	/* longest points to the first code of length max_length. */
	/* Since codes are sorted by increasing code length, */
	/* longest-1 is the desired code of length M. */
	int M1 = ++(longest[-1].code_length);

	switch (max_length - M1) {
	    case 0:		/* M == N-1 */
		--longest;
		break;
	    case 1:		/* M == N-2 */
		longest++->code_length = M1;
		break;
	    default:
		longest->code_length = M1 + 1;
		longest[1].code_length = M1 + 1;
		longest[2].code_length--;
		longest += 3;
	}
    }
}

/* Compute an optimal Huffman code from an input data set. */
/* The client must have set all the elements of *def. */
int
hc_compute(hc_definition * def, const long *freqs, gs_memory_t * mem)
{
    uint num_values = def->num_values;
    count_node *nodes =
    (count_node *) gs_alloc_byte_array(mem, num_values * 2 - 1,
				       sizeof(count_node), "hc_compute");
    int i;
    count_node *lowest;
    count_node *comb;

    if (nodes == 0)
	return_error(gs_error_VMerror);

    /* Create leaf nodes for the input data. */
    for (i = 0; i < num_values; ++i)
	nodes[i].freq = freqs[i], nodes[i].value = i;

    /* Create a list sorted by increasing frequency. */
    /* Also initialize the tree structure. */
    qsort(nodes, num_values, sizeof(count_node), compare_freqs);
    for (i = 0; i < num_values; ++i)
	nodes[i].next = &nodes[i - 1],
	    nodes[i].code_length = 0,
	    nodes[i].left = nodes[i].right = 0;
    nodes[0].next = 0;
    debug_print_nodes(nodes, num_values, "after sort", 0);

    /* Construct the Huffman code tree. */
    for (lowest = &nodes[num_values - 1], comb = &nodes[num_values];;
	 ++comb
	) {
	count_node *pn1 = lowest;
	count_node *pn2 = pn1->next;
	long freq = pn1->freq + pn2->freq;

	/* Create a parent for the two lowest-frequency nodes. */
	lowest = pn2->next;
	comb->freq = freq;
	if (pn1->code_length <= pn2->code_length)
	    comb->left = pn1, comb->right = pn2,
		comb->code_length = pn2->code_length + 1;
	else
	    comb->left = pn2, comb->right = pn1,
		comb->code_length = pn1->code_length + 1;
	if (lowest == 0)	/* no nodes left to combine */
	    break;
	/* Insert comb in the sorted list. */
	if (freq < lowest->freq)
	    comb->next = lowest, lowest = comb;
	else {
	    count_node *here = lowest;

	    while (here->next != 0 && freq >= here->next->freq)
		here = here->next;
	    comb->next = here->next;
	    here->next = comb;
	}
    }

    /* comb (i.e., &nodes[num_values * 2 - 2] is the root of the tree. */
    /* Note that the left and right children of an interior node */
    /* were constructed before, and therefore have lower indices */
    /* in the nodes array than, the parent node.  Thus we can assign */
    /* the code lengths (node depths) in a single descending-order */
    /* sweep. */
    comb++->code_length = 0;
    while (comb > nodes + num_values) {
	--comb;
	comb->left->code_length = comb->right->code_length =
	    comb->code_length + 1;
    }
    debug_print_nodes(nodes, num_values * 2 - 1, "after combine", 0);

    /* Sort the leaves again by code length. */
    qsort(nodes, num_values, sizeof(count_node), compare_code_lengths);
    debug_print_nodes(nodes, num_values, "after re-sort", num_values);

    /* Limit the code length to def->num_counts. */
    hc_limit_code_lengths(nodes, num_values, def->num_counts);
    debug_print_nodes(nodes, num_values, "after limit", num_values);

    /* Sort within each code length by increasing code value. */
    /* This doesn't affect data compression, but it makes */
    /* the code definition itself compress better using our */
    /* incremental encoding. */
    for (i = num_values; i > 0;) {
	int j = i;
	int len = nodes[--j].code_length;

	while (j > 0 && nodes[j - 1].code_length == len)
	    --j;
	qsort(&nodes[j], i - j, sizeof(count_node), compare_values);
	i = j;
    }

    /* Extract the definition from the nodes. */
    memset(def->counts, 0, sizeof(*def->counts) * (def->num_counts + 1));
    for (i = 0; i < num_values; ++i) {
	def->values[i] = nodes[i].value;
	def->counts[nodes[i].code_length]++;
    }

    /* All done, release working storage. */
    gs_free_object(mem, nodes, "hc_compute");
    return 0;
}

/* ------ Byte string <-> definition procedures ------ */

/*
 * We define a compressed representation for (well-behaved) definitions
 * as a byte string.  A "well-behaved" definition is one where if
 * code values A and B have the same code length and A < B,
 * A precedes B in the values table of the definition, and hence
 * A's encoding lexicographically precedes B's.
 *
 * The successive bytes in the compressed string  give the code lengths for
 * runs of decoded values, in the form nnnnllll where nnnn is the number of
 * consecutive values -1 and llll is the code length -1.
 */

/* Convert a definition to a byte string. */
/* The caller must provide the byte string, of length def->num_values. */
/* Assume (do not check) that the definition is well-behaved. */
/* Return the actual length of the string. */
int
hc_bytes_from_definition(byte * dbytes, const hc_definition * def)
{
    int i, j;
    byte *bp = dbytes;
    const byte *lp = dbytes;
    const byte *end = dbytes + def->num_values;
    const ushort *values = def->values;

    /* Temporarily use the output string as a map from */
    /* values to code lengths. */
    for (i = 1; i <= def->num_counts; i++)
	for (j = 0; j < def->counts[i]; j++)
	    bp[*values++] = i;

    /* Now construct the actual string. */
    while (lp < end) {
	const byte *vp;
	byte len = *lp;

	for (vp = lp + 1; vp < end && vp < lp + 16 && *vp == len;)
	    vp++;
	*bp++ = ((vp - lp - 1) << 4) + (len - 1);
	lp = vp;
    }

    return bp - dbytes;
}

/* Extract num_counts and num_values from a byte string. */
void
hc_sizes_from_bytes(hc_definition * def, const byte * dbytes, int num_bytes)
{
    uint num_counts = 0, num_values = 0;
    int i;

    for (i = 0; i < num_bytes; i++) {
	int n = (dbytes[i] >> 4) + 1;
	int l = (dbytes[i] & 15) + 1;

	if (l > num_counts)
	    num_counts = l;
	num_values += n;
    }
    def->num_counts = num_counts;
    def->num_values = num_values;
}

/* Convert a byte string back to a definition. */
/* The caller must initialize *def, including allocating counts and values. */
void
hc_definition_from_bytes(hc_definition * def, const byte * dbytes)
{
    int v, i;
    ushort counts[max_hc_length + 1];

    /* Make a first pass to set the counts for each code length. */
    memset(counts, 0, sizeof(counts[0]) * (def->num_counts + 1));
    for (i = 0, v = 0; v < def->num_values; i++) {
	int n = (dbytes[i] >> 4) + 1;
	int l = (dbytes[i] & 15) + 1;

	counts[l] += n;
	v += n;
    }

    /* Now fill in the definition. */
    memcpy(def->counts, counts, sizeof(counts[0]) * (def->num_counts + 1));
    for (i = 1, v = 0; i <= def->num_counts; i++) {
	uint prev = counts[i];

	counts[i] = v;
	v += prev;
    }
    for (i = 0, v = 0; v < def->num_values; i++) {
	int n = (dbytes[i] >> 4) + 1;
	int l = (dbytes[i] & 15) + 1;
	int j;

	for (j = 0; j < n; n++)
	    def->values[counts[l]++] = v++;
    }
}

/* ------ Definition -> table procedures ------ */

/* Generate the encoding table from the definition. */
/* The size of the encode array is def->num_values. */
void
hc_make_encoding(hce_code * encode, const hc_definition * def)
{
    uint next = 0;
    const ushort *pvalue = def->values;
    uint i, k;

    for (i = 1; i <= def->num_counts; i++) {
	for (k = 0; k < def->counts[i]; k++, pvalue++, next++) {
	    hce_code *pce = encode + *pvalue;

	    pce->code = next;
	    pce->code_length = i;
	}
	next <<= 1;
    }
}

/* We decode in two steps, first indexing into a table with */
/* a fixed number of bits from the source, and then indexing into */
/* an auxiliary table if necessary.  (See shc.h for details.) */

/* Calculate the size of the decoding table. */
uint
hc_sizeof_decoding(const hc_definition * def, int initial_bits)
{
    uint size = 1 << initial_bits;
    uint carry = 0, mask = (uint) ~ 1;
    uint i;

    for (i = initial_bits + 1; i <= def->num_counts;
	 i++, carry <<= 1, mask <<= 1
	) {
	carry += def->counts[i];
	size += carry & mask;
	carry &= ~mask;
    }
    return size;
}

/* Generate the decoding tables. */
void
hc_make_decoding(hcd_code * decode, const hc_definition * def,
		 int initial_bits)
{				/* Make entries for single-dispatch codes. */
    {
	hcd_code *pcd = decode;
	const ushort *pvalue = def->values;
	uint i, k, d;

	for (i = 0; i <= initial_bits; i++) {
	    for (k = 0; k < def->counts[i]; k++, pvalue++) {
		for (d = 1 << (initial_bits - i); d > 0;
		     d--, pcd++
		    )
		    pcd->value = *pvalue,
			pcd->code_length = i;
	    }
	}
    }
    /* Make entries for two-dispatch codes. */
    /* By working backward, we can do this more easily */
    /* in a single pass. */
    {
	uint dsize = hc_sizeof_decoding(def, initial_bits);
	hcd_code *pcd = decode + (1 << initial_bits);
	hcd_code *pcd2 = decode + dsize;
	const ushort *pvalue = def->values + def->num_values;
	uint entries_left = 0, slots_left = 0, mult_shift = 0;
	uint i = def->num_counts + 1, j;

	for (;;) {
	    if (slots_left == 0) {
		if (entries_left != 0) {
		    slots_left = 1 << (i - initial_bits);
		    mult_shift = 0;
		    continue;
		}
		if (--i <= initial_bits)
		    break;
		entries_left = def->counts[i];
		continue;
	    }
	    if (entries_left == 0) {
		entries_left = def->counts[--i];
		mult_shift++;
		continue;
	    }
	    --entries_left, --pvalue;
	    for (j = 1 << mult_shift; j > 0; j--) {
		--pcd2;
		pcd2->value = *pvalue;
		pcd2->code_length = i - initial_bits;
	    }
	    if ((slots_left -= 1 << mult_shift) == 0) {
		--pcd;
		pcd->value = pcd2 - decode;
		pcd->code_length = i + mult_shift;
	    }
	}
    }
}
