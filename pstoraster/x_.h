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

/* x_.h */
/* Header for including X library calls in Ghostscript X11 driver */

/* Some versions of the X library use `private' as a member name, so: */
#undef private

/* Under DEC C, make sure abs() and exit() are defined. */
#ifdef __DECC
#  include <stdlib.h>
#endif

/* Most X implementations have _Xdebug, but VMS DECWindows doesn't. */
#ifndef VMS
#  define have_Xdebug
#endif

#ifdef VMS

#  ifdef __GNUC__

/*   Names of external functions which contain upper case letters are
 *   modified by the VMS GNU C compiler to prevent confusion between
 *   names such as XOpen and xopen.  GNU C does this by translating a
 *   name like XOpen into xopen_aaaaaaaax with "aaaaaaaa" a hexadecimal
 *   string.  However, this causes problems when we link against the
 *   X library which doesn't contain a routine named xopen_aaaaaaaax.
 *   So, we use #define's to map all X routine names to lower case.
 *   (Note that routines like BlackPixelOfScreen, which are [for VMS]
 *   preprocessor macros, do not appear here.)
 */

/*
 * The names redefined here are those which the current Ghostscript X11
 * driver happens to use: this list may grow in the future.
 */

#    define XAllocColor			xalloccolor
#    define XAllocNamedColor		xallocnamedcolor
#    define XCloseDisplay		xclosedisplay
#    define XCopyArea			xcopyarea
#    define XCreateGC			xcreategc
#    define XCreatePixmap		xcreatepixmap
#    define XCreateWindow		xcreatewindow
#    define XDestroyImage		xdestroyimage
#    define XDisplayString		xdisplaystring
#    define XDrawLine			xdrawline
#    define XDrawPoint			xdrawpoint
#    define XDrawString			xdrawstring
#    define XFillPolygon		xfillpolygon
#    define XFillRectangle		xfillrectangle
#    define XFillRectangles		xfillrectangles
#    define XFlush			xflush
#    define XFree			xfree
#    define XFreeColors			xfreecolors
#    define XFreeFont			xfreefont
#    define XFreeFontNames		xfreefontnames
#    define XFreeGC			xfreegc
#    define XFreePixmap			xfreepixmap
#    define XGetDefault			xgetdefault
#    define XGetGCValues		xgetgcvalues
#    define XGetGeometry		xgetgeometry
#    define XGetImage			xgetimage
#    define XGetRGBColormaps		xgetrgbcolormaps
#    define XGetVisualInfo		xgetvisualinfo
#    define XGetWindowAttributes	xgetwindowattributes
#    define XGetWindowProperty		xgetwindowproperty
#    define XInternAtom			xinternatom
#    define XListFonts			xlistfonts
#    define XLoadQueryFont		xloadqueryfont
#    define XMapWindow			xmapwindow
#    define XNextEvent			xnextevent
#    define XOpenDisplay		xopendisplay
#    define XPutImage			xputimage
#    define XQueryColor			xquerycolor
#    define XResizeWindow		xresizewindow
#    define XSendEvent			xsendevent
#    define XSetBackground		xsetbackground
#    define XSetClipMask		xsetclipmask
#    define XSetClipOrigin		xsetcliporigin
#    define XSetErrorHandler		xseterrorhandler
#    define XSetFillStyle		xsetfillstyle
#    define XSetFont			xsetfont
#    define XSetForeground		xsetforeground
#    define XSetFunction		xsetfunction
#    define XSetLineAttributes		xsetlineattributes
#    define XSetTile			xsettile
#    define XSetWindowBackgroundPixmap	xsetwindowbackgroundpixmap
#    define XSetWMHints			xsetwmhints
#    define XSetWMNormalHints		xsetwmnormalhints
#    define XStoreName			xstorename
#    define XSync			xsync
#    define XVisualIDFromVisual		xvisualidfromvisual
#    define XWMGeometry			xwmgeometry
#    define XtAppCreateShell		xtappcreateshell
#    define XtCloseDisplay		xtclosedisplay
#    define XtCreateApplicationContext	xtcreateapplicationcontext
#    define XtDestroyApplicationContext	xtdestroyapplicationcontext
#    define XtDestroyWidget		xtdestroywidget
#    define XtAppSetFallbackResources	xtappsetfallbackresources
#    define XtGetApplicationResources	xtgetapplicationresources
#    define XtOpenDisplay		xtopendisplay
#    define XtToolkitInitialize		xttoolkitinitialize

#    define CADDR_T			/* Without this DEFINE, VAX GNUC    */
					/* gets trashed reading Intrinsic.h */
#  endif				/* ifdef __GNUC__ */

#  include <decw$include/Xlib.h>
#  include <decw$include/Xproto.h>
#  include <decw$include/Xatom.h>
#  include <decw$include/Xutil.h>
#  include <decw$include/Intrinsic.h>
#  include <decw$include/StringDefs.h>
#  include <decw$include/Shell.h>

#else					/* !ifdef VMS */

#  include <X11/Xlib.h>
#  include <X11/Xproto.h>
#  include <X11/Xatom.h>
#  include <X11/Xutil.h>
#  include <X11/Intrinsic.h>
#  include <X11/StringDefs.h>
#  include <X11/Shell.h>

#endif					/* VMS */

/* X11R3 doesn't have XtOffsetOf, but it has XtOffset. */
#ifndef XtOffsetOf
#  ifdef offsetof
#    define XtOffsetOf(s_type,field) offsetof(s_type,field)
#  else
#    define XtOffsetOf(s_type,field) XtOffset(s_type*,field)
#  endif
#endif

/* Include standard colormap stuff only for X11R4 and later. */
#  if defined(XtSpecificationRelease) && (XtSpecificationRelease >= 4)
#    define HaveStdCMap 1
#  else
#    define HaveStdCMap 0
/* This function is not defined in R3. */
#    undef XVisualIDFromVisual
#    define XVisualIDFromVisual(vis) ((vis)->visualid)
#  endif

/* Restore the definition of `private'. */
#define private private_
