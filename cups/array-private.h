/*
 * "$Id$"
 *
 *   Private array definitions for CUPS.
 *
 *   Copyright 2011-2012 by Apple Inc.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 */

#ifndef _CUPS_ARRAY_PRIVATE_H_
#  define _CUPS_ARRAY_PRIVATE_H_

/*
 * Include necessary headers...
 */

#  include <cups/array.h>


/*
 * C++ magic...
 */

#  ifdef __cplusplus
extern "C" {
#  endif /* __cplusplus */


/*
 * Functions...
 */

extern int		_cupsArrayAddStrings(cups_array_t *a, const char *s,
			                     char delim) _CUPS_API_1_5;
extern cups_array_t	*_cupsArrayNewStrings(const char *s, char delim)
			                      _CUPS_API_1_5;

#  ifdef __cplusplus
}
#  endif /* __cplusplus */
#endif /* !_CUPS_ARRAY_PRIVATE_H_ */

/*
 * End of "$Id$".
 */
