/* Copyright (C) 1993, 1995 Aladdin Enterprises.  All rights reserved.
  
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

/* sfilter.h */
/* Definitions for simple Ghostscript streams */
/* Requires scommon.h; should require strimpl.h only if any templates */
/* are referenced, but some compilers always require strimpl.h. */

#include "gstypes.h"			/* for gs_[const_]string */

/*
 * Define the processing states of the simplest Ghostscript streams.
 * We use abbreviations for the stream names so as not to exceed the
 * 31-character limit that some compilers put on identifiers.
 *
 * The processing state of a stream has three logical sections:
 * parameters set by the client before the stream is opened,
 * values computed from the parameters at initialization time,
 * and values that change dynamically.  Unless otherwise indicated,
 * all structure members change dynamically.
 */

/* (T)BCPEncode */
/* (no state) */
extern const stream_template s_BCPE_template;
extern const stream_template s_TBCPE_template;

/* (T)BCPDecode */
typedef struct stream_BCPD_state_s {
	stream_state_common;
		/* The client sets the following before initialization. */
	int (*signal_interrupt)(P1(stream_state *));
	int (*request_status)(P1(stream_state *));
		/* The following are updated dynamically. */
	bool escaped;
	int matched;			/* TBCP only */
	int copy_count;			/* TBCP only */
	const byte _ds *copy_ptr;	/* TBCP only */
} stream_BCPD_state;
#define private_st_BCPD_state()	/* in sbcp.c */\
  gs_private_st_simple(st_BCPD_state, stream_BCPD_state, "(T)BCPDecode state")
extern const stream_template s_BCPD_template;
extern const stream_template s_TBCPD_template;

/* eexecEncode */
typedef struct stream_exE_state_s {
	stream_state_common;
		/* The following parameters are set by the client. */
	ushort cstate;		/* encryption state */
} stream_exE_state;
#define private_st_exE_state()	/* in sfilter1.c */\
  gs_private_st_simple(st_exE_state, stream_exE_state, "eexecEncode state")
extern const stream_template s_exE_template;

/* eexecDecode */
typedef struct stream_PFBD_state_s stream_PFBD_state;
typedef struct stream_exD_state_s {
	stream_state_common;
		/* The following parameters are set by the client. */
	ushort cstate;		/* encryption state */
	stream_PFBD_state *pfb_state;	/* state of underlying */
				/* PFBDecode stream, if any */
	int binary;		/* 1=binary, 0=hex, -1=don't know yet */
		/* The following change dynamically. */
	int odd;		/* odd digit */
	long record_left;	/* data left in binary record in .PFB file, */
				/* max_long if not reading a .PFB file */
	int skip;		/* # of decoded bytes to skip */
} stream_exD_state;
#define private_st_exD_state()	/* in seexec.c */\
  gs_private_st_ptrs1(st_exD_state, stream_exD_state, "eexecDecode state",\
    exd_enum_ptrs, exd_reloc_ptrs, pfb_state)
extern const stream_template s_exD_template;

/* NullEncode/Decode */
/* (no state) */
extern const stream_template s_Null_template;
#define s_NullE_template s_Null_template
#define s_NullD_template s_Null_template

/* PFBDecode */
/* The typedef for the state appears under eexecDecode above. */
/*typedef*/ struct stream_PFBD_state_s {
	stream_state_common;
		/* The following parameters are set by the client. */
	int binary_to_hex;
		/* The following change dynamically. */
	int record_type;
	ulong record_left;		/* bytes left in current record */
} /*stream_PFBD_state*/;
#define private_st_PFBD_state()	/* in sfilter1.c */\
  gs_private_st_simple(st_PFBD_state, stream_PFBD_state, "PFBDecode state")
extern const stream_template s_PFBD_template;

/* SubFileDecode */
typedef struct stream_SFD_state_s {
	stream_state_common;
		/* The following parameters are set by the client. */
	long count;		/* # of EODs to scan over */
	gs_const_string eod;
		/* The following change dynamically. */
	uint match;		/* # of matched chars not copied to output */
	uint copy_count;	/* # of matched characters left to copy */
	uint copy_ptr;		/* index of next character to copy */
} stream_SFD_state;
#define private_st_SFD_state()	/* in sfilter1.c */\
  gs_private_st_composite(st_SFD_state, stream_SFD_state,\
    "SubFileDecode state", sfd_enum_ptrs, sfd_reloc_ptrs)
extern const stream_template s_SFD_template;
