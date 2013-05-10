/* Copyright (C) 1992, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* zimage2.c */
/* image operator extensions for Level 2 PostScript */
#include "math_.h"
#include "memory_.h"
#include "ghost.h"
#include "errors.h"
#include "oper.h"
#include "gscolor.h"
#include "gscspace.h"
#include "gscolor2.h"
#include "gsmatrix.h"
#include "gsimage.h"
#include "idict.h"
#include "idparam.h"
#include "iimage.h"
#include "ilevel.h"
#include "igstate.h"		/* for igs */

/* Define a structure for acquiring image parameters. */
typedef struct image_params_s {
	gs_image_t image;
	bool MultipleDataSources;
	ref DataSource[4];
	const float *pDecode;
} image_params;

/* Common code for unpacking an image dictionary. */
/* Assume *op is a dictionary. */
private int
image_dict_unpack(os_ptr op, image_params *pip, int max_bits_per_component)
{	int code;
	int num_components;
	int decode_size;
	ref *pds;

	check_dict_read(*op);
	num_components =
	  gs_color_space_num_components(gs_currentcolorspace(igs));
	if ( num_components < 1 )
	  return_error(e_rangecheck);		/* Pattern space not allowed */
	if ( max_bits_per_component == 1 )	/* imagemask */
	  num_components = 1;			/* for Decode */
#define pim (&pip->image)
	if ( (code = dict_int_param(op, "ImageType", 1, 1, 1,
				    &code)) < 0 ||
	     (code = dict_int_param(op, "Width", 0, 0x7fff, -1,
				    &pim->Width)) < 0 ||
	     (code = dict_int_param(op, "Height", 0, 0x7fff, -1,
				    &pim->Height)) < 0 ||
	     (code = dict_matrix_param(op, "ImageMatrix",
				    &pim->ImageMatrix)) < 0 ||
	     (code = dict_bool_param(op, "MultipleDataSources", false,
				    &pip->MultipleDataSources)) < 0 ||
	     (code = dict_int_param(op, "BitsPerComponent", 0,
				    max_bits_per_component, -1,
				    &pim->BitsPerComponent)) < 0 ||
	     (code = decode_size = dict_float_array_param(op, "Decode",
				    num_components * 2,
				    &pim->Decode[0], NULL)) < 0 ||
	     (code = dict_bool_param(op, "Interpolate", false,
				    &pim->Interpolate)) < 0 ||
	     (code = dict_bool_param(op, "CombineWithColor", false,
				    &pim->CombineWithColor)) < 0
	   )
	  return code;
	if ( decode_size == 0 )
	  pip->pDecode = 0;
	else if ( decode_size != num_components * 2 )
	  return_error(e_rangecheck);
	else
	  pip->pDecode = &pim->Decode[0];
	/* Extract and check the data sources. */
	if ( (code = dict_find_string(op, "DataSource", &pds)) < 0 )
	  return code;
	if ( pip->MultipleDataSources )
	{	check_type_only(*pds, t_array);
		if ( r_size(pds) != num_components )
		  return_error(e_rangecheck);
		memcpy(&pip->DataSource[0], pds->value.refs, sizeof(ref) * num_components);
	}
	else
	  pip->DataSource[0] = *pds;
#undef pim
	return 0;
}

/* (<width> <height> <bits/sample> <matrix> <datasrc> image -) */
/* <dict> image - */
private int
z2image(register os_ptr op)
{	if ( level2_enabled )
	{	check_op(1);
		if ( r_has_type(op, t_dictionary) )
		  {	image_params ip;
			int code;

			gs_image_t_init_color(&ip.image);
			code = image_dict_unpack(op, &ip, 12);
			if ( code < 0 )
			  return code;
			ip.image.ColorSpace = gs_currentcolorspace(igs);
			return zimage_setup(&ip.image, ip.MultipleDataSources,
					    &ip.DataSource[0], 1);
		  }
	}
	/* Level 1 image operator */
	check_op(5);
	return zimage(op);
}

/* (<width> <height> <paint_1s> <matrix> <datasrc> imagemask -) */
/* <dict> imagemask - */
private int
z2imagemask(register os_ptr op)
{	if ( level2_enabled )
	{	check_op(1);
		if ( r_has_type(op, t_dictionary) )
		  {	image_params ip;
			int code;

			gs_image_t_init_mask(&ip.image, false);
			code = image_dict_unpack(op, &ip, 1);
			if ( code < 0 )
			  return code;
			if ( ip.MultipleDataSources )
			  return_error(e_rangecheck);
			return zimage_setup(&ip.image, false,
					    &ip.DataSource[0], 1);
		  }
	}
	/* Level 1 imagemask operator */
	check_op(5);
	return zimagemask(op);
}

/* ------ Initialization procedure ------ */

/* Note that these override the definitions in zpaint.c. */
BEGIN_OP_DEFS(zimage2_l2_op_defs) {
		op_def_begin_level2(),
	{"1image", z2image},
	{"1imagemask", z2imagemask},
END_OP_DEFS(0) }
