/* Copyright (C) 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gxclmem.c,v 1.5 2001/02/07 01:25:40 mike Exp $ */
/* RAM-based command list implementation */
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsmalloc.h"		/* for gs_memory_default */
#include "gxclmem.h"
#include <config.h>

/*
 * Based on: memfile.c        Version: 1.4 3/21/95 14:59:33 by Ray Johnston.
 * Copyright assigned to Aladdin Enterprises.
 */

/*****************************************************************************

   This package is more or less optimal for use by the clist routines, with
   a couple of the more likely to change "tuning" parameters given in the
   two macros below -- NEED_TO_COMPRESS and GET_NUM_RAW_BUFFERS. Usually
   the NEED_TO_COMPRESS decision will be deferred as long as possible based
   on some total system free RAM space remaining.

   The data structures are in "memfile.h", and the primary 'tuning' parameter
   is MEMFILE_DATA_SIZE. This should not be too small to keep the overhead
   ratio of the block structures to the clist data small. A value of 16384
   is probably in the ballpark.

   The concept is that a memory based "file" is created initially without
   compression, with index blocks every MEMFILE_DATA_SIZE of the file. The
   primary blocks (used by the memfile_fseek logic) for indexing into the
   file are called 'logical' (LOG_MEMFILE_BLK) and the data in stored in a
   different block called a 'physical' block (PHYS_MEMFILE_BLK). When the
   file is not yet compressed, indicated by (f->phys_curr==NULL), then there
   is one physical block for each logical block. The physical block also has
   the 'data_limit' set to NULL if the data is not compressed. Thus when a
   file is not compressed there is one physical block for each logical block.

COMPRESSION.

   When compression is triggered for a file then all of the blocks except
   the last are compressed.  Compression will result in a physical block
   that holds data for more than one logical block. Each logical block now
   points to the start of compressed data in a physical block with the
   'phys_pdata' pointer. The 'data_limit' pointer in the physical block is
   where the compression logic stopped storing data (as stream data
   compressors are allowed to do). The data for the logical block may span
   to the next physical block. Once physical blocks are compressed, they are
   chained together using the 'link' field.

   The 'f->phys_curr' points to the block being filled by compression, with
   the 'f->wt.ptr' pointing to the last byte filled in the block. These are
   used during subsequent compression when the last logical block of the
   file fills the physical block.

DECOMPRESSION.

   During reading the clist, if the logical block points to an uncompressed
   physical block, then 'memfile_get_pdata' simply sets the 'pdata' and the
   'pdata_end' pointers. If the logical block was compressed, then it may
   still be resident in a cache of decompression buffers. The number of these
   decompression buffers is not critical -- even one is enough, but having
   more may prevent decompressing blocks more than once (a cache_miss). The
   number of decompression buffers, called "raw" buffers, that are attempted
   to allocate can be changed with the GET_NUM_RAW_BUFFERS macro, but no
   error occurs if less than that number can be allocated.

   If the logical block still resides in a decompression cache buffer, then
   the 'raw_block' will identify the block. If the data for a logical block
   only exists in compressed form, then the "tail" of the list of decompression
   buffers is re-used, marking the 'raw_block' of the logical block that was
   previously associated with this data to NULL.

   Whichever raw decompression buffer is accessed is moved to the head of the
   decompression buffer list in order to keep the tail of the list as the
   "least recently used".

   There are some DEBUG global static variables used to count the number of
   cache hits "tot_cache_hits" and the number of times a logical block is
   decompressed "tot_cache_miss". Note that the actual number of cache miss
   events is 'f->log_length/MEMFILE_DATA_SIZE - tot_cache_miss' since we
   assume that every logical block must be decmpressed at least once.

   Empirical results so far indicate that if one cache raw buffer for every
   32 logical blocks, then the hit/miss ratio exceeds 99%. Of course, the
   number of raw buffers should be more than 1 if possible, and in many
   implementations (single threaded), the memory usage does not increase
   during the page output step so almost all of memory can be used for
   these raw buffers to prevent the likelihood of a cache miss.

   Of course, this is dependent on reasonably efficient clist blocking
   during writing which is dependent on the data and on the BufferSpace
   value which determines the number of clist band data buffers available.
   Empirical testing shows that the overall efficiency is best if the
   BufferSpace value is 1,000,000 (as in the original Ghostscript source).
   [Note: I expected to be able to use smaller buffer sizes for some cases,
    but this resulted in a high level of thrashing...RJJ]

LIMITATIONS.

   The most serious limitation is caused by the way 'memfile_fwrite' decides
   to free up and re-initialize a file. If memfile_fwrite is called after
   a seek to any location except the start of the file, then an error is
   issued since logic is not present to properly free up on a partial file.
   This is not a problem as used by the 'clist' logic since rewind is used
   to position to the start of a file when re-using it after an 'erasepage'.

   Since the 'clist' logic always traverses the clist using fseek's to ever
   increasing locations, no optimizations of backward seeks was implemented.
   This would be relatively easy with back chain links or bi-directional
   "X-OR" pointer information to link the logical block chain. The rewind
   function is optimal and moves directly to the start of the file.

********************************************************************************/

/*
   The need to compress should be conditional on the amount of available
   memory, but we don't have a way to communicate this to these routines.
   Instead, we simply start compressing when we've allocated more than
   COMPRESSION_THRESHOLD amount of data.  The threshold should be at
   least as large as the fixed overhead of the compressor plus the
   decompressor, plus the expected compressed size of a block that size.
 */
private const long COMPRESSION_THRESHOLD = 300000;

#define NEED_TO_COMPRESS(f)\
  ((f)->ok_to_compress && (f)->total_space > COMPRESSION_THRESHOLD)

   /* FOR NOW ALLOCATE 1 raw buffer for every 32 blocks (at least 8)    */
#define GET_NUM_RAW_BUFFERS( f ) 					\
	 max(f->log_length/MEMFILE_DATA_SIZE/32, 8)

#define MALLOC(f, siz, cname)\
  (void *)gs_alloc_bytes((f)->data_memory, siz, cname)
#define FREE(f, obj, cname)\
  (gs_free_object((f)->data_memory, obj, cname),\
   (f)->total_space -= sizeof(*(obj)))

/* Structure descriptor for GC */
private_st_MEMFILE();

	/* forward references */
private void memfile_free_mem(P1(MEMFILE * f));
private int memfile_init_empty(P1(MEMFILE * f));

/************************************************/
/*   #define DEBUG      /- force statistics -/  */
/************************************************/

#ifdef DEBUG
long tot_compressed;
long tot_raw;
long tot_cache_miss;
long tot_cache_hits;
long tot_swap_out;

/*
   The following pointers are here only for helping with a dumb debugger
   that can't inspect local variables!
 */
byte *decomp_wt_ptr0, *decomp_wt_limit0;
const byte *decomp_rd_ptr0, *decomp_rd_limit0;
byte *decomp_wt_ptr1, *decomp_wt_limit1;
const byte *decomp_rd_ptr1, *decomp_rd_limit1;

#endif

/* ----------------------------- Memory Allocation --------------------- */
void *	/* allocated memory's address, 0 if failure */
allocateWithReserve(
         MEMFILE  *f,			/* file to allocate mem to */
         int      sizeofBlock,		/* size of block to allocate */
         int      *return_code,         /* RET 0 ok, -ve GS-style error, or +1 if OK but low memory */
	 const   char     *allocName,		/* name to allocate by */
	 const   char     *errorMessage         /* error message to print */
)
{
    int code = 0;	/* assume success */
    void *block = MALLOC(f, sizeofBlock, allocName);

    if (block == NULL) {
	/* Try to recover block from reserve */
	if (sizeofBlock == sizeof(LOG_MEMFILE_BLK)) {
	    if (f->reserveLogBlockCount > 0) {
		block = f->reserveLogBlockChain;
		f->reserveLogBlockChain = f->reserveLogBlockChain->link;
		--f->reserveLogBlockCount;
	    }
	} else if (sizeofBlock == sizeof(PHYS_MEMFILE_BLK) ||
		   sizeofBlock == sizeof(RAW_BUFFER)
		   ) {
	    if (f->reservePhysBlockCount > 0) {
		block = f->reservePhysBlockChain;
		f->reservePhysBlockChain = f->reservePhysBlockChain->link;
		--f->reservePhysBlockCount;
	    }
	}
	if (block != NULL)
	    code = 1;	/* successful, but allocated from reserve */
    }
    if (block != NULL)
	f->total_space += sizeofBlock;
    else
	code = gs_note_error(gs_error_VMerror);
    *return_code = code;
    return block;
}   

/* ---------------- Open/close/unlink ---------------- */

int
memfile_fopen(char fname[gp_file_name_sizeof], const char *fmode,
	      clist_file_ptr /*MEMFILE * */  * pf,
	      gs_memory_t *mem, gs_memory_t *data_mem, bool ok_to_compress)
{
    MEMFILE *f = 0;
    int code = 0;

    /* We don't implement reopening an existing file. */
    if (fname[0] != 0 || fmode[0] != 'w') {
	code = gs_note_error(gs_error_invalidfileaccess);
	goto finish;
    }

    /* There is no need to set fname in this implementation, */
    /* but we do it anyway. */
    fname[0] = (ok_to_compress ? 'a' : 'b');
    fname[1] = 0;

    f = gs_alloc_struct(mem, MEMFILE, &st_MEMFILE,
			"memfile_open_scratch(MEMFILE)");
    if (f == NULL) {
	eprintf1("memfile_open_scratch(%s): gs_alloc_struct failed\n", fname);
	code = gs_note_error(gs_error_VMerror);
	goto finish;
    }
    f->memory = mem;
    f->data_memory = data_mem;
    /* init an empty file, BEFORE allocating de/compress state */
    f->compress_state = 0;	/* make clean for GC, or alloc'n failure */
    f->decompress_state = 0;
    f->total_space = 0;
    f->reservePhysBlockChain = NULL;
    f->reservePhysBlockCount = 0;
    f->reserveLogBlockChain = NULL;
    f->reserveLogBlockCount = 0;
    /* init an empty file           */
    if ((code = memfile_init_empty(f)) < 0)
	goto finish;
    if ((code = memfile_set_memory_warning(f, 0)) < 0)
	goto finish;
    /*
     * Disregard the ok_to_compress flag, since the size threshold gives us
     * a much better criterion for deciding when compression is appropriate.
     */
#ifdef HAVE_LIBZ
    f->ok_to_compress = /*ok_to_compress */ true;
#else
    f->ok_to_compress = false;
#endif /* HAVE_LIBZ */
    f->compress_state = 0;	/* make clean for GC */
    f->decompress_state = 0;
    if (f->ok_to_compress) {
	const stream_state *compress_proto = clist_compressor_state(NULL);
	const stream_state *decompress_proto = clist_decompressor_state(NULL);
	const stream_template *compress_template = compress_proto->template;
	const stream_template *decompress_template = decompress_proto->template;

	f->compress_state =
	    gs_alloc_struct(mem, stream_state, compress_template->stype,
			    "memfile_open_scratch(compress_state)");
	f->decompress_state =
	    gs_alloc_struct(mem, stream_state, decompress_template->stype,
			    "memfile_open_scratch(decompress_state)");
	if (f->compress_state == 0 || f->decompress_state == 0) {
	    eprintf1("memfile_open_scratch(%s): gs_alloc_struct failed\n", fname);
	    code = gs_note_error(gs_error_VMerror);
	    goto finish;
	}
	memcpy(f->compress_state, compress_proto,
	       gs_struct_type_size(compress_template->stype));
	f->compress_state->memory = mem;
	memcpy(f->decompress_state, decompress_proto,
	       gs_struct_type_size(decompress_template->stype));
	f->decompress_state->memory = mem;
	if (compress_template->set_defaults)
	    (*compress_template->set_defaults) (f->compress_state);
	if (decompress_template->set_defaults)
	    (*decompress_template->set_defaults) (f->decompress_state);
    }
    f->total_space = 0;

#ifdef DEBUG
    /* If this is the start, init some statistics.       */
    /* Hack: we know the 'a' file is opened first. */
    if (*fname == 'a') {
	tot_compressed = 0;
	tot_raw = 0;
	tot_cache_miss = 0;
	tot_cache_hits = 0;
	tot_swap_out = 0;
    }
#endif
finish:
    if (code < 0) {
	/* return failure, clean up memory before leaving */
	if (f != NULL)
	    memfile_fclose((clist_file_ptr)f, fname, true);
    } else {
      /* return success */
      *pf = f;
    }
    return code;
}

int
memfile_fclose(clist_file_ptr cf, const char *fname, bool delete)
{
    MEMFILE *const f = (MEMFILE *)cf;

    /* We don't implement closing without deletion. */
    if (!delete)
	return_error(gs_error_invalidfileaccess);
    memfile_free_mem(f);

    /* Free reserve blocks; don't do it in memfile_free_mem because */
    /* that routine gets called to reinit file */
    while (f->reserveLogBlockChain != NULL) {
	LOG_MEMFILE_BLK *block = f->reserveLogBlockChain;

	f->reserveLogBlockChain = block->link;
	FREE(f, block, "memfile_set_block_size");
    }
    while (f->reservePhysBlockChain != NULL) {
	PHYS_MEMFILE_BLK *block = f->reservePhysBlockChain;

	f->reservePhysBlockChain = block->link;
	FREE(f, block, "memfile_set_block_size");
    }

    /* deallocate de/compress state */
    gs_free_object(f->memory, f->decompress_state,
		   "memfile_close_and_unlink(decompress_state)");
    gs_free_object(f->memory, f->compress_state,
		   "memfile_close_and_unlink(compress_state)");

    /* deallocate the memfile object proper */
    gs_free_object(f->memory, f, "memfile_close_and_unlink(MEMFILE)");
    return 0;
}

int
memfile_unlink(const char *fname)
{
    /*
     * Since we have no way to represent a memfile other than by the
     * pointer, we don't (can't) implement unlinking.
     */
    return_error(gs_error_invalidfileaccess);
}

/* ---------------- Writing ---------------- */

/* Pre-alloc enough reserve mem blox to guarantee a write of N bytes will succeed */
int	/* returns 0 ok, gs_error_VMerror if insufficient */
memfile_set_memory_warning(clist_file_ptr cf, int bytes_left)
{
    MEMFILE *const f = (MEMFILE *)cf;
    int code = 0;
    /*
     * Determine req'd memory block count from bytes_left.
     * Allocate enough phys & log blocks to hold bytes_left
     * + 1 phys blk for compress_log_blk + 1 phys blk for decompress.
     */
    int logNeeded =
	(bytes_left + MEMFILE_DATA_SIZE - 1) / MEMFILE_DATA_SIZE;
    int physNeeded = logNeeded;

    if (bytes_left > 0)
	++physNeeded;
    if (f->raw_head == NULL)
	++physNeeded;	/* have yet to allocate read buffers */

    /* Allocate or free memory depending on need */
    while (logNeeded > f->reserveLogBlockCount) {
	LOG_MEMFILE_BLK *block =
	    MALLOC( f, sizeof(LOG_MEMFILE_BLK), "memfile_set_block_size" );

	if (block == NULL) {
	    code = gs_note_error(gs_error_VMerror);
	    goto finish;
	}
	block->link = f->reserveLogBlockChain;
	f->reserveLogBlockChain = block;
	++f->reserveLogBlockCount;
    }
    while (logNeeded < f->reserveLogBlockCount) {
	LOG_MEMFILE_BLK *block = f->reserveLogBlockChain;

	f->reserveLogBlockChain = block->link;
	FREE(f, block, "memfile_set_block_size");
	--f->reserveLogBlockCount;
    }
    while (physNeeded > f->reservePhysBlockCount) {
	PHYS_MEMFILE_BLK *block =
	    MALLOC( f,
		    max( sizeof(PHYS_MEMFILE_BLK), sizeof(RAW_BUFFER) ),
		    "memfile_set_block_size");

	if (block == NULL) {
	    code = gs_note_error(gs_error_VMerror);
	    goto finish;
	}
	block->link = f->reservePhysBlockChain;
	f->reservePhysBlockChain = block;
	++f->reservePhysBlockCount;
    }
    while (physNeeded < f->reservePhysBlockCount) {
	PHYS_MEMFILE_BLK *block = f->reservePhysBlockChain;

	f->reservePhysBlockChain = block->link;
	FREE(f, block, "memfile_set_block_size");
	--f->reservePhysBlockCount;
    }
    f->error_code = 0;	/* memfile_set_block_size is how user resets this */
finish:
    return code;
}

private int
compress_log_blk(MEMFILE * f, LOG_MEMFILE_BLK * bp)
{
    int status;
    int ecode = 0;		/* accumulate low-memory warnings */
    int code;
    long compressed_size;
    byte *start_ptr;
    PHYS_MEMFILE_BLK *newphys;

    /* compress this block */
    f->rd.ptr = (const byte *)(bp->phys_blk->data) - 1;
    f->rd.limit = f->rd.ptr + MEMFILE_DATA_SIZE;

    bp->phys_blk = f->phys_curr;
    bp->phys_pdata = (char *)(f->wt.ptr) + 1;
    if (f->compress_state->template->reinit != 0)
	(*f->compress_state->template->reinit)(f->compress_state);
    compressed_size = 0;

    start_ptr = f->wt.ptr;
    status = (*f->compress_state->template->process)(f->compress_state,
						     &(f->rd), &(f->wt), true);
    bp->phys_blk->data_limit = (char *)(f->wt.ptr);

    if (status == 1) {		/* More output space needed (see strimpl.h) */
	/* allocate another physical block, then compress remainder       */
	compressed_size = f->wt.limit - start_ptr;
	newphys =
	    allocateWithReserve(f, sizeof(*newphys), &code, "memfile newphys",
			"compress_log_blk : MALLOC for 'newphys' failed\n");
	if (code < 0)
	    return code;
	ecode |= code;	/* accumulate any low-memory warnings */
	newphys->link = NULL;
	bp->phys_blk->link = newphys;
	f->phys_curr = newphys;
	f->wt.ptr = (byte *) (newphys->data) - 1;
	f->wt.limit = f->wt.ptr + MEMFILE_DATA_SIZE;

	start_ptr = f->wt.ptr;
	status =
	    (*f->compress_state->template->process)(f->compress_state,
						    &(f->rd), &(f->wt), true);
	if (status != 0) {
	    /*
	     * You'd think the above line is a bug, but in real life 1 src
	     * block never ends up getting split across 3 dest blocks.
	     */
	    /* CHANGE memfile_set_memory_warning if this assumption changes. */
	    eprintf("Compression required more than one full block!\n");
	    return_error(gs_error_Fatal);
	}
	newphys->data_limit = (char *)(f->wt.ptr);
    }
    compressed_size += f->wt.ptr - start_ptr;
    if (compressed_size > MEMFILE_DATA_SIZE) {
	eprintf2("\nCompression didn't - raw=%d, compressed=%ld\n",
		 MEMFILE_DATA_SIZE, compressed_size);
    }
#ifdef DEBUG
    tot_compressed += compressed_size;
#endif
    return (status < 0 ? gs_note_error(gs_error_ioerror) : ecode);
}				/* end "compress_log_blk()"                                     */

/*      Internal (private) routine to handle end of logical block       */
private int	/* ret 0 ok, -ve error, or +ve low-memory warning */
memfile_next_blk(MEMFILE * f)
{
    LOG_MEMFILE_BLK *bp = f->log_curr_blk;
    LOG_MEMFILE_BLK *newbp;
    PHYS_MEMFILE_BLK *newphys, *oldphys;
    int ecode = 0;		/* accumulate low-memory warnings */
    int code;

    if (f->phys_curr == NULL) {	/* means NOT compressing                */
	/* allocate a new block                                           */
	newphys =
	    allocateWithReserve(f, sizeof(*newphys), &code, "memfile newphys",
			"memfile_next_blk: MALLOC 1 for 'newphys' failed\n");
	if (code < 0)
	    return code;
	ecode |= code;	/* accumulate low-mem warnings */
	newphys->link = NULL;
	newphys->data_limit = NULL;	/* raw                          */

	newbp =
	    allocateWithReserve(f, sizeof(*newbp), &code, "memfile newbp",
			"memfile_next_blk: MALLOC 1 for 'newbp' failed\n");
	if (code < 0) {
	    FREE(f, newphys, "memfile newphys");
	    return code;
	}
	ecode |= code;	/* accumulate low-mem warnings */
	bp->link = newbp;
	newbp->link = NULL;
	newbp->raw_block = NULL;
	f->log_curr_blk = newbp;

#ifdef HAVE_LIBZ
	/* check if need to start compressing                             */
	if (NEED_TO_COMPRESS(f)) {
	    if_debug0(':', "[:]Beginning compression\n");
	    /* compress the entire file up to this point                   */
	    if (!f->compressor_initialized) {
		int code = 0;

		if (f->compress_state->template->init != 0)
		    code = (*f->compress_state->template->init) (f->compress_state);
		if (code < 0)
		    return_error(gs_error_VMerror);  /****** BOGUS ******/
		if (f->decompress_state->template->init != 0)
		    code = (*f->decompress_state->template->init)
			(f->decompress_state);
		if (code < 0)
		    return_error(gs_error_VMerror);  /****** BOGUS ******/
		f->compressor_initialized = true;
	    }
	    /* Write into the new physical block we just allocated,        */
	    /* replace it after the loop (after some blocks are freed)     */
	    f->phys_curr = newphys;
	    f->wt.ptr = (byte *) (newphys->data) - 1;
	    f->wt.limit = f->wt.ptr + MEMFILE_DATA_SIZE;
	    bp = f->log_head;
	    while (bp != newbp) {	/* don't compress last block    */
		int code;

		oldphys = bp->phys_blk;
		if ((code = compress_log_blk(f, bp)) < 0)
		    return code;
		ecode |= code;
		FREE(f, oldphys, "memfile_next_blk(oldphys)");
		bp = bp->link;
	    }			/* end while( ) compress loop                           */
	    /* Allocate a physical block for this (last) logical block     */
	    newphys =
		allocateWithReserve(f, sizeof(*newphys), &code,
			"memfile newphys",
			"memfile_next_blk: MALLOC 2 for 'newphys' failed\n");
	    if (code < 0)
		return code;
	    ecode |= code;	/* accumulate low-mem warnings */
	    newphys->link = NULL;
	    newphys->data_limit = NULL;		/* raw                  */

	}			/* end convert file to compressed                                 */
#endif /* HAVE_LIBZ */
	newbp->phys_blk = newphys;
	f->pdata = newphys->data;
	f->pdata_end = newphys->data + MEMFILE_DATA_SIZE;
    }    /* end if NOT compressing                                 */
#ifdef HAVE_LIBZ
    /* File IS being compressed                                       */ 
    else {
	int code;

	oldphys = bp->phys_blk;	/* save raw phys block ID               */
	/* compresses bp on phys list  */
	if ((code = compress_log_blk(f, bp)) < 0)
	    return code;
	ecode |= code;
	newbp =
	    allocateWithReserve(f, sizeof(*newbp), &code, "memfile newbp",
			"memfile_next_blk: MALLOC 2 for 'newbp' failed\n");
	if (code < 0)
	    return code;
	ecode |= code;
	bp->link = newbp;
	newbp->link = NULL;
	newbp->raw_block = NULL;
	/* Re-use the raw phys block for this new logical blk             */
	newbp->phys_blk = oldphys;
	f->pdata = oldphys->data;
	f->pdata_end = f->pdata + MEMFILE_DATA_SIZE;
	f->log_curr_blk = newbp;
    }				/* end else (when we are compressing)                           */
#endif /* HAVE_LIBZ */

    return (0);
}

int	/* returns # of chars actually written */
memfile_fwrite_chars(const void *data, uint len, clist_file_ptr cf)
{
    const char *str = (const char *)data;
    MEMFILE *f = (MEMFILE *) cf;
    uint count = len;
    int ecode;

    /* check if we are writing to the start of the file.  If so, then    */
    /* free the file memory and re-initialize it (frees memory)          */
    if (f->log_curr_pos == 0) {
	int code;

	memfile_free_mem(f);
	if ((code = memfile_init_empty(f)) < 0) {
	    f->error_code = code;
	    return 0;
	}
    }
    if (f->log_curr_blk->link != 0) {
	eprintf(" Write file truncate -- need to free physical blocks.\n");
    }
    while (count) {
	uint move_count = f->pdata_end - f->pdata;

	if (move_count == 0) {
	    if ((ecode = memfile_next_blk(f)) != 0) {
		f->error_code = ecode;
		if (ecode < 0)
		    return 0;
	    }
	} else {
	    if (move_count > count)
		move_count = count;
	    memmove(f->pdata, str, move_count);
	    f->pdata += move_count;
	    str += move_count;
	    count -= move_count;
	}
    }
    f->log_curr_pos += len;
    f->log_length = f->log_curr_pos;	/* truncate length to here      */
#ifdef DEBUG
    tot_raw += len;
#endif
    return (len);
}

/*                                                                      */
/*      Internal routine to set the f->pdata and f->pdata_end pointers  */
/*      for the current logical block f->log_curr_blk                   */
/*                                                                      */
/*      If data only exists in compressed form, allocate a raw buffer   */
/*      and decompress it.                                              */
/*                                                                      */

private int
memfile_get_pdata(MEMFILE * f)
{
    int i, num_raw_buffers, status;
    LOG_MEMFILE_BLK *bp = f->log_curr_blk;

    if (bp->phys_blk->data_limit == NULL) {
	/* Not compressed, return this data pointer                       */
	f->pdata = (bp->phys_blk)->data;
	i = f->log_curr_pos % MEMFILE_DATA_SIZE;	/* pos within block     */
	i = f->log_curr_pos - i;	/* base of block        */
	if (i + MEMFILE_DATA_SIZE > f->log_length)
	    f->pdata_end = f->pdata + f->log_length - i;
	else
	    f->pdata_end = f->pdata + MEMFILE_DATA_SIZE;
    }
#ifdef HAVE_LIBZ
    else {
	/* data was compressed                                            */
	if (f->raw_head == NULL) {
	    /* need to allocate the raw buffer pool                        */
	    num_raw_buffers = GET_NUM_RAW_BUFFERS(f);
	    if (f->reservePhysBlockCount) {
            /* HACK: allocate reserve block that's been reserved for
	     * decompression.  This buffer's block was pre-allocated to make
	     * sure we won't come up short here. Take from chain instead of
	     * allocateWithReserve() since this buf would just be wasted if
	     * allowed to remain preallocated. */
		f->raw_head = (RAW_BUFFER *)f->reservePhysBlockChain;
		f->reservePhysBlockChain = f->reservePhysBlockChain->link;
		--f->reservePhysBlockCount;
	    } else {
		int code;

		f->raw_head =
		    allocateWithReserve(f, sizeof(*f->raw_head), &code,
					"memfile raw buffer",
			"memfile_get_pdata: MALLOC for 'raw_head' failed\n");
		if (code < 0)
		    return code;
	    }
	    f->raw_head->back = NULL;
	    f->raw_tail = f->raw_head;
	    f->raw_tail->log_blk = NULL;
	    for (i = 0; i < num_raw_buffers; i++) {
		f->raw_tail->fwd = (RAW_BUFFER *) MALLOC(f, sizeof(RAW_BUFFER),
						      "memfile raw buffer");
		/* if MALLOC fails, then just stop allocating            */
		if (!f->raw_tail->fwd)
		    break;
		f->total_space += sizeof(RAW_BUFFER);
		f->raw_tail->fwd->back = f->raw_tail;
		f->raw_tail = f->raw_tail->fwd;
		f->raw_tail->log_blk = NULL;
	    }
	    f->raw_tail->fwd = NULL;
	    num_raw_buffers = i + 1;	/* if MALLOC failed, then OK    */
	    if_debug1(':', "[:]Number of raw buffers allocated=%d\n",
		      num_raw_buffers);
	}			/* end allocating the raw buffer pool (first time only)           */
	if (bp->raw_block == NULL) {
#ifdef DEBUG
	    tot_cache_miss++;	/* count every decompress       */
#endif
	    /* find a raw buffer and decompress                            */
	    if (f->raw_tail->log_blk != NULL) {
		/* This block was in use, grab it                           */
#ifdef DEBUG
		tot_swap_out++;
#endif
		f->raw_tail->log_blk->raw_block = NULL;		/* data no longer here */
		f->raw_tail->log_blk = NULL;
	    }
	    /* Use the last raw block in the chain (the oldest)            */
	    f->raw_tail->back->fwd = NULL;	/* disconnect from tail */
	    f->raw_tail->fwd = f->raw_head;	/* new head             */
	    f->raw_head->back = f->raw_tail;
	    f->raw_tail = f->raw_tail->back;
	    f->raw_head = f->raw_head->back;
	    f->raw_head->back = NULL;
	    f->raw_head->log_blk = bp;

	    /* Decompress the data into this raw block                     */
	    /* Initialize the decompressor                              */
	    if (f->decompress_state->template->reinit != 0)
		(*f->decompress_state->template->reinit) (f->decompress_state);
	    /* Set pointers and call the decompress routine             */
	    f->wt.ptr = (byte *) (f->raw_head->data) - 1;
	    f->wt.limit = f->wt.ptr + MEMFILE_DATA_SIZE;
	    f->rd.ptr = (const byte *)(bp->phys_pdata) - 1;
	    f->rd.limit = (const byte *)bp->phys_blk->data_limit;
#ifdef DEBUG
	    decomp_wt_ptr0 = f->wt.ptr;
	    decomp_wt_limit0 = f->wt.limit;
	    decomp_rd_ptr0 = f->rd.ptr;
	    decomp_rd_limit0 = f->rd.limit;
#endif
	    status = (*f->decompress_state->template->process)
		(f->decompress_state, &(f->rd), &(f->wt), true);
	    if (status == 0) {	/* More input data needed */
		/* switch to next block and continue decompress             */
		int back_up = 0;	/* adjust pointer backwards     */

		if (f->rd.ptr != f->rd.limit) {
		    /* transfer remainder bytes from the previous block      */
		    back_up = f->rd.limit - f->rd.ptr;
		    for (i = 0; i < back_up; i++)
			*(bp->phys_blk->link->data - back_up + i) = *++f->rd.ptr;
		}
		f->rd.ptr = (const byte *)bp->phys_blk->link->data - back_up - 1;
		f->rd.limit = (const byte *)bp->phys_blk->link->data_limit;
#ifdef DEBUG
		decomp_wt_ptr1 = f->wt.ptr;
		decomp_wt_limit1 = f->wt.limit;
		decomp_rd_ptr1 = f->rd.ptr;
		decomp_rd_limit1 = f->rd.limit;
#endif
		status = (*f->decompress_state->template->process)
		    (f->decompress_state, &(f->rd), &(f->wt), true);
		if (status == 0) {
		    eprintf("Decompression required more than one full block!\n");
		    return_error(gs_error_Fatal);
		}
	    }
	    bp->raw_block = f->raw_head;	/* point to raw block           */
	}
	/* end if( raw_block == NULL ) meaning need to decompress data    */ 
	else {
	    /* data exists in the raw data cache, if not raw_head, move it */
	    if (bp->raw_block != f->raw_head) {
		/*          move to raw_head                                */
		/*          prev.fwd = this.fwd                             */
		bp->raw_block->back->fwd = bp->raw_block->fwd;
		if (bp->raw_block->fwd != NULL)
		    /*               next.back = this.back                   */
		    bp->raw_block->fwd->back = bp->raw_block->back;
		else
		    f->raw_tail = bp->raw_block->back;	/* tail = prev        */
		f->raw_head->back = bp->raw_block;	/* head.back = this     */
		bp->raw_block->fwd = f->raw_head;	/* this.fwd = orig head */
		f->raw_head = bp->raw_block;	/* head = this          */
		f->raw_head->back = NULL;	/* this.back = NULL     */
#ifdef DEBUG
		tot_cache_hits++;	/* counting here prevents repeats since */
		/* won't count if already at head       */
#endif
	    }
	}
	f->pdata = bp->raw_block->data;
	f->pdata_end = f->pdata + MEMFILE_DATA_SIZE;
	/* NOTE: last block is never compressed, so a compressed block    */
	/*        is always full size.                                    */
    }				/* end else (when data was compressed)                             */
#endif /* HAVE_LIBZ */

    return (0);
}

/* ---------------- Reading ---------------- */

int
memfile_fread_chars(void *data, uint len, clist_file_ptr cf)
{
    char *str = (char *)data;
    MEMFILE *f = (MEMFILE *) cf;
    uint count = len, num_read, move_count;

    num_read = f->log_length - f->log_curr_pos;
    if (count > num_read)
	count = num_read;
    num_read = count;

    while (count) {
	f->log_curr_pos++;	/* move into next byte */
	if (f->pdata == f->pdata_end) {
	    f->log_curr_blk = (f->log_curr_blk)->link;
	    memfile_get_pdata(f);
	}
	move_count = f->pdata_end - f->pdata;
	if (move_count > count)
	    move_count = count;
	f->log_curr_pos += move_count - 1;	/* new position         */
	memmove(str, f->pdata, move_count);
	str += move_count;
	f->pdata += move_count;
	count -= move_count;
    }

    return (num_read);
}

/* ---------------- Position/status ---------------- */

int
memfile_ferror_code(clist_file_ptr cf)
{
    return (((MEMFILE *) cf)->error_code);	/* errors stored here */
}

long
memfile_ftell(clist_file_ptr cf)
{
    return (((MEMFILE *) cf)->log_curr_pos);
}

void
memfile_rewind(clist_file_ptr cf, bool discard_data, const char *ignore_fname)
{
    MEMFILE *f = (MEMFILE *) cf;

    if (discard_data) {
	memfile_free_mem(f);
	/* We have to call memfile_init_empty to preserve invariants. */
	memfile_init_empty(f);
    } else {
	f->log_curr_blk = f->log_head;
	f->log_curr_pos = 0;
	memfile_get_pdata(f);
    }
}

int
memfile_fseek(clist_file_ptr cf, long offset, int mode, const char *ignore_fname)
{
    MEMFILE *f = (MEMFILE *) cf;
    long i, block_num, new_pos;

    switch (mode) {
	case SEEK_SET:		/* offset from the beginning of the file */
	    new_pos = offset;
	    break;

	case SEEK_CUR:		/* offset from the current position in the file */
	    new_pos = offset + f->log_curr_pos;
	    break;

	case SEEK_END:		/* offset back from the end of the file */
	    new_pos = f->log_length - offset;
	    break;

	default:
	    return (-1);
    }
    if (new_pos < 0 || new_pos > f->log_length)
	return -1;
    if ((f->pdata == f->pdata_end) && (f->log_curr_blk->link != NULL)) {
	/* log_curr_blk is actually one block behind log_curr_pos         */
	f->log_curr_blk = f->log_curr_blk->link;
    }
    block_num = new_pos / MEMFILE_DATA_SIZE;
    i = f->log_curr_pos / MEMFILE_DATA_SIZE;
    if (block_num < i) {	/* if moving backwards, start at beginning */
	f->log_curr_blk = f->log_head;
	i = 0;
    }
    for (; i < block_num; i++) {
	f->log_curr_blk = f->log_curr_blk->link;
    }
    f->log_curr_pos = new_pos;
    memfile_get_pdata(f);	/* pointers to start of block           */
    f->pdata += new_pos - (block_num * MEMFILE_DATA_SIZE);

    return 0;			/* return "normal" status                       */
}

/* ---------------- Internal routines ---------------- */

private void
memfile_free_mem(MEMFILE * f)
{
    LOG_MEMFILE_BLK *bp, *tmpbp;

#ifdef DEBUG
    /* output some diagnostics about the effectiveness                   */
    if (tot_raw > 100) {
	if_debug2(':', "[:]tot_raw=%ld, tot_compressed=%ld\n",
		  tot_raw, tot_compressed);
    }
    if (tot_cache_hits != 0) {
	if_debug3(':', "[:]Cache hits=%ld, cache misses=%ld, swapouts=%ld\n",
		 tot_cache_hits,
		 tot_cache_miss - (f->log_length / MEMFILE_DATA_SIZE),
		 tot_swap_out);
    }
    tot_raw = 0;
    tot_compressed = 0;
    tot_cache_hits = 0;
    tot_cache_miss = 0;
    tot_swap_out = 0;
#endif

    /* Free up memory that was allocated for the memfile              */
    bp = f->log_head;

/******************************************************************
 * The following was the original algorithm here.  This algorithm has a bug:
 * the second loop references the physical blocks again after they have been
 * freed.
 ******************************************************************/

#if 0				/**************** *************** */

    if (bp != NULL) {
	/* Free the physical blocks that make up the compressed data      */
	PHYS_MEMFILE_BLK *pphys = (f->log_head)->phys_blk;

	if (pphys->data_limit != NULL) {
	    /* the data was compressed, free the chain of blocks             */
	    while (pphys != NULL) {
		PHYS_MEMFILE_BLK *tmpphys = pphys->link;

		FREE(f, pphys, "memfile_free_mem(pphys)");
		pphys = tmpphys;
	    }
	}
    }
    /* free the logical blocks                                        */
    while (bp != NULL) {
	/* if this logical block was not compressed, free the phys_blk  */
	if (bp->phys_blk->data_limit == NULL) {
	    FREE(f, bp->phys_blk, "memfile_free_mem(phys_blk)");
	}
	tmpbp = bp->link;
	FREE(f, bp, "memfile_free_mem(log_blk)");
	bp = tmpbp;
    }

#else /**************** *************** */
# if 1				/**************** *************** */

/****************************************************************
 * This algorithm is correct (we think).
 ****************************************************************/

    if (bp != NULL) {
	/* Null out phys_blk pointers to compressed data. */
	PHYS_MEMFILE_BLK *pphys = bp->phys_blk;

	{
	    for (tmpbp = bp; tmpbp != NULL; tmpbp = tmpbp->link)
		if (tmpbp->phys_blk->data_limit != NULL)
		    tmpbp->phys_blk = 0;
	}
	/* Free the physical blocks that make up the compressed data      */
	if (pphys->data_limit != NULL) {
	    /* the data was compressed, free the chain of blocks             */
	    while (pphys != NULL) {
		PHYS_MEMFILE_BLK *tmpphys = pphys->link;

		FREE(f, pphys, "memfile_free_mem(pphys)");
		pphys = tmpphys;
	    }
	}
    }
    /* Now free the logical blocks, and any uncompressed physical blocks. */
    while (bp != NULL) {
	if (bp->phys_blk != NULL) {
	    FREE(f, bp->phys_blk, "memfile_free_mem(phys_blk)");
	}
	tmpbp = bp->link;
	FREE(f, bp, "memfile_free_mem(log_blk)");
	bp = tmpbp;
    }

/***********************************************************************
 * This algorithm appears to be both simpler and free of the bug that
 * occasionally causes the older one to reference freed blocks; but in
 * fact it can miss blocks, because the very last compressed logical block
 * can have spill into a second physical block, which is not referenced by
 * any logical block.
 ***********************************************************************/

# else				/**************** *************** */

    {
	PHYS_MEMFILE_BLK *prev_phys = 0;

	while (bp != NULL) {
	    PHYS_MEMFILE_BLK *phys = bp->phys_blk;

	    if (phys != prev_phys) {
		FREE(f, phys, "memfile_free_mem(phys_blk)");
		prev_phys = phys;
	    }
	    tmpbp = bp->link;
	    FREE(f, bp, "memfile_free_mem(log_blk)");
	    bp = tmpbp;
	}
    }

# endif				/**************** *************** */
#endif /**************** *************** */

    f->log_head = NULL;

#ifdef HAVE_LIBZ
    /* Free any internal compressor state. */
    if (f->compressor_initialized) {
	if (f->decompress_state->template->release != 0)
	    (*f->decompress_state->template->release) (f->decompress_state);
	if (f->compress_state->template->release != 0)
	    (*f->compress_state->template->release) (f->compress_state);
	f->compressor_initialized = false;
    }
#endif /* HAVE_LIBZ */

    /* free the raw buffers                                           */
    while (f->raw_head != NULL) {
	RAW_BUFFER *tmpraw = f->raw_head->fwd;

	FREE(f, f->raw_head, "memfile_free_mem(raw)");
	f->raw_head = tmpraw;
    }
}

private int
memfile_init_empty(MEMFILE * f)
{
    PHYS_MEMFILE_BLK *pphys;
    LOG_MEMFILE_BLK *plog;

   /* Zero out key fields so that allocation failure will be unwindable */
    f->phys_curr = NULL; 	/* flag as file not compressed    	*/
    f->log_head = NULL;
    f->log_curr_blk = NULL;
    f->log_curr_pos = 0;
    f->log_length = 0;
    f->raw_head = NULL;
    f->compressor_initialized = false;
    f->total_space = 0;

    /* File empty - get a physical mem block (includes the buffer area)  */
    pphys = MALLOC(f, sizeof(*pphys), "memfile pphys");
    if (!pphys) {
	eprintf("memfile_init_empty: MALLOC for 'pphys' failed\n");
	return_error(gs_error_VMerror);
    }
    f->total_space += sizeof(*pphys);
    pphys->data_limit = NULL;	/* raw data for now     */

   /* Get logical mem block to go with physical one */
    plog = (LOG_MEMFILE_BLK *)MALLOC( f, sizeof(*plog), "memfile_init_empty" );
    if (plog == NULL) {
	FREE(f, pphys, "memfile_init_empty");
	eprintf("memfile_init_empty: MALLOC for log_curr_blk failed\n");
	return_error(gs_error_VMerror);
    }
    f->total_space += sizeof(*plog);
    f->log_head = f->log_curr_blk = plog;
    f->log_curr_blk->link = NULL;
    f->log_curr_blk->phys_blk = pphys;
    f->log_curr_blk->phys_pdata = NULL;
    f->log_curr_blk->raw_block = NULL;

    f->pdata = pphys->data;
    f->pdata_end = f->pdata + MEMFILE_DATA_SIZE;

    f->error_code = 0;

    return 0;
}
