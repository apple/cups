/* Copyright (C) 1993, 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: sstring.c,v 1.2 2000/03/08 23:15:28 mike Exp $ */
/* String and hexstring streams (filters) */
#include "stdio_.h"		/* includes std.h */
#include "memory_.h"
#include "string_.h"
#include "strimpl.h"
#include "sstring.h"
#include "scanchar.h"

/* ------ ASCIIHexEncode ------ */

private_st_AXE_state();

/* Initialize the state */
private int
s_AXE_init(stream_state * st)
{
    stream_AXE_state *const ss = (stream_AXE_state *) st;

    return s_AXE_init_inline(ss);
}

/* Process a buffer */
private int
s_AXE_process(stream_state * st, stream_cursor_read * pr,
	      stream_cursor_write * pw, bool last)
{
    stream_AXE_state *const ss = (stream_AXE_state *) st;
    const byte *p = pr->ptr;
    byte *q = pw->ptr;
    int rcount = pr->limit - p;
    int wcount = pw->limit - q;
    int count;
    int pos = ss->count;
    const char *hex_digits = "0123456789abcdef";
    int status = 0;

    if (last)
	wcount--;		/* leave room for '>' */
    wcount -= (wcount + 64) / 65;	/* leave room for \n */
    wcount >>= 1;		/* 2 chars per input byte */
    count = (wcount < rcount ? (status = 1, wcount) : rcount);
    while (--count >= 0) {
	*++q = hex_digits[*++p >> 4];
	*++q = hex_digits[*p & 0xf];
	if (!(++pos & 31) && (count != 0 || !last))
	    *++q = '\n';
    }
    if (last && status == 0)
	*++q = '>';
    pr->ptr = p;
    pw->ptr = q;
    ss->count = pos & 31;
    return status;
}

/* Stream template */
const stream_template s_AXE_template =
{&st_AXE_state, s_AXE_init, s_AXE_process, 1, 3
};

/* ------ ASCIIHexDecode ------ */

private_st_AXD_state();

/* Initialize the state */
private int
s_AXD_init(stream_state * st)
{
    stream_AXD_state *const ss = (stream_AXD_state *) st;

    return s_AXD_init_inline(ss);
}

/* Process a buffer */
private int
s_AXD_process(stream_state * st, stream_cursor_read * pr,
	      stream_cursor_write * pw, bool last)
{
    stream_AXD_state *const ss = (stream_AXD_state *) st;
    int code = s_hex_process(pr, pw, &ss->odd, hex_ignore_whitespace);

    switch (code) {
	case 0:
	    if (ss->odd >= 0 && last) {
		if (pw->ptr == pw->limit)
		    return 1;
		*++(pw->ptr) = ss->odd << 4;
	    }
	    /* falls through */
	case 1:
	    /* We still need to read ahead and check for EOD. */
	    for (; pr->ptr < pr->limit; pr->ptr++)
		if (scan_char_decoder[pr->ptr[1]] != ctype_space) {
		    if (pr->ptr[1] == '>') {
			pr->ptr++;
			goto eod;
		    }
		    return 1;
		}
	    return 0;		/* still need to scan ahead */
	default:
	    return code;
	case ERRC:
	    ;
    }
    /*
     * Check for EOD.  ERRC implies at least one more character
     * was read; we must unread it, since the caller might have
     * invoked the filter with exactly the right count to read all
     * the available data, and we might be reading past the end.
     */
    if (*pr->ptr != '>') {	/* EOD */
	--(pr->ptr);
	return ERRC;
    }
  eod:if (ss->odd >= 0) {
	if (pw->ptr == pw->limit)
	    return 1;
	*++(pw->ptr) = ss->odd << 4;
    }
    return EOFC;
}

/* Stream template */
const stream_template s_AXD_template =
{&st_AXD_state, s_AXD_init, s_AXD_process, 2, 1
};

/* ------ PSStringEncode ------ */

/* Process a buffer */
private int
s_PSSE_process(stream_state * st, stream_cursor_read * pr,
	       stream_cursor_write * pw, bool last)
{
    const byte *p = pr->ptr;
    const byte *rlimit = pr->limit;
    byte *q = pw->ptr;
    byte *wlimit = pw->limit;
    int status = 0;

    /* This doesn't have to be very efficient. */
    while (p < rlimit) {
	int c = *++p;

	if (c < 32 || c >= 127) {
	    const char *pesc;
	    const char *const esc = "\n\r\t\b\f";

	    if (c < 32 && c != 0 && (pesc = strchr(esc, c)) != 0) {
		if (wlimit - q < 2) {
		    --p;
		    status = 1;
		    break;
		}
		*++q = '\\';
		*++q = "nrtbf"[pesc - esc];
		continue;
	    }
	    if (wlimit - q < 4) {
		--p;
		status = 1;
		break;
	    }
	    q[1] = '\\';
	    q[2] = (c >> 6) + '0';
	    q[3] = ((c >> 3) & 7) + '0';
	    q[4] = (c & 7) + '0';
	    q += 4;
	    continue;
	} else if (c == '(' || c == ')' || c == '\\') {
	    if (wlimit - q < 2) {
		--p;
		status = 1;
		break;
	    }
	    *++q = '\\';
	} else {
	    if (q == wlimit) {
		--p;
		status = 1;
		break;
	    }
	}
	*++q = c;
    }
    if (last && status == 0) {
	if (q == wlimit)
	    status = 1;
	else
	    *++q = ')';
    }
    pr->ptr = p;
    pw->ptr = q;
    return status;
}

/* Stream template */
const stream_template s_PSSE_template =
{&st_stream_state, NULL, s_PSSE_process, 1, 4
};

/* ------ PSStringDecode ------ */

private_st_PSSD_state();

/* Initialize the state */
private int
s_PSSD_init(stream_state * st)
{
    stream_PSSD_state *const ss = (stream_PSSD_state *) st;

    return s_PSSD_init_inline(ss);
}

/* Process a buffer */
private int
s_PSSD_process(stream_state * st, stream_cursor_read * pr,
	       stream_cursor_write * pw, bool last)
{
    stream_PSSD_state *const ss = (stream_PSSD_state *) st;
    const byte *p = pr->ptr;
    const byte *rlimit = pr->limit;
    byte *q = pw->ptr;
    byte *wlimit = pw->limit;
    int status = 0;
    int c;

#define check_p(n)\
  if ( p == rlimit ) { p -= n; goto out; }
#define check_q(n)\
  if ( q == wlimit ) { p -= n; status = 1; goto out; }
    while (p < rlimit) {
	c = *++p;
	if (c == '\\' && !ss->from_string) {
	    check_p(1);
	    switch ((c = *++p)) {
		case 'n':
		    c = '\n';
		    goto put;
		case 'r':
		    c = '\r';
		    goto put;
		case 't':
		    c = '\t';
		    goto put;
		case 'b':
		    c = '\b';
		    goto put;
		case 'f':
		    c = '\f';
		    goto put;
		default:	/* ignore the \ */
		  put:check_q(2);
		    *++q = c;
		    continue;
		case char_CR:	/* ignore, check for following \n */
		    check_p(2);
		    if (p[1] == char_EOL)
			p++;
		    continue;
		case char_EOL:	/* ignore */
		    continue;
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		    {
			int d;

			check_p(2);
			d = p[1];
			c -= '0';
			if (d >= '0' && d <= '7') {
			    if (p + 1 == rlimit) {
				p -= 2;
				goto out;
			    }
			    check_q(2);
			    c = (c << 3) + d - '0';
			    d = p[2];
			    if (d >= '0' && d <= '7') {
				c = (c << 3) + d - '0';
				p += 2;
			    } else
				p++;
			} else
			    check_q(2);
			*++q = c;
			continue;
		    }
	    }
	} else
	    switch (c) {
		case '(':
		    check_q(1);
		    ss->depth++;
		    break;
		case ')':
		    if (ss->depth == 0) {
			status = EOFC;
			goto out;
		    }
		    check_q(1);
		    ss->depth--;
		    break;
		case char_CR:	/* convert to \n */
		    check_p(1);
		    check_q(1);
		    if (p[1] == char_EOL)
			p++;
		    *++q = '\n';
		    continue;
		case char_EOL:
		    c = '\n';
		default:
		    check_q(1);
		    break;
	    }
	*++q = c;
    }
#undef check_p
#undef check_q
  out:pr->ptr = p;
    pw->ptr = q;
    if (last && status == 0 && p != rlimit)
	status = ERRC;
    return status;
}

/* Stream template */
const stream_template s_PSSD_template =
{&st_PSSD_state, s_PSSD_init, s_PSSD_process, 4, 1
};

/* ------ Utilities ------ */

/*
 * Convert hex data to binary.  Return 1 if we filled the string, 0 if
 * we ran out of input data before filling the string, or ERRC on error.
 * The caller must set *odd_digit to -1 before the first call;
 * after each call, if an odd number of hex digits has been read (total),
 * *odd_digit is the odd digit value, otherwise *odd_digit = -1.
 * See strimpl.h for the definition of syntax.
 */
int
s_hex_process(stream_cursor_read * pr, stream_cursor_write * pw,
	      int *odd_digit, hex_syntax syntax)
{
    const byte *p = pr->ptr;
    const byte *rlimit = pr->limit;
    byte *q = pw->ptr;
    byte *wlimit = pw->limit;
    byte *q0 = q;
    byte val1 = (byte) * odd_digit;
    byte val2;
    uint rcount;
    byte *flimit;
    const byte *const decoder = scan_char_decoder;
    int code = 0;

    if (q >= wlimit)
	return 1;
    if (val1 <= 0xf)
	goto d2;
  d1:if ((rcount = (rlimit - p) >> 1) == 0)
	goto x1;
    /* Set up a fast end-of-loop check, so we don't have to test */
    /* both p and q against their respective limits. */
    flimit = (rcount < wlimit - q ? q + rcount : wlimit);
  f1:if ((val1 = decoder[p[1]]) <= 0xf &&
	(val2 = decoder[p[2]]) <= 0xf
	) {
	p += 2;
	*++q = (val1 << 4) + val2;
	if (q < flimit)
	    goto f1;
	if (q >= wlimit)
	    goto px;
    }
  x1:if (p >= rlimit)
	goto end1;
    if ((val1 = decoder[*++p]) > 0xf) {
	if (val1 == ctype_space) {
	    switch (syntax) {
		case hex_ignore_whitespace:
		    goto x1;
		case hex_ignore_leading_whitespace:
		    if (q == q0 && *odd_digit < 0)
			goto x1;
		    --p;
		    code = 1;
		    goto end1;
		case hex_ignore_garbage:
		    goto x1;
	    }
	} else if (syntax == hex_ignore_garbage)
	    goto x1;
	code = ERRC;
	goto end1;
    }
  d2:if (p >= rlimit) {
	*odd_digit = val1;
	goto ended;
    }
    if ((val2 = decoder[*++p]) > 0xf) {
	if (val2 == ctype_space)
	    switch (syntax) {
		case hex_ignore_whitespace:
		    goto d2;
		case hex_ignore_leading_whitespace:
		    if (q == q0)
			goto d2;
		    --p;
		    *odd_digit = val1;
		    code = 1;
		    goto ended;
		case hex_ignore_garbage:	/* pacify compilers */
		    ;
	    }
	if (syntax == hex_ignore_garbage)
	    goto d2;
	*odd_digit = val1;
	code = ERRC;
	goto ended;
    }
    *++q = (val1 << 4) + val2;
    if (q < wlimit)
	goto d1;
  px:code = 1;
  end1:*odd_digit = -1;
  ended:pr->ptr = p;
    pw->ptr = q;
    return code;
}
