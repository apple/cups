/* Copyright (C) 1994, 1996, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gscdefs.h,v 1.2 2000/03/08 23:14:34 mike Exp $ */
/* Prototypes for configuration definitions in gconfig.c. */

#ifndef gscdefs_INCLUDED
#  define gscdefs_INCLUDED

#include "gconfigv.h"

/*
 * This file may be #included in places that don't even have stdpre.h,
 * so it mustn't use any Ghostscript definitions in any code that is
 * actually processed here (as opposed to being part of a macro
 * definition).
 */

/* Miscellaneous system constants (read-only systemparams). */
/* They should all be const, but one application needs some of them */
/* to be writable.... */

#if SYSTEM_CONSTANTS_ARE_WRITABLE
#  define CONFIG_CONST		/* */
#else
#  define CONFIG_CONST const
#endif

extern CONFIG_CONST long gs_buildtime;
extern const char *CONFIG_CONST gs_copyright;
extern const char *CONFIG_CONST gs_product;
extern CONFIG_CONST long gs_revision;
extern CONFIG_CONST long gs_revisiondate;
extern CONFIG_CONST long gs_serialnumber;

/* Installation directories and files */
extern const char *const gs_doc_directory;
extern const char *const gs_lib_default_path;
extern const char *const gs_init_file;

/* Resource tables.  In order to avoid importing a large number of types, */
/* we only provide macros for the externs, not the externs themselves. */

/* We need the extra typedef so that the const will apply to the table. */
#define extern_gx_init_table()\
  typedef void (*gx_init_proc)(P1(gs_memory_t *));\
  extern const gx_init_proc gx_init_table[]

/* We need the extra typedef so that the const will apply to the table. */
#define extern_gx_io_device_table()\
  extern const gx_io_device * const gx_io_device_table[]
extern const unsigned gx_io_device_table_count;

/* Return the list of device prototypes, the list of their structure */
/* descriptors, and (as the value) the length of the lists. */
#define extern_gs_lib_device_list()\
  int gs_lib_device_list(P2(const gx_device * const **plist,\
			    gs_memory_struct_type_t **pst))

#endif /* gscdefs_INCLUDED */
