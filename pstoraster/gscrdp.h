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

/*$Id: gscrdp.h,v 1.1 2000/03/13 18:57:55 mike Exp $ */
/* Interface for device-specified CRDs */

#ifndef gscrdp_INCLUDED
#  define gscrdp_INCLUDED

#include "gscie.h"
#include "gsparam.h"

/*
 * A driver can provide any number of its own CRDs through (read-only)
 * device parameters whose values are slightly modified PostScript-style
 * dictionaries.  The driver doesn't need to concern itself with how the
 * parameters are encoded: it simply constructs a CRD and calls
 * param_write_cie_render1.
 */
int param_write_cie_render1(P4(gs_param_list * plist, gs_param_name key,
			       const gs_cie_render * pcrd,
			       gs_memory_t * mem));

/*
 * For internal use, we also provide an API that writes the CRD directly
 * into a parameter list, rather than as a named parameter in a larger
 * list.
 */
int param_put_cie_render1(P3(gs_param_list * plist, const gs_cie_render * pcrd,
			     gs_memory_t * mem));

/*
 * Client code that wants to initialize a CRD from a device parameter
 * uses the following complementary procedure.  The customary way to
 * use this is:

 gs_c_param_list list;
 ...
 gs_c_param_list_write(&list, mem);
 gs_c_param_request(&list, "ParamName");
 code = gs_getdeviceparams(dev, &list);
 << error if code < 0 >>
 gs_c_param_list_read(&list);
 code = gs_cie_render1_param_initialize(pcrd, &list, "ParamName", dev);
 gs_c_param_list_release(&list);
 << error if code < 0 >>

 * where "ParamName" is the parameter name, e.g., "CRDDefault".
 */
int gs_cie_render1_param_initialize(P4(gs_cie_render * pcrd,
				       gs_param_list * plist,
				       gs_param_name key,
				       gx_device * dev));

/*
 * Again, we provide an internal procedure that doesn't involve a
 * parameter name.
 */
int param_get_cie_render1(P3(gs_cie_render * pcrd,
			     gs_param_list * plist,
			     gx_device * dev));

/*
 * The actual representation of the CRD is a slightly modified PostScript
 * ColorRenderingType 1 dictionary.  THE FOLLOWING IS SUBJECT TO CHANGE
 * WITHOUT NOTICE.  Specifically, the following keys are different:
 *      ColorRenderingType = 101
 *      (Instead of TransformPQR = [T1 T2 T3]:)
 *        TransformPQRName = procedure name (a name)
 *        TransformPQRData = procedure data (a string)
 *      (Instead of EncodeLMN/ABC = [E1 E2 E3]:)
 *        EncodeLMN/ABCValues = [V1,1 V1,2 ... V3,N], where Vi,j is the
 *          j'th sampled value of the i'th encoding array, mapped linearly
 *          to the corresponding domain (see gscie.h)
 *      (Instead of RenderTable = [NA NB NC table m T1 ... Tm]:)
 *        RenderTableSize = [NA NB NC m]
 *        RenderTableTable = table (an array of strings)
 *        RenderTableTValues = [V1,1 V1,2 ... Vm,N] (see above)
 * The PostScript setcolorrendering operator selects the correct operator
 * according to the ColorRenderingType key.
 */

#endif /* gscrdp_INCLUDED */
