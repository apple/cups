/* Copyright (C) 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gxclmem.h */
/* Defines and declarations for clist implementation in  memory. */
#include "gxclio.h"		/* defines interface */
#include "strimpl.h"		/* stream structures	*/
#include "slzwx.h"		/* LZW structures 	*/

#define COMPRESS_TEMPLATE s_LZWE_template
#define DECOMPRESS_TEMPLATE s_LZWD_template

#  define MEMFILE_DATA_SIZE	16384

   /*	============================================================	*/
   /*									*/
   /*	Memfile structure definitions.					*/
   /*									*/
   /*	The PHYS structures are the elements actually allocated in	*/
   /*	RAM, containing the compressed data (or optionally raw data)	*/
   /*									*/
   /*	There can be several LOG (logical) elements per physical	*/
   /*	element, depending on the compression. The MEMFILE pdata	*/
   /*	item always points into a raw block of data.			*/
   /*									*/
   /*	============================================================	*/

   typedef struct RAW_BUFFER {
      struct RAW_BUFFER		*fwd, *back;
      struct LOG_MEMFILE_BLK	*log_blk;
      char			data[MEMFILE_DATA_SIZE];
   } RAW_BUFFER ;

   typedef struct PHYS_MEMFILE_BLK {
      struct PHYS_MEMFILE_BLK	*link;
      char		*data_limit;	/* end of data when compressed	*/
					/* NULL if not compressed	*/
      char		data_spare[4];	/* used during de-compress	*/
      char		data[MEMFILE_DATA_SIZE];
   } PHYS_MEMFILE_BLK ;

   typedef struct LOG_MEMFILE_BLK {
      struct LOG_MEMFILE_BLK	*link;
      PHYS_MEMFILE_BLK		*phys_blk;
      char			*phys_pdata;
      RAW_BUFFER		*raw_block;		/* or NULL */
   } LOG_MEMFILE_BLK ;

   typedef struct MEMFILE {
      gs_memory_t *memory;	/* storage allocator */
      bool ok_to_compress;	/* if true, OK to compress this file */
	/* logical file properties */
      LOG_MEMFILE_BLK	*log_head;
      LOG_MEMFILE_BLK	*log_curr_blk;
      long		log_length;	/* updated during write		    */
      long		log_curr_pos;	/* updated during seek, close, read */
      char		*pdata;	/* raw data */
      char		*pdata_end;
	/* physical file properties */
      PHYS_MEMFILE_BLK	*phys_curr;	/* NULL if not compressing	*/
      RAW_BUFFER	*raw_head, *raw_tail;
      int		error_code;	/* used by CLIST_ferror		*/
      stream_cursor_read  rd;	/* use .ptr, .limit */
      stream_cursor_write wt;	/* use .ptr, .limit */
      bool		compressor_initialized;
      stream_LZW_state	filter_state;
   } MEMFILE ;

/* Make the memfile_... operations aliases for the clist_... operations. */

#define memfile_open_scratch(fname, pcf, mem, compress)\
  clist_open_scratch(fname, pcf, mem, compress)
#define memfile_fclose_and_unlink(cf, fname)\
  clist_fclose_and_unlink(cf, fname)

#define memfile_space_available(req)\
  clist_space_available(req)
#define memfile_fwrite_chars(data, len, cf)\
  clist_fwrite_chars(data, len, cf)

#define memfile_fread_chars(data, len, cf)\
  clist_fread_chars(data, len, cf)

#define memfile_ferror_code(cf) clist_ferror_code(cf)
#define memfile_ftell(cf) clist_ftell(cf)
#define memfile_rewind(cf, discard) clist_rewind(cf, discard)
#define memfile_fseek(cf, offset, mode) clist_fseek(cf, offset, mode)
