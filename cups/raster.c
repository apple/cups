/*
 * "$Id: raster.c,v 1.3 1999/03/24 21:20:40 mike Exp $"
 *
 *   Raster file routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-1999 by Easy Software Products.
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
 * Contents:
 *
 *   cupsRasterClose()       - Close a raster stream.
 *   cupsRasterOpen()        - Open a raster stream.
 *   cupsRasterReadHeader()  - Read a raster page header.
 *   cupsRasterReadPixels()  - Read raster pixels.
 *   cupsRasterWriteHeader() - Write a raster page header.
 *   cupsRasterWritePixels() - Write raster pixels.
 */

/*
 * Include necessary headers...
 */

#include "raster.h"
#include <stdio.h>
#include <stdlib.h>

#if defined(WIN32) || defined(__EMX__)
#  include <io.h>
#else
#  include <unistd.h>
#endif /* WIN32 || __EMX__ */


/*
 * 'cupsRasterClose()' - Close a raster stream.
 */

void
cupsRasterClose(cups_raster_t *r)	/* I - Stream to close */
{
  if (r != NULL)
    free(r);
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

    if (read(fd, &(r->sync), sizeof(r->sync)) < sizeof(r->sync))
    {
      free(r);
      return (NULL);
    }

    if (r->sync != CUPS_RASTER_SYNC &&
        r->sync != CUPS_RASTER_REVSYNC)
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
    if (write(fd, &(r->sync), sizeof(r->sync)) < sizeof(r->sync))
    {
      free(r);
      return (NULL);
    }
  }

  return (r);
}


/*
 * 'cupsRasterReadHeader()' - Read a raster page header.
 */

unsigned					/* O - 1 on success, 0 on fail */
cupsRasterReadHeader(cups_raster_t      *r,	/* I - Raster stream */
                     cups_page_header_t *h)	/* I - Pointer to header data */
{
  int		len;				/* Number of words to swap */
  union swap_s					/* Swapping structure */
  {
    unsigned char	b[4];
    unsigned		v;
  }		*s;


  if (r == NULL || r->mode != CUPS_RASTER_READ)
    return (0);

  if (read(r->fd, h, sizeof(cups_page_header_t)) < sizeof(cups_page_header_t))
    return (0);

  if (r->sync == CUPS_RASTER_REVSYNC)
    for (len = (sizeof(cups_page_header_t) - 256) / 4,
             s = (union swap_s *)&(h->AdvanceDistance);
	 len > 0;
	 len --, s ++)
      s->v = (((((s->b[3] << 8) | s->b[2]) << 8) | s->b[1]) << 8) | s->b[0];

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
  if (r == NULL || r->mode != CUPS_RASTER_READ)
    return (0);
  else
    return (read(r->fd, p, len));
}


/*
 * 'cupsRasterWriteHeader()' - Write a raster page header.
 */
 
unsigned
cupsRasterWriteHeader(cups_raster_t *r,
                      cups_page_header_t *h)
{
  if (r == NULL || r->mode != CUPS_RASTER_WRITE)
    return (0);
  else
    return (write(r->fd, h, sizeof(cups_page_header_t)) ==
              sizeof(cups_page_header_t));
}


/*
 * 'cupsRasterWritePixels()' - Write raster pixels.
 */

unsigned
cupsRasterWritePixels(cups_raster_t *r,
                      unsigned char *p,
		      unsigned      len)
{
  if (r == NULL || r->mode != CUPS_RASTER_WRITE)
    return (0);
  else
    return (write(r->fd, p, len));
}


/*
 * End of "$Id: raster.c,v 1.3 1999/03/24 21:20:40 mike Exp $".
 */
