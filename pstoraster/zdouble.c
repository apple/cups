/* Copyright (C) 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* zdouble.c */
/* Double-precision floating point arithmetic operators */
#include "math_.h"
#include "memory_.h"
#include "string_.h"
#include "ctype_.h"
#include "ghost.h"
#include "gxfarith.h"
#include "errors.h"
#include "oper.h"
#include "store.h"

/*
 * Thanks to Jean-Pierre Demailly of the Institut Fourier of the
 * Universit\'e de Grenoble I <demailly@fourier.grenet.fr> for proposing
 * this package and for arranging the funding for its creation.
 *
 * These operators work with doubles represented as 8-byte strings.  When
 * applicable, they write their result into a string supplied as an argument.
 * They also accept ints and reals as arguments.
 */

/******
 ****** Time expended: 2.75 hours.
 ******/

/* Forward references */
private int near double_params_result(P3(os_ptr, int, double *));
private int near double_params(P3(os_ptr, int, double *));
private int near double_result(P3(os_ptr, int, double));
private int near double_unary(P2(os_ptr, double (*)(P1(double))));

#define dbegin_unary()\
	double num;\
	int code = double_params_result(op, 1, &num);\
	if ( code < 0 )\
	  return code

#define dbegin_binary()\
	double num[2];\
	int code = double_params_result(op, 2, num);\
	if ( code < 0 )\
	  return code

/* ------ Arithmetic ------ */

/* <dnum1> <dnum2> <dresult> .dadd <dresult> */
private int
zdadd(os_ptr op)
{	dbegin_binary();
	return double_result(op, 2, num[0] + num[1]);
}

/* <dnum1> <dnum2> <dresult> .ddiv <dresult> */
private int
zddiv(os_ptr op)
{	dbegin_binary();
	if ( num[1] == 0.0 )
	  return_error(e_undefinedresult);
	return double_result(op, 2, num[0] / num[1]);
}

/* <dnum1> <dnum2> <dresult> .dmul <dresult> */
private int
zdmul(os_ptr op)
{	dbegin_binary();
	return double_result(op, 2, num[0] * num[1]);
}

/* <dnum1> <dnum2> <dresult> .dsub <dresult> */
private int
zdsub(os_ptr op)
{	dbegin_binary();
	return double_result(op, 2, num[0] - num[1]);
}

/* ------ Simple functions ------ */

/* <dnum> <dresult> .dabs <dresult> */
private int
zdabs(os_ptr op)
{	return double_unary(op, fabs);
}

/* <dnum> <dresult> .dceiling <dresult> */
private int
zdceiling(os_ptr op)
{	return double_unary(op, ceil);
}

/* <dnum> <dresult> .dfloor <dresult> */
private int
zdfloor(os_ptr op)
{	return double_unary(op, floor);
}

/* <dnum> <dresult> .dneg <dresult> */
private int
zdneg(os_ptr op)
{	dbegin_unary();
	return double_result(op, 1, -num);
}

/* <dnum> <dresult> .dround <dresult> */
private int
zdround(os_ptr op)
{	dbegin_unary();
	return double_result(op, 1, floor(num + 0.5));
}

/* <dnum> <dresult> .dsqrt <dresult> */
private int
zdsqrt(os_ptr op)
{	dbegin_unary();
	if ( num < 0.0 )
	  return_error(e_rangecheck);
	return double_result(op, 1, sqrt(num));
}

/* <dnum> <dresult> .dtruncate <dresult> */
private int
zdtruncate(os_ptr op)
{	dbegin_unary();
	return double_result(op, 1, (num < 0 ? ceil(num) : floor(num)));
}

/* ------ Transcendental functions ------ */

private int near
darc(os_ptr op, double (*afunc)(P1(double)))
{	dbegin_unary();
	return double_result(op, 1, (*afunc)(num) * radians_to_degrees);
}
/* <dnum> <dresult> .darccos <dresult> */
private int
zdarccos(os_ptr op)
{	return darc(op, acos);
}
/* <dnum> <dresult> .darcsin <dresult> */
private int
zdarcsin(os_ptr op)
{	return darc(op, asin);
}

/* <dnum> <ddenom> <dresult> .datan <dresult> */
private int
zdatan(register os_ptr op)
{	double result;
	dbegin_binary();
	if ( num[0] == 0 )		/* on X-axis, special case */
	   {	if ( num[1] == 0 )
		  return_error(e_undefinedresult);
		result = (num[1] < 0 ? 180 : 0);
	   }
	else
	   {	result = atan2(num[0], num[1]) * radians_to_degrees;
		if ( result < 0 )
		  result += 360;
	   }
	return double_result(op, 2, result);
}

/* <dnum> <dresult> .dcos <dresult> */
private int
zdcos(os_ptr op)
{	return double_unary(op, gs_cos_degrees);
}

/* <dbase> <dexponent> <dresult> .dexp <dresult> */
private int
zdexp(os_ptr op)
{	double ipart;
	dbegin_binary();
	if ( num[0] == 0.0 && num[1] == 0.0 )
	  return_error(e_undefinedresult);
	if ( num[0] < 0.0 && modf(num[1], &ipart) != 0.0 )
	  return_error(e_undefinedresult);
	return double_result(op, 2, pow(num[0], num[1]));
}

private int near
dlog(os_ptr op, double (*lfunc)(P1(double)))
{	dbegin_unary();
	if ( num <= 0.0 )
	  return_error(e_rangecheck);
	return double_result(op, 1, (*lfunc)(num));
}
/* <dposnum> <dresult> .dln <dresult> */
private int
zdln(os_ptr op)
{	return dlog(op, log);
}
/* <dposnum> <dresult> .dlog <dresult> */
private int
zdlog(os_ptr op)
{	return dlog(op, log10);
}

/* <dnum> <dresult> .dsin <dresult> */
private int
zdsin(os_ptr op)
{	return double_unary(op, gs_sin_degrees);
}

/* ------ Comparison ------ */

private int near
dcompare(os_ptr op, int mask)
{	double num[2];
	int code = double_params(op, 2, num);
	if ( code < 0 )
	  return code;
	make_bool(op - 1,
		  (mask & (num[0] < num[1] ? 1 : num[0] > num[1] ? 4 : 2))
		   != 0);
	pop(1);
	return 0;
}
/* <dnum1> <dnum2> .deq <bool> */
private int
zdeq(os_ptr op)
{	return dcompare(op, 2);
}
/* <dnum1> <dnum2> .dge <bool> */
private int
zdge(os_ptr op)
{	return dcompare(op, 6);
}
/* <dnum1> <dnum2> .dgt <bool> */
private int
zdgt(os_ptr op)
{	return dcompare(op, 4);
}
/* <dnum1> <dnum2> .dle <bool> */
private int
zdle(os_ptr op)
{	return dcompare(op, 3);
}
/* <dnum1> <dnum2> .dlt <bool> */
private int
zdlt(os_ptr op)
{	return dcompare(op, 1);
}
/* <dnum1> <dnum2> .dne <bool> */
private int
zdne(os_ptr op)
{	return dcompare(op, 5);
}

/* ------ Conversion ------ */

/* Take the easy way out.... */
#define max_chars 50

/* <dnum> <dresult> .cvd <dresult> */
private int
zcvd(os_ptr op)
{	dbegin_unary();
	return double_result(op, 1, num);
}

/* <string> <dresult> .cvsd <dresult> */
private int
zcvsd(os_ptr op)
{	int code = double_params_result(op, 0, NULL);
	double num;
	char buf[max_chars + 2];
	char *str = buf;
	uint len;
	char end;

	if ( code < 0 )
	  return code;
	check_read_type(op[-1], t_string);
	len = r_size(op - 1);
	if ( len > max_chars )
	  return_error(e_limitcheck);
	memcpy(str, op[-1].value.bytes, len);
	/*
	 * We check syntax in the following way: we remove whitespace,
	 * verify that the string contains only [0123456789+-.dDeE],
	 * then append a $ and then check that the next character after
	 * the scanned number is a $.
	 */
	while ( len > 0 && isspace(*str) )
	  ++str, --len;
	while ( len > 0 && isspace(str[len - 1]) )
	  --len;
	str[len] = 0;
	if ( strspn(str, "0123456789+-.dDeE") != len )
	  return_error(e_syntaxerror);
	strcat(str, "$");
	if ( sscanf(str, "%lf%c", &num, &end) != 2 || end != '$' )
	  return_error(e_syntaxerror);
	return double_result(op, 1, num);
}

/* <dnum> .dcvi <int> */
private int
zdcvi(os_ptr op)
{
#define alt_min_long (-1L << (arch_sizeof_long * 8 - 1))
#define alt_max_long (~(alt_min_long))
	static const double min_int_real = (alt_min_long * 1.0 - 1);
	static const double max_int_real = (alt_max_long * 1.0 + 1);
	double num;
	int code = double_params(op, 1, &num);
	if ( code < 0 )
	  return code;

	if ( num < min_int_real || num > max_int_real )
	  return_error(e_rangecheck);
	make_int(op, (long)num);	/* truncates toward 0 */
	return 0;
}

/* <dnum> .dcvr <real> */
private int
zdcvr(os_ptr op)
{
#define b30 (0x40000000L * 1.0)
#define max_mag (0xffffff * b30 * b30 * b30 * 0x4000)
	static const float min_real = -max_mag;
	static const float max_real = max_mag;
#undef b30
#undef max_mag
	double num;
	int code = double_params(op, 1, &num);
	if ( code < 0 )
	  return code;
	if ( num < min_real || num > max_real )
	  return_error(e_rangecheck);
	make_real(op, (float)num);
	return 0;
}

/* <dnum> <string> .dcvs <substring> */
private int
zdcvs(os_ptr op)
{	double num;
	int code = double_params(op - 1, 1, &num);
	char str[max_chars + 1];
	int len;

	if ( code < 0 )
	  return code;
	check_write_type(*op, t_string);
	/*
	 * To get fully accurate output results for IEEE double-
	 * precision floats (53 bits of mantissa), the ANSI
	 * %g default of 6 digits is not enough; 16 are needed.
	 * Unfortunately, using %.16g produces unfortunate artifacts such as
	 * 1.2 printing as 1.200000000000005.  Therefore, we print using %g,
	 * and if the result isn't accurate enough, print again
	 * using %.16g.
	 */
	   {	double scanned;
		sprintf(str, "%g", num);
		sscanf(str, "%lf", &scanned);
		if ( scanned != num )
		  sprintf(str, "%.16g", num);
	   }
	len = strlen(str);
	if ( len > r_size(op) )
	  return_error(e_rangecheck);
	memcpy(op->value.bytes, str, len);
	op[-1] = *op;
	r_set_size(op - 1, len);
	pop(1);
	return 0;
}

/* ------ Initialization table ------ */

BEGIN_OP_DEFS(zdouble_op_defs) {
		/* Arithmetic */
	{"3.dadd", zdadd},
	{"3.ddiv", zddiv},
	{"3.dmul", zdmul},
	{"3.dsub", zdsub},
		/* Simple functions */
	{"2.dabs", zdabs},
	{"2.dceiling", zdceiling},
	{"2.dfloor", zdfloor},
	{"2.dneg", zdneg},
	{"2.dround", zdround},
	{"2.dsqrt", zdsqrt},
	{"2.dtruncate", zdtruncate},
		/* Transcendental functions */
	{"2.darccos", zdarccos},
	{"2.darcsin", zdarcsin},
	{"3.datan", zdatan},
	{"2.dcos", zdcos},
	{"3.dexp", zdexp},
	{"2.dln", zdln},
	{"2.dlog", zdlog},
	{"2.dsin", zdsin},
		/* Comparison */
	{"2.deq", zdeq},
	{"2.dge", zdge},
	{"2.dgt", zdgt},
	{"2.dle", zdle},
	{"2.dlt", zdlt},
	{"2.dne", zdne},
		/* Conversion */
	{"2.cvd", zcvd},
	{"2.cvsd", zcvsd},
	{"1.dcvi", zdcvi},
	{"1.dcvr", zdcvr},
	{"2.dcvs", zdcvs},
END_OP_DEFS(0) }

/* ------ Internal procedures ------ */

/* Get some double arguments. */
private int near
double_params(os_ptr op, int count, double *pval)
{	pval += count;
	while ( --count >= 0 )
	   {	switch ( r_type(op) )
		   {
		case t_real:
			*--pval = op->value.realval;
			break;
		case t_integer:
			*--pval = op->value.intval;
			break;
		case t_string:
			if ( !r_has_attr(op, a_read) ||
			     r_size(op) != sizeof(double)
			   )
			  return_error(e_typecheck);
			--pval;
			memcpy(pval, op->value.bytes, sizeof(double));
			break;
		case t__invalid:
			return_error(e_stackunderflow);
		default:
			return_error(e_typecheck);
		   }
		op--;
	   }
	return 0;
}

/* Get some double arguments, and check for a double result. */
private int near
double_params_result(os_ptr op, int count, double *pval)
{	check_write_type(*op, t_string);
	if ( r_size(op) != sizeof(double) )
	  return_error(e_typecheck);
	return double_params(op - 1, count, pval);
}

/* Return a double result. */
private int near
double_result(os_ptr op, int count, double result)
{	os_ptr op1 = op - count;
	ref_assign_inline(op1, op);
	memcpy(op1->value.bytes, &result, sizeof(double));
	pop(count);
	return 0;
}

/* Apply a unary function to a double operand. */
private int near
double_unary(os_ptr op, double (*func)(P1(double)))
{	dbegin_unary();
	return double_result(op, 1, (*func)(num));
}
