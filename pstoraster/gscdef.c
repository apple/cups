/*
  Copyright 1993-2000 by Easy Software Products.
  Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gscdef.c */
/* Configuration scalars */
#include "stdpre.h"
#include "gscdefs.h"		/* interface */
#include "gconfig.h"		/* for #defines */

/* ---------------- Miscellaneous system parameters ---------------- */

/* All of these can be set in the makefile. */
/* They should all be const; see gscdefs.h for more information. */

#ifndef GS_BUILDTIME
#  define GS_BUILDTIME\
	0		/****** HOW TO SET THIS? ******/
#endif
long gs_buildtime = GS_BUILDTIME;

#ifndef GS_COPYRIGHT
#  define GS_COPYRIGHT\
	"Copyright 1993-2000 Easy Software Products, All Rights Reserved.\n"\
	"Copyright 1996 Aladdin Enterprises, Menlo Park, CA.  All rights reserved."
#endif
const char *gs_copyright = GS_COPYRIGHT;	

#ifndef GS_PRODUCT
#  define GS_PRODUCT\
	"ESP Print Pro v4.1"
#endif
const char *gs_product = GS_PRODUCT;

#ifndef GS_REVISION
#  define GS_REVISION\
	40000		/* MMmmPP */
#endif
long gs_revision = GS_REVISION;		/* should be const, see gscdefs.h */

#ifndef GS_REVISIONDATE
#  define GS_REVISIONDATE\
	19990507	/* year x 10000 + month x 100 + day. */
#endif
long gs_revisiondate = GS_REVISIONDATE;	/* should be const, see gscdefs.h */

#ifndef GS_SERIALNUMBER
#  define GS_SERIALNUMBER\
	40000		/* MMmmPP */
#endif
long gs_serialnumber = GS_SERIALNUMBER;	/* should be const, see gscdefs.h */

/* ---------------- Installation directories and files ---------------- */

/* Here is where the library search path and the name of the */
/* initialization file are defined. */

/* Define the default library search path. */
const char *gs_lib_default_path = CUPS_DATADIR "/pstoraster:" CUPS_DATADIR "/fonts";

/* Define the interpreter initialization file. */
const char *gs_init_file = "gs_init.ps";
