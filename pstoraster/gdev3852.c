/* Copyright (C) 1989, 1992, 1993, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/*
This driver program created by Kevin M. Gift <kgift@draper.com> in Sept. 1992.
Modified 3/93 to correct bug in cnt_2prn size.
Modified 3/93 to dimension page back to 8.5, which seems to
   work better than the actual page width of 7.6, ie. it uses
	the full printing width of the printer.
	It was modeled after the V2.4.1 HP Paintjet driver (gdevpjet.c) */

/* gdev3852.c */
/* IBM 3852 JetPrinter color ink jet driver for Ghostscript */

#include "gdevprn.h"
#include "gdevpcl.h"

/* X_DPI and Y_DPI must be the same - use the maximum graphics resolution */
/*   for this printer  */
#define X_DPI 84
#define Y_DPI 84

/* We round up LINE_SIZE to a multiple of 8 bytes */
/* because that's the unit of transposition from pixels to planes. */
/* Should = 96 (KMG) */
#define LINE_SIZE ((X_DPI * 86 / 10 + 63) / 64 * 8)

/* The device descriptor */
private dev_proc_print_page(jetp3852_print_page);
private gx_device_procs jetp3852_procs =
  prn_color_procs(gdev_prn_open, gdev_prn_output_page, gdev_prn_close,
    gdev_pcl_3bit_map_rgb_color, gdev_pcl_3bit_map_color_rgb);
gx_device_printer far_data gs_jetp3852_device =
  prn_device(jetp3852_procs, "jetp3852",
	86,				/* width_10ths, 8.6" (?) */
	110,				/* height_10ths, 11" */
	X_DPI, Y_DPI,
	0.0, 0, 0.0, 0,		/* left, bottom, right, top margins */
	3, jetp3852_print_page);


/* ------ Internal routines ------ */

/* Send the page to the printer.  */
private int
jetp3852_print_page(gx_device_printer *pdev, FILE *prn_stream)
{
#define DATA_SIZE (LINE_SIZE * 8)

   unsigned int cnt_2prn;
	unsigned int count,tempcnt;
	unsigned char vtp,cntc1,cntc2;
	int line_size_color_plane;

	byte data[DATA_SIZE];
 	byte plane_data[LINE_SIZE * 3];

	/* Set initial condition for printer */
	fputs("\033@",prn_stream);

	/* Send each scan line in turn */
	   {	int lnum;
		int line_size = gdev_mem_bytes_per_scan_line((gx_device *)pdev);
		int num_blank_lines = 0;
		for ( lnum = 0; lnum < pdev->height; lnum++ )
		   {	byte _ss *end_data = data + line_size;
			gdev_prn_copy_scan_lines(pdev, lnum,
						 (byte *)data, line_size);
			/* Remove trailing 0s. */
			while ( end_data > data && end_data[-1] == 0 )
				end_data--;
			if ( end_data == data )
			   {	/* Blank line */
				num_blank_lines++;
			   }
			else
			   {	int i;
				byte _ss *odp;
				byte _ss *row;

				/* Pad with 0s to fill out the last */
				/* block of 8 bytes. */
				memset(end_data, 0, 7);

				/* Transpose the data to get pixel planes. */
				for ( i = 0, odp = plane_data; i < DATA_SIZE;
				      i += 8, odp++
				    )
				 { /* The following is for 16-bit machines */
#define spread3(c)\
 { 0, c, c*0x100, c*0x101, c*0x10000L, c*0x10001L, c*0x10100L, c*0x10101L }
				   static ulong spr40[8] = spread3(0x40);
				   static ulong spr8[8] = spread3(8);
				   static ulong spr2[8] = spread3(2);
				   register byte _ss *dp = data + i;
				   register ulong pword =
				     (spr40[dp[0]] << 1) +
				     (spr40[dp[1]]) +
				     (spr40[dp[2]] >> 1) +
				     (spr8[dp[3]] << 1) +
				     (spr8[dp[4]]) +
				     (spr8[dp[5]] >> 1) +
				     (spr2[dp[6]]) +
				     (spr2[dp[7]] >> 1);
				   odp[0] = (byte)(pword >> 16);
				   odp[LINE_SIZE] = (byte)(pword >> 8);
				   odp[LINE_SIZE*2] = (byte)(pword);
				 }
				/* Skip blank lines if any */
				if ( num_blank_lines > 0 )
				   {	
					if (lnum == 0) 
					  { /* Skip down the page from the top */
     					 /* set line spacing = 1/8 inch */
	   				fputs("\0330",prn_stream);
		   			/* Set vertical tab */
		  	   		vtp = (num_blank_lines  / 8);
						fprintf(prn_stream,"\033B%c\000",vtp);
						/* Do vertical tab */
				    	fputs("\013",prn_stream);
				    	num_blank_lines = 0;
						}
					 else
					   { /* Do "dot skips" */
						while(num_blank_lines > 255)
						  {
						  fputs("\033e\377",prn_stream);
						  num_blank_lines -= 255;
						  }
						vtp = num_blank_lines; 
						fprintf(prn_stream,"\033e%c",vtp);
						num_blank_lines = 0;
					 }
				   }

				/* Transfer raster graphics in the order R, G, B. */
				/* Apparently it is stored in B, G, R */
				/* Calculate the amount of data to send by what */
				/* Ghostscript tells us the scan line_size in (bytes) */

				count = line_size / 3;
				line_size_color_plane = count / 3;
			   cnt_2prn = line_size_color_plane * 3 + 5;
				tempcnt = cnt_2prn;
				cntc1 = (tempcnt & 0xFF00) >> 8;
				cntc2 = (tempcnt & 0x00FF);
				fprintf(prn_stream, "\033[O%c%c\200\037",cntc2,cntc1);
				fputc('\000',prn_stream);
   			fputs("\124\124",prn_stream);

				for ( row = plane_data + LINE_SIZE * 2, i = 0; 
				      i < 3; row -= LINE_SIZE, i++ )     
				{	int jj;
				   byte ctemp;
				   odp = row;
					/* Complement bytes */
					for (jj=0; jj< line_size_color_plane; jj++)
					  { ctemp = *odp;
					    *odp++ = ~ctemp;
						 }
					fwrite(row, sizeof(byte),
					    line_size_color_plane, prn_stream);
					}
			   }
		   }
	   }

	/* eject page */
 	fputs("\014", prn_stream);  

	return 0;
}
