/* Copyright (C) 1989, 1995, 1996, 1997 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gsline.c,v 1.2 2000/03/08 23:14:43 mike Exp $ */
/* Line parameter operators for Ghostscript library */
#include "math_.h"
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gxfixed.h"		/* ditto */
#include "gxmatrix.h"		/* for gzstate */
#include "gzstate.h"
#include "gsline.h"		/* for prototypes */
#include "gzline.h"

/* ------ Device-independent parameters ------ */

#define pgs_lp gs_currentlineparams_inline(pgs)

/* setlinewidth */
int
gs_setlinewidth(gs_state * pgs, floatp width)
{
    gx_set_line_width(pgs_lp, width);
    return 0;
}

/* currentlinewidth */
float
gs_currentlinewidth(const gs_state * pgs)
{
    return gx_current_line_width(pgs_lp);
}

/* setlinecap */
int
gs_setlinecap(gs_state * pgs, gs_line_cap cap)
{
    if ((uint) cap > gs_line_cap_max)
	return_error(gs_error_rangecheck);
    pgs_lp->cap = cap;
    return 0;
}

/* currentlinecap */
gs_line_cap
gs_currentlinecap(const gs_state * pgs)
{
    return pgs_lp->cap;
}

/* setlinejoin */
int
gs_setlinejoin(gs_state * pgs, gs_line_join join)
{
    if ((uint) join > gs_line_join_max)
	return_error(gs_error_rangecheck);
    pgs_lp->join = join;
    return 0;
}

/* currentlinejoin */
gs_line_join
gs_currentlinejoin(const gs_state * pgs)
{
    return pgs_lp->join;
}

/* setmiterlimit */
int
gx_set_miter_limit(gx_line_params * plp, floatp limit)
{
    if (limit < 1.0)
	return_error(gs_error_rangecheck);
    plp->miter_limit = limit;
    /*
     * Compute the miter check value.  The supplied miter limit is an
     * upper bound on 1/sin(phi/2); we convert this to a lower bound on
     * tan(phi).  Note that if phi > pi/2, this is negative.  We use the
     * half-angle and angle-sum formulas here to avoid the trig functions.
     * We also need a special check for phi/2 close to pi/4.
     * Some C compilers can't handle this as a conditional expression....
     */
    {
	double limit_squared = limit * limit;

	if (limit_squared < 2.0001 && limit_squared > 1.9999)
	    plp->miter_check = 1.0e6;
	else
	    plp->miter_check =
		sqrt(limit_squared - 1) * 2 / (limit_squared - 2);
    }
    return 0;
}
int
gs_setmiterlimit(gs_state * pgs, floatp limit)
{
    return gx_set_miter_limit(pgs_lp, limit);
}

/* currentmiterlimit */
float
gs_currentmiterlimit(const gs_state * pgs)
{
    return pgs_lp->miter_limit;
}

/* setdash */
int
gx_set_dash(gx_dash_params * dash, const float *pattern, uint length,
	    floatp offset, gs_memory_t * mem)
{
    uint n = length;
    const float *dfrom = pattern;
    bool ink = true;
    int index = 0;
    float pattern_length = 0.0;
    float dist_left;
    float *ppat = dash->pattern;

    /* Check the dash pattern. */
    while (n--) {
	float elt = *dfrom++;

	if (elt < 0)
	    return_error(gs_error_rangecheck);
	pattern_length += elt;
    }
    if (length == 0) {		/* empty pattern */
	dist_left = 0.0;
	if (mem && ppat) {
	    gs_free_object(mem, ppat, "gx_set_dash(old pattern)");
	    ppat = 0;
	}
    } else {
	uint size = length * sizeof(float);

	if (pattern_length == 0)
	    return_error(gs_error_rangecheck);
	/* Compute the initial index, ink_on, and distance left */
	/* in the pattern, according to the offset. */
#define f_mod(a, b) ((a) - floor((a) / (b)) * (b))
	if (length & 1) {	/* Odd and even repetitions of the pattern */
	    /* have opposite ink values! */
	    float length2 = pattern_length * 2;

	    dist_left = f_mod(offset, length2);
	    if (dist_left >= pattern_length)
		dist_left -= pattern_length, ink = !ink;
	} else
	    dist_left = f_mod(offset, pattern_length);
	while ((dist_left -= pattern[index]) >= 0 &&
	       (dist_left > 0 || pattern[index] != 0)
	    )
	    ink = !ink, index++;
	if (mem) {
	    if (ppat == 0)
		ppat = (float *)gs_alloc_bytes(mem, size,
					       "gx_set_dash(pattern)");
	    else if (length != dash->pattern_size)
		ppat = gs_resize_object(mem, ppat, size,
					"gx_set_dash(pattern)");
	    if (ppat == 0)
		return_error(gs_error_VMerror);
	}
	memcpy(ppat, pattern, length * sizeof(float));
    }
    dash->pattern = ppat;
    dash->pattern_size = length;
    dash->offset = offset;
    dash->pattern_length = pattern_length;
    dash->init_ink_on = ink;
    dash->init_index = index;
    dash->init_dist_left = -dist_left;
    return 0;
}
int
gs_setdash(gs_state * pgs, const float *pattern, uint length, floatp offset)
{
    return gx_set_dash(&pgs_lp->dash, pattern, length, offset,
		       pgs->memory);
}

/* currentdash */
uint
gs_currentdash_length(const gs_state * pgs)
{
    return pgs_lp->dash.pattern_size;
}
const float *
gs_currentdash_pattern(const gs_state * pgs)
{
    return pgs_lp->dash.pattern;
}
float
gs_currentdash_offset(const gs_state * pgs)
{
    return pgs_lp->dash.offset;
}

/* Internal accessor for line parameters */
const gx_line_params *
gs_currentlineparams(const gs_imager_state * pis)
{
    return gs_currentlineparams_inline(pis);
}

/* ------ Device-dependent parameters ------ */

/* setflat */
int
gs_imager_setflat(gs_imager_state * pis, floatp flat)
{
    if (flat <= 0.2)
	flat = 0.2;
    else if (flat > 100)
	flat = 100;
    pis->flatness = flat;
    return 0;
}
int
gs_setflat(gs_state * pgs, floatp flat)
{
    return gs_imager_setflat((gs_imager_state *) pgs, flat);
}

/* currentflat */
float
gs_currentflat(const gs_state * pgs)
{
    return pgs->flatness;
}

/* setstrokeadjust */
int
gs_setstrokeadjust(gs_state * pgs, bool stroke_adjust)
{
    pgs->stroke_adjust = stroke_adjust;
    return 0;
}

/* currentstrokeadjust */
bool
gs_currentstrokeadjust(const gs_state * pgs)
{
    return pgs->stroke_adjust;
}

/* ------ Extensions ------ */

/* Device-independent */

/* setdashadapt */
void
gs_setdashadapt(gs_state * pgs, bool adapt)
{
    pgs_lp->dash.adapt = adapt;
}

/* currentdashadapt */
bool
gs_imager_currentdashadapt(const gs_imager_state * pis)
{
    return gs_currentlineparams_inline(pis)->dash.adapt;
}
bool
gs_currentdashadapt(const gs_state * pgs)
{
    return gs_imager_currentdashadapt((const gs_imager_state *)pgs);
}

/* Device-dependent */

/* setaccuratecurves */
void
gs_setaccuratecurves(gs_state * pgs, bool accurate)
{
    pgs->accurate_curves = accurate;
}

/* currentaccuratecurves */
bool
gs_imager_currentaccuratecurves(const gs_imager_state * pis)
{
    return pis->accurate_curves;
}
bool
gs_currentaccuratecurves(const gs_state * pgs)
{
    return gs_imager_currentaccuratecurves((const gs_imager_state *)pgs);
}

/* setdotlength */
int
gx_set_dot_length(gx_line_params * plp, floatp length, bool absolute)
{
    if (length < 0)
	return_error(gs_error_rangecheck);
    plp->dot_length = length;
    plp->dot_length_absolute = absolute;
    return 0;
}
int
gs_setdotlength(gs_state * pgs, floatp length, bool absolute)
{
    return gx_set_dot_length(pgs_lp, length, absolute);
}

/* currentdotlength */
float
gs_currentdotlength(const gs_state * pgs)
{
    return pgs_lp->dot_length;
}
bool
gs_currentdotlength_absolute(const gs_state * pgs)
{
    return pgs_lp->dot_length_absolute;
}
