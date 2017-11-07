/*
 * Private image library definitions for CUPS.
 *
 * Copyright 2007-2015 by Apple Inc.
 * Copyright 1993-2006 by Easy Software Products.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more information.
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
