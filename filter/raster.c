/*
 * "$Id: raster.c,v 1.2.2.8 2004/02/03 04:08:18 mike Exp $"
 *
 *   Raster file routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2003 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Easy Software Products and are protected by Federal
 *   copyright law.  Distribution and use rights for the CUPS Raster source
 *   files are outlined in the GNU Library General Public License, located
 *   in the "pstoraster" directory.  If this file is missing or damaged
 *   please contact Easy Software Products at:
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
 *   This code and any derivative of it may be used and distributed
 *   freely under the terms of the GNU General Public License when
 *   used with GNU Ghostscript or its derivatives.  Use of the code
 *   (or any derivative of it) with software other than GNU
 *   GhostScript (or its derivatives) is governed by the CUPS license
 *   agreement.
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   cupsRasterClose()        - Close a raster stream.
 *   cupsRasterOpen()         - Open a raster stream.
 *   cupsRasterReadHeader()   - Read a V1 raster page header.
 *   cupsRasterReadHeader2()  - Read a V2 raster page header.
 *   cupsRasterReadPixels()   - Read raster pixels.
 *   cupsRasterWriteHeader()  - Write a V1 raster page header.
 *   cupsRasterWriteHeader2() - Write a V2 raster page header.
 *   cupsRasterWritePixels()  - Write raster pixels.
 *   cups_raster_update()     - Update the raster header and row count for the
 *                              current page.
 *   cups_raster_write()      - Write a row of raster data...
 *   cups_read()              - Read bytes from a file.
 *   cups_write()             - Write bytes to a file.
 */

/*
 * Include necessary headers...
 */

#include "raster.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <cups/string.h>

#if defined(WIN32) || defined(__EMX__)
#  include <io.h>
#else
#  include <unistd.h>
#endif /* WIN32 || __EMX__ */


/*
 * Local functions...
 */

static unsigned	cups_raster_read_header(cups_raster_t *r);
static void	cups_raster_update(cups_raster_t *r);
static int	cups_raster_write(cups_raster_t *r);
static int	cups_read(int fd, char *buf, int bytes);
static int	cups_write(int fd, const char *buf, int bytes);


/*
 * 'cupsRasterClose()' - Close a raster stream.
 */

void
cupsRasterClose(cups_raster_t *r)	/* I - Stream to close */
{
  if (r != NULL)
  {
    if (r->pixels)
      free(r->pixels);

    free(r);
  }
}


/*
 * 'cupsRasterOpen()' - Open a raster stream.
 */

cups_raster_t *				/* O - New stream */
cupsRasterOpen(int         fd,		/* I - File descriptor */
               cups_mode_t mode)	/* I - Mode */
{
  cups_raster_t	*r;			/* New stream */


  if ((r = calloc(sizeof(cups_raster_t), 1)) == NULL)
    return (NULL);

  r->fd   = fd;
  r->mode = mode;

  if (mode == CUPS_RASTER_READ)
  {
   /*
    * Open for read - get sync word...
    */

    if (cups_read(r->fd, (char *)&(r->sync), sizeof(r->sync)) < sizeof(r->sync))
    {
      free(r);
      return (NULL);
    }

    if (r->sync != CUPS_RASTER_SYNC &&
        r->sync != CUPS_RASTER_REVSYNC &&
        r->sync != CUPS_RASTER_SYNCv1 &&
        r->sync != CUPS_RASTER_REVSYNCv1)
    {
      free(r);
      return (NULL);
    }
  }
  else
  {
   /*
    * Open for write - put sync word...
    */

    r->sync = CUPS_RASTER_SYNC;
    if (cups_write(r->fd, (char *)&(r->sync), sizeof(r->sync)) < sizeof(r->sync))
    {
      free(r);
      return (NULL);
    }
  }

  return (r);
}


/*
 * 'cupsRasterReadHeader()' - Read a V1 raster page header.
 */

unsigned					/* O - 1 on success, 0 on fail */
cupsRasterReadHeader(cups_raster_t      *r,	/* I - Raster stream */
                     cups_page_header_t *h)	/* I - Pointer to header data */
{
 /*
  * Get the raster header...
  */

  if (!cups_raster_read_header(r))
    return (0);
  
 /*
  * Copy the header to the user-supplied buffer...
  */

  memcpy(h, &(r->header), sizeof(cups_page_header2_t));

  return (1);
}


/*
 * 'cupsRasterReadHeader2()' - Read a V2 raster page header.
 */

unsigned					/* O - 1 on success, 0 on fail */
cupsRasterReadHeader2(cups_raster_t       *r,	/* I - Raster stream */
                      cups_page_header2_t *h)	/* I - Pointer to header data */
{
 /*
  * Get the raster header...
  */

  if (!cups_raster_read_header(r))
    return (0);
  
 /*
  * Copy the header to the user-supplied buffer...
  */

  memcpy(h, &(r->header), sizeof(cups_page_header2_t));

  return (1);
}


/*
 * 'cupsRasterReadPixels()' - Read raster pixels.
 */

unsigned				/* O - Number of bytes read */
cupsRasterReadPixels(cups_raster_t *r,	/* I - Raster stream */
                     unsigned char *p,	/* I - Pointer to pixel buffer */
		     unsigned      len)	/* I - Number of bytes to read */
{
  int		bytes;			/* Bytes read */
  unsigned	remaining;		/* Bytes remaining */
  unsigned char	*ptr,			/* Pointer to read buffer */
		byte;			/* Byte from file */


  if (r == NULL || r->mode != CUPS_RASTER_READ || r->remaining == 0)
    return (0);

  remaining = len;

  while (remaining > 0 && r->remaining > 0)
  {
    if (r->count == 0)
    {
     /*
      * Need to read a new row...
      */

      if (remaining == r->header.cupsBytesPerLine)
	ptr = p;
      else
	ptr = r->pixels;

      if (r->sync == CUPS_RASTER_SYNCv1 || r->sync == CUPS_RASTER_REVSYNCv1)
      {
       /*
	* Read without compression...
	*/

        if (cups_read(r->fd, ptr, r->header.cupsBytesPerLine) <
	        r->header.cupsBytesPerLine)
	  return (0);

        r->count = 1;
      }
      else
      {
       /*
        * Read using a modified TIFF "packbits" compression...
	*/

        unsigned char	*temp;
	int		count;
	unsigned char	byte;


        if (cups_read(r->fd, &byte, 1) < 1)
	  return (0);

        r->count = byte + 1;

        if (r->count > 1)
	  ptr = r->pixels;

        temp  = ptr;
	bytes = r->header.cupsBytesPerLine;

	while (bytes > 0)
	{
	 /*
	  * Get a new repeat count...
	  */

          if (cups_read(r->fd, &byte, 1) < 1)
	    return (0);

	  if (byte & 128)
	  {
	   /*
	    * Copy N literal pixels...
	    */

	    count = (257 - byte) * r->bpp;

            if (count > bytes)
	      count = bytes;

            if (cups_read(r->fd, temp, count) < count)
	      return (0);

	    temp  += count;
	    bytes -= count;
	  }
	  else
	  {
	   /*
	    * Repeat the next N bytes...
	    */

            count = (byte + 1) * r->bpp;
            if (count > bytes)
	      count = bytes;

            if (count < r->bpp)
	      break;

	    bytes -= count;

            if (cups_read(r->fd, temp, r->bpp) < r->bpp)
	      return (0);

	    temp  += r->bpp;
	    count -= r->bpp;

	    while (count > 0)
	    {
	      memcpy(temp, temp - r->bpp, r->bpp);
	      temp  += r->bpp;
	      count -= r->bpp;
            }
	  }
	}
      }

      if (r->header.cupsBitsPerColor == 16 &&
          (r->sync == CUPS_RASTER_REVSYNC || r->sync == CUPS_RASTER_REVSYNCv1))
      {
       /*
	* Swap bytes in the pixel data...
	*/

        unsigned char	*temp;
	int		count;


        for (temp = ptr, count = r->header.cupsBytesPerLine;
	     count > 0;
	     temp += 2, count -= 2)
	{
	  byte    = temp[0];
	  temp[0] = temp[1];
	  temp[1] = byte;
	}
      }

      if (remaining >= r->header.cupsBytesPerLine)
      {
	bytes       = r->header.cupsBytesPerLine;
        r->pcurrent = r->pixels;
	r->count --;
	r->remaining --;
      }
      else
      {
	bytes       = remaining;
        r->pcurrent = r->pixels + bytes;
      }

      if (ptr != p)
        memcpy(p, ptr, bytes);
    }
    else
    {
     /*
      * Copy fragment from buffer...
      */

      if ((bytes = r->pend - r->pcurrent) > remaining)
        bytes = remaining;

      memcpy(p, r->pcurrent, bytes);
      r->pcurrent += bytes;

      if (r->pcurrent >= r->pend)
      {
        r->pcurrent = r->pixels;
	r->count --;
	r->remaining --;
      }
    }

    remaining -= bytes;
    p         += bytes;
  }

  return (len);
}


/*
 * 'cupsRasterWriteHeader()' - Write a V2 raster page header.
 */
 
unsigned					/* O - 1 on success, 0 on failure */
cupsRasterWriteHeader(cups_raster_t      *r,	/* I - Raster stream */
                      cups_page_header_t *h)	/* I - Raster page header */
{
  if (r == NULL || r->mode != CUPS_RASTER_WRITE)
    return (0);

 /*
  * Make a copy of the header, and compute the number of raster
  * lines in the page image...
  */

  memset(&(r->header), 0, sizeof(r->header));
  memcpy(&(r->header), h, sizeof(cups_page_header_t));

  cups_raster_update(r);

 /*
  * Write the raster header...
  */

  return (cups_write(r->fd, (char *)&(r->header), sizeof(r->header)) > 0);
}


/*
 * 'cupsRasterWriteHeader2()' - Write a V2 raster page header.
 */
 
unsigned					/* O - 1 on success, 0 on failure */
cupsRasterWriteHeader2(cups_raster_t       *r,	/* I - Raster stream */
                       cups_page_header2_t *h)	/* I - Raster page header */
{
  if (r == NULL || r->mode != CUPS_RASTER_WRITE)
    return (0);

 /*
  * Make a copy of the header, and compute the number of raster
  * lines in the page image...
  */

  memcpy(&(r->header), h, sizeof(cups_page_header2_t));

  cups_raster_update(r);

 /*
  * Write the raster header...
  */

  return (cups_write(r->fd, (char *)&(r->header), sizeof(r->header)) > 0);
}


/*
 * 'cupsRasterWritePixels()' - Write raster pixels.
 */

unsigned				/* O - Number of bytes written */
cupsRasterWritePixels(cups_raster_t *r,	/* I - Raster stream */
                      unsigned char *p,	/* I - Bytes to write */
		      unsigned      len)/* I - Number of bytes to write */
{
  int		bytes;			/* Bytes read */
  unsigned	remaining;		/* Bytes remaining */


  if (r == NULL || r->mode != CUPS_RASTER_WRITE || r->remaining == 0)
    return (0);

  remaining = len;

  while (remaining > 0)
  {
   /*
    * Figure out the number of remaining bytes on the current line...
    */

    if ((bytes = remaining) > (r->pend - r->pcurrent))
      bytes = r->pend - r->pcurrent;

    if (r->count > 0)
    {
     /*
      * Check to see if this line is the same as the previous line...
      */

      if (memcmp(p, r->pcurrent, bytes))
      {
        if (!cups_raster_write(r))
	  return (0);

	r->count = 0;
      }
      else
      {
       /*
        * Mark more bytes as the same...
	*/

        r->pcurrent += bytes;

	if (r->pcurrent >= r->pend)
	{
	 /*
          * Increase the repeat count...
	  */

	  r->count ++;
	  r->pcurrent = r->pixels;

	 /*
          * Flush out this line if it is the last one...
	  */

	  r->remaining --;
	  if (r->remaining == 0)
	    return (cups_raster_write(r));
	  else if (r->count == 256)
	  {
	    cups_raster_write(r);
	    r->count = 0;
	  }
	}
      }
    }

    if (r->count == 0)
    {
     /*
      * Copy the raster data to the buffer...
      */

      memcpy(r->pcurrent, p, bytes);

      r->pcurrent += bytes;

      if (r->pcurrent >= r->pend)
      {
       /*
        * Increase the repeat count...
	*/

	r->count ++;
	r->pcurrent = r->pixels;

       /*
        * Flush out this line if it is the last one...
	*/

	r->remaining --;
	if (r->remaining == 0)
	  return (cups_raster_write(r));
      }
    }

    remaining -= bytes;
    p         += bytes;
  }

  return (len);
}


/*
 * 'cups_raster_read_header()' - Read a raster page header.
 */

static unsigned					/* O - 1 on success, 0 on fail */
cups_raster_read_header(cups_raster_t *r)	/* I - Raster stream */
{
  int		len;				/* Number of words to swap */
  union swap_s					/* Swapping structure */
  {
    unsigned char	b[4];
    unsigned		v;
  }		*s;


  if (r == NULL || r->mode != CUPS_RASTER_READ)
    return (0);

 /*
  * Get the length of the raster header...
  */

  if (r->sync == CUPS_RASTER_SYNCv1 || r->sync == CUPS_RASTER_REVSYNCv1)
    len = sizeof(cups_page_header_t);
  else
    len = sizeof(cups_page_header2_t);

 /*
  * Read the header...
  */

  memset(&(r->header), 0, sizeof(r->header));

  if (cups_read(r->fd, (char *)&(r->header), len) < len)
    return (0);

 /*
  * Swap bytes as needed...
  */

  if (r->sync == CUPS_RASTER_REVSYNC || r->sync == CUPS_RASTER_REVSYNCv1)
    for (len = 68, s = (union swap_s *)&(r->header.AdvanceDistance);
	 len > 0;
	 len --, s ++)
      s->v = (((((s->b[3] << 8) | s->b[2]) << 8) | s->b[1]) << 8) | s->b[0];

 /*
  * Update the header and row count...
  */

  cups_raster_update(r);

  return (1);
}


/*
 * 'cups_raster_update()' - Update the raster header and row count for the
 *                          current page.
 */

static void
cups_raster_update(cups_raster_t *r)	/* I - Raster stream */
{
  if (r->sync == CUPS_RASTER_SYNCv1 || r->sync == CUPS_RASTER_REVSYNCv1)
  {
   /*
    * Set the "cupsNumColors" field according to the colorspace...
    */

    switch (r->header.cupsColorSpace)
    {
      case CUPS_CSPACE_W :
      case CUPS_CSPACE_K :
      case CUPS_CSPACE_WHITE :
      case CUPS_CSPACE_GOLD :
      case CUPS_CSPACE_SILVER :
      case CUPS_CSPACE_ICC1 :
          r->header.cupsNumColors = 1;
	  break;

      case CUPS_CSPACE_ICC2 :
          r->header.cupsNumColors = 2;
	  break;

      case CUPS_CSPACE_RGB :
      case CUPS_CSPACE_CMY :
      case CUPS_CSPACE_YMC :
      case CUPS_CSPACE_CIEXYZ :
      case CUPS_CSPACE_CIELab :
      case CUPS_CSPACE_ICC3 :
          r->header.cupsNumColors = 3;
	  break;

      case CUPS_CSPACE_RGBA :
      case CUPS_CSPACE_CMYK :
      case CUPS_CSPACE_YMCK :
      case CUPS_CSPACE_KCMY :
      case CUPS_CSPACE_GMCK :
      case CUPS_CSPACE_GMCS :
      case CUPS_CSPACE_ICC4 :
          r->header.cupsNumColors = 4;
	  break;

      case CUPS_CSPACE_KCMYcm :
          if (r->header.cupsBitsPerPixel < 8)
            r->header.cupsNumColors = 6;
	  else
            r->header.cupsNumColors = 4;
	  break;

      case CUPS_CSPACE_ICC5 :
      case CUPS_CSPACE_ICC6 :
      case CUPS_CSPACE_ICC7 :
      case CUPS_CSPACE_ICC8 :
      case CUPS_CSPACE_ICC9 :
      case CUPS_CSPACE_ICCA :
      case CUPS_CSPACE_ICCB :
      case CUPS_CSPACE_ICCC :
      case CUPS_CSPACE_ICCD :
      case CUPS_CSPACE_ICCE :
      case CUPS_CSPACE_ICCF :
          r->header.cupsNumColors = r->header.cupsColorSpace -
	                            CUPS_CSPACE_ICC1 + 1;
	  break;
    }
  }

 /*
  * Set the number of bytes per pixel/color...
  */

  if (r->header.cupsColorOrder == CUPS_ORDER_CHUNKED)
    r->bpp = (r->header.cupsBitsPerPixel + 7) / 8;
  else
    r->bpp = (r->header.cupsBitsPerColor + 7) / 8;

 /*
  * Set the number of remaining rows...
  */

  if (r->header.cupsColorOrder == CUPS_ORDER_PLANAR)
    r->remaining = r->header.cupsHeight * r->header.cupsNumColors;
  else
    r->remaining = r->header.cupsHeight;

 /*
  * Allocate the read/write buffer...
  */

  if (r->pixels != NULL)
    free(r->pixels);

  r->pixels   = calloc(r->header.cupsBytesPerLine, 1);
  r->pcurrent = r->pixels;
  r->pend     = r->pixels + r->header.cupsBytesPerLine;
  r->count    = 0;
}


/*
 * 'cups_raster_write()' - Write a row of raster data...
 */

static int				/* O - Number of bytes written */
cups_raster_write(cups_raster_t *r)	/* I - Raster stream */
{
  unsigned char	*start,			/* Start of sequence */
		*ptr,			/* Current pointer in sequence */
		byte;			/* Byte to write */
  int		count;			/* Count */


 /*
  * Write the row repeat count...
  */

  byte = r->count - 1;

  if (cups_write(r->fd, &byte, 1) < 1)
    return (0);

 /*
  * Write using a modified TIFF "packbits" compression...
  */

  for (ptr = r->pixels; ptr < r->pend;)
  {
    start = ptr;
    ptr += r->bpp;

    if (ptr == r->pend)
    {
     /*
      * Encode a single pixel at the end...
      */

      byte = 0;
      if (cups_write(r->fd, &byte, 1) < 1)
        return (0);

      if (cups_write(r->fd, start, r->bpp) < r->bpp)
        return (0);
    }
    else if (memcmp(start, ptr, r->bpp) == 0)
    {
     /*
      * Encode a sequence of repeating pixels...
      */

      for (count = 2; count < 128 && ptr < (r->pend - r->bpp); count ++, ptr += r->bpp)
        if (memcmp(ptr, ptr + r->bpp, r->bpp) != 0)
	  break;

      ptr += r->bpp;

      byte = count - 1;

      if (cups_write(r->fd, &byte, 1) < 1)
        return (0);

      if (cups_write(r->fd, start, r->bpp) < r->bpp)
        return (0);
    }
    else
    {
     /*
      * Encode a sequence of non-repeating pixels...
      */

      for (count = 1; count < 127 && ptr < (r->pend - r->bpp); count ++, ptr += r->bpp)
        if (memcmp(ptr, ptr + r->bpp, r->bpp) == 0)
	  break;

      if (ptr >= (r->pend - r->bpp) && count < 128)
      {
        count ++;
	ptr += r->bpp;
      }
 
      byte = 257 - count;

      if (cups_write(r->fd, &byte, 1) < 1)
        return (0);

      count *= r->bpp;

      if (cups_write(r->fd, start, count) < count)
        return (0);
    }
  }

  return (r->header.cupsBytesPerLine);
}


/*
 * 'cups_read()' - Read bytes from a file.
 */

static int					/* O - Bytes read or -1 */
cups_read(int  fd,				/* I - File descriptor */
          char *buf,				/* I - Buffer for read */
	  int  bytes)				/* I - Number of bytes to read */
{
  int	count,					/* Number of bytes read */
	total;					/* Total bytes read */


  for (total = 0; total < bytes; total += count, buf += count)
  {
    count = read(fd, buf, bytes - total);

    if (count == 0)
      return (0);
    else if (count < 0)
    {
      if (errno != EAGAIN && errno != EINTR)
        count = 0;
      else
        return (-1);
    }
  }

  return (total);
}


/*
 * 'cups_write()' - Write bytes to a file.
 */

static int					/* O - Bytes written or -1 */
cups_write(int        fd,			/* I - File descriptor */
           const char *buf,			/* I - Bytes to write */
	   int        bytes)			/* I - Number of bytes to write */
{
  int	count,					/* Number of bytes written */
	total;					/* Total bytes written */


  for (total = 0; total < bytes; total += count, buf += count)
  {
    count = write(fd, buf, bytes - total);

    if (count < 0)
    {
      if (errno != EAGAIN && errno != EINTR)
        count = 0;
      else
        return (-1);
    }
  }

  return (total);
}


/*
 * End of "$Id: raster.c,v 1.2.2.8 2004/02/03 04:08:18 mike Exp $".
 */
