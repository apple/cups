/*
  Copyright 1993-2000 by Easy Software Products.
  Copyright 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.

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

/*$Id: gscdefs.c,v 1.1 2000/03/08 23:14:34 mike Exp $ */
/* Configuration scalars */
#include "stdpre.h"
#include "gscdefs.h"		/* interface */
#include "gconf.h"		/* for #defines */
#include <config.h>

/* ---------------- Miscellaneous system parameters ---------------- */

/* All of these can be set in the makefile. */
/* Normally they are all const; see gscdefs.h for more information. */

#ifndef GS_BUILDTIME
#  define GS_BUILDTIME\
	0			/* should be set in the makefile */
#endif
CONFIG_CONST long gs_buildtime = GS_BUILDTIME;

#ifndef GS_COPYRIGHT
#  define GS_COPYRIGHT\
 	"Copyright 1993-2000 Easy Software Products, All Rights Reserved.\n"\
 	"Copyright 1998 Aladdin Enterprises, Menlo Park, CA.  All rights reserved."
#endif
const char *CONFIG_CONST gs_copyright = GS_COPYRIGHT;

#ifndef GS_PRODUCT
#  define GS_PRODUCT CUPS_SVERSION
#endif
const char *CONFIG_CONST gs_product = GS_PRODUCT;

const char *
gs_program_name(void)
{
    return gs_product;
}

CONFIG_CONST long gs_revision = 550;
CONFIG_CONST long gs_revisiondate = 20000308;

#ifndef GS_SERIALNUMBER
#  define GS_SERIALNUMBER\
	40100
#endif
CONFIG_CONST long gs_serialnumber = GS_SERIALNUMBER;

/* ---------------- Installation directories and files ---------------- */

/* Here is where the library search path, the name of the */
/* initialization file, and the doc directory are defined. */

/* Define the documentation directory (only used in help messages). */
const char *const gs_doc_directory = GS_DOCDIR; /**** DELETE ME ****/

/* Define the default library search path. */
const char *const gs_lib_default_path = CUPS_DATADIR "/pstoraster:" CUPS_DATADIR "/fonts";

/* Define the interpreter initialization file. */
const char *const gs_init_file = "gs_init.ps";
