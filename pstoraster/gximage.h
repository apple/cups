/* Copyright (C) 1989, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gximage.h */
/* Internal definitions for image rendering */
/* Requires gxcpath.h, gxdevmem.h, gxdcolor.h, gzpath.h */
#include "gsiparam.h"
#include "gxcspace.h"
#include "strimpl.h"		/* for siscale.h */
#include "siscale.h"
#include "gxdda.h"

/* Interface for routine used to unpack and shuffle incoming samples. */
/* The Unix C compiler can't handle typedefs for procedure */
/* (as opposed to pointer-to-procedure) types, */
/* so we have to do it with macros instead. */
#define iunpack_proc(proc)\
  void proc(P6(byte *bptr, const byte *data, uint dsize,\
	       const sample_map *pmap, int spread, uint inpos))
/* Interface for routine used to render a (source) scan line. */
#define irender_proc(proc)\
  int proc(P5(gx_image_enum *penum, byte *buffer, uint w, int h,\
	      gx_device *dev))

/*
 * Incoming samples may go through two different transformations:
 *
 *	- For N-bit input samples with N <= 8, N-to-8-bit expansion
 *	may involve a lookup map.  Currently this map is either an
 *	identity function or a subtraction from 1 (inversion).
 *
 *	- The 8-bit or frac expanded sample may undergo decoding (a linear
 *	transformation) before being handed off to the color mapping
 *	machinery.
 *
 * If the decoding function's range is [0..1], we fold it into the
 * expansion lookup; otherwise we must compute it separately.
 * For speed, we distinguish 3 different cases of the decoding step:
 */
typedef enum {
	sd_none,		/* decoded during expansion */
	sd_lookup,		/* use lookup_decode table */
	sd_compute		/* compute using base and factor */
} sample_decoding;
typedef struct sample_map_s {

	/* The following union implements the expansion of sample */
	/* values from N bits to 8, and a possible inversion. */

	union {

		bits32 lookup4x1to32[16]; /* 1 bit/sample, not spreading */
		bits16 lookup2x2to16[16]; /* 2 bits/sample, not spreading */
		byte lookup8[256];	/* 1 bit/sample, spreading [2] */
					/* 2 bits/sample, spreading [4] */
					/* 4 bits/sample [16] */
					/* 8 bits/sample [256] */

	} table;

	/* If an 8-bit fraction doesn't represent the decoded value */
	/* accurately enough, but the samples have 4 bits or fewer, */
	/* we precompute the decoded values into a table. */
	/* Different entries are used depending on bits/sample: */
	/*	1,8,12 bits/sample: 0,15	*/
	/*	2 bits/sample: 0,5,10,15	*/
	/*	4 bits/sample: all	*/

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

/* Define the distinct postures of an image. */
/* Each posture includes its reflected variant. */
typedef enum {
  image_portrait = 0,			/* 0 or 180 degrees */
  image_landscape,			/* 90 or 270 degrees */
  image_skewed				/* any other transformation */
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

typedef struct gx_image_enum_s gx_image_enum;
struct gx_image_enum_s {
	/* We really want the map structure to be long-aligned, */
	/* so we choose shorter types for some flags. */
		/* Following are set at structure initialization */
	int width;
	int height;
	byte bps;			/* bits per sample: 1, 2, 4, 8, 12 */
	byte unpack_bps;		/* bps for computing unpack proc, */
					/* set to 8 if no unpacking */
	byte log2_xbytes;		/* log2(bytes per expanded sample): */
					/* 0 if bps <= 8, log2(sizeof(frac)) */
					/* if bps > 8 */
	byte spp;			/* samples per pixel: 1, 3, or 4 */
	byte num_planes;		/* spp if colors are separated, */
					/* 1 otherwise */
	byte spread;			/* num_planes << log2_xbytes */
	byte masked;			/* 0 = [color]image, 1 = imagemask */
	byte interpolate;		/* true if Interpolate requested */
	gs_matrix matrix;		/* image space -> device space */
	fixed mtx, mty;			/* device coords of image origin */
	gs_fixed_point row_extent;	/* total change in X/Y over one row, */
					/* for row DDA */
	iunpack_proc((*unpack));
	irender_proc((*render));
	gs_state *pgs;			/****** BEING PHASED OUT ******/
	const gs_imager_state *pis;
	const gs_color_space *pcs;	/* color space of image */
	gs_memory_t *memory;
	gx_device *dev;
	byte *buffer;			/* for expanding samples to a */
					/* byte or frac */
	uint buffer_size;
	byte *line;			/* buffer for an output scan line */
	uint line_size;
	uint line_width;		/* width of line in device pixels */
	uint bytes_per_row;		/* # of input bytes per row */
					/* (per plane, if spp == 1 and */
					/* num_planes > 1) */
	image_posture posture;
	byte use_rop;			/* true if CombineWithColor requested */
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
	byte slow_loop;			/* true if !(skewed | */
					/* imagemask with a halftone) */
	byte device_color;		/* true if device color space and */
					/* standard decoding */
	gs_fixed_rect clip_outer;	/* outer box of clip path */
	gs_fixed_rect clip_inner;	/* inner box of clip path */
	gs_logical_operation_t log_op;	/* logical operation */
	fixed adjust;			/* adjustment when rendering */
					/* characters */
	gx_device_clip *clip_dev;	/* clipping device (if needed) */
	gx_device_rop_texture *rop_dev;	/* RasterOp device (if needed) */
	stream_IScale_state *scaler;	/* scale state for */
					/* Interpolate (if needed) */
	/* Following are updated dynamically */
	int x, y;			/* next source x & y */
	uint byte_in_row;		/* current input byte position in row */
	fixed xcur, ycur;		/* device x, y of current row */
	gx_dda_fixed next_x;		/* DDA for xcur, holds next values */
					/* when render proc is called */
	gx_dda_fixed next_y;		/* DDA for ycur ditto */
	int line_xy;			/* x or y value at start of buffered line */
	int yci, hci;			/* integer y & h of row (portrait) */
	int xci, wci;			/* integer x & w of row (landscape) */
	/* The maps are set at initialization.  We put them here */
	/* so that the scalars will have smaller offsets. */
	sample_map map[4];
	/* Entries 0 and 255 of the following are set at initialization */
	/* for monochrome images; other entries are updated dynamically. */
	gx_image_clue clues[256];
#define icolor0 clues[0].dev_color
#define icolor1 clues[255].dev_color
};
/* Enumerate the pointers in an image enumerator. */
#define gx_image_enum_do_ptrs(m)\
  m(0,pgs) m(1,pis) m(2,pcs) m(3,dev) m(4,buffer) m(5,line)\
  m(6,clip_dev) m(7,rop_dev) m(8,scaler)
#define gx_image_enum_num_ptrs 9
#define private_st_gx_image_enum() /* in gsimage.c */\
  gs_private_st_composite(st_gx_image_enum, gx_image_enum, "gx_image_enum",\
    image_enum_enum_ptrs, image_enum_reloc_ptrs)

/* Compare two device colors for equality. */
#define dev_color_eq(devc1, devc2)\
  (gx_dc_is_pure(&(devc1)) ?\
   gx_dc_is_pure(&(devc2)) &&\
   gx_dc_pure_color(&(devc1)) == gx_dc_pure_color(&(devc2)) :\
   gx_dc_is_binary_halftone(&(devc1)) ?\
   gx_dc_is_binary_halftone(&(devc2)) &&\
   gx_dc_binary_color0(&(devc1)) == gx_dc_binary_color0(&(devc2)) &&\
   gx_dc_binary_color1(&(devc1)) == gx_dc_binary_color1(&(devc2)) &&\
   (devc1).colors.binary.b_level == (devc2).colors.binary.b_level :\
   false)
