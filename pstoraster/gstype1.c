/* Copyright (C) 1990, 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gstype1.c,v 1.3 2000/03/08 23:14:48 mike Exp $ */
/* Adobe Type 1 charstring interpreter */
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

/*
 * Define whether to always do Flex segments as curves.
 * This is only an issue because some old Adobe DPS fonts
 * seem to violate the Flex specification in a way that requires this.
 * We changed this from 1 to 0 in release 5.02: if it causes any
 * problems, we'll implement a more sophisticated test.
 */
#define ALWAYS_DO_FLEX_AS_CURVE 0

/* ------ Main interpreter ------ */

/* Define a pointer to the charstring interpreter stack. */
typedef fixed *cs_ptr;

/*
 * Continue interpreting a Type 1 charstring.  If str != 0, it is taken as
 * the byte string to interpret.  Return 0 on successful completion, <0 on
 * error, or >0 when client intervention is required (or allowed).  The int*
 * argument is where the othersubr # is stored for callothersubr.
 */
private int
gs_type1_charstring_interpret(gs_type1_state * pcis,
			      const gs_const_string * str, int *pindex)
{
    gs_font_type1 *pfont = pcis->pfont;
    gs_type1_data *pdata = &pfont->data;
    bool encrypted = pdata->lenIV >= 0;
    gs_op1_state s;
    fixed cstack[ostack_size];

#define cs0 cstack[0]
#define ics0 fixed2int_var(cs0)
#define cs1 cstack[1]
#define ics1 fixed2int_var(cs1)
#define cs2 cstack[2]
#define ics2 fixed2int_var(cs2)
#define cs3 cstack[3]
#define ics3 fixed2int_var(cs3)
#define cs4 cstack[4]
#define ics4 fixed2int_var(cs4)
#define cs5 cstack[5]
#define ics5 fixed2int_var(cs5)
    cs_ptr csp;

#define clear csp = cstack - 1
    ip_state *ipsp = &pcis->ipstack[pcis->ips_count - 1];
    register const byte *cip;
    register crypt_state state;
    register int c;
    int code = 0;
    fixed ftx = pcis->origin.x, fty = pcis->origin.y;

    switch (pcis->init_done) {
	case -1:
	    break;
	case 0:
	    gs_type1_finish_init(pcis, &s);	/* sets sfc, ptx, pty, origin */
	    ftx = pcis->origin.x, fty = pcis->origin.y;
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
		*++csp = int2fixed(lw);
		if (lw != fixed2long(*csp))
		    return_error(gs_error_rangecheck);
	    } else		/* not possible */
		return_error(gs_error_invalidfont);
	  pushed:if_debug3('1', "[1]%d: (%d) %f\n",
		      (int)(csp - cstack), c, fixed2float(*csp));
	    continue;
	}
#ifdef DEBUG
	if (gs_debug['1']) {
	    static const char *const c1names[] =
	    {char1_command_names};

	    if (c1names[c] == 0)
		dlprintf2("[1]0x%lx: %02x??\n", (ulong) (cip - 1), c);
	    else
		dlprintf3("[1]0x%lx: %02x %s\n", (ulong) (cip - 1), c,
			  c1names[c]);
	}
#endif
	switch ((char_command) c) {
#define cnext clear; goto top
#define inext goto top

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
		if (code < 0)
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
		apply_path_hints(pcis, false);
		type1_hstem(pcis, cs0, cs1);
		cnext;
	    case cx_vstem:
		apply_path_hints(pcis, false);
		type1_vstem(pcis, cs0, cs1);
		cnext;
	    case cx_vmoveto:
		cs1 = cs0;
		cs0 = 0;
		accum_y(cs1);
	      move:		/* cs0 = dx, cs1 = dy for hint checking. */
		if ((pcis->hint_next != 0 || path_is_drawing(sppath)) &&
		    pcis->flex_count == flex_max
		    )
		    apply_path_hints(pcis, true);
		code = gx_path_add_point(sppath, ptx, pty);
		goto cc;
	    case cx_rlineto:
		accum_xy(cs0, cs1);
	      line:		/* cs0 = dx, cs1 = dy for hint checking. */
		code = gx_path_add_line(sppath, ptx, pty);
	      cc:if (code < 0)
		    return code;
	      pp:if_debug2('1', "[1]pt=(%g,%g)\n",
			  fixed2float(ptx), fixed2float(pty));
		cnext;
	    case cx_hlineto:
		accum_x(cs0);
		cs1 = 0;
		goto line;
	    case cx_vlineto:
		cs1 = cs0;
		cs0 = 0;
		accum_y(cs1);
		goto line;
	    case cx_rrcurveto:
		code = gs_op1_rrcurveto(&s, cs0, cs1, cs2, cs3, cs4, cs5);
		goto cc;
	    case cx_endchar:
		code = gs_type1_endchar(pcis);
		if (code == 1) {
		    /* do accent of seac */
		    spt = pcis->position;
		    ipsp = &pcis->ipstack[pcis->ips_count - 1];
		    cip = ipsp->char_string.data;
		    goto call;
		}
		return code;
	    case cx_rmoveto:
		accum_xy(cs0, cs1);
		goto move;
	    case cx_hmoveto:
		accum_x(cs0);
		cs1 = 0;
		goto move;
	    case cx_vhcurveto:
		{
		    gs_fixed_point pt1, pt2;
		    fixed ax0 = sppath->position.x - ptx;
		    fixed ay0 = sppath->position.y - pty;

		    accum_y(cs0);
		    pt1.x = ptx + ax0, pt1.y = pty + ay0;
		    accum_xy(cs1, cs2);
		    pt2.x = ptx, pt2.y = pty;
		    accum_x(cs3);
		    code = gx_path_add_curve(sppath, pt1.x, pt1.y, pt2.x, pt2.y, ptx, pty);
		}
		goto cc;
	    case cx_hvcurveto:
		{
		    gs_fixed_point pt1, pt2;
		    fixed ax0 = sppath->position.x - ptx;
		    fixed ay0 = sppath->position.y - pty;

		    accum_x(cs0);
		    pt1.x = ptx + ax0, pt1.y = pty + ay0;
		    accum_xy(cs1, cs2);
		    pt2.x = ptx, pt2.y = pty;
		    accum_y(cs3);
		    code = gx_path_add_curve(sppath, pt1.x, pt1.y, pt2.x, pt2.y, ptx, pty);
		}
		goto cc;

		/* Commands only recognized in Type 1 charstrings, */
		/* plus 'escape'. */

	    case c1_closepath:
		code = gs_op1_closepath(&s);
		apply_path_hints(pcis, true);
		goto cc;
	    case c1_hsbw:
		gs_type1_sbw(pcis, cs0, fixed_0, cs1, fixed_0);
rsbw:		/* Give the caller the opportunity to intervene. */
		pcis->os_count = 0;	/* clear */
		ipsp->ip = cip, ipsp->dstate = state;
		pcis->ips_count = ipsp - &pcis->ipstack[0] + 1;
		/* If we aren't in a seac, do nothing else now; */
		/* finish_init will take care of the rest. */
		if (pcis->init_done < 0) {
		    /* Finish init when we return. */
		    pcis->init_done = 0;
		} else {
		    /* Accumulate the side bearing now, but don't do it */
		    /* a second time for the base character of a seac. */
		    if (pcis->seac_accent < 0)
			accum_xy(pcis->lsb.x, pcis->lsb.y);
		    pcis->position.x = ptx;
		    pcis->position.y = pty;
		}
		return type1_result_sbw;
	    case cx_escape:
		charstring_next(*cip, state, c, encrypted);
		++cip;
#ifdef DEBUG
		if (gs_debug['1'] && c < char1_extended_command_count) {
		    static const char *const ce1names[] =
		    {char1_extended_command_names};

		    if (ce1names[c] == 0)
			dlprintf2("[1]0x%lx: %02x??\n", (ulong) (cip - 1), c);
		    else
			dlprintf3("[1]0x%lx: %02x %s\n", (ulong) (cip - 1), c,
				  ce1names[c]);
		}
#endif
		switch ((char1_extended_command) c) {
		    case ce1_dotsection:
			pcis->dotsection_flag ^=
			    (dotsection_in ^ dotsection_out);
			cnext;
		    case ce1_vstem3:
			apply_path_hints(pcis, false);
			if (!pcis->vstem3_set && pcis->fh.use_x_hints) {
			    center_vstem(pcis, pcis->lsb.x + cs2, cs3);
			    /* Adjust the current point */
			    /* (center_vstem handles everything else). */
			    ptx += pcis->vs_offset.x;
			    pty += pcis->vs_offset.y;
			    pcis->vstem3_set = true;
			}
			type1_vstem(pcis, cs0, cs1);
			type1_vstem(pcis, cs2, cs3);
			type1_vstem(pcis, cs4, cs5);
			cnext;
		    case ce1_hstem3:
			apply_path_hints(pcis, false);
			type1_hstem(pcis, cs0, cs1);
			type1_hstem(pcis, cs2, cs3);
			type1_hstem(pcis, cs4, cs5);
			cnext;
		    case ce1_seac:
			code = gs_type1_seac(pcis, cstack + 1, cstack[0],
					     ipsp);
			if (code != 0) {
			    *pindex = ics3;
			    return code;
			}
			clear;
			cip = ipsp->char_string.data;
			goto call;
		    case ce1_sbw:
			gs_type1_sbw(pcis, cs0, cs1, cs2, cs3);
			goto rsbw;
		    case ce1_div:
			csp[-1] = float2fixed((float)csp[-1] / (float)*csp);
			--csp;
			goto pushed;
		    case ce1_undoc15:
			/* See gstype1.h for information on this opcode. */
			cnext;
		    case ce1_callothersubr:
			{
			    int num_results;

#define fpts pcis->flex_points
			    /* We must remember to pop both the othersubr # */
			    /* and the argument count off the stack. */
			    switch (*pindex = fixed2int_var(*csp)) {
				case 0:
				    {	/* We have to do something really sleazy */
					/* here, namely, make it look as though */
					/* the rmovetos never really happened, */
					/* because we don't want to interrupt */
					/* the current subpath. */
					gs_fixed_point ept;

#if defined(DEBUG) || !ALWAYS_DO_FLEX_AS_CURVE
					fixed fheight = csp[-4];
					gs_fixed_point hpt;

#endif

					if (pcis->flex_count != 8)
					    return_error(gs_error_invalidfont);
					/* Assume the next two opcodes */
					/* are `pop' `pop'.  Unfortunately, some */
					/* Monotype fonts put these in a Subr, */
					/* so we can't just look ahead in the */
					/* opcode stream. */
					pcis->ignore_pops = 2;
					csp[-4] = csp[-3] - pcis->asb_diff;
					csp[-3] = csp[-2];
					csp -= 3;
					gx_path_current_point(sppath, &ept);
					gx_path_add_point(sppath, fpts[0].x, fpts[0].y);
					sppath->state_flags =	/* <--- sleaze */
					    pcis->flex_path_state_flags;
#if defined(DEBUG) || !ALWAYS_DO_FLEX_AS_CURVE
					/* Decide whether to do the flex as a curve. */
					hpt.x = fpts[1].x - fpts[4].x;
					hpt.y = fpts[1].y - fpts[4].y;
					if_debug3('1',
					  "[1]flex: d=(%g,%g), height=%g\n",
						  fixed2float(hpt.x), fixed2float(hpt.y),
						fixed2float(fheight) / 100);
#endif
#if !ALWAYS_DO_FLEX_AS_CURVE	/* See beginning of file. */
					if (any_abs(hpt.x) + any_abs(hpt.y) <
					    fheight / 100
					    ) {		/* Do the flex as a line. */
					    code = gx_path_add_line(sppath,
							      ept.x, ept.y);
					} else
#endif
					{	/* Do the flex as a curve. */
					    code = gx_path_add_curve(sppath,
						       fpts[2].x, fpts[2].y,
						       fpts[3].x, fpts[3].y,
						      fpts[4].x, fpts[4].y);
					    if (code < 0)
						return code;
					    code = gx_path_add_curve(sppath,
						       fpts[5].x, fpts[5].y,
						       fpts[6].x, fpts[6].y,
						      fpts[7].x, fpts[7].y);
					}
				    }
				    if (code < 0)
					return code;
				    pcis->flex_count = flex_max;	/* not inside flex */
				    inext;
				case 1:
				    gx_path_current_point(sppath, &fpts[0]);
				    pcis->flex_path_state_flags =	/* <--- more sleaze */
					sppath->state_flags;
				    pcis->flex_count = 1;
				    csp -= 2;
				    inext;
				case 2:
				    if (pcis->flex_count >= flex_max)
					return_error(gs_error_invalidfont);
				    gx_path_current_point(sppath,
						 &fpts[pcis->flex_count++]);
				    csp -= 2;
				    inext;
				case 3:
				    /* Assume the next opcode is a `pop'. */
				    /* See above as to why we don't just */
				    /* look ahead in the opcode stream. */
				    pcis->ignore_pops = 1;
				    replace_stem_hints(pcis);
				    csp -= 2;
				    inext;
				case 14:
				    num_results = 1;
				  blend:{
					int num_values = fixed2int_var(csp[-1]);
					int k1 = num_values / num_results - 1;
					int i, j;
					cs_ptr base, deltas;

					if (num_values < num_results ||
					    num_values % num_results != 0
					    )
					    return_error(gs_error_invalidfont);
					base = csp - 1 - num_values;
					deltas = base + num_results - 1;
					for (j = 0; j < num_results;
					     j++, base++, deltas += k1
					    )
					    for (i = 1; i <= k1; i++)
						*base += deltas[i] *
						    pdata->WeightVector.values[i];
					csp = base - 1;
				    }
				    pcis->ignore_pops = num_results;
				    inext;
				case 15:
				    num_results = 2;
				    goto blend;
				case 16:
				    num_results = 3;
				    goto blend;
				case 17:
				    num_results = 4;
				    goto blend;
				case 18:
				    num_results = 6;
				    goto blend;
			    }
			}
#undef fpts
			/* Not a recognized othersubr, */
			/* let the client handle it. */
			{
			    int scount = csp - cstack;
			    int n;

			    /* Copy the arguments to the caller's stack. */
			    if (scount < 1 || csp[-1] < 0 ||
				csp[-1] > int2fixed(scount - 1)
				)
				return_error(gs_error_invalidfont);
			    n = fixed2int_var(csp[-1]);
			    code = (*pdata->procs->push) (pfont, csp - (n + 1), n);
			    if (code < 0)
				return_error(code);
			    scount -= n + 1;
			    pcis->position.x = ptx;
			    pcis->position.y = pty;
			    apply_path_hints(pcis, false);
			    /* Exit to caller */
			    ipsp->ip = cip, ipsp->dstate = state;
			    pcis->os_count = scount;
			    pcis->ips_count = ipsp - &pcis->ipstack[0] + 1;
			    if (scount)
				memcpy(pcis->ostack, cstack, scount * sizeof(fixed));
			    return type1_result_callothersubr;
			}
		    case ce1_pop:
			/* Check whether we're ignoring the pops after */
			/* a known othersubr. */
			if (pcis->ignore_pops != 0) {
			    pcis->ignore_pops--;
			    inext;
			}
			++csp;
			code = (*pdata->procs->pop) (pfont, csp);
			if (code < 0)
			    return_error(code);
			goto pushed;
		    case ce1_setcurrentpoint:
			ptx = ftx, pty = fty;
			cs0 += pcis->adxy.x;
			cs1 += pcis->adxy.y;
			accum_xy(cs0, cs1);
			goto pp;
		    default:
			return_error(gs_error_invalidfont);
		}
		/*NOTREACHED */

		/* Fill up the dispatch up to 32. */

	      case_c1_undefs:
	    default:		/* pacify compiler */
		return_error(gs_error_invalidfont);
	}
    }
}

/* Register the interpreter. */
void
gs_gstype1_init(gs_memory_t * mem)
{
    gs_charstring_interpreter[1] = gs_type1_charstring_interpret;
}
