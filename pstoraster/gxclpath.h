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

/* gxclpath.h */
/* Internal definitions for higher-level command list facilities. */
/* Extends (requires) gxcldev.h */
#include "gxfixed.h"			/* for gzpath.h */

/* Extend the command set.  See gxcldev.h for more information. */
typedef enum {
	cmd_op_misc2 = 0xd0,		/* (see below) */
	  cmd_opv_set_flatness = 0xd0,	/* flatness(float) */
	  cmd_opv_set_fill_adjust = 0xd1,   /* adjust_x/y(fixed) */
	  cmd_opv_set_ctm = 0xd2,	/* (0=0,0, 1=V,V, 2=V,-V, 3=U,V)x */
					/* (xx+yy,xy+yx)(0=0, 1=V)x(tx,ty), */
					/* 0..5 x coeff(float) */
	  cmd_opv_set_line_width = 0xd3,   /* width(float) */
	  cmd_opv_set_misc = 0xd4,	/* overprint(1)stroke_adj(1) */
					/* cap(3)join(3) */
	  cmd_opv_set_miter_limit = 0xd5,   /* miter limit(float) */
	  cmd_opv_set_dash = 0xd6,	/* n, offset(float), n x (float) */
	  cmd_opv_enable_clip = 0xd7,	/* (nothing) */
	  cmd_opv_disable_clip = 0xd8,	/* (nothing) */
	  cmd_opv_begin_clip = 0xd9,	/* (nothing) */
	  cmd_opv_end_clip = 0xda,	/* outside? */
	  cmd_opv_set_color_space = 0xdb,  /* base(4)Indexed?(1)0(3) */
					/* [, hival#] */
	  cmd_opv_set_color_mapping = 0xdc,  /******NYI******/
	  cmd_opv_begin_image = 0xdd,	/* BPCi(3)(0=mask)Interpolate(1) */
					/* Matrix?(1)Decode?(1) */
					/* adjust/CombineWithColor(1)0(1), */
					/* shape, width#, height#, */
					/* [, aabbcd00, 0..6 x coeff(float)] */
					/* [, (0=default, 1=swapped default, */
					/* 2=0,V, 3=U,V)x4, */
					/* 0..8 x decode(float)] */
	  cmd_opv_image_data = 0xde,	/* nbytes# (premature EOD if 0), */
					/* (0=same, 1=+1, 2=+d, 3=-d)x */
					/* (x,y,w,h), [dx#,] [dy#,] [dw#,] */
					/* [dh#,] <data> */
	cmd_op_segment = 0xe0,		/* (see below) */
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
	  cmd_opv_closepath = 0xec,	/* (nothing) */
	cmd_op_path = 0xf0,		/* (see below) */
	  cmd_opv_fill = 0xf0,
	  cmd_opv_eofill = 0xf1,
	  cmd_opv_stroke = 0xf2,
	  cmd_opv_htfill = 0xf3,
	  cmd_opv_hteofill = 0xf4,
	  cmd_opv_htstroke = 0xf5
} gx_cmd_xop;

static const byte clist_segment_op_num_operands[] =
 { 2, 2, 1, 1, 6, 4, 4, 4, 4, 4, 6, 6, 0
 };

#define cmd_misc2_op_name_strings\
  "set_flatness", "set_fill_adjust", "set_ctm", "set_line_width",\
  "set_misc", "set_miter_limit", "set_dash", "enable_clip",\
  "disable_clip", "begin_clip", "end_clip", "set_color_space",\
  "set_color_mapping", "begin_image", "image_data", "?df?"

#define cmd_segment_op_name_strings\
  "rmoveto", "rlineto", "hlineto", "vlineto",\
  "rrcurveto", "hvcurveto", "vhcurveto", "nrcurveto",\
  "rncurveto", "rmlineto", "rm2lineto", "rm3lineto",\
  "closepath", "?ed?", "?ee?", "?ef?"

#define cmd_path_op_name_strings\
  "fill", "eofill", "stroke", "htfill",\
  "hteofill", "htstroke", "?f6?", "?f7?",\
  "?f8?", "?f9?", "?fa?", "?fb?",\
  "?fc?", "?fd?", "?fe?", "?ff?"

/*
 * We represent path coordinates as 'fixed' values in a variable-length,
 * relative form (s/t = sign, x/y = integer, f/g = fraction):
 *	00sxxxxx xfffffff ffffftyy yyyygggg gggggggg
 *	01sxxxxx xxxxffff ffffffff
 *	10sxxxxx xxxxxxxx xxxxffff ffffffff
 *	110sxxxx xxxxxxff
 *	111----- (a full-size `fixed' value)
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
int cmd_put_drawing_color(P3(gx_device_clist_writer *cldev,
			     gx_clist_state *pcls,
			     const gx_drawing_color *pdcolor));

/* Clear (a) specific 'known' flag(s) for all bands. */
/* We must do this whenever the value of a 'known' parameter changes. */
void cmd_clear_known(P2(gx_device_clist_writer *cldev, uint known));

/* Check whether we need to change the clipping path in the device. */
bool cmd_check_clip_path(P2(gx_device_clist_writer *cldev,
			    const gx_clip_path *pcpath));

/* Construct the parameters for writing out a matrix. */
/* We need a buffer of at least 1 + 6 * sizeof(float) bytes. */
byte *cmd_for_matrix(P2(byte *cbuf, const gs_matrix *pmat));

/* Write out values of any unknown parameters. */
#define cmd_do_write_unknown(cldev, pcls, must_know)\
  if ( ~(pcls)->known & (must_know) )\
    { int code = cmd_write_unknown(cldev, pcls, must_know);\
      if ( code < 0 ) return code;\
    }
int cmd_write_unknown(P3(gx_device_clist_writer *cldev, gx_clist_state *pcls,
			 uint must_know));
