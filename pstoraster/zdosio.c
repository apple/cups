/* Copyright (C) 1992 Aladdin Enterprises.  All rights reserved.
  
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

/* zdosio.c */
/* MS-DOS direct I/O operators. */
/* These should NEVER be included in a released configuration! */
#include "dos_.h"
#include "ghost.h"
#include "errors.h"
#include "oper.h"
#include "store.h"

/* <port> .inport <word> */
private int
zinport(register os_ptr op)
{	check_type(*op, t_integer);
	make_int(op, inport((int)op->value.intval));
	return 0;
}

/* <port> .inportb <byte> */
private int
zinportb(register os_ptr op)
{	check_type(*op, t_integer);
	make_int(op, inportb((int)op->value.intval));
	return 0;
}

/* <port> <word> .outport - */
private int
zoutport(register os_ptr op)
{	check_type(*op, t_integer);
	check_type(op[-1], t_integer);
	outport((int)op[-1].value.intval, (int)op->value.intval);
	pop(1);
	return 0;
}

/* <port> <byte> .outportb - */
private int
zoutportb(register os_ptr op)
{	check_type(*op, t_integer);
	check_int_leu(op[-1], 0xff);
	outportb((int)op[-1].value.intval, (byte)op->value.intval);
	pop(1);
	return 0;
}

/* <loc> .peek <byte> */
private int
zpeek(register os_ptr op)
{	check_type(*op, t_integer);
	make_int(op, *(byte *)(op->value.intval));
	return 0;
}

/* <loc> <byte> .poke - */
private int
zpoke(register os_ptr op)
{	check_type(*op, t_integer);
	check_int_leu(op[-1], 0xff);
	*(byte *)(op[-1].value.intval) = (byte)op->value.intval;
	pop(1);
	return 0;
}

/* ------ Operator initialization ------ */

BEGIN_OP_DEFS(zdosio_op_defs) {
	{"1.inport", zinport},
	{"1.inportb", zinportb},
	{"2.outport", zoutport},
	{"2.outportb", zoutportb},
	{"1.peek", zpeek},
	{"2.poke", zpoke},
END_OP_DEFS(0) }
