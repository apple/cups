/* Copyright (C) 1992, 1993, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gp_mswin.h */
/* Definitions common to MS Windows implementation */
/* (used by both C code and Windows 'resource') */
  
#define GSTEXT_ICON	50
#define GSIMAGE_ICON	51
#define SPOOL_PORT	100
#define CANCEL_PCDONE	101
#define CANCEL_PRINTING	102

#ifndef RC_INVOKED		/* NOTA BENE */

/* system menu constants for image window */
#define M_COPY_CLIP 1

/* externals from gp_mswin.c */

/* Patch 26.10.94 :for Microsoft C/C++ 8.0 32-Bit       */
/* "_export" is Microsoft 16-Bit specific.              */
/* With MS-C/C++ 8.00 32 Bit "_export" causes an error. */
#if defined(_WIN32) & defined(_MSC_VER)
#define _export
#endif

extern HWND hwndtext;
extern HWND hDlgModeless;
extern HINSTANCE phInstance;
extern const LPSTR szAppName;
extern BOOL is_win32s;
extern BOOL CALLBACK _export AbortProc(HDC, int);
extern BOOL CALLBACK _export CancelDlgProc(HWND, UINT, WPARAM, LPARAM);
extern int is_spool(const char *queue);

#endif				/* !defined(RC_INVOKED) */
