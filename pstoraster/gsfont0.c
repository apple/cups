/* Copyright (C) 1994, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gsfont0.c */
/* Composite font operations for Ghostscript library */
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsstruct.h"
#include "gxfixed.h"
#include "gsmatrix.h"
#include "gxdevice.h"
#include "gxdevmem.h"
#include "gxchar.h"
#include "gxfcache.h"			/* gs_font_dir */
#include "gxfont.h"
#include "gxfont0.h"

/* Structure descriptor */
private struct_proc_enum_ptrs(font_type0_enum_ptrs);
private struct_proc_reloc_ptrs(font_type0_reloc_ptrs);
public_st_gs_font_type0();
#define pfont ((gs_font_type0 *)vptr)
private ENUM_PTRS_BEGIN(font_type0_enum_ptrs) ENUM_PREFIX(st_gs_font, gs_type0_data_max_ptrs);
	ENUM_PTR(0, gs_font_type0, data.Encoding);
	ENUM_PTR(1, gs_font_type0, data.FDepVector);
	case 2:
		if ( pfont->data.FMapType == fmap_SubsVector )
		  {	*pep = (void *)&pfont->data.SubsVector;
			return ptr_const_string_type;
		  }
		else
		  {	*pep = 0;
			break;
		  }
ENUM_PTRS_END
private RELOC_PTRS_BEGIN(font_type0_reloc_ptrs) RELOC_PREFIX(st_gs_font);
	RELOC_PTR(gs_font_type0, data.Encoding);
	RELOC_PTR(gs_font_type0, data.FDepVector);
	if ( pfont->data.FMapType == fmap_SubsVector )
	  RELOC_CONST_STRING_PTR(gs_font_type0, data.SubsVector);
RELOC_PTRS_END
#undef pfont

/* Adjust a composite font by concatenating a given matrix */
/* to the FontMatrix of all descendant composite fonts. */
private int
gs_type0_adjust_matrix(gs_font_dir *pdir, gs_font_type0 *pfont,
  const gs_matrix *pmat)
{	gs_font **pdep = pfont->data.FDepVector;
	uint fdep_size = pfont->data.fdep_size;
	gs_font **ptdep;
	uint i;
	/* Check for any descendant composite fonts. */
	for ( i = 0; i < fdep_size; i++ )
	  if ( pdep[i]->FontType == ft_composite )
	    break;
	if ( i == fdep_size )
		return 0;
	ptdep = gs_alloc_struct_array(pfont->memory, fdep_size, gs_font *,
				      &st_gs_font_ptr_element,
				      "gs_type0_adjust_font(FDepVector)");
	if ( ptdep == 0 )
		return_error(gs_error_VMerror);
	memcpy(ptdep, pdep, sizeof(gs_font *) * fdep_size);
	for ( ; i < fdep_size; i++ )
	  if ( pdep[i]->FontType == ft_composite )
	{	int code = gs_makefont(pdir, pdep[i], pmat, &ptdep[i]);
		if ( code < 0 )
			return code;
	}
	pfont->data.FDepVector = ptdep;
	return 0;
}

/* Finish defining a composite font, */
/* by adjusting its descendants' FontMatrices. */
int
gs_type0_define_font(gs_font_dir *pdir, gs_font *pfont)
{	const gs_matrix *pmat = &pfont->FontMatrix;
	/* Check for the identity matrix, which is common in root fonts. */
	if ( pmat->xx == 1.0 && pmat->yy == 1.0 &&
	     pmat->xy == 0.0 && pmat->yx == 0.0 &&
	     pmat->tx == 0.0 && pmat->ty == 0.0
	   )
		return 0;
	return gs_type0_adjust_matrix(pdir, (gs_font_type0 *)pfont, pmat);
}

/* Finish scaling a composite font similarly. */
int
gs_type0_make_font(gs_font_dir *pdir, const gs_font *pfont,
  const gs_matrix *pmat, gs_font **ppfont)
{	return gs_type0_adjust_matrix(pdir, (gs_font_type0 *)*ppfont, pmat);
}
