/* Copyright (C) 1989, 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gximage.h,v 1.2 2000/03/08 23:15:01 mike Exp $ */
/* Requires gxcpath.h, gxdevmem.h, gxdcolor.h, gzpath.h */

#ifndef gximage_INCLUDED
#  define gximage_INCLUDED

#include "gsiparam.h"
#include "gxcspace.h"
#include "strimpl.h"		/* for siscale.h */
#include "siscale.h"
#include "gxdda.h"
#include "gxiparam.h"
#include "gxsample.h"

/* Define the abstract type for the image enumerator state. */
typedef struct gx_image_enum_s gx_image_enum;

/*
 * Incoming samples may go through two different transformations:
 *
 *      - For N-bit input samples with N <= 8, N-to-8-bit expansion
 *      may involve a lookup map.  Currently this map is either an
 *      identity function or a subtraction from 1 (inversion).
 *
 *      - The 8-bit or frac expanded sample may undergo decoding (a linear
 *      transformation) before being handed off to the color mapping
 *      machinery.
 *
 * If the decoding function's range is [0..1], we fold it into the
 * expansion lookup; otherwise we must compute it separately.
 * For speed, we distinguish 3 different cases of the decoding step:
 */
typedef enum {
    sd_none,			/* decoded during expansion */
    sd_lookup,			/* use lookup_decode table */
    sd_compute			/* compute using base and factor */
} sample_decoding;
typedef struct sample_map_s {

    sample_lookup_t table;

    /* If an 8-bit fraction doesn't represent the decoded value */
    /* accurately enough, but the samples have 4 bits or fewer, */
    /* we precompute the decoded values into a table. */
    /* Different entries are used depending on bits/sample: */
    /*      1,8,12 bits/sample: 0,15        */
    /*      2 bits/sample: 0,5,10,15        */
    /*      4 bits/sample: all      */

    float decode_lookup[16];
#define decode_base decode_lookup[0]
#define decode_max decode_lookup[15]

    /* In the worst case, we have to do the decoding on the fly. */
    /* The value is base + sample * factor, where the sample is */
    /* an 8-bit (unsigned) integer or a frac. */

    double decode_factor;

    sample_decoding decoding;

} sample_map;

/* Decode an 8-bit sample into a floating point color component. */
/* penum points to the gx_image_enum structure. */
#define decode_sample(sample_value, cc, i)\
  switch ( penum->map[i].decoding )\
  {\
  case sd_none:\
    cc.paint.values[i] = (sample_value) * (1.0 / 255.0);  /* faster than / */\
    break;\
  case sd_lookup:	/* <= 4 significant bits */\
    cc.paint.values[i] =\
      penum->map[i].decode_lookup[(sample_value) >> 4];\
    break;\
  case sd_compute:\
    cc.paint.values[i] =\
      penum->map[i].decode_base + (sample_value) * penum->map[i].decode_factor;\
  }

/* Decode a frac value similarly. */
#define decode_frac(frac_value, cc, i)\
  cc.paint.values[i] =\
    penum->map[i].decode_base + (frac_value) * penum->map[i].decode_factor

/*
 * Declare the variable that holds the 12-bit unpacking procedure
 * if 12-bit samples are supported.
 */
extern sample_unpack_proc((*sample_unpack_12_proc));

/*
 * Define the interface for routines used to render a (source) scan line.
 * If the buffer is the original client's input data, it may be unaligned;
 * otherwise, it will always be aligned.
 *
 * The image_render procedures work on fully expanded, complete rows.  These
 * take a height argument, which is an integer >= 0; they return a negative
 * code, or the number of rows actually processed (which may be less than
 * the height).  height = 0 is a special call to indicate that there is no
 * more input data; this is necessary because the last scan lines of the
 * source data may not produce any output.
 */
#define irender_proc(proc)\
  int proc(P6(gx_image_enum *penum, const byte *buffer, int data_x,\
	      uint w, int h, gx_device *dev))
typedef irender_proc((*irender_proc_t));

/*
 * Define 'strategy' procedures for selecting imaging methods.  Strategies
 * are called in the order in which they are declared below, so each one may
 * assume that all the previous ones failed.  If a strategy succeeds, it may
 * update the enumerator structure as well as returning the rendering
 * procedure.
 *
 * Note that strategies are defined by procedure members of a structure, so
 * that they may be omitted from configurations where they are not desired.
 */
#define image_strategy_proc(proc)\
  irender_proc_t proc(P1(gx_image_enum *penum))
typedef image_strategy_proc((*image_strategy_proc_t));
typedef struct gx_image_strategies_s {
    image_strategy_proc_t interpolate;
    image_strategy_proc_t simple;
    image_strategy_proc_t fracs;
    image_strategy_proc_t mono;
    image_strategy_proc_t color;
} gx_image_strategies_t;
extern gx_image_strategies_t image_strategies;

/* Define the distinct postures of an image. */
/* Each posture includes its reflected variant. */
typedef enum {
    image_portrait = 0,		/* 0 or 180 degrees */
    image_landscape,		/* 90 or 270 degrees */
    image_skewed		/* any other transformation */
} image_posture;

/* Define an entry in the image color table.  For single-source-plane */
/* images, the table index is the sample value, and the key is not used; */
/* for multiple-plane (color) images, the table index is a hash of the key, */
/* which is the concatenation of the source pixel components. */
/* "Clue" = Color LookUp Entry (by analogy with CLUT). */
typedef struct gx_image_clue_s {
    gx_device_color dev_color;
    bits32 key;
} gx_image_clue;

/* Main state structure */

#ifndef gx_device_rop_texture_DEFINED
#  define gx_device_rop_texture_DEFINED
typedef struct gx_device_rop_texture_s gx_device_rop_texture;

#endif

struct gx_image_enum_s {
    gx_image_enum_common;
    /* We really want the map structure to be long-aligned, */
    /* so we choose shorter types for some flags. */
    /* Following are set at structure initialization */
    byte bps;			/* bits per sample: 1, 2, 4, 8, 12 */
    byte unpack_bps;		/* bps for computing unpack proc, */
    /* set to 8 if no unpacking */
    byte log2_xbytes;		/* log2(bytes per expanded sample): */
    /* 0 if bps <= 8, log2(sizeof(frac)) */
    /* if bps > 8 */
    byte spp;			/* samples per pixel: 1, 3, or 4 */
    /* (1, 2, 3, 4, or 5 if alpha is allowed) */
    gs_image_alpha_t alpha;	/* Alpha from image structure */
    /*byte num_planes; *//* spp if colors are separated, */
    /* 1 otherwise (in common part) */
    byte spread;		/* num_planes << log2_xbytes */
    byte masked;		/* 0 = [color]image, 1 = imagemask */
    byte interpolate;		/* true if Interpolate requested */
    gs_matrix matrix;		/* image space -> device space */
    struct r_ {
	int x, y, w, h;		/* subrectangle being rendered */
    } rect;
    gs_fixed_point x_extent, y_extent;	/* extent of one row of rect */
                   sample_unpack_proc((*unpack));
                   irender_proc((*render));
    const gs_imager_state *pis;
    const gs_color_space *pcs;	/* color space of image */
    gs_memory_t *memory;
    byte *buffer;		/* for expanding samples to a */
    /* byte or frac */
    uint buffer_size;
    byte *line;			/* buffer for an output scan line */
    uint line_size;
    uint line_width;		/* width of line in device pixels */
    image_posture posture;
    byte use_rop;		/* true if CombineWithColor requested */
    byte clip_image;		/* mask, see below */
    /* Either we are clipping to a rectangle, in which case */
    /* the individual x/y flags may be set, or we are clipping */
    /* to a general region, in which case only clip_region */
    /* is set. */
#define image_clip_xmin 1
#define image_clip_xmax 2
#define image_clip_ymin 4
#define image_clip_ymax 8
#define image_clip_region 0x10
    byte slow_loop;		/* true if must use slower loop */
    /* (if needed) */
    byte device_color;		/* true if device color space and */
    /* standard decoding */
    gs_fixed_rect clip_outer;	/* outer box of clip path */
    gs_fixed_rect clip_inner;	/* inner box of clip path */
    gs_logical_operation_t log_op;	/* logical operation */
    fixed adjust;		/* adjustment when rendering */
    /* characters */
    fixed dxx, dxy;		/* fixed versions of matrix */
    /* components (as needed) */
    gx_device_clip *clip_dev;	/* clipping device (if needed) */
    gx_device_rop_texture *rop_dev;	/* RasterOp device (if needed) */
    stream_IScale_state *scaler;	/* scale state for */
    /* Interpolate (if needed) */
    /* Following are updated dynamically */
    int y;			/* next source y */
    gs_fixed_point cur, prev;	/* device x, y of current & */
    /* previous row */
    struct dd_ {
	gx_dda_fixed_point row;	/* DDA for row origin, has been */
	/* advanced when render proc called */
	gx_dda_fixed_point pixel0;	/* DDA for first pixel of row */
    } dda;
    int line_xy;		/* x or y value at start of buffered line */
    int xi_next;		/* expected xci of next row */
    /* (landscape only) */
    gs_int_point xyi;		/* integer origin of row */
    /* (Interpolate only) */
    int yci, hci;		/* integer y & h of row (portrait) */
    int xci, wci;		/* integer x & w of row (landscape) */
    /* The maps are set at initialization.  We put them here */
    /* so that the scalars will have smaller offsets. */
    sample_map map[5];		/* 4 colors + alpha */
    /* Entries 0 and 255 of the following are set at initialization */
    /* for monochrome images; other entries are updated dynamically. */
    gx_image_clue clues[256];
#define icolor0 clues[0].dev_color
#define icolor1 clues[255].dev_color
};

/* Enumerate the pointers in an image enumerator. */
#define gx_image_enum_do_ptrs(m)\
  m(0,pis) m(1,pcs) m(2,dev) m(3,buffer) m(4,line)\
  m(5,clip_dev) m(6,rop_dev) m(7,scaler)
#define gx_image_enum_num_ptrs 8
#define private_st_gx_image_enum() /* in gsimage.c */\
  gs_private_st_composite(st_gx_image_enum, gx_image_enum, "gx_image_enum",\
    image_enum_enum_ptrs, image_enum_reloc_ptrs)

/* Compare two device colors for equality. */
/* We can special-case this for speed later if we care. */
#define dev_color_eq(devc1, devc2)\
  gx_device_color_equal(&(devc1), &(devc2))

#endif /* gximage_INCLUDED */
