/* Copyright (C) 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* zcrd.c */
/* CIE color rendering operators */
#include "math_.h"
#include "ghost.h"
#include "errors.h"
#include "oper.h"
#include "gsstruct.h"
#include "gscspace.h"
#include "gscolor2.h"
#include "gscie.h"
#include "estack.h"
#include "ialloc.h"
#include "idict.h"
#include "idparam.h"
#include "igstate.h"
#include "icie.h"
#include "isave.h"
#include "ivmspace.h"
#include "store.h"		/* for make_null */

/* Forward references */
private int zcrd_params(P4(os_ptr, gs_cie_render *, ref_cie_render_procs *, gs_memory_t *));
private int cache_colorrendering(P3(gs_cie_render *,
  const ref_cie_render_procs *, gs_state *));

/* Allocator structure type for CIE rendering structure */
private_st_cie_render();

/* - currentcolorrendering <dict> */
private int
zcurrentcolorrendering(register os_ptr op)
{	push(1);
	*op = istate->colorrendering.dict;
	return 0;
}

/* <dict> setcolorrendering - */
private int
zsetcolorrendering(register os_ptr op)
{	gs_memory_t *mem = gs_state_memory(igs);
	int code;
	es_ptr ep = esp;
	gs_cie_render *pcie;
	ref_cie_render_procs procs_old;

	check_read_type(*op, t_dictionary);
	check_dict_read(*op);
	rc_alloc_struct_0(pcie, gs_cie_render, &st_cie_render, mem,
			  return_error(e_VMerror),
			  "setcolorrendering");
	/* gs_setcolorrendering may refer to istate->colorrendering.procs. */
	procs_old = istate->colorrendering.procs;
	code = zcrd_params(op, pcie, &istate->colorrendering.procs, mem);
	if ( code < 0 ||
	     (code = gs_setcolorrendering(igs, pcie)) < 0 ||
	     (code = cache_colorrendering(pcie, &istate->colorrendering.procs, igs)) < 0
	   )
	{	rc_free_struct(pcie, mem, "setcolorrendering");
		istate->colorrendering.procs = procs_old;
		esp = ep;
		return code;
	}
	istate->colorrendering.dict = *op;
	pop(1);
	return (esp == ep ? 0 : o_push_estack);
}
/* Get the CRD parameters from the PostScript dictionary. */
private int
zcrd_params(os_ptr op, gs_cie_render *pcie,
  ref_cie_render_procs *pcprocs, gs_memory_t *mem)
{	int code;
	int ignore;
	ref *pRT;

	if ( (code = dict_int_param(op, "ColorRenderingType", 1, 1, 0, &ignore)) < 0 ||
	     (code = dict_matrix3_param(op, "MatrixLMN", &pcie->MatrixLMN)) != matrix3_ok ||
	     (code = dict_proc3_param(op, "EncodeLMN", &pcprocs->EncodeLMN)) < 0 ||
	     (code = dict_range3_param(op, "RangeLMN", &pcie->RangeLMN)) < 0 ||
	     (code = dict_matrix3_param(op, "MatrixABC", &pcie->MatrixABC)) != matrix3_ok ||
	     (code = dict_proc3_param(op, "EncodeABC", &pcprocs->EncodeABC)) < 0 ||
	     (code = dict_range3_param(op, "RangeABC", &pcie->RangeABC)) < 0 ||
	     (code = cie_points_param(op, &pcie->points)) < 0 ||
	     (code = dict_matrix3_param(op, "MatrixPQR", &pcie->MatrixPQR)) != matrix3_ok ||
	     (code = dict_range3_param(op, "RangePQR", &pcie->RangePQR)) < 0 ||
	     (code = dict_proc3_param(op, "TransformPQR", &pcprocs->TransformPQR)) < 0
	   )
	  return (code < 0 ? code : gs_note_error(e_rangecheck));
#define rRT pcie->RenderTable.lookup
	if ( dict_find_string(op, "RenderTable", &pRT) > 0 )
	{	const ref *prte;
		int i;

		check_read_type(*pRT, t_array);
		if ( r_size(pRT) < 5 )
		  return_error(e_rangecheck);
		prte = pRT->value.const_refs;
		check_type_only(prte[4], t_integer);
		if ( !(prte[4].value.intval == 3 || prte[4].value.intval == 4) )
		  return_error(e_rangecheck);
		rRT.n = 3;
		rRT.m = prte[4].value.intval;
		if ( r_size(pRT) != rRT.m + 5 )
		  return_error(e_rangecheck);
		prte += 5;
		for ( i = 0; i < rRT.m; i++ )
		  check_proc_only(prte[i]);
		code = cie_table_param(pRT, &rRT, mem);
		if ( code < 0 )
		  return code;
		make_const_array(&pcprocs->RenderTableT,
				 a_readonly | r_space(pRT),
				 rRT.m, prte);
	}
	else
	{	rRT.table = 0;
		make_null(&pcprocs->RenderTableT);
	}
#undef rRT
	pcie->EncodeLMN = Encode_default;
	pcie->EncodeABC = Encode_default;
	pcie->TransformPQR = TransformPQR_default;
	pcie->RenderTable.T = RenderTableT_default;
	return 0;
}

/* Cache the results of the color rendering procedures. */
private int cie_cache_render_finish(P1(os_ptr));
private int
cache_colorrendering(gs_cie_render *pcie,
  const ref_cie_render_procs *pcrprocs, gs_state *pgs)
{	es_ptr ep = esp;
	int code = gs_cie_render_init(pcie);	/* sets Domain values */
	int i;

	/* We must run gs_cie_render_complete when we're done. */
	if ( code < 0 ||
	     (gs_cie_cs_common(pgs) != 0 &&
	      (code = cie_cache_joint(pcrprocs, pgs)) < 0) ||	/* do this last */
	     (code = cie_cache_push_finish(cie_cache_render_finish, pgs, pcie)) < 0 ||
	     (code = cie_prepare_cache3(&pcie->DomainLMN, pcrprocs->EncodeLMN.value.const_refs, &pcie->caches.EncodeLMN[0], pcie, pgs, "Encode.LMN")) < 0 ||
	     (code = cie_prepare_cache3(&pcie->DomainABC, pcrprocs->EncodeABC.value.const_refs, &pcie->caches.EncodeABC[0], pcie, pgs, "Encode.ABC")) < 0
	   )
	  {	esp = ep;
		return code;
	  }
	if ( pcie->RenderTable.lookup.table != 0 )
	  { bool is_identity = true;
	    for ( i = 0; i < pcie->RenderTable.lookup.m; i++ )
	      if ( r_size(pcrprocs->RenderTableT.value.const_refs + i) != 0 )
		{ is_identity = false;
		  break;
		}
	    pcie->caches.RenderTableT_is_identity = is_identity;
	    if ( !is_identity )
	      for ( i = 0; i < pcie->RenderTable.lookup.m; i++ )
		if ( (code =
		      cie_prepare_cache(Range4_default.ranges,
					pcrprocs->RenderTableT.value.const_refs + i,
					&pcie->caches.RenderTableT[i].floats,
					pcie, pgs, "RenderTable.T")) < 0
		   )
		  { esp = ep;
		    return code;
		  }
	  }
	return o_push_estack;
}

/* Finish up after loading the rendering caches. */
private int
cie_cache_render_finish(os_ptr op)
{	gs_cie_render *pcie = r_ptr(op, gs_cie_render);
	int code;
	if ( pcie->RenderTable.lookup.table != 0 && !pcie->caches.RenderTableT_is_identity )
	{	/* Convert the RenderTableT cache from floats to fracs. */
		int j;
		for ( j = 0; j < pcie->RenderTable.lookup.m; j++ )
		  gs_cie_cache_to_fracs(&pcie->caches.RenderTableT[j]);
	}
	code = gs_cie_render_complete(pcie);
	if ( code < 0 )
	  return code;
	pop(1);
	return 0;
}

/* ------ Internal routines ------ */

/* Load the joint caches. */
private int
  cie_exec_tpqr(P1(os_ptr)),
  cie_post_exec_tpqr(P1(os_ptr)),
  cie_tpqr_finish(P1(os_ptr));
int
cie_cache_joint(const ref_cie_render_procs *pcrprocs, gs_state *pgs)
{	const gs_cie_render *pcier = gs_currentcolorrendering(pgs);
	/* The former installation procedures have allocated */
	/* the joint caches and filled in points_sd. */
	gx_cie_joint_caches *pjc = gx_currentciecaches(pgs);
	ref pqr_procs;
#define pqr_refs pqr_procs.value.refs
	uint space;
	int code;
	int i;

	if ( pcier == 0 )	/* cache is not set up yet */
	  return 0;
	if ( pjc == 0 )
	  return_error(e_VMerror);
	code = ialloc_ref_array(&pqr_procs, a_readonly, 3*(1+4+4*6),
				"cie_cache_common");
	if ( code < 0 ) return code;
	/* When we're done, deallocate the procs and complete the caches. */
	check_estack(3);
	cie_cache_push_finish(cie_tpqr_finish, pgs, pgs);
	*++esp = pqr_procs;
	space = r_space(&pqr_procs);
	for ( i = 0; i < 3; i++ )
	{	ref *p = pqr_refs + 3 + (4+4*6) * i;
		const float *ppt = (float *)&pjc->points_sd;
		int j;

		make_array(pqr_refs + i, a_readonly | a_executable | space,
			   4, p);
		make_array(p, a_readonly | space, 4*6, p + 4);
		p[1] = pcrprocs->TransformPQR.value.refs[i];
		make_oper(p + 2, 0, cie_exec_tpqr);
		make_oper(p + 3, 0, cie_post_exec_tpqr);
		for ( j = 0, p += 4; j < 4*6; j++, p++, ppt++ )
		  make_real(p, *ppt);
	}
	return cie_prepare_cache3(&pcier->RangePQR,
				  pqr_procs.value.const_refs,
				  &pjc->TransformPQR[0],
				  pjc, pgs, "Transform.PQR");
}

/* Private operator to shuffle arguments for the TransformPQR procedure: */
/* v [ws wd bs bd] proc -> -mark- ws wd bs bd v proc + exec */
private int
cie_exec_tpqr(register os_ptr op)
{	const ref *ppt = op[-1].value.const_refs;
	uint space = r_space(op - 1);
	int i;

	check_op(4);
	push(4);
	*op = op[-4];		/* proc */
	op[-1] = op[-6];	/* v */
	for ( i = 0; i < 4; i++ )
	  make_const_array(op - 5 + i, a_readonly | space,
			   6, ppt + i * 6);
	make_mark(op - 6);
	return zexec(op);
}

/* Remove extraneous values from the stack after executing */
/* the TransformPQR procedure.  -mark- ... v -> v */
private int
cie_post_exec_tpqr(register os_ptr op)
{	uint count = ref_stack_counttomark(&o_stack);
	ref vref;

	if ( count < 2 )
	  return_error(e_unmatchedmark);
	vref = *op;
	ref_stack_pop(&o_stack, count - 1);
	*osp = vref;
	return 0;
}

/* Free the procs array and complete the joint caches. */
private int
cie_tpqr_finish(register os_ptr op)
{	gs_state *pgs = r_ptr(op, gs_state);
	ifree_ref_array(op - 1, "cie_tpqr_finish");
	gs_cie_cs_complete(pgs, false);
	pop(2);
	return 0;
}

/* ------ Initialization procedure ------ */

BEGIN_OP_DEFS(zcrd_l2_op_defs) {
		op_def_begin_level2(),
	{"0currentcolorrendering", zcurrentcolorrendering},
	{"1setcolorrendering", zsetcolorrendering},
		/* Internal operators */
	{"1%cie_render_finish", cie_cache_render_finish},
	{"3%cie_exec_tpqr", cie_exec_tpqr},
	{"2%cie_post_exec_tpqr", cie_post_exec_tpqr},
	{"1%cie_tpqr_finish", cie_tpqr_finish},
END_OP_DEFS(0) }
