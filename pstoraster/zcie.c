/* Copyright (C) 1992, 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: zcie.c,v 1.2 2000/03/08 23:15:32 mike Exp $ */
/* CIE color operators */
#include "math_.h"
#include "memory_.h"
#include "ghost.h"
#include "oper.h"
#include "gsstruct.h"
#include "gxcspace.h"		/* gscolor2.h requires gscspace.h */
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

/* CIE color dictionaries are so complex that */
/* we handle the CIE case of setcolorspace separately here. */

/* Empty procedures */
static const ref empty_procs[4] =
{
    empty_ref_data(t_array, a_readonly | a_executable),
    empty_ref_data(t_array, a_readonly | a_executable),
    empty_ref_data(t_array, a_readonly | a_executable),
    empty_ref_data(t_array, a_readonly | a_executable)
};

/* Redefined CIE color space installers that load the cache. */
extern cs_proc_install_cspace(gx_install_CIEDEFG);
extern cs_proc_install_cspace(gx_install_CIEDEF);
extern cs_proc_install_cspace(gx_install_CIEABC);
extern cs_proc_install_cspace(gx_install_CIEA);
private cs_proc_install_cspace(cs_install_zCIEDEFG);
private cs_proc_install_cspace(cs_install_zCIEDEF);
private cs_proc_install_cspace(cs_install_zCIEABC);
private cs_proc_install_cspace(cs_install_zCIEA);

/* ------ Parameter extraction utilities ------ */

/* Get a range array parameter from a dictionary. */
/* We know that count <= 4. */
int
dict_ranges_param(const ref * pdref, const char *kstr, int count,
		  gs_range * prange)
{
    int code = dict_float_array_param(pdref, kstr, count * 2,
				      (float *)prange, NULL);

    if (code < 0)
	return code;
    else if (code == 0)
	memcpy(prange, Range4_default.ranges, count * sizeof(gs_range));
    else if (code != count * 2)
	return_error(e_rangecheck);
    return 0;
}

/* Get an array of procedures from a dictionary. */
/* We know count <= countof(empty_procs). */
int
dict_proc_array_param(const ref * pdict, const char *kstr,
		      uint count, ref * pparray)
{
    ref *pvalue;

    if (dict_find_string(pdict, kstr, &pvalue) > 0) {
	uint i;

	check_array_only(*pvalue);
	if (r_size(pvalue) != count)
	    return_error(e_rangecheck);
	for (i = 0; i < count; i++) {
	    ref proc;

	    array_get(pvalue, (long)i, &proc);
	    check_proc_only(proc);
	}
	*pparray = *pvalue;
    } else
	make_const_array(pparray, a_readonly | avm_foreign,
			 count, &empty_procs[0]);
    return 0;
}

/* Get WhitePoint and BlackPoint values. */
int
cie_points_param(const ref * pdref, gs_cie_wb * pwb)
{
    int code;

    if ((code = dict_float_array_param(pdref, "WhitePoint", 3, (float *)&pwb->WhitePoint, NULL)) != 3 ||
	(code = dict_float_array_param(pdref, "BlackPoint", 3, (float *)&pwb->BlackPoint, (const float *)&BlackPoint_default)) != 3
	)
	return (code < 0 ? code : gs_note_error(e_rangecheck));
    if (pwb->WhitePoint.u <= 0 ||
	pwb->WhitePoint.v != 1 ||
	pwb->WhitePoint.w <= 0 ||
	pwb->BlackPoint.u < 0 ||
	pwb->BlackPoint.v < 0 ||
	pwb->BlackPoint.w < 0
	)
	return_error(e_rangecheck);
    return 0;
}

/* Process a 3- or 4-dimensional lookup table from a dictionary. */
/* The caller has set pclt->n and pclt->m. */
/* ptref is known to be a readable array of size at least n+1. */
private int cie_3d_table_param(P4(const ref * ptable, uint count, uint nbytes,
				  gs_const_string * strings));
int
cie_table_param(const ref * ptref, gx_color_lookup_table * pclt,
		gs_memory_t * mem)
{
    int n = pclt->n, m = pclt->m;
    const ref *pta = ptref->value.const_refs;
    int i;
    uint nbytes;
    int code;
    gs_const_string *table;

    for (i = 0; i < n; ++i) {
	check_type_only(pta[i], t_integer);
	if (pta[i].value.intval <= 1 || pta[i].value.intval > max_ushort)
	    return_error(e_rangecheck);
	pclt->dims[i] = (int)pta[i].value.intval;
    }
    nbytes = m * pclt->dims[n - 2] * pclt->dims[n - 1];
    if (n == 3) {
	/* gs_alloc_byte_array is ****** WRONG ****** */
	table =
	    (gs_const_string *) gs_alloc_byte_array(mem, pclt->dims[0],
						    sizeof(gs_const_string),
						    "cie_table_param");
	if (table == 0)
	    return_error(e_VMerror);
	code = cie_3d_table_param(pta + 3, pclt->dims[0], nbytes, table);
    } else {			/* n == 4 */
	int d0 = pclt->dims[0], d1 = pclt->dims[1];
	uint ntables = d0 * d1;
	const ref *psuba;

	check_read_type(pta[4], t_array);
	if (r_size(pta + 4) != d0)
	    return_error(e_rangecheck);
	/* gs_alloc_byte_array is ****** WRONG ****** */
	table =
	    (gs_const_string *) gs_alloc_byte_array(mem, ntables,
						    sizeof(gs_const_string),
						    "cie_table_param");
	if (table == 0)
	    return_error(e_VMerror);
	psuba = pta[4].value.const_refs;
	for (i = 0; i < d0; ++i) {
	    code = cie_3d_table_param(psuba + i, d1, nbytes, table + d1 * i);
	    if (code < 0)
		break;
	}
    }
    if (code < 0) {
	gs_free_object(mem, table, "cie_table_param");
	return code;
    }
    pclt->table = table;
    return 0;
}
private int
cie_3d_table_param(const ref * ptable, uint count, uint nbytes,
		   gs_const_string * strings)
{
    const ref *rstrings;
    uint i;

    check_read_type(*ptable, t_array);
    if (r_size(ptable) != count)
	return_error(e_rangecheck);
    rstrings = ptable->value.const_refs;
    for (i = 0; i < count; ++i) {
	const ref *const prt2 = rstrings + i;

	check_read_type(*prt2, t_string);
	if (r_size(prt2) != nbytes)
	    return_error(e_rangecheck);
	strings[i].data = prt2->value.const_bytes;
	strings[i].size = nbytes;
    }
    return 0;
}

/* ------ CIE setcolorspace ------ */

/* Common code for the CIEBased* cases of setcolorspace. */
private int
cie_lmnp_param(const ref * pdref, gs_cie_common * pcie, ref_cie_procs * pcprocs)
{
    int code;

    if ((code = dict_range3_param(pdref, "RangeLMN", &pcie->RangeLMN)) < 0 ||
    (code = dict_proc3_param(pdref, "DecodeLMN", &pcprocs->DecodeLMN)) < 0 ||
	(code = dict_matrix3_param(pdref, "MatrixLMN", &pcie->MatrixLMN)) != matrix3_ok ||
	(code = cie_points_param(pdref, &pcie->points)) < 0
	)
	return (code < 0 ? code : gs_note_error(e_rangecheck));
    pcie->DecodeLMN = DecodeLMN_default;
    return 0;
}

/* Common code for the CIEBasedABC/DEF[G] cases of setcolorspace. */
private int
cie_abc_param(const ref * pdref, gs_cie_abc * pcie, ref_cie_procs * pcprocs)
{
    int code;

    if ((code = dict_range3_param(pdref, "RangeABC", &pcie->RangeABC)) < 0 ||
	(code = dict_proc3_param(pdref, "DecodeABC", &pcprocs->Decode.ABC)) < 0 ||
	(code = dict_matrix3_param(pdref, "MatrixABC", &pcie->MatrixABC)) != matrix3_ok ||
	(code = cie_lmnp_param(pdref, &pcie->common, pcprocs)) < 0
	)
	return (code < 0 ? code : gs_note_error(e_rangecheck));
    pcie->DecodeABC = DecodeABC_default;
    return 0;
}

/* Finish setting a CIE space. */
private int
set_cie_finish(os_ptr op, gs_color_space * pcs, const ref_cie_procs * pcprocs)
{
    ref_colorspace cspace_old;
    uint edepth = ref_stack_count(&e_stack);
    int code;

    /* The color space installation procedure may refer to */
    /* istate->colorspace.procs. */
    cspace_old = istate->colorspace;
    istate->colorspace.procs.cie = *pcprocs;
    code = gs_setcolorspace(igs, pcs);
    /* Delete the extra reference to the parameter tables. */
    gs_cspace_release(pcs);
    /* Free the top-level object, which was copied by gs_setcolorspace. */
    gs_free_object(gs_state_memory(igs), pcs, "set_cie_finish");
    if (code < 0) {
	istate->colorspace = cspace_old;
	ref_stack_pop_to(&e_stack, edepth);
	return code;
    }
    pop(1);
    return (ref_stack_count(&e_stack) == edepth ? 0 : o_push_estack);	/* installation will load the caches */
}

/* <dict> .setciedefgspace - */
private int
zsetciedefgspace(register os_ptr op)
{
    gs_memory_t *mem = gs_state_memory(igs);
    gs_color_space *pcs;
    ref_color_procs procs;
    gs_cie_defg *pcie;
    int code;
    ref *ptref;

    check_type(*op, t_dictionary);
    check_dict_read(*op);
    if ((code = dict_find_string(op, "Table", &ptref)) <= 0)
	return (code < 0 ? code : gs_note_error(e_rangecheck));
    check_read_type(*ptref, t_array);
    if (r_size(ptref) != 5)
	return_error(e_rangecheck);
    procs = istate->colorspace.procs;
    code = gs_cspace_build_CIEDEFG(&pcs, NULL, mem);
    if (code < 0)
	return code;
    pcie = pcs->params.defg;
    pcie->common.install_cspace = cs_install_zCIEDEFG;
    pcie->Table.n = 4;
    pcie->Table.m = 3;
    if ((code = dict_ranges_param(op, "RangeDEFG", 4, pcie->RangeDEFG.ranges)) < 0 ||
	(code = dict_proc_array_param(op, "DecodeDEFG", 4, &procs.cie.PreDecode.DEFG)) < 0 ||
	(code = dict_ranges_param(op, "RangeHIJK", 4, pcie->RangeHIJK.ranges)) < 0 ||
	(code = cie_table_param(ptref, &pcie->Table, mem)) < 0 ||
	(code = cie_abc_param(op, (gs_cie_abc *) pcie, &procs.cie)) < 0
	) {
	gs_cspace_release(pcs);
	gs_free_object(mem, pcs, "setcolorspace(CIEBasedDEFG)");
	return code;
    }
    return set_cie_finish(op, pcs, &procs.cie);
}

/* <dict> .setciedefspace - */
private int
zsetciedefspace(register os_ptr op)
{
    gs_memory_t *mem = gs_state_memory(igs);
    gs_color_space *pcs;
    ref_color_procs procs;
    gs_cie_def *pcie;
    int code;
    ref *ptref;

    check_type(*op, t_dictionary);
    check_dict_read(*op);
    if ((code = dict_find_string(op, "Table", &ptref)) <= 0)
	return (code < 0 ? code : gs_note_error(e_rangecheck));
    check_read_type(*ptref, t_array);
    if (r_size(ptref) != 4)
	return_error(e_rangecheck);
    procs = istate->colorspace.procs;
    code = gs_cspace_build_CIEDEF(&pcs, NULL, mem);
    if (code < 0)
	return code;
    pcie = pcs->params.def;
    pcie->common.install_cspace = cs_install_zCIEDEF;
    pcie->Table.n = 3;
    pcie->Table.m = 3;
    if ((code = dict_range3_param(op, "RangeDEF", &pcie->RangeDEF)) < 0 ||
	(code = dict_proc3_param(op, "DecodeDEF", &procs.cie.PreDecode.DEF)) < 0 ||
	(code = dict_range3_param(op, "RangeHIJ", &pcie->RangeHIJ)) < 0 ||
	(code = cie_table_param(ptref, &pcie->Table, mem)) < 0 ||
	(code = cie_abc_param(op, (gs_cie_abc *) pcie, &procs.cie)) < 0
	) {
	gs_cspace_release(pcs);
	gs_free_object(mem, pcs, "setcolorspace(CIEBasedDEF)");
	return code;
    }
    return set_cie_finish(op, pcs, &procs.cie);
}

/* <dict> .setcieabcspace - */
private int
zsetcieabcspace(register os_ptr op)
{
    gs_memory_t *mem = gs_state_memory(igs);
    gs_color_space *pcs;
    ref_color_procs procs;
    gs_cie_abc *pcie;
    int code;

    check_type(*op, t_dictionary);
    check_dict_read(*op);
    procs = istate->colorspace.procs;
    code = gs_cspace_build_CIEABC(&pcs, NULL, mem);
    if (code < 0)
	return code;
    pcie = pcs->params.abc;
    pcie->common.install_cspace = cs_install_zCIEABC;
    code = cie_abc_param(op, pcie, &procs.cie);
    if (code < 0) {
	gs_cspace_release(pcs);
	gs_free_object(mem, pcs, "setcolorspace(CIEBasedABC)");
	return code;
    }
    return set_cie_finish(op, pcs, &procs.cie);
}

/* <dict> .setcieaspace - */
private int
zsetcieaspace(register os_ptr op)
{
    gs_memory_t *mem = gs_state_memory(igs);
    gs_color_space *pcs;
    ref_color_procs procs;
    gs_cie_a *pcie;
    int code;

    check_type(*op, t_dictionary);
    check_dict_read(*op);
    procs = istate->colorspace.procs;
    if ((code = dict_proc_param(op, "DecodeA", &procs.cie.Decode.A, true)) < 0)
	return code;
    code = gs_cspace_build_CIEA(&pcs, NULL, mem);
    if (code < 0)
	return code;
    pcie = pcs->params.a;
    pcie->common.install_cspace = cs_install_zCIEA;
    if ((code = dict_float_array_param(op, "RangeA", 2, (float *)&pcie->RangeA, (const float *)&RangeA_default)) != 2 ||
	(code = dict_float_array_param(op, "MatrixA", 3, (float *)&pcie->MatrixA, (const float *)&MatrixA_default)) != 3 ||
	(code = cie_lmnp_param(op, &pcie->common, &procs.cie)) < 0
	) {
	gs_cspace_release(pcs);
	gs_free_object(mem, pcs, "setcolorspace(CIEBasedA)");
	return code;
    }
    pcie->DecodeA = DecodeA_default;
    return set_cie_finish(op, pcs, &procs.cie);
}

/* ------ Install a CIE-based color space. ------ */

/* Forward references */
private int cache_common(P4(gs_cie_common *, const ref_cie_procs *,
			    void *, gs_ref_memory_t *));
private int cache_abc_common(P4(gs_cie_abc *, const ref_cie_procs *,
				void *, gs_ref_memory_t *));

private int cie_defg_finish(P1(os_ptr));
private int
cs_install_zCIEDEFG(gs_color_space * pcs, gs_state * pgs)
{
    es_ptr ep = esp;
    gs_cie_defg *pcie = pcs->params.defg;
    gs_ref_memory_t *imem = (gs_ref_memory_t *) gs_state_memory(pgs);
    const int_gstate *pigs = gs_int_gstate(pgs);
    const ref_cie_procs *pcprocs = &pigs->colorspace.procs.cie;
    int code = gx_install_CIEDEFG(pcs, pgs);	/* base routine */

    if (code < 0 ||
	(code = cie_cache_joint(&pigs->colorrendering.procs, pgs)) < 0 ||	/* do this last */
	(code = cie_cache_push_finish(cie_defg_finish, imem, pcie)) < 0 ||
	(code = cie_prepare_cache4(&pcie->RangeDEFG,
				   pcprocs->PreDecode.DEFG.value.const_refs,
				   &pcie->caches_defg.DecodeDEFG[0],
				   pcie, imem, "Decode.DEFG")) < 0 ||
	(code = cache_abc_common((gs_cie_abc *)pcie, pcprocs, pcie, imem)) < 0
	) {
	esp = ep;
	return code;
    }
    return o_push_estack;
}
private int
cie_defg_finish(os_ptr op)
{
    gs_cie_defg_complete(r_ptr(op, gs_cie_defg));
    pop(1);
    return 0;
}

private int cie_def_finish(P1(os_ptr));
private int
cs_install_zCIEDEF(gs_color_space * pcs, gs_state * pgs)
{
    es_ptr ep = esp;
    gs_cie_def *pcie = pcs->params.def;
    gs_ref_memory_t *imem = (gs_ref_memory_t *) gs_state_memory(pgs);
    const int_gstate *pigs = gs_int_gstate(pgs);
    const ref_cie_procs *pcprocs = &pigs->colorspace.procs.cie;
    int code = gx_install_CIEDEF(pcs, pgs);	/* base routine */

    if (code < 0 ||
	(code = cie_cache_joint(&pigs->colorrendering.procs, pgs)) < 0 ||	/* do this last */
	(code = cie_cache_push_finish(cie_def_finish, imem, pcie)) < 0 ||
	(code = cie_prepare_cache3(&pcie->RangeDEF,
				   pcprocs->PreDecode.DEF.value.const_refs,
				   &pcie->caches_def.DecodeDEF[0],
				   pcie, imem, "Decode.DEF")) < 0 ||
	(code = cache_abc_common((gs_cie_abc *)pcie, pcprocs, pcie, imem)) < 0
	) {
	esp = ep;
	return code;
    }
    return o_push_estack;
}
private int
cie_def_finish(os_ptr op)
{
    gs_cie_def_complete(r_ptr(op, gs_cie_def));
    pop(1);
    return 0;
}

private int cie_abc_finish(P1(os_ptr));
private int
cs_install_zCIEABC(gs_color_space * pcs, gs_state * pgs)
{
    es_ptr ep = esp;
    gs_cie_abc *pcie = pcs->params.abc;
    gs_ref_memory_t *imem = (gs_ref_memory_t *) gs_state_memory(pgs);
    const int_gstate *pigs = gs_int_gstate(pgs);
    const ref_cie_procs *pcprocs = &pigs->colorspace.procs.cie;
    int code = gx_install_CIEABC(pcs, pgs);	/* base routine */

    if (code < 0 ||
	(code = cie_cache_joint(&pigs->colorrendering.procs, pgs)) < 0 ||	/* do this last */
	(code = cie_cache_push_finish(cie_abc_finish, imem, pcie)) < 0 ||
	(code = cache_abc_common(pcie, pcprocs, pcie, imem)) < 0
	) {
	esp = ep;
	return code;
    }
    return o_push_estack;
}
private int
cie_abc_finish(os_ptr op)
{
    gs_cie_abc_complete(r_ptr(op, gs_cie_abc));
    pop(1);
    return 0;
}

private int cie_a_finish(P1(os_ptr));
private int
cs_install_zCIEA(gs_color_space * pcs, gs_state * pgs)
{
    es_ptr ep = esp;
    gs_cie_a *pcie = pcs->params.a;
    gs_ref_memory_t *imem = (gs_ref_memory_t *) gs_state_memory(pgs);
    const int_gstate *pigs = gs_int_gstate(pgs);
    const ref_cie_procs *pcprocs = &pigs->colorspace.procs.cie;
    int code = gx_install_CIEA(pcs, pgs);	/* base routine */

    if (code < 0 ||
	(code = cie_cache_joint(&pigs->colorrendering.procs, pgs)) < 0 ||	/* do this last */
	(code = cie_cache_push_finish(cie_a_finish, imem, pcie)) < 0 ||
	(code = cie_prepare_cache(&pcie->RangeA, &pcprocs->Decode.A, &pcie->caches.DecodeA.floats, pcie, imem, "Decode.A")) < 0 ||
	(code = cache_common(&pcie->common, pcprocs, pcie, imem)) < 0
	) {
	esp = ep;
	return code;
    }
    return o_push_estack;
}
private int
cie_a_finish(os_ptr op)
{
    gs_cie_a_complete(r_ptr(op, gs_cie_a));
    pop(1);
    return 0;
}

/* Common cache code */

private int
cache_abc_common(gs_cie_abc * pcie, const ref_cie_procs * pcprocs,
		 void *container, gs_ref_memory_t * imem)
{
    int code =
	cie_prepare_cache3(&pcie->RangeABC,
			   pcprocs->Decode.ABC.value.const_refs,
			   &pcie->caches.DecodeABC[0], pcie, imem,
			   "Decode.ABC");

    return (code < 0 ? code :
	    cache_common(&pcie->common, pcprocs, pcie, imem));
}

private int
cache_common(gs_cie_common * pcie, const ref_cie_procs * pcprocs,
	     void *container, gs_ref_memory_t * imem)
{
    return cie_prepare_cache3(&pcie->RangeLMN,
			      pcprocs->DecodeLMN.value.const_refs,
			      &pcie->caches.DecodeLMN[0], container, imem,
			      "Decode.LMN");
}

/* ------ Internal routines ------ */

/* Prepare to cache the values for one or more procedures. */
private int cie_cache_finish1(P1(os_ptr));
private int cie_cache_finish(P1(os_ptr));
int
cie_prepare_cache(const gs_range * domain, const ref * proc,
		  cie_cache_floats * pcache, void *container,
		  gs_ref_memory_t * imem, client_name_t cname)
{
    int space = imemory_space(imem);
    gs_for_loop_params flp;
    es_ptr ep;

    gs_cie_cache_init(&pcache->params, &flp, domain, cname);
    pcache->params.is_identity = r_size(proc) == 0;
    /*
     * If a matrix was singular, it is possible that flp.step = 0.
     * In this case, flp.limit = flp.init as well.
     * Execute the procedure once, and replicate the result.
     */
    if (flp.step == 0) {
	check_estack(5);
	ep = esp;
	make_real(ep + 5, flp.init);
	ep[4] = *proc;
	make_op_estack(ep + 3, cie_cache_finish1);
	esp += 5;
    } else {
	check_estack(9);
	ep = esp;
	make_real(ep + 9, flp.init);
	make_real(ep + 8, flp.step);
	make_real(ep + 7, flp.limit);
	ep[6] = *proc;
	r_clear_attrs(ep + 6, a_executable);
	make_op_estack(ep + 5, zcvx);
	make_op_estack(ep + 4, zfor);
	make_op_estack(ep + 3, cie_cache_finish);
	esp += 9;
    }
    /*
     * The caches are embedded in the middle of other
     * structures, so we represent the pointer to the cache
     * as a pointer to the container plus an offset.
     */
    make_int(ep + 2, (char *)pcache - (char *)container);
    make_struct(ep + 1, space, container);
    return o_push_estack;
}
/* Note that pc3 may be 0, indicating that there are only 3 caches to load. */
int
cie_prepare_caches_4(const gs_range * domains, const ref * procs,
		     cie_cache_floats * pc0, cie_cache_floats * pc1,
		     cie_cache_floats * pc2, cie_cache_floats * pc3,
		     void *container,
		     gs_ref_memory_t * imem, client_name_t cname)
{
    cie_cache_floats *pcn[4];
    int i, n, code = 0;

    pcn[0] = pc0, pcn[1] = pc1, pcn[2] = pc2;
    if (pc3 == 0)
	n = 3;
    else
	pcn[3] = pc3, n = 4;
    for (i = 0; i < n && code >= 0; ++i)
	code = cie_prepare_cache(domains + i, procs + i, pcn[i],
				 container, imem, cname);
    return code;
}

/* Store the result of caching one procedure. */
private int
cie_cache_finish_store(os_ptr op, bool replicate)
{
    cie_cache_floats *pcache;
    int code;

    check_esp(2);
    /* See above for the container + offset representation of */
    /* the pointer to the cache. */
    pcache = (cie_cache_floats *) (r_ptr(esp - 1, char) + esp->value.intval);

    if_debug3('c', "[c]cache 0x%lx base=%g, factor=%g:\n",
	      (ulong) pcache, pcache->params.base, pcache->params.factor);
    if (replicate ||
	(code = float_params(op, gx_cie_cache_size, &pcache->values[0])) < 0
	) {
	/* We might have underflowed the current stack block. */
	/* Handle the parameters one-by-one. */
	uint i;

	for (i = 0; i < gx_cie_cache_size; i++) {
	    code = float_param(ref_stack_index(&o_stack,
			       (replicate ? 0 : gx_cie_cache_size - 1 - i)),
			       &pcache->values[i]);
	    if (code < 0)
		return code;
	}
    }
#ifdef DEBUG
    if (gs_debug_c('c')) {
	int i;

	for (i = 0; i < gx_cie_cache_size; i += 4)
	    dlprintf5("[c]  cache[%3d]=%g, %g, %g, %g\n", i,
		      pcache->values[i], pcache->values[i + 1],
		      pcache->values[i + 2], pcache->values[i + 3]);
    }
#endif
    ref_stack_pop(&o_stack, (replicate ? 1 : gx_cie_cache_size));
    esp -= 2;			/* pop pointer to cache */
    return o_pop_estack;
}
private int
cie_cache_finish(os_ptr op)
{
    return cie_cache_finish_store(op, false);
}
private int
cie_cache_finish1(os_ptr op)
{
    return cie_cache_finish_store(op, true);
}

/* Push a finishing procedure on the e-stack. */
/* ptr will be the top element of the o-stack. */
int
cie_cache_push_finish(int (*finish_proc) (P1(os_ptr)), gs_ref_memory_t * imem,
		      void *data)
{
    check_estack(2);
    push_op_estack(finish_proc);
    ++esp;
    make_struct(esp, imemory_space(imem), data);
    return o_push_estack;
}

/* ------ Initialization procedure ------ */

const op_def zcie_l2_op_defs[] =
{
    op_def_begin_level2(),
    {"1.setcieaspace", zsetcieaspace},
    {"1.setcieabcspace", zsetcieabcspace},
    {"1.setciedefspace", zsetciedefspace},
    {"1.setciedefgspace", zsetciedefgspace},
		/* Internal operators */
    {"1%cie_defg_finish", cie_defg_finish},
    {"1%cie_def_finish", cie_def_finish},
    {"1%cie_abc_finish", cie_abc_finish},
    {"1%cie_a_finish", cie_a_finish},
    {"0%cie_cache_finish", cie_cache_finish},
    {"1%cie_cache_finish1", cie_cache_finish1},
    op_def_end(0)
};
