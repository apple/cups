/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* zfont42.c */
/* Type 42 font creation operator */
#include "ghost.h"
#include "errors.h"
#include "oper.h"
#include "gsccode.h"
#include "gsmatrix.h"
#include "gxfont.h"
#include "gxfont42.h"
#include "bfont.h"
#include "idict.h"
#include "idparam.h"
#include "store.h"

/* Forward references */
private int z42_string_proc(P4(gs_font_type42 *, ulong, uint, const byte **));

/* <string|name> <font_dict> .buildfont42 <string|name> <font> */
/* Build a type 42 (TrueType) font. */
private int
zbuildfont42(os_ptr op)
{	build_proc_refs build;
	ref *psfnts;
	ref sfnts0;
#define sfd (sfnts0.value.const_bytes)
	gs_font_type42 *pfont;
	font_data *pdata;
	int code;

	code = build_proc_name_refs(&build,
				    "%Type42BuildChar", "%Type42BuildGlyph");
	if ( code < 0 )
	  return code;
	check_type(*op, t_dictionary);
	if ( dict_find_string(op, "sfnts", &psfnts) <= 0 )
	  return_error(e_invalidfont);
	if ( (code = array_get(psfnts, 0L, &sfnts0)) < 0 )
	  return code;
	if ( !r_has_type(&sfnts0, t_string) )
	  return_error(e_typecheck);
	code = build_gs_primitive_font(op, (gs_font_base **)&pfont,
				       ft_TrueType,
				       &st_gs_font_type42, &build);
	if ( code != 0 )
	  return code;
	pdata = pfont_data(pfont);
	ref_assign(&pdata->u.type42.sfnts, psfnts);
	pfont->data.string_proc = z42_string_proc;
	pfont->data.proc_data = (char *)pdata;
	code = gs_type42_font_init(pfont);
	if ( code < 0 )
	  return code;
	return define_gs_font((gs_font *)pfont);
}

/* ------ Initialization procedure ------ */

BEGIN_OP_DEFS(zfont42_op_defs) {
	{"2.buildfont42", zbuildfont42},
END_OP_DEFS(0) }

/* Procedure for accessing the sfnts array. */
private int
z42_string_proc(gs_font_type42 *pfont, ulong offset, uint length,
  const byte **pdata)
{	const font_data *pfdata = pfont_data(pfont);
	ulong left = offset;
	uint index = 0;

	for ( ; ; ++index )
	  { ref rstr;
	    int code = array_get(&pfdata->u.type42.sfnts, index, &rstr);
	    if ( code < 0 )
	      return code;
	    if ( !r_has_type(&rstr, t_string) )
	      return_error(e_typecheck);
	    if ( left < r_size(&rstr) )
	      { if ( left + length > r_size(&rstr) )
		  return_error(e_rangecheck);
		*pdata = rstr.value.const_bytes + left;
		return 0;
	      }
	    left -= r_size(&rstr);
	  }
}
