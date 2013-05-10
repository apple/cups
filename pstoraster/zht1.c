/* Copyright (C) 1994 Aladdin Enterprises.  All rights reserved.
  
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

/* zht1.c */
/* setcolorscreen operator */
#include "ghost.h"
#include "memory_.h"
#include "errors.h"
#include "oper.h"
#include "estack.h"
#include "gsstruct.h"			/* must precede igstate.h, */
					/* because of #ifdef in gsht.h */
#include "ialloc.h"
#include "igstate.h"
#include "gsmatrix.h"
#include "gxdevice.h"			/* for gzht.h */
#include "gzht.h"
#include "gsstate.h"
#include "store.h"

/* Imported from zht.c */
int zscreen_params(P2(os_ptr, gs_screen_halftone *));
int zscreen_enum_init(P6(os_ptr, const gx_ht_order *, gs_screen_halftone *,
			 ref *, int, int (*)(P1(os_ptr))));

/* Dummy spot function */
float
spot_dummy(floatp x, floatp y)
{	return (x + y) / 2;
}

/* <red_freq> ... <gray_proc> setcolorscreen - */
private int setcolorscreen_finish(P1(os_ptr));
private int setcolorscreen_cleanup(P1(os_ptr));
private int
zsetcolorscreen(register os_ptr op)
{	gs_colorscreen_halftone cscreen;
	ref sprocs[4];
	gs_halftone *pht;
	gx_device_halftone *pdht;
	int i;
	int code = 0;
	for ( i = 0; i < 4; i++ )
	{	os_ptr op1 = op - 9 + i * 3;
		int code = zscreen_params(op1, &cscreen.screens.indexed[i]);
		if ( code < 0 )
			return code;
		cscreen.screens.indexed[i].spot_function = spot_dummy;
		sprocs[i] = *op1;
	}
	check_estack(8);		/* for sampling screens */
	pht = ialloc_struct(gs_halftone, &st_halftone, "setcolorscreen");
	pdht = ialloc_struct(gx_device_halftone, &st_device_halftone,
			     "setcolorscreen");
	if ( pht == 0 || pdht == 0 )
		code = gs_note_error(e_VMerror);
	else
	  {	pht->type = ht_type_colorscreen;
		pht->params.colorscreen = cscreen;
		code = gs_sethalftone_prepare(igs, pht, pdht);
	  }
	if ( code >= 0 )
	  {	/* Schedule the sampling of the screens. */
		es_ptr esp0 = esp;		/* for backing out */
		esp += 8;
		make_mark_estack(esp - 7, es_other, setcolorscreen_cleanup);
		memcpy(esp - 6, sprocs, sizeof(ref) * 4);	/* procs */
		make_istruct(esp - 2, 0, pht);
		make_istruct(esp - 1, 0, pdht);
		make_op_estack(esp, setcolorscreen_finish);
		for ( i = 0; i < 4; i++ )
		{	/* Shuffle the indices to correspond to */
			/* the component order. */
			code = zscreen_enum_init(op,
				&pdht->components[(i + 1) & 3].corder,
				&pht->params.colorscreen.screens.indexed[i],
				&sprocs[i], 0, 0);
			if ( code < 0 )
			{	esp = esp0;
				break;
			}
		}
	}
	if ( code < 0 )
	  {	ifree_object(pdht, "setcolorscreen");
		ifree_object(pht, "setcolorscreen");
		return code;
	  }
	pop(12);
	return o_push_estack;
}
/* Install the color screen after sampling. */
private int
setcolorscreen_finish(os_ptr op)
{	gx_device_halftone *pdht = r_ptr(esp, gx_device_halftone);
	int code;
	pdht->order = pdht->components[0].corder;
	code = gx_ht_install(igs, r_ptr(esp - 1, gs_halftone), pdht);
	if ( code < 0 )
		return code;
	memcpy(istate->screen_procs.indexed, esp - 5, sizeof(ref) * 4);
	make_null(&istate->halftone);
	esp -= 7;
	setcolorscreen_cleanup(op);
	return o_pop_estack;
}
/* Clean up after installing the color screen. */
private int
setcolorscreen_cleanup(os_ptr op)
{	ifree_object(esp[7].value.pstruct,
		     "setcolorscreen_cleanup(device halftone)");
	ifree_object(esp[6].value.pstruct,
		     "setcolorscreen_cleanup(halftone)");
	return 0;
}

/* ------ Initialization procedure ------ */

BEGIN_OP_DEFS(zht1_op_defs) {
	{"<setcolorscreen", zsetcolorscreen},
		/* Internal operators */
	{"0%setcolorscreen_finish", setcolorscreen_finish},
END_OP_DEFS(0) }
