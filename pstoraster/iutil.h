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

/* iutil.h */
/* Prototypes for procedures in iutil.c */
/* Requires imemory.h, ostack.h */

/* ------ Object utilities ------ */

/* Copy refs from one place to another. */
/* (If we are copying to the stack, we can just use memcpy.) */
void refcpy_to_new(P3(ref *to, const ref *from, uint size));
int refcpy_to_old(P5(ref *aref, uint index, const ref *from, uint size, client_name_t cname));

/* Fill an array with nulls. */
void refset_null(P2(ref *to, uint size));

/* Compare two objects for equality. */
bool obj_eq(P2(const ref *, const ref *));

/* Compare two objects for identity. */
/* (This is not a standard PostScript concept.) */
bool obj_ident_eq(P2(const ref *, const ref *));

/*
 * Create a printable representation of an object, a la cvs (full_print =
 * false) or == (full_print = true).  Return 0 if OK, <0 if the destination
 * wasn't large enough or the object's contents weren't readable.
 * If the object was a string or name, store a pointer to its characters
 * even if it was too large.  Note that if full_print is true, the only
 * allowed types are boolean, integer, and real.
 */
int obj_cvp(P6(const ref *op, byte *str, uint len, uint *prlen,
	       const byte **pchars, bool full_print));
#define obj_cvs(op, str, len, prlen, pchars)\
  obj_cvp(op, str, len, prlen, pchars, false)

/* Get an element from an array (packed or not). */
int array_get(P3(const ref *, long, ref *));

/* Get an element from a packed array. */
/* (This works for ordinary arrays too.) */
/* Source and destination are allowed to overlap if the source is packed, */
/* or if they are identical. */
void packed_get(P2(const ref_packed *, ref *));

/* Check to make sure an interval contains no object references */
/* to a space younger than a given one. */
/* Return 0 or e_invalidaccess. */
int refs_check_space(P3(const ref *refs, uint size, uint space));

/* ------ String utilities ------ */

/* Convert a C string to a string object. */
int string_to_ref(P4(const char *, ref *, gs_ref_memory_t *, client_name_t));

/* Convert a string object to a C string. */
/* Return 0 iff the buffer can't be allocated. */
char *ref_to_string(P3(const ref *, gs_memory_t *, client_name_t));

/* ------ Operand utilities ------ */

/* Get N numeric operands from the stack or an array. */
int num_params(P3(const ref *, int, float *));

/* Get a single real parameter. */
/* The only possible error is e_typecheck. */
int real_param(P2(const ref *, float *));

/* Get an integer parameter in a given range. */
int int_param(P3(const ref *, int, int *));

/* Make real values on the stack. */
void make_reals(P3(ref *, const float *, int));

/* Define the gs_matrix type if necessary. */
#ifndef gs_matrix_DEFINED
#  define gs_matrix_DEFINED
typedef struct gs_matrix_s gs_matrix;
#endif

/* Read a matrix operand. */
int read_matrix(P2(const ref *, gs_matrix *));

/* Write a matrix operand. */
int write_matrix(P2(ref *, const gs_matrix *));
