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

/*$Id: zchar32.c,v 1.1 2000/03/08 23:15:32 mike Exp $ */
/* Type 32 font glyph operators */
#include "ghost.h"
#include "oper.h"
#include "gsccode.h"		/* for gxfont.h */
#include "gsmatrix.h"
#include "gsutil.h"
#include "gxfixed.h"
#include "gxchar.h"
#include "gxfont.h"
#include "gxfcache.h"
#include "ifont.h"
#include "igstate.h"
#include "store.h"

/* ([wx wy llx lly urx ury] | [w0x w0y llx lly urx ury w1x w1y vx vy]) */
/*   <bitmap> <cid> <type32font> <str22> .makeglyph32 <<same with substr>> */
private int
zmakeglyph32(os_ptr op)
{
    bool long_form;
    uint msize;
    double metrics[10];
    int wx, llx, lly, urx, ury;
    int width, height, raster;
    gs_font *pfont;
    int code;
    byte *str;

    check_array(op[-4]);
    msize = r_size(op - 4);
    switch (msize) {
	case 10:
	    long_form = true;
	    break;
	case 6:
	    long_form = false;
	    break;
	default:
	    return_error(e_rangecheck);
    }
    code = num_params(op[-4].value.refs + msize - 1, msize, metrics);
    if (code < 0)
	return code;
    if (~code & 0x3c)		/* check llx .. ury for integers */
	return_error(e_typecheck);
    check_read_type(op[-3], t_string);
    llx = (int)metrics[2];
    lly = (int)metrics[3];
    urx = (int)metrics[4];
    ury = (int)metrics[5];
    width = urx - llx;
    height = ury - lly;
    raster = (width + 7) >> 3;
    if (width < 0 || height < 0 || r_size(op - 3) != raster * height)
	return_error(e_rangecheck);
    check_int_leu(op[-2], 65535);
    code = font_param(op - 1, &pfont);
    if (code < 0)
	return code;
    if (pfont->FontType != ft_CID_bitmap)
	return_error(e_invalidfont);
    check_write_type(*op, t_string);
    if (r_size(op) < 22)
	return_error(e_rangecheck);
    str = op->value.bytes;
    if (long_form || metrics[0] != (wx = (int)metrics[0]) ||
	metrics[1] != 0 || height == 0 ||
	((wx | width | height | (llx + 128) | (lly + 128)) & ~255) != 0
	) {
	/* Use the long form. */
	int i, n = (long_form ? 10 : 6);

	str[0] = 0;
	str[1] = long_form;
	for (i = 0; i < n; ++i) {
	    int v = (int)metrics[i];  /* no floating point widths yet */

	    str[2 + 2 * i] = (byte)(v >> 8);
	    str[2 + 2 * i + 1] = (byte)v;
	}
	r_set_size(op, 2 + n * 2);
    } else {
	/* Use the short form. */
	str[0] = (byte)width;
	str[1] = (byte)height;
	str[2] = (byte)wx;
	str[3] = (byte)(llx + 128);
	str[4] = (byte)(lly + 128);
	r_set_size(op, 5);
    }
    return code;
}

/* <cid_min> <cid_max> <type32font> .removeglyphs - */
typedef struct {
    gs_glyph cid_min, cid_max;
    gs_font *font;
} font_cid_range_t;
private bool
select_cid_range(cached_char * cc, void *range_ptr)
{
    const font_cid_range_t *range = range_ptr;

    return (cc->code >= range->cid_min &&
	    cc->code <= range->cid_max &&
	    cc->pair->font == range->font);
}
private int
zremoveglyphs(os_ptr op)
{
    int code;
    font_cid_range_t range;

    check_int_leu(op[-2], 65535);
    check_int_leu(op[-1], 65535);
    code = font_param(op, &range.font);
    if (code < 0)
	return code;
    if (range.font->FontType != ft_CID_bitmap)
	return_error(e_invalidfont);
    range.cid_min = gs_min_cid_glyph + op[-2].value.intval;
    range.cid_max = gs_min_cid_glyph + op[-1].value.intval;
    gx_purge_selected_cached_chars(range.font->dir, select_cid_range,
				   &range);
    pop(3);
    return 0;
}

/* <str5/14/22> .getmetrics32 <width> <height> <w0x> ... <vy> 5/14 */
/* <str5/14/22> .getmetrics32 <width> <height> <wx> ... <ury> 22 */
private int
zgetmetrics32(os_ptr op)
{
    const byte *data;
    uint size;
    int i, n = 6;
    os_ptr wop;

    check_read_type(*op, t_string);
    data = op->value.const_bytes;
    size = r_size(op);
    if (size < 5)
	return_error(e_rangecheck);
    if (data[0]) {
	/* Short form. */
	int llx = (int)data[3] - 128, lly = (int)data[4] - 128;

	n = 6;
	size = 5;
	push(8);
	make_int(op - 6, data[2]); /* wx */
	make_int(op - 5, 0);	/* wy */
	make_int(op - 4, llx);
	make_int(op - 3, lly);
	make_int(op - 2, llx + data[0]); /* urx */
	make_int(op - 1, lly + data[1]); /* ury */
    } else {
	if (data[1]) {
	    /* Long form, both WModes. */
	    if (size < 22)
		return_error(e_rangecheck);
	    n = 10;
	    size = 22;
	} else {
	    /* Long form, WMode = 0 only. */
	    if (size < 14)
		return_error(e_rangecheck);
	    n = 6;
	    size = 14;
	}
	push(2 + n);
	for (i = 0; i < n; ++i)
	    make_int(op - n + i,
		     ((int)((data[2 * i + 2] << 8) + data[2 * i + 3]) ^ 0x8000)
		       - 0x8000);
    }
    wop = op - n;
    make_int(wop - 2, wop[4].value.intval - wop[2].value.intval);
    make_int(wop - 1, wop[5].value.intval - wop[3].value.intval);
    make_int(op, size);
    return 0;
}

/* ------ Initialization procedure ------ */

const op_def zchar32_op_defs[] =
{
    {"1.getmetrics32", zgetmetrics32},
    {"4.makeglyph32", zmakeglyph32},
    {"3.removeglyphs", zremoveglyphs},
    op_def_end(0)
};
