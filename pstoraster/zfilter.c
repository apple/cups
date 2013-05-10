/* Copyright (C) 1993, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* zfilter.c */
/* Filter creation */
#include "memory_.h"
#include "ghost.h"
#include "errors.h"
#include "oper.h"
#include "gsstruct.h"
#include "ialloc.h"
#include "idict.h"
#include "idparam.h"
#include "stream.h"
#include "strimpl.h"
#include "sfilter.h"
#include "srlx.h"
#include "sstring.h"
#include "ifilter.h"
#include "files.h"		/* for filter_open, file_d'_buffer_size */

/* Define whether we are including some non-standard filters for testing. */
#define TEST

/* <source> ASCIIHexEncode/filter <file> */
/* <source> <dict_ignored> ASCIIHexEncode/filter <file> */
private int
zAXE(os_ptr op)
{	return filter_write_simple(op, &s_AXE_template);
}

/* <target> ASCIIHexDecode/filter <file> */
/* <target> <dict_ignored> ASCIIHexDecode/filter <file> */
private int
zAXD(os_ptr op)
{	return filter_read_simple(op, &s_AXD_template);
}

/* <target> NullEncode/filter <file> */
/* <target> <dict_ignored> NullEncode/filter <file> */
private int
zNullE(os_ptr op)
{	return filter_write_simple(op, &s_NullE_template);
}

/* <source> <bool> PFBDecode/filter <file> */
/* <source> <bool> <dict_ignored> PFBDecode/filter <file> */
private int
zPFBD(os_ptr op)
{	stream_PFBD_state state;
	os_ptr sop = op;
	int npop = 1;

	if ( r_has_type(op, t_dictionary) )
	  ++npop, --sop;
	check_type(*sop, t_boolean);
	state.binary_to_hex = sop->value.boolval;
	return filter_read(op, npop, &s_PFBD_template, (stream_state *)&state,
			   0);
}

/* <target> PSStringEncode/filter <file> */
/* <target> <dict_ignored> PSStringEncode/filter <file> */
private int
zPSSE(os_ptr op)
{	return filter_write_simple(op, &s_PSSE_template);
}

/* ------ RunLength filters ------ */

/* Common setup for RLE and RLD filters. */
private int
rl_setup(os_ptr op, bool *eod)
{	if ( r_has_type(op, t_dictionary) )
	  {	int code;
		check_dict_read(*op);
		if ( (code = dict_bool_param(op, "EndOfData", true, eod)) < 0 )
		  return code;
		return 1;
	  }
	else
	  {	*eod = true;
		return 0;
	  }
}

/* <target> <record_size> RunLengthEncode/filter <file> */
/* <target> <record_size> <dict> RunLengthEncode/filter <file> */
private int
zRLE(register os_ptr op)
{	stream_RLE_state state;
	int code = rl_setup(op, &state.EndOfData);
	if ( code < 0 )
	  return code;
	check_int_leu(op[-code], max_uint);
	state.record_size = op->value.intval;
	return filter_write(op, 1 + code, &s_RLE_template, (stream_state *)&state, 0);
}

/* <source> RunLengthDecode/filter <file> */
/* <source> <dict> RunLengthDecode/filter <file> */
private int
zRLD(os_ptr op)
{	stream_RLD_state state;
	int code = rl_setup(op, &state.EndOfData);
	if ( code < 0 )
	  return code;
	return filter_read(op, code, &s_RLD_template, (stream_state *)&state, 0);
}

/* <source> <EODcount> <EODstring> SubFileDecode/filter <file> */
/* <source> <EODcount> <EODstring> <dict_ignored> SubFileDecode/filter <file> */
private int
zSFD(os_ptr op)
{	stream_SFD_state state;
	os_ptr sop = op;
	int npop = 2;

	if ( r_has_type(op, t_dictionary) )
	  ++npop, --sop;
	check_type(sop[-1], t_integer);
	check_read_type(*sop, t_string);
	if ( sop[-1].value.intval < 0 )
	  return_error(e_rangecheck);
	state.count = sop[-1].value.intval;
	state.eod.data = sop->value.const_bytes;
	state.eod.size = r_size(sop);
	return filter_read(op, npop, &s_SFD_template, (stream_state *)&state,
			   r_space(sop));
}

#ifdef TEST

#include "store.h"

/* <size> BigStringEncode/filter <file> */
private int BSE_close(P1(stream *));
private int
zBSE(os_ptr op)
{	stream *s;
	byte *data;
	long len;

	check_type(op[-0], t_integer);
	len = op[-0].value.intval;
	if ( len < 0 )
	  return_error(e_rangecheck);

	data = ialloc_string(len, "BigStringEncode(string)");
	if ( !data )
	  return_error(e_VMerror);
	s = file_alloc_stream(imemory, "BigStringEncode(stream)");
	if ( !s )
	  {	ifree_string(data, len, "BigStringEncode(string)");
		return_error(e_VMerror);
	  }
	swrite_string(s, data, len);
	s->is_temp = 0;
	s->read_id = 0;
	s->procs.close = BSE_close;
	s->save_close = BSE_close;
	make_file(op,
		  ((a_write | a_execute) | icurrent_space),
		  s->write_id,
		  s);
	return 0;
}
private int
BSE_close(stream *s)
{	return 0;
}

#endif				/* TEST */

/* ------ Utilities ------ */

/* Forward references */
private int filter_ensure_buf(P3(stream **, uint, bool));

/* Set up an input filter. */
const stream_procs s_new_read_procs =
{	s_std_noavailable, s_std_noseek, s_std_read_reset,
	s_std_read_flush, s_filter_close
};
int
filter_read(os_ptr op, int npop, const stream_template *template,
  stream_state *st, uint space)
{	uint min_size = template->min_out_size + max_min_left;
	uint save_space = ialloc_space(idmemory);
	register os_ptr sop = op - npop;
	stream *s;
	stream *sstrm;
	int code;

	/* Check to make sure that the underlying data */
	/* can function as a source for reading. */
	switch ( r_type(sop) )
	{
	case t_string:
		check_read(*sop);
		ialloc_set_space(idmemory, max(space, r_space(sop)));
		sstrm = file_alloc_stream(imemory,
					  "filter_read(string stream)");
		if ( sstrm == 0 )
		{	code = gs_note_error(e_VMerror);
			goto out;
		}
		sread_string(sstrm, sop->value.bytes, r_size(sop));
		sstrm->is_temp = 1;
		break;
	case t_file:
		check_read_known_file(sstrm, sop, return);
		ialloc_set_space(idmemory, max(space, r_space(sop)));
		goto ens;
	default:
		check_proc(*sop);
		ialloc_set_space(idmemory, max(space, r_space(sop)));
		code = sread_proc(sop, &sstrm);
		if ( code < 0 )
		  goto out;
		sstrm->is_temp = 2;
ens:		code = filter_ensure_buf(&sstrm,
					 template->min_in_size +
				          sstrm->state->template->min_out_size,
					 false);
		if ( code < 0 )
		  goto out;
		break;
	   }
	if ( min_size < 128 )
	  min_size = file_default_buffer_size;
	code = filter_open("r", min_size, (ref *)sop,
			   &s_new_read_procs, template, st);
	if ( code < 0 )
	  goto out;
	s = fptr(sop);
	s->strm = sstrm;
	pop(npop);
out:	ialloc_set_space(idmemory, save_space);
	return code;
}
int
filter_read_simple(os_ptr op, const stream_template *template)
{	return filter_read(op, (r_has_type(op, t_dictionary) ? 1 : 0),
			   template, NULL, 0);
}

/* Set up an output filter. */
const stream_procs s_new_write_procs =
{	s_std_noavailable, s_std_noseek, s_std_write_reset,
	s_std_write_flush, s_filter_close
};
int
filter_write(os_ptr op, int npop, const stream_template *template,
  stream_state *st, uint space)
{	uint min_size = template->min_in_size + max_min_left;
	uint save_space = ialloc_space(idmemory);
	register os_ptr sop = op - npop;
	stream *s;
	stream *sstrm;
	int code;

	/* Check to make sure that the underlying data */
	/* can function as a sink for writing. */
	switch ( r_type(sop) )
	{
	case t_string:
		check_write(*sop);
		ialloc_set_space(idmemory, max(space, r_space(sop)));
		sstrm = file_alloc_stream(imemory,
					  "filter_write(string)");
		if ( sstrm == 0 )
		{	code = gs_note_error(e_VMerror);
			goto out;
		}
		swrite_string(sstrm, sop->value.bytes, r_size(sop));
		sstrm->is_temp = 1;
		break;
	case t_file:
		check_write_known_file(sstrm, sop, return);
		ialloc_set_space(idmemory, max(space, r_space(sop)));
		goto ens;
	default:
		check_proc(*sop);
		ialloc_set_space(idmemory, max(space, r_space(sop)));
		code = swrite_proc(sop, &sstrm);
		if ( code < 0 )
		  goto out;
		sstrm->is_temp = 2;
ens:		code = filter_ensure_buf(&sstrm,
					 template->min_out_size +
				          sstrm->state->template->min_in_size,
					 true);
		if ( code < 0 )
		  goto out;
		break;
	}
	if ( min_size < 128 )
	  min_size = file_default_buffer_size;
	code = filter_open("w", min_size, (ref *)sop,
			   &s_new_write_procs, template, st);
	if ( code < 0 )
	  goto out;
	s = fptr(sop);
	s->strm = sstrm;
	pop(npop);
out:	ialloc_set_space(idmemory, save_space);
	return code;
}
int
filter_write_simple(os_ptr op, const stream_template *template)
{	return filter_write(op, (r_has_type(op, t_dictionary) ? 1 : 0),
			    template, NULL, 0);
}

/* Define a byte-at-a-time NullDecode filter for intermediate buffers. */
/* (The standard NullDecode filter can read ahead too far.) */
private int
s_Null1D_process(stream_state *st, stream_cursor_read *pr,
  stream_cursor_write *pw, bool last)
{	if ( pr->ptr >= pr->limit )
	  return 0;
	if ( pw->ptr >= pw->limit )
	  return 1;
	*++(pw->ptr) = *++(pr->ptr);
	return 1;
}
private const stream_template s_Null1D_template =
{	&st_stream_state, NULL, s_Null1D_process, 1, 1
};

/* Ensure a minimum buffer size for a filter. */
/* This may require creating an intermediate stream. */
private int
filter_ensure_buf(stream **ps, uint min_buf_size, bool writing)
{	stream *s = *ps;
	uint min_size = min_buf_size + max_min_left;
	stream *bs;
	ref bsop;
	int code;

	if ( s->modes == 0 /* stream is closed */ || s->bsize >= min_size )
	  return 0;
	/* Otherwise, allocate an intermediate stream. */
	if ( s->cbuf == 0 )
	  {	/* This is a newly created procedure stream. */
		/* Just allocate a buffer for it. */
		uint len = max(min_size, 128);
		byte *buf = ialloc_bytes(len, "filter_ensure_buf");
		if ( buf == 0 )
		  return_error(e_VMerror);
		s->cbuf = buf;
		s->srptr = s->srlimit = s->swptr = buf - 1;
		s->swlimit = buf - 1 + len;
		s->bsize = s->cbsize = len;
		return 0;
	  }
	else
	  {	/* Allocate an intermediate stream. */
		if ( writing )
		  code = filter_open("w", min_size, &bsop, &s_new_write_procs,
				     &s_NullE_template, NULL);
		else
		  code = filter_open("r", min_size, &bsop, &s_new_read_procs,
				     &s_Null1D_template, NULL);
		if ( code < 0 )
		  return code;
		bs = fptr(&bsop);
		bs->strm = s;
		bs->is_temp = 2;
		*ps = bs;
		return code;
	  }
}

/* Mark a (filter) stream as temporary. */
/* We define this here to avoid importing stream.h into zf*.c. */
void
filter_mark_temp(const ref *fop, int is_temp)
{	fptr(fop)->is_temp = is_temp;
}

/* ------ Initialization procedure ------ */

BEGIN_OP_DEFS(zfilter_op_defs) {
		/* We enter PSStringEncode and SubFileDecode (only) */
		/* as separate operators. */
	{"1.psstringencode", zPSSE},
	{"3.subfiledecode", zSFD},
		op_def_begin_filter(),
	{"1ASCIIHexEncode", zAXE},
	{"1ASCIIHexDecode", zAXD},
	{"1NullEncode", zNullE},
	{"2PFBDecode", zPFBD},
	{"1PSStringEncode", zPSSE},
	{"2RunLengthEncode", zRLE},
	{"1RunLengthDecode", zRLD},
	{"3SubFileDecode", zSFD},
#ifdef TEST
	{"1BigStringEncode", zBSE},
#endif
END_OP_DEFS(0) }
