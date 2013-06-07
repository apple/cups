/*
 * "$Id: ppdx.c 3833 2012-05-23 22:51:18Z msweet $"
 *
 *   Example code for encoding and decoding large amounts of data in a PPD file.
 *   This would typically be used in a driver to save configuration/state
 *   information that could be used by an application.
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
 * Contents:
 *
 *   ppdxReadData()  - Read encoded data from a ppd_file_t *.
 *   ppdxWriteData() - Writes encoded data to stderr using PPD: messages.
 */

/*
 * Include necessary headers...
 */

#include "ppdx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>			/* For compression of the data */


/*
 * Constants...
 */

#define PPDX_MAX_VALUE	(PPD_MAX_LINE - PPD_MAX_NAME - 4)
					/* Max value length with delimiters + nul */
#define PPDX_MAX_CHUNK	(PPDX_MAX_VALUE * 3 / 4)
					/* Max length of each chunk when Base64-encoded */


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

void *					/* O - Data or NULL */
ppdxReadData(ppd_file_t *ppd,		/* I - PPD file */
             const char *name,		/* I - Keyword prefix */
             size_t     *datasize)	/* O - Size of data or NULL for don't care */
{
  char		keyword[PPD_MAX_NAME],	/* Keyword name */
		decoded[PPDX_MAX_CHUNK + 1];
					/* Decoded string */
  unsigned	chunk = 0;		/* Current chunk number */
  int		len;			/* Length of current chunk */
  ppd_attr_t	*attr;			/* Keyword/value from PPD file */
  Bytef		*data;			/* Pointer to data */
  size_t	alloc_size;		/* Allocated size of data buffer */
  z_stream	decomp;			/* Decompressor stream */
  int		error;			/* Error/status from inflate() */


 /*
  * Range check input...
  */

  if (datasize)
    *datasize = 0;

  if (!ppd || !name)
    return (NULL);

 /*
  * First see if there are any instances of the named keyword in the PPD...
  */

  snprintf(keyword, sizeof(keyword), "%s%04x", name, chunk);
  if ((attr = ppdFindAttr(ppd, keyword, NULL)) == NULL)
    return (NULL);

 /*
  * Allocate some memory and start decoding...
  */

  data       = malloc(257);
  alloc_size = 256;

  memset(&decomp, 0, sizeof(decomp));
  decomp.next_out  = data;
  decomp.avail_out = 256;

  inflateInit(&decomp);

  do
  {
   /*
    * Grab the data from the current attribute and decode it...
    */

    len = sizeof(decoded);
    if (!httpDecode64_2(decoded, &len, attr->value) || len == 0)
      break;

//    printf("chunk %04x has length %d\n", chunk, len);

   /*
    * Decompress this chunk...
    */

    decomp.next_in  = decoded;
    decomp.avail_in = len;

    do
    {
      Bytef	*temp;			/* Temporary pointer */
      size_t	temp_size;		/* Temporary allocation size */

//      printf("Before inflate: avail_in=%d, avail_out=%d\n", decomp.avail_in,
//             decomp.avail_out);

      if ((error = inflate(&decomp, Z_NO_FLUSH)) < Z_OK)
      {
        fprintf(stderr, "ERROR: inflate returned %d (%s)\n", error, decomp.msg);
        break;
      }

//      printf("After inflate: avail_in=%d, avail_out=%d, error=%d\n",
//             decomp.avail_in, decomp.avail_out, error);

      if (decomp.avail_out == 0)
      {
	if (alloc_size < 2048)
	  temp_size = alloc_size * 2;
	else if (alloc_size < PPDX_MAX_DATA)
	  temp_size = alloc_size + 2048;
	else
	  break;

	if ((temp = realloc(data, temp_size + 1)) == NULL)
	{
	  free(data);
	  return (NULL);
	}

	decomp.next_out  = temp + (decomp.next_out - data);
	decomp.avail_out = temp_size - alloc_size;
	data             = temp;
	alloc_size       = temp_size;
      }
    }
    while (decomp.avail_in > 0);

    chunk ++;
    snprintf(keyword, sizeof(keyword), "%s%04x", name, chunk);
  }
  while ((attr = ppdFindAttr(ppd, keyword, NULL)) != NULL);

  inflateEnd(&decomp);

 /*
  * Nul-terminate the data (usually a string)...
  */

  *(decomp.next_out) = '\0';

  if (datasize)
    *datasize = decomp.next_out - data;

  return (data);
}


/*
 * 'ppdxWriteData()' - Writes encoded data to stderr using PPD: messages.
 *
 * Writes chunked data to the PPD file using PPD: messages sent to stderr for
 * cupsd.  "name" must be a valid PPD keyword string whose length is less than
 * 37 characters to allow for chunk numbering.  "data" provides a pointer to the
 * data to be written, and "datasize" provides the length.
 */

extern void
ppdxWriteData(const char *name,		/* I - Base name of keyword */
              const void *data,		/* I - Data to write */
              size_t     datasize)	/* I - Number of bytes in data */
{
  char		buffer[PPDX_MAX_CHUNK],	/* Chunk buffer */
		encoded[PPDX_MAX_VALUE + 1],
					/* Encoded data */
		pair[PPD_MAX_LINE],	/* name=value pair */
		line[PPDX_MAX_STATUS],	/* Line buffer */
		*lineptr,		/* Current position in line buffer */
		*lineend;		/* End of line buffer */
  unsigned	chunk = 0;		/* Current chunk number */
  int		len;			/* Length of current chunk */
  z_stream	comp;			/* Compressor stream */
  int		error;			/* Error/status from deflate() */


 /*
  * Range check input...
  */

  if (!name || (!data && datasize > 0) || datasize > PPDX_MAX_DATA)
    return;

  strlcpy(line, "PPD:", sizeof(line));
  lineptr = line + 4;
  lineend = line + sizeof(line) - 2;

  if (datasize > 0)
  {
   /*
    * Compress and encode output...
    */

    memset(&comp, 0, sizeof(comp));
    comp.next_in   = (Bytef *)data;
    comp.avail_in  = datasize;

    deflateInit(&comp, 9);

    do
    {
     /*
      * Compress a chunk...
      */

      comp.next_out  = buffer;
      comp.avail_out = sizeof(buffer);

      if ((error = deflate(&comp, Z_FINISH)) < Z_OK)
      {
        fprintf(stderr, "ERROR: deflate returned %d (%s)\n", error, comp.msg);
        break;
      }

     /*
      * Write a chunk...
      */

      len = sizeof(buffer) - comp.avail_out;
      httpEncode64_2(encoded, sizeof(encoded), buffer, len);

      len = (int)snprintf(pair, sizeof(pair), " %s%04x=%s", name, chunk,
			  encoded);
#ifdef DEBUG
      fprintf(stderr, "DEBUG: *%s%04x: \"%s\"\n", name, chunk, encoded);
#endif /* DEBUG */

      if ((lineptr + len) >= lineend)
      {
	*lineptr++ = '\n';
	*lineptr   = '\0';

	fputs(line, stderr);
	lineptr = line + 4;
      }

      strlcpy(lineptr, pair, lineend - lineptr);
      lineptr += len;

     /*
      * Setup for the next one...
      */

      chunk ++;
    }
    while (comp.avail_out == 0);
  }

  deflateEnd(&comp);

 /*
  * Write a trailing empty chunk to signal EOD...
  */

  len = (int)snprintf(pair, sizeof(pair), " %s%04x=\"\"", name, chunk);
#ifdef DEBUG
  fprintf(stderr, "DEBUG: *%s%04x: \"\"\n", name, chunk);
#endif /* DEBUG */

  if ((lineptr + len) >= lineend)
  {
    *lineptr++ = '\n';
    *lineptr   = '\0';

    fputs(line, stderr);
    lineptr = line + 4;
  }

  strlcpy(lineptr, pair, lineend - lineptr);
  lineptr += len;

  *lineptr++ = '\n';
  *lineptr   = '\0';

  fputs(line, stderr);
}


/*
 * End of "$Id: ppdx.c 3833 2012-05-23 22:51:18Z msweet $".
 */
