/*
  Copyright 2001 by Easy Software Products.
  Copyright 1993, 1994, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: stdpre.h,v 1.2.2.1 2001/05/13 18:38:32 mike Exp $ */
/* Standard definitions for Aladdin Enterprises code not needing arch.h */

#ifndef stdpre_INCLUDED
#  define stdpre_INCLUDED

/*
 * Here we deal with the vagaries of various C compilers.  We assume that:
 *      ANSI-standard Unix compilers define __STDC__.
 *      gcc defines __GNUC__.
 *      Borland Turbo C and Turbo C++ define __MSDOS__ and __TURBOC__.
 *      Borland C++ defines __BORLANDC__, __MSDOS__, and __TURBOC__.
 *      Microsoft C/C++ defines _MSC_VER and _MSDOS.
 *      Watcom C defines __WATCOMC__ and MSDOS.
 *      MetroWerks C defines __MWERKS__.
 *
 * We arrange to define __MSDOS__ on all the MS-DOS platforms.
 */
#if (defined(MSDOS) || defined(_MSDOS)) && !defined(__MSDOS__)
#  define __MSDOS__
#endif
/*
 * Also, not used much here, but used in other header files, we assume:
 *      Unix System V environments define SYSV.
 *      The SCO ODT compiler defines M_SYSV and M_SYS3.
 *      VMS systems define VMS.
 *      OSF/1 compilers define __osf__ or __OSF__.
 *        (The VMS and OSF/1 C compilers handle prototypes and const,
 *        but do not define __STDC__.)
 *      bsd 4.2 or 4.3 systems define BSD4_2.
 *      POSIX-compliant environments define _POSIX_SOURCE.
 *      Motorola 88K BCS/OCS systems defined m88k.
 *
 * We make fairly heroic efforts to confine all uses of these flags to
 * header files, and never to use them in code.
 */
#if defined(__osf__) && !defined(__OSF__)
#  define __OSF__		/* */
#endif
#if defined(M_SYSV) && !defined(SYSV)
#  define SYSV			/* */
#endif
#if defined(M_SYS3) && !defined(__SVR3)
#  define __SVR3		/* */
#endif

#if defined(__STDC__) || defined(__MSDOS__) || defined(__convex__) || defined(VMS) || defined(__OSF__) || defined(__WIN32__) || defined(__IBMC__) || defined(M_UNIX) || defined(__GNUC__) || defined(__BORLANDC__)
# if !(defined(M_XENIX) && !defined(__GNUC__))	/* SCO Xenix cc is broken */
#  define __PROTOTYPES__	/* */
# endif
#endif

/* Define dummy values for __FILE__ and __LINE__ if the compiler */
/* doesn't provide these.  Note that places that use __FILE__ */
/* must check explicitly for a null pointer. */
#ifndef __FILE__
#  define __FILE__ NULL
#endif
#ifndef __LINE__
#  define __LINE__ 0
#endif

/* Disable 'const' and 'volatile' if the compiler can't handle them. */
#ifndef __PROTOTYPES__
#  undef const
#  define const			/* */
#  undef volatile
#  define volatile		/* */
#endif

/* Disable 'inline' if the compiler can't handle it. */
#ifdef __DECC
#  undef inline
#  define inline __inline
#else
#  if !(defined(__GNUC__) || defined(__MWERKS__) || defined(inline))
#    define inline		/* */
#  endif
#endif

/*
 * Some compilers give a warning if a function call that returns a value
 * is used as a statement; a few compilers give an error for the construct
 * (void)0, which is contrary to the ANSI standard.  Since we don't know of
 * any compilers that do both, we define a macro here for discarding
 * the value of an expression statement, which can be defined as either
 * including or not including the cast.  (We don't conditionalize this here,
 * because no commercial compiler gives the error on (void)0, although
 * some give warnings.)
 */
#define discard(expr) ((void)(expr))

/*
 * The SVR4.2 C compiler incorrectly considers the result of << and >>
 * to be unsigned if the left operand is signed and the right operand is
 * unsigned.  We believe this only causes trouble in Ghostscript code when
 * the right operand is a sizeof(...), which is unsigned for this compiler.
 * Therefore, we replace the relevant uses of sizeof with size_of:
 */
#define size_of(x) ((int)(sizeof(x)))

/*
 * Disable MS-DOS specialized pointer types on non-MS-DOS systems.
 * Watcom C defines near, far, and huge as macros, so we must undef them.
 * far_data is used for static data that must get its own segment.
 * This is supported in Borland C++, but none of the others.
 */
#undef far_data
#if defined(__TURBOC__) && !defined(__WIN32__)
#  ifdef __BORLANDC__
#    define far_data far
#  else
#    define far_data		/* */
#  endif
#else
#  undef near
#  define near			/* */
#  undef far
#  define far			/* */
#  define far_data		/* */
#  undef huge
#  define huge			/* */
#  define _cs			/* */
#  define _ds			/* */
/* _es is never safe to use */
#  define _ss			/* */
#endif

/* Get the size of a statically declared array. */
#define countof(a) (sizeof(a) / sizeof((a)[0]))
#define count_of(a) (size_of(a) / size_of((a)[0]))

/*
 * Get the offset of a structure member.  Amazingly enough, the simpler
 * definition works on all compilers except for one broken MIPS compiler
 * and the IBM RS/6000.  Unfortunately, because of these two compilers,
 * we have to use the more complex definition.  Even more unfortunately,
 * the more complex definition doesn't work on the MetroWerks
 * CodeWarrior compiler (Macintosh and BeOS).
 */
#ifdef __MWERKS__
#define offset_of(type, memb)\
 ((int) &((type *) 0)->memb)
#else
#define offset_of(type, memb)\
 ((int) ( (char *)&((type *)0)->memb - (char *)((type *)0) ))
#endif

/*
 * Get the alignment of a pointer modulo a given power of 2.
 * There is no portable way to do this, but the following definition
 * works on all reasonable systems.
 */
#define alignment_mod(ptr, modu)\
  ((uint)( ((const char *)(ptr) - (const char *)0) & ((modu) - 1) ))

/* Define short names for the unsigned types. */
typedef unsigned char byte;
typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long ulong;

/* Since sys/types.h often defines one or more of these (depending on */
/* the platform), we have to take steps to prevent name clashes. */
/*** NOTE: This requires that you include std.h *before* any other ***/
/*** header file that includes sys/types.h. ***/
#define bool bool_		/* (maybe not needed) */
#define uchar uchar_
#define uint uint_
#define ushort ushort_
#define ulong ulong_
#include <sys/types.h>
#undef bool
#undef uchar
#undef uint
#undef ushort
#undef ulong

/*
 * Define a Boolean type.  Even though we would like it to be
 * unsigned char, it pretty well has to be int, because
 * that's what all the relational operators and && and || produce.
 * We can't make it an enumerated type, because ints don't coerce
 * freely to enums (although the opposite is true).
 * Unfortunately, at least some C++ compilers have a built-in bool type,
 * and the MetroWerks C++ compiler insists that bool be equivalent to
 * unsigned char.
 */
#ifndef __cplusplus
#ifdef __BEOS__
typedef unsigned char bool;

#else
typedef int bool;

#endif
#endif
/*
 * MetroWerks CodeWarrior predefines true and false, probably as 1 and 0.
 * We need to cancel those definitions for our own code.
 */
#undef false
#define false ((bool)0)
#undef true
#define true ((bool)1)

/*
 * Compilers disagree as to whether macros used in macro arguments
 * should be expanded at the time of the call, or at the time of
 * final expansion.  Even authoritative documents disagree: the ANSI
 * standard says the former, but Harbison and Steele's book says the latter.
 * In order to work around this discrepancy, we have to do some very
 * ugly things in a couple of places.  We mention it here because
 * it might well trip up future developers.
 */

/*
 * Define the type to be used for ordering pointers (<, >=, etc.).
 * The Borland and Microsoft large models only compare the offset part
 * of segmented pointers.  Semantically, the right type to use for the
 * comparison is char huge *, but we have no idea how expensive comparing
 * such pointers is, and any type that compares all the bits of the pointer,
 * gives the right result for pointers in the same segment, and keeps
 * different segments disjoint will do.
 */
#if defined(__TURBOC__) || defined(_MSC_VER)
typedef unsigned long ptr_ord_t;

#else
typedef const char *ptr_ord_t;

#endif
/* Define all the pointer comparison operations. */
#define _ptr_cmp(p1, rel, p2)  ((ptr_ord_t)(p1) rel (ptr_ord_t)(p2))
#define ptr_le(p1, p2) _ptr_cmp(p1, <=, p2)
#define ptr_lt(p1, p2) _ptr_cmp(p1, <, p2)
#define ptr_ge(p1, p2) _ptr_cmp(p1, >=, p2)
#define ptr_gt(p1, p2) _ptr_cmp(p1, >, p2)
#define ptr_between(ptr, lo, hi)\
  (ptr_ge(ptr, lo) && ptr_lt(ptr, hi))

/* Define  min and max, but make sure to use the identical definition */
/* to the one that all the compilers seem to have.... */
#ifndef min
#  define min(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#  define max(a, b) (((a) > (b)) ? (a) : (b))
#endif

/* Define a standard way to round values to a (constant) modulus. */
#define round_down(value, modulus)\
  ( (modulus) & ((modulus) - 1) ?	/* not a power of 2 */\
    (value) - (value) % (modulus) :\
    (value) & -(modulus) )
#define round_up(value, modulus)\
  ( (modulus) & ((modulus) - 1) ?	/* not a power of 2 */\
    ((value) + ((modulus) - 1)) / (modulus) * (modulus) :\
    ((value) + ((modulus) - 1)) & -(modulus) )

/*
 * In pre-ANSI C, float parameters get converted to double.
 * However, if we pass a float to a function that has been declared
 * with a prototype, and the parameter has been declared as float,
 * the ANSI standard specifies that the parameter is left as float.
 * To avoid problems caused by missing prototypes,
 * we declare almost all float parameters as double.
 */
typedef double floatp;

/*
 * Because of C's strange insistence that ; is a terminator and not a
 * separator, compound statements {...} are not syntactically equivalent to
 * single statements.  Therefore, we define here a compound-statement
 * construct that *is* syntactically equivalent to a single statement.
 * Usage is
 *      BEGIN
 *        ...statements...
 *      END
 */
/*** MRS: If you didn't treat C like a macro language, maybe this wouldn't
 ***      be a problem; changed to { and } (which do work) to avoid
 ***      needless compiler warnings...
 ***/
#define BEGIN	{
#define END	}

/*
 * Define a handy macro for a statement that does nothing.
 * We can't just use an empty statement, since this upsets some compilers.
 */
/*** MRS: Fix DO_NOTHING to use a void statement.  Again, not using
 ***      macros makes life easier...
 ***/
#ifndef DO_NOTHING
#  define DO_NOTHING (void)stdout
#endif

/*
 * For accountability, debugging, and error messages,
 * we pass a client identification string to alloc and free,
 * and possibly other places as well. Define the type for these strings.
 * The definition used to have a _ds modifier, so we had to coerce
 * them when passing them to printf at all; this is no longer needed.
 */
typedef const char *client_name_t;

#define client_name_string(cname) (cname)

/*
 * If we are debugging, make all static variables and procedures public
 * so they get passed through the linker.
 */
#define public			/* */
/*
 * We separate out the definition of private this way so that
 * we can temporarily #undef it to handle the X Windows headers,
 * which define a member named private.
 */
#ifdef NOPRIVATE
# define private_		/* */
#else
# define private_ static
#endif
#define private private_

/*
 * Macros for argument templates.  ANSI C has these, as does Turbo C,
 * but older pcc-derived (K&R) Unix compilers don't.  The syntax is
 *      resulttype func(Pn(arg1, ..., argn));
 */

#ifdef __PROTOTYPES__
# define P0() void
# define P1(t1) t1
# define P2(t1,t2) t1,t2
# define P3(t1,t2,t3) t1,t2,t3
# define P4(t1,t2,t3,t4) t1,t2,t3,t4
# define P5(t1,t2,t3,t4,t5) t1,t2,t3,t4,t5
# define P6(t1,t2,t3,t4,t5,t6) t1,t2,t3,t4,t5,t6
# define P7(t1,t2,t3,t4,t5,t6,t7) t1,t2,t3,t4,t5,t6,t7
# define P8(t1,t2,t3,t4,t5,t6,t7,t8) t1,t2,t3,t4,t5,t6,t7,t8
# define P9(t1,t2,t3,t4,t5,t6,t7,t8,t9) t1,t2,t3,t4,t5,t6,t7,t8,t9
# define P10(t1,t2,t3,t4,t5,t6,t7,t8,t9,t10) t1,t2,t3,t4,t5,t6,t7,t8,t9,t10
# define P11(t1,t2,t3,t4,t5,t6,t7,t8,t9,t10,t11) t1,t2,t3,t4,t5,t6,t7,t8,t9,t10,t11
# define P12(t1,t2,t3,t4,t5,t6,t7,t8,t9,t10,t11,t12) t1,t2,t3,t4,t5,t6,t7,t8,t9,t10,t11,t12
# define P13(t1,t2,t3,t4,t5,t6,t7,t8,t9,t10,t11,t12,t13) t1,t2,t3,t4,t5,t6,t7,t8,t9,t10,t11,t12,t13
# define P14(t1,t2,t3,t4,t5,t6,t7,t8,t9,t10,t11,t12,t13,t14) t1,t2,t3,t4,t5,t6,t7,t8,t9,t10,t11,t12,t13,t14
# define P15(t1,t2,t3,t4,t5,t6,t7,t8,t9,t10,t11,t12,t13,t14,t15) t1,t2,t3,t4,t5,t6,t7,t8,t9,t10,t11,t12,t13,t14,t15
# define P16(t1,t2,t3,t4,t5,t6,t7,t8,t9,t10,t11,t12,t13,t14,t15,t16) t1,t2,t3,t4,t5,t6,t7,t8,t9,t10,t11,t12,t13,t14,t15,t16
#else
# define P0()			/* */
# define P1(t1)			/* */
# define P2(t1,t2)		/* */
# define P3(t1,t2,t3)		/* */
# define P4(t1,t2,t3,t4)	/* */
# define P5(t1,t2,t3,t4,t5)	/* */
# define P6(t1,t2,t3,t4,t5,t6)	/* */
# define P7(t1,t2,t3,t4,t5,t6,t7)	/* */
# define P8(t1,t2,t3,t4,t5,t6,t7,t8)	/* */
# define P9(t1,t2,t3,t4,t5,t6,t7,t8,t9)		/* */
# define P10(t1,t2,t3,t4,t5,t6,t7,t8,t9,t10)	/* */
# define P11(t1,t2,t3,t4,t5,t6,t7,t8,t9,t10,t11)	/* */
# define P12(t1,t2,t3,t4,t5,t6,t7,t8,t9,t10,t11,t12)	/* */
# define P13(t1,t2,t3,t4,t5,t6,t7,t8,t9,t10,t11,t12,t13)	/* */
# define P14(t1,t2,t3,t4,t5,t6,t7,t8,t9,t10,t11,t12,t13,t14)	/* */
# define P15(t1,t2,t3,t4,t5,t6,t7,t8,t9,t10,t11,t12,t13,t14,t15)	/* */
# define P16(t1,t2,t3,t4,t5,t6,t7,t8,t9,t10,t11,t12,t13,t14,t15,t16)	/* */
#endif

/* Define success and failure codes for 'exit'. */
#ifdef VMS
#  define exit_OK 1
#  define exit_FAILED 18
#else
#  define exit_OK 0
#  define exit_FAILED 1
#endif
/*
 * Define the informational exit status.
 * We need to distinguish information returns because under MS Windows,
 * they must return like an error so that the text window stays on the
 * screen, while on other platforms, they must return successfully.
 * Note that we define both gs_exit_INFO (before platform-specific
 * mapping of 0 to exit_OK and 1 to exit_FAILED) and exit_INFO.
 */
#if defined(_WINDOWS) || defined(_Windows)
#  define exit_INFO exit_FAILED
#  define gs_exit_INFO 1
#else
#  define exit_INFO exit_OK
#  define gs_exit_INFO 0
#endif

#endif /* stdpre_INCLUDED */
