/* Copyright (C) 1994-1996, Russell Lang.  All rights reserved.
  
  This file is part of GNU Ghostscript.
  
  GNU Ghostscript is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY.  No author or distributor accepts responsibility
  to anyone for the consequences of using it or for whether it serves any
  particular purpose or works at all, unless he says so in writing.  Refer
  to the GNU General Public License for full details.
  
  Everyone is granted permission to copy, modify and redistribute GNU
  Ghostscript, but only under the conditions described in the GNU General
  Public License.  A copy of this license is supposed to have been given
  to you along with GNU Ghostscript so you can know your rights and
  responsibilities.  It should be in a file named COPYING.  Among other
  things, the copyright notice and this notice must be preserved on all
  copies.
  
  Aladdin Enterprises supports the work of the GNU Project, but is not
  affiliated with the Free Software Foundation or the GNU Project.  GNU
  Ghostscript, as distributed by Aladdin Enterprises, does not require any
  GNU software to build or run it.
*/


/*$Id: gsdll.h,v 1.1 2000/03/13 18:57:55 mike Exp $ */

#ifndef gsdll_INCLUDED
#  define gsdll_INCLUDED

#ifndef _GSDLL_H
#define _GSDLL_H

#ifndef GSDLLEXPORT
#define GSDLLEXPORT
#endif

#ifdef __WINDOWS__
#define _Windows
#endif

/* type of exported functions */
#ifdef _Windows
#ifdef _WATCOM_
#define GSDLLAPI GSDLLEXPORT
#else
#define GSDLLAPI CALLBACK GSDLLEXPORT
#endif
#else
#ifdef __IBMC__
#define GSDLLAPI _System
#else
#define GSDLLAPI
#endif
#endif

#ifdef _Windows
#define GSDLLCALLLINK
#define GSFAR FAR
#else
#ifdef __IBMC__
#define GSDLLCALLLINK _System
#else
#define GSDLLCALLLINK
#endif
#define GSFAR
#endif

/* global pointer to callback */
typedef int (GSFAR * GSDLLCALLLINK GSDLL_CALLBACK) (int, char GSFAR *, unsigned long);
extern GSDLL_CALLBACK pgsdll_callback;

/* message values for callback */
#define GSDLL_STDIN 1		/* get count characters to str from stdin */
			/* return number of characters read */
#define GSDLL_STDOUT 2		/* put count characters from str to stdout */
			/* return number of characters written */
#define GSDLL_DEVICE 3		/* device = str has been opened if count=1 */
			/*                    or closed if count=0 */
#define GSDLL_SYNC 4		/* sync_output for device str */
#define GSDLL_PAGE 5		/* output_page for device str */
#define GSDLL_SIZE 6		/* resize for device str */
			/* LOWORD(count) is new xsize */
			/* HIWORD(count) is new ysize */
#define GSDLL_POLL 7		/* Called from gp_check_interrupt */
			/* Can be used by caller to poll the message queue */
			/* Normally returns 0 */
			/* To abort gsdll_execute_cont(), return a */
			/* non zero error code until gsdll_execute_cont() */
			/* returns */

/* return values from gsdll_init() */
#define GSDLL_INIT_IN_USE  100	/* DLL is in use */
#define GSDLL_INIT_QUIT    101	/* quit or EOF during init */
				  /* This is not an error. */
				  /* gsdll_exit() must not be called */


/* DLL exported  functions */
/* for load time dynamic linking */
int GSDLLAPI gsdll_revision(char GSFAR * GSFAR * product, char GSFAR * GSFAR * copyright, long GSFAR * gs_revision, long GSFAR * gs_revisiondate);
int GSDLLAPI gsdll_init(GSDLL_CALLBACK callback, HWND hwnd, int argc, char GSFAR * GSFAR * argv);
int GSDLLAPI gsdll_execute_begin(void);
int GSDLLAPI gsdll_execute_cont(const char GSFAR * str, int len);
int GSDLLAPI gsdll_execute_end(void);
int GSDLLAPI gsdll_exit(void);
int GSDLLAPI gsdll_lock_device(unsigned char *device, int flag);

#ifdef _Windows
HGLOBAL GSDLLAPI gsdll_copy_dib(unsigned char GSFAR * device);
HPALETTE GSDLLAPI gsdll_copy_palette(unsigned char GSFAR * device);
void GSDLLAPI gsdll_draw(unsigned char GSFAR * device, HDC hdc, LPRECT dest, LPRECT src);
int GSDLLAPI gsdll_get_bitmap_row(unsigned char *device, LPBITMAPINFOHEADER pbmih,
		     LPRGBQUAD prgbquad, LPBYTE * ppbyte, unsigned int row);

#else
unsigned long gsdll_get_bitmap(unsigned char *device, unsigned char **pbitmap);

#endif

/* Function pointer typedefs */
/* for run time dynamic linking */
typedef int (GSDLLAPI * PFN_gsdll_revision) (char GSFAR * GSFAR *, char GSFAR * GSFAR *, long GSFAR *, long GSFAR *);
typedef int (GSDLLAPI * PFN_gsdll_init) (GSDLL_CALLBACK, HWND, int argc, char GSFAR * GSFAR * argv);
typedef int (GSDLLAPI * PFN_gsdll_execute_begin) (void);
typedef int (GSDLLAPI * PFN_gsdll_execute_cont) (const char GSFAR * str, int len);
typedef int (GSDLLAPI * PFN_gsdll_execute_end) (void);
typedef int (GSDLLAPI * PFN_gsdll_exit) (void);
typedef int (GSDLLAPI * PFN_gsdll_lock_device) (unsigned char GSFAR *, int);

#ifdef _Windows
typedef HGLOBAL(GSDLLAPI * PFN_gsdll_copy_dib) (unsigned char GSFAR *);
typedef HPALETTE(GSDLLAPI * PFN_gsdll_copy_palette) (unsigned char GSFAR *);
typedef void (GSDLLAPI * PFN_gsdll_draw) (unsigned char GSFAR *, HDC, LPRECT, LPRECT);
typedef int (GSDLLAPI * PFN_gsdll_get_bitmap_row) (unsigned char *device, LPBITMAPINFOHEADER pbmih,
		     LPRGBQUAD prgbquad, LPBYTE * ppbyte, unsigned int row);

#else
typedef long (*GSDLLAPI PFN_gsdll_get_bitmap) (unsigned char *, unsigned char **);

#endif

#endif

#endif /* gsdll_INCLUDED */
