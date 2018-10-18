/*
 * Private debugging APIs for CUPS.
 *
 * Copyright © 2007-2018 by Apple Inc.
 * Copyright © 1997-2005 by Easy Software Products.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more
 * information.
 */

#ifndef _CUPS_DEBUG_PRIVATE_H_
#  define _CUPS_DEBUG_PRIVATE_H_


/*
 * Include necessary headers...
 */

#  include <cups/versioning.h>


/*
 * C++ magic...
 */

#  ifdef __cplusplus
extern "C" {
#  endif /* __cplusplus */


/*
 * The debug macros are used if you compile with DEBUG defined.
 *
 * Usage:
 *
 *   DEBUG_set("logfile", "level", "filter", 1)
 *
 * The DEBUG_set macro allows an application to programmatically enable (or
 * disable) debug logging.  The arguments correspond to the CUPS_DEBUG_LOG,
 * CUPS_DEBUG_LEVEL, and CUPS_DEBUG_FILTER environment variables.  The 1 on the
 * end forces the values to override the environment.
 */

#  ifdef DEBUG
#    define DEBUG_set(logfile,level,filter) _cups_debug_set(logfile,level,filter,1)
#  else
#    define DEBUG_set(logfile,level,filter)
#  endif /* DEBUG */


/*
 * Prototypes...
 */

extern void	_cups_debug_set(const char *logfile, const char *level, const char *filter, int force) _CUPS_PRIVATE;
#  ifdef _WIN32
extern int	_cups_gettimeofday(struct timeval *tv, void *tz) _CUPS_PRIVATE;
#    define gettimeofday(a,b) _cups_gettimeofday(a, b)
#  endif /* _WIN32 */

#  ifdef __cplusplus
}
#  endif /* __cplusplus */

#endif /* !_CUPS_DEBUG_PRIVATE_H_ */
