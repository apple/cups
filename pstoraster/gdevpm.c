/* Copyright (C) 1992, 1993, 1994, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gdevpm.c */
/*
 * OS/2 Presentation manager driver
 * By Russell Lang (based on gdevmswn.c and gdevwdib.c)
 *
 * If Ghostscript is a PM application, stdin/stdout are not
 * provided and so no text window is available.
 * If Ghostscript is a windowed text application, a message queue
 * can't be created so a PM window for graphics can't be created.
 * The solution used here is to have two programs - gsos2.exe is a 
 * text application and the outboard PM driver is gspmdrv.exe.
 * Another solution may be to make Ghostscript a PM application
 * and use VIO calls to provide a text window.
 *
 * If PM GSview starts Ghostscript, PM GSview displays the
 * bitmap instead of the PM driver (gspmdrv.exe).
 *
 * Since Ghostscript is not a PM application, this driver creates a
 * BMP bitmap in a named shared memory block and a second process
 * gspmdrv.exe reads this memory block and provides the PM window.  
 * Communication to gspmdrv.exe is via the shared memory block
 * and semaphores.
 */

#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_WINWINDOWMGR
#define INCL_DEV
#define INCL_GPIBITMAPS

#include <os2.h>
#include "string_.h"
#include <stdlib.h>
#include "gx.h"
#include "gserrors.h"
#include "gsexit.h"			/* for gs_exit_status */
#include "gxdevice.h"

#include "gp.h"
#include "gpcheck.h"
#include "gsparam.h"
#include "gdevpccm.h"

#include "gxdevmem.h"
#include "gdevpm.h"
#ifdef __DLL__
#include "gsdll.h"
#endif

#define MIN_COMMIT 4096    /* memory is committed in these size chunks */
#define ID_NAME "GSPMDRV_%u_%u"

/* Initial values for width and height */
#define INITIAL_RESOLUTION 96
#define INITIAL_WIDTH (INITIAL_RESOLUTION * 85 / 10 + 1)
#define INITIAL_HEIGHT (INITIAL_RESOLUTION * 11 + 1)

/* A macro for casting the device argument */
#define pmdev ((gx_device_pm *)dev)


#define pm_gsview_sizeof 80
typedef struct gx_device_pm_s gx_device_pm;

#define gx_device_pm_common\
	int BitsPerPixel;\
	int alpha_text;\
	int alpha_graphics;\
	int UpdateInterval;\
	char GSVIEW[pm_gsview_sizeof];\
	BOOL dll;\
	int nColors;\
	BOOL updating;\
	HTIMER update_timer;\
	HEV sync_event;\
	HEV next_event;\
	HMTX bmp_mutex;\
	HQUEUE drv_queue;\
	HQUEUE term_queue;\
	ULONG session_id;\
	PID process_id;\
	PID gspid;\
	unsigned char *bitmap;\
	ULONG committed;\
	PBITMAPINFO2 bmi

/* The device descriptor */
struct gx_device_pm_s {
	gx_device_common;
	gx_device_pm_common;
	gx_device_memory mdev;
};

/* Device procedures */

/* See gxdevice.h for the definitions of the procedures. */
private dev_proc_open_device(pm_open);
private dev_proc_get_initial_matrix(pm_get_initial_matrix);
private dev_proc_sync_output(pm_sync_output);
private dev_proc_output_page(pm_output_page);
private dev_proc_close_device(pm_close);
private dev_proc_map_rgb_color(pm_map_rgb_color);
private dev_proc_map_color_rgb(pm_map_color_rgb);
private dev_proc_fill_rectangle(pm_fill_rectangle);
private dev_proc_copy_mono(pm_copy_mono);
private dev_proc_copy_color(pm_copy_color);
private dev_proc_get_bits(pm_get_bits);
private dev_proc_get_params(pm_get_params);
private dev_proc_put_params(pm_put_params);
private dev_proc_get_alpha_bits(pm_get_alpha_bits);

private gx_device_procs pm_procs = {
	pm_open,
	pm_get_initial_matrix,
	pm_sync_output,
	pm_output_page,
	pm_close,
	pm_map_rgb_color,
	pm_map_color_rgb,
	pm_fill_rectangle,
	NULL,			/* tile rectangle */
	pm_copy_mono,
	pm_copy_color,
	NULL,			/* draw line */
	pm_get_bits,
	pm_get_params,
	pm_put_params,
	NULL,			/* map_cmyk_color */
	gx_default_get_xfont_procs,
	NULL,			/* get_xfont_device */
	NULL,			/* map_rgb_alpha_color */
	gx_page_device_get_page_device,
	pm_get_alpha_bits
};
#ifdef __DLL__
gx_device_pm far_data gs_os2dll_device = {
	std_device_std_body(gx_device_pm, &pm_procs, "os2dll",
	  INITIAL_WIDTH, INITIAL_HEIGHT,
	  INITIAL_RESOLUTION, INITIAL_RESOLUTION),
	 { 0 },		/* std_procs */
	8,		/* BitsPerPixel */
	1, 1,		/* alpha */
	5000,		/* UpdateInterval */
	"\0",		/* GSVIEW */
	1		/* is DLL device */
};
#endif
gx_device_pm far_data gs_os2pm_device = {
	std_device_std_body(gx_device_pm, &pm_procs, "os2pm",
	  INITIAL_WIDTH, INITIAL_HEIGHT,
	  INITIAL_RESOLUTION, INITIAL_RESOLUTION),
	 { 0 },		/* std_procs */
	8,		/* BitsPerPixel */
	1, 1,		/* alpha */
	5000,		/* UpdateInterval */
	"\0",		/* GSVIEW */
	0		/* is not DLL device */
};

/* Compress a gx_color_value into an 8-bit PM color value, */
/* using only the high order 5 bits. */
#define pm_color_value(z)\
  ((((z) >> (gx_color_value_bits - 5)) << 3) +\
   ((z) >> (gx_color_value_bits - 3)))

/* prototypes for internal procedures */
private void pm_makepalette(gx_device_pm *);
private void pm_update(gx_device_pm *);
private uint pm_set_bits_per_pixel(gx_device_pm *, int);
private uint pm_palette_size(gx_device_pm *);
private int pm_alloc_bitmap(gx_device_pm *, gx_device *);
private int pm_run_gspmdrv(gx_device_pm *);
private void pm_write_bmp(gx_device_pm *);

/* Open the pm driver */
int
pm_open(gx_device *dev)
{
	int ccode;
	CHAR id[128];
	CHAR name[128];
	PTIB pptib;
	PPIB pppib;

	if (!pmdev->dll && (_osmode == DOS_MODE)) {
		fprintf(stderr,"os2pm driver can't be used under DOS\n");
		return gs_error_limitcheck;
	}

	if (DosGetInfoBlocks(&pptib, &pppib)) {
		fprintf(stderr,"\npm_open: Couldn't get pid\n");
		return gs_error_limitcheck;
	}
#ifdef __DLL__
	if (pppib->pib_ultype == 3)	     /* if caller is PM app */
	    pmdev->gspid = pppib->pib_ulpid; /* use caller pid */
	else
#endif
	    pmdev->gspid = pppib->pib_ulppid; /* use parent (CMD.EXE) pid */
	sprintf(id, ID_NAME, pmdev->gspid, (ULONG)dev);

	/* Allocate, but don't commit, enough memory for the largest */
	/* possible bitmap (13Mbytes = A3 x 150dpi x 24bits) */
#ifdef __DLL__
	if (pmdev->dll) {
	    /* We don't need to use shared memory for the DLL */
	    if (DosAllocMem((PPVOID)&pmdev->bitmap, 
		13*1024*1024, PAG_READ | PAG_WRITE)) {
		fprintf(stderr,"pm_open: failed allocating BMP memory\n");
		return gs_error_limitcheck;
	    }
	}
	else 
#endif
	{
	    /* Shared memory is common to all processes so we don't want to allocate too much */
	    sprintf(name, SHARED_NAME, *pmdev->GSVIEW ? pmdev->GSVIEW : id);
	    if (DosAllocSharedMem((PPVOID)&pmdev->bitmap, name,
		13*1024*1024, PAG_READ | PAG_WRITE)) {
		fprintf(stderr,"pm_open: failed allocating shared BMP memory %s\n", name);
		return gs_error_limitcheck;
	    }
	}
	
	/* commit one page so there is enough storage for a */
	/* bitmap header and palette */
	if (DosSetMem(pmdev->bitmap, MIN_COMMIT, PAG_COMMIT | PAG_DEFAULT)) {
		DosFreeMem(pmdev->bitmap);
		fprintf(stderr,"pm_open: failed committing BMP memory\n");
		return gs_error_limitcheck;
	}
	pmdev->committed = MIN_COMMIT;

	if (pmdev->dll) {
	    /* Create mutex - used for preventing another thread from accessing */
	    /* bitmap while we are changing the bitmap size. Initially unowned. */
	    sprintf(name, MUTEX_NAME, id);
	    if (DosCreateMutexSem(name, &(pmdev->bmp_mutex), 0, FALSE)) {
		DosFreeMem(pmdev->bitmap);
		DosCloseEventSem(pmdev->sync_event);
		DosCloseQueue(pmdev->drv_queue);
		fprintf(stderr,"pm_open: failed to create mutex semaphore %s\n", name);
		return gs_error_limitcheck;
	    }
	}
	else {
	  if (*pmdev->GSVIEW) {
	    APIRET rc;
	    /* GSview has already created the necessary objects */
	    /* so we use Open instead of Create */
	    rc = 0;
	    if (!rc) {
	    	sprintf(name, NEXT_NAME, pmdev->GSVIEW);
		rc = DosOpenEventSem(name, &pmdev->next_event);
	    }
	    if (!rc) {
		sprintf(name, MUTEX_NAME, pmdev->GSVIEW);
		rc = DosOpenMutexSem(name, &pmdev->bmp_mutex);
	    }
	    if (!rc) {
		PID owner_pid;
		sprintf(name, QUEUE_NAME, pmdev->GSVIEW);
		rc = DosOpenQueue(&owner_pid, &pmdev->drv_queue, name);
	    }
	    if (rc) {
		DosFreeMem(pmdev->bitmap);
		DosCloseEventSem(pmdev->next_event);
		fprintf(stderr, "pm_open: failed to open %s, rc = %u\n", name, rc);
		return gs_error_limitcheck;
	    }
	  }
	  else {	/* not GSVIEW */
	    /* Create update event semaphore */
	    sprintf(name, SYNC_NAME, id);
	    if (DosCreateEventSem(name, &(pmdev->sync_event), 0, FALSE)) {
		DosFreeMem(pmdev->bitmap);
		fprintf(stderr,"pm_open: failed to create event semaphore %s\n", name);
		return gs_error_limitcheck;
	    }
	    /* Create mutex - used for preventing gspmdrv from accessing */
	    /* bitmap while we are changing the bitmap size. Initially unowned. */
	    sprintf(name, MUTEX_NAME, id);
	    if (DosCreateMutexSem(name, &(pmdev->bmp_mutex), 0, FALSE)) {
		DosFreeMem(pmdev->bitmap);
		DosCloseEventSem(pmdev->sync_event);
		DosCloseQueue(pmdev->drv_queue);
		fprintf(stderr,"pm_open: failed to create mutex semaphore %s\n", name);
		return gs_error_limitcheck;
	    }
	  }
	}

	if ( (pm_set_bits_per_pixel(pmdev, pmdev->BitsPerPixel) < 0) ||
	     (gdev_mem_device_for_bits(dev->color_info.depth) == 0) )
	{
	    if (!pmdev->dll) {
		if (*pmdev->GSVIEW) {
		    DosCloseQueue(pmdev->drv_queue);
		    DosCloseEventSem(pmdev->next_event);
		}
		else
		    DosCloseEventSem(pmdev->sync_event);
	    }
	    DosCloseMutexSem(pmdev->bmp_mutex);
	    DosFreeMem(pmdev->bitmap);
	    return gs_error_limitcheck;
	}

	/* initialise bitmap header */
	pmdev->bmi = (PBITMAPINFO2)pmdev->bitmap;
	pmdev->bmi->cbFix = 40;		/* OS/2 2.0 and Windows 3.0 compatible */
	pmdev->bmi->cx = dev->width;
	pmdev->bmi->cy = dev->height;
	pmdev->bmi->cPlanes = 1;
	pmdev->bmi->cBitCount = dev->color_info.depth;
	pmdev->bmi->ulCompression = BCA_UNCOMP;
	pmdev->bmi->cbImage = 0;
	pmdev->bmi->cxResolution = (ULONG)(dev->x_pixels_per_inch / 25.4 * 1000);
	pmdev->bmi->cyResolution = (ULONG)(dev->y_pixels_per_inch / 25.4 * 1000);
	if (pmdev->BitsPerPixel <= 8) {
	    pmdev->bmi->cclrUsed = 1<<(pmdev->BitsPerPixel);
	    pmdev->bmi->cclrImportant = pmdev->nColors;
	}
	else {
	    pmdev->bmi->cclrUsed = 0;
	    pmdev->bmi->cclrImportant = 0;
	}

	pm_makepalette(pmdev);

	/* commit pages */
	ccode = pm_alloc_bitmap((gx_device_pm *)dev, dev);
	if (ccode < 0) {
	    if (!pmdev->dll) {
		if (*pmdev->GSVIEW) {
		    DosCloseQueue(pmdev->drv_queue);
		    DosCloseEventSem(pmdev->next_event);
		}
		else
		    DosCloseEventSem(pmdev->sync_event);
	    }
	    DosCloseMutexSem(pmdev->bmp_mutex);
	    DosFreeMem(pmdev->bitmap);
	    return ccode;
	}

	if (*pmdev->GSVIEW)
		return 0;	/* GSview will handle displaying */

#ifdef __DLL__
	if (pmdev->dll) {
		/* notify caller about new device */
		(*pgsdll_callback)(GSDLL_DEVICE, (unsigned char *)pmdev, 1);
		return 0;	/* caller will handle displaying */
	}
#endif

	ccode = pm_run_gspmdrv(pmdev);
	if (ccode < 0) {
		DosFreeMem(pmdev->bitmap);
		DosCloseEventSem(pmdev->sync_event);
		DosCloseMutexSem(pmdev->bmp_mutex);
	}
		
	return ccode;
}

/* Get the initial matrix.  BMPs, unlike most displays, */
/* put (0,0) in the lower left corner. */
private void
pm_get_initial_matrix(gx_device *dev, gs_matrix *pmat)
{	pmat->xx = dev->x_pixels_per_inch / 72.0;
	pmat->xy = 0;
	pmat->yx = 0;
	pmat->yy = dev->y_pixels_per_inch / 72.0;
	pmat->tx = 0;
	pmat->ty = 0;
	if (*pmdev->GSVIEW)
	   pm_update((gx_device_pm *)dev);  /* let GSVIEW know we are drawing */
}

/* Make the output appear on the screen. */
int
pm_sync_output(gx_device *dev)
{
#ifdef __DLL__
    if (pmdev->dll) {
	(*pgsdll_callback)(GSDLL_SYNC, (unsigned char *)dev, 0);
	return 0;
    }
#endif

    /* tell gspmdrv or GSview process to update display */
    if (*pmdev->GSVIEW) {
	APIRET rc;
	rc = DosWriteQueue(pmdev->drv_queue, GS_SYNC, 0, NULL, 0);
	if (rc)
	    fprintf(stderr,"pm_sync_output: DosWriteQueue error %d\n",rc);
    }
    else {
    	if (pmdev->updating)
    	    DosStopTimer(pmdev->update_timer);
	DosPostEventSem(pmdev->sync_event);
    }
    pmdev->updating = FALSE;
    return(0);
}

/* Make the output appear on the screen  */
/* and bring image window to foreground. */
int
pm_output_page(gx_device *dev, int copies, int flush)
{
int code;
APIRET rc;
#ifdef DEBUG
	pm_write_bmp(pmdev);
#endif
#ifdef __DLL__
        if (pmdev->dll) {
	    (*pgsdll_callback)(GSDLL_PAGE, (unsigned char *)dev, 0);
	    return 0;
        }
#endif

	if (*pmdev->GSVIEW) {
	    if (copies == -2) {
    	        rc = DosWriteQueue(pmdev->drv_queue, GS_END, 0, NULL, 0);
    	        if (rc)
		    fprintf(stderr,"pm_output_page: DosWriteQueue error %d\n",rc);
	    }
	    else if (copies == -1) {
    	        rc = DosWriteQueue(pmdev->drv_queue, GS_BEGIN, 0, NULL, 0);
    	        if (rc)
		    fprintf(stderr,"pm_output_page: DosWriteQueue error %d\n",rc);
	    }
	    else {
	        ULONG count;
	        pmdev->updating = FALSE;
	        /* signal GSview that another page is ready */
    	        rc = DosWriteQueue(pmdev->drv_queue, GS_PAGE, 0, NULL, 0);
    	        if (rc)
		    fprintf(stderr,"pm_output_page: DosWriteQueue error %d\n",rc);
	        /* wait for GSview to signal we can move on to next page */
	        DosWaitEventSem(pmdev->next_event, SEM_INDEFINITE_WAIT);
	        DosResetEventSem(pmdev->next_event, &count);
	    }
	    code = 0;
	}
	else {
	    code = pm_sync_output(dev);
	    rc = DosSelectSession(pmdev->session_id);
	    if (rc) {
	      DosSleep(2000);	/* give gspmdrv.exe a chance to run */
	      rc = DosSelectSession(pmdev->session_id);
	      if (rc == ERROR_SMG_NO_TARGET_WINDOW) {
	          DosSleep(5000);	/* give gspmdrv.exe a chance to run */
	          rc = DosSelectSession(pmdev->session_id);  /* try yet again */
	      }
	      if ( (rc == ERROR_SMG_SESSION_NOT_FOUND) ||
		 (rc == ERROR_SMG_INVALID_SESSION_ID) ){
		/* someone has killed the session */
		REQUESTDATA Request;
		ULONG DataLength;
		PVOID DataAddress;
		PULONG QueueEntry;
		BYTE ElemPriority;
		/* Close gspmdrv driver */
		DosStopSession(STOP_SESSION_SPECIFIED, pmdev->session_id);
		Request.pid = pmdev->gspid;
		Request.ulData = 0;
		/* wait for termination queue, queue is then closed by session manager */
		DosReadQueue(pmdev->term_queue, &Request, &DataLength, 
			&DataAddress, 0, DCWW_WAIT, &ElemPriority, (HEV)NULL);
		DosCloseQueue(pmdev->term_queue);
		pmdev->term_queue = (HQUEUE)0;
		/* restart it */
		pm_run_gspmdrv(pmdev);
		DosSleep(2000);	/* give gspmdrv.exe a chance to run */
		rc = DosSelectSession(pmdev->session_id);
	      }
	      if (rc == ERROR_SMG_SESSION_NOT_FOREGRND)
		DosBeep(400,50);
	      else if (rc)
		fprintf(stderr,"pm_output_page: Select Session error code %u\n", rc);
	    }
	}
	return code;
}

/* Close the pm driver */
int
pm_close(gx_device *dev)
{
APIRET rc;
#ifdef __DLL__
    if (pmdev->dll) {
	DosRequestMutexSem(pmdev->bmp_mutex, 60000);
        (*pgsdll_callback)(GSDLL_DEVICE, (unsigned char *)dev, 0);
	DosReleaseMutexSem(pmdev->bmp_mutex);
    }
    else
#endif
    {
	if (*pmdev->GSVIEW) {
	    if (gs_exit_status) {
		ULONG count;
		/* pause so error messages can be read */
		DosResetEventSem(pmdev->next_event, &count);
		DosWriteQueue(pmdev->drv_queue, GS_ERROR, 0, NULL, 0);
		DosWaitEventSem(pmdev->next_event, SEM_INDEFINITE_WAIT);
		DosResetEventSem(pmdev->next_event, &count);
	    }
    	    rc = DosWriteQueue(pmdev->drv_queue, GS_CLOSE, 0, NULL, 0);
    	    if (rc)
		fprintf(stderr,"pm_close: DosWriteQueue error %d\n",rc);
	}
	else {
	    REQUESTDATA Request;
	    ULONG DataLength;
	    PVOID DataAddress;
	    PULONG QueueEntry;
	    BYTE ElemPriority;
	    /* Close gspmdrv driver */
	    DosStopSession(STOP_SESSION_SPECIFIED, pmdev->session_id);
	    Request.pid = pmdev->gspid;
	    Request.ulData = 0;
	    /* wait for termination queue, queue is then closed by session manager */
	    DosReadQueue(pmdev->term_queue, &Request, &DataLength, 
		&DataAddress, 0, DCWW_WAIT, &ElemPriority, (HEV)NULL);
	    /* queue needs to be closed by us */
	    DosCloseQueue(pmdev->term_queue);
	}
    }
    /* release memory */
    DosFreeMem(pmdev->bitmap);
    pmdev->bitmap = (unsigned char *)NULL;
    pmdev->committed = 0;

    if (!pmdev->dll) {
	/* close objects */
	if (*pmdev->GSVIEW) {
	    DosCloseQueue(pmdev->drv_queue);
	    DosCloseEventSem(pmdev->next_event);
	}
	else {
	    DosCloseEventSem(pmdev->sync_event);
	    /* stop update timer */
	    if (pmdev->updating)
		DosStopTimer(pmdev->update_timer);
	    pmdev->updating = FALSE;
	}
    }

    DosCloseMutexSem(pmdev->bmp_mutex);
    return(0);
}


/* Map a r-g-b color to the colors available under PM */
gx_color_index
pm_map_rgb_color(gx_device *dev, gx_color_value r, gx_color_value g,
  gx_color_value b)
{
	switch(dev->color_info.depth) {
	  case 24:
		return	((b >> (gx_color_value_bits - 8)) << 16) +
			((g >> (gx_color_value_bits - 8)) << 8) +
			((r >> (gx_color_value_bits - 8)));
	  case 8: {
		int i;
		RGB2 *prgb;
		byte cr, cg, cb;

		/* map colors to 0->255 in 32 steps */
		cr = pm_color_value(r);
		cg = pm_color_value(g);
		cb = pm_color_value(b);

		prgb = (RGB2 *)((PBYTE)pmdev->bmi + pmdev->bmi->cbFix);
		/* search in palette */
		for ( i = 0; i < pmdev->nColors; i++, prgb++ )
		{	if ( !((cr ^ prgb->bRed) & 0xf8) &&
			     !((cg ^ prgb->bGreen) & 0xf8) &&
			     !((cb ^ prgb->bBlue) & 0xf8)
			   )
				return((gx_color_index)i);	/* found it */
		}

		/* next try adding it to palette */
		if (i < 230) { /* allow 26 for PM and other apps */
			prgb->bRed = cr;
			prgb->bGreen = cg;
			prgb->bBlue = cb;
			prgb->fcOptions = 0;
			pmdev->nColors = i+1;
			pmdev->bmi->cclrImportant = pmdev->nColors;
    			if (*pmdev->GSVIEW) {
			    APIRET rc;
			    rc = DosWriteQueue(pmdev->drv_queue, GS_PALCHANGE, 0, NULL, 0);
			    if (rc)
	    			fprintf(stderr,"pm_sync_output: DosWriteQueue error %d\n",rc);
    			}
			return((gx_color_index)i);	/* return new palette index */
		}

		return(gx_no_color_index);  /* not found - dither instead */
		}
	  case 4:
		if ((r == g) && (g == b) && (r >= gx_max_color_value / 3 * 2 - 1)
		   && (r < gx_max_color_value / 4 * 3))
			return ((gx_color_index)8);	/* light gray */
		return pc_4bit_map_rgb_color(dev, r, g, b);
	}
	return (gx_default_map_rgb_color(dev,r,g,b));
}

/* Map a color code to r-g-b. */
int
pm_map_color_rgb(gx_device *dev, gx_color_index color,
  gx_color_value prgb[3])
{	gx_color_value one;
	switch(dev->color_info.depth) {
	  case 24:
		one = (gx_color_value) (gx_max_color_value / 255);
		prgb[0] = ((color)     & 255) * one;
		prgb[1] = ((color>>8)  & 255) * one;
		prgb[2] = ((color>>16) & 255) * one;
		break;
	  case 8:
		if (!dev->is_open)
		    return -1;
		{
		RGB2 *argb = (RGB2 *)((PBYTE)pmdev->bmi + pmdev->bmi->cbFix);
		one = (gx_color_value) (gx_max_color_value / 255);
		prgb[0] = argb[(int)color].bRed * one;
		prgb[1] = argb[(int)color].bGreen * one;
		prgb[2] = argb[(int)color].bBlue * one;
		}
		break;
	  case 4:
		if (color == 8)	/* VGA light gray */
		    prgb[0] = prgb[1] = prgb[2] = (gx_max_color_value / 4 * 3);
		else
		    pc_4bit_map_color_rgb(dev, color, prgb);
		break;
	  default:
		prgb[0] = prgb[1] = prgb[2] = 
			(int)color ? gx_max_color_value : 0;
	}
	return 0;
}

#define pmmdev ((gx_device *)&pmdev->mdev)
#define pmmproc(proc) (*dev_proc(&pmdev->mdev, proc))

/* Fill a rectangle. */
private int
pm_fill_rectangle(gx_device *dev, int x, int y, int w, int h,
  gx_color_index color)
{
	pmmproc(fill_rectangle)(pmmdev, x, y, w, h, color);
	pm_update((gx_device_pm *)dev);
	return 0;
}

/* Copy a monochrome bitmap.  The colors are given explicitly. */
/* Color = gx_no_color_index means transparent (no effect on the image). */
private int
pm_copy_mono(gx_device *dev,
  const byte *base, int sourcex, int raster, gx_bitmap_id id,
  int x, int y, int w, int h,
  gx_color_index zero, gx_color_index one)
{
	pmmproc(copy_mono)(pmmdev, base, sourcex, raster, id,
					 x, y, w, h, zero, one);
	pm_update((gx_device_pm *)dev);
	return 0;
}

/* Copy a color pixel map.  This is just like a bitmap, except that */
/* each pixel takes 8 or 4 bits instead of 1 when device driver has color. */
private int
pm_copy_color(gx_device *dev,
  const byte *base, int sourcex, int raster, gx_bitmap_id id,
  int x, int y, int w, int h)
{
	pmmproc(copy_color)(pmmdev, base, sourcex, raster, id,
					  x, y, w, h);
	pm_update((gx_device_pm *)dev);
	return 0;
}

int
pm_get_bits(gx_device *dev, int y, byte *str, byte **actual_data)
{
	return pmmproc(get_bits)(pmmdev, y, str, actual_data);
}
/* Get PM parameters */
int
pm_get_params(gx_device *dev, gs_param_list *plist)
{	int code = gx_default_get_params(dev, plist);
	gs_param_string gvs;
	gvs.data = pmdev->GSVIEW, gvs.size = strlen(gvs.data),
	  gvs.persistent = false;
	code < 0 ||
	(code = param_write_int(plist, "UpdateInterval", &pmdev->UpdateInterval)) < 0 ||
	(code = param_write_string(plist, "GSVIEW", &gvs)) < 0;
	return code;
}

/* Put parameters. */
private int
pm_put_alpha_param(gs_param_list *plist, gs_param_name param_name, int *pa,
  bool alpha_ok)
{	int code = param_read_int(plist, param_name, pa);
	switch ( code )
	{
	case 0:
		switch ( *pa )
		  {
		  case 1:
			return 0;
		  case 2: case 4:
			if ( alpha_ok )
			  return 0;
		  default:
			code = gs_error_rangecheck;
		  }
	default:
		param_signal_error(plist, param_name, code);
	case 1:
		;
	}
	return code;
}


/* Set PM parameters -- size and resolution. */
/* We implement this ourselves so that we can do it without */
/* closing and opening the device. */
/* Also set BitsPerPixel and GSVIEW if device not open */
int
pm_put_params(gx_device *dev, gs_param_list *plist)
{	int ecode = 0, code;
	bool reopen = false;
	bool is_open = dev->is_open;
	int width = dev->width;
	int height = dev->height;
	int old_bpp = dev->color_info.depth;
	int bpp = old_bpp;
	int uii = pmdev->UpdateInterval;
	gs_param_string gsvs;
	int atext = pmdev->alpha_text, agraphics = pmdev->alpha_graphics;
	bool alpha_ok;

	/* Handle extra parameters */
	switch ( code = param_read_string(plist, "GSVIEW", &gsvs) )
	{
	case 0:
		if ( gsvs.size == strlen(pmdev->GSVIEW) &&
		     !memcmp(pmdev->GSVIEW, gsvs.data, gsvs.size)
		   )
		  {	gsvs.data = 0;
			break;
		  }
		if ( dev->is_open )
		  ecode = gs_error_rangecheck;
		else if ( gsvs.size >= pm_gsview_sizeof )
		  ecode = gs_error_limitcheck;
		else
		  break;
		goto gsve;
	default:
		ecode = code;
gsve:		param_signal_error(plist, "GSVIEW", ecode);
	case 1:
		gsvs.data = 0;
		break;
	}

	switch ( code = param_read_int(plist, "UpdateInterval", &uii) )
	{
	case 0:
		if ( uii < 0 )
		  ecode = gs_error_rangecheck;
		else
		  break;
		goto uie;
	default:
		ecode = code;
uie:		param_signal_error(plist, "UpdateInterval", ecode);
	case 1:
		break;
	}

	switch ( code = param_read_int(plist, "BitsPerPixel", &bpp) )
	{
	case 0:
		if ( dev->is_open && bpp != old_bpp )
		  ecode = gs_error_rangecheck;
		else
		  {	code = pm_set_bits_per_pixel(pmdev, bpp);
			if ( code < 0 )
			  ecode = code;
			else
			  break;
		  }
		goto bppe;
	default:
		ecode = code;
bppe:		param_signal_error(plist, "BitsPerPixel", ecode);
	case 1:
		break;
	}

	alpha_ok = pmdev->color_info.depth >= 8;
	if ( (code = pm_put_alpha_param(plist, "TextAlphaBits", &pmdev->alpha_text, alpha_ok)) < 0 )
	  ecode = code;
	if ( (code = pm_put_alpha_param(plist, "GraphicsAlphaBits", &pmdev->alpha_graphics, alpha_ok)) < 0 )
	  ecode = code;

	if ( ecode >= 0 )
	  {	/* Prevent gx_default_put_params from closing the device. */
		dev->is_open = false;
		ecode = gx_default_put_params(dev, plist);
		dev->is_open = is_open;
	  }
	if ( ecode < 0 )
	  {
		if ( bpp != old_bpp )
		  pm_set_bits_per_pixel(pmdev, old_bpp);
		  pmdev->alpha_text = atext;
		  pmdev->alpha_graphics = agraphics;
		return ecode;
	  }

	/* Hand off the change to the implementation. */
	/* obtain mutex - to prevent gspmdrv from using bitmap */
	/* while we change its size */
	if (DosRequestMutexSem(pmdev->bmp_mutex, 20000) == ERROR_TIMEOUT)
	    fprintf(stderr, "pm_put_params: mutex timeout\n");
	if ( is_open && (old_bpp != bpp ||
			 dev->width != width || dev->height != height)
	   )
	{	int ccode;
		ccode = pm_alloc_bitmap(pmdev, dev);
		if ( ccode < 0 )
		{	/* Bad news!  Some of the other device parameters */
			/* may have changed.  We don't handle this. */
			/* This is ****** WRONG ******. */
			dev->width = width;
			dev->height = height;
		  	pm_set_bits_per_pixel(pmdev, old_bpp);
			pmdev->alpha_text = atext;
			pmdev->alpha_graphics = agraphics;
			pm_alloc_bitmap(pmdev, dev);
			DosReleaseMutexSem(pmdev->bmp_mutex);
			return ccode;
		}
		reopen = true;
	}
	pmdev->UpdateInterval = uii;
	if ( gsvs.data != 0 )
	  {	memcpy(pmdev->GSVIEW, gsvs.data, gsvs.size);
		pmdev->GSVIEW[gsvs.size] = 0;
	  }

	if ( dev->is_open && reopen ) {
	    /* need to update bitmap info header also */
	    pmdev->bmi->cx = dev->width;
	    pmdev->bmi->cy = dev->height;
	    /* update bit count and palette */
	    pmdev->bmi->cBitCount = dev->color_info.depth;
	    pmdev->bmi->cclrUsed = 1<<(pmdev->BitsPerPixel);
	    pmdev->bmi->cclrImportant = pmdev->nColors;
	    pm_makepalette(pmdev);
	    /* erase bitmap - before window gets redrawn */
	    (*dev_proc(dev, fill_rectangle))(dev,
		0, 0, dev->width, dev->height,
		pm_map_rgb_color(dev, gx_max_color_value, 
		    gx_max_color_value, gx_max_color_value));
	    /* cause scroll bars to be redrawn */
	    /* need to signal gspmdrv that bitmap size has changed */
	    /* or perhaps gspmdrv can check if the bitmap size has */
	    /* before each use */

#ifdef __DLL__
	    if (pmdev->dll)
	        (*pgsdll_callback)(GSDLL_SIZE, (unsigned char *)dev,
		    (dev->width & 0xffff) + ( (dev->height & 0xffff)<<16) );
#endif
	}

	/* release bmp mutex */
	DosReleaseMutexSem(pmdev->bmp_mutex);
	return 0;
}


/* Get the number of alpha bits. */
int
pm_get_alpha_bits(gx_device *dev, graphics_object_type type)
{	return (type == go_text ? pmdev->alpha_text : pmdev->alpha_graphics);
}


#ifdef __DLL__
/* ------ DLL routines ------ */
/* store at pbitmap the address of the bitmap */
/* device is a pointer to Ghostscript device from GSDLL_DEVICE message */
unsigned long GSDLLAPI
gsdll_get_bitmap(unsigned char *device, unsigned char **pbitmap)
{
gx_device *dev = (gx_device *)device;
    *pbitmap =  (unsigned char *)(pmdev->bmi );
    return 0;
}

/* Lock the device (so it's size cannot be changed) if flag = TRUE */
/* or unlock the device if flag = FALSE */
/* device is a pointer to Ghostscript device from GSDLL_DEVICE message */
int GSDLLAPI
gsdll_lock_device(unsigned char *device, int flag)
{
gx_device *dev = (gx_device *)device;
APIRET rc;
    if (flag)
	rc = DosRequestMutexSem(pmdev->bmp_mutex, 60000);
    else
	rc = DosReleaseMutexSem(pmdev->bmp_mutex);
    return rc;
}

#endif /* __DLL__ */

/* ------ Internal routines ------ */

#undef pmdev


/* start gspmdrv.exe */
private int
pm_run_gspmdrv(gx_device_pm *pmdev)
{
	int ccode;
	PCHAR pdrvname = "gspmdrv.exe";
	CHAR error_message[256];
	CHAR term_queue_name[128];
	CHAR id[128];
	CHAR arg[1024];
	STARTDATA sdata;
	APIRET rc;
	PTIB pptib;
	PPIB pppib;
	CHAR progname[256];
	PCHAR tail;

	sprintf(id, ID_NAME, pmdev->gspid, (ULONG)pmdev);

	/* Create termination queue - used to find out when gspmdrv terminates */
	sprintf(term_queue_name, "\\QUEUES\\TERMQ_%s", id);
	if (DosCreateQueue(&(pmdev->term_queue), QUE_FIFO, term_queue_name)) {
		fprintf(stderr,"pm_run_gspmdrv: failed to create termination queue\n");
		return gs_error_limitcheck;
	}

	/* get full path to gsos2.exe and hence path to gspmdrv.exe */
	if ( (rc = DosGetInfoBlocks(&pptib, &pppib)) != 0 ) {
		fprintf(stderr,"pm_run_gspmdrv: Couldn't get module handle, rc = %d\n", rc);
		return gs_error_limitcheck;
	}
	if ( (rc = DosQueryModuleName(pppib->pib_hmte, sizeof(progname)-1, progname)) != 0 ) {
		fprintf(stderr,"pm_run_gspmdrv: Couldn't get module name, rc = %d\n", rc);
		return gs_error_limitcheck;
	}
	if ((tail = strrchr(progname,'\\')) != (PCHAR)NULL)
	{
		tail++;
		*tail = '\0';
	}
	else 
		tail = progname;
	strcat(progname, pdrvname);

	/* Open the PM driver session gspmdrv.exe */
	/* arguments are: */
	/*  (1) -d (display) option */
	/*  (2) id string */
	sprintf(arg, "-d %s", id);

	/* because gspmdrv.exe is a different EXE type to gs.exe, 
	 * we must use start session not DosExecPgm() */
	sdata.Length = sizeof(sdata);
	sdata.Related = SSF_RELATED_CHILD;	/* to be a child  */
	sdata.FgBg = SSF_FGBG_BACK;		/* start in background */
	sdata.TraceOpt = 0;
	sdata.PgmTitle = "Ghostscript PM driver session";
	sdata.PgmName = progname;
	sdata.PgmInputs = arg;
	sdata.TermQ = term_queue_name;
	sdata.Environment = pppib->pib_pchenv;	/* use Parent's environment */
	sdata.InheritOpt = 0;			/* Can't inherit from parent because different sesison type */
	sdata.SessionType = SSF_TYPE_DEFAULT;	/* default is PM */
	sdata.IconFile = NULL;
	sdata.PgmHandle = 0;
	sdata.PgmControl = 0;
	sdata.InitXPos = 0;
	sdata.InitYPos = 0;
	sdata.InitXSize = 0;
	sdata.InitYSize = 0;
	sdata.ObjectBuffer = error_message;
	sdata.ObjectBuffLen = sizeof(error_message);

	rc = DosStartSession(&sdata, &pmdev->session_id, &pmdev->process_id);
	if (rc == ERROR_FILE_NOT_FOUND) {
	    sdata.PgmName = pdrvname;
	    rc = DosStartSession(&sdata, &pmdev->session_id, &pmdev->process_id);
	}
	if (rc) {
	    fprintf(stderr,"pm_run_gspmdrv: failed to run %s, rc = %d\n", sdata.PgmName, rc);
	    fprintf(stderr,"pm_run_gspmdrv: error_message: %s\n", error_message);
	    return gs_error_limitcheck;
	}

	return 0;

}

/* Allocate the backing bitmap. */
private int
pm_alloc_bitmap(gx_device_pm *pmdev, gx_device *param_dev)
{
	gx_device_memory mdev;
	byte *base;
	ulong data_size;
	uint ptr_size;
	uint pal_size;
	uint raster;
	ULONG rc;
	ULONG needed;

	/* Finish initializing the bitmap. */

	gs_make_mem_device(&mdev, gdev_mem_device_for_bits(pmdev->color_info.depth), 0, 0, (gx_device *)pmdev);
	mdev.width = param_dev->width;
	mdev.height = param_dev->height;
	/* BMP files need width rounded up so that a scan line is */
	/* a multiple of 4 bytes. */
	/* This is currently done by gdev_mem_raster(). */
	/* It may be better to do it here explicitly in case */
	/* gdev_mem_raster changes. */
	raster = gdev_mem_raster(&mdev);
	data_size = (ulong)raster * mdev.height;
	ptr_size = sizeof(byte **) * mdev.height;
	pal_size = pm_palette_size(pmdev);
	needed = pmdev->bmi->cbFix + pal_size + data_size + ptr_size;
	/* round up to page boundary */
	needed = (needed + MIN_COMMIT - 1) & (~(MIN_COMMIT-1));
	if (needed > pmdev->committed) {
		/* commit more memory */
		if (rc = DosSetMem(pmdev->bitmap + pmdev->committed, 
			needed - pmdev->committed, 
			PAG_COMMIT | PAG_DEFAULT)) {
			fprintf(stderr,"No memory in pm_alloc_bitmap, rc = %d\n",rc);
			return gs_error_limitcheck;
		}
		pmdev->committed = needed;
	}
	/* Shared memory can't be decommitted */
#ifdef __DLL__
	if (needed < pmdev->committed) {
		/* decommit memory */
		if (rc = DosSetMem(pmdev->bitmap + needed, 
			pmdev->committed - needed, 
			PAG_DECOMMIT)) {
			fprintf(stderr,"Failed to decommit memory in pm_alloc_bitmap, rc = %d\n",rc);
			return gs_error_limitcheck;
		}
		pmdev->committed = needed;
	}
#endif

	/* Nothing can go wrong now.... */
	base = pmdev->bitmap + pmdev->bmi->cbFix + pm_palette_size(pmdev);
	pmdev->mdev = mdev;
	pmdev->mdev.base = (byte *)base;
	pmmproc(open_device)((gx_device *)&pmdev->mdev);
	pmdev->bmi->cbImage = data_size;
	return 0;
}

private void
pm_makepalette(gx_device_pm *pmdev)
{
	int i, val;
	RGB2 *argb = (RGB2 *)( (PBYTE)pmdev->bmi + pmdev->bmi->cbFix );
	if (pmdev->BitsPerPixel > 8)
		return;	/* these don't use a palette */

	for (i=0; i<pmdev->nColors; i++) {
	  switch (pmdev->nColors) {
		case 64:
		  /* colors are rrggbb */
		  argb[i].bRed   = ((i & 0x30)>>4)*85;
		  argb[i].bGreen = ((i & 0xC)>>2)*85;
		  argb[i].bBlue  = (i & 3)*85;
		  argb[i].fcOptions = 0;
		  /* zero unused entries */
		  argb[i+64].bRed = argb[i+64].bGreen = argb[i+64].bBlue  = 0;
		  argb[i+64].fcOptions = 0;
		  argb[i+128].bRed = argb[i+128].bGreen = argb[i+128].bBlue  = 0;
		  argb[i+128].fcOptions = 0;
		  argb[i+192].bRed = argb[i+192].bGreen = argb[i+192].bBlue  = 0;
		  argb[i+192].fcOptions = 0;
		  break;
		case 16:
		  /* colors are irgb */
		  val = (i & 8 ? 255 : 128);
		  argb[i].bRed   = i & 4 ? val : 0;
		  argb[i].bGreen = i & 2 ? val : 0;
		  argb[i].bBlue  = i & 1 ? val : 0;
		  if (i == 8) {	/* light gray */
		      argb[i].bRed   = 
		      argb[i].bGreen = 
		      argb[i].bBlue  = 192;
		      argb[i].fcOptions = 0;
		  }
		  break;
		case 2:
		  argb[i].bRed =
		    argb[i].bGreen =
		    argb[i].bBlue = (i ? 255 : 0);
		  argb[i].fcOptions = 0;
		  break;
	  }
	}
}


/* Cause display to be updated periodically */
private void 
pm_update(gx_device_pm *pmdev)
{
	if (pmdev->updating)
		return;
	if (!pmdev->UpdateInterval)
		return;
	if (*pmdev->GSVIEW) {
	    APIRET rc;
	    rc = DosWriteQueue(pmdev->drv_queue, GS_UPDATING, 0, NULL, 0);
            if (rc)
		fprintf(stderr,"pm_update: DosWriteQueue error %d\n",rc);
	}
	else {
	    DosStartTimer(pmdev->UpdateInterval, (HSEM)pmdev->sync_event, 
		&pmdev->update_timer);
	}
	pmdev->updating = TRUE;
}

private uint
pm_set_bits_per_pixel(gx_device_pm *pmdev, int bpp)
{
static const gx_device_color_info pm_24bit_color = dci_color(24,255,255);
static const gx_device_color_info pm_8bit_color = dci_color(8,31,4);
static const gx_device_color_info pm_4bit_color = dci_pc_4bit;
static const gx_device_color_info pm_2color = dci_black_and_white;
	switch (bpp) {
	    case 24:
		pmdev->color_info = pm_24bit_color;
		pmdev->nColors = (1<<24);
		break;
	    case 8:
		/* use 64 static colors and 166 dynamic colors from 8 planes */
		pmdev->color_info = pm_8bit_color;
		pmdev->nColors = 64;
		break;
	    case 4:
		pmdev->color_info = pm_4bit_color;
		pmdev->nColors = 16;
		break;
	    case 1:
		pmdev->color_info = pm_2color;
		pmdev->nColors = 2;
		break;
	    default:
		return (gs_error_rangecheck);
	}
	pmdev->BitsPerPixel = bpp;
	return 0;
}

/* return length of BMP palette in bytes */
private uint
pm_palette_size(gx_device_pm *pmdev)
{
	switch(pmdev->color_info.depth) {
	    case 24:
		return 0;
	    case 8:
		return 256*sizeof(RGB2);
	    case 4:
		return 16*sizeof(RGB2);
	}
	/* must be two color */
	return 2*sizeof(RGB2);
}

/* This is used for testing */
/* Write out a BMP file to "out.bmp" */
private void 
pm_write_bmp(gx_device_pm *pmdev)
{
	BITMAPFILEHEADER2 bmfh;
	uint bmfh_length = sizeof(BITMAPFILEHEADER2) - sizeof(BITMAPINFOHEADER2);
	uint length;	/* bitmap length */
	ULONG fh;	/* file handle */
	ULONG action;
	ULONG count;
	bmfh.usType = 0x4d42;	/* "BM" */
	length = pmdev->bmi->cbFix + pm_palette_size(pmdev)
	   + ( (gdev_mem_raster(&pmdev->mdev) * pmdev->mdev.height) );
	bmfh.cbSize = bmfh_length + length;
	bmfh.xHotspot = bmfh.yHotspot = 0;
	bmfh.offBits = bmfh_length + pmdev->bmi->cbFix + pm_palette_size(pmdev);
	if (DosOpen("out.bmp",	/* filename */
		&fh,		/* pointer to handle */
		&action,	/* pointer to result */
		0,		/* initial length */
		FILE_NORMAL,	/* normal file */
		OPEN_ACTION_CREATE_IF_NEW | OPEN_ACTION_REPLACE_IF_EXISTS,
		OPEN_ACCESS_WRITEONLY | OPEN_SHARE_DENYREADWRITE,
		0)) {
		fprintf(stderr,"error opening out.bmp\n");
		return;
	}
	if (DosWrite(fh, (PBYTE)&bmfh, bmfh_length, &count))
		fprintf(stderr,"error writing header for out.bmp\n");
	if (DosWrite(fh, pmdev->bitmap, length, &count))
		fprintf(stderr,"error writing out.bmp\n");
	if (DosClose(fh))
		fprintf(stderr,"error closing out.bmp\n");
}
