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

/* gscie.c */
/* CIE color rendering for Ghostscript */
#include "math_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsstruct.h"
#include "gsmatrix.h"			/* for gscolor2.h */
#include "gxcspace.h"
#include "gscolor2.h"			/* for gs_set/currentcolorrendering */
#include "gscie.h"
#include "gxarith.h"
#include "gxdevice.h"			/* for gxcmap.h */
#include "gxcmap.h"
#include "gzstate.h"

/* Forward references */
private void cie_joint_caches_init(P3(gx_cie_joint_caches *,
  const gs_cie_common *, const gs_cie_render *));
private void cie_joint_caches_complete(P3(gx_cie_joint_caches *,
  const gs_cie_common *, const gs_cie_render *));
private void near cie_mult3(P3(const gs_vector3 *, const gs_matrix3 *,
  gs_vector3 *));
private void near cie_matrix_mult3(P3(const gs_matrix3 *, const gs_matrix3 *,
  gs_matrix3 *));
private void near cie_invert3(P2(const gs_matrix3 *, gs_matrix3 *));
private void near cie_matrix_init(P1(gs_matrix3 *));

#define set_restrict_index(i, v, n)\
  if ( (uint)(i = (int)(v)) >= (n) )\
    i = (i < 0 ? 0 : (n) - 1)
#define restrict_index(v, n, itemp)\
  ((uint)(itemp = (int)(v)) >= (n) ?\
   (itemp < 0 ? 0 : (n) - 1) : itemp)

/* Compute a cache index as (vin - base) * factor. */
/* vin, base, factor, and the result are cie_cached_values. */
/* We know that the result doesn't exceed (gx_cie_cache_size - 1) << fbits. */
#define lookup_index(vin, pcache, fbits)\
  ((vin) <= (pcache)->vecs.params.base ? 0 :\
   (vin) >= (pcache)->vecs.params.limit ? (gx_cie_cache_size - 1) << (fbits) :\
   cie_cached_product2int( ((vin) - (pcache)->vecs.params.base),\
			   (pcache)->vecs.params.factor, fbits ))
#define lookup_value(vin, pcache)\
  ((pcache)->vecs.values[lookup_index(vin, pcache, 0)])

#define if_restrict(v, range)\
  if ( (v) < (range).rmin ) v = (range).rmin;\
  else if ( (v) > (range).rmax ) v = (range).rmax

/* Define the template for loading a cache. */
/* If we had parameterized types, or a more flexible type system, */
/* this could be done with a single procedure. */
#define cie_cache_init3(pcache3, plp3, prange3, cname)\
	gs_cie_cache_init(&(pcache3)->floats.params, &(plp3)[0], &(prange3)[0], cname);\
	gs_cie_cache_init(&(pcache3)[1].floats.params, &(plp3)[1], &(prange3)[1], cname);\
	gs_cie_cache_init(&(pcache3)[2].floats.params, &(plp3)[2], &(prange3)[2], cname)
#define CIE_LOAD_CACHE3_BODY(pcache, domains, rprocs, pcie, cname)\
{	int i, j;\
	gs_for_loop_params lp[3];\
	cie_cache_init3(pcache, lp, domains, cname);\
	for ( i = 0; i < gx_cie_cache_size; i++ )\
	  for ( j = 0; j < 3; lp[j].init += lp[j].step, j++ )\
	    pcache[j].floats.values[i] =\
	      (*(rprocs)->procs[j])(lp[j].init, pcie);\
}

/* ================ Color space definition ================ */

/* Allocator structure types */
private_st_joint_caches();
private_st_const_string();
public_st_const_string_element();
#define sptr ((gs_const_string *)vptr)
private ENUM_PTRS_BEGIN(const_string_enum_ptrs) return 0;
	case 0: *pep = (void *)sptr; return ptr_const_string_type;
ENUM_PTRS_END
private RELOC_PTRS_BEGIN(const_string_reloc_ptrs) {
	gs_reloc_const_string(sptr, gcst);
} RELOC_PTRS_END
#undef sptr

/* Define the CIE color space types. */
/* We use CIExxx rather than CIEBasedxxx in some places because */
/* gcc under VMS only retains 23 characters of procedure names, */
/* and DEC C truncates all identifiers at 31 characters. */
private cs_proc_concrete_space(gx_concrete_space_CIE);
cs_declare_procs(private, gx_concretize_CIEDEFG, gx_install_CIEDEFG,
  gx_adjust_cspace_CIEDEFG,
  gx_enum_ptrs_CIEDEFG, gx_reloc_ptrs_CIEDEFG);
cs_declare_procs(private, gx_concretize_CIEDEF, gx_install_CIEDEF,
  gx_adjust_cspace_CIEDEF,
  gx_enum_ptrs_CIEDEF, gx_reloc_ptrs_CIEDEF);
private cs_proc_remap_color(gx_remap_CIEABC);
cs_declare_procs(private, gx_concretize_CIEABC, gx_install_CIEABC,
  gx_adjust_cspace_CIEABC,
  gx_enum_ptrs_CIEABC, gx_reloc_ptrs_CIEABC);
cs_declare_procs(private, gx_concretize_CIEA, gx_install_CIEA,
  gx_adjust_cspace_CIEA,
  gx_enum_ptrs_CIEA, gx_reloc_ptrs_CIEA);
const gs_color_space_type
	gs_color_space_type_CIEDEFG =
	 { gs_color_space_index_CIEDEFG, 3, true,
	   gx_init_paint_3, gx_concrete_space_CIE,
	   gx_concretize_CIEDEFG, NULL,
	   gx_default_remap_color, gx_install_CIEDEFG,
	   gx_adjust_cspace_CIEDEFG, gx_no_adjust_color_count,
	   gx_enum_ptrs_CIEDEFG, gx_reloc_ptrs_CIEDEFG
	 },
	gs_color_space_type_CIEDEF =
	 { gs_color_space_index_CIEDEF, 3, true,
	   gx_init_paint_3, gx_concrete_space_CIE,
	   gx_concretize_CIEDEF, NULL,
	   gx_default_remap_color, gx_install_CIEDEF,
	   gx_adjust_cspace_CIEDEF, gx_no_adjust_color_count,
	   gx_enum_ptrs_CIEDEF, gx_reloc_ptrs_CIEDEF
	 },
	gs_color_space_type_CIEABC =
	 { gs_color_space_index_CIEABC, 3, true,
	   gx_init_paint_3, gx_concrete_space_CIE,
	   gx_concretize_CIEABC, NULL,
	   gx_remap_CIEABC, gx_install_CIEABC,
	   gx_adjust_cspace_CIEABC, gx_no_adjust_color_count,
	   gx_enum_ptrs_CIEABC, gx_reloc_ptrs_CIEABC
	 },
	gs_color_space_type_CIEA =
	 { gs_color_space_index_CIEA, 1, true,
	   gx_init_paint_1, gx_concrete_space_CIE,
	   gx_concretize_CIEA, NULL,
	   gx_default_remap_color, gx_install_CIEA,
	   gx_adjust_cspace_CIEA, gx_no_adjust_color_count,
	   gx_enum_ptrs_CIEA, gx_reloc_ptrs_CIEA
	 };

/* GC procedures for CIE color spaces. */
/* These have to come after the cs_declare_procs because of */
/* a bug in the VAX C compiler. */

#define pcs ((gs_color_space *)vptr)

private ENUM_PTRS_BEGIN(gx_enum_ptrs_CIEDEFG) return 0;
	ENUM_PTR(0, gs_color_space, params.defg);
ENUM_PTRS_END
private RELOC_PTRS_BEGIN(gx_reloc_ptrs_CIEDEFG) {
	RELOC_PTR(gs_color_space, params.defg);
} RELOC_PTRS_END

private ENUM_PTRS_BEGIN(gx_enum_ptrs_CIEDEF) return 0;
	ENUM_PTR(0, gs_color_space, params.def);
ENUM_PTRS_END
private RELOC_PTRS_BEGIN(gx_reloc_ptrs_CIEDEF) {
	RELOC_PTR(gs_color_space, params.def);
} RELOC_PTRS_END

private ENUM_PTRS_BEGIN(gx_enum_ptrs_CIEABC) return 0;
	ENUM_PTR(0, gs_color_space, params.abc);
ENUM_PTRS_END
private RELOC_PTRS_BEGIN(gx_reloc_ptrs_CIEABC) {
	RELOC_PTR(gs_color_space, params.abc);
} RELOC_PTRS_END

private ENUM_PTRS_BEGIN(gx_enum_ptrs_CIEA) return 0;
	ENUM_PTR(0, gs_color_space, params.a);
ENUM_PTRS_END
private RELOC_PTRS_BEGIN(gx_reloc_ptrs_CIEA) {
	RELOC_PTR(gs_color_space, params.a);
} RELOC_PTRS_END

#undef pcs

/* ------ Default values for CIE dictionary elements ------ */

/* Default transformation procedures. */

private float
a_identity(floatp in, const gs_cie_a *pcie)
{	return in;
}
private float
abc_identity(floatp in, const gs_cie_abc *pcie)
{	return in;
}
private float
def_identity(floatp in, const gs_cie_def *pcie)
{	return in;
}
private float
defg_identity(floatp in, const gs_cie_defg *pcie)
{	return in;
}
private float
common_identity(floatp in, const gs_cie_common *pcie)
{	return in;
}
private float
render_identity(floatp in, const gs_cie_render *pcie)
{	return in;
}
private float
tpqr_identity(floatp in, const gs_cie_wbsd *pwbsd, const gs_cie_render *pcie)
{	return in;
}
private frac
render_table_identity(byte in, const gs_cie_render *pcie)
{	return byte2frac(in);
}

/* Default vectors and matrices. */

const gs_range3 Range3_default = { {{0,1}, {0,1}, {0,1}} };
const gs_range4 Range4_default = { {{0,1}, {0,1}, {0,1}, {0,1}} };
const gs_cie_defg_proc4 DecodeDEFG_default =
 { { defg_identity, defg_identity, defg_identity, defg_identity } };
const gs_cie_def_proc3 DecodeDEF_default =
 { { def_identity, def_identity, def_identity } };
const gs_cie_abc_proc3 DecodeABC_default =
 { { abc_identity, abc_identity, abc_identity } };
const gs_cie_common_proc3 DecodeLMN_default =
 { { common_identity, common_identity, common_identity } };
const gs_matrix3 Matrix3_default = { {1,0,0}, {0,1,0}, {0,0,1}, 1 /*true*/ };
const gs_range RangeA_default = {0,1};
const gs_cie_a_proc DecodeA_default = a_identity;
const gs_vector3 MatrixA_default = { 1, 1, 1 };
const gs_vector3 BlackPoint_default = { 0, 0, 0 };
const gs_cie_render_proc3 Encode_default =
 { { render_identity, render_identity, render_identity } };
const gs_cie_transform_proc3 TransformPQR_default =
 { { tpqr_identity, tpqr_identity, tpqr_identity } };
const gs_cie_render_table_procs RenderTableT_default =
 { { render_table_identity, render_table_identity, render_table_identity,
     render_table_identity
 } };

/* ------ Adjust reference counts for a CIE color space ------ */

private void
gx_adjust_cspace_CIEDEFG(const gs_color_space *pcs, gs_state *pgs, int delta)
{	rc_adjust_const(pcs->params.defg, delta, pgs->memory,
			"gx_adjust_cspace_CIEDEFG");
}

private void
gx_adjust_cspace_CIEDEF(const gs_color_space *pcs, gs_state *pgs, int delta)
{	rc_adjust_const(pcs->params.def, delta, pgs->memory,
			"gx_adjust_cspace_CIEDEF");
}

private void
gx_adjust_cspace_CIEABC(const gs_color_space *pcs, gs_state *pgs, int delta)
{	rc_adjust_const(pcs->params.abc, delta, pgs->memory,
			"gx_adjust_cspace_CIEABC");
}

private void
gx_adjust_cspace_CIEA(const gs_color_space *pcs, gs_state *pgs, int delta)
{	rc_adjust_const(pcs->params.a, delta, pgs->memory,
			"gx_adjust_cspace_CIEA");
}

/* ================ Table setup ================ */

/* ------ Install a CIE color space ------ */

private int cie_load_common_cache(P3(gs_cie_common *, gs_state *,
  client_name_t));
private void near cie_cache_mult(P3(gx_cie_vector_cache *, const gs_vector3 *,
				    const cie_cache_floats *));
private bool near cie_cache_mult3(P2(gx_cie_vector_cache *,
				     const gs_matrix3 *));

private int
gx_install_CIEDEFG(gs_color_space *pcs, gs_state *pgs)
{	/****** NOT IMPLEMENTED YET ******/
	return_error(gs_error_undefined);
}

private int
gx_install_CIEDEF(gs_color_space *pcs, gs_state *pgs)
{	/****** NOT IMPLEMENTED YET ******/
	return_error(gs_error_undefined);
}

private int
gx_install_CIEABC(gs_color_space *pcs, gs_state *pgs)
{	gs_cie_abc *pcie = pcs->params.abc;
	cie_matrix_init(&pcie->MatrixABC);
	CIE_LOAD_CACHE3_BODY(pcie->caches.DecodeABC, pcie->RangeABC.ranges,
			     &pcie->DecodeABC, pcie, "DecodeABC");
	gs_cie_abc_complete(pcie);
	return cie_load_common_cache(&pcie->common, pgs, "gx_install_CIEABC");
}

private int
gx_install_CIEA(gs_color_space *pcs, gs_state *pgs)
{	gs_cie_a *pcie = pcs->params.a;
	int i;
	gs_for_loop_params lp;
	float in;
	gs_cie_cache_init(&pcie->caches.DecodeA.floats.params, &lp,
			  &pcie->RangeA, "DecodeA");
	for ( i = 0, in = lp.init; i < gx_cie_cache_size; in += lp.step, i++ )
	  pcie->caches.DecodeA.floats.values[i] =
	    (*pcie->DecodeA)(in, pcie);
	gs_cie_a_complete(pcie);
	return cie_load_common_cache(&pcie->common, pgs, "gx_install_CIEA");
}

/* Load the common caches when installing the color space. */
private int
cie_load_common_cache(gs_cie_common *pcie, gs_state *pgs, client_name_t cname)
{	gx_cie_joint_caches *pjc;

	cie_matrix_init(&pcie->MatrixLMN);
	CIE_LOAD_CACHE3_BODY(pcie->caches.DecodeLMN, pcie->RangeLMN.ranges,
			     &pcie->DecodeLMN, pcie, "DecodeLMN")
	if ( pgs->cie_render == 0 )
	  return 0;
	pjc = gx_currentciecaches(pgs);
	if ( pjc == 0 )
	  return_error(gs_error_VMerror);
	cie_joint_caches_init(pjc, pcie, pgs->cie_render);
	cie_joint_caches_complete(pjc, pcie, pgs->cie_render);
	return 0;
}

/* Complete loading a CIEBasedABC color space. */
/* This routine is not idempotent. */
void
gs_cie_abc_complete(gs_cie_abc *pcie)
{	pcie->caches.skipABC =
	  cie_cache_mult3(pcie->caches.DecodeABC, &pcie->MatrixABC);
}

/* Complete loading a CIEBasedA color space. */
/* This routine is not idempotent. */
void
gs_cie_a_complete(gs_cie_a *pcie)
{	cie_cache_mult(&pcie->caches.DecodeA, &pcie->MatrixA,
		       &pcie->caches.DecodeA.floats);
}

/* Convert a scalar cache to a vector cache by multiplying */
/* the scalar values by a vector. */
private void near
cie_cache_mult(gx_cie_vector_cache *pcache, const gs_vector3 *pvec,
  const cie_cache_floats *pcf)
{	int i;
	cie_vector_cache_params params;
	params.is_identity = pcf->params.is_identity;
	params.base = float2cie_cached(pcf->params.base);
	params.factor = float2cie_cached(pcf->params.factor);
	params.limit =
	  float2cie_cached((gx_cie_cache_size - 1) / pcf->params.factor +
			   pcf->params.base);
	/* Loop from top to bottom so that we don't */
	/* overwrite elements before they're used, */
	/* in case pcf is an alias for pcache->floats. */
	for ( i = gx_cie_cache_size; --i >= 0; )
	  {	float f = pcf->values[i];
		pcache->vecs.values[i].u = float2cie_cached(f * pvec->u);
		pcache->vecs.values[i].v = float2cie_cached(f * pvec->v);
		pcache->vecs.values[i].w = float2cie_cached(f * pvec->w);
	  }
	pcache->vecs.params = params;
}
/* Convert 3 scalar caches to vector caches by multiplying by a matrix. */
/* Return true iff the resulting cache is an identity transformation. */
private bool near
cie_cache_mult3(gx_cie_vector_cache *pc /*[3]*/, const gs_matrix3 *pmat)
{	cie_cache_mult(pc, &pmat->cu, &pc->floats);
	cie_cache_mult(pc + 1, &pmat->cv, &pc[1].floats);
	cie_cache_mult(pc + 2, &pmat->cw, &pc[2].floats);
	return pmat->is_identity & pc[0].vecs.params.is_identity &
	   pc[1].vecs.params.is_identity & pc[2].vecs.params.is_identity;
}

/* ------ Install a rendering dictionary ------ */

/* setcolorrendering */
int
gs_setcolorrendering(gs_state *pgs, gs_cie_render *pcie)
{	int code = gs_cie_render_init(pcie);
	if ( code < 0 )
	  return code;
	rc_assign(pgs->cie_render, pcie, pgs->memory,
		  "gs_setcolorrendering");
	/* Load the caches. */
	CIE_LOAD_CACHE3_BODY(pcie->caches.EncodeLMN, pcie->DomainLMN.ranges,
			     &pcie->EncodeLMN, pcie, "EncodeLMN");
	CIE_LOAD_CACHE3_BODY(pcie->caches.EncodeABC, pcie->DomainABC.ranges,
			     &pcie->EncodeABC, pcie, "EncodeABC");
	if ( pcie->RenderTable.lookup.table != 0 )
	  { int i, j, m = pcie->RenderTable.lookup.m;
	    gs_for_loop_params flp;
	    for ( j = 0; j < m; j++ )
	      gs_cie_cache_init(&pcie->caches.RenderTableT[j].fracs.params,
				&flp, &Range3_default.ranges[0],
				"RenderTableT");
	    /****** ASSUMES gx_cie_cache_size >= 256 ******/
	    for ( i = 0; i < 256; i++ )
	      for ( j = 0; j < m; j++ )
		pcie->caches.RenderTableT[j].fracs.values[i] =
		  (*pcie->RenderTable.T.procs[j])((byte)i, pcie);
	  }
	code = gs_cie_render_complete(pcie);
	if ( code < 0 )
	  return code;
	/* Initialize the joint caches if needed. */
	gs_cie_cs_complete(pgs, true);
	gx_unset_dev_color(pgs);
	return code;
}

/* currentcolorrendering */
const gs_cie_render *
gs_currentcolorrendering(const gs_state *pgs)
{	return pgs->cie_render;
}

/* Unshare (allocating if necessary) the joint caches. */
gx_cie_joint_caches *
gx_currentciecaches(gs_state *pgs)
{	rc_unshare_struct(pgs->cie_joint_caches, gx_cie_joint_caches,
			  &st_joint_caches, pgs->memory,
			  return 0, "gx_currentciecaches");
	return pgs->cie_joint_caches;
}

/* ------ Compute the parameters for loading a cache. ------ */
/* Sets base and factor. */

void
gs_cie_cache_init(cie_cache_params *pcache, gs_for_loop_params *pflp,
  const gs_range *domain, client_name_t cname)
{	/*
	 * We need to map the values in the range
	 * [domain->rmin..domain->rmax].  However, if neither rmin
	 * nor rmax is zero and the function is non-linear,
	 * this can lead to anomalies at zero, which is the
	 * default value for CIE colors.  The "correct" way to
	 * approach this is to run the mapping functions on demand,
	 * but we don't want to deal with the complexities of the
	 * callbacks this would involve (especially in the middle of
	 * rendering images); instead, we adjust the range so that zero
	 * maps precisely to a cache slot.  Define:
	 *	a = domain->rmin;
	 *	b = domain->rmax;
	 *	R = b - a;
	 *	N = gx_cie_cache_size - 1;
	 *	f(v) = N(v-a)/R;
	 *	x = f(0).
	 * If x is not an integer, we can either increase b or
	 * decrease a to make it one.  In the former case, compute:
	 *	Kb = floor(x); R'b = N(0-a)/Kb; b' = a + R'b.
	 * In the latter case, compute:
	 *	Ka = ceiling(x-N); R'a = N(0-b)/Ka; a' = b - R'a.
	 * We choose whichever method stretches the range the least,
	 * i.e., the one whose R' value (R'a or R'b) is smaller.
	 */
	double a = domain->rmin, b = domain->rmax;
	double R = b - a;
#define N (gx_cie_cache_size - 1)
	double delta;
	/* Adjust the range if necessary. */
	if ( a < 0 && b >= 0 )
	  {	double x = -N * a / R;	/* must be > 0 */
		double Kb = floor(x);		/* must be >= 0 */
		double Ka = ceil(x) - N;	/* must be <= 0 */
		if ( Kb == 0 || (Ka != 0 && -b / Ka < -a / Kb) )	/* use R'a */
		  R = -N * b / Ka, a = b - R;
		else			/* use R'b */
		  R = -N * a / Kb, b = a + R;
	  }
	delta = R / N;
#ifdef CIE_CACHE_INTERPOLATE
	pcache->base = a;		/* no rounding */
#else
	pcache->base = a - delta / 2;	/* so lookup will round */
#endif
	pcache->factor = (delta == 0 ? 0 : N / R);
	if_debug4('c', "[c]cache %s 0x%lx base=%g, factor=%g\n",
		  (const char *)cname, (ulong)pcache,
		  pcache->base, pcache->factor);
	pflp->init = a;
	pflp->step = delta;
	pflp->limit = b + delta / 2;
}

/* ------ Complete a rendering structure ------ */

/* Compute values derived from the rendering structure parameters */
/* other than the cached procedure values.  This routine is idempotent. */
private void near cie_transform_range3(P3(const gs_range3 *,
  const gs_matrix3 *, gs_range3 *));
int
gs_cie_render_init(gs_cie_render *pcie)
{	gs_matrix3 PQR_inverse;
	cie_matrix_init(&pcie->MatrixLMN);
	cie_matrix_init(&pcie->MatrixABC);
	cie_matrix_init(&pcie->MatrixPQR);
	cie_invert3(&pcie->MatrixPQR, &PQR_inverse);
	cie_matrix_mult3(&pcie->MatrixLMN, &PQR_inverse,
			 &pcie->MatrixPQR_inverse_LMN);
	cie_transform_range3(&pcie->RangePQR, &pcie->MatrixPQR_inverse_LMN,
			     &pcie->DomainLMN);
	cie_transform_range3(&pcie->RangeLMN, &pcie->MatrixABC,
			     &pcie->DomainABC);
	cie_mult3(&pcie->points.WhitePoint, &pcie->MatrixPQR, &pcie->wdpqr);
	cie_mult3(&pcie->points.BlackPoint, &pcie->MatrixPQR, &pcie->bdpqr);
	return 0;
}

/* Transform a set of ranges. */
private void near
cie_transform_range(const gs_range3 *in, floatp mu, floatp mv, floatp mw,
  gs_range *out)
{	float umin = mu * in->ranges[0].rmin,
	  umax = mu * in->ranges[0].rmax;
	float vmin = mv * in->ranges[1].rmin,
	  vmax = mv * in->ranges[1].rmax;
	float wmin = mw * in->ranges[2].rmin,
	  wmax = mw * in->ranges[2].rmax;
	float temp;
#define swap(x, y) temp = x, x = y, y = temp

	if ( umin > umax ) swap(umin, umax);
	if ( vmin > vmax ) swap(vmin, vmax);
	if ( wmin > wmax ) swap(wmin, wmax);
	out->rmin = umin + vmin + wmin;
	out->rmax = umax + vmax + wmax;
#undef swap
}
private void near
cie_transform_range3(const gs_range3 *in, const gs_matrix3 *mat,
  gs_range3 *out)
{	cie_transform_range(in, mat->cu.u, mat->cv.u, mat->cw.u,
			    &out->ranges[0]);
	cie_transform_range(in, mat->cu.v, mat->cv.v, mat->cw.v,
			    &out->ranges[1]);
	cie_transform_range(in, mat->cu.w, mat->cv.w, mat->cw.w,
			    &out->ranges[2]);
}

/* Complete the loading of the rendering caches. */
/* Note that this routine may make non-idempotent changes to */
/* the values in the caches. */
private void near cie_cache_restrict(P2(cie_cache_floats *, const gs_range *));
#define cie_cache_restrict3(pcache3, pr3)\
	cie_cache_restrict(&(pcache3)[0].floats, &(pr3)->ranges[0]);\
	cie_cache_restrict(&(pcache3)[1].floats, &(pr3)->ranges[1]);\
	cie_cache_restrict(&(pcache3)[2].floats, &(pr3)->ranges[2])
int
gs_cie_render_complete(gs_cie_render *pcie)
{	/* Since range restriction happens immediately after */
	/* the cache lookup, we can save a step by restricting */
	/* the values in the cache entries. */
	cie_cache_restrict3(pcie->caches.EncodeLMN, &pcie->RangeLMN);
	cie_cache_restrict3(pcie->caches.EncodeABC, &pcie->RangeABC);
	/* If there is no lookup table, we want the final ABC values */
	/* to be fracs; if there is a table, we want them to be */
	/* appropriately scaled ints. */
	pcie->MatrixABCEncode = pcie->MatrixABC;
	  {	int c;
		double f;
		for ( c = 0; c < 3; c++ )
		  {	gx_cie_scalar_cache *pcache =
			  &pcie->caches.EncodeABC[c];
			if ( pcie->RenderTable.lookup.table == 0 )
			{	cie_cache_restrict(&pcache->floats,
						   &Range3_default.ranges[0]);
				gs_cie_cache_to_fracs(pcache);
				pcache->fracs.params.is_identity = false;
			}
			else
			{	int i;
				int n = pcie->RenderTable.lookup.dims[c];
#ifdef CIE_RENDER_TABLE_INTERPOLATE
#  define scale_index(f, n, itemp)\
     restrict_index(f * (1 << _cie_interpolate_bits),\
		    (n) << _cie_interpolate_bits, itemp)
#else
				int m = pcie->RenderTable.lookup.m;
				int k =
				  (c == 0 ? 1 : c == 1 ?
				   m * pcie->RenderTable.lookup.dims[2] : m);
#  define scale_index(f, n, itemp)\
     (restrict_index(f, n, itemp) * k)
#endif
				const gs_range *prange =
				  pcie->RangeABC.ranges + c;
				/* Loop from top to bottom so that we don't */
				/* overwrite elements before they're used. */
				for ( i = gx_cie_cache_size; --i >= 0; )
				{	float v =
					  (pcache->floats.values[i] -
					   prange->rmin) * (n - 1) /
					  (prange->rmax - prange->rmin)
#ifndef CIE_RENDER_TABLE_INTERPOLATE
					    + 0.5
#endif
					      ;
					int itemp;

					if_debug5('c',
					  "[c]cache[%d][%d] = %g => %g => %d\n",
					  c, i, pcache->floats.values[i], v,
					  scale_index(v, n, itemp));
					pcache->ints.values[i] =
					  scale_index(v, n, itemp);
				}
				pcache->ints.params = pcache->floats.params;	/* (not necessary) */
				pcache->ints.params.is_identity = false;
#undef scale_index
			}
		  }
		/* Fold the scaling of the EncodeABC cache index */
		/* into MatrixABC. */
#define mabc(i, t)\
  f = pcie->caches.EncodeABC[i].floats.params.factor;\
  pcie->MatrixABCEncode.cu.t *= f;\
  pcie->MatrixABCEncode.cv.t *= f;\
  pcie->MatrixABCEncode.cw.t *= f;\
  pcie->EncodeABC_base[i] =\
    float2cie_cached(pcie->caches.EncodeABC[i].floats.params.base * f)
		mabc(0, u);
		mabc(1, v);
		mabc(2, w);
		pcie->MatrixABCEncode.is_identity = 0;
	  }
#undef mabc
	cie_cache_mult3(pcie->caches.EncodeLMN, &pcie->MatrixABCEncode);
	return 0;
}

/* Apply a range restriction to one cache or 3 caches. */
private void near
cie_cache_restrict(cie_cache_floats *pcache, const gs_range *prange)
{	int i;
	for ( i = 0; i < gx_cie_cache_size; i++ )
	  if_restrict(pcache->values[i], *prange);
}

/* Convert a cache from floats to fracs. */
void
gs_cie_cache_to_fracs(gx_cie_scalar_cache *pcache)
{	int i;
	/* Loop from bottom to top so that we don't */
	/* overwrite elements before they're used. */
	for ( i = 0; i < gx_cie_cache_size; ++i )
	  pcache->fracs.values[i] = float2frac(pcache->floats.values[i]);
	pcache->fracs.params = pcache->floats.params;	/* (not necessary) */
}

/* ------ Fill in the joint cache ------ */

/* If the current color space is a CIE space, or has a CIE base space, */
/* return a pointer to the common part of the space; otherwise return 0. */
const gs_cie_common *
gs_cie_cs_common(gs_state *pgs)
{	const gs_color_space *pcs = pgs->color_space;

sw:	switch ( pcs->type->index )
	  {
	  case gs_color_space_index_CIEABC:
	    return &pcs->params.abc->common;
	  case gs_color_space_index_CIEA:
	    return &pcs->params.a->common;
	  case gs_color_space_index_Separation:
	    pcs = (const gs_color_space *)&pcs->params.separation.alt_space;
	    goto sw;
	  case gs_color_space_index_Indexed:
	    pcs = gs_color_space_indexed_base_space(pcs);
	    goto sw;
	  case gs_color_space_index_Pattern:
	    /****** WHAT? ******/
	  default:
	    return 0;
	  }
}

/* Finish loading the joint caches for the current color space. */
void
gs_cie_cs_complete(gs_state *pgs, bool init)
{	const gs_cie_common *common = gs_cie_cs_common(pgs);

	if ( common )
	  { if ( init )
	      cie_joint_caches_init(pgs->cie_joint_caches, common,
				    pgs->cie_render);
	    cie_joint_caches_complete(pgs->cie_joint_caches, common,
				      pgs->cie_render);
	  }
}

/* Compute values derived from the color space and rendering parameters */
/* other than the cached procedure values.  This routine is idempotent. */
private void
cie_joint_caches_init(gx_cie_joint_caches *pjc,
  const gs_cie_common *pcie, const gs_cie_render *pcier)
{	pjc->points_sd.ws.xyz = pcie->points.WhitePoint;
	cie_mult3(&pjc->points_sd.ws.xyz, &pcier->MatrixPQR,
		  &pjc->points_sd.ws.pqr);
	pjc->points_sd.bs.xyz = pcie->points.BlackPoint;
	cie_mult3(&pjc->points_sd.bs.xyz, &pcier->MatrixPQR,
		  &pjc->points_sd.bs.pqr);
	pjc->points_sd.wd.xyz = pcier->points.WhitePoint;
	pjc->points_sd.wd.pqr = pcier->wdpqr;
	pjc->points_sd.bd.xyz = pcier->points.BlackPoint;
	pjc->points_sd.bd.pqr = pcier->bdpqr;
	cie_matrix_mult3(&pcier->MatrixPQR, &pcie->MatrixLMN,
			 &pjc->MatrixLMN_PQR);
	/* Load the TransformPQR caches. */
	{ int i, j;
	  gs_for_loop_params lp[3];
	  cie_cache_init3(pjc->TransformPQR, lp, pcier->RangePQR.ranges,
			  "TransformPQR");
	  for ( i = 0; i < gx_cie_cache_size; i++ )
	    for ( j = 0; j < 3; lp[j].init += lp[j].step, j++ )
	      pjc->TransformPQR[j].floats.values[i] =
		(*pcier->TransformPQR.procs[j])(lp[j].init, &pjc->points_sd, pcier);
	}
}

/* Complete the loading of the joint caches.  Note that this routine */
/* may make non-idempotent changes to the values in the caches. */
private void
cie_joint_caches_complete(gx_cie_joint_caches *pjc,
  const gs_cie_common *pcie, const gs_cie_render *pcier)
{	int j;
	cie_cache_restrict3(pjc->TransformPQR, &pcier->RangePQR);
	for ( j = 0; j < 3; j++ )
	  cie_cache_mult(&pjc->DecodeLMN[j],
			 &pjc->MatrixLMN_PQR.cu + j,
			 &pcie->caches.DecodeLMN[j].floats);
	pjc->skipLMN = pjc->MatrixLMN_PQR.is_identity &
	  pjc->DecodeLMN[0].vecs.params.is_identity &
	  pjc->DecodeLMN[1].vecs.params.is_identity &
	  pjc->DecodeLMN[2].vecs.params.is_identity;
	pjc->skipPQR =
	  cie_cache_mult3(pjc->TransformPQR, &pcier->MatrixPQR_inverse_LMN);
}

/* ================ Color rendering (using the caches) ================ */

private int near cie_remap_finish(P3(const cie_cached_vector3 *,
  frac *, const gs_state *));
private void near cie_lookup_mult3(P2(cie_cached_vector3 *, const gx_cie_vector_cache *));
#ifdef DEBUG
private void near
cie_lookup_map3(cie_cached_vector3 *pvec,
  const gx_cie_vector_cache *pc /*[3]*/, const char _ds *cname)
{	if_debug5('c', "[c]lookup %s 0x%lx [%g %g %g]\n",
		  (const char *)cname, (ulong)pc,
		  cie_cached2float(pvec->u), cie_cached2float(pvec->v),
		  cie_cached2float(pvec->w));
	cie_lookup_mult3(pvec, pc);
	if_debug3('c', "        =[%g %g %g]\n",
		  cie_cached2float(pvec->u), cie_cached2float(pvec->v),
		  cie_cached2float(pvec->w));
}
#else
#  define cie_lookup_map3(pvec, pc, cname) cie_lookup_mult3(pvec, pc)
#endif

/* Determine the concrete space underlying a CIEBased space. */
private const gs_color_space cie_rgb_space =
	{ &gs_color_space_type_DeviceRGB };
private const gs_color_space cie_cmyk_space =
	{ &gs_color_space_type_DeviceCMYK };
private const gs_color_space *
gx_concrete_space_CIE(const gs_color_space *pcs, const gs_state *pgs)
{	const gs_cie_render *pcie = pgs->cie_render;
	if ( pcie == 0 || pcie->RenderTable.lookup.table == 0 ||
	     pcie->RenderTable.lookup.m == 3
	   )
	  return &cie_rgb_space;
	else				/* pcie->RenderTable.lookup.m == 4 */
	  return &cie_cmyk_space;
}

/* Render a CIEBasedDEFG color. */
private int
gx_concretize_CIEDEFG(const gs_client_color *pc, const gs_color_space *pcs,
  frac *pconc, const gs_state *pgs)
{	/****** NOT IMPLEMENTED YET ******/
	return_error(gs_error_undefined);
}

/* Render a CIEBasedDEF color. */
private int
gx_concretize_CIEDEF(const gs_client_color *pc, const gs_color_space *pcs,
  frac *pconc, const gs_state *pgs)
{	/****** NOT IMPLEMENTED YET ******/
	return_error(gs_error_undefined);
}

/* Render a CIEBasedABC color. */
/* We provide both remap and concretize, but only the former */
/* needs to be efficient. */
private int
gx_remap_CIEABC(const gs_client_color *pc, const gs_color_space *pcs,
  gx_device_color *pdc, const gs_state *pgs)
{	frac conc[4];
	const gs_cie_abc *pcie = pcs->params.abc;
	cie_cached_vector3 vec3;

	if_debug3('c', "[c]remap CIEABC [%g %g %g]\n",
		  pc->paint.values[0], pc->paint.values[1],
		  pc->paint.values[2]);
	vec3.u = float2cie_cached(pc->paint.values[0]);
	vec3.v = float2cie_cached(pc->paint.values[1]);
	vec3.w = float2cie_cached(pc->paint.values[2]);

	/* Apply DecodeABC and MatrixABC. */
#define vabc vec3
#define vlmn vec3
	if ( !pcie->caches.skipABC )
	  cie_lookup_map3(&vabc /*&vlmn*/, &pcie->caches.DecodeABC[0],
			  "Decode/MatrixABC");
#undef vabc
	switch ( cie_remap_finish(&vlmn, conc, pgs) )
	  {
	  case 3:
		if_debug3('c', "[c]=RGB [%g %g %g]\n",
			  frac2float(conc[0]), frac2float(conc[1]),
  			  frac2float(conc[2]));
		gx_remap_concrete_rgb(conc[0], conc[1], conc[2], pdc, pgs);
		return 0;
	  case 4:
		if_debug4('c', "[c]=CMYK [%g %g %g %g]\n",
			  frac2float(conc[0]), frac2float(conc[1]),
  			  frac2float(conc[2]), frac2float(conc[3]));
		gx_remap_concrete_cmyk(conc[0], conc[1], conc[2], conc[3],
				       pdc, pgs);
		return 0;
	  }
	/* Can't happen. */
	return_error(gs_error_unknownerror);
#undef vlmn
}
private int
gx_concretize_CIEABC(const gs_client_color *pc, const gs_color_space *pcs,
  frac *pconc, const gs_state *pgs)
{	const gs_cie_abc *pcie = pcs->params.abc;
	cie_cached_vector3 vec3;

	if_debug3('c', "[c]concretize CIEABC [%g %g %g]\n",
		  pc->paint.values[0], pc->paint.values[1],
		  pc->paint.values[2]);
	vec3.u = float2cie_cached(pc->paint.values[0]);
	vec3.v = float2cie_cached(pc->paint.values[1]);
	vec3.w = float2cie_cached(pc->paint.values[2]);
#define vabc vec3
#define vlmn vec3
	if ( !pcie->caches.skipABC )
	  cie_lookup_map3(&vabc /*&vlmn*/, &pcie->caches.DecodeABC[0],
			  "Decode/MatrixABC");
#undef vabc
	cie_remap_finish(&vlmn, pconc, pgs);
#undef vlmn
	return 0;
}

/* Render a CIEBasedA color. */
private int
gx_concretize_CIEA(const gs_client_color *pc, const gs_color_space *pcs,
  frac *pconc, const gs_state *pgs)
{	const gs_cie_a *pcie = pcs->params.a;
	cie_cached_value a = float2cie_cached(pc->paint.values[0]);
	cie_cached_vector3 vlmn;

	if_debug1('c', "[c]concretize CIEA %g\n", pc->paint.values[0]);

	/* Apply DecodeA and MatrixA. */
	vlmn = lookup_value(a, &pcie->caches.DecodeA);
	return cie_remap_finish(&vlmn, pconc, pgs);
}

/* Common rendering code. */
/* Return 3 if RGB, 4 if CMYK. */
private int near
cie_remap_finish(const cie_cached_vector3 *plmn, frac *pconc,
  const gs_state *pgs)
{	const gs_cie_render *pcie = pgs->cie_render;
	const gx_cie_joint_caches *pjc = pgs->cie_joint_caches;
	const gs_const_string *table;
	cie_cached_vector3 vec3;
	int tabc[3];		/* indices for final EncodeABC lookup */

	if ( pcie == 0 )
	  {	/* No rendering has been defined yet. */
		/* Just return black. */
		pconc[0] = pconc[1] = pconc[2] = frac_0;
		return 3;
	  }

	/* Apply DecodeLMN, MatrixLMN(decode), and MatrixPQR. */
#define vlmn vec3
	vlmn = *plmn;
#define vpqr vec3
	if ( !pjc->skipLMN )
	  cie_lookup_map3(&vlmn /*&vpqr*/, &pjc->DecodeLMN[0],
			  "Decode/MatrixLMN+MatrixPQR");
#undef vlmn

	/* Apply TransformPQR, MatrixPQR', and MatrixLMN(encode). */
#define vlmn vec3
	if ( !pjc->skipPQR )
	  cie_lookup_map3(&vpqr /*&vlmn*/, &pjc->TransformPQR[0],
			  "Transform/Matrix'PQR+MatrixLMN");
#undef vpqr

	/* Apply EncodeLMN and MatrixABC(encode). */
#define vabc vec3
	cie_lookup_map3(&vlmn /*&vabc*/, &pcie->caches.EncodeLMN[0],
			"EncodeLMN+MatrixABC");
#undef vlmn
	/* MatrixABCEncode includes the scaling of the EncodeABC */
	/* cache index. */
#define set_tabc(i, t)\
  set_restrict_index(tabc[i],\
		     cie_cached2int(vabc.t - pcie->EncodeABC_base[i],\
				    _cie_interpolate_bits),\
		     gx_cie_cache_size << _cie_interpolate_bits)
	set_tabc(0, u);
	set_tabc(1, v);
	set_tabc(2, w);
	table = pcie->RenderTable.lookup.table;
	if ( table == 0 )
	{	/* No further transformation. */
		/* The final mapping step includes both restriction to */
		/* the range [0..1] and conversion to fracs. */
#define eabc(i)\
  cie_interpolate_fracs(pcie->caches.EncodeABC[i].fracs.values, tabc[i])
		pconc[0] = eabc(0);
		pconc[1] = eabc(1);
		pconc[2] = eabc(2);
#undef eabc
		return 3;
	}
	else
	{	/* Use the RenderTable. */
		int m = pcie->RenderTable.lookup.m;
#define rt_lookup(j, i) pcie->caches.RenderTableT[j].fracs.values[i]
#ifdef CIE_RENDER_TABLE_INTERPOLATE

		/* The final mapping step includes restriction to the */
		/* ranges [0..dims[c]] as ints with interpolation bits. */
		fixed rfix[3];

#define eabc(i)\
  cie_interpolate_fracs(pcie->caches.EncodeABC[i].ints.values, tabc[i])
#define fabc(i)\
  (eabc(i) << (_fixed_shift - _cie_interpolate_bits))
		rfix[0] = fabc(0);
		rfix[1] = fabc(1);
		rfix[2] = fabc(2);
		if_debug6('c', "[c]ABC=%g,%g,%g => iabc=%g,%g,%g\n",
			  cie_cached2float(vabc.u), cie_cached2float(vabc.v),
			  cie_cached2float(vabc.w), fixed2float(rfix[0]),
			  fixed2float(rfix[1]), fixed2float(rfix[2]));
		gx_color_interpolate_linear(rfix, &pcie->RenderTable.lookup,
					    pconc);
		if_debug3('c', "[c]  interpolated => %g,%g,%g\n",
			  frac2float(pconc[0]), frac2float(pconc[1]),
			  frac2float(pconc[2]));
		if ( !pcie->caches.RenderTableT_is_identity )
		  {	/* Map the interpolated values. */
#define frac2cache_index(v) frac2bits(v, gx_cie_log2_cache_size)
			pconc[0] = rt_lookup(0, frac2cache_index(pconc[0]));
			pconc[1] = rt_lookup(1, frac2cache_index(pconc[1]));
			pconc[2] = rt_lookup(2, frac2cache_index(pconc[2]));
			if ( m > 3 )
			  pconc[3] = rt_lookup(3, frac2cache_index(pconc[3]));
#undef frac2cache_index
		  }

#else				/* !CIE_RENDER_TABLE_INTERPOLATE */

		/* The final mapping step includes restriction to the */
		/* ranges [0..dims[c]], plus scaling of the indices */
		/* in the strings. */
#define ri(i)\
  pcie->caches.EncodeABC[i].ints.values[tabc[i] >> _cie_interpolate_bits]
		int ia = ri(0);
		int ib = ri(1);		/* pre-multiplied by m * NC */
		int ic = ri(2);		/* pre-multiplied by m */
		const byte *prtc = table[ia].data + ib + ic;
			/* (*pcie->RenderTable.T)(prtc, m, pcie, pconc); */

		if_debug6('c', "[c]ABC=%g,%g,%g => iabc=%d,%d,%d\n",
			  cie_cached2float(vabc.u), cie_cached2float(vabc.v),
			  cie_cached2float(vabc.w), ia, ib, ic);
		if ( pcie->caches.RenderTableT_is_identity )
		  {	pconc[0] = byte2frac(prtc[0]);
			pconc[1] = byte2frac(prtc[1]);
			pconc[2] = byte2frac(prtc[2]);
			if ( m > 3 )
			  pconc[3] = byte2frac(prtc[3]);
		  }
		else
		  {
#if gx_cie_log2_cache_size == 8
#  define byte2cache_index(b) (b)
#else
# if gx_cie_log2_cache_size > 8
#  define byte2cache_index(b)\
    ( ((b) << (gx_cie_log2_cache_size - 8)) +\
      ((b) >> (16 - gx_cie_log2_cache_size)) )
# else	/* < 8 */
#  define byte2cache_index(b) ((b) >> (8 - gx_cie_log2_cache_size))
# endif
#endif
			pconc[0] = rt_lookup(0, byte2cache_index(prtc[0]));
			pconc[1] = rt_lookup(1, byte2cache_index(prtc[1]));
			pconc[2] = rt_lookup(2, byte2cache_index(prtc[2]));
			if ( m > 3 )
			  pconc[3] = rt_lookup(3, byte2cache_index(prtc[3]));
#undef byte2cache_index
		  }

#endif				/* !CIE_RENDER_TABLE_INTERPOLATE */
#undef ri
#undef rt_lookup
		return m;
	}
}

/* ================ Utilities ================ */

#define if_debug_vector3(str, vec)\
  if_debug4('c', "%s[%g %g %g]\n", str, vec->u, vec->v, vec->w)
#define if_debug_matrix3(str, mat)\
  if_debug10('c', "%s[%g %g %g / %g %g %g / %g %g %g]\n", str,\
    mat->cu.u, mat->cu.v, mat->cu.w, mat->cv.u, mat->cv.v, mat->cv.w,\
    mat->cw.u, mat->cw.v, mat->cw.w)

/* Multiply a vector by a matrix. */
/* Note that we are computing M * V where v is a column vector. */
private void near
cie_mult3(const gs_vector3 *in, register const gs_matrix3 *mat,
  gs_vector3 *out)
{	if_debug_vector3("[c]mult", in);
	if_debug_matrix3("	*", mat);
	{	float u = in->u, v = in->v, w = in->w;
		out->u = (u * mat->cu.u) + (v * mat->cv.u) + (w * mat->cw.u);
		out->v = (u * mat->cu.v) + (v * mat->cv.v) + (w * mat->cw.v);
		out->w = (u * mat->cu.w) + (v * mat->cv.w) + (w * mat->cw.w);
	}
	if_debug_vector3("	=", out);
}

/* Multiply two matrices.  We assume the result is not an alias for */
/* either of the operands.  Note that the composition of the transformations */
/* M1 followed by M2 is M2 * M1, not M1 * M2.  (See gscie.h for details.) */
private void near
cie_matrix_mult3(const gs_matrix3 *ma, const gs_matrix3 *mb, gs_matrix3 *mc)
{	if_debug_matrix3("[c]matrix_mult", ma);
	if_debug_matrix3("             *", mb);
	cie_mult3(&mb->cu, ma, &mc->cu);
	cie_mult3(&mb->cv, ma, &mc->cv);
	cie_mult3(&mb->cw, ma, &mc->cw);
	cie_matrix_init(mc);
	if_debug_matrix3("             =", mc);
}

/* Invert a matrix. */
/* The output must not be an alias for the input. */
private void near
cie_invert3(register const gs_matrix3 *in, register gs_matrix3 *out)
{	/* This is a brute force algorithm; maybe there are better. */
	/* We label the array elements */
	/*   [ A B C ]   */
	/*   [ D E F ]   */
	/*   [ G H I ]   */
#define A cu.u
#define B cv.u
#define C cw.u
#define D cu.v
#define E cv.v
#define F cw.v
#define G cu.w
#define H cv.w
#define I cw.w
	double coA = in->E * in->I - in->F * in->H; 
	double coB = in->F * in->G - in->D * in->I; 
	double coC = in->D * in->H - in->E * in->G;
	double det = in->A * coA + in->B * coB + in->C * coC;
	if_debug_matrix3("[c]invert", in);
	out->A = coA / det;
	out->D = coB / det;
	out->G = coC / det;
	out->B = (in->C * in->H - in->B * in->I) / det;
	out->E = (in->A * in->I - in->C * in->G) / det;
	out->H = (in->B * in->G - in->A * in->H) / det;
	out->C = (in->B * in->F - in->C * in->E) / det;
	out->F = (in->C * in->D - in->A * in->F) / det;
	out->I = (in->A * in->E - in->B * in->D) / det;
	if_debug_matrix3("        =", out);
#undef A
#undef B
#undef C
#undef D
#undef E
#undef F
#undef G
#undef H
#undef I
	out->is_identity = in->is_identity;
}

/* Look up 3 values in a cache, with cached post-multiplication. */
private void near
cie_lookup_mult3(cie_cached_vector3 *pvec, const gx_cie_vector_cache *pc /*[3]*/)
{
	/****** Interpolating at intermediate stages doesn't seem to ******/
	/****** make things better, and slows things down, so....    ******/
#ifdef CIE_INTERPOLATE_INTERMEDIATE
	/* Interpolate between adjacent cache entries. */
	/* This is expensive! */
#ifdef CIE_CACHE_USE_FIXED
#  define lookup_interpolate_between(v0, v1, i, ftemp)\
     cie_interpolate_between(v0, v1, i)
#else
	float ftu, ftv, ftw;
#  define lookup_interpolate_between(v0, v1, i, ftemp)\
     ((v0) + ((v1) - (v0)) *\
      ((ftemp = float_rshift(i, _cie_interpolate_bits)), ftemp - (int)ftemp))
#endif

	cie_cached_value iu =
	  lookup_index(pvec->u, pc, _cie_interpolate_bits);
	const cie_cached_vector3 *pu =
	  &pc[0].vecs.values[(int)cie_cached_rshift(iu,
						    _cie_interpolate_bits)];
	const cie_cached_vector3 *pu1 =
	  (iu >= (gx_cie_cache_size - 1) << _cie_interpolate_bits ?
	   pu : pu + 1);

	cie_cached_value iv =
	  lookup_index(pvec->v, pc + 1, _cie_interpolate_bits);
	const cie_cached_vector3 *pv =
	  &pc[1].vecs.values[(int)cie_cached_rshift(iv,
						    _cie_interpolate_bits)];
	const cie_cached_vector3 *pv1 =
	  (iv >= (gx_cie_cache_size - 1) << _cie_interpolate_bits ?
	   pv : pv + 1);

	cie_cached_value iw =
	  lookup_index(pvec->w, pc + 2, _cie_interpolate_bits);
	const cie_cached_vector3 *pw =
	  &pc[2].vecs.values[(int)cie_cached_rshift(iw,
						    _cie_interpolate_bits)];
	const cie_cached_vector3 *pw1 =
	  (iw >= (gx_cie_cache_size - 1) << _cie_interpolate_bits ?
	   pw : pw + 1);

	pvec->u = lookup_interpolate_between(pu->u, pu1->u, iu, ftu) +
	  lookup_interpolate_between(pv->u, pv1->u, iv, ftv) +
	  lookup_interpolate_between(pw->u, pw1->u, iw, ftw);
	pvec->v = lookup_interpolate_between(pu->v, pu1->v, iu, ftu) +
	  lookup_interpolate_between(pv->v, pv1->v, iv, ftv) +
	  lookup_interpolate_between(pw->v, pw1->v, iw, ftw);
	pvec->w = lookup_interpolate_between(pu->w, pu1->w, iu, ftu) +
	  lookup_interpolate_between(pv->w, pv1->w, iv, ftv) +
	  lookup_interpolate_between(pw->w, pw1->w, iw, ftw);
#else
	const cie_cached_vector3 *pu = &lookup_value(pvec->u, pc);
	const cie_cached_vector3 *pv = &lookup_value(pvec->v, pc + 1);
	const cie_cached_vector3 *pw = &lookup_value(pvec->w, pc + 2);
	pvec->u = pu->u + pv->u + pw->u;
	pvec->v = pu->v + pv->v + pw->v;
	pvec->w = pu->w + pv->w + pw->w;
#endif
}

/* Set the is_identity flag that accelerates multiplication. */
private void near
cie_matrix_init(register gs_matrix3 *mat)
{	mat->is_identity =
	  mat->cu.u == 1.0 && is_fzero2(mat->cu.v, mat->cu.w) &&
	    mat->cv.v == 1.0 && is_fzero2(mat->cv.u, mat->cv.w) &&
	      mat->cw.w == 1.0 && is_fzero2(mat->cw.u, mat->cw.v);
}
