/* Copyright (C) 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: iminst.h,v 1.2 2000/03/08 23:15:13 mike Exp $ */
/* Definition of interpreter instance */
/* Requires stdio_.h, gsmemory.h, iref.h */

#ifndef iminst_INCLUDED
#  define iminst_INCLUDED

#ifndef gs_main_instance_DEFINED
#  define gs_main_instance_DEFINED
typedef struct gs_main_instance_s gs_main_instance;

#endif

/*
 * Define the structure of a search path.  Currently there is only one,
 * but there might be more someday.
 *
 *      container - an array large enough to hold the specified maximum
 * number of directories.  Both the array and all the strings in it are
 * in the 'foreign' VM space.
 *      list - the initial interval of container that defines the actual
 * search list.
 *      env - the contents of an environment variable, implicitly added
 * at the end of the list; may be 0.
 *      final - the final set of directories specified in the makefile;
 * may be 0.
 *      count - the number of elements in the list, excluding a possible
 * initial '.', env, and final.
 */
typedef struct gs_file_path_s {
    ref container;
    ref list;
    const char *env;
    const char *final;
    uint count;
} gs_file_path;

/*
 * Here is where we actually define the structure of interpreter instances.
 * Clients should not reference any of the members.
 */
struct gs_main_instance_s {
    /* The following are set during initialization. */
    FILE *fstdin;
    FILE *fstdout;
    FILE *fstderr;
    gs_memory_t *heap;		/* (C) heap allocator */
    uint memory_chunk_size;	/* 'wholesale' allocation unit */
    ulong name_table_size;
    uint run_buffer_size;
    int init_done;		/* highest init done so far */
    int user_errors;		/* define what to do with errors */
    bool search_here_first;	/* if true, make '.' first lib dir */
    bool run_start;		/* if true, run 'start' after */
    /* processing command line */
    gs_file_path lib_path;	/* library search list (GS_LIB) */
    long base_time[2];		/* starting usertime */
};

/*
 * Note that any file that uses the following definition of default values
 * must include gconfig.h, because of SEARCH_HERE_FIRST.
 */
#define gs_main_instance_default_init_values\
 0, 0, 0, 0, 20000, 0, 0, -1, 0, SEARCH_HERE_FIRST, 1
extern const gs_main_instance gs_main_instance_init_values;

#endif /* iminst_INCLUDED */
