/*
 * "$Id: ppdx.h 3833 2012-05-23 22:51:18Z msweet $"
 *
 *   Header for PPD data encoding example code.
 *
 *   Copyright 2012 by Apple Inc.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 */

#ifndef _PPDX_H_
#  define _PPDX_H_


/*
 * Include necessary headers...
 */

#  include <cups/ppd.h>


/*
 * C++ magic...
 */

#  ifdef __cplusplus
extern "C" {
#  endif /* __cplusplus */


/*
 * Maximum amount of data to encode/decode...
 */

#  define PPDX_MAX_STATUS	1024	/* Limit on log messages in 10.6 */
#  define PPDX_MAX_DATA		16777216/* 16MiB */


/*
 * 'ppdxReadData()' - Read encoded data from a ppd_file_t *.
 *
 * Reads chunked data in the PPD file "ppd" using the prefix "name".  Returns
 * an allocated pointer to the data (which is nul-terminated for convenience)
 * along with the length of the data in the variable pointed to by "datasize",
 * which can be NULL to indicate the caller doesn't need the length.
 *
 * Returns NULL if no data is present in the PPD with the prefix.
 */

extern void	*ppdxReadData(ppd_file_t *ppd, const char *name,
		              size_t *datasize);


/*
 * 'ppdxWriteData()' - Writes encoded data to stderr using PPD: messages.
 *
 * Writes chunked data to the PPD file using PPD: messages sent to stderr for
 * cupsd.  "name" must be a valid PPD keyword string whose length is less than
 * 37 characters to allow for chunk numbering.  "data" provides a pointer to the
 * data to be written, and "datasize" provides the length.
 */

extern void	ppdxWriteData(const char *name, const void *data,
			      size_t datasize);


#  ifdef __cplusplus
}
#  endif /* __cplusplus */

#endif /* !_PPDX_H */

/*
 * End of "$Id: ppdx.h 3833 2012-05-23 22:51:18Z msweet $".
 */
