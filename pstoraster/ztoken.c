/* Copyright (C) 1994 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: ztoken.c,v 1.2 2000/03/08 23:15:42 mike Exp $ */
/* Token reading operators */
#include "ghost.h"
#include "oper.h"
#include "estack.h"
#include "gsstruct.h"		/* for iscan.h */
#include "stream.h"
#include "files.h"
#include "store.h"
#include "strimpl.h"		/* for sfilter.h */
#include "sfilter.h"		/* for iscan.h */
#include "iscan.h"

/* <file> token <obj> -true- */
/* <string> token <post> <obj> -true- */
/* <string|file> token -false- */
private int ztoken_continue(P1(os_ptr));
private int token_continue(P4(os_ptr, stream *, scanner_state *, bool));
int
ztoken(register os_ptr op)
{
    switch (r_type(op)) {
	default:
	    return_op_typecheck(op);
	case t_file: {
	    stream *s;
	    scanner_state state;

	    check_read_file(s, op);
	    check_ostack(1);
	    scanner_state_init(&state, false);
	    return token_continue(op, s, &state, true);
	}
	case t_string: {
	    ref token;
	    int code = scan_string_token(op, &token);

	    switch (code) {
	    case scan_EOF:	/* no tokens */
		make_false(op);
		return 0;
	    default:
		if (code < 0)
		    return code;
	    }
	    push(2);
	    op[-1] = token;
	    make_true(op);
	    return 0;
	}
    }
}
/* Continue reading a token after an interrupt or callout. */
/* *op is the scanner state; op[-1] is the file. */
private int
ztoken_continue(os_ptr op)
{
    stream *s;
    scanner_state *pstate;

    check_read_file(s, op - 1);
    check_stype(*op, st_scanner_state);
    pstate = r_ptr(op, scanner_state);
    pop(1);
    return token_continue(osp, s, pstate, false);
}
/* Common code for token reading. */
private int
token_continue(os_ptr op, stream * s, scanner_state * pstate, bool save)
{
    int code;
    ref token;

    /* Note that scan_token may change osp! */
    /* Also, we must temporarily remove the file from the o-stack */
    /* when calling scan_token, in case we are scanning a procedure. */
    ref fref;

    ref_assign(&fref, op);
again:
    pop(1);
    code = scan_token(s, &token, pstate);
    op = osp;
    switch (code) {
	default:		/* error */
	    push(1);
	    ref_assign(op, &fref);
	    break;
	case scan_BOS:
	    code = 0;
	case 0:		/* read a token */
	    push(2);
	    ref_assign(op - 1, &token);
	    make_true(op);
	    break;
	case scan_EOF:		/* no tokens */
	    push(1);
	    make_false(op);
	    code = 0;
	    break;
	case scan_Refill:	/* need more data */
	    push(1);
	    ref_assign(op, &fref);
	    code = scan_handle_refill(op, pstate, save, false,
				      ztoken_continue);
	    switch (code) {
		case 0:	/* state is not copied to the heap */
		    goto again;
		case o_push_estack:
		    return code;
	    }
	    break;		/* error */
    }
    if (code <= 0 && !save) {	/* Deallocate the scanner state record. */
	ifree_object(pstate, "token_continue");
    }
    return code;
}

/* <file> .tokenexec - */
/* Read a token and do what the interpreter would do with it. */
/* This is different from token + exec because literal procedures */
/* are not executed (although binary object sequences ARE executed). */
int ztokenexec_continue(P1(os_ptr));	/* export for interpreter */
private int tokenexec_continue(P4(os_ptr, stream *, scanner_state *, bool));
int
ztokenexec(register os_ptr op)
{
    stream *s;
    scanner_state state;

    check_read_file(s, op);
    check_estack(1);
    scanner_state_init(&state, false);
    return tokenexec_continue(op, s, &state, true);
}
/* Continue reading a token for execution after an interrupt or callout. */
/* *op is the scanner state; op[-1] is the file. */
/* We export this because this is how the interpreter handles a */
/* scan_Refill for an executable file. */
int
ztokenexec_continue(os_ptr op)
{
    stream *s;
    scanner_state *pstate;

    check_read_file(s, op - 1);
    check_stype(*op, st_scanner_state);
    pstate = r_ptr(op, scanner_state);
    pop(1);
    return tokenexec_continue(osp, s, pstate, false);
}
/* Common code for token reading + execution. */
private int
tokenexec_continue(os_ptr op, stream * s, scanner_state * pstate, bool save)
{
    int code;

    /* Note that scan_token may change osp! */
    /* Also, we must temporarily remove the file from the o-stack */
    /* when calling scan_token, in case we are scanning a procedure. */
    ref fref;

    ref_assign(&fref, op);
    pop(1);
again:
    code = scan_token(s, (ref *) (esp + 1), pstate);
    op = osp;
    switch (code) {
	case 0:
	    if (r_is_proc(esp + 1)) {	/* Treat procedure as a literal. */
		push(1);
		ref_assign(op, esp + 1);
		code = 0;
		break;
	    }
	    /* falls through */
	case scan_BOS:
	    ++esp;
	    code = o_push_estack;
	    break;
	case scan_EOF:		/* no tokens */
	    code = 0;
	    break;
	case scan_Refill:	/* need more data */
	    code = scan_handle_refill(&fref, pstate, save, true,
				      ztokenexec_continue);
	    switch (code) {
		case 0:	/* state is not copied to the heap */
		    goto again;
		case o_push_estack:
		    return code;
	    }
	    /* falls through */
	default:		/* error */
	    break;
    }
    if (code < 0) {		/* Push the operand back on the stack. */
	push(1);
	ref_assign(op, &fref);
    }
    if (!save) {		/* Deallocate the scanner state record. */
	ifree_object(pstate, "token_continue");
    }
    return code;
}

/* ------ Initialization procedure ------ */

const op_def ztoken_op_defs[] =
{
    {"1token", ztoken},
    {"1.tokenexec", ztokenexec},
		/* Internal operators */
    {"2%ztokenexec_continue", ztokenexec_continue},
    op_def_end(0)
};
