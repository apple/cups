/* Copyright (C) 1991, 1995 Aladdin Enterprises.  All rights reserved.
  
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

/* opdef.h */
/* Operator definition interface for Ghostscript */

/*
 * Operator procedures take the pointer to the top of the o-stack
 * as their argument.  They return 0 for success, a negative code
 * for an error, or a positive code for some uncommon situations (see below).
 */

/* Structure for initializing the operator table. */
/*
 * Each operator file declares an array of these, of the following kind:

BEGIN_OP_DEFS(my_defs) {
	{"1name", zname},
	    ...
END_OP_DEFS(iproc) }

 * where iproc is an initialization procedure for the file, or 0.
 * This definition always appears at the END of the file,
 * to avoid the need for forward declarations for all the
 * operator procedures.
 *
 * Operators may be stored in dictionaries other than systemdict.
 * We support this with op_def entries of a special form:

	op_def_begin_dict("dictname"),

 */
typedef struct {
	const char _ds *oname;
	op_proc_p proc;
} op_def;
typedef const op_def *op_def_ptr;
#define op_def_begin_dict(dname) {dname, 0}
#define op_def_begin_filter() op_def_begin_dict("filterdict")
#define op_def_begin_level2() op_def_begin_dict("level2dict")
#define op_def_is_begin_dict(def) ((def)->proc == 0)
#define op_def_end(iproc) {(char _ds *)0, (op_proc_p)iproc}

/*
 * We need to define each op_defs table as a procedure that returns
 * the actual table, because of cross-segment linking restrictions
 * in the Borland C compiler for MS Windows.
 */

#define BEGIN_OP_DEFS(xx_op_defs)\
const op_def *xx_op_defs(P0())\
{	static const far_data op_def op_defs_[] =

#define END_OP_DEFS(iproc)\
		op_def_end(iproc)\
	};\
	return op_defs_;

/*
 * Internal operators whose names begin with %, such as continuation
 * operators, do not appear in systemdict.  Ghostscript assumes
 * that these operators cannot appear anywhere (in executable form)
 * except on the e-stack; to maintain this invariant, the execstack
 * operator converts them to literal form, and cvx refuses to convert
 * them back.  As a result of this invariant, they do not need to
 * push themselves back on the e-stack when executed, since the only
 * place they could have come from was the e-stack.
 */
#define op_def_is_internal(def) ((def)->oname[1] == '%')

/*
 * All operators are catalogued in a table; this is necessary because
 * they must have a short packed representation for the sake of 'bind'.
 * The `size' of an operator is normally its index in this table;
 * however, internal operators have a `size' of 0, and their true index
 * must be found by searching the table for their procedure address.
 */
ushort op_find_index(P1(const ref *));
#define op_index(opref)\
  (r_size(opref) == 0 ? op_find_index(opref) : r_size(opref))
/*
 * There are actually two kinds of operators: the real ones (t_operator),
 * and ones defined by procedures (t_oparray).  The catalog for t_operators
 * is op_def_table, and their index is in the range [1..op_def_count-1].
 */
#define op_index_is_operator(index) ((index) < op_def_count)
/*
 * Because of a bug in Sun's SC1.0 compiler,
 * we have to spell out the typedef for op_def_ptr here:
 */
extern const op_def **op_def_table;
extern uint op_def_count;
#define op_num_args(opref) (op_def_table[op_index(opref)]->oname[0] - '0')
#define op_index_proc(index) (op_def_table[index]->proc)
/*
 * There are two catalogs for t_oparrays, one global and one local.
 * Operator indices for the global table are in the range
 *	[op_def_count..op_def_count+op_array_global.count-1]
 * Operator indices for the local table are in the range
 *	[op_def_count+r_size(&op_array_global.table)..
 *	  op_def_count+r_size(&op_array_global.table)+op_array_local.count-1]
 */
typedef struct op_array_table_s {
	ref table;			/* t_array */
	ushort *nx_table;		/* name indices */
	uint count;			/* # of occupied entries */
	uint base_index;		/* operator index of first entry */
	uint attrs;			/* ref attrs of ops in this table */
	ref *root_p;			/* self-pointer for GC root */
} op_array_table;
extern op_array_table
  op_array_table_global,
  op_array_table_local;
#define op_index_op_array_table(index)\
  ((index) < op_array_table_local.base_index ?\
   &op_array_table_global : &op_array_table_local)

/*
 * Convert an operator index to an operator or oparray ref.
 * This is only used for debugging and for 'get' from packed arrays,
 * so it doesn't have to be very fast.
 */
void op_index_ref(P2(uint, ref *));
