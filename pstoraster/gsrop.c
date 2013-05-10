/* Copyright (C) 1995 Aladdin Enterprises.  All rights reserved.
  
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

/* gsrop.c */
/* RasterOp / transparency / render algorithm accessing for library */
#include "gx.h"
#include "gserrors.h"
#include "gzstate.h"
#include "gsrop.h"

#define set_log_op(pgs, lopv)\
  (pgs)->log_op = (lopv)

/* setrasterop */
void
gs_setrasterop(gs_state *pgs, gs_rop3_t rop)
{	set_log_op(pgs, (rop & rop3_1) | (pgs->log_op & ~rop3_1));
}

/* currentrasterop */
gs_rop3_t
gs_currentrasterop(const gs_state *pgs)
{	return pgs->log_op & lop_rop_mask;
}

/* setsourcetransparent */
void
gs_setsourcetransparent(gs_state *pgs, bool transparent)
{	set_log_op(pgs,
		   (transparent ? pgs->log_op & ~lop_S_transparent:
		    pgs->log_op | lop_S_transparent));
}

/* currentsourcetransparent */
bool
gs_currentsourcetransparent(const gs_state *pgs)
{	return (pgs->log_op & lop_S_transparent) != 0;
}

/* settexturetransparent */
void
gs_settexturetransparent(gs_state *pgs, bool transparent)
{	set_log_op(pgs,
		   (transparent ? pgs->log_op & ~lop_T_transparent:
		    pgs->log_op | lop_T_transparent));
}

/* currenttexturetransparent */
bool
gs_currenttexturetransparent(const gs_state *pgs)
{	return (pgs->log_op & lop_T_transparent) != 0;
}

/* setrenderalgorithm */
int
gs_setrenderalgorithm(gs_state *pgs, int render_algorithm)
{	if ( render_algorithm < render_algorithm_min ||
	     render_algorithm > render_algorithm_max
	   )
	  return_error(gs_error_rangecheck);
	set_log_op(pgs,
		   (render_algorithm << lop_ral_shift) |
		   (pgs->log_op & ~(lop_ral_mask << lop_ral_shift)));
	return 0;
}

/* currentrenderalgorithm */
int
gs_currentrenderalgorithm(const gs_state *pgs)
{	return (pgs->log_op >> lop_ral_shift) & lop_ral_mask;
}
