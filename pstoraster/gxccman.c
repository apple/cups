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

/* gxccman.c */
/* Character cache management routines for Ghostscript library */
#include "gx.h"
#include "memory_.h"
#include "gpcheck.h"
#include "gserrors.h"
#include "gsstruct.h"
#include "gsbitops.h"
#include "gsutil.h"			/* for gs_next_ids */
#include "gxfixed.h"
#include "gxmatrix.h"
#include "gzstate.h"
#include "gzpath.h"
#include "gxdevice.h"
#include "gxdevmem.h"
#include "gxchar.h"
#include "gxfont.h"
#include "gxfcache.h"
#include "gxxfont.h"

/* Define the descriptors for the cache structures. */
private_st_cached_fm_pair();
private_st_cached_fm_pair_elt();
/*private_st_cached_char();*/		/* unused */
private_st_cached_char_ptr();		/* unused */
private_st_cached_char_ptr_elt();
/* GC procedures */
/* We do all the work in font_dir_enum/reloc_ptrs in gsfont.c. */
/* See gxfcache.h for details. */
private ENUM_PTRS_BEGIN(cc_ptr_enum_ptrs) return 0;
ENUM_PTRS_END
private RELOC_PTRS_BEGIN(cc_ptr_reloc_ptrs) {
} RELOC_PTRS_END

/* Forward references */
private gx_xfont *lookup_xfont_by_name(P6(gx_device *, gx_xfont_procs *, gs_font_name *, int, const cached_fm_pair *, const gs_matrix *));
private cached_char *alloc_char(P2(gs_font_dir *, ulong));
private cached_char *alloc_char_in_chunk(P2(gs_font_dir *, ulong));
private void hash_remove_cached_char(P2(gs_font_dir *, uint));
private void shorten_cached_char(P3(gs_font_dir *, cached_char *, uint));

/* ====== Initialization ====== */

/* Allocate and initialize the character cache elements of a font directory. */
int
gx_char_cache_alloc(gs_memory_t *mem, register gs_font_dir *pdir,
  uint bmax, uint mmax, uint cmax, uint upper)
{	/* Since we use open hashing, we must increase cmax somewhat. */
	uint chsize = (cmax + (cmax >> 1)) | 31;
	cached_fm_pair *mdata;
	cached_char **chars;

	/* Round up chsize to a power of 2. */
	while ( chsize & (chsize + 1) )
	  chsize |= chsize >> 1;
	chsize++;
	mdata = gs_alloc_struct_array(mem, mmax, cached_fm_pair,
				      &st_cached_fm_pair_element,
				      "font_dir_alloc(mdata)");
	chars = gs_alloc_struct_array(mem, chsize, cached_char *,
				      &st_cached_char_ptr_element,
				      "font_dir_alloc(chars)");
	if ( mdata == 0 || chars == 0 )
	  {	gs_free_object(mem, chars, "font_dir_alloc(chars)");
		gs_free_object(mem, mdata, "font_dir_alloc(mdata)");
		return_error(gs_error_VMerror);
	  }
	pdir->fmcache.mmax = mmax;
	pdir->fmcache.mdata = mdata;
	pdir->ccache.memory = mem;
	pdir->ccache.bmax = bmax;
	pdir->ccache.cmax = cmax;
	pdir->ccache.lower = upper / 10;
	pdir->ccache.upper = upper;
	pdir->ccache.table = chars;
	pdir->ccache.table_mask = chsize - 1;
	gx_char_cache_init(pdir);
	return 0;
}

/* Initialize the character cache. */
void
gx_char_cache_init(register gs_font_dir *dir)
{	int i;
	cached_fm_pair *pair;
	char_cache_chunk *cck =
	  (char_cache_chunk *)gs_malloc(1, sizeof(char_cache_chunk),
					"initial_chunk");

	dir->fmcache.msize = 0;
	dir->fmcache.mnext = 0;
	gx_bits_cache_chunk_init(cck, NULL, 0);
	gx_bits_cache_init((gx_bits_cache *)&dir->ccache, cck);
	dir->ccache.bspace = 0;
	memset((char *)dir->ccache.table, 0,
	       (dir->ccache.table_mask + 1) * sizeof(cached_char *));
	for ( i = 0, pair = dir->fmcache.mdata;
	      i < dir->fmcache.mmax; i++, pair++
	    )
	  {	pair->index = i;
		fm_pair_init(pair);
	  }
}

/* ====== Purging ====== */

/* Purge from the character cache all entries selected by */
/* a client-supplied procedure. */
void
gx_purge_selected_cached_chars(gs_font_dir *dir,
  bool (*proc)(P2(cached_char *, void *)), void *proc_data)
{	int chi;
	int cmax = dir->ccache.table_mask;

	for ( chi = 0; chi <= cmax; )
	{	cached_char *cc = dir->ccache.table[chi];
		if ( cc != 0 && (*proc)(cc, proc_data) )
		  {	hash_remove_cached_char(dir, chi);
			gx_free_cached_char(dir, cc);
		  }
		else
		  chi++;
	}
}

/* ====== Font-level routines ====== */

/* Add a font/matrix pair to the cache. */
/* (This is only exported for gxccache.c.) */
cached_fm_pair *
gx_add_fm_pair(register gs_font_dir *dir, gs_font *font, const gs_uid *puid,
  const gs_state *pgs)
{	register cached_fm_pair *pair =
	  dir->fmcache.mdata + dir->fmcache.mnext;
	cached_fm_pair *mend =
	  dir->fmcache.mdata + dir->fmcache.mmax;

	if ( dir->fmcache.msize == dir->fmcache.mmax ) /* cache is full */
	{	/* Prefer an entry with num_chars == 0, if any. */
		int count;
		for ( count = dir->fmcache.mmax;
		      --count >= 0 && pair->num_chars != 0;
		    )
		  if ( ++pair == mend )
		    pair = dir->fmcache.mdata;
		gs_purge_fm_pair(dir, pair, 0);
	}
	else
	{	/* Look for an empty entry.  (We know there is one.) */
		while ( !fm_pair_is_free(pair) )
		  if ( ++pair == mend )
		    pair = dir->fmcache.mdata;
	}
	dir->fmcache.msize++;
	dir->fmcache.mnext = pair + 1 - dir->fmcache.mdata;
	if ( dir->fmcache.mnext == dir->fmcache.mmax )
		dir->fmcache.mnext = 0;
	pair->font = font;
	pair->UID = *puid;
	/* The OSF/1 compiler doesn't like casting a pointer to */
	/* a shorter int.... */
	pair->hash = (uint)(ulong)pair % 549;	/* scramble bits */
	pair->mxx = pgs->char_tm.xx, pair->mxy = pgs->char_tm.xy;
	pair->myx = pgs->char_tm.yx, pair->myy = pgs->char_tm.yy;
	pair->num_chars = 0;
	pair->xfont_tried = false;
	pair->xfont = 0;
	if_debug8('k', "[k]adding pair 0x%lx: font=0x%lx [%g %g %g %g] UID %ld, 0x%lx\n",
		  (ulong)pair, (ulong)font,
		  pair->mxx, pair->mxy, pair->myx, pair->myy,
		  (long)pair->UID.id, (ulong)pair->UID.xvalues);
	return pair;
}

/* Look up the xfont for a font/matrix pair. */
/* (This is only exported for gxccache.c.) */
void
gx_lookup_xfont(const gs_state *pgs, cached_fm_pair *pair, int encoding_index)
{	gx_device *dev = gs_currentdevice(pgs);
	gx_device *fdev = (*dev_proc(dev, get_xfont_device))(dev);
	gs_font *font = pair->font;
	gx_xfont_procs *procs = (*dev_proc(fdev, get_xfont_procs))(fdev);
	gx_xfont *xf = 0;

	/* We mustn't attempt to use xfonts for stroked characters, */
	/* because such characters go outside their bounding box. */
	if ( procs != 0 && font->PaintType == 0 )
	{	gs_matrix mat;

		mat.xx = pair->mxx, mat.xy = pair->mxy;
		mat.yx = pair->myx, mat.yy = pair->myy;
		mat.tx = 0, mat.ty = 0;
		/* xfonts can outlive their invocations, */
		/* but restore purges them properly. */
		pair->memory = pgs->memory;
		if ( font->key_name.size != 0 )
			xf = lookup_xfont_by_name(fdev, procs,
				&font->key_name, encoding_index,
				pair, &mat);
#define font_name_eq(pfn1,pfn2)\
  ((pfn1)->size == (pfn2)->size &&\
   !memcmp((char *)(pfn1)->chars, (char *)(pfn2)->chars, (pfn1)->size))
		if ( xf == 0 && font->font_name.size != 0 &&
			     /* Avoid redundant lookup */
		     !font_name_eq(&font->font_name, &font->key_name)
		   )
			xf = lookup_xfont_by_name(fdev, procs,
				&font->font_name, encoding_index,
				pair, &mat);
		if ( xf == 0 && font->FontType != ft_composite &&
		     uid_is_valid(&((gs_font_base *)font)->UID)
		   )
		{	/* Look for an original font with the same UID. */
			gs_font_dir *pdir = font->dir;
			gs_font *pfont;

			for ( pfont = pdir->orig_fonts; pfont != 0;
			      pfont = pfont->next
			    )
			{	if ( pfont->FontType != ft_composite &&
				     uid_equal(&((gs_font_base *)pfont)->UID,
					       &((gs_font_base *)font)->UID) &&
				     pfont->key_name.size != 0 &&
				     !font_name_eq(&font->key_name,
					           &pfont->key_name)
				   )
				{	xf = lookup_xfont_by_name(fdev, procs,
						&pfont->key_name,
						encoding_index, pair, &mat);
					if ( xf != 0 )
					  break;
				}
			}
		}
	}
	pair->xfont = xf;
}

/* ------ Internal routines ------ */

/* Purge from the caches all references to a given font/matrix pair, */
/* or just characters that depend on its xfont. */
#define cpair ((cached_fm_pair *)vpair)
private bool
purge_fm_pair_char(cached_char *cc, void *vpair)
{	return cc_pair(cc) == cpair;
}
private bool
purge_fm_pair_char_xfont(cached_char *cc, void *vpair)
{	return cc_pair(cc) == cpair && cpair->xfont == 0 && !cc_has_bits(cc);
}
#undef cpair
void
gs_purge_fm_pair(gs_font_dir *dir, cached_fm_pair *pair, int xfont_only)
{	if_debug2('k', "[k]purging pair 0x%lx%s\n",
		  (ulong)pair, (xfont_only ? " (xfont only)" : ""));
	if ( pair->xfont != 0 )
	  {	(*pair->xfont->common.procs->release)(pair->xfont,
						      pair->memory);
		pair->xfont_tried = false;
		pair->xfont = 0;
	  }
	gx_purge_selected_cached_chars(dir,
				       (xfont_only ? purge_fm_pair_char_xfont :
					purge_fm_pair_char),
				       pair);
	if ( !xfont_only )
	  {
#ifdef DEBUG
		if ( pair->num_chars != 0 )
		  {	lprintf1("Error in gs_purge_fm_pair: num_chars =%d\n",
				 pair->num_chars);
		  }
#endif
		fm_pair_set_free(pair);
		dir->fmcache.msize--;
	  }
}

/* Look up an xfont by name. */
/* The caller must already have done get_xfont_device to get the proper */
/* device to pass as the first argument to lookup_font. */
private gx_xfont *
lookup_xfont_by_name(gx_device *fdev, gx_xfont_procs *procs,
  gs_font_name *pfstr, int encoding_index, const cached_fm_pair *pair,
  const gs_matrix *pmat)
{	gx_xfont *xf;
	if_debug5('k', "[k]lookup xfont %s [%g %g %g %g]\n",
		  pfstr->chars, pmat->xx, pmat->xy, pmat->yx, pmat->yy);
	xf = (*procs->lookup_font)(fdev,
				   &pfstr->chars[0], pfstr->size,
				   encoding_index, &pair->UID,
				   pmat, pair->memory);
	if_debug1('k', "[k]... xfont=0x%lx\n", (ulong)xf);
	return xf;
}

/* ====== Character-level routines ====== */

/*
 * Allocate storage for caching a rendered character with possible
 * oversampling and/or alpha.  Return the cached_char if OK, 0 if too big.
 * If the character is being oversampled, make the size decision
 * on the basis of the final (scaled-down) size.
 *
 * The iwidth and iheight parameters include scaling up for oversampling
 * (multiplication by 1 << pscale->{x,y}.)
 * The depth parameter is the final number of alpha bits;
 * depth <= x scale * y scale.
 * If dev == NULL, this is an xfont-only entry.
 * If dev != NULL, set up the memory device(s); in this case, if dev2 is
 * not NULL, dev should be an alpha-buffer device with dev2 (an alpha
 * device) as target.
 */
cached_char *
gx_alloc_char_bits(gs_font_dir *dir, gx_device_memory *dev,
  gx_device_memory *dev2, ushort iwidth, ushort iheight,
  const gs_log2_scale_point *pscale, int depth)
{	int log2_xscale = pscale->x;
	int log2_yscale = pscale->y;
	int log2_depth = depth >> 1;	/* works for 1,2,4 */
	uint nwidth_bits = (iwidth >> log2_xscale) << log2_depth;
	ulong isize, icdsize;
	uint iraster;
	cached_char *cc;
	gx_device_memory mdev;
	gx_device_memory *pdev = dev;
	gx_device_memory *pdev2;

	if ( dev == NULL )
	{	mdev.memory = 0;
		mdev.target = 0;
		pdev = &mdev;
	}
	pdev2 = (dev2 == 0 ? pdev : dev2);

	/* Compute the scaled-down bitmap size, and test against */
	/* the maximum cachable character size. */

	iraster = bitmap_raster(nwidth_bits);
	if ( iraster != 0 && iheight >> log2_yscale > dir->ccache.upper / iraster )
	{	if_debug5('k', "[k]no cache bits: scale=%dx%d, raster/scale=%u, height/scale=%u, upper=%u\n",
			  1 << log2_xscale, 1 << log2_yscale,
			  iraster, iheight, dir->ccache.upper);
		return 0;		/* too big */
	}

	/* Compute the actual bitmap size(s) and allocate the bits. */

	if ( dev2 == 0 )
	  {	/* Render to a full (possibly oversampled) bitmap; */
		/* compress (if needed) when done. */
		gs_make_mem_mono_device(pdev, pdev->memory, pdev->target);
		pdev->width = iwidth;
		pdev->height = iheight;
		isize = gdev_mem_bitmap_size(pdev);
	  }
	else
	  {	/* Use an alpha-buffer device to compress as we go. */
		gs_make_mem_alpha_device(dev2, dev2->memory, NULL, depth);
		dev2->width = iwidth >> log2_xscale;
		dev2->height = iheight >> log2_yscale;
		gs_make_mem_abuf_device(dev, dev->memory, (gx_device *)dev2,
					pscale, depth, 0);
		dev->width = iwidth;
		dev->height = 2 << log2_yscale;
		isize = gdev_mem_bitmap_size(dev) +
		  gdev_mem_bitmap_size(dev2);
	  }
	icdsize = isize + sizeof_cached_char;
	cc = alloc_char(dir, icdsize);
	if ( cc == 0 )
	  return 0;
	if_debug4('k', "[k]adding char 0x%lx:%u(%u,%u)\n",
		  (ulong)cc, (uint)icdsize, iwidth, iheight);

	/* Fill in the entry. */

	cc_set_depth(cc, depth);
	cc->xglyph = gx_no_xglyph;
	/* Set the width and height to those of the device. */
	/* Note that if we are oversampling without an alpha buffer. */
	/* these are not the final unscaled dimensions. */
	cc->width = pdev2->width;
	cc->height = pdev2->height;
	cc->shift = 0;
	cc_set_raster(cc, gdev_mem_raster(pdev2));
	cc_set_pair_only(cc, 0);	/* not linked in yet */
	cc->id = gx_no_bitmap_id;

	/* Open the cache device(s). */

	if ( dev2 )
	  {	/* The second device is an alpha device that targets */
		/* the real storage for the character. */
		byte *bits = cc_bits(cc);
		uint bsize = (uint)gdev_mem_bitmap_size(dev2);

		memset(bits, 0, bsize);
		dev2->base = bits;
		(*dev_proc(dev2, open_device))((gx_device *)dev2);
		dev->base = bits + bsize;
		(*dev_proc(dev, open_device))((gx_device *)dev);
	  }
	else if ( dev )
	  gx_open_cache_device(dev, cc);

	return cc;
}

/* Open the cache device. */
void
gx_open_cache_device(gx_device_memory *dev, cached_char *cc)
{	byte *bits = cc_bits(cc);

	dev->width = cc->width;
	dev->height = cc->height;
	memset((char *)bits, 0, (uint)gdev_mem_bitmap_size(dev));
	dev->base = bits;
	(*dev_proc(dev, open_device))((gx_device *)dev);	/* initialize */
}

/* Remove a character from the cache. */
void
gx_free_cached_char(gs_font_dir *dir, cached_char *cc)
{	char_cache_chunk *cck = cc->chunk;

	dir->ccache.chunks = cck;
	dir->ccache.cnext = (byte *)cc - cck->data;
	if ( cc_pair(cc) != 0 )
	   {	/* might be allocated but not added to table yet */
		cc_pair(cc)->num_chars--;
	   }
	if_debug2('k', "[k]freeing char 0x%lx, pair=0x%lx\n",
		  (ulong)cc, (ulong)cc_pair(cc));
	gx_bits_cache_free((gx_bits_cache *)&dir->ccache, &cc->head, cck);
}

/* Add a character to the cache */
void
gx_add_cached_char(gs_font_dir *dir, gx_device_memory *dev,
  cached_char *cc, cached_fm_pair *pair, const gs_log2_scale_point *pscale)
{	if_debug5('k', "[k]chaining char 0x%lx: pair=0x%lx, glyph=0x%lx, wmode=%d, depth=%d\n",
		  (ulong)cc, (ulong)pair, (ulong)cc->code,
		  cc->wmode, cc_depth(cc));
	if ( dev != NULL )
	  {	static const gs_log2_scale_point no_scale = { 0, 0 };
		/* Close the device, to flush the alpha buffer if any. */
		(*dev_proc(dev, close_device))((gx_device *)dev);
		gx_add_char_bits(dir, cc,
				 (gs_device_is_abuf((gx_device *)dev) ?
				  &no_scale : pscale));
	  }
	/* Add the new character to the hash table. */
	{	uint chi = chars_head_index(cc->code, pair);
		while ( dir->ccache.table[chi &= dir->ccache.table_mask] != 0 )
		  chi++;
		dir->ccache.table[chi] = cc;
		cc_set_pair(cc, pair);
		pair->num_chars++;
	}
}

/* Adjust the bits of a newly-rendered character, by unscaling */
/* and compressing or converting to alpha values if necessary. */
void
gx_add_char_bits(gs_font_dir *dir, cached_char *cc,
  const gs_log2_scale_point *plog2_scale)
{	int log2_x = plog2_scale->x, log2_y = plog2_scale->y;
	uint raster = cc_raster(cc);
	byte *bits = cc_bits(cc);
	int depth = cc_depth(cc);
	int log2_depth = depth >> 1;	/* works for 1,2,4 */
	uint nwidth_bits, nraster;
	gs_int_rect bbox;

#ifdef DEBUG
	if ( cc->width % (1 << log2_x) != 0 ||
	     cc->height % (1 << log2_y) != 0
	   )
	  {	lprintf4("size %d,%d not multiple of scale %d,%d!\n",
			 cc->width, cc->height,
			 1 << log2_x, 1 << log2_y);
		cc->width &= -1 << log2_x;
		cc->height &= -1 << log2_y;
	  }
#endif

	/*
	 * Compute the bounding box before compressing.
	 * We may have to scan more bits, but this is a lot faster than
	 * compressing the white space.  Note that all bbox values are
	 * in bits, not pixels.
	 */

	bits_bounding_box(bits, cc->height, raster, &bbox);

	/*
	 * If the character was oversampled, compress it now.
	 * In this case we know that log2_depth <= log2_x.
	 * If the character was not oversampled, or if we converted
	 * oversampling to alpha dynamically (using an alpha buffer
	 * intermediate device), log2_x and log2_y are both zero,
	 * but in the latter case we may still have depth > 1.
	 */

	if ( (log2_x | log2_y) != 0 )
	  {	if_debug5('k', "[k]compressing %dx%d by %dx%d to depth=%d\n",
			  cc->width, cc->height, 1 << log2_x, 1 << log2_y,
			  depth);
		if ( gs_debug_c('K') )
		  debug_dump_bitmap(bits, raster, cc->height,
				    "[K]uncompressed bits");
		/* Truncate/round the bbox to a multiple of the scale. */
		{ int scale_x = 1 << log2_x;
		  bbox.p.x &= -scale_x;
		  bbox.q.x = (bbox.q.x + scale_x - 1) & -scale_x;
		}
		{ int scale_y = 1 << log2_y;
		  bbox.p.y &= -scale_y;
		  bbox.q.y = (bbox.q.y + scale_y - 1) & -scale_y;
		}
		cc->width = (bbox.q.x - bbox.p.x) >> log2_x;
		cc->height = (bbox.q.y - bbox.p.y) >> log2_y;
		nwidth_bits = cc->width << log2_depth;
		nraster = bitmap_raster(nwidth_bits);
		bits_compress_scaled(bits + raster * bbox.p.y, bbox.p.x,
				     cc->width << log2_x,
				     cc->height << log2_y,
				     raster,
				     bits, nraster, plog2_scale, log2_depth);
		bbox.p.x >>= log2_x;
		bbox.p.y >>= log2_y;
	  }
	else
	  {	/* No oversampling, just remove white space. */
		const byte *from = bits + raster * bbox.p.y + (bbox.p.x >> 3);

		cc->height = bbox.q.y - bbox.p.y;
		/*
		 * We'd like to trim off left and right blank space,
		 * but currently we're only willing to move bytes, not bits.
		 * (If we ever want to do better, we must remember that
		 * we can only trim whole pixels, and a pixel may occupy
		 * more than one bit.)
		 */
		bbox.p.x &= ~7;			/* adjust to byte boundary */
		bbox.p.x >>= log2_depth;	/* bits => pixels */
		bbox.q.x = (bbox.q.x + depth - 1) >> log2_depth;  /* ditto */
		cc->width = bbox.q.x - bbox.p.x;
		nwidth_bits = cc->width << log2_depth;
		nraster = bitmap_raster(nwidth_bits);
		if ( bbox.p.x != 0 || nraster != raster )
		  {	/* Move the bits down and over. */
			byte *to = bits;
			uint n = cc->height;
			/* We'd like to move only 
				uint nbytes = (nwidth_bits + 7) >> 3;
			 * bytes per scan line, but unfortunately this drops
			 * the guaranteed zero padding at the end.
			 */

			for ( ; n--; from += raster, to += nraster )
			  memmove(to, from, /*nbytes*/nraster);
		  }
		else if ( bbox.p.y != 0 )
		  {	/* Just move the bits down. */
			memmove(bits, from, raster * cc->height);
		  }
	  }		

	/* Adjust the offsets to account for removed white space. */

	cc->offset.x -= int2fixed(bbox.p.x);
	cc->offset.y -= int2fixed(bbox.p.y);

	/* Discard the memory device overhead that follows the bits, */
	/* and any space reclaimed from unscaling or compression. */

	cc_set_raster(cc, nraster);
	{	uint diff = round_down(cc->head.size - sizeof_cached_char -
				         nraster * cc->height,
				       align_cached_char_mod);

		if ( diff >= sizeof(cached_char_head) )
		{	shorten_cached_char(dir, cc, diff);
			if_debug2('K', "[K]shortening char 0x%lx by %u (adding)\n",
				  (ulong)cc, diff);
		}
	}

	/* Assign a bitmap id. */

	cc->id = gs_next_ids(1);
}

/* Purge from the caches all references to a given font. */
void
gs_purge_font_from_char_caches(gs_font_dir *dir, const gs_font *font)
{	cached_fm_pair *pair = dir->fmcache.mdata;
	int count = dir->fmcache.mmax;

	if_debug1('k', "[k]purging font 0x%lx\n",
		  (ulong)font);
	while ( count-- )
	{	if ( pair->font == font )
		{	if ( uid_is_valid(&pair->UID) )
			{	/* Keep the entry. */
				pair->font = 0;
			}
			else
				gs_purge_fm_pair(dir, pair, 0);
		}
		pair++;
	}
}

/* ------ Internal routines ------ */

/* Allocate data space for a cached character, adding a new chunk if needed. */
private cached_char *
alloc_char(gs_font_dir *dir, ulong icdsize)
{	/* Try allocating at the current position first. */
	cached_char *cc = alloc_char_in_chunk(dir, icdsize);

	if ( cc == 0 )
	{	if ( dir->ccache.bspace < dir->ccache.bmax )
		{	/* Allocate another chunk. */
			char_cache_chunk *cck_prev = dir->ccache.chunks;
			char_cache_chunk *cck;
			uint cksize = dir->ccache.bmax / 5 + 1;
			uint tsize = dir->ccache.bmax - dir->ccache.bspace;
			byte *cdata;

			if ( cksize > tsize )
			  cksize = tsize;
			if ( icdsize + sizeof(cached_char_head) > cksize )
			{	if_debug2('k', "[k]no cache bits: cdsize+head=%lu, cksize=%u\n",
					  icdsize + sizeof(cached_char_head),
					  cksize);
				return 0;		/* wouldn't fit */
			}
			cck = (char_cache_chunk *)gs_malloc(1, sizeof(*cck),
							"char cache chunk");
			if ( cck == 0 )
			  return 0;
			cdata = (byte *)gs_malloc(cksize, 1,
						  "char cache chunk");
			if ( cdata == 0 )
			{	gs_free((char *)cck, 1, sizeof(*cck),
					"char cache chunk");
				return 0;
			}
			gx_bits_cache_chunk_init(cck, cdata, cksize);
			cck->next = cck_prev->next;
			cck_prev->next = cck;
			dir->ccache.bspace += cksize;
			dir->ccache.chunks = cck;
		}
		else
		{	/* Cycle through existing chunks. */
			char_cache_chunk *cck_init = dir->ccache.chunks;
			char_cache_chunk *cck = cck_init;
			while ( (dir->ccache.chunks = cck = cck->next) != cck_init )
			  {	dir->ccache.cnext = 0;
				cc = alloc_char_in_chunk(dir, icdsize);
				if ( cc != 0 )
				  return cc;
			  }
		}
		dir->ccache.cnext = 0;
		cc = alloc_char_in_chunk(dir, icdsize);
	}
	return cc;
}

/* Allocate a character in the current chunk. */
private cached_char *
alloc_char_in_chunk(gs_font_dir *dir, ulong icdsize)
{	char_cache_chunk *cck = dir->ccache.chunks;
	cached_char_head *cch;
#define cc ((cached_char *)cch)
	int code;

	while ( (code = gx_bits_cache_alloc((gx_bits_cache *)&dir->ccache,
					    icdsize, &cch)) < 0
	      )
	  {	if ( cch == 0 )
		  {	/* Not enough room to allocate in this chunk. */
			return 0;
		  }
		{	/* Free the character */
			cached_fm_pair *pair = cc_pair(cc);

			if ( pair != 0 )
			  {	uint chi = chars_head_index(cc->code, pair);
				while ( dir->ccache.table[chi & dir->ccache.table_mask] != cc )
				  chi++;
				hash_remove_cached_char(dir, chi);
			  }
			gx_free_cached_char(dir, cc);
		}
	}
	cc->chunk = cck;
	cc->loc = (byte *)cc - cck->data;
	return cc;
#undef cc
}

/* Remove the cached_char at a given index in the hash table. */
/* In order not to slow down lookup, we relocate following entries. */
private void
hash_remove_cached_char(gs_font_dir *dir, uint chi)
{	uint mask = dir->ccache.table_mask;
	uint from = ((chi &= mask) + 1) & mask;
	cached_char *cc;

	dir->ccache.table[chi] = 0;
	while ( (cc = dir->ccache.table[from]) != 0 )
	  {	/* Loop invariants: chars[chi] == 0; */
		/* chars[chi+1..from] != 0. */
		uint fchi = chars_head_index(cc->code, cc_pair(cc));

		/* If chi <= fchi < from, we relocate the character. */
		/* Note that '<=' must take wraparound into account. */
		if ( (chi < from ? chi <= fchi && fchi < from :
		      chi <= fchi || fchi < from)
		   )
		  {	dir->ccache.table[chi] = cc;
			dir->ccache.table[from] = 0;
			chi = from;
		  }
		from = (from + 1) & mask;
	  }
}

/* Shorten a cached character. */
/* diff >= sizeof(cached_char_head). */
private void
shorten_cached_char(gs_font_dir *dir, cached_char *cc, uint diff)
{	gx_bits_cache_shorten((gx_bits_cache *)&dir->ccache, &cc->head,
			      diff, cc->chunk);
	if_debug2('K', "[K]shortening creates free block 0x%lx(%u)\n",
		  (ulong)((byte *)cc + cc->head.size), diff);
}
