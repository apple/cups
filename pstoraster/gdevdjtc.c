/* Copyright (C) 1989, 1990, 1991 Aladdin Enterprises.  All rights reserved.
  
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

/* gdevdjtc.c */
/* HP DeskJet 500C driver */
#include "gdevprn.h"
#include "gdevpcl.h"
#include "malloc_.h"

/***
 *** Note: this driver was contributed by a user, Alfred Kayser:
 ***       please contact AKayser@et.tudelft.nl if you have questions.
 ***/
                                              
#ifndef SHINGLING        /* Interlaced, multi-pass printing */
#define SHINGLING 1      /* 0 = none, 1 = 50%, 2 = 25%, 2 is best & slowest */
#endif

#ifndef DEPLETION        /* 'Intelligent' dot-removal */ 
#define DEPLETION 1      /* 0 = none, 1 = 25%, 2 = 50%, 1 best for graphics? */
#endif                   /* Use 0 for transparencies */

#define X_DPI 300
#define Y_DPI 300
/* bytes per line for DeskJet Color */
#define LINE_SIZE ((X_DPI * 85 / 10 + 63) / 64 * 8)

/* The device descriptors */
private dev_proc_print_page(djet500c_print_page);

private gx_device_procs djet500c_procs =
  prn_color_procs(gdev_prn_open, gdev_prn_output_page, gdev_prn_close,
    gdev_pcl_3bit_map_rgb_color, gdev_pcl_3bit_map_color_rgb);

gx_device_printer far_data gs_djet500c_device =
  prn_device(djet500c_procs, "djet500c",
    85,                /* width_10ths, 8.5" */
    120,		/* height_10ths, 12" */
    X_DPI, Y_DPI,
    0.25, 0.25, 0.25, 0.25,        /* margins */
    3, djet500c_print_page);

/* Forward references */
private int djet500c_print_page(P2(gx_device_printer *, FILE *));

static int mode2compress(P3(byte *row, byte *end_row, byte *compressed));

/* The DeskJet 500C uses additive colors in separate planes. */
/* We only keep one bit of color, with 1 = R, 2 = G, 4 = B. */
/* Because the buffering routines assume 0 = white, */
/* we complement all the color components. */

/* Send the page to the printer.  For speed, compress each scan line, */
/* since computer-to-printer communication time is often a bottleneck. */
/* The DeskJet Color can compress (mode 2) */

private int
djet500c_print_page(gx_device_printer *pdev, FILE *fprn)
{
    byte *bitData=NULL;
    byte *plane1=NULL;
    byte *plane2=NULL;
    byte *plane3=NULL;
    int bitSize=0;
    int planeSize=0;

    /* select the most compressed mode available & clear tmp storage */
    /* put printer in known state */
    fputs("\033E",fprn);
    
    /* ends raster graphics to set raster graphics resolution */
    fputs("\033*rbC", fprn);	/*  was \033*rB  */

    /* set raster graphics resolution -- 300 dpi */
    fputs("\033*t300R", fprn);                                           
    
    /* A4, skip perf, def. paper tray */ 
    fputs("\033&l26a0l1H", fprn); 

    /* RGB Mode */
    fputs("\033*r3U", fprn);    
    
    /* set depletion level */
    fprintf(fprn, "\033*o%dD", DEPLETION);
    
    /* set shingling level */
    fprintf(fprn, "\033*o%dQ", SHINGLING);
    
    /* move to top left of page & set current position */
    fputs("\033*p0x0Y", fprn); /* cursor pos: 0,0 */
     
    fputs("\033*b2M", fprn);	/*  mode 2 compression for now  */
    
    fputs("\033*r0A", fprn);  /* start graf. left */
    
    /* Send each scan line in turn */
       {    int lnum;
	int num_blank_lines = 0;
	int lineSize = gdev_mem_bytes_per_scan_line((gx_device *)pdev);
	if (lineSize>bitSize)
	{                         
            if (bitData) free(bitData);
            bitSize=lineSize;
            bitData=(byte*)malloc(bitSize+16);
	}
	for (lnum=0; lnum<pdev->height; lnum++)
	{    
            byte *endData;
            
            gdev_prn_copy_scan_lines(pdev, lnum, bitData, lineSize);

            /* Remove trailing 0s. */
            endData = bitData + lineSize;
            while ( (endData>bitData) && (endData[-1] == 0) )
		endData--;
            if (endData == bitData)
		num_blank_lines++;
            else
            {    int count, k, i, lineLen;

		/* Pad with 0s to fill out the last */
		/* block of 8 bytes. */
		memset(endData, 0, 7);
                              
		lineLen=((endData-bitData)+7)/8;    /* Round to next 8multiple */           
		if (planeSize<lineLen)                             
		{
                    if (plane1) free(plane1);
                    if (plane2) free(plane2);
                    if (plane3) free(plane3);
                    planeSize=lineLen;
                    plane1=(byte*)malloc(planeSize+8);
                    plane2=(byte*)malloc(planeSize+8);
                    plane3=(byte*)malloc(planeSize+8);
		}
		/* Transpose the data to get pixel planes. */
		for (k=i=0; k<lineLen; i+=8, k++)
		{ 
                   register ushort t, c;
                   
                   /* Three smaller loops are better optimizable and use less
                      vars, so most of them can be in registers even on pc's */
                   for (c=t=0;t<8;t++)
			c = (c<<1) | (bitData[t+i]&4);
                   plane3[k] = ~(byte)(c>>2);
                   for (c=t=0;t<8;t++)
			c = (c<<1) | (bitData[t+i]&2);
                   plane2[k] = ~(byte)(c>>1);
                   for (c=t=0;t<8;t++)
			c = (c<<1) | (bitData[t+i]&1);
                   plane1[k] = ~(byte)(c);
		}           

		/* Skip blank lines if any */
		if (num_blank_lines > 0)
		{    /* move down from current position */
                    fprintf(fprn, "\033*b%dY", num_blank_lines);
                    num_blank_lines = 0;
		}

		/* Transfer raster graphics */
		/* in the order R, G, B. */
		/* lineLen is at least bitSize/8, so bitData can easily be used to store
                   lineLen of bytes */
		/* P.s. mode9 compression is akward(??) to use, because the lineLenght's
                   are different, so we are stuck with mode 2, which is good enough */
                   
		/* set the line width */
		fprintf(fprn, "\033*r%dS", lineLen*8);

		count = mode2compress(plane1, plane1 + lineLen, bitData);
		fprintf(fprn, "\033*b%dV", count);
		fwrite(bitData, sizeof(byte), count, fprn); 
		count = mode2compress(plane2, plane2 + lineLen, bitData);
		fprintf(fprn, "\033*b%dV", count);
		fwrite(bitData, sizeof(byte), count, fprn); 
		count = mode2compress(plane3, plane3 + lineLen, bitData);
		fprintf(fprn, "\033*b%dW", count);
		fwrite(bitData, sizeof(byte), count, fprn); 
            }
	}
    }
    /* end raster graphics */
    fputs("\033*rbC", fprn);	/*  was \033*rB  */
    fputs("\033*r1U", fprn);	/*  back to 1 plane  */

       /* put printer in known state */
    fputs("\033E",fprn);
    
    /* eject page */
    fputs("\033&l0H", fprn);        
    
    /* release allocated memory */
    if (bitData) free(bitData);
    if (plane1) free(plane1);
    if (plane2) free(plane2);
    if (plane3) free(plane3);

    return 0;
}


/*
 * Mode 2 Row compression routine for the HP DeskJet & LaserJet IIp.
 * Compresses data from row up to end_row, storing the result
 * starting at compressed.  Returns the number of bytes stored.
 * Runs of K<=127 literal bytes are encoded as K-1 followed by
 * the bytes; runs of 2<=K<=127 identical bytes are encoded as
 * 257-K followed by the byte.
 * In the worst case, the result is N+(N/127)+1 bytes long,
 * where N is the original byte count (end_row - row).
 * I can't use the general pcl version, because it assume even linelength's
 */

static int
mode2compress(byte *row, byte *end_row, byte *compressed)
{    
    register byte *exam; /* word being examined in the row to compress */
    register byte *cptr = compressed; /* output pointer into compressed bytes */
    int i, count, len;
    byte test;

    exam = row;
    while (1)
    {                        
	test = *exam++;
	/* Advance exam until test==*exam  or exam==end_row */
	while ((test != *exam) && (exam < end_row))
            test = *exam++;                    
	/* row points to start of differing bytes,
           exam points to start of consequtive series 
                    or to end of row */
	if (exam<end_row) exam--;
	len=exam-row;   
	while (len>0)
	{
            count=len;
            if (count>127) count=127;
            *cptr++=count-1;  
            for (i=0;i<count;i++) *cptr++ = *row++;
            len-=count;
	}     
	if (exam>=end_row) break;     /* done */             
	exam++;     /* skip first same byte */
	while ((test == *exam) && (exam < end_row))  /* skip all same bytes */
            exam++;
	/* exam points now first different word or to end of data */
	len = exam-row;
	while (len>0)
	{     
            count=len;
            if (count>127) count=127;
            *cptr++=(257-count);
            *cptr++=test;             
            len-=count;
	}                
	if (exam>=end_row) break;            /* end of data */
	row = exam;    /* row points to first dissimular byte */
    }                                          
    return (cptr-compressed);
}
