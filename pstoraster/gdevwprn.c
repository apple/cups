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

/* gdevwprn.c */
/*
 * Microsoft Windows 3.n printer driver for Ghostscript.
 * Original version by Russell Lang and
 * L. Peter Deutsch, Aladdin Enterprises.
 */
#include "gdevmswn.h"
#include "gp.h"
#include "commdlg.h"

/*
 ****** NOTE: this module and gdevwddb should be refactored.
 * The drawing routines are almost identical.
 * The differences are that the mswinprn doesn't use an extra
 * palette (gdevwddb.c could probably be made to work with 
 * one palette also), mswinprn doesn't call win_update() because
 * hwndimg doesn't exist, and the HDC is hdcmf not hdcbit.
 ******/

/* Make sure we cast to the correct structure type. */
typedef struct gx_device_win_prn_s gx_device_win_prn;
#undef wdev
#define wdev ((gx_device_win_prn *)dev)

/* Forward references */
private void near win_prn_addtool(P2(gx_device_win_prn *, int));
private void near win_prn_maketools(P2(gx_device_win_prn *, HDC));
private void near win_prn_destroytools(P1(gx_device_win_prn *));

/* Device procedures */

/* See gxdevice.h for the definitions of the procedures. */
private dev_proc_open_device(win_prn_open);
private dev_proc_close_device(win_prn_close);
private dev_proc_sync_output(win_prn_sync_output);
private dev_proc_output_page(win_prn_output_page);
private dev_proc_map_rgb_color(win_prn_map_rgb_color);
private dev_proc_fill_rectangle(win_prn_fill_rectangle);
private dev_proc_tile_rectangle(win_prn_tile_rectangle);
private dev_proc_copy_mono(win_prn_copy_mono);
private dev_proc_copy_color(win_prn_copy_color);
private dev_proc_draw_line(win_prn_draw_line);

/* The device descriptor */
struct gx_device_win_prn_s {
	gx_device_common;
	gx_device_win_common;

	/* Handles */

	HPEN hpen, *hpens;
	uint hpensize;
	HBRUSH hbrush, *hbrushs;
	uint hbrushsize;
#define select_brush(color)\
  if (wdev->hbrush != wdev->hbrushs[color])\
   {	wdev->hbrush = wdev->hbrushs[color];\
	SelectObject(wdev->hdcmf,wdev->hbrush);\
   }
	/* A staging bitmap for copy_mono. */
	/* We want one big enough to handle the standard 16x16 halftone; */
	/* this is also big enough for ordinary-size characters. */

#define bmWidthBytes 4		/* must be even */
#define bmWidthBits (bmWidthBytes * 8)
#define bmHeight 32
	HBITMAP FAR hbmmono;
	HDC FAR hdcmono;
	gx_bitmap_id bm_id;

	HDC hdcprn;
	HDC hdcmf;
	char mfname[128];
	DLGPROC lpfnAbortProc;
};
private gx_device_procs win_prn_procs = {
	win_prn_open,
	NULL,			/* get_initial_matrix */
	win_prn_sync_output,
	win_prn_output_page,
	win_prn_close,
	win_prn_map_rgb_color,
	win_map_color_rgb,
	win_prn_fill_rectangle,
	win_prn_tile_rectangle,
	win_prn_copy_mono,
	win_prn_copy_color,
	win_prn_draw_line,
	NULL,			/* get_bits */
	NULL,			/* get_params */
	NULL,			/* put_params */
	NULL,			/* map_cmyk_color */
	win_get_xfont_procs
};
gx_device_win_prn far_data gs_mswinprn_device = {
	std_device_std_body(gx_device_win_prn, &win_prn_procs, "mswinprn",
	  INITIAL_WIDTH, INITIAL_HEIGHT, 	/* win_open() fills these in later */
	  INITIAL_RESOLUTION, INITIAL_RESOLUTION	/* win_open() fills these in later */
	),
	 { 0 },				/* std_procs */
	0,				/* BitsPerPixel */
	2,				/* nColors */
};

/* Open the win_prn driver */
private int
win_prn_open(gx_device *dev)
{	int depth;
	PRINTDLG pd;
	FILE *f;
	POINT offset;
	POINT size;
	float m[4];

	memset(&pd, 0, sizeof(PRINTDLG));
	pd.lStructSize = sizeof(PRINTDLG);
	pd.hwndOwner = hwndtext;
	pd.Flags = PD_PRINTSETUP | PD_RETURNDC;
	if (!PrintDlg(&pd)) {
	    /* device not opened - exit ghostscript */
	    return gs_error_limitcheck;
	}
	GlobalFree(pd.hDevMode);
	GlobalFree(pd.hDevNames);
	pd.hDevMode = pd.hDevNames = NULL;
	wdev->hdcprn = pd.hDC;
	if (!(GetDeviceCaps(wdev->hdcprn, RASTERCAPS) != RC_BITBLT)) {
	    DeleteDC(wdev->hdcprn);
	    return gs_error_limitcheck;
	}

#ifdef __WIN32__
	wdev->lpfnAbortProc = (DLGPROC)AbortProc;
#else
#ifdef __DLL__
	wdev->lpfnAbortProc = (DLGPROC)GetProcAddress(phInstance, "AbortProc");
#else
	wdev->lpfnAbortProc = (DLGPROC)MakeProcInstance((FARPROC)AbortProc,phInstance);
#endif
#endif
	Escape(wdev->hdcprn,SETABORTPROC,0,(LPSTR)wdev->lpfnAbortProc,NULL);  
	if (Escape(wdev->hdcprn, STARTDOC, strlen(szAppName),szAppName, NULL) <= 0) {
#if !defined(__WIN32__) && !defined(__DLL__)
	    FreeProcInstance((FARPROC)wdev->lpfnAbortProc);
#endif
	    DeleteDC(wdev->hdcprn);
	    return gs_error_limitcheck;
	}

	f = gp_open_scratch_file(gp_scratch_file_name_prefix, 
			wdev->mfname, "wb");
	if (f == (FILE *)NULL) {
	    Escape(wdev->hdcprn,ENDDOC,0,NULL,NULL);
#if !defined(__WIN32__) && !defined(__DLL__)
	    FreeProcInstance((FARPROC)wdev->lpfnAbortProc);
#endif
	    DeleteDC(wdev->hdcprn);
	    return  gs_error_limitcheck;
	}
	unlink(wdev->mfname);
	wdev->hdcmf = CreateMetaFile(wdev->mfname);

	dev->x_pixels_per_inch = GetDeviceCaps(wdev->hdcprn, LOGPIXELSX);
	dev->y_pixels_per_inch = GetDeviceCaps(wdev->hdcprn, LOGPIXELSY);
	Escape(wdev->hdcprn, GETPHYSPAGESIZE, NULL, NULL, (LPPOINT)&size);
	dev->width = size.x;
	dev->height = size.y;
	Escape(wdev->hdcprn, GETPRINTINGOFFSET, NULL, NULL, (LPPOINT)&offset);
	m[0] /*left*/ = offset.x / dev->x_pixels_per_inch;
	m[3] /*top*/ = offset.y / dev->y_pixels_per_inch;
	m[2] /*right*/ =
		(size.x - offset.x - GetDeviceCaps(wdev->hdcprn, HORZRES))
		 / dev->x_pixels_per_inch;
	m[1] /*bottom*/ =
		(size.y - offset.y - GetDeviceCaps(wdev->hdcprn, VERTRES))
		 / dev->y_pixels_per_inch
		+ 0.15;  /* hack to add a bit more margin for deskjet printer */
	gx_device_set_margins(dev, m, true);

	/* Set parameters that were unknown before opening device */
	/* Find out if the device supports color */
	/* We recognize 2, 16 or 256 color devices */
	depth = GetDeviceCaps(wdev->hdcprn,PLANES) * GetDeviceCaps(wdev->hdcprn,BITSPIXEL);
	if ( depth >= 8 ) { /* use 64 static colors and 166 dynamic colors from 8 planes */
		static const gx_device_color_info win_256color = dci_color(8,31,4);
		dev->color_info = win_256color;
		wdev->nColors = 64;
	}
	else if ( depth >= 4 ) {
		static const gx_device_color_info win_16ega_color = dci_color(4, 2, 3);
		dev->color_info = win_16ega_color;
		wdev->nColors = 16;
	} 
	else {   /* default is black_and_white */
		wdev->nColors = 2;
	}

	/* create palette for display */
	if ((wdev->limgpalette = win_makepalette((gx_device_win *)dev))
		== (LPLOGPALETTE)NULL) {
		HMETAFILE hmf = CloseMetaFile(wdev->hdcmf);
		DeleteMetaFile(hmf);
		unlink(wdev->mfname);
		Escape(wdev->hdcprn,ENDDOC,0,NULL,NULL);
#if !defined(__WIN32__) && !defined(__DLL__)
	        FreeProcInstance((FARPROC)wdev->lpfnAbortProc);
#endif
		DeleteDC(wdev->hdcprn);
		return win_nomemory();
	}
	wdev->himgpalette = CreatePalette(wdev->limgpalette);

	/* Create the bitmap and DC for copy_mono. */
	wdev->hbmmono = CreateBitmap(bmWidthBits, bmHeight, 1, 1, NULL);
	wdev->hdcmono = CreateCompatibleDC(wdev->hdcprn);
	if ( wdev->hbmmono == NULL || wdev->hdcmono == NULL ) {
		HMETAFILE hmf = CloseMetaFile(wdev->hdcmf);
		DeleteMetaFile(hmf);
		unlink(wdev->mfname);
		Escape(wdev->hdcprn,ENDDOC,0,NULL,NULL);
#if !defined(__WIN32__) && !defined(__DLL__)
		FreeProcInstance((FARPROC)wdev->lpfnAbortProc);
#endif
		DeleteDC(wdev->hdcprn);
		gs_free((char *)(wdev->limgpalette), 1, sizeof(LOGPALETTE) + 
			(1<<(wdev->color_info.depth)) * sizeof(PALETTEENTRY),
			"win_prn_open");
		return win_nomemory();
	}
	SetMapMode(wdev->hdcmono, GetMapMode(wdev->hdcprn));
	SelectObject(wdev->hdcmono, wdev->hbmmono);
	(void) SelectPalette(wdev->hdcmf,wdev->himgpalette,NULL);
	RealizePalette(wdev->hdcmf);
	win_prn_maketools(wdev,wdev->hdcmf);
	wdev->bm_id = gx_no_bitmap_id;

	return 0;
}


/* Close the win_prn driver */
private int
win_prn_close(gx_device *dev)
{
HMETAFILE hmf;
	/* Free resources */
	Escape(wdev->hdcprn,ENDDOC,0,NULL,NULL);
#if !defined(__WIN32__) && !defined(__DLL__)
	FreeProcInstance((FARPROC)wdev->lpfnAbortProc);
#endif
	DeleteDC(wdev->hdcprn);
	hmf = CloseMetaFile(wdev->hdcmf);
	DeleteMetaFile(hmf);
	unlink(wdev->mfname);

	win_prn_destroytools(wdev);
	DeleteDC(wdev->hdcmono);
	DeleteObject(wdev->hbmmono);
	DeleteObject(wdev->himgpalette);
	gs_free((char *)(wdev->limgpalette), 1, sizeof(LOGPALETTE) + 
		(1<<(wdev->color_info.depth)) * sizeof(PALETTEENTRY),
		"win_prn_close");
	return(0);
}

/* Do nothing */
int
win_prn_sync_output(gx_device *dev)
{
    return 0;
}

/* Write page to printer */
int
win_prn_output_page(gx_device *dev, int copies, int flush)
{
RECT rect;
HMETAFILE hmf;
	hmf = CloseMetaFile(wdev->hdcmf);
	
	Escape(wdev->hdcprn, NEXTBAND, NULL, NULL, (LPRECT)&rect);
	while (!IsRectEmpty(&rect)) {
	    PlayMetaFile(wdev->hdcprn, hmf);
	    if (Escape(wdev->hdcprn, NEXTBAND, NULL, NULL, (LPRECT)&rect) <= 0)
		break;
	}
	DeleteMetaFile(hmf);
	unlink(wdev->mfname);
	wdev->hdcmf = CreateMetaFile(wdev->mfname);
	(void) SelectPalette(wdev->hdcmf,wdev->himgpalette,NULL);
	RealizePalette(wdev->hdcmf);
	SelectObject(wdev->hdcmf,wdev->hpen);
	SelectObject(wdev->hdcmf,wdev->hbrush);

	return 0;
}


/* Map a r-g-b color to the colors available under Windows */
private gx_color_index
win_prn_map_rgb_color(gx_device *dev, gx_color_value r, gx_color_value g,
  gx_color_value b)
{	int i = wdev->nColors;
	gx_color_index color = win_map_rgb_color(dev, r, g, b);
	if ( color != i ) return color;
	(void) SelectPalette(wdev->hdcmf,wdev->himgpalette,NULL);
	RealizePalette(wdev->hdcmf);
	win_prn_addtool(wdev, i);

	return color;
}


/* Macro for filling a rectangle with a color. */
/* Note that it starts with a declaration. */
#define fill_rect(x, y, w, h, color)\
RECT rect;\
rect.left = x, rect.top = y;\
rect.right = x + w, rect.bottom = y + h;\
FillRect(wdev->hdcmf, &rect, wdev->hbrushs[(int)color])


/* Fill a rectangle. */
private int
win_prn_fill_rectangle(gx_device *dev, int x, int y, int w, int h,
  gx_color_index color)
{
	fit_fill(dev, x, y, w, h);
	/* Use PatBlt for filling.  Special-case black. */
	if ( color == 0 )
		PatBlt(wdev->hdcmf, x, y, w, h, rop_write_0s);
	else
	{	select_brush((int)color);
		PatBlt(wdev->hdcmf, x, y, w, h, rop_write_pattern);
	}

	return 0;
}

/* Tile a rectangle.  If neither color is transparent, */
/* pre-clear the rectangle to color0 and just tile with color1. */
/* This is faster because of how win_copy_mono is implemented. */
/* Note that this also does the right thing for colored tiles. */
private int
win_prn_tile_rectangle(gx_device *dev, const gx_tile_bitmap *tile,
  int x, int y, int w, int h, gx_color_index czero, gx_color_index cone,
  int px, int py)
{	fit_fill(dev, x, y, w, h);
	if ( czero != gx_no_color_index && cone != gx_no_color_index )
	   {	fill_rect(x, y, w, h, czero);
		czero = gx_no_color_index;
	   }
	if ( tile->raster == bmWidthBytes && tile->size.y <= bmHeight &&
	     (px | py) == 0 && cone != gx_no_color_index
	   )
	{	/* We can do this much more efficiently */
		/* by using the internal algorithms of copy_mono */
		/* and gx_default_tile_rectangle. */
		int width = tile->size.x;
		int height = tile->size.y;
		int rwidth = tile->rep_width;
		int irx = ((rwidth & (rwidth - 1)) == 0 ? /* power of 2 */
			x & (rwidth - 1) :
			x % rwidth);
		int ry = y % tile->rep_height;
		int icw = width - irx;
		int ch = height - ry;
		int ex = x + w, ey = y + h;
		int fex = ex - width, fey = ey - height;
		int cx, cy;

		select_brush((int)cone);

		if ( tile->id != wdev->bm_id || tile->id == gx_no_bitmap_id )
		{	wdev->bm_id = tile->id;
			SetBitmapBits(wdev->hbmmono,
				      (DWORD)(bmWidthBytes * tile->size.y),
				      (BYTE *)tile->data);
		}

#define copy_tile(srcx, srcy, tx, ty, tw, th)\
  BitBlt(wdev->hdcmf, tx, ty, tw, th, wdev->hdcmono, srcx, srcy, rop_write_at_1s)

		if ( ch > h ) ch = h;
		for ( cy = y; ; )
		   {	if ( w <= icw )
				copy_tile(irx, ry, x, cy, w, ch);
			else
			{	copy_tile(irx, ry, x, cy, icw, ch);
				cx = x + icw;
				while ( cx <= fex )
				{	copy_tile(0, ry, cx, cy, width, ch);
					cx += width;
				}
				if ( cx < ex )
				{	copy_tile(0, ry, cx, cy, ex - cx, ch);
				}
			}
			if ( (cy += ch) >= ey ) break;
			ch = (cy > fey ? ey - cy : height);
			ry = 0;
		   }

		return 0;
	}
	return gx_default_tile_rectangle(dev, tile, x, y, w, h, czero, cone, px, py);
}


/* Draw a line */
private int
win_prn_draw_line(gx_device *dev, int x0, int y0, int x1, int y1,
  gx_color_index color)
{
	if (wdev->hpen != wdev->hpens[(int)color]) {
		wdev->hpen = wdev->hpens[(int)color];
		SelectObject(wdev->hdcmf,wdev->hpen);
	}
#ifdef __WIN32__
	MoveToEx(wdev->hdcmf, x0, y0, NULL);
#else
	MoveTo(wdev->hdcmf, x0, y0);
#endif
	LineTo(wdev->hdcmf, x1, y1);
	return 0;
}

/* Copy a monochrome bitmap.  The colors are given explicitly. */
/* Color = gx_no_color_index means transparent (no effect on the image). */
private int
win_prn_copy_mono(gx_device *dev,
  const byte *base, int sourcex, int raster, gx_bitmap_id id,
  int x, int y, int w, int h,
  gx_color_index zero, gx_color_index one)
{	int endx;
	const byte *ptr_line;
	int width_bytes, height;
	DWORD rop = rop_write_at_1s;
	int color;
	BYTE aBit[bmWidthBytes * bmHeight];
	BYTE *aptr = aBit;

	fit_copy(dev, base, sourcex, raster, id, x, y, w, h);

	if ( sourcex & ~7 )
	{	base += sourcex >> 3;
		sourcex &= 7;
	}

	/* Break up large transfers into smaller ones. */
	while ( (endx = sourcex + w) > bmWidthBits )
	{	int lastx = (endx - 1) & -bmWidthBits;
		int subw = endx - lastx;
		int code = win_prn_copy_mono(dev, base, lastx,
					     raster, gx_no_bitmap_id,
					     x + lastx - sourcex, y,
					     subw, h, zero, one);
		if ( code < 0 ) return code;
		w -= subw;
	}
	while ( h > bmHeight )
	{	int code;
		h -= bmHeight;
		code = win_prn_copy_mono(dev, base + h * raster, sourcex,
					 raster, gx_no_bitmap_id,
					 x, y + h, w, bmHeight, zero, one);
		if ( code < 0 ) return code;
	}

	width_bytes = (sourcex + w + 7) >> 3;
	ptr_line = base;

	if ( zero == gx_no_color_index )
	   {	if ( one == gx_no_color_index ) return 0;
		color = (int)one;
		if ( color == 0 )
			rop = rop_write_0_at_1s;
		else
			select_brush(color);
	   }
	else
	   {	if ( one == gx_no_color_index )
		   {	color = (int)zero;
			rop = rop_write_at_0s;
		   }
		else
		   {	/* Pre-clear the rectangle to zero */
			fill_rect(x, y, w, h, zero);
			color = (int)one;
		   }
		select_brush(color);
	   }

	if ( id != wdev->bm_id || id == gx_no_bitmap_id )
	{	wdev->bm_id = id;
		if ( raster == bmWidthBytes )
		{	/* We can do the whole thing in a single transfer! */
			SetBitmapBits(wdev->hbmmono,
				      (DWORD)(bmWidthBytes * h),
				      (BYTE *)base);
		}
		else
		{	for ( height = h; height--;
			      ptr_line += raster, aptr += bmWidthBytes
			    )
			{	/* Pack the bits into the bitmap. */
				switch ( width_bytes )
				{
					default: memcpy(aptr, ptr_line, width_bytes); break;
					case 4: aptr[3] = ptr_line[3];
					case 3: aptr[2] = ptr_line[2];
					case 2: aptr[1] = ptr_line[1];
					case 1: aptr[0] = ptr_line[0];
				}
			}
			SetBitmapBits(wdev->hbmmono,
				      (DWORD)(bmWidthBytes * h),
				      &aBit[0]);
		}
	}

	BitBlt(wdev->hdcmf, x, y, w, h, wdev->hdcmono, sourcex, 0, rop);
	return 0;
}


/* Copy a color pixel map.  This is just like a bitmap, except that */
/* each pixel takes 8 or 4 bits instead of 1 when device driver has color. */
private int
win_prn_copy_color(gx_device *dev,
  const byte *base, int sourcex, int raster, gx_bitmap_id id,
  int x, int y, int w, int h)
{
	fit_copy(dev, base, sourcex, raster, id, x, y, w, h);

	if ( gx_device_has_color(dev) )
	{
	switch(dev->color_info.depth) {
	  case 8:
	    {	int xi, yi;
		int skip = raster - w;
		const byte *sptr = base + sourcex;
  		if ( w <= 0 ) return 0;
  		if ( x < 0 || x + w > dev->width )
			return_error(gs_error_rangecheck);
		for ( yi = y; yi - y < h; yi++ )
		   {
			for ( xi = x; xi - x < w; xi++ )
			   {	int color =  *sptr++;
				SetPixel(wdev->hdcmf,xi,yi,PALETTEINDEX(color));
			   }
			sptr += skip;
		   }
		}
		break;
	  case 4:
	   {	/* color device, four bits per pixel */
		const byte *line = base + (sourcex >> 1);
		int dest_y = y, end_x = x + w;

		if ( w <= 0 ) return 0;
		while ( h-- )              /* for each line */
		   {	const byte *source = line;
			register int dest_x = x;
			if ( sourcex & 1 )    /* odd nibble first */
			   {	int color =  *source++ & 0xf;
				SetPixel(wdev->hdcmf,dest_x,dest_y,PALETTEINDEX(color));
				dest_x++;
			   }
			/* Now do full bytes */
			while ( dest_x < end_x )
			   {	int color = *source >> 4;
				SetPixel(wdev->hdcmf,dest_x,dest_y,PALETTEINDEX(color));
				dest_x++;
				if ( dest_x < end_x )
				   {	color =  *source++ & 0xf;
					SetPixel(wdev->hdcmf,dest_x,dest_y,PALETTEINDEX(color));
					dest_x++;
				   }
			   }
			dest_y++;
			line += raster;
		   }
	   }
	   break;
	default:
		return(-1); /* panic */
	}
	}
	else 
	/* monochrome device: one bit per pixel */
	   {	/* bitmap is the same as win_copy_mono: one bit per pixel */
		win_prn_copy_mono(dev, base, sourcex, raster, id, x, y, w, h,
			(gx_color_index)0, 
			(gx_color_index)(dev->color_info.depth==8 ? 63 : dev->color_info.max_gray));
	   }
	return 0;
}


/* ------ Internal routines ------ */

#undef wdev


private void near
win_prn_addtool(gx_device_win_prn *wdev, int i)
{
	wdev->hpens[i] = CreatePen(PS_SOLID, 1, PALETTEINDEX(i));
	wdev->hbrushs[i] = CreateSolidBrush(PALETTEINDEX(i));
}


private void near
win_prn_maketools(gx_device_win_prn *wdev, HDC hdc)
{	int i;
	wdev->hpensize = (1<<(wdev->color_info.depth)) * sizeof(HPEN);
	wdev->hpens = (HPEN *)gs_malloc(1, wdev->hpensize,
					"win_prn_maketools(pens)");
	wdev->hbrushsize = (1<<(wdev->color_info.depth)) * sizeof(HBRUSH);
	wdev->hbrushs = (HBRUSH *)gs_malloc(1, wdev->hbrushsize,
					    "win_prn_maketools(brushes)");
	if (wdev->hpens && wdev->hbrushs) {
		for (i=0; i<wdev->nColors; i++)
			win_prn_addtool(wdev, i);

		wdev->hpen = wdev->hpens[0];
		SelectObject(hdc,wdev->hpen);

		wdev->hbrush = wdev->hbrushs[0];
		SelectObject(hdc,wdev->hbrush);
	}
}


private void near
win_prn_destroytools(gx_device_win_prn *wdev)
{	int i;
	for (i=0; i<wdev->nColors; i++) {
		DeleteObject(wdev->hpens[i]);
		DeleteObject(wdev->hbrushs[i]);
	}
	gs_free((char *)wdev->hbrushs, 1, wdev->hbrushsize,
		"win_prn_destroytools(brushes)");
	gs_free((char *)wdev->hpens, 1, wdev->hpensize,
		"win_prn_destroytools(pens)");
}
