/* Copyright (C) 1994, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gsht1.c */
/* Extended halftone operators for Ghostscript library */
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsstruct.h"
#include "gsutil.h"			/* for gs_next_ids */
#include "gzstate.h"
#include "gxdevice.h"			/* for gzht.h */
#include "gzht.h"

/* Define the size of the halftone tile cache. */
#define max_tile_bytes_LARGE 4096
#define max_tile_bytes_SMALL 512
#if arch_small_memory
#  define max_tile_cache_bytes max_tile_bytes_SMALL
#else
#  define max_tile_cache_bytes\
     (gs_if_debug_c('.') ? max_tile_bytes_SMALL : max_tile_bytes_LARGE)
#endif

/* Imports from gsht.c */
int gx_ht_process_screen(P4(gs_screen_enum *, gs_state *,
  gs_screen_halftone *, bool));

/* Imports from gscolor.c */
void load_transfer_map(P3(gs_state *, gx_transfer_map *, floatp));

/* Forward declarations */
private int process_spot(P3(gx_ht_order *, gs_state *,
  gs_spot_halftone *));
private int process_threshold(P3(gx_ht_order *, gs_state *,
  gs_threshold_halftone *));

/* Structure types */
public_st_halftone_component();
public_st_ht_component_element();
private_st_ht_order_component();
private_st_ht_order_comp_element();

/* GC procedures */

#define hptr ((gs_halftone_component *)vptr)

private ENUM_PTRS_BEGIN(halftone_component_enum_ptrs) return 0;
	case 0:
		if ( hptr->type != ht_type_threshold )
		  return 0;
		ENUM_RETURN_CONST_STRING_PTR(gs_halftone_component, params.threshold.thresholds);
ENUM_PTRS_END

private RELOC_PTRS_BEGIN(halftone_component_reloc_ptrs) {
	if ( hptr->type == ht_type_threshold )
	  RELOC_CONST_STRING_PTR(gs_halftone_component, params.threshold.thresholds);
} RELOC_PTRS_END

#undef hptr

/* setcolorscreen */
int
gs_setcolorscreen(gs_state *pgs, gs_colorscreen_halftone *pht)
{	gs_halftone ht;
	ht.type = ht_type_colorscreen;
	ht.params.colorscreen = *pht;
	return gs_sethalftone(pgs, &ht);
}

/* currentcolorscreen */
int
gs_currentcolorscreen(gs_state *pgs, gs_colorscreen_halftone *pht)
{	int code;
	switch ( pgs->halftone->type )
	{
	case ht_type_colorscreen:
		*pht = pgs->halftone->params.colorscreen;
		return 0;
	default:
		code = gs_currentscreen(pgs, &pht->screens.colored.gray);
		if ( code < 0 )
			return code;
		pht->screens.colored.red = pht->screens.colored.gray;
		pht->screens.colored.green = pht->screens.colored.gray;
		pht->screens.colored.blue = pht->screens.colored.gray;
		return 0;
	}
}

/* Set the halftone in the graphics state. */
int
gs_sethalftone(gs_state *pgs, gs_halftone *pht)
{	gx_device_halftone dev_ht;
	int code = gs_sethalftone_prepare(pgs, pht, &dev_ht);
	if ( code < 0 )
		return code;
	return gx_ht_install(pgs, pht, &dev_ht);
}
/* Prepare the halftone, but don't install it. */
int
gs_sethalftone_prepare(gs_state *pgs, gs_halftone *pht,
  gx_device_halftone *pdht)
{	gs_memory_t *mem = pgs->memory;
	gx_ht_order_component *pocs = 0;
	int code = 0;
	switch ( pht->type )
	{
	case ht_type_colorscreen:
	{	gs_screen_halftone *phc =
			pht->params.colorscreen.screens.indexed;
		static const gs_ht_separation_name cnames[4] =
		{ gs_ht_separation_Default, gs_ht_separation_Red,
		  gs_ht_separation_Green, gs_ht_separation_Blue
		};
		static const int cindex[4] = { 3, 0, 1, 2 };
		int i;
		pocs = gs_alloc_struct_array(mem, 4,
					     gx_ht_order_component,
					     &st_ht_order_component_element,
					     "gs_sethalftone");
		if ( pocs == 0 )
			return_error(gs_error_VMerror);
		for ( i = 0; i < 4; i++ )
		{	gs_screen_enum senum;
			int ci = cindex[i];
			gx_ht_order_component *poc = &pocs[i];
			code = gx_ht_process_screen(&senum, pgs, &phc[ci],
						gs_currentaccuratescreens());
			if ( code < 0 )
				break;
#define sorder senum.order
			poc->corder = sorder;
			poc->cname = cnames[i];
			if ( i == 0 )		/* Gray = Default */
				pdht->order = sorder;
			else
			{	uint tile_bytes =
				  sorder.raster *
				    (sorder.num_bits / sorder.width);
				uint num_tiles =
				  max_tile_cache_bytes / tile_bytes + 1;
				gx_ht_cache *pcache =
				  gx_ht_alloc_cache(mem, num_tiles,
						    tile_bytes * num_tiles);
				if ( pcache == 0 )
				  {	code = gs_note_error(gs_error_VMerror);
					break;
				  }
				poc->corder.cache = pcache;
				gx_ht_init_cache(pcache, &poc->corder);
			}
#undef sorder
		}
		if ( code < 0 )
		  break;
		pdht->components = pocs;
		pdht->num_comp = 4;
	}	break;
	case ht_type_spot:
		code = process_spot(&pdht->order, pgs, &pht->params.spot);
		if ( code < 0 )
			return code;
		pdht->components = 0;
		break;
	case ht_type_threshold:
		code = process_threshold(&pdht->order, pgs,
					 &pht->params.threshold);
		if ( code < 0 )
			return code;
		pdht->components = 0;
		break;
	case ht_type_multiple:
	case ht_type_multiple_colorscreen:
	{	uint count = pht->params.multiple.num_comp;
		bool have_Default = false;
		uint i;
		gs_halftone_component *phc = pht->params.multiple.components;
		gx_ht_order_component *poc_next;
		pocs = gs_alloc_struct_array(mem, count,
					     gx_ht_order_component,
					     &st_ht_order_component_element,
					     "gs_sethalftone");
		if ( pocs == 0 )
			return_error(gs_error_VMerror);
		poc_next = pocs + 1;
		for ( i = 0; i < count; i++, phc++ )
		{	gx_ht_order_component *poc;
			if ( phc->cname == gs_ht_separation_Default )
			{	if ( have_Default )
				{	/* Duplicate Default */
					code = gs_note_error(gs_error_rangecheck);
					break;
				}
				poc = pocs;
				have_Default = true;
			}
			else if ( i == count - 1 && !have_Default )
			{	/* No Default */
				code = gs_note_error(gs_error_rangecheck);
				break;
			}
			else
				poc = poc_next++;
			poc->cname = phc->cname;
			switch ( phc->type )
			{
			case ht_type_spot:
				code = process_spot(&poc->corder, pgs,
						    &phc->params.spot);
				break;
			case ht_type_threshold:
				code = process_threshold(&poc->corder, pgs,
						&phc->params.threshold);
				break;
			default:
				code = gs_note_error(gs_error_rangecheck);
				break;
			}
			if ( code < 0 )
			  break;
			if ( poc != pocs )
			{	gx_ht_cache *pcache =
				  gx_ht_alloc_cache(mem, 1,
						    poc->corder.raster *
						    (poc->corder.num_bits /
						     poc->corder.width));
				if ( pcache == 0 )
				  {	code = gs_note_error(gs_error_VMerror);
					break;
				  }
				poc->corder.cache = pcache;
				gx_ht_init_cache(pcache, &poc->corder);
			}
		}
		if ( code < 0 )
		  break;
		pdht->order = pocs[0].corder;		/* Default */
		if ( count == 1 )
		{	/* We have only a Default; */
			/* we don't need components. */
			gs_free_object(mem, pocs, "gs_sethalftone");
			pdht->components = 0;
		}
		else
		{	pdht->components = pocs;
			pdht->num_comp = count;
		}
	}	break;
	default:
		return_error(gs_error_rangecheck);
	}
	if ( code < 0 )
	  gs_free_object(mem, pocs, "gs_sethalftone");
	return code;
}

/* ------ Internal routines ------ */

/* Process a transfer function override, if any. */
private int
process_transfer(gx_ht_order *porder, gs_state *pgs,
  gs_mapping_proc tproc)
{	gx_transfer_map *pmap;
	if ( tproc == 0 )
		return 0;
	pmap = gs_alloc_struct(pgs->memory, gx_transfer_map, &st_transfer_map,
			       "process_transfer");
	if ( pmap == 0 )
	  return_error(gs_error_VMerror);
	pmap->proc = tproc;
	pmap->id = gs_next_ids(1);
	load_transfer_map(pgs, pmap, 0.0);
	porder->transfer = pmap;
	return 0;
}

/* Process a spot plane. */
private int
process_spot(gx_ht_order *porder, gs_state *pgs,
  gs_spot_halftone *phsp)
{	gs_screen_enum senum;
	int code = gx_ht_process_screen(&senum, pgs, &phsp->screen,
					phsp->accurate_screens);
	if ( code < 0 )
		return code;
	*porder = senum.order;
	return process_transfer(porder, pgs, phsp->transfer);
}

/* Process a threshold plane. */
private int
process_threshold(gx_ht_order *porder, gs_state *pgs,
  gs_threshold_halftone *phtp)
{	int code = gx_ht_alloc_order(porder, phtp->width, phtp->height,
				     0, 256, pgs->memory);
	if ( code < 0 )
		return code;
	gx_ht_construct_threshold_order(porder, phtp->thresholds.data);
	return process_transfer(porder, pgs, phtp->transfer);
}

/* Construct the halftone order from a threshold array. */
void
gx_ht_construct_threshold_order(gx_ht_order *porder, const byte *thresholds)
{	uint size = porder->num_bits;
	uint *levels = porder->levels;
	gx_ht_bit *bits = porder->bits;
	uint i, j;
	for ( i = 0; i < size; i++ )
		bits[i].mask = max(1, thresholds[i]);
	gx_sort_ht_order(bits, size);
	/* We want to set levels[j] to the lowest value of i */
	/* such that bits[i].mask > j. */
	for ( i = 0, j = 0; i < size; i++ )
	{	if ( bits[i].mask != j )
		{	if_debug3('h', "[h]levels[%u..%u] = %u\n",
				  j, (uint)bits[i].mask, i);
			while ( j < bits[i].mask )
				levels[j++] = i;
		}
	}
	while ( j < 256 )
		levels[j++] = size;
	gx_ht_construct_bits(porder);
}
