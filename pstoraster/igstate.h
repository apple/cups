/* Copyright (C) 1989, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* igstate.h */
/* Ghostscript interpreter graphics state definition */
#include "gsstate.h"
#include "gxstate.h"			/* for 'client data' access */
#include "istruct.h"			/* for gstate obj definition */

/*
 * From the interpreter's point of view, the graphics state is largely opaque,
 * i.e., the interpreter is just another client of the library.
 * The interpreter does require additional items in the graphics state;
 * these are "client data" from the library's point of view.
 * Most of the complexity in this added state comes from
 * the parameters associated with the various Level 2 color spaces.
 * Note that the added information consists entirely of refs.
 */

/*
 * The interpreter represents graphics state objects in a slightly
 * unnatural way, namely, by a t_astruct ref that points to an object
 * of type st_igstate_obj, which is essentially a t_struct ref that in turn
 * points to a real graphics state (object of type st_gs_state).
 * We do this so that save and restore can manipulate the intermediate
 * object and not have to worry about copying entire gs_states.
 *
 * Because a number of different operators must test whether an object
 * is a gstate, we make an exception to our convention of declaring
 * structure descriptors only in the place where the structure itself
 * is defined (see gsstruct.h for more information on this).
 */
typedef struct igstate_obj_s {
	ref gstate;		/* t_struct / st_gs_state */
} igstate_obj;
extern_st(st_igstate_obj);
#define public_st_igstate_obj()	/* in zdps1.c */\
  gs_public_st_ref_struct(st_igstate_obj, igstate_obj, "gstatetype")
#define igstate_ptr(rp) r_ptr(&r_ptr(rp, igstate_obj)->gstate, gs_state)

/* CIE transformation procedures */
typedef struct ref_cie_procs_s {
	union {
		ref DEFG;
		ref DEF;
	} PreDecode;
	union {
		ref ABC;
		ref A;
	} Decode;
	ref DecodeLMN;
} ref_cie_procs;
/* CIE rendering transformation procedures */
typedef struct ref_cie_render_procs_s {
	ref TransformPQR, EncodeLMN, EncodeABC, RenderTableT;
} ref_cie_render_procs;

/* Separation name and tint transform */
typedef struct ref_separation_params_s {
	ref layer_name, tint_transform;
} ref_separation_params;

/* All color space parameters. */
/* All of these are optional. */
/* Note that they may actually be the parameters for an underlying or */
/* alternate space for a special space. */
typedef struct ref_color_procs_s {
	ref_cie_procs cie;
	union {
		ref_separation_params separation;
		ref index_proc;
	} special;
} ref_color_procs;
typedef struct ref_colorspace_s {
	ref array;		/* color space (array), */
		/* only relevant if the current */
		/* color space has parameters associated with it. */
	ref_color_procs procs;	/* associated procedures/parameters, */
		/* only relevant for CIE, Separation, Indexed/CIE, */
		/* Indexed with procedure, or a Pattern with one of these. */
} ref_colorspace;

typedef struct int_gstate_s {
	ref dash_pattern;		/* (array) */
		/* Screen_procs are only relevant if setscreen was */
		/* executed more recently than sethalftone */
		/* (for this graphics context). */
	union {
		ref indexed[4];
		struct {
			/* The components must be in this order: */
			ref red, green, blue, gray;
		} colored;
	} screen_procs,			/* halftone screen procedures */
	  transfer_procs;		/* transfer procedures */
	ref black_generation;		/* (procedure) */
	ref undercolor_removal;		/* (procedure) */
	ref_colorspace colorspace;
		/* Pattern is only relevant if the current color space */
		/* is a pattern space. */
	ref pattern;			/* pattern (dictionary) */
	struct {
		ref dict;		/* CIE color rendering dictionary */
		ref_cie_render_procs procs;	/* (see above) */
	} colorrendering;
		/* Halftone is only relevant if sethalftone was executed */
		/* more recently than setscreen for this graphics context. */
		/* setscreen sets it to null. */
	ref halftone;			/* halftone (dictionary) */
		/* Pagedevice is only relevant if setpagedevice was */
		/* executed more recently than nulldevice, setcachedevice, */
		/* or setdevice with a non-page device (for this */
		/* graphics context).  If the current device is not a */
		/* page device, pagedevice is an empty dictionary. */
	ref pagedevice;			/* page device (dictionary) */
} int_gstate;
extern ref i_null_pagedevice;
#define clear_pagedevice(pigs) ((pigs)->pagedevice = i_null_pagedevice)
/*
 * Even though the interpreter's part of the graphics state actually
 * consists of refs, allocating it as refs tends to create sandbars;
 * since it is always allocated and freed as a unit, we can treat it
 * as an ordinary structure.
 */
#define private_st_int_gstate()	/* in zgstate.c */\
  gs_private_st_ref_struct(st_int_gstate, int_gstate, "int_gstate")

/* Enumerate the refs in an int_gstate. */
/* Since all the elements of an int_gstate are refs, this is simple. */
#define int_gstate_map_refs(p,m)\
 { register ref *rp_ = (ref *)(p);\
   register int i = sizeof(int_gstate) / sizeof(ref);\
   do { m(rp_); ++rp_; } while ( --i );\
 }

/* Get the int_gstate from a gs_state. */
#define gs_int_gstate(pgs) ((int_gstate *)gs_state_client_data(pgs))

/* The current instances. */
extern gs_state *igs;
#define istate gs_int_gstate(igs)
