/* Copyright (C) 1989, 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gxfill.c,v 1.2 2000/03/08 23:14:58 mike Exp $ */
/* Lower-level path filling procedures */
#include "math_.h"		/* for floor in fixed_mult_quo */
#include "gx.h"
#include "gserrors.h"
#include "gsstruct.h"
#include "gxfixed.h"
#include "gxdevice.h"
#include "gzpath.h"
#include "gzcpath.h"
#include "gxdcolor.h"
#include "gxhttile.h"
#include "gxistate.h"
#include "gxpaint.h"		/* for prototypes */

/* Define which fill algorithm(s) to use. */
#define FILL_SCAN_LINES
#define FILL_CURVES
#define FILL_TRAPEZOIDS

/* Define the structure for keeping track of active lines. */
typedef struct active_line_s active_line;
struct active_line_s {
    gs_fixed_point start;	/* x,y where line starts */
    gs_fixed_point end;		/* x,y where line ends */
    gs_fixed_point diff;	/* end - start */
#define al_dx(alp) ((alp)->diff.x)
#define al_dy(alp) ((alp)->diff.y)
    fixed y_fast_max;		/* can do x_at_y in fixed point */
    /* if y <= y_fast_max */
#define set_al_points(alp, startp, endp)\
  (alp)->diff.x = (endp).x - (startp).x,\
  (alp)->y_fast_max = max_fixed /\
    (((alp)->diff.x >= 0 ? (alp)->diff.x : -(alp)->diff.x) | 1) + (startp).y,\
  (alp)->diff.y = (endp).y - (startp).y,\
  (alp)->start = startp, (alp)->end = endp
#define al_x_at_y(alp, yv)\
  ((yv) == (alp)->end.y ? (alp)->end.x :\
   ((yv) <= (alp)->y_fast_max ?\
    ((yv) - (alp)->start.y) * al_dx(alp) / al_dy(alp) :\
    (INCR_EXPR(slow_x),\
     fixed_mult_quo(al_dx(alp), (yv) - (alp)->start.y, al_dy(alp)))) +\
   (alp)->start.x)
    fixed x_current;		/* current x position */
    fixed x_next;		/* x position at end of band */
    const segment *pseg;	/* endpoint of this line */
    int direction;		/* direction of line segment */
#define dir_up 1
#define dir_horizontal 0	/* (these are handled specially) */
#define dir_down (-1)
    int curve_k;		/* # of subdivisions for curves, */
    /* -1 for lines */
    curve_cursor cursor;	/* cursor for curves, */
    /* unused for lines */
/* "Pending" lines (not reached in the Y ordering yet) use next and prev */
/* to order lines by increasing starting Y.  "Active" lines (being scanned) */
/* use next and prev to order lines by increasing current X, or if the */
/* current Xs are equal, by increasing final X. */
    active_line *prev, *next;
/* Link together active_lines allocated individually */
    active_line *alloc_next;
};

/*
 * The active_line structure isn't really simple, but since its instances
 * only exist temporarily during a fill operation, we don't have to
 * worry about a garbage collection occurring.
 */
gs_private_st_simple(st_active_line, active_line, "active_line");

/* Define the ordering criterion for active lines. */
/* The xc argument is a copy of lp2->x_current. */
#define x_precedes(lp1, lp2, xc)\
  (lp1->x_current < xc || (lp1->x_current == xc &&\
   (lp1->start.x > lp2->start.x || lp1->end.x < lp2->end.x)))

#ifdef DEBUG
/* Internal procedures for printing and checking active lines. */
private void
print_active_line(const char *label, const active_line * alp)
{
    dlprintf5("[f]%s 0x%lx(%d): x_current=%f x_next=%f\n",
	      label, (ulong) alp, alp->direction,
	      fixed2float(alp->x_current), fixed2float(alp->x_next));
    dlprintf5("    start=(%f,%f) pt_end=0x%lx(%f,%f)\n",
	      fixed2float(alp->start.x), fixed2float(alp->start.y),
	      (ulong) alp->pseg,
	      fixed2float(alp->end.x), fixed2float(alp->end.y));
    dlprintf2("    prev=0x%lx next=0x%lx\n",
	      (ulong) alp->prev, (ulong) alp->next);
}
private void
print_line_list(const active_line * flp)
{
    const active_line *lp;

    for (lp = flp; lp != 0; lp = lp->next) {
	fixed xc = lp->x_current, xn = lp->x_next;

	dlprintf3("[f]0x%lx(%d): x_current/next=%g",
		  (ulong) lp, lp->direction,
		  fixed2float(xc));
	if (xn != xc)
	    dprintf1("/%g", fixed2float(xn));
	dputc('\n');
    }
}
#define print_al(label,alp)\
  if ( gs_debug_c('F') ) print_active_line(label, alp)
private int
check_line_list(const active_line * flp)
{
    const active_line *alp;

    if (flp != 0)
	for (alp = flp->prev->next; alp != 0; alp = alp->next)
	    if (alp->next != 0 && alp->next->x_current < alp->x_current) {
		lprintf("[f]Lines out of order!\n");
		print_active_line("   1:", alp);
		print_active_line("   2:", alp->next);
		return_error(gs_error_Fatal);
	    }
    return 0;
}
#else
#define print_al(label,alp) DO_NOTHING
#endif

/* Line list structure */
struct line_list_s {
    gs_memory_t *memory;
    active_line *active_area;	/* allocated active_line list */
    active_line *next_active;	/* next allocation slot */
    active_line *limit;		/* limit of local allocation */
    int close_count;		/* # of added closing lines */
    active_line *y_list;	/* Y-sorted list of pending lines */
    active_line *y_line;	/* most recently inserted line */
    active_line x_head;		/* X-sorted list of active lines */
#define x_list x_head.next
    /* Put the arrays last so the scalars will have */
    /* small displacements. */
    /* Allocate a few active_lines locally */
    /* to avoid round trips through the allocator. */
#if arch_small_memory
#  define max_local_active 5	/* don't overburden the stack */
#else
#  define max_local_active 20
#endif
    active_line local_active[max_local_active];
};
typedef struct line_list_s line_list;
typedef line_list *ll_ptr;

/* Forward declarations */
private void init_line_list(P2(ll_ptr, gs_memory_t *));
private void unclose_path(P2(gx_path *, int));
private void free_line_list(P1(ll_ptr));
private int add_y_list(P5(gx_path *, ll_ptr, fixed, fixed,
			  const gs_fixed_rect *));
private int add_y_line(P4(const segment *, const segment *, int, ll_ptr));
private void insert_x_new(P2(active_line *, ll_ptr));
private bool end_x_line(P1(active_line *));

#define fill_loop_proc(proc)\
int proc(P11(ll_ptr, gx_device *,\
  const gx_fill_params *, const gx_device_color *, gs_logical_operation_t,\
  const gs_fixed_rect *, fixed, fixed, fixed, fixed, fixed))
private fill_loop_proc(fill_loop_by_scan_lines);
private fill_loop_proc(fill_loop_by_trapezoids);

/* Statistics */
#ifdef DEBUG
struct stats_fill_s {
    long
         fill, fill_alloc, y_up, y_down, horiz, x_step, slow_x, iter, find_y,
         band, band_step, band_fill, afill, slant, slant_shallow, sfill;
} stats_fill;

#  define INCR(x) (++(stats_fill.x))
#  define INCR_EXPR(x) INCR(x)
#  define INCR_BY(x,n) (stats_fill.x += (n))
#else
#  define INCR(x) DO_NOTHING
#  define INCR_EXPR(x) discard(0)
#  define INCR_BY(x,n) DO_NOTHING
#endif

/*
 * This is the general path filling algorithm.
 * It uses the center-of-pixel rule for filling.
 * We can implement Microsoft's upper-left-corner-of-pixel rule
 * by subtracting (0.5, 0.5) from all the coordinates in the path.
 *
 * The adjust parameters are a hack for keeping regions
 * from coming out too faint: they specify an amount by which to expand
 * the sides of every filled region.
 * Setting adjust = fixed_half is supposed to produce the effect of Adobe's
 * any-part-of-pixel rule, but it doesn't quite, because of the
 * closed/open interval rule for regions.  We detect this as a special case
 * and do the slightly ugly things necessary to make it work.
 */

/*
 * Tweak the fill adjustment if necessary so that (nearly) empty
 * rectangles are guaranteed to produce some output.  This is a hack
 * to work around a bug in the Microsoft Windows PostScript driver,
 * which draws thin lines by filling zero-width rectangles, and in
 * some other drivers that try to fill epsilon-width rectangles.
 */
void
gx_adjust_if_empty(const gs_fixed_rect * pbox, gs_fixed_point * adjust)
{
    const fixed
          dx = pbox->q.x - pbox->p.x, dy = pbox->q.y - pbox->p.y;

    if (dx < fixed_half && dy >= int2fixed(2)) {
	adjust->x = arith_rshift_1(fixed_1 + fixed_epsilon - dx);
	if_debug1('f', "[f]thin adjust_x=%g\n",
		  fixed2float(adjust->x));
    } else if (dy < fixed_half && dx >= int2fixed(2)) {
	adjust->y = arith_rshift_1(fixed_1 + fixed_epsilon - dy);
	if_debug1('f', "[f]thin adjust_y=%g\n",
		  fixed2float(adjust->y));
    }
}

/*
 * Fill a path.  This is the default implementation of the driver
 * fill_path procedure.
 */
int
gx_default_fill_path(gx_device * pdev, const gs_imager_state * pis,
		     gx_path * ppath, const gx_fill_params * params,
		 const gx_device_color * pdevc, const gx_clip_path * pcpath)
{
    gs_fixed_point adjust;

#define adjust_x adjust.x
#define adjust_y adjust.y
    gs_logical_operation_t lop = pis->log_op;
    gs_fixed_rect ibox, bbox;
    gx_device_clip cdev;
    gx_device *dev = pdev;
    gx_device *save_dev = dev;
    gx_path ffpath;
    gx_path *pfpath;
    int code;
    fixed adjust_left, adjust_right, adjust_below, adjust_above;
    int max_fill_band = dev->max_fill_band;

#define no_band_mask ((fixed)(-1) << (sizeof(fixed) * 8 - 1))
    bool fill_by_trapezoids;
    line_list lst;

    adjust = params->adjust;
    /*
     * Compute the bounding box before we flatten the path.
     * This can save a lot of time if the path has curves.
     * If the path is neither fully within nor fully outside
     * the quick-check boxes, we could recompute the bounding box
     * and make the checks again after flattening the path,
     * but right now we don't bother.
     */
    gx_path_bbox(ppath, &ibox);
    if (params->fill_zero_width)
	gx_adjust_if_empty(&ibox, &adjust);
    /* Check the bounding boxes. */
    if_debug6('f', "[f]adjust=%g,%g bbox=(%g,%g),(%g,%g)\n",
	      fixed2float(adjust_x), fixed2float(adjust_y),
	      fixed2float(ibox.p.x), fixed2float(ibox.p.y),
	      fixed2float(ibox.q.x), fixed2float(ibox.q.y));
    if (pcpath)
	gx_cpath_inner_box(pcpath, &bbox);
    else
	(*dev_proc(dev, get_clipping_box)) (dev, &bbox);
    if (!rect_within(ibox, bbox)) {	/*
					 * Intersect the path box and the clip bounding box.
					 * If the intersection is empty, this fill is a no-op.
					 */
	if (pcpath)
	    gx_cpath_outer_box(pcpath, &bbox);
	if_debug4('f', "   outer_box=(%g,%g),(%g,%g)\n",
		  fixed2float(bbox.p.x), fixed2float(bbox.p.y),
		  fixed2float(bbox.q.x), fixed2float(bbox.q.y));
	rect_intersect(ibox, bbox);
	if (ibox.p.x - adjust_x >= ibox.q.x + adjust_x ||
	    ibox.p.y - adjust_y >= ibox.q.y + adjust_y
	    ) {			/* Intersection of boxes is empty! */
	    return 0;
	}
#undef adjust_x
#undef adjust_y
	/*
	 * The path is neither entirely inside the inner clip box
	 * nor entirely outside the outer clip box.
	 * If we had to flatten the path, this is where we would
	 * recompute its bbox and make the tests again,
	 * but we don't bother right now.
	 *
	 * If there is a clipping path, set up a clipping device.
	 */
	if (pcpath) {
	    dev = (gx_device *) & cdev;
	    gx_make_clip_device(&cdev, &cdev, gx_cpath_list(pcpath));
	    cdev.target = save_dev;
	    cdev.max_fill_band = save_dev->max_fill_band;
	    (*dev_proc(dev, open_device)) (dev);
	}
    }
    /*
     * Compute the proper adjustment values.
     * To get the effect of the any-part-of-pixel rule,
     * we may have to tweak them slightly.
     * NOTE: We changed the adjust_right/above value from 0.5+epsilon
     * to 0.5 in release 5.01; even though this does the right thing
     * in every case we could imagine, we aren't confident that it's
     * correct.  (The old values were definitely incorrect, since they
     * caused 1-pixel-wide/high objects to color 2 pixels even if
     * they fell exactly on pixel boundaries.)
     */
    if (adjust.x == fixed_half)
	adjust_left = fixed_half - fixed_epsilon,
	    adjust_right = fixed_half /* + fixed_epsilon */ ;	/* see above */
    else
	adjust_left = adjust_right = adjust.x;
    if (adjust.y == fixed_half)
	adjust_below = fixed_half - fixed_epsilon,
	    adjust_above = fixed_half /* + fixed_epsilon */ ;	/* see above */
    else
	adjust_below = adjust_above = adjust.y;
    /* Initialize the active line list. */
    init_line_list(&lst, ppath->memory);
    /*
     * We have a choice of two different filling algorithms:
     * scan-line-based and trapezoid-based.  They compare as follows:
     *
     *      Scan    Trap
     *      ----    ----
     *       no     +yes    perfectly accurate Y adjustment
     *       skip   +draw   0-height horizontal lines
     *       slow   +fast   rectangles
     *      +fast    slow   curves
     *      +yes     no     write pixels at most once
     *
     * Normally we use the scan line algorithm for characters, where
     * curve speed is important and no Y adjustment is involved, and for
     * non-idempotent RasterOps, where double pixel writing must be
     * avoided, and the trapezoid algorithm otherwise.
     */
#define double_write_ok lop_is_idempotent(lop)
#ifdef FILL_SCAN_LINES
#  ifdef FILL_TRAPEZOIDS
    fill_by_trapezoids =
	((adjust_below | adjust_above) != 0 || !gx_path_has_curves(ppath) ||
	 params->flatness >= 1.0) && double_write_ok;
#  else
    fill_by_trapezoids = false;
#  endif
#else
    fill_by_trapezoids = double_write_ok;
#endif
#undef double_write_ok
    /*
     * Pre-process curves.  When filling by trapezoids, we need to
     * flatten the path completely; when filling by scan lines, we only
     * need to monotonize it, unless FILL_CURVES is undefined.
     */
    gx_path_init_local(&ffpath, ppath->memory);
    if (!gx_path_has_curves(ppath))	/* don't need to flatten */
	pfpath = ppath;
    else
#ifdef FILL_CURVES
    if (fill_by_trapezoids) {
	gx_path_init_local(&ffpath, ppath->memory);
	code = gx_path_add_flattened_accurate(ppath, &ffpath,
					      params->flatness,
					      pis->accurate_curves);
	if (code < 0)
	    return code;
	pfpath = &ffpath;
    } else if (gx_path_is_monotonic(ppath))
	pfpath = ppath;
    else {
	gx_path_init_local(&ffpath, ppath->memory);
	code = gx_path_add_monotonized(ppath, &ffpath);
	if (code < 0)
	    return code;
	pfpath = &ffpath;
    }
#else
    {
	gx_path_init_local(&ffpath, ppath->memory);
	code = gx_path_add_flattened_accurate(ppath, &ffpath,
					      params->flatness,
					      pis->accurate_curves);
	if (code < 0)
	    return code;
	pfpath = &ffpath;
    }
#endif
    if ((code = add_y_list(pfpath, &lst, adjust_below, adjust_above, &ibox)) < 0)
	goto nope;
    {
	fill_loop_proc((*fill_loop));

	/* Some short-sighted compilers won't allow a conditional here.... */
	if (fill_by_trapezoids)
	    fill_loop = fill_loop_by_trapezoids;
	else
	    fill_loop = fill_loop_by_scan_lines;
	code = (*fill_loop)
	    (&lst, dev, params, pdevc, lop, &ibox,
	     adjust_left, adjust_right, adjust_below, adjust_above,
	   (max_fill_band == 0 ? no_band_mask : int2fixed(-max_fill_band)));
    }
  nope:if (lst.close_count != 0)
	unclose_path(pfpath, lst.close_count);
    free_line_list(&lst);
    if (pfpath != ppath)	/* had to flatten */
	gx_path_free(pfpath, "gx_default_fill_path(flattened path)");
#ifdef DEBUG
    if (gs_debug_c('f')) {
	dlputs("[f]  # alloc    up  down  horiz step slowx  iter  find  band bstep bfill\n");
	dlprintf5(" %5ld %5ld %5ld %5ld %5ld",
		  stats_fill.fill, stats_fill.fill_alloc,
		  stats_fill.y_up, stats_fill.y_down,
		  stats_fill.horiz);
	dlprintf4(" %5ld %5ld %5ld %5ld",
		  stats_fill.x_step, stats_fill.slow_x,
		  stats_fill.iter, stats_fill.find_y);
	dlprintf3(" %5ld %5ld %5ld\n",
		  stats_fill.band, stats_fill.band_step,
		  stats_fill.band_fill);
	dlputs("[f]    afill slant shall sfill\n");
	dlprintf4("       %5ld %5ld %5ld %5ld\n",
		  stats_fill.afill, stats_fill.slant,
		  stats_fill.slant_shallow, stats_fill.sfill);
    }
#endif
    return code;
}

/* Initialize the line list for a path. */
private void
init_line_list(ll_ptr ll, gs_memory_t * mem)
{
    ll->memory = mem;
    ll->active_area = 0;
    ll->next_active = ll->local_active;
    ll->limit = ll->next_active + max_local_active;
    ll->close_count = 0;
    ll->y_list = 0;
    ll->y_line = 0;
    INCR(fill);
}

/* Unlink any line_close segments added temporarily. */
private void
unclose_path(gx_path * ppath, int count)
{
    subpath *psub;

    for (psub = ppath->first_subpath; count != 0;
	 psub = (subpath *) psub->last->next
	)
	if (psub->last == (segment *) & psub->closer) {
	    segment *prev = psub->closer.prev, *next = psub->closer.next;

	    prev->next = next;
	    if (next)
		next->prev = prev;
	    psub->last = prev;
	    count--;
	}
}

/* Free the line list. */
private void
free_line_list(ll_ptr ll)
{
    gs_memory_t *mem = ll->memory;
    active_line *alp;

    /* Free any individually allocated active_lines. */
    while ((alp = ll->active_area) != 0) {
	active_line *next = alp->alloc_next;

	gs_free_object(mem, alp, "active line");
	ll->active_area = next;
    }
}

/*
 * Construct a Y-sorted list of segments for rasterizing a path.  We assume
 * the path is non-empty.  Only include non-horizontal lines or (monotonic)
 * curve segments where one endpoint is locally Y-minimal, and horizontal
 * lines that might color some additional pixels.
 */
private int
add_y_list(gx_path * ppath, ll_ptr ll, fixed adjust_below, fixed adjust_above,
	   const gs_fixed_rect * pbox)
{
    register segment *pseg = (segment *) ppath->first_subpath;
    int close_count = 0;

    /* fixed xmin = pbox->p.x; *//* not currently used */
    fixed ymin = pbox->p.y;

    /* fixed xmax = pbox->q.x; *//* not currently used */
    fixed ymax = pbox->q.y;
    int code;

    while (pseg) {		/* We know that pseg points to a subpath head (s_start). */
	subpath *psub = (subpath *) pseg;
	segment *plast = psub->last;
	int dir = 2;		/* hack to skip first segment */
	int first_dir, prev_dir;
	segment *prev;

	if (plast->type != s_line_close) {	/* Create a fake s_line_close */
	    line_close_segment *lp = &psub->closer;
	    segment *next = plast->next;

	    lp->next = next;
	    lp->prev = plast;
	    plast->next = (segment *) lp;
	    if (next)
		next->prev = (segment *) lp;
	    lp->type = s_line_close;
	    lp->pt = psub->pt;
	    lp->sub = psub;
	    psub->last = plast = (segment *) lp;
	    ll->close_count++;
	}
	while ((prev_dir = dir, prev = pseg,
		(pseg = pseg->next) != 0 && pseg->type != s_start)
	    ) {			/*
				 * This element is either a line or a monotonic
				 * curve segment.
				 */
	    fixed iy = pseg->pt.y;
	    fixed py = prev->pt.y;

	    /*
	     * Segments falling entirely outside the ibox in Y
	     * are treated as though they were horizontal, *
	     * i.e., they are never put on the list.
	     */
#define compute_dir(xo, xe, yo, ye)\
  (ye > yo ? (ye <= ymin || yo >= ymax ? 0 : dir_up) :\
   ye < yo ? (yo <= ymin || ye >= ymax ? 0 : dir_down) :\
   2)
#define add_dir_lines(prev2, prev, this, pdir, dir)\
  if ( pdir )\
   { if ( (code = add_y_line(prev2, prev, pdir, ll)) < 0 ) return code; }\
  if ( dir )\
   { if ( (code = add_y_line(prev, this, dir, ll)) < 0 ) return code; }
	    dir = compute_dir(prev->pt.x, pseg->pt.x, py, iy);
	    if (dir == 2) {	/* Put horizontal lines on the list */
		/* if they would color any pixels. */
		if (fixed2int_pixround(iy - adjust_below) <
		    fixed2int_pixround(iy + adjust_above)
		    ) {
		    INCR(horiz);
		    if ((code = add_y_line(prev, pseg,
					   dir_horizontal, ll)) < 0
			)
			return code;
		}
		dir = 0;
	    }
	    if (dir > prev_dir) {
		add_dir_lines(prev->prev, prev, pseg, prev_dir, dir);
	    } else if (prev_dir == 2)	/* first segment */
		first_dir = dir;
	    if (pseg == plast) {	/*
					 * We skipped the first segment of the
					 * subpath, so the last segment must receive
					 * special consideration.  Note that we have
					 * `closed' all subpaths.
					 */
		if (first_dir > dir) {
		    add_dir_lines(prev, pseg, psub->next,
				  dir, first_dir);
		}
	    }
	}
#undef compute_dir
#undef add_dir_lines
    }
    return close_count;
}
/*
 * Internal routine to test a segment and add it to the pending list if
 * appropriate.
 */
private int
add_y_line(const segment * prev_lp, const segment * lp, int dir, ll_ptr ll)
{
    gs_fixed_point this, prev;
    register active_line *alp = ll->next_active;
    fixed y_start;

    if (alp == ll->limit) {	/* Allocate separately */
	alp = gs_alloc_struct(ll->memory, active_line,
			      &st_active_line, "active line");
	if (alp == 0)
	    return_error(gs_error_VMerror);
	alp->alloc_next = ll->active_area;
	ll->active_area = alp;
	INCR(fill_alloc);
    } else
	ll->next_active++;
    this.x = lp->pt.x;
    this.y = lp->pt.y;
    prev.x = prev_lp->pt.x;
    prev.y = prev_lp->pt.y;
    switch ((alp->direction = dir)) {
	case dir_up:
	    y_start = prev.y;
	    set_al_points(alp, prev, this);
	    alp->pseg = lp;
	    break;
	case dir_down:
	    y_start = this.y;
	    set_al_points(alp, this, prev);
	    alp->pseg = prev_lp;
	    break;
	case dir_horizontal:
	    y_start = this.y;	/* = prev.y */
	    alp->start = prev;
	    alp->end = this;
	    /* Don't need to set dx or y_fast_max */
	    alp->pseg = prev_lp;	/* may not need this either */
	    break;
    }
    /* Insert the new line in the Y ordering */
    {
	register active_line *yp = ll->y_line;
	register active_line *nyp;

	if (yp == 0) {
	    alp->next = alp->prev = 0;
	    ll->y_list = alp;
	} else if (y_start >= yp->start.y) {	/* Insert the new line after y_line */
	    while (INCR_EXPR(y_up),
		   ((nyp = yp->next) != NULL &&
		    y_start > nyp->start.y)
		)
		yp = nyp;
	    alp->next = nyp;
	    alp->prev = yp;
	    yp->next = alp;
	    if (nyp)
		nyp->prev = alp;
	} else {		/* Insert the new line before y_line */
	    while (INCR_EXPR(y_down),
		   ((nyp = yp->prev) != NULL &&
		    y_start < nyp->start.y)
		)
		yp = nyp;
	    alp->prev = nyp;
	    alp->next = yp;
	    yp->prev = alp;
	    if (nyp)
		nyp->next = alp;
	    else
		ll->y_list = alp;
	}
    }
    ll->y_line = alp;
    print_al("add ", alp);
    return 0;
}

/* ---------------- Filling loop utilities ---------------- */

/* Insert a newly active line in the X ordering. */
private void
insert_x_new(active_line * alp, ll_ptr ll)
{
    register active_line *next;
    register active_line *prev = &ll->x_head;
    register fixed x = alp->start.x;

    alp->x_current = x;
    while (INCR_EXPR(x_step),
	   (next = prev->next) != 0 && x_precedes(next, alp, x)
	)
	prev = next;
    alp->next = next;
    alp->prev = prev;
    if (next != 0)
	next->prev = alp;
    prev->next = alp;
}

/* Handle a line segment that just ended.  Return true iff this was */
/* the end of a line sequence. */
private bool
end_x_line(active_line * alp)
{
    const segment *pseg = alp->pseg;

    /*
     * The computation of next relies on the fact that
     * all subpaths have been closed.  When we cycle
     * around to the other end of a subpath, we must be
     * sure not to process the start/end point twice.
     */
    const segment *next =
    (alp->direction == dir_up ?
     (				/* Upward line, go forward along path. */
	 pseg->type == s_line_close ?	/* end of subpath */
	 ((const line_close_segment *)pseg)->sub->next :
	 pseg->next) :
     (				/* Downward line, go backward along path. */
	 pseg->type == s_start ?	/* start of subpath */
	 ((const subpath *)pseg)->last->prev :
	 pseg->prev)
    );
    gs_fixed_point npt;

    npt.y = next->pt.y;
    if_debug5('F', "[F]ended 0x%lx: pseg=0x%lx y=%f next=0x%lx npt.y=%f\n",
	      (ulong) alp, (ulong) pseg, fixed2float(pseg->pt.y),
	      (ulong) next, fixed2float(npt.y));
    if (npt.y <= pseg->pt.y) {	/* End of a line sequence */
	active_line *nlp = alp->next;

	alp->prev->next = nlp;
	if (nlp)
	    nlp->prev = alp->prev;
	if_debug1('F', "[F]drop 0x%lx\n", (ulong) alp);
	return true;
    }
    alp->pseg = next;
    npt.x = next->pt.x;
    set_al_points(alp, alp->end, npt);
    print_al("repl", alp);
    return false;
}

#define loop_fill_rectangle(x, y, w, h)\
  gx_fill_rectangle_device_rop(x, y, w, h, pdevc, dev, lop)
#define loop_fill_rectangle_direct(x, y, w, h)\
  (fill_direct ?\
   (*fill_rect)(dev, x, y, w, h, cindex) :\
   gx_fill_rectangle_device_rop(x, y, w, h, pdevc, dev, lop))

/* ---------------- Scan line filling loop ---------------- */

/* Forward references */
private void set_scan_line_points(P2(active_line *, fixed));

/* Main filling loop. */
private int
fill_loop_by_scan_lines(ll_ptr ll, gx_device * dev,
	       const gx_fill_params * params, const gx_device_color * pdevc,
		     gs_logical_operation_t lop, const gs_fixed_rect * pbox,
			fixed adjust_left, fixed adjust_right,
		    fixed adjust_below, fixed adjust_above, fixed band_mask)
{
    int rule = params->rule;
    fixed fixed_flat = float2fixed(params->flatness);
    bool fill_direct = color_writes_pure(pdevc, lop);
    gx_color_index cindex;

    dev_proc_fill_rectangle((*fill_rect));
    active_line *yll = ll->y_list;
    fixed y_limit = pbox->q.y;
    fixed y;

    /*
     * The meaning of adjust_below (B) and adjust_above (A) is that
     * the pixels that would normally be painted at coordinate Y get
     * "smeared" to coordinates Y-B through Y+A-epsilon, inclusive.
     * This is equivalent to saying that the pixels actually painted
     * at coordinate Y are those contributed by scan lines Y-A+epsilon
     * through Y+B, inclusive, or up to Y+B+epsilon, half-open.
     * (A = B = 0 is a special case, equivalent to B = 0, A = epsilon.)
     */
    fixed look_below =
    (adjust_above == fixed_0 ? fixed_0 : adjust_above - fixed_epsilon);
    fixed look_above =
    adjust_below + fixed_epsilon;
    fixed look_height = look_above + look_below;
    bool do_adjust = look_height > fixed_epsilon;

    if (yll == 0)		/* empty list */
	return 0;
    if (fill_direct)
	cindex = pdevc->colors.pure,
	    fill_rect = dev_proc(dev, fill_rectangle);
#define next_pixel_center(y)\
  (fixed_pixround(y) + fixed_half)
    y = next_pixel_center(yll->start.y) - look_below;	/* first Y sample point */
    ll->x_list = 0;
    ll->x_head.x_current = min_fixed;	/* stop backward scan */
    while (1) {
	active_line *alp, *nlp;
	fixed x;
	fixed ya = y + look_height;

	INCR(iter);
	/* Move newly active lines from y to x list. */
	while (yll != 0 && yll->start.y < ya) {
	    active_line *ynext = yll->next;	/* insert smashes next/prev links */

	    if (yll->direction == dir_horizontal) {	/* Ignore for now. */
	    } else {
		insert_x_new(yll, ll);
		set_scan_line_points(yll, fixed_flat);
	    }
	    yll = ynext;
	}
	/* Check whether we've reached the maximum y. */
	if (y >= y_limit)
	    break;
	if (ll->x_list == 0) {	/* No active lines, skip to next start */
	    if (yll == 0)
		break;		/* no lines left */
	    y = next_pixel_center(yll->start.y) - look_below;
	    continue;
	}
	/* Update active lines to y. */
	x = min_fixed;
	for (alp = ll->x_list; alp != 0; alp = nlp) {
	    fixed nx;

	    nlp = alp->next;
	  e:if (alp->end.y <= y) {
		if (end_x_line(alp))
		    continue;
		set_scan_line_points(alp, fixed_flat);
		goto e;
	    }
	    /* Note that if Y adjustment is in effect, */
	    /* alp->start.y might be greater than y. */
	    nx = alp->x_current =
		(alp->start.y >= y ? alp->start.x :
		 alp->curve_k < 0 ?
		 al_x_at_y(alp, y) :
		 gx_curve_x_at_y(&alp->cursor, y));
	    if (nx < x) {	/* Move this line backward in the list. */
		active_line *ilp = alp;

		while (nx < (ilp = ilp->prev)->x_current);
		/* Now ilp->x_current <= nx < ilp->next->x_cur. */
		alp->prev->next = alp->next;
		if (alp->next)
		    alp->next->prev = alp->prev;
		if (ilp->next)
		    ilp->next->prev = alp;
		alp->next = ilp->next;
		ilp->next = alp;
		alp->prev = ilp;
		continue;
	    }
	    x = nx;
	}

	/* Fill inside regions at y. */
	{
	    int inside = 0;
	    int x1_prev = min_int;

	    /* rule = -1 for winding number rule, i.e. */
	    /* we are inside if the winding number is non-zero; */
	    /* rule = 1 for even-odd rule, i.e. */
	    /* we are inside if the winding number is odd. */
#define inside_path_p() ((inside & rule) != 0)
	    INCR(band);
	    for (alp = ll->x_list; alp != 0; alp = alp->next) {		/* We're outside a filled region. */
		int x0 = fixed2int_pixround(alp->x_current -
					    adjust_left);

		/*
		 * This doesn't handle lines that cross
		 * within the adjustment region, but it's a
		 * good start.
		 */
		if (do_adjust && alp->end.x < alp->start.x) {
		    fixed xa = (alp->end.y < ya ? alp->end.x :
				alp->curve_k < 0 ?
				al_x_at_y(alp, ya) :
				gx_curve_x_at_y(&alp->cursor,
						ya));
		    int x0a = fixed2int_pixround(xa -
						 adjust_left);

		    if (x0a < x0)
			x0 = x0a;
		}
		for (;;) {	/* We're inside a filled region. */
		    print_al("step", alp);
		    INCR(band_step);
		    inside += alp->direction;
		    if (!inside_path_p())
			break;
		    /*
		     * Since we're dealing with closed
		     * paths, the test for alp == 0
		     * shouldn't be needed, but we may have
		     * omitted lines that are to the right
		     * of the clipping region.  */
		    if ((alp = alp->next) == 0)
			goto out;
		}
#undef inside_path_p
		/*
		 * We just went from inside to outside, so
		 * fill the region.  Avoid writing pixels
		 * twice.
		 */

		if (x0 < x1_prev)
		    x0 = x1_prev;
		{
		    int x1 = fixed2int_rounded(alp->x_current +
					       adjust_right);

		    if (do_adjust && alp->end.x > alp->start.x) {
			fixed xa = (alp->end.y < ya ?
				    alp->end.x :
				    alp->curve_k < 0 ?
				    al_x_at_y(alp, ya) :
				    gx_curve_x_at_y(&alp->cursor,
						    ya));
			int x1a = fixed2int_rounded(xa +
						    adjust_right);

			if (x1a > x1)
			    x1 = x1a;
		    }
		    if (x1 > x0) {
			int code =
			loop_fill_rectangle_direct(x0,
						   fixed2int_var(y),
						   x1 - x0, 1);

			if_debug3('F', "[F]drawing [%d:%d),%d\n",
				  x0, x1, fixed2int_var(y));
			if (code < 0)
			    return code;
			x1_prev = x1;
		    }
		}
	    }
	  out:;
	}
	y += fixed_1;
    }
    return 0;
}

private void
set_scan_line_points(active_line * alp, fixed fixed_flat)
{
    const segment *pseg = alp->pseg;
    const gs_fixed_point *pp0;

    if (alp->direction < 0) {
	pseg =
	    (pseg->type == s_line_close ?
	     ((const line_close_segment *)pseg)->sub->next :
	     pseg->next);
	if (pseg->type != s_curve) {
	    alp->curve_k = -1;
	    return;
	}
	pp0 = &alp->end;
    } else {
	if (pseg->type != s_curve) {
	    alp->curve_k = -1;
	    return;
	}
	pp0 = &alp->start;
    }
#define pcseg ((const curve_segment *)pseg)
    alp->curve_k =
	gx_curve_log2_samples(pp0->x, pp0->y, pcseg, fixed_flat);
    gx_curve_cursor_init(&alp->cursor, pp0->x, pp0->y, pcseg,
			 alp->curve_k);
#undef pcseg
}

/* ---------------- Trapezoid filling loop ---------------- */

/* Forward references */
private int fill_slant_adjust(P12(fixed, fixed, fixed, fixed, fixed,
				  fixed, fixed, fixed, const gs_fixed_rect *,
	     const gx_device_color *, gx_device *, gs_logical_operation_t));
private void resort_x_line(P1(active_line *));

/****** PATCH ******/
#define loop_fill_trapezoid_fixed(fx0, fw0, fy0, fx1, fw1, fh)\
  loop_fill_trap(dev, fx0, fw0, fy0, fx1, fw1, fh, pbox, pdevc, lop)
private int
loop_fill_trap(gx_device * dev, fixed fx0, fixed fw0, fixed fy0,
	       fixed fx1, fixed fw1, fixed fh, const gs_fixed_rect * pbox,
	       const gx_device_color * pdevc, gs_logical_operation_t lop)
{
    fixed fy1 = fy0 + fh;
    fixed ybot = max(fy0, pbox->p.y);
    fixed ytop = min(fy1, pbox->q.y);
    gs_fixed_edge left, right;

    if (ybot >= ytop)
	return 0;
    left.start.y = right.start.y = fy0;
    left.end.y = right.end.y = fy1;
    right.start.x = (left.start.x = fx0) + fw0;
    right.end.x = (left.end.x = fx1) + fw1;
    return (*dev_proc(dev, fill_trapezoid))
	(dev, &left, &right, ybot, ytop, false, pdevc, lop);
}
/****** END PATCH ******/

/* Main filling loop.  Takes lines off of y_list and adds them to */
/* x_list as needed.  band_mask limits the size of each band, */
/* by requiring that ((y1 - 1) & band_mask) == (y0 & band_mask). */
private int
fill_loop_by_trapezoids(ll_ptr ll, gx_device * dev,
	       const gx_fill_params * params, const gx_device_color * pdevc,
		     gs_logical_operation_t lop, const gs_fixed_rect * pbox,
			fixed adjust_left, fixed adjust_right,
		    fixed adjust_below, fixed adjust_above, fixed band_mask)
{
    int rule = params->rule;
    const fixed y_limit = pbox->q.y;
    active_line *yll = ll->y_list;
    fixed y;
    int code;
    bool fill_direct = color_writes_pure(pdevc, lop);
    gx_color_index cindex;

    dev_proc_fill_rectangle((*fill_rect));
/*
 * Define a faster test for
 *      fixed2int_pixround(y - below) != fixed2int_pixround(y + above)
 * where we know
 *      0 <= below <= _fixed_pixround_v,
 *      0 <= above <= min(fixed_half, fixed_1 - below).
 * Subtracting out the integer parts, this is equivalent to
 *      fixed2int_pixround(fixed_fraction(y) - below) !=
 *        fixed2int_pixround(fixed_fraction(y) + above)
 * or to
 *      fixed2int(fixed_fraction(y) + _fixed_pixround_v - below) !=
 *        fixed2int(fixed_fraction(y) + _fixed_pixround_v + above)
 * Letting A = _fixed_pixround_v - below and B = _fixed_pixround_v + above,
 * we can rewrite this as
 *      fixed2int(fixed_fraction(y) + A) != fixed2int(fixed_fraction(y) + B)
 * Because of the range constraints given above, this is true precisely when
 *      fixed_fraction(y) + A < fixed_1 && fixed_fraction(y) + B >= fixed_1
 * or equivalently
 *      fixed_fraction(y + B) < B - A.
 * i.e.
 *      fixed_fraction(y + _fixed_pixround_v + above) < below + above
 */
    fixed y_span_delta = _fixed_pixround_v + adjust_above;
    fixed y_span_limit = adjust_below + adjust_above;

#define adjusted_y_spans_pixel(y)\
  fixed_fraction((y) + y_span_delta) < y_span_limit

    if (yll == 0)
	return 0;		/* empty list */
    if (fill_direct)
	cindex = pdevc->colors.pure,
	    fill_rect = dev_proc(dev, fill_rectangle);
    y = yll->start.y;		/* first Y value */
    ll->x_list = 0;
    ll->x_head.x_current = min_fixed;	/* stop backward scan */
    while (1) {
	fixed y1;
	active_line *endp, *alp, *stopx;
	fixed x;
	int draw;

	INCR(iter);
	/* Move newly active lines from y to x list. */
	while (yll != 0 && yll->start.y == y) {
	    active_line *ynext = yll->next;	/* insert smashes next/prev links */

	    if (yll->direction == dir_horizontal) {	/* This is a hack to make sure that */
		/* isolated horizontal lines get stroked. */
		int yi = fixed2int_pixround(y - adjust_below);
		int xi, wi;

		if (yll->start.x <= yll->end.x)
		    xi = fixed2int_pixround(yll->start.x -
					    adjust_left),
			wi = fixed2int_pixround(yll->end.x +
						adjust_right) - xi;
		else
		    xi = fixed2int_pixround(yll->end.x -
					    adjust_left),
			wi = fixed2int_pixround(yll->start.x +
						adjust_right) - xi;
		code = loop_fill_rectangle_direct(xi, yi, wi, 1);
		if (code < 0)
		    return code;
	    } else
		insert_x_new(yll, ll);
	    yll = ynext;
	}
	/* Check whether we've reached the maximum y. */
	if (y >= y_limit)
	    break;
	if (ll->x_list == 0) {	/* No active lines, skip to next start */
	    if (yll == 0)
		break;		/* no lines left */
	    y = yll->start.y;
	    continue;
	}
	/* Find the next evaluation point. */
	/* Start by finding the smallest y value */
	/* at which any currently active line ends */
	/* (or the next to-be-active line begins). */
	y1 = (yll != 0 ? yll->start.y : y_limit);
	/* Make sure we don't exceed the maximum band height. */
	{
	    fixed y_band = y | ~band_mask;

	    if (y1 > y_band)
		y1 = y_band + 1;
	}
	for (alp = ll->x_list; alp != 0; alp = alp->next)
	    if (alp->end.y < y1)
		y1 = alp->end.y;
#ifdef DEBUG
	if (gs_debug_c('F')) {
	    dlprintf2("[F]before loop: y=%f y1=%f:\n",
		      fixed2float(y), fixed2float(y1));
	    print_line_list(ll->x_list);
	}
#endif
	/* Now look for line intersections before y1. */
	x = min_fixed;
#define have_pixels()\
  (fixed_pixround(y - adjust_below) < fixed_pixround(y1 + adjust_above))
	draw = (have_pixels()? 1 : -1);
	/*
	 * Loop invariants:
	 *      alp = endp->next;
	 *      for all lines lp from stopx up to alp,
	 *        lp->x_next = al_x_at_y(lp, y1).
	 */
	for (alp = stopx = ll->x_list;
	     INCR_EXPR(find_y), alp != 0;
	     endp = alp, alp = alp->next
	    ) {
	    fixed nx = al_x_at_y(alp, y1);
	    fixed dx_old, dx_den;

	    /* Check for intersecting lines. */
	    if (nx >= x)
		x = nx;
	    else if
		    (draw >= 0 &&	/* don't bother if no pixels */
		     (dx_old = alp->x_current - endp->x_current) >= 0 &&
		     (dx_den = dx_old + endp->x_next - nx) > dx_old
		) {		/* Make a good guess at the intersection */
		/* Y value using only local information. */
		fixed dy = y1 - y, y_new;

		if_debug3('f', "[f]cross: dy=%g, dx_old=%g, dx_new=%g\n",
			  fixed2float(dy), fixed2float(dx_old),
			  fixed2float(dx_den - dx_old));
		/* Do the computation in single precision */
		/* if the values are small enough. */
		y_new =
		    ((dy | dx_old) < 1L << (size_of(fixed) * 4 - 1) ?
		     dy * dx_old / dx_den :
		     fixed_mult_quo(dy, dx_old, dx_den))
		    + y;
		/* The crossing value doesn't have to be */
		/* very accurate, but it does have to be */
		/* greater than y and less than y1. */
		if_debug3('f', "[f]cross y=%g, y_new=%g, y1=%g\n",
			  fixed2float(y), fixed2float(y_new),
			  fixed2float(y1));
		stopx = alp;
		if (y_new <= y)
		    y_new = y + 1;
		if (y_new < y1) {
		    y1 = y_new;
		    nx = al_x_at_y(alp, y1);
		    draw = 0;
		}
		if (nx > x)
		    x = nx;
	    }
	    alp->x_next = nx;
	}
	/* Recompute next_x for lines before the intersection. */
	for (alp = ll->x_list; alp != stopx; alp = alp->next)
	    alp->x_next = al_x_at_y(alp, y1);
#ifdef DEBUG
	if (gs_debug_c('F')) {
	    dlprintf1("[F]after loop: y1=%f\n", fixed2float(y1));
	    print_line_list(ll->x_list);
	}
#endif
	/* Fill a multi-trapezoid band for the active lines. */
	/* Don't bother if no pixel centers lie within the band. */
	if (draw > 0 || (draw == 0 && have_pixels())) {

/*******************************************************************/
/* For readability, we start indenting from the left margin again. */
/*******************************************************************/

	    fixed height = y1 - y;
	    fixed xlbot, xltop;	/* as of last "outside" line */
	    int inside = 0;
	    active_line *nlp;

	    INCR(band);
	    for (x = min_fixed, alp = ll->x_list; alp != 0; alp = nlp) {
		fixed xbot = alp->x_current;
		fixed xtop = alp->x_current = alp->x_next;

#define nx xtop
		fixed wtop;
		int xi, xli;
		int code;

		print_al("step", alp);
		INCR(band_step);
		nlp = alp->next;
		/* Handle ended or out-of-order lines.  After this, */
		/* the only member of alp we use is alp->direction. */
		if (alp->end.y != y1 || !end_x_line(alp)) {
		    if (nx <= x)
			resort_x_line(alp);
		    else
			x = nx;
		}
#undef nx
		/* rule = -1 for winding number rule, i.e. */
		/* we are inside if the winding number is non-zero; */
		/* rule = 1 for even-odd rule, i.e. */
		/* we are inside if the winding number is odd. */
#define inside_path_p() ((inside & rule) != 0)
		if (!inside_path_p()) {		/* i.e., outside */
		    inside += alp->direction;
		    if (inside_path_p())	/* about to go in */
			xlbot = xbot, xltop = xtop;
		    continue;
		}
		/* We're inside a region being filled. */
		inside += alp->direction;
		if (inside_path_p())	/* not about to go out */
		    continue;
#undef inside_path_p
		/* We just went from inside to outside, so fill the region. */
		wtop = xtop - xltop;
		INCR(band_fill);
		/* If lines are temporarily out of */
		/* order, wtop might be negative. */
		/* Patch this up now. */
		if (wtop < 0) {
		    if_debug2('f', "[f]patch %g,%g\n",
			      fixed2float(xltop), fixed2float(xtop));
		    xtop = xltop += arith_rshift(wtop, 1);
		    wtop = 0;
		}
		if ((adjust_left | adjust_right) != 0) {
		    xlbot -= adjust_left;
		    xbot += adjust_right;
		    xltop -= adjust_left;
		    xtop += adjust_right;
		    wtop = xtop - xltop;
		}
		if ((xli = fixed2int_var_pixround(xltop)) ==
		    fixed2int_var_pixround(xlbot) &&
		    (xi = fixed2int_var_pixround(xtop)) ==
		    fixed2int_var_pixround(xbot)
		    ) {		/* Rectangle. */
		    int yi = fixed2int_pixround(y - adjust_below);
		    int wi = fixed2int_pixround(y1 + adjust_above) - yi;

		    code = loop_fill_rectangle_direct(xli, yi,
						      xi - xli, wi);
		} else if ((adjust_below | adjust_above) != 0) {	/*
									 * We want to get the effect of filling an area whose
									 * outline is formed by dragging a square of side adj2
									 * along the border of the trapezoid.  This is *not*
									 * equivalent to simply expanding the corners by
									 * adjust: There are 3 cases needing different
									 * algorithms, plus rectangles as a fast special case.
									 */
		    fixed wbot = xbot - xlbot;

		    if (xltop <= xlbot) {
			if (xtop >= xbot) {	/* Top wider than bottom. */
			    code = loop_fill_trapezoid_fixed(
					      xlbot, wbot, y - adjust_below,
						       xltop, wtop, height);
			    if (adjusted_y_spans_pixel(y1)) {
				if (code < 0)
				    return code;
				INCR(afill);
				code = loop_fill_rectangle_direct(
				 xli, fixed2int_pixround(y1 - adjust_below),
				     fixed2int_var_pixround(xtop) - xli, 1);
			    }
			} else {	/* Slanted trapezoid. */
			    code = fill_slant_adjust(xlbot, xbot, y,
					  xltop, xtop, height, adjust_below,
						     adjust_above, pbox,
						     pdevc, dev, lop);
			}
		    } else {
			if (xtop <= xbot) {	/* Bottom wider than top. */
			    if (adjusted_y_spans_pixel(y)) {
				INCR(afill);
				xli = fixed2int_var_pixround(xlbot);
				code = loop_fill_rectangle_direct(
				  xli, fixed2int_pixround(y - adjust_below),
				     fixed2int_var_pixround(xbot) - xli, 1);
				if (code < 0)
				    return code;
			    }
			    code = loop_fill_trapezoid_fixed(
					      xlbot, wbot, y + adjust_above,
						       xltop, wtop, height);
			} else {	/* Slanted trapezoid. */
			    code = fill_slant_adjust(xlbot, xbot, y,
					  xltop, xtop, height, adjust_below,
						     adjust_above, pbox,
						     pdevc, dev, lop);
			}
		    }
		} else		/* No Y adjustment. */
		    code = loop_fill_trapezoid_fixed(xlbot, xbot - xlbot,
						     y, xltop, wtop, height);
		if (code < 0)
		    return code;
	    }

/**************************************************************/
/* End of section requiring less indentation for readability. */
/**************************************************************/

	} else {		/* Just scan for ended or out-of-order lines. */
	    active_line *nlp;

	    for (x = min_fixed, alp = ll->x_list; alp != 0;
		 alp = nlp
		) {
		fixed nx = alp->x_current = alp->x_next;

		nlp = alp->next;
		if_debug4('F',
			  "[F]check 0x%lx,x=%g 0x%lx,x=%g\n",
			  (ulong) alp->prev, fixed2float(x),
			  (ulong) alp, fixed2float(nx));
		if (alp->end.y == y1) {
		    if (end_x_line(alp))
			continue;
		}
		if (nx <= x)
		    resort_x_line(alp);
		else
		    x = nx;
	    }
	}
#ifdef DEBUG
	if (gs_debug_c('f')) {
	    int code = check_line_list(ll->x_list);

	    if (code < 0)
		return code;
	}
#endif
	y = y1;
    }
    return 0;
}

/*
 * Handle the case of a slanted trapezoid with adjustment.
 * To do this exactly right requires filling a central trapezoid
 * (or rectangle) plus two horizontal almost-rectangles.
 */
private int
fill_slant_adjust(fixed xlbot, fixed xbot, fixed y,
		  fixed xltop, fixed xtop, fixed height, fixed adjust_below,
		  fixed adjust_above, const gs_fixed_rect * pbox,
		  const gx_device_color * pdevc, gx_device * dev,
		  gs_logical_operation_t lop)
{
    fixed y1 = y + height;

    dev_proc_fill_trapezoid((*fill_trap)) =
	dev_proc(dev, fill_trapezoid);
    const fixed yb = y - adjust_below;
    const fixed ya = y + adjust_above;
    const fixed y1b = y1 - adjust_below;
    const fixed y1a = y1 + adjust_above;
    const gs_fixed_edge *plbot;
    const gs_fixed_edge *prbot;
    const gs_fixed_edge *pltop;
    const gs_fixed_edge *prtop;
    gs_fixed_edge vert_left, slant_left, vert_right, slant_right;
    int code;

    INCR(slant);

    /* Set up all the edges, even though we may not need them all. */

    if (xlbot < xltop) {	/* && xbot < xtop */
	vert_left.start.x = vert_left.end.x = xlbot;
	vert_left.start.y = yb, vert_left.end.y = ya;
	vert_right.start.x = vert_right.end.x = xtop;
	vert_right.start.y = y1b, vert_right.end.y = y1a;
	slant_left.start.y = ya, slant_left.end.y = y1a;
	slant_right.start.y = yb, slant_right.end.y = y1b;
	plbot = &vert_left, prbot = &slant_right,
	    pltop = &slant_left, prtop = &vert_right;
    } else {
	vert_left.start.x = vert_left.end.x = xltop;
	vert_left.start.y = y1b, vert_left.end.y = y1a;
	vert_right.start.x = vert_right.end.x = xbot;
	vert_right.start.y = yb, vert_right.end.y = ya;
	slant_left.start.y = yb, slant_left.end.y = y1b;
	slant_right.start.y = ya, slant_right.end.y = y1a;
	plbot = &slant_left, prbot = &vert_right,
	    pltop = &vert_left, prtop = &slant_right;
    }
    slant_left.start.x = xlbot, slant_left.end.x = xltop;
    slant_right.start.x = xbot, slant_right.end.x = xtop;

    if (ya >= y1b) {		/*
				 * The upper and lower adjustment bands overlap.
				 * Since the entire entity is less than 2 pixels high
				 * in this case, we could handle it very efficiently
				 * with no more than 2 rectangle fills, but for right now
				 * we don't attempt to do this.
				 */
	int iyb = fixed2int_var_pixround(yb);
	int iya = fixed2int_var_pixround(ya);
	int iy1b = fixed2int_var_pixround(y1b);
	int iy1a = fixed2int_var_pixround(y1a);

	INCR(slant_shallow);
	if (iy1b > iyb) {
	    code = (*fill_trap) (dev, plbot, prbot,
				 yb, y1b, false, pdevc, lop);
	    if (code < 0)
		return code;
	}
	if (iya > iy1b) {
	    int ix = fixed2int_var_pixround(vert_left.start.x);
	    int iw = fixed2int_var_pixround(vert_right.start.x) - ix;

	    code = loop_fill_rectangle(ix, iy1b, iw, iya - iy1b);
	    if (code < 0)
		return code;
	}
	if (iy1a > iya)
	    code = (*fill_trap) (dev, pltop, prtop,
				 ya, y1a, false, pdevc, lop);
	else
	    code = 0;
    } else {			/*
				 * Clip the trapezoid if possible.  This can save a lot
				 * of work when filling paths that cross band boundaries.
				 */
	fixed yac;

	if (pbox->p.y < ya) {
	    code = (*fill_trap) (dev, plbot, prbot,
				 yb, ya, false, pdevc, lop);
	    if (code < 0)
		return code;
	    yac = ya;
	} else
	    yac = pbox->p.y;
	if (pbox->q.y > y1b) {
	    code = (*fill_trap) (dev, &slant_left, &slant_right,
				 yac, y1b, false, pdevc, lop);
	    if (code < 0)
		return code;
	    code = (*fill_trap) (dev, pltop, prtop,
				 y1b, y1a, false, pdevc, lop);
	} else
	    code = (*fill_trap) (dev, &slant_left, &slant_right,
				 yac, pbox->q.y, false, pdevc, lop);
    }
    return code;
}

/* Re-sort the x list by moving alp backward to its proper spot. */
private void
resort_x_line(active_line * alp)
{
    active_line *prev = alp->prev;
    active_line *next = alp->next;
    fixed nx = alp->x_current;

    prev->next = next;
    if (next)
	next->prev = prev;
    while (!x_precedes(prev, alp, nx)) {
	if_debug2('f', "[f]swap 0x%lx,0x%lx\n",
		  (ulong) alp, (ulong) prev);
	next = prev, prev = prev->prev;
    }
    alp->next = next;
    alp->prev = prev;
    /* next might be null, if alp was in */
    /* the correct spot already. */
    if (next)
	next->prev = alp;
    prev->next = alp;
}
