/* Copyright (C) 1994 Aladdin Enterprises.  All rights reserved.
  
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

/* gspmdrv.c */
/* Presentation Manager driver for Ghostscript */
/* Written by Russell Lang */

/* To display output from os2pm driver: */
/*   gspmdrv -d id_string */
/* To display BMP file (used for testing display code) */
/*   gspmdrv -b filename.bmp */

#define INCL_DOS
#define INCL_WIN
#define INCL_GPI
#include <os2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/emxload.h>
#include "gspmdrv.h"
#include "gdevpm.h"

#ifndef min
#define min(x,y)  ( (x) < (y) ? (x) : (y) )
#endif
#ifndef max
#define max(x,y)  ( (x) > (y) ? (x) : (y) )
#endif

HEV update_event_sem;
HMTX bmp_mutex_sem;

/* bitmap details */
typedef struct tagBM {
    BOOL	valid;
    BOOL	old_bmp;	/* bitmap type */
    PBITMAPINFO2 pbmi;		/* pointer to bitmap info */
    PBYTE	bits;		/* pointer to bitmap bits */
    int		width;
    int		height;
    int		planes;
    int		depth;
    int		palsize;
    int 	palimportant;
    int		old_width;
    int		old_height;
    int		old_planes;
    int		old_depth;
    int		old_palsize;
    int		old_palimportant;
} BMAP;

typedef struct tagDISPLAY {
	LONG	planes;
	LONG	bitcount;
	LONG	hasPalMan;	/* Palette Manager */
	BOOL	hpal_exists;
	HPAL	hpal;
} DISPLAY;

/* options that are saved in INI file */
typedef struct tagOPTIONS {
	POINTL	img_origin;
	POINTL	img_size;
	BOOL	img_max;
} OPTIONS;
#define CW_USEDEFAULT 32768


BMAP bitmap;
DISPLAY display;
OPTIONS option;
PBYTE bbuffer;		/* for BMP file display */
POINTL scroll_pos;	/* not used */	/* not used */
ULONG os_version;
char *section = "Ghostscript Image";

HAB hab;		/* Anchor Block */
HWND hwnd_frame;
HWND hwnd_bmp;
HWND hwnd_gs;		/* window handle for CMD.EXE that started gs */
TID update_tid;
#define WM_GSUPDATE WM_USER+1
#define SB_TOP 20
#define SB_BOTTOM 21


MRESULT EXPENTRY ClientWndProc(HWND, ULONG, MPARAM, MPARAM);
MRESULT EXPENTRY AboutDlgProc(HWND, ULONG, MPARAM, MPARAM);
APIRET init_window(void);
void fix_sysmenu(HWND);
APIRET restore_window_position(SWP *pswp);
BOOL scan_bitmap(BMAP *pbm);
void read_profile(void);
void write_profile(void);
APIRET init_display(int argc, char *argv[]); 
APIRET init_bitmap(int argc, char *argv[]); 
void copy_clipboard(void);
HBITMAP make_bitmap(BMAP *pbm, ULONG left, ULONG bottom, ULONG right, ULONG top, ULONG depth);

void
debugbeep(int type)
{
#ifdef DEBUG
int i;
/* current debug beeps are: */
/* 1. Null handle PS */
/* 2. make_bitmap() failed */
/* 3. GpiDrawBits() or WinDrawBitmap() failed */
/* 4. Null handle PS from WinBeginPaint() */
	for (i=0; i<type; i++) {
	    DosBeep(400+100*type,50);
	    DosSleep(50);
	}
#endif
}


/* display message */
int
message_box(char *str, int icon)
{
  	return WinMessageBox(HWND_DESKTOP, hwnd_frame ? hwnd_frame : HWND_DESKTOP, 
		str, "gspmdrv.exe", 0, icon | MB_MOVEABLE | MB_OK);
}

void error_message(char *str)
{
  	WinMessageBox(HWND_DESKTOP, HWND_DESKTOP, str, "gspmdrv.exe", 0, MB_MOVEABLE | MB_ICONHAND | MB_OK);
	WinPostMsg(hwnd_frame, WM_QUIT, MPFROMLONG(0), MPFROMLONG(0));
}

/* Update thread */
/* This thread waits for the update event semaphore from gs.exe */
/* then generates a WM_PAINT message for the bitmap */
/* This thread must NOT call C library functions */
VOID APIENTRY update_func(ULONG unused)
{
	APIRET rc;
	BOOL flag;
	ULONG count;
	unused = unused;	/* to shut up warning */
	while (!DosQueryEventSem(update_event_sem, &count)) {
	    /* loop while semaphore exists */
	    DosWaitEventSem(update_event_sem, SEM_INDEFINITE_WAIT);
	    DosResetEventSem(update_event_sem, &count);
	    WinPostMsg(hwnd_bmp, WM_GSUPDATE, MPFROMLONG(0), MPFROMLONG(0));
	}
}

VOID APIENTRY exit_func(ULONG code)
{
	write_profile();
	DosCloseEventSem(update_event_sem);
	DosCloseMutexSem(bmp_mutex_sem);
	DosFreeMem((PVOID)bitmap.pbmi);
	DosExitList(EXLST_EXIT, 0);
	code = code;	/* to shut up warning */
}

void
find_hwnd_gs(char *gsid)
{
ULONG ulCount;
ULONG ulLength;
ULONG pBase;
ULONG cbBuf;
PSWBLOCK pswblk;
PSWENTRY pswentry;
SWCNTRL *pswc;
int i;
ULONG pid;
char buf[256];
char *p, *s;
	PTIB pptib;
	PPIB pppib;

	/* extract gs pid from command line id */
	strcpy(buf, gsid);
	for (p=buf; *p && *p!='_';p++)
		;
	*p++='\0';
	s = p;
	for (p=buf; *p && *p!='_';p++)
		;
	*p='\0';
	pid = atoi(s);	/* pid is Process ID of CMD.EXE that started gsos2.exe */
	
	ulCount = WinQuerySwitchList(hab, NULL, 0);	/* get num of items */
	cbBuf = (ulCount * sizeof(SWENTRY)) + sizeof(HSWITCH);
	pswblk = (PSWBLOCK) malloc(cbBuf+32768);
	ulCount = WinQuerySwitchList(hab, pswblk, cbBuf);	/* get num of items */
	for (i=0; i<ulCount; i++) {
	    pswentry = &pswblk->aswentry[i];
	    pswc = &pswentry->swctl;
	    if (pid == pswc->idProcess)
		hwnd_gs = pswc->hwnd;	/* save window handle */
	}
}


int
main(int argc, char *argv[])
{
  HMQ hand_mq;		/* message queue */
  QMSG q_mess;		/* message queue */
  APIRET rc = 0;


  hab = WinInitialize(0);	/* Get the Anchor Block */

  hand_mq = WinCreateMsgQueue(hab, 0); /* start a queue */

  if (argc < 2) {
	rc = 1;
	error_message("Usage: gspmdrv -d id_string");
  }

  if (!rc) {
    if (strcmp(argv[1],"-d")==0) {
	rc = init_display(argc, argv);
    }
    else if (strcmp(argv[1],"-b")==0) {
	rc = init_bitmap(argc, argv);
    }
    else {
	rc = 1;
	error_message("Usage: gspmdrv -d id_string");
    }
  }

  if (!rc) {
  	rc = DosCreateThread(&update_tid, update_func, 0, 0, 8192);
  	if (rc)
    		error_message("Failed to create update thread");
  }

  if (!rc)
     rc = init_window();

  if (!rc)
      WinShowWindow(hwnd_frame, TRUE);

  if (!rc) {
      /* keep gspmdrv.exe in memory for number of minutes specified in */
      /* environment variable GS_LOAD */
      _emxload_env("GS_LOAD");
  }

  DosExitList(EXLST_ADD, exit_func);

  /* message loop */
  while (!rc && WinGetMsg(hab, &q_mess, 0L, 0, 0))
      WinDispatchMsg(hab, &q_mess);

  /* Shut down the application window and queue */
  DosKillThread(update_tid);
  WinDestroyWindow(hwnd_frame);
  WinDestroyMsgQueue(hand_mq);
  WinTerminate(hab);
  return rc;
}

APIRET
init_window()
{
  ULONG version[3];
  SWP swp;
  APIRET rc = 0;
  ULONG flFlags;	/* Window frame definition */
  unsigned char class[] = "gspmdrvClass";  /* class name */

  if (DosQuerySysInfo(QSV_VERSION_MAJOR, QSV_VERSION_REVISION, &version, sizeof(version)))
      os_version = 201000;  /* a guess */
  else {
      os_version = version[0]*10000 + version[1]*100 + version[2];
  }

  /* define the frame constants */
  flFlags = FCF_TITLEBAR |	/* have a title bar */
            FCF_SIZEBORDER |	/* have a sizeable window */
            FCF_MINMAX |	/* have a min and max button */
            FCF_SYSMENU | 	/* include a system menu */
            FCF_VERTSCROLL |	/* vertical scroll bar */
            FCF_HORZSCROLL |	/* horizontal scroll bar */
	    FCF_TASKLIST |	/* show it in window list */
	    FCF_ICON; 		/* Load icon from resources */

    /* save SHELL default size and location */
    rc = WinQueryTaskSizePos(hab, 0, &swp);
    if (rc)
	return rc;

    read_profile();
    if ((option.img_size.x==0) || (option.img_size.y==0))
	option.img_size.x = option.img_size.y = CW_USEDEFAULT;

    if (!rc) {
	HPS ps = WinGetPS(HWND_DESKTOP);
	HDC hdc = GpiQueryDevice(ps);
	DevQueryCaps(hdc, CAPS_COLOR_PLANES, 1, &display.planes);
	DevQueryCaps(hdc, CAPS_COLOR_BITCOUNT, 1, &display.bitcount);
	DevQueryCaps(hdc, CAPS_ADDITIONAL_GRAPHICS, 1, &display.hasPalMan);
	display.hasPalMan &= CAPS_PALETTE_MANAGER;
	WinReleasePS(ps);
    }

    if (!rc) {
    if (!WinRegisterClass(	/* register this window class */
  	hab,			/* anchor block */
  	(PSZ)class,		/* class name */
  	(PFNWP) ClientWndProc,	/* window function */
  	CS_SIZEREDRAW |		/* window style */
	CS_MOVENOTIFY,
  	0))			/* no storage */
	exit(1);

    hwnd_frame = WinCreateStdWindow(
  	HWND_DESKTOP,		/* window type */
	0,			/* frame style is not WS_VISIBLE */
  	&flFlags,		/* definitions */
  	(PSZ)class,		/* client class */
  	(PSZ)"Ghostscript Image",  /* title */
  	WS_VISIBLE,		/* client style */
  	0,			/* resource module */
  	ID_GSPMDRV,		/* resource identifier */
  	&hwnd_bmp);		/* pointer to client */

    fix_sysmenu(hwnd_frame);
    }
    rc = restore_window_position(&swp);

    return rc;
}


void
write_profile(void)
{
char profile[64];
	sprintf(profile, "%d %d", option.img_origin.x, option.img_origin.y);
	PrfWriteProfileString(HINI_USERPROFILE, section, "Origin", profile);
	sprintf(profile, "%d %d", option.img_size.x, option.img_size.y);
	PrfWriteProfileString(HINI_USERPROFILE, section, "Size", profile);
	sprintf(profile, "%d", option.img_max);
	PrfWriteProfileString(HINI_USERPROFILE, section, "Maximized", profile);
}

void
read_profile(void)
{
char profile[64];
	PrfQueryProfileString(HINI_USERPROFILE, section, "Origin", "", profile, sizeof(profile));
	if (sscanf(profile,"%d %d", &option.img_origin.x, &option.img_origin.y) != 2) {
		option.img_origin.x = CW_USEDEFAULT;
		option.img_origin.y = CW_USEDEFAULT;
	}
	PrfQueryProfileString(HINI_USERPROFILE, section, "Size", "", profile, sizeof(profile));
	if (sscanf(profile,"%d %d", &option.img_size.x, &option.img_size.y) != 2) {
		option.img_size.x = CW_USEDEFAULT;
		option.img_size.y = CW_USEDEFAULT;
	}
	PrfQueryProfileString(HINI_USERPROFILE, section, "Maximized", "", profile, sizeof(profile));
	if (sscanf(profile,"%d", &option.img_max) != 1)
		option.img_max = 0;
}

void
fix_sysmenu(HWND hwnd)
{
MENUITEM mi;
HWND hwndSysMenu;
    if (!WinSendMsg(WinWindowFromID(hwnd, FID_SYSMENU), MM_QUERYITEM, 
		MPFROM2SHORT(SC_SYSMENU, TRUE), MPFROMP(&mi))) {
	message_box("failed getting system menu handle",0); 
	return;
    }
    hwndSysMenu = mi.hwndSubMenu;
    mi.iPosition = MIT_END;
    mi.afStyle = MIS_SEPARATOR;
    mi.afAttribute = 0;
    mi.id = 0;
    mi.hwndSubMenu = 0;
    mi.hItem = 0;
    WinSendMsg(hwndSysMenu, MM_INSERTITEM, MPFROMP(&mi), NULL);
    mi.afStyle = MIS_TEXT;
    mi.id = IDM_ABOUT;
    WinSendMsg(hwndSysMenu, MM_INSERTITEM, MPFROMP(&mi), "About...");
    mi.id = IDM_COPY;
    WinSendMsg(hwndSysMenu, MM_INSERTITEM, MPFROMP(&mi), "Copy");
}

APIRET
restore_window_position(SWP *pswp)
{
    SWP swp;
    swp.fl = SWP_MOVE | SWP_SIZE | SWP_SHOW;

    if (option.img_max) {
        /* Get maximized frame window position and size. */
	if (!WinGetMaxPosition(hwnd_frame, &swp))
	    return 1;
        swp.fl |= SWP_MAXIMIZE ;
    }
    else if ((option.img_size.x   != CW_USEDEFAULT) &&
	     (option.img_size.y   != CW_USEDEFAULT) &&
	     (option.img_origin.y != CW_USEDEFAULT) &&
	     (option.img_origin.y != CW_USEDEFAULT)) {
        LONG cxClientMax ;
        LONG cyClientMax ;
        LONG cyTitleBar ;
        LONG cxSizeBorder ;
        LONG cySizeBorder ;

        /* get maximum client window size */
        cxClientMax = WinQuerySysValue (HWND_DESKTOP, SV_CXFULLSCREEN) ;
        cyClientMax = WinQuerySysValue (HWND_DESKTOP, SV_CYFULLSCREEN) ;
        cyTitleBar = WinQuerySysValue (HWND_DESKTOP, SV_CYTITLEBAR) ;
        cxSizeBorder = WinQuerySysValue (HWND_DESKTOP, SV_CXSIZEBORDER) ;
        cySizeBorder = WinQuerySysValue (HWND_DESKTOP, SV_CYSIZEBORDER) ;
        cyClientMax += cyTitleBar ;

         /* Make sure x origin is within display boundaries */
         swp.x = option.img_origin.x ;
         if (swp.x < -cxSizeBorder)
             swp.x = 0 ;

         /* Make sure window isn't too wide, or negative value */
         swp.cx = option.img_size.x ;
         if (swp.cx >= cxClientMax || swp.cx < 0) {
             swp.cx = cxClientMax ;
             swp.x = 0 ;
         }

         if ((swp.x + swp.cx) > (cxClientMax + cxSizeBorder))
             swp.x = cxClientMax + cxSizeBorder - swp.cx ;

         /* Make sure y origin is within display boundaries */
         swp.y = option.img_origin.y ;
         if (swp.y < -cySizeBorder)
             swp.y = 0 ;

         /* Make sure window isn't too high, or negative value */
         swp.cy = option.img_size.y ;
         if (swp.cy > cyClientMax || swp.cy < 0) {
             swp.cy = cyClientMax ;
             swp.y = 0 ;
         }

         if ((swp.y + swp.cy) > (cyClientMax + cySizeBorder))
             swp.y = cyClientMax + cySizeBorder - swp.cy ;
    }
    else { /* No saved position -- use supplied position */
	swp = *pswp;
	option.img_origin.x = swp.x;
	option.img_origin.y = swp.y;
	option.img_size.x = swp.cx;
	option.img_size.y = swp.cy;
	option.img_max = FALSE;
        swp.fl = SWP_MOVE | SWP_SIZE | SWP_SHOW;
    }

    if (hwnd_gs)
	swp.fl |= SWP_ZORDER;
    /* Position and size this frame window */
    if (!WinSetWindowPos(hwnd_frame, hwnd_gs,
	swp.x, swp.y, swp.cx, swp.cy, swp.fl))
	return 1;
    return 0;
}

APIRET init_display(int argc, char *argv[]) 
{
  char buf[256];
  char name[256];
  APIRET rc = 0;

  if (argc != 3) {
	rc = 1;
	error_message("Usage: gspmdrv -d id_string");
  }

  find_hwnd_gs(argv[2]);

  if (!rc) {
	sprintf(name, SHARED_NAME, argv[2]);
  	rc = DosGetNamedSharedMem((PVOID *)&bitmap.pbmi, name, PAG_READ | PAG_WRITE);
  	if (rc) {
		sprintf(buf, "Failed to open: bmp shared memory \"%s\" rc = %d", argv[0], rc);
		error_message(buf);
	}
  }
  if (!rc) {
	sprintf(name, SYNC_NAME, argv[2]);
	rc = DosOpenEventSem(name, &update_event_sem);
  	if (rc) {
		sprintf(buf, "Failed to open: update event semaphore \"%s\" rc = %d", argv[1], rc);
		error_message(buf);
	}
  }
  if (!rc) {
	sprintf(name, MUTEX_NAME, argv[2]);
	rc = DosOpenMutexSem(name, &bmp_mutex_sem);
  	if (rc) {
		sprintf(buf, "Failed to open: bmp mutex semaphore \"%s\" rc = %d", argv[1], rc);
		error_message(buf);
	}
  }

  if (!rc) {
  	scan_bitmap(&bitmap);
	bitmap.valid = TRUE;
  }

  return rc;
}


APIRET init_bitmap(int argc, char *argv[])
{
  char buf[256];
  APIRET rc = 0;
  HFILE hf;
  ULONG action, count, length;
  PBITMAPFILEHEADER2 pbmfh;

  if (argc != 3)
	return 1;	/* error - no filename */
  
  /* open bitmap */
  if ( (rc = DosOpen(argv[2], &hf, &action, 0, FILE_NORMAL, FILE_OPEN,
		OPEN_ACCESS_READONLY | OPEN_SHARE_DENYREADWRITE, 0))
	!= (APIRET)0) {
	sprintf(buf,"Error opening: %s",argv[2]);
	error_message(buf);
	return rc;
  }

  rc = DosSetFilePtr(hf, 0, FILE_END, &length);
  if (rc) {
    sprintf(buf, "failed seeking to EOF: error = %d", rc);
    error_message(buf);
    return rc;
  }

  rc = DosSetFilePtr(hf, 0, FILE_BEGIN, &count);
  if (rc) {
    sprintf(buf, "failed seeking to BOF: error = %d", rc);
    error_message(buf);
    return rc;
  };

  /* allocate memory for bitmap */
  if ( (rc = DosAllocMem((PPVOID)&bbuffer, length, PAG_READ | PAG_WRITE | PAG_COMMIT))
	!= (APIRET)0 ){
    sprintf(buf, "failed allocating memory");
    error_message(buf);
    return rc;
  }

  rc = DosRead(hf, bbuffer, length, &count);
  DosClose(hf);
  if (rc) {
    sprintf(buf, "failed reading bitmap, error = %u, count = %u", rc, count);
    error_message(buf);
    return rc;
  }

  /* extract some info about bitmap */
  pbmfh = (PBITMAPFILEHEADER2)bbuffer;
  bitmap.pbmi = (PBITMAPINFO2)(&pbmfh->bmp2);

  scan_bitmap(&bitmap);
  bitmap.valid = TRUE;

  sprintf(buf, "bitmap width = %d, height = %d", bitmap.width, bitmap.height);
  message_box(buf,0);
  return rc;
}

#define MAX_PAL_SIZE 256
void
make_palette(BMAP *pbm)
{
ULONG tbl[MAX_PAL_SIZE];
PRGB2 palptr = (PRGB2) ((PBYTE)(pbm->pbmi) + pbm->pbmi->cbFix);
RGB *old_palptr = (RGB *)palptr;
int palcount = pbm->palimportant;
int i;
BOOL old_bmp = (pbm->pbmi->cbFix == sizeof(BITMAPINFOHEADER));
    if (old_bmp) {
	for (i=0; i<palcount; i++) {
	    tbl[i] = (old_palptr->bRed<<16) + (old_palptr->bGreen<<8) + (old_palptr->bBlue);
	    palptr++;
	}
    }
    else {
	for (i=0; i<palcount; i++) {
	    tbl[i] = (palptr->bRed<<16) + (palptr->bGreen<<8) + (palptr->bBlue);
	    palptr++;
	}
    }
    if (display.hpal_exists)
	GpiDeletePalette(display.hpal);
    display.hpal = GpiCreatePalette(hab, 0L, LCOLF_CONSECRGB, palcount, tbl);
    display.hpal_exists = TRUE;
}



/* scan bitmap */
/* update bitmap structure */
/* return value is TRUE if bitmap dimension has changed */
BOOL
scan_bitmap(BMAP *pbm)
{
PBITMAPINFO2 pbmi = pbm->pbmi;
PBITMAPINFO old_pbmi = (PBITMAPINFO)pbmi;
BOOL old_bmp = (pbmi->cbFix == sizeof(BITMAPINFOHEADER));

   if (old_bmp) {
  	/* it is a BITMAPINFO */
  	switch(old_pbmi->cBitCount) {
  	  case 24:
  	    pbm->palsize = 0;
  	    break;
  	  case 8:
  	    pbm->palsize = 256;
  	    break;
  	  case 4:
  	    pbm->palsize = 16;
  	    break;
  	  case 1:
  	    pbm->palsize = 2;
  	    break;
  	  default:
	    pbm->valid = FALSE;
  	    error_message("scan_bitmap: wrong number of bits"); /* panic */
  	    return FALSE;
	}
	pbm->palimportant = pbm->palsize;
	pbm->palsize = pbm->palsize * sizeof(RGB);
        pbm->bits   = (PBYTE)old_pbmi + old_pbmi->cbFix + pbm->palsize;
	pbm->width  = old_pbmi->cx;
	pbm->height = old_pbmi->cy;
	pbm->planes = old_pbmi->cPlanes;
	pbm->depth  = old_pbmi->cBitCount;
    }
    else {
 	/* it is a BITMAPINFO2 */
  	switch(pbmi->cBitCount) {
  	  case 24:
  	    pbm->palsize = 0;
  	    break;
  	  case 8:
  	    pbm->palsize = 256;
  	    break;
  	  case 4:
  	    pbm->palsize = 16;
  	    break;
  	  case 1:
  	    pbm->palsize = 2;
  	    break;
  	  default:
	    pbm->valid = FALSE;
  	    error_message("scan_bitmap: wrong number of bits"); /* panic */
  	    return FALSE;
	}
	if ( (pbmi->cbFix > (&(pbmi->cclrUsed) - &(pbmi->cbFix)))
		&& (pbmi->cclrUsed != 0) && (pbmi->cBitCount != 24) )
	    pbm->palsize = pbmi->cclrUsed;
	pbm->palimportant = pbm->palsize;
	if ( (pbmi->cbFix > (&(pbmi->cclrImportant) - &(pbmi->cbFix)))
		&& (pbmi->cclrImportant != 0) && (pbmi->cBitCount != 24) )
	    pbm->palimportant = pbmi->cclrImportant;
	pbm->palsize = pbm->palsize * sizeof(RGB2);
        pbm->bits   = (PBYTE)pbmi + pbmi->cbFix + pbm->palsize;
	pbm->width  = pbmi->cx;
	pbm->height = pbmi->cy;
	pbm->planes = pbmi->cPlanes;
	pbm->depth  = pbmi->cBitCount;
    }

    if ((pbm->palsize != pbm->old_palsize) || (pbm->palimportant != pbm->old_palimportant)) {
	if ( (pbm->depth == 8) && display.hasPalMan )
	    make_palette(pbm);
	pbm->old_palimportant = pbm->palimportant;
    }

    if ( (pbm->width   == pbm->old_width) && 
    	 (pbm->height  == pbm->old_height) &&
	 (pbm->planes  == pbm->old_planes) && 
	 (pbm->depth   == pbm->old_depth) &&
	 (pbm->palsize == pbm->old_palsize) &&
	 (pbm->old_bmp == old_bmp) )
	return FALSE;

    /* bitmap has changed */
    pbm->old_width   = pbm->width;
    pbm->old_height  = pbm->height;
    pbm->old_planes  = pbm->planes;
    pbm->old_depth   = pbm->depth;
    pbm->old_palsize = pbm->palsize;
    pbm->old_bmp     = old_bmp;
    return TRUE;
}


void
update_scroll_bars(void)
{
	/* Cause update of scroll bars etc. */
	SWP swp;
	WinQueryWindowPos(hwnd_bmp, &swp);
	WinSendMsg(hwnd_bmp, WM_SIZE, MPFROM2SHORT(swp.cx, swp.cy), MPFROM2SHORT(swp.cx, swp.cy));
}


/* copy bitmap to the clipboard */
void
copy_clipboard(void)
{
HBITMAP hbmp;
    if (!bitmap.valid) {
	message_box("Cannot copy to clipboard:\nNo Bitmap displayed",0);
	return;
    }
    if (WinOpenClipbrd(hab)) {
	/* get bmp mutex to stop gs.exe changing bitmap while we copy it */
	DosRequestMutexSem(bmp_mutex_sem, 10000);
	if (scan_bitmap(&bitmap)) {
	    /* bitmap has changed */
	    update_scroll_bars();
	}
	hbmp = make_bitmap(&bitmap, 0, 0, bitmap.width, bitmap.height, bitmap.depth);
	if (hbmp) {
	    WinEmptyClipbrd(hab);
	    WinSetClipbrdData(hab, (ULONG)hbmp, CF_BITMAP, CFI_HANDLE);
	}
	
	DosReleaseMutexSem(bmp_mutex_sem);
	WinCloseClipbrd(hab);
    }
}


HBITMAP
make_bitmap(BMAP *pbm, ULONG left, ULONG bottom, ULONG right, ULONG top, ULONG depth)
{
HDC hdc = DEV_ERROR, hdcMem = DEV_ERROR;
HPS hps = GPI_ERROR;
HBITMAP hbmp = GPI_ERROR, hbmr = HBM_ERROR;
SIZEL sizePS;
BITMAPINFOHEADER2 bmih;

	if ( (left == right) || (bottom == top) )
		return (HBITMAP)NULL;

	if (right > pbm->width)
		right = pbm->width;
	if (left > pbm->width)
		left = 0;
	if (top > pbm->height)
		top = pbm->height;
	if (bottom > pbm->height)
		bottom = 0;
		
	memset(&bmih, 0, sizeof(bmih));
	bmih.cbFix = sizeof(BITMAPINFOHEADER2);
	bmih.cx = right - left;
	bmih.cy = top - bottom;
	bmih.cPlanes = 1;
	bmih.cBitCount = depth;

	/* create memory DC compatible with screen */
	hdcMem = DevOpenDC(hab, OD_MEMORY, "*", 0L, NULL, NULLHANDLE);

	sizePS.cx = right - left;
	sizePS.cy = top - bottom;
	if (hdcMem != DEV_ERROR)
	    hps = GpiCreatePS(hab, hdcMem, &sizePS, 
		PU_PELS | GPIF_DEFAULT | GPIT_MICRO | GPIA_ASSOC );

	if (hps != GPI_ERROR)
		hbmp = GpiCreateBitmap(hps, &bmih, 0L, NULL, NULL);

	if (hbmp != GPI_ERROR)
		hbmr = GpiSetBitmap(hps, hbmp);


	if (hbmr != HBM_ERROR) {
		LONG rc;
		ERRORID eid;
  	    	POINTL apts[4];
		/* target is inclusive */
		apts[0].x = 0;
		apts[0].y = 0;
		apts[1].x = right - left - 1;
		apts[1].y = top - bottom - 1;
		/* source is not inclusive of top & right borders */
		apts[2].x = left;
		apts[2].y = bottom;
		apts[3].x = right;
		apts[3].y = top;

		rc = 0;
		eid = WinGetLastError(hab);
	    	rc = GpiDrawBits(hps, pbm->bits, pbm->pbmi, 4, apts, 
			(bitmap.depth != 1) ? ROP_SRCCOPY : ROP_NOTSRCCOPY, 0);
		if (rc==0) {
			char buf[256];
			eid = WinGetLastError(hab);
			sprintf(buf,"make_bitmap: GpiDrawBits rc = %08x, eid = %08x",rc, eid);
			message_box(buf, 0);
		}
	}
	
	if (hbmr != HBM_ERROR)
		GpiSetBitmap(hps, (ULONG)0);
	if (hps != GPI_ERROR)
		GpiDestroyPS(hps);
	if (hdcMem != DEV_ERROR)
		DevCloseDC(hdcMem);

	if ( (hbmr == HBM_ERROR) || (hdcMem == DEV_ERROR) ||
		(hbmp == GPI_ERROR) || (hps == GPI_ERROR) ) {
		if (hbmp != GPI_ERROR)
			GpiDeleteBitmap(hbmp);
		debugbeep(2);
		return 0;
	}
	return hbmp;
}

MRESULT
paint_bitmap(HPS ps, PRECTL prect, int scrollx, int scrolly)
{
    POINTL apts[4];
    int wx, wy;
    if (WinIsRectEmpty(hab, prect))
	return 0;
    if (ps == NULLHANDLE) {
	debugbeep(1);
    }
    /* source is not inclusive of top & right borders */
    wx = prect->xRight - prect->xLeft; /* update width */
    wy = prect->yTop - prect->yBottom; /* update height */
    apts[2].x   = prect->xLeft + scrollx;
    apts[2].y = prect->yBottom + scrolly;
    if (apts[2].x > bitmap.width)
	    apts[2].x = bitmap.width;
    if (apts[2].x + wx > bitmap.width)
	    wx = bitmap.width - apts[2].x;
    apts[3].x = apts[2].x + wx;
    if (apts[2].y > bitmap.height)
	    apts[2].y = bitmap.height;
    if (apts[2].y + wy > bitmap.height)
	    wy = bitmap.height - apts[2].y;
    apts[3].y = apts[2].y + wy;
    /* target is inclusive */
    apts[0].x = prect->xLeft;
    apts[0].y = prect->yBottom;
    apts[1].x = prect->xLeft + wx - 1;
    apts[1].y = prect->yBottom + wy - 1;

    if ( (display.bitcount == 4)  /* standard VGA is buggy */
	|| ( (os_version==201100) && (display.bitcount==8) && (bitmap.depth==1)) /* S3 and ATI GU are buggy */
         ) {
	/* slow code to dodge OS/2 bugs */
	/* this code double buffers the bitmap and works on a standard VGA
	 * but didn't work on an ATI Ultra Graphics Pro in 8514 emulation
	 */
	/* This won't work for version 2.11, S3 or ATI GU, 8bit/pixel display, 8bit/pixel bitmap */
	HBITMAP hbmp;
	/* create a bitmap */
	hbmp = make_bitmap(&bitmap, apts[2].x, apts[2].y, apts[3].x, apts[3].y, bitmap.depth);
	/* Draw it to the display */
	if (hbmp) {
	    WinDrawBitmap(ps, hbmp, NULL, &apts[0], CLR_BLACK, CLR_WHITE, DBM_NORMAL);
	    GpiDeleteBitmap(hbmp);
	}
    }
    else {
	/* fast code which doesn't always work */
	/* This code works on the Trident SVGA and 8514 in 256 color mode,
 	 * but GpiDrawBits fails with a SYS3175 on the standard VGA.
	 */
	/* This won't work for version 2.11, S3 or ATI GU, 8bit/pixel display, 1bit/pixel bitmap */
	GpiDrawBits(ps, bitmap.bits, bitmap.pbmi, 4, apts, 
		(bitmap.depth != 1) ? ROP_SRCCOPY : ROP_NOTSRCCOPY, 0);
    }

    return 0;
}


/* This is the window function */
MRESULT EXPENTRY ClientWndProc(HWND hwnd, ULONG mess, 
			MPARAM mp1, MPARAM mp2)
{
  char buf[256];
  static int cxClient, cyClient;
  static int cxAdjust, cyAdjust;
  static int nHscrollMax, nHscrollPos;
  static int nVscrollMax, nVscrollPos;
  int nHscrollInc;
  int nVscrollInc;
  HWND hwndScroll;
  HPS hps;
  RECTL rect;
  ULONG ulclr;

    switch(mess) {
	case WM_CREATE:
		break;
	case WM_ERASEBACKGROUND:
		/* by returning TRUE, the Presentation Manager automatically clears
		 * the window each time the window is resized or moved.
		 */
		return (MRESULT)TRUE;
	case WM_GSUPDATE:
		 if (!WinInvalidateRect(hwnd_bmp, (PRECTL)NULL, TRUE))
			error_message("error invalidating rect");
  		 if (!WinUpdateWindow(hwnd_bmp))
			error_message("error updating window");
		 return 0;
	case WM_COMMAND:
	    switch(LONGFROMMP(mp1)) {
		case IDM_ABOUT:
		    WinDlgBox(HWND_DESKTOP, hwnd, AboutDlgProc, 0, IDD_ABOUT, 0);
		    break;
		case IDM_COPY:
		    copy_clipboard();
		    break;
	    }
	    break;
	case WM_REALIZEPALETTE:
	    if ((bitmap.depth == 8) && display.hasPalMan && display.hpal_exists) {
		hps = WinGetPS(hwnd);
		if (hps == NULLHANDLE)
		    debugbeep(1);
		GpiSelectPalette(hps, display.hpal);
	        if (WinRealizePalette(hwnd, hps, &ulclr) > 0)
		    WinInvalidateRect(hwnd, NULL, FALSE);
		GpiSelectPalette(hps, (HPAL)NULL);
		WinReleasePS(hps);
	        return 0;
	    }
	    break;	/* use default processing */
	case WM_PAINT:
	    /* Refresh the window each time the WM_PAINT message is received */

	    /* get bmp mutex to stop gs.exe changing bitmap while we paint */
	    DosRequestMutexSem(bmp_mutex_sem, 10000);
	    if (scan_bitmap(&bitmap))
		update_scroll_bars(); /* bitmap has changed */

	    if (!bitmap.valid) {
	        DosReleaseMutexSem(bmp_mutex_sem);
	        hps = WinBeginPaint(hwnd, (ULONG)0, &rect);
	        if (hps == NULLHANDLE)
		    debugbeep(4);
		WinFillRect(hps, &rect, CLR_BACKGROUND);
		WinEndPaint(hwnd);
		return 0;
	    }

	    hps = WinBeginPaint(hwnd, (HPS)NULL, &rect);
	    if (hps == NULLHANDLE)
		debugbeep(4);
	    if ((bitmap.depth == 8) && display.hasPalMan && display.hpal_exists) {
	        GpiSelectPalette(hps, display.hpal);
                WinRealizePalette(hwnd, hps, &ulclr);
	        paint_bitmap(hps, &rect, nHscrollPos, nVscrollMax - nVscrollPos);
	        GpiSelectPalette(hps, (HPAL)NULL);
	    }
	    else
	        paint_bitmap(hps, &rect, nHscrollPos, nVscrollMax - nVscrollPos);
	    WinEndPaint(hwnd);

	    DosReleaseMutexSem(bmp_mutex_sem);
	    return 0;
	case WM_MOVE:
	    /* don't interrogate the window location immediately since */
	    /* it causes the Diamond Stealth VL24 with IBM S3 drivers */
	    /* to corrupt the display */
	    DosSleep(50);
	    if (hwnd_frame) {  /* save window position for INI file */
		SWP swp;
		WinQueryWindowPos(WinQueryWindow(hwnd, QW_PARENT), &swp);
		if (!(swp.fl & SWP_MINIMIZE)) {
		    option.img_origin.x = swp.x;
		    option.img_origin.y = swp.y;
		    option.img_max = ((swp.fl & SWP_MAXIMIZE) != 0);
		}
	    }
	    return 0;
	case WM_SIZE:
		cyClient = SHORT2FROMMP(mp2);
		cxClient = SHORT1FROMMP(mp2);

		cyAdjust = min(bitmap.height, cyClient) - cyClient;
		cyClient += cyAdjust;

		nVscrollMax = max(0, bitmap.height - cyClient);
		nVscrollPos = min(nVscrollPos, nVscrollMax);
		scroll_pos.y = nVscrollMax - nVscrollPos;

		if (!bitmap.valid)
			cyClient = cyAdjust = nVscrollMax = nVscrollPos;

		hwndScroll = WinWindowFromID(WinQueryWindow(hwnd, QW_PARENT), FID_VERTSCROLL);
		WinSendMsg(hwndScroll, SBM_SETSCROLLBAR, MPFROMLONG(nVscrollPos), 
			MPFROM2SHORT(0, nVscrollMax));
		if (bitmap.valid)
		    WinSendMsg(hwndScroll, SBM_SETTHUMBSIZE, MPFROM2SHORT(cyClient, bitmap.height),
			MPFROMLONG(0));
		else
		    WinSendMsg(hwndScroll, SBM_SETTHUMBSIZE, MPFROM2SHORT(1, 1),
			MPFROMLONG(0));

		cxAdjust = min(bitmap.width,  cxClient) - cxClient;
		cxClient += cxAdjust;

		nHscrollMax = max(0, bitmap.width - cxClient);
		nHscrollPos = min(nHscrollPos, nHscrollMax);
		scroll_pos.x = nHscrollPos;

		if (!bitmap.valid)
			cxClient = cxAdjust = nHscrollMax = nHscrollPos;

		hwndScroll = WinWindowFromID(WinQueryWindow(hwnd, QW_PARENT), FID_HORZSCROLL);
		WinSendMsg(hwndScroll, SBM_SETSCROLLBAR, MPFROMLONG(nHscrollPos), 
			MPFROM2SHORT(0, nHscrollMax));
		if (bitmap.valid)
		    WinSendMsg(hwndScroll, SBM_SETTHUMBSIZE, MPFROM2SHORT(cxClient, bitmap.width),
			MPFROMLONG(0));
		else
		    WinSendMsg(hwndScroll, SBM_SETTHUMBSIZE, MPFROM2SHORT(1, 1),
			MPFROMLONG(0));

		if ((cxAdjust!=0 || cyAdjust!=0)) {
		    SWP swp;
		    WinQueryWindowPos(WinQueryWindow(hwnd, QW_PARENT), &swp);
		    WinSetWindowPos(WinQueryWindow(hwnd, QW_PARENT), 0, 
			swp.x, swp.y - cyAdjust,
			swp.cx + cxAdjust, swp.cy + cyAdjust, SWP_SIZE | SWP_MOVE);
		    cxAdjust = cyAdjust = 0;
		}
	    	if (hwnd_frame) { /* save window size for INI file */
		    SWP swp;
		    WinQueryWindowPos(WinQueryWindow(hwnd, QW_PARENT), &swp);
		    if (!(swp.fl & SWP_MINIMIZE)) {
		        option.img_size.x = swp.cx;
		        option.img_size.y = swp.cy;
		        option.img_max = ((swp.fl & SWP_MAXIMIZE) != 0);
		    }
		}
		break;
	case WM_VSCROLL:
	    switch(SHORT2FROMMP(mp2)) {
		case SB_LINEUP:
			nVscrollInc = -cyClient/16;
			break;
		case SB_LINEDOWN:
			nVscrollInc = cyClient/16;
			break;
		case SB_PAGEUP:
			nVscrollInc = min(-1,-cyClient);
			break;
		case SB_PAGEDOWN:
			nVscrollInc = max(1,cyClient);
			break;
		case SB_SLIDERPOSITION:
			nVscrollInc = SHORT1FROMMP(mp2) - nVscrollPos;
			break;
		case SB_TOP:
			nVscrollInc = -nVscrollPos;
			break;
		case SB_BOTTOM:
			nVscrollInc = nVscrollMax - nVscrollPos;
			break;
		default:
			nVscrollInc = 0;
	    }
	    if ((nVscrollInc = max(-nVscrollPos, 
		min(nVscrollInc, nVscrollMax - nVscrollPos)))!=0) {
		LONG lComplexity;
		hwndScroll = WinWindowFromID(WinQueryWindow(hwnd, QW_PARENT), FID_VERTSCROLL);
		nVscrollPos += nVscrollInc;
		scroll_pos.y = nVscrollMax - nVscrollPos;
		lComplexity = WinScrollWindow(hwnd, 0, nVscrollInc, (PRECTL)NULL, (PRECTL)NULL, 
			(HRGN)NULLHANDLE, (PRECTL)&rect, 0);
		WinSendMsg(hwndScroll, SBM_SETPOS, MPFROMLONG(nVscrollPos), 0);
		if (lComplexity != RGN_RECT) {
		    WinInvalidateRect(hwnd, (PRECTL)NULL, FALSE);
		    WinUpdateWindow(hwnd);
		}
		else {
		    /* redraw exposed area */
		    hps = WinGetPS(hwnd);
		    if (hps == NULLHANDLE)
			debugbeep(1);
	            if ((bitmap.depth == 8) && display.hasPalMan && display.hpal_exists) {
	                GpiSelectPalette(hps, display.hpal);
                        WinRealizePalette(hwnd, hps, &ulclr);
		        paint_bitmap(hps, &rect, nHscrollPos, nVscrollMax - nVscrollPos);
	                GpiSelectPalette(hps, (HPAL)NULL);
	            }
		    else
		        paint_bitmap(hps, &rect, nHscrollPos, nVscrollMax - nVscrollPos);
		    WinReleasePS(hps);
		}
	    }
	    break;
	case WM_HSCROLL:
	    switch(SHORT2FROMMP(mp2)) {
		case SB_LINELEFT:
			nHscrollInc = -cxClient/16;
			break;
		case SB_LINERIGHT:
			nHscrollInc = cyClient/16;
			break;
		case SB_PAGELEFT:
			nHscrollInc = min(-1,-cxClient);
			break;
		case SB_PAGERIGHT:
			nHscrollInc = max(1,cxClient);
			break;
		case SB_SLIDERPOSITION:
			nHscrollInc = SHORT1FROMMP(mp2) - nHscrollPos;
			break;
		default:
			nHscrollInc = 0;
	    }
	    if ((nHscrollInc = max(-nHscrollPos, 
		min(nHscrollInc, nHscrollMax - nHscrollPos)))!=0) {
		LONG lComplexity;
		hwndScroll = WinWindowFromID(WinQueryWindow(hwnd, QW_PARENT), FID_HORZSCROLL);
		nHscrollPos += nHscrollInc;
		scroll_pos.x = nHscrollPos;
		lComplexity = WinScrollWindow(hwnd, -nHscrollInc, 0, (PRECTL)NULL, (PRECTL)NULL, 
			(HRGN)NULLHANDLE, (PRECTL)&rect, 0);
		/* need to send next message BEFORE redrawing, otherwise S3 driver screws up */
		WinSendMsg(hwndScroll, SBM_SETPOS, MPFROMLONG(nHscrollPos), 0);
		if (lComplexity != RGN_RECT) {
		    WinInvalidateRect(hwnd, (PRECTL)NULL, FALSE);
		    WinUpdateWindow(hwnd);
		}
		else {
		    /* redraw exposed area */
		    hps = WinGetPS(hwnd);
		    if (hps == NULLHANDLE)
			debugbeep(1);
	            if ((bitmap.depth == 8) && display.hasPalMan && display.hpal_exists) {
	                GpiSelectPalette(hps, display.hpal);
                        WinRealizePalette(hwnd, hps, &ulclr);
		        paint_bitmap(hps, &rect, nHscrollPos, nVscrollMax - nVscrollPos);
	                GpiSelectPalette(hps, (HPAL)NULL);
	            }
		    else
		        paint_bitmap(hps, &rect, nHscrollPos, nVscrollMax - nVscrollPos);
		    WinReleasePS(hps);
		}
	    }
	    break;
	case WM_CHAR:	/* process keystrokes here */
	    if (SHORT1FROMMP(mp1) & KC_CHAR) {
		/* pass control to gs if ENTER pressed */
		if (hwnd_gs && (SHORT1FROMMP(mp2) == '\r'))
			WinSetActiveWindow(HWND_DESKTOP, hwnd_gs);
	    }
	    /* Process only key presses, not key releases */
	    if (SHORT1FROMMP(mp1) & KC_KEYUP)
		break;
	    if (SHORT1FROMMP(mp1) & KC_VIRTUALKEY) {
		USHORT vkey = SHORT2FROMMP(mp2);
		switch(vkey) {
		    case VK_HOME:
		    	WinSendMsg(hwnd, WM_VSCROLL, MPFROMLONG(0), MPFROM2SHORT(0, SB_TOP));
		    	break;
		    case VK_END:
		    	WinSendMsg(hwnd, WM_VSCROLL, MPFROMLONG(0), MPFROM2SHORT(0, SB_BOTTOM));
		    	break;
		    case VK_UP:
		    	WinSendMsg(hwnd, WM_VSCROLL, MPFROMLONG(0), MPFROM2SHORT(0, SB_LINEUP));
		    	break;
		    case VK_DOWN:
		    	WinSendMsg(hwnd, WM_VSCROLL, MPFROMLONG(0), MPFROM2SHORT(0, SB_LINEDOWN));
		    	break;
		    case VK_PAGEUP:
		    	WinSendMsg(hwnd, WM_VSCROLL, MPFROMLONG(0), MPFROM2SHORT(0, SB_PAGEUP));
		    	break;
		    case VK_PAGEDOWN:
		    	WinSendMsg(hwnd, WM_VSCROLL, MPFROMLONG(0), MPFROM2SHORT(0, SB_PAGEDOWN));
		    	break;
		    case VK_LEFT:
	    		if (SHORT1FROMMP(mp1) & KC_CTRL)
		    	    WinSendMsg(hwnd, WM_HSCROLL, MPFROMLONG(0), MPFROM2SHORT(0, SB_PAGELEFT));
		  	else
		    	    WinSendMsg(hwnd, WM_HSCROLL, MPFROMLONG(0), MPFROM2SHORT(0, SB_LINELEFT));
		    	break;
		    case VK_RIGHT:
	    		if (SHORT1FROMMP(mp1) & KC_CTRL)
		    	    WinSendMsg(hwnd, WM_HSCROLL, MPFROMLONG(0), MPFROM2SHORT(0, SB_PAGERIGHT));
		    	else
		    	    WinSendMsg(hwnd, WM_HSCROLL, MPFROMLONG(0), MPFROM2SHORT(0, SB_LINERIGHT));
		    	break;
		}
	    }
	    break;
	default:
	    /* All messages not handled by the ClientWndProc must be passed
	     * along to the Presentation Manager for default processing
	     */
	    return WinDefWindowProc(hwnd, mess, mp1, mp2);
  }
  return (MRESULT)FALSE;
}



/* About Dialog Box */
MRESULT EXPENTRY AboutDlgProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  switch(msg) {
    case  WM_COMMAND:
      switch(COMMANDMSG(&msg)->cmd) {
	case DID_OK:
          WinDismissDlg(hwnd, TRUE);
          return (MRESULT)TRUE;
      }
      break;
  }
  return WinDefDlgProc(hwnd, msg, mp1, mp2);
}	
