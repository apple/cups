/* Copyright (C) 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gdevpsdf.c,v 1.1 2000/03/08 23:14:25 mike Exp $ */
/* Common utilities for PostScript and PDF writers */
#include "string_.h"
#include "gx.h"
#include "gserrors.h"
#include "gdevpsdf.h"
#include "gdevpstr.h"
#include "scanchar.h"
#include "strimpl.h"
#include "sa85x.h"
#include "scfx.h"
#include "sstring.h"

/* Structure descriptor */
public_st_device_psdf();

/* ---------------- Vector implementation procedures ---------------- */

int
psdf_setlinewidth(gx_device_vector * vdev, floatp width)
{
    pprintg1(gdev_vector_stream(vdev), "%g w\n", width);
    return 0;
}

int
psdf_setlinecap(gx_device_vector * vdev, gs_line_cap cap)
{
    pprintd1(gdev_vector_stream(vdev), "%d J\n", cap);
    return 0;
}

int
psdf_setlinejoin(gx_device_vector * vdev, gs_line_join join)
{
    pprintd1(gdev_vector_stream(vdev), "%d j\n", join);
    return 0;
}

int
psdf_setmiterlimit(gx_device_vector * vdev, floatp limit)
{
    pprintg1(gdev_vector_stream(vdev), "%g M\n", limit);
    return 0;
}

int
psdf_setdash(gx_device_vector * vdev, const float *pattern, uint count,
	     floatp offset)
{
    stream *s = gdev_vector_stream(vdev);
    int i;

    pputs(s, "[ ");
    for (i = 0; i < count; ++i)
	pprintg1(s, "%g ", pattern[i]);
    pprintg1(s, "] %g d\n", offset);
    return 0;
}

int
psdf_setflat(gx_device_vector * vdev, floatp flatness)
{
    pprintg1(gdev_vector_stream(vdev), "%g i\n", flatness);
    return 0;
}

int
psdf_setlogop(gx_device_vector * vdev, gs_logical_operation_t lop,
	      gs_logical_operation_t diff)
{
/****** SHOULD AT LEAST DETECT SET-0 & SET-1 ******/
    return 0;
}

int
psdf_setfillcolor(gx_device_vector * vdev, const gx_drawing_color * pdc)
{
    return psdf_set_color(vdev, pdc, "rg");
}

int
psdf_setstrokecolor(gx_device_vector * vdev, const gx_drawing_color * pdc)
{
    return psdf_set_color(vdev, pdc, "RG");
}

int
psdf_dorect(gx_device_vector * vdev, fixed x0, fixed y0, fixed x1, fixed y1,
	    gx_path_type_t type)
{
    int code = (*vdev_proc(vdev, beginpath)) (vdev, type);

    if (code < 0)
	return code;
    pprintg4(gdev_vector_stream(vdev), "%g %g %g %g re\n",
	     fixed2float(x0), fixed2float(y0),
	     fixed2float(x1 - x0), fixed2float(y1 - y0));
    return (*vdev_proc(vdev, endpath)) (vdev, type);
}

int
psdf_beginpath(gx_device_vector * vdev, gx_path_type_t type)
{
    return 0;
}

int
psdf_moveto(gx_device_vector * vdev, floatp x0, floatp y0, floatp x, floatp y,
	    bool first, gx_path_type_t type)
{
    pprintg2(gdev_vector_stream(vdev), "%g %g m\n", x, y);
    return 0;
}

int
psdf_lineto(gx_device_vector * vdev, floatp x0, floatp y0, floatp x, floatp y,
	    gx_path_type_t type)
{
    pprintg2(gdev_vector_stream(vdev), "%g %g l\n", x, y);
    return 0;
}

int
psdf_curveto(gx_device_vector * vdev, floatp x0, floatp y0,
	   floatp x1, floatp y1, floatp x2, floatp y2, floatp x3, floatp y3,
	     gx_path_type_t type)
{
    if (x1 == x0 && y1 == y0)
	pprintg4(gdev_vector_stream(vdev), "%g %g %g %g v\n",
		 x2, y2, x3, y3);
    else if (x3 == x2 && y3 == y2)
	pprintg4(gdev_vector_stream(vdev), "%g %g %g %g y\n",
		 x1, y1, x2, y2);
    else
	pprintg6(gdev_vector_stream(vdev), "%g %g %g %g %g %g c\n",
		 x1, y1, x2, y2, x3, y3);
    return 0;
}

int
psdf_closepath(gx_device_vector * vdev, floatp x0, floatp y0,
	       floatp x_start, floatp y_start, gx_path_type_t type)
{
    pputs(gdev_vector_stream(vdev), "h\n");
    return 0;
}

/* endpath is deliberately omitted. */

/* ---------------- Utilities ---------------- */

int
psdf_set_color(gx_device_vector * vdev, const gx_drawing_color * pdc,
	       const char *rgs)
{
    if (!gx_dc_is_pure(pdc))
	return_error(gs_error_rangecheck);
    {
	stream *s = gdev_vector_stream(vdev);
	gx_color_index color = gx_dc_pure_color(pdc);
	float r = (color >> 16) / 255.0;
	float g = ((color >> 8) & 0xff) / 255.0;
	float b = (color & 0xff) / 255.0;

	if (r == g && g == b)
	    pprintg1(s, "%g", r), pprints1(s, " %s\n", rgs + 1);
	else
	    pprintg3(s, "%g %g %g", r, g, b), pprints1(s, " %s\n", rgs);
    }
    return 0;
}

/* ---------------- Binary data writing ---------------- */

/* Begin writing binary data. */
int
psdf_begin_binary(gx_device_psdf * pdev, psdf_binary_writer * pbw)
{
    pbw->strm = pdev->strm;
    pbw->dev = pdev;
    /* If not binary, set up the encoding stream. */
    if (!pdev->binary_ok)
	psdf_encode_binary(pbw, &s_A85E_template, NULL);
    return 0;
}

/* Add an encoding filter.  The client must have allocated the stream state, */
/* if any, using pdev->v_memory. */
int
psdf_encode_binary(psdf_binary_writer * pbw, const stream_template * template,
		   stream_state * ss)
{
    gx_device_psdf *pdev = pbw->dev;
    gs_memory_t *mem = pdev->v_memory;
    stream *es = s_alloc(mem, "psdf_encode_binary(stream)");
    stream_state *ess = (ss == 0 ? (stream_state *) es : ss);
    uint bsize = max(template->min_out_size, 256);	/* arbitrary */
    byte *buf = gs_alloc_bytes(mem, bsize, "psdf_encode_binary(buf)");

    if (es == 0 || buf == 0) {
	gs_free_object(mem, buf, "psdf_encode_binary(buf)");
	gs_free_object(mem, es, "psdf_encode_binary(stream)");
	return_error(gs_error_VMerror);
    }
    if (ess == 0)
	ess = (stream_state *) es;
    s_std_init(es, buf, bsize, &s_filter_write_procs, s_mode_write);
    ess->template = template;
    ess->memory = mem;
    es->procs.process = template->process;
    es->memory = mem;
    es->state = ess;
    if (template->init)
	(*template->init) (ess);
    es->strm = pbw->strm;
    pbw->strm = es;
    return 0;
}

/* Add a 2-D CCITTFax encoding filter. */
int
psdf_CFE_binary(psdf_binary_writer * pbw, int w, int h, bool invert)
{
    gx_device_psdf *pdev = pbw->dev;
    gs_memory_t *mem = pdev->v_memory;
    const stream_template *template = &s_CFE_template;
    stream_CFE_state *st =
    gs_alloc_struct(mem, stream_CFE_state, template->stype,
		    "psdf_CFE_binary");
    int code;

    if (st == 0)
	return_error(gs_error_VMerror);
    (*template->set_defaults) ((stream_state *) st);
    st->K = -1;
    st->Columns = w;
    st->Rows = h;
    st->BlackIs1 = !invert;
    code = psdf_encode_binary(pbw, template, (stream_state *) st);
    if (code < 0)
	gs_free_object(mem, st, "psdf_CFE_binary");
    return code;
}

/* Finish writing binary data. */
int
psdf_end_binary(psdf_binary_writer * pbw)
{
    gx_device_psdf *pdev = pbw->dev;

    /* Close the filters in reverse order. */
    /* Stop before we try to close the file stream. */
    while (pbw->strm != pdev->strm) {
	stream *next = pbw->strm->strm;

	sclose(pbw->strm);
	pbw->strm = next;
    }
    return 0;
}

/*
 * Write a string in its shortest form ( () or <> ).  Note that
 * this form is different depending on whether binary data are allowed.
 * Currently we don't support ASCII85 strings ( <~ ~> ).
 */
void
psdf_write_string(stream * s, const byte * str, uint size, int print_ok)
{
    uint added = 0;
    uint i;
    const stream_template *template;
    stream_AXE_state state;
    stream_state *st = NULL;

    if (print_ok & print_binary_ok) {	/* Only need to escape (, ), \, CR, EOL. */
	pputc(s, '(');
	for (i = 0; i < size; ++i) {
	    byte ch = str[i];

	    switch (ch) {
		case char_CR:
		    pputs(s, "\\r");
		    continue;
		case char_EOL:
		    pputs(s, "\\n");
		    continue;
		case '(':
		case ')':
		case '\\':
		    pputc(s, '\\');
	    }
	    pputc(s, ch);
	}
	pputc(s, ')');
	return;
    }
    for (i = 0; i < size; ++i) {
	byte ch = str[i];

	if (ch == 0 || ch >= 127)
	    added += 3;
	else if (strchr("()\\\n\r\t\b\f", ch) != 0)
	    ++added;
	else if (ch < 32)
	    added += 3;
    }

    if (added < size) {		/* More efficient to represent as PostScript string. */
	template = &s_PSSE_template;
	pputc(s, '(');
    } else {			/* More efficient to represent as hex string. */
	template = &s_AXE_template;
	st = (stream_state *) & state;
	s_AXE_init_inline(&state);
	pputc(s, '<');
    }

    {
	byte buf[100];		/* size is arbitrary */
	stream_cursor_read r;
	stream_cursor_write w;
	int status;

	r.ptr = str - 1;
	r.limit = r.ptr + size;
	w.limit = buf + sizeof(buf) - 1;
	do {
	    w.ptr = buf - 1;
	    status = (*template->process) (st, &r, &w, true);
	    pwrite(s, buf, (uint) (w.ptr + 1 - buf));
	}
	while (status == 1);
    }
}

/* Set up a write stream that just keeps track of the position. */
int
psdf_alloc_position_stream(stream ** ps, gs_memory_t * mem)
{
    stream *s = *ps = s_alloc(mem, "psdf_alloc_position_stream");

    if (s == 0)
	return_error(gs_error_VMerror);
    swrite_position_only(s);
    return 0;
}

/* ---------------- Parameter printing ---------------- */

typedef struct printer_param_list_s {
    gs_param_list_common;
    stream *strm;
    param_printer_params_t params;
    int print_ok;
    bool any;
} printer_param_list_t;

gs_private_st_ptrs1(st_printer_param_list, printer_param_list_t,
  "printer_param_list_t", printer_plist_enum_ptrs, printer_plist_reloc_ptrs,
		    strm);
const param_printer_params_t param_printer_params_default =
{
    param_printer_params_default_values
};

/* We'll implement the other printers later if we have to. */
private param_proc_xmit_typed(param_print_typed);
/*private param_proc_begin_xmit_collection(param_print_begin_collection); */
/*private param_proc_end_xmit_collection(param_print_end_collection); */
private const gs_param_list_procs printer_param_list_procs = {
    param_print_typed,
    NULL /* begin_collection */ ,
    NULL /* end_collection */ ,
    NULL /* get_next_key */ ,
    gs_param_request_default,
    gs_param_requested_default
};

int
psdf_alloc_param_printer(gs_param_list ** pplist,
			 const param_printer_params_t * ppp, stream * s,
			 int print_ok, gs_memory_t * mem)
{
    printer_param_list_t *prlist =
    gs_alloc_struct(mem, printer_param_list_t, &st_printer_param_list,
		    "psdf_alloc_param_printer");

    *pplist = (gs_param_list *) prlist;
    if (prlist == 0)
	return_error(gs_error_VMerror);
    prlist->procs = &printer_param_list_procs;
    prlist->memory = mem;
    prlist->strm = s;
    prlist->params = *ppp;
    prlist->print_ok = print_ok;
    prlist->any = false;
    return 0;
}

void
psdf_free_param_printer(gs_param_list * plist)
{
    if (plist) {
	printer_param_list_t *prlist = (printer_param_list_t *) plist;

	if (prlist->any && prlist->params.suffix)
	    pputs(prlist->strm, prlist->params.suffix);
	gs_free_object(prlist->memory, plist, "psdf_free_param_printer");
    }
}

#define prlist ((printer_param_list_t *)plist)
private int
param_print_typed(gs_param_list * plist, gs_param_name pkey,
		  gs_param_typed_value * pvalue)
{
    stream *s = prlist->strm;

    if (!prlist->any) {
	if (prlist->params.prefix)
	    pputs(s, prlist->params.prefix);
	prlist->any = true;
    }
    if (prlist->params.item_prefix)
	pputs(s, prlist->params.item_prefix);
    pprints1(s, "/%s", pkey);
    switch (pvalue->type) {
	case gs_param_type_null:
	    pputs(s, " null");
	    break;
	case gs_param_type_bool:
	    pputs(s, (pvalue->value.b ? " true" : " false"));
	    break;
	case gs_param_type_int:
	    pprintd1(s, " %d", pvalue->value.i);
	    break;
	case gs_param_type_long:
	    pprintld1(s, " %l", pvalue->value.l);
	    break;
	case gs_param_type_float:
	    pprintg1(s, " %g", pvalue->value.f);
	    break;
	case gs_param_type_string:
	    psdf_write_string(s, pvalue->value.s.data, pvalue->value.s.size,
			      prlist->print_ok);
	    break;
	case gs_param_type_name:
/****** SHOULD USE #-ESCAPES FOR PDF ******/
	    pputc(s, '/');
	    pwrite(s, pvalue->value.n.data, pvalue->value.n.size);
	    break;
	case gs_param_type_int_array:
	    {
		uint i;
		char sepr = (pvalue->value.ia.size <= 10 ? ' ' : '\n');

		pputc(s, '[');
		for (i = 0; i < pvalue->value.ia.size; ++i) {
		    pprintd1(s, "%d", pvalue->value.ia.data[i]);
		    pputc(s, sepr);
		}
		pputc(s, ']');
	    }
	    break;
	case gs_param_type_float_array:
	    {
		uint i;
		char sepr = (pvalue->value.fa.size <= 10 ? ' ' : '\n');

		pputc(s, '[');
		for (i = 0; i < pvalue->value.fa.size; ++i) {
		    pprintg1(s, "%g", pvalue->value.fa.data[i]);
		    pputc(s, sepr);
		}
		pputc(s, ']');
	    }
	    break;
	    /*case gs_param_type_string_array: */
	    /*case gs_param_type_name_array: */
	default:
	    return_error(gs_error_typecheck);
    }
    if (prlist->params.item_suffix)
	pputs(s, prlist->params.item_suffix);
    return 0;
}

#undef prlist
