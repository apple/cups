/*
 * Imaging library stubs for CUPS.
 *
 * Copyright © 2018 by Apple Inc.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more
 * information.
 */

/*
 * Include necessary headers...
 */

#include "raster-private.h"


/*
 * These stubs wrap the real functions in libcups - this allows one library to
 * provide all of the CUPS API functions while still supporting the old split
 * library organization...
 */


/*
 * Local functions...
 */

static ssize_t	cups_read_fd(void *ctx, unsigned char *buf, size_t bytes);
static ssize_t	cups_write_fd(void *ctx, unsigned char *buf, size_t bytes);



/*
 * 'cupsRasterClose()' - Close a raster stream.
 *
 * The file descriptor associated with the raster stream must be closed
 * separately as needed.
 */

void
cupsRasterClose(cups_raster_t *r)	/* I - Stream to close */
{
  _cupsRasterDelete(r);
}


/*
 * 'cupsRasterErrorString()' - Return the last error from a raster function.
 *
 * If there are no recent errors, `NULL` is returned.
 *
 * @since CUPS 1.3/macOS 10.5@
 */

const char *				/* O - Last error or `NULL` */
cupsRasterErrorString(void)
{
  return (_cupsRasterErrorString());
}


/*
 * 'cupsRasterInitPWGHeader()' - Initialize a page header for PWG Raster output.
 *
 * The "media" argument specifies the media to use.
 *
 * The "type" argument specifies a "pwg-raster-document-type-supported" value
 * that controls the color space and bit depth of the raster data.
 *
 * The "xres" and "yres" arguments specify the raster resolution in dots per
 * inch.
 *
 * The "sheet_back" argument specifies a "pwg-raster-document-sheet-back" value
 * to apply for the back side of a page.  Pass @code NULL@ for the front side.
 *
 * @since CUPS 2.2/macOS 10.12@
 */

int					/* O - 1 on success, 0 on failure */
cupsRasterInitPWGHeader(
    cups_page_header2_t *h,		/* I - Page header */
    pwg_media_t         *media,		/* I - PWG media information */
    const char          *type,		/* I - PWG raster type string */
    int                 xdpi,		/* I - Cross-feed direction (horizontal) resolution */
    int                 ydpi,		/* I - Feed direction (vertical) resolution */
    const char          *sides,		/* I - IPP "sides" option value */
    const char          *sheet_back)	/* I - Transform for back side or @code NULL@ for none */
{
  return (_cupsRasterInitPWGHeader(h, media, type, xdpi, ydpi, sides, sheet_back));
}


/*
 * 'cupsRasterOpen()' - Open a raster stream using a file descriptor.
 *
 * This function associates a raster stream with the given file descriptor.
 * For most printer driver filters, "fd" will be 0 (stdin).  For most raster
 * image processor (RIP) filters that generate raster data, "fd" will be 1
 * (stdout).
 *
 * When writing raster data, the @code CUPS_RASTER_WRITE@,
 * @code CUPS_RASTER_WRITE_COMPRESS@, or @code CUPS_RASTER_WRITE_PWG@ mode can
 * be used - compressed and PWG output is generally 25-50% smaller but adds a
 * 100-300% execution time overhead.
 */

cups_raster_t *				/* O - New stream */
cupsRasterOpen(int         fd,		/* I - File descriptor */
               cups_mode_t mode)	/* I - Mode - @code CUPS_RASTER_READ@,
	                                       @code CUPS_RASTER_WRITE@,
					       @code CUPS_RASTER_WRITE_COMPRESSED@,
					       or @code CUPS_RASTER_WRITE_PWG@ */
{
  if (mode == CUPS_RASTER_READ)
    return (_cupsRasterNew(cups_read_fd, (void *)((intptr_t)fd), mode));
  else
    return (_cupsRasterNew(cups_write_fd, (void *)((intptr_t)fd), mode));
}


/*
 * 'cupsRasterOpenIO()' - Open a raster stream using a callback function.
 *
 * This function associates a raster stream with the given callback function and
 * context pointer.
 *
 * When writing raster data, the @code CUPS_RASTER_WRITE@,
 * @code CUPS_RASTER_WRITE_COMPRESS@, or @code CUPS_RASTER_WRITE_PWG@ mode can
 * be used - compressed and PWG output is generally 25-50% smaller but adds a
 * 100-300% execution time overhead.
 */

cups_raster_t *				/* O - New stream */
cupsRasterOpenIO(
    cups_raster_iocb_t iocb,		/* I - Read/write callback */
    void               *ctx,		/* I - Context pointer for callback */
    cups_mode_t        mode)		/* I - Mode - @code CUPS_RASTER_READ@,
	                                       @code CUPS_RASTER_WRITE@,
					       @code CUPS_RASTER_WRITE_COMPRESSED@,
					       or @code CUPS_RASTER_WRITE_PWG@ */
{
  return (_cupsRasterNew(iocb, ctx, mode));
}


/*
 * 'cupsRasterReadHeader()' - Read a raster page header and store it in a
 *                            version 1 page header structure.
 *
 * This function is deprecated. Use @link cupsRasterReadHeader2@ instead.
 *
 * Version 1 page headers were used in CUPS 1.0 and 1.1 and contain a subset
 * of the version 2 page header data. This function handles reading version 2
 * page headers and copying only the version 1 data into the provided buffer.
 *
 * @deprecated@
 */

unsigned				/* O - 1 on success, 0 on failure/end-of-file */
cupsRasterReadHeader(
    cups_raster_t      *r,		/* I - Raster stream */
    cups_page_header_t *h)		/* I - Pointer to header data */
{
 /*
  * Get the raster header...
  */

  if (!_cupsRasterReadHeader(r))
  {
    memset(h, 0, sizeof(cups_page_header_t));
    return (0);
  }

 /*
  * Copy the header to the user-supplied buffer...
  */

  memcpy(h, &(r->header), sizeof(cups_page_header_t));
  return (1);
}


/*
 * 'cupsRasterReadHeader2()' - Read a raster page header and store it in a
 *                             version 2 page header structure.
 *
 * @since CUPS 1.2/macOS 10.5@
 */

unsigned				/* O - 1 on success, 0 on failure/end-of-file */
cupsRasterReadHeader2(
    cups_raster_t       *r,		/* I - Raster stream */
    cups_page_header2_t *h)		/* I - Pointer to header data */
{
 /*
  * Get the raster header...
  */

  if (!_cupsRasterReadHeader(r))
  {
    memset(h, 0, sizeof(cups_page_header2_t));
    return (0);
  }

 /*
  * Copy the header to the user-supplied buffer...
  */

  memcpy(h, &(r->header), sizeof(cups_page_header2_t));
  return (1);
}


/*
 * 'cupsRasterReadPixels()' - Read raster pixels.
 *
 * For best performance, filters should read one or more whole lines.
 * The "cupsBytesPerLine" value from the page header can be used to allocate
 * the line buffer and as the number of bytes to read.
 */

unsigned				/* O - Number of bytes read */
cupsRasterReadPixels(
    cups_raster_t *r,			/* I - Raster stream */
    unsigned char *p,			/* I - Pointer to pixel buffer */
    unsigned      len)			/* I - Number of bytes to read */
{
  return (_cupsRasterReadPixels(r, p, len));
}


/*
 * 'cupsRasterWriteHeader()' - Write a raster page header from a version 1 page
 *                             header structure.
 *
 * This function is deprecated. Use @link cupsRasterWriteHeader2@ instead.
 *
 * @deprecated@
 */

unsigned				/* O - 1 on success, 0 on failure */
cupsRasterWriteHeader(
    cups_raster_t      *r,		/* I - Raster stream */
    cups_page_header_t *h)		/* I - Raster page header */
{
  if (r == NULL || r->mode == CUPS_RASTER_READ)
    return (0);

 /*
  * Make a copy of the header and write using the private function...
  */

  memset(&(r->header), 0, sizeof(r->header));
  memcpy(&(r->header), h, sizeof(cups_page_header_t));

  return (_cupsRasterWriteHeader(r));
}


/*
 * 'cupsRasterWriteHeader2()' - Write a raster page header from a version 2
 *                              page header structure.
 *
 * The page header can be initialized using @link cupsRasterInitPWGHeader@.
 *
 * @since CUPS 1.2/macOS 10.5@
 */

unsigned				/* O - 1 on success, 0 on failure */
cupsRasterWriteHeader2(
    cups_raster_t       *r,		/* I - Raster stream */
    cups_page_header2_t *h)		/* I - Raster page header */
{
  if (r == NULL || r->mode == CUPS_RASTER_READ)
    return (0);

 /*
  * Make a copy of the header, and compute the number of raster
  * lines in the page image...
  */

  memcpy(&(r->header), h, sizeof(cups_page_header2_t));

  return (_cupsRasterWriteHeader(r));
}


/*
 * 'cupsRasterWritePixels()' - Write raster pixels.
 *
 * For best performance, filters should write one or more whole lines.
 * The "cupsBytesPerLine" value from the page header can be used to allocate
 * the line buffer and as the number of bytes to write.
 */

unsigned				/* O - Number of bytes written */
cupsRasterWritePixels(
    cups_raster_t *r,			/* I - Raster stream */
    unsigned char *p,			/* I - Bytes to write */
    unsigned      len)			/* I - Number of bytes to write */
{
  return (_cupsRasterWritePixels(r, p, len));
}


/*
 * 'cups_read_fd()' - Read bytes from a file.
 */

static ssize_t				/* O - Bytes read or -1 */
cups_read_fd(void          *ctx,	/* I - File descriptor as pointer */
             unsigned char *buf,	/* I - Buffer for read */
	     size_t        bytes)	/* I - Maximum number of bytes to read */
{
  int		fd = (int)((intptr_t)ctx);
					/* File descriptor */
  ssize_t	count;			/* Number of bytes read */


#ifdef _WIN32 /* Sigh */
  while ((count = read(fd, buf, (unsigned)bytes)) < 0)
#else
  while ((count = read(fd, buf, bytes)) < 0)
#endif /* _WIN32 */
    if (errno != EINTR && errno != EAGAIN)
      return (-1);

  return (count);
}


/*
 * 'cups_write_fd()' - Write bytes to a file.
 */

static ssize_t				/* O - Bytes written or -1 */
cups_write_fd(void          *ctx,	/* I - File descriptor pointer */
              unsigned char *buf,	/* I - Bytes to write */
	      size_t        bytes)	/* I - Number of bytes to write */
{
  int		fd = (int)((intptr_t)ctx);
					/* File descriptor */
  ssize_t	count;			/* Number of bytes written */


#ifdef _WIN32 /* Sigh */
  while ((count = write(fd, buf, (unsigned)bytes)) < 0)
#else
  while ((count = write(fd, buf, bytes)) < 0)
#endif /* _WIN32 */
    if (errno != EINTR && errno != EAGAIN)
      return (-1);

  return (count);
}
