/* Copyright (C) 1991, 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gxcpath.c,v 1.3 2000/03/08 23:14:55 mike Exp $ */
/* Implementation of clipping paths, other than actual clipping */
#include "gx.h"
#include "gserrors.h"
#include "gsstruct.h"
#include "gsutil.h"
#include "gsline.h"
#include "gxdevice.h"
#include "gxfixed.h"
#include "gscoord.h"		/* needs gsmatrix.h */
#include "gxstate.h"
#include "gzpath.h"
#include "gzcpath.h"

/* Imported from gxacpath.c */
extern int gx_cpath_intersect_slow(P4(gs_state *, gx_clip_path *,
				      gx_path *, int));

/* Forward references */
private void gx_clip_list_from_rectangle(P2(gx_clip_list *, gs_fixed_rect *));

/* Other structure types */
public_st_clip_rect();
private_st_clip_list();
public_st_clip_path();
private_st_clip_rect_list();
public_st_device_clip();
private_st_cpath_enum();

/* GC procedures for gx_clip_path */
#define cptr ((gx_clip_path *)vptr)
private 
ENUM_PTRS_BEGIN(clip_path_enum_ptrs) return ENUM_USING(st_path, &cptr->path, sizeof(cptr->path), index - 1);

case 0:
ENUM_RETURN((cptr->rect_list == &cptr->local_list ? 0 :
	     cptr->rect_list));
ENUM_PTRS_END
private RELOC_PTRS_BEGIN(clip_path_reloc_ptrs)
{
    if (cptr->rect_list != &cptr->local_list)
	RELOC_VAR(cptr->rect_list);
    RELOC_USING(st_path, &cptr->path, sizeof(gx_path));
}
RELOC_PTRS_END
#undef cptr

/* GC procedures for gx_device_clip */
#define cptr ((gx_device_clip *)vptr)
private ENUM_PTRS_BEGIN(device_clip_enum_ptrs)
{
    if (index < st_clip_list_max_ptrs + 1)
	return ENUM_USING(st_clip_list, &cptr->list,
			  sizeof(gx_clip_list), index - 1);
    return ENUM_USING(st_device_forward, vptr,
		      sizeof(gx_device_forward),
		      index - (st_clip_list_max_ptrs + 1));
}
case 0:
ENUM_RETURN((cptr->current == &cptr->list.single ? NULL :
	     (void *)cptr->current));
ENUM_PTRS_END
private RELOC_PTRS_BEGIN(device_clip_reloc_ptrs)
{
    if (cptr->current == &cptr->list.single)
	cptr->current = &((gx_device_clip *)RELOC_OBJ(vptr))->list.single;
    else
	RELOC_PTR(gx_device_clip, current);
    RELOC_USING(st_clip_list, &cptr->list, sizeof(gx_clip_list));
    RELOC_USING(st_device_forward, vptr, sizeof(gx_device_forward));
}
RELOC_PTRS_END
#undef cptr

/* Define an empty clip list. */
private const gx_clip_list clip_list_empty =
{
    {0, 0, min_int, max_int, 0, 0},
    0, 0, 0, 0			/*false */
};

/* Debugging */

#ifdef DEBUG
/* Validate a clipping path. */
bool				/* only exported for gxacpath.c */
clip_list_validate(const gx_clip_list * clp)
{
    if (clp->count <= 1)
	return (clp->head == 0 && clp->tail == 0 &&
		clp->single.next == 0 && clp->single.prev == 0);
    else {
	const gx_clip_rect *prev = clp->head;
	const gx_clip_rect *ptr;
	bool ok = true;

	while ((ptr = prev->next) != 0) {
	    if (ptr->ymin > ptr->ymax || ptr->xmin > ptr->xmax ||
		!(ptr->ymin >= prev->ymax ||
		  (ptr->ymin == prev->ymin &&
		   ptr->ymax == prev->ymax &&
		   ptr->xmin >= prev->xmax)) ||
		ptr->prev != prev
		) {
		clip_rect_print('q', "WRONG:", ptr);
		ok = false;
	    }
	    prev = ptr;
	}
	return ok && prev == clp->tail;
    }
}
#endif

/* ------ Clipping path memory management ------ */

private rc_free_proc(rc_free_cpath_list);
private rc_free_proc(rc_free_cpath_list_local);

/* Initialize those parts of the contents of a clip path that aren't */
/* part of the path. */
private void
cpath_init_rectangle(gx_clip_path * pcpath, gs_fixed_rect * pbox)
{
    gx_clip_list_from_rectangle(&pcpath->rect_list->list, pbox);
    pcpath->inner_box = *pbox;
    pcpath->path_valid = false;
    pcpath->path.bbox = *pbox;
    gx_cpath_set_outer_box(pcpath);
    pcpath->id = gs_next_ids(1);	/* path changed => change id */
}
private void
cpath_init_own_contents(gx_clip_path * pcpath)
{				/* We could make null_rect static, but then it couldn't be const. */
    gs_fixed_rect null_rect;

    null_rect.p.x = null_rect.p.y = null_rect.q.x = null_rect.q.y = 0;
    cpath_init_rectangle(pcpath, &null_rect);
}
private void
cpath_share_own_contents(gx_clip_path * pcpath, const gx_clip_path * shared)
{
    pcpath->inner_box = shared->inner_box;
    pcpath->path_valid = shared->path_valid;
    pcpath->outer_box = shared->outer_box;
    pcpath->id = shared->id;
}

/* Allocate only the segments of a clipping path on the heap. */
private int
cpath_alloc_list(gx_clip_rect_list ** prlist, gs_memory_t * mem,
		 client_name_t cname)
{
    rc_alloc_struct_1(*prlist, gx_clip_rect_list, &st_clip_rect_list, mem,
		      return_error(gs_error_VMerror), cname);
    (*prlist)->rc.free = rc_free_cpath_list;
    return 0;
}
int
gx_cpath_init_contained_shared(gx_clip_path * pcpath,
	const gx_clip_path * shared, gs_memory_t * mem, client_name_t cname)
{
    if (shared) {
	if (shared->path.segments == &shared->path.local_segments) {
	    lprintf1("Attempt to share (local) segments of clip path 0x%lx!\n",
		     (ulong) shared);
	    return_error(gs_error_Fatal);
	}
	*pcpath = *shared;
	pcpath->path.memory = mem;
	pcpath->path.allocation = path_allocated_contained;
	rc_increment(pcpath->path.segments);
	rc_increment(pcpath->rect_list);
    } else {
	int code = cpath_alloc_list(&pcpath->rect_list, mem, cname);

	if (code < 0)
	    return code;
	code = gx_path_alloc_contained(&pcpath->path, mem, cname);
	if (code < 0) {
	    gs_free_object(mem, pcpath->rect_list, cname);
	    pcpath->rect_list = 0;
	    return code;
	}
	cpath_init_own_contents(pcpath);
    }
    return 0;
}
#define gx_cpath_alloc_contents(pcpath, shared, mem, cname)\
  gx_cpath_init_contained_shared(pcpath, shared, mem, cname)

/* Allocate all of a clipping path on the heap. */
gx_clip_path *
gx_cpath_alloc_shared(const gx_clip_path * shared, gs_memory_t * mem,
		      client_name_t cname)
{
    gx_clip_path *pcpath =
    gs_alloc_struct(mem, gx_clip_path, &st_clip_path, cname);
    int code;

    if (pcpath == 0)
	return 0;
    code = gx_cpath_alloc_contents(pcpath, shared, mem, cname);
    if (code < 0) {
	gs_free_object(mem, pcpath, cname);
	return 0;
    }
    pcpath->path.allocation = path_allocated_on_heap;
    return pcpath;
}

/* Initialize a stack-allocated clipping path. */
int
gx_cpath_init_local_shared(gx_clip_path * pcpath, const gx_clip_path * shared,
			   gs_memory_t * mem)
{
    if (shared) {
	if (shared->path.segments == &shared->path.local_segments) {
	    lprintf1("Attempt to share (local) segments of clip path 0x%lx!\n",
		     (ulong) shared);
	    return_error(gs_error_Fatal);
	}
	pcpath->path = shared->path;
	pcpath->path.allocation = path_allocated_on_stack;
	rc_increment(pcpath->path.segments);
	pcpath->rect_list = shared->rect_list;
	rc_increment(pcpath->rect_list);
	cpath_share_own_contents(pcpath, shared);
    } else {
	gx_path_init_local(&pcpath->path, mem);
	rc_init_free(&pcpath->local_list, mem, 1, rc_free_cpath_list_local);
	pcpath->rect_list = &pcpath->local_list;
	cpath_init_own_contents(pcpath);
    }
    return 0;
}

/* Unshare a clipping path. */
int
gx_cpath_unshare(gx_clip_path * pcpath)
{
    int code = gx_path_unshare(&pcpath->path);
    gx_clip_rect_list *rlist = pcpath->rect_list;

    if (code < 0)
	return code;
    if (rlist->rc.ref_count > 1) {
	int code = cpath_alloc_list(&pcpath->rect_list, pcpath->path.memory,
				    "gx_cpath_unshare");

	if (code < 0)
	    return code;
	/* Copy the rectangle list. */
/**************** NYI ****************/
	rc_decrement(rlist, "gx_cpath_unshare");
    }
    return code;
}

/* Free a clipping path. */
void
gx_cpath_free(gx_clip_path * pcpath, client_name_t cname)
{
    rc_decrement(pcpath->rect_list, cname);
    /* Clean up pointers for GC. */
    pcpath->rect_list = 0;
    {
	gx_path_allocation_t alloc = pcpath->path.allocation;

	if (alloc == path_allocated_on_heap) {
	    pcpath->path.allocation = path_allocated_contained;
	    gx_path_free(&pcpath->path, cname);
	    gs_free_object(pcpath->path.memory, pcpath, cname);
	} else
	    gx_path_free(&pcpath->path, cname);
    }
}

/* Assign a clipping path, preserving the source. */
int
gx_cpath_assign_preserve(gx_clip_path * pcpto, gx_clip_path * pcpfrom)
{
    int code = gx_path_assign_preserve(&pcpto->path, &pcpfrom->path);
    gx_clip_rect_list *fromlist = pcpfrom->rect_list;
    gx_clip_rect_list *tolist = pcpto->rect_list;
    gx_path path;

    if (code < 0)
	return 0;
    if (fromlist == &pcpfrom->local_list) {
	/* We can't use pcpfrom's list object. */
	if (tolist == &pcpto->local_list || tolist->rc.ref_count > 1) {
	    /* We can't use pcpto's list either.  Allocate a new one. */
	    int code = cpath_alloc_list(&tolist, tolist->rc.memory,
					"gx_cpath_assign");

	    if (code < 0)
		return code;
	    rc_decrement(pcpto->rect_list, "gx_cpath_assign");
	} else {
	    /* Use pcpto's list object. */
	    rc_free_cpath_list_local(tolist->rc.memory, tolist,
				     "gx_cpath_assign");
	}
	tolist->list = fromlist->list;
	pcpfrom->rect_list = tolist;
	rc_increment(tolist);
    } else {
	/* We can use pcpfrom's list object. */
	rc_increment(fromlist);
	rc_decrement(pcpto->rect_list, "gx_cpath_assign");
    }
    path = pcpto->path, *pcpto = *pcpfrom, pcpto->path = path;
    return 0;
}

/* Assign a clipping path, releasing the source. */
int
gx_cpath_assign_free(gx_clip_path * pcpto, gx_clip_path * pcpfrom)
{				/* For right now, just do assign + free. */
    int code = gx_cpath_assign_preserve(pcpto, pcpfrom);

    if (code < 0)
	return 0;
    gx_cpath_free(pcpfrom, "gx_cpath_assign_free");
    return 0;
}

/* Free the clipping list when its reference count goes to zero. */
private void
rc_free_cpath_list_local(gs_memory_t * mem, void *vrlist,
			 client_name_t cname)
{
    gx_clip_rect_list *rlist = (gx_clip_rect_list *) vrlist;

    gx_clip_list_free(&rlist->list, mem);
}
private void
rc_free_cpath_list(gs_memory_t * mem, void *vrlist, client_name_t cname)
{
    rc_free_cpath_list_local(mem, vrlist, cname);
    gs_free_object(mem, vrlist, cname);
}

/* ------ Clipping path accessing ------ */

/* Return the path of a clipping path. */
int
gx_cpath_to_path(gx_clip_path * pcpath, gx_path * ppath)
{
    if (!pcpath->path_valid) {
	/* Synthesize a path. */
	gs_cpath_enum cenum;
	gs_fixed_point pts[3];
	gx_path rpath;
	int code;

	gx_path_init_local(&rpath, pcpath->path.memory);
	gx_cpath_enum_init(&cenum, pcpath);
	while ((code = gx_cpath_enum_next(&cenum, pts)) != 0) {
	    switch (code) {
		case gs_pe_moveto:
		    code = gx_path_add_point(&rpath, pts[0].x, pts[0].y);
		    break;
		case gs_pe_lineto:
		    code = gx_path_add_line_notes(&rpath, pts[0].x, pts[0].y,
					       gx_cpath_enum_notes(&cenum));
		    break;
		case gs_pe_curveto:
		    code = gx_path_add_curve_notes(&rpath, pts[0].x, pts[0].y,
						   pts[1].x, pts[1].y,
						   pts[2].x, pts[2].y,
					       gx_cpath_enum_notes(&cenum));
		    break;
		case gs_pe_closepath:
		    code = gx_path_close_subpath_notes(&rpath,
					       gx_cpath_enum_notes(&cenum));
		    break;
		default:
		    if (code >= 0)
			code = gs_note_error(gs_error_unregistered);
	    }
	    if (code < 0)
		break;
	}
	if (code >= 0)
	    code = gx_path_assign_free(&pcpath->path, &rpath);
	if (code < 0) {
	    gx_path_free(&rpath, "gx_cpath_to_path error");
	    return code;
	}
	pcpath->path_valid = true;
    }
    return gx_path_assign_preserve(ppath, &pcpath->path);
}

/* Return the inner and outer check rectangles for a clipping path. */
/* Return true iff the path is a rectangle. */
/* Note that these must return something strange if we are using */
/* outside clipping. */
bool
gx_cpath_inner_box(const gx_clip_path * pcpath, gs_fixed_rect * pbox)
{
    if (gx_cpath_is_outside(pcpath)) {
	pbox->p.x = pbox->p.y = pbox->q.x = pbox->q.y = 0;
	return false;
    } else {
	*pbox = pcpath->inner_box;
	return clip_list_is_rectangle(gx_cpath_list(pcpath));
    }
}
bool
gx_cpath_outer_box(const gx_clip_path * pcpath, gs_fixed_rect * pbox)
{
    if (gx_cpath_is_outside(pcpath)) {
	pbox->p.x = pbox->p.y = min_fixed;
	pbox->q.x = pbox->q.y = max_fixed;
	return false;
    } else {
	*pbox = pcpath->outer_box;
	return clip_list_is_rectangle(gx_cpath_list(pcpath));
    }
}

/* Test if a clipping path includes a rectangle. */
/* The rectangle need not be oriented correctly, i.e. x0 > x1 is OK. */
bool
gx_cpath_includes_rectangle(register const gx_clip_path * pcpath,
			    fixed x0, fixed y0, fixed x1, fixed y1)
{
    return
	(x0 <= x1 ?
	 (pcpath->inner_box.p.x <= x0 && x1 <= pcpath->inner_box.q.x) :
	 (pcpath->inner_box.p.x <= x1 && x0 <= pcpath->inner_box.q.x)) &&
	(y0 <= y1 ?
	 (pcpath->inner_box.p.y <= y0 && y1 <= pcpath->inner_box.q.y) :
	 (pcpath->inner_box.p.y <= y1 && y0 <= pcpath->inner_box.q.y));
}

/* Set the current outsideness of a clipping path. */
int
gx_cpath_set_outside(gx_clip_path * pcpath, bool outside)
{
    if (outside != gx_cpath_list(pcpath)->outside) {
	pcpath->id = gs_next_ids(1);	/* path changed => change id */
	gx_cpath_list(pcpath)->outside = outside;
    }
    return 0;
}

/* Return the current outsideness of a clipping path. */
bool
gx_cpath_is_outside(const gx_clip_path * pcpath)
{
    return gx_cpath_list(pcpath)->outside;
}

/* Set the outer clipping box to the path bounding box, */
/* expanded to pixel boundaries. */
void
gx_cpath_set_outer_box(gx_clip_path * pcpath)
{
    pcpath->outer_box.p.x = fixed_floor(pcpath->path.bbox.p.x);
    pcpath->outer_box.p.y = fixed_floor(pcpath->path.bbox.p.y);
    pcpath->outer_box.q.x = fixed_ceiling(pcpath->path.bbox.q.x);
    pcpath->outer_box.q.y = fixed_ceiling(pcpath->path.bbox.q.y);
}

/* ------ Clipping path setting ------ */

/* Create a rectangular clipping path. */
/* The supplied rectangle may not be oriented correctly, */
/* but it will be oriented correctly upon return. */
private int
cpath_set_rectangle(gx_clip_path * pcpath, gs_fixed_rect * pbox)
{
    gx_clip_rect_list *rlist = pcpath->rect_list;

    if (rlist->rc.ref_count <= 1)
	gx_clip_list_free(&rlist->list, rlist->rc.memory);
    else {
	int code = cpath_alloc_list(&pcpath->rect_list, pcpath->path.memory,
				    "gx_cpath_from_rectangle");

	if (code < 0)
	    return code;
	rc_decrement(rlist, "gx_cpath_from_rectangle");
	rlist = pcpath->rect_list;
    }
    cpath_init_rectangle(pcpath, pbox);
    return 0;
}
int
gx_cpath_from_rectangle(gx_clip_path * pcpath, gs_fixed_rect * pbox)
{
    int code = gx_path_new(&pcpath->path);

    if (code < 0)
	return code;
    return cpath_set_rectangle(pcpath, pbox);
}
int
gx_cpath_reset(gx_clip_path * pcpath)
{
    gs_fixed_rect null_rect;

    null_rect.p.x = null_rect.p.y = null_rect.q.x = null_rect.q.y = 0;
    return gx_cpath_from_rectangle(pcpath, &null_rect);
}

/* Intersect a new clipping path with an old one. */
/* Flatten the new path first (in a copy) if necessary. */
int
gx_cpath_clip(gs_state *pgs, gx_clip_path *pcpath, gx_path *ppath_orig,
	      int rule)
{
    gx_path fpath;
    gx_path *ppath = ppath_orig;
    gs_fixed_rect old_box, new_box;
    int code;

    /* Flatten the path if necessary. */
    if (gx_path_has_curves_inline(ppath)) {
	gx_path_init_local(&fpath, gs_state_memory(pgs));
	code = gx_path_add_flattened_accurate(ppath, &fpath,
					      gs_currentflat(pgs),
					      gs_currentaccuratecurves(pgs));
	if (code < 0)
	    return code;
	ppath = &fpath;
    }
    /**************** SHOULD CHANGE THIS TO KEEP PATH ****************/
    if (gx_cpath_inner_box(pcpath, &old_box) &&
	((code = gx_path_is_rectangle(ppath, &new_box)) ||
	 gx_path_is_void(ppath))
	) {
	bool changed = false;
	bool outside = gx_cpath_is_outside(pcpath);

	if (!code) {
	    /* The new path is void. */
	    if (gx_path_current_point(ppath, &new_box.p) < 0) {
		/* Use the user space origin (arbitrarily). */
		gs_point origin;

		gs_transform(pgs, 0.0, 0.0, &origin);
		new_box.p.x = float2fixed(origin.x);
		new_box.p.y = float2fixed(origin.y);
		changed = true;
	    }
	    new_box.q = new_box.p;
	} else {
	    /* Intersect the two rectangles if necessary. */
	    if (old_box.p.x > new_box.p.x)
		new_box.p.x = old_box.p.x, changed = true;
	    if (old_box.p.y > new_box.p.y)
		new_box.p.y = old_box.p.y, changed = true;
	    if (old_box.q.x < new_box.q.x)
		new_box.q.x = old_box.q.x, changed = true;
	    if (old_box.q.y < new_box.q.y)
		new_box.q.y = old_box.q.y, changed = true;
	    /* Check for a degenerate rectangle. */
	    if (new_box.q.x < new_box.p.x)
		new_box.q.x = new_box.p.x;
	    if (new_box.q.y < new_box.p.y)
		new_box.q.y = new_box.p.y;
	}
	if (changed) {
	    /* Defer constructing the path. */
	    gx_path_new(&pcpath->path);
	    pcpath->path_valid = false;
	} else {
	    gx_path_assign_preserve(&pcpath->path, ppath);
	    pcpath->path_valid = true;
	}
	ppath->bbox = new_box;
	cpath_set_rectangle(pcpath, &new_box);
	pcpath->rect_list->list.outside = outside;
    } else {
	/* Existing clip path is not a rectangle.  Intersect the slow way. */
	bool path_valid =
	    gx_cpath_inner_box(pcpath, &old_box) &&
	    gx_path_bbox(ppath, &new_box) >= 0 &&
	    gx_cpath_includes_rectangle(pcpath,
					new_box.p.x, new_box.p.y,
					new_box.q.x, new_box.q.y);

	code = gx_cpath_intersect_slow(pgs, pcpath, ppath, rule);
	if (code >= 0 && path_valid) {
	    gx_path_assign_preserve(&pcpath->path, ppath_orig);
	    pcpath->path_valid = true;
	}
    }
    if (ppath != ppath_orig)
	gx_path_free(ppath, "gx_cpath_clip");
    return code;
}

/* Scale a clipping path by a power of 2. */
int
gx_cpath_scale_exp2(gx_clip_path * pcpath, int log2_scale_x, int log2_scale_y)
{
    int code =
	gx_path_scale_exp2(&pcpath->path, log2_scale_x, log2_scale_y);
    gx_clip_list *list = gx_cpath_list(pcpath);
    gx_clip_rect *pr;

    if (code < 0)
	return code;
    /* Scale the fixed entries. */
    gx_rect_scale_exp2(&pcpath->inner_box, log2_scale_x, log2_scale_y);
    gx_rect_scale_exp2(&pcpath->outer_box, log2_scale_x, log2_scale_y);
    /* Scale the clipping list. */
    pr = list->head;
    if (pr == 0)
	pr = &list->single;
    for (; pr != 0; pr = pr->next)
	if (pr != list->head && pr != list->tail) {
#define scale_v(v, s)\
  if ( pr->v != min_int && pr->v != max_int )\
    pr->v = (s >= 0 ? pr->v << s : pr->v >> -s)
	    scale_v(xmin, log2_scale_x);
	    scale_v(xmax, log2_scale_x);
	    scale_v(ymin, log2_scale_y);
	    scale_v(ymax, log2_scale_y);
#undef scale_v
	}
    pcpath->id = gs_next_ids(1);	/* path changed => change id */
    return 0;
}

/* ------ Clipping list routines ------ */

/* Initialize a clip list. */
void
gx_clip_list_init(gx_clip_list * clp)
{
    *clp = clip_list_empty;
}

/* Initialize a clip list to a rectangle. */
/* The supplied rectangle may not be oriented correctly, */
/* but it will be oriented correctly upon return. */
private void
gx_clip_list_from_rectangle(register gx_clip_list * clp,
			    register gs_fixed_rect * rp)
{
    gx_clip_list_init(clp);
    if (rp->p.x > rp->q.x) {
	fixed t = rp->p.x;

	rp->p.x = rp->q.x;
	rp->q.x = t;
    }
    if (rp->p.y > rp->q.y) {
	fixed t = rp->p.y;

	rp->p.y = rp->q.y;
	rp->q.y = t;
    }
    clp->single.xmin = fixed2int_var(rp->p.x);
    clp->single.ymin = fixed2int_var(rp->p.y);
    clp->single.xmax = fixed2int_var_ceiling(rp->q.x);
    clp->single.ymax = fixed2int_var_ceiling(rp->q.y);
    clp->count = 1;
    clp->outside = false;
}

/* Start enumerating a clipping path. */
int
gx_cpath_enum_init(gs_cpath_enum * penum, gx_clip_path * pcpath)
{
    if ((penum->using_path = pcpath->path_valid)) {
	gx_path_enum_init(&penum->path_enum, &pcpath->path);
	penum->rp = penum->visit = 0;
    } else {
	gx_path empty_path;
	gx_clip_list *clp = gx_cpath_list(pcpath);
	gx_clip_rect *head = (clp->count <= 1 ? &clp->single : clp->head);
	gx_clip_rect *rp;

	/* Initialize the pointers in the path_enum properly. */
	gx_path_init_local(&empty_path, pcpath->path.memory);
	gx_path_enum_init(&penum->path_enum, &empty_path);
	penum->visit = head;
	for (rp = head; rp != 0; rp = rp->next)
	    rp->to_visit =
		(rp->xmin < rp->xmax && rp->ymin < rp->ymax ?
		 visit_left | visit_right : 0);
	penum->rp = 0;		/* scan will initialize */
	penum->any_rectangles = false;
	penum->state = cpe_scan;
	penum->have_line = false;
    }
    return 0;
}

/* Enumerate the next segment of a clipping path. */
/* In general, this produces a path made up of zillions of tiny lines. */
int
gx_cpath_enum_next(gs_cpath_enum * penum, gs_fixed_point pts[3])
{
    if (penum->using_path)
	return gx_path_enum_next(&penum->path_enum, pts);
#define set_pt(xi, yi)\
  (pts[0].x = int2fixed(xi), pts[0].y = int2fixed(yi))
#define set_line(xi, yi)\
  (penum->line_end.x = (xi), penum->line_end.y = (yi), penum->have_line = true)
    if (penum->have_line) {
	set_pt(penum->line_end.x, penum->line_end.y);
	penum->have_line = false;
	return gs_pe_lineto;
    } {
	gx_clip_rect *visit = penum->visit;
	gx_clip_rect *rp = penum->rp;
	cpe_visit_t first_visit = penum->first_visit;
	cpe_state_t state = penum->state;
	gx_clip_rect *look;
	int code;

	switch (state) {

	    case cpe_scan:
		/* Look for the start of an edge to trace. */
		for (; visit != 0; visit = visit->next) {
		    if (visit->to_visit & visit_left) {
			set_pt(visit->xmin, visit->ymin);
			first_visit = visit_left;
			state = cpe_left;
		    } else if (visit->to_visit & visit_right) {
			set_pt(visit->xmax, visit->ymax);
			first_visit = visit_right;
			state = cpe_right;
		    } else
			continue;
		    rp = visit;
		    code = gs_pe_moveto;
		    penum->any_rectangles = true;
		    goto out;
		}
		/* We've enumerated all the edges. */
		state = cpe_done;
		if (!penum->any_rectangles) {
		    /* We didn't have any rectangles. */
		    set_pt(fixed_0, fixed_0);
		    code = gs_pe_moveto;
		    break;
		}
		/* falls through */

	    case cpe_done:
		/* All done. */
		code = 0;
		break;

/* We can't use the BEGIN ... END hack here: we need to be able to break. */
#define return_line(px, py)\
  set_pt(px, py); code = gs_pe_lineto; break

	    case cpe_left:

	      left:		/* Trace upward along a left edge. */
		/* We're at the lower left corner of rp. */
		rp->to_visit &= ~visit_left;
		/* Look for an adjacent rectangle above rp. */
		for (look = rp;
		     (look = look->next) != 0 &&
		     (look->ymin == rp->ymin ||
		      (look->ymin == rp->ymax && look->xmax <= rp->xmin));
		    );
		/* Now we know look->ymin >= rp->ymax. */
		if (look == 0 || look->ymin > rp->ymax ||
		    look->xmin >= rp->xmax
		    ) {		/* No adjacent rectangle, switch directions. */
		    state =
			(rp == visit && first_visit == visit_right ? cpe_close :
			 (set_line(rp->xmax, rp->ymax), cpe_right));
		    return_line(rp->xmin, rp->ymax);
		}
		/* We found an adjacent rectangle. */
		/* See if it also adjoins a rectangle to the left of rp. */
		{
		    gx_clip_rect *prev = rp->prev;
		    gx_clip_rect *cur = rp;

		    if (prev != 0 && prev->ymax == rp->ymax &&
			look->xmin < prev->xmax
			) {	/* There's an adjoining rectangle as well. */
			/* Switch directions. */
			rp = prev;
			state =
			    (rp == visit && first_visit == visit_right ? cpe_close :
			     (set_line(prev->xmax, prev->ymax), cpe_right));
			return_line(cur->xmin, cur->ymax);
		    }
		    rp = look;
		    if (rp == visit && first_visit == visit_left)
			state = cpe_close;
		    else if (rp->xmin == cur->xmin)
			goto left;
		    else
			set_line(rp->xmin, rp->ymin);
		    return_line(cur->xmin, cur->ymax);
		}

	    case cpe_right:

	      right:		/* Trace downward along a right edge. */
		/* We're at the upper right corner of rp. */
		rp->to_visit &= ~visit_right;
		/* Look for an adjacent rectangle below rp. */
		for (look = rp;
		     (look = look->prev) != 0 &&
		     (look->ymax == rp->ymax ||
		      (look->ymax == rp->ymin && look->xmin >= rp->xmax));
		    );
		/* Now we know look->ymax <= rp->ymin. */
		if (look == 0 || look->ymax < rp->ymin ||
		    look->xmax <= rp->xmin
		    ) {		/* No adjacent rectangle, switch directions. */
		    state =
			(rp == visit && first_visit == visit_left ? cpe_close :
			 (set_line(rp->xmin, rp->ymin), cpe_left));
		    return_line(rp->xmax, rp->ymin);
		}
		/* We found an adjacent rectangle. */
		/* See if it also adjoins a rectangle to the right of rp. */
		{
		    gx_clip_rect *next = rp->next;
		    gx_clip_rect *cur = rp;

		    if (next != 0 && next->ymin == rp->ymin &&
			look->xmax > next->xmin
			) {	/* There's an adjoining rectangle as well. */
			/* Switch directions. */
			rp = next;
			state =
			    (rp == visit && first_visit == visit_left ? cpe_close :
			     (set_line(next->xmin, next->ymin), cpe_left));
			return_line(cur->xmax, cur->ymin);
		    }
		    rp = look;
		    if (rp == visit && first_visit == visit_right)
			state = cpe_close;
		    else if (rp->xmax == cur->xmax)
			goto right;
		    else
			set_line(rp->xmax, rp->ymax);
		    return_line(cur->xmax, cur->ymin);
		}

#undef return_line

	    case cpe_close:
		/* We've gone all the way around an edge. */
		code = gs_pe_closepath;
		state = cpe_scan;
		break;

	    default:
		return_error(gs_error_unknownerror);
	}

      out:			/* Store the state before exiting. */
	penum->visit = visit;
	penum->rp = rp;
	penum->first_visit = first_visit;
	penum->state = state;
	return code;
    }
#undef set_pt
#undef set_line
}
segment_notes
gx_cpath_enum_notes(const gs_cpath_enum * penum)
{
    return sn_none;
}

/* Free a clip list. */
void
gx_clip_list_free(gx_clip_list * clp, gs_memory_t * mem)
{
    gx_clip_rect *rp = clp->tail;

    while (rp != 0) {
	gx_clip_rect *prev = rp->prev;

	gs_free_object(mem, rp, "gx_clip_list_free");
	rp = prev;
    }
    gx_clip_list_init(clp);
}

/* ------ Debugging printout ------ */

#ifdef DEBUG

/* Print a clipping path */
void
gx_cpath_print(const gx_clip_path * pcpath)
{
    const gx_clip_rect *pr;
    const gx_clip_list *list = gx_cpath_list(pcpath);

    if (pcpath->path_valid)
	gx_path_print(&pcpath->path);
    else
	dlputs("   (path not valid)\n");
    dlprintf4("   inner_box=(%g,%g),(%g,%g)\n",
	      fixed2float(pcpath->inner_box.p.x),
	      fixed2float(pcpath->inner_box.p.y),
	      fixed2float(pcpath->inner_box.q.x),
	      fixed2float(pcpath->inner_box.q.y));
    dlprintf4("     outer_box=(%g,%g),(%g,%g)",
	      fixed2float(pcpath->outer_box.p.x),
	      fixed2float(pcpath->outer_box.p.y),
	      fixed2float(pcpath->outer_box.q.x),
	      fixed2float(pcpath->outer_box.q.y));
    dprintf4("     rule=%d outside=%d count=%d list.refct=%ld\n",
	     pcpath->rule, list->outside, list->count,
	     pcpath->rect_list->rc.ref_count);
    switch (list->count) {
	case 0:
	    pr = 0;
	    break;
	case 1:
	    pr = &list->single;
	    break;
	default:
	    pr = list->head;
    }
    for (; pr != 0; pr = pr->next)
	dlprintf4("   rect: (%d,%d),(%d,%d)\n",
		  pr->xmin, pr->ymin, pr->xmax, pr->ymax);
}

#endif /* DEBUG */
