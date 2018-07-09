/*
 * Private MIME type/conversion database definitions for CUPS.
 *
 * Copyright © 2011-2018 by Apple Inc.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more
 * information.
 */

#ifndef _CUPS_MIME_PRIVATE_H_
#  define _CUPS_MIME_PRIVATE_H_

#  include "mime.h"


/*
 * C++ magic...
 */

#  ifdef __cplusplus
extern "C" {
#  endif /* __cplusplus */


/*
 * Prototypes...
 */

extern void	_mimeError(mime_t *mime, const char *format, ...) _CUPS_FORMAT(2, 3);


#  ifdef __cplusplus
}
#  endif /* __cplusplus */
#endif /* !_CUPS_MIME_PRIVATE_H_ */
