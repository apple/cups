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

/*$Id: zcrd.c,v 1.2 2000/03/08 23:15:33 mike Exp $ */
/* CIE color rendering operators */
#include "math_.h"
#include "ghost.h"
#include "oper.h"
#include "gsstruct.h"
#include "gscspace.h"
#include "gscolor2.h"
#include "gscrd.h"
#include "estack.h"
#include "ialloc.h"
#include "idict.h"
#include "idparam.h"
#include "igstate.h"
#include "icie.h"
#include "ivmspace.h"
#include "store.h"		/* for make_null */

/*#define TEST*/

/* Forward references */
private int zcrd1_proc_params(P2(os_ptr op, ref_cie_render_procs * pcprocs));
private int zcrd1_params(P4(os_ptr op, gs_cie_render * pcrd,
			ref_cie_render_procs * pcprocs, gs_memory_t * mem));
private int cache_colorrendering1(P3(gs_cie_render * pcrd,
				     const ref_cie_render_procs * pcprocs,
				     gs_ref_memory_t * imem));

/* - currentcolorrendering <dict> */
private int
zcurrentcolorrendering(os_ptr op)
{
    push(1);
    *op = istate->colorrendering.dict;
    return 0;
}

/* <dict> <crd> .setcolorrendering1 - */
private int
zsetcolorrendering1(os_ptr op)
{
    es_ptr ep = esp;
    ref_cie_render_procs procs;
    int code;

    check_type(op[-1], t_dictionary);
    check_stype(*op, st_cie_render1);
    code = zcrd1_proc_params(op - 1, &procs);
    if (code < 0)
	return code;
    code = gs_setcolorrendering(igs, r_ptr(op, gs_cie_render));
    if (code < 0)
	return code;
    if (gs_cie_cs_common(igs) != 0 &&
	(code = cie_cache_joint(&procs, igs)) < 0
	)
	return code;
    istate->colorrendering.dict = op[-1];
    istate->colorrendering.procs = procs;
    pop(2);
    return (esp == ep ? 0 : o_push_estack);
}

/* <dict> .buildcolorrendering1 <crd> */
private int
zbuildcolorrendering1(os_ptr op)
{
    gs_memory_t *mem = gs_state_memory(igs);
    int code;
    es_ptr ep = esp;
    gs_cie_render *pcrd;
    ref_cie_render_procs procs;

    check_read_type(*op, t_dictionary);
    check_dict_read(*op);
    code = gs_cie_render1_build(&pcrd, mem, ".setcolorrendering1");
    if (code < 0)
	return code;
    code = zcrd1_params(op, pcrd, &procs, mem);
    if (code < 0 ||
    (code = cache_colorrendering1(pcrd, &procs, (gs_ref_memory_t *) mem)) < 0
	) {
	rc_free_struct(pcrd, ".setcolorrendering1");
	esp = ep;
	return code;
    }
    /****** FIX refct ******/
    /*rc_decrement(pcrd, ".setcolorrendering1"); *//* build sets rc = 1 */
    istate->colorrendering.dict = *op;
    make_istruct_new(op, a_readonly, pcrd);
    return (esp == ep ? 0 : o_push_estack);
}
/* Get ColorRenderingType 1 procedures from the PostScript dictionary. */
private int
zcrd1_proc_params(os_ptr op, ref_cie_render_procs * pcprocs)
{
    int code;
    ref *pRT;

    if ((code = dict_proc3_param(op, "EncodeLMN", &pcprocs->EncodeLMN)) < 0 ||
      (code = dict_proc3_param(op, "EncodeABC", &pcprocs->EncodeABC)) < 0 ||
    (code = dict_proc3_param(op, "TransformPQR", &pcprocs->TransformPQR)) < 0
	)
	return (code < 0 ? code : gs_note_error(e_rangecheck));
    if (dict_find_string(op, "RenderTable", &pRT) > 0) {
	const ref *prte;
	int size;
	int i;

	check_read_type(*pRT, t_array);
	size = r_size(pRT);
	if (size < 5)
	    return_error(e_rangecheck);
	prte = pRT->value.const_refs;
	for (i = 5; i < size; i++)
	    check_proc_only(prte[i]);
	make_const_array(&pcprocs->RenderTableT, a_readonly | r_space(pRT),
			 size - 5, prte + 5);
    } else
	make_null(&pcprocs->RenderTableT);
    return 0;
}
/* Get ColorRenderingType 1 parameters from the PostScript dictionary. */
private int
zcrd1_params(os_ptr op, gs_cie_render * pcrd,
	     ref_cie_render_procs * pcprocs, gs_memory_t * mem)
{
    int code;
    int ignore;
    gx_color_lookup_table *const prtl = &pcrd->RenderTable.lookup;
    ref *pRT;

    if ((code = dict_int_param(op, "ColorRenderingType", 1, 1, 0, &ignore)) < 0 ||
	(code = zcrd1_proc_params(op, pcprocs)) < 0 ||
	(code = dict_matrix3_param(op, "MatrixLMN", &pcrd->MatrixLMN)) != matrix3_ok ||
	(code = dict_range3_param(op, "RangeLMN", &pcrd->RangeLMN)) < 0 ||
	(code = dict_matrix3_param(op, "MatrixABC", &pcrd->MatrixABC)) != matrix3_ok ||
	(code = dict_range3_param(op, "RangeABC", &pcrd->RangeABC)) < 0 ||
	(code = cie_points_param(op, &pcrd->points)) < 0 ||
	(code = dict_matrix3_param(op, "MatrixPQR", &pcrd->MatrixPQR)) != matrix3_ok ||
	(code = dict_range3_param(op, "RangePQR", &pcrd->RangePQR)) < 0
	)
	return (code < 0 ? code : gs_note_error(e_rangecheck));
    if (dict_find_string(op, "RenderTable", &pRT) > 0) {
	const ref *prte = pRT->value.const_refs;

	/* Finish unpacking and checking the RenderTable parameter. */
	check_type_only(prte[4], t_integer);
	if (!(prte[4].value.intval == 3 || prte[4].value.intval == 4))
	    return_error(e_rangecheck);
	prtl->n = 3;
	prtl->m = prte[4].value.intval;
	if (r_size(pRT) != prtl->m + 5)
	    return_error(e_rangecheck);
	code = cie_table_param(pRT, prtl, mem);
	if (code < 0)
	    return code;
    } else {
	prtl->table = 0;
    }
    pcrd->EncodeLMN = Encode_default;
    pcrd->EncodeABC = Encode_default;
    pcrd->TransformPQR = TransformPQR_default;
    pcrd->RenderTable.T = RenderTableT_default;
    return 0;
}

/* Cache the results of the color rendering procedures. */
private int cie_cache_render_finish(P1(os_ptr));
private int
cache_colorrendering1(gs_cie_render * pcrd,
		      const ref_cie_render_procs * pcrprocs,
		      gs_ref_memory_t * imem)
{
    es_ptr ep = esp;
    int code = gs_cie_render_init(pcrd);	/* sets Domain values */
    int i;

    if (code < 0 ||
	(code = cie_cache_push_finish(cie_cache_render_finish, imem, pcrd)) < 0 ||
	(code = cie_prepare_cache3(&pcrd->DomainLMN, pcrprocs->EncodeLMN.value.const_refs, &pcrd->caches.EncodeLMN[0], pcrd, imem, "Encode.LMN")) < 0 ||
	(code = cie_prepare_cache3(&pcrd->DomainABC, pcrprocs->EncodeABC.value.const_refs, &pcrd->caches.EncodeABC[0], pcrd, imem, "Encode.ABC")) < 0
	) {
	esp = ep;
	return code;
    }
    if (pcrd->RenderTable.lookup.table != 0) {
	bool is_identity = true;

	for (i = 0; i < pcrd->RenderTable.lookup.m; i++)
	    if (r_size(pcrprocs->RenderTableT.value.const_refs + i) != 0) {
		is_identity = false;
		break;
	    }
	pcrd->caches.RenderTableT_is_identity = is_identity;
	if (!is_identity)
	    for (i = 0; i < pcrd->RenderTable.lookup.m; i++)
		if ((code =
		     cie_prepare_cache(Range4_default.ranges,
				pcrprocs->RenderTableT.value.const_refs + i,
				       &pcrd->caches.RenderTableT[i].floats,
				       pcrd, imem, "RenderTable.T")) < 0
		    ) {
		    esp = ep;
		    return code;
		}
    }
    return o_push_estack;
}

/* Finish up after loading the rendering caches. */
private int
cie_cache_render_finish(os_ptr op)
{
    gs_cie_render *pcrd = r_ptr(op, gs_cie_render);
    int code;

    if (pcrd->RenderTable.lookup.table != 0 &&
	!pcrd->caches.RenderTableT_is_identity
	) {
	/* Convert the RenderTableT cache from floats to fracs. */
	int j;

	for (j = 0; j < pcrd->RenderTable.lookup.m; j++)
	    gs_cie_cache_to_fracs(&pcrd->caches.RenderTableT[j]);
    }
    pcrd->status = CIE_RENDER_STATUS_SAMPLED;
    code = gs_cie_render_complete(pcrd);
    if (code < 0)
	return code;
    /* Note that the cache holds the only record of the values. */
    pcrd->EncodeLMN = EncodeLMN_from_cache;
    pcrd->EncodeABC = EncodeABC_from_cache;
    pop(1);
    return 0;
}

/* ------ Internal procedures ------ */

/* Load the joint caches. */
private int
    cie_exec_tpqr(P1(os_ptr)), cie_post_exec_tpqr(P1(os_ptr)), cie_tpqr_finish(P1(os_ptr));
int
cie_cache_joint(const ref_cie_render_procs * pcrprocs, gs_state * pgs)
{
    const gs_cie_render *pcrd = gs_currentcolorrendering(pgs);
    /*
     * The former installation procedures have allocated
     * the joint caches and filled in points_sd.
     */
    gx_cie_joint_caches *pjc = gx_currentciecaches(pgs);
    gs_ref_memory_t *imem = (gs_ref_memory_t *) gs_state_memory(pgs);
    ref pqr_procs;
    uint space;
    int code;
    int i;

    if (pcrd == 0)		/* cache is not set up yet */
	return 0;
    if (pjc == 0)
	return_error(e_VMerror);
    code = ialloc_ref_array(&pqr_procs, a_readonly, 3 * (1 + 4 + 4 * 6),
			    "cie_cache_common");
    if (code < 0)
	return code;
    /* When we're done, deallocate the procs and complete the caches. */
    check_estack(3);
    cie_cache_push_finish(cie_tpqr_finish, imem, pgs);
    *++esp = pqr_procs;
    space = r_space(&pqr_procs);
    for (i = 0; i < 3; i++) {
	ref *p = pqr_procs.value.refs + 3 + (4 + 4 * 6) * i;
	const float *ppt = (float *)&pjc->points_sd;
	int j;

	make_array(pqr_procs.value.refs + i, a_readonly | a_executable | space,
		   4, p);
	make_array(p, a_readonly | space, 4 * 6, p + 4);
	p[1] = pcrprocs->TransformPQR.value.refs[i];
	make_oper(p + 2, 0, cie_exec_tpqr);
	make_oper(p + 3, 0, cie_post_exec_tpqr);
	for (j = 0, p += 4; j < 4 * 6; j++, p++, ppt++)
	    make_real(p, *ppt);
    }
    return cie_prepare_cache3(&pcrd->RangePQR,
			      pqr_procs.value.const_refs,
			      &pjc->TransformPQR[0],
			      pjc, imem, "Transform.PQR");
}

/* Private operator to shuffle arguments for the TransformPQR procedure: */
/* v [ws wd bs bd] proc -> -mark- ws wd bs bd v proc + exec */
private int
cie_exec_tpqr(os_ptr op)
{
    const ref *ppt = op[-1].value.const_refs;
    uint space = r_space(op - 1);
    int i;

    check_op(3);
    push(4);
    *op = op[-4];		/* proc */
    op[-1] = op[-6];		/* v */
    for (i = 0; i < 4; i++)
	make_const_array(op - 5 + i, a_readonly | space,
			 6, ppt + i * 6);
    make_mark(op - 6);
    return zexec(op);
}

/* Remove extraneous values from the stack after executing */
/* the TransformPQR procedure.  -mark- ... v -> v */
private int
cie_post_exec_tpqr(os_ptr op)
{
    uint count = ref_stack_counttomark(&o_stack);
    ref vref;

    if (count < 2)
	return_error(e_unmatchedmark);
    vref = *op;
    ref_stack_pop(&o_stack, count - 1);
    *osp = vref;
    return 0;
}

/* Free the procs array and complete the joint caches. */
private int
cie_tpqr_finish(os_ptr op)
{
    gs_state *pgs = r_ptr(op, gs_state);
    int code;

    ifree_ref_array(op - 1, "cie_tpqr_finish");
    code = gs_cie_cs_complete(pgs, false);
    pop(2);
    return code;
}

/* ------ Operators for testing ------ */

#ifdef TEST

#include "gscrdp.h"
#include "iparam.h"

/* - .currentcrd <dict> */
private int
zcurrentcrd(os_ptr op)
{
    stack_param_list list;
    int code;

    stack_param_list_write(&list, &o_stack, NULL);
    code = param_write_cie_render1((gs_param_list *) & list, "CRD",
				   gs_currentcolorrendering(igs), imemory);
    if (code < 0)
	return code;
    ref_assign(osp - 1, osp);
    pop(1);
    return 0;
}

/* <dict> .setcrd - */
private int
zsetcrd(os_ptr op)
{
    gs_memory_t *mem = gs_state_memory(igs);
    dict_param_list list;
    gs_cie_render *pcrd = 0;
    int code;

    check_type(*op, t_dictionary);
    code = dict_param_list_read(&list, op, NULL, false);
    if (code < 0)
	return code;
    code = gs_cie_render1_build(&pcrd, mem, ".setcrd");
    if (code >= 0 &&
	(code = param_get_cie_render1(pcrd, (gs_param_list *) & list,
				      gs_currentdevice(igs))) >= 0
	) {
	code = gs_setcolorrendering(igs, pcrd);
    }
    if (pcrd)
	rc_decrement(pcrd, ".setcrd");	/* build sets rc = 1 */
    iparam_list_release(&list);
    if (code < 0)
	return code;
    pop(1);
    return 0;
}

#endif

/* ------ Initialization procedure ------ */

const op_def zcrd_l2_op_defs[] =
{
    op_def_begin_level2(),
    {"0currentcolorrendering", zcurrentcolorrendering},
    {"2.setcolorrendering1", zsetcolorrendering1},
    {"1.buildcolorrendering1", zbuildcolorrendering1},
		/* Internal "operators" */
    {"1%cie_render_finish", cie_cache_render_finish},
    {"3%cie_exec_tpqr", cie_exec_tpqr},
    {"2%cie_post_exec_tpqr", cie_post_exec_tpqr},
    {"1%cie_tpqr_finish", cie_tpqr_finish},
		/* Testing */
#ifdef TEST
    {"0.currentcrd", zcurrentcrd},
    {"1.setcrd", zsetcrd},
#endif
    op_def_end(0)
};
