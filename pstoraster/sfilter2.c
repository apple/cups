/* Copyright (C) 1991, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: sfilter2.c,v 1.2 2000/03/08 23:15:24 mike Exp $ */
/* Simple Level 2 filters */
#include "stdio_.h"		/* includes std.h */
#include "memory_.h"
#include "strimpl.h"
#include "sa85x.h"
#include "sbtx.h"
#include "scanchar.h"

/* ------ ASCII85Encode ------ */

/* Process a buffer */
#define LINE_LIMIT 65
private int
s_A85E_process(stream_state * st, stream_cursor_read * pr,
	       stream_cursor_write * pw, bool last)
{
    register const byte *p = pr->ptr;
    register byte *q = pw->ptr;
    byte *qn = q + LINE_LIMIT;
    const byte *rlimit = pr->limit;
    byte *wlimit = pw->limit;
    int status = 0;
    int count;

    for (; (count = rlimit - p) >= 4; p += 4) {
	ulong word =
	    ((ulong) (((uint) p[1] << 8) + p[2]) << 16) +
	    (((uint) p[3] << 8) + p[4]);

	if (word == 0) {
	    if (wlimit - q < 2) {
		status = 1;
		break;
	    }
	    *++q = 'z';
	} else {
	    ulong v4 = word / 85;	/* max 85^4 */
	    ulong v3 = v4 / 85;	/* max 85^3 */
	    uint v2 = v3 / 85;	/* max 85^2 */
	    uint v1 = v2 / 85;	/* max 85 */

put:	    if (wlimit - q < 6) {
		status = 1;
		break;
	    }
	    q[1] = (byte) v1 + '!';
	    q[2] = (byte) (v2 - v1 * 85) + '!';
	    q[3] = (byte) ((uint) v3 - v2 * 85) + '!';
	    q[4] = (byte) ((uint) v4 - (uint) v3 * 85) + '!';
	    q[5] = (byte) ((uint) word - (uint) v4 * 85) + '!';
	    /*
	     * Two consecutive '%' characters at the beginning
	     * of the line will confuse some document managers:
	     * insert (an) EOL(s) if necessary to prevent this.
	     */
	    if (q[1] == '%') {
		if (q == pw->ptr) {
		    /*
		     * The very first character written is a %.
		     * Add an EOL before it in case the last
		     * character of the previous batch was a %.
		     */
		    *++q = '\n';
		    qn = q + LINE_LIMIT;
		    goto put;
		}
		if (q[2] == '%' && *q == '\n') {
		    /*
		     * We may have to insert more than one EOL if
		     * there are more than two %s in a row.
		     */
		    int extra =
		    (q[3] != '%' ? 1 : q[4] != '%' ? 2 :
		     q[5] != '%' ? 3 : 4);

		    if (wlimit - q < 6 + extra) {
			status = 1;
			break;
		    }
		    switch (extra) {
			case 4:
			    q[9] = '%', q[8] = '\n';
			    goto e3;
			case 3:
			    q[8] = q[5];
			  e3:q[7] = '%', q[6] = '\n';
			    goto e2;
			case 2:
			    q[7] = q[5], q[6] = q[4];
			  e2:q[5] = '%', q[4] = '\n';
			    goto e1;
			case 1:
			    q[6] = q[5], q[5] = q[4], q[4] = q[3];
			  e1:q[3] = '%', q[2] = '\n';
		    }
		    q += extra;
		    qn = q + (5 + LINE_LIMIT);
		}
	    }
	    q += 5;
	}
	if (q >= qn) {
	    *++q = '\n';
	    qn = q + LINE_LIMIT;
	}
    }
    /* Check for final partial word. */
    if (last && status == 0 && count < 4) {
	if (wlimit - q < (count == 0 ? 2 : count + 3))
	    status = 1;
	else {
	    ulong word = 0;
	    ulong divisor = 85L * 85 * 85 * 85;

	    switch (count) {
		case 3:
		    word += (uint) p[3] << 8;
		case 2:
		    word += (ulong) p[2] << 16;
		case 1:
		    word += (ulong) p[1] << 24;
		    p += count;
		    while (count-- >= 0) {
			ulong v = word / divisor;	/* actually only a byte */

			*++q = (byte) v + '!';
			word -= v * divisor;
			divisor /= 85;
		    }
		    /*case 0: */
	    }
	    *++q = '~';
	    *++q = '>';
	}
    }
    pr->ptr = p;
    pw->ptr = q;
    return status;
}
#undef LINE_LIMIT

/* Stream template */
const stream_template s_A85E_template = {
    &st_stream_state, NULL, s_A85E_process, 4, 6
};

/* ------ ASCII85Decode ------ */

private_st_A85D_state();

/* Initialize the state */
private int
s_A85D_init(stream_state * st)
{
    stream_A85D_state *const ss = (stream_A85D_state *) st;

    return s_A85D_init_inline(ss);
}

/* Process a buffer */
private int a85d_finish(P3(int, ulong, stream_cursor_write *));
private int
s_A85D_process(stream_state * st, stream_cursor_read * pr,
	       stream_cursor_write * pw, bool last)
{
    stream_A85D_state *const ss = (stream_A85D_state *) st;
    register const byte *p = pr->ptr;
    register byte *q = pw->ptr;
    const byte *rlimit = pr->limit;
    byte *wlimit = pw->limit;
    int ccount = ss->odd;
    ulong word = ss->word;
    int status = 0;

    while (p < rlimit) {
	int ch = *++p;
	uint ccode = ch - '!';

	if (ccode < 85) {	/* catches ch < '!' as well */
	    if (wlimit - q < 4) {
		p--;
		status = 1;
		break;
	    }
	    word = word * 85 + ccode;
	    if (++ccount == 5) {
		q[1] = (byte) (word >> 24);
		q[2] = (byte) (word >> 16);
		q[3] = (byte) ((uint) word >> 8);
		q[4] = (byte) word;
		q += 4;
		word = 0;
		ccount = 0;
	    }
	} else if (ch == 'z' && ccount == 0) {
	    if (wlimit - q < 4) {
		p--;
		status = 1;
		break;
	    }
	    q[1] = q[2] = q[3] = q[4] = 0,
		q += 4;
	} else if (scan_char_decoder[ch] == ctype_space)
	    DO_NOTHING;
	else if (ch == '~') {
	    /* Handle odd bytes. */
	    if (p == rlimit) {
		if (last)
		    status = ERRC;
		else
		    p--;
		break;
	    }
	    if ((int)(wlimit - q) < ccount - 1) {
		status = 1;
		p--;
		break;
	    }
	    if (*++p != '>') {
		status = ERRC;
		break;
	    }
	    pw->ptr = q;
	    status = a85d_finish(ccount, word, pw);
	    q = pw->ptr;
	    break;
	} else {		/* syntax error or exception */
	    status = ERRC;
	    break;
	}
    }
    pw->ptr = q;
    if (status == 0 && last) {
	if ((int)(wlimit - q) < ccount - 1)
	    status = 1;
	else
	    status = a85d_finish(ccount, word, pw);
    }
    pr->ptr = p;
    ss->odd = ccount;
    ss->word = word;
    return status;
}
/* Handle the end of input data. */
private int
a85d_finish(int ccount, ulong word, stream_cursor_write * pw)
{
    /* Assume there is enough room in the output buffer! */
    byte *q = pw->ptr;
    int status = EOFC;

    switch (ccount) {
	case 0:
	    break;
	case 1:		/* syntax error */
	    status = ERRC;
	    break;
	case 2:		/* 1 odd byte */
	    word = word * (85L * 85 * 85) + 0xffffffL;
	    goto o1;
	case 3:		/* 2 odd bytes */
	    word = word * (85L * 85) + 0xffffL;
	    goto o2;
	case 4:		/* 3 odd bytes */
	    word = word * 85 + 0xffL;
	    q[3] = (byte) (word >> 8);
o2:	    q[2] = (byte) (word >> 16);
o1:	    q[1] = (byte) (word >> 24);
	    q += ccount - 1;
	    pw->ptr = q;
    }
    return status;
}

/* Stream template */
const stream_template s_A85D_template = {
    &st_A85D_state, s_A85D_init, s_A85D_process, 2, 4
};

/* ------ ByteTranslateEncode/Decode ------ */

private_st_BT_state();

/* Process a buffer.  Note that the same code serves for both streams. */
private int
s_BT_process(stream_state * st, stream_cursor_read * pr,
	     stream_cursor_write * pw, bool last)
{
    stream_BT_state *const ss = (stream_BT_state *) st;
    const byte *p = pr->ptr;
    byte *q = pw->ptr;
    uint rcount = pr->limit - p;
    uint wcount = pw->limit - q;
    uint count;
    int status;

    if (rcount <= wcount)
	count = rcount, status = 0;
    else
	count = wcount, status = 1;
    while (count--)
	*++q = ss->table[*++p];
    pr->ptr = p;
    pw->ptr = q;
    return status;
}

/* Stream template */
const stream_template s_BTE_template = {
    &st_BT_state, NULL, s_BT_process, 1, 1
};
const stream_template s_BTD_template = {
    &st_BT_state, NULL, s_BT_process, 1, 1
};
