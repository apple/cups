/* Copyright (C) 1995 Aladdin Enterprises.  All rights reserved.
  
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

/* icfontab.c */
/* Table of compiled fonts */
#include "ccfont.h"

/* This is compiled separately and linked with the fonts themselves, */
/* in a shared library when applicable. */

#undef font_
#define font_(fname, fproc, zfproc) extern ccfont_fproc fproc;
#ifndef GCONFIGF_H
# include "gconfigf.h"
#else
# include GCONFIGF_H
#endif

private ccfont_fproc *fprocs[] = {
#undef font_
#define font_(fname, fproc, zfproc) &fproc, /* fname, zfproc are not needed */
#ifndef GCONFIGF_H
# include "gconfigf.h"
#else
# include GCONFIGF_H
#endif
  0
};

int
ccfont_fprocs(int *pnum_fprocs, ccfont_fproc ***pfprocs)
{
  *pnum_fprocs = countof(fprocs) - 1;
  *pfprocs = &fprocs[0];
  return ccfont_version;	/* for compatibility checking */
}
