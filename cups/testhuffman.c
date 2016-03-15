/*
 * "$Id: testhuffman.c 11992 2014-07-03 13:54:10Z msweet $"
 *
 * HTTP/2 Huffman compression/decompression unit tests for CUPS.
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

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "http.h"
#include "huffman-private.h"


/*
 * Test data (from HPACK-08)...
 */

typedef struct _http2_huffman_test_s
{
  const char	*s;			/* Literal string */
  unsigned char	h[256];			/* Huffman string */
  size_t	hlen;			/* Length of Huffman string */
} _http2_huffman_test_t;

static const _http2_huffman_test_t test_data[] =
{
  { "www.example.com", { 0xf1, 0xe3, 0xc2, 0xe5, 0xf2, 0x3a, 0x6b, 0xa0, 0xab, 0x90, 0xf4, 0xff }, 12 },
  { "no-cache", { 0xa8, 0xeb, 0x10, 0x64, 0x9c, 0xbf }, 6 },
  { "custom-key", { 0x25, 0xa8, 0x49, 0xe9, 0x5b, 0xa9, 0x7d, 0x7f }, 8 },
  { "custom-value", { 0x25, 0xa8, 0x49, 0xe9, 0x5b, 0xb8, 0xe8, 0xb4, 0xbf }, 9 },
  { "302", { 0x64, 0x02 }, 2 },
  { "private", { 0xae, 0xc3, 0x77, 0x1a, 0x4b }, 5 },
  { "Mon, 21 Oct 2013 20:13:21 GMT", { 0xd0, 0x7a, 0xbe, 0x94, 0x10, 0x54, 0xd4, 0x44, 0xa8, 0x20, 0x05, 0x95, 0x04, 0x0b, 0x81, 0x66, 0xe0, 0x82, 0xa6, 0x2d, 0x1b, 0xff }, 22 },
  { "https://www.example.com", { 0x9d, 0x29, 0xad, 0x17, 0x18, 0x63, 0xc7, 0x8f, 0x0b, 0x97, 0xc8, 0xe9, 0xae, 0x82, 0xae, 0x43, 0xd3 }, 17 },
  { "Mon, 21 Oct 2013 20:13:22 GMT", { 0xd0, 0x7a, 0xbe, 0x94, 0x10, 0x54, 0xd4, 0x44, 0xa8, 0x20, 0x05, 0x95, 0x04, 0x0b, 0x81, 0x66, 0xe0, 0x84, 0xa6, 0x2d, 0x1b, 0xff }, 22 },
  { "gzip", { 0x9b, 0xd9, 0xab }, 3 },
  { "foo=ASDJKHQKBZXOQWEOPIUAXQWEOIU; max-age=3600; version=1", { 0x94, 0xe7, 0x82, 0x1d, 0xd7, 0xf2, 0xe6, 0xc7, 0xb3, 0x35, 0xdf, 0xdf, 0xcd, 0x5b, 0x39, 0x60, 0xd5, 0xaf, 0x27, 0x08, 0x7f, 0x36, 0x72, 0xc1, 0xab, 0x27, 0x0f, 0xb5, 0x29, 0x1f, 0x95, 0x87, 0x31, 0x60, 0x65, 0xc0, 0x03, 0xed, 0x4e, 0xe5, 0xb1, 0x06, 0x3d, 0x50, 0x07 }, 45 }
};


/*
 * Local functions...
 */

static void	printhex(const unsigned char *data, size_t len);


/*
 * 'main()' - Run HTTP/2 Huffman tests.
 */

int					/* O - Exit status */
main(void)
{
  int		i,			/* Looping var */
		status = 0;		/* Status of test */
  char		s[131072];		/* Literal string */
  size_t	slen;			/* Length of literal string */
  unsigned char	huffdata[131072];	/* Huffman encoded data */
  size_t	hufflen;		/* Length of Huffman encoded data */
  unsigned char	data[65536];		/* Test data to simulate Kerberos nonsense */
  char		base64[131072];
					/* Base64 representation of data + "Negotiate " */
  time_t	start, end;		/* Timing information for benchmarks */


 /*
  * Test examples from HPACK-08...
  */

  for (i = 0; i < (int)(sizeof(test_data) / sizeof(test_data[0])); i ++)
  {
    printf("_http2HuffmanEncode(\"%s\"): ", test_data[i].s);
    fflush(stdout);

    hufflen = _http2HuffmanEncode(huffdata, sizeof(huffdata), test_data[i].s);
    if (hufflen != test_data[i].hlen || memcmp(huffdata, test_data[i].h, hufflen))
    {
      puts("FAIL");
      status = 1;
      printf("    Got %d bytes: ", (int)hufflen);
      printhex(huffdata, hufflen);
      printf("    Expected %d bytes: ", (int)test_data[i].hlen);
      printhex(test_data[i].h, test_data[i].hlen);
    }
    else
      puts("PASS");

    printf("_http2HuffmanDecode(\"%s\"): ", test_data[i].s);
    fflush(stdout);

    slen = _http2HuffmanDecode(s, sizeof(s), test_data[i].h, test_data[i].hlen);
    if (slen != strlen(test_data[i].s) || strcmp(s, test_data[i].s))
    {
      puts("FAIL");
      status = 1;
      if (!slen)
	slen = strlen(s);
      printf("    Got %d bytes: %s\n", (int)slen, s);
    }
    else
      puts("PASS");
  }

 /*
  * Test random 64k blob representing a worst-case Kerberos ticket...
  */

  CUPS_SRAND(time(NULL));

  for (i = 0; i < (int)sizeof(data); i ++)
    data[i] = i/* (unsigned char)CUPS_RAND() */;

  strlcpy(base64, "Negotiate ", sizeof(base64));
  httpEncode64_2(base64 + 10, sizeof(base64) - 10, (char *)data, sizeof(data));

  fputs("_http2HuffmanEncode(kerberos ticket): ", stdout);
  fflush(stdout);

  hufflen = _http2HuffmanEncode(huffdata, sizeof(huffdata), base64);
  slen    = strlen(base64);
  printf("PASS (%d bytes, %d%% of original %d bytes)\n", (int)hufflen, (int)(100 * hufflen / slen), (int)slen);

  fputs("_http2HuffmanDecode(kerberos ticket): ", stdout);
  fflush(stdout);

  slen = _http2HuffmanDecode(s, sizeof(s), huffdata, hufflen);
  if (slen != strlen(base64) || strcmp(s, base64))
  {
    const char	*sptr, *bptr;

    puts("FAIL");
    status = 1;

    for (sptr = s, bptr = base64; *sptr && *bptr; sptr ++, bptr ++)
      if (*sptr != *bptr)
        break;

    if (!slen)
      slen = strlen(s);
    printf("    Got %d bytes, expected %d bytes\n", (int)slen, (int)strlen(base64));
    printf("    Difference starting at offset %d: %s\n", (int)(sptr - s), sptr);
    printf("    Expected: %s\n", bptr);
  }
  else
    puts("PASS");

  puts("\nBenchmarks:\n");

#define TESTENCODE 25000
  time(&start);
  for (i = 0; i < TESTENCODE; i ++)
    hufflen = _http2HuffmanEncode(huffdata, sizeof(huffdata), base64);
  time(&end);

  slen = strlen(base64);
  printf("    _http2HuffmanEncode: %.1f MB/second\n", (double)slen * TESTENCODE / (end - start) / 1024 / 1024);

#define TESTDECODE 2500
  time(&start);
  for (i = 0; i < TESTDECODE; i ++)
    _http2HuffmanDecode(s, sizeof(s), huffdata, hufflen);
  time(&end);

  printf("    _http2HuffmanDecode: %.1f MB/second\n", (double)hufflen * TESTDECODE / (end - start) / 1024 / 1024);

  return (status);
}


/*
 * 'printhex()' - Print a string as hex characters.
 */

static void
printhex(const unsigned char *data,	/* I - String */
         size_t              len)	/* I - Length */
{
  while (len > 0)
  {
    printf("%02X", *data & 255);
    data ++;
    len --;
  }

  putchar('\n');
}


/*
 * End of "$Id: testhuffman.c 11992 2014-07-03 13:54:10Z msweet $".
 */
