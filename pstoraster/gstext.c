/* Copyright (C) 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gstext.c,v 1.1 2000/03/08 23:14:48 mike Exp $ */
/* Driver text interface support */
#include "std.h"
#include "gstypes.h"
#include "gdebug.h"
#include "gserror.h"
#include "gserrors.h"
#include "gsmemory.h"
#include "gsstruct.h"
#include "gstypes.h"
#include "gxdevcli.h"
#include "gxpath.h"
#include "gxtext.h"
#include "gzstate.h"

/* GC descriptors */
public_st_gs_text_params();
public_st_gs_text_enum();

#define tptr ((gs_text_params_t *)vptr)

private 
ENUM_PTRS_BEGIN(text_params_enum_ptrs) return 0;

case 0:
if (tptr->operation & TEXT_FROM_STRING) {
    /*
     * We only need the string descriptor temporarily, but we can't
     * put it in a local variable, because that would create a dangling
     * pointer as soon as we return.
     */
    tptr->gc_string.data = tptr->data.bytes;
    tptr->gc_string.size = tptr->size;
    return ENUM_CONST_STRING(&tptr->gc_string);
}
if (tptr->operation & TEXT_FROM_BYTES)
    return ENUM_OBJ(tptr->data.bytes);
if (tptr->operation & TEXT_FROM_CHARS)
    return ENUM_OBJ(tptr->data.chars);
if (tptr->operation & TEXT_FROM_GLYPHS)
    return ENUM_OBJ(tptr->data.glyphs);
return ENUM_OBJ(NULL);
case 1:
return ENUM_OBJ(tptr->operation & TEXT_REPLACE_X_WIDTHS ?
		tptr->x_widths : NULL);
case 2:
return ENUM_OBJ(tptr->operation & TEXT_REPLACE_Y_WIDTHS ?
		tptr->y_widths : NULL);
ENUM_PTRS_END

private RELOC_PTRS_BEGIN(text_params_reloc_ptrs)
{
    if (tptr->operation & TEXT_FROM_STRING) {
	gs_const_string str;

	str.data = tptr->data.bytes;
	str.size = tptr->size;
	RELOC_CONST_STRING_VAR(str);
	tptr->data.bytes = str.data;
    } else if (tptr->operation & TEXT_FROM_BYTES)
	RELOC_OBJ_VAR(tptr->data.bytes);
    else if (tptr->operation & TEXT_FROM_CHARS)
	RELOC_OBJ_VAR(tptr->data.chars);
    else if (tptr->operation & TEXT_FROM_GLYPHS)
	RELOC_OBJ_VAR(tptr->data.glyphs);
    if (tptr->operation & TEXT_REPLACE_X_WIDTHS)
	RELOC_OBJ_VAR(tptr->x_widths);
    if (tptr->operation & TEXT_REPLACE_Y_WIDTHS)
	RELOC_OBJ_VAR(tptr->y_widths);
}
RELOC_PTRS_END

#undef tptr

#define eptr ((gs_text_enum_t *)vptr)

private ENUM_PTRS_BEGIN(text_enum_enum_ptrs) ENUM_USING(st_gs_text_params, &eptr->text, sizeof(eptr->text), index - 1);
case 0:
return ENUM_OBJ(gx_device_enum_ptr(eptr->dev));
ENUM_PTRS_END

private RELOC_PTRS_BEGIN(text_enum_reloc_ptrs)
{
    RELOC_USING(st_gs_text_params, &eptr->text, sizeof(eptr->text));
    gx_device_reloc_ptr(eptr->dev, gcst);
}
RELOC_PTRS_END

#undef eptr

/* Begin processing text. */
int
gx_device_text_begin(gx_device * dev, gs_imager_state * pis,
		     const gs_text_params_t * text, const gs_font * font,
		     gx_path * path,	/* unless DO_NONE & !RETURN_WIDTH */
		     const gx_device_color * pdcolor,	/* DO_DRAW */
		     const gx_clip_path * pcpath,	/* DO_DRAW */
		     gs_memory_t * mem, gs_text_enum_t ** ppte)
{
    if (TEXT_OPERATION_IS_INVALID(text->operation))
	return_error(gs_error_rangecheck);
    {
	gx_path *tpath =
	    ((text->operation & TEXT_DO_NONE) &&
	     !(text->operation & TEXT_RETURN_WIDTH) ? 0 : path);
	int code =
	    (*dev_proc(dev, text_begin))
	    (dev, pis, text, font, tpath,
	     (text->operation & TEXT_DO_DRAW ? pdcolor : 0),
	     (text->operation & TEXT_DO_DRAW ? pcpath : 0),
	     mem, ppte);
	gs_text_enum_t *pte = *ppte;

	if (code < 0)
	    return code;
	pte->text = *text;
	pte->dev = dev;
	pte->index = 0;
	return code;
    }
}

/* Begin processing text based on a graphics state. */
int
gs_text_begin(gs_state * pgs, const gs_text_params_t * text,
	      gs_memory_t * mem, gs_text_enum_t ** ppte)
{
    gx_clip_path *pcpath = 0;

    if (text->operation & TEXT_DO_DRAW) {
	int code = gx_effective_clip_path(pgs, &pcpath);

	if (code < 0)
	    return code;
	gx_set_dev_color(pgs);
    }
    return gx_device_text_begin(pgs->device, (gs_imager_state *) pgs,
				text, pgs->font, pgs->path, pgs->dev_color,
				pcpath, mem, ppte);
}

/* Begin PostScript-equivalent text operations. */
int
gs_show_begin(gs_state * pgs, const byte * str, uint size,
	      gs_memory_t * mem, gs_text_enum_t ** ppte)
{
    gs_text_params_t text;

    text.operation = TEXT_FROM_STRING | TEXT_DO_DRAW | TEXT_RETURN_WIDTH;
    text.data.bytes = str, text.size = size;
    return gs_text_begin(pgs, &text, mem, ppte);
}
int
gs_ashow_begin(gs_state * pgs, floatp ax, floatp ay, const byte * str, uint size,
	       gs_memory_t * mem, gs_text_enum_t ** ppte)
{
    gs_text_params_t text;

    text.operation = TEXT_FROM_STRING | TEXT_ADD_TO_ALL_WIDTHS |
	TEXT_DO_DRAW | TEXT_RETURN_WIDTH;
    text.data.bytes = str, text.size = size;
    text.delta_all.x = ax;
    text.delta_all.y = ay;
    return gs_text_begin(pgs, &text, mem, ppte);
}
int
gs_widthshow_begin(gs_state * pgs, floatp cx, floatp cy, gs_char chr,
		   const byte * str, uint size,
		   gs_memory_t * mem, gs_text_enum_t ** ppte)
{
    gs_text_params_t text;

    text.operation = TEXT_FROM_STRING | TEXT_ADD_TO_SPACE_WIDTH |
	TEXT_DO_DRAW | TEXT_RETURN_WIDTH;
    text.data.bytes = str, text.size = size;
    text.delta_space.x = cx;
    text.delta_space.y = cy;
    text.space.s_char = chr;
    return gs_text_begin(pgs, &text, mem, ppte);
}
int
gs_awidthshow_begin(gs_state * pgs, floatp cx, floatp cy, gs_char chr,
		    floatp ax, floatp ay, const byte * str, uint size,
		    gs_memory_t * mem, gs_text_enum_t ** ppte)
{
    gs_text_params_t text;

    text.operation = TEXT_FROM_STRING |
	TEXT_ADD_TO_ALL_WIDTHS | TEXT_ADD_TO_SPACE_WIDTH |
	TEXT_DO_DRAW | TEXT_RETURN_WIDTH;
    text.data.bytes = str, text.size = size;
    text.delta_space.x = cx;
    text.delta_space.y = cy;
    text.space.s_char = chr;
    text.delta_all.x = ax;
    text.delta_all.y = ay;
    return gs_text_begin(pgs, &text, mem, ppte);
}
int
gs_kshow_begin(gs_state * pgs, const byte * str, uint size,
	       gs_memory_t * mem, gs_text_enum_t ** ppte)
{
    gs_text_params_t text;

    text.operation = TEXT_FROM_STRING | TEXT_DO_DRAW | TEXT_INTERVENE |
	TEXT_RETURN_WIDTH;
    text.data.bytes = str, text.size = size;
    return gs_text_begin(pgs, &text, mem, ppte);
}
int
gs_xyshow_begin(gs_state * pgs, const byte * str, uint size,
		const float *x_widths, const float *y_widths,
		gs_memory_t * mem, gs_text_enum_t ** ppte)
{
    gs_text_params_t text;

    text.operation = TEXT_FROM_STRING |
	TEXT_REPLACE_X_WIDTHS | TEXT_REPLACE_Y_WIDTHS |
	TEXT_DO_DRAW | TEXT_INTERVENE | TEXT_RETURN_WIDTH;
    text.data.bytes = str, text.size = size;
    text.x_widths = x_widths;
    text.y_widths = y_widths;
    return gs_text_begin(pgs, &text, mem, ppte);
}
int
gs_glyphshow_begin(gs_state * pgs, gs_glyph glyph,
		   gs_memory_t * mem, gs_text_enum_t ** ppte)
{
    gs_text_params_t text;

    /****** SET glyphs ******/
    text.size = 1;
    text.operation = TEXT_FROM_GLYPHS | TEXT_DO_DRAW | TEXT_RETURN_WIDTH;
    return gs_text_begin(pgs, &text, mem, ppte);
}
int
gs_cshow_begin(gs_state * pgs, const byte * str, uint size,
	       gs_memory_t * mem, gs_text_enum_t ** ppte)
{
    gs_text_params_t text;

    text.operation = TEXT_FROM_STRING | TEXT_DO_NONE;
    text.data.bytes = str, text.size = size;
    return gs_text_begin(pgs, &text, mem, ppte);
}
int
gs_stringwidth_begin(gs_state * pgs, const byte * str, uint size,
		     gs_memory_t * mem, gs_text_enum_t ** ppte)
{
    gs_text_params_t text;

    text.operation = TEXT_FROM_STRING | TEXT_DO_NONE | TEXT_RETURN_WIDTH;
    text.data.bytes = str, text.size = size;
    return gs_text_begin(pgs, &text, mem, ppte);
}
int
gs_charpath_begin(gs_state * pgs, const byte * str, uint size, bool stroke_path,
		  gs_memory_t * mem, gs_text_enum_t ** ppte)
{
    gs_text_params_t text;

    text.operation = TEXT_FROM_STRING | TEXT_RETURN_WIDTH |
	(stroke_path ? TEXT_DO_TRUE_CHARPATH : TEXT_DO_FALSE_CHARPATH);
    text.data.bytes = str, text.size = size;
    return gs_text_begin(pgs, &text, mem, ppte);
}
int
gs_charboxpath_begin(gs_state * pgs, const byte * str, uint size,
		bool stroke_path, gs_memory_t * mem, gs_text_enum_t ** ppte)
{
    gs_text_params_t text;

    text.operation = TEXT_FROM_STRING | TEXT_RETURN_WIDTH |
	(stroke_path ? TEXT_DO_TRUE_CHARBOXPATH : TEXT_DO_FALSE_CHARBOXPATH);
    text.data.bytes = str, text.size = size;
    return gs_text_begin(pgs, &text, mem, ppte);
}
int
gs_glyphpath_begin(gs_state * pgs, gs_glyph glyph, bool stroke_path,
		   gs_memory_t * mem, gs_text_enum_t ** ppte)
{
    gs_text_params_t text;

    text.operation = TEXT_FROM_GLYPHS | TEXT_RETURN_WIDTH |
	(stroke_path ? TEXT_DO_TRUE_CHARPATH : TEXT_DO_FALSE_CHARPATH);
    /****** SET glyphs ******/
    text.size = 1;
    return gs_text_begin(pgs, &text, mem, ppte);
}

/* Process text after 'begin'. */
int
gs_text_process(gs_text_enum_t * pte)
{
    return pte->procs->process(pte);
}

/* Set text metrics and optionally enable caching. */
int
gs_text_setcharwidth(gs_text_enum_t * pte, const double wxy[2])
{
    return pte->procs->set_cache(pte, wxy, TEXT_SET_CHAR_WIDTH);
}
int
gs_text_setcachedevice(gs_text_enum_t * pte, const double wbox[6])
{
    return pte->procs->set_cache(pte, wbox, TEXT_SET_CACHE_DEVICE);
}
int
gs_text_setcachedevice2(gs_text_enum_t * pte, const double wbox2[10])
{
    return pte->procs->set_cache(pte, wbox2, TEXT_SET_CACHE_DEVICE2);
}

/* Release the text processing structures. */
void
gs_text_release(gs_text_enum_t * pte, client_name_t cname)
{
    rc_decrement_only(pte, cname);
}
