/* Copyright (C) 1994, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gscdefs.h */
/* Prototypes for configuration definitions in gconfig.c. */

/* Miscellaneous system constants (read-only systemparams). */
/* They should all be const, but one application needs some of them */
/* to be writable.... */
extern long gs_buildtime;
extern const char *gs_copyright;
extern const char *gs_product;
extern long gs_revision;
extern long gs_revisiondate;
extern long gs_serialnumber;

/* Installation directories and files */
extern const char *gs_doc_directory;
extern const char *gs_lib_default_path;
extern const char *gs_init_file;

/* Resource tables.  In order to avoid importing a large number of types, */
/* we only provide macros for the externs, not the externs themselves. */

#define extern_gx_init_table()\
  extern void (*gx_init_table[])(P1(gs_memory_t *))

#define extern_gx_io_device_table()\
  extern gx_io_device *gx_io_device_table[]

/* Return the list of device prototypes, the list of their structure */
/* descriptors, and (as the value) the length of the lists. */
#define extern_gs_lib_device_list()\
  int gs_lib_device_list(P2(const gx_device ***plist,\
			    gs_memory_struct_type_t **pst))
