/* Copyright (C) 1992, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gdevtknk.c */
/*   
   Tektronix Ink-jet plotter driver.
   This code is written for 4696 and 4695 plotters, it may easily be 
   adopted to the 4393 and 4394 models as well, simply by adding new
   device descriptors with other geometrical characteristics.
*/
#include "gdevprn.h"
#include "malloc_.h"

/* Thanks to Karsten Spang (spang@nbivax.nbi.dk) for contributing */
/* this code to Aladdin Enterprises. */


/* The device descriptor */
/* We need our own color mapping procedures. */
private dev_proc_map_rgb_color(tekink_map_rgb_color);
private dev_proc_map_color_rgb(tekink_map_color_rgb);
private dev_proc_print_page(tekink_print_page);
private gx_device_procs tekink_procs =
    prn_color_procs(gdev_prn_open, gdev_prn_output_page, gdev_prn_close,
	tekink_map_rgb_color, tekink_map_color_rgb);


/* 
   Device descriptor for the Tek 4696.
   The 4696 plotter uses roll media, thus the y size is arbitrary. The
   value below is chosen to make the image area A*-format like, i.e. the 
   aspect ratio is close to sqrt(2).
*/
gx_device_printer far_data gs_tek4696_device =
    prn_device(tekink_procs,"tek4696",
    85,120,	/* Page size in 10th of inches */
    120,120,	/* Resolution in DPI */
    0.0,0.0,0.0,0.0,	/* Margins */
    4,		/* Bits per pixel */
    tekink_print_page);

/* Color mapping.
   The tek inkjets use subtractive colors B=0 M=1 Y=2 C=3. These are 
   represented as 4 bits B=1 M=2 Y=4 C=8 in a byte. This gives:
      White   =  0
      Black   =  1
      Magenta =  2
      Yellow  =  4
      Red     =  6
      Cyan    =  8
      Blue    = 10
      Green   = 12
   The remaining values are unused. (They give ugly results if sent to the
   plotter.) Of course this could have been compressed into 3 bits, but 
   as the palette color memory device uses 8 bits anyway, this is easier,
   and perhaps faster.
*/

static gx_color_index rgb_to_index[8]={1,6,12,4,10,2,8,0};
static ushort index_to_rgb[16][3]={
    {65535,65535,65535}, /* White */
    {0,0,0}, /* Black */
    {65535,0,65535}, /* Magenta */
    {2,2,2}, /* Unused */
    {65535,65535,0}, /* Yellow */
    {2,2,2}, /* Unused */
    {65535,0,0}, /* Red */
    {2,2,2}, /* Unused */
    {0,65535,65535}, /* Cyan */
    {2,2,2}, /* Unused */
    {0,0,65535}, /* Blue */
    {2,2,2}, /* Unused */
    {0,65535,0}, /* Green */
    {2,2,2}, /* Unused */
    {2,2,2}, /* Unused */
    {2,2,2}  /* Unused */
};

/* Map an RGB color to a printer color. */
private gx_color_index
tekink_map_rgb_color(gx_device *dev, ushort r, ushort g, ushort b)
{
    return(rgb_to_index[(((b>32767) << 2) + ((g>32767) << 1) + 
			(r>32767)) & 7]);
}

/* Map the printer color back to RGB. */
private int
tekink_map_color_rgb(gx_device *dev, gx_color_index color, ushort prgb[3])
{
    register ushort c = (ushort)color;
    register int i;
    if (c>15) return -1;
    if (index_to_rgb[c][0]==2) return -1;
    for (i=0;i<3;i++){
	prgb[i]=index_to_rgb[c][i];
    }
    return 0;
}

/* Send the page to the printer. */
private int
tekink_print_page(gx_device_printer *pdev,FILE *prn_stream)
{
    int line_size,color_line_size,scan_line,num_bytes,scan_lines,color_plane;
    int roll_paper,out_line,micro_line,pending_micro_lines,line_blank,
	blank_lines;
    byte *outdata,*indata1,*bdata1,*mdata1,*ydata1,*cdata1;
    register byte *indata,*bdatap,*mdatap,*ydatap,*cdatap;
    register byte bdata,mdata,ydata,cdata;
    register byte mask,inbyte;
    register byte *indataend,*outdataend;
    
    /* Allocate a temporary buffer for color separation.
       The buffer is partitioned into an input buffer and four
       output buffers for the color planes. The output buffers
       are allocated with an extra sentinel byte. */
    
    line_size = gdev_mem_bytes_per_scan_line((gx_device *)pdev);
    color_line_size=(pdev->width+7)/8;
    indata1=(byte *)malloc(line_size+4*(color_line_size+1));
    if (indata1==NULL) return -1;
    /* pointers to the partions */
    indataend=indata1+line_size;
    bdata1=indataend;
    mdata1=bdata1+(color_line_size+1);
    ydata1=mdata1+(color_line_size+1);
    cdata1=ydata1+(color_line_size+1);

    /* Does this device use roll paper? */
    roll_paper=!strcmp(pdev->dname,"tek4696");
    
    out_line=0;
    blank_lines=0;
    scan_lines=pdev->height;
    for (scan_line=0;scan_line<scan_lines;scan_line++){
	/* get data */
	gdev_prn_copy_scan_lines(pdev,scan_line,indata1,line_size);
	/* Separate data into color planes */
	bdatap = bdata1+1;
	mdatap = mdata1+1;
	ydatap = ydata1+1;
	cdatap = cdata1+1;
	bdata=0;
	mdata=0;
	cdata=0;
	ydata=0;
	mask=0x80;
    	memset(indataend,0,4*(color_line_size+1));
	for (indata=indata1;indata<indataend;indata++){
            inbyte = *indata;
            if (inbyte&0x01) bdata|=mask;
            if (inbyte&0x02) mdata|=mask;
            if (inbyte&0x04) ydata|=mask;
            if (inbyte&0x08) cdata|=mask;
            mask>>=1;
            if (!mask){
		*(bdatap++) = bdata;
		*(mdatap++) = mdata;
		*(cdatap++) = cdata;
		*(ydatap++) = ydata;
		bdata=0;
		mdata=0;
		cdata=0;
		ydata=0;
		mask=0x80;
            }
	}
	if (mask!=0x80){
            *bdatap = bdata;
            *mdatap = mdata;
            *cdatap = cdata;
            *ydatap = ydata;
	}
	line_blank=1;
	/* Output each of the four color planes */
	for (color_plane=0;color_plane<4;color_plane++){
            outdata=indataend+(color_plane*(color_line_size+1));
            outdataend=outdata+color_line_size;
            	
            /* Remove trailing spaces and output the color line if it is 
               not blank */
            *outdata=0xff;
            while (!(*outdataend)) outdataend--;
            if (num_bytes=(outdataend-outdata)){
            	line_blank=0;
            	/* On encountering the first non-blank data, output pending
            	   blank lines */
		if (blank_lines){
                    pending_micro_lines=((out_line+blank_lines+1)/4)-
			(out_line/4);
                    for (micro_line=0;micro_line<pending_micro_lines;
                    	micro_line++){
			fputs("\033A",prn_stream);
                    }
                    out_line+=blank_lines;
                    blank_lines=0;
            	}
		fprintf(prn_stream,"\033I%c%03d",'0'+(out_line%4)+
                    4*color_plane,num_bytes);
		fwrite(outdata+1,1,num_bytes,prn_stream);
            }
	} /* loop over color planes */
        
	/* If this line is blank, and if it is a roll paper model,
           count the line. Otherwise output the line */
	if (line_blank&&roll_paper){
            /* Only increment the blank line count, if non blank lines 
               have been encountered previously, i.e. skip leading blank
               lines. */
            if (out_line) blank_lines++;
	}
	else{
            if (out_line%4==3){
		/* Write micro line feed code */
		fputs("\033A",prn_stream);
            }
            out_line++;
	}
    } /* loop over scan lines */
    
    /* if the number of scan lines written is not a multiple of four, 
       write the final micro line feed code */
    if (out_line%4){
	fputs("\033A",prn_stream);
    }
    /* Separate this plot from the next */
    if (roll_paper){
    	fputs("\n\n\n\n\n",prn_stream);
    }
    else{
    	fputs("\f",prn_stream);
    }
    
    /* Deallocate temp buffer */
    free(indata1);
    return 0;
}
