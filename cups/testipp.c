/*
 * "$Id: testipp.c,v 1.1 2003/03/20 02:45:22 mike Exp $"
 *
 *   IPP test program for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2003 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Easy Software Products and are protected by Federal
 *   copyright law.  Distribution and use rights are outlined in the file
 *   "LICENSE.txt" which should have been included with this file.  If this
 *   file is missing or damaged please contact Easy Software Products
 *   at:
 *
 *       Attn: CUPS Licensing Information
 *       Easy Software Products
 *       44141 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   main() - Main entry.
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include "ipp.h"


/*
 * Local globals...
 */

int		rpos;				/* Current position in buffer */
ipp_uchar_t	wbuffer[8192];			/* Write buffer */
int		wused;				/* Number of bytes in buffer */
ipp_uchar_t	collection[] =			/* Collection buffer */
		{
		  0x01, 0x01,			/* IPP version */
		  0x00, 0x02,			/* Print-Job operation */
		  0x00, 0x00, 0x00, 0x01,	/* Request ID */
		  IPP_TAG_JOB,			/* job group tag */
		  IPP_TAG_BEGIN_COLLECTION,	/* begCollection tag */
		  0x00, 0x09,			/* Name length + name */
		  'm', 'e', 'd', 'i', 'a', '-', 'c', 'o', 'l',
		  0x00, 0x00,			/* No value */
		  IPP_TAG_MEMBERNAME,		/* memberAttrName tag */
		  0x00, 0x00,			/* No name */
		  0x00, 0x0b,			/* Value length + value */
		  'm', 'e', 'd', 'i', 'a', '-', 'c', 'o', 'l', 'o', 'r',
		  IPP_TAG_KEYWORD,		/* keyword tag */
		  0x00, 0x00,			/* No name */
		  0x00, 0x04,			/* Value length + value */
		  'b', 'l', 'u', 'e',
		  IPP_TAG_END_COLLECTION,	/* endCollection tag */
		  0x00, 0x00,			/* No name */
		  0x00, 0x00,			/* No value */
		  IPP_TAG_END			/* end tag */
		};


/*
 * Local functions...
 */

void	hex_dump(ipp_uchar_t *buffer, int bytes);
int	read_cb(void *data, ipp_uchar_t *buffer, int bytes);
int	write_cb(void *data, ipp_uchar_t *buffer, int bytes);


/*
 * 'main()' - Main entry.
 */

int				/* O - Exit status */
main(int  argc,			/* I - Number of command-line arguments */
     char *argv[])		/* I - Command-line arguments */
{
  ipp_t		*col;		/* Collection */
  ipp_t		*request;	/* Request */
  ipp_state_t	state;		/* State */


  request = ippNew();
  request->request.op.version[0]   = 0x01;
  request->request.op.version[1]   = 0x01;
  request->request.op.operation_id = IPP_PRINT_JOB;
  request->request.op.request_id   = 1;

  col = ippNew();
  ippAddString(col, IPP_TAG_JOB, IPP_TAG_KEYWORD, "media-color", NULL, "blue");
  ippAddCollection(request, IPP_TAG_JOB, "media-col", col);

  wused = 0;
  while ((state = ippWriteIO(wbuffer, write_cb, 1, NULL, request)) != IPP_DATA)
    if (state == IPP_ERROR)
      break;

  if (state != IPP_DATA)
    puts("ERROR writing collection attribute!");

  printf("%d bytes written:\n", wused);
  hex_dump(wbuffer, wused);

  if (wused != sizeof(collection))
  {
    printf("ERROR expected %d bytes!\n", sizeof(collection));
    hex_dump(collection, sizeof(collection));
  }
  else if (memcmp(wbuffer, collection, wused))
  {
    puts("ERROR output does not match baseline!");
    hex_dump(collection, sizeof(collection));
  }

  ippDelete(col);
  ippDelete(request);

  request = ippNew();
  rpos    = 0;

  while ((state = ippReadIO(wbuffer, read_cb, 1, NULL, request)) != IPP_DATA)
    if (state == IPP_ERROR)
      break;

  if (state != IPP_DATA)
    puts("ERROR reading collection attribute!");

  printf("%d bytes read.\n", rpos);

  return (0);
}


void
hex_dump(ipp_uchar_t *buffer,
         int         bytes)
{
  int	i, j;
  int	ch;


  for (i = 0; i < bytes; i += 16)
  {
    printf("%04x ", i);

    for (j = 0; j < 16; j ++)
      if ((i + j) < bytes)
        printf(" %02x", buffer[i + j]);
      else
        printf("   ");

    putchar(' ');
    putchar(' ');

    for (j = 0; j < 16 && (i + j) < bytes; j ++)
    {
      ch = buffer[i + j] & 127;

      if (ch < ' ' || ch == 127)
        putchar('.');
      else
        putchar(ch);
    }

    putchar('\n');
  }
}


int
read_cb(void        *data,
        ipp_uchar_t *buffer,
	int         bytes)
{
  int	count;


  for (count = bytes; count > 0 && rpos < wused; count --, rpos ++)
    *buffer++ = wbuffer[rpos];

  return (bytes - count);
}


int
write_cb(void        *data,
         ipp_uchar_t *buffer,
	 int         bytes)
{
  int	count;

  printf("write_cb(data=%p, buffer=%p, bytes=%d)\n", data, buffer, bytes);

  for (count = bytes; count > 0 && wused < sizeof(wbuffer); count --, wused ++)
    wbuffer[wused] = *buffer++;

  printf("    count=%d, returning %d\n", count, bytes - count);

  return (bytes - count);
}


/*
 * End of "$Id: testipp.c,v 1.1 2003/03/20 02:45:22 mike Exp $".
 */
