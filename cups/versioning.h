/*
 * "$Id$"
 *
 *   API versioning definitions for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007 by Apple Inc.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 */

#ifndef _CUPS_VERSIONING_H_
#  define _CUPS_VERSIONING_H_

/*
 * This header defines several constants - _CUPS_DEPRECATED,
 * _CUPS_API_1_1, _CUPS_API_1_1_19, _CUPS_API_1_1_20, _CUPS_API_1_1_21,
 * _CUPS_API_1_2, _CUPS_API_1_3, _CUPS_API_1_4 - which add compiler-
 * specific attributes that flag functions that are deprecated or added
 * in particular releases.  On Mac OS X, the _CUPS_API_* constants are
 * defined based on the value of the MAC_OS_X_VERSION_MAX_ALLOWED constant
 * provided by the compiler.
 */

#  if defined(__APPLE__) && !defined(_CUPS_SOURCE)
#    if MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_3
#      define _CUPS_API_1_1_19 __attribute__((unavailable))
#      define _CUPS_API_1_1_20 __attribute__((unavailable))
#      define _CUPS_API_1_1_21 __attribute__((unavailable))
#      define _CUPS_API_1_2 __attribute__((unavailable))
#      define _CUPS_API_1_3 __attribute__((unavailable))
#      define _CUPS_API_1_4 __attribute__((unavailable))
#    elif MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_4
#      define _CUPS_API_1_1_19
#      define _CUPS_API_1_1_20 __attribute__((unavailable))
#      define _CUPS_API_1_1_21 __attribute__((unavailable))
#      define _CUPS_API_1_2 __attribute__((unavailable))
#      define _CUPS_API_1_3 __attribute__((unavailable))
#      define _CUPS_API_1_4 __attribute__((unavailable))
#    elif MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_5
#      define _CUPS_API_1_1_19
#      define _CUPS_API_1_1_20
#      define _CUPS_API_1_1_21
#      define _CUPS_API_1_2 __attribute__((unavailable))
#      define _CUPS_API_1_3 __attribute__((unavailable))
#      define _CUPS_API_1_4 __attribute__((unavailable))
#    elif MAC_OS_X_VERSION_MAX_ALLOWED == MAC_OS_X_VERSION_10_5
#      define _CUPS_API_1_1_19
#      define _CUPS_API_1_1_20
#      define _CUPS_API_1_1_21
#      define _CUPS_API_1_2
#      define _CUPS_API_1_3
#      define _CUPS_API_1_4 __attribute__((unavailable))
#    else
#      define _CUPS_API_1_1_19
#      define _CUPS_API_1_1_20
#      define _CUPS_API_1_1_21
#      define _CUPS_API_1_2
#      define _CUPS_API_1_3
#      define _CUPS_API_1_4
#    endif /* MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_x */
#  else
#    define _CUPS_API_1_1_19
#    define _CUPS_API_1_1_20
#    define _CUPS_API_1_1_21
#    define _CUPS_API_1_2
#    define _CUPS_API_1_3
#    define _CUPS_API_1_4
#  endif /* __APPLE__ */

/*
 * With GCC 3.0 and higher, we can mark old APIs "deprecated" so you get
 * a warning at compile-time.
 */

#  if defined(__GNUC__) && __GNUC__ > 2
#    define _CUPS_DEPRECATED __attribute__ ((__deprecated__))
#  else
#    define _CUPS_DEPRECATED
#  endif /* __GNUC__ && __GNUC__ > 2 */

#endif /* !_CUPS_VERSIONING_H_ */

/*
 * End of "$Id$".
 */
