/* Copyright (C) 1993, 1995, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: iparam.h,v 1.2 2000/03/08 23:15:14 mike Exp $ */
/* Requires ialloc.h, istack.h */

#ifndef iparam_INCLUDED
#  define iparam_INCLUDED

#include "gsparam.h"

/*
 * This file defines the interface to iparam.c, which provides
 * several implementations of the parameter dictionary interface
 * defined in gsparam.h:
 *      - an implementation using dictionary objects;
 *      - an implementation using name/value pairs in an array;
 *      - an implementation using name/value pairs on a stack.
 *
 * When reading ('putting'), these implementations keep track of
 * which parameters have been referenced and which have caused errors.
 * The results array contains 0 for a parameter that has not been accessed,
 * 1 for a parameter accessed without error, or <0 for an error.
 */

typedef struct iparam_loc_s {
    ref *pvalue;		/* (actually const) */
    int *presult;
} iparam_loc;

#define iparam_list_common\
	gs_param_list_common;\
	union {\
	  struct {		/* reading */\
	    int (*read)(P3(iparam_list *, const ref *, iparam_loc *));\
	    ref policies;	/* policy dictionary or null */\
	    bool require_all;	/* if true, require all params to be known */\
	  } r;\
	  struct {		/* writing */\
	    int (*write)(P3(iparam_list *, const ref *, const ref *));\
	    ref wanted;		/* desired keys or null */\
	  } w;\
	} u;\
	int (*enumerate)(P4(iparam_list *, gs_param_enumerator_t *, gs_param_key_t *, ref_type *));\
	int *results;		/* (only used when reading, 0 when writing) */\
	uint count;		/* # of key/value pairs */\
	bool int_keys		/* if true, keys are integers */
typedef struct iparam_list_s iparam_list;
struct iparam_list_s {
    iparam_list_common;
};

typedef struct dict_param_list_s {
    iparam_list_common;
    ref dict;			/* dictionary or array */
} dict_param_list;
typedef struct array_param_list_s {
    iparam_list_common;
    ref *bot;
    ref *top;
} array_param_list;

/* For stack lists, the bottom of the list is just above a mark. */
typedef struct stack_param_list_s {
    iparam_list_common;
    ref_stack *pstack;
    uint skip;			/* # of top items to skip (reading only) */
} stack_param_list;

/* Procedural interface */
/*
 * For dict_param_list_read (only), the second parameter may be NULL,
 * equivalent to an empty dictionary.
 * The 3rd (const ref *) parameter is the policies dictionary when reading,
 * or the key selection dictionary when writing; it may be NULL in either case.
 * If the bool parameter is true, if there are any unqueried parameters,
 * the commit procedure will return an e_undefined error.
 */
int dict_param_list_read(P4(dict_param_list *, const ref * /*t_dictionary */ ,
			    const ref *, bool));
int dict_param_list_write(P3(dict_param_list *, ref * /*t_dictionary */ ,
			     const ref *));
int array_indexed_param_list_read(P4(dict_param_list *, const ref * /*t_*array */ ,
				     const ref *, bool));
int array_indexed_param_list_write(P3(dict_param_list *, ref * /*t_*array */ ,
				      const ref *));
int array_param_list_read(P5(array_param_list *, ref *, uint,
			     const ref *, bool));
int stack_param_list_read(P5(stack_param_list *, ref_stack *, uint,
			     const ref *, bool));
int stack_param_list_write(P3(stack_param_list *, ref_stack *,
			      const ref *));

#define iparam_list_release(plist)\
  ifree_object((plist)->results, "iparam_list_release")

#endif /* iparam_INCLUDED */
