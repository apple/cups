/* Copyright (C) 1989, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gsstate.c */
/* Miscellaneous graphics state operators for Ghostscript library */
#include "gx.h"
#include "memory_.h"
#include "gserrors.h"
#include "gsstruct.h"
#include "gsutil.h"		/* for gs_next_ids */
#include "gzstate.h"
#include "gxcspace.h"		/* here for gscolor2.h */
#include "gscolor2.h"
#include "gscoord.h"		/* for gs_initmatrix */
#include "gscie.h"
#include "gxcmap.h"
#include "gxdevice.h"
#include "gzht.h"
#include "gzline.h"
#include "gspath.h"
#include "gzpath.h"
#include "gzcpath.h"

/* Imported values */
/* The following should include a 'const', but for some reason */
/* the Watcom compiler won't accept it, even though it happily accepts */
/* the same construct everywhere else. */
extern /*const*/ gx_color_map_procs *cmap_procs_default;

/* Forward references */
private gs_state *gstate_alloc(P2(gs_memory_t *, client_name_t));
private void gstate_set_contents(P2(gs_state *, gs_state_contents *));
private gs_state *gstate_clone(P3(gs_state *, client_name_t, bool));
private void gstate_free_contents(P1(gs_state *));
private void gstate_share_paths(P1(const gs_state *));
private void gstate_copy(P3(gs_state *, const gs_state *, client_name_t));

/*
 * Graphics state storage management is complicated.  There are many
 * different classes of storage associated with a graphics state:
 *
 * (1) The gstate object itself.  This includes some objects physically
 *	embedded within the gstate object, but because of garbage collection
 *	requirements, there are no embedded objects that and can be
 *	referenced by non-transient pointers.  We assume that the gstate
 *	stack "owns" its gstates and that we can free the top gstate when
 *	doing a restore.
 *
 * (2) Objects that are referenced directly by the gstate and whose lifetime
 *	is independent of the gstate.  These are garbage collected, not
 *	reference counted, so we don't need to do anything special with them
 *	when manipulating gstates.  Currently this includes:
 *		font, device
 *
 * (3) Objects that are referenced directly by the gstate, may be shared
 *	among gstates, and should disappear when no gstates reference them.
 *	We use reference counting to manage these.  Currently these are:
 *		cie_render, black_generation, undercolor_removal,
 *		set_transfer.*, cie_joint_caches
 *	effective_transfer.* may point to some of the same objects as
 *	set_transfer.*, but don't contribute to the reference count.
 *
 * (4) Objects that are referenced directly by exactly one gstate and that
 *	are not referenced (except transiently) from any other object.
 *	We lump most of these together into a single gs_state_contents
 *	object in order to reduce the load on the allocator for gsave and
 *	grestore.  Each gs_gstate_contents is referenced by exactly one
 *	gstate, and nowhere else.  We reference the individual members
 *	through pointers, just as though they were allocated individually,
 *	so that we wouldn't have to keep remembering to put & in front of
 *	references to its components, which would otherwise have to be
 *	embedded in the gstate object.  Again, we do not allow non-transient
 *	references to the objects embedded in this object, aside from
 *	a single pointer from the gstate object to each embedded component.
 *	Currently these are:
 *		path, clip_path, line_params (Not In Contents Object),
 *		halftone, color_space (NICO), ccolor, dev_color,
 *		dev_ht (NICO)
 *	color_space and dev_ht are special cases, since they are referenced
 *	both directly and also indirectly (from image enumerators and
 *	dev_color respectively).  We simply allocate them as separate
 *	objects.
 *
 * (5) Objects referenced indirectly from gstate objects of category (4),
 *	including objects that may also be referenced directly by the gstate.
 *	The individual routines that manipulate these are responsible
 *	for doing the right kind of reference counting or whatever.
 *	Currently:
 *		path and clip_path require gx_path_share/release,
 *		  which use a 1-bit reference count;
 *		color_space and ccolor require cs_adjust_color/cspace_count
 *		  or cs_adjust_counts, which use a full reference count;
 *		We count on garbage collection or restore to deallocate
 *		  sub-objects of line_params, halftone, and dev_ht;
 *		dev_color has no references to storage that it owns.
 *
 * (6) The "client data" for a gstate.  For the interpreter, this is
 *	the refs associated with the gstate, such as the screen procedures.
 *	Again, we maintain the invariant that there is exactly one gstate
 *	that refers to a given client data object.  Client-supplied
 *	procedures manage client data.
 *
 * This situation is unnecessarily complicated.  We should get rid of
 * the contents object and use reference counting to manage individually
 * the objects it currently contains.  We should use full reference
 * counting for paths, and use the freeing procedure to release the
 * individual path elements.  However, making these changes runs a large
 * risk of introducing hard-to-find bugs, so we don't plan to make them
 * in the foreseeable future.
 *
 * Note that for client data and gstate-related composite objects, it is not
 * necessarily the case that when we allocate a gstate, it will reference
 * the related object that we allocate at the same time; for example,
 * when we do a gsave, the old contents go with the new gstate object
 * and vice versa.  The same is true of freeing.  */

/* The structure for allocating (most of) the contents of a gstate */
/* all at once.  The typedef is in gzstate.c. */
struct gs_state_contents_s {
	gx_path path;
	gx_clip_path clip_path;
	gs_halftone halftone;
	gs_client_color ccolor;
	gx_device_color dev_color;
};

/* Enumerate the pointers in a graphics state, other than the ones */
/* that point to the gs_state_contents. */
#define gs_state_do_ptrs(m)\
  m(0,saved) m(1,ht_cache) m(2,contents)\
  m(3,cie_render) m(4,black_generation) m(5,undercolor_removal)\
  m(6,set_transfer.colored.red) m(7,set_transfer.colored.green)\
  m(8,set_transfer.colored.blue) m(9,set_transfer.colored.gray)\
  m(10,effective_transfer.colored.red) m(11,effective_transfer.colored.green)\
  m(12,effective_transfer.colored.blue) m(13,effective_transfer.colored.gray)\
  m(14,cie_joint_caches) m(15,pattern_cache) m(16,font) m(17,root_font)\
  m(18,show_gstate) /*m(---,device)*/ m(19,client_data)\
  /****** OUT OF ORDER: ******/ m(20,dev_ht) m(21,color_space)
#define gs_state_num_ptrs 22
/* Enumerate the pointers to the gs_state_contents. */
#define gs_state_do_contents_ptrs(m)\
  m(0,path) m(1,clip_path) m(2,halftone) m(3,ccolor) m(4,dev_color)

/* GC descriptors */
private_st_line_params();
private_st_imager_state();
private_st_gs_state();
/* Components of the graphics state */
gs_private_st_composite(st_gs_state_contents, gs_state_contents,
  "gs_state_contents", state_contents_enum_ptrs, state_contents_reloc_ptrs);
public_st_transfer_map();

/* GC procedures for gs_state */
#define gsvptr ((gs_state *)vptr)
private ENUM_PTRS_BEGIN(gs_state_enum_ptrs) ENUM_PREFIX(st_imager_state, gs_state_num_ptrs + 1);
#define e1(i,elt) ENUM_PTR(i,gs_state,elt);
	gs_state_do_ptrs(e1)
	case gs_state_num_ptrs:		/* handle device specially */
	  *pep = gx_device_enum_ptr(gsvptr->device);
	  break;
#undef e1
ENUM_PTRS_END
private RELOC_PTRS_BEGIN(gs_state_reloc_ptrs) {
	RELOC_PREFIX(st_imager_state);
	{ /* Save the contents pointer before relocation. */
	  byte *cont = (byte *)gsvptr->contents;
	  long reloc;
#define r1(i,elt) RELOC_PTR(gs_state,elt);
	  gs_state_do_ptrs(r1)
#undef r1
	  gsvptr->device = gx_device_reloc_ptr(gsvptr->device, gcst);
	  /* Now relocate the pointers into the contents. */
	  reloc = cont - (byte *)gsvptr->contents;
#define r1(i,elt)\
  gsvptr->elt = (void *)((byte *)gsvptr->elt - reloc);
	  gs_state_do_contents_ptrs(r1)
#undef r1
	}
} RELOC_PTRS_END
#undef gsvptr

/* GC procedures for gs_state_contents */
#define cptr ((gs_state_contents *)vptr)
private ENUM_PTRS_BEGIN_PROC(state_contents_enum_ptrs) {
	gs_ptr_type_t ret;
#define next_comp(np, st, e)\
  if ( index < np ) { ret = (*st.enum_ptrs)(&cptr->e, sizeof(cptr->e), index, pep); goto rx; }\
  index -= np
#define last_comp(np, st, e)\
  return (*st.enum_ptrs)(&cptr->e, sizeof(cptr->e), index, pep)
	next_comp(st_path_max_ptrs, st_path, path);
	next_comp(st_clip_path_max_ptrs, st_clip_path, clip_path);
	next_comp(st_halftone_max_ptrs, st_halftone, halftone);
	next_comp(st_client_color_max_ptrs, st_client_color, ccolor);
	last_comp(st_device_color_max_ptrs, st_device_color, dev_color);
#undef next_comp
#undef last_comp
rx:	if ( ret == 0 )
	{	/* A component ran out of pointers early. */
		/* Just return a null so we can keep going. */
		*pep = 0;
		return ptr_struct_type;
	}
	return ret;
ENUM_PTRS_END_PROC }
private RELOC_PTRS_BEGIN(state_contents_reloc_ptrs) {
	(*st_path.reloc_ptrs)(&cptr->path, sizeof(gx_path), gcst);
	(*st_clip_path.reloc_ptrs)(&cptr->clip_path, sizeof(gx_clip_path), gcst);
	(*st_halftone.reloc_ptrs)(&cptr->halftone, sizeof(gs_halftone), gcst);
	(*st_client_color.reloc_ptrs)(&cptr->ccolor, sizeof(gs_client_color), gcst);
	(*st_device_color.reloc_ptrs)(&cptr->dev_color, sizeof(gx_device_color), gcst);
} RELOC_PTRS_END
#undef cptr

/* ------ Operations on the entire graphics state ------ */

/* Allocate and initialize a graphics state. */
private float
null_transfer(floatp gray, const gx_transfer_map *pmap)
{	return gray;
}
gs_state *
gs_state_alloc(gs_memory_t *mem)
{	register gs_state *pgs = gstate_alloc(mem, "gs_state_alloc");
	if ( pgs == 0 )
	  return 0;
	pgs->saved = 0;
	gstate_set_contents(pgs, pgs->contents);

	/* Initialize all reference-counted pointers in the gs_state. */

	pgs->cie_render = 0;
	pgs->black_generation = 0;
	pgs->undercolor_removal = 0;
	/* Allocate an initial transfer map. */
	rc_alloc_struct_n(pgs->set_transfer.colored.gray,
			  gx_transfer_map, &st_transfer_map,
			  mem, return 0, "gs_state_alloc", 4);
	pgs->set_transfer.colored.gray->proc = null_transfer;
	pgs->set_transfer.colored.gray->id = gs_next_ids(1);
	pgs->set_transfer.colored.gray->values[0] = frac_0;
	pgs->set_transfer.colored.red =
	  pgs->set_transfer.colored.green =
	  pgs->set_transfer.colored.blue =
	  pgs->set_transfer.colored.gray;
	pgs->effective_transfer = pgs->set_transfer;
	pgs->cie_joint_caches = 0;
	pgs->client_data = 0;

	/* Initialize other things not covered by initgraphics */

	gx_path_init(pgs->path, mem);
	gx_cpath_init(pgs->clip_path, mem);
	pgs->ht_cache = gx_ht_alloc_cache(mem,
					  gx_ht_cache_default_tiles(),
					  gx_ht_cache_default_bits());
	{	/*
		 * It would be nice if we could use statically initialized
		 * structures for the levels and bits arrays, but that would
		 * confuse the GC.
		 */
		uint *levels =
		  (uint *)gs_alloc_bytes(mem, sizeof(uint),
					 "gs_state_alloc(ht levels)");
		gx_ht_bit *bits =
		  (gx_ht_bit *)gs_alloc_bytes(mem, sizeof(gx_ht_bit),
					      "gs_state_alloc(ht bits)");
		gx_device_halftone *pdht = pgs->dev_ht;

		pdht->order.width = pdht->order.height = 1;
		pdht->order.raster = bitmap_raster(1);
		pdht->order.shift = 0;
		pdht->order.num_levels = pdht->order.num_bits = 1;
		levels[0] = 1;
		pdht->order.levels = levels;
		bits[0].offset = 0;
		bits[0].mask = 0;
		pdht->order.bits = bits;
		pdht->order.cache = pgs->ht_cache;
		pdht->order.transfer = 0;
		pdht->components = 0;
	}
	pgs->pattern_cache = 0;
	gs_sethalftonephase(pgs, 0, 0);
	/* Initialize things so that gx_remap_color won't crash. */
	pgs->color_space->type = &gs_color_space_type_DeviceGray;
	gx_set_device_color_1(pgs);
	pgs->overprint = false;
	pgs->cmap_procs = cmap_procs_default;
	gs_nulldevice(pgs);
	gs_setalpha(pgs, 1.0);
	gs_settransfer(pgs, null_transfer);
	gs_setflat(pgs, 1.0);
	gs_setfilladjust(pgs, 0.25, 0.25);
	gs_setstrokeadjust(pgs, true);
	pgs->font = 0;		/* Not right, but acceptable until the */
				/* PostScript code does the first setfont. */
	pgs->root_font = 0;	/* ditto */
	pgs->in_cachedevice = 0;
	pgs->in_charpath = 0;
	pgs->show_gstate = 0;
	pgs->level = 0;
	pgs->client_data = 0;
	if ( gs_initgraphics(pgs) < 0 )
	   {	/* Something went very wrong */
		return 0;
	   }
	return pgs;
}

/* Set the client data in a graphics state. */
/* This should only be done to a newly created state. */
void
gs_state_set_client(gs_state *pgs, void *pdata,
  const gs_state_client_procs *pprocs)
{	pgs->client_data = pdata;
	pgs->client_procs = *pprocs;
}

/* Get the client data from a graphics state. */
#undef gs_state_client_data		/* gzstate.h makes this a macro */
void *
gs_state_client_data(const gs_state *pgs)
{	return pgs->client_data;
}

/* Free a graphics state */
int
gs_state_free(gs_state *pgs)
{	gstate_free_contents(pgs);
	gs_free_object(pgs->memory, pgs, "gs_state_free");
	return 0;
}

/* Save the graphics state */
int
gs_gsave(gs_state *pgs)
{	gs_state *pnew = gstate_clone(pgs, "gs_gsave", true);
	if ( pnew == 0 )
	  return_error(gs_error_VMerror);
	gx_path_share(pgs->path);
	gx_cpath_share(pgs->clip_path);
	pgs->saved = pnew;
	if ( pgs->show_gstate == pgs )
	  pgs->show_gstate = pnew->show_gstate = pnew;
	pgs->level++;
	if_debug2('g', "[g]gsave -> 0x%lx, level = %d\n",
		  (ulong)pnew, pgs->level);
	return 0;
}

/* Restore the graphics state. */
int
gs_grestore(gs_state *pgs)
{	gs_state *saved = pgs->saved;
	void *pdata = pgs->client_data;
	void *sdata;
	if_debug2('g', "[g]grestore 0x%lx, level was %d\n",
		  (ulong)saved, pgs->level);
	if ( !saved )		/* shouldn't happen */
	  return gs_gsave(pgs);
	sdata = saved->client_data;
	if ( saved->pattern_cache == 0 )
	  saved->pattern_cache = pgs->pattern_cache;
	/* Swap back the client data pointers. */
	pgs->client_data = sdata;
	saved->client_data = pdata;
	if ( pdata != 0 && sdata != 0 )
	  (*pgs->client_procs.copy)(pdata, sdata);
	gstate_free_contents(pgs);
	*pgs = *saved;
	if ( pgs->show_gstate == saved )
	  pgs->show_gstate = pgs;
	gs_free_object(pgs->memory, saved, "gs_grestore");
	if ( pgs->saved )
	  return 0;
	return gs_gsave(pgs);
}

/* Restore to the bottommost graphics state.  Also clear */
/* the halftone caches, so stale pointers don't survive a restore. */
int
gs_grestoreall(gs_state *pgs)
{	int code;
	if ( !pgs->saved )		/* shouldn't happen */
	  return gs_gsave(pgs);
	while ( pgs->saved->saved )
	{	int code = gs_grestore(pgs);
		if ( code < 0 )
		  return code;
	}
	code = gs_grestore(pgs);
	if ( code < 0 )
	  return code;
	gx_ht_clear_cache(pgs->ht_cache);
	return code;
}

/* Allocate and return a new graphics state. */
gs_state *
gs_gstate(gs_state *pgs)
{	gs_state *pnew;
	gstate_share_paths(pgs);
	pnew = gstate_clone(pgs, "gs_gstate", false);
	if ( pnew == 0 )
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
gs_copygstate(gs_state *pto, const gs_state *pfrom)
{	gstate_copy(pto, pfrom, "gs_copygstate");
	return 0;
}

/* Copy the current graphics state to a previously allocated one. */
int
gs_currentgstate(gs_state *pto, const gs_state *pgs)
{	gstate_share_paths(pgs);
	gstate_copy(pto, pgs, "gs_currentgstate");
	return 0;
}

/* Restore the current graphics state from a previously allocated one. */
int
gs_setgstate(gs_state *pgs, const gs_state *pfrom)
{	/*
	 * The implementation is the same as currentgstate,
	 * except we must preserve the saved pointer, the level,
	 * and possibly the show_gstate.
	 */
	gs_state *saved = pgs->saved;
	gs_state *saved_show = pgs->show_gstate;
	int level = pgs->level;

	gstate_copy(pgs, pfrom, "gs_setgstate");
	pgs->saved = saved;
	pgs->level = level;
	pgs->show_gstate =
	  (pgs->show_gstate == pfrom ? pgs : saved_show);
	return 0;
}

/* Get the allocator pointer of a graphics state. */
/* This is provided only for the interpreter */
/* and for color space implementation. */
gs_memory_t *
gs_state_memory(const gs_state *pgs)
{	return pgs->memory;
}

/* Get the saved pointer of the graphics state. */
/* This is provided only for Level 2 grestore. */
gs_state *
gs_state_saved(const gs_state *pgs)
{	return pgs->saved;
}

/* Swap the saved pointer of the graphics state. */
/* This is provided only for save/restore. */
gs_state *
gs_state_swap_saved(gs_state *pgs, gs_state *new_saved)
{	gs_state *saved = pgs->saved;
	pgs->saved = new_saved;
	return saved;
}

/* Swap the memory pointer of the graphics state. */
/* This is provided only for the interpreter. */
gs_memory_t *
gs_state_swap_memory(gs_state *pgs, gs_memory_t *mem)
{	gs_memory_t *memory = pgs->memory;
	pgs->memory = mem;
	return memory;
}

/* ------ Operations on components ------ */

/* Reset most of the graphics state */
int
gs_initgraphics(register gs_state *pgs)
{	int code;
	gs_initmatrix(pgs);
	if (	(code = gs_newpath(pgs)) < 0 ||
		(code = gs_initclip(pgs)) < 0 ||
		(code = gs_setlinewidth(pgs, 1.0)) < 0 ||
		(code = gs_setlinecap(pgs, gs_cap_butt)) < 0 ||
		(code = gs_setlinejoin(pgs, gs_join_miter)) < 0 ||
		(code = gs_setdash(pgs, (float *)0, 0, 0.0)) < 0 ||
		(code = gs_setgray(pgs, 0.0)) < 0 ||
		(code = gs_setmiterlimit(pgs, 10.0)) < 0
	   )
	  return code;
	gs_init_rop(pgs);
	return 0;
}

/* setfilladjust */
int
gs_setfilladjust(gs_state *pgs, floatp adjust_x, floatp adjust_y)
{
#define adjust_fill_adjust(v)\
  if ( v < 0 ) v = 0; else if ( v > 0.5 ) v = 0.5
	adjust_fill_adjust(adjust_x);
	pgs->fill_adjust.x = float2fixed(adjust_x);
	adjust_fill_adjust(adjust_y);
	pgs->fill_adjust.y = float2fixed(adjust_y);
	return 0;
}

/* currentfilladjust */
int
gs_currentfilladjust(const gs_state *pgs, gs_point *adjust)
{	adjust->x = fixed2float(pgs->fill_adjust.x);
	adjust->y = fixed2float(pgs->fill_adjust.y);
	return 0;
}

/* ------ Internal routines ------ */

/* Allocate a gstate and its contents. */
private gs_state *
gstate_alloc(gs_memory_t *mem, client_name_t cname)
{	gs_state *pgs =
	  gs_alloc_struct(mem, gs_state, &st_gs_state, cname);
	gs_state_contents *cont =
	  gs_alloc_struct(mem, gs_state_contents, &st_gs_state_contents, cname);
	gx_device_halftone *pdht =
	  gs_alloc_struct(mem, gx_device_halftone, &st_device_halftone, cname);
	gs_color_space *pcs =
	  gs_alloc_struct(mem, gs_color_space, &st_color_space, cname);

	if ( pgs == 0 || cont == 0 || pdht == 0 || pcs == 0 )
	  {	gs_free_object(mem, pcs, cname);
		gs_free_object(mem, pdht, cname);
		gs_free_object(mem, cont, cname);
		gs_free_object(mem, pgs, cname);
		return 0;
	  }
	pgs->memory = mem;
	pgs->contents = cont;
	pgs->dev_ht = pdht;
	pgs->color_space = pcs;
	return pgs;
}

/* Set the contents pointers of a gstate. */
private void
gstate_set_contents(gs_state *pgs, gs_state_contents *cont)
{	pgs->contents = cont;
#define gset(element)\
  pgs->element = &cont->element;
	gset(path);
	gset(clip_path);
	gset(halftone);
	gset(ccolor);
	gset(dev_color);
#undef gset
}

/* Clone an existing graphics state. */
/* Return 0 if the allocation fails. */
/* The client is responsible for calling gx_[c]path_share on */
/* whichever of the old and new paths is appropriate. */
/* If for_gsave is true, the clone refers to the old contents, */
/* and we switch the old state to refer to the new contents. */
private gs_state *
gstate_clone(register gs_state *pfrom, client_name_t cname, bool for_gsave)
{	gs_memory_t *mem = pfrom->memory;
	gs_state_contents *cfrom = pfrom->contents;
	gx_device_halftone *dhtfrom = pfrom->dev_ht;
	gs_color_space *csfrom = pfrom->color_space;
	gs_state *pgs = gstate_alloc(mem, cname);
	gs_state_contents *cont;
	gx_device_halftone *pdht;
	gs_color_space *pcs;

	if ( pgs == 0 )
	  return 0;
	cont = pgs->contents;
	pdht = pgs->dev_ht;
	pcs = pgs->color_space;
	/* Increment references from gstate object. */
	*pgs = *pfrom;
	if ( pgs->client_data != 0 )
	{	void *pdata = pgs->client_data =
		  (*pgs->client_procs.alloc)(mem);
		if ( pdata == 0 ||
		     (*pgs->client_procs.copy)(pdata, pfrom->client_data) < 0
		   )
		  {	gs_free_object(mem, pcs, cname);
			gs_free_object(mem, pdht, cname);
			gs_free_object(mem, cont, cname);
			gs_free_object(mem, pgs, cname);
			return 0;
		  }
	}
	rc_increment(pgs->set_transfer.colored.gray);
	rc_increment(pgs->set_transfer.colored.red);
	rc_increment(pgs->set_transfer.colored.green);
	rc_increment(pgs->set_transfer.colored.blue);
	rc_increment(pgs->cie_render);
	rc_increment(pgs->black_generation);
	rc_increment(pgs->undercolor_removal);
	rc_increment(pgs->cie_joint_caches);
	if ( for_gsave )
	  {	gstate_set_contents(pgs, cfrom);
		pgs->dev_ht = dhtfrom;
		pgs->color_space = csfrom;
		gstate_set_contents(pfrom, cont);
		pfrom->dev_ht = pdht;
		pfrom->color_space = pcs;
	  }
	else
	  {	gstate_set_contents(pgs, cont);
		pgs->dev_ht = pdht;
		pgs->color_space = pcs;
	  }
	*cont = *cfrom;
	*pdht = *dhtfrom;
	*pcs = *csfrom;
	cs_adjust_counts(pgs, 1);
	return pgs;
}

/* Release the composite parts of a graphics state, */
/* but not the state itself. */
private void
gstate_free_contents(gs_state *pgs)
{	gs_memory_t *mem = pgs->memory;
	static const char cname[] = "gstate_free_contents";
#define rcdecr(element)\
  rc_decrement(pgs->element, mem, cname)
	gx_path_release(pgs->path);
	gx_cpath_release(pgs->clip_path);
	rcdecr(cie_joint_caches);
	rcdecr(set_transfer.colored.gray);
	rcdecr(set_transfer.colored.blue);
	rcdecr(set_transfer.colored.green);
	rcdecr(set_transfer.colored.red);
	rcdecr(undercolor_removal);
	rcdecr(black_generation);
	rcdecr(cie_render);
	cs_adjust_counts(pgs, -1);
	if ( pgs->client_data != 0 )
	  (*pgs->client_procs.free)(pgs->client_data, mem);
	gs_free_object(mem, pgs->color_space, cname);
	gs_free_object(mem, pgs->dev_ht, cname);
	gs_free_object(mem, pgs->contents, cname);
#undef rcdecr
}

/*
 * Mark both the old and new paths as shared when copying a gstate off-stack.
 * If the old path was previously shared, we must search up
 * the graphics state stack so we can mark its original ancestor
 * as shared, because the off-stack copy defeats the one-bit
 * reference count.
 */
private void
gstate_share_paths(const gs_state *pgs)
{	if ( pgs->path->shares_segments )
	  {	const gs_state *pcur;
		const gs_state *prev;
		const subpath *first;
		for ( pcur = pgs, first = pgs->path->first_subpath;
		      (prev = pcur->saved) != 0 &&
		        prev->path->first_subpath == first;
		      pcur = prev
		    )
		  if ( !prev->path->shares_segments )
		    {	gx_path_share(prev->path);
			break;
		    }
	  }
	else
	  gx_path_share(pgs->path);
	if ( pgs->clip_path->path.shares_segments )
	  {	const gs_state *pcur;
		const gs_state *prev;
		const subpath *first;
		for ( pcur = pgs, first = pgs->clip_path->path.first_subpath;
		      (prev = pcur->saved) != 0 &&
		        prev->clip_path->path.first_subpath == first;
		      pcur = prev
		    )
		  if ( !prev->clip_path->path.shares_segments )
		    {	gx_cpath_share(prev->clip_path);
			break;
		    }
	  }
	if ( pgs->clip_path->shares_list )
	  {	const gs_state *pcur;
		const gs_state *prev;
		const gx_clip_rect *head;
		for ( pcur = pgs, head = pgs->clip_path->list.head;
		      (prev = pcur->saved) != 0 &&
		        prev->clip_path->list.head == head;
		      pcur = prev
		    )
		  if ( !prev->clip_path->shares_list )
		    {	gx_cpath_share(prev->clip_path);
			break;
		    }
	  }
	gx_cpath_share(pgs->clip_path);
}

/* Copy one gstate to another. */
private void
gstate_copy(gs_state *pto, const gs_state *pfrom, client_name_t cname)
{	gs_memory_t *mem = pto->memory;
	gs_state_contents *cto = pto->contents;
	gx_device_halftone *dhtto = pto->dev_ht;
	gs_color_space *csto = pto->color_space;

	/* It's OK to decrement the counts before incrementing them, */
	/* because anything that is going to survive has a count of */
	/* at least 2 (pto and somewhere else) initially. */
	/* Handle references from contents. */
	cs_adjust_counts(pto, -1);
	gx_path_release(pto->path);
	gx_cpath_release(pto->clip_path);
	*cto = *pfrom->contents;
	*dhtto = *pfrom->dev_ht;
	*csto = *pfrom->color_space;
	cs_adjust_counts(pto, 1);
	gx_path_share(pto->path);
	gx_cpath_share(pto->clip_path);
	/* Handle references from gstate object. */
#define rccopy(element)\
  rc_pre_assign(pto->element, pfrom->element, mem, cname)
	rccopy(cie_joint_caches);
	rccopy(set_transfer.colored.gray);
	rccopy(set_transfer.colored.blue);
	rccopy(set_transfer.colored.green);
	rccopy(set_transfer.colored.red);
	rccopy(undercolor_removal);
	rccopy(black_generation);
	rccopy(cie_render);
#undef rccopy
	{	struct gx_pattern_cache_s *pcache = pto->pattern_cache;
		void *pdata = pto->client_data;
		*pto = *pfrom;
		pto->client_data = pdata;
		if ( pto->pattern_cache == 0 )
		  pto->pattern_cache = pcache;
		if ( pfrom->client_data != 0 )
		  (*pfrom->client_procs.copy)(pdata, pfrom->client_data);
	}
	gstate_set_contents(pto, cto);
	pto->dev_ht = dhtto;
	pto->color_space = csto;
	pto->show_gstate =
	  (pfrom->show_gstate == pfrom ? pto : 0);
}
