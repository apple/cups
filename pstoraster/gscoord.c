/* Copyright (C) 1989, 1995 Aladdin Enterprises.  All rights reserved.
  
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

/* gscoord.c */
/* Coordinate system operators for Ghostscript library */
#include "math_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsccode.h"			/* for gxfont.h */
#include "gxfarith.h"
#include "gxfixed.h"
#include "gxmatrix.h"
#include "gxfont.h"			/* for char_tm */
#include "gxpath.h"			/* for gx_path_translate */
#include "gzstate.h"
#include "gxcoord.h"			/* requires gsmatrix, gsstate */
#include "gxdevice.h"

/* Choose whether to enable the rounding code in update_ctm. */
#define ROUND_CTM_FIXED 1

/* Forward declarations */
#ifdef DEBUG
#define trace_ctm(pgs) trace_matrix_fixed(&(pgs)->ctm)
private void near trace_matrix_fixed(P1(const gs_matrix_fixed *));
private void near trace_matrix(P1(const gs_matrix *));
#endif

/* Macro for ensuring ctm_inverse is valid */
#ifdef DEBUG
#define print_inverse(pgs)\
if ( gs_debug_c('x') )\
	dprintf("[x]Inverting:\n"), trace_ctm(pgs), trace_matrix(&pgs->ctm_inverse)
#else
#define print_inverse(pgs) DO_NOTHING
#endif
#define ensure_inverse_valid(pgs)\
	if ( !pgs->ctm_inverse_valid )\
	   {	int code = ctm_set_inverse(pgs);\
		if ( code < 0 ) return code;\
	   }

private int
ctm_set_inverse(gs_state *pgs)
{	int code = gs_matrix_invert(&ctm_only(pgs), &pgs->ctm_inverse);
	print_inverse(pgs);
	if ( code < 0 ) return code;
	pgs->ctm_inverse_valid = true;
	return 0;
}

/* Machinery for updating fixed version of ctm. */
/*
 * We (conditionally) adjust the floating point translation
 * so that it exactly matches the (rounded) fixed translation.
 * This avoids certain unpleasant rounding anomalies, such as
 * 0 0 moveto currentpoint not returning 0 0, and () stringwidth
 * not returning 0 0.
 */
#if ROUND_CTM_FIXED
#  define update_t_fixed(mat, t, t_fixed, v)\
    (set_float2fixed_vars((mat).t_fixed, v),\
     set_fixed2float_var((mat).t, (mat).t_fixed))
#else					/* !ROUND_CTM_FIXED */
#  define update_t_fixed(mat, t, t_fixed, v)\
    ((mat).t = (v),\
     set_float2fixed_vars((mat).t_fixed, (mat).t))
#endif					/* (!)ROUND_CTM_FIXED */
#define f_fits_in_fixed(f) f_fits_in_bits(f, fixed_int_bits)
#define update_matrix_fixed(mat, xt, yt)\
  ((mat).txy_fixed_valid = (f_fits_in_fixed(xt) && f_fits_in_fixed(yt) ?\
			    (update_t_fixed(mat, tx, tx_fixed, xt),\
			     update_t_fixed(mat, ty, ty_fixed, yt), true) :\
			    ((mat).tx = (xt), (mat).ty = (yt), false)))
#define update_ctm(pgs, xt, yt)\
  (pgs->ctm_inverse_valid = false,\
   pgs->char_tm_valid = false,\
   update_matrix_fixed(pgs->ctm, xt, yt))

/* ------ Coordinate system definition ------ */

int
gs_initmatrix(gs_state *pgs)
{	gs_matrix imat;
	gs_defaultmatrix(pgs, &imat);
	update_ctm(pgs, imat.tx, imat.ty);
	set_ctm_only(pgs, imat);
#ifdef DEBUG
if ( gs_debug_c('x') )
	dprintf("[x]initmatrix:\n"), trace_ctm(pgs);
#endif
	return 0;
}

int
gs_defaultmatrix(const gs_state *pgs, gs_matrix *pmat)
{	gx_device *dev;
	if ( pgs->ctm_default_set )	/* set after Install */
	  { *pmat = pgs->ctm_default;
	    return 1;
	  }
	dev = gs_currentdevice_inline(pgs);
	gs_deviceinitialmatrix(dev, pmat);
	/* Add in the translation for the Margins. */
	pmat->tx += dev->Margins[0] *
	  dev->HWResolution[0] / dev->MarginsHWResolution[0];
	pmat->ty += dev->Margins[1] *
	  dev->HWResolution[1] / dev->MarginsHWResolution[1];
	return 0;
}

int
gs_setdefaultmatrix(gs_state *pgs, const gs_matrix *pmat)
{	if ( pmat == NULL )
	  pgs->ctm_default_set = false;
	else
	  { pgs->ctm_default = *pmat;
	    pgs->ctm_default_set = true;
	  }
	return 0;
}

int
gs_currentmatrix(const gs_state *pgs, gs_matrix *pmat)
{	*pmat = ctm_only(pgs);
	return 0;
}

/* Set the current transformation matrix for rendering text. */
/* Note that this may be based on a font other than the current font. */
int
gs_setcharmatrix(gs_state *pgs, const gs_matrix *pmat)
{	gs_matrix cmat;
	int code = gs_matrix_multiply(pmat, &ctm_only(pgs), &cmat);
	if ( code < 0 )
	  return code;
	update_matrix_fixed(pgs->char_tm, cmat.tx, cmat.ty);
	char_tm_only(pgs) = cmat;
#ifdef DEBUG
if ( gs_debug_c('x') )
	dprintf("[x]setting char_tm:"), trace_matrix_fixed(&pgs->char_tm);
#endif
	pgs->char_tm_valid = true;
	return 0;
}

/* Read (after possibly computing) the current transformation matrix */
/* for rendering text.  If force=true, update char_tm if it is invalid; */
/* if force=false, don't update char_tm, and return an error code. */
int
gs_currentcharmatrix(gs_state *pgs, gs_matrix *ptm, bool force)
{	if ( !pgs->char_tm_valid )
	{	int code;
		if ( !force )
		  return_error(gs_error_undefinedresult);
		code = gs_setcharmatrix(pgs, &pgs->font->FontMatrix);
		if ( code < 0 )
		  return code;
	}
	if ( ptm != NULL )
	  *ptm = char_tm_only(pgs);
	return 0;
}

int
gs_setmatrix(gs_state *pgs, const gs_matrix *pmat)
{	update_ctm(pgs, pmat->tx, pmat->ty);
	set_ctm_only(pgs, *pmat);
#ifdef DEBUG
if ( gs_debug_c('x') )
	dprintf("[x]setmatrix:\n"), trace_ctm(pgs);
#endif
	return 0;
}

int
gs_settocharmatrix(gs_state *pgs)
{	if ( pgs->char_tm_valid )
	{	pgs->ctm = pgs->char_tm;
		pgs->ctm_inverse_valid = false;
		return 0;
	}
	else
		return_error(gs_error_undefinedresult);
}

int
gs_translate(gs_state *pgs, floatp dx, floatp dy)
{	gs_point pt;
	int code;
	if ( (code = gs_distance_transform(dx, dy, &ctm_only(pgs), &pt)) < 0 )
	  return code;
	pt.x += pgs->ctm.tx;
	pt.y += pgs->ctm.ty;
	update_ctm(pgs, pt.x, pt.y);
#ifdef DEBUG
if ( gs_debug_c('x') )
	dprintf4("[x]translate: %f %f -> %f %f\n",
		 dx, dy, pt.x, pt.y),
	trace_ctm(pgs);
#endif
	return 0;
}

int
gs_scale(gs_state *pgs, floatp sx, floatp sy)
{	pgs->ctm.xx *= sx;
	pgs->ctm.xy *= sx;
	pgs->ctm.yx *= sy;
	pgs->ctm.yy *= sy;
	pgs->ctm_inverse_valid = false, pgs->char_tm_valid = false;
#ifdef DEBUG
if ( gs_debug_c('x') )
	dprintf2("[x]scale: %f %f\n", sx, sy), trace_ctm(pgs);
#endif
	return 0;
}

int
gs_rotate(gs_state *pgs, floatp ang)
{	int code = gs_matrix_rotate(&ctm_only(pgs), ang,
				    &ctm_only_writable(pgs));
	pgs->ctm_inverse_valid = false, pgs->char_tm_valid = false;
#ifdef DEBUG
if ( gs_debug_c('x') )
	dprintf1("[x]rotate: %f\n", ang), trace_ctm(pgs);
#endif
	return code;
}

int
gs_concat(gs_state *pgs, const gs_matrix *pmat)
{	gs_matrix cmat;
	int code = gs_matrix_multiply(pmat, &ctm_only(pgs), &cmat);
	if ( code < 0 )
	  return code;
	update_ctm(pgs, cmat.tx, cmat.ty);
	set_ctm_only(pgs, cmat);
#ifdef DEBUG
if ( gs_debug_c('x') )
	dprintf("[x]concat:\n"), trace_matrix(pmat), trace_ctm(pgs);
#endif
	return code;
}

/* ------ Coordinate transformation ------ */

int
gs_transform(gs_state *pgs, floatp x, floatp y, gs_point *pt)
{	return gs_point_transform(x, y, &ctm_only(pgs), pt);
}

int
gs_dtransform(gs_state *pgs, floatp dx, floatp dy, gs_point *pt)
{	return gs_distance_transform(dx, dy, &ctm_only(pgs), pt);
}

int
gs_itransform(gs_state *pgs, floatp x, floatp y, gs_point *pt)
{	/* If the matrix isn't skewed, we get more accurate results */
	/* by using transform_inverse than by using the inverse matrix. */
	if ( !is_skewed(&pgs->ctm) )
	   {	return gs_point_transform_inverse(x, y, &ctm_only(pgs), pt);
	   }
	else
	   {	ensure_inverse_valid(pgs);
		return gs_point_transform(x, y, &pgs->ctm_inverse, pt);
	   }
}

int
gs_idtransform(gs_state *pgs, floatp dx, floatp dy, gs_point *pt)
{	/* If the matrix isn't skewed, we get more accurate results */
	/* by using transform_inverse than by using the inverse matrix. */
	if ( !is_skewed(&pgs->ctm) )
	   {	return gs_distance_transform_inverse(dx, dy,
						     &ctm_only(pgs), pt);
	   }
	else
	   {	ensure_inverse_valid(pgs);
		return gs_distance_transform(dx, dy, &pgs->ctm_inverse, pt);
	   }
}

int
gs_imager_idtransform(const gs_imager_state *pis, floatp dx, floatp dy,
  gs_point *pt)
{	return gs_distance_transform_inverse(dx, dy, &ctm_only(pis), pt);
}

/* ------ For internal use only ------ */

/* Set the translation to a fixed value, and translate any existing path. */
/* Used by gschar.c to prepare for a BuildChar or BuildGlyph procedure. */
int
gx_translate_to_fixed(register gs_state *pgs, fixed px, fixed py)
{	double fpx = fixed2float(px);
	double fdx = fpx - pgs->ctm.tx;
	double fpy = fixed2float(py);
	double fdy = fpy - pgs->ctm.ty;
	fixed dx, dy;
	int code;

	if ( pgs->ctm.txy_fixed_valid )
	  {	dx = float2fixed(fdx);
		dy = float2fixed(fdy);
		code = gx_path_translate(pgs->path, dx, dy);
		if ( code < 0 )
		  return code;
		if ( pgs->char_tm_valid && pgs->char_tm.txy_fixed_valid )
		  pgs->char_tm.tx_fixed += dx,
		  pgs->char_tm.ty_fixed += dy;
	  }
	else
	  {	if ( !gx_path_is_null(pgs->path) )
		  return_error(gs_error_limitcheck);
	  }
	pgs->ctm.tx = fpx;
	pgs->ctm.tx_fixed = px;
	pgs->ctm.ty = fpy;
	pgs->ctm.ty_fixed = py;
	pgs->ctm.txy_fixed_valid = true;
	pgs->ctm_inverse_valid = false;
	if ( pgs->char_tm_valid )
	{	/* Update char_tm now, leaving it valid. */
		pgs->char_tm.tx += fdx;
		pgs->char_tm.ty += fdy;
	}
#ifdef DEBUG
if ( gs_debug_c('x') )
	dprintf2("[x]translate_to_fixed %g, %g:\n",
		 fixed2float(px), fixed2float(py)),
	trace_ctm(pgs),
	dprintf("[x]   char_tm:\n"),
	trace_matrix_fixed(&pgs->char_tm);
#endif
	return 0;
}

/* Scale the CTM and character matrix for oversampling. */
int
gx_scale_char_matrix(register gs_state *pgs, int sx, int sy)
{
#define scale_cxy(s, vx, vy)\
  if ( s != 1 )\
   {	pgs->ctm.vx *= s;\
	pgs->ctm.vy *= s;\
	pgs->ctm_inverse_valid = false;\
	if ( pgs->char_tm_valid )\
	{	pgs->char_tm.vx *= s;\
		pgs->char_tm.vy *= s;\
	}\
   }
	scale_cxy(sx, xx, yx);
	scale_cxy(sy, xy, yy);
#undef scale_cxy
	if_debug2('x', "[x]char scale: %d %d\n", sx, sy);
	return 0;
}

/* Compute the coefficients for fast fixed-point distance transformations */
/* from a transformation matrix. */
/* We should cache the coefficients with the ctm.... */
int
gx_matrix_to_fixed_coeff(const gs_matrix *pmat, register fixed_coeff *pfc,
  int max_bits)
{	gs_matrix ctm;
	int scale = -10000;
	int expt, shift;
	ctm = *pmat;
	pfc->skewed = 0;
	if ( !is_fzero(ctm.xx) )
	   {	discard(frexp(ctm.xx, &scale));
	   }
	if ( !is_fzero(ctm.xy) )
	   {	discard(frexp(ctm.xy, &expt));
		if ( expt > scale ) scale = expt;
		pfc->skewed = 1;
	   }
	if ( !is_fzero(ctm.yx) )
	   {	discard(frexp(ctm.yx, &expt));
		if ( expt > scale ) scale = expt;
		pfc->skewed = 1;
	   }
	if ( !is_fzero(ctm.yy) )
	   {	discard(frexp(ctm.yy, &expt));
		if ( expt > scale ) scale = expt;
	   }
	scale = sizeof(long) * 8 - 1 - max_bits - scale;
	shift = scale - _fixed_shift;
	if ( shift > 0 )
	   {	pfc->shift = shift;
		pfc->round = (fixed)1 << (shift - 1);
	   }
	else
	   {	pfc->shift = 0;
		pfc->round = 0;
		scale -= shift;
	   }
#define set_c(c)\
  if ( is_fzero(ctm.c) ) pfc->c.f = 0, pfc->c.l = 0;\
  else pfc->c.f = ldexp(ctm.c, _fixed_shift), pfc->c.l = (long)ldexp(ctm.c, scale)
	set_c(xx);
	set_c(xy);
	set_c(yx);
	set_c(yy);
#ifdef DEBUG
if ( gs_debug_c('x') )
   {	dprintf6("[x]ctm: [%6g %6g %6g %6g %6g %6g]\n",
		 ctm.xx, ctm.xy, ctm.yx, ctm.yy, ctm.tx, ctm.ty);
	dprintf6("   scale=%d fc: [0x%lx 0x%lx 0x%lx 0x%lx] shift=%d\n",
		 scale, pfc->xx.l, pfc->xy.l, pfc->yx.l, pfc->yy.l,
		 pfc->shift);
   }
#endif
	pfc->max_bits = max_bits;
	return 0;
}

/* ------ Debugging printout ------ */

#ifdef DEBUG

/* Print a matrix */
private void near
trace_matrix_fixed(const gs_matrix_fixed *pmat)
{	trace_matrix((const gs_matrix *)pmat);
	if ( pmat->txy_fixed_valid )
	  {	dprintf2("\t\tt_fixed: [%6g %6g]\n",
			 fixed2float(pmat->tx_fixed),
			 fixed2float(pmat->ty_fixed));
	  }
	else
	  {	dputs("\t\tt_fixed not valid\n");
	  }
}
private void near
trace_matrix(register const gs_matrix *pmat)
{	dprintf6("\t[%6g %6g %6g %6g %6g %6g]\n",
		 pmat->xx, pmat->xy, pmat->yx, pmat->yy, pmat->tx, pmat->ty);
}

#endif
