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

/* gdevos2p.c */
/*
 * OS/2 printer device
 * by Russell Lang.
 * Derived from mswinpr2 device by Russell Lang and
 * L. Peter Deutsch, Aladdin Enterprises.
 */

/* This device works when GS is a DLL loaded by a PM program */
/* It does not work when GS is a text mode EXE */

/* This driver uses the printer default size and resolution and
 * ignores page size and resolution set using -gWIDTHxHEIGHT and
 * -rXxY.  You must still set the correct PageSize to get the
 * correct clipping path.  If you don't specify a value for
 * -dBitsPerPixel, the depth will be obtained from the printer
 * device context.
 */

#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_DEV
#define INCL_GPIBITMAPS
#define INCL_SPL
#define INCL_SPLDOSPRINT
#define INCL_SPLERRORS

#include <os2.h>

#include "gdevprn.h"
#include "gdevpccm.h"
#include "gp.h"

extern HWND hwndtext;		/* in gp_os2.h */
extern const char *gs_product;

typedef struct tagOS2QL {
    PRQINFO3	*prq;	/* queue list */
    ULONG	len;	/* bytes in queue list (for gs_free) */
    int		defqueue; /* default queue */
    int 	nqueues;  /* number of queues */
} OS2QL;

#ifndef NERR_BufTooSmall
#define NERR_BufTooSmall 2123	/* For SplEnumQueue */
#endif

/* Make sure we cast to the correct structure type. */
typedef struct gx_device_os2prn_s gx_device_os2prn;
#undef opdev
#define opdev ((gx_device_os2prn *)dev)

/* Device procedures */

/* See gxdevice.h for the definitions of the procedures. */
private dev_proc_open_device(os2prn_open);
private dev_proc_close_device(os2prn_close);
private dev_proc_print_page(os2prn_print_page);
private dev_proc_map_rgb_color(os2prn_map_rgb_color);
private dev_proc_map_color_rgb(os2prn_map_color_rgb);
private dev_proc_put_params(os2prn_put_params);
private dev_proc_get_params(os2prn_get_params);

private void os2prn_set_bpp(gx_device *dev, int depth);
private int os2prn_get_queue_list(OS2QL *ql);
private void os2prn_free_queue_list(OS2QL *ql);
int os2prn_get_printer(OS2QL *ql);

private gx_device_procs os2prn_procs =
  prn_color_params_procs(os2prn_open, gdev_prn_output_page, os2prn_close,
   os2prn_map_rgb_color, os2prn_map_color_rgb, 
   os2prn_get_params, os2prn_put_params);


/* The device descriptor */
struct gx_device_os2prn_s {
	gx_device_common;
	gx_prn_device_common;
	HAB hab;
	HDC hdc;
	HPS hps;
	char queue_name[256];	/* OS/2 printer queue name */
	int newframe;		/* false before first page */
	OS2QL ql;
	int clipbox[4];		/* llx, lly, urx, ury in pixels */
	HDC hdcMem;
	HPS hpsMem;
};

gx_device_os2prn far_data gs_os2prn_device = {
  prn_device_std_body(gx_device_os2prn, os2prn_procs, "os2prn",
  DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS, 72, 72,
  0, 0, 0, 0,
  0, os2prn_print_page), /* depth = 0 */
  0,	/* hab */
  0,	/* hdc */
  0,	/* hps */
  ""	/* queue_name */
};

/* Open the os2prn driver */
private int
os2prn_open(gx_device *dev)
{	int code;
	PTIB pptib;
	PPIB pppib;
	DEVOPENSTRUC dop;
	ULONG cbBuf;
	ULONG cbNeeded;
	APIRET rc;
	PBYTE pbuf;
	char *p;
	SIZEL sizlPage;
	LONG caps[2];
	HCINFO hcinfo;
	LONG nforms;
	float m[4];
	int depth;
	FILE *pfile;
	int i;
	char *prefix = "\\\\spool\\";  /* 8 characters long */

	PRQINFO3 *pprq;
	gx_device_os2prn *oprn;

	oprn = opdev;

	if (DosGetInfoBlocks(&pptib, &pppib)) {
		fprintf(stderr,"\nos2prn_open: Couldn't get pid\n");
		return gs_error_limitcheck;
	}
	if (pppib->pib_ultype != 3) {	     
	    /* if caller is not PM app */
	    fprintf(stderr,"os2prn device can only be used from a PM application\n");
	    return gs_error_limitcheck;
	}

	opdev->hab = WinQueryAnchorBlock(hwndtext);
	opdev->newframe = 0;

	if (os2prn_get_queue_list(&opdev->ql))
	    return gs_error_limitcheck;

	if (opdev->queue_name[0] == '\0') {
	    /* obtain printer name from filename */
	    p = opdev->fname;
    	    for (i=0; i<8; i++) {
	        if (prefix[i] == '\\') {
	            if ((*p != '\\') && (*p != '/'))
			break;
	        }
	        else if (tolower(*p) != prefix[i])
	            break;
	        p++;
	    }
	    if (i==8 && (strlen(p)!=0))
		strcpy(opdev->queue_name, p);
	}

	pprq = NULL;
	if (opdev->queue_name[0] != '\0') {
	    for (i=0; i<opdev->ql.nqueues; i++) {
		if (strcmp(opdev->ql.prq[i].pszName, opdev->queue_name) == 0) {
		    pprq = &(opdev->ql.prq[i]);
		    break;
	 	}
	    }
        }
	else {
	    /* use default queue */
	    pprq = &(opdev->ql.prq[opdev->ql.defqueue]);
	}
	if (pprq == (PRQINFO3 *)NULL) {
	    fprintf(stderr, "Invalid os2prn queue  name -sOS2QUEUE=\042%s\042\n", opdev->queue_name);
	    fprintf(stderr, "Valid device names are:\n");
	    for (i=0; i<opdev->ql.nqueues; i++) {
		fprintf(stderr, "  -sOS2QUEUE=\042%s\042\n", opdev->ql.prq[i].pszName);
	    }
	    return gs_error_rangecheck;
	}


	/* open printer device */
	memset(&dop, 0, sizeof(dop));
	dop.pszLogAddress = pprq->pszName;	/* queue name */
	p = strchr(pprq->pszDriverName, '.');
	if (p != (char *)NULL)
	    *p = '\0';
	dop.pszDriverName = pprq->pszDriverName;
	dop.pszDataType = "PM_Q_STD";
	dop.pdriv = pprq->pDriverData;
	opdev->hdc = DevOpenDC(opdev->hab, OD_QUEUED, "*", 9L, (PDEVOPENDATA)&dop, (HDC)NULL);
	if (opdev->hdc == DEV_ERROR) {
	    ERRORID eid = WinGetLastError(opdev->hab);
	    fprintf(stderr, "DevOpenDC for printer error 0x%x\n", eid);
	    return gs_error_limitcheck;
	}

	os2prn_free_queue_list(&opdev->ql);

	/* find out resolution of printer */
	/* this is returned in pixels/metre */
	DevQueryCaps(opdev->hdc, CAPS_HORIZONTAL_RESOLUTION, 2, caps);
	dev->x_pixels_per_inch = (int)(caps[0] * 0.0254 + 0.5);
	dev->y_pixels_per_inch = (int)(caps[1] * 0.0254 + 0.5);

	/* find out page size and margins */
	/* these are returned in millimetres */
	nforms = DevQueryHardcopyCaps(opdev->hdc, 0, 0, &hcinfo);
	for (i=0; i<nforms; i++) {
    	    DevQueryHardcopyCaps(opdev->hdc, i, 1, &hcinfo);
	    if (hcinfo.flAttributes & HCAPS_CURRENT)
		break;	/* this is the default page size */
	}
	/* GS size is in pixels */
	dev->width = hcinfo.cx * caps[0] / 1000;
	dev->height = hcinfo.cy * caps[1] / 1000;
	/* GS margins are in inches */
	m[0] /*left*/   =  hcinfo.xLeftClip / 25.4;
	m[1] /*bottom*/ =  hcinfo.yBottomClip / 25.4;
	m[2] /*right*/  = (hcinfo.cx - hcinfo.xRightClip) / 25.4;
	m[3] /*top*/    = (hcinfo.cy - hcinfo.yTopClip) / 25.4;
	gx_device_set_margins(dev, m, true);
	/* set bounding box in pixels for later drawing */
	opdev->clipbox[0] = (int)(hcinfo.xLeftClip   / 25.4 * dev->x_pixels_per_inch+1); /* round inwards */
	opdev->clipbox[1] = (int)(hcinfo.yBottomClip / 25.4 * dev->y_pixels_per_inch+1);
	opdev->clipbox[2] = (int)(hcinfo.xRightClip  / 25.4 * dev->x_pixels_per_inch);
	opdev->clipbox[3] = (int)(hcinfo.yTopClip    / 25.4 * dev->y_pixels_per_inch);

	/* get presentation space */
	sizlPage.cx = dev->width;
	sizlPage.cy = dev->height;
	opdev->hps = GpiCreatePS(opdev->hab, opdev->hdc, &sizlPage, 
		PU_PELS | GPIF_DEFAULT | GPIT_NORMAL | GPIA_ASSOC);

	depth = dev->color_info.depth;
	if (depth == 0) {
	    /* Set parameters that were unknown before opening device */
	    /* Find out if the device supports color */
	    /* We recognize 1, 3, 8 and 24 bit color devices */
	    DevQueryCaps(opdev->hdc, CAPS_COLOR_PLANES, 2, caps);
	    /* caps[0] is #color planes, caps[1] is #bits per plane */
	    depth = caps[0] * caps[1];
	}
	os2prn_set_bpp(dev, depth);

	/* create a memory DC compatible with printer */
	opdev->hdcMem = DevOpenDC(opdev->hab, OD_MEMORY, "*", 0L, NULL, opdev->hdc);
	if (opdev->hdcMem == DEV_ERROR) {
	    ERRORID eid = WinGetLastError(opdev->hab);
	    fprintf(stderr, "DevOpenDC for memory error 0x%x\n", eid);
	    return gs_error_limitcheck;
	}
	sizlPage.cx = dev->width;
	sizlPage.cy = dev->height;
	opdev->hpsMem = GpiCreatePS(opdev->hab, opdev->hdcMem, &sizlPage, 
		PU_PELS | GPIF_DEFAULT | GPIT_NORMAL | GPIA_ASSOC );
	if (opdev->hpsMem == GPI_ERROR) {
	    ERRORID eid = WinGetLastError(opdev->hab);
	    fprintf(stderr, "GpiCreatePS for memory error 0x%x\n", eid);
	    return gs_error_limitcheck;
	}

	if (DevEscape(opdev->hdc, DEVESC_STARTDOC, (LONG)strlen(gs_product), 
		(char *)gs_product, NULL, NULL) == DEVESC_ERROR) {
	    ERRORID eid = WinGetLastError(opdev->hab);
	    fprintf(stderr, "DEVESC_STARTDOC error 0x%x\n", eid);
	    return gs_error_limitcheck;
	}

	/* gdev_prn_open opens a temporary file which we don't want */
	/* so we specify the name now so we can delete it later */
	pfile = gp_open_scratch_file(gp_scratch_file_name_prefix, 
		opdev->fname, "wb");
	fclose(pfile);
	code = gdev_prn_open(dev);

	return code;
}

/* Close the os2prn driver */
private int
os2prn_close(gx_device *dev)
{	int code;
	LONG lOut;
	USHORT usJobID;
	/* tell printer that all is finished */
	DevEscape(opdev->hdc, DEVESC_ENDDOC, 0L, NULL, &lOut, (PBYTE)&usJobID);
	/* Free resources */
	GpiAssociate(opdev->hps, (HDC)NULL);
	GpiDestroyPS(opdev->hps);
	DevCloseDC(opdev->hdc);

	if (opdev->hpsMem != GPI_ERROR)
		GpiDestroyPS(opdev->hpsMem);
	if (opdev->hdcMem != DEV_ERROR)
		DevCloseDC(opdev->hdcMem);

	code = gdev_prn_close(dev);
	/* delete unwanted temporary file */
	unlink(opdev->fname);
	return code;
}

/* Get os2pm parameters */
int
os2prn_get_params(gx_device *dev, gs_param_list *plist)
{	int code = gdev_prn_get_params(dev, plist);
	gs_param_string qs;
	qs.data = opdev->queue_name, qs.size = strlen(qs.data),
	  qs.persistent = false;
	code < 0 ||
	(code = param_write_string(plist, "OS2QUEUE", &qs)) < 0;
	return code;
}



/* We implement this ourselves so that we can change BitsPerPixel */
/* before the device is opened */
int
os2prn_put_params(gx_device *dev, gs_param_list *plist)
{	int ecode = 0, code;
	int old_bpp = dev->color_info.depth;
	int bpp = old_bpp;
	gs_param_string qs;

	/* Handle extra parameters */
	switch ( code = param_read_string(plist, "OS2QUEUE", &qs) )
	{
	case 0:
		if ( qs.size == strlen(opdev->queue_name) &&
		     !memcmp(opdev->queue_name, qs.data, qs.size)
		   )
		  {	qs.data = 0;
			break;
		  }
		if ( dev->is_open )
		  ecode = gs_error_rangecheck;
		else if ( qs.size >= sizeof(opdev->queue_name))
		  ecode = gs_error_limitcheck;
		else
		  break;
		goto qe;
	default:
		ecode = code;
qe:		param_signal_error(plist, "OS2QUEUE", ecode);
	case 1:
		qs.data = 0;
		break;
	}

	switch ( code = param_read_int(plist, "BitsPerPixel", &bpp) )
	{
	case 0:
		if ( dev->is_open )
		  ecode = gs_error_rangecheck;
		else
		  {	/* change dev->color_info is valid before device is opened */
			os2prn_set_bpp(dev, bpp);
			break;
		  }
		goto bppe;
	default:
		ecode = code;
bppe:		param_signal_error(plist, "BitsPerPixel", ecode);
	case 1:
		break;
	}

	if ( ecode >= 0 )
		ecode = gdev_prn_put_params(dev, plist);

	if ( ( ecode >= 0 ) && ( qs.data != 0 ) )
	  {	memcpy(opdev->queue_name, qs.data, qs.size);
		opdev->queue_name[qs.size] = 0;
	  }

	return ecode;
}



/* ------ Internal routines ------ */

#undef opdev
#define opdev ((gx_device_os2prn *)pdev)

/************************************************/


/* ------ Private definitions ------ */


/* new os2prn_print_page routine */

/* Write BMP header to memory, then send bitmap to printer */
/* one scan line at a time */
private int
os2prn_print_page(gx_device_printer *pdev, FILE *file)
{	int raster = gdev_prn_raster(pdev);
	/* BMP scan lines are padded to 32 bits. */
	ulong bmp_raster = (raster+3) & (~3);
	ulong bmp_raster_multi;
	int height = pdev->height;
	int depth = pdev->color_info.depth;
	byte *row;
	int y;
	int code = 0;			/* return code */
	POINTL apts[4];
	APIRET rc;
	POINTL aptsb[4];
	HBITMAP hbmp, hbmr;
	int i, lines;
	int ystart, yend;
	int yslice;

	struct bmi_s {
		BITMAPINFOHEADER2 h;
		RGB2 pal[256];
	} bmi;

	yslice = 65535 / bmp_raster;
	bmp_raster_multi = bmp_raster * yslice;
	row = (byte *)gs_malloc(bmp_raster_multi, 1, "bmp file buffer");
	if ( row == 0 )			/* can't allocate row buffer */
		return_error(gs_error_VMerror);

	if (opdev->newframe)
	    DevEscape(opdev->hdc, DEVESC_NEWFRAME, 0L, NULL, NULL, NULL);
	opdev->newframe = 1;

	/* Write the info header. */

	memset(&bmi.h, 0, sizeof(bmi.h));
	bmi.h.cbFix = sizeof(bmi.h);
	bmi.h.cx = pdev->width;  /* opdev->mdev.width; */
	/* bmi.h.cy = height; */
	bmi.h.cy = yslice; 	/* size for memory PS */
	bmi.h.cPlanes = 1;
	bmi.h.cBitCount = pdev->color_info.depth;

	/* Write the palette. */

	if ( depth <= 8 )
	{	int i;
		gx_color_value rgb[3];
		PRGB2 pq;
		bmi.h.cclrUsed = 1 << depth;
		bmi.h.cclrImportant = 1 << depth;
		for ( i = 0; i != 1 << depth; i++ )
		{	(*dev_proc(pdev, map_color_rgb))((gx_device *)pdev,
				(gx_color_index)i, rgb);
			pq = &bmi.pal[i];
			pq->bRed   = gx_color_value_to_byte(rgb[0]);
			pq->bGreen = gx_color_value_to_byte(rgb[1]);
			pq->bBlue  = gx_color_value_to_byte(rgb[2]);
			pq->fcOptions = 0;
		}
	}
	else {
		bmi.h.cclrUsed = 0;
		bmi.h.cclrImportant = 0;
	}

	/* for GpiDrawBits */
	/* target is inclusive */
	apts[0].x = 0;
	apts[0].y = 0;  /* filled in later */
	apts[1].x = pdev->width-1;
	apts[1].y = 0;  /* filled in later */
	/* source is not inclusive of top & right borders */
	apts[2].x = 0;
	apts[2].y = 0;
	apts[3].x = pdev->width;
	apts[3].y = 0;  /* filled in later */

	/* for GpiBitBlt */
	/* target is not inclusive */
	aptsb[0].x = opdev->clipbox[0];
	aptsb[0].y = 0;  /* filled in later */
	aptsb[1].x = opdev->clipbox[2];
	aptsb[1].y = 0;  /* filled in later */
	/* source is not inclusive */
	aptsb[2].x = opdev->clipbox[0];
	aptsb[2].y = 0;
	aptsb[3].x = opdev->clipbox[2];
	aptsb[3].y = 0;	 /* filled in later */

	/* write the bits */
	ystart = opdev->clipbox[3];
	yend = opdev->clipbox[1];
	y = ystart;
	while (y > yend) {
	    /* create a bitmap for the memory DC */
	    hbmp = GpiCreateBitmap(opdev->hpsMem, &bmi.h, 0L, NULL, NULL);
	    if (hbmp == GPI_ERROR)
	       goto bmp_done;
	    hbmr = GpiSetBitmap(opdev->hpsMem, hbmp);

	    /* copy slice to memory bitmap */
	    if (y > yend + yslice)
		lines = yslice;
	    else
		lines = y - yend;
	    y -= lines;
	    for (i=lines-1; i>=0; i--)
	        gdev_prn_copy_scan_lines(pdev, ystart-1 - (y+i), row + (bmp_raster*i), raster);
	    apts[0].y = 0;		/* target */
	    apts[1].y = lines;
	    apts[3].y = lines-1;	/* source */
	    /* copy DIB bitmap to memory bitmap */
	    rc = GpiDrawBits(opdev->hpsMem, row, (BITMAPINFO2 *)&bmi, 4, apts, 
		(depth != 1) ? ROP_SRCCOPY : ROP_NOTSRCCOPY, 0);

	    /* copy slice to printer */
	    aptsb[0].y = y;
	    aptsb[1].y = y+lines;
	    aptsb[3].y = lines;
	    rc = GpiBitBlt(opdev->hps, opdev->hpsMem, 4, aptsb, ROP_SRCCOPY, BBO_IGNORE);

	    /* delete bitmap */
	    if (hbmr != HBM_ERROR)
		    GpiSetBitmap(opdev->hpsMem, (ULONG)0);
	    hbmr = HBM_ERROR;
	    if (hbmp != GPI_ERROR)
		    GpiDeleteBitmap(hbmp);
	    hbmp = GPI_ERROR;
	}

bmp_done:
	if (row)
	    gs_free((char *)row, bmp_raster_multi, 1, "bmp file buffer");

	return code;
}

/* combined color mappers */

/* 24-bit color mappers (taken from gdevmem2.c). */
/* Note that OS/2 expects RGB values in the order B,G,R. */

/* Map a r-g-b color to a color index. */
private gx_color_index
os2prn_map_rgb_color(gx_device *dev, gx_color_value r, gx_color_value g,
  gx_color_value b)
{
    switch(dev->color_info.depth) {
      case 1:
	return gdev_prn_map_rgb_color(dev, r, g, b);
      case 4:
	/* use only 8 colors */
	return  (r > (gx_max_color_value / 2 + 1) ? 4 : 0) +
	         (g > (gx_max_color_value / 2 + 1) ? 2 : 0) +
	         (b > (gx_max_color_value / 2 + 1) ? 1 : 0) ;
      case 8:
	return pc_8bit_map_rgb_color(dev, r, g, b);
      case 24:
	return gx_color_value_to_byte(r) +
	       ((uint)gx_color_value_to_byte(g) << 8) +
	       ((ulong)gx_color_value_to_byte(b) << 16);
    }
    return 0; /* error */
}

/* Map a color index to a r-g-b color. */
private int
os2prn_map_color_rgb(gx_device *dev, gx_color_index color,
  gx_color_value prgb[3])
{
    switch(dev->color_info.depth) {
      case 1:
	gdev_prn_map_color_rgb(dev, color, prgb);
	break;
      case 4:
	/* use only 8 colors */
	prgb[0] = (color & 4) ? gx_max_color_value : 0;
	prgb[1] = (color & 2) ? gx_max_color_value : 0;
	prgb[2] = (color & 1) ? gx_max_color_value : 0;
	break;
      case 8:
	pc_8bit_map_color_rgb(dev, color, prgb);
	break;
      case 24:
	prgb[2] = gx_color_value_from_byte(color >> 16);
	prgb[1] = gx_color_value_from_byte((color >> 8) & 0xff);
	prgb[0] = gx_color_value_from_byte(color & 0xff);
	break;
    }
    return 0;
}

void
os2prn_set_bpp(gx_device *dev, int depth)
{
	if (depth > 8) {
	    static const gx_device_color_info os2prn_24color = dci_std_color(24);
	    dev->color_info = os2prn_24color;
	}
	else if ( depth >= 8 ) {
	    /* 8-bit (SuperVGA-style) color. */
	    /* (Uses a fixed palette of 3,3,2 bits.) */
	    static const gx_device_color_info os2prn_8color = dci_pc_8bit;
	    dev->color_info = os2prn_8color;
	}
	else if ( depth >= 3) {
	    /* 3 plane printer */
	    /* suitable for impact dot matrix CMYK printers */
	    /* create 4-bit bitmap, but only use 8 colors */
	    static const gx_device_color_info os2prn_4color = {3, 4, 1, 1, 2, 2};
	    dev->color_info = os2prn_4color;
	}
	else {   /* default is black_and_white */
	    static const gx_device_color_info os2prn_1color = dci_std_color(1);
	    dev->color_info = os2prn_1color;
	}
}

/* Get list of queues from SplEnumQueue */
/* returns 0 if OK, non-zero for error */
private int
os2prn_get_queue_list(OS2QL *ql)
{
    SPLERR splerr;
    USHORT jobCount;
    ULONG  cbBuf;
    ULONG  cTotal;
    ULONG  cReturned;
    ULONG  cbNeeded;
    ULONG  ulLevel;
    ULONG  i;
    PSZ    pszComputerName;
    PBYTE  pBuf;
    PPRQINFO3 prq;
 
    ulLevel = 3L;
    pszComputerName = (PSZ)NULL ;
    splerr = SplEnumQueue(pszComputerName, ulLevel, pBuf, 0L, /* cbBuf */
                          &cReturned, &cTotal,
                          &cbNeeded, NULL);
    if ( splerr == ERROR_MORE_DATA || splerr == NERR_BufTooSmall ) {
	pBuf = gs_malloc(cbNeeded, 1, "OS/2 printer device info buffer");
	ql->prq = (PRQINFO3 *)pBuf;
	if (ql->prq != (PRQINFO3 *)NULL) {
	  ql->len = cbNeeded;
          cbBuf = cbNeeded ;
          splerr = SplEnumQueue(pszComputerName, ulLevel, pBuf, cbBuf,
                                  &cReturned, &cTotal,
                                  &cbNeeded, NULL);
          if (splerr == NO_ERROR) {
             /* Set pointer to point to the beginning of the buffer.           */
             prq = (PPRQINFO3)pBuf ;
             /* cReturned has the count of the number of PRQINFO3 structures.  */
	     ql->nqueues = cReturned;
	     ql->defqueue = 0;
             for (i=0;i < cReturned ; i++) {
		if ( prq->fsType & PRQ3_TYPE_APPDEFAULT )
		     ql->defqueue = i;
                prq++;
             }/*endfor cReturned */
          }
       }
    } 
    else {
       /* If we are here we had a bad error code. Print it and some other info.*/
       fprintf(stdout, "SplEnumQueue Error=%ld, Total=%ld, Returned=%ld, Needed=%ld\n",
               splerr, cTotal, cReturned, cbNeeded) ;
    }
    if (splerr)
        return splerr;
    return 0;
}


private void
os2prn_free_queue_list(OS2QL *ql)
{
    gs_free((char *)ql->prq, ql->len, 1, "os2prn queue list");
    ql->prq = NULL;
    ql->len = 0;
    ql->defqueue = 0;
    ql->nqueues = 0;
}
