/* Copyright (C) 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gxclpath.h,v 1.3 2001/03/16 20:42:06 mike Exp $ */
/* Extends (requires) gxcldev.h */

#ifndef gxclpath_INCLUDED
#  define gxclpath_INCLUDED

#include "gxfixed.h"		/* for gzpath.h */

/* Define the flags indicating whether a band knows the current values of */
/* various miscellaneous parameters (pcls->known). */
#define flatness_known		(1<<0)
#define fill_adjust_known	(1<<1)
#define ctm_known		(1<<2)
#define line_width_known	(1<<3)
#define miter_limit_known	(1<<4)
#define misc0_known		(1<<5)
#define misc1_known		(1<<6)
#define dash_known		(1<<7)
#define alpha_known		(1<<8)
#define clip_path_known		(1<<9)
#define stroke_all_known	((1<<10)-1)
#define color_space_known	(1<<10)
/*#define all_known             ((1<<11)-1) */

/* Define the drawing color types for distinguishing different */
/* fill/stroke command variations. */
typedef enum {
    cmd_dc_type_pure = 0,
    cmd_dc_type_ht = 1,
    cmd_dc_type_color = 2
} cmd_dc_type;

/* Extend the command set.  See gxcldev.h for more information. */
typedef enum {
    cmd_op_misc2 = 0xd0,	/* (see below) */
    cmd_opv_set_flatness = 0xd0,	/* flatness(float) */
    cmd_opv_set_fill_adjust = 0xd1,	/* adjust_x/y(fixed) */
    cmd_opv_set_ctm = 0xd2,	/* (0=0,0, 1=V,V, 2=V,-V, 3=U,V)x */
				/* (xx+yy,yx+xy)(0=0, 1=V)x(tx,ty), */
				/* 0..5 x coeff(float) */
    cmd_opv_set_line_width = 0xd3,	/* width(float) */
    cmd_opv_set_misc2 = 0xd4,
#define cmd_set_misc2_cap_join (0 << 6)		/* 00: cap(3)join(3) */
#define cmd_set_misc2_ac_op_sa (1 << 6)		/* 01: 0(3)acc.curves(1)overprint(1) */
				/*   stroke_adj(1) */
#define cmd_set_misc2_notes (2 << 6)	/* 10: seg.notes(6) */
#define cmd_set_misc2_alpha (3 << 6)	/* 11: -unused-, alpha  */
    cmd_opv_set_miter_limit = 0xd5,	/* miter limit(float) */
    cmd_opv_set_dash = 0xd6,	/* adapt(1)abs.dot(1)n(6), dot */
				/* length(float), offset(float), */
				/* n x (float) */
    cmd_opv_enable_clip = 0xd7,	/* (nothing) */
    cmd_opv_disable_clip = 0xd8,	/* (nothing) */
    cmd_opv_begin_clip = 0xd9,	/* (nothing) */
    cmd_opv_end_clip = 0xda,	/* outside? */
    cmd_opv_set_color_space = 0xdb,	/* base(4)Indexed?(2)0(3) */
				/* [, hival#, table|map] */
    cmd_opv_begin_image = 0xdc,	/* BPCi(3)(0=mask) */
				/* more params(1) */
				/* Matrix?(1)Decode?(1) */
				/* adjust/CombineWithColor(1) */
				/* rect?(1), */
				/* [format(2)Interpolate(1)Alpha(2) */
				/* 0(3),] width#, height#, */
				/* [, aabbcd00, 0..6 x coeff(float)] */
				/* [, (0=default, 1=swapped default, */
				/* 2=0,V, 3=U,V)x4, */
				/* 0..8 x decode(float)], */
				/* [, x0#, w-x1#, y0#, h-y1#] */
    cmd_opv_image_data = 0xdd,	/* height# (premature EOD if 0), */
				/* raster#, <data> */
    cmd_opv_set_color = 0xde,	/* (0000abcd | */
				/*  0001aaaa abbbbbcc cccddddd), */
				/* (3|4) x level#: colored halftone */
				/* with base colors a,b,c,d */
    cmd_opv_put_params = 0xdf,	/* (nothing) */
    cmd_op_segment = 0xe0,	/* (see below) */
    cmd_opv_rmoveto = 0xe0,	/* dx%, dy% */
    cmd_opv_rlineto = 0xe1,	/* dx%, dy% */
    cmd_opv_hlineto = 0xe2,	/* dx% */
    cmd_opv_vlineto = 0xe3,	/* dy% */
    cmd_opv_rrcurveto = 0xe4,	/* dx1%,dy1%, dx2%,dy2%, dx3%,dy3% */
    cmd_opv_hvcurveto = 0xe5,	/* dx1%, dx2%,dy2%, dy3% */
    cmd_opv_vhcurveto = 0xe6,	/* dy1%, dx2%,dy2%, dx3% */
    cmd_opv_nrcurveto = 0xe7,	/* dx2%,dy2%, dx3%,dy3% */
    cmd_opv_rncurveto = 0xe8,	/* dx1%,dy1%, dx2%,dy2% */
    cmd_opv_rmlineto = 0xe9,	/* dx1%,dy1%, dx2%,dy2% */
    cmd_opv_rm2lineto = 0xea,	/* dx1%,dy1%, dx2%,dy2%, dx3%,dy3% */
    cmd_opv_rm3lineto = 0xeb,	/* dx1%,dy1%, dx2%,dy2%, dx3%,dy3%, */
				/* [-dx2,-dy2 implicit] */
    cmd_opv_vqcurveto = 0xec,	/* dy1%, dx2%[,dy2=dx2 with sign */
				/* of dy1, dx3=dy1 with sign of dx2] */
    cmd_opv_hqcurveto = 0xed,	/* dx1%, [dx2=dy2 with sign */
				/* of dx1,]%dy2, [dy3=dx1 with sign */
				/* of dy2] */
    cmd_opv_closepath = 0xee,	/* (nothing) */
    cmd_op_path = 0xf0,		/* (see below) */
    /* The path drawing commands come in groups: */
    /* each group consists of a base command plus an offset */
    /* which is a cmd_dc_type. */
    cmd_opv_fill = 0xf0,
    cmd_opv_htfill = 0xf1,
    cmd_opv_colorfill = 0xf2,
    cmd_opv_eofill = 0xf3,
    cmd_opv_hteofill = 0xf4,
    cmd_opv_coloreofill = 0xf5,
    cmd_opv_stroke = 0xf6,
    cmd_opv_htstroke = 0xf7,
    cmd_opv_colorstroke = 0xf8
} gx_cmd_xop;

static const byte clist_segment_op_num_operands[] =
{2, 2, 1, 1, 6, 4, 4, 4, 4, 4, 6, 6, 2, 2, 0
};

#define cmd_misc2_op_name_strings\
  "set_flatness", "set_fill_adjust", "set_ctm", "set_line_width",\
  "set_misc2", "set_miter_limit", "set_dash", "enable_clip",\
  "disable_clip", "begin_clip", "end_clip", "set_color_space",\
  "begin_image", "image_data", "set_color", "put_params"

#define cmd_segment_op_name_strings\
  "rmoveto", "rlineto", "hlineto", "vlineto",\
  "rrcurveto", "hvcurveto", "vhcurveto", "nrcurveto",\
  "rncurveto", "rmlineto", "rm2lineto", "rm3lineto",\
  "vqcurveto", "hqcurveto", "closepath", "?ef?"

#define cmd_path_op_name_strings\
  "fill", "htfill", "colorfill", "eofill",\
  "hteofill", "coloreofill", "stroke", "htstroke",\
  "colorstroke", "?f9?", "?fa?", "?fb?",\
  "?fc?", "?fd?", "?fe?", "?ff?"

/*
 * We represent path coordinates as 'fixed' values in a variable-length,
 * relative form (s/t = sign, x/y = integer, f/g = fraction):
 *      00sxxxxx xfffffff ffffftyy yyyygggg gggggggg
 *      01sxxxxx xxxxffff ffffffff
 *      10sxxxxx xxxxxxxx xxxxffff ffffffff
 *      110sxxxx xxxxxxff
 *      111----- (a full-size `fixed' value)
 */
#define is_bits(d, n) !(((d) + ((fixed)1 << ((n) - 1))) & (-(fixed)1 << (n)))

/* ---------------- Driver procedure support ---------------- */

/* The procedures and macros defined here are used when writing */
/* (gxclimag.c, gxclpath.c). */

/* Compare and update members of the imager state. */
#define state_neq(member)\
  cdev->imager_state.member != pis->member
#define state_update(member)\
  cdev->imager_state.member = pis->member

/* ------ Exported by gxclpath.c ------ */

/* Write out the color for filling, stroking, or masking. */
/* Return a cmd_dc_type. */
int cmd_put_drawing_color(P3(gx_device_clist_writer * cldev,
			     gx_clist_state * pcls,
			     const gx_drawing_color * pdcolor));

/* Clear (a) specific 'known' flag(s) for all bands. */
/* We must do this whenever the value of a 'known' parameter changes. */
void cmd_clear_known(P2(gx_device_clist_writer * cldev, uint known));

/* Write out values of any unknown parameters. */
#define cmd_do_write_unknown(cldev, pcls, must_know)\
  ( ~((pcls)->known) & (must_know) ?\
    cmd_write_unknown(cldev, pcls, must_know) : 0 )
int cmd_write_unknown(P3(gx_device_clist_writer * cldev, gx_clist_state * pcls,
			 uint must_know));

/* Check whether we need to change the clipping path in the device. */
bool cmd_check_clip_path(P2(gx_device_clist_writer * cldev,
			    const gx_clip_path * pcpath));

/* Construct the parameters for writing out a matrix. */
/* We need a buffer of at least 1 + 6 * sizeof(float) bytes. */
byte *cmd_for_matrix(P2(byte * cbuf, const gs_matrix * pmat));

#endif /* gxclpath_INCLUDED */
