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

/*$Id: gsdsrc.c,v 1.1 2000/03/08 23:14:40 mike Exp $ */
/* DataSource procedures */

#include "memory_.h"
#include "gx.h"
#include "gsdsrc.h"
#include "gserrors.h"
#include "stream.h"

/* GC descriptor */
public_st_data_source();
#define psrc ((gs_data_source_t *)vptr)
private 
ENUM_PTRS_BEGIN(data_source_enum_ptrs)
{
    if (psrc->type == data_source_type_string)
	ENUM_RETURN_CONST_STRING_PTR(gs_data_source_t, data.str);
    else if (psrc->type == data_source_type_stream)
	ENUM_RETURN_PTR(gs_data_source_t, data.strm);
    else			/* bytes or floats */
	ENUM_RETURN_PTR(gs_data_source_t, data.str.data);
}
ENUM_PTRS_END
private RELOC_PTRS_BEGIN(data_source_reloc_ptrs)
{
    if (psrc->type == data_source_type_string)
	RELOC_CONST_STRING_PTR(gs_data_source_t, data.str);
    else if (psrc->type == data_source_type_stream)
	RELOC_PTR(gs_data_source_t, data.strm);
    else			/* bytes or floats */
	RELOC_PTR(gs_data_source_t, data.str.data);
}
RELOC_PTRS_END
#undef psrc

/* Access data from a string or a byte object. */
/* Does *not* check bounds. */
int
data_source_access_string(const gs_data_source_t * psrc, ulong start,
			  uint length, byte * buf, const byte ** ptr)
{
    const byte *p = psrc->data.str.data + start;

    if (ptr)
	*ptr = p;
    else
	memcpy(buf, p, length);
    return 0;
}
/* access_bytes is identical to access_string, but has a different */
/* GC procedure. */
int
data_source_access_bytes(const gs_data_source_t * psrc, ulong start,
			 uint length, byte * buf, const byte ** ptr)
{
    const byte *p = psrc->data.str.data + start;

    if (ptr)
	*ptr = p;
    else
	memcpy(buf, p, length);
    return 0;
}

/* Access data from a stream. */
/* Returns gs_error_rangecheck if out of bounds. */
int
data_source_access_stream(const gs_data_source_t * psrc, ulong start,
			  uint length, byte * buf, const byte ** ptr)
{
    stream *s = psrc->data.strm;
    const byte *p;

    if (start >= s->position &&
	(p = start - s->position + s->cbuf) + length <=
	s->cursor.r.limit + 1
	) {
	if (ptr)
	    *ptr = p;
	else
	    memcpy(buf, p, length);
    } else {
	uint nread;
	int code = sseek(s, start);

	if (code < 0)
	    return_error(gs_error_rangecheck);
	code = sgets(s, buf, length, &nread);
	if (code < 0)
	    return_error(gs_error_rangecheck);
	if (nread != length)
	    return_error(gs_error_rangecheck);
	if (ptr)
	    *ptr = buf;
    }
    return 0;
}
