/*
 * This file is distributed with Ghostscript, but its author,
 * Tanmoy Bhattacharya (tanmoy@qcd.lanl.gov) hereby places it in the
 * public domain.
 */

/* gdevsgi.c */
/* SGI raster file driver */
#include "gdevprn.h"
#include "gdevsgi.h"

#define X_DPI 72
#define Y_DPI 72

#define sgi_prn_device(procs, dev_name, num_comp, depth, max_gray, max_color, print_page)\
{prn_device_body(gx_device_printer, procs, dev_name, \
		 DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS, X_DPI, Y_DPI, \
		 0, 0, 0, 0, \
		 num_comp, depth, max_gray, max_color, max_gray+1, max_color+1, \
		 print_page)}

private dev_proc_map_rgb_color(sgi_map_rgb_color);
private dev_proc_map_color_rgb(sgi_map_color_rgb);

private dev_proc_print_page(sgi_print_page);

private gx_device_procs sgi_procs =
  prn_color_procs(gdev_prn_open, gdev_prn_output_page, gdev_prn_close,
		  sgi_map_rgb_color, sgi_map_color_rgb);

gx_device_printer far_data gs_sgirgb_device =
  sgi_prn_device(sgi_procs, "sgirgb", 3, 24, 255, 255, sgi_print_page);

private gx_color_index
sgi_map_rgb_color(gx_device *dev, ushort r, ushort g, ushort b)
{      ushort bitspercolor = dev->color_info.depth / 3;
       ulong max_value = (1 << bitspercolor) - 1;
       return ((r*max_value / gx_max_color_value) << (bitspercolor * 2)) +
	      ((g*max_value / gx_max_color_value) << bitspercolor) +
	      (b*max_value / gx_max_color_value);
}

private int
sgi_map_color_rgb(gx_device *dev, gx_color_index color, ushort prgb[3])
{	ushort bitspercolor = dev->color_info.depth / 3;
	ushort colormask = (1 << bitspercolor) - 1;

	prgb[0] = ((color >> (bitspercolor * 2)) & colormask) *
		(ulong)gx_max_color_value / colormask;
	prgb[1] = ((color >> bitspercolor) & colormask) *
		(ulong)gx_max_color_value / colormask;
	prgb[2] = (color & colormask) *
		(ulong)gx_max_color_value / colormask;
	return 0;
}

typedef struct sgi_cursor_s {
  gx_device_printer *dev;
  int bpp;
  uint line_size;
  byte *data;
  int lnum;
} sgi_cursor;

private int
sgi_begin_page(gx_device_printer *bdev, FILE *pstream, sgi_cursor _ss *pcur)
{
     uint line_size = gdev_mem_bytes_per_scan_line((gx_device_printer*)bdev);
     byte *data = (byte*)gs_malloc(line_size, 1, "sgi_begin_page");
     IMAGE *header= (IMAGE*)gs_malloc(sizeof(IMAGE),1,"sgi_begin_page");
     char filler= '\0';
     int i;

     if ((data == (byte*)0)||(header == (IMAGE*)0)) return -1;

     bzero(header,sizeof(IMAGE));
     header->imagic = IMAGIC;
     header->type = RLE(1);
     header->dim = 3;
     header->xsize=bdev->width;
     header->ysize=bdev->height;
     header->zsize=3;
     header->min  = 0;
     header->max  = bdev->color_info.max_color;
     header->wastebytes = 0;
     strncpy(header->name,"gs picture",80);
     header->colormap = CM_NORMAL;
     header->dorev=0;
     fwrite(header,sizeof(IMAGE),1,pstream);
     for (i=0; i<512-sizeof(IMAGE); i++) fputc(filler,pstream);
     pcur->dev = bdev;
     pcur->bpp = bdev->color_info.depth;
     pcur->line_size = line_size;
     pcur->data = data;
     return 0;
}

private int
sgi_next_row(sgi_cursor _ss *pcur)
{    if (pcur->lnum < 0)
       return 1;
     gdev_prn_copy_scan_lines((gx_device_printer*)pcur->dev,
			      pcur->lnum--, pcur->data, pcur->line_size);
     return 0;
}

#define bdev ((gx_device_printer *)pdev)

private int
sgi_print_page(gx_device_printer *pdev, FILE *pstream)
{      sgi_cursor cur;
       int code = sgi_begin_page(bdev, pstream, &cur);
       uint bpe, mask;
       int separation;
       long *rowsizes=(long*)gs_malloc(4,3*bdev->height,"sgi_print_page");
       byte *edata ;
       long lastval; byte*sptr;
       int rownumber;
#define aref2(a,b) a*bdev->height+b
       edata =  (byte*)gs_malloc(cur.line_size, 1, "sgi_begin_page");
       if((code<0)||(rowsizes==(long*)NULL)||(edata==(byte*)NULL)) return(-1);
       fwrite(rowsizes,sizeof(long),3*bdev->height,pstream); /* rowstarts */
       fwrite(rowsizes,sizeof(long),3*bdev->height,pstream); /* rowsizes */
       lastval = 512+sizeof(long)*6*bdev->height;
       fseek(pstream,lastval,0);
       for (separation=0; separation < 3; separation++)
	 {
	   cur.lnum = cur.dev->height-1;
	   rownumber = 0;
	   bpe = cur.bpp/3;
	   mask = (1<<bpe) - 1;
	   while ( !(code=sgi_next_row(&cur)))
	     { byte *bp;
	       uint x;
	       int shift;
	       byte *curcol=cur.data;
	       byte *startcol=edata;
	       int count;
	       byte todo, cc;
	       byte *iptr, *sptr, *optr, *ibufend;
	       for (bp = cur.data, x=0, shift = 8 - cur.bpp;
		    x < bdev->width;
		    )
		 { ulong pixel = 0;
		   uint r, g, b;
		   switch (cur.bpp >> 3)
		     {
		     case 3: pixel = (ulong)*bp << 16; bp++;
		     case 2: pixel += (uint)*bp << 8; bp++;
		     case 1: pixel += *bp; bp++; break;
		     case 0: pixel = *bp >> shift;
		       if ((shift-=cur.bpp) < 0)
			 bp++, shift += 8; break;
		     }
		   ++x;
		   b = pixel & mask; pixel >>= bpe;
		   g = pixel & mask; pixel >>= bpe;
		   r = pixel & mask;
		   switch(separation)
		     {
		     case 0: *curcol++=r; break;
		     case 1: *curcol++=g; break;
		     case 2: *curcol++=b; break;
		     }
		 }
	       iptr=cur.data;
	       optr=startcol;
	       ibufend=curcol-1;
	       while(iptr<ibufend) {
		 sptr = iptr;						
		 iptr += 2;
		 while((iptr<ibufend)&&((iptr[-2]!=iptr[-1])||(iptr[-1]!=iptr[0])))
		   iptr++;
		 iptr -= 2;
		 count = iptr-sptr;
		 while(count) {
		   todo = count>126 ? 126:count;
		   count -= todo;
		   *optr++ = 0x80|todo;
		   while(todo--)
		     *optr++ = *sptr++;
		 }
		 sptr = iptr;
		 cc = *iptr++;
		 while( (iptr<ibufend) && (*iptr == cc) )
		   iptr++;
		 count = iptr-sptr;
		 while(count) {
		   todo = count>126 ? 126:count;
		   count -= todo;
		   *optr++ = todo;
		   *optr++ = cc;
		 }
	       }
	       rowsizes[aref2(separation,rownumber++)] = optr-startcol;
	       fwrite(startcol,1,optr-startcol,pstream);
	     }
	 }
       fseek(pstream,512L,0);
       for(separation=0; separation<3; separation++)
	 for(rownumber=0; rownumber<bdev->height; rownumber++)
	   {fputc((char)(lastval>>24),pstream);
	    fputc((char)(lastval>>16),pstream);
	    fputc((char)(lastval>>8),pstream);
	    fputc((char)(lastval),pstream);
	    lastval+=rowsizes[aref2(separation,rownumber)];}
       for(separation=0; separation<3; separation++)
	 for(rownumber=0; rownumber<bdev->height; rownumber++)
	   {lastval=rowsizes[aref2(separation,rownumber)];
	    fputc((char)(lastval>>24),pstream);
	    fputc((char)(lastval>>16),pstream);
	    fputc((char)(lastval>>8),pstream);
	    fputc((char)(lastval),pstream);}
       gs_free((char*)cur.data, cur.line_size, 1,
		 "sgi_print_page(done)");
       gs_free((char*)edata, cur.line_size, 1, "sgi_print_page(done)");
       gs_free((char*)rowsizes,4,3*bdev->height,"sgi_print_page(done)");
       return (code < 0 ? code : 0);
}
