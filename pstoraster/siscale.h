/* Copyright (C) 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* siscale.h */
/* Image scaling filter state definition */
/* Requires strimpl.h */
#include "gconfigv.h"

/* Define whether to accumulate pixels in fixed or floating point. */
#if USE_FPU <= 0

	/* Accumulate pixels in fixed point. */

typedef int PixelWeight;
#  if arch_ints_are_short
typedef long AccumTmp;
#  else
typedef int AccumTmp;
#  endif
#define num_weight_bits\
  ((sizeof(AccumTmp) - maxSizeofPixel) * 8 - (log2_max_support + 1))
#define scale_PixelWeight(factor) ((int)((factor) * (1 << num_weight_bits)))
#define unscale_AccumTmp(atemp) arith_rshift(atemp, num_weight_bits)

#else		/* USE_FPU > 0 */

	/* Accumulate pixels in floating point. */

typedef float PixelWeight;
typedef double AccumTmp;
#define scale_PixelWeight(factor) (factor)
#define unscale_AccumTmp(atemp) ((int)(atemp))

#endif		/* USE_FPU */

/* Input values */
/*typedef byte PixelIn;*/		/* see sizeofPixelIn below */
/*#define MaxValueIn 255*/		/* see MaxValueIn below */

/* Temporary intermediate values */
typedef byte PixelTmp;
typedef int PixelTmp2;			/* extra width for clamping sum */
#define minPixelTmp 0
#define maxPixelTmp 255
#define unitPixelTmp 255

/* Max of all pixel sizes */
#define maxSizeofPixel 2

/* Output values */
/*typedef byte PixelOut;*/		/* see sizeofPixelOut below */
/*#define MaxValueOut 255*/		/* see MaxValueOut below */

/*
 * The 'support' S of the digital filter is the value such that the filter
 * is guaranteed to be zero for all arguments outside the range [-S..S].
 * We artificially limit the support so that we can put an upper bound
 * on the time required to compute an output value and on the amount of
 * storage required for the X-filtered input data; this also allows us
 * to use pre-scaled fixed-point values for the weights if we wish.
 *
 * 8x8 pixels should be enough for any reasonable application....
 */
#define log2_max_support 3
#define max_support (1 << log2_max_support)

/* Auxiliary structures. */

typedef struct {
	PixelWeight	weight;		/* float or scaled fraction */
} CONTRIB;

typedef struct {
	int	index;		/* index of first element in list of */
				/* contributors */
	int	n;		/* number of contributors */
				/* (not multiplied by stride) */
	int	first_pixel;	/* offset of first value in source data */
} CLIST;

/* ImageScaleEncode / ImageScaleDecode */
typedef struct stream_IScale_state_s {
	stream_state_common;
		/* The client sets the following before initialization. */
	int Colors;			/* any number >= 1 */
	int BitsPerComponentIn;		/* bits per input value, 8 or 16 */
	uint MaxValueIn;		/* max value of input component */
	int WidthIn, HeightIn;
	int BitsPerComponentOut;	/* bits per output value, 8 or 16 */
	uint MaxValueOut;		/* max value of output component */
	int WidthOut, HeightOut;
		/* The init procedure sets the following. */
	int sizeofPixelIn;	/* bytes per input value, 1 or 2 */
	int sizeofPixelOut;	/* bytes per output value, 1 or 2 */
	double xscale, yscale;
	void /*PixelIn*/ *src;
	void /*PixelOut*/ *dst;
	PixelTmp *tmp;
	CLIST *contrib;
	CONTRIB *items;
		/* The following are updated dynamically. */
	int src_y;
	uint src_offset, src_size;
	int dst_y;
	uint dst_offset, dst_size;
	CLIST dst_next_list;		/* for next output value */
	int dst_last_index;		/* highest index used in list */
	CONTRIB dst_items[max_support];	/* ditto */
} stream_IScale_state;
extern_st(st_IScale_state);	/* so clients can allocate */
#define public_st_IScale_state()	/* in siscale.c */\
  gs_public_st_ptrs5(st_IScale_state, stream_IScale_state,\
    "ImageScaleEncode/Decode state",\
    iscale_state_enum_ptrs, iscale_state_reloc_ptrs,\
    dst, src, tmp, contrib, items)
extern const stream_template s_IScale_template;
