/*
  Copyright 1993-2000 by Easy Software Products.
  Copyright 1989, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.

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

/*$Id: gsmisc.c,v 1.4 2000/03/08 23:14:44 mike Exp $ */
/* Miscellaneous utilities for Ghostscript library */
#include "ctype_.h"
#include "malloc_.h"
#include "math_.h"
#include "memory_.h"
#include "string_.h"
#include "gx.h"
#include "gpcheck.h"		/* for gs_return_check_interrupt */
#include "gserrors.h"
#include "gconfigv.h"		/* for USE_ASM */
#include "gxfarith.h"
#include "gxfixed.h"

/* Define private replacements for stdin, stdout, and stderr. */
FILE *gs_stdin, *gs_stdout, *gs_stderr;

/* Ghostscript writes debugging output to gs_debug_out. */
/* We define gs_debug and gs_debug_out even if DEBUG isn't defined, */
/* so that we can compile individual modules with DEBUG set. */
char gs_debug[128];
FILE *gs_debug_out;

/* Test whether a given debugging option is selected. */
/* Upper-case letters automatically include their lower-case counterpart. */
bool
gs_debug_c(int c)
{
    return
	(c >= 'a' && c <= 'z' ? gs_debug[c] | gs_debug[c ^ 32] : gs_debug[c]);
}

/* Define the formats for debugging printout. */
const char *const dprintf_file_and_line_format = "%10s(%4d): ";
const char *const dprintf_file_only_format = "%10s(unkn): ";

/*
 * Define the trace printout procedures.  We always include these, in case
 * other modules were compiled with DEBUG set.
 */
private const char *
dprintf_file_tail(const char *file)
{
    const char *tail = file + strlen(file);

    while (tail > file &&
	   (isalnum(tail[-1]) || tail[-1] == '.' || tail[-1] == '_')
	)
	--tail;
    return tail;
}
void
dprintf_file_and_line(FILE * f, const char *file, int line)
{
    if (gs_debug['/'])
	fprintf(f, dprintf_file_and_line_format,
		dprintf_file_tail(file), line);
}
void
dprintf_file(FILE * f, const char *file)
{
    if (gs_debug['/'])
	fprintf(f, dprintf_file_only_format, dprintf_file_tail(file));
}
void
eprintf_program_name(FILE * f, const char *program_name)
{
    if (program_name)
	fprintf(f, "%s: ", program_name);
}
void
lprintf_file_and_line(FILE * f, const char *file, int line)
{
    fprintf(f, "%s(%d): ", file, line);
}
void
lprintf_file_only(FILE * f, const char *file)
{
    fprintf(f, "%s(?): ", file);
}

/* Log an error return.  We always include this, in case other */
/* modules were compiled with DEBUG set. */
#undef gs_log_error		/* in case DEBUG isn't set */
int
gs_log_error(int err, const char *file, int line)
{
    if (gs_log_errors) {
	if (file == NULL)
	    dprintf1("Returning error %d.\n", err);
	else
	    dprintf3("%s(%d): Returning error %d.\n",
		     (const char *)file, line, err);
    }
    return err;
}

/* Check for interrupts before a return. */
int
gs_return_check_interrupt(int code)
{
    if (code < 0)
	return code;
    {
	int icode = gp_check_interrupts();

	return (icode == 0 ? code :
		gs_note_error((icode > 0 ? gs_error_interrupt : icode)));
    }
}

/* ------ Substitutes for missing C library functions ------ */

#ifdef memory__need_memmove	/* see memory_.h */
/* Copy bytes like memcpy, guaranteed to handle overlap correctly. */
/* ANSI C defines the returned value as being the src argument, */
/* but with the const restriction removed! */
void *
gs_memmove(void *dest, const void *src, size_t len)
{
    if (!len)
	return (void *)src;
#define bdest ((byte *)dest)
#define bsrc ((const byte *)src)
    /* We use len-1 for comparisons because adding len */
    /* might produce an offset overflow on segmented systems. */
    if (ptr_le(bdest, bsrc)) {
	register byte *end = bdest + (len - 1);

	if (ptr_le(bsrc, end)) {	/* Source overlaps destination from above. */
	    register const byte *from = bsrc;
	    register byte *to = bdest;

	    for (;;) {
		*to = *from;
		if (to >= end)	/* faster than = */
		    return (void *)src;
		to++;
		from++;
	    }
	}
    } else {
	register const byte *from = bsrc + (len - 1);

	if (ptr_le(bdest, from)) {	/* Source overlaps destination from below. */
	    register const byte *end = bsrc;
	    register byte *to = bdest + (len - 1);

	    for (;;) {
		*to = *from;
		if (from <= end)	/* faster than = */
		    return (void *)src;
		to--;
		from--;
	    }
	}
    }
#undef bdest
#undef bsrc
    /* No overlap, it's safe to use memcpy. */
    memcpy(dest, src, len);
    return (void *)src;
}
#endif

#ifdef memory__need_memchr	/* see memory_.h */
/* ch should obviously be char rather than int, */
/* but the ANSI standard declaration uses int. */
const char *
gs_memchr(const char *ptr, int ch, size_t len)
{
    if (len > 0) {
	register const char *p = ptr;
	register uint count = len;

	do {
	    if (*p == (char)ch)
		return p;
	    p++;
	} while (--count);
    }
    return 0;
}
#endif

#ifdef memory__need_memset	/* see memory_.h */
/* ch should obviously be char rather than int, */
/* but the ANSI standard declaration uses int. */
void *
gs_memset(void *dest, register int ch, size_t len)
{
    if (ch == 0)
	bzero(dest, len);
    else if (len > 0) {
	register char *p = dest;
	register uint count = len;

	do {
	    *p++ = (char)ch;
	} while (--count);
    }
    return dest;
}
#endif

#ifdef malloc__need_realloc	/* see malloc_.h */
/* Some systems have non-working implementations of realloc. */
void *
gs_realloc(void *old_ptr, size_t old_size, size_t new_size)
{
    void *new_ptr;

    /**** MRS - 64-bit align all data structures!!!!!!!!!!! ****/
    new_size = (new_size + 7) & ~7;

    if (new_size) {
	new_ptr = malloc(new_size);
	if (new_ptr == NULL)
	    return NULL;
    } else
	new_ptr = NULL;
    /* We have to pass in the old size, since we have no way to */
    /* determine it otherwise. */
    if (old_ptr != NULL) {
	if (new_ptr != NULL)
	    memcpy(new_ptr, old_ptr, min(old_size, new_size));
	free(old_ptr);
    }
    return new_ptr;
}
#endif

/* ------ Debugging support ------ */

/* Dump a region of memory. */
void
debug_dump_bytes(const byte * from, const byte * to, const char *msg)
{
    const byte *p = from;

    if (from < to && msg)
	dprintf1("%s:\n", msg);
    while (p != to) {
	const byte *q = min(p + 16, to);

	dprintf1("0x%lx:", (ulong) p);
	while (p != q)
	    dprintf1(" %02x", *p++);
	dputc('\n');
    }
}

/* Dump a bitmap. */
void
debug_dump_bitmap(const byte * bits, uint raster, uint height, const char *msg)
{
    uint y;
    const byte *data = bits;

    for (y = 0; y < height; ++y, data += raster)
	debug_dump_bytes(data, data + raster, (y == 0 ? msg : NULL));
}

/* Print a string. */
void
debug_print_string(const byte * chrs, uint len)
{
    uint i;

    for (i = 0; i < len; i++)
	dputc(chrs[i]);
    fflush(dstderr);
}

/* ------ Arithmetic ------ */

/* Compute M modulo N.  Requires N > 0; guarantees 0 <= imod(M,N) < N, */
/* regardless of the whims of the % operator for negative operands. */
int
imod(int m, int n)
{
    if (n <= 0)
	return 0;		/* sanity check */
    if (m >= 0)
	return m % n;
    {
	int r = -m % n;

	return (r == 0 ? 0 : n - r);
    }
}

/* Compute the GCD of two integers. */
int
igcd(int x, int y)
{
    int c = x, d = y;

    if (c < 0)
	c = -c;
    if (d < 0)
	d = -d;
    while (c != 0 && d != 0)
	if (c > d)
	    c %= d;
	else
	    d %= c;
    return d + c;		/* at most one is non-zero */
}

#if defined(set_fmul2fixed_vars) && !USE_ASM

/*
 * Floating multiply with fixed result, for avoiding floating point in
 * common coordinate transformations.  Assumes IEEE representation,
 * 16-bit short, 32-bit long.  Optimized for the case where the first
 * operand has no more than 16 mantissa bits, e.g., where it is a user space
 * coordinate (which are often integers).
 *
 * The assembly language version of this code is actually faster than
 * the FPU, if the code is compiled with FPU_TYPE=0 (which requires taking
 * a trap on every FPU operation).  If there is no FPU, the assembly
 * language version of this code is over 10 times as fast as the emulated FPU.
 */
/* Some of the following code has been tweaked for the Borland 16-bit */
/* compiler.  The tweaks do not change the algorithms. */
#if arch_ints_are_short && !defined(FOR80386)
#  define SHORT_ARITH
#endif
int
set_fmul2fixed_(fixed * pr, long /*float */ a, long /*float */ b)
{
#ifdef SHORT_ARITH
#  define long_rsh8_ushort(x)\
    (((ushort)(x) >> 8) | ((ushort)((ulong)(x) >> 16) << 8))
#  define utemp ushort
#else
#  define long_rsh8_ushort(x) ((ushort)((x) >> 8))
#  define utemp ulong
#endif
    /* utemp may be either ushort or ulong.  This is OK because */
    /* we only use ma and mb in multiplications involving */
    /* a long or ulong operand. */
    utemp ma = long_rsh8_ushort(a) | 0x8000;
    utemp mb = long_rsh8_ushort(b) | 0x8000;
    int e = 260 + _fixed_shift - ((
				      (((uint) ((ulong) a >> 16)) & 0x7f80) +
				      (((uint) ((ulong) b >> 16)) & 0x7f80)
				  ) >> 7);
    ulong p1 = ma * (b & 0xff);
    ulong p = (ulong) ma * mb;

#define p_bits (size_of(p) * 8)

    if ((byte) a) {		/* >16 mantissa bits */
	ulong p2 = (a & 0xff) * mb;

	p += ((((uint) (byte) a * (uint) (byte) b) >> 8) + p1 + p2) >> 8;
    } else
	p += p1 >> 8;
    if ((uint) e < p_bits)	/* e = -1 is possible */
	p >>= e;
    else if (e >= p_bits) {	/* also detects a=0 or b=0 */
	*pr = fixed_0;
	return 0;
    } else if (e >= -(p_bits - 1) || p >= 1L << (p_bits - 1 + e))
	return_error(gs_error_limitcheck);
    else
	p <<= -e;
    *pr = ((a ^ b) < 0 ? -p : p);
    return 0;
}
int
set_dfmul2fixed_(fixed * pr, ulong /*double lo */ xalo, long /*float */ b, long /*double hi */ xahi)
{
#ifdef SHORT_ARITH
#  define long_lsh3(x) ((((x) << 1) << 1) << 1)
#  define long_rsh(x,ng16) ((uint)((x) >> 16) >> (ng16 - 16))
#else
#  define long_lsh3(x) ((x) << 3)
#  define long_rsh(x,ng16) ((x) >> ng16)
#endif
    return set_fmul2fixed_(pr,
			   (xahi & 0xc0000000) +
			   (long_lsh3(xahi) & 0x3ffffff8) +
			   long_rsh(xalo, 29),
			   b);
}

#endif

#if USE_FPU_FIXED

/*
 * Convert from floating point to fixed point with scaling.
 * These are efficient algorithms for FPU-less machines.
 */
#define mbits_float 23
#define mbits_double 20
int
set_float2fixed_(fixed * pr, long /*float */ vf, int frac_bits)
{
    fixed mantissa;
    int shift;

    if (!(vf & 0x7f800000)) {
	*pr = fixed_0;
	return 0;
    }
    mantissa = (fixed) ((vf & 0x7fffff) | 0x800000);
    shift = ((vf >> 23) & 255) - (127 + 23) + frac_bits;
    if (shift >= 0) {
	if (shift >= sizeof(fixed) * 8 - 24)
	    return_error(gs_error_limitcheck);
	if (vf < 0)
	    mantissa = -mantissa;
	*pr = (fixed) (mantissa << shift);
    } else
	*pr = (shift < -24 ? fixed_0 :
	       vf < 0 ? -(fixed) (mantissa >> -shift) :		/* truncate */
	       (fixed) (mantissa >> -shift));
    return 0;
}
int
set_double2fixed_(fixed * pr, ulong /*double lo */ lo,
		  long /*double hi */ hi, int frac_bits)
{
    fixed mantissa;
    int shift;

    if (!(hi & 0x7ff00000)) {
	*pr = fixed_0;
	return 0;
    }
    /* We only use 31 bits of mantissa even if sizeof(long) > 4. */
    mantissa = (fixed) (((hi & 0xfffff) << 10) | (lo >> 22) | 0x40000000);
    shift = ((hi >> 20) & 2047) - (1023 + 30) + frac_bits;
    if (shift > 0)
	return_error(gs_error_limitcheck);
    *pr = (shift < -30 ? fixed_0 :
	   hi < 0 ? -(fixed) (mantissa >> -shift) :	/* truncate */
	   (fixed) (mantissa >> -shift));
    return 0;
}
/*
 * Given a fixed value x with fbits bits of fraction, set v to the mantissa
 * (left-justified in 32 bits) and f to the exponent word of the
 * corresponding floating-point value with mbits bits of mantissa in the
 * first word.  (The exponent part of f is biased by -1, because we add the
 * top 1-bit of the mantissa to it.)
 */
static const byte f2f_shifts[] =
{4, 3, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0};

#define f2f_declare(v, f)\
	ulong v;\
	long f
#define f2f(x, v, f, mbits, fbits)\
	if ( x < 0 )\
	  f = 0xc0000000 + (29 << mbits) - ((long)fbits << mbits), v = -x;\
	else\
	  f = 0x40000000 + (29 << mbits) - ((long)fbits << mbits), v = x;\
	if ( v < 0x8000 )\
	  v <<= 15, f -= 15 << mbits;\
	if ( v < 0x800000 )\
	  v <<= 8, f -= 8 << mbits;\
	if ( v < 0x8000000 )\
	  v <<= 4, f -= 4 << mbits;\
	{ int shift = f2f_shifts[v >> 28];\
	  v <<= shift, f -= shift << mbits;\
	}
long
fixed2float_(fixed x, int frac_bits)
{
    f2f_declare(v, f);

    if (x == 0)
	return 0;
    f2f(x, v, f, mbits_float, frac_bits);
    return f + (((v >> 7) + 1) >> 1);
}
void
set_fixed2double_(double *pd, fixed x, int frac_bits)
{
    f2f_declare(v, f);

    if (x == 0) {
	((long *)pd)[1 - arch_is_big_endian] = 0;
	((ulong *) pd)[arch_is_big_endian] = 0;
    } else {
	f2f(x, v, f, mbits_double, frac_bits);
	((long *)pd)[1 - arch_is_big_endian] = f + (v >> 11);
	((ulong *) pd)[arch_is_big_endian] = v << 21;
    }
}

/*
 * Compute A * B / C when 0 <= B < C and A * B exceeds (or might exceed)
 * the capacity of a long.
 */
#ifdef DEBUG
struct {
    long mnanb, mnab, manb, mab, mnc, mdq, mde, mds, mqh, mql;
} fmq_stat;

#  define mincr(x) ++fmq_stat.x
#else
#  define mincr(x) DO_NOTHING
#endif
fixed
fixed_mult_quo(fixed signed_A, fixed B, fixed C)
{				/* First compute A * B in double-fixed precision. */
    ulong A = (signed_A < 0 ? -signed_A : signed_A);
    long msw;
    ulong lsw;
    ulong p1;

#define num_bits (sizeof(fixed) * 8)
#define half_bits (num_bits / 2)
#define half_mask ((1L << half_bits) - 1)
    if (B <= half_mask) {
	if (A <= half_mask) {
	    fixed Q = (ulong) (A * B) / (ulong) C;

	    mincr(mnanb);
	    return (signed_A < 0 ? -Q : Q);
	}
	/*
	 * We might still have C <= half_mask, which we can
	 * handle with a simpler algorithm.
	 */
	lsw = (A & half_mask) * B;
	p1 = (A >> half_bits) * B;
	if (C <= half_mask) {
	    ulong q0 = (p1 += lsw >> half_bits) / C;
	    ulong rem = ((p1 - C * q0) << half_bits) + (lsw & half_mask);
	    ulong Q = (q0 << half_bits) + rem / C;

	    mincr(mnc);
	    return (signed_A < 0 ? -Q : Q);
	}
	msw = p1 >> half_bits;
	mincr(manb);
    } else if (A <= half_mask) {
	p1 = A * (B >> half_bits);
	msw = p1 >> half_bits;
	lsw = A * (B & half_mask);
	mincr(mnab);
    } else {			/* We have to compute all 4 products.  :-( */
	ulong lo_A = A & half_mask;
	ulong hi_A = A >> half_bits;
	ulong lo_B = B & half_mask;
	ulong hi_B = B >> half_bits;
	ulong p1x = hi_A * lo_B;

	msw = hi_A * hi_B;
	lsw = lo_A * lo_B;
	p1 = lo_A * hi_B;
	if (p1 > max_ulong - p1x)
	    msw += 1L << half_bits;
	p1 += p1x;
	msw += p1 >> half_bits;
	mincr(mab);
    }
    /* Finish up by adding the low half of p1 to the high half of lsw. */
#if max_fixed < max_long
    p1 &= half_mask;
#endif
    p1 <<= half_bits;
    if (p1 > max_ulong - lsw)
	msw++;
    lsw += p1;
    /*
     * Now divide the double-length product by C.  Note that we know msw
     * < C (otherwise the quotient would overflow).  Start by shifting
     * (msw,lsw) and C left until C >= 1 << (num_bits - 1).
     */
    {
	ulong denom = C;
	int shift = 0;

#define bits_4th (num_bits / 4)
	if (denom < 1L << (num_bits - bits_4th)) {
	    mincr(mdq);
	    denom <<= bits_4th, shift += bits_4th;
	}
#undef bits_4th
#define bits_8th (num_bits / 8)
	if (denom < 1L << (num_bits - bits_8th)) {
	    mincr(mde);
	    denom <<= bits_8th, shift += bits_8th;
	}
#undef bits_8th
	while (!(denom & (1L << (num_bits - 1)))) {
	    mincr(mds);
	    denom <<= 1, ++shift;
	}
	msw = (msw << shift) + (lsw >> (num_bits - shift));
	lsw <<= shift;
#if max_fixed < max_long
	lsw &= (1L << (sizeof(fixed) * 8)) - 1;
#endif
	/* Compute a trial upper-half quotient. */
	{
	    ulong hi_D = denom >> half_bits;
	    ulong lo_D = denom & half_mask;
	    ulong hi_Q = (ulong) msw / hi_D;

	    /* hi_Q might be too high by 1 or 2, but it isn't too low. */
	    ulong p0 = hi_Q * hi_D;
	    ulong p1 = hi_Q * lo_D;
	    ulong hi_P;

	    while ((hi_P = p0 + (p1 >> half_bits)) > msw ||
		   (hi_P == msw && ((p1 & half_mask) << half_bits) > lsw)
		) {		/* hi_Q was too high by 1. */
		--hi_Q;
		p0 -= hi_D;
		p1 -= lo_D;
		mincr(mqh);
	    }
	    p1 = (p1 & half_mask) << half_bits;
	    if (p1 > lsw)
		msw--;
	    lsw -= p1;
	    msw -= hi_P;
	    /* Now repeat to get the lower-half quotient. */
	    msw = (msw << half_bits) + (lsw >> half_bits);
#if max_fixed < max_long
	    lsw &= half_mask;
#endif
	    lsw <<= half_bits;
	    {
		ulong lo_Q = (ulong) msw / hi_D;
		long Q;

		p1 = lo_Q * lo_D;
		p0 = lo_Q * hi_D;
		while ((hi_P = p0 + (p1 >> half_bits)) > msw ||
		       (hi_P == msw && ((p1 & half_mask) << half_bits) > lsw)
		    ) {		/* lo_Q was too high by 1. */
		    --lo_Q;
		    p0 -= hi_D;
		    p1 -= lo_D;
		    mincr(mql);
		}
		Q = (hi_Q << half_bits) + lo_Q;
		return (signed_A < 0 ? -Q : Q);
	    }
	}
    }
#undef half_bits
#undef half_mask
}

#endif

/* Trace calls on sqrt when debugging. */
#undef sqrt
extern double sqrt(P1(double));
double
gs_sqrt(double x, const char *file, int line)
{
    if (gs_debug_c('~')) {
	fprintf(stdout, "[~]sqrt(%g) at %s:%d\n",
		x, (const char *)file, line);
	fflush(stdout);
    }
    return sqrt(x);
}

/*
 * Define sine and cosine functions that take angles in degrees rather than
 * radians, and that are implemented efficiently on machines with slow
 * (or no) floating point.
 */
#if USE_FPU < 0			/****** maybe should be <= 0 ? ***** */

#define sin0 0.00000000000000000
#define sin1 0.01745240643728351
#define sin2 0.03489949670250097
#define sin3 0.05233595624294383
#define sin4 0.06975647374412530
#define sin5 0.08715574274765817
#define sin6 0.10452846326765346
#define sin7 0.12186934340514748
#define sin8 0.13917310096006544
#define sin9 0.15643446504023087
#define sin10 0.17364817766693033
#define sin11 0.19080899537654480
#define sin12 0.20791169081775931
#define sin13 0.22495105434386498
#define sin14 0.24192189559966773
#define sin15 0.25881904510252074
#define sin16 0.27563735581699916
#define sin17 0.29237170472273671
#define sin18 0.30901699437494740
#define sin19 0.32556815445715670
#define sin20 0.34202014332566871
#define sin21 0.35836794954530027
#define sin22 0.37460659341591201
#define sin23 0.39073112848927377
#define sin24 0.40673664307580015
#define sin25 0.42261826174069944
#define sin26 0.43837114678907740
#define sin27 0.45399049973954675
#define sin28 0.46947156278589081
#define sin29 0.48480962024633706
#define sin30 0.50000000000000000
#define sin31 0.51503807491005416
#define sin32 0.52991926423320490
#define sin33 0.54463903501502708
#define sin34 0.55919290347074679
#define sin35 0.57357643635104605
#define sin36 0.58778525229247314
#define sin37 0.60181502315204827
#define sin38 0.61566147532565829
#define sin39 0.62932039104983739
#define sin40 0.64278760968653925
#define sin41 0.65605902899050728
#define sin42 0.66913060635885824
#define sin43 0.68199836006249848
#define sin44 0.69465837045899725
#define sin45 0.70710678118654746
#define sin46 0.71933980033865108
#define sin47 0.73135370161917046
#define sin48 0.74314482547739413
#define sin49 0.75470958022277201
#define sin50 0.76604444311897801
#define sin51 0.77714596145697090
#define sin52 0.78801075360672190
#define sin53 0.79863551004729283
#define sin54 0.80901699437494745
#define sin55 0.81915204428899180
#define sin56 0.82903757255504174
#define sin57 0.83867056794542394
#define sin58 0.84804809615642596
#define sin59 0.85716730070211222
#define sin60 0.86602540378443860
#define sin61 0.87461970713939574
#define sin62 0.88294759285892688
#define sin63 0.89100652418836779
#define sin64 0.89879404629916704
#define sin65 0.90630778703664994
#define sin66 0.91354545764260087
#define sin67 0.92050485345244037
#define sin68 0.92718385456678731
#define sin69 0.93358042649720174
#define sin70 0.93969262078590832
#define sin71 0.94551857559931674
#define sin72 0.95105651629515353
#define sin73 0.95630475596303544
#define sin74 0.96126169593831889
#define sin75 0.96592582628906831
#define sin76 0.97029572627599647
#define sin77 0.97437006478523525
#define sin78 0.97814760073380558
#define sin79 0.98162718344766398
#define sin80 0.98480775301220802
#define sin81 0.98768834059513777
#define sin82 0.99026806874157036
#define sin83 0.99254615164132198
#define sin84 0.99452189536827329
#define sin85 0.99619469809174555
#define sin86 0.99756405025982420
#define sin87 0.99862953475457383
#define sin88 0.99939082701909576
#define sin89 0.99984769515639127
#define sin90 1.00000000000000000

private const double sin_table[361] =
{
    sin0,
    sin1, sin2, sin3, sin4, sin5, sin6, sin7, sin8, sin9, sin10,
    sin11, sin12, sin13, sin14, sin15, sin16, sin17, sin18, sin19, sin20,
    sin21, sin22, sin23, sin24, sin25, sin26, sin27, sin28, sin29, sin30,
    sin31, sin32, sin33, sin34, sin35, sin36, sin37, sin38, sin39, sin40,
    sin41, sin42, sin43, sin44, sin45, sin46, sin47, sin48, sin49, sin50,
    sin51, sin52, sin53, sin54, sin55, sin56, sin57, sin58, sin59, sin60,
    sin61, sin62, sin63, sin64, sin65, sin66, sin67, sin68, sin69, sin70,
    sin71, sin72, sin73, sin74, sin75, sin76, sin77, sin78, sin79, sin80,
    sin81, sin82, sin83, sin84, sin85, sin86, sin87, sin88, sin89, sin90,
    sin89, sin88, sin87, sin86, sin85, sin84, sin83, sin82, sin81, sin80,
    sin79, sin78, sin77, sin76, sin75, sin74, sin73, sin72, sin71, sin70,
    sin69, sin68, sin67, sin66, sin65, sin64, sin63, sin62, sin61, sin60,
    sin59, sin58, sin57, sin56, sin55, sin54, sin53, sin52, sin51, sin50,
    sin49, sin48, sin47, sin46, sin45, sin44, sin43, sin42, sin41, sin40,
    sin39, sin38, sin37, sin36, sin35, sin34, sin33, sin32, sin31, sin30,
    sin29, sin28, sin27, sin26, sin25, sin24, sin23, sin22, sin21, sin20,
    sin19, sin18, sin17, sin16, sin15, sin14, sin13, sin12, sin11, sin10,
    sin9, sin8, sin7, sin6, sin5, sin4, sin3, sin2, sin1, sin0,
    -sin1, -sin2, -sin3, -sin4, -sin5, -sin6, -sin7, -sin8, -sin9, -sin10,
    -sin11, -sin12, -sin13, -sin14, -sin15, -sin16, -sin17, -sin18, -sin19, -sin20,
    -sin21, -sin22, -sin23, -sin24, -sin25, -sin26, -sin27, -sin28, -sin29, -sin30,
    -sin31, -sin32, -sin33, -sin34, -sin35, -sin36, -sin37, -sin38, -sin39, -sin40,
    -sin41, -sin42, -sin43, -sin44, -sin45, -sin46, -sin47, -sin48, -sin49, -sin50,
    -sin51, -sin52, -sin53, -sin54, -sin55, -sin56, -sin57, -sin58, -sin59, -sin60,
    -sin61, -sin62, -sin63, -sin64, -sin65, -sin66, -sin67, -sin68, -sin69, -sin70,
    -sin71, -sin72, -sin73, -sin74, -sin75, -sin76, -sin77, -sin78, -sin79, -sin80,
    -sin81, -sin82, -sin83, -sin84, -sin85, -sin86, -sin87, -sin88, -sin89, -sin90,
    -sin89, -sin88, -sin87, -sin86, -sin85, -sin84, -sin83, -sin82, -sin81, -sin80,
    -sin79, -sin78, -sin77, -sin76, -sin75, -sin74, -sin73, -sin72, -sin71, -sin70,
    -sin69, -sin68, -sin67, -sin66, -sin65, -sin64, -sin63, -sin62, -sin61, -sin60,
    -sin59, -sin58, -sin57, -sin56, -sin55, -sin54, -sin53, -sin52, -sin51, -sin50,
    -sin49, -sin48, -sin47, -sin46, -sin45, -sin44, -sin43, -sin42, -sin41, -sin40,
    -sin39, -sin38, -sin37, -sin36, -sin35, -sin34, -sin33, -sin32, -sin31, -sin30,
    -sin29, -sin28, -sin27, -sin26, -sin25, -sin24, -sin23, -sin22, -sin21, -sin20,
    -sin19, -sin18, -sin17, -sin16, -sin15, -sin14, -sin13, -sin12, -sin11, -sin10,
    -sin9, -sin8, -sin7, -sin6, -sin5, -sin4, -sin3, -sin2, -sin1, -sin0
};

double
gs_sin_degrees(double ang)
{
    int ipart;

    if (is_fneg(ang))
	ang = 180 - ang;
    ipart = (int)ang;
    if (ipart >= 360) {
	int arem = ipart % 360;

	ang -= (ipart - arem);
	ipart = arem;
    }
    return
	(ang == ipart ? sin_table[ipart] :
	 sin_table[ipart] + (sin_table[ipart + 1] - sin_table[ipart]) *
	 (ang - ipart));
}

double
gs_cos_degrees(double ang)
{
    int ipart;

    if (is_fneg(ang))
	ang = 90 - ang;
    else
	ang += 90;
    ipart = (int)ang;
    if (ipart >= 360) {
	int arem = ipart % 360;

	ang -= (ipart - arem);
	ipart = arem;
    }
    return
	(ang == ipart ? sin_table[ipart] :
	 sin_table[ipart] + (sin_table[ipart + 1] - sin_table[ipart]) *
	 (ang - ipart));
}

void
gs_sincos_degrees(double ang, gs_sincos_t * psincos)
{
    psincos->sin = gs_sin_degrees(ang);
    psincos->cos = gs_cos_degrees(ang);
    psincos->orthogonal =
	(is_fzero(psincos->sin) || is_fzero(psincos->cos));
}

#else /* we have floating point */

static const int isincos[5] =
{0, 1, 0, -1, 0};

double
gs_sin_degrees(double ang)
{
    double quot = ang / 90;

    if (floor(quot) == quot) {
	/*
	 * We need 4.0, rather than 4, here because of non-ANSI compilers.
	 * The & 3 is because quot might be negative.
	 */
	return isincos[(int)fmod(quot, 4.0) & 3];
    }
    return sin(ang * (M_PI / 180));
}

double
gs_cos_degrees(double ang)
{
    double quot = ang / 90;

    if (floor(quot) == quot) {
	/* See above re the following line. */
	return isincos[((int)fmod(quot, 4.0) & 3) + 1];
    }
    return cos(ang * (M_PI / 180));
}

void
gs_sincos_degrees(double ang, gs_sincos_t * psincos)
{
    double quot = ang / 90;

    if (floor(quot) == quot) {
	/* See above re the following line. */
	int quads = (int)fmod(quot, 4.0) & 3;

	psincos->sin = isincos[quads];
	psincos->cos = isincos[quads + 1];
	psincos->orthogonal = true;
    } else {
	double arad = ang * (M_PI / 180);

	psincos->sin = sin(arad);
	psincos->cos = cos(arad);
	psincos->orthogonal = false;
    }
}

#endif /* USE_FPU */
