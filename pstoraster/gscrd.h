/* Copyright (C) 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gscrd.h,v 1.1 2000/03/13 18:57:55 mike Exp $ */
/* Interface for CIE color rendering dictionary creation */

#ifndef gscrd_INCLUDED
#  define gscrd_INCLUDED

#include "gscie.h"

/*
 * Allocate and minimally initialize a CRD.  Note that this procedure sets
 * the reference count of the structure to 1, not 0.  gs_setcolorrendering
 * will increment the reference count again, so unless you want the
 * structure to stay allocated permanently (or until a garbage collection),
 * you should call rc_decrement(pcrd, "client name") *after* calling
 * gs_setcolorrendering.
 */
int
    gs_cie_render1_build(P3(gs_cie_render ** ppcrd, gs_memory_t * mem,
			    client_name_t cname));

/*
 * Initialize a CRD given all of the relevant parameters.
 * Any of the pointers except WhitePoint may be zero, meaning
 * use the default values.
 *
 * The actual point, matrix, range, and procedure values are copied into the
 * CRD, but only the pointer to the color lookup table
 * (RenderTable.lookup.table) is copied, not the table itself.
 */
int
    gs_cie_render1_initialize(P14(gs_cie_render * pcrd, void *client_data,
				  const gs_vector3 * WhitePoint,
				  const gs_vector3 * BlackPoint,
				  const gs_matrix3 * MatrixPQR,
				  const gs_range3 * RangePQR,
				const gs_cie_transform_proc3 * TransformPQR,
				  const gs_matrix3 * MatrixLMN,
				  const gs_cie_render_proc3 * EncodeLMN,
				  const gs_range3 * RangeLMN,
				  const gs_matrix3 * MatrixABC,
				  const gs_cie_render_proc3 * EncodeABC,
				  const gs_range3 * RangeABC,
				const gs_cie_render_table_t * RenderTable));

/*
 * Set or access the client_data pointer in a CRD.
 * The macro is an L-value.
 */
#define gs_cie_render_client_data(pcrd) ((pcrd)->client_data)

#endif /* gscrd_INCLUDED */
