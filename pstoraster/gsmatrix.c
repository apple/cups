/* Copyright (C) 1989, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gsmatrix.c,v 1.2 2000/03/08 23:14:44 mike Exp $ */
/* Matrix operators for Ghostscript library */
#include "math_.h"
#include "gx.h"
#include "gserrors.h"
#include "gxfarith.h"
#include "gxfixed.h"
#include "gxmatrix.h"

/* The identity matrix */
private const gs_matrix gs_identity_matrix =
{identity_matrix_body};

/* ------ Matrix creation ------ */

/* Create an identity matrix */
void
gs_make_identity(gs_matrix * pmat)
{
    *pmat = gs_identity_matrix;
}

/* Create a translation matrix */
int
gs_make_translation(floatp dx, floatp dy, gs_matrix * pmat)
{
    *pmat = gs_identity_matrix;
    pmat->tx = dx;
    pmat->ty = dy;
    return 0;
}

/* Create a scaling matrix */
int
gs_make_scaling(floatp sx, floatp sy, gs_matrix * pmat)
{
    *pmat = gs_identity_matrix;
    pmat->xx = sx;
    pmat->yy = sy;
    return 0;
}

/* Create a rotation matrix. */
/* The angle is in degrees. */
int
gs_make_rotation(floatp ang, gs_matrix * pmat)
{
    gs_sincos_t sincos;

    gs_sincos_degrees(ang, &sincos);
    pmat->yy = pmat->xx = sincos.cos;
    pmat->xy = sincos.sin;
    pmat->yx = -sincos.sin;
    pmat->tx = pmat->ty = 0.0;
    return 0;
}

/* ------ Matrix arithmetic ------ */

/* Multiply two matrices.  We should check for floating exceptions, */
/* but for the moment it's just too awkward. */
/* Since this is used heavily, we check for shortcuts. */
int
gs_matrix_multiply(const gs_matrix * pm1, const gs_matrix * pm2, gs_matrix * pmr)
{
    double xx1 = pm1->xx, yy1 = pm1->yy;
    double tx1 = pm1->tx, ty1 = pm1->ty;
    double xx2 = pm2->xx, yy2 = pm2->yy;
    double xy2 = pm2->xy, yx2 = pm2->yx;

    if (is_xxyy(pm1)) {
	pmr->tx = tx1 * xx2 + pm2->tx;
	pmr->ty = ty1 * yy2 + pm2->ty;
	if (is_fzero(xy2))
	    pmr->xy = 0;
	else
	    pmr->xy = xx1 * xy2,
		pmr->ty += tx1 * xy2;
	pmr->xx = xx1 * xx2;
	if (is_fzero(yx2))
	    pmr->yx = 0;
	else
	    pmr->yx = yy1 * yx2,
		pmr->tx += ty1 * yx2;
	pmr->yy = yy1 * yy2;
    } else {
	double xy1 = pm1->xy, yx1 = pm1->yx;

	pmr->xx = xx1 * xx2 + xy1 * yx2;
	pmr->xy = xx1 * xy2 + xy1 * yy2;
	pmr->yy = yx1 * xy2 + yy1 * yy2;
	pmr->yx = yx1 * xx2 + yy1 * yx2;
	pmr->tx = tx1 * xx2 + ty1 * yx2 + pm2->tx;
	pmr->ty = tx1 * xy2 + ty1 * yy2 + pm2->ty;
    }
    return 0;
}

/* Invert a matrix.  Return gs_error_undefinedresult if not invertible. */
int
gs_matrix_invert(const gs_matrix * pm, gs_matrix * pmr)
{				/* We have to be careful about fetch/store order, */
    /* because pm might be the same as pmr. */
    if (is_xxyy(pm)) {
	if (is_fzero(pm->xx) || is_fzero(pm->yy))
	    return_error(gs_error_undefinedresult);
	pmr->tx = -(pmr->xx = 1.0 / pm->xx) * pm->tx;
	pmr->xy = 0.0;
	pmr->yx = 0.0;
	pmr->ty = -(pmr->yy = 1.0 / pm->yy) * pm->ty;
    } else {
	double det = pm->xx * pm->yy - pm->xy * pm->yx;
	double mxx = pm->xx, mtx = pm->tx;

	if (det == 0)
	    return_error(gs_error_undefinedresult);
	pmr->xx = pm->yy / det;
	pmr->xy = -pm->xy / det;
	pmr->yx = -pm->yx / det;
	pmr->yy = mxx / det;	/* xx is already changed */
	pmr->tx = -(mtx * pmr->xx + pm->ty * pmr->yx);
	pmr->ty = -(mtx * pmr->xy + pm->ty * pmr->yy);	/* tx ditto */
    }
    return 0;
}

/* Translate a matrix, possibly in place. */
int
gs_matrix_translate(const gs_matrix * pm, floatp dx, floatp dy, gs_matrix * pmr)
{
    gs_point trans;
    int code = gs_distance_transform(dx, dy, pm, &trans);

    if (code < 0)
	return code;
    if (pmr != pm)
	*pmr = *pm;
    pmr->tx += trans.x;
    pmr->ty += trans.y;
    return 0;
}

/* Scale a matrix, possibly in place. */
int
gs_matrix_scale(const gs_matrix * pm, floatp sx, floatp sy, gs_matrix * pmr)
{
    pmr->xx = pm->xx * sx;
    pmr->xy = pm->xy * sx;
    pmr->yx = pm->yx * sy;
    pmr->yy = pm->yy * sy;
    if (pmr != pm) {
	pmr->tx = pm->tx;
	pmr->ty = pm->ty;
    }
    return 0;
}

/* Rotate a matrix, possibly in place.  The angle is in degrees. */
int
gs_matrix_rotate(const gs_matrix * pm, floatp ang, gs_matrix * pmr)
{
    double mxx, mxy;
    gs_sincos_t sincos;

    gs_sincos_degrees(ang, &sincos);
    mxx = pm->xx, mxy = pm->xy;
    pmr->xx = sincos.cos * mxx + sincos.sin * pm->yx;
    pmr->xy = sincos.cos * mxy + sincos.sin * pm->yy;
    pmr->yx = sincos.cos * pm->yx - sincos.sin * mxx;
    pmr->yy = sincos.cos * pm->yy - sincos.sin * mxy;
    if (pmr != pm) {
	pmr->tx = pm->tx;
	pmr->ty = pm->ty;
    }
    return 0;
}

/* ------ Coordinate transformations (floating point) ------ */

/* Note that all the transformation routines take separate */
/* x and y arguments, but return their result in a point. */

/* Transform a point. */
int
gs_point_transform(floatp x, floatp y, const gs_matrix * pmat,
		   gs_point * ppt)
{
    ppt->x = x * pmat->xx + pmat->tx;
    ppt->y = y * pmat->yy + pmat->ty;
    if (!is_fzero(pmat->yx))
	ppt->x += y * pmat->yx;
    if (!is_fzero(pmat->xy))
	ppt->y += x * pmat->xy;
    return 0;
}

/* Inverse-transform a point. */
/* Return gs_error_undefinedresult if the matrix is not invertible. */
int
gs_point_transform_inverse(floatp x, floatp y, const gs_matrix * pmat,
			   gs_point * ppt)
{
    if (is_xxyy(pmat)) {
	if (is_fzero(pmat->xx) || is_fzero(pmat->yy))
	    return_error(gs_error_undefinedresult);
	ppt->x = (x - pmat->tx) / pmat->xx;
	ppt->y = (y - pmat->ty) / pmat->yy;
	return 0;
    } else if (is_xyyx(pmat)) {
	if (is_fzero(pmat->xy) || is_fzero(pmat->yx))
	    return_error(gs_error_undefinedresult);
	ppt->x = (y - pmat->ty) / pmat->xy;
	ppt->y = (x - pmat->tx) / pmat->yx;
	return 0;
    } else {			/* There are faster ways to do this, */
	/* but we won't implement one unless we have to. */
	gs_matrix imat;
	int code = gs_matrix_invert(pmat, &imat);

	if (code < 0)
	    return code;
	return gs_point_transform(x, y, &imat, ppt);
    }
}

/* Transform a distance. */
int
gs_distance_transform(floatp dx, floatp dy, const gs_matrix * pmat,
		      gs_point * pdpt)
{
    pdpt->x = dx * pmat->xx;
    pdpt->y = dy * pmat->yy;
    if (!is_fzero(pmat->yx))
	pdpt->x += dy * pmat->yx;
    if (!is_fzero(pmat->xy))
	pdpt->y += dx * pmat->xy;
    return 0;
}

/* Inverse-transform a distance. */
/* Return gs_error_undefinedresult if the matrix is not invertible. */
int
gs_distance_transform_inverse(floatp dx, floatp dy,
			      const gs_matrix * pmat, gs_point * pdpt)
{
    if (is_xxyy(pmat)) {
	if (is_fzero(pmat->xx) || is_fzero(pmat->yy))
	    return_error(gs_error_undefinedresult);
	pdpt->x = dx / pmat->xx;
	pdpt->y = dy / pmat->yy;
    } else if (is_xyyx(pmat)) {
	if (is_fzero(pmat->xy) || is_fzero(pmat->yx))
	    return_error(gs_error_undefinedresult);
	pdpt->x = dy / pmat->xy;
	pdpt->y = dx / pmat->yx;
    } else {
	double det = pmat->xx * pmat->yy - pmat->xy * pmat->yx;

	if (det == 0)
	    return_error(gs_error_undefinedresult);
	pdpt->x = (dx * pmat->yy - dy * pmat->yx) / det;
	pdpt->y = (dy * pmat->xx - dx * pmat->xy) / det;
    }
    return 0;
}

/* Compute the bounding box of 4 points. */
int
gs_points_bbox(const gs_point pts[4], gs_rect * pbox)
{
#define assign_min_max(vmin, vmax, v0, v1)\
  if ( v0 < v1 ) vmin = v0, vmax = v1; else vmin = v1, vmax = v0
#define assign_min_max_4(vmin, vmax, v0, v1, v2, v3)\
  { double min01, max01, min23, max23;\
    assign_min_max(min01, max01, v0, v1);\
    assign_min_max(min23, max23, v2, v3);\
    vmin = min(min01, min23);\
    vmax = max(max01, max23);\
  }
    assign_min_max_4(pbox->p.x, pbox->q.x,
		     pts[0].x, pts[1].x, pts[2].x, pts[3].x);
    assign_min_max_4(pbox->p.y, pbox->q.y,
		     pts[0].y, pts[1].y, pts[2].y, pts[3].y);
#undef assign_min_max
#undef assign_min_max_4
    return 0;
}

/* Transform or inverse-transform a bounding box. */
/* Return gs_error_undefinedresult if the matrix is not invertible. */
private int
bbox_transform_either_only(const gs_rect * pbox_in, const gs_matrix * pmat,
			   gs_point pts[4],
     int (*point_xform) (P4(floatp, floatp, const gs_matrix *, gs_point *)))
{
    int code;

    if ((code = (*point_xform) (pbox_in->p.x, pbox_in->p.y, pmat, &pts[0])) < 0 ||
	(code = (*point_xform) (pbox_in->p.x, pbox_in->q.y, pmat, &pts[1])) < 0 ||
	(code = (*point_xform) (pbox_in->q.x, pbox_in->p.y, pmat, &pts[2])) < 0 ||
     (code = (*point_xform) (pbox_in->q.x, pbox_in->q.y, pmat, &pts[3])) < 0
	)
	DO_NOTHING;
    return code;
}

private int
bbox_transform_either(const gs_rect * pbox_in, const gs_matrix * pmat,
		      gs_rect * pbox_out,
     int (*point_xform) (P4(floatp, floatp, const gs_matrix *, gs_point *)))
{
    int code;

    /*
     * In principle, we could transform only one point and two
     * distance vectors; however, because of rounding, we will only
     * get fully consistent results if we transform all 4 points.
     * We must compute the max and min after transforming,
     * since a rotation may be involved.
     */
    gs_point pts[4];

    if ((code = bbox_transform_either_only(pbox_in, pmat, pts, point_xform)) < 0)
	return code;
    return gs_points_bbox(pts, pbox_out);
}
int
gs_bbox_transform(const gs_rect * pbox_in, const gs_matrix * pmat,
		  gs_rect * pbox_out)
{
    return bbox_transform_either(pbox_in, pmat, pbox_out,
				 gs_point_transform);
}
int
gs_bbox_transform_only(const gs_rect * pbox_in, const gs_matrix * pmat,
		       gs_point points[4])
{
    return bbox_transform_either_only(pbox_in, pmat, points,
				      gs_point_transform);
}
int
gs_bbox_transform_inverse(const gs_rect * pbox_in, const gs_matrix * pmat,
			  gs_rect * pbox_out)
{
    return bbox_transform_either(pbox_in, pmat, pbox_out,
				 gs_point_transform_inverse);
}

/* ------ Coordinate transformations (to fixed point) ------ */

#define f_fits_in_fixed(f) f_fits_in_bits(f, fixed_int_bits)

/* Transform a point with a fixed-point result. */
int
gs_point_transform2fixed(const gs_matrix_fixed * pmat,
			 floatp x, floatp y, gs_fixed_point * ppt)
{
    fixed px, py, t;
    double dtemp;
    int code;

    if (!pmat->txy_fixed_valid) {	/* The translation is out of range.  Do the */
	/* computation in floating point, and convert to */
	/* fixed at the end. */
	gs_point fpt;

	gs_point_transform(x, y, (const gs_matrix *)pmat, &fpt);
	if (!(f_fits_in_fixed(fpt.x) && f_fits_in_fixed(fpt.y)))
	    return_error(gs_error_limitcheck);
	ppt->x = float2fixed(fpt.x);
	ppt->y = float2fixed(fpt.y);
	return 0;
    }
    if (!is_fzero(pmat->xy)) {	/* Hope for 90 degree rotation */
	if ((code = set_dfmul2fixed_vars(px, y, pmat->yx, dtemp)) < 0 ||
	    (code = set_dfmul2fixed_vars(py, x, pmat->xy, dtemp)) < 0
	    )
	    return code;
	if (!is_fzero(pmat->xx)) {
	    if ((code = set_dfmul2fixed_vars(t, x, pmat->xx, dtemp)) < 0)
		return code;
	    px += t;		/* should check for overflow */
	}
	if (!is_fzero(pmat->yy)) {
	    if ((code = set_dfmul2fixed_vars(t, y, pmat->yy, dtemp)) < 0)
		return code;
	    py += t;		/* should check for overflow */
	}
    } else {
	if ((code = set_dfmul2fixed_vars(px, x, pmat->xx, dtemp)) < 0 ||
	    (code = set_dfmul2fixed_vars(py, y, pmat->yy, dtemp)) < 0
	    )
	    return code;
	if (!is_fzero(pmat->yx)) {
	    if ((code = set_dfmul2fixed_vars(t, y, pmat->yx, dtemp)) < 0)
		return code;
	    px += t;		/* should check for overflow */
	}
    }
    ppt->x = px + pmat->tx_fixed;	/* should check for overflow */
    ppt->y = py + pmat->ty_fixed;	/* should check for overflow */
    return 0;
}

/* Transform a distance with a fixed-point result. */
int
gs_distance_transform2fixed(const gs_matrix_fixed * pmat,
			    floatp dx, floatp dy, gs_fixed_point * ppt)
{
    fixed px, py, t;
    double dtemp;
    int code;

    if ((code = set_dfmul2fixed_vars(px, dx, pmat->xx, dtemp)) < 0 ||
	(code = set_dfmul2fixed_vars(py, dy, pmat->yy, dtemp)) < 0
	)
	return code;
    if (!is_fzero(pmat->yx)) {
	if ((code = set_dfmul2fixed_vars(t, dy, pmat->yx, dtemp)) < 0)
	    return code;
	px += t;		/* should check for overflow */
    }
    if (!is_fzero(pmat->xy)) {
	if ((code = set_dfmul2fixed_vars(t, dx, pmat->xy, dtemp)) < 0)
	    return code;
	py += t;		/* should check for overflow */
    }
    ppt->x = px;
    ppt->y = py;
    return 0;
}
