/* Copyright (C) 1994 Aladdin Enterprises.  All rights reserved.
  
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

/* sdct.h */
/* Definitions for DCT filters for Ghostscript streams */
/* Requires stream.h, strimpl.h, jpeg/jpeglib.h */
#include <setjmp.h>			/* for jmp_buf */

/* ------ DCT filters ------ */

/*
 * Define the stream state.
 * The jpeg_xxx_data structs are allocated in immovable memory
 * to simplify use of the IJG library.
 */
#define jpeg_stream_data_common\
		/* We put a copy of the stream template here, because */\
		/* the minimum buffer sizes depend on the image parameters. */\
	stream_template template;\
	struct jpeg_error_mgr err;\
	jmp_buf exit_jmpbuf;\
		/* The following are documented in Adobe TN 5116. */\
	int Picky;		/* 0 or 1 */\
	int Relax		/* 0 or 1 */
typedef struct jpeg_stream_data_s {
	jpeg_stream_data_common;
} jpeg_stream_data;
typedef struct jpeg_compress_data_s {
	jpeg_stream_data_common;
	/* cinfo must immediately follow the common fields */
	struct jpeg_compress_struct cinfo;
	struct jpeg_destination_mgr destination;
} jpeg_compress_data;
typedef struct jpeg_decompress_data_s {
	jpeg_stream_data_common;
	/* dinfo must immediately follow the common fields, */
	/* so that it has same offset as cinfo. */
	struct jpeg_decompress_struct dinfo;
	struct jpeg_source_mgr source;
	long skip;		/* # of bytes remaining to skip in input */
	bool input_eod;		/* true when no more input data available */
	bool faked_eoi;		/* true when fill_input_buffer inserted EOI */
	byte * scanline_buffer;	/* buffer for oversize scanline, or NULL */
	uint bytes_in_scanline;	/* # of bytes remaining to output from same */
} jpeg_decompress_data;

/* The stream state itself.  This is kept in garbage-collectable memory. */
typedef struct stream_DCT_state_s {
	stream_state_common;
		/* The following are set before initialization. */
		/* Note that most JPEG parameters go straight into */
		/* the IJG data structures, not into this struct. */
	gs_const_string Markers;	/* NULL if no Markers parameter */
	float QFactor;
	int ColorTransform;		/* -1 if not specified */
	bool NoMarker;			/* DCTEncode only */
		/* This is a pointer to immovable storage. */
	union _jd {
		jpeg_stream_data *common;
		jpeg_compress_data *compress;
		jpeg_decompress_data *decompress;
	} data;
		/* DCTEncode sets this before initialization;
		 * DCTDecode cannot set it until the JPEG headers are read.
		 */
	uint scan_line_size;
		/* The following are updated dynamically. */
	int phase;
} stream_DCT_state;
/* The state descriptor is public only to allow us to split up */
/* the encoding and decoding filters. */
extern_st(st_DCT_state);
#define public_st_DCT_state()	/* in sdctc.c */\
  gs_public_st_composite(st_DCT_state, stream_DCT_state,\
    "DCTEncode/Decode state", dct_enum_ptrs, dct_reloc_ptrs)
extern const stream_template s_DCTD_template;
extern const stream_template s_DCTE_template;
