/* Copyright (C) 1994, 1995, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: zfproc.c,v 1.2 2000/03/08 23:15:37 mike Exp $ */
/* Procedure-based filter stream support */
#include "memory_.h"
#include "ghost.h"
#include "oper.h"		/* for ifilter.h */
#include "estack.h"
#include "gsstruct.h"
#include "ialloc.h"
#include "istruct.h"		/* for RELOC_REF_VAR */
#include "stream.h"
#include "strimpl.h"
#include "ifilter.h"
#include "files.h"
#include "store.h"

/* ---------------- Generic ---------------- */

/* GC procedures */
#define pptr ((stream_proc_state *)vptr)
private 
CLEAR_MARKS_PROC(sproc_clear_marks)
{
    r_clear_attrs(&pptr->proc, l_mark);
    r_clear_attrs(&pptr->data, l_mark);
}
private 
ENUM_PTRS_BEGIN(sproc_enum_ptrs) return 0;

case 0:
ENUM_RETURN_REF(&pptr->proc);
case 1:
ENUM_RETURN_REF(&pptr->data);
ENUM_PTRS_END
private RELOC_PTRS_BEGIN(sproc_reloc_ptrs);
RELOC_REF_VAR(pptr->proc);
r_clear_attrs(&pptr->proc, l_mark);
RELOC_REF_VAR(pptr->data);
r_clear_attrs(&pptr->data, l_mark);
RELOC_PTRS_END
#undef pptr

/* Structure type for procedure-based streams. */
private_st_stream_proc_state();

/* Allocate and open a procedure-based filter. */
/* The caller must have checked that *sop is a procedure. */
private int
s_proc_init(ref * sop, stream ** psstrm, uint mode,
	    const stream_template * temp, const stream_procs * procs)
{
    stream *sstrm = file_alloc_stream(imemory, "s_proc_init(stream)");
    stream_proc_state *state =
    (stream_proc_state *) s_alloc_state(imemory, &st_sproc_state,
					"s_proc_init(state)");

    if (sstrm == 0 || state == 0) {
	ifree_object(state, "s_proc_init(state)");
	/*ifree_object(sstrm, "s_proc_init(stream)"); *//* just leave it on the file list */
	return_error(e_VMerror);
    }
    s_std_init(sstrm, NULL, 0, procs, mode);
    sstrm->procs.process = temp->process;
    state->template = temp;
    state->memory = imemory;
    state->eof = 0;
    state->proc = *sop;
    make_empty_string(&state->data, a_all);
    state->index = 0;
    sstrm->state = (stream_state *) state;
    *psstrm = sstrm;
    return 0;
}

/* Handle an interrupt during a stream operation. */
/* This is logically unrelated to procedure streams, */
/* but it is also associated with the interpreter stream machinery. */
private int
s_handle_intc(const ref * pstate, int nstate, int (*cont) (P1(os_ptr)))
{
    int npush = nstate + 2;

    check_estack(npush);
    if (nstate)
	memcpy(esp + 2, pstate, nstate * sizeof(ref));
#if 0				/* **************** */
    {
	int code = gs_interpret_error(e_interrupt, (ref *) (esp + npush));

	if (code < 0)
	    return code;
    }
#else /* **************** */
    npush--;
#endif /* **************** */
    make_op_estack(esp + 1, cont);
    esp += npush;
    return o_push_estack;
}


/* ---------------- Read streams ---------------- */

/* Forward references */
private stream_proc_process(s_proc_read_process);
private int s_proc_read_continue(P1(os_ptr));

/* Stream templates */
private const stream_template s_proc_read_template = {
    &st_sproc_state, NULL, s_proc_read_process, 1, 1, NULL
};
private const stream_procs s_proc_read_procs = {
    s_std_noavailable, s_std_noseek, s_std_read_reset,
    s_std_read_flush, s_std_null, NULL
};

/* Allocate and open a procedure-based read stream. */
/* The caller must have checked that *sop is a procedure. */
int
sread_proc(ref * sop, stream ** psstrm)
{
    int code =
	s_proc_init(sop, psstrm, s_mode_read, &s_proc_read_template,
		    &s_proc_read_procs);

    if (code < 0)
	return code;
    (*psstrm)->end_status = CALLC;
    return code;
}

/* Handle an input request. */
private int
s_proc_read_process(stream_state * st, stream_cursor_read * ignore_pr,
		    stream_cursor_write * pw, bool last)
{
    /* Move data from the string returned by the procedure */
    /* into the stream buffer, or ask for a callback. */
    stream_proc_state *const ss = (stream_proc_state *) st;
    uint count = r_size(&ss->data) - ss->index;

    if (count > 0) {
	uint wcount = pw->limit - pw->ptr;

	if (wcount < count)
	    count = wcount;
	memcpy(pw->ptr + 1, ss->data.value.bytes + ss->index, count);
	pw->ptr += count;
	ss->index += count;
	return 1;
    }
    return (ss->eof ? EOFC : CALLC);
}

/* Handle an exception (INTC or CALLC) from a read stream */
/* whose buffer is empty. */
int
s_handle_read_exception(int status, const ref * fop, const ref * pstate,
			int nstate, int (*cont) (P1(os_ptr)))
{
    int npush = nstate + 4;
    stream *ps;

    switch (status) {
	case INTC:
	    return s_handle_intc(pstate, nstate, cont);
	case CALLC:
	    break;
	default:
	    return_error(e_ioerror);
    }
    /* Find the stream whose buffer needs refilling. */
    for (ps = fptr(fop); ps->strm != 0;)
	ps = ps->strm;
    check_estack(npush);
    if (nstate)
	memcpy(esp + 2, pstate, nstate * sizeof(ref));
    make_op_estack(esp + 1, cont);
    esp += npush;
    make_op_estack(esp - 2, s_proc_read_continue);
    esp[-1] = *fop;
    r_clear_attrs(esp - 1, a_executable);
    *esp = ((stream_proc_state *) ps->state)->proc;
    return o_push_estack;
}
/* Continue a read operation after returning from a procedure callout. */
/* osp[0] contains the file (pushed on the e-stack by handle_read_status); */
/* osp[-1] contains the new data string (pushed by the procedure). */
/* The top of the e-stack contains the real continuation. */
private int
s_proc_read_continue(os_ptr op)
{
    os_ptr opbuf = op - 1;
    stream *ps;
    stream_proc_state *ss;

    check_file(ps, op);
    check_read_type(*opbuf, t_string);
    while ((ps->end_status = 0, ps->strm) != 0)
	ps = ps->strm;
    ss = (stream_proc_state *) ps->state;
    ss->data = *opbuf;
    ss->index = 0;
    if (r_size(opbuf) == 0)
	ss->eof = true;
    pop(2);
    return 0;
}

/* ---------------- Write streams ---------------- */

/* Forward references */
private stream_proc_process(s_proc_write_process);
private int s_proc_write_continue(P1(os_ptr));

/* Stream templates */
private const stream_template s_proc_write_template = {
    &st_sproc_state, NULL, s_proc_write_process, 1, 1, NULL
};
private const stream_procs s_proc_write_procs = {
    s_std_noavailable, s_std_noseek, s_std_write_reset,
    s_std_write_flush, s_std_null, NULL
};

/* Allocate and open a procedure-based write stream. */
/* The caller must have checked that *sop is a procedure. */
int
swrite_proc(ref * sop, stream ** psstrm)
{
    return s_proc_init(sop, psstrm, s_mode_write, &s_proc_write_template,
		       &s_proc_write_procs);
}

/* Handle an output request. */
private int
s_proc_write_process(stream_state * st, stream_cursor_read * pr,
		     stream_cursor_write * ignore_pw, bool last)
{
    /* Move data from the stream buffer to the string */
    /* returned by the procedure, or ask for a callback. */
    stream_proc_state *const ss = (stream_proc_state *) st;
    uint rcount = pr->limit - pr->ptr;

    if (rcount > 0) {
	uint wcount = r_size(&ss->data) - ss->index;
	uint count = min(rcount, wcount);

	memcpy(ss->data.value.bytes + ss->index, pr->ptr + 1, count);
	pr->ptr += count;
	ss->index += count;
	if (rcount > wcount)
	    return CALLC;
	else if (last) {
	    ss->eof = true;
	    return CALLC;
	} else
	    return 0;
    }
    return ((ss->eof = last) ? EOFC : 0);
}

/* Handle an exception (INTC or CALLC) from a write stream */
/* whose buffer is full. */
int
s_handle_write_exception(int status, const ref * fop, const ref * pstate,
			 int nstate, int (*cont) (P1(os_ptr)))
{
    stream *ps;
    stream_proc_state *psst;

    switch (status) {
	case INTC:
	    return s_handle_intc(pstate, nstate, cont);
	case CALLC:
	    break;
	default:
	    return_error(e_ioerror);
    }
    /* Find the stream whose buffer needs emptying. */
    for (ps = fptr(fop); ps->strm != 0;)
	ps = ps->strm;
    psst = (stream_proc_state *) ps->state;
    if (psst->eof) {
	/* This is the final call from closing the stream. */
	/* Don't run the continuation. */
	check_estack(5);
	esp += 5;
	make_op_estack(esp - 4, zpop);	/* pop the file */
	make_op_estack(esp - 3, zpop);	/* pop the string returned */
					/* by the procedure */
	make_false(esp - 1);
    } else {
	int npush = nstate + 6;

	check_estack(npush);
	if (nstate)
	    memcpy(esp + 2, pstate, nstate * sizeof(ref));
	make_op_estack(esp + 1, cont);
	esp += npush;
	make_op_estack(esp - 4, s_proc_write_continue);
	esp[-3] = *fop;
	r_clear_attrs(esp - 3, a_executable);
	make_true(esp - 1);
    }
    esp[-2] = psst->proc;
    *esp = psst->data;
    r_set_size(esp, psst->index);
    return o_push_estack;
}
/* Continue a write operation after returning from a procedure callout. */
/* osp[0] contains the file (pushed on the e-stack by handle_write_status); */
/* osp[-1] contains the new buffer string (pushed by the procedure). */
/* The top of the e-stack contains the real continuation. */
private int
s_proc_write_continue(os_ptr op)
{
    os_ptr opbuf = op - 1;
    stream *ps;
    stream_proc_state *ss;

    check_file(ps, op);
    check_write_type(*opbuf, t_string);
    while ((ps->end_status = 0, ps->strm) != 0)
	ps = ps->strm;
    ss = (stream_proc_state *) ps->state;
    ss->data = *opbuf;
    ss->index = 0;
    pop(2);
    return 0;
}

/* ------ Initialization procedure ------ */

const op_def zfproc_op_defs[] =
{
		/* Internal operators */
    {"2%s_proc_read_continue", s_proc_read_continue},
    {"2%s_proc_write_continue", s_proc_write_continue},
    op_def_end(0)
};
