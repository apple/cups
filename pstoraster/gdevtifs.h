/* Copyright (C) 1994, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gdevtifs.h */
/* Definitions for writing TIFF file formats. */

#ifndef gdevtifs_INCLUDED
#  define gdevtifs_INCLUDED

/* ================ TIFF specification ================ */

/* Based on TIFF specification version 6.0 obtained from */
/* sgi.com:graphics/tiff/TIFF6.ps.Z. */

/*
 * The sizes of TIFF data types are system-independent.  Therefore,
 * we cannot use short, long, etc., but must use types of known sizes.
 */
#if arch_sizeof_short == 2
typedef short TIFF_short;			/* no plausible alternative */
typedef unsigned short TIFF_ushort;
#endif
#if arch_sizeof_int == 4
typedef int TIFF_long;
typedef unsigned int TIFF_ulong;
#else
# if arch_sizeof_long == 4
typedef long TIFF_long;
typedef unsigned long TIFF_ulong;
# endif
#endif

/*
 * Define the TIFF file header.
 */
typedef	struct TIFF_header_s {
	TIFF_ushort magic;		/* magic number (defines byte order) */
	TIFF_ushort version;		/* TIFF version number */
	TIFF_ulong diroff;		/* byte offset to first directory */
} TIFF_header;

#define	TIFF_magic_big_endian		0x4d4d		/* 'MM' */
#define	TIFF_magic_little_endian	0x4949		/* 'II' */

#define	TIFF_version_value	42

/*
 * Define an individual entry in a TIFF directory.  Within a directory,
 * the entries must be sorted by increasing tag value.
 *
 * The value field contains either the offset of the field data in the file,
 * or, if the value fits in 32 bits, the value itself, left-justified.
 * Field data may appear anywhere in the file, so long as each data block is
 * aligned on a 32-bit boundary and is disjoint from all other data blocks.
 */
typedef	struct TIFF_dir_entry_s {
	TIFF_ushort tag;	/* TIFF_tag */
	TIFF_ushort type;	/* TIFF_data_type */
	TIFF_ulong count;	/* number of items (spec calls this 'length') */
	TIFF_ulong value;	/* byte offset to field data, */
				/* or actual value if <=4 bytes */
} TIFF_dir_entry;

/*
 * Define the tag data type values.
 */
typedef	enum {
	TIFF_BYTE	= 1,	/* 8-bit unsigned integer */
	TIFF_ASCII	= 2,	/* 8-bit bytes with last byte null */
	TIFF_SHORT	= 3,	/* 16-bit unsigned integer */
	TIFF_LONG	= 4,	/* 32-bit unsigned integer */
	TIFF_RATIONAL	= 5,	/* 64-bit unsigned fraction */
				/* (ratio of two 32-bit unsigned integers) */
	TIFF_SBYTE	= 6,	/* 8-bit signed integer */
	TIFF_UNDEFINED	= 7,	/* 8-bit untyped data */
	TIFF_SSHORT	= 8,	/* 16-bit signed integer */
	TIFF_SLONG	= 9,	/* 32-bit signed integer */
	TIFF_SRATIONAL	= 10,	/* 64-bit signed fraction */
				/* (ratio of two 32-bit signed integers) */
	TIFF_FLOAT	= 11,	/* 32-bit IEEE floating point */
	TIFF_DOUBLE	= 12,	/* 64-bit IEEE floating point */
		/* A flag to indicate the value is indirect. */
		/* This is only used internally; it is not part of the */
		/* TIFF specification (although it should be!). */
	TIFF_INDIRECT = 128
} TIFF_data_type;

/*
 * Define the tag values we need.  Note that this is only a very small subset
 * of all the values defined in the TIFF specification; we will add more
 * as the need arises.
 */
typedef enum {
	TIFFTAG_SubFileType =		254,	/* subfile data descriptor */
#define	    SubFileType_reduced_image	0x1	/* reduced resolution version */
#define	    SubFileType_page		0x2	/* one page of many */
#define	    SubFileType_mask		0x4	/* transparency mask */
	TIFFTAG_ImageWidth =		256,	/* image width in pixels */
	TIFFTAG_ImageLength =		257,	/* image height in pixels */
	TIFFTAG_BitsPerSample =		258,	/* bits per channel (sample) */
	TIFFTAG_Compression =		259,	/* data compression technique */
#define	    Compression_none		1	/* dump mode */
#define	    Compression_CCITT_RLE	2	/* CCITT modified Huffman RLE */
#define	    Compression_CCITT_T4	3	/* CCITT T.4 fax encoding */
#define	    Compression_CCITT_T6	4	/* CCITT T.6 fax encoding */
#define	    Compression_LZW		5	/* Lempel-Ziv  & Welch */
#define	    Compression_JPEG		6	/* !JPEG compression */
#define	    Compression_NeXT		32766	/* NeXT 2-bit RLE */
#define	    Compression_CCITT_RLEW	32771	/* #1 w/ word alignment */
#define	    Compression_PackBits	32773	/* Macintosh RLE */
#define	    Compression_Thunderscan	32809	/* ThunderScan RLE */
	TIFFTAG_Photometric =		262,	/* photometric interpretation */
#define	    Photometric_min_is_white	0	/* min value is white */
#define	    Photometric_min_is_black	1	/* min value is black */
#define	    Photometric_RGB		2	/* RGB color model */
#define	    Photometric_palette		3	/* color map indexed */
#define	    Photometric_mask		4	/* $holdout mask */
#define	    Photometric_separated	5	/* !color separations */
#define	    Photometric_YCbCr		6	/* !CCIR 601 */
#define	    Photometric_CIE_Lab		8	/* !1976 CIE L*a*b* */
	TIFFTAG_FillOrder =		266,	/* data order within a byte */
#define	    FillOrder_MSB2LSB		1	/* most significant -> least */
#define	    FillOrder_LSB2MSB		2	/* least significant -> most */
	TIFFTAG_StripOffsets =		273,	/* offsets to data strips */
	TIFFTAG_Orientation =		274,	/* +image Orientation */
#define	    Orientation_top_left	1	/* row 0 top, col 0 lhs */
#define	    Orientation_top_right	2	/* row 0 top, col 0 rhs */
#define	    Orientation_bot_right	3	/* row 0 bottom, col 0 rhs */
#define	    Orientation_bot_left	4	/* row 0 bottom, col 0 lhs */
#define	    Orientation_left_top	5	/* row 0 lhs, col 0 top */
#define	    Orientation_right_top	6	/* row 0 rhs, col 0 top */
#define	    Orientation_right_bot	7	/* row 0 rhs, col 0 bottom */
#define	    Orientation_left_bot	8	/* row 0 lhs, col 0 bottom */
	TIFFTAG_SamplesPerPixel =	277,	/* samples per pixel */
	TIFFTAG_RowsPerStrip =		278,	/* rows per strip of data */
	TIFFTAG_StripByteCounts =	279,	/* bytes counts for strips */
	TIFFTAG_XResolution =		282,	/* pixels/resolution in x */
	TIFFTAG_YResolution =		283,	/* pixels/resolution in y */
	TIFFTAG_PlanarConfig =		284,	/* storage organization */
#define	    PlanarConfig_contig		1	/* single image plane */
#define	    PlanarConfig_separate	2	/* separate planes of data */
	TIFFTAG_T4Options =		292,	/* 32 flag bits */
#define	    T4Options_2D_encoding	0x1	/* 2-dimensional coding */
#define	    T4Options_uncompressed	0x2	/* data not compressed */
#define	    T4Options_fill_bits		0x4	/* fill to byte boundary */
	TIFFTAG_T6Options =		293,	/* 32 flag bits */
#define	    T6Options_uncompressed	0x2	/* data not compressed */
	TIFFTAG_ResolutionUnit =	296,	/* units of resolutions */
#define	    ResolutionUnit_none		1	/* no meaningful units */
#define	    ResolutionUnit_inch		2	/* english */
#define	    ResolutionUnit_centimeter	3	/* metric */
	TIFFTAG_PageNumber =		297,	/* page number if multi-page */
	TIFFTAG_Software =		305,	/* software name & release */
	TIFFTAG_DateTime =		306,	/* creation date and time */
	TIFFTAG_CleanFaxData =		327	/* regenerated line info */
#define	    CleanFaxData_clean		0	/* no errors detected */
#define	    CleanFaxData_regenerated	1	/* receiver regenerated lines */
#define	    CleanFaxData_unclean	2	/* uncorrected errors exist */
} TIFF_tag;

/* ================ Implementation ================ */

/*
 * Define the added driver state for TIFF writing.
 */
typedef struct gdev_tiff_state_s {
	long prev_dir;		/* file offset of previous directory offset */
	long dir_off;		/* file offset of next write */
	int ntags;		/* # of tags in directory */
	int vsize;		/* size of values following tags */
		/* Record offsets of values */
	int offset_StripByteCounts;
} gdev_tiff_state;

/*
 * Begin writing a TIFF page.  This procedure supplies a standard set of
 * tags; the client can provide additional tags (pre-sorted) and
 * indirect values.
 */
int gdev_tiff_begin_page(P7(gx_device_printer *pdev, gdev_tiff_state *tifs,
			    FILE *fp,
			    const TIFF_dir_entry *entries, int entry_count,
			    const byte *values, int value_size));

/*
 * Finish writing a TIFF page.  All data written between begin and end
 * is considered to be a single strip.
 */
int gdev_tiff_end_page(P2(gdev_tiff_state *tifs, FILE *fp));

#endif				/* gdevtifs_INCLUDED */
