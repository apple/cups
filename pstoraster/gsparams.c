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

/*$Id: gsparams.c,v 1.1 2000/03/08 23:14:45 mike Exp $ */
/* Generic parameter list serializer & expander */

/* Initial version 2/1/98 by John Desrosiers (soho@crl.com) */

#include "gx.h"
#include "memory_.h"
#include "gserrors.h"
#include "gsparams.h"

/* ----------- Local Type Decl's ------------ */
typedef struct {
    byte *buf;			/* current buffer ptr */
    byte *buf_end;		/* end of buffer */
    unsigned total_sizeof;	/* current # bytes in buf */
} WriteBuffer;

/* ---------- Forward refs ----------- */
private void
     align_to(P2(
		    const byte ** src,	/* pointer to align */
		    unsigned alignment	/* alignment, must be power of 2 */
	      ));
private void
     put_word(P2(
		    unsigned source,	/* number to put to buffer */
		    WriteBuffer * dest	/* destination descriptor */
	      ));
private void
     put_bytes(P3(
		     const byte * source,	/* bytes to put to buffer */
		     unsigned source_sizeof,	/* # bytes to put */
		     WriteBuffer * dest		/* destination descriptor */
	       ));
private void
     put_alignment(P2(
			 unsigned alignment,	/* alignment to match, must be power 2 */
			 WriteBuffer * dest	/* destination descriptor */
		   ));

/* Get word compressed with put_word */
private unsigned		/* decompressed word */
         get_word(P1(
			const byte ** src	/* UPDATES: ptr to src buf ptr */
		  ));


/* ------------ Serializer ------------ */
/* Serialize the contents of a gs_param_list (including sub-dicts) */
int				/* ret -ve err, else # bytes needed to represent param list, whether */

/* or not it actually fit into buffer. List was successully */

/* serialized only if if this # is <= supplied buf size. */
gs_param_list_serialize(
			   gs_param_list * list,	/* root of list to serialize */
					/* list MUST BE IN READ MODE */
			   byte * buf,	/* destination buffer (can be 0) */
			   int buf_sizeof	/* # bytes available in buf (can be 0) */
)
{
    int code = 0;
    int temp_code;
    gs_param_enumerator_t key_enum;
    gs_param_key_t key;
    WriteBuffer write_buf;

    write_buf.buf = buf;
    write_buf.buf_end = buf + (buf ? buf_sizeof : 0);
    write_buf.total_sizeof = 0;
    param_init_enumerator(&key_enum);

    /* Each item is serialized as ("word" means compressed word):
     *  word: key sizeof + 1, or 0 if end of list/dict
     *  word: data type(gs_param_type_xxx)
     *  byte[]: key, including trailing \0 
     *  (if simple type)
     *   byte[]: unpacked representation of data
     *  (if simple array or string)
     *   byte[]: unpacked mem image of gs_param_xxx_array structure
     *   pad: to array alignment
     *   byte[]: data associated with array contents
     *  (if string/name array)
     *   byte[]: unpacked mem image of gs_param_string_array structure
     *   pad: to void *
     *   { gs_param_string structure mem image;
     *     data associated with string;
     *   } for each string in array
     *  (if dict/dict_int_keys)
     *   word: # of entries in dict,
     *   pad: to void *
     *   dict entries follow immediately until end-of-dict
     *
     * NB that this format is designed to allow using an input buffer
     * as the direct source of data when expanding a gs_c_param_list
     */
    /* Enumerate all the keys; use keys to get their typed values */
    while ((code = param_get_next_key(list, &key_enum, &key)) == 0) {
	int value_top_sizeof;
	int value_base_sizeof;

	/* Get next datum & put its type & key to buffer */
	gs_param_typed_value value;
	char string_key[256];

	if (sizeof(string_key) < key.size + 1) {
	    code = gs_note_error(gs_error_rangecheck);
	    break;
	}
	memcpy(string_key, key.data, key.size);
	string_key[key.size] = 0;
	if ((code = param_read_typed(list, string_key, &value)) != 0) {
	    code = code > 0 ? gs_note_error(gs_error_unknownerror) : code;
	    break;
	}
	put_word((unsigned)key.size + 1, &write_buf);
	put_word((unsigned)value.type, &write_buf);
	put_bytes((byte *) string_key, key.size + 1, &write_buf);

	/* Put value & its size to buffer */
	value_top_sizeof = gs_param_type_sizes[value.type];
	value_base_sizeof = gs_param_type_base_sizes[value.type];
	switch (value.type) {
	    case gs_param_type_null:
	    case gs_param_type_bool:
	    case gs_param_type_int:
	    case gs_param_type_long:
	    case gs_param_type_float:
		put_bytes((byte *) & value.value, value_top_sizeof, &write_buf);
		break;

	    case gs_param_type_string:
	    case gs_param_type_name:
	    case gs_param_type_int_array:
	    case gs_param_type_float_array:
		put_bytes((byte *) & value.value, value_top_sizeof, &write_buf);
		put_alignment(value_base_sizeof, &write_buf);
		value_base_sizeof *= value.value.s.size;
		put_bytes(value.value.s.data, value_base_sizeof, &write_buf);
		break;

	    case gs_param_type_string_array:
	    case gs_param_type_name_array:
		value_base_sizeof *= value.value.sa.size;
		put_bytes((const byte *)&value.value, value_top_sizeof, &write_buf);
		put_alignment(sizeof(void *), &write_buf);

		put_bytes((const byte *)value.value.sa.data, value_base_sizeof,
			  &write_buf);
		{
		    int str_count;
		    const gs_param_string *sa;

		    for (str_count = value.value.sa.size,
			 sa = value.value.sa.data; str_count-- > 0; ++sa)
			put_bytes(sa->data, sa->size, &write_buf);
		}
		break;

	    case gs_param_type_dict:
	    case gs_param_type_dict_int_keys:
		put_word(value.value.d.size, &write_buf);
		put_alignment(sizeof(void *), &write_buf);

		{
		    int bytes_written =
		    gs_param_list_serialize(value.value.d.list,
					    write_buf.buf,
		     write_buf.buf ? write_buf.buf_end - write_buf.buf : 0);

		    temp_code = param_end_read_dict(list,
						    (const char *)key.data,
						    &value.value.d);
		    if (bytes_written < 0)
			code = bytes_written;
		    else {
			code = temp_code;
			if (bytes_written)
			    put_bytes(write_buf.buf, bytes_written, &write_buf);
		    }
		}
		break;

	    default:
		code = gs_note_error(gs_error_unknownerror);
		break;
	}
	if (code < 0)
	    break;
    }

    /* Write end marker, which is an (illegal) 0 key length */
    if (code >= 0) {
	put_word(0, &write_buf);
	code = write_buf.total_sizeof;
    }
    return code;
}


/* ------------ Expander --------------- */
/* Expand a buffer into a gs_param_list (including sub-dicts) */
int				/* ret -ve err, +ve # of chars read from buffer */
gs_param_list_unserialize(
			     gs_param_list * list,	/* root of list to expand to */
					/* list MUST BE IN WRITE MODE */
			     const byte * buf	/* source buffer */
)
{
    int code = 0;
    const byte *orig_buf = buf;

    do {
	gs_param_typed_value typed;
	gs_param_name key;
	unsigned key_sizeof;
	int value_top_sizeof;
	int value_base_sizeof;
	int temp_code;
	gs_param_type type;

	/* key length, 0 indicates end of data */
	key_sizeof = get_word(&buf);
	if (key_sizeof == 0)	/* end of data */
	    break;

	/* data type */
	type = (gs_param_type) get_word(&buf);

	/* key */
	key = (gs_param_name) buf;
	buf += key_sizeof;

	/* Data values */
	value_top_sizeof = gs_param_type_sizes[type];
	value_base_sizeof = gs_param_type_base_sizes[type];
	typed.type = type;
	if (type != gs_param_type_dict && type != gs_param_type_dict_int_keys) {
	    memcpy(&typed.value, buf, value_top_sizeof);
	    buf += value_top_sizeof;
	}
	switch (type) {
	    case gs_param_type_null:
	    case gs_param_type_bool:
	    case gs_param_type_int:
	    case gs_param_type_long:
	    case gs_param_type_float:
		break;

	    case gs_param_type_string:
	    case gs_param_type_name:
	    case gs_param_type_int_array:
	    case gs_param_type_float_array:
		align_to(&buf, value_base_sizeof);
		typed.value.s.data = buf;
		typed.value.s.persistent = false;
		buf += typed.value.s.size * value_base_sizeof;
		break;

	    case gs_param_type_string_array:
	    case gs_param_type_name_array:
		align_to(&buf, sizeof(void *));

		typed.value.sa.data = (const gs_param_string *)buf;
		typed.value.sa.persistent = false;
		buf += typed.value.s.size * value_base_sizeof;
		{
		    int str_count;
		    gs_param_string *sa;

		    for (str_count = typed.value.sa.size,
			 sa = (gs_param_string *) typed.value.sa.data;
			 str_count-- > 0; ++sa) {
			sa->data = buf;
			sa->persistent = false;
			buf += sa->size;
		    }
		}
		break;

	    case gs_param_type_dict:
	    case gs_param_type_dict_int_keys:
		typed.value.d.size = get_word(&buf);
		code = param_begin_write_dict
		    (list, key, &typed.value.d, type == gs_param_type_dict_int_keys);
		if (code < 0)
		    break;
		align_to(&buf, sizeof(void *));

		code = gs_param_list_unserialize(typed.value.d.list, buf);
		temp_code = param_end_write_dict(list, key, &typed.value.d);
		if (code >= 0) {
		    buf += code;
		    code = temp_code;
		}
		break;

	    default:
		code = gs_note_error(gs_error_unknownerror);
		break;
	}
	if (code < 0)
	    break;
	if (typed.type != gs_param_type_dict && typed.type != gs_param_type_dict_int_keys)
	    code = param_write_typed(list, key, &typed);
    }
    while (code >= 0);

    return code >= 0 ? buf - orig_buf : code;
}


/* ---------- Utility functions -------- */

/* Align a byte pointer on the next Nth byte */
private void
align_to(
	    const byte ** src,	/* pointer to align */
	    unsigned alignment	/* alignment, must be power of 2 */
)
{
    *src += -(int)alignment_mod(*src, alignment) & (alignment - 1);
}

/* Put compressed word repr to a buffer */
private void
put_word(
	    unsigned source,	/* number to put to buffer */
	    WriteBuffer * dest	/* destination descriptor */
)
{
    do {
	byte chunk = source & 0x7f;

	if (source >= 0x80)
	    chunk |= 0x80;
	source >>= 7;
	++dest->total_sizeof;
	if (dest->buf && dest->buf < dest->buf_end)
	    *dest->buf++ = chunk;
    }
    while (source != 0);
}

/* Put array of bytes to buffer */
private void
put_bytes(
	     const byte * source,	/* bytes to put to buffer */
	     unsigned source_sizeof,	/* # bytes to put */
	     WriteBuffer * dest	/* destination descriptor */
)
{
    dest->total_sizeof += source_sizeof;
    if (dest->buf && dest->buf + source_sizeof <= dest->buf_end) {
	if (dest->buf != source)
	    memcpy(dest->buf, source, source_sizeof);
	dest->buf += source_sizeof;
    }
}

/* Pad destination out to req'd alignment w/zeros */
private void
put_alignment(
		 unsigned alignment,	/* alignment to match, must be power 2 */
		 WriteBuffer * dest	/* destination descriptor */
)
{
    static const byte zero =
    {0};

    while ((dest->total_sizeof & (alignment - 1)) != 0)
	put_bytes(&zero, 1, dest);
}

/* Get word compressed with put_word */
private unsigned		/* decompressed word */
get_word(
	    const byte ** src	/* UPDATES: ptr to src buf ptr */
)
{
    unsigned dest = 0;
    byte chunk;
    unsigned shift = 0;

    do {
	chunk = *(*src)++;
	dest |= (chunk & 0x7f) << shift;
	shift += 7;
    }
    while (chunk & 0x80);

    return dest;
}
