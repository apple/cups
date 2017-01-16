/*
 * Private image library definitions for CUPS.
 *
 * Copyright 2007-2015 by Apple Inc.
 * Copyright 1993-2006 by Easy Software Products.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * missing or damaged, see the license at "http://www.cups.org/".
 *
 * This file is subject to the Apple OS-Developed Software exception.
 */

#ifndef _CUPS_RASTER_PRIVATE_H_
#  define _CUPS_RASTER_PRIVATE_H_

/*
 * Include necessary headers...
 */

#  include "raster.h"
#  include <cups/cups.h>
#  include <cups/debug-private.h>
#  include <cups/string-private.h>
#  ifdef WIN32
#    include <io.h>
#    include <winsock2.h>		/* for htonl() definition */
#  else
#    include <unistd.h>
#    include <fcntl.h>
#  endif /* WIN32 */


/*
 * min/max macros...
 */

#  ifndef max
#    define 	max(a,b)	((a) > (b) ? (a) : (b))
#  endif /* !max */
#  ifndef min
#    define 	min(a,b)	((a) < (b) ? (a) : (b))
#  endif /* !min */


/*
 * Prototypes...
 */

extern int		_cupsRasterExecPS(cups_page_header2_t *h,
			                  int *preferred_bits,
			                  const char *code)
			                  __attribute__((nonnull(3)));
extern void		_cupsRasterAddError(const char *f, ...)
			__attribute__((__format__(__printf__, 1, 2)));
extern void		_cupsRasterClearError(void);

#endif /* !_CUPS_RASTER_PRIVATE_H_ */
