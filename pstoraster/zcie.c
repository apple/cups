/* Copyright (C) 1992, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* zcie.c */
/* CIE color operators */
#include "math_.h"
#include "memory_.h"
#include "ghost.h"
#include "errors.h"
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

/* Forward references */
private int cache_common(P4(gs_cie_common *, const ref_cie_procs *,
  void *, const gs_state *));

/* Allocator structure types for CIE structures */
private_st_cie_defg();
private_st_cie_def();
private_st_cie_abc();
private_st_cie_a();

/* Empty procedures */
static ref empty_procs[4];

/* Original CIE color space types. */
/* We use CIExxx rather than CIEBasedxxx in some places because */
/* gcc under VMS only retains 23 characters of procedure names, */
/* and DEC C truncates all identifiers at 31 characters. */
extern const gs_color_space_type
	gs_color_space_type_CIEDEFG,
	gs_color_space_type_CIEDEF,
	gs_color_space_type_CIEABC,
	gs_color_space_type_CIEA;
/* Redefined CIE color space types (that load the cache when installed) */
gs_color_space_type
	cs_type_zCIEDEFG,
	cs_type_zCIEDEF,
	cs_type_zCIEABC,
	cs_type_zCIEA;
private cs_proc_install_cspace(cs_install_zCIEDEFG);
private cs_proc_install_cspace(cs_install_zCIEDEF);
private cs_proc_install_cspace(cs_install_zCIEABC);
private cs_proc_install_cspace(cs_install_zCIEA);

/* Initialization */
private void
zcie_init(void)
{
	/* Make the null (default) transformation procedures. */
	make_empty_const_array(&empty_procs[0], a_readonly + a_executable);
	make_empty_const_array(&empty_procs[1], a_readonly + a_executable);
	make_empty_const_array(&empty_procs[2], a_readonly + a_executable);
	make_empty_const_array(&empty_procs[3], a_readonly + a_executable);

	/* Create the modified color space types. */
	cs_type_zCIEDEFG = gs_color_space_type_CIEDEFG;
	cs_type_zCIEDEFG.install_cspace = cs_install_zCIEDEFG;
	cs_type_zCIEDEF = gs_color_space_type_CIEDEF;
	cs_type_zCIEDEF.install_cspace = cs_install_zCIEDEF;
	cs_type_zCIEABC = gs_color_space_type_CIEABC;
	cs_type_zCIEABC.install_cspace = cs_install_zCIEABC;
	cs_type_zCIEA = gs_color_space_type_CIEA;
	cs_type_zCIEA.install_cspace = cs_install_zCIEA;

}

/* ------ Parameter extraction utilities ------ */

/* Get a range array parameter from a dictionary. */
/* We know that count <= 4. */
int
dict_ranges_param(const ref *pdref, const char _ds *kstr, int count,
  gs_range *prange)
{	int code = dict_float_array_param(pdref, kstr, count * 2,
					  (float *)prange, NULL);
	if ( code < 0 )
	  return code;
	else if ( code == 0 )
	  memcpy(prange, Range4_default.ranges, count * sizeof(gs_range));
	else if ( code != count * 2 )
	  return_error(e_rangecheck);
	return 0;
}

/* Get an array of procedures from a dictionary. */
/* We know count <= countof(empty_procs). */
int
dict_proc_array_param(const ref *pdict, const char _ds *kstr,
  uint count, ref *pparray)
{	ref *pvalue;
	if ( dict_find_string(pdict, kstr, &pvalue) > 0 )
	{	uint i;
		check_array_only(*pvalue);
		if ( r_size(pvalue) != count )
		  return_error(e_rangecheck);
		for ( i = 0; i < count; i++ )
		{	ref proc;
			array_get(pvalue, (long)i, &proc);
			check_proc_only(proc);
		}
		*pparray = *pvalue;
	}
	else
		make_const_array(pparray, a_readonly | avm_foreign,
				 count, &empty_procs[0]);
	return 0;
}
		
/* Get WhitePoint and BlackPoint values. */
int
cie_points_param(const ref *pdref, gs_cie_wb *pwb)
{	int code;
	if ( (code = dict_float_array_param(pdref, "WhitePoint", 3, (float *)&pwb->WhitePoint, NULL)) != 3 ||
	     (code = dict_float_array_param(pdref, "BlackPoint", 3, (float *)&pwb->BlackPoint, (const float *)&BlackPoint_default)) != 3
	   )
	  return (code < 0 ? code : gs_note_error(e_rangecheck));
	if ( pwb->WhitePoint.u <= 0 ||
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
private int cie_3d_table_param(P4(const ref *ptable, uint count, uint nbytes,
  gs_const_string *strings));
int
cie_table_param(const ref *ptref, gx_color_lookup_table *pclt,
  gs_memory_t *mem)
{	int n = pclt->n, m = pclt->m;
	const ref *pta = ptref->value.const_refs;
	int i;
	uint nbytes;
	int code;
	gs_const_string *table;

	for ( i = 0; i < n; ++i )
	  { check_type_only(pta[i], t_integer);
	    if ( pta[i].value.intval <= 1 || pta[i].value.intval > max_ushort )
	      return_error(e_rangecheck);
	    pclt->dims[i] = (int)pta[i].value.intval;
	  }
	nbytes = m * pclt->dims[n-2] * pclt->dims[n-1];
	if ( n == 3 )
	  { /* gs_alloc_byte_array is ****** WRONG ****** */
	    table =
	      (gs_const_string *)gs_alloc_byte_array(mem, pclt->dims[0],
						     sizeof(gs_const_string),
						     "cie_table_param");
	    if ( table == 0 )
	      return_error(e_VMerror);
	    code = cie_3d_table_param(pta + 3, pclt->dims[0], nbytes,
				      table);
	  }
	else			/* n == 4 */
	  { int d0 = pclt->dims[0], d1 = pclt->dims[1];
	    uint ntables = d0 * d1;
	    const ref *psuba;
	    check_read_type(pta[4], t_array);
	    if ( r_size(pta + 4) != d0 )
	      return_error(e_rangecheck);
	    /* gs_alloc_byte_array is ****** WRONG ****** */
	    table =
	      (gs_const_string *)gs_alloc_byte_array(mem, ntables,
						     sizeof(gs_const_string),
						     "cie_table_param");
	    if ( table == 0 )
	      return_error(e_VMerror);
	    psuba = pta[4].value.const_refs;
	    for ( i = 0; i < d0; ++i )
	      { code = cie_3d_table_param(psuba + i, d1, nbytes,
					  table + d1 * i);
		if ( code < 0 )
		  break;
	      }
	  }
	if ( code < 0 )
	  { gs_free_object(mem, table, "cie_table_param");
	    return code;
	  }
	pclt->table = table;
	return 0;
}
private int
cie_3d_table_param(const ref *ptable, uint count, uint nbytes,
  gs_const_string *strings)
{	const ref *rstrings;
	uint i;

	check_read_type(*ptable, t_array);
	if ( r_size(ptable) != count )
	  return_error(e_rangecheck);
	rstrings = ptable->value.const_refs;
	for ( i = 0; i < count; ++i )
	  { const ref *prt2 = rstrings + i;
	    check_read_type(*prt2, t_string);
	    if ( r_size(prt2) != nbytes )
	      return_error(e_rangecheck);
	    strings[i].data = rstrings[i].value.const_bytes;
	    strings[i].size = nbytes;
	  }
	return 0;
}

/* ------ CIE setcolorspace ------ */

/* Common code for the CIEBased* cases of setcolorspace. */
private int
cie_lmnp_param(const ref *pdref, gs_cie_common *pcie, ref_cie_procs *pcprocs)
{	int code;
	if ( (code = dict_range3_param(pdref, "RangeLMN", &pcie->RangeLMN)) < 0 ||
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
cie_abc_param(const ref *pdref, gs_cie_abc_common *pcie,
  ref_cie_procs *pcprocs)
{	int code;
	if ( (code = dict_range3_param(pdref, "RangeABC", &pcie->RangeABC)) < 0 ||
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
set_cie_finish(os_ptr op, gs_color_space *pcs, const ref_cie_procs *pcprocs)
{	ref_colorspace cspace_old;
	uint edepth = ref_stack_count(&e_stack);
	int code;

	/* The color space installation procedure may refer to */
	/* istate->colorspace.procs. */
	cspace_old = istate->colorspace;
	istate->colorspace.procs.cie = *pcprocs;
	code = gs_setcolorspace(igs, pcs);
	if ( code < 0 )
	{	istate->colorspace = cspace_old;
		ref_stack_pop_to(&e_stack, edepth);
		return code;
	}
	pop(1);
	return (ref_stack_count(&e_stack) == edepth ? 0 : o_push_estack);  /* installation will load the caches */
}

#ifdef NEW_CIE			/**************** ****************/

/* <dict> .setciedefgspace - */
private int
zsetciedefgspace(register os_ptr op)
{	gs_memory_t *mem = gs_state_memory(igs);
	gs_color_space cs;
	ref_color_procs procs;
	gs_cie_defg *pcie;
	int code;
	ref *ptref;

	check_type(*op, t_dictionary);
	check_dict_read(*op);
	procs = istate->colorspace.procs;
	rc_alloc_struct_0(pcie, gs_cie_defg, &st_cie_defg, mem,
			  return_error(e_VMerror),
			  "setcolorspace(CIEBasedDEFG)");
	if ( (code = dict_find_string(op, "Table", &ptref)) <= 0 )
	  return (code < 0 ? code : gs_note_error(e_rangecheck));
	check_read_type(*ptref, t_array);
	if ( r_size(ptref) != 5 )
	  return_error(e_rangecheck);
	pcie->Table.n = 4;
	pcie->Table.m = 3;
	if ( (code = dict_ranges_param(op, "RangeDEFG", 4, pcie->RangeDEFG.ranges)) < 0 ||
	     (code = dict_proc_array_param(op, "DecodeDEFG", 4, &procs.cie.PreDecode.DEFG)) < 0 ||
	     (code = dict_ranges_param(op, "RangeHIJK", 4, pcie->RangeHIJK.ranges)) < 0 ||
	     (code = cie_table_param(ptref, &pcie->Table, mem)) < 0 ||
	     (code = cie_abc_param(op, &pcie->abc, &procs.cie)) < 0
	   )
	{	rc_free_struct(pcie, mem, "setcolorspace(CIEBasedDEFG)");
		return code;
	}
	cs.params.defg = pcie;
	cs.type = &cs_type_zCIEDEFG;
	return set_cie_finish(op, &cs, &procs.cie);
}

/* <dict> .setciedefspace - */
private int
zsetciedefspace(register os_ptr op)
{	gs_memory_t *mem = gs_state_memory(igs);
	gs_color_space cs;
	ref_color_procs procs;
	gs_cie_def *pcie;
	int code;
	ref *ptref;

	check_type(*op, t_dictionary);
	check_dict_read(*op);
	procs = istate->colorspace.procs;
	rc_alloc_struct_0(pcie, gs_cie_def, &st_cie_def, mem,
			  return_error(e_VMerror),
			  "setcolorspace(CIEBasedDEF)");
	if ( (code = dict_find_string(op, "Table", &ptref)) <= 0 )
	  return (code < 0 ? code : gs_note_error(e_rangecheck));
	check_read_type(*ptref, t_array);
	if ( r_size(ptref) != 4 )
	  return_error(e_rangecheck);
	pcie->Table.n = 4;
	pcie->Table.m = 3;
	if ( (code = dict_range3_param(op, "RangeDEF", &pcie->RangeDEF)) < 0 ||
	     (code = dict_proc3_param(op, "DecodeDEF", &procs.cie.PreDecode.DEF)) < 0 ||
	     (code = dict_range3_param(op, "RangeHIJ", &pcie->RangeHIJ)) < 0 ||
	     (code = cie_table_param(ptref, &pcie->Table, mem)) < 0 ||
	     (code = cie_abc_param(op, &pcie->abc, &procs.cie)) < 0
	   )
	{	rc_free_struct(pcie, mem, "setcolorspace(CIEBasedDEF)");
		return code;
	}
	cs.params.def = pcie;
	cs.type = &cs_type_zCIEDEF;
	return set_cie_finish(op, &cs, &procs.cie);
}

#endif /*NEW_CIE*/		/**************** ****************/

/* <dict> .setcieabcspace - */
private int
zsetcieabcspace(register os_ptr op)
{	gs_memory_t *mem = gs_state_memory(igs);
	gs_color_space cs;
	ref_color_procs procs;
	gs_cie_abc *pcie;
	int code;

	check_type(*op, t_dictionary);
	check_dict_read(*op);
	procs = istate->colorspace.procs;
	rc_alloc_struct_0(pcie, gs_cie_abc, &st_cie_abc, mem,
			  return_error(e_VMerror),
			  "setcolorspace(CIEBasedABC)");
	code = cie_abc_param(op, pcie, &procs.cie);
	if ( code < 0 )
	{	rc_free_struct(pcie, mem, "setcolorspace(CIEBasedABC)");
		return code;
	}
	cs.params.abc = pcie;
	cs.type = &cs_type_zCIEABC;
	return set_cie_finish(op, &cs, &procs.cie);
}

/* <dict> .setcieaspace - */
private int
zsetcieaspace(register os_ptr op)
{	gs_memory_t *mem = gs_state_memory(igs);
	gs_color_space cs;
	ref_color_procs procs;
	gs_cie_a *pcie;
	int code;

	check_type(*op, t_dictionary);
	check_dict_read(*op);
	procs = istate->colorspace.procs;
	if ( (code = dict_proc_param(op, "DecodeA", &procs.cie.Decode.A, true)) < 0 )
	  return code;
	rc_alloc_struct_0(pcie, gs_cie_a, &st_cie_a, mem,
			  return_error(e_VMerror),
			  "setcolorspace(CIEBasedA)");
	if ( (code = dict_float_array_param(op, "RangeA", 2, (float *)&pcie->RangeA, (const float *)&RangeA_default)) != 2 ||
	     (code = dict_float_array_param(op, "MatrixA", 3, (float *)&pcie->MatrixA, (const float *)&MatrixA_default)) != 3 ||
	     (code = cie_lmnp_param(op, &pcie->common, &procs.cie)) < 0
	   )
	{	rc_free_struct(pcie, mem, "setcolorspace(CIEBasedA)");
		return (code < 0 ? code : gs_note_error(e_rangecheck));
	}
	pcie->DecodeA = DecodeA_default;
	cs.params.a = pcie;
	cs.type = &cs_type_zCIEA;
	return set_cie_finish(op, &cs, &procs.cie);
}

/* ------ Install a CIE-based color space. ------ */

/* The new CIEBasedDEF[G] spaces aren't really implemented yet.... */
private int
cs_install_zCIEDEFG(gs_color_space *pcs, gs_state *pgs)
{	return_error(e_undefined);
}
private int
cs_install_zCIEDEF(gs_color_space *pcs, gs_state *pgs)
{	return_error(e_undefined);
}

private int cie_abc_finish(P1(os_ptr));
private int
cs_install_zCIEABC(gs_color_space *pcs, gs_state *pgs)
{	es_ptr ep = esp;
	gs_cie_abc *pcie = pcs->params.abc;
	const int_gstate *pigs = gs_int_gstate(pgs);
	const ref_cie_procs *pcprocs = &pigs->colorspace.procs.cie;
	int code =
	  (*gs_color_space_type_CIEABC.install_cspace)(pcs, pgs);	/* former routine */
	if ( code < 0 ||
	     (code = cie_cache_joint(&pigs->colorrendering.procs, pgs)) < 0 ||	/* do this last */
	     (code = cie_cache_push_finish(cie_abc_finish, pgs, pcie)) < 0 ||
#ifdef NEW_CIE
	     (code = cie_prepare_cache3(&pcie->abc.RangeABC, pcprocs->Decode.ABC.value.const_refs, &pcie->abc.caches.DecodeABC[0], pcie, pgs, "Decode.ABC")) < 0 ||
	     (code = cache_common(&pcie->abc.common, pcprocs, pcie, pgs)) < 0
#else
	     (code = cie_prepare_cache3(&pcie->RangeABC, pcprocs->Decode.ABC.value.const_refs, &pcie->caches.DecodeABC[0], pcie, pgs, "Decode.ABC")) < 0 ||
	     (code = cache_common(&pcie->common, pcprocs, pcie, pgs)) < 0
#endif
	   )
	{	esp = ep;
		return code;
	}
	return o_push_estack;
}
private int
cie_abc_finish(os_ptr op)
{	gs_cie_abc_complete(r_ptr(op, gs_cie_abc));
	pop(1);
	return 0;
}

private int cie_a_finish(P1(os_ptr));
private int
cs_install_zCIEA(gs_color_space *pcs, gs_state *pgs)
{	es_ptr ep = esp;
	gs_cie_a *pcie = pcs->params.a;
	const int_gstate *pigs = gs_int_gstate(pgs);
	const ref_cie_procs *pcprocs = &pigs->colorspace.procs.cie;
	int code =
	  (*gs_color_space_type_CIEA.install_cspace)(pcs, pgs);	/* former routine */
	if ( code < 0 ||
	     (code = cie_cache_joint(&pigs->colorrendering.procs, pgs)) < 0 ||	/* do this last */
	     (code = cie_cache_push_finish(cie_a_finish, pgs, pcie)) < 0 ||
	     (code = cie_prepare_cache(&pcie->RangeA, &pcprocs->Decode.A, &pcie->caches.DecodeA.floats, pcie, pgs, "Decode.A")) < 0 ||
	     (code = cache_common(&pcie->common, pcprocs, pcie, pgs)) < 0
	   )
	{	esp = ep;
		return code;
	}
	return o_push_estack;
}
private int
cie_a_finish(os_ptr op)
{	gs_cie_a_complete(r_ptr(op, gs_cie_a));
	pop(1);
	return 0;
}

/* Common cache code */
private int
cache_common(gs_cie_common *pcie, const ref_cie_procs *pcprocs,
  void *container, const gs_state *pgs)
{	return cie_prepare_cache3(&pcie->RangeLMN,
				  pcprocs->DecodeLMN.value.const_refs,
				  &pcie->caches.DecodeLMN[0], container, pgs,
				  "Decode.LMN");
}

/* ------ Internal routines ------ */

/* Prepare to cache the values for one or more procedures. */
private int cie_cache_finish(P1(os_ptr));
int
cie_prepare_cache(const gs_range *domain, const ref *proc,
  cie_cache_floats *pcache, void *container, const gs_state *pgs,
  client_name_t cname)
{	int space = imemory_space((gs_ref_memory_t *)gs_state_memory(pgs));
	gs_for_loop_params flp;
	register es_ptr ep;

	check_estack(9);
	ep = esp;
	gs_cie_cache_init(&pcache->params, &flp, domain, cname);
	pcache->params.is_identity = r_size(proc) == 0;
	make_real(ep + 9, flp.init);
	make_real(ep + 8, flp.step);
	make_real(ep + 7, flp.limit);
	ep[6] = *proc;
	r_clear_attrs(ep + 6, a_executable);
	make_op_estack(ep + 5, zcvx);
	make_op_estack(ep + 4, zfor);
	make_op_estack(ep + 3, cie_cache_finish);
	/*
	 * The caches are embedded in the middle of other
	 * structures, so we represent the pointer to the cache
	 * as a pointer to the container plus an offset.
	 */
	make_int(ep + 2, (char *)pcache - (char *)container);
	make_struct(ep + 1, space, container);
	esp += 9;
	return o_push_estack;
}
private int
cie_prepare_caches(const gs_range *domains, const ref *procs,
  cie_cache_floats **ppc, int count, void *container, const gs_state *pgs,
  client_name_t cname)
{	int i, code = 0;
	for ( i = 0; i < count; ++i )
	  if ( (code = cie_prepare_cache(domains + i, procs + i, ppc[i],
					 container, pgs, cname)) < 0
	     )
	    return code;
	return code;
}
int
cie_prepare_caches_3(const gs_range3 *domains, const ref *procs,
  cie_cache_floats *pc0, cie_cache_floats *pc1, cie_cache_floats *pc2,
  void *container, const gs_state *pgs, client_name_t cname)
{	cie_cache_floats *pc3[3];
	pc3[0] = pc0, pc3[1] = pc1, pc3[2] = pc2;
	return cie_prepare_caches((const gs_range *)domains, procs, pc3, 3,
				  container, pgs, cname);
}

/* Store the result of caching one procedure. */
private int
cie_cache_finish(os_ptr op)
{	cie_cache_floats *pcache;
	int code;
	check_esp(2);
	/* See above for the container + offset representation of */
	/* the pointer to the cache. */
	pcache = (cie_cache_floats *)(r_ptr(esp - 1, char) + esp->value.intval);
	code = num_params(op, gx_cie_cache_size, &pcache->values[0]);
	if_debug3('c', "[c]cache 0x%lx base=%g, factor=%g:\n",
		  (ulong)pcache, pcache->params.base, pcache->params.factor);
	if ( code < 0 )
	  {	/* We might have underflowed the current stack block. */
		/* Handle the parameters one-by-one. */
		uint i;
		for ( i = 0; i < gx_cie_cache_size; i++ )
		  {	code = real_param(ref_stack_index(&o_stack,
						gx_cie_cache_size - 1 - i),
					  &pcache->values[i]);
			if ( code < 0 )
			  return code;
		  }
	  }
#ifdef DEBUG
	if ( gs_debug_c('c') )
	{	int i;
		for ( i = 0; i < gx_cie_cache_size; i += 4 )
		  dprintf5("[c]  cache[%3d]=%g, %g, %g, %g\n", i,
			   pcache->values[i], pcache->values[i + 1],
			   pcache->values[i + 2], pcache->values[i + 3]);
	}
#endif
	ref_stack_pop(&o_stack, gx_cie_cache_size);
	esp -= 2;			/* pop pointer to cache */
	return o_pop_estack;
}

/* Push a finishing procedure on the e-stack. */
/* ptr will be the top element of the o-stack. */
int
cie_cache_push_finish(int (*proc)(P1(os_ptr)), gs_state *pgs, void *ptr)
{	check_estack(2);
	push_op_estack(proc);
	++esp;
	make_struct(esp, imemory_space((gs_ref_memory_t *)gs_state_memory(pgs)), ptr);
	return o_push_estack;
}

/* ------ Initialization procedure ------ */

BEGIN_OP_DEFS(zcie_l2_op_defs) {
		op_def_begin_level2(),
	{"1.setcieaspace", zsetcieaspace},
	{"1.setcieabcspace", zsetcieabcspace},
#ifdef NEW_CIE
	{"1.setciedefspace", zsetciedefspace},
	{"1.setciedefgspace", zsetciedefgspace},
#endif
		/* Internal operators */
	{"1%cie_abc_finish", cie_abc_finish},
	{"1%cie_a_finish", cie_a_finish},
	{"0%cie_cache_finish", cie_cache_finish},
END_OP_DEFS(zcie_init) }
