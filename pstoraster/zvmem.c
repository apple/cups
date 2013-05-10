/* Copyright (C) 1989, 1995 Aladdin Enterprises.  All rights reserved.
  
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

/* zvmem.c */
/* "Virtual memory" operators */
#include "ghost.h"
#include "gsstruct.h"
#include "errors.h"
#include "oper.h"
#include "estack.h"			/* for checking in restore */
#include "ialloc.h"
#include "idict.h"			/* ditto */
#include "igstate.h"
#include "isave.h"
#include "dstack.h"
#include "store.h"
#include "gsmatrix.h"			/* for gsstate.h */
#include "gsstate.h"

/* Make an invalid file object. */
extern void make_invalid_file(P1(ref *)); /* in zfile.c */

/* 'Save' structure */
typedef struct vm_save_s vm_save_t;
struct vm_save_s {
	gs_state *gsave;		/* old graphics state */
};
gs_private_st_ptrs1(st_vm_save, vm_save_t, "savetype",
  vm_save_enum_ptrs, vm_save_reloc_ptrs, gsave);

/* - save <save> */
int
zsave(register os_ptr op)
{	uint space = icurrent_space;
	vm_save_t *vmsave;
	ulong sid;
	int code;
	gs_state *prev;

	/*ivalidate_spaces();*/		/****** DOESN'T WORK ******/
	ialloc_set_space(idmemory, avm_local);
	vmsave = ialloc_struct(vm_save_t, &st_vm_save, "zsave");
	ialloc_set_space(idmemory, space);
	if ( vmsave == 0 )
	  return_error(e_VMerror);
	sid = alloc_save_state(idmemory, vmsave);
	if ( sid == 0 )
	{	ifree_object(vmsave, "zsave");
		return_error(e_VMerror);
	}
	if_debug2('u', "[u]vmsave 0x%lx, id = %lu\n",
		  (ulong)vmsave, (ulong)sid);
	code = zgsave(op);
	if ( code < 0 )
	  return code;
	/* Cut the chain so we can't grestore past here. */
	prev = gs_state_swap_saved(igs, (gs_state *)0);
	code = zgsave(op);
	if ( code < 0 )
	  return code;
	vmsave->gsave = prev;
	push(1);
	make_tav(op, t_save, 0, saveid, sid);
	/*ivalidate_spaces();*/		/****** DOESN'T WORK ******/
	return 0;
}

/* <save> restore - */
private int restore_check_operand(P2(os_ptr, alloc_save_t **));
private int restore_check_stack(P3(const ref_stack *, const alloc_save_t *, bool));
private void restore_fix_stack(P3(ref_stack *, const alloc_save_t *, bool));
int
zrestore(register os_ptr op)
{	alloc_save_t *asave;
	bool last;
	vm_save_t *vmsave;
	int code = restore_check_operand(op, &asave);

	if ( code < 0 )
	  return code;
	/*ivalidate_spaces();*/		/****** DOESN'T WORK ******/
	if_debug2('u', "[u]vmrestore 0x%lx, id = %lu\n",
		  (ulong)alloc_save_client_data(asave),
		  (ulong)op->value.saveid);
	/* Check the contents of the stacks. */
	osp--;
	{	int code;
		if ( (code = restore_check_stack(&o_stack, asave, false)) < 0 ||
		     (code = restore_check_stack(&e_stack, asave, true)) < 0 ||
		     (code = restore_check_stack(&d_stack, asave, false)) < 0
		   )
		  {	osp++;
			return code;
		  }
	}
	/* Reset l_new in all stack entries if the new save level is zero. */
	/* Also do some special fixing on the e-stack. */
	restore_fix_stack(&o_stack, asave, false);
	restore_fix_stack(&e_stack, asave, true);
	restore_fix_stack(&d_stack, asave, false);
	/* Iteratively restore the state of memory, */
	/* also doing a grestoreall at each step. */
	do
	  {	vmsave = alloc_save_client_data(alloc_save_current(idmemory));
		/* Restore the graphics state. */
		gs_grestoreall(igs);
		gs_state_swap_saved(gs_state_saved(igs), vmsave->gsave);
		gs_grestore(igs);
		gs_grestore(igs);
		/*
		 * If alloc_save_space decided to do a second save, the vmsave
		 * object was allocated one save level less deep than the
		 * current level, so ifree_object won't actually free it;
		 * however, it points to a gsave object that definitely
		 * *has* been freed.  In order not to trip up the garbage
		 * collector, we clear the gsave pointer now.
		 */
		vmsave->gsave = 0;
		/* Now it's safe to restore the state of memory. */
		last = alloc_restore_state_step(asave);
	  }
	while ( !last );
	{ uint space = icurrent_space;
	  ialloc_set_space(idmemory, avm_local);
	  ifree_object(vmsave, "zrestore");
	  ialloc_set_space(idmemory, space);
	}
	dict_set_top();		/* reload dict stack cache */
	/*ivalidate_spaces();*/		/****** DOESN'T WORK ******/
	return 0;
}
/* Check the operand of a restore. */
private int
restore_check_operand(os_ptr op, alloc_save_t **pasave)
{	vm_save_t *vmsave;
	ulong sid;
	alloc_save_t *asave;
	check_type(*op, t_save);
	vmsave = r_ptr(op, vm_save_t);
	if ( vmsave == 0 )		/* invalidated save */
	  return_error(e_invalidrestore);
	sid = op->value.saveid;
	asave = alloc_find_save(idmemory, sid);
	if ( asave == 0 )
	  return_error(e_invalidrestore);
	*pasave = asave;
	return 0;
}
/* Check a stack to make sure all its elements are older than a save. */
private int
restore_check_stack(const ref_stack *pstack, const alloc_save_t *asave,
  bool is_estack)
{	STACK_LOOP_BEGIN(pstack, bot, size)
	  {	const ref *stkp;
		for ( stkp = bot; size; stkp++, size-- )
		  {	const void *ptr;
			switch ( r_type(stkp) )
			  {
			  case t_array:
			    ptr = stkp->value.refs; break;
			  case t_dictionary:
			    ptr = stkp->value.pdict; break;
			  case t_file:
			    /* Don't check executable files on the e-stack. */
			    if ( r_has_attr(stkp, a_executable) && is_estack )
			      continue;
			    ptr = stkp->value.pfile; break;
			  case t_name:
			    /* Names are special because of how they are allocated. */
			    if ( alloc_name_is_since_save(stkp, asave) )
			      return_error(e_invalidrestore);
			    continue;
			  case t_string:
			    /* Don't check empty executable strings */
			    /* on the e-stack. */
			    if ( r_size(stkp) == 0 &&
				 r_has_attr(stkp, a_executable) && is_estack
			       )
			      continue;
			    ptr = stkp->value.bytes; break;
			  case t_mixedarray:
			  case t_shortarray:
			    ptr = stkp->value.packed; break;
			  case t_device:
			    ptr = stkp->value.pdevice; break;
			  case t_fontID:
			  case t_struct:
			  case t_astruct:
			    ptr = stkp->value.pstruct; break;
			  default:
			    continue;
			  }
			if ( alloc_is_since_save(ptr, asave) )
			  return_error(e_invalidrestore);
		  }
	  }
	STACK_LOOP_END(bot, size)
	return 0;			/* OK */
}
/*
 * If the new save level is zero, fix up the contents of a stack
 * by clearing the l_new bit in all the entries (since we can't tolerate
 * values with l_new set if the save level is zero).
 * Also, in any case, fix up the e-stack by replacing empty executable
 * strings and closed executable files that are newer than the save
 * with canonical ones that aren't.
 *
 * Note that this procedure is only called if restore_check_stack succeeded.
 */
private void
restore_fix_stack(ref_stack *pstack, const alloc_save_t *asave,
  bool is_estack)
{	STACK_LOOP_BEGIN(pstack, bot, size)
	  {	ref *stkp;
		for ( stkp = bot; size; stkp++, size-- )
		  {	r_clear_attrs(stkp, l_new); /* always do it, no harm */
			if ( is_estack )
			  {	ref ofile;
				ref_assign(&ofile, stkp);
				switch ( r_type(stkp) )
				  {
				  case t_string:
				    if ( r_size(stkp) == 0 &&
					alloc_is_since_save(stkp->value.bytes,
							    asave)
				       )
				      {	make_empty_const_string(stkp,
								avm_foreign);
					break;
				      }
				    continue;
				  case t_file:
				    if ( alloc_is_since_save(stkp->value.pfile,
							     asave)
				       )
				      {	make_invalid_file(stkp);
					break;
				      }
				    continue;
				  default:
				    continue;
				  }
				r_copy_attrs(stkp, a_all | a_executable,
					     &ofile);
			  }
		  }
	  }
	STACK_LOOP_END(bot, size)
}

/* - vmstatus <save_level> <vm_used> <vm_maximum> */
private int
zvmstatus(register os_ptr op)
{	gs_memory_status_t mstat, dstat;
	gs_memory_status(imemory, &mstat);
	if ( imemory == imemory_global )
	  {	gs_memory_status_t sstat;
		gs_memory_status(imemory_system, &sstat);
		mstat.allocated += sstat.allocated;
		mstat.used += sstat.used;
	  }
	gs_memory_status(&gs_memory_default, &dstat);
	push(3);
	make_int(op - 2, alloc_save_level(idmemory));
	make_int(op - 1, mstat.used);
	make_int(op, mstat.allocated + dstat.allocated - dstat.used);
	return 0;
}

/* ------ Non-standard extensions ------ */

/* <save> .forgetsave - */
private int
zforgetsave(register os_ptr op)
{	alloc_save_t *asave;
	vm_save_t *vmsave;
	int code = restore_check_operand(op, &asave);
	if ( code < 0 )
	  return 0;
	vmsave = alloc_save_client_data(asave);
	/* Reset l_new in all stack entries if the new save level is zero. */
	restore_fix_stack(&o_stack, asave, false);
	restore_fix_stack(&e_stack, asave, false);
	restore_fix_stack(&d_stack, asave, false);
	/* Forget the gsaves, by deleting the bottom gstate on */
	/* the current stack and the top one on the saved stack and then */
	/* concatenating the stacks together. */
	  {	gs_state *pgs = igs;
		gs_state *last;
		while ( gs_state_saved(last = gs_state_saved(pgs)) != 0 )
		  pgs = last;
		gs_state_swap_saved(last, vmsave->gsave);
		gs_grestore(last);
		gs_grestore(last);
	  }
	/* Forget the save in the memory manager. */
	alloc_forget_save(asave);
	{ uint space = icurrent_space;
	  ialloc_set_space(idmemory, avm_local);
	/* See above for why we clear the gsave pointer here. */
	  vmsave->gsave = 0;
	  ifree_object(vmsave, "zrestore");
	  ialloc_set_space(idmemory, space);
	}
	pop(1);
	return 0;
}

/* ------ Initialization procedure ------ */

BEGIN_OP_DEFS(zvmem_op_defs) {
	{"1.forgetsave", zforgetsave},
	{"1restore", zrestore},
	{"0save", zsave},
	{"0vmstatus", zvmstatus},
END_OP_DEFS(0) }
