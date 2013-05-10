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

/* gxpath.c */
/* Internal path construction routines for Ghostscript library */
#include "gx.h"
#include "gserrors.h"
#include "gsstruct.h"
#include "gxfixed.h"
#include "gzpath.h"

/* These routines all assume that all points are */
/* already in device coordinates, and in fixed representation. */
/* As usual, they return either 0 or a (negative) error code. */

/* Forward references */
private subpath *path_alloc_copy(P1(gx_path *));
private int gx_path_new_subpath(P1(gx_path *));
#ifdef DEBUG
private void gx_print_segment(P1(const segment *));
#  define trace_segment(msg, pseg)\
     if ( gs_debug_c('P') ) dprintf(msg), gx_print_segment(pseg);
#else
#  define trace_segment(msg, pseg) DO_NOTHING
#endif

/* Macro for checking a point against a preset bounding box. */
#define outside_bbox(ppath, px, py)\
 (px < ppath->bbox.p.x || px > ppath->bbox.q.x ||\
  py < ppath->bbox.p.y || py > ppath->bbox.q.y)
#define check_in_bbox(ppath, px, py)\
  if ( outside_bbox(ppath, px, py) )\
	return_error(gs_error_rangecheck)

/* Structure descriptors for paths and path segment types. */
public_st_path();
private_st_segment();
private_st_line();
private_st_line_close();
private_st_curve();
private_st_subpath();

/* ------ Initialize/free paths ------ */

/* Allocate and initialize a path. */
gx_path *
gx_path_alloc(gs_memory_t *mem, client_name_t cname)
{	gx_path *ppath = gs_alloc_struct(mem, gx_path, &st_path, cname);
	if ( ppath == 0 )
	  return 0;
	gx_path_init(ppath, mem);
	return ppath;
}

/* Initialize a path */
void
gx_path_init(gx_path *ppath, gs_memory_t *mem)
{	ppath->memory = mem;
	gx_path_reset(ppath);
}
void
gx_path_reset(register gx_path *ppath)
{	ppath->box_last = 0;
	ppath->position_valid = 0;
	ppath->first_subpath = ppath->current_subpath = 0;
	ppath->subpath_count = 0;
	ppath->curve_count = 0;
	ppath->subpath_open = 0;
	ppath->shares_segments = 0;
	ppath->bbox_set = 0;
}

/* Release the contents of a path.  We do this in reverse order */
/* so as to maximize LIFO allocator behavior. */
void
gx_path_release(gx_path *ppath)
{	segment *pseg;
	if ( ppath->first_subpath == 0 ) return;	/* empty path */
	if ( ppath->shares_segments ) return;	/* segments are shared */
	pseg = (segment *)ppath->current_subpath->last;
	while ( pseg )
	{	segment *prev = pseg->prev;
		trace_segment("[P]release", pseg);
		gs_free_object(ppath->memory, pseg, "gx_path_release");
		pseg = prev;
	}
	ppath->first_subpath = 0;	/* prevent re-release */
}

/* Mark a path as shared */
void
gx_path_share(gx_path *ppath)
{	if ( ppath->first_subpath ) ppath->shares_segments = 1;
}

/* ------ Incremental path building ------ */

/* Macro for opening the current subpath. */
/* ppath points to the path; psub has been set to ppath->current_subpath. */
#define path_open()\
	if ( ppath->subpath_open <= 0 )\
	   {	int code;\
		if ( !ppath->position_valid )\
		  return_error(gs_error_nocurrentpoint);\
		code = gx_path_new_subpath(ppath);\
		if ( code < 0 ) return code;\
		psub = ppath->current_subpath;\
	   }

/* Macros for allocating path segments. */
/* Note that they assume that ppath points to the path, */
/* and that psub points to the current subpath. */
/* We have to split the macro into two because of limitations */
/* on the size of a single statement (sigh). */
#define p_alloc(pseg,size)\
  if_debug2('A', "[P]0x%lx<%lu>\n", (ulong)(pseg), (ulong)(size))
#define path_unshare(ppath)\
  if(ppath->shares_segments)\
    if(!(psub = path_alloc_copy(ppath)))return_error(gs_error_VMerror)
#define path_alloc_segment(pseg,ctype,pstype,stype,cname)\
  path_unshare(ppath);\
  if( !(pseg = gs_alloc_struct(ppath->memory, ctype, pstype, cname)) )\
    return_error(gs_error_VMerror);\
  p_alloc(pseg, sizeof(ctype));\
  pseg->type = stype, pseg->next = 0
#define path_alloc_link(pseg)\
  { segment *prev = psub->last;\
    prev->next = (segment *)pseg;\
    pseg->prev = prev;\
    psub->last = (segment *)pseg;\
  }

/* Open a new subpath */
private int
gx_path_new_subpath(gx_path *ppath)
{	subpath *psub = ppath->current_subpath;
	register subpath *spp;
	path_alloc_segment(spp, subpath, &st_subpath, s_start,
			   "gx_path_new_subpath");
	spp->last = (segment *)spp;
	spp->curve_count = 0;
	spp->is_closed = 0;
	spp->pt = ppath->position;
	ppath->subpath_open = 1;
	if ( !psub )			/* first subpath */
	   {	ppath->first_subpath = spp;
		spp->prev = 0;
	   }
	else
	   {	segment *prev = psub->last;
		prev->next = (segment *)spp;
		spp->prev = prev;
	   }
	ppath->current_subpath = spp;
	ppath->subpath_count++;
	trace_segment("[P]", (const segment *)spp);
	return 0;
}

/* Add a point to the current path (moveto). */
int
gx_path_add_point(register gx_path *ppath, fixed x, fixed y)
{	if ( ppath->bbox_set )
		check_in_bbox(ppath, x, y);
	ppath->subpath_open = -1;
	ppath->position_valid = 1;
	ppath->position.x = x;
	ppath->position.y = y;
	return 0;
}

/* Add a relative point to the current path (rmoveto). */
int
gx_path_add_relative_point(register gx_path *ppath, fixed dx, fixed dy)
{	if ( !ppath->position_valid )
	  return_error(gs_error_nocurrentpoint);
	if ( ppath->bbox_set )
	  check_in_bbox(ppath, ppath->position.x + dx, ppath->position.y + dy);
	ppath->subpath_open = -1;
	ppath->position.x += dx;
	ppath->position.y += dy;
	return 0;
}

/* Set the segment point and the current point in the path. */
/* Assumes ppath points to the path. */
#define path_set_point(pseg, fx, fy)\
	(pseg)->pt.x = ppath->position.x = (fx),\
	(pseg)->pt.y = ppath->position.y = (fy)

/* Add a line to the current path (lineto). */
int
gx_path_add_line(gx_path *ppath, fixed x, fixed y)
{	subpath *psub = ppath->current_subpath;
	register line_segment *lp;
	if ( ppath->bbox_set )
		check_in_bbox(ppath, x, y);
	path_open();
	path_alloc_segment(lp, line_segment, &st_line, s_line,
			   "gx_path_add_line");
	path_alloc_link(lp);
	path_set_point(lp, x, y);
	trace_segment("[P]", (segment *)lp);
	return 0;
}

/* Add multiple lines to the current path. */
int
gx_path_add_lines(gx_path *ppath, const gs_fixed_point *ppts, int count)
{	subpath *psub = ppath->current_subpath;
	segment *prev;
	register line_segment *lp = 0;
	int i;
	int code = 0;
	if ( count <= 0 )
	  return 0;
	path_open();
	path_unshare(ppath);
	prev = psub->last;
	/* We could do better than the following, but this is a start. */
	/* Note that we don't make any attempt to undo partial additions */
	/* if we fail partway through; this is equivalent to what would */
	/* happen with multiple calls on gx_path_add_line. */
	for ( i = 0; i < count; i++ )
	  {	fixed x = ppts[i].x;
		fixed y = ppts[i].y;
		line_segment *next;
		if ( ppath->bbox_set && outside_bbox(ppath, x, y) )
		  {	code = gs_note_error(gs_error_rangecheck);
			break;
		  }
		if ( !(next = gs_alloc_struct(ppath->memory, line_segment,
					      &st_line, "gx_path_add_lines"))
		   )
		  {	code = gs_note_error(gs_error_VMerror);
			break;
		  }
		lp = next;
		p_alloc(lp, sizeof(line_segment));
		lp->type = s_line;
		prev->next = (segment *)lp;
		lp->prev = prev;
		lp->pt.x = x;
		lp->pt.y = y;
		prev = (segment *)lp;
		trace_segment("[P]", (segment *)lp);
	  }
	if ( lp != 0 )
	  ppath->position.x = lp->pt.x,
	  ppath->position.y = lp->pt.y,
	  psub->last = (segment *)lp,
	  lp->next = 0;
	return code;
}

/* Add a rectangle to the current path. */
/* This is a special case of adding a closed polygon. */
int
gx_path_add_rectangle(gx_path *ppath, fixed x0, fixed y0, fixed x1, fixed y1)
{	gs_fixed_point pts[3];
	int code;
	pts[0].x = x0;
	pts[1].x = pts[2].x = x1;
	pts[2].y = y0;
	pts[0].y = pts[1].y = y1;
	if ( (code = gx_path_add_point(ppath, x0, y0)) < 0 ||
	     (code = gx_path_add_lines(ppath, pts, 3)) < 0 ||
	     (code = gx_path_close_subpath(ppath)) < 0
	   )
	  return code;
	return 0;
}

/* Add a curve to the current path (curveto). */
int
gx_path_add_curve(gx_path *ppath,
  fixed x1, fixed y1, fixed x2, fixed y2, fixed x3, fixed y3)
{	subpath *psub = ppath->current_subpath;
	register curve_segment *lp;
	if ( ppath->bbox_set )
	{	check_in_bbox(ppath, x1, y1);
		check_in_bbox(ppath, x2, y2);
		check_in_bbox(ppath, x3, y3);
	}
	path_open();
	path_alloc_segment(lp, curve_segment, &st_curve, s_curve,
			   "gx_path_add_curve");
	path_alloc_link(lp);
	lp->p1.x = x1;
	lp->p1.y = y1;
	lp->p2.x = x2;
	lp->p2.y = y2;
	path_set_point(lp, x3, y3);
	psub->curve_count++;
	ppath->curve_count++;
	trace_segment("[P]", (segment *)lp);
	return 0;
}

/*
 * Add an approximation of an arc to the current path.
 * The current point of the path is the initial point of the arc;
 * parameters are the final point of the arc
 * and the point at which the extended tangents meet.
 * We require that the arc be less than a semicircle.
 * The arc may go either clockwise or counterclockwise.
 * The approximation is a very simple one: a single curve
 * whose other two control points are a fraction F of the way
 * to the intersection of the tangents, where
 *	F = (4/3)(1 / (1 + sqrt(1+(d/r)^2)))
 * where r is the radius and d is the distance from either tangent
 * point to the intersection of the tangents.  This produces
 * a curve whose center point, as well as its ends, lies on
 * the desired arc.
 *
 * Because F has to be computed in user space, we let the client
 * compute it and pass it in as an argument.
 */
int
gx_path_add_partial_arc(gx_path *ppath,
  fixed x3, fixed y3, fixed xt, fixed yt, floatp fraction)
{	fixed x0 = ppath->position.x, y0 = ppath->position.y;
	return gx_path_add_curve(ppath,
			x0 + (fixed)((xt - x0) * fraction),
			y0 + (fixed)((yt - y0) * fraction),
			x3 + (fixed)((xt - x3) * fraction),
			y3 + (fixed)((yt - y3) * fraction),
			x3, y3);
}

/* Append a path to another path, and reset the first path. */
/* Currently this is only used to append a path to its parent */
/* (the path in the previous graphics context). */
int
gx_path_add_path(gx_path *ppath, gx_path *ppfrom)
{	subpath *psub;
	path_unshare(ppfrom);
	path_unshare(ppath);
	if ( ppfrom->first_subpath )	/* i.e. ppfrom not empty */
	   {	if ( ppath->first_subpath )	/* i.e. ppath not empty */
		   {	subpath *psub = ppath->current_subpath;
			segment *pseg = psub->last;
			subpath *pfsub = ppfrom->first_subpath;
			pseg->next = (segment *)pfsub;
			pfsub->prev = pseg;
		   }
		else
			ppath->first_subpath = ppfrom->first_subpath;
		ppath->current_subpath = ppfrom->current_subpath;
		ppath->subpath_count += ppfrom->subpath_count;
		ppath->curve_count += ppfrom->curve_count;
	   }
	/* Transfer the remaining state. */
	ppath->position = ppfrom->position;
	ppath->position_valid = ppfrom->position_valid;
	ppath->subpath_open = ppfrom->subpath_open;
	gx_path_reset(ppfrom);		/* reset the source path */
	return 0;
}

/* Add a path or its bounding box to the enclosing path, */
/* and reset the first path.  Only used for implementing charpath and its */
/* relatives. */
int
gx_path_add_char_path(gx_path *to_path, gx_path *from_path,
  gs_char_path_mode mode)
{	switch ( mode )
	  {
	  default:			/* shouldn't happen! */
		gx_path_reset(from_path);
		return 0;
	  case cpm_true_charpath:
	  case cpm_false_charpath:
		return gx_path_add_path(to_path, from_path);
	  case cpm_true_charboxpath:
	    { gs_fixed_rect bbox;
	      gx_path_bbox(from_path, &bbox);
	      return gx_path_add_rectangle(to_path, bbox.p.x, bbox.p.y,
					   bbox.q.x, bbox.q.y);
	    }
	  case cpm_false_charboxpath:
	    { gs_fixed_rect bbox;
	      int code;
	      gx_path_bbox(from_path, &bbox);
	      return
		((code = gx_path_add_point(to_path, bbox.p.x, bbox.p.y)) < 0 ?
		 code : gx_path_add_line(to_path, bbox.q.x, bbox.q.y));
	    }
	  }
}

/* Close the current subpath. */
int
gx_path_close_subpath(gx_path *ppath)
{	subpath *psub = ppath->current_subpath;
	register line_close_segment *lp;
	int code;
	switch ( ppath->subpath_open )
	{
	case 0:
		return 0;
	case -1:
		code = gx_path_new_subpath(ppath);
		if ( code < 0 )
			return 0;
		psub = ppath->current_subpath;
	/*case 1:*/
	}
	path_alloc_segment(lp, line_close_segment, &st_line_close, s_line_close,
			   "gx_path_close_subpath");
	path_alloc_link(lp);
	path_set_point(lp, psub->pt.x, psub->pt.y);
	lp->sub = psub;
	psub->is_closed = 1;
	ppath->subpath_open = 0;
	trace_segment("[P]", (segment *)lp);
	return 0;
}

/* Remove the last line from the current subpath, and then close it. */
/* The Type 1 font hinting routines use this if a path ends with */
/* a line to the start followed by a closepath. */
int
gx_path_pop_close_subpath(gx_path *ppath)
{	subpath *psub = ppath->current_subpath;
	segment *pseg;
	segment *prev;
	if ( psub == 0 || (pseg = psub->last) == 0 ||
	     pseg->type != s_line
	   )
	  return_error(gs_error_unknownerror);
	prev = pseg->prev;
	prev->next = 0;
	psub->last = prev;
	gs_free_object(ppath->memory, pseg, "gx_path_pop_close_subpath");
	return gx_path_close_subpath(ppath);
}

/* ------ Internal routines ------ */

/* Copy the current path, because it was shared. */
/* Return a pointer to the current subpath, or 0. */
private subpath *
path_alloc_copy(gx_path *ppath)
{	gx_path path_new;
	int code;
	code = gx_path_copy(ppath, &path_new, 1);
	if ( code < 0 ) return 0;
	*ppath = path_new;
	ppath->shares_segments = 0;
	return ppath->current_subpath;
}

/* ------ Debugging printout ------ */

#ifdef DEBUG

/* Print out a path with a label */
void
gx_dump_path(const gx_path *ppath, const char *tag)
{	dprintf2("[P]Path 0x%lx %s:\n", (ulong)ppath, tag);
	gx_path_print(ppath);
}

/* Print a path */
void
gx_path_print(const gx_path *ppath)
{	const segment *pseg = (const segment *)ppath->first_subpath;
	dprintf4("   subpaths=%d, curves=%d, point=(%f,%f)\n",
		 ppath->subpath_count, ppath->curve_count,
		 fixed2float(ppath->position.x),
		 fixed2float(ppath->position.y));
	dprintf5("   box=(%f,%f),(%f,%f) last=0x%lx\n",
		 fixed2float(ppath->bbox.p.x), fixed2float(ppath->bbox.p.y),
		 fixed2float(ppath->bbox.q.x), fixed2float(ppath->bbox.q.y),
		 (ulong)ppath->box_last);
	while ( pseg )
	   {	gx_print_segment(pseg);
		pseg = pseg->next;
	   }
}
private void
gx_print_segment(const segment *pseg)
{	char out[80];
	sprintf(out, "   0x%lx<0x%lx,0x%lx>: %%s (%6g,%6g) ",
		(ulong)pseg, (ulong)pseg->prev, (ulong)pseg->next,
		fixed2float(pseg->pt.x), fixed2float(pseg->pt.y));
	switch ( pseg->type )
	   {
	case s_start:
#define psub ((const subpath *)pseg)
		dprintf1(out, "start");
		dprintf2("#curves=%d last=0x%lx",
			 psub->curve_count, (ulong)psub->last);
#undef psub
		break;
	case s_curve:
		dprintf1(out, "curve");
#define pcur ((const curve_segment *)pseg)
		dprintf4("\n\tp1=(%f,%f) p2=(%f,%f)",
			 fixed2float(pcur->p1.x), fixed2float(pcur->p1.y),
			 fixed2float(pcur->p2.x), fixed2float(pcur->p2.y));
#undef pcur
		break;
	case s_line:
		dprintf1(out, "line");
		break;
	case s_line_close:
#define plc ((const line_close_segment *)pseg)
		dprintf1(out, "close");
		dprintf1(" 0x%lx", (ulong)(plc->sub));
#undef plc
		break;
	default:
	   {	char t[20];
		sprintf(t, "type 0x%x", pseg->type);
		dprintf1(out, t);
	   }
	   }
	dputc('\n');
}

#endif					/* DEBUG */
