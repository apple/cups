/* Copyright (C) 1990, 1996, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: ibnum.c,v 1.2 2000/03/08 23:15:08 mike Exp $ */
/* Level 2 encoded number reading utilities for Ghostscript */
#include "math_.h"
#include "memory_.h"
#include "ghost.h"
#include "errors.h"
#include "stream.h"
#include "ibnum.h"
#include "imemory.h"		/* for iutil.h */
#include "iutil.h"

/* Define the number of bytes for a given format of encoded number. */
const byte enc_num_bytes[] = {
    enc_num_bytes_values
};

/* ------ Encoded number reading ------ */

/* Set up to read from an encoded number array/string. */
/* Return <0 for error, or a number format. */
int
num_array_format(const ref * op)
{
    switch (r_type(op)) {
	case t_string:
	    {
		/* Check that this is a legitimate encoded number string. */
		const byte *bp = op->value.bytes;
		int format;

		if (r_size(op) < 4 || bp[0] != bt_num_array_value)
		    return_error(e_rangecheck);
		format = bp[1];
		if (!num_is_valid(format) ||
		    sdecodeshort(bp + 2, format) !=
		    (r_size(op) - 4) / encoded_number_bytes(format)
		    )
		    return_error(e_rangecheck);
		return format;
	    }
	case t_array:
	case t_mixedarray:
	case t_shortarray:
	    return num_array;
	default:
	    return_error(e_typecheck);
    }
}

/* Get the number of elements in an encoded number array/string. */
uint
num_array_size(const ref * op, int format)
{
    return (format == num_array ? r_size(op) :
	    (r_size(op) - 4) / encoded_number_bytes(format));
}

/* Get an encoded number from an array/string according to the given format. */
/* Put the value in np->value.{intval,realval}. */
/* Return t_int if integer, t_real if real, t_null if end of stream, */
/* or an error if the format is invalid. */
int
num_array_get(const ref * op, int format, uint index, ref * np)
{
    if (format == num_array) {
	int code = array_get(op, (long)index, np);

	if (code < 0)
	    return t_null;
	switch (r_type(np)) {
	    case t_integer:
		return t_integer;
	    case t_real:
		return t_real;
	    default:
		return_error(e_rangecheck);
	}
    } else {
	uint nbytes = encoded_number_bytes(format);

	if (index >= (r_size(op) - 4) / nbytes)
	    return t_null;
	return sdecode_number(op->value.bytes + 4 + index * nbytes,
			      format, np);
    }
}

/* Internal routine to decode a number in a given format. */
/* Same returns as sget_encoded_number. */
static const double binary_scale[32] = {
#define EXPN2(n) (0.5 / (1L << (n-1)))
    1.0, EXPN2(1), EXPN2(2), EXPN2(3),
    EXPN2(4), EXPN2(5), EXPN2(6), EXPN2(7),
    EXPN2(8), EXPN2(9), EXPN2(10), EXPN2(11),
    EXPN2(12), EXPN2(13), EXPN2(14), EXPN2(15),
    EXPN2(16), EXPN2(17), EXPN2(18), EXPN2(19),
    EXPN2(20), EXPN2(21), EXPN2(22), EXPN2(23),
    EXPN2(24), EXPN2(25), EXPN2(26), EXPN2(27),
    EXPN2(28), EXPN2(29), EXPN2(30), EXPN2(31)
#undef EXPN2
};
int
sdecode_number(const byte * str, int format, ref * np)
{
    switch (format & 0x170) {
	case num_int32:
	case num_int32 + 16:
	    if ((format & 31) == 0) {
		np->value.intval = sdecodelong(str, format);
		return t_integer;
	    } else {
		np->value.realval =
		    (double)sdecodelong(str, format) *
		    binary_scale[format & 31];
		return t_real;
	    }
	case num_int16:
	    if ((format & 15) == 0) {
		np->value.intval = sdecodeshort(str, format);
		return t_integer;
	    } else {
		np->value.realval =
		    sdecodeshort(str, format) *
		    binary_scale[format & 15];
		return t_real;
	    }
	case num_float:
	    np->value.realval = sdecodefloat(str, format);
	    return t_real;
	default:
	    return_error(e_syntaxerror);	/* invalid format?? */
    }
}

/* ------ Decode number ------ */

/* Decode encoded numbers from a string according to format. */

/* Decode a (16-bit, signed or unsigned) short. */
uint
sdecodeushort(const byte * p, int format)
{
    int a = p[0], b = p[1];

    return (num_is_lsb(format) ? (b << 8) + a : (a << 8) + b);
}
int
sdecodeshort(const byte * p, int format)
{
    int v = (int)sdecodeushort(p, format);

    return (v & 0x7fff) - (v & 0x8000);
}

/* Decode a (32-bit, signed) long. */
long
sdecodelong(const byte * p, int format)
{
    int a = p[0], b = p[1], c = p[2], d = p[3];
    long v = (num_is_lsb(format) ?
	      ((long)d << 24) + ((long)c << 16) + (b << 8) + a :
	      ((long)a << 24) + ((long)b << 16) + (c << 8) + d);

    /*
     * The following is only needed if sizeof(long) > 4, but it does
     * no harm if sizeof(long) == 4.
     */
    return (v ^ 0x80000000L) - 0x80000000L;
}

/* Decode a float.  We assume that native floats occupy 32 bits. */
float
sdecodefloat(const byte * p, int format)
{
    bits32 lnum = (bits32) sdecodelong(p, format);
    float fnum;

#if !arch_floats_are_IEEE
    if (format != num_float_native) {
	/* We know IEEE floats take 32 bits. */
	/* Convert IEEE float to native float. */
	int sign_expt = lnum >> 23;
	int expt = sign_expt & 0xff;
	long mant = lnum & 0x7fffff;

	if (expt == 0 && mant == 0)
	    fnum = 0;
	else {
	    mant += 0x800000;
	    fnum = (float)ldexp((float)mant, expt - 127 - 24);
	}
	if (sign_expt & 0x100)
	    fnum = -fnum;
    } else
#endif
	fnum = *(float *)&lnum;
    return fnum;
}
