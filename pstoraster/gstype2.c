/* Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gstype2.c,v 1.1 2000/03/08 23:14:49 mike Exp $ */
/* Adobe Type 2 charstring interpreter */
#include "math_.h"
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsstruct.h"
#include "gxarith.h"
#include "gxfixed.h"
#include "gxmatrix.h"
#include "gxcoord.h"
#include "gxistate.h"
#include "gzpath.h"
#include "gxfont.h"
#include "gxfont1.h"
#include "gxtype1.h"

/* NOTE: The following are not yet implemented:
 *	Registry items other than 0
 *	Counter masks (but they are parsed correctly)
 *	'random' operator
 */

/* Define a pointer to the charstring interpreter stack. */
typedef fixed *cs_ptr;

/* ------ Internal routines ------ */

/*
 * Set the character width.  This is provided as an optional extra operand
 * on the stack for the first operator.  After setting the width, we remove
 * the extra operand, and back up the interpreter pointer so we will
 * re-execute the operator when control re-enters the interpreter.
 */
#define check_first_operator(explicit_width)\
  BEGIN\
    if ( pcis->init_done < 0 )\
      { ipsp->ip = cip, ipsp->dstate = state;\
	return type2_sbw(pcis, csp, cstack, ipsp, explicit_width);\
      }\
  END
private int
type2_sbw(gs_type1_state * pcis, cs_ptr csp, cs_ptr cstack, ip_state * ipsp,
	  bool explicit_width)
{
    fixed wx;

    if (explicit_width) {
	wx = cstack[0] + pcis->pfont->data.nominalWidthX;
	memmove(cstack, cstack + 1, (csp - cstack) * sizeof(*cstack));
	--csp;
    } else
	wx = pcis->pfont->data.defaultWidthX;
    gs_type1_sbw(pcis, fixed_0, fixed_0, wx, fixed_0);
    /* Back up the interpretation pointer. */
    {
	ip_state *ipsp = &pcis->ipstack[pcis->ips_count - 1];

	ipsp->ip--;
	decrypt_skip_previous(*ipsp->ip, ipsp->dstate);
    }
    /* Save the interpreter state. */
    pcis->os_count = csp + 1 - cstack;
    pcis->ips_count = ipsp - &pcis->ipstack[0] + 1;
    memcpy(pcis->ostack, cstack, pcis->os_count * sizeof(cstack[0]));
    if (pcis->init_done < 0) {	/* Finish init when we return. */
	pcis->init_done = 0;
    }
    return type1_result_sbw;
}
private int
type2_vstem(gs_type1_state * pcis, cs_ptr csp, cs_ptr cstack)
{
    fixed x = 0;
    cs_ptr ap;

    apply_path_hints(pcis, false);
    for (ap = cstack; ap + 1 <= csp; x += ap[1], ap += 2)
	type1_vstem(pcis, x += ap[0], ap[1]);
    pcis->num_hints += (csp + 1 - cstack) >> 1;
    return 0;
}

/* Enable only the hints selected by a mask. */
private void
enable_hints(stem_hint_table * psht, const byte * mask)
{
    stem_hint *table = &psht->data[0];
    stem_hint *ph = table + psht->current;

    for (ph = &table[psht->count]; --ph >= table;) {
	ph->active = (mask[ph->index >> 3] & (0x80 >> (ph->index & 7))) != 0;
	if_debug6('1', "[1]  %s %u: %g(%g),%g(%g)\n",
		  (ph->active ? "enable" : "disable"), ph->index,
		  fixed2float(ph->v0), fixed2float(ph->dv0),
		  fixed2float(ph->v1), fixed2float(ph->dv1));
    }
}

/* ------ Main interpreter ------ */

/*
 * Continue interpreting a Type 2 charstring.  If str != 0, it is taken as
 * the byte string to interpret.  Return 0 on successful completion, <0 on
 * error, or >0 when client intervention is required (or allowed).  The int*
 * argument is only for compatibility with the Type 1 charstring interpreter.
 */
private int
gs_type2_charstring_interpret(gs_type1_state * pcis,
			    const gs_const_string * str, int *ignore_pindex)
{
    gs_font_type1 *pfont = pcis->pfont;
    gs_type1_data *pdata = &pfont->data;
    bool encrypted = pdata->lenIV >= 0;
    gs_op1_state s;
    fixed cstack[ostack_size];
    cs_ptr csp;

#define clear csp = cstack - 1
    ip_state *ipsp = &pcis->ipstack[pcis->ips_count - 1];
    register const byte *cip;
    register crypt_state state;
    register int c;
    cs_ptr ap;
    bool vertical;
    int code = 0;

/****** FAKE THE REGISTRY ******/
    struct {
	float *values;
	uint size;
    } Registry[1];

    Registry[0].values = pcis->pfont->data.WeightVector.values;

    switch (pcis->init_done) {
	case -1:
	    break;
	case 0:
	    gs_type1_finish_init(pcis, &s);	/* sets sfc, ptx, pty, origin */
	    break;
	default /*case 1 */ :
	    ptx = pcis->position.x;
	    pty = pcis->position.y;
	    sfc = pcis->fc;
    }
    sppath = pcis->path;
    s.pcis = pcis;
    init_cstack(cstack, csp, pcis);

    if (str == 0)
	goto cont;
    ipsp->char_string = *str;
    cip = str->data;
  call:state = crypt_charstring_seed;
    if (encrypted) {
	int skip = pdata->lenIV;

	/* Skip initial random bytes */
	for (; skip > 0; ++cip, --skip)
	    decrypt_skip_next(*cip, state);
    }
    goto top;
  cont:cip = ipsp->ip;
    state = ipsp->dstate;
  top:for (;;) {
	uint c0 = *cip++;

	charstring_next(c0, state, c, encrypted);
	if (c >= c_num1) {
	    /* This is a number, decode it and push it on the stack. */

	    if (c < c_pos2_0) {	/* 1-byte number */
		decode_push_num1(csp, c);
	    } else if (c < cx_num4) {	/* 2-byte number */
		decode_push_num2(csp, c, cip, state, encrypted);
	    } else if (c == cx_num4) {	/* 4-byte number */
		long lw;

		decode_num4(lw, cip, state, encrypted);
		/* 32-bit numbers are 16:16. */
		*++csp = arith_rshift(lw, 16 - _fixed_shift);
	    } else		/* not possible */
		return_error(gs_error_invalidfont);
	  pushed:if_debug3('1', "[1]%d: (%d) %f\n",
		      (int)(csp - cstack), c, fixed2float(*csp));
	    continue;
	}
#ifdef DEBUG
	if (gs_debug['1']) {
	    static const char *const c2names[] =
	    {char2_command_names};

	    if (c2names[c] == 0)
		dlprintf2("[1]0x%lx: %02x??\n", (ulong) (cip - 1), c);
	    else
		dlprintf3("[1]0x%lx: %02x %s\n", (ulong) (cip - 1), c,
			  c2names[c]);
	}
#endif
	switch ((char_command) c) {
#define cnext clear; goto top

		/* Commands with identical functions in Type 1 and Type 2, */
		/* except for 'escape'. */

	    case c_undef0:
	    case c_undef2:
	    case c_undef17:
		return_error(gs_error_invalidfont);
	    case c_callsubr:
		c = fixed2int_var(*csp) + pdata->subroutineNumberBias;
		code = (*pdata->procs->subr_data)
		    (pfont, c, false, &ipsp[1].char_string);
	      subr:if (code < 0)
		    return_error(code);
		--csp;
		ipsp->ip = cip, ipsp->dstate = state;
		++ipsp;
		cip = ipsp->char_string.data;
		goto call;
	    case c_return:
		--ipsp;
		goto cont;
	    case c_undoc15:
		/* See gstype1.h for information on this opcode. */
		cnext;

		/* Commands with similar but not identical functions */
		/* in Type 1 and Type 2 charstrings. */

	    case cx_hstem:
		goto hstem;
	    case cx_vstem:
		goto vstem;
	    case cx_vmoveto:
		check_first_operator(csp > cstack);
		accum_y(*csp);
	      move:if ((pcis->hint_next != 0 || path_is_drawing(sppath)))
		    apply_path_hints(pcis, true);
		code = gx_path_add_point(sppath, ptx, pty);
	      cc:if (code < 0)
		    return code;
		goto pp;
	    case cx_rlineto:
		for (ap = cstack; ap + 1 <= csp; ap += 2) {
		    accum_xy(ap[0], ap[1]);
		    code = gx_path_add_line(sppath, ptx, pty);
		    if (code < 0)
			return code;
		}
	      pp:if_debug2('1', "[1]pt=(%g,%g)\n",
			  fixed2float(ptx), fixed2float(pty));
		cnext;
	    case cx_hlineto:
		vertical = false;
		goto hvl;
	    case cx_vlineto:
		vertical = true;
	      hvl:for (ap = cstack; ap <= csp; vertical = !vertical, ++ap) {
		    if (vertical)
			accum_y(*ap);
		    else
			accum_x(*ap);
		    code = gx_path_add_line(sppath, ptx, pty);
		    if (code < 0)
			return code;
		}
		goto pp;
	    case cx_rrcurveto:
		for (ap = cstack; ap + 5 <= csp; ap += 6) {
		    code = gs_op1_rrcurveto(&s, ap[0], ap[1], ap[2],
					    ap[3], ap[4], ap[5]);
		    if (code < 0)
			return code;
		}
		goto pp;
	    case cx_endchar:
		/*
		 * It is an undocumented (!) feature of Type 2 CharStrings
		 * that if endchar is invoked with 4 or 5 operands, it is
		 * equivalent to the Type 1 seac operator!  In this case,
		 * the asb operand of seac is missing: we assume it is
		 * the same as the l.s.b. of the accented character.
		 */
		if (csp >= cstack + 3) {
		    check_first_operator(csp > cstack + 3);
		    code = gs_type1_seac(pcis, cstack, pcis->lsb.x, ipsp);
		    if (code < 0)
			return code;
		    clear;
		    cip = ipsp->char_string.data;
		    goto call;
		}
		/*
		 * This might be the only operator in the charstring.
		 * In this case, there might be a width on the stack.
		 */
		check_first_operator(csp >= cstack);
		code = gs_type1_endchar(pcis);
		if (code == 1) {
		    /*
		     * Reset the total hint count so that hintmask will
		     * parse its following data correctly.
		     * (gs_type1_endchar already reset the actual hint
		     * tables.)
		     */
		    pcis->num_hints = 0;
		    /* do accent of seac */
		    spt = pcis->position;
		    ipsp = &pcis->ipstack[pcis->ips_count - 1];
		    cip = ipsp->char_string.data;
		    goto call;
		}
		return code;
	    case cx_rmoveto:
		check_first_operator(csp > cstack + 1);
		accum_xy(csp[-1], *csp);
		goto move;
	    case cx_hmoveto:
		check_first_operator(csp > cstack);
		accum_x(*csp);
		goto move;
	    case cx_vhcurveto:
		vertical = true;
		goto hvc;
	    case cx_hvcurveto:
		vertical = false;
	      hvc:for (ap = cstack; ap + 3 <= csp; vertical = !vertical, ap += 4) {
		    gs_fixed_point pt1, pt2;
		    fixed ax0 = sppath->position.x - ptx;
		    fixed ay0 = sppath->position.y - pty;

		    if (vertical)
			accum_y(ap[0]);
		    else
			accum_x(ap[0]);
		    pt1.x = ptx + ax0, pt1.y = pty + ay0;
		    accum_xy(ap[1], ap[2]);
		    pt2.x = ptx, pt2.y = pty;
		    if (vertical) {
			if (ap + 4 == csp)
			    accum_xy(ap[3], ap[4]);
			else
			    accum_x(ap[3]);
		    } else {
			if (ap + 4 == csp)
			    accum_xy(ap[4], ap[3]);
			else
			    accum_y(ap[3]);
		    }
		    code = gx_path_add_curve(sppath, pt1.x, pt1.y,
					     pt2.x, pt2.y, ptx, pty);
		    if (code < 0)
			return code;
		}
		goto pp;

			/***********************
			 * New Type 2 commands *
			 ***********************/

	    case c2_blend:
		{
		    int n = fixed2int_var(*csp);
		    int num_values = csp - cstack;
		    gs_font_type1 *pfont = pcis->pfont;
		    int k = pfont->data.WeightVector.count;
		    int i, j;
		    cs_ptr base, deltas;

		    base = csp - 1 - num_values;
		    deltas = base + n - 1;
		    for (j = 0; j < n; j++, base++, deltas += k - 1)
			for (i = 1; i < k; i++)
			    *base += deltas[i] * pfont->data.WeightVector.values[i];
		}
		cnext;
	    case c2_hstemhm:
		pcis->have_hintmask = true;
	      hstem:check_first_operator(!((csp - cstack) & 1));
		apply_path_hints(pcis, false);
		{
		    fixed x = 0;

		    for (ap = cstack; ap + 1 <= csp; x += ap[1], ap += 2)
			type1_hstem(pcis, x += ap[0], ap[1]);
		}
		pcis->num_hints += (csp + 1 - cstack) >> 1;
		cnext;
	    case c2_hintmask:
		/*
		 * A hintmask at the beginning of the CharString is
		 * equivalent to vstemhm + hintmask.  For simplicity, we use
		 * this interpretation everywhere.
		 */
		pcis->have_hintmask = true;
		check_first_operator(!((csp - cstack) & 1));
		type2_vstem(pcis, csp, cstack);
		clear;
		/* (falls through) */
	    case c2_cntrmask:
		{
		    byte mask[max_total_stem_hints / 8];
		    int i;

		    if_debug3('1', "[1]mask[%d:%dv,%dh]", pcis->num_hints,
			  pcis->vstem_hints.count, pcis->hstem_hints.count);
		    for (i = 0; i < pcis->num_hints; ++cip, i += 8) {
			charstring_next(*cip, state, mask[i >> 3], encrypted);
			if_debug1('1', " 0x%02x", mask[i >> 3]);
		    }
		    if_debug0('1', "\n");
		    ipsp->ip = cip;
		    ipsp->dstate = state;
		    if (c == c2_cntrmask) {
/****** NYI ******/
		    } else {	/* hintmask or equivalent */
			if_debug0('1', "[1]hstem hints:\n");
			enable_hints(&pcis->hstem_hints, mask);
			if_debug0('1', "[1]vstem hints:\n");
			enable_hints(&pcis->vstem_hints, mask);
		    }
		}
		break;
	    case c2_vstemhm:
		pcis->have_hintmask = true;
	      vstem:check_first_operator(!((csp - cstack) & 1));
		type2_vstem(pcis, csp, cstack);
		cnext;
	    case c2_rcurveline:
		for (ap = cstack; ap + 5 <= csp; ap += 6) {
		    code = gs_op1_rrcurveto(&s, ap[0], ap[1], ap[2], ap[3],
					    ap[4], ap[5]);
		    if (code < 0)
			return code;
		}
		accum_xy(ap[0], ap[1]);
		code = gx_path_add_line(sppath, ptx, pty);
		goto cc;
	    case c2_rlinecurve:
		for (ap = cstack; ap + 7 <= csp; ap += 2) {
		    accum_xy(ap[0], ap[1]);
		    code = gx_path_add_line(sppath, ptx, pty);
		    if (code < 0)
			return code;
		}
		code = gs_op1_rrcurveto(&s, ap[0], ap[1], ap[2], ap[3],
					ap[4], ap[5]);
		goto cc;
	    case c2_vvcurveto:
		ap = cstack;
		{
		    int n = csp + 1 - cstack;
		    fixed dxa = (n & 1 ? *ap++ : 0);

		    for (; ap + 3 <= csp; ap += 4) {
			code = gs_op1_rrcurveto(&s, dxa, ap[0], ap[1], ap[2],
						fixed_0, ap[3]);
			if (code < 0)
			    return code;
			dxa = 0;
		    }
		}
		goto pp;
	    case c2_hhcurveto:
		ap = cstack;
		{
		    int n = csp + 1 - cstack;
		    fixed dya = (n & 1 ? *ap++ : 0);

		    for (; ap + 3 <= csp; ap += 4) {
			code = gs_op1_rrcurveto(&s, ap[0], dya, ap[1], ap[2],
						ap[3], fixed_0);
			if (code < 0)
			    return code;
			dya = 0;
		    }
		}
		goto pp;
	    case c2_shortint:
		{
		    int c1, c2;

		    charstring_next(*cip, state, c1, encrypted);
		    ++cip;
		    charstring_next(*cip, state, c2, encrypted);
		    ++cip;
		    *++csp = int2fixed((((c1 ^ 0x80) - 0x80) << 8) + c2);
		}
		goto pushed;
	    case c2_callgsubr:
		c = fixed2int_var(*csp) + pdata->gsubrNumberBias;
		code = (*pdata->procs->subr_data)
		    (pfont, c, true, &ipsp[1].char_string);
		goto subr;
	    case cx_escape:
		charstring_next(*cip, state, c, encrypted);
		++cip;
#ifdef DEBUG
		if (gs_debug['1'] && c < char2_extended_command_count) {
		    static const char *const ce2names[] =
		    {char2_extended_command_names};

		    if (ce2names[c] == 0)
			dlprintf2("[1]0x%lx: %02x??\n", (ulong) (cip - 1), c);
		    else
			dlprintf3("[1]0x%lx: %02x %s\n", (ulong) (cip - 1), c,
				  ce2names[c]);
		}
#endif
		switch ((char2_extended_command) c) {
		    case ce2_and:
			csp[-1] = ((csp[-1] != 0) & (*csp != 0) ? fixed_1 : 0);
			--csp;
			break;
		    case ce2_or:
			csp[-1] = (csp[-1] | *csp ? fixed_1 : 0);
			--csp;
			break;
		    case ce2_not:
			*csp = (*csp ? 0 : fixed_1);
			break;
		    case ce2_store:
			{
			    int i, n = fixed2int_var(*csp);
			    float *to = Registry[fixed2int_var(csp[-3])].values +
			    fixed2int_var(csp[-2]);
			    const fixed *from =
			    pcis->transient_array + fixed2int_var(csp[-1]);

			    for (i = 0; i < n; ++i)
				to[i] = fixed2float(from[i]);
			}
			csp -= 4;
			break;
		    case ce2_abs:
			if (*csp < 0)
			    *csp = -*csp;
			break;
		    case ce2_add:
			csp[-1] += *csp;
			--csp;
			break;
		    case ce2_sub:
			csp[-1] -= *csp;
			--csp;
			break;
		    case ce2_div:
			csp[-1] = float2fixed((double)csp[-1] / *csp);
			--csp;
			break;
		    case ce2_load:
			/* The specification says there is no j (starting index */
			/* in registry array) argument.... */
			{
			    int i, n = fixed2int_var(*csp);
			    const float *from = Registry[fixed2int_var(csp[-2])].values;
			    fixed *to =
			    pcis->transient_array + fixed2int_var(csp[-1]);

			    for (i = 0; i < n; ++i)
				to[i] = float2fixed(from[i]);
			}
			csp -= 3;
			break;
		    case ce2_neg:
			*csp = -*csp;
			break;
		    case ce2_eq:
			csp[-1] = (csp[-1] == *csp ? fixed_1 : 0);
			--csp;
			break;
		    case ce2_drop:
			--csp;
			break;
		    case ce2_put:
			pcis->transient_array[fixed2int_var(*csp)] = csp[-1];
			csp -= 2;
			break;
		    case ce2_get:
			*csp = pcis->transient_array[fixed2int_var(*csp)];
			break;
		    case ce2_ifelse:
			if (csp[-1] > *csp)
			    csp[-3] = csp[-2];
			csp -= 3;
			break;
		    case ce2_random:
			++csp;
			/****** NYI ******/
			break;
		    case ce2_mul:
			{
			    double prod = fixed2float(csp[-1]) * *csp;

			    csp[-1] =
				(prod > max_fixed ? max_fixed :
				 prod < min_fixed ? min_fixed : prod);
			}
			--csp;
			break;
		    case ce2_sqrt:
			if (*csp >= 0)
			    *csp = float2fixed(sqrt(fixed2float(*csp)));
			break;
		    case ce2_dup:
			csp[1] = *csp;
			++csp;
			break;
		    case ce2_exch:
			{
			    fixed top = *csp;

			    *csp = csp[-1], csp[-1] = top;
			}
			break;
		    case ce2_index:
			*csp =
			    (*csp < 0 ? csp[-1] : csp[-1 - fixed2int_var(csp[-1])]);
			break;
		    case ce2_roll:
			{
			    int distance = fixed2int_var(*csp);
			    int count = fixed2int_var(csp[-1]);
			    cs_ptr bot;

			    csp -= 2;
			    if (count < 0 || count > csp + 1 - cstack)
				return_error(gs_error_invalidfont);
			    if (count == 0)
				break;
			    if (distance < 0)
				distance = count - (-distance % count);
			    bot = csp + 1 - count;
			    while (--distance >= 0) {
				fixed top = *csp;

				memmove(bot + 1, bot,
					(count - 1) * sizeof(fixed));
				*bot = top;
			    }
			}
			break;
		    case ce2_hflex:
			csp[6] = fixed_half;	/* fd/100 */
			csp[4] = *csp, csp[5] = 0;	/* dx6, dy6 */
			csp[2] = csp[-1], csp[3] = -csp[-5];	/* dx5, dy5 */
			*csp = csp[-2], csp[1] = 0;	/* dx4, dy4 */
			csp[-2] = csp[-3], csp[-1] = 0;		/* dx3, dy3 */
			csp[-3] = csp[-4], csp[-4] = csp[-5];	/* dx2, dy2 */
			csp[-5] = 0;	/* dy1 */
			csp += 6;
			goto flex;
		    case ce2_flex:
			*csp /= 100;	/* fd/100 */
flex:			{
			    fixed x_join = csp[-12] + csp[-10] + csp[-8];
			    fixed y_join = csp[-11] + csp[-9] + csp[-7];
			    fixed x_end = x_join + csp[-6] + csp[-4] + csp[-2];
			    fixed y_end = y_join + csp[-5] + csp[-3] + csp[-1];
			    gs_point join, end;
			    double flex_depth;

			    if ((code =
				 gs_distance_transform(fixed2float(x_join),
						       fixed2float(y_join),
						       &ctm_only(pcis->pis),
						       &join)) < 0 ||
				(code =
				 gs_distance_transform(fixed2float(x_end),
						       fixed2float(y_end),
						       &ctm_only(pcis->pis),
						       &end)) < 0
				)
				return code;
			    /*
			     * Use the X or Y distance depending on whether
			     * the curve is more horizontal or more
			     * vertical.
			     */
			    if (any_abs(end.y) > any_abs(end.x))
				flex_depth = join.x;
			    else
				flex_depth = join.y;
			    if (fabs(flex_depth) < fixed2float(*csp)) {
				/* Do flex as line. */
				accum_xy(x_end, y_end);
				code = gx_path_add_line(sppath, ptx, pty);
			    } else {
				/*
				 * Do flex as curve.  We can't jump to rrc,
				 * because the flex operators don't clear
				 * the stack (!).
				 */
				code = gs_op1_rrcurveto(&s,
					csp[-12], csp[-11], csp[-10],
					csp[-9], csp[-8], csp[-7]);
				if (code < 0)
				    return code;
				code = gs_op1_rrcurveto(&s,
					csp[-6], csp[-5], csp[-4],
					csp[-3], csp[-2], csp[-1]);
			    }
			    if (code < 0)
				return code;
			    csp -= 13;
			}
			cnext;
		    case ce2_hflex1:
			csp[4] = fixed_half;	/* fd/100 */
			csp[2] = *csp, csp[3] = 0;	/* dx6, dy6 */
			*csp = csp[-2], csp[1] = csp[-1];	/* dx5, dy5 */
			csp[-2] = csp[-3], csp[-1] = 0;		/* dx4, dy4 */
			csp[-3] = 0;	/* dy3 */
			csp += 4;
			goto flex;
		    case ce2_flex1:
			{
			    fixed dx = csp[-10] + csp[-8] + csp[-6] + csp[-4] + csp[-2];
			    fixed dy = csp[-9] + csp[-7] + csp[-5] + csp[-3] + csp[-1];

			    if (any_abs(dx) > any_abs(dy))
				csp[1] = -dy;	/* d6 is dx6 */
			    else
				csp[1] = *csp, *csp = -dx;	/* d6 is dy6 */
			}
			csp[2] = fixed_half;	/* fd/100 */
			csp += 2;
			goto flex;
		}
		break;

		/* Fill up the dispatch up to 32. */

	      case_c2_undefs:
	    default:		/* pacify compiler */
		return_error(gs_error_invalidfont);
	}
    }
}

/* Register the interpreter. */
void
gs_gstype2_init(gs_memory_t * mem)
{
    gs_charstring_interpreter[2] = gs_type2_charstring_interpret;
}
