/* Copyright (C) 1991, 1992 Aladdin Enterprises.  All rights reserved.
  
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

/* dos_.h */
/* Generic MS-DOS interface */

/* This file is needed because the various DOS compilers */
/* provide slightly different procedures for interfacing to DOS and */
/* the I/O hardware, and because the Watcom compiler is 32-bit. */
#include <dos.h>
#if defined(__WATCOMC__) || defined(_MSC_VER)

/* ---------------- Microsoft C/C++, all models; */
/* ---------------- Watcom compiler, 32-bit flat model. */
/* ---------------- inp/outp prototypes are in conio.h, not dos.h. */

#  include <conio.h>
#  define inport(px) inpw(px)
#  define inportb(px) inp(px)
#  define outport(px,w) outpw(px,w)
#  define outportb(px,b) outp(px,b)
#  define enable() _enable()
#  define disable() _disable()
#  define PTR_OFF(ptr) ((ushort)(uint)(ptr))
/* Define the structure and procedures for file enumeration. */
#define ff_name name
#define dos_findfirst(n,b) _dos_findfirst(n, _A_NORMAL | _A_RDONLY, b)
#define dos_findnext(b) _dos_findnext(b)

/* Define things that differ between Watcom and Microsoft. */
#  ifdef __WATCOMC__
#    define MK_PTR(seg,off) (((seg) << 4) + (off))
#    define int86 int386
#    define int86x int386x
#    define rshort w
#    define ff_struct_t struct find_t
#  else
#    define MK_PTR(seg,off) (((ulong)(seg) << 16) + (off))
#    define cputs _cputs
#    define fdopen _fdopen
#    define O_BINARY _O_BINARY
#    define REGS _REGS
#    define rshort x
#    define ff_struct_t struct _find_t
#    define stdprn _stdprn
#  endif

#else			/* not Watcom or Microsoft */

/* ---------------- Borland compiler, 16:16 pseudo-segmented model. */
/* ---------------- ffblk is in dir.h, not dos.h. */
#include <dir.h>
#  define MK_PTR(seg,off) MK_FP(seg,off)
#  define PTR_OFF(ptr) FP_OFF(ptr)
/* Define the regs union tag for short registers. */
#  define rshort x
/* Define the structure and procedures for file enumeration. */
#define ff_struct_t struct ffblk
#define dos_findfirst(n,b) findfirst(n, b, 0)
#define dos_findnext(b) findnext(b)

#endif
