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

/* gdevwpr2.c */
/*
 * Microsoft Windows 3.n printer driver for Ghostscript.
 * Original version by Russell Lang and
 * L. Peter Deutsch, Aladdin Enterprises.
 * Modified by rjl 1995-03-29 to use BMP printer code
 */

/* This driver uses the printer default size and resolution and
 * ignores page size and resolution set using -gWIDTHxHEIGHT and
 * -rXxY.  You must still set the correct PageSize to get the
 * correct clipping path
 */

#include "gdevprn.h"
#include "gdevpccm.h"

#include "windows_.h"
#include <shellapi.h>
#include "gp_mswin.h"

#include "gp.h"
#include "commdlg.h"


/* Make sure we cast to the correct structure type. */
typedef struct gx_device_win_pr2_s gx_device_win_pr2;
#undef wdev
#define wdev ((gx_device_win_pr2 *)dev)

/* Device procedures */

/* See gxdevice.h for the definitions of the procedures. */
private dev_proc_open_device(win_pr2_open);
private dev_proc_close_device(win_pr2_close);
private dev_proc_print_page(win_pr2_print_page);
private dev_proc_map_rgb_color(win_pr2_map_rgb_color);
private dev_proc_map_color_rgb(win_pr2_map_color_rgb);
private dev_proc_put_params(win_pr2_put_params);

private void win_pr2_set_bpp(gx_device *dev, int depth);

private gx_device_procs win_pr2_procs =
  prn_color_params_procs(win_pr2_open, gdev_prn_output_page, win_pr2_close,
   win_pr2_map_rgb_color, win_pr2_map_color_rgb, 
   gdev_prn_get_params, win_pr2_put_params);


/* The device descriptor */
typedef struct gx_device_win_pr2_s gx_device_win_pr2;
struct gx_device_win_pr2_s {
	gx_device_common;
	gx_prn_device_common;
	HDC hdcprn;
	DLGPROC lpfnAbortProc;
	DLGPROC lpfnCancelProc;
};

gx_device_win_pr2 far_data gs_mswinpr2_device = {
  prn_device_std_body(gx_device_win_pr2, win_pr2_procs, "mswinpr2",
  DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS, 72, 72,
  0, 0, 0, 0,
  0, win_pr2_print_page), /* depth = 0 */
  0,	/* hdcprn */
  NULL	/* lpfnAbortProc */
};

private int win_pr2_getdc(gx_device_win_pr2 *);

/* Open the win_pr2 driver */
private int
win_pr2_open(gx_device *dev)
{	int code;
	int depth;
	PRINTDLG pd;
	POINT offset;
	POINT size;
	float m[4];
	FILE *pfile;

	if (hDlgModeless) {
	    /* device cannot opened twice since only one hDlgModeless */
	    return gs_error_limitcheck;
	}

	/* get a HDC for the printer */
        if (!win_pr2_getdc(wdev)) {
	    /* couldn't get a printer from -sOutputFile= */
	    /* Prompt with dialog box */
	    memset(&pd, 0, sizeof(pd));
	    pd.lStructSize = sizeof(pd);
	    pd.hwndOwner = hwndtext;
	    pd.Flags = PD_PRINTSETUP | PD_RETURNDC;
	    if (!PrintDlg(&pd)) {
		/* device not opened - exit ghostscript */
	        return gs_error_Fatal;	/* exit Ghostscript cleanly */
	    }
	    GlobalFree(pd.hDevMode);
	    GlobalFree(pd.hDevNames);
	    pd.hDevMode = pd.hDevNames = NULL;
	    wdev->hdcprn = pd.hDC;
	}
	if (!(GetDeviceCaps(wdev->hdcprn, RASTERCAPS) != RC_DIBTODEV)) {
	    fprintf(stderr, "Windows printer does not have RC_DIBTODEV\n");
	    DeleteDC(wdev->hdcprn);
	    return gs_error_limitcheck;
	}

	/* initialise printer, install abort proc */
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
	if (Escape(wdev->hdcprn, STARTDOC, lstrlen(szAppName), szAppName, NULL) <= 0) {
#if !defined(__WIN32__) && !defined(__DLL__)
	    FreeProcInstance((FARPROC)wdev->lpfnAbortProc);
#endif
	    DeleteDC(wdev->hdcprn);
	    return gs_error_limitcheck;
	}

	dev->x_pixels_per_inch = GetDeviceCaps(wdev->hdcprn, LOGPIXELSX);
	dev->y_pixels_per_inch = GetDeviceCaps(wdev->hdcprn, LOGPIXELSY);
	Escape(wdev->hdcprn, GETPHYSPAGESIZE, NULL, NULL, (LPPOINT)&size);
	gx_device_set_width_height(dev, (int)size.x, (int)size.y);
	Escape(wdev->hdcprn, GETPRINTINGOFFSET, NULL, NULL, (LPPOINT)&offset);
	/* m[] gives margins in inches */
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

	depth = dev->color_info.depth;
	if (depth == 0) {
	    /* Set parameters that were unknown before opening device */
	    /* Find out if the device supports color */
	    /* We recognize 1, 4 (but uses only 3), 8 and 24 bit color devices */
	    depth = GetDeviceCaps(wdev->hdcprn,PLANES) * GetDeviceCaps(wdev->hdcprn,BITSPIXEL);
	}
	win_pr2_set_bpp(dev, depth);

	/* gdev_prn_open opens a temporary file which we don't want */
	/* so we specify the name now so we can delete it later */
	pfile = gp_open_scratch_file(gp_scratch_file_name_prefix, 
		wdev->fname, "wb");
	fclose(pfile);
	code = gdev_prn_open(dev);
	/* delete unwanted temporary file */
	unlink(wdev->fname);

	/* inform user of progress with dialog box and allow cancel */
#ifdef __WIN32__
	wdev->lpfnCancelProc = (DLGPROC)CancelDlgProc;
#else
#ifdef __DLL__
	wdev->lpfnCancelProc = (DLGPROC)GetProcAddress(phInstance, "CancelDlgProc");
#else
	wdev->lpfnCancelProc = (DLGPROC)MakeProcInstance((FARPROC)CancelDlgProc, phInstance);
#endif
#endif
	hDlgModeless = CreateDialog(phInstance, "CancelDlgBox", hwndtext, wdev->lpfnCancelProc);
	ShowWindow(hDlgModeless, SW_HIDE);

	return code;
};

/* Close the win_pr2 driver */
private int
win_pr2_close(gx_device *dev)
{	int code;
	int aborted = FALSE;
	/* Free resources */

	if (!hDlgModeless)
	    aborted = TRUE;
	DestroyWindow(hDlgModeless);
	hDlgModeless = 0;
#if !defined(__WIN32__) && !defined(__DLL__)
	FreeProcInstance((FARPROC)wdev->lpfnCancelProc);
#endif

	if (aborted)
	    Escape(wdev->hdcprn,ABORTDOC,0,NULL,NULL);
	else
	    Escape(wdev->hdcprn,ENDDOC,0,NULL,NULL);

#if !defined(__WIN32__) && !defined(__DLL__)
	FreeProcInstance((FARPROC)wdev->lpfnAbortProc);
#endif
	DeleteDC(wdev->hdcprn);
	code = gdev_prn_close(dev);
	return code;
}


/* ------ Internal routines ------ */

#undef wdev
#define wdev ((gx_device_win_pr2 *)pdev)

/************************************************/


/* ------ Private definitions ------ */


/* new win_pr2_print_page routine */

/* Write BMP header to memory, then send bitmap to printer */
/* one scan line at a time */
private int
win_pr2_print_page(gx_device_printer *pdev, FILE *file)
{	int raster = gdev_prn_raster(pdev);
	/* BMP scan lines are padded to 32 bits. */
	ulong bmp_raster = raster + (-raster & 3);
	ulong bmp_raster_multi;
	int scan_lines, yslice, lines, i;
	int width;
	int depth = pdev->color_info.depth;
	byte *row;
	int y;
	int code = 0;			/* return code */
	MSG msg;
	char dlgtext[32];
	HGLOBAL hrow;

	struct bmi_s {
		BITMAPINFOHEADER h;
		RGBQUAD pal[256];
	} bmi;

	scan_lines = dev_print_scan_lines(pdev);
	width = pdev->width - ((dev_l_margin(pdev) + dev_r_margin(pdev) -
		dev_x_offset(pdev)) * pdev->x_pixels_per_inch);

	yslice = 65535 / bmp_raster;	/* max lines in 64k */
	bmp_raster_multi = bmp_raster * yslice;
	hrow = GlobalAlloc(0, bmp_raster_multi);
	row = GlobalLock(hrow);
	if ( row == 0 )			/* can't allocate row buffer */
		return_error(gs_error_VMerror);

	/* Write the info header. */

	bmi.h.biSize = sizeof(bmi.h);
	bmi.h.biWidth = pdev->width;  /* wdev->mdev.width; */
	bmi.h.biHeight = yslice;
	bmi.h.biPlanes = 1;
	bmi.h.biBitCount = pdev->color_info.depth;
	bmi.h.biCompression = 0;
	bmi.h.biSizeImage = 0;			/* default */
	bmi.h.biXPelsPerMeter = 0;		/* default */
	bmi.h.biYPelsPerMeter = 0;		/* default */

	/* Write the palette. */

	if ( depth <= 8 )
	{	int i;
		gx_color_value rgb[3];
		LPRGBQUAD pq;
		bmi.h.biClrUsed = 1 << depth;
		bmi.h.biClrImportant = 1 << depth;
		for ( i = 0; i != 1 << depth; i++ )
		{	(*dev_proc(pdev, map_color_rgb))((gx_device *)pdev,
				(gx_color_index)i, rgb);
			pq = &bmi.pal[i];
			pq->rgbRed   = gx_color_value_to_byte(rgb[0]);
			pq->rgbGreen = gx_color_value_to_byte(rgb[1]);
			pq->rgbBlue  = gx_color_value_to_byte(rgb[2]);
			pq->rgbReserved = 0;
		}
	}
	else {
		bmi.h.biClrUsed = 0;
		bmi.h.biClrImportant = 0;
	}

	sprintf(dlgtext, "Printing page %d", (int)(pdev->PageCount)+1);
	SetWindowText(GetDlgItem(hDlgModeless, CANCEL_PRINTING), dlgtext);
	ShowWindow(hDlgModeless, SW_SHOW);

	for ( y = 0; y < scan_lines; ) {
	    /* copy slice to row buffer */
	    if (y > scan_lines - yslice)
		lines = scan_lines - y;
	    else
		lines = yslice;
	    for (i=0; i<lines; i++)
	        gdev_prn_copy_scan_lines(pdev, y+i, 
		    row + (bmp_raster*(lines-1-i)), raster);
	    SetDIBitsToDevice(wdev->hdcprn, 0, y, pdev->width, lines,
		0, 0, 0, lines,
		row,
		(BITMAPINFO FAR *)&bmi, DIB_RGB_COLORS);
	    y += lines;

	    /* inform user of progress */
	    sprintf(dlgtext, "%d%% done", (int)(y * 100L / scan_lines));
	    SetWindowText(GetDlgItem(hDlgModeless, CANCEL_PCDONE), dlgtext);
	    /* process message loop */
	    while (PeekMessage(&msg, hDlgModeless, 0, 0, PM_REMOVE)) {
		if ((hDlgModeless == 0) || !IsDialogMessage(hDlgModeless, &msg)) {
		    TranslateMessage(&msg);
		    DispatchMessage(&msg);
		}
	    }
	    if (hDlgModeless == 0) {
	        /* user pressed cancel button */
		break;
	    }
	}

	if (hDlgModeless == 0)
	    code = gs_error_Fatal;	/* exit Ghostscript cleanly */
	else {
	    /* push out the page */
	    SetWindowText(GetDlgItem(hDlgModeless, CANCEL_PCDONE), "Ejecting page...");
	    Escape(wdev->hdcprn,NEWFRAME,0,NULL,NULL);
	    ShowWindow(hDlgModeless, SW_HIDE);
	}

bmp_done:
	GlobalUnlock(hrow);
	GlobalFree(hrow);

	return code;
}

/* combined color mappers */

/* 24-bit color mappers (taken from gdevmem2.c). */
/* Note that Windows expects RGB values in the order B,G,R. */

/* Map a r-g-b color to a color index. */
private gx_color_index
win_pr2_map_rgb_color(gx_device *dev, gx_color_value r, gx_color_value g,
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
win_pr2_map_color_rgb(gx_device *dev, gx_color_index color,
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
win_pr2_set_bpp(gx_device *dev, int depth)
{
	if (depth > 8) {
	    static const gx_device_color_info win_pr2_24color = dci_std_color(24);
	    dev->color_info = win_pr2_24color;
	}
	else if ( depth >= 8 ) {
	    /* 8-bit (SuperVGA-style) color. */
	    /* (Uses a fixed palette of 3,3,2 bits.) */
	    static const gx_device_color_info win_pr2_8color = dci_pc_8bit;
	    dev->color_info = win_pr2_8color;
	}
	else if ( depth >= 3) {
	    /* 3 plane printer */
	    /* suitable for impact dot matrix CMYK printers */
	    /* create 4-bit bitmap, but only use 8 colors */
	    static const gx_device_color_info win_pr2_4color = {3, 4, 1, 1, 2, 2};
	    dev->color_info = win_pr2_4color;
	}
	else {   /* default is black_and_white */
	    static const gx_device_color_info win_pr2_1color = dci_std_color(1);
	    dev->color_info = win_pr2_1color;
	}
}


/* We implement this ourselves so that we can change BitsPerPixel */
/* before the device is opened */
int
win_pr2_put_params(gx_device *dev, gs_param_list *plist)
{	int ecode = 0, code;
	int old_bpp = dev->color_info.depth;
	int bpp = old_bpp;

	switch ( code = param_read_int(plist, "BitsPerPixel", &bpp) )
	{
	case 0:
		if ( dev->is_open )
		  ecode = gs_error_rangecheck;
		else
		  {	/* change dev->color_info is valid before device is opened */
			win_pr2_set_bpp(dev, bpp);
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
	return ecode;
}

#undef wdev

#ifndef __WIN32__
#include <print.h>
#endif


/* Get Device Context for printer */
private int
win_pr2_getdc(gx_device_win_pr2 *wdev)
{
char *device;
char *devices;
char *p;
char driverbuf[512];
char *driver;
char *output;
char *devcap;
int devcapsize;
int size;

int i, n;
POINT *pp;
int paperindex;
int paperwidth, paperheight;
int orientation;
int papersize;
char papername[64];
char drvname[32];
HINSTANCE hlib;
LPFNDEVMODE pfnExtDeviceMode;
LPFNDEVCAPS pfnDeviceCapabilities;
LPDEVMODE podevmode, pidevmode;

#ifdef __WIN32__
HANDLE hprinter;
#endif


    /* first try to derive the printer name from -sOutputFile= */
    /* is printer if name prefixed by \\spool\ */
    if ( is_spool(wdev->fname) )
	device = wdev->fname + 8;   /* skip over \\spool\ */
    else
	return FALSE;

    /* now try to match the printer name against the [Devices] section */
    if ( (devices = gs_malloc(4096, 1, "win_pr2_getdc")) == (char *)NULL )
	return FALSE;
    GetProfileString("Devices", NULL, "", devices, 4096);
    p = devices;
    while (*p) {
	if (strcmp(p, device) == 0)
	    break;
	p += strlen(p) + 1;
    }
    if (*p == '\0')
	p = NULL;
    gs_free(devices, 4096, 1, "win_pr2_getdc");
    if (p == NULL)
	return FALSE;	/* doesn't match an available printer */

    /* the printer exists, get the remaining information from win.ini */
    GetProfileString("Devices", device, "", driverbuf, sizeof(driverbuf));
    driver = strtok(driverbuf, ",");
    output = strtok(NULL, ",");
#ifdef __WIN32__
    if (is_win32s)
#endif
    {
	strcpy(drvname, driver);
	strcat(drvname, ".drv");
	driver = drvname;
    }


#ifdef __WIN32__

    if (!is_win32s) {
	if (!OpenPrinter(device, &hprinter, NULL))
	    return FALSE;
	size = DocumentProperties(NULL, hprinter, device, NULL, NULL, 0);
	if ( (podevmode = gs_malloc(size, 1, "win_pr2_getdc")) == (LPDEVMODE)NULL ) {
	    ClosePrinter(hprinter);
	    return FALSE;
	}
	if ( (pidevmode = gs_malloc(size, 1, "win_pr2_getdc")) == (LPDEVMODE)NULL ) {
	    gs_free(podevmode, size, 1, "win_pr2_getdc");
	    ClosePrinter(hprinter);
	    return FALSE;
	}
	DocumentProperties(NULL, hprinter, device, podevmode, NULL, DM_OUT_BUFFER);
	pfnDeviceCapabilities = (LPFNDEVCAPS)DeviceCapabilities;
    } else 
#endif
    {  /* Win16 and Win32s */
	/* now load the printer driver */
	hlib = LoadLibrary(driver);
	if (hlib < (HINSTANCE)HINSTANCE_ERROR)
	    return FALSE;

	/* call ExtDeviceMode() to get default parameters */
	pfnExtDeviceMode = (LPFNDEVMODE)GetProcAddress(hlib, "ExtDeviceMode");
	if (pfnExtDeviceMode == (LPFNDEVMODE)NULL) {
	    FreeLibrary(hlib);
	    return FALSE;
	}
	pfnDeviceCapabilities = (LPFNDEVCAPS)GetProcAddress(hlib, "DeviceCapabilities");
	if (pfnDeviceCapabilities == (LPFNDEVCAPS)NULL) {
	    FreeLibrary(hlib);
	    return FALSE;
	}
	size = pfnExtDeviceMode(NULL, hlib, NULL, device, output, NULL, NULL, 0);
	if ( (podevmode = gs_malloc(size, 1, "win_pr2_getdc")) == (LPDEVMODE)NULL ) {
	    FreeLibrary(hlib);
	    return FALSE;
	}
	if ( (pidevmode = gs_malloc(size, 1, "win_pr2_getdc")) == (LPDEVMODE)NULL ) {
	    gs_free(podevmode, size, 1, "win_pr2_getdc");
	    FreeLibrary(hlib);
	    return FALSE;
	}
	pfnExtDeviceMode(NULL, hlib, podevmode, device, output,
	    NULL, NULL, DM_OUT_BUFFER);
    }

    /* now find out what paper sizes are available */
    devcapsize = pfnDeviceCapabilities(device, output, DC_PAPERSIZE, NULL, NULL);
    devcapsize *= sizeof(POINT);
    if ( (devcap = gs_malloc(devcapsize, 1, "win_pr2_getdc")) == (LPBYTE)NULL )
        return FALSE;
    n = pfnDeviceCapabilities(device, output, DC_PAPERSIZE, devcap, NULL);
    paperwidth = wdev->MediaSize[0] / 72 * 254; 
    paperheight = wdev->MediaSize[1] / 72 * 254; 
    papername[0] = '\0';
    papersize = 0;
    paperindex = -1;
    orientation = DMORIENT_PORTRAIT;
    pp = (POINT *)devcap;
    for (i=0; i<n; i++, pp++) {
	if ( (pp->x < paperwidth + 20) && (pp->x > paperwidth - 20) &&
	     (pp->y < paperheight + 20) && (pp->y > paperheight - 20) ) {
	    paperindex = i;
	    paperwidth = pp->x;
	    paperheight = pp->y;
	    break;
	}
    }
    if (paperindex < 0) {
	/* try again in landscape */
        pp = (POINT *)devcap;
	for (i=0; i<n; i++, pp++) {
	    if ( (pp->x < paperheight + 20) && (pp->x > paperheight - 20) &&
		 (pp->y < paperwidth + 20) && (pp->y > paperwidth - 20) ) {
		paperindex = i;
		paperwidth = pp->x;
		paperheight = pp->y;
		orientation = DMORIENT_LANDSCAPE;
		break;
	    }
	}
    }
    gs_free(devcap, devcapsize, 1, "win_pr2_getdc");

    /* get the dmPaperSize */
    devcapsize = pfnDeviceCapabilities(device, output, DC_PAPERS, NULL, NULL);
    devcapsize *= sizeof(WORD);
    if ( (devcap = gs_malloc(devcapsize, 1, "win_pr2_getdc")) == (LPBYTE)NULL )
        return FALSE;
    n = pfnDeviceCapabilities(device, output, DC_PAPERS, devcap, NULL);
    if ( (paperindex >= 0) && (paperindex < n) )
	papersize = ((WORD *)devcap)[paperindex];
    gs_free(devcap, devcapsize, 1, "win_pr2_getdc");

    /* get the paper name */
    devcapsize = pfnDeviceCapabilities(device, output, DC_PAPERNAMES, NULL, NULL);
    devcapsize *= 64;
    if ( (devcap = gs_malloc(devcapsize, 1, "win_pr2_getdc")) == (LPBYTE)NULL )
        return FALSE;
    n = pfnDeviceCapabilities(device, output, DC_PAPERNAMES, devcap, NULL);
    if ( (paperindex >= 0) && (paperindex < n) )
	strcpy(papername, devcap + paperindex*64);
    gs_free(devcap, devcapsize, 1, "win_pr2_getdc");
	
    memcpy(pidevmode, podevmode, size);

    pidevmode->dmFields = 0;

    pidevmode->dmFields |= DM_DEFAULTSOURCE;
    pidevmode->dmDefaultSource = 0;

    pidevmode->dmFields |= DM_ORIENTATION;
    pidevmode->dmOrientation = orientation;

    if (papersize)
        pidevmode->dmFields |=  DM_PAPERSIZE;
    else
        pidevmode->dmFields &= (~DM_PAPERSIZE);
    pidevmode->dmPaperSize = papersize;

    pidevmode->dmFields |=  (DM_PAPERLENGTH | DM_PAPERWIDTH);
    pidevmode->dmPaperLength = paperheight;
    pidevmode->dmPaperWidth = paperwidth;


#ifdef WIN32
    if (!is_win32s) {
	/* change the page size by changing the form */
	/* WinNT only */
	/* Win95 returns FALSE to GetForm */
	LPBYTE lpbForm;
	FORM_INFO_1 *fi1;
	DWORD dwBuf;
	DWORD dwNeeded;
	dwNeeded = 0;
	dwBuf = 1024;
        if ( (lpbForm = gs_malloc(dwBuf, 1, "win_pr2_getdc")) == (LPBYTE)NULL ) {
	    gs_free(podevmode, size, 1, "win_pr2_getdc");
	    gs_free(pidevmode, size, 1, "win_pr2_getdc");
	    ClosePrinter(hprinter);
	    return FALSE;
	}
	if (GetForm(hprinter, papername, 1, lpbForm, dwBuf, &dwNeeded)) {
	    fi1 = (FORM_INFO_1 *)lpbForm;
	    pidevmode->dmFields |= DM_FORMNAME;
	    SetForm(hprinter, papername, 1, (LPBYTE)fi1);
	}
	gs_free(lpbForm, dwBuf, 1, "win_pr2_getdc");

	strcpy(pidevmode->dmFormName, papername);
	pidevmode->dmFields |= DM_FORMNAME;

/*
	pidevmode->dmFields &= DM_FORMNAME;
        pidevmode->dmDefaultSource = 0;
*/

	/* merge the entries */
	DocumentProperties(NULL, hprinter, device, podevmode, pidevmode, DM_IN_BUFFER | DM_OUT_BUFFER);

	ClosePrinter(hprinter);
	/* now get a DC */
	wdev->hdcprn = CreateDC(driver, device, NULL, podevmode);
    }
    else 
#endif
    { /* Win16 and Win32s */
	pfnExtDeviceMode(NULL, hlib, podevmode, device, output,
	    pidevmode, NULL, DM_IN_BUFFER | DM_OUT_BUFFER);
	/* release the printer driver */
	FreeLibrary(hlib);
	/* now get a DC */
	if (is_win32s)
	    strtok(driver, ".");	/* remove .drv */
	wdev->hdcprn = CreateDC(driver, device, output, podevmode);
    }

    gs_free(pidevmode, size, 1, "win_pr2_getdc");
    gs_free(podevmode, size, 1, "win_pr2_getdc");

    if (wdev->hdcprn != (HDC)NULL) 
	return TRUE;	/* success */
   
    /* fall back to prompting user */
    return FALSE;
}
