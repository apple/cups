/* Copyright (C) 1995, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: icie.h,v 1.2 2000/03/08 23:15:08 mike Exp $ */
/* Internal definitions for interpreter CIE color handling */

#ifndef icie_INCLUDED
#  define icie_INCLUDED

/*
 * All of the routines below are exported by zcie.c for zcrd.c,
 * except for cie_cache_joint which is exported by zcrd.c for zcie.c.
 */

/* ------ Parameter acquisition ------ */

/* Get a range array parameter from a dictionary. */
/* We know that count <= 4. */
int dict_ranges_param(P4(const ref * pdref, const char *kstr, int count,
			 gs_range * prange));

/* Get 3 ranges from a dictionary. */
#define dict_range3_param(pdref, kstr, prange3)\
  dict_ranges_param(pdref, kstr, 3, (prange3)->ranges)

/* Get a 3x3 matrix parameter from a dictionary. */
#define dict_matrix3_param(op, kstr, pmat)\
  dict_float_array_param(op, kstr, 9, (float *)pmat,\
			 (const float *)&Matrix3_default)
#define matrix3_ok 9

/* Get an array of procedures from a dictionary. */
/* We know count <= countof(empty_procs). */
int dict_proc_array_param(P4(const ref * pdict, const char *kstr,
			     uint count, ref * pparray));

/* Get 3 procedures from a dictionary. */
#define dict_proc3_param(op, kstr, pparray)\
  dict_proc_array_param(op, kstr, 3, pparray)

/* Get WhitePoint and BlackPoint values. */
int cie_points_param(P2(const ref * pdref, gs_cie_wb * pwb));

/* Process a 3- or 4-dimensional lookup table from a dictionary. */
/* The caller has set pclt->n and pclt->m. */
/* ptref is known to be a readable array of size at least n+1. */
int cie_table_param(P3(const ref * ptable, gx_color_lookup_table * pclt,
		       gs_memory_t * mem));

/* ------ Internal routines ------ */

int cie_cache_push_finish(P3(int (*finish_proc) (P1(os_ptr)),
			     gs_ref_memory_t * imem, void *data));
int cie_prepare_cache(P6(const gs_range * domain, const ref * proc,
			 cie_cache_floats * pcache, void *container,
			 gs_ref_memory_t * imem, client_name_t cname));
int cie_prepare_caches_4(P9(const gs_range * domains, const ref * procs,
			    cie_cache_floats * pc0,
			    cie_cache_floats * pc1,
			    cie_cache_floats * pc2,
			    cie_cache_floats * pc3 /* may be 0 */,
			    void *container,
			    gs_ref_memory_t * imem, client_name_t cname));
#define cie_prepare_cache3(d3,p3,c3,pcie,imem,cname)\
  cie_prepare_caches_4((d3)->ranges, p3,\
		       &(c3)->floats, &(c3)[1].floats, &(c3)[2].floats,\
		       NULL, pcie, imem, cname)
#define cie_prepare_cache4(d4,p4,c4,pcie,imem,cname)\
  cie_prepare_caches_4((d4)->ranges, p4,\
		       &(c4)->floats, &(c4)[1].floats, &(c4)[2].floats,\
		       &(c4)[3].floats, pcie, imem, cname)

int cie_cache_joint(P2(const ref_cie_render_procs *, gs_state *));

#endif /* icie_INCLUDED */
