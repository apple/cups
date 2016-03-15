/*
 * "$Id: huffman.c 11990 2014-07-02 21:13:22Z msweet $"
 *
 * HTTP/2 Huffman compression/decompression routines for CUPS.
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

/*
 * Include necessary headers...
 */

#include "debug-private.h"
#include "huffman-private.h"
#include "thread-private.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/*
 * Constants...
 */

#define _HTTP2_HUFFMAN_MAX	30	/* Max Huffman bits in table */


/*
 * Huffman table from HPACK-08 draft.
 */

typedef struct _http2_huffman_s		/**** Huffman code table ****/
{
  int		code;			/* Code */
  short		len;			/* Length in bits */
  short		ch;			/* Character */
} _http2_huffman_t;

static _http2_huffman_t http2_decode[256];
					/* Decoder values */
static int		http2_decode_max[_HTTP2_HUFFMAN_MAX + 1];
static const _http2_huffman_t	*http2_decode_next[_HTTP2_HUFFMAN_MAX + 1];
static int		http2_decode_init = 0;
static _cups_mutex_t	http2_decode_mutex = _CUPS_MUTEX_INITIALIZER;
static const _http2_huffman_t http2_encode[256] =
{					/* Encoder values */
  { 0x1ff8,	13,	0x00 },
  { 0x7fffd8,	23,	0x01 },
  { 0xfffffe2,	28,	0x02 },
  { 0xfffffe3,	28,	0x03 },
  { 0xfffffe4,	28,	0x04 },
  { 0xfffffe5,	28,	0x05 },
  { 0xfffffe6,	28,	0x06 },
  { 0xfffffe7,	28,	0x07 },
  { 0xfffffe8,	28,	0x08 },
  { 0xffffea,	24,	0x09 },
  { 0xffffffc,	30,	0x0a },
  { 0xfffffe9,	28,	0x0b },
  { 0xfffffea,	28,	0x0c },
  { 0xffffffd,	30,	0x0d },
  { 0xfffffeb,	28,	0x0e },
  { 0xfffffec,	28,	0x0f },
  { 0xfffffed,	28,	0x10 },
  { 0xfffffee,	28,	0x11 },
  { 0xfffffef,	28,	0x12 },
  { 0xffffff0,	28,	0x13 },
  { 0xffffff1,	28,	0x14 },
  { 0xffffff2,	28,	0x15 },
  { 0xffffffe,	30,	0x16 },
  { 0xffffff3,	28,	0x17 },
  { 0xffffff4,	28,	0x18 },
  { 0xffffff5,	28,	0x19 },
  { 0xffffff6,	28,	0x1a },
  { 0xffffff7,	28,	0x1b },
  { 0xffffff8,	28,	0x1c },
  { 0xffffff9,	28,	0x1d },
  { 0xffffffa,	28,	0x1e },
  { 0xffffffb,	28,	0x1f },
  { 0x14,	6,	0x20 },
  { 0x3f8,	10,	0x21 },
  { 0x3f9,	10,	0x22 },
  { 0xffa,	12,	0x23 },
  { 0x1ff9,	13,	0x24 },
  { 0x15,	6,	0x25 },
  { 0xf8,	8,	0x26 },
  { 0x7fa,	11,	0x27 },
  { 0x3fa,	10,	0x28 },
  { 0x3fb,	10,	0x29 },
  { 0xf9,	8,	0x2a },
  { 0x7fb,	11,	0x2b },
  { 0xfa,	8,	0x2c },
  { 0x16,	6,	0x2d },
  { 0x17,	6,	0x2e },
  { 0x18,	6,	0x2f },
  { 0x0,	5,	0x30 },
  { 0x1,	5,	0x31 },
  { 0x2,	5,	0x32 },
  { 0x19,	6,	0x33 },
  { 0x1a,	6,	0x34 },
  { 0x1b,	6,	0x35 },
  { 0x1c,	6,	0x36 },
  { 0x1d,	6,	0x37 },
  { 0x1e,	6,	0x38 },
  { 0x1f,	6,	0x39 },
  { 0x5c,	7,	0x3a },
  { 0xfb,	8,	0x3b },
  { 0x7ffc,	15,	0x3c },
  { 0x20,	6,	0x3d },
  { 0xffb,	12,	0x3e },
  { 0x3fc,	10,	0x3f },
  { 0x1ffa,	13,	0x40 },
  { 0x21,	6,	0x41 },
  { 0x5d,	7,	0x42 },
  { 0x5e,	7,	0x43 },
  { 0x5f,	7,	0x44 },
  { 0x60,	7,	0x45 },
  { 0x61,	7,	0x46 },
  { 0x62,	7,	0x47 },
  { 0x63,	7,	0x48 },
  { 0x64,	7,	0x49 },
  { 0x65,	7,	0x4a },
  { 0x66,	7,	0x4b },
  { 0x67,	7,	0x4c },
  { 0x68,	7,	0x4d },
  { 0x69,	7,	0x4e },
  { 0x6a,	7,	0x4f },
  { 0x6b,	7,	0x50 },
  { 0x6c,	7,	0x51 },
  { 0x6d,	7,	0x52 },
  { 0x6e,	7,	0x53 },
  { 0x6f,	7,	0x54 },
  { 0x70,	7,	0x55 },
  { 0x71,	7,	0x56 },
  { 0x72,	7,	0x57 },
  { 0xfc,	8,	0x58 },
  { 0x73,	7,	0x59 },
  { 0xfd,	8,	0x5a },
  { 0x1ffb,	13,	0x5b },
  { 0x7fff0,	19,	0x5c },
  { 0x1ffc,	13,	0x5d },
  { 0x3ffc,	14,	0x5e },
  { 0x22,	6,	0x5f },
  { 0x7ffd,	15,	0x60 },
  { 0x3,	5,	0x61 },
  { 0x23,	6,	0x62 },
  { 0x4,	5,	0x63 },
  { 0x24,	6,	0x64 },
  { 0x5,	5,	0x65 },
  { 0x25,	6,	0x66 },
  { 0x26,	6,	0x67 },
  { 0x27,	6,	0x68 },
  { 0x6,	5,	0x69 },
  { 0x74,	7,	0x6a },
  { 0x75,	7,	0x6b },
  { 0x28,	6,	0x6c },
  { 0x29,	6,	0x6d },
  { 0x2a,	6,	0x6e },
  { 0x7,	5,	0x6f },
  { 0x2b,	6,	0x70 },
  { 0x76,	7,	0x71 },
  { 0x2c,	6,	0x72 },
  { 0x8,	5,	0x73 },
  { 0x9,	5,	0x74 },
  { 0x2d,	6,	0x75 },
  { 0x77,	7,	0x76 },
  { 0x78,	7,	0x77 },
  { 0x79,	7,	0x78 },
  { 0x7a,	7,	0x79 },
  { 0x7b,	7,	0x7a },
  { 0x7ffe,	15,	0x7b },
  { 0x7fc,	11,	0x7c },
  { 0x3ffd,	14,	0x7d },
  { 0x1ffd,	13,	0x7e },
  { 0xffffffc,	28,	0x7f },
  { 0xfffe6,	20,	0x80 },
  { 0x3fffd2,	22,	0x81 },
  { 0xfffe7,	20,	0x82 },
  { 0xfffe8,	20,	0x83 },
  { 0x3fffd3,	22,	0x84 },
  { 0x3fffd4,	22,	0x85 },
  { 0x3fffd5,	22,	0x86 },
  { 0x7fffd9,	23,	0x87 },
  { 0x3fffd6,	22,	0x88 },
  { 0x7fffda,	23,	0x89 },
  { 0x7fffdb,	23,	0x8a },
  { 0x7fffdc,	23,	0x8b },
  { 0x7fffdd,	23,	0x8c },
  { 0x7fffde,	23,	0x8d },
  { 0xffffeb,	24,	0x8e },
  { 0x7fffdf,	23,	0x8f },
  { 0xffffec,	24,	0x90 },
  { 0xffffed,	24,	0x91 },
  { 0x3fffd7,	22,	0x92 },
  { 0x7fffe0,	23,	0x93 },
  { 0xffffee,	24,	0x94 },
  { 0x7fffe1,	23,	0x95 },
  { 0x7fffe2,	23,	0x96 },
  { 0x7fffe3,	23,	0x97 },
  { 0x7fffe4,	23,	0x98 },
  { 0x1fffdc,	21,	0x99 },
  { 0x3fffd8,	22,	0x9a },
  { 0x7fffe5,	23,	0x9b },
  { 0x3fffd9,	22,	0x9c },
  { 0x7fffe6,	23,	0x9d },
  { 0x7fffe7,	23,	0x9e },
  { 0xffffef,	24,	0x9f },
  { 0x3fffda,	22,	0xa0 },
  { 0x1fffdd,	21,	0xa1 },
  { 0xfffe9,	20,	0xa2 },
  { 0x3fffdb,	22,	0xa3 },
  { 0x3fffdc,	22,	0xa4 },
  { 0x7fffe8,	23,	0xa5 },
  { 0x7fffe9,	23,	0xa6 },
  { 0x1fffde,	21,	0xa7 },
  { 0x7fffea,	23,	0xa8 },
  { 0x3fffdd,	22,	0xa9 },
  { 0x3fffde,	22,	0xaa },
  { 0xfffff0,	24,	0xab },
  { 0x1fffdf,	21,	0xac },
  { 0x3fffdf,	22,	0xad },
  { 0x7fffeb,	23,	0xae },
  { 0x7fffec,	23,	0xaf },
  { 0x1fffe0,	21,	0xb0 },
  { 0x1fffe1,	21,	0xb1 },
  { 0x3fffe0,	22,	0xb2 },
  { 0x1fffe2,	21,	0xb3 },
  { 0x7fffed,	23,	0xb4 },
  { 0x3fffe1,	22,	0xb5 },
  { 0x7fffee,	23,	0xb6 },
  { 0x7fffef,	23,	0xb7 },
  { 0xfffea,	20,	0xb8 },
  { 0x3fffe2,	22,	0xb9 },
  { 0x3fffe3,	22,	0xba },
  { 0x3fffe4,	22,	0xbb },
  { 0x7ffff0,	23,	0xbc },
  { 0x3fffe5,	22,	0xbd },
  { 0x3fffe6,	22,	0xbe },
  { 0x7ffff1,	23,	0xbf },
  { 0x3ffffe0,	26,	0xc0 },
  { 0x3ffffe1,	26,	0xc1 },
  { 0xfffeb,	20,	0xc2 },
  { 0x7fff1,	19,	0xc3 },
  { 0x3fffe7,	22,	0xc4 },
  { 0x7ffff2,	23,	0xc5 },
  { 0x3fffe8,	22,	0xc6 },
  { 0x1ffffec,	25,	0xc7 },
  { 0x3ffffe2,	26,	0xc8 },
  { 0x3ffffe3,	26,	0xc9 },
  { 0x3ffffe4,	26,	0xca },
  { 0x7ffffde,	27,	0xcb },
  { 0x7ffffdf,	27,	0xcc },
  { 0x3ffffe5,	26,	0xcd },
  { 0xfffff1,	24,	0xce },
  { 0x1ffffed,	25,	0xcf },
  { 0x7fff2,	19,	0xd0 },
  { 0x1fffe3,	21,	0xd1 },
  { 0x3ffffe6,	26,	0xd2 },
  { 0x7ffffe0,	27,	0xd3 },
  { 0x7ffffe1,	27,	0xd4 },
  { 0x3ffffe7,	26,	0xd5 },
  { 0x7ffffe2,	27,	0xd6 },
  { 0xfffff2,	24,	0xd7 },
  { 0x1fffe4,	21,	0xd8 },
  { 0x1fffe5,	21,	0xd9 },
  { 0x3ffffe8,	26,	0xda },
  { 0x3ffffe9,	26,	0xdb },
  { 0xffffffd,	28,	0xdc },
  { 0x7ffffe3,	27,	0xdd },
  { 0x7ffffe4,	27,	0xde },
  { 0x7ffffe5,	27,	0xdf },
  { 0xfffec,	20,	0xe0 },
  { 0xfffff3,	24,	0xe1 },
  { 0xfffed,	20,	0xe2 },
  { 0x1fffe6,	21,	0xe3 },
  { 0x3fffe9,	22,	0xe4 },
  { 0x1fffe7,	21,	0xe5 },
  { 0x1fffe8,	21,	0xe6 },
  { 0x7ffff3,	23,	0xe7 },
  { 0x3fffea,	22,	0xe8 },
  { 0x3fffeb,	22,	0xe9 },
  { 0x1ffffee,	25,	0xea },
  { 0x1ffffef,	25,	0xeb },
  { 0xfffff4,	24,	0xec },
  { 0xfffff5,	24,	0xed },
  { 0x3ffffea,	26,	0xee },
  { 0x7ffff4,	23,	0xef },
  { 0x3ffffeb,	26,	0xf0 },
  { 0x7ffffe6,	27,	0xf1 },
  { 0x3ffffec,	26,	0xf2 },
  { 0x3ffffed,	26,	0xf3 },
  { 0x7ffffe7,	27,	0xf4 },
  { 0x7ffffe8,	27,	0xf5 },
  { 0x7ffffe9,	27,	0xf6 },
  { 0x7ffffea,	27,	0xf7 },
  { 0x7ffffeb,	27,	0xf8 },
  { 0xffffffe,	28,	0xf9 },
  { 0x7ffffec,	27,	0xfa },
  { 0x7ffffed,	27,	0xfb },
  { 0x7ffffee,	27,	0xfc },
  { 0x7ffffef,	27,	0xfd },
  { 0x7fffff0,	27,	0xfe },
  { 0x3ffffee,	26,	0xff }
};
static const unsigned char http2_masks[9] = { 0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff };
					/* Bitmasks */


/*
 * Local functions...
 */

static int	http2_compare_decode(const _http2_huffman_t *a, const _http2_huffman_t *b);


/*
 * '_http2HuffmanDecode()' - Decode (decompress) a HTTP/2 Huffman-encoded string.
 *
 * The "dst" string is nul-terminated even if the total length of the Huffman-
 * encoded string does not fit.  The return value contains the actual length
 * of the string after decoding.
 */

size_t					/* O - Actual length of string or 0 on error */
_http2HuffmanDecode(
    char                *dst,		/* I - Destination string buffer */
    size_t              dstsize,	/* I - Size of string buffer */
    const unsigned char *src,		/* I - Incoming Huffman data */
    size_t              srclen)		/* I - Length of incoming Huffman data */
{
  char			*dstptr,	/* Pointer into string buffer */
			*dstend;	/* End of string buffer */
  const unsigned char	*srcend;	/* End of Huffman data */
  unsigned char		srcbyte;	/* Current source string byte */
  int			srcavail,	/* How many bits are available in the current byte? */
			code,		/* Assembled code */
			len,		/* Length of assembled code */
			bits;		/* Bits to grab */
  const _http2_huffman_t *dptr,		/* Pointer into decoder table */
			*dend;		/* End of decoder table */


  DEBUG_printf(("4_http2HuffmanDecode(dst=%p, dstsize=" CUPS_LLFMT ", src=%p, srclen=" CUPS_LLFMT ")", dst, CUPS_LLCAST dstsize, src, CUPS_LLCAST srclen));

 /*
  * Initialize the decoder array as needed...
  */

  dend = http2_decode + (sizeof(http2_decode) / sizeof(http2_decode[0]));

  if (!http2_decode_init)
  {
    _cupsMutexLock(&http2_decode_mutex);
    if (!http2_decode_init)
    {
      http2_decode_init = 1;
      memcpy(http2_decode, http2_encode, sizeof(http2_decode));
      qsort(http2_decode, sizeof(http2_decode) / sizeof(http2_decode[0]), sizeof(http2_decode[0]), (int (*)(const void *, const void *))http2_compare_decode);

      for (len = 0, dptr = http2_decode; dptr < dend; dptr ++)
      {
        if (len != dptr->len)
        {
          http2_decode_next[len] = dptr;
          len = dptr->len;
        }

        if ((dptr + 1) < dend && dptr[1].len != len)
          http2_decode_max[len] = dptr->code;
      }
    }
    _cupsMutexUnlock(&http2_decode_mutex);
  }

 /*
  * Decode the string.
  *
  * Note: Initial implementation that has very little optimization applied.
  */

  dstptr   = dst;
  dstend   = dst + dstsize - 1;
  srcavail = 0;
  srcbyte  = 0;
  srcend   = src + srclen;

  while (src < srcend || srcavail > 0)
  {
   /*
    * Each Huffman code has a minimum of 5 bits.  We do a linear search of the
    * decode table, which has been sorted in ascending order for length and
    * code.  If we don't find the code in the table we return an error.
    */

    code = 0;
    len  = 0;
    dptr = http2_decode;

    DEBUG_printf(("5_http2HuffmanDecode: init srcbyte=%02x, srcavail=%d", srcbyte, srcavail));

    while (dptr < dend)
    {
      while (len < dptr->len)
      {
       /*
        * Get N more bits from the input...
        */

        if (srcavail == 0)
        {
          if (src < srcend)
          {
	    srcbyte  = *src++;
	    srcavail = 8;

	    DEBUG_printf(("5_http2HuffmanDecode: cont srcbyte=%02x, srcavail=%d", srcbyte, srcavail));
	  }
	  else if (len < 8 && code == http2_masks[len])
	    break;
	  else
	  {
	    DEBUG_puts("5_http2HuffmanDecode: Early end-of-string.");
	    return (0);
	  }
        }

        if ((bits = dptr->len - len) > srcavail)
          bits = srcavail;

        DEBUG_printf(("5_http2HuffmanDecode: Pulling %d bits", bits));

        if (bits == srcavail)
        {
	  if (len == 0)
	  {
	    if (bits == 8)
	      code = srcbyte;
	    else
	      code = srcbyte & http2_masks[bits];
	  }
	  else
	    code = (code << bits) | (srcbyte & http2_masks[bits]);

          srcavail = 0;
        }
        else if (len == 0)
        {
          code     = (srcbyte >> (srcavail - bits)) & http2_masks[bits];
          srcavail -= bits;
        }
        else
        {
          code     = (code << bits) | ((srcbyte >> (srcavail - bits)) & http2_masks[bits]);
          srcavail -= bits;
        }

	len += bits;

#ifdef DEBUG
        if (len < dptr->len)
          DEBUG_printf(("5_http2HuffmanDecode: code=%x, len=%d, srcavail=%d", code, len, srcavail));
#endif /* DEBUG */
      }

      DEBUG_printf(("5_http2HuffmanDecode: code=%x, len=%d, dptr->len=%d", code, len, dptr->len));

      if (len < dptr->len)
        break;

      if (code > http2_decode_max[len])
      {
        dptr = http2_decode_next[len];
        continue;
      }

      while (len == dptr->len)
        if (dptr->code == code)
          break;
        else
          dptr ++;

      if (dptr->code == code && dptr->len == len)
      {
        DEBUG_printf(("5_http2HuffmanDecode: code=%x, len=%d, match='%c' (0x%02x)", (unsigned)code, len, dptr->ch, dptr->ch));

        if (dstptr < dstend)
          *dstptr = (char)dptr->ch;

        dstptr ++;
        break;
      }
#ifdef DEBUG
      else
        DEBUG_printf(("5_http2HuffmanDecode: code=%x, len=%d, no match", (unsigned)code, len));
#endif /* DEBUG */
    }
  }

  if (dstptr < dstend)
    *dstptr = '\0';
  else
    *dstend = '\0';

  return ((size_t)(dstptr - dst));
}


/*
 * '_http2HuffmanEncode()' - Encode (compress) a string using HTTP/2 Huffman-coding.
 *
 * The return value contains the actual length of the string after encoding.
 */

size_t					/* O - Number of bytes used for Huffman */
_http2HuffmanEncode(
    unsigned char *dst,			/* I - Output buffer */
    size_t        dstsize,		/* I - Size of output buffer */
    const char    *src)			/* I - String to encode */
{
  unsigned char	*dstptr,		/* Pointer into buffer */
		*dstend,		/* End of buffer */
		dstbyte;		/* Current output byte */
  int		dstused,		/* Current bits used */
		dstremaining;		/* Remaining bits */
  int		ch;			/* Current character */
  int		code,			/* Huffman code */
		len;			/* Length of Huffman code */


 /*
  * Note: Initial implementation that has very little optimization applied.
  */

  dstptr  = dst;
  dstend  = dst + dstsize;
  dstbyte = 0;
  dstused = 0;

  while (*src)
  {
    ch   = *src++ & 255;
    code = http2_encode[ch].code;
    len  = http2_encode[ch].len;

    while (len > 0)
    {
      if (dstused == 0)
      {
        if (len == 8)
        {
          dstbyte = (unsigned char)code;
          dstused = 8;
        }
        else if (len > 8)
        {
          dstbyte = (unsigned char)(code >> (len - 8));
          dstused = 8;
        }
        else
        {
          dstbyte = (unsigned char)(code << (8 - len));
          dstused = len;
        }

        len -= dstused;
      }
      else
      {
        dstremaining = 8 - dstused;

        if (len == dstremaining)
        {
          dstbyte |= (unsigned char)(code & http2_masks[dstremaining]);
          dstused = 8;
          len     = 0;
        }
        else if (len > dstremaining)
        {
          dstbyte |= (unsigned char)((code >> (len - dstremaining)) & http2_masks[dstremaining]);
          dstused = 8;
          len -= dstremaining;
        }
        else
        {
          dstbyte |= (unsigned char)((code << (dstremaining - len)) & http2_masks[dstremaining]);
          dstused += len;
          len = 0;
        }
      }

      if (dstused == 8)
      {
       /*
        * "Write" a byte to the output buffer
        */

        if (dstptr < dstend)
          *dstptr = dstbyte;

        dstptr ++;
        dstused = 0;
      }
    }
  }

  if (dstused)
  {
   /*
    * Pad the output string with 1's as an End-Of-String code...
    */

    dstremaining = 8 - dstused;
    dstbyte |= http2_masks[dstremaining];
    if (dstptr < dstend)
      *dstptr = dstbyte;
    dstptr ++;
  }

  return ((size_t)(dstptr - dst));
}


/*
 * 'http2_compare_decode()' - Compare two Huffman codes for decoding.
 */

static int				/* O - Result of comparison */
http2_compare_decode(
    const _http2_huffman_t *a,		/* I - First code */
    const _http2_huffman_t *b)		/* I - Second code */
{
  int	result;				/* Result of comparison */


  if ((result = a->len - b->len) == 0)
    result = a->code - b->code;

  return (result);
}


/*
 * End of "$Id: huffman.c 11990 2014-07-02 21:13:22Z msweet $".
 */
