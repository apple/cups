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

/*$Id: gsstate.c,v 1.2 2000/03/08 23:14:47 mike Exp $ */
/* Miscellaneous graphics state operators for Ghostscript library */
#include "gx.h"
#include "memory_.h"
#include "gserrors.h"
#include "gsstruct.h"
#include "gsutil.h"		/* for gs_next_ids */
#include "gzstate.h"
#include "gxcspace.h"		/* here for gscolor2.h */
#include "gsalpha.h"
#include "gscolor2.h"
#include "gscoord.h"		/* for gs_initmatrix */
#include "gscie.h"
#include "gxcmap.h"
#include "gxdevice.h"
#include "gxpcache.h"
#include "gzht.h"
#include "gzline.h"
#include "gspath.h"
#include "gzpath.h"
#include "gzcpath.h"

/* Imported values */
/* The following should include a 'const', but for some reason */
/* the Watcom compiler won't accept it, even though it happily accepts */
/* the same construct everywhere else. */
extern /*const */ gx_color_map_procs *const cmap_procs_default;

/* Forward references */
private gs_state *gstate_alloc(P3(gs_memory_t *, client_name_t,
				  const gs_state *));
private gs_state *gstate_clone(P4(gs_state *, gs_memory_t *, client_name_t,
				  gs_state_copy_reason_t));
private void gstate_free_contents(P1(gs_state *));
private int gstate_copy(P4(gs_state *, const gs_state *,
			   gs_state_copy_reason_t, client_name_t));

/*
 * Graphics state storage management is complicated.  There are many
 * different classes of storage associated with a graphics state:
 *
 * (1) The gstate object itself.  This includes some objects physically
 *      embedded within the gstate object, but because of garbage collection
 *      requirements, there are no embedded objects that can be
 *      referenced by non-transient pointers.  We assume that the gstate
 *      stack "owns" its gstates and that we can free the top gstate when
 *      doing a restore.
 *
 * (2) Objects that are referenced directly by the gstate and whose lifetime
 *      is independent of the gstate.  These are garbage collected, not
 *      reference counted, so we don't need to do anything special with them
 *      when manipulating gstates.  Currently this includes:
 *              font, device
 *
 * (3) Objects that are referenced directly by the gstate, may be shared
 *      among gstates, and should disappear when no gstates reference them.
 *      These fall into two groups:
 *
 *   (3a) Objects that are logically connected to individual gstates.
 *      We use reference counting to manage these.  Currently these are:
 *              halftone, dev_ht, cie_render, black_generation,
 *              undercolor_removal, set_transfer.*, cie_joint_caches
 *      effective_transfer.* may point to some of the same objects as
 *      set_transfer.*, but don't contribute to the reference count.
 *      Similarly, dev_color may point to the dev_ht object.  For
 *      simplicity, we initialize all of these pointers to 0 and then
 *      allocate the object itself when needed.
 *
 *   (3b) Objects whose lifetimes are associated with something else.
 *      Currently these are:
 *              ht_cache, which is associated with the entire gstate
 *                stack, is allocated with the very first graphics state,
 *                and currently is never freed;
 *              pattern_cache, which is associated with the entire
 *                stack, is allocated when first needed, and currently
 *                is never freed;
 *              view_clip, which is associated with the current
 *                save level (effectively, with the gstate sub-stack
 *                back to the save) and is managed specially.
 *
 * (4) Objects that are referenced directly by exactly one gstate and that
 *      are not referenced (except transiently) from any other object.
 *      These fall into two groups:
 *
 *   (4b) Objects allocated individually, for the given reason:
 *              line_params.dash.pattern (variable-length),
 *              color_space, path, clip_path, effective_clip.path,
 *              ccolor, dev_color
 *                  (may be referenced from image enumerators or elsewhere)
 *
 *   (4b) The "client data" for a gstate.  For the interpreter, this is
 *      the refs associated with the gstate, such as the screen procedures.
 *      Client-supplied procedures manage client data.
 *
 * (5) Objects referenced indirectly from gstate objects of category (4),
 *      including objects that may also be referenced directly by the gstate.
 *      The individual routines that manipulate these are responsible
 *      for doing the right kind of reference counting or whatever.
 *      Currently:
 *              path, clip_path, and (if different from both clip_path
 *                and view_clip) effective_clip.path require
 *                gx_path_assign/free, which uses a reference count;
 *              color_space and ccolor require cs_adjust_color/cspace_count
 *                or cs_adjust_counts, which use a reference count;
 *              dev_color has no references to storage that it owns.
 *      We count on garbage collection or restore to deallocate
 *        sub-objects of halftone.
 *
 * Note that when after a gsave, the existing gstate references the related
 * objects that we allocate at the same time, and the newly allocated gstate
 * references the old related objects.  Similarly, during a grestore, we
 * free the related objects referenced by the current gstate, but after the
 * grestore, we free the saved gstate, not the current one.  However, when
 * we allocate gstates off-stack, the newly allocated gstate does reference
 * the newly allocated component objects.  Note also that setgstate /
 * currentgstate may produce gstates in which different allocators own
 * different sub-objects; this is OK, because restore guarantees that there
 * won't be any dangling pointers (as long as we don't allow pointers from
 * global gstates to local objects).
 */

/*
 * Enumerate the pointers in a graphics state, other than the ones in the
 * imager state, and device, which must be handled specially.
 */
#define gs_state_do_ptrs(m)\
  m(0,saved) m(1,path) m(2,clip_path) m(3,view_clip) m(4,effective_clip_path)\
  m(5,color_space) m(6,ccolor) m(7,dev_color)\
  m(8,font) m(9,root_font) m(10,show_gstate) /*m(---,device)*/\
  m(11,client_data)
#define gs_state_num_ptrs 12

/*
 * Define these elements of the graphics state that are allocated
 * individually for each state, except for line_params.dash.pattern.
 * Note that effective_clip_shared is not on the list.
 */
typedef struct gs_state_parts_s {
    gx_path *path;
    gx_clip_path *clip_path;
    gx_clip_path *effective_clip_path;
    gs_color_space *color_space;
    gs_client_color *ccolor;
    gx_device_color *dev_color;
} gs_state_parts;

#define GSTATE_ASSIGN_PARTS(pto, pfrom)\
  ((pto)->path = (pfrom)->path, (pto)->clip_path = (pfrom)->clip_path,\
   (pto)->effective_clip_path = (pfrom)->effective_clip_path,\
   (pto)->color_space = (pfrom)->color_space,\
   (pto)->ccolor = (pfrom)->ccolor, (pto)->dev_color = (pfrom)->dev_color)

/* GC descriptors */
private_st_line_params();
private_st_imager_state();
private_st_imager_state_shared();
private_st_gs_state();

/* GC procedures for gs_imager_state */
#define pis ((gs_imager_state *)vptr)
private 
ENUM_PTRS_BEGIN(imager_state_enum_ptrs) ENUM_SUPER(gs_imager_state, st_line_params, line_params, st_imager_state_num_ptrs - st_line_params_num_ptrs);

ENUM_PTR(0, gs_imager_state, shared);
#define e1(i,elt) ENUM_PTR(i+1,gs_imager_state,elt);
gs_cr_state_do_ptrs(e1)
#undef e1
ENUM_PTRS_END
private RELOC_PTRS_BEGIN(imager_state_reloc_ptrs)
{
    RELOC_SUPER(gs_imager_state, st_line_params, line_params);
    RELOC_PTR(gs_imager_state, shared);
#define r1(i,elt) RELOC_PTR(gs_imager_state,elt);
    gs_cr_state_do_ptrs(r1)
#undef r1
} RELOC_PTRS_END
#undef pis

/* GC procedures for gs_state */
#define gsvptr ((gs_state *)vptr)
private ENUM_PTRS_BEGIN(gs_state_enum_ptrs) ENUM_PREFIX(st_imager_state, gs_state_num_ptrs + 1);
#define e1(i,elt) ENUM_PTR(i,gs_state,elt);
gs_state_do_ptrs(e1)
case gs_state_num_ptrs:	/* handle device specially */
ENUM_RETURN(gx_device_enum_ptr(gsvptr->device));
#undef e1
ENUM_PTRS_END
private RELOC_PTRS_BEGIN(gs_state_reloc_ptrs)
{
    RELOC_PREFIX(st_imager_state);
    {
#define r1(i,elt) RELOC_PTR(gs_state,elt);
	gs_state_do_ptrs(r1)
#undef r1
	    gsvptr->device = gx_device_reloc_ptr(gsvptr->device, gcst);
    }
}
RELOC_PTRS_END
#undef gsvptr

/* Copy client data, using the copy_for procedure if available, */
/* the copy procedure otherwise. */
private int
gstate_copy_client_data(gs_state * pgs, void *dto, void *dfrom,
			gs_state_copy_reason_t reason)
{
    return (pgs->client_procs.copy_for != 0 ?
	    (*pgs->client_procs.copy_for) (dto, dfrom, reason) :
	    (*pgs->client_procs.copy) (dto, dfrom));
}

/* ------ Operations on the entire graphics state ------ */

/* Initialize an imager state, other than the parts covered by */
/* gs_imager_state_initial. */
/* The halftone, dev_ht, and ht_cache elements are not set or used. */
private float
null_transfer(floatp gray, const gx_transfer_map * pmap)
{
    return gray;
}
private void
rc_free_imager_shared(gs_memory_t * mem, void *data, client_name_t cname)
{
    gs_imager_state_shared_t * const shared =
	(gs_imager_state_shared_t *)data;

    if (shared->cs_DeviceCMYK) {
	gs_cspace_release(shared->cs_DeviceCMYK);
	gs_free_object(mem, shared->cs_DeviceCMYK, "shared DeviceCMYK");
    }
    if (shared->cs_DeviceRGB) {
	gs_cspace_release(shared->cs_DeviceRGB);
	gs_free_object(mem, shared->cs_DeviceRGB, "shared DeviceRGB");
    }
    if (shared->cs_DeviceGray) {
	gs_cspace_release(shared->cs_DeviceGray);
	gs_free_object(mem, shared->cs_DeviceGray, "shared DeviceGray");
    }
    rc_free_struct_only(mem, data, cname);
}

int
gs_imager_state_initialize(gs_imager_state * pis, gs_memory_t * mem)
{
    pis->memory = mem;
    /* Preallocate color spaces. */
    {
	int code;
	gs_imager_state_shared_t *shared;

	rc_alloc_struct_1(shared, gs_imager_state_shared_t,
			  &st_imager_state_shared, mem,
			  return_error(gs_error_VMerror),
			  "gs_imager_state_init(shared)");
	shared->cs_DeviceGray = shared->cs_DeviceRGB =
	    shared->cs_DeviceCMYK = 0; /* in case we bail out */
	shared->rc.free = rc_free_imager_shared;
	if ((code = gs_cspace_build_DeviceGray(&shared->cs_DeviceGray, mem)) < 0 ||
	    (code = gs_cspace_build_DeviceRGB(&shared->cs_DeviceRGB, mem)) < 0 ||
	    (code = gs_cspace_build_DeviceCMYK(&shared->cs_DeviceCMYK, mem)) < 0
	    ) {
	    rc_free_imager_shared(mem, shared, "gs_imager_state_init(shared)");
	    return code;
	}
	pis->shared = shared;
    }
    /* Skip halftone */
    {
	int i;

	for (i = 0; i < gs_color_select_count; ++i)
	    pis->screen_phase[i].x = pis->screen_phase[i].y = 0;
    }
    /* Skip dev_ht */
    /* Skip ht_cache */
    pis->cie_render = 0;
    pis->black_generation = 0;
    pis->undercolor_removal = 0;
    /* Allocate an initial transfer map. */
    rc_alloc_struct_n(pis->set_transfer.colored.gray,
		      gx_transfer_map, &st_transfer_map,
		      mem, return_error(gs_error_VMerror),
		      "gs_imager_state_init(transfer)", 4);
    pis->set_transfer.colored.gray->proc = null_transfer;
    pis->set_transfer.colored.gray->id = gs_next_ids(1);
    pis->set_transfer.colored.gray->values[0] = frac_0;
    pis->set_transfer.colored.red =
	pis->set_transfer.colored.green =
	pis->set_transfer.colored.blue =
	pis->set_transfer.colored.gray;
    pis->effective_transfer = pis->set_transfer;
    pis->cie_joint_caches = 0;
    pis->cmap_procs = cmap_procs_default;
    pis->pattern_cache = 0;
    return 0;
}

/* Release an imager state. */
void
gs_imager_state_release(gs_imager_state * pis)
{
    const char *const cname = "gs_imager_state_release";

#define RCDECR(element)\
    rc_decrement(pis->element, cname)

    RCDECR(cie_joint_caches);
    RCDECR(set_transfer.colored.gray);
    RCDECR(set_transfer.colored.blue);
    RCDECR(set_transfer.colored.green);
    RCDECR(set_transfer.colored.red);
    RCDECR(undercolor_removal);
    RCDECR(black_generation);
    RCDECR(cie_render);
    RCDECR(shared);
#undef RCDECR
}

/* Allocate and initialize a graphics state. */
gs_state *
gs_state_alloc(gs_memory_t * mem)
{
    gs_state *pgs = gstate_alloc(mem, "gs_state_alloc", NULL);

    if (pgs == 0)
	return 0;
    {
	static const gs_imager_state gstate_initial =
	{
	    gs_imager_state_initial(1.0)
	};

	*(gs_imager_state *) pgs = gstate_initial;
    }
    /*
     * Just enough of the state is initialized at this point
     * that it's OK to call gs_state_free if an allocation fails.
     */
    rc_alloc_struct_1(pgs->halftone, gs_halftone, &st_halftone, mem,
		      goto fail, "gs_state_alloc(halftone)");
    pgs->saved = 0;

    /* Initialize the color rendering state. */

    pgs->halftone->type = ht_type_none;
    pgs->dev_ht = 0;
    pgs->ht_cache = gx_ht_alloc_cache(mem,
				      gx_ht_cache_default_tiles(),
				      gx_ht_cache_default_bits());
    gs_imager_state_initialize((gs_imager_state *) pgs, mem);
    pgs->client_data = 0;

    /* Initialize other things not covered by initgraphics */

    pgs->path = gx_path_alloc(mem, "gs_state_alloc(path)");
    pgs->clip_path = gx_cpath_alloc(mem, "gs_state_alloc(clip_path)");
    pgs->view_clip = gx_cpath_alloc(mem, "gs_state_alloc(view_clip)");
    pgs->view_clip->rule = 0;	/* no clipping */
    pgs->effective_clip_id = pgs->clip_path->id;
    pgs->effective_view_clip_id = gs_no_id;
    pgs->effective_clip_path = pgs->clip_path;
    pgs->effective_clip_shared = true;
    /* Initialize things so that gx_remap_color won't crash. */
    pgs->color_space->type = &gs_color_space_type_DeviceGray;
    gx_set_device_color_1(pgs);
    pgs->overprint = false;
    pgs->device = 0;		/* setting device adjusts refcts */
    gs_nulldevice(pgs);
    gs_setalpha(pgs, 1.0);
    gs_settransfer(pgs, null_transfer);
    gs_setflat(pgs, 1.0);
    gs_setfilladjust(pgs, 0.25, 0.25);
    gs_setlimitclamp(pgs, false);
    gs_setstrokeadjust(pgs, true);
    pgs->font = 0;		/* Not right, but acceptable until the */
    /* PostScript code does the first setfont. */
    pgs->root_font = 0;		/* ditto */
    pgs->in_cachedevice = 0;
    pgs->in_charpath = (gs_char_path_mode) 0;
    pgs->show_gstate = 0;
    pgs->level = 0;
    pgs->client_data = 0;
    if (gs_initgraphics(pgs) < 0) {
	/* Something went very wrong */
	return 0;
    }
    return pgs;
  fail:
    gs_state_free(pgs);
    return 0;
}

/* Set the client data in a graphics state. */
/* This should only be done to a newly created state. */
void
gs_state_set_client(gs_state * pgs, void *pdata,
		    const gs_state_client_procs * pprocs)
{
    pgs->client_data = pdata;
    pgs->client_procs = *pprocs;
}

/* Get the client data from a graphics state. */
#undef gs_state_client_data	/* gzstate.h makes this a macro */
void *
gs_state_client_data(const gs_state * pgs)
{
    return pgs->client_data;
}

/* Free a graphics state. */
int
gs_state_free(gs_state * pgs)
{
    gstate_free_contents(pgs);
    gs_free_object(pgs->memory, pgs, "gs_state_free");
    return 0;
}

/* Save the graphics state. */
int
gs_gsave(gs_state * pgs)
{
    gs_state *pnew = gstate_clone(pgs, pgs->memory, "gs_gsave",
				  copy_for_gsave);

    if (pnew == 0)
	return_error(gs_error_VMerror);
    pgs->saved = pnew;
    if (pgs->show_gstate == pgs)
	pgs->show_gstate = pnew->show_gstate = pnew;
    pgs->level++;
    if_debug2('g', "[g]gsave -> 0x%lx, level = %d\n",
	      (ulong) pnew, pgs->level);
    return 0;
}

/* Save the graphics state for a 'save'. */
/* We cut the stack below the new gstate, and return the old one. */
/* In addition to an ordinary gsave, we create a new view clip path. */
int
gs_gsave_for_save(gs_state * pgs, gs_state ** psaved)
{
    int code;
    gx_clip_path *old_cpath = pgs->view_clip;
    gx_clip_path *new_cpath;

    if (old_cpath) {
	new_cpath =
	    gx_cpath_alloc_shared(old_cpath, pgs->memory,
				  "gs_gsave_for_save(view_clip)");
	if (new_cpath == 0)
	    return_error(gs_error_VMerror);
    } else {
	new_cpath = 0;
    }
    code = gs_gsave(pgs);
    if (code < 0) {
	if (new_cpath)
	    gx_cpath_free(new_cpath, "gs_gsave_for_save(view_clip)");
	return code;
    }
    if (pgs->effective_clip_path == pgs->view_clip)
	pgs->effective_clip_path = new_cpath;
    pgs->view_clip = new_cpath;
    /* Cut the stack so we can't grestore past here. */
    *psaved = pgs->saved;
    pgs->saved = 0;
    return code;
}

/* Restore the graphics state. */
int
gs_grestore(gs_state * pgs)
{
    gs_state *saved = pgs->saved;
    void *pdata = pgs->client_data;
    void *sdata;

    if_debug2('g', "[g]grestore 0x%lx, level was %d\n",
	      (ulong) saved, pgs->level);
    if (!saved)			/* shouldn't happen */
	return gs_gsave(pgs);
    sdata = saved->client_data;
    if (saved->pattern_cache == 0)
	saved->pattern_cache = pgs->pattern_cache;
    /* Swap back the client data pointers. */
    pgs->client_data = sdata;
    saved->client_data = pdata;
    if (pdata != 0 && sdata != 0)
	gstate_copy_client_data(pgs, pdata, sdata, copy_for_grestore);
    gstate_free_contents(pgs);
    *pgs = *saved;
    if (pgs->show_gstate == saved)
	pgs->show_gstate = pgs;
    gs_free_object(pgs->memory, saved, "gs_grestore");
    if (pgs->saved)
	return 0;
    return gs_gsave(pgs);
}

/* Restore the graphics state for a 'restore', splicing the old stack */
/* back on.  Note that we actually do a grestoreall + 2 grestores. */
int
gs_grestoreall_for_restore(gs_state * pgs, gs_state * saved)
{
    int code;

    while (pgs->saved->saved) {
	code = gs_grestore(pgs);
	if (code < 0)
	    return code;
    }
    /* Make sure we don't leave dangling pointers in the caches. */
    gx_ht_clear_cache(pgs->ht_cache);
    if (pgs->pattern_cache)
	(*pgs->pattern_cache->free_all) (pgs->pattern_cache);
    pgs->saved->saved = saved;
    code = gs_grestore(pgs);
    if (code < 0)
	return code;
    if (pgs->view_clip) {
	gx_cpath_free(pgs->view_clip, "gs_grestoreall_for_restore");
	pgs->view_clip = 0;
    }
    return gs_grestore(pgs);
}


/* Restore to the bottommost graphics state (at this save level). */
int
gs_grestoreall(gs_state * pgs)
{
    int code;

    if (!pgs->saved)		/* shouldn't happen */
	return gs_gsave(pgs);
    while (pgs->saved->saved) {
	code = gs_grestore(pgs);
	if (code < 0)
	    return code;
    }
    code = gs_grestore(pgs);
    if (code < 0)
	return code;
    return code;
}

/* Allocate and return a new graphics state. */
gs_state *
gs_gstate(gs_state * pgs)
{
    return gs_state_copy(pgs, pgs->memory);
}
gs_state *
gs_state_copy(gs_state * pgs, gs_memory_t * mem)
{
    gs_state *pnew;

    /* Prevent 'capturing' the view clip path. */
    gx_clip_path *view_clip = pgs->view_clip;

    pgs->view_clip = 0;
    pnew = gstate_clone(pgs, mem, "gs_gstate", copy_for_gstate);
    pgs->view_clip = view_clip;
    if (pnew == 0)
	return 0;
    pnew->saved = 0;
    /*
     * Prevent dangling references from the show_gstate pointer.  If
     * this context is its own show_gstate, set the pointer in the clone
     * to point to the clone; otherwise, set the pointer in the clone to
     * 0, and let gs_setgstate fix it up.
     */
    pnew->show_gstate =
	(pgs->show_gstate == pgs ? pnew : 0);
    return pnew;
}

/* Copy one previously allocated graphics state to another. */
int
gs_copygstate(gs_state * pto, const gs_state * pfrom)
{
    return gstate_copy(pto, pfrom, copy_for_copygstate, "gs_copygstate");
}

/* Copy the current graphics state to a previously allocated one. */
int
gs_currentgstate(gs_state * pto, const gs_state * pgs)
{
    int code =
    gstate_copy(pto, pgs, copy_for_currentgstate, "gs_currentgstate");

    if (code >= 0)
	pto->view_clip = 0;
    return code;
}

/* Restore the current graphics state from a previously allocated one. */
int
gs_setgstate(gs_state * pgs, const gs_state * pfrom)
{
    /*
     * The implementation is the same as currentgstate,
     * except we must preserve the saved pointer, the level,
     * the view clip, and possibly the show_gstate.
     */
    gs_state *saved_show = pgs->show_gstate;
    int level = pgs->level;
    gx_clip_path *view_clip = pgs->view_clip;
    int code;

    pgs->view_clip = 0;		/* prevent refcount decrementing */
    code = gstate_copy(pgs, pfrom, copy_for_setgstate, "gs_setgstate");
    if (code < 0)
	return code;
    pgs->level = level;
    pgs->view_clip = view_clip;
    pgs->show_gstate =
	(pgs->show_gstate == pfrom ? pgs : saved_show);
    return 0;
}

/* Get the allocator pointer of a graphics state. */
/* This is provided only for the interpreter */
/* and for color space implementation. */
gs_memory_t *
gs_state_memory(const gs_state * pgs)
{
    return pgs->memory;
}

/* Get the saved pointer of the graphics state. */
/* This is provided only for Level 2 grestore. */
gs_state *
gs_state_saved(const gs_state * pgs)
{
    return pgs->saved;
}

/* Swap the saved pointer of the graphics state. */
/* This is provided only for save/restore. */
gs_state *
gs_state_swap_saved(gs_state * pgs, gs_state * new_saved)
{
    gs_state *saved = pgs->saved;

    pgs->saved = new_saved;
    return saved;
}

/* Swap the memory pointer of the graphics state. */
/* This is provided only for the interpreter. */
gs_memory_t *
gs_state_swap_memory(gs_state * pgs, gs_memory_t * mem)
{
    gs_memory_t *memory = pgs->memory;

    pgs->memory = mem;
    return memory;
}

/* ------ Operations on components ------ */

/* Reset most of the graphics state */
int
gs_initgraphics(gs_state * pgs)
{
    int code;

    gs_initmatrix(pgs);
    if ((code = gs_newpath(pgs)) < 0 ||
	(code = gs_initclip(pgs)) < 0 ||
	(code = gs_setlinewidth(pgs, 1.0)) < 0 ||
	(code = gs_setlinecap(pgs, gs_cap_butt)) < 0 ||
	(code = gs_setlinejoin(pgs, gs_join_miter)) < 0 ||
	(code = gs_setdash(pgs, (float *)0, 0, 0.0)) < 0 ||
	(gs_setdashadapt(pgs, false),
	 (code = gs_setdotlength(pgs, 0.0, false))) < 0 ||
	(code = gs_setgray(pgs, 0.0)) < 0 ||
	(code = gs_setmiterlimit(pgs, 10.0)) < 0
	)
	return code;
    gs_init_rop(pgs);
    return 0;
}

/* setfilladjust */
int
gs_setfilladjust(gs_state * pgs, floatp adjust_x, floatp adjust_y)
{
#define CLAMP_TO_HALF(v)\
    ((v) <= 0 ? fixed_0 : (v) >= 0.5 ? fixed_half : float2fixed(v));

    pgs->fill_adjust.x = CLAMP_TO_HALF(adjust_x);
    pgs->fill_adjust.y = CLAMP_TO_HALF(adjust_y);
    return 0;
#undef CLAMP_TO_HALF
}

/* currentfilladjust */
int
gs_currentfilladjust(const gs_state * pgs, gs_point * adjust)
{
    adjust->x = fixed2float(pgs->fill_adjust.x);
    adjust->y = fixed2float(pgs->fill_adjust.y);
    return 0;
}

/* setlimitclamp */
void
gs_setlimitclamp(gs_state * pgs, bool clamp)
{
    pgs->clamp_coordinates = clamp;
}

/* currentlimitclamp */
bool
gs_currentlimitclamp(const gs_state * pgs)
{
    return pgs->clamp_coordinates;
}

/* ------ Internal routines ------ */

/* Free the privately allocated parts of a gstate. */
private void
gstate_free_parts(const gs_state * parts, gs_memory_t * mem, client_name_t cname)
{
    gs_free_object(mem, parts->dev_color, cname);
    gs_free_object(mem, parts->ccolor, cname);
    gs_free_object(mem, parts->color_space, cname);
    if (!parts->effective_clip_shared)
	gx_cpath_free(parts->effective_clip_path, cname);
    gx_cpath_free(parts->clip_path, cname);
    gx_path_free(parts->path, cname);
}

/* Allocate the privately allocated parts of a gstate. */
private int
gstate_alloc_parts(gs_state * parts, const gs_state * shared,
		   gs_memory_t * mem, client_name_t cname)
{
    parts->path =
	(shared ?
	 gx_path_alloc_shared(shared->path, mem,
			      "gstate_alloc_parts(path)") :
	 gx_path_alloc(mem, "gstate_alloc_parts(path)"));
    parts->clip_path =
	(shared ?
	 gx_cpath_alloc_shared(shared->clip_path, mem,
			       "gstate_alloc_parts(clip_path)") :
	 gx_cpath_alloc(mem, "gstate_alloc_parts(clip_path)"));
    if (!shared || shared->effective_clip_shared) {
	parts->effective_clip_path = parts->clip_path;
	parts->effective_clip_shared = true;
    } else {
	parts->effective_clip_path =
	    gx_cpath_alloc_shared(shared->effective_clip_path, mem,
				  "gstate_alloc_parts(effective_clip_path)");
	parts->effective_clip_shared = false;
    }
    parts->color_space =
	gs_alloc_struct(mem, gs_color_space, &st_color_space, cname);
    parts->ccolor =
	gs_alloc_struct(mem, gs_client_color, &st_client_color, cname);
    parts->dev_color =
	gs_alloc_struct(mem, gx_device_color, &st_device_color, cname);
    if (parts->path == 0 || parts->clip_path == 0 ||
	parts->effective_clip_path == 0 ||
	parts->color_space == 0 || parts->ccolor == 0 ||
	parts->dev_color == 0
	) {
	gstate_free_parts(parts, mem, cname);
	return_error(gs_error_VMerror);
    }
    return 0;
}

/*
 * Allocate a gstate and its contents.
 * If pfrom is not NULL, the path, clip_path, and (if distinct from both
 * clip_path and view_clip) effective_clip_path share the segments of
 * pfrom's corresponding path(s).
 */
private gs_state *
gstate_alloc(gs_memory_t * mem, client_name_t cname, const gs_state * pfrom)
{
    gs_state *pgs =
    gs_alloc_struct(mem, gs_state, &st_gs_state, cname);

    if (pgs == 0)
	return 0;
    if (gstate_alloc_parts(pgs, pfrom, mem, cname) < 0) {
	gs_free_object(mem, pgs, cname);
	return 0;
    }
    pgs->memory = mem;
    return pgs;
}

/* Copy the dash pattern from one gstate to another. */
private int
gstate_copy_dash(gs_state * pto, const gs_state * pfrom)
{
    return gs_setdash(pto, pfrom->line_params.dash.pattern,
		      pfrom->line_params.dash.pattern_size,
		      pfrom->line_params.dash.offset);
}

/* Clone an existing graphics state. */
/* Return 0 if the allocation fails. */
/* If reason is for_gsave, the clone refers to the old contents, */
/* and we switch the old state to refer to the new contents. */
private gs_state *
gstate_clone(gs_state * pfrom, gs_memory_t * mem, client_name_t cname,
	     gs_state_copy_reason_t reason)
{
    gs_state *pgs = gstate_alloc(mem, cname, pfrom);
    gs_state_parts parts;

    if (pgs == 0)
	return 0;
    GSTATE_ASSIGN_PARTS(&parts, pgs);
    *pgs = *pfrom;
    /* Copy the dash pattern if necessary. */
    if (pgs->line_params.dash.pattern) {
	int code;

	pgs->line_params.dash.pattern = 0;	/* force allocation */
	code = gstate_copy_dash(pgs, pfrom);
	if (code < 0)
	    goto fail;
    }
    if (pgs->client_data != 0) {
	void *pdata = pgs->client_data = (*pgs->client_procs.alloc) (mem);

	if (pdata == 0 ||
	 gstate_copy_client_data(pgs, pdata, pfrom->client_data, reason) < 0
	    )
	    goto fail;
    }
    rc_increment(pgs->set_transfer.colored.gray);
    rc_increment(pgs->set_transfer.colored.red);
    rc_increment(pgs->set_transfer.colored.green);
    rc_increment(pgs->set_transfer.colored.blue);
    rc_increment(pgs->halftone);
    rc_increment(pgs->dev_ht);
    rc_increment(pgs->cie_render);
    rc_increment(pgs->black_generation);
    rc_increment(pgs->undercolor_removal);
    rc_increment(pgs->cie_joint_caches);
    rc_increment(pgs->device);
    *parts.color_space = *pfrom->color_space;
    *parts.ccolor = *pfrom->ccolor;
    *parts.dev_color = *pfrom->dev_color;
    if (reason == copy_for_gsave) {
	float *dfrom = pfrom->line_params.dash.pattern;
	float *dto = pgs->line_params.dash.pattern;

	GSTATE_ASSIGN_PARTS(pfrom, &parts);
	pgs->line_params.dash.pattern = dfrom;
	pfrom->line_params.dash.pattern = dto;
    } else {
	GSTATE_ASSIGN_PARTS(pgs, &parts);
    }
    cs_adjust_counts(pgs, 1);
    return pgs;
  fail:
    gs_free_object(mem, pgs->line_params.dash.pattern, cname);
    GSTATE_ASSIGN_PARTS(pgs, &parts);
    gstate_free_parts(pgs, mem, cname);
    gs_free_object(mem, pgs, cname);
    return 0;
}

/* Release the composite parts of a graphics state, */
/* but not the state itself. */
private void
gstate_free_contents(gs_state * pgs)
{
    gs_memory_t *mem = pgs->memory;
    gx_device_halftone *pdht = pgs->dev_ht;
    const char *const cname = "gstate_free_contents";

#define RCDECR(element)\
    rc_decrement(pgs->element, cname)

    RCDECR(device);
    RCDECR(cie_joint_caches);
    RCDECR(set_transfer.colored.gray);
    RCDECR(set_transfer.colored.blue);
    RCDECR(set_transfer.colored.green);
    RCDECR(set_transfer.colored.red);
    RCDECR(undercolor_removal);
    RCDECR(black_generation);
    RCDECR(cie_render);
    if (pdht != 0 && pdht->rc.ref_count == 1) {
	/* Make sure we don't leave dangling pointers in the cache. */
	gx_ht_cache *pcache = pgs->ht_cache;

	if (pcache->order.bits == pdht->order.bits ||
	    pcache->order.levels == pdht->order.levels
	    )
	    gx_ht_clear_cache(pcache);
	gx_device_halftone_release(pdht, pdht->rc.memory);
    }
    RCDECR(dev_ht);
    RCDECR(halftone);
    cs_adjust_counts(pgs, -1);
    if (pgs->client_data != 0)
	(*pgs->client_procs.free) (pgs->client_data, mem);
    gs_free_object(mem, pgs->line_params.dash.pattern, cname);
    gstate_free_parts(pgs, mem, cname);
#undef RCDECR
}

/* Copy one gstate to another. */
private int
gstate_copy(gs_state * pto, const gs_state * pfrom,
	    gs_state_copy_reason_t reason, client_name_t cname)
{
    gs_state_parts parts;

    GSTATE_ASSIGN_PARTS(&parts, pto);
    /* Copy the dash pattern if necessary. */
    if (pfrom->line_params.dash.pattern || pto->line_params.dash.pattern) {
	int code = gstate_copy_dash(pto, pfrom);

	if (code < 0)
	    return code;
    }
    /*
     * It's OK to decrement the counts before incrementing them,
     * because anything that is going to survive has a count of
     * at least 2 (pto and somewhere else) initially.
     * Handle references from contents.
     */
    cs_adjust_counts(pto, -1);
    gx_path_assign_preserve(pto->path, pfrom->path);
    gx_cpath_assign_preserve(pto->clip_path, pfrom->clip_path);
    /*
     * effective_clip_shared will be copied, but we need to do the
     * right thing with effective_clip_path.
     */
    if (pfrom->effective_clip_shared) {
	/*
	 * pfrom->effective_clip_path is either pfrom->view_clip or
	 * pfrom->clip_path.
	 */
	parts.effective_clip_path =
	    (pfrom->effective_clip_path == pfrom->view_clip ?
	     pto->view_clip : parts.clip_path);
    } else
	gx_cpath_assign_preserve(pto->effective_clip_path,
				 pfrom->effective_clip_path);
    *parts.color_space = *pfrom->color_space;
    *parts.ccolor = *pfrom->ccolor;
    *parts.dev_color = *pfrom->dev_color;
    cs_adjust_counts(pto, 1);
    /* Handle references from gstate object. */
#define RCCOPY(element)\
    rc_pre_assign(pto->element, pfrom->element, cname)
    RCCOPY(device);
    RCCOPY(cie_joint_caches);
    RCCOPY(set_transfer.colored.gray);
    RCCOPY(set_transfer.colored.blue);
    RCCOPY(set_transfer.colored.green);
    RCCOPY(set_transfer.colored.red);
    RCCOPY(undercolor_removal);
    RCCOPY(black_generation);
    RCCOPY(cie_render);
    RCCOPY(dev_ht);
    RCCOPY(halftone);
    {
	struct gx_pattern_cache_s *pcache = pto->pattern_cache;
	void *pdata = pto->client_data;
	gs_memory_t *mem = pto->memory;
	gs_state *saved = pto->saved;
	float *pattern = pto->line_params.dash.pattern;

	*pto = *pfrom;
	pto->client_data = pdata;
	pto->memory = mem;
	pto->saved = saved;
	pto->line_params.dash.pattern = pattern;
	if (pto->pattern_cache == 0)
	    pto->pattern_cache = pcache;
	if (pfrom->client_data != 0) {
	    /* We need to break 'const' here. */
	    gstate_copy_client_data((gs_state *) pfrom, pdata,
				    pfrom->client_data, reason);
	}
    }
    GSTATE_ASSIGN_PARTS(pto, &parts);
#undef RCCOPY
    pto->show_gstate =
	(pfrom->show_gstate == pfrom ? pto : 0);
    return 0;
}
