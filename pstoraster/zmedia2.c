/*
  Copyright 1993-1999 by Easy Software Products.
  Copyright (C) 1993, 1995 Aladdin Enterprises.  All rights reserved.
  
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

/* zmedia.c */
/* Media matching for setpagedevice */
#include "math_.h"
#include "memory_.h"
#include "ghost.h"
#include "gsmatrix.h"
#include "errors.h"
#include "oper.h"
#include "idict.h"
#include "idparam.h"
#include "iname.h"
#include "store.h"

/* <pagedict> <attrdict> <policydict> <keys> .matchmedia <key> true */
/* <pagedict> <attrdict> <policydict> <keys> .matchmedia false */
/* <pagedict> null <policydict> <keys> .matchmedia null true */
private int zmatch_page_size(P7(const ref *pvreq, const ref *pvmed,
  int policy, int orient, float *best_mismatch, gs_matrix *pmat,
  gs_point *pmsize));
private int
zmatchmedia(register os_ptr op)
{	os_ptr preq = op - 3;
	os_ptr pattr = op - 2;
	os_ptr ppol = op - 1;
#define pkeys op
	int policy_default;
	float best_mismatch = (float)max_long;		/* adhoc */
	ref mmkey, nmkey;
	float mbest = best_mismatch;
	uint matched_priority;
	ref no_priority;
	ref *ppriority;
	int mepos, orient;
	int code;
	int ai;
	ref aelt[2];
#define mkey aelt[0]
#define mdict aelt[1]

	if ( r_has_type(pattr, t_null) )
	  {	check_op(4);
		make_null(op - 3);
		make_true(op - 2);
		pop(2);
		return 0;
	  }
	check_type(*preq, t_dictionary);
	check_dict_read(*preq);
	check_type(*pattr, t_dictionary);
	check_dict_read(*pattr);
	check_type(*ppol, t_dictionary);
	check_dict_read(*ppol);
	check_array(*pkeys);
	check_read(*pkeys);
/**** MRS - This code 1) doesn't work, and 2) if it did work, why do we want it? ****/
#if 0
	switch ( code = dict_int_null_param(preq, "MediaPosition", 0, 0x7fff,
					    0, &mepos) )
	  {
	  default: return code;
	  case 2:
	  case 1: mepos = -1;
	  case 0: ;
	  }
	switch ( code = dict_int_null_param(preq, "Orientation", 0, 3,
					    0, &orient) )
	  {
	  default: return code;
	  case 2:
	  case 1: orient = -1;
	  case 0: ;
	  }
#else
        mepos  = -1;
	orient = -1;
#endif /* 0 */

	code = dict_int_param(ppol, "PolicyNotFound", 0, 7, 0,
			      &policy_default);
	if ( code < 0 )
	  return code;
	if ( dict_find_string(pattr, "Priority", &ppriority) > 0 )
	{	check_array_only(*ppriority);
		check_read(*ppriority);
	}
	else
	{	make_empty_array(&no_priority, a_readonly);
		ppriority = &no_priority;
	}
#define reset_match()\
  matched_priority = r_size(ppriority),\
  make_null(&mmkey),\
  make_null(&nmkey)
	reset_match();
	for ( ai = dict_first(pattr); (ai = dict_next(pattr, ai, aelt)) >= 0; )
	{	if ( r_has_type(&mdict, t_dictionary) &&
		     r_has_attr(dict_access_ref(&mdict), a_read) &&
		     r_has_type(&mkey, t_integer) &&
		     (mepos < 0 || mkey.value.intval == mepos)
		   )
		{	bool match_all;
			uint ki, pi;

			code = dict_bool_param(&mdict, "MatchAll", false,
					       &match_all);
			if ( code < 0 )
			  return code;
			for ( ki = 0; ki < r_size(pkeys); ki++ )
			{	ref key;
				ref kstr;
				ref *prvalue;
				ref *pmvalue;
				ref *ppvalue;
				int policy;

				array_get(pkeys, ki, &key);
				if ( dict_find(&mdict, &key, &pmvalue) <= 0 )
				  continue;
				if ( dict_find(preq, &key, &prvalue) <= 0 ||
				     r_has_type(prvalue, t_null)
				   )
				{	if ( match_all )
					  goto no;
					else
					  continue;
				}
				/* Look for the Policies entry for this key. */
				if ( dict_find(ppol, &key, &ppvalue) > 0 )
				  { check_type_only(*ppvalue, t_integer);
				    policy = ppvalue->value.intval;
				  }
				else
				  policy = policy_default;
	/*
	 * Match a requested attribute value with the attribute value in the
	 * description of a medium.  For all attributes except PageSize,
	 * matching means equality.  PageSize is special; see match_page_size
	 * below.
	 */
				if ( r_has_type(&key, t_name) &&
				     (name_string_ref(&key, &kstr),
				     r_size(&kstr) == 8 &&
				     !memcmp(kstr.value.bytes, "PageSize", 8))
				   )
				  { gs_matrix ignore_mat;
				    gs_point ignore_msize;
				    if ( zmatch_page_size(prvalue, pmvalue,
							  policy, orient,
							  &best_mismatch,
							  &ignore_mat,
							  &ignore_msize)
					<= 0 )
				      goto no;
				  }
				else
				  if ( !obj_eq(prvalue, pmvalue) )
				    goto no;
			}
			/* We have a match.  If it is a better match */
			/* than the current best one, it supersedes it */
			/* regardless of priority. */
			if ( best_mismatch < mbest )
			  { mbest = best_mismatch;
			    reset_match();
			  }
			/* In case of a tie, see if the new match has */
			/* priority. */
			for ( pi = matched_priority; pi > 0; )
			{	ref pri;
				pi--;
				array_get(ppriority, pi, &pri);
				if ( obj_eq(&mkey, &pri) )
				{	/* Yes, higher priority. */
					mmkey = mkey;
					matched_priority = pi;
					break;
				}
			}
			/* Save the match in case no match has priority. */
			nmkey = mkey;
no:			;
		}
	}
#undef mkey
#undef mdict
#undef pkeys
	if ( r_has_type(&nmkey, t_null) )
	{	make_false(op - 3);
		pop(3);
	}
	else
	{	if ( r_has_type(&mmkey, t_null) )
		  op[-3] = nmkey;
		else
		  op[-3] = mmkey;
		make_true(op - 2);
		pop(2);
	}
	return 0;
}

/* [<req_x> <req_y>] [<med_x0> <med_y0> (<med_x1> <med_y1> | )]
 *     <policy> <matrix|null> <orient|null> .matchpagesize
 *   <matrix|null> <med_x> <med_y> true   -or-  false
 */
private int
zmatchpagesize(register os_ptr op)
{	gs_matrix mat;
	float ignore_mismatch = (float)max_long;
	gs_point media_size;
	int orient, code;

	check_type(op[-2], t_integer);
	if ( r_has_type(op - 1, t_null) )
	  orient = -1;
	else
	  { check_int_leu(op[-1], 3);
	    orient = (int)op[-1].value.intval;
	  }
	code = zmatch_page_size(op - 4, op - 3, (int)op[-2].value.intval,
				orient,
				&ignore_mismatch, &mat, &media_size);
	switch ( code )
	  {
	  default:
		return code;
	  case 0:
		make_false(op - 4);
		pop(4);
		break;
	  case 1:
		code = write_matrix(op, &mat);
		if ( code < 0 && !r_has_type(op, t_null) )
		  return code;
		op[-4] = *op;
		make_real(op - 3, media_size.x);
		make_real(op - 2, media_size.y);
		make_true(op - 1);
		pop(1);
		break;
	  }
	return 0;
}
/* Match the PageSize.  See below for details. */
private bool match_page_size(P7(const gs_point *request,
  const gs_rect *medium, int policy, int orient,
  float *best_mismatch, gs_matrix *pmat, gs_point *pmsize));
private int
zmatch_page_size(const ref *pvreq, const ref *pvmed, int policy, int orient,
  float *best_mismatch, gs_matrix *pmat, gs_point *pmsize)
{	uint nm;
	check_array(*pvreq);
	check_array(*pvmed);
	if ( !(r_size(pvreq) == 2 && ((nm = r_size(pvmed)) == 2 || nm == 4)) )
	  return_error(e_rangecheck);
	{	ref rv[6];
		uint i;
		float v[6];
		int code;

		array_get(pvreq, 0, &rv[0]);
		array_get(pvreq, 1, &rv[1]);
		for ( i = 0; i < 4; ++i )
		  array_get(pvmed, i % nm, &rv[i + 2]);
		if ( (code = num_params(rv + 5, 6, v)) < 0 )
		  return code;
		{ gs_point request;
		  gs_rect medium;

		  request.x = v[0], request.y = v[1];
		  medium.p.x = v[2], medium.p.y = v[3],
		    medium.q.x = v[4], medium.q.y = v[5];
		  return match_page_size(&request, &medium, policy, orient,
					 best_mismatch, pmat, pmsize);
		}
	}
}
/*
 * Match a requested PageSize with the PageSize of a medium.  The medium
 * may specify either a single value [mx my] or a range
 * [mxmin mymin mxmax mymax]; matching means equality or inclusion
 * to within a tolerance of 5, possibly swapping the requested X and Y.
 * Take the Policies value into account, keeping track of the discrepancy
 * if needed.  When a match is found, also return the matrix to be
 * concatenated after setting up the default matrix, and the actual
 * media size.
 *
 * NOTE: The algorithm here doesn't work properly for variable-size media
 * when the match isn't exact.  We'll fix it if we ever need to.
 */
private void make_adjustment_matrix(P5(const gs_point *request,
  const gs_rect *medium, gs_matrix *pmat, bool scale, int rotate));
private bool
match_page_size(const gs_point *request, const gs_rect *medium, int policy,
  int orient, float *best_mismatch, gs_matrix *pmat, gs_point *pmsize)
{	double rx = request->x, ry = request->y;

	if ( rx - medium->p.x >= -5 && rx - medium->q.x <= 5 &&
	     ry - medium->p.y >= -5 && ry - medium->q.y <= 5 &&
	     (orient < 0 || !(orient & 1))
	   )
	  { *best_mismatch = 0;
	    make_adjustment_matrix(request, medium, pmat, false,
				   (orient >= 0 ? orient : 0));
	  }
	else if ( rx - medium->p.y >= -5 && rx - medium->q.y <= 5 &&
	     ry - medium->p.x >= -5 && ry - medium->q.x <= 5 &&
	     (orient < 0 || (orient & 1))
	   )
	  { *best_mismatch = 0;
	    make_adjustment_matrix(request, medium, pmat, false,
				   (orient >= 0 ? orient :
				    rx < ry ? -1 : 1));
	  }
	else
	{ int rotate =
	    (orient >= 0 ? orient :
	     rx < ry ?
	     (medium->q.x > medium->q.y ? -1 : 0) :
	     (medium->q.x < medium->q.y ? 1 : 0));
	  bool larger =
	    (rotate ? medium->q.y >= rx && medium->q.x >= ry :
	     medium->q.x >= rx && medium->q.y >= ry);
	  bool adjust = false;
	  float mismatch = medium->q.x * medium->q.y - rx * ry;

	  switch ( policy )
	    {
	    default:		/* exact match only */
	      return false;
	    case 3:		/* nearest match, adjust */
	      adjust = true;
	    case 5:		/* nearest match, don't adjust */
	      if ( fabs(mismatch) >= fabs(*best_mismatch) )
		return false;
	      break;
	    case 4:		/* next larger match, adjust */
	      adjust = true;
	    case 6:		/* next larger match, don't adjust */
	      if ( !larger || mismatch >= *best_mismatch )
		return false;
	      break;
	    }
	  if ( adjust )
	    make_adjustment_matrix(request, medium, pmat, !larger, rotate);
	  else
	    { gs_rect req_rect;
	      req_rect.p = *request;
	      req_rect.q = *request;
	      make_adjustment_matrix(request, &req_rect, pmat, false, rotate);
	    }
	  *best_mismatch = mismatch;
	}
	if ( pmat->xx == 0 )
	  { /* Swap request X and Y. */
	    double temp = rx;
	    rx = ry, ry = temp;
	  }
#define adjust_into(req, mmin, mmax)\
  (req < mmin ? mmin : req > mmax ? mmax : req)
	pmsize->x = adjust_into(rx, medium->p.x, medium->q.x);
	pmsize->y = adjust_into(ry, medium->p.y, medium->q.y);
	return true;
}
/*
 * Compute the adjustment matrix for scaling and/or rotating the page
 * to match the medium.  If the medium is completely flexible in a given
 * dimension (e.g., roll media in one dimension, or displays in both),
 * we must adjust its size in that dimension to match the request.
 * We recognize this by medium->p.{x,y} = 0.
 */
private void
make_adjustment_matrix(const gs_point *request, const gs_rect *medium,
  gs_matrix *pmat, bool scale, int rotate)
{	double rx = request->x, ry = request->y;
	double mx = medium->q.x, my = medium->q.y;

	/* Rotate the request if necessary. */
	if ( rotate & 1 )
	  { double temp = rx;
	    rx = ry, ry = temp;
	  }

	/* Adjust the medium size if flexible. */
	if ( medium->p.x == 0 && mx > rx )
	  mx = rx;
	if ( medium->p.y == 0 && my > ry )
	  my = ry;

	/* Translate to align the centers. */
	gs_make_translation(mx / 2, my / 2, pmat);

	/* Rotate if needed. */
	if ( rotate )
	  gs_matrix_rotate(pmat, 90.0 * rotate, pmat);

	/* Scale if needed. */
	if ( scale )
	  { double xfactor = mx / rx;
	    double yfactor = my / ry;
	    double factor = min(xfactor, yfactor);
	    if ( factor < 1 )
	      gs_matrix_scale(pmat, factor, factor, pmat);
	  }

	/* Now translate the origin back, */
	/* using the original, unswapped request. */
	gs_matrix_translate(pmat, -request->x / 2, -request->y / 2, pmat);
}

/* ------ Initialization procedure ------ */

BEGIN_OP_DEFS(zmedia2_l2_op_defs) {
		op_def_begin_level2(),
	{"3.matchmedia", zmatchmedia},
	{"5.matchpagesize", zmatchpagesize},
END_OP_DEFS(0) }
