/* Copyright (C) 1993, 1995, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: zdevice2.c,v 1.2 2000/03/08 23:15:34 mike Exp $ */
/* Level 2 device operators */
#include "math_.h"
#include "memory_.h"
#include "ghost.h"
#include "oper.h"
#include "dstack.h"		/* for dict_find_name */
#include "estack.h"
#include "idict.h"
#include "idparam.h"
#include "igstate.h"
#include "iname.h"
#include "store.h"
#include "gxdevice.h"
#include "gsstate.h"

/* Forward references */
private int z2copy_gstate(P1(os_ptr));
private int push_callout(P1(const char *));

/* Extend the `copy' operator to deal with gstates. */
/* This is done with a hack -- we know that gstates are the only */
/* t_astruct subtype that implements copy. */
private int
z2copy(register os_ptr op)
{
    int code = zcopy(op);

    if (code >= 0)
	return code;
    if (!r_has_type(op, t_astruct))
	return code;
    return z2copy_gstate(op);
}

/* - .currentshowpagecount <count> true */
/* - .currentshowpagecount false */
private int
zcurrentshowpagecount(register os_ptr op)
{
    gx_device *dev = gs_currentdevice(igs);

    if ((*dev_proc(dev, get_page_device))(dev) == 0) {
	push(1);
	make_false(op);
    } else {
	push(2);
	make_int(op - 1, dev->ShowpageCount);
	make_true(op);
    }
    return 0;
}

/* - .currentpagedevice <dict> <bool> */
private int
zcurrentpagedevice(register os_ptr op)
{
    gx_device *dev = gs_currentdevice(igs);

    push(2);
    if ((*dev_proc(dev, get_page_device))(dev) != 0) {
	op[-1] = istate->pagedevice;
	make_true(op);
    } else {
	make_null(op - 1);
	make_false(op);
    }
    return 0;
}

/* <local_dict|null> .setpagedevice - */
private int
zsetpagedevice(register os_ptr op)
{
    int code;

/******
    if ( igs->in_cachedevice )
	return_error(e_undefined);
 ******/
    if (r_has_type(op, t_dictionary)) {
	check_dict_read(*op);
#if 0	/****************/
	/*
	 * In order to avoid invalidaccess errors on setpagedevice,
	 * the dictionary must be allocated in local VM.
	 */
	if (!(r_is_local(op)))
	    return_error(e_invalidaccess);
#endif	/****************/
	/* Make the dictionary read-only. */
	code = zreadonly(op);
	if (code < 0)
	    return code;
    } else {
	check_type(*op, t_null);
    }
    istate->pagedevice = *op;
    pop(1);
    return 0;
}

/* Default Install/BeginPage/EndPage procedures */
/* that just call the procedure in the device. */

/* - .callinstall - */
private int
zcallinstall(os_ptr op)
{
    gx_device *dev = gs_currentdevice(igs);

    if ((dev = (*dev_proc(dev, get_page_device))(dev)) != 0) {
	int code = (*dev->page_procs.install) (dev, igs);

	if (code < 0)
	    return code;
    }
    return 0;
}

/* <showpage_count> .callbeginpage - */
private int
zcallbeginpage(os_ptr op)
{
    gx_device *dev = gs_currentdevice(igs);

    check_type(*op, t_integer);
    if ((dev = (*dev_proc(dev, get_page_device))(dev)) != 0) {
	int code = (*dev->page_procs.begin_page)(dev, igs);

	if (code < 0)
	    return code;
    }
    pop(1);
    return 0;
}

/* <showpage_count> <reason_int> .callendpage <flush_bool> */
private int
zcallendpage(os_ptr op)
{
    gx_device *dev = gs_currentdevice(igs);
    int code;

    check_type(op[-1], t_integer);
    check_type(*op, t_integer);
    if ((dev = (*dev_proc(dev, get_page_device))(dev)) != 0) {
	code = (*dev->page_procs.end_page)(dev, (int)op->value.intval, igs);
	if (code < 0)
	    return code;
	if (code > 1)
	    return_error(e_rangecheck);
    } else {
	code = (op->value.intval == 2 ? 0 : 1);
    }
    make_bool(op - 1, code);
    pop(1);
    return 0;
}

/* ------ Wrappers for operators that save the graphics state. ------ */

/* When saving the state with the current device a page device, */
/* we need to make sure that the page device dictionary exists */
/* so that grestore can use it to reset the device parameters. */
/* This may have significant performance consequences, but we don't see */
/* any way around it. */

/* Check whether we need to call out to create the page device dictionary. */
private bool
save_page_device(gs_state *pgs)
{
    return 
	(r_has_type(&gs_int_gstate(pgs)->pagedevice, t_null) &&
	 (*dev_proc(gs_currentdevice(pgs), get_page_device))(gs_currentdevice(pgs)) != 0);
}

/* - gsave - */
private int
z2gsave(os_ptr op)
{
    if (!save_page_device(igs))
	return gs_gsave(igs);
    return push_callout("%gsavepagedevice");
}

/* - save - */
private int
z2save(os_ptr op)
{
    if (!save_page_device(igs))
	return zsave(op);
    return push_callout("%savepagedevice");
}

/* - gstate <gstate> */
private int
z2gstate(os_ptr op)
{
    if (!save_page_device(igs))
	return zgstate(op);
    return push_callout("%gstatepagedevice");
}

/* <gstate1> <gstate2> copy <gstate2> */
private int
z2copy_gstate(os_ptr op)
{
    if (!save_page_device(igs))
	return zcopy_gstate(op);
    return push_callout("%copygstatepagedevice");
}

/* <gstate> currentgstate <gstate> */
private int
z2currentgstate(os_ptr op)
{
    if (!save_page_device(igs))
	return zcurrentgstate(op);
    return push_callout("%currentgstatepagedevice");
}

/* ------ Wrappers for operators that reset the graphics state. ------ */

/* Check whether we need to call out to restore the page device. */
private bool
restore_page_device(const gs_state * pgs_old, const gs_state * pgs_new)
{
    gx_device *dev_old = gs_currentdevice(pgs_old);
    gx_device *dev_new;
    gx_device *dev_t1;
    gx_device *dev_t2;

    if ((dev_t1 = (*dev_proc(dev_old, get_page_device)) (dev_old)) == 0)
	return false;
    dev_new = gs_currentdevice(pgs_new);
    if (dev_old != dev_new) {
	if ((dev_t2 = (*dev_proc(dev_new, get_page_device)) (dev_new)) == 0)
	    return false;
	if (dev_t1 != dev_t2)
	    return true;
    }
    /* The current implementation of setpagedevice just sets new */
    /* parameters in the same device object, so we have to check */
    /* whether the page device dictionaries are the same. */
    {
	const ref *ppd1 = &gs_int_gstate(pgs_old)->pagedevice;
	const ref *ppd2 = &gs_int_gstate(pgs_new)->pagedevice;

	return (r_type(ppd1) != r_type(ppd2) ||
		(r_has_type(ppd1, t_dictionary) &&
		 ppd1->value.pdict != ppd2->value.pdict));
    }
}

/* - grestore - */
private int
z2grestore(os_ptr op)
{
    if (!restore_page_device(igs, gs_state_saved(igs)))
	return gs_grestore(igs);
    return push_callout("%grestorepagedevice");
}

/* - grestoreall - */
private int
z2grestoreall(os_ptr op)
{
    for (;;) {
	if (!restore_page_device(igs, gs_state_saved(igs))) {
	    bool done = !gs_state_saved(gs_state_saved(igs));

	    gs_grestore(igs);
	    if (done)
		break;
	} else
	    return push_callout("%grestoreallpagedevice");
    }
    return 0;
}

/* <save> restore - */
private int
z2restore(os_ptr op)
{
    for (;;) {
	if (!restore_page_device(igs, gs_state_saved(igs))) {
	    zgrestore(op);
	    if (!gs_state_saved(gs_state_saved(igs)))
		break;
	} else
	    return push_callout("%restorepagedevice");
    }
    return zrestore(op);
}

/* <gstate> setgstate - */
private int
z2setgstate(os_ptr op)
{
    check_stype(*op, st_igstate_obj);
    if (!restore_page_device(igs, igstate_ptr(op)))
	return zsetgstate(op);
    return push_callout("%setgstatepagedevice");
}

/* ------ Initialization procedure ------ */

const op_def zdevice2_l2_op_defs[] =
{
    op_def_begin_level2(),
    {"0.currentshowpagecount", zcurrentshowpagecount},
    {"0.currentpagedevice", zcurrentpagedevice},
    {"1.setpagedevice", zsetpagedevice},
		/* Note that the following replace prior definitions */
		/* in the indicated files: */
    {"1copy", z2copy},		/* zdps1.c */
    {"0gsave", z2gsave},	/* zgstate.c */
    {"0save", z2save},		/* zvmem.c */
    {"0gstate", z2gstate},	/* zdps1.c */
    {"1currentgstate", z2currentgstate},	/* zdps1.c */
    {"0grestore", z2grestore},	/* zgstate.c */
    {"0grestoreall", z2grestoreall},	/* zgstate.c */
    {"1restore", z2restore},	/* zvmem.c */
    {"1setgstate", z2setgstate},	/* zdps1.c */
		/* Default Install/BeginPage/EndPage procedures */
		/* that just call the procedure in the device. */
    {"0.callinstall", zcallinstall},
    {"1.callbeginpage", zcallbeginpage},
    {"2.callendpage", zcallendpage},
    op_def_end(0)
};

/* ------ Internal routines ------ */

/* Call out to a PostScript procedure. */
private int
push_callout(const char *callout_name)
{
    int code;

    check_estack(1);
    code = name_enter_string(callout_name, esp + 1);
    if (code < 0)
	return code;
    ++esp;
    r_set_attrs(esp, a_executable);
    return o_push_estack;
}
