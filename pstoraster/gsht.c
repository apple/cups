/* Copyright (C) 1989, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gsht.c,v 1.2 2000/03/08 23:14:41 mike Exp $ */
/* setscreen operator for Ghostscript library */
#include "memory_.h"
#include <stdlib.h>		/* for qsort */
#include "gx.h"
#include "gserrors.h"
#include "gsstruct.h"
#include "gsutil.h"		/* for gs_next_ids */
#include "gxarith.h"		/* for igcd */
#include "gzstate.h"
#include "gxdevice.h"		/* for gzht.h */
#include "gzht.h"

/* Forward declarations */
void gx_set_effective_transfer(P1(gs_state *));

/* Structure types */
public_st_ht_order();
private_st_ht_order_component();
public_st_ht_order_comp_element();
public_st_halftone();
public_st_device_halftone();

/* GC procedures */

#define hptr ((gs_halftone *)vptr)

private 
ENUM_PTRS_BEGIN(halftone_enum_ptrs) return 0;

case 0:
switch (hptr->type)
{
    case ht_type_spot:
ENUM_RETURN((hptr->params.spot.transfer == 0 ?
	     hptr->params.spot.transfer_closure.data :
	     0));
    case ht_type_threshold:
ENUM_RETURN_CONST_STRING_PTR(gs_halftone, params.threshold.thresholds);
    case ht_type_client_order:
ENUM_RETURN(hptr->params.client_order.client_data);
    case ht_type_multiple:
    case ht_type_multiple_colorscreen:
ENUM_RETURN(hptr->params.multiple.components);
    case ht_type_none:
    case ht_type_screen:
    case ht_type_colorscreen:
return 0;
}
case 1:
switch (hptr->type) {
    case ht_type_threshold:
	ENUM_RETURN((hptr->params.threshold.transfer == 0 ?
		     hptr->params.threshold.transfer_closure.data :
		     0));
    case ht_type_client_order:
	ENUM_RETURN(hptr->params.threshold.transfer_closure.data);
    default:
	return 0;
}
ENUM_PTRS_END

private RELOC_PTRS_BEGIN(halftone_reloc_ptrs)
{
    switch (hptr->type) {
	case ht_type_spot:
	    if (hptr->params.spot.transfer == 0)
		RELOC_PTR(gs_halftone, params.spot.transfer_closure.data);
	    break;
	case ht_type_threshold:
	    RELOC_CONST_STRING_PTR(gs_halftone, params.threshold.thresholds);
	    if (hptr->params.threshold.transfer == 0)
		RELOC_PTR(gs_halftone, params.threshold.transfer_closure.data);
	    break;
	case ht_type_client_order:
	    RELOC_PTR(gs_halftone, params.client_order.client_data);
	    RELOC_PTR(gs_halftone, params.client_order.transfer_closure.data);
	    break;
	case ht_type_multiple:
	case ht_type_multiple_colorscreen:
	    RELOC_PTR(gs_halftone, params.multiple.components);
	    break;
	case ht_type_none:
	case ht_type_screen:
	case ht_type_colorscreen:
	    break;
    }
}
RELOC_PTRS_END

#undef hptr

/* setscreen */
int
gs_setscreen(gs_state * pgs, gs_screen_halftone * phsp)
{
    gs_screen_enum senum;
    int code = gx_ht_process_screen(&senum, pgs, phsp,
				    gs_currentaccuratescreens());

    if (code < 0)
	return code;
    return gs_screen_install(&senum);
}

/* currentscreen */
int
gs_currentscreen(const gs_state * pgs, gs_screen_halftone * phsp)
{
    switch (pgs->halftone->type) {
	case ht_type_screen:
	    *phsp = pgs->halftone->params.screen;
	    return 0;
	case ht_type_colorscreen:
	    *phsp = pgs->halftone->params.colorscreen.screens.colored.gray;
	    return 0;
	default:
	    return_error(gs_error_undefined);
    }
}

/* .currentscreenlevels */
int
gs_currentscreenlevels(const gs_state * pgs)
{
    return pgs->dev_ht->order.num_levels;
}

/* .setscreenphase */
int
gx_imager_setscreenphase(gs_imager_state * pis, int x, int y,
			 gs_color_select_t select)
{
    if (select == gs_color_select_all) {
	int i;

	for (i = 0; i < gs_color_select_count; ++i)
	    gx_imager_setscreenphase(pis, x, y, (gs_color_select_t) i);
	return 0;
    } else if (select < 0 || select >= gs_color_select_count)
	return_error(gs_error_rangecheck);
    pis->screen_phase[select].x = x;
    pis->screen_phase[select].y = y;
    return 0;
}
int
gs_setscreenphase(gs_state * pgs, int x, int y, gs_color_select_t select)
{
    int code = gx_imager_setscreenphase((gs_imager_state *) pgs, x, y,
					select);

    /*
     * If we're only setting the source phase, we don't need to do
     * unset_dev_color, because the source phase doesn't affect painting
     * with the current color.
     */
    if (code >= 0 && (select == gs_color_select_texture ||
		      select == gs_color_select_all)
	)
	gx_unset_dev_color(pgs);
    return code;
}

/* .currentscreenphase */
int
gs_currentscreenphase(const gs_state * pgs, gs_int_point * pphase,
		      gs_color_select_t select)
{
    if (select < 0 || select >= gs_color_select_count)
	return_error(gs_error_rangecheck);
    *pphase = pgs->screen_phase[select];
    return 0;
}

/* currenthalftone */
int
gs_currenthalftone(gs_state * pgs, gs_halftone * pht)
{
    *pht = *pgs->halftone;
    return 0;
}

/* ------ Internal routines ------ */

/* Process one screen plane. */
int
gx_ht_process_screen_memory(gs_screen_enum * penum, gs_state * pgs,
		gs_screen_halftone * phsp, bool accurate, gs_memory_t * mem)
{
    gs_point pt;
    int code = gs_screen_init_memory(penum, pgs, phsp, accurate, mem);

    if (code < 0)
	return code;
    while ((code = gs_screen_currentpoint(penum, &pt)) == 0)
	if ((code = gs_screen_next(penum, (*phsp->spot_function) (pt.x, pt.y))) < 0)
	    return code;
    return 0;
}

/* Internal procedure to allocate and initialize either an internally */
/* generated or a client-defined halftone order. */
private int
gx_ht_alloc_ht_order(gx_ht_order * porder, uint width, uint height,
	uint num_levels, uint num_bits, uint strip_shift, gs_memory_t * mem)
{
    gx_compute_cell_values(&porder->params);
    porder->width = width;
    porder->height = height;
    porder->raster = bitmap_raster(width);
    porder->shift = strip_shift;
    porder->orig_height = porder->height;
    porder->orig_shift = porder->shift;
    porder->full_height = ht_order_full_height(porder);
    porder->num_levels = num_levels;
    porder->num_bits = num_bits;
    porder->levels =
	(uint *) gs_alloc_byte_array(mem, num_levels, sizeof(uint),
				     "ht order(levels)");
    porder->bits =
	(gx_ht_bit *) gs_alloc_byte_array(mem, num_bits, sizeof(gx_ht_bit),
					  "ht order(bits)");
    if (porder->levels == 0 || porder->bits == 0) {
	gs_free_object(mem, porder->bits, "ht order(bits)");
	gs_free_object(mem, porder->levels, "ht order(levels)");
	return_error(gs_error_VMerror);
    }
    porder->cache = 0;
    porder->transfer = 0;
    return 0;
}

/* Allocate and initialize the contents of a halftone order. */
/* The client must have set the defining values in porder->params. */
int
gx_ht_alloc_order(gx_ht_order * porder, uint width, uint height,
		  uint strip_shift, uint num_levels, gs_memory_t * mem)
{
    gx_ht_order order;
    int code;

    order = *porder;
    gx_compute_cell_values(&order.params);
    code = gx_ht_alloc_ht_order(&order, width, height, num_levels,
				width * height, strip_shift, mem);
    if (code < 0)
	return code;
    *porder = order;
    return 0;
}

/* Allocate and initialize the contents of a client-defined halftone order. */
int
gx_ht_alloc_client_order(gx_ht_order * porder, uint width, uint height,
			 uint num_levels, uint num_bits, gs_memory_t * mem)
{
    gx_ht_order order;
    int code;

    order = *porder;
    order.params.M = width, order.params.N = 0;
    order.params.R = 1;
    order.params.M1 = height, order.params.N1 = 0;
    order.params.R1 = 1;
    gx_compute_cell_values(&order.params);
    code = gx_ht_alloc_ht_order(&order, width, height, num_levels,
				num_bits, 0, mem);
    if (code < 0)
	return code;
    *porder = order;
    return 0;
}

/* Compare keys ("masks", actually sample values) for qsort. */
private int
compare_samples(const void *p1, const void *p2)
{
    ht_sample_t m1 = ((const gx_ht_bit *)p1)->mask;
    ht_sample_t m2 = ((const gx_ht_bit *)p2)->mask;

    return (m1 < m2 ? -1 : m1 > m2 ? 1 : 0);
}
/* Sort the halftone order by sample value. */
void
gx_sort_ht_order(gx_ht_bit * recs, uint N)
{
    int i;

    /* Tag each sample with its index, for sorting. */
    for (i = 0; i < N; i++)
	recs[i].offset = i;
    qsort((void *)recs, N, sizeof(*recs), compare_samples);
#ifdef DEBUG
    if (gs_debug_c('H')) {
	uint i;

	dlputs("[H]Sorted samples:\n");
	for (i = 0; i < N; i++)
	    dlprintf3("%5u: %5u: %u\n",
		      i, recs[i].offset, recs[i].mask);
    }
#endif
}

/*
 * Construct the halftone order from a sampled spot function.  Only width x
 * strip samples have been filled in; we must replicate the resulting sorted
 * order vertically, shifting it by shift each time.  See gxdht.h regarding
 * the invariants that must be restored.
 */
void
gx_ht_construct_spot_order(gx_ht_order * porder)
{
    uint width = porder->width;
    uint num_levels = porder->num_levels;	/* = width x strip */
    uint strip = num_levels / width;
    gx_ht_bit *bits = porder->bits;
    uint *levels = porder->levels;
    uint shift = porder->orig_shift;
    uint full_height = porder->full_height;
    uint num_bits = porder->num_bits;
    uint copies = num_bits / (width * strip);
    gx_ht_bit *bp = bits + num_bits - 1;
    uint i;

    gx_sort_ht_order(bits, num_levels);
    if_debug5('h',
	      "[h]spot order: num_levels=%u w=%u h=%u strip=%u shift=%u\n",
	      num_levels, width, porder->orig_height, strip, shift);
    /* Fill in the levels array, replicating the bits vertically */
    /* if needed. */
    for (i = num_levels; i > 0;) {
	uint offset = bits[--i].offset;
	uint x = offset % width;
	uint hy = offset - x;
	uint k;

	levels[i] = i * copies;
	for (k = 0; k < copies;
	     k++, bp--, hy += num_levels, x = (x + width - shift) % width
	    )
	    bp->offset = hy + x;
    }
    /* If we have a complete halftone, restore the invariant. */
    if (num_bits == width * full_height) {
	porder->height = full_height;
	porder->shift = 0;
    }
    gx_ht_construct_bits(porder);
}

/* Construct a single offset/mask. */
void
gx_ht_construct_bit(gx_ht_bit * bit, int width, int bit_num)
{
    uint padding = bitmap_raster(width) * 8 - width;
    int pix = bit_num;
    ht_mask_t mask;
    byte *pb;

    pix += pix / width * padding;
    bit->offset = (pix >> 3) & -size_of(mask);
    mask = (ht_mask_t) 1 << (~pix & (ht_mask_bits - 1));
    /* Replicate the mask bits. */
    pix = ht_mask_bits - width;
    while ((pix -= width) >= 0)
	mask |= mask >> width;
    /* Store the mask, reversing bytes if necessary. */
    bit->mask = 0;
    for (pb = (byte *) & bit->mask + (sizeof(mask) - 1);
	 mask != 0;
	 mask >>= 8, pb--
	)
	*pb = (byte) mask;
}

/* Construct offset/masks from the whitening order. */
/* porder->bits[i].offset contains the index of the bit position */
/* that is i'th in the whitening order. */
void
gx_ht_construct_bits(gx_ht_order * porder)
{
    uint i;
    gx_ht_bit *phb;

    for (i = 0, phb = porder->bits; i < porder->num_bits; i++, phb++)
	gx_ht_construct_bit(phb, porder->width, phb->offset);
#ifdef DEBUG
    if (gs_debug_c('H')) {
	dlprintf1("[H]Halftone order bits 0x%lx:\n", (ulong) porder->bits);
	for (i = 0, phb = porder->bits; i < porder->num_bits; i++, phb++)
	    dlprintf3("%4d: %u:0x%lx\n", i, phb->offset,
		      (ulong) phb->mask);
    }
#endif
}

/* Release a gx_device_halftone by freeing its components. */
/* (Don't free the gx_device_halftone itself.) */
void
gx_ht_order_release(gx_ht_order * porder, gs_memory_t * mem, bool free_cache)
{
    if (free_cache && porder->cache)
	gx_ht_free_cache(mem, porder->cache);
    gs_free_object(mem, porder->transfer, "gx_ht_order_release(transfer)");
    gs_free_object(mem, porder->bits, "gx_ht_order_release(bits)");
    gs_free_object(mem, porder->levels, "gx_ht_order_release(levels)");
}
void
gx_device_halftone_release(gx_device_halftone * pdht, gs_memory_t * mem)
{
    if (pdht->components) {
	int i;

	/* One of the components might be the same as the default */
	/* order, so check that we don't free it twice. */
	for (i = 0; i < pdht->num_comp; ++i)
	    if (pdht->components[i].corder.bits !=
		pdht->order.bits
		) {		/* Currently, all orders except the default one */
		/* own their caches. */
		gx_ht_order_release(&pdht->components[i].corder, mem, true);
	    }
	gs_free_object(mem, pdht->components,
		       "gx_dev_ht_release(components)");
	pdht->components = 0;
	pdht->num_comp = 0;
    }
    gx_ht_order_release(&pdht->order, mem, false);
}

/* Install a device halftone in an imager state. */
/* Note that this does not read or update the client halftone. */
int
gx_imager_dev_ht_install(gs_imager_state * pis,
			 const gx_device_halftone * pdht, gs_halftone_type type, const gx_device * dev)
{
    gx_device_halftone *pgdht = pis->dev_ht;

    if ((ulong) pdht->order.raster * (pdht->order.num_bits /
			       pdht->order.width) > pis->ht_cache->bits_size
	)
	return_error(gs_error_limitcheck);
    if (pgdht != 0 && pgdht->rc.ref_count == 1 &&
	pgdht->rc.memory == pdht->rc.memory
	) {			/* The current device halftone isn't shared. */
	/* Just release its components. */
	gx_device_halftone_release(pgdht, pgdht->rc.memory);
    } else {			/* The device halftone is shared or not yet allocated. */
	rc_unshare_struct(pis->dev_ht, gx_device_halftone,
			  &st_device_halftone, pdht->rc.memory,
			  return_error(gs_error_VMerror),
			  "gx_imager_dev_ht_install");
	pgdht = pis->dev_ht;
    }
    {
	rc_header rc;

	rc = pgdht->rc;
	*pgdht = *pdht;
	pgdht->rc = rc;
    }
    pgdht->id = gs_next_ids(1);
    pgdht->type = type;
    /* Clear the cache, to avoid confusion in case the address of */
    /* a new order vector matches that of a (deallocated) old one. */
    gx_ht_clear_cache(pis->ht_cache);
    /* Set the color_indices according to the device color_info. */
    /* Also compute the LCM of the primary color cell sizes. */
    /* Note that for strip halftones, the "cell size" is the */
    /* theoretical fully expanded size with shift = 0. */
    if (pdht->components != 0) {
	static const gs_ht_separation_name dcnames[5][4] =
	{
	    {gs_ht_separation_Default},		/* not used */
	    {gs_ht_separation_Default, gs_ht_separation_Default,
	     gs_ht_separation_Default, gs_ht_separation_Gray
	    },
	    {gs_ht_separation_Default},		/* not used */
	    {gs_ht_separation_Red, gs_ht_separation_Green,
	     gs_ht_separation_Blue, gs_ht_separation_Default
	    },
	    {gs_ht_separation_Cyan, gs_ht_separation_Magenta,
	     gs_ht_separation_Yellow, gs_ht_separation_Black
	    }
	};
	static const gs_ht_separation_name cscnames[4] =
	{gs_ht_separation_Red, gs_ht_separation_Green,
	 gs_ht_separation_Blue, gs_ht_separation_Default
	};
	int num_comps = dev->color_info.num_components;
	const gs_ht_separation_name *cnames = dcnames[num_comps];
	int lcm_width = 1, lcm_height = 1;
	uint i;

	/* Halftones set by setcolorscreen, and (we think) */
	/* Type 2 and Type 4 halftones, are supposed to work */
	/* for both RGB and CMYK, so we need a special check here. */
	if (num_comps == 4 &&
	    (type == ht_type_colorscreen ||
	     type == ht_type_multiple_colorscreen)
	    )
	    cnames = cscnames;
	if_debug4('h', "[h]dcnames=%lu,%lu,%lu,%lu\n",
		  (ulong) cnames[0], (ulong) cnames[1],
		  (ulong) cnames[2], (ulong) cnames[3]);
	memset(pgdht->color_indices, 0, sizeof(pdht->color_indices));
	for (i = 0; i < pdht->num_comp; i++) {
	    const gx_ht_order_component *pcomp =
	    &pdht->components[i];
	    int j;

	    if_debug2('h', "[h]cname[%d]=%lu\n",
		      i, (ulong) pcomp->cname);
	    for (j = 0; j < 4; j++) {
		if (pcomp->cname == cnames[j]) {
		    if_debug2('h', "[h]color_indices[%d]=%d\n",
			      j, i);
		    pgdht->color_indices[j] = i;
		}
	    }
	}
	/* Now do a second pass to compute the LCM. */
	/* We have to do it this way in case some entry in */
	/* color_indices is still 0. */
	for (i = 0; i < 4; ++i) {
	    const gx_ht_order_component *pcomp =
	    &pdht->components[pgdht->color_indices[i]];
	    uint cw = pcomp->corder.width;
	    uint ch = pcomp->corder.full_height;
	    int dw = lcm_width / igcd(lcm_width, cw);
	    int dh = lcm_height / igcd(lcm_height, ch);

	    lcm_width = (cw > max_int / dw ? max_int : cw * dw);
	    lcm_height = (ch > max_int / dh ? max_int : ch * dh);
	}
	pgdht->lcm_width = lcm_width;
	pgdht->lcm_height = lcm_height;
    } else {			/* Only one component. */
	pgdht->lcm_width = pgdht->order.width;
	pgdht->lcm_height = pgdht->order.full_height;
    }
    if_debug2('h', "[h]LCM=(%d,%d)\n",
	      pgdht->lcm_width, pgdht->lcm_height);
    gx_imager_set_effective_xfer(pis);
    return 0;
}

/*
 * Install a new halftone in the graphics state.  Note that we copy the top
 * level of the gs_halftone and the gx_device_halftone, and take ownership
 * of any substructures.
 */
int
gx_ht_install(gs_state * pgs, const gs_halftone * pht,
	      const gx_device_halftone * pdht)
{
    gs_memory_t *mem = pht->rc.memory;
    gs_halftone *old_ht = pgs->halftone;
    gs_halftone *new_ht;
    int code;

    if (old_ht != 0 && old_ht->rc.memory == mem &&
	old_ht->rc.ref_count == 1
	)
	new_ht = old_ht;
    else
	rc_alloc_struct_1(new_ht, gs_halftone, &st_halftone,
			  mem, return_error(gs_error_VMerror),
			  "gx_ht_install(new halftone)");
    code = gx_imager_dev_ht_install((gs_imager_state *) pgs,
			     pdht, pht->type, gs_currentdevice_inline(pgs));
    if (code < 0) {
	if (new_ht != old_ht)
	    gs_free_object(mem, new_ht, "gx_ht_install(new halftone)");
	return code;
    }
    if (new_ht != old_ht)
	rc_decrement(old_ht, "gx_ht_install(old halftone)");
    {
	rc_header rc;

	rc = new_ht->rc;
	*new_ht = *pht;
	new_ht->rc = rc;
    }
    pgs->halftone = new_ht;
    gx_unset_dev_color(pgs);
    return 0;
}

/* Reestablish the effective transfer functions, taking into account */
/* any overrides from halftone dictionaries. */
void
gx_imager_set_effective_xfer(gs_imager_state * pis)
{
    const gx_device_halftone *pdht = pis->dev_ht;

    pis->effective_transfer = pis->set_transfer;	/* default */
    if (pdht == 0)
	return;			/* not initialized yet */
    if (pdht->components == 0) {	/* Check for transfer function override in single halftone */
	gx_transfer_map *pmap = pdht->order.transfer;

	if (pmap != 0)
	    pis->effective_transfer.indexed[0] =
		pis->effective_transfer.indexed[1] =
		pis->effective_transfer.indexed[2] =
		pis->effective_transfer.indexed[3] = pmap;
    } else {			/* Check in all 4 standard separations */
	int i;

	for (i = 0; i < 4; ++i) {
	    gx_transfer_map *pmap =
	    pdht->components[pdht->color_indices[i]].corder.
	    transfer;

	    if (pmap != 0)
		pis->effective_transfer.indexed[i] = pmap;
	}
    }
}
void
gx_set_effective_transfer(gs_state * pgs)
{
    gx_imager_set_effective_xfer((gs_imager_state *) pgs);
}
