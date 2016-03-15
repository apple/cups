/*
 * "$Id: huffman-private.h 11985 2014-07-02 15:41:16Z msweet $"
 *
 * HTTP/2 Huffman compression/decompression definitions for CUPS.
 *
 * Copyright 2014 by Apple Inc.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * This file is subject to the Apple OS-Developed Software exception.
 */

#ifndef _CUPS_HUFFMAN_PRIVATE_H_
#  define _CUPS_HUFFMAN_PRIVATE_H_

/*
 * Include necessary headers...
 */

#  include "versioning.h"
#  include <stdlib.h>


/*
 * C++ magic...
 */

#  ifdef __cplusplus
extern "C" {
#  endif /* __cplusplus */


/*
 * Functions...
 */

extern size_t	_http2HuffmanDecode(char *dst, size_t dstsize, const unsigned char *src, size_t srclen);
extern size_t	_http2HuffmanEncode(unsigned char *dst, size_t dstsize, const char *src);


/*
 * C++ magic...
 */

#  ifdef __cplusplus
}
#  endif /* __cplusplus */
#endif /* !_CUPS_HUFFMAN_PRIVATE_H_ */

/*
 * End of "$Id: huffman-private.h 11985 2014-07-02 15:41:16Z msweet $".
 */
