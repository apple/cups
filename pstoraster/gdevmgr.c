/* Copyright (C) 1992, 1993, 1994 Aladdin Enterprises.  All rights reserved.
  
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

/* gdevmgr.c */
/* MGR device driver */
#include "gdevprn.h"
#include "gdevpccm.h"
#include "gdevmgr.h"

/* Structure for MGR devices, which extend the generic printer device. */
struct gx_device_mgr_s {
	gx_device_common;
	gx_prn_device_common;
	/* Add MGR specific variables */
	int mgr_depth;
};
typedef struct gx_device_mgr_s gx_device_mgr;

static struct nclut clut[256];

private unsigned int clut2mgr(P2(int, int));
private void swap_bwords(P2(unsigned char *, int));

/* ------ The device descriptors ------ */

/*
 * Default X and Y resolution.
 */
#define X_DPI 72
#define Y_DPI 72

/* Macro for generating MGR device descriptors. */
#define mgr_prn_device(procs, dev_name, num_comp, depth, mgr_depth,\
	max_gray, max_rgb, dither_gray, dither_rgb, print_page)\
{	prn_device_body(gx_device_mgr, procs, dev_name,\
	  DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS, X_DPI, Y_DPI,\
	  0, 0, 0, 0,\
	  num_comp, depth, max_gray, max_rgb, dither_gray, dither_rgb,\
	  print_page),\
	  mgr_depth\
}

/* For all mgr variants we do some extra things at opening time. */
/* private dev_proc_open_device(gdev_mgr_open); */
#define gdev_mgr_open gdev_prn_open		/* no we don't! */

/* And of course we need our own print-page routines. */
private dev_proc_print_page(mgr_print_page);
private dev_proc_print_page(mgrN_print_page);
private dev_proc_print_page(cmgrN_print_page);

/* The device procedures */
private gx_device_procs mgr_procs =
    prn_procs(gdev_mgr_open, gdev_prn_output_page, gdev_prn_close);
private gx_device_procs mgrN_procs =
    prn_color_procs(gdev_mgr_open, gdev_prn_output_page, gdev_prn_close,
	gx_default_gray_map_rgb_color, gx_default_gray_map_color_rgb);
private gx_device_procs cmgr4_procs =
    prn_color_procs(gdev_mgr_open, gdev_prn_output_page, gdev_prn_close,
	pc_4bit_map_rgb_color, pc_4bit_map_color_rgb);
private gx_device_procs cmgr8_procs =
    prn_color_procs(gdev_mgr_open, gdev_prn_output_page, gdev_prn_close,
	pc_8bit_map_rgb_color, pc_8bit_map_color_rgb);

/* The device descriptors themselves */
gx_device_mgr far_data gs_mgrmono_device =
  mgr_prn_device( mgr_procs,  "mgrmono", 1,  1, 1,   1,   0, 2, 0, mgr_print_page);
gx_device_mgr far_data gs_mgrgray2_device =
  mgr_prn_device(mgrN_procs,  "mgrgray2",1,  8, 2, 255,   0, 4, 0, mgrN_print_page);
gx_device_mgr far_data gs_mgrgray4_device =
  mgr_prn_device(mgrN_procs,  "mgrgray4",1,  8, 4, 255,   0,16, 0, mgrN_print_page);
gx_device_mgr far_data gs_mgrgray8_device =
  mgr_prn_device(mgrN_procs,  "mgrgray8",1,  8, 8, 255,   0, 0, 0, mgrN_print_page);
gx_device_mgr far_data gs_mgr4_device =
  mgr_prn_device(cmgr4_procs, "mgr4",    3,  8, 4,   1,   1, 4, 3, cmgrN_print_page);
gx_device_mgr far_data gs_mgr8_device =
  mgr_prn_device(cmgr8_procs, "mgr8",    3,  8, 8, 255, 255, 6, 5, cmgrN_print_page);

/* ------ Internal routines ------ */

/* Define a "cursor" that keeps track of where we are in the page. */
typedef struct mgr_cursor_s {
	gx_device_mgr *dev;
	int bpp;			/* bits per pixel */
	uint line_size;			/* bytes per scan line */
	byte *data;			/* output row buffer */
	int lnum;			/* row within page */
} mgr_cursor;

/* Begin an MGR output page. */
/* Write the header information and initialize the cursor. */
private int
mgr_begin_page(gx_device_mgr *bdev, FILE *pstream, mgr_cursor _ss *pcur)
{	struct b_header head;
	uint line_size =
		gdev_prn_raster((gx_device_printer *)bdev) + 3;
	byte *data = (byte *)gs_malloc(line_size, 1, "mgr_begin_page");
	if ( data == 0 )
		return_error(gs_error_VMerror);

	/* Write the header */
	B_PUTHDR8(&head, bdev->width, bdev->height, bdev->mgr_depth);
	fprintf(pstream, "");
	if ( fwrite(&head, 1, sizeof(head), pstream) < sizeof(head) )
		return_error(gs_error_ioerror);
	fflush(pstream);

	/* Initialize the cursor. */
	pcur->dev = bdev;
	pcur->bpp = bdev->color_info.depth;
	pcur->line_size = line_size;
	pcur->data = data;
	pcur->lnum = 0;
	return 0;
}

/* Advance to the next row.  Return 0 if more, 1 if done. */
private int
mgr_next_row(mgr_cursor _ss *pcur)
{	if ( pcur->lnum >= pcur->dev->height )
	   {	gs_free((char *)pcur->data, pcur->line_size, 1,
			"mgr_next_row(done)");
		return 1;
	   }
	gdev_prn_copy_scan_lines((gx_device_printer *)pcur->dev,
				 pcur->lnum++, pcur->data, pcur->line_size);
	return 0;
}

/* ------ Individual page printing routines ------ */

#define bdev ((gx_device_mgr *)pdev)

/* Print a monochrome page. */
private int
mgr_print_page(gx_device_printer *pdev, FILE *pstream)
{	mgr_cursor cur;
	int mgr_wide;
	int code = mgr_begin_page(bdev, pstream, &cur);
	if ( code < 0 ) return code;

	mgr_wide = bdev->width;
	if (mgr_wide & 7)
	   mgr_wide += 8 - (mgr_wide & 7);
	   
	while ( !(code = mgr_next_row(&cur)) )
	   {	if ( fwrite(cur.data, sizeof(char), mgr_wide / 8, pstream) <
                    mgr_wide / 8)
		return_error(gs_error_ioerror);
	   }
	return (code < 0 ? code : 0);
}


/* Print a gray-mapped page. */
static unsigned char bgreytable[16], bgreybacktable[16];
static unsigned char bgrey256table[256], bgrey256backtable[256];        
/* private */
int
mgrN_print_page(gx_device_printer *pdev, FILE *pstream)
{	mgr_cursor cur;
	int i, j, k, mgr_wide;
	uint mgr_line_size;
	byte *bp, *data, *dp;
        
	int code = mgr_begin_page(bdev, pstream, &cur);
	if ( code < 0 ) return code;

	mgr_wide = bdev->width;
	if ( bdev->mgr_depth == 2 && mgr_wide & 3 )
            mgr_wide += 4 - (mgr_wide & 3);
	if ( bdev->mgr_depth == 4 && mgr_wide & 1 )
            mgr_wide++;
	mgr_line_size = mgr_wide / ( 8 / bdev->mgr_depth );

	if ( bdev->mgr_depth == 4 )
            for ( i = 0; i < 16; i++ ) {
		bgreytable[i] = mgrlut[LUT_BGREY][RGB_RED][i];
		bgreybacktable[bgreytable[i]] = i;
            }

	if ( bdev->mgr_depth == 8 ) {
            for ( i = 0; i < 16; i++ ) {
		bgrey256table[i] = mgrlut[LUT_BGREY][RGB_RED][i] << 4;
		bgrey256backtable[bgrey256table[i]] = i;
            }
            for ( i = 16,j = 0; i < 256; i++ ) {
		for ( k = 0; k < 16; k++ )
                  if ( j == mgrlut[LUT_BGREY][RGB_RED][k] << 4 ) {
                    j++;
                    break;
                  }
		bgrey256table[i] = j;
		bgrey256backtable[j++] = i;
            }
	}

	if ( bdev->mgr_depth != 8 )
        	data = (byte *)gs_malloc(mgr_line_size, 1, "mgrN_print_page");
        
	while ( !(code = mgr_next_row(&cur)) )
	   {
		switch (bdev->mgr_depth) {
			case 2:
				for (i = 0,dp = data,bp = cur.data; i < mgr_line_size; i++) {
					*dp =	*(bp++) & 0xc0;
					*dp |= (*(bp++) & 0xc0) >> 2;
					*dp |= (*(bp++) & 0xc0) >> 4;
                                    *(dp++) |= (*(bp++) & 0xc0) >> 6;
				}
                		if ( fwrite(data, sizeof(byte), mgr_line_size, pstream) < mgr_line_size )
                                	return_error(gs_error_ioerror);
				break;
	                        
			case 4:
				for (i = 0,dp = data, bp = cur.data; i < mgr_line_size; i++) {
					*dp =  bgreybacktable[*(bp++) >> 4] << 4;
                                    *(dp++) |= bgreybacktable[*(bp++) >> 4];
				}
                		if ( fwrite(data, sizeof(byte), mgr_line_size, pstream) < mgr_line_size )
                                	return_error(gs_error_ioerror);
				break;
	                        
			case 8:
				for (i = 0,bp = cur.data; i < mgr_line_size; i++, bp++)
	                              *bp = bgrey256backtable[*bp];
                		if ( fwrite(cur.data, sizeof(cur.data[0]), mgr_line_size, pstream)
					< mgr_line_size )
                                	return_error(gs_error_ioerror);
				break;
		}  
	   }
	if (bdev->mgr_depth != 8)
        	gs_free((char *)data, mgr_line_size, 1, "mgrN_print_page(done)");

	if (bdev->mgr_depth == 2) {
            for (i = 0; i < 4; i++) {
               clut[i].colnum = i;
               clut[i].red    = clut[i].green = clut[i].blue = clut2mgr(i, 2);
	    }
   	}
	if (bdev->mgr_depth == 4) {
            for (i = 0; i < 16; i++) {
               clut[i].colnum = i;
               clut[i].red    = clut[i].green = clut[i].blue = clut2mgr(bgreytable[i], 4);
	    }
   	}
	if (bdev->mgr_depth == 8) {
            for (i = 0; i < 256; i++) {
               clut[i].colnum = i;
               clut[i].red    = clut[i].green = clut[i].blue = clut2mgr(bgrey256table[i], 8);
	    }
   	}
#if !arch_is_big_endian
	swap_bwords( (unsigned char *) clut, sizeof( struct nclut ) * i );
#endif
	if ( fwrite(&clut, sizeof(struct nclut), i, pstream) < i )
            return_error(gs_error_ioerror);
	return (code < 0 ? code : 0);
}

/* Print a color page. */
private int
cmgrN_print_page(gx_device_printer *pdev, FILE *pstream)
{	mgr_cursor cur;
	int i, j, mgr_wide, r, g, b, colors8;
	uint mgr_line_size;
	byte *bp, *data, *dp;
	ushort *wp, prgb[3];
	ulong max_value;
	unsigned char table[256], backtable[256];
        
	int code = mgr_begin_page(bdev, pstream, &cur);
	if ( code < 0 ) return code;

	mgr_wide = bdev->width;
	if (bdev->mgr_depth == 4 && mgr_wide & 1)
            mgr_wide++;
	mgr_line_size = mgr_wide / (8 / bdev->mgr_depth);
       	data = (byte *)gs_malloc(mgr_line_size, 1, "cmgrN_print_page");

       	if ( bdev->mgr_depth == 8 ) {
            memset( table, 0, sizeof(table) );
            for ( r = 0; r <= 6; r++ )
		for ( g = 0; g <= 6; g++ )
       	            for ( b = 0; b <= 6; b++ )
       			if ( r == g && g == b )
                            table[ r + (256-7) ] = 1;
			else
                            table[ (r << 5) + (g << 2) + (b >> 1) ] = 1;
            for ( i = j = 0; i < sizeof(table); i++ )
		if ( table[i] == 1 ) {
                    backtable[i] = j;
                    table[j++] = i;
		}
            colors8 = j;
	}
	while ( !(code = mgr_next_row(&cur)) )
	   {
		switch (bdev->mgr_depth) {
			case 4:
				for (i = 0,dp = data, bp = cur.data; i < mgr_line_size; i++) {
					*dp =  *(bp++) << 4;
                                    *(dp++) |= *(bp++) & 0x0f;
				}
                		if ( fwrite(data, sizeof(byte), mgr_line_size, pstream) < mgr_line_size )
                                	return_error(gs_error_ioerror);
				break;
	                        
			case 8:
				for (i = 0,bp = cur.data; i < mgr_line_size; i++, bp++)
	                              *bp = backtable[*bp] + MGR_RESERVEDCOLORS;
                		if ( fwrite(cur.data, sizeof(cur.data[0]), mgr_line_size, pstream) < mgr_line_size )
                                	return_error(gs_error_ioerror);
				break;
		}  
	   }
       	gs_free((char *)data, mgr_line_size, 1, "cmgrN_print_page(done)");
       	
	if (bdev->mgr_depth == 4) {
            for (i = 0; i < 16; i++) {
               pc_4bit_map_color_rgb((gx_device *)0, (gx_color_index) i, prgb);
               clut[i].colnum = i;
               clut[i].red    = clut2mgr(prgb[0], 16);
               clut[i].green  = clut2mgr(prgb[1], 16);
               clut[i].blue   = clut2mgr(prgb[2], 16);
	    }
   	}
	if (bdev->mgr_depth == 8) {
            for (i = 0; i < colors8; i++) {
               pc_8bit_map_color_rgb((gx_device *)0, (gx_color_index)
                   table[i], prgb);
               clut[i].colnum = MGR_RESERVEDCOLORS + i;
               clut[i].red    = clut2mgr(prgb[0], 16);
               clut[i].green  = clut2mgr(prgb[1], 16);
               clut[i].blue   = clut2mgr(prgb[2], 16);
	    }
   	}
#if !arch_is_big_endian
	swap_bwords( (unsigned char *) clut, sizeof( struct nclut ) * i );
#endif    
	if ( fwrite(&clut, sizeof(struct nclut), i, pstream) < i )
            return_error(gs_error_ioerror);
	return (code < 0 ? code : 0);
}


/* convert the 8-bit look-up table into the standard MGR look-up table */
private unsigned int
clut2mgr(
  register int v,		/* value in clut */
  register int bits		/* number of bits in clut */
)
{
  register unsigned int i;

  i = (unsigned int) 0xffffffff / ((1<<bits)-1);
  return((v*i)/0x10000);
}


/*
 * s w a p _ b w o r d s
 */
private void
swap_bwords(register unsigned char *p, int n)
{
  register unsigned char c;

  n /= 2;
  
  for (; n > 0; n--, p += 2) {
    c    = p[0];
    p[0] = p[1];
    p[1] = c;
  }
}
