/* Copyright (C) 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gscolor3.c,v 1.1 2000/03/08 23:14:37 mike Exp $ */
/* "Operators" for LanguageLevel 3 color facilities */
#include "gx.h"
#include "gserrors.h"
#include "gscspace.h"		/* for gscolor2.h */
#include "gsmatrix.h"		/* for gscolor2.h */
#include "gscolor2.h"
#include "gscolor3.h"
#include "gspath.h"
#include "gzstate.h"
#include "gxshade.h"

/* setsmoothness */
int
gs_setsmoothness(gs_state * pgs, floatp smoothness)
{
    pgs->smoothness =
	(smoothness < 0 ? 0 : smoothness > 1 ? 1 : smoothness);
    return 0;
}

/* currentsmoothness */
float
gs_currentsmoothness(const gs_state * pgs)
{
    return pgs->smoothness;
}

/* shfill */
int
gs_shfill(gs_state * pgs, const gs_shading_t * psh)
{
    int code = gs_gsave(pgs);

    if (code < 0)
	return code;
    if ((code = gs_setcolorspace(pgs, psh->params.ColorSpace)) < 0 ||
	(code = gs_clippath(pgs)) < 0 ||
	(code = gs_shading_fill_path(psh, pgs->path,
				     gs_currentdevice(pgs),
				     (gs_imager_state *)pgs)) < 0
	)
	DO_NOTHING;
    gs_grestore(pgs);
    return code;
}
