/* Copyright (C) 1993, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* zchar1.c */
/* Type 1 character display operator */
#include "ghost.h"
#include "errors.h"
#include "oper.h"
#include "gsstruct.h"
#include "gxfixed.h"
#include "gxmatrix.h"
#include "gxchar.h"		/* for gs_type1_init in gstype1.h */
				/* (should only be gschar.h) */
#include "gxdevice.h"		/* for gxfont.h */
#include "gxfont.h"
#include "gxfont1.h"
#include "gxtype1.h"
#include "gzstate.h"		/* for path for gs_type1_init */
				/* (should only be gsstate.h) */
#include "gspaint.h"		/* for gs_fill, gs_stroke */
#include "gspath.h"
#include "estack.h"
#include "ialloc.h"
#include "ichar.h"
#include "icharout.h"
#include "idict.h"
#include "ifont.h"
#include "igstate.h"
#include "store.h"

/* Test whether a font is Type 1 compatible. */
#define font_is_type1_compatible(pfont)\
  ((pfont)->FontType == ft_encrypted || (pfont)->FontType == ft_disk_based)

/* Forward references */
private int type1addpath_continue(P1(os_ptr));
private int type1_call_OtherSubr(P3(gs_type1_state *, int (*)(P1(os_ptr)), const ref *));
private int type1_continue_dispatch(P3(gs_type1_state *, const ref *, ref *));
private int op_type1_cleanup(P1(os_ptr));
private void op_type1_free(P1(os_ptr));

/* ---------------- .type1execchar ---------------- */

/*
 * This is the workhorse for %Type1BuildChar, %Type1BuildGlyph,
 * CCRun, and CID fonts.  Eventually this will appear in the C API;
 * even now, its normal control path doesn't use any continuations.
 */

/* Forward references */
private void
  type1_cis_get_metrics(P2(const gs_type1_state *pcis, float psbw[4]));
/* <font> <code|name> <name> <charstring> .type1execchar - */
private int type1getsbw_continue(P1(os_ptr));
private int bbox_fill(P1(os_ptr));
private int bbox_stroke(P1(os_ptr));
private int nobbox_continue(P1(os_ptr));
private int nobbox_fill(P1(os_ptr));
private int nobbox_stroke(P1(os_ptr));
private int
ztype1execchar(register os_ptr op)
{	gs_font *pfont;
#define pbfont ((gs_font_base *)pfont)
#define pfont1 ((gs_font_type1 *)pfont)
	const gs_type1_data *pdata;
	int code = font_param(op - 3, &pfont);
	gs_show_enum *penum = op_show_find();
	gs_type1_state cis;
#define pcis (&cis)
	int present;
	float sbw[4];
	ref other_subr;

	if ( code < 0 )
	  return code;
	if ( penum == 0 || !font_is_type1_compatible(pfont) )
	  return_error(e_undefined);
	pdata = &pfont1->data;
	/*
	 * Any reasonable implementation would execute something like
	 *	1 setmiterlimit 0 setlinejoin 0 setlinecap
	 * here, but apparently the Adobe implementations aren't reasonable.
	 *
	 * If this is a stroked font, set the stroke width.
	 */
	if ( pfont->PaintType )
	  gs_setlinewidth(igs, pfont->StrokeWidth);
	check_estack(3);	/* for continuations */
	/*
	 * Execute the definition of the character.
	 */
	if ( r_is_proc(op) )
	  return zchar_exec_char_proc(op);
	/*
	 * The definition must be a Type 1 CharString.
	 * Note that we do not require read access: this is deliberate.
	 */
	check_type(*op, t_string);
	if ( r_size(op) <= pdata->lenIV )
	  return_error(e_invalidfont);
	/*
	 * In order to make character oversampling work, we must
	 * set up the cache before calling .type1addpath.
	 * To do this, we must get the bounding box from the FontBBox,
	 * and the width from the CharString or the Metrics.
	 * If the FontBBox isn't valid, we can't do any of this.
	 */
	check_ostack(3);	/* for .type1xxx args */
	present = zchar_get_metrics(pbfont, op - 1, sbw);
	if ( present < 0 )
	  return present;
	/* Establish a current point. */
	code = gs_moveto(igs, 0.0, 0.0);
	if ( code < 0 )
	  return code;
	code = gs_type1_init(pcis, penum, NULL,
			     gs_show_in_charpath(penum) != cpm_show,
			     pfont1->PaintType, pfont1);
	if ( code < 0 )
	  return code;
	if ( pfont1->FontBBox.q.x > pfont1->FontBBox.p.x &&
	     pfont1->FontBBox.q.y > pfont1->FontBBox.p.y
	   )
	  {	/*
		 * We have a valid bounding box.  If we don't have Metrics
		 * for this character, start interpreting the CharString;
		 * do the setcachedevice as soon as we know the
		 * (side bearing and) width.
		 */
		if ( present == metricsNone )
		{	/* Get the width from the CharString, */
			/* then set the cache device. */
			ref cnref;

			/* Since an OtherSubr callout might change osp, */
			/* save the character name now. */
			ref_assign(&cnref, op - 1);
			code = type1_continue_dispatch(pcis, op, &other_subr);
			switch ( code )
			  {
			  default:		/* code < 0 or done, error */
			    return((code < 0 ? code :
				    gs_note_error(e_invalidfont)));
			  case type1_result_callothersubr:	/* unknown OtherSubr */
			    return type1_call_OtherSubr(pcis,
							type1getsbw_continue,
							&other_subr);
			  case type1_result_sbw:	/* [h]sbw, done */
			    break;
			  }
			type1_cis_get_metrics(pcis, sbw);
			return zchar_set_cache(osp, pbfont, &cnref,
					       NULL, sbw + 2,
					       &pfont1->FontBBox,
					       bbox_fill, bbox_stroke);
		}
		else
		{	/* We have the width and bounding box: */
			/* set up the cache device now. */
			return zchar_set_cache(op, pbfont, op - 1,
					       (present ==
						metricsSideBearingAndWidth ?
						sbw : NULL),
					       sbw + 2, &pfont1->FontBBox,
					       bbox_fill, bbox_stroke);
		}
	  }
	else
	  {	/*
		 * The FontBBox is not valid.  In this case,
		 * we do the .type1addpath first, then the setcachedevice.
		 * Oversampling is not possible.
		 */
		const ref *opstr = op;
		if ( present == metricsSideBearingAndWidth )
		{	gs_point sbpt;
			sbpt.x = sbw[0], sbpt.y = sbw[1];
			gs_type1_set_lsb(pcis, &sbpt);
		}
		/* Continue interpreting. */
icont:		code = type1_continue_dispatch(pcis, opstr, &other_subr);
		switch ( code )
		  {
		  case 0:			/* all done */
		    break;
		  default:			/* code < 0, error */
		    return code;
		  case type1_result_callothersubr:	/* unknown OtherSubr */
		    push_op_estack(nobbox_continue);
		    return type1_call_OtherSubr(pcis, type1addpath_continue,
						&other_subr);
		  case type1_result_sbw:	/* [h]sbw, just continue */
		    opstr = 0;
		    goto icont;
		  }
		pop(1);  op -= 1;		/* pop the charstring */
		return nobbox_continue(op);
	  }
#undef pcis
#undef pfont1
#undef pbfont
}

/* Handle the results of gs_type1_interpret. */
/* pcref points to a t_string ref. */
private int
type1_continue_dispatch(gs_type1_state *pcis, const ref *pcref, ref *pos)
{	int value;
	int code;
	gs_const_string charstring;
	gs_const_string *pchars;

	if ( pcref == 0 )
	  {	pchars = 0;
	  }
	else
	  {	charstring.data = pcref->value.const_bytes;
		charstring.size = r_size(pcref);
		pchars = &charstring;
	  }
	code = gs_type1_interpret(pcis, pchars, &value);
	switch ( code )
	{
	case type1_result_callothersubr:
	{	/* The Type 1 interpreter handles all known OtherSubrs, */
		/* so this must be an unknown one. */
		font_data *pfdata = pfont_data(gs_currentfont(igs));
		code = array_get(&pfdata->u.type1.OtherSubrs,
				 (long)value, pos);
		return (code < 0 ? code : type1_result_callothersubr);
	}
	}
	return code;
}

/* Do a callout to an OtherSubr implemented in PostScript. */
/* The caller must have done a check_estack. */
private int
type1_call_OtherSubr(gs_type1_state *pcis, int (*cont)(P1(os_ptr)),
  const ref *pos)
{	/* Move the Type 1 interpreter state to the heap. */
	gs_type1_state *hpcis = ialloc_struct(gs_type1_state,
					     &st_gs_type1_state,
					     ".type1addpath");

	if ( hpcis == 0 )
	  return_error(e_VMerror);
	*hpcis = *pcis;
	push_mark_estack(es_show, op_type1_cleanup);
	++esp;
	make_istruct(esp, 0, hpcis);
	push_op_estack(cont);
	++esp;
	*esp = *pos;
	return o_push_estack;
}

/* Continue from an OtherSubr callout while getting metrics. */
private int
type1getsbw_continue(os_ptr op)
{	ref other_subr;
	gs_type1_state *pcis = r_ptr(esp, gs_type1_state);
	int code;

	check_ostack(3);	/* for returning the result */
	code = type1_continue_dispatch(pcis, NULL, &other_subr);
	op = osp;		/* in case z1_push/pop_proc was called */
	switch ( code )
	{
	default:		/* code < 0 or done, error */
		op_type1_free(op);
		return((code < 0 ? code : gs_note_error(e_invalidfont)));
	case type1_result_callothersubr:	/* unknown OtherSubr */
		push_op_estack(type1getsbw_continue);
		++esp;
		*esp = other_subr;
		return o_push_estack;
	case type1_result_sbw:			/* [h]sbw, done */
	  {	float sbw[4];
		const gs_font_base *pbfont =
		  (const gs_font_base *)pcis->pfont;

		/* Get the metrics before freeing the state. */
		type1_cis_get_metrics(pcis, sbw);
		op_type1_free(op);
		return zchar_set_cache(op, pbfont, op, sbw, sbw + 2,
				       &pbfont->FontBBox,
				       bbox_fill, bbox_stroke);
	  }
	}
}

/* Clean up after a Type 1 callout. */
private int
op_type1_cleanup(os_ptr op)
{	ifree_object(r_ptr(esp + 2, void), "op_type1_cleanup");
	return 0;
}
private void
op_type1_free(os_ptr op)
{	/*
	 * In order to avoid popping from the e-stack and then pushing onto
	 * it, which would violate an interpreter invariant, we simply
	 * overwrite the two e-stack items being discarded (hpcis and the
	 * cleanup operator) with empty procedures.
	 */
	make_empty_const_array(esp - 1, a_readonly + a_executable);
	make_empty_const_array(esp, a_readonly + a_executable);
	op_type1_cleanup(op);
}

/* <font> <code|name> <name> <charstring> <sbx> <sby> %bbox_{fill|stroke} - */
/* <font> <code|name> <name> <charstring> %bbox_{fill|stroke} - */
private int bbox_finish(P2(os_ptr, int (*)(P1(os_ptr))));
private int
bbox_fill(os_ptr op)
{	return bbox_finish(op, nobbox_fill);
}
private int
bbox_stroke(os_ptr op)
{	return bbox_finish(op, nobbox_stroke);
}
private int
bbox_finish(os_ptr op, int (*cont)(P1(os_ptr)))
{	gs_font *pfont;
#define pfont1 ((gs_font_type1 *)pfont)
	gs_type1_data *pdata;
	int code;
	gs_show_enum *penum = op_show_find();
	gs_type1_state cis;		/* stack allocate to avoid sandbars */
#define pcis (&cis)
	float sbxy[2];
	gs_point sbpt;
	gs_point *psbpt = 0;
	os_ptr opc = op;
	const ref *opstr;
	ref other_subr;

	if ( !r_has_type(opc, t_string) )
	  {	check_op(3);
		code = num_params(op, 2, sbxy);
		if ( code < 0 )
		  return code;
		sbpt.x = sbxy[0];
		sbpt.y = sbxy[1];
		psbpt = &sbpt;
		opc -= 2;
		check_type(*opc, t_string);
	  }
	code = font_param(opc - 3, &pfont);
	if ( code < 0 )
	  return code;
	if ( penum == 0 || !font_is_type1_compatible(pfont) )
	  return_error(e_undefined);
	pdata = &pfont1->data;
	if ( r_size(opc) <= pdata->lenIV )
	  return_error(e_invalidfont);
	check_estack(5);	/* in case we need to do a callout */
	code = gs_type1_init(pcis, penum, psbpt,
			     gs_show_in_charpath(penum) != cpm_show,
			     pfont1->PaintType, pfont1);
	if ( code < 0 )
	  return code;
	opstr = opc;
icont:	code = type1_continue_dispatch(pcis, opstr, &other_subr);
	switch ( code )
	{
	case 0:			/* all done */
		/* Call the continuation now. */
		pop((psbpt == 0 ? 1 : 3));
		return (*cont)(osp);
	default:		/* code < 0, error */
		return code;
	case type1_result_callothersubr:	/* unknown OtherSubr */
		push_op_estack(cont);		/* call later */
		return type1_call_OtherSubr(pcis, type1addpath_continue,
					    &other_subr);
	case type1_result_sbw:			/* [h]sbw, just continue */
		opstr = 0;
		goto icont;
	}
	pop((psbpt == 0 ? 1 : 3));
	return code;
#undef pfont1
#undef pcis
}

/* Continue from an OtherSubr callout. */
private int
type1addpath_continue(os_ptr op)
{	ref other_subr;
	gs_type1_state *pcis = r_ptr(esp, gs_type1_state);
	int code;
cont:	code = type1_continue_dispatch(pcis, NULL, &other_subr);
	op = osp;		/* in case z1_push/pop_proc was called */
	switch ( code )
	{
	case 0:			/* all done */
	{	/* Assume the OtherSubrs didn't mess with the o-stack.... */
		int npop = (r_has_type(op, t_string) ? 1 : 3);
		pop(npop);  op -= npop;
		op_type1_free(op);
		return 0;
	}
	default:		/* code < 0 or done, error */
		op_type1_free(op);
		return((code < 0 ? code : gs_note_error(e_invalidfont)));
	case type1_result_callothersubr:	/* unknown OtherSubr */
		push_op_estack(type1addpath_continue);
		++esp;
		*esp = other_subr;
		return o_push_estack;
	case type1_result_sbw:			/* [h]sbw, just continue */
		goto cont;
	}
}

/* Finish the no-FontBBox case after constructing the path. */
/* <font> <code|name> <name> %nobbox_continue - */
private int
nobbox_continue(os_ptr op)
{	int code;
	gs_rect bbox;
	gs_font *pfont;
#define pbfont ((gs_font_base *)pfont)
	float sbw[4];

	if ( (code = gs_pathbbox(igs, &bbox)) < 0 ||
	     (code = font_param(op - 2, &pfont)) < 0 ||
	     (code = zchar_get_metrics(pbfont, op, sbw)) < 0
	   )
	  return code;
	if ( code == metricsNone )
	{	gs_point endpt;
		if ( (code = gs_currentpoint(igs, &endpt)) < 0 )
		  return code;
		sbw[2] = endpt.x, sbw[3] = endpt.y;
	}
	return zchar_set_cache(op, pbfont, op, NULL, sbw + 2, &bbox,
			       nobbox_fill, nobbox_stroke);
#undef pbfont
}
/* Finish by popping the operands and filling or stroking. */
private int
nobbox_fill(os_ptr op)
{	pop(3);
	return gs_fill(igs);
}
private int
nobbox_stroke(os_ptr op)
{	pop(3);
	return gs_stroke(igs);
}

/* ------ Internal procedures ------ */

/* Get the metrics (l.s.b. and width) from the Type 1 interpreter. */
private void
type1_cis_get_metrics(const gs_type1_state *pcis, float psbw[4])
{	psbw[0] = fixed2float(pcis->lsb.x);
	psbw[1] = fixed2float(pcis->lsb.y);
	psbw[2] = fixed2float(pcis->width.x);
	psbw[3] = fixed2float(pcis->width.y);
}

/* ------ Initialization procedure ------ */

BEGIN_OP_DEFS(zchar1_op_defs) {
	{"4.type1execchar", ztype1execchar},
		/* Internal operators */
	{"0%type1addpath_continue", type1addpath_continue},
	{"3%nobbox_fill", nobbox_fill},
	{"3%nobbox_stroke", nobbox_stroke},
	{"0%type1getsbw_continue", type1getsbw_continue},
	{"4%bbox_fill", bbox_fill},
	{"4%bbox_stroke", bbox_stroke},
	{"3%nobbox_continue", nobbox_continue},
END_OP_DEFS(0) }

/* ------ Auxiliary procedures for type 1 fonts ------ */

/* These are exported for zfont1.c. */

int
z1_subr_proc(gs_font_type1 *pfont, int index, gs_const_string *pstr)
{	const font_data *pfdata = pfont_data(pfont);
	ref subr;
	int code;

	code = array_get(&pfdata->u.type1.Subrs, index, &subr);
	if ( code < 0 )
	  return code;
	check_type_only(subr, t_string);
	pstr->data = subr.value.const_bytes;
	pstr->size = r_size(&subr);
	return 0;
}

int
z1_seac_proc(gs_font_type1 *pfont, int index, gs_const_string *pstr)
{	const font_data *pfdata = pfont_data(pfont);
	ref *pcstr;
	ref enc_entry;
	int code = array_get(&StandardEncoding, (long)index, &enc_entry);

	if ( code < 0 )
	  return code;
	if ( dict_find(&pfdata->CharStrings, &enc_entry, &pcstr) <= 0 )
	  return_error(e_undefined);
	check_type_only(*pcstr, t_string);
	pstr->data = pcstr->value.const_bytes;
	pstr->size = r_size(pcstr);
	return 0;
}

int
z1_push_proc(gs_font_type1 *ignore, const fixed *pf, int count)
{	const fixed *p = pf + count - 1;
	int i;

	check_ostack(count);
	for ( i = 0; i < count; i++, p-- )
	  {	osp++;
		make_real(osp, fixed2float(*p));
	  }
	return 0;
}

int
z1_pop_proc(gs_font_type1 *ignore, fixed *pf)
{	float val;
	int code = num_params(osp, 1, &val);

	if ( code < 0 )
	  return code;
	*pf = float2fixed(val);
	osp--;
	return 0;
}
