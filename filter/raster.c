/*
 * Raster file routines for CUPS.
 *
 * Copyright 2007-2018 by Apple Inc.
 * Copyright 1997-2006 by Easy Software Products.
 *
 * This file is part of the CUPS Imaging library.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * missing or damaged, see the license at "http://www.cups.org/".
 *
 * This file is subject to the Apple OS-Developed Software exception.
 */

/*
 * Include necessary headers...
 */

#include <cups/raster-private.h>
#ifdef HAVE_STDINT_H
#  include <stdint.h>
#endif /* HAVE_STDINT_H */


/*
 * Private structures...
 */

struct _cups_raster_s			/**** Raster stream data ****/
{
  unsigned		sync;		/* Sync word from start of stream */
  void			*ctx;		/* File descriptor */
  cups_raster_iocb_t	iocb;		/* IO callback */
  cups_mode_t		mode;		/* Read/write mode */
  cups_page_header2_t	header;		/* Raster header for current page */
  unsigned		rowheight,	/* Row height in lines */
			count,		/* Current row run-length count */
			remaining,	/* Remaining rows in page image */
			bpp;		/* Bytes per pixel/color */
  unsigned char		*pixels,	/* Pixels for current row */
			*pend,		/* End of pixel buffer */
			*pcurrent;	/* Current byte in pixel buffer */
  int			compressed,	/* Non-zero if data is compressed */
			swapped;	/* Non-zero if data is byte-swapped */
  unsigned char		*buffer,	/* Read/write buffer */
			*bufptr,	/* Current (read) position in buffer */
			*bufend;	/* End of current (read) buffer */
  size_t		bufsize;	/* Buffer size */
#ifdef DEBUG
  size_t		iostart,	/* Start of read/write buffer */
			iocount;	/* Number of bytes read/written */
#endif /* DEBUG */
  unsigned		apple_page_count;/* Apple raster page count */
};

typedef void (*_cups_copyfunc_t)(void *dst, const void *src, size_t bytes);


/*
 * Local globals...
 */

#ifdef DEBUG
  static const char * const cups_color_spaces[] =
  {					/* Color spaces */
    "CUPS_CSPACE_W",
    "CUPS_CSPACE_RGB",
    "CUPS_CSPACE_RGBA",
    "CUPS_CSPACE_K",
    "CUPS_CSPACE_CMY",
    "CUPS_CSPACE_YMC",
    "CUPS_CSPACE_CMYK",
    "CUPS_CSPACE_YMCK",
    "CUPS_CSPACE_KCMY",
    "CUPS_CSPACE_KCMYcm",
    "CUPS_CSPACE_GMCK",
    "CUPS_CSPACE_GMCS",
    "CUPS_CSPACE_WHITE",
    "CUPS_CSPACE_GOLD",
    "CUPS_CSPACE_SILVER",
    "CUPS_CSPACE_CIEXYZ",
    "CUPS_CSPACE_CIELab",
    "CUPS_CSPACE_RGBW",
    "CUPS_CSPACE_SW",
    "CUPS_CSPACE_SRGB",
    "CUPS_CSPACE_ADOBERGB",
    "21",
    "22",
    "23",
    "24",
    "25",
    "26",
    "27",
    "28",
    "29",
    "30",
    "31",
    "CUPS_CSPACE_ICC1",
    "CUPS_CSPACE_ICC2",
    "CUPS_CSPACE_ICC3",
    "CUPS_CSPACE_ICC4",
    "CUPS_CSPACE_ICC5",
    "CUPS_CSPACE_ICC6",
    "CUPS_CSPACE_ICC7",
    "CUPS_CSPACE_ICC8",
    "CUPS_CSPACE_ICC9",
    "CUPS_CSPACE_ICCA",
    "CUPS_CSPACE_ICCB",
    "CUPS_CSPACE_ICCC",
    "CUPS_CSPACE_ICCD",
    "CUPS_CSPACE_ICCE",
    "CUPS_CSPACE_ICCF",
    "47",
    "CUPS_CSPACE_DEVICE1",
    "CUPS_CSPACE_DEVICE2",
    "CUPS_CSPACE_DEVICE3",
    "CUPS_CSPACE_DEVICE4",
    "CUPS_CSPACE_DEVICE5",
    "CUPS_CSPACE_DEVICE6",
    "CUPS_CSPACE_DEVICE7",
    "CUPS_CSPACE_DEVICE8",
    "CUPS_CSPACE_DEVICE9",
    "CUPS_CSPACE_DEVICEA",
    "CUPS_CSPACE_DEVICEB",
    "CUPS_CSPACE_DEVICEC",
    "CUPS_CSPACE_DEVICED",
    "CUPS_CSPACE_DEVICEE",
    "CUPS_CSPACE_DEVICEF"
  };
  static const char * const cups_modes[] =
  {					/* Open modes */
    "CUPS_RASTER_READ",
    "CUPS_RASTER_WRITE",
    "CUPS_RASTER_WRITE_COMPRESSED",
    "CUPS_RASTER_WRITE_PWG",
    "CUPS_RASTER_WRITE_APPLE"
  };
#endif /* DEBUG */


/*
 * Local functions...
 */

static ssize_t	cups_raster_io(cups_raster_t *r, unsigned char *buf, size_t bytes);
static unsigned	cups_raster_read_header(cups_raster_t *r);
static ssize_t	cups_raster_read(cups_raster_t *r, unsigned char *buf, size_t bytes);
static int	cups_raster_update(cups_raster_t *r);
static ssize_t	cups_raster_write(cups_raster_t *r, const unsigned char *pixels);
static ssize_t	cups_read_fd(void *ctx, unsigned char *buf, size_t bytes);
static void	cups_swap(unsigned char *buf, size_t bytes);
static void	cups_swap_copy(unsigned char *dst, const unsigned char *src, size_t bytes);
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
  if (r != NULL)
  {
    if (r->buffer)
      free(r->buffer);

    if (r->pixels)
      free(r->pixels);

    free(r);
  }
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
  if (!h || !media || !type || xdpi <= 0 || ydpi <= 0)
  {
    _cupsRasterAddError("%s", strerror(EINVAL));
    return (0);
  }

 /*
  * Initialize the page header...
  */

  memset(h, 0, sizeof(cups_page_header2_t));

  strlcpy(h->cupsPageSizeName, media->pwg, sizeof(h->cupsPageSizeName));

  h->PageSize[0] = (unsigned)(72 * media->width / 2540);
  h->PageSize[1] = (unsigned)(72 * media->length / 2540);

  /* This never gets written but is needed for some applications */
  h->cupsPageSize[0] = 72.0f * media->width / 2540.0f;
  h->cupsPageSize[1] = 72.0f * media->length / 2540.0f;

  h->ImagingBoundingBox[2] = h->PageSize[0];
  h->ImagingBoundingBox[3] = h->PageSize[1];

  h->HWResolution[0] = (unsigned)xdpi;
  h->HWResolution[1] = (unsigned)ydpi;

  h->cupsWidth  = (unsigned)(media->width * xdpi / 2540);
  h->cupsHeight = (unsigned)(media->length * ydpi / 2540);

  if (h->cupsWidth > 0x00ffffff || h->cupsHeight > 0x00ffffff)
  {
    _cupsRasterAddError("Raster dimensions too large.");
    return (0);
  }

  h->cupsInteger[CUPS_RASTER_PWG_ImageBoxRight]  = h->cupsWidth;
  h->cupsInteger[CUPS_RASTER_PWG_ImageBoxBottom] = h->cupsHeight;

 /*
  * Colorspace and bytes per line...
  */

  if (!strcmp(type, "adobe-rgb_8"))
  {
    h->cupsBitsPerColor = 8;
    h->cupsBitsPerPixel = 24;
    h->cupsColorSpace   = CUPS_CSPACE_ADOBERGB;
  }
  else if (!strcmp(type, "adobe-rgb_16"))
  {
    h->cupsBitsPerColor = 16;
    h->cupsBitsPerPixel = 48;
    h->cupsColorSpace   = CUPS_CSPACE_ADOBERGB;
  }
  else if (!strcmp(type, "black_1"))
  {
    h->cupsBitsPerColor = 1;
    h->cupsBitsPerPixel = 1;
    h->cupsColorSpace   = CUPS_CSPACE_K;
  }
  else if (!strcmp(type, "black_8"))
  {
    h->cupsBitsPerColor = 8;
    h->cupsBitsPerPixel = 8;
    h->cupsColorSpace   = CUPS_CSPACE_K;
  }
  else if (!strcmp(type, "black_16"))
  {
    h->cupsBitsPerColor = 16;
    h->cupsBitsPerPixel = 16;
    h->cupsColorSpace   = CUPS_CSPACE_K;
  }
  else if (!strcmp(type, "cmyk_8"))
  {
    h->cupsBitsPerColor = 8;
    h->cupsBitsPerPixel = 32;
    h->cupsColorSpace   = CUPS_CSPACE_CMYK;
  }
  else if (!strcmp(type, "cmyk_16"))
  {
    h->cupsBitsPerColor = 16;
    h->cupsBitsPerPixel = 64;
    h->cupsColorSpace   = CUPS_CSPACE_CMYK;
  }
  else if (!strncmp(type, "device", 6) && type[6] >= '1' && type[6] <= '9')
  {
    int ncolors, bits;			/* Number of colors and bits */


    if (sscanf(type, "device%d_%d", &ncolors, &bits) != 2 || ncolors > 15 || (bits != 8 && bits != 16))
    {
      _cupsRasterAddError("Unsupported raster type \'%s\'.", type);
      return (0);
    }

    h->cupsBitsPerColor = (unsigned)bits;
    h->cupsBitsPerPixel = (unsigned)(ncolors * bits);
    h->cupsColorSpace   = (cups_cspace_t)(CUPS_CSPACE_DEVICE1 + ncolors - 1);
  }
  else if (!strcmp(type, "rgb_8"))
  {
    h->cupsBitsPerColor = 8;
    h->cupsBitsPerPixel = 24;
    h->cupsColorSpace   = CUPS_CSPACE_RGB;
  }
  else if (!strcmp(type, "rgb_16"))
  {
    h->cupsBitsPerColor = 16;
    h->cupsBitsPerPixel = 48;
    h->cupsColorSpace   = CUPS_CSPACE_RGB;
  }
  else if (!strcmp(type, "sgray_1"))
  {
    h->cupsBitsPerColor = 1;
    h->cupsBitsPerPixel = 1;
    h->cupsColorSpace   = CUPS_CSPACE_SW;
  }
  else if (!strcmp(type, "sgray_8"))
  {
    h->cupsBitsPerColor = 8;
    h->cupsBitsPerPixel = 8;
    h->cupsColorSpace   = CUPS_CSPACE_SW;
  }
  else if (!strcmp(type, "sgray_16"))
  {
    h->cupsBitsPerColor = 16;
    h->cupsBitsPerPixel = 16;
    h->cupsColorSpace   = CUPS_CSPACE_SW;
  }
  else if (!strcmp(type, "srgb_8"))
  {
    h->cupsBitsPerColor = 8;
    h->cupsBitsPerPixel = 24;
    h->cupsColorSpace   = CUPS_CSPACE_SRGB;
  }
  else if (!strcmp(type, "srgb_16"))
  {
    h->cupsBitsPerColor = 16;
    h->cupsBitsPerPixel = 48;
    h->cupsColorSpace   = CUPS_CSPACE_SRGB;
  }
  else
  {
    _cupsRasterAddError("Unsupported raster type \'%s\'.", type);
    return (0);
  }

  h->cupsColorOrder   = CUPS_ORDER_CHUNKED;
  h->cupsNumColors    = h->cupsBitsPerPixel / h->cupsBitsPerColor;
  h->cupsBytesPerLine = (h->cupsWidth * h->cupsBitsPerPixel + 7) / 8;

 /*
  * Duplex support...
  */

  h->cupsInteger[CUPS_RASTER_PWG_CrossFeedTransform] = 1;
  h->cupsInteger[CUPS_RASTER_PWG_FeedTransform]      = 1;

  if (sides)
  {
    if (!strcmp(sides, "two-sided-long-edge"))
    {
      h->Duplex = 1;
    }
    else if (!strcmp(sides, "two-sided-short-edge"))
    {
      h->Duplex = 1;
      h->Tumble = 1;
    }
    else if (strcmp(sides, "one-sided"))
    {
      _cupsRasterAddError("Unsupported sides value \'%s\'.", sides);
      return (0);
    }

    if (sheet_back)
    {
      if (!strcmp(sheet_back, "flipped"))
      {
        if (h->Tumble)
          h->cupsInteger[CUPS_RASTER_PWG_CrossFeedTransform] = 0xffffffffU;
        else
          h->cupsInteger[CUPS_RASTER_PWG_FeedTransform] = 0xffffffffU;
      }
      else if (!strcmp(sheet_back, "manual-tumble"))
      {
        if (h->Tumble)
        {
          h->cupsInteger[CUPS_RASTER_PWG_CrossFeedTransform] = 0xffffffffU;
          h->cupsInteger[CUPS_RASTER_PWG_FeedTransform]      = 0xffffffffU;
        }
      }
      else if (!strcmp(sheet_back, "rotated"))
      {
        if (!h->Tumble)
        {
          h->cupsInteger[CUPS_RASTER_PWG_CrossFeedTransform] = 0xffffffffU;
          h->cupsInteger[CUPS_RASTER_PWG_FeedTransform]      = 0xffffffffU;
        }
      }
      else if (strcmp(sheet_back, "normal"))
      {
	_cupsRasterAddError("Unsupported sheet_back value \'%s\'.", sheet_back);
	return (0);
      }
    }
  }

  return (1);
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
  DEBUG_printf(("cupsRasterOpen(fd=%d, mode=%s)", fd, cups_modes[mode]));

  if (mode == CUPS_RASTER_READ)
    return (cupsRasterOpenIO(cups_read_fd, (void *)((intptr_t)fd), mode));
  else
    return (cupsRasterOpenIO(cups_write_fd, (void *)((intptr_t)fd), mode));
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
  cups_raster_t	*r;			/* New stream */


  DEBUG_printf(("cupsRasterOpenIO(iocb=%p, ctx=%p, mode=%s)", (void *)iocb, ctx, cups_modes[mode]));

  _cupsRasterClearError();

  if ((r = calloc(sizeof(cups_raster_t), 1)) == NULL)
  {
    _cupsRasterAddError("Unable to allocate memory for raster stream: %s\n",
                        strerror(errno));
    DEBUG_puts("1cupsRasterOpenIO: Returning NULL.");
    return (NULL);
  }

  r->ctx  = ctx;
  r->iocb = iocb;
  r->mode = mode;

  if (mode == CUPS_RASTER_READ)
  {
   /*
    * Open for read - get sync word...
    */

    if (cups_raster_io(r, (unsigned char *)&(r->sync), sizeof(r->sync)) !=
            sizeof(r->sync))
    {
      _cupsRasterAddError("Unable to read header from raster stream: %s\n",
                          strerror(errno));
      free(r);
      DEBUG_puts("1cupsRasterOpenIO: Unable to read header, returning NULL.");
      return (NULL);
    }

    if (r->sync != CUPS_RASTER_SYNC &&
        r->sync != CUPS_RASTER_REVSYNC &&
        r->sync != CUPS_RASTER_SYNCv1 &&
        r->sync != CUPS_RASTER_REVSYNCv1 &&
        r->sync != CUPS_RASTER_SYNCv2 &&
        r->sync != CUPS_RASTER_REVSYNCv2 &&
        r->sync != CUPS_RASTER_SYNCapple &&
        r->sync != CUPS_RASTER_REVSYNCapple)
    {
      _cupsRasterAddError("Unknown raster format %08x!\n", r->sync);
      free(r);
      DEBUG_puts("1cupsRasterOpenIO: Unknown format, returning NULL.");
      return (NULL);
    }

    if (r->sync == CUPS_RASTER_SYNCv2 ||
        r->sync == CUPS_RASTER_REVSYNCv2 ||
        r->sync == CUPS_RASTER_SYNCapple ||
        r->sync == CUPS_RASTER_REVSYNCapple)
      r->compressed = 1;

    DEBUG_printf(("1cupsRasterOpenIO: sync=%08x", r->sync));

    if (r->sync == CUPS_RASTER_REVSYNC ||
        r->sync == CUPS_RASTER_REVSYNCv1 ||
        r->sync == CUPS_RASTER_REVSYNCv2 ||
        r->sync == CUPS_RASTER_REVSYNCapple)
      r->swapped = 1;

    if (r->sync == CUPS_RASTER_SYNCapple ||
        r->sync == CUPS_RASTER_REVSYNCapple)
    {
      unsigned char	header[8];	/* File header */

      if (cups_raster_io(r, (unsigned char *)header, sizeof(header)) !=
	      sizeof(header))
      {
	_cupsRasterAddError("Unable to read header from raster stream: %s\n",
			    strerror(errno));
	free(r);
	DEBUG_puts("1cupsRasterOpenIO: Unable to read header, returning NULL.");
	return (NULL);
      }
    }

#ifdef DEBUG
    r->iostart = r->iocount;
#endif /* DEBUG */
  }
  else
  {
   /*
    * Open for write - put sync word...
    */

    switch (mode)
    {
      default :
      case CUPS_RASTER_WRITE :
          r->sync = CUPS_RASTER_SYNC;
	  break;

      case CUPS_RASTER_WRITE_COMPRESSED :
          r->compressed = 1;
          r->sync       = CUPS_RASTER_SYNCv2;
	  break;

      case CUPS_RASTER_WRITE_PWG :
          r->compressed = 1;
          r->sync       = htonl(CUPS_RASTER_SYNC_PWG);
          r->swapped    = r->sync != CUPS_RASTER_SYNC_PWG;
	  break;

      case CUPS_RASTER_WRITE_APPLE :
          r->compressed     = 1;
          r->sync           = htonl(CUPS_RASTER_SYNCapple);
          r->swapped        = r->sync != CUPS_RASTER_SYNCapple;
          r->apple_page_count = 0xffffffffU;
	  break;
    }

    if (cups_raster_io(r, (unsigned char *)&(r->sync), sizeof(r->sync)) < (ssize_t)sizeof(r->sync))
    {
      _cupsRasterAddError("Unable to write raster stream header: %s\n",
                          strerror(errno));
      free(r);
      DEBUG_puts("1cupsRasterOpenIO: Unable to write header, returning NULL.");
      return (NULL);
    }
  }

  DEBUG_printf(("1cupsRasterOpenIO: compressed=%d, swapped=%d, returning %p", r->compressed, r->swapped, (void *)r));

  return (r);
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
  DEBUG_printf(("cupsRasterReadHeader(r=%p, h=%p)", (void *)r, (void *)h));

 /*
  * Get the raster header...
  */

  if (!cups_raster_read_header(r))
  {
    memset(h, 0, sizeof(cups_page_header_t));
    DEBUG_puts("1cupsRasterReadHeader: Unable to read page header, returning 0.");
    return (0);
  }

 /*
  * Copy the header to the user-supplied buffer...
  */

  memcpy(h, &(r->header), sizeof(cups_page_header_t));

  DEBUG_printf(("1cupsRasterReadHeader: cupsColorSpace=%s", cups_color_spaces[h->cupsColorSpace]));
  DEBUG_printf(("1cupsRasterReadHeader: cupsBitsPerColor=%u", h->cupsBitsPerColor));
  DEBUG_printf(("1cupsRasterReadHeader: cupsBitsPerPixel=%u", h->cupsBitsPerPixel));
  DEBUG_printf(("1cupsRasterReadHeader: cupsBytesPerLine=%u", h->cupsBytesPerLine));
  DEBUG_printf(("1cupsRasterReadHeader: cupsWidth=%u", h->cupsWidth));
  DEBUG_printf(("1cupsRasterReadHeader: cupsHeight=%u", h->cupsHeight));

  DEBUG_puts("1cupsRasterReadHeader: Returning 1.");
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

  DEBUG_printf(("cupsRasterReadHeader2(r=%p, h=%p)", (void *)r, (void *)h));

  if (!cups_raster_read_header(r))
  {
    memset(h, 0, sizeof(cups_page_header2_t));
    DEBUG_puts("1cupsRasterReadHeader2: Unable to read header, returning 0.");
    return (0);
  }

 /*
  * Copy the header to the user-supplied buffer...
  */

  memcpy(h, &(r->header), sizeof(cups_page_header2_t));

  DEBUG_printf(("1cupsRasterReadHeader2: cupsColorSpace=%s", cups_color_spaces[h->cupsColorSpace]));
  DEBUG_printf(("1cupsRasterReadHeader2: cupsBitsPerColor=%u", h->cupsBitsPerColor));
  DEBUG_printf(("1cupsRasterReadHeader2: cupsBitsPerPixel=%u", h->cupsBitsPerPixel));
  DEBUG_printf(("1cupsRasterReadHeader2: cupsBytesPerLine=%u", h->cupsBytesPerLine));
  DEBUG_printf(("1cupsRasterReadHeader2: cupsWidth=%u", h->cupsWidth));
  DEBUG_printf(("1cupsRasterReadHeader2: cupsHeight=%u", h->cupsHeight));

  DEBUG_puts("1cupsRasterReadHeader2: Returning 1.");
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
cupsRasterReadPixels(cups_raster_t *r,	/* I - Raster stream */
                     unsigned char *p,	/* I - Pointer to pixel buffer */
		     unsigned      len)	/* I - Number of bytes to read */
{
  ssize_t	bytes;			/* Bytes read */
  unsigned	cupsBytesPerLine;	/* cupsBytesPerLine value */
  unsigned	remaining;		/* Bytes remaining */
  unsigned char	*ptr,			/* Pointer to read buffer */
		byte,			/* Byte from file */
		*temp;			/* Pointer into buffer */
  unsigned	count;			/* Repetition count */


  DEBUG_printf(("cupsRasterReadPixels(r=%p, p=%p, len=%u)", (void *)r, (void *)p, len));

  if (r == NULL || r->mode != CUPS_RASTER_READ || r->remaining == 0 ||
      r->header.cupsBytesPerLine == 0)
  {
    DEBUG_puts("1cupsRasterReadPixels: Returning 0.");
    return (0);
  }

  DEBUG_printf(("1cupsRasterReadPixels: compressed=%d, remaining=%u", r->compressed, r->remaining));

  if (!r->compressed)
  {
   /*
    * Read without compression...
    */

    r->remaining -= len / r->header.cupsBytesPerLine;

    if (cups_raster_io(r, p, len) < (ssize_t)len)
    {
      DEBUG_puts("1cupsRasterReadPixels: Read error, returning 0.");
      return (0);
    }

   /*
    * Swap bytes as needed...
    */

    if (r->swapped &&
        (r->header.cupsBitsPerColor == 16 ||
         r->header.cupsBitsPerPixel == 12 ||
         r->header.cupsBitsPerPixel == 16))
      cups_swap(p, len);

   /*
    * Return...
    */

    DEBUG_printf(("1cupsRasterReadPixels: Returning %u", len));

    return (len);
  }

 /*
  * Read compressed data...
  */

  remaining        = len;
  cupsBytesPerLine = r->header.cupsBytesPerLine;

  while (remaining > 0 && r->remaining > 0)
  {
    if (r->count == 0)
    {
     /*
      * Need to read a new row...
      */

      if (remaining == cupsBytesPerLine)
	ptr = p;
      else
	ptr = r->pixels;

     /*
      * Read using a modified PackBits compression...
      */

      if (!cups_raster_read(r, &byte, 1))
      {
	DEBUG_puts("1cupsRasterReadPixels: Read error, returning 0.");
	return (0);
      }

      r->count = (unsigned)byte + 1;

      if (r->count > 1)
	ptr = r->pixels;

      temp  = ptr;
      bytes = (ssize_t)cupsBytesPerLine;

      while (bytes > 0)
      {
       /*
	* Get a new repeat count...
	*/

        if (!cups_raster_read(r, &byte, 1))
	{
	  DEBUG_puts("1cupsRasterReadPixels: Read error, returning 0.");
	  return (0);
	}

        if (byte == 128)
        {
         /*
          * Clear to end of line...
          */

          switch (r->header.cupsColorSpace)
          {
            case CUPS_CSPACE_W :
            case CUPS_CSPACE_RGB :
            case CUPS_CSPACE_SW :
            case CUPS_CSPACE_SRGB :
            case CUPS_CSPACE_RGBW :
            case CUPS_CSPACE_ADOBERGB :
                memset(temp, 0xff, (size_t)bytes);
                break;
            default :
                memset(temp, 0x00, (size_t)bytes);
                break;
          }

          temp += bytes;
          bytes = 0;
        }
	else if (byte & 128)
	{
	 /*
	  * Copy N literal pixels...
	  */

	  count = (unsigned)(257 - byte) * r->bpp;

          if (count > (unsigned)bytes)
	    count = (unsigned)bytes;

          if (!cups_raster_read(r, temp, count))
	  {
	    DEBUG_puts("1cupsRasterReadPixels: Read error, returning 0.");
	    return (0);
	  }

	  temp  += count;
	  bytes -= (ssize_t)count;
	}
	else
	{
	 /*
	  * Repeat the next N bytes...
	  */

          count = ((unsigned)byte + 1) * r->bpp;
          if (count > (unsigned)bytes)
	    count = (unsigned)bytes;

          if (count < r->bpp)
	    break;

	  bytes -= (ssize_t)count;

          if (!cups_raster_read(r, temp, r->bpp))
	  {
	    DEBUG_puts("1cupsRasterReadPixels: Read error, returning 0.");
	    return (0);
	  }

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

     /*
      * Swap bytes as needed...
      */

      if ((r->header.cupsBitsPerColor == 16 ||
           r->header.cupsBitsPerPixel == 12 ||
           r->header.cupsBitsPerPixel == 16) &&
          r->swapped)
      {
        DEBUG_puts("1cupsRasterReadPixels: Swapping bytes.");
        cups_swap(ptr, (size_t)cupsBytesPerLine);
      }

     /*
      * Update pointers...
      */

      if (remaining >= cupsBytesPerLine)
      {
	bytes       = (ssize_t)cupsBytesPerLine;
        r->pcurrent = r->pixels;
	r->count --;
	r->remaining --;
      }
      else
      {
	bytes       = (ssize_t)remaining;
        r->pcurrent = r->pixels + bytes;
      }

     /*
      * Copy data as needed...
      */

      if (ptr != p)
        memcpy(p, ptr, (size_t)bytes);
    }
    else
    {
     /*
      * Copy fragment from buffer...
      */

      if ((unsigned)(bytes = (int)(r->pend - r->pcurrent)) > remaining)
        bytes = (ssize_t)remaining;

      memcpy(p, r->pcurrent, (size_t)bytes);
      r->pcurrent += bytes;

      if (r->pcurrent >= r->pend)
      {
        r->pcurrent = r->pixels;
	r->count --;
	r->remaining --;
      }
    }

    remaining -= (unsigned)bytes;
    p         += bytes;
  }

  DEBUG_printf(("1cupsRasterReadPixels: Returning %u", len));

  return (len);
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
  DEBUG_printf(("cupsRasterWriteHeader(r=%p, h=%p)", (void *)r, (void *)h));

  if (r == NULL || r->mode == CUPS_RASTER_READ)
  {
    DEBUG_puts("1cupsRasterWriteHeader: Returning 0.");
    return (0);
  }

  DEBUG_printf(("1cupsRasterWriteHeader: cupsColorSpace=%s", cups_color_spaces[h->cupsColorSpace]));
  DEBUG_printf(("1cupsRasterWriteHeader: cupsBitsPerColor=%u", h->cupsBitsPerColor));
  DEBUG_printf(("1cupsRasterWriteHeader: cupsBitsPerPixel=%u", h->cupsBitsPerPixel));
  DEBUG_printf(("1cupsRasterWriteHeader: cupsBytesPerLine=%u", h->cupsBytesPerLine));
  DEBUG_printf(("1cupsRasterWriteHeader: cupsWidth=%u", h->cupsWidth));
  DEBUG_printf(("1cupsRasterWriteHeader: cupsHeight=%u", h->cupsHeight));

 /*
  * Make a copy of the header, and compute the number of raster
  * lines in the page image...
  */

  memset(&(r->header), 0, sizeof(r->header));
  memcpy(&(r->header), h, sizeof(cups_page_header_t));

  if (!cups_raster_update(r))
  {
    DEBUG_puts("1cupsRasterWriteHeader: Unable to update parameters, returning 0.");
    return (0);
  }

  if (r->mode == CUPS_RASTER_WRITE_APPLE)
  {
    r->rowheight = h->HWResolution[0] / h->HWResolution[1];

    if (h->HWResolution[0] != (r->rowheight * h->HWResolution[1]))
      return (0);
  }
  else
    r->rowheight = 1;

 /*
  * Write the raster header...
  */

  if (r->mode == CUPS_RASTER_WRITE_PWG)
  {
   /*
    * PWG raster data is always network byte order with much of the page header
    * zeroed.
    */

    cups_page_header2_t	fh;		/* File page header */

    memset(&fh, 0, sizeof(fh));

    strlcpy(fh.MediaClass, "PwgRaster", sizeof(fh.MediaClass));
					/* PwgRaster */
    strlcpy(fh.MediaColor, r->header.MediaColor, sizeof(fh.MediaColor));
    strlcpy(fh.MediaType, r->header.MediaType, sizeof(fh.MediaType));
    strlcpy(fh.OutputType, r->header.OutputType, sizeof(fh.OutputType));
					/* PrintContentType */

    fh.CutMedia              = htonl(r->header.CutMedia);
    fh.Duplex                = htonl(r->header.Duplex);
    fh.HWResolution[0]       = htonl(r->header.HWResolution[0]);
    fh.HWResolution[1]       = htonl(r->header.HWResolution[1]);
    fh.ImagingBoundingBox[0] = htonl(r->header.ImagingBoundingBox[0]);
    fh.ImagingBoundingBox[1] = htonl(r->header.ImagingBoundingBox[1]);
    fh.ImagingBoundingBox[2] = htonl(r->header.ImagingBoundingBox[2]);
    fh.ImagingBoundingBox[3] = htonl(r->header.ImagingBoundingBox[3]);
    fh.InsertSheet           = htonl(r->header.InsertSheet);
    fh.Jog                   = htonl(r->header.Jog);
    fh.LeadingEdge           = htonl(r->header.LeadingEdge);
    fh.ManualFeed            = htonl(r->header.ManualFeed);
    fh.MediaPosition         = htonl(r->header.MediaPosition);
    fh.MediaWeight           = htonl(r->header.MediaWeight);
    fh.NumCopies             = htonl(r->header.NumCopies);
    fh.Orientation           = htonl(r->header.Orientation);
    fh.PageSize[0]           = htonl(r->header.PageSize[0]);
    fh.PageSize[1]           = htonl(r->header.PageSize[1]);
    fh.Tumble                = htonl(r->header.Tumble);
    fh.cupsWidth             = htonl(r->header.cupsWidth);
    fh.cupsHeight            = htonl(r->header.cupsHeight);
    fh.cupsBitsPerColor      = htonl(r->header.cupsBitsPerColor);
    fh.cupsBitsPerPixel      = htonl(r->header.cupsBitsPerPixel);
    fh.cupsBytesPerLine      = htonl(r->header.cupsBytesPerLine);
    fh.cupsColorOrder        = htonl(r->header.cupsColorOrder);
    fh.cupsColorSpace        = htonl(r->header.cupsColorSpace);
    fh.cupsNumColors         = htonl(r->header.cupsNumColors);
    fh.cupsInteger[0]        = htonl(r->header.cupsInteger[0]);
					/* TotalPageCount */
    fh.cupsInteger[1]        = htonl(r->header.cupsInteger[1]);
					/* CrossFeedTransform */
    fh.cupsInteger[2]        = htonl(r->header.cupsInteger[2]);
					/* FeedTransform */
    fh.cupsInteger[3]        = htonl(r->header.cupsInteger[3]);
					/* ImageBoxLeft */
    fh.cupsInteger[4]        = htonl(r->header.cupsInteger[4]);
					/* ImageBoxTop */
    fh.cupsInteger[5]        = htonl(r->header.cupsInteger[5]);
					/* ImageBoxRight */
    fh.cupsInteger[6]        = htonl(r->header.cupsInteger[6]);
					/* ImageBoxBottom */
    fh.cupsInteger[7]        = htonl(r->header.cupsInteger[7]);
					/* BlackPrimary */
    fh.cupsInteger[8]        = htonl(r->header.cupsInteger[8]);
					/* PrintQuality */
    fh.cupsInteger[14]       = htonl(r->header.cupsInteger[14]);
					/* VendorIdentifier */
    fh.cupsInteger[15]       = htonl(r->header.cupsInteger[15]);
					/* VendorLength */

    void *dst = fh.cupsReal; /* Bypass bogus compiler warning */
    void *src = r->header.cupsReal;
    memcpy(dst, src, sizeof(fh.cupsReal) + sizeof(fh.cupsString));
					/* VendorData */

    strlcpy(fh.cupsRenderingIntent, r->header.cupsRenderingIntent,
            sizeof(fh.cupsRenderingIntent));
    strlcpy(fh.cupsPageSizeName, r->header.cupsPageSizeName,
            sizeof(fh.cupsPageSizeName));

    return (cups_raster_io(r, (unsigned char *)&fh, sizeof(fh)) == sizeof(fh));
  }
  else if (r->mode == CUPS_RASTER_WRITE_APPLE)
  {
   /*
    * Raw raster data is always network byte order with most of the page header
    * zeroed.
    */

    unsigned char appleheader[32];	/* Raw page header */

    if (r->apple_page_count == 0xffffffffU)
    {
     /*
      * Write raw page count from raster page header...
      */

      r->apple_page_count = r->header.cupsInteger[0];

      appleheader[0] = 'A';
      appleheader[1] = 'S';
      appleheader[2] = 'T';
      appleheader[3] = 0;
      appleheader[4] = (unsigned char)(r->apple_page_count >> 24);
      appleheader[5] = (unsigned char)(r->apple_page_count >> 16);
      appleheader[6] = (unsigned char)(r->apple_page_count >> 8);
      appleheader[7] = (unsigned char)(r->apple_page_count);

      if (cups_raster_io(r, appleheader, 8) != 8)
        return (0);
    }

    memset(appleheader, 0, sizeof(appleheader));

    appleheader[0]  = (unsigned char)r->header.cupsBitsPerPixel;
    appleheader[1]  = r->header.cupsColorSpace == CUPS_CSPACE_SRGB ? 1 :
                        r->header.cupsColorSpace == CUPS_CSPACE_RGBW ? 2 :
                        r->header.cupsColorSpace == CUPS_CSPACE_ADOBERGB ? 3 :
                        r->header.cupsColorSpace == CUPS_CSPACE_W ? 4 :
                        r->header.cupsColorSpace == CUPS_CSPACE_RGB ? 5 :
                        r->header.cupsColorSpace == CUPS_CSPACE_CMYK ? 6 : 0;
    appleheader[12] = (unsigned char)(r->header.cupsWidth >> 24);
    appleheader[13] = (unsigned char)(r->header.cupsWidth >> 16);
    appleheader[14] = (unsigned char)(r->header.cupsWidth >> 8);
    appleheader[15] = (unsigned char)(r->header.cupsWidth);
    appleheader[16] = (unsigned char)(r->header.cupsHeight >> 24);
    appleheader[17] = (unsigned char)(r->header.cupsHeight >> 16);
    appleheader[18] = (unsigned char)(r->header.cupsHeight >> 8);
    appleheader[19] = (unsigned char)(r->header.cupsHeight);
    appleheader[20] = (unsigned char)(r->header.HWResolution[0] >> 24);
    appleheader[21] = (unsigned char)(r->header.HWResolution[0] >> 16);
    appleheader[22] = (unsigned char)(r->header.HWResolution[0] >> 8);
    appleheader[23] = (unsigned char)(r->header.HWResolution[0]);

    return (cups_raster_io(r, appleheader, sizeof(appleheader)) == sizeof(appleheader));
  }
  else
    return (cups_raster_io(r, (unsigned char *)&(r->header), sizeof(r->header))
		== sizeof(r->header));
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
  DEBUG_printf(("cupsRasterWriteHeader(r=%p, h=%p)", (void *)r, (void *)h));

  if (r == NULL || r->mode == CUPS_RASTER_READ)
  {
    DEBUG_puts("1cupsRasterWriteHeader2: Returning 0.");
    return (0);
  }

  DEBUG_printf(("1cupsRasterWriteHeader2: cupsColorSpace=%s", cups_color_spaces[h->cupsColorSpace]));
  DEBUG_printf(("1cupsRasterWriteHeader2: cupsBitsPerColor=%u", h->cupsBitsPerColor));
  DEBUG_printf(("1cupsRasterWriteHeader2: cupsBitsPerPixel=%u", h->cupsBitsPerPixel));
  DEBUG_printf(("1cupsRasterWriteHeader2: cupsBytesPerLine=%u", h->cupsBytesPerLine));
  DEBUG_printf(("1cupsRasterWriteHeader2: cupsWidth=%u", h->cupsWidth));
  DEBUG_printf(("1cupsRasterWriteHeader2: cupsHeight=%u", h->cupsHeight));

 /*
  * Make a copy of the header, and compute the number of raster
  * lines in the page image...
  */

  memcpy(&(r->header), h, sizeof(cups_page_header2_t));

  if (!cups_raster_update(r))
  {
    DEBUG_puts("1cupsRasterWriteHeader: Unable to update parameters, returning 0.");
    return (0);
  }

  if (r->mode == CUPS_RASTER_WRITE_APPLE)
  {
    r->rowheight = h->HWResolution[0] / h->HWResolution[1];

    if (h->HWResolution[0] != (r->rowheight * h->HWResolution[1]))
      return (0);
  }
  else
    r->rowheight = 1;

 /*
  * Write the raster header...
  */

  if (r->mode == CUPS_RASTER_WRITE_PWG)
  {
   /*
    * PWG raster data is always network byte order with most of the page header
    * zeroed.
    */

    cups_page_header2_t	fh;		/* File page header */

    memset(&fh, 0, sizeof(fh));
    strlcpy(fh.MediaClass, "PwgRaster", sizeof(fh.MediaClass));
    strlcpy(fh.MediaColor, r->header.MediaColor, sizeof(fh.MediaColor));
    strlcpy(fh.MediaType, r->header.MediaType, sizeof(fh.MediaType));
    strlcpy(fh.OutputType, r->header.OutputType, sizeof(fh.OutputType));
    strlcpy(fh.cupsRenderingIntent, r->header.cupsRenderingIntent,
            sizeof(fh.cupsRenderingIntent));
    strlcpy(fh.cupsPageSizeName, r->header.cupsPageSizeName,
            sizeof(fh.cupsPageSizeName));

    fh.CutMedia              = htonl(r->header.CutMedia);
    fh.Duplex                = htonl(r->header.Duplex);
    fh.HWResolution[0]       = htonl(r->header.HWResolution[0]);
    fh.HWResolution[1]       = htonl(r->header.HWResolution[1]);
    fh.ImagingBoundingBox[0] = htonl(r->header.ImagingBoundingBox[0]);
    fh.ImagingBoundingBox[1] = htonl(r->header.ImagingBoundingBox[1]);
    fh.ImagingBoundingBox[2] = htonl(r->header.ImagingBoundingBox[2]);
    fh.ImagingBoundingBox[3] = htonl(r->header.ImagingBoundingBox[3]);
    fh.InsertSheet           = htonl(r->header.InsertSheet);
    fh.Jog                   = htonl(r->header.Jog);
    fh.LeadingEdge           = htonl(r->header.LeadingEdge);
    fh.ManualFeed            = htonl(r->header.ManualFeed);
    fh.MediaPosition         = htonl(r->header.MediaPosition);
    fh.MediaWeight           = htonl(r->header.MediaWeight);
    fh.NumCopies             = htonl(r->header.NumCopies);
    fh.Orientation           = htonl(r->header.Orientation);
    fh.PageSize[0]           = htonl(r->header.PageSize[0]);
    fh.PageSize[1]           = htonl(r->header.PageSize[1]);
    fh.Tumble                = htonl(r->header.Tumble);
    fh.cupsWidth             = htonl(r->header.cupsWidth);
    fh.cupsHeight            = htonl(r->header.cupsHeight);
    fh.cupsBitsPerColor      = htonl(r->header.cupsBitsPerColor);
    fh.cupsBitsPerPixel      = htonl(r->header.cupsBitsPerPixel);
    fh.cupsBytesPerLine      = htonl(r->header.cupsBytesPerLine);
    fh.cupsColorOrder        = htonl(r->header.cupsColorOrder);
    fh.cupsColorSpace        = htonl(r->header.cupsColorSpace);
    fh.cupsNumColors         = htonl(r->header.cupsNumColors);
    fh.cupsInteger[0]        = htonl(r->header.cupsInteger[0]);
    fh.cupsInteger[1]        = htonl(r->header.cupsInteger[1]);
    fh.cupsInteger[2]        = htonl(r->header.cupsInteger[2]);
    fh.cupsInteger[3]        = htonl((unsigned)(r->header.cupsImagingBBox[0] * r->header.HWResolution[0] / 72.0));
    fh.cupsInteger[4]        = htonl((unsigned)(r->header.cupsImagingBBox[1] * r->header.HWResolution[1] / 72.0));
    fh.cupsInteger[5]        = htonl((unsigned)(r->header.cupsImagingBBox[2] * r->header.HWResolution[0] / 72.0));
    fh.cupsInteger[6]        = htonl((unsigned)(r->header.cupsImagingBBox[3] * r->header.HWResolution[1] / 72.0));
    fh.cupsInteger[7]        = htonl(0xffffff);

    return (cups_raster_io(r, (unsigned char *)&fh, sizeof(fh)) == sizeof(fh));
  }
  else if (r->mode == CUPS_RASTER_WRITE_APPLE)
  {
   /*
    * Raw raster data is always network byte order with most of the page header
    * zeroed.
    */

    unsigned char appleheader[32];	/* Raw page header */
    unsigned height = r->header.cupsHeight * r->rowheight;
					/* Computed page height */

    if (r->apple_page_count == 0xffffffffU)
    {
     /*
      * Write raw page count from raster page header...
      */

      r->apple_page_count = r->header.cupsInteger[0];

      appleheader[0] = 'A';
      appleheader[1] = 'S';
      appleheader[2] = 'T';
      appleheader[3] = 0;
      appleheader[4] = (unsigned char)(r->apple_page_count >> 24);
      appleheader[5] = (unsigned char)(r->apple_page_count >> 16);
      appleheader[6] = (unsigned char)(r->apple_page_count >> 8);
      appleheader[7] = (unsigned char)(r->apple_page_count);

      if (cups_raster_io(r, appleheader, 8) != 8)
        return (0);
    }

    memset(appleheader, 0, sizeof(appleheader));

    appleheader[0]  = (unsigned char)r->header.cupsBitsPerPixel;
    appleheader[1]  = r->header.cupsColorSpace == CUPS_CSPACE_SRGB ? 1 :
                        r->header.cupsColorSpace == CUPS_CSPACE_RGBW ? 2 :
                        r->header.cupsColorSpace == CUPS_CSPACE_ADOBERGB ? 3 :
                        r->header.cupsColorSpace == CUPS_CSPACE_W ? 4 :
                        r->header.cupsColorSpace == CUPS_CSPACE_RGB ? 5 :
                        r->header.cupsColorSpace == CUPS_CSPACE_CMYK ? 6 : 0;
    appleheader[12] = (unsigned char)(r->header.cupsWidth >> 24);
    appleheader[13] = (unsigned char)(r->header.cupsWidth >> 16);
    appleheader[14] = (unsigned char)(r->header.cupsWidth >> 8);
    appleheader[15] = (unsigned char)(r->header.cupsWidth);
    appleheader[16] = (unsigned char)(height >> 24);
    appleheader[17] = (unsigned char)(height >> 16);
    appleheader[18] = (unsigned char)(height >> 8);
    appleheader[19] = (unsigned char)(height);
    appleheader[20] = (unsigned char)(r->header.HWResolution[0] >> 24);
    appleheader[21] = (unsigned char)(r->header.HWResolution[0] >> 16);
    appleheader[22] = (unsigned char)(r->header.HWResolution[0] >> 8);
    appleheader[23] = (unsigned char)(r->header.HWResolution[0]);

    return (cups_raster_io(r, appleheader, sizeof(appleheader)) == sizeof(appleheader));
  }
  else
    return (cups_raster_io(r, (unsigned char *)&(r->header), sizeof(r->header))
		== sizeof(r->header));
}


/*
 * 'cupsRasterWritePixels()' - Write raster pixels.
 *
 * For best performance, filters should write one or more whole lines.
 * The "cupsBytesPerLine" value from the page header can be used to allocate
 * the line buffer and as the number of bytes to write.
 */

unsigned				/* O - Number of bytes written */
cupsRasterWritePixels(cups_raster_t *r,	/* I - Raster stream */
                      unsigned char *p,	/* I - Bytes to write */
		      unsigned      len)/* I - Number of bytes to write */
{
  ssize_t	bytes;			/* Bytes read */
  unsigned	remaining;		/* Bytes remaining */


  DEBUG_printf(("cupsRasterWritePixels(r=%p, p=%p, len=%u), remaining=%u", (void *)r, (void *)p, len, r->remaining));

  if (r == NULL || r->mode == CUPS_RASTER_READ || r->remaining == 0)
    return (0);

  if (!r->compressed)
  {
   /*
    * Without compression, just write the raster data raw unless the data needs
    * to be swapped...
    */

    r->remaining -= len / r->header.cupsBytesPerLine;

    if (r->swapped &&
        (r->header.cupsBitsPerColor == 16 ||
         r->header.cupsBitsPerPixel == 12 ||
         r->header.cupsBitsPerPixel == 16))
    {
      unsigned char	*bufptr;	/* Pointer into write buffer */

     /*
      * Allocate a write buffer as needed...
      */

      if ((size_t)len > r->bufsize)
      {
	if (r->buffer)
	  bufptr = realloc(r->buffer, len);
	else
	  bufptr = malloc(len);

	if (!bufptr)
	  return (0);

	r->buffer  = bufptr;
	r->bufsize = len;
      }

     /*
      * Byte swap the pixels and write them...
      */

      cups_swap_copy(r->buffer, p, len);

      bytes = cups_raster_io(r, r->buffer, len);
    }
    else
      bytes = cups_raster_io(r, p, len);

    if (bytes < len)
      return (0);
    else
      return (len);
  }

 /*
  * Otherwise, compress each line...
  */

  for (remaining = len; remaining > 0; remaining -= (unsigned)bytes, p += bytes)
  {
   /*
    * Figure out the number of remaining bytes on the current line...
    */

    if ((bytes = (ssize_t)remaining) > (ssize_t)(r->pend - r->pcurrent))
      bytes = (ssize_t)(r->pend - r->pcurrent);

    if (r->count > 0)
    {
     /*
      * Check to see if this line is the same as the previous line...
      */

      if (memcmp(p, r->pcurrent, (size_t)bytes))
      {
        if (cups_raster_write(r, r->pixels) <= 0)
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

	  r->count += r->rowheight;
	  r->pcurrent = r->pixels;

	 /*
          * Flush out this line if it is the last one...
	  */

	  r->remaining --;

	  if (r->remaining == 0)
	  {
	    if (cups_raster_write(r, r->pixels) <= 0)
	      return (0);
	    else
	      return (len);
	  }
	  else if (r->count > (256 - r->rowheight))
	  {
	    if (cups_raster_write(r, r->pixels) <= 0)
	      return (0);

	    r->count = 0;
	  }
	}

	continue;
      }
    }

    if (r->count == 0)
    {
     /*
      * Copy the raster data to the buffer...
      */

      memcpy(r->pcurrent, p, (size_t)bytes);

      r->pcurrent += bytes;

      if (r->pcurrent >= r->pend)
      {
       /*
        * Increase the repeat count...
	*/

	r->count += r->rowheight;
	r->pcurrent = r->pixels;

       /*
        * Flush out this line if it is the last one...
	*/

	r->remaining --;

	if (r->remaining == 0)
	{
	  if (cups_raster_write(r, r->pixels) <= 0)
	    return (0);
	}
      }
    }
  }

  return (len);
}


/*
 * 'cups_raster_read_header()' - Read a raster page header.
 */

static unsigned				/* O - 1 on success, 0 on fail */
cups_raster_read_header(
    cups_raster_t *r)			/* I - Raster stream */
{
  size_t	len;			/* Length for read/swap */


  DEBUG_printf(("3cups_raster_read_header(r=%p), r->mode=%s", (void *)r, r ? cups_modes[r->mode] : ""));

  if (r == NULL || r->mode != CUPS_RASTER_READ)
    return (0);

  DEBUG_printf(("4cups_raster_read_header: r->iocount=" CUPS_LLFMT, CUPS_LLCAST r->iocount));

  memset(&(r->header), 0, sizeof(r->header));

 /*
  * Read the header...
  */

  switch (r->sync)
  {
    default :
       /*
	* Get the length of the raster header...
	*/

	if (r->sync == CUPS_RASTER_SYNCv1 || r->sync == CUPS_RASTER_REVSYNCv1)
	  len = sizeof(cups_page_header_t);
	else
	  len = sizeof(cups_page_header2_t);

	DEBUG_printf(("4cups_raster_read_header: len=%d", (int)len));

       /*
        * Read it...
        */

	if (cups_raster_read(r, (unsigned char *)&(r->header), len) < (ssize_t)len)
	{
	  DEBUG_printf(("4cups_raster_read_header: EOF, r->iocount=" CUPS_LLFMT, CUPS_LLCAST r->iocount));
	  return (0);
	}

       /*
	* Swap bytes as needed...
	*/

	if (r->swapped)
	{
	  unsigned	*s,		/* Current word */
			temp;		/* Temporary copy */


	  DEBUG_puts("4cups_raster_read_header: Swapping header bytes.");

	  for (len = 81, s = &(r->header.AdvanceDistance);
	       len > 0;
	       len --, s ++)
	  {
	    temp = *s;
	    *s   = ((temp & 0xff) << 24) |
		   ((temp & 0xff00) << 8) |
		   ((temp & 0xff0000) >> 8) |
		   ((temp & 0xff000000) >> 24);

	    DEBUG_printf(("4cups_raster_read_header: %08x => %08x", temp, *s));
	  }
	}
        break;

    case CUPS_RASTER_SYNCapple :
    case CUPS_RASTER_REVSYNCapple :
        {
          unsigned char	appleheader[32];	/* Raw header */
          static const unsigned rawcspace[] =
          {
            CUPS_CSPACE_SW,
            CUPS_CSPACE_SRGB,
            CUPS_CSPACE_RGBW,
            CUPS_CSPACE_ADOBERGB,
            CUPS_CSPACE_W,
            CUPS_CSPACE_RGB,
            CUPS_CSPACE_CMYK
          };
          static const unsigned rawnumcolors[] =
          {
            1,
            3,
            4,
            3,
            1,
            3,
            4
          };

	  if (cups_raster_read(r, appleheader, sizeof(appleheader)) < (ssize_t)sizeof(appleheader))
	  {
	    DEBUG_printf(("4cups_raster_read_header: EOF, r->iocount=" CUPS_LLFMT, CUPS_LLCAST r->iocount));
	    return (0);
	  }

	  strlcpy(r->header.MediaClass, "PwgRaster", sizeof(r->header.MediaClass));
					      /* PwgRaster */
          r->header.cupsBitsPerPixel = appleheader[0];
          r->header.cupsColorSpace   = appleheader[1] >= (sizeof(rawcspace) / sizeof(rawcspace[0])) ? CUPS_CSPACE_DEVICE1 : rawcspace[appleheader[1]];
          r->header.cupsNumColors    = appleheader[1] >= (sizeof(rawnumcolors) / sizeof(rawnumcolors[0])) ? 1 : rawnumcolors[appleheader[1]];
          r->header.cupsBitsPerColor = r->header.cupsBitsPerPixel / r->header.cupsNumColors;
          r->header.cupsWidth        = ((((((unsigned)appleheader[12] << 8) | (unsigned)appleheader[13]) << 8) | (unsigned)appleheader[14]) << 8) | (unsigned)appleheader[15];
          r->header.cupsHeight       = ((((((unsigned)appleheader[16] << 8) | (unsigned)appleheader[17]) << 8) | (unsigned)appleheader[18]) << 8) | (unsigned)appleheader[19];
          r->header.cupsBytesPerLine = r->header.cupsWidth * r->header.cupsBitsPerPixel / 8;
          r->header.cupsColorOrder   = CUPS_ORDER_CHUNKED;
          r->header.HWResolution[0]  = r->header.HWResolution[1] = ((((((unsigned)appleheader[20] << 8) | (unsigned)appleheader[21]) << 8) | (unsigned)appleheader[22]) << 8) | (unsigned)appleheader[23];

          if (r->header.HWResolution[0] > 0)
          {
	    r->header.PageSize[0]     = (unsigned)(r->header.cupsWidth * 72 / r->header.HWResolution[0]);
	    r->header.PageSize[1]     = (unsigned)(r->header.cupsHeight * 72 / r->header.HWResolution[1]);
	    r->header.cupsPageSize[0] = (float)(r->header.cupsWidth * 72.0 / r->header.HWResolution[0]);
	    r->header.cupsPageSize[1] = (float)(r->header.cupsHeight * 72.0 / r->header.HWResolution[1]);
          }

          r->header.cupsInteger[0] = r->apple_page_count;
          r->header.cupsInteger[7] = 0xffffff;
        }
        break;
  }

 /*
  * Update the header and row count...
  */

  if (!cups_raster_update(r))
    return (0);

  DEBUG_printf(("4cups_raster_read_header: cupsBitsPerPixel=%u, cupsBitsPerColor=%u, cupsBytesPerLine=%u, cupsWidth=%u, cupsHeight=%u, r->bpp=%d", r->header.cupsBitsPerPixel, r->header.cupsBitsPerColor, r->header.cupsBytesPerLine, r->header.cupsWidth, r->header.cupsHeight, r->bpp));

  return (r->header.cupsBitsPerPixel > 0 && r->header.cupsBitsPerPixel <= 240 && r->header.cupsBitsPerColor > 0 && r->header.cupsBitsPerColor <= 16 && r->header.cupsBytesPerLine > 0 && r->header.cupsBytesPerLine <= 0x7fffffff && r->header.cupsHeight != 0 && (r->header.cupsBytesPerLine % r->bpp) == 0);
}


/*
 * 'cups_raster_io()' - Read/write bytes from a context, handling interruptions.
 */

static ssize_t				/* O - Bytes read/write or -1 */
cups_raster_io(cups_raster_t *r,	/* I - Raster stream */
               unsigned char *buf,	/* I - Buffer for read/write */
               size_t        bytes)	/* I - Number of bytes to read/write */
{
  ssize_t	count,			/* Number of bytes read/written */
		total;			/* Total bytes read/written */


  DEBUG_printf(("5cups_raster_io(r=%p, buf=%p, bytes=" CUPS_LLFMT ")", (void *)r, (void *)buf, CUPS_LLCAST bytes));

  for (total = 0; total < (ssize_t)bytes; total += count, buf += count)
  {
    count = (*r->iocb)(r->ctx, buf, bytes - (size_t)total);

    DEBUG_printf(("6cups_raster_io: count=%d, total=%d", (int)count, (int)total));
    if (count == 0)
      break;
    else if (count < 0)
    {
      DEBUG_puts("6cups_raster_io: Returning -1 on error.");
      return (-1);
    }

#ifdef DEBUG
    r->iocount += (size_t)count;
#endif /* DEBUG */
  }

  DEBUG_printf(("6cups_raster_io: iocount=" CUPS_LLFMT, CUPS_LLCAST r->iocount));
  DEBUG_printf(("6cups_raster_io: Returning " CUPS_LLFMT ".", CUPS_LLCAST total));

  return (total);
}


/*
 * 'cups_raster_read()' - Read through the raster buffer.
 */

static ssize_t				/* O - Number of bytes read */
cups_raster_read(cups_raster_t *r,	/* I - Raster stream */
                 unsigned char *buf,	/* I - Buffer */
                 size_t        bytes)	/* I - Number of bytes to read */
{
  ssize_t	count,			/* Number of bytes read */
		remaining,		/* Remaining bytes in buffer */
		total;			/* Total bytes read */


  DEBUG_printf(("4cups_raster_read(r=%p, buf=%p, bytes=" CUPS_LLFMT "), offset=" CUPS_LLFMT, (void *)r, (void *)buf, CUPS_LLCAST bytes, CUPS_LLCAST (r->iostart + r->bufptr - r->buffer)));

  if (!r->compressed)
    return (cups_raster_io(r, buf, bytes));

 /*
  * Allocate a read buffer as needed...
  */

  count = (ssize_t)(2 * r->header.cupsBytesPerLine);
  if (count < 65536)
    count = 65536;

  if ((size_t)count > r->bufsize)
  {
    ssize_t offset = r->bufptr - r->buffer;
					/* Offset to current start of buffer */
    ssize_t end = r->bufend - r->buffer;/* Offset to current end of buffer */
    unsigned char *rptr;		/* Pointer in read buffer */

    if (r->buffer)
      rptr = realloc(r->buffer, (size_t)count);
    else
      rptr = malloc((size_t)count);

    if (!rptr)
      return (0);

    r->buffer  = rptr;
    r->bufptr  = rptr + offset;
    r->bufend  = rptr + end;
    r->bufsize = (size_t)count;
  }

 /*
  * Loop until we have read everything...
  */

  for (total = 0, remaining = (int)(r->bufend - r->bufptr);
       total < (ssize_t)bytes;
       total += count, buf += count)
  {
    count = (ssize_t)bytes - total;

    DEBUG_printf(("5cups_raster_read: count=" CUPS_LLFMT ", remaining=" CUPS_LLFMT ", buf=%p, bufptr=%p, bufend=%p", CUPS_LLCAST count, CUPS_LLCAST remaining, (void *)buf, (void *)r->bufptr, (void *)r->bufend));

    if (remaining == 0)
    {
      if (count < 16)
      {
       /*
        * Read into the raster buffer and then copy...
	*/

#ifdef DEBUG
        r->iostart += (size_t)(r->bufend - r->buffer);
#endif /* DEBUG */

        remaining = (*r->iocb)(r->ctx, r->buffer, r->bufsize);
	if (remaining <= 0)
	  return (0);

	r->bufptr = r->buffer;
	r->bufend = r->buffer + remaining;

#ifdef DEBUG
        r->iocount += (size_t)remaining;
#endif /* DEBUG */
      }
      else
      {
       /*
        * Read directly into "buf"...
	*/

	count = (*r->iocb)(r->ctx, buf, (size_t)count);

	if (count <= 0)
	  return (0);

#ifdef DEBUG
	r->iostart += (size_t)count;
        r->iocount += (size_t)count;
#endif /* DEBUG */

	continue;
      }
    }

   /*
    * Copy bytes from raster buffer to "buf"...
    */

    if (count > remaining)
      count = remaining;

    if (count == 1)
    {
     /*
      * Copy 1 byte...
      */

      *buf = *(r->bufptr)++;
      remaining --;
    }
    else if (count < 128)
    {
     /*
      * Copy up to 127 bytes without using memcpy(); this is
      * faster because it avoids an extra function call and is
      * often further optimized by the compiler...
      */

      unsigned char	*bufptr;	/* Temporary buffer pointer */

      remaining -= count;

      for (bufptr = r->bufptr; count > 0; count --, total ++)
	*buf++ = *bufptr++;

      r->bufptr = bufptr;
    }
    else
    {
     /*
      * Use memcpy() for a large read...
      */

      memcpy(buf, r->bufptr, (size_t)count);
      r->bufptr += count;
      remaining -= count;
    }
  }

  DEBUG_printf(("5cups_raster_read: Returning %ld", (long)total));

  return (total);
}


/*
 * 'cups_raster_update()' - Update the raster header and row count for the
 *                          current page.
 */

static int				/* O - 1 on success, 0 on failure */
cups_raster_update(cups_raster_t *r)	/* I - Raster stream */
{
  if (r->sync == CUPS_RASTER_SYNCv1 || r->sync == CUPS_RASTER_REVSYNCv1 ||
      r->header.cupsNumColors == 0)
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
      case CUPS_CSPACE_SW :
          r->header.cupsNumColors = 1;
	  break;

      case CUPS_CSPACE_RGB :
      case CUPS_CSPACE_CMY :
      case CUPS_CSPACE_YMC :
      case CUPS_CSPACE_CIEXYZ :
      case CUPS_CSPACE_CIELab :
      case CUPS_CSPACE_SRGB :
      case CUPS_CSPACE_ADOBERGB :
      case CUPS_CSPACE_ICC1 :
      case CUPS_CSPACE_ICC2 :
      case CUPS_CSPACE_ICC3 :
      case CUPS_CSPACE_ICC4 :
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
          r->header.cupsNumColors = 3;
	  break;

      case CUPS_CSPACE_RGBA :
      case CUPS_CSPACE_RGBW :
      case CUPS_CSPACE_CMYK :
      case CUPS_CSPACE_YMCK :
      case CUPS_CSPACE_KCMY :
      case CUPS_CSPACE_GMCK :
      case CUPS_CSPACE_GMCS :
          r->header.cupsNumColors = 4;
	  break;

      case CUPS_CSPACE_KCMYcm :
          if (r->header.cupsBitsPerPixel < 8)
            r->header.cupsNumColors = 6;
	  else
            r->header.cupsNumColors = 4;
	  break;

      case CUPS_CSPACE_DEVICE1 :
      case CUPS_CSPACE_DEVICE2 :
      case CUPS_CSPACE_DEVICE3 :
      case CUPS_CSPACE_DEVICE4 :
      case CUPS_CSPACE_DEVICE5 :
      case CUPS_CSPACE_DEVICE6 :
      case CUPS_CSPACE_DEVICE7 :
      case CUPS_CSPACE_DEVICE8 :
      case CUPS_CSPACE_DEVICE9 :
      case CUPS_CSPACE_DEVICEA :
      case CUPS_CSPACE_DEVICEB :
      case CUPS_CSPACE_DEVICEC :
      case CUPS_CSPACE_DEVICED :
      case CUPS_CSPACE_DEVICEE :
      case CUPS_CSPACE_DEVICEF :
          r->header.cupsNumColors = r->header.cupsColorSpace -
	                            CUPS_CSPACE_DEVICE1 + 1;
	  break;

      default :
          /* Unknown color space */
          return (0);
    }
  }

 /*
  * Set the number of bytes per pixel/color...
  */

  if (r->header.cupsColorOrder == CUPS_ORDER_CHUNKED)
    r->bpp = (r->header.cupsBitsPerPixel + 7) / 8;
  else
    r->bpp = (r->header.cupsBitsPerColor + 7) / 8;

  if (r->bpp == 0)
    r->bpp = 1;

 /*
  * Set the number of remaining rows...
  */

  if (r->header.cupsColorOrder == CUPS_ORDER_PLANAR)
    r->remaining = r->header.cupsHeight * r->header.cupsNumColors;
  else
    r->remaining = r->header.cupsHeight;

 /*
  * Allocate the compression buffer...
  */

  if (r->compressed)
  {
    if (r->pixels != NULL)
      free(r->pixels);

    if ((r->pixels = calloc(r->header.cupsBytesPerLine, 1)) == NULL)
    {
      r->pcurrent = NULL;
      r->pend     = NULL;
      r->count    = 0;

      return (0);
    }

    r->pcurrent = r->pixels;
    r->pend     = r->pixels + r->header.cupsBytesPerLine;
    r->count    = 0;
  }

  return (1);
}


/*
 * 'cups_raster_write()' - Write a row of compressed raster data...
 */

static ssize_t				/* O - Number of bytes written */
cups_raster_write(
    cups_raster_t       *r,		/* I - Raster stream */
    const unsigned char *pixels)	/* I - Pixel data to write */
{
  const unsigned char	*start,		/* Start of sequence */
			*ptr,		/* Current pointer in sequence */
			*pend,		/* End of raster buffer */
			*plast;		/* Pointer to last pixel */
  unsigned char		*wptr;		/* Pointer into write buffer */
  unsigned		bpp,		/* Bytes per pixel */
			count;		/* Count */
  _cups_copyfunc_t	cf;		/* Copy function */


  DEBUG_printf(("3cups_raster_write(r=%p, pixels=%p)", (void *)r, (void *)pixels));

 /*
  * Determine whether we need to swap bytes...
  */

  if (r->swapped && (r->header.cupsBitsPerColor == 16 || r->header.cupsBitsPerPixel == 12 || r->header.cupsBitsPerPixel == 16))
  {
    DEBUG_puts("4cups_raster_write: Swapping bytes when writing.");
    cf = (_cups_copyfunc_t)cups_swap_copy;
  }
  else
    cf = (_cups_copyfunc_t)memcpy;

  /*
  * Allocate a write buffer as needed...
  */

  count = r->header.cupsBytesPerLine * 2;
  if (count < 65536)
    count = 65536;

  if ((size_t)count > r->bufsize)
  {
    if (r->buffer)
      wptr = realloc(r->buffer, count);
    else
      wptr = malloc(count);

    if (!wptr)
    {
      DEBUG_printf(("4cups_raster_write: Unable to allocate " CUPS_LLFMT " bytes for raster buffer: %s", CUPS_LLCAST count, strerror(errno)));
      return (-1);
    }

    r->buffer  = wptr;
    r->bufsize = count;
  }

 /*
  * Write the row repeat count...
  */

  bpp     = r->bpp;
  pend    = pixels + r->header.cupsBytesPerLine;
  plast   = pend - bpp;
  wptr    = r->buffer;
  *wptr++ = (unsigned char)(r->count - 1);

 /*
  * Write using a modified PackBits compression...
  */

  for (ptr = pixels; ptr < pend;)
  {
    start = ptr;
    ptr += bpp;

    if (ptr == pend)
    {
     /*
      * Encode a single pixel at the end...
      */

      *wptr++ = 0;
      (*cf)(wptr, start, bpp);
      wptr += bpp;
    }
    else if (!memcmp(start, ptr, bpp))
    {
     /*
      * Encode a sequence of repeating pixels...
      */

      for (count = 2; count < 128 && ptr < plast; count ++, ptr += bpp)
        if (memcmp(ptr, ptr + bpp, bpp))
	  break;

      *wptr++ = (unsigned char)(count - 1);
      (*cf)(wptr, ptr, bpp);
      wptr += bpp;
      ptr  += bpp;
    }
    else
    {
     /*
      * Encode a sequence of non-repeating pixels...
      */

      for (count = 1; count < 128 && ptr < plast; count ++, ptr += bpp)
        if (!memcmp(ptr, ptr + bpp, bpp))
	  break;

      if (ptr >= plast && count < 128)
      {
        count ++;
	ptr += bpp;
      }

      *wptr++ = (unsigned char)(257 - count);

      count *= bpp;
      (*cf)(wptr, start, count);
      wptr += count;
    }
  }

  DEBUG_printf(("4cups_raster_write: Writing " CUPS_LLFMT " bytes.", CUPS_LLCAST (wptr - r->buffer)));

  return (cups_raster_io(r, r->buffer, (size_t)(wptr - r->buffer)));
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
    {
      DEBUG_printf(("8cups_read_fd: %s", strerror(errno)));
      return (-1);
    }

  DEBUG_printf(("8cups_read_fd: Returning %d bytes.", (int)count));

  return (count);
}


/*
 * 'cups_swap()' - Swap bytes in raster data...
 */

static void
cups_swap(unsigned char *buf,		/* I - Buffer to swap */
          size_t        bytes)		/* I - Number of bytes to swap */
{
  unsigned char	even, odd;		/* Temporary variables */


  bytes /= 2;

  while (bytes > 0)
  {
    even   = buf[0];
    odd    = buf[1];
    buf[0] = odd;
    buf[1] = even;

    buf += 2;
    bytes --;
  }
}


/*
 * 'cups_swap_copy()' - Copy and swap bytes in raster data...
 */

static void
cups_swap_copy(
    unsigned char       *dst,		/* I - Destination */
    const unsigned char *src,		/* I - Source */
    size_t              bytes)		/* I - Number of bytes to swap */
{
  bytes /= 2;

  while (bytes > 0)
  {
    dst[0] = src[1];
    dst[1] = src[0];

    dst += 2;
    src += 2;
    bytes --;
  }
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
    {
      DEBUG_printf(("8cups_write_fd: %s", strerror(errno)));
      return (-1);
    }

  return (count);
}
