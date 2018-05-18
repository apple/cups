/*
 * Private transform API implementation for CUPS.
 *
 * Copyright © 2016-2018 by Apple Inc.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more
 * information.
 */

/*
 * Include necessary headers...
 */

#include "xform-private.h"
#include "xform-dither.h"
#include "ppd-private.h"
#include "string-private.h"

#ifdef __APPLE__
#  include <CoreGraphics/CoreGraphics.h>
extern void CGContextSetCTM(CGContextRef c, CGAffineTransform m);

#elif defined(HAVE_MUPDF)
#  include <mupdf/fitz.h>
static inline fz_matrix fz_make_matrix(float a, float b, float c, float d, float e, float f) {
  fz_matrix ret = { a, b, c, d, e, f };
  return (ret);
}
#endif /* __APPLE__ */


/*
 * Constants...
 */

//#define XFORM_MAX_RASTER	16777216

#  define XFORM_RED_MASK	0x000000ff
#  define XFORM_GREEN_MASK	0x0000ff00
#  define XFORM_BLUE_MASK	0x00ff0000
#  define XFORM_RGB_MASK	(XFORM_RED_MASK | XFORM_GREEN_MASK |  XFORM_BLUE_MASK)
#  define XFORM_BG_MASK		(XFORM_BLUE_MASK | XFORM_GREEN_MASK)
#  define XFORM_RG_MASK		(XFORM_RED_MASK | XFORM_GREEN_MASK)


/*
 * Types...
 */

struct _xform_ctx_s
{
  char			*format;	/* Output format */
  xform_capabilities_t	capabilities;	/* Output capabilities */
  xform_logcb_t		logcb;		/* Logging callback */
  void			*logdata;	/* User data for logging callback */
  xform_writecb_t	writecb;	/* Write callback */
  void			*writedata;	/* User data for write callback */

  int			num_options;	/* Number of job options */
  cups_option_t		*options;	/* Job options */
  unsigned		copies;		/* Number of copies */
  cups_page_header2_t	header;		/* Page header */
  cups_page_header2_t	back_header;	/* Page header for back side */
  cups_page_header2_t	mheader;	/* Page header for monochrome pages */
  cups_page_header2_t	back_mheader;	/* Page header for monochrome pages on back side */
  int			borderless;	/* Borderless media? */
  unsigned char		*band_buffer;	/* Band buffer */
  unsigned		band_height;	/* Band height */
  unsigned		band_bpp;	/* Bytes per pixel in band */

  /* Set by start_job callback */
  cups_raster_t		*ras;		/* Raster stream */

  /* Set by start_page callback */
  unsigned		left, top, right, bottom;
					/* Image (print) box with origin at top left */
  unsigned		out_blanks;	/* Blank lines */
  size_t		out_length;	/* Output buffer size */
  unsigned char		*out_buffer;	/* Output (bit) buffer */
  unsigned char		*comp_buffer;	/* Compression buffer */

  /* Callbacks */
  void			(*end_job)(xform_ctx_t *ctx);
  void			(*end_page)(xform_ctx_t *ctx, unsigned page);
  void			(*start_job)(xform_ctx_t *ctx);
  void			(*start_page)(xform_ctx_t *ctx, unsigned page);
  void			(*write_line)(xform_ctx_t *ctx, unsigned y, const unsigned char *line);
};


/*
 * Local functions...
 */

static void	default_log_cb(void *user_data, xform_loglevel_t debug, const char *message);
static ssize_t	default_write_cb(void *user_data, const unsigned char *buffer, size_t length);

#ifdef HAVE_MUPDF
static void	pack_graya(unsigned char *row, size_t num_pixels);
#endif /* HAVE_MUPDF */
static void	pack_rgba_to_rgb(unsigned char *row, size_t num_pixels);
static void	pack_rgba_to_gray(unsigned char *row, size_t num_pixels);

static void	pcl_end_job(xform_ctx_t *ctx);
static void	pcl_end_page(xform_ctx_t *ctx, unsigned page);
static void	pcl_init(xform_ctx_t *ctx);
static void	pcl_printf(xform_ctx_t *ctx, const char *format, ...) __attribute__ ((__format__ (__printf__, 2, 3)));
static void	pcl_start_job(xform_ctx_t *ctx);
static void	pcl_start_page(xform_ctx_t *ctx, unsigned page);
static void	pcl_write_line(xform_ctx_t *ctx, unsigned y, const unsigned char *line);

static void	pdf_end_job(xform_ctx_t *ctx);
static void	pdf_end_page(xform_ctx_t *ctx, unsigned page);
static void	pdf_init(xform_ctx_t *ctx);
static void	pdf_start_job(xform_ctx_t *ctx);
static void	pdf_start_page(xform_ctx_t *ctx, unsigned page);

static void	png_end_job(xform_ctx_t *ctx);
static void	png_end_page(xform_ctx_t *ctx, unsigned page);
static void	png_init(xform_ctx_t *ctx);
static void	png_start_job(xform_ctx_t *ctx);
static void	png_start_page(xform_ctx_t *ctx, unsigned page);
static void	png_write_line(xform_ctx_t *ctx, unsigned y, const unsigned char *line);

static void	ps_end_job(xform_ctx_t *ctx);
static void	ps_end_page(xform_ctx_t *ctx, unsigned page);
static void	ps_init(xform_ctx_t *ctx);
static void	ps_start_job(xform_ctx_t *ctx);
static void	ps_start_page(xform_ctx_t *ctx, unsigned page);

static void	raster_end_job(xform_ctx_t *ctx);
static void	raster_end_page(xform_ctx_t *ctx, unsigned page);
static void	raster_init(xform_ctx_t *ctx);
static void	raster_start_job(xform_ctx_t *ctx);
static void	raster_start_page(xform_ctx_t *ctx, unsigned page);
static void	raster_write_line(xform_ctx_t *ctx, unsigned y, const unsigned char *line);

static void	xform_log(xform_ctx_t *ctx, xform_loglevel_t level, const char *message, ...);

static int	xform_document(const char *filename, const char *informat, const char *outformat, const char *resolutions, const char *sheet_back, const char *types, int num_options, cups_option_t *options, xform_writecb_t cb, void *ctx);
static int	xform_setup(xform_ctx_t *ras, const char *outformat, const char *resolutions, const char *types, const char *sheet_back, int color, unsigned pages, int num_options, cups_option_t *options);




/*
 * 'xformDelete()' - Free memory associated with a transform context.
 */

void
xformDelete(xform_ctx_t *ctx)		/* I - Transform context */
{
  free(ctx->format);
  cupsFreeOptions(ctx->num_options, ctx->options);
  free(ctx);
}


/*
 * 'xformNew()' - Create a new transform context.
 */

xform_ctx_t *				/* O - New transform context */
xformNew(
    const char           *outformat,	/* I - Output MIME media type */
    xform_capabilities_t *outcaps)	/* I - Output capabilities */
{
  xform_ctx_t	*ctx;			/* New context */


  if ((ctx = (xform_ctx_t *)calloc(1, sizeof(xform_ctx_t))) != NULL)
  {
    ctx->format       = strdup(outformat);
    ctx->capabilities = *outcaps;
    xformSetLogCallback(ctx, NULL, NULL);
    xformSetWriteCallback(ctx, NULL, NULL);

    if (!_cups_strcasecmp(outformat, XFORM_FORMAT_APPLE_RASTER) || !_cups_strcasecmp(outformat, XFORM_FORMAT_PWG_RASTER))
      raster_init(ctx);
    else if (!_cups_strcasecmp(outformat, XFORM_FORMAT_PCL))
      pcl_init(ctx);
    else if (!_cups_strcasecmp(outformat, XFORM_FORMAT_PDF))
      pdf_init(ctx);
    else if (!_cups_strcasecmp(outformat, XFORM_FORMAT_PNG))
      png_init(ctx);			/* For first-page previews */
    else if (!_cups_strcasecmp(outformat, XFORM_FORMAT_POSTSCRIPT))
      ps_init(ctx);
    else
    {
      free(ctx);
      return (NULL);
    }
  }

  return (ctx);
}


/*
 * 'xformRun()' - Transform a file.
 */

int					/* O - 1 on success, 0 on failure */
xformRun(xform_ctx_t *ctx,		/* I - Transform context */
         const char  *infile,		/* I - Input filename or `NULL` for `stdin` */
         const char  *informat)		/* I - Input MIME media type */
{
  return (0);
}


/*
 * 'xformSetLogCallback()' - Set the logging callback.
 */

void
xformSetLogCallback(
    xform_ctx_t   *ctx,			/* I - Transform context */
    xform_logcb_t logcb,		/* I - Logging callback */
    void          *logdata)		/* I - User data pointer for callback */
{
  if (logcb)
  {
    ctx->logcb   = logcb;
    ctx->logdata = logdata;
  }
  else
  {
    ctx->logcb   = default_log_cb;
    ctx->logdata = NULL;
  }
}


/*
 * 'xformSetOptions()' - Set transform options.
 */

void
xformSetOptions(
    xform_ctx_t   *ctx,			/* I - Transform context */
    int           num_options,		/* I - Number of options */
    cups_option_t *options)		/* I - Options */
{
  cupsFreeOptions(ctx->num_options, ctx->options);

  ctx->num_options = 0;
  ctx->options     = NULL;

  while (num_options > 0)
  {
    ctx->num_options = cupsAddOption(options->name, options->value, ctx->num_options, &ctx->options);
    options ++;
    num_options --;
  }
}


/*
 * 'xformSetWriteCallback()' - Set the output callback.
 */

void
xformSetWriteCallback(
    xform_ctx_t     *ctx,		/* I - Transform context */
    xform_writecb_t writecb,		/* I - Write callback */
    void            *writedata)		/* I - User data pointer for callback */
{
  if (writecb)
  {
    ctx->writecb   = writecb;
    ctx->writedata = writedata;
  }
  else
  {
    ctx->writecb   = default_write_cb;
    ctx->writedata = NULL;
  }
}


/*
 * 'default_log_cb()' - Default logging callback (to stderr).
 */

static void
default_log_cb(
    void             *user_data,	/* I - User data pointer (unused) */
    xform_loglevel_t level,		/* I - Log level */
    const char       *message)		/* I - Message */
{
  static const char * const levels[] =	/* Log level prefixes */
  {
    "DEBUG: ",
    "INFO: ",
    "ERROR: ",
    "ATTR: "
  };


  (void)user_data;

  fprintf(stderr, "%s%s\n", levels[level], message);
}


/*
 * 'default_write_cb()' - Default output callback (to stdout).
 */

static ssize_t				/* O - Number of bytes written */
default_write_cb(
    void                *user_data,	/* I - User data pointer (unused) */
    const unsigned char *buffer,	/* I - Buffer to write */
    size_t              length)		/* I - Number of bytes */
{
  (void)user_data;

  return (write(1, buffer, length));
}


#ifdef HAVE_MUPDF
/*
 * 'pack_graya()' - Pack GRAYX scanlines into GRAY scanlines.
 *
 * This routine is suitable only for 8 bit GRAYX data packed into GRAY bytes.
 */

static void
pack_graya(unsigned char *row,		/* I - Row of pixels to pack */
	   size_t        num_pixels)	/* I - Number of pixels in row */
{
  unsigned char *src_byte;		/* Remaining source bytes */
  unsigned char *dest_byte;		/* Remaining destination bytes */


  for (src_byte = row + 2, dest_byte = row + 1, num_pixels --; num_pixels > 0; num_pixels --, src_byte += 2)
    *dest_byte++ = *src_byte;
}
#endif /* HAVE_MUPDF */


/*
 * 'pack_rgba_to_rgb()' - Pack RGBX scanlines into RGB scanlines.
 *
 * This routine is suitable only for 8 bit RGBX data packed into RGB bytes.
 */

static void
pack_rgba_to_rgb(
    unsigned char *row,			/* I - Row of pixels to pack */
    size_t        num_pixels)		/* I - Number of pixels in row */
{
  size_t	num_quads = num_pixels / 4;
					/* Number of 4 byte samples to pack */
  size_t	leftover_pixels = num_pixels & 3;
					/* Number of pixels remaining */
  unsigned	*quad_row = (unsigned *)row;
					/* 32-bit pixel pointer */
  unsigned	*dest = quad_row;	/* Destination pointer */
  unsigned char *src_byte;		/* Remaining source bytes */
  unsigned char *dest_byte;		/* Remaining destination bytes */


 /*
  * Copy all of the groups of 4 pixels we can...
  */

  while (num_quads > 0)
  {
    *dest++ = (quad_row[0] & XFORM_RGB_MASK) | (quad_row[1] << 24);
    *dest++ = ((quad_row[1] & XFORM_BG_MASK) >> 8) |
              ((quad_row[2] & XFORM_RG_MASK) << 16);
    *dest++ = ((quad_row[2] & XFORM_BLUE_MASK) >> 16) | (quad_row[3] << 8);
    quad_row += 4;
    num_quads --;
  }

 /*
  * Then handle the leftover pixels...
  */

  src_byte  = (unsigned char *)quad_row;
  dest_byte = (unsigned char *)dest;

  while (leftover_pixels > 0)
  {
    *dest_byte++ = *src_byte++;
    *dest_byte++ = *src_byte++;
    *dest_byte++ = *src_byte++;
    src_byte ++;
    leftover_pixels --;
  }
}


/*
 * 'pack_rgba_to_gray()' - Pack RGBX scanlines into GRAY scanlines.
 *
 * This routine is suitable only for 8 bit RGBX data packed into GRAY bytes.
 */

static void
pack_rgba_to_gray(
    unsigned char *row,			/* I - Row of pixels to pack */
    size_t        num_pixels)		/* I - Number of pixels in row */
{
  unsigned char *src_byte;		/* Remaining source bytes */
  unsigned char *dest_byte;		/* Remaining destination bytes */


  for (src_byte = row + 3, dest_byte = row; num_pixels > 0; num_pixels --, src_byte += 4)
    *dest_byte++ = *src_byte;
}


/*
 * 'pcl_end_job()' - End a PCL "job".
 */

static void
pcl_end_job(xform_ctx_t *ctx)		/* I - Transform context */
{
 /*
  * Send a PCL reset sequence.
  */

  (*ctx->writecb)(ctx->writedata, (const unsigned char *)"\033E", 2);
}


/*
 * 'pcl_end_page()' - End of PCL page.
 */

static void
pcl_end_page(xform_ctx_t *ctx,		/* I - Transform context */
	     unsigned    page)		/* I - Current page */
{
 /*
  * End graphics...
  */

  (*ctx->writecb)(ctx->writedata, (const unsigned char *)"\033*r0B", 5);

 /*
  * Formfeed as needed...
  */

  if (!(ctx->header.Duplex && (page & 1)))
    (*ctx->writecb)(ctx->writedata, (const unsigned char *)"\014", 1);

 /*
  * Free the output buffer...
  */

  free(ctx->out_buffer);
  ctx->out_buffer = NULL;
}


/*
 * 'pcl_init()' - Initialize callbacks for PCL output.
 */

static void
pcl_init(xform_ctx_t *ctx)		/* I - Transform context */
{
  ctx->end_job    = pcl_end_job;
  ctx->end_page   = pcl_end_page;
  ctx->start_job  = pcl_start_job;
  ctx->start_page = pcl_start_page;
  ctx->write_line = pcl_write_line;
}


/*
 * 'pcl_printf()' - Write a formatted string.
 */

static void
pcl_printf(xform_ctx_t *ctx,		/* I - Transform context */
	   const char  *format,		/* I - Printf-style format string */
	   ...)				/* I - Additional arguments as needed */
{
  va_list	ap;			/* Argument pointer */
  char		buffer[8192];		/* Buffer */


  va_start(ap, format);
  vsnprintf(buffer, sizeof(buffer), format, ap);
  va_end(ap);

  (*ctx->writecb)(ctx->writedata, (const unsigned char *)buffer, strlen(buffer));
}


/*
 * 'pcl_start_job()' - Start a PCL "job".
 */

static void
pcl_start_job(xform_ctx_t *ctx)		/* I - Transform context */
{
 /*
  * Send a PCL reset sequence.
  */

  (*ctx->writecb)(ctx->writedata, (const unsigned char *)"\033E", 2);
}


/*
 * 'pcl_start_page()' - Start a PCL page.
 */

static void
pcl_start_page(xform_ctx_t *ras,	/* I - Raster information */
               unsigned    page)	/* I - Current page */
{
 /*
  * Setup margins to be 1/6" top and bottom and 1/4" or .135" on the
  * left and right.
  */

  ras->top    = ras->header.HWResolution[1] / 6;
  ras->bottom = ras->header.cupsHeight - ras->header.HWResolution[1] / 6 - 1;

  if (ras->header.PageSize[1] == 842)
  {
   /* A4 gets special side margins to expose an 8" print area */
    ras->left  = (ras->header.cupsWidth - 8 * ras->header.HWResolution[0]) / 2;
    ras->right = ras->left + 8 * ras->header.HWResolution[0] - 1;
  }
  else
  {
   /* All other sizes get 1/4" margins */
    ras->left  = ras->header.HWResolution[0] / 4;
    ras->right = ras->header.cupsWidth - ras->header.HWResolution[0] / 4 - 1;
  }

  if (!ras->header.Duplex || (page & 1))
  {
   /*
    * Set the media size...
    */

    pcl_printf(ras, "\033&l12D\033&k12H");
					/* Set 12 LPI, 10 CPI */
    pcl_printf(ras, "\033&l0O");	/* Set portrait orientation */

    switch (ras->header.PageSize[1])
    {
      case 540 : /* Monarch Envelope */
          pcl_printf(ras, "\033&l80A");
	  break;

      case 595 : /* A5 */
          pcl_printf(ras, "\033&l25A");
	  break;

      case 624 : /* DL Envelope */
          pcl_printf(ras, "\033&l90A");
	  break;

      case 649 : /* C5 Envelope */
          pcl_printf(ras, "\033&l91A");
	  break;

      case 684 : /* COM-10 Envelope */
          pcl_printf(ras, "\033&l81A");
	  break;

      case 709 : /* B5 Envelope */
          pcl_printf(ras, "\033&l100A");
	  break;

      case 756 : /* Executive */
          pcl_printf(ras, "\033&l1A");
	  break;

      case 792 : /* Letter */
          pcl_printf(ras, "\033&l2A");
	  break;

      case 842 : /* A4 */
          pcl_printf(ras, "\033&l26A");
	  break;

      case 1008 : /* Legal */
          pcl_printf(ras, "\033&l3A");
	  break;

      case 1191 : /* A3 */
          pcl_printf(ras, "\033&l27A");
	  break;

      case 1224 : /* Tabloid */
          pcl_printf(ras, "\033&l6A");
	  break;
    }

   /*
    * Set top margin and turn off perforation skip...
    */

    pcl_printf(ras, "\033&l%uE\033&l0L", 12 * ras->top / ras->header.HWResolution[1]);

    if (ras->header.Duplex)
    {
      int mode = ras->header.Duplex ? 1 + ras->header.Tumble != 0 : 0;

      pcl_printf(ras, "\033&l%dS", mode);
					/* Set duplex mode */
    }
  }
  else if (ras->header.Duplex)
    pcl_printf(ras, "\033&a2G");	/* Print on back side */

 /*
  * Set graphics mode...
  */

  pcl_printf(ras, "\033*t%uR", ras->header.HWResolution[0]);
					/* Set resolution */
  pcl_printf(ras, "\033*r%uS", ras->right - ras->left + 1);
					/* Set width */
  pcl_printf(ras, "\033*r%uT", ras->bottom - ras->top + 1);
					/* Set height */
  pcl_printf(ras, "\033&a0H\033&a%uV", 720 * ras->top / ras->header.HWResolution[1]);
					/* Set position */

  pcl_printf(ras, "\033*b2M");	/* Use PackBits compression */
  pcl_printf(ras, "\033*r1A");	/* Start graphics */

 /*
  * Allocate the output buffer...
  */

  ras->out_blanks  = 0;
  ras->out_length  = (ras->right - ras->left + 8) / 8;
  ras->out_buffer  = malloc(ras->out_length);
  ras->comp_buffer = malloc(2 * ras->out_length + 2);
}


/*
 * 'pcl_write_line()' - Write a line of raster data.
 */

static void
pcl_write_line(
    xform_ctx_t         *ctx,		/* I - Transform context */
    unsigned            y,		/* I - Line number */
    const unsigned char *line)		/* I - Pixels on line */
{
  unsigned	x;			/* Column number */
  unsigned char	bit,			/* Current bit */
		byte,			/* Current byte */
		*outptr,		/* Pointer into output buffer */
		*outend,		/* End of output buffer */
		*start,			/* Start of sequence */
		*compptr;		/* Pointer into compression buffer */
  unsigned	count;			/* Count of bytes for output */


  if (line[0] == 255 && !memcmp(line, line + 1, ctx->right - ctx->left))
  {
   /*
    * Skip blank line...
    */

    ctx->out_blanks ++;
    return;
  }

 /*
  * Dither the line into the output buffer...
  */

  y &= 63;

  for (x = ctx->left, bit = 128, byte = 0, outptr = ctx->out_buffer; x <= ctx->right; x ++, line ++)
  {
    if (*line <= threshold[x & 63][y])
      byte |= bit;

    if (bit == 1)
    {
      *outptr++ = byte;
      byte      = 0;
      bit       = 128;
    }
    else
      bit >>= 1;
  }

  if (bit != 128)
    *outptr++ = byte;

 /*
  * Apply compression...
  */

  compptr = ctx->comp_buffer;
  outend  = outptr;
  outptr  = ctx->out_buffer;

  while (outptr < outend)
  {
    if ((outptr + 1) >= outend)
    {
     /*
      * Single byte on the end...
      */

      *compptr++ = 0x00;
      *compptr++ = *outptr++;
    }
    else if (outptr[0] == outptr[1])
    {
     /*
      * Repeated sequence...
      */

      outptr ++;
      count = 2;

      while (outptr < (outend - 1) &&
	     outptr[0] == outptr[1] &&
	     count < 127)
      {
	outptr ++;
	count ++;
      }

      *compptr++ = (unsigned char)(257 - count);
      *compptr++ = *outptr++;
    }
    else
    {
     /*
      * Non-repeated sequence...
      */

      start = outptr;
      outptr ++;
      count = 1;

      while (outptr < (outend - 1) &&
	     outptr[0] != outptr[1] &&
	     count < 127)
      {
	outptr ++;
	count ++;
      }

      *compptr++ = (unsigned char)(count - 1);

      memcpy(compptr, start, count);
      compptr += count;
    }
  }

 /*
  * Output the line...
  */

  if (ctx->out_blanks > 0)
  {
   /*
    * Skip blank lines first...
    */

    pcl_printf(ctx, "\033*b%dY", ctx->out_blanks);
    ctx->out_blanks = 0;
  }

  pcl_printf(ctx, "\033*b%dW", (int)(compptr - ctx->comp_buffer));
  (*ctx->writecb)(ctx->writedata, ctx->comp_buffer, (size_t)(compptr - ctx->comp_buffer));
}


/*
 * 'pdf_end_job()' - End a PDF "job".
 */

static void
pdf_end_job(xform_ctx_t *ctx)		/* I - Transform context */
{
}


/*
 * 'pdf_end_page()' - End a PDF page.
 */

static void
pdf_end_page(xform_ctx_t *ctx,		/* I - Transform context */
             unsigned    page)		/* I - Page number */
{
}


/*
 * 'pdf_init()' - Initialize PDF output.
 */

static void
pdf_init(xform_ctx_t *ctx)		/* I - Transform context */
{
  ctx->end_job    = pdf_end_job;
  ctx->end_page   = pdf_end_page;
  ctx->start_job  = pdf_start_job;
  ctx->start_page = pdf_start_page;
}


/*
 * 'pdf_start_job()' - Start a PDF "job".
 */

static void
pdf_start_job(xform_ctx_t *ctx)		/* I - Transform context */
{
}


/*
 * 'pdf_start_page()' - Start a PDF page.
 */

static void
pdf_start_page(xform_ctx_t *ctx,	/* I - Transform context */
               unsigned    page)	/* I - Page number */
{
}


/*
 * 'png_end_job()' - End a PNG "job".
 */

static void
png_end_job(xform_ctx_t *ctx)		/* I - Transform context */
{
}


/*
 * 'png_end_page()' - End a PNG page.
 */

static void
png_end_page(xform_ctx_t *ctx,		/* I - Transform context */
             unsigned    page)		/* I - Page number */
{
}


/*
 * 'png_init()' - Initialize PNG output.
 */

static void
png_init(xform_ctx_t *ctx)		/* I - Transform context */
{
  ctx->end_job    = png_end_job;
  ctx->end_page   = png_end_page;
  ctx->start_job  = png_start_job;
  ctx->start_page = png_start_page;
  ctx->write_line = png_write_line;
}


/*
 * 'png_start_job()' - Start a PNG "job".
 */

static void
png_start_job(xform_ctx_t *ctx)		/* I - Transform context */
{
}


/*
 * 'png_start_page()' - Start a PNG page.
 */

static void
png_start_page(xform_ctx_t *ctx,	/* I - Transform context */
               unsigned    page)	/* I - Page number */
{
}


/*
 * 'png_write_line()' - Write a line on a page.
 */

static void
png_write_line(
    xform_ctx_t         *ctx,		/* I - Transform context */
    unsigned            y,		/* I - Line number */
    const unsigned char *line)		/* I - Pixels */
{
}


/*
 * 'ps_end_job()' - End a PostScript "job".
 */

static void
ps_end_job(xform_ctx_t *ctx)		/* I - Transform context */
{
}


/*
 * 'ps_end_page()' - End a PostScript page.
 */

static void
ps_end_page(xform_ctx_t *ctx,		/* I - Transform context */
            unsigned    page)		/* I - Page number */
{
}


/*
 * 'ps_init()' - Initialize a PostScript output context.
 */

static void
ps_init(xform_ctx_t *ctx)		/* I - Transform context */
{
  ctx->end_job    = ps_end_job;
  ctx->end_page   = ps_end_page;
  ctx->start_job  = ps_start_job;
  ctx->start_page = ps_start_page;
}


/*
 * 'ps_start_job()' - Start a PostScript "job".
 */

static void
ps_start_job(xform_ctx_t *ctx)		/* I - Transform context */
{
}


/*
 * 'ps_start_page()' - Start a PostScript page.
 */

static void
ps_start_page(xform_ctx_t *ctx,		/* I - Transform context */
              unsigned    page)		/* I - Page number */
{
}


/*
 * 'raster_end_job()' - End a raster "job".
 */

static void
raster_end_job(xform_ctx_t *ctx)	/* I - Transform context */
{
  cupsRasterClose(ctx->ras);
}


/*
 * 'raster_end_page()' - End of raster page.
 */

static void
raster_end_page(xform_ctx_t *ctx,	/* I - Transform context */
	        unsigned    page)	/* I - Current page */
{
  (void)page;

  if (ctx->header.cupsBitsPerPixel == 1)
  {
    free(ctx->out_buffer);
    ctx->out_buffer = NULL;
  }
}


/*
 * 'raster_init()' - Initialize callbacks for raster output.
 */

static void
raster_init(xform_ctx_t *ctx)		/* I - Transform context */
{
  ctx->end_job    = raster_end_job;
  ctx->end_page   = raster_end_page;
  ctx->start_job  = raster_start_job;
  ctx->start_page = raster_start_page;
  ctx->write_line = raster_write_line;
}


/*
 * 'raster_start_job()' - Start a raster "job".
 */

static void
raster_start_job(xform_ctx_t *ctx)	/* I - Transform context */
{
  ctx->ras = cupsRasterOpenIO((cups_raster_iocb_t)ctx->writecb, ctx->writedata, !strcmp(ctx->format, "image/pwg-raster") ? CUPS_RASTER_WRITE_PWG : CUPS_RASTER_WRITE_APPLE);
}


/*
 * 'raster_start_page()' - Start a raster page.
 */

static void
raster_start_page(xform_ctx_t *ctx,	/* I - Transform context */
		  unsigned    page)	/* I - Current page */
{
  ctx->left   = 0;
  ctx->top    = 0;
  ctx->right  = ctx->header.cupsWidth - 1;
  ctx->bottom = ctx->header.cupsHeight - 1;

  if (ctx->header.Duplex && !(page & 1))
    cupsRasterWriteHeader2(ctx->ras, &ctx->back_header);
  else
    cupsRasterWriteHeader2(ctx->ras, &ctx->header);

  if (ctx->header.cupsBitsPerPixel == 1)
  {
    ctx->out_length = ctx->header.cupsBytesPerLine;
    ctx->out_buffer = malloc(ctx->header.cupsBytesPerLine);
  }
}


/*
 * 'raster_write_line()' - Write a line of raster data.
 */

static void
raster_write_line(
    xform_ctx_t         *ctx,		/* I - Transform context */
    unsigned            y,		/* I - Line number */
    const unsigned char *line)		/* I - Pixels on line */
{
  if (ctx->header.cupsBitsPerPixel == 1)
  {
   /*
    * Dither the line into the output buffer...
    */

    unsigned		x;		/* Column number */
    unsigned char	bit,		/* Current bit */
			byte,		/* Current byte */
			*outptr;	/* Pointer into output buffer */

    y &= 63;

    if (ctx->header.cupsColorSpace == CUPS_CSPACE_SW)
    {
      for (x = ctx->left, bit = 128, byte = 0, outptr = ctx->out_buffer; x <= ctx->right; x ++, line ++)
      {
	if (*line > threshold[x % 25][y])
	  byte |= bit;

	if (bit == 1)
	{
	  *outptr++ = byte;
	  byte      = 0;
	  bit       = 128;
	}
	else
	  bit >>= 1;
      }
    }
    else
    {
      for (x = ctx->left, bit = 128, byte = 0, outptr = ctx->out_buffer; x <= ctx->right; x ++, line ++)
      {
	if (*line <= threshold[x & 63][y])
	  byte |= bit;

	if (bit == 1)
	{
	  *outptr++ = byte;
	  byte      = 0;
	  bit       = 128;
	}
	else
	  bit >>= 1;
      }
    }

    if (bit != 128)
      *outptr++ = byte;

    cupsRasterWritePixels(ctx->ras, ctx->out_buffer, ctx->header.cupsBytesPerLine);
  }
  else
    cupsRasterWritePixels(ctx->ras, (unsigned char *)line, ctx->header.cupsBytesPerLine);
}


/*
 * 'xform_log()' - Log a message.
 */

static void
xform_log(xform_ctx_t      *ctx,	/* I - Transform context */
          xform_loglevel_t level,	/* I - Log level */
          const char       *message,	/* I - Printf-style message */
          ...)				/* I - Additional arguments as needed */
{
  va_list	ap;			/* Pointer to additional arguments */
  char		buffer[2048];		/* Message buffer */


  va_start(ap, message);
  vsnprintf(buffer, sizeof(buffer), message, ap);
  va_end(ap);

  (ctx->logcb)(ctx->logdata, level, buffer);
}


#if 0
/*
 * 'main()' - Main entry for transform utility.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  int		i;			/* Looping var */
  const char	*filename = NULL,	/* File to transform */
		*content_type,		/* Source content type */
		*device_uri,		/* Destination URI */
		*output_type,		/* Destination content type */
		*resolutions,		/* pwg-raster-document-resolution-supported */
		*sheet_back,		/* pwg-raster-document-sheet-back */
		*types,			/* pwg-raster-document-type-supported */
		*opt;			/* Option character */
  int		num_options;		/* Number of options */
  cups_option_t	*options;		/* Options */
  int		fd = 1;			/* Output file/socket */
  http_t	*http = NULL;		/* Output HTTP connection */
  void		*write_ptr = &fd;	/* Pointer to file/socket/HTTP connection */
  char		resource[1024];		/* URI resource path */
  xform_writecb_t write_cb = (xform_writecb_t)write_fd;
					/* Write callback */
  int		status = 0;		/* Exit status */
  _cups_thread_t monitor = 0;		/* Monitoring thread ID */


 /*
  * Process the command-line...
  */

  num_options  = load_env_options(&options);
  content_type = getenv("CONTENT_TYPE");
  device_uri   = getenv("DEVICE_URI");
  output_type  = getenv("OUTPUT_TYPE");
  resolutions  = getenv("PWG_RASTER_DOCUMENT_RESOLUTION_SUPPORTED");
  sheet_back   = getenv("PWG_RASTER_DOCUMENT_SHEET_BACK");
  types        = getenv("PWG_RASTER_DOCUMENT_TYPE_SUPPORTED");

  if ((opt = getenv("SERVER_LOGLEVEL")) != NULL)
  {
    if (!strcmp(opt, "debug"))
      Verbosity = 2;
    else if (!strcmp(opt, "info"))
      Verbosity = 1;
  }

  for (i = 1; i < argc; i ++)
  {
    if (!strncmp(argv[i], "--", 2))
    {
      if (!strcmp(argv[i], "--help"))
      {
        usage(0);
      }
      else if (!strcmp(argv[i], "--version"))
      {
        puts(CUPS_SVERSION);
      }
      else
      {
	fprintf(stderr, "ERROR: Unknown option '%s'.\n", argv[i]);
	usage(1);
      }
    }
    else if (argv[i][0] == '-')
    {
      for (opt = argv[i] + 1; *opt; opt ++)
      {
        switch (*opt)
	{
	  case 'd' :
	      i ++;
	      if (i >= argc)
	        usage(1);

	      device_uri = argv[i];
	      break;

	  case 'i' :
	      i ++;
	      if (i >= argc)
	        usage(1);

	      content_type = argv[i];
	      break;

	  case 'm' :
	      i ++;
	      if (i >= argc)
	        usage(1);

	      output_type = argv[i];
	      break;

	  case 'o' :
	      i ++;
	      if (i >= argc)
	        usage(1);

	      num_options = cupsParseOptions(argv[i], num_options, &options);
	      break;

	  case 'r' : /* pwg-raster-document-resolution-supported values */
	      i ++;
	      if (i >= argc)
	        usage(1);

	      resolutions = argv[i];
	      break;

	  case 's' : /* pwg-raster-document-sheet-back value */
	      i ++;
	      if (i >= argc)
	        usage(1);

	      sheet_back = argv[i];
	      break;

	  case 't' : /* pwg-raster-document-type-supported values */
	      i ++;
	      if (i >= argc)
	        usage(1);

	      types = argv[i];
	      break;

	  case 'v' : /* Be verbose... */
	      Verbosity ++;
	      break;

	  default :
	      fprintf(stderr, "ERROR: Unknown option '-%c'.\n", *opt);
	      usage(1);
	      break;
	}
      }
    }
    else if (!filename)
      filename = argv[i];
    else
      usage(1);
  }

 /*
  * Check that we have everything we need...
  */

  if (!filename)
    usage(1);

  if (!content_type)
  {
    if ((opt = strrchr(filename, '.')) != NULL)
    {
      if (!strcmp(opt, ".pdf"))
        content_type = "application/pdf";
      else if (!strcmp(opt, ".jpg") || !strcmp(opt, ".jpeg"))
        content_type = "image/jpeg";
    }
  }

  if (!content_type)
  {
    fprintf(stderr, "ERROR: Unknown format for \"%s\", please specify with '-i' option.\n", filename);
    usage(1);
  }
  else if (strcmp(content_type, "application/pdf") && strcmp(content_type, "image/jpeg"))
  {
    fprintf(stderr, "ERROR: Unsupported format \"%s\" for \"%s\".\n", content_type, filename);
    usage(1);
  }

  if (!output_type)
  {
    fputs("ERROR: Unknown output format, please specify with '-m' option.\n", stderr);
    usage(1);
  }
  else if (strcmp(output_type, "application/vnd.hp-pcl") && strcmp(output_type, "image/pwg-raster") && strcmp(output_type, "image/urf"))
  {
    fprintf(stderr, "ERROR: Unsupported output format \"%s\".\n", output_type);
    usage(1);
  }

  if (!resolutions)
    resolutions = "300dpi";
  if (!sheet_back)
    sheet_back = "normal";
  if (!types)
    types = "sgray_8";

 /*
  * If the device URI is specified, open the connection...
  */

  if (device_uri)
  {
    char	scheme[32],		/* URI scheme */
		userpass[256],		/* URI user:pass */
		host[256],		/* URI host */
		service[32];		/* Service port */
    int		port;			/* URI port number */
    http_addrlist_t *list;		/* Address list for socket */

    if (httpSeparateURI(HTTP_URI_CODING_ALL, device_uri, scheme, sizeof(scheme), userpass, sizeof(userpass), host, sizeof(host), &port, resource, sizeof(resource)) < HTTP_URI_STATUS_OK)
    {
      fprintf(stderr, "ERROR: Invalid device URI \"%s\".\n", device_uri);
      usage(1);
    }

    if (strcmp(scheme, "socket") && strcmp(scheme, "ipp") && strcmp(scheme, "ipps"))
    {
      fprintf(stderr, "ERROR: Unsupported device URI scheme \"%s\".\n", scheme);
      usage(1);
    }

    snprintf(service, sizeof(service), "%d", port);
    if ((list = httpAddrGetList(host, AF_UNSPEC, service)) == NULL)
    {
      fprintf(stderr, "ERROR: Unable to lookup device URI host \"%s\": %s\n", host, cupsLastErrorString());
      return (1);
    }

    if (!strcmp(scheme, "socket"))
    {
     /*
      * AppSocket connection...
      */

      if (!httpAddrConnect2(list, &fd, 30000, NULL))
      {
	fprintf(stderr, "ERROR: Unable to connect to \"%s\" on port %d: %s\n", host, port, cupsLastErrorString());
	return (1);
      }
    }
    else
    {
      http_encryption_t encryption;	/* Encryption mode */
      ipp_t		*request,	/* IPP request */
			*response;	/* IPP response */
      ipp_attribute_t	*attr;		/* operations-supported */
      int		create_job = 0;	/* Support for Create-Job/Send-Document? */
      const char	*job_name;	/* Title of job */
      const char	*media;		/* Value of "media" option */
      const char	*sides;		/* Value of "sides" option */

     /*
      * Connect to the IPP/IPPS printer...
      */

      if (port == 443 || !strcmp(scheme, "ipps"))
        encryption = HTTP_ENCRYPTION_ALWAYS;
      else
        encryption = HTTP_ENCRYPTION_IF_REQUESTED;

      if ((http = httpConnect2(host, port, list, AF_UNSPEC, encryption, 1, 30000, NULL)) == NULL)
      {
	fprintf(stderr, "ERROR: Unable to connect to \"%s\" on port %d: %s\n", host, port, cupsLastErrorString());
	return (1);
      }

     /*
      * See if it supports Create-Job + Send-Document...
      */

      request = ippNewRequest(IPP_OP_GET_PRINTER_ATTRIBUTES);
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, device_uri);
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "requested-attributes", NULL, "operations-supported");

      response = cupsDoRequest(http, request, resource);
      if (cupsLastError() > IPP_STATUS_OK_EVENTS_COMPLETE)
      {
        fprintf(stderr, "ERROR: Unable to get printer capabilities: %s\n", cupsLastErrorString());
	return (1);
      }

      if ((attr = ippFindAttribute(response, "operations-supported", IPP_TAG_ENUM)) == NULL)
      {
        fputs("ERROR: Unable to get list of supported operations from printer.\n", stderr);
	return (1);
      }

      create_job = ippContainsInteger(attr, IPP_OP_CREATE_JOB) && ippContainsInteger(attr, IPP_OP_SEND_DOCUMENT);

      ippDelete(response);

     /*
      * Create the job and start printing...
      */

      if ((job_name = getenv("IPP_JOB_NAME")) == NULL)
      {
	if ((job_name = strrchr(filename, '/')) != NULL)
	  job_name ++;
	else
	  job_name = filename;
      }

      if (create_job)
      {
        int		job_id = 0;	/* Job ID */

        request = ippNewRequest(IPP_OP_CREATE_JOB);
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, device_uri);
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "job-name", NULL, job_name);

        response = cupsDoRequest(http, request, resource);
        if ((attr = ippFindAttribute(response, "job-id", IPP_TAG_INTEGER)) != NULL)
	  job_id = ippGetInteger(attr, 0);
        ippDelete(response);

	if (cupsLastError() > IPP_STATUS_OK_EVENTS_COMPLETE)
	{
	  fprintf(stderr, "ERROR: Unable to create print job: %s\n", cupsLastErrorString());
	  return (1);
	}
	else if (job_id <= 0)
	{
          fputs("ERROR: No job-id for created print job.\n", stderr);
	  return (1);
	}

        request = ippNewRequest(IPP_OP_SEND_DOCUMENT);
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, device_uri);
	ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "job-id", job_id);
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_MIMETYPE, "document-format", NULL, output_type);
        ippAddBoolean(request, IPP_TAG_OPERATION, "last-document", 1);
      }
      else
      {
        request = ippNewRequest(IPP_OP_PRINT_JOB);
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, device_uri);
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_MIMETYPE, "document-format", NULL, output_type);
      }

      if ((media = cupsGetOption("media", num_options, options)) != NULL)
        ippAddString(request, IPP_TAG_JOB, IPP_TAG_KEYWORD, "media", NULL, media);

      if ((sides = cupsGetOption("sides", num_options, options)) != NULL)
        ippAddString(request, IPP_TAG_JOB, IPP_TAG_KEYWORD, "sides", NULL, sides);

      if (cupsSendRequest(http, request, resource, 0) != HTTP_STATUS_CONTINUE)
      {
        fprintf(stderr, "ERROR: Unable to send print data: %s\n", cupsLastErrorString());
	return (1);
      }

      ippDelete(request);

      write_cb  = (xform_writecb_t)httpWrite2;
      write_ptr = http;

      monitor = _cupsThreadCreate((_cups_thread_func_t)monitor_ipp, (void *)device_uri);
    }

    httpAddrFreeList(list);
  }

 /*
  * Do transform...
  */

  status = xform_document(filename, content_type, output_type, resolutions, sheet_back, types, num_options, options, write_cb, write_ptr);

  if (http)
  {
    ippDelete(cupsGetResponse(http, resource));

    if (cupsLastError() > IPP_STATUS_OK_EVENTS_COMPLETE)
    {
      fprintf(stderr, "ERROR: Unable to send print data: %s\n", cupsLastErrorString());
      status = 1;
    }

    httpClose(http);
  }
  else if (fd != 1)
    close(fd);

  if (monitor)
    _cupsThreadCancel(monitor);

  return (status);
}


/*
 * 'load_env_options()' - Load options from the environment.
 */

extern char **environ;

static int				/* O - Number of options */
load_env_options(
    cups_option_t **options)		/* I - Options */
{
  int	i;				/* Looping var */
  char	name[256],			/* Option name */
	*nameptr,			/* Pointer into name */
	*envptr;			/* Pointer into environment variable */
  int	num_options = 0;		/* Number of options */


  *options = NULL;

 /*
  * Load all of the IPP_xxx environment variables as options...
  */

  for (i = 0; environ[i]; i ++)
  {
    envptr = environ[i];

    if (strncmp(envptr, "IPP_", 4))
      continue;

    for (nameptr = name, envptr += 4; *envptr && *envptr != '='; envptr ++)
    {
      if (nameptr > (name + sizeof(name) - 1))
        continue;

      if (*envptr == '_')
        *nameptr++ = '-';
      else
        *nameptr++ = (char)_cups_tolower(*envptr);
    }

    *nameptr = '\0';
    if (*envptr == '=')
      envptr ++;

    num_options = cupsAddOption(name, envptr, num_options, options);
  }

  return (num_options);
}


/*
 * 'monitor_ipp()' - Monitor IPP printer status.
 */

static void *				/* O - Thread exit status */
monitor_ipp(const char *device_uri)	/* I - Device URI */
{
  int		i;			/* Looping var */
  http_t	*http;			/* HTTP connection */
  ipp_t		*request,		/* IPP request */
		*response;		/* IPP response */
  ipp_attribute_t *attr;		/* IPP response attribute */
  char		scheme[32],		/* URI scheme */
		userpass[256],		/* URI user:pass */
		host[256],		/* URI host */
		resource[1024];		/* URI resource */
  int		port;			/* URI port number */
  http_encryption_t encryption;		/* Encryption to use */
  int		delay = 1,		/* Current delay */
		next_delay,		/* Next delay */
		prev_delay = 0;		/* Previous delay */
  char		pvalues[10][1024];	/* Current printer attribute values */
  static const char * const pattrs[10] =/* Printer attributes we need */
  {
    "marker-colors",
    "marker-levels",
    "marker-low-levels",
    "marker-high-levels",
    "marker-names",
    "marker-types",
    "printer-alert",
    "printer-state-reasons",
    "printer-supply",
    "printer-supply-description"
  };


  httpSeparateURI(HTTP_URI_CODING_ALL, device_uri, scheme, sizeof(scheme), userpass, sizeof(userpass), host, sizeof(host), &port, resource, sizeof(resource));

  if (port == 443 || !strcmp(scheme, "ipps"))
    encryption = HTTP_ENCRYPTION_ALWAYS;
  else
    encryption = HTTP_ENCRYPTION_IF_REQUESTED;

  while ((http = httpConnect2(host, port, NULL, AF_UNSPEC, encryption, 1, 30000, NULL)) == NULL)
  {
    fprintf(stderr, "ERROR: Unable to connect to \"%s\" on port %d: %s\n", host, port, cupsLastErrorString());
    sleep(30);
  }

 /*
  * Report printer state changes until we are canceled...
  */

  for (;;)
  {
   /*
    * Poll for the current state...
    */

    request = ippNewRequest(IPP_OP_GET_PRINTER_ATTRIBUTES);
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, device_uri);
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());
    ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "requested-attributes", (int)(sizeof(pattrs) / sizeof(pattrs[0])), NULL, pattrs);

    response = cupsDoRequest(http, request, resource);

   /*
    * Report any differences...
    */

    for (attr = ippFirstAttribute(response); attr; attr = ippNextAttribute(response))
    {
      const char *name = ippGetName(attr);
      char	value[1024];		/* Name and value */


      if (!name)
        continue;

      for (i = 0; i < (int)(sizeof(pattrs) / sizeof(pattrs[0])); i ++)
        if (!strcmp(name, pattrs[i]))
	  break;

      if (i >= (int)(sizeof(pattrs) / sizeof(pattrs[0])))
        continue;

      ippAttributeString(attr, value, sizeof(value));

      if (strcmp(value, pvalues[i]))
      {
        if (!strcmp(name, "printer-state-reasons"))
	  fprintf(stderr, "STATE: %s\n", value);
	else
	  fprintf(stderr, "ATTR: %s='%s'\n", name, value);

        strlcpy(pvalues[i], value, sizeof(pvalues[i]));
      }
    }

    ippDelete(response);

   /*
    * Sleep until the next update...
    */

    sleep((unsigned)delay);

    next_delay = (delay + prev_delay) % 12;
    prev_delay = next_delay < delay ? 0 : delay;
    delay      = next_delay;
  }

  return (NULL);
}


/*
 * 'usage()' - Show program usage.
 */

static void
usage(int status)			/* I - Exit status */
{
  puts("Usage: ipptransform [options] filename\n");
  puts("Options:");
  puts("  --help");
  puts("  -d device-uri");
  puts("  -i input/format");
  puts("  -m output/format");
  puts("  -o \"name=value [... name=value]\"");
  puts("  -r resolution[,...,resolution]");
  puts("  -s {flipped|manual-tumble|normal|rotated}");
  puts("  -t type[,...,type]");
  puts("  -v\n");
  puts("Device URIs: socket://address[:port], ipp://address[:port]/resource, ipps://address[:port]/resource");
  puts("Input Formats: application/pdf, image/jpeg");
  puts("Output Formats: application/vnd.hp-pcl, image/pwg-raster, image/urf");
  puts("Options: copies, media, media-col, page-ranges, print-color-mode, print-quality, print-scaling, printer-resolution, sides");
  puts("Resolutions: NNNdpi or NNNxNNNdpi");
  puts("Types: black_1, sgray_1, sgray_8, srgb_8");

  exit(status);
}


/*
 * 'write_fd()' - Write to a file/socket.
 */

static ssize_t				/* O - Number of bytes written or -1 on error */
write_fd(int                 *fd,	/* I - File descriptor */
         const unsigned char *buffer,	/* I - Buffer */
         size_t              bytes)	/* I - Number of bytes to write */
{
  ssize_t	temp,			/* Temporary byte count */
		total = 0;		/* Total bytes written */


  while (bytes > 0)
  {
    if ((temp = write(*fd, buffer, bytes)) < 0)
    {
      if (errno == EINTR || errno == EAGAIN)
        continue;
      else
        return (-1);
    }

    total  += temp;
    bytes  -= (size_t)temp;
    buffer += temp;
  }

  return (total);
}



#ifdef __APPLE__
/*
 * 'xform_document()' - Transform a file for printing.
 */

static int				/* O - 0 on success, 1 on error */
xform_document(
    const char       *filename,		/* I - File to transform */
    const char       *informat,		/* I - Input document (MIME media type */
    const char       *outformat,	/* I - Output format (MIME media type) */
    const char       *resolutions,	/* I - Supported resolutions */
    const char       *sheet_back,	/* I - Back side transform */
    const char       *types,		/* I - Supported types */
    int              num_options,	/* I - Number of options */
    cups_option_t    *options,		/* I - Options */
    xform_writecb_t cb,		/* I - Write callback */
    void             *ctx)		/* I - Write context */
{
  CFURLRef		url;		/* CFURL object for PDF filename */
  CGPDFDocumentRef	document= NULL;	/* Input document */
  CGPDFPageRef		pdf_page;	/* Page in PDF file */
  CGImageSourceRef	src;		/* Image reader */
  CGImageRef		image = NULL;	/* Image */
  xform_ctx_t	ras;		/* Raster info */
  size_t		max_raster;	/* Maximum raster memory to use */
  const char		*max_raster_env;/* IPPTRANSFORM_MAX_RASTER env var */
  CGColorSpaceRef	cs;		/* Quartz color space */
  CGContextRef		context;	/* Quartz bitmap context */
  CGBitmapInfo		info;		/* Bitmap flags */
  size_t		band_size;	/* Size of band line */
  double		xscale, yscale;	/* Scaling factor */
  CGAffineTransform 	transform,	/* Transform for page */
			back_transform;	/* Transform for back side */
  CGRect		dest;		/* Destination rectangle */
  unsigned		pages = 1;	/* Number of pages */
  int			color = 1;	/* Does the PDF have color? */
  const char		*page_ranges;	/* "page-ranges" option */
  unsigned		first = 1,	/* First page of range */
			last = 1;	/* Last page of range */
  const char		*print_scaling;	/* print-scaling option */
  size_t		image_width,	/* Image width */
			image_height;	/* Image height */
  int			image_rotation;	/* Image rotation */
  double		image_xscale,	/* Image scaling */
			image_yscale;
  unsigned		copy;		/* Current copy */
  unsigned		page;		/* Current page */
  unsigned		media_sheets = 0,
			impressions = 0;/* Page/sheet counters */


 /*
  * Open the file...
  */

  if ((url = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault, (const UInt8 *)filename, (CFIndex)strlen(filename), false)) == NULL)
  {
    fputs("ERROR: Unable to create CFURL for file.\n", stderr);
    return (1);
  }

  if (!strcmp(informat, "application/pdf"))
  {
   /*
    * Open the PDF...
    */

    document = CGPDFDocumentCreateWithURL(url);
    CFRelease(url);

    if (!document)
    {
      fputs("ERROR: Unable to create CFPDFDocument for file.\n", stderr);
      return (1);
    }

    if (CGPDFDocumentIsEncrypted(document))
    {
     /*
      * Only support encrypted PDFs with a blank password...
      */

      if (!CGPDFDocumentUnlockWithPassword(document, ""))
      {
	fputs("ERROR: Document is encrypted and cannot be unlocked.\n", stderr);
	CGPDFDocumentRelease(document);
	return (1);
      }
    }

    if (!CGPDFDocumentAllowsPrinting(document))
    {
      fputs("ERROR: Document does not allow printing.\n", stderr);
      CGPDFDocumentRelease(document);
      return (1);
    }

   /*
    * Check page ranges...
    */

    if ((page_ranges = cupsGetOption("page-ranges", num_options, options)) != NULL)
    {
      if (sscanf(page_ranges, "%u-%u", &first, &last) != 2 || first > last)
      {
	fprintf(stderr, "ERROR: Bad \"page-ranges\" value '%s'.\n", page_ranges);
	CGPDFDocumentRelease(document);
	return (1);
      }

      pages = (unsigned)CGPDFDocumentGetNumberOfPages(document);
      if (first > pages)
      {
	fputs("ERROR: \"page-ranges\" value does not include any pages to print in the document.\n", stderr);
	CGPDFDocumentRelease(document);
	return (1);
      }

      if (last > pages)
	last = pages;
    }
    else
    {
      first = 1;
      last  = (unsigned)CGPDFDocumentGetNumberOfPages(document);
    }

    pages = last - first + 1;
  }
  else
  {
   /*
    * Open the image...
    */

    if ((src = CGImageSourceCreateWithURL(url, NULL)) == NULL)
    {
      CFRelease(url);
      fputs("ERROR: Unable to create CFImageSourceRef for file.\n", stderr);
      return (1);
    }

    if ((image = CGImageSourceCreateImageAtIndex(src, 0, NULL)) == NULL)
    {
      CFRelease(src);
      CFRelease(url);

      fputs("ERROR: Unable to create CFImageRef for file.\n", stderr);
      return (1);
    }

    CFRelease(src);
    CFRelease(url);

    pages = 1;
  }

 /*
  * Setup the raster context...
  */

  if (xform_setup(&ras, outformat, resolutions, sheet_back, types, color, pages, num_options, options))
  {
    CGPDFDocumentRelease(document);
    return (1);
  }

  if (ras.header.cupsBitsPerPixel != 24)
  {
   /*
    * Grayscale output...
    */

    ras.band_bpp = 1;
    info         = kCGImageAlphaNone;
    cs           = CGColorSpaceCreateWithName(kCGColorSpaceGenericGrayGamma2_2);
  }
  else
  {
   /*
    * Color (sRGB) output...
    */

    ras.band_bpp = 4;
    info         = kCGImageAlphaNoneSkipLast;
    cs           = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
  }

  max_raster     = XFORM_MAX_RASTER;
  max_raster_env = getenv("IPPTRANSFORM_MAX_RASTER");
  if (max_raster_env && strtol(max_raster_env, NULL, 10) > 0)
    max_raster = (size_t)strtol(max_raster_env, NULL, 10);

  band_size = ras.header.cupsWidth * ras.band_bpp;
  if ((ras.band_height = (unsigned)(max_raster / band_size)) < 1)
    ras.band_height = 1;
  else if (ras.band_height > ras.header.cupsHeight)
    ras.band_height = ras.header.cupsHeight;

  ras.band_buffer = malloc(ras.band_height * band_size);
  context         = CGBitmapContextCreate(ras.band_buffer, ras.header.cupsWidth, ras.band_height, 8, band_size, cs, info);

  CGColorSpaceRelease(cs);

  /* Don't anti-alias or interpolate when creating raster data */
  CGContextSetAllowsAntialiasing(context, 0);
  CGContextSetInterpolationQuality(context, kCGInterpolationNone);

  xscale = ras.header.HWResolution[0] / 72.0;
  yscale = ras.header.HWResolution[1] / 72.0;

  if (Verbosity > 1)
    fprintf(stderr, "DEBUG: xscale=%g, yscale=%g\n", xscale, yscale);
  CGContextScaleCTM(context, xscale, yscale);

  if (Verbosity > 1)
    fprintf(stderr, "DEBUG: Band height=%u, page height=%u, page translate 0.0,%g\n", ras.band_height, ras.header.cupsHeight, -1.0 * (ras.header.cupsHeight - ras.band_height) / yscale);
  CGContextTranslateCTM(context, 0.0, -1.0 * (ras.header.cupsHeight - ras.band_height) / yscale);

  dest.origin.x    = dest.origin.y = 0.0;
  dest.size.width  = ras.header.cupsWidth * 72.0 / ras.header.HWResolution[0];
  dest.size.height = ras.header.cupsHeight * 72.0 / ras.header.HWResolution[1];

 /*
  * Get print-scaling value...
  */

  if ((print_scaling = cupsGetOption("print-scaling", num_options, options)) == NULL)
    if ((print_scaling = getenv("PRINTER_PRINT_SCALING_DEFAULT")) == NULL)
      print_scaling = "auto";

 /*
  * Start the conversion...
  */

  if (Verbosity > 1)
    fprintf(stderr, "DEBUG: cupsPageSize=[%g %g]\n", ras.header.cupsPageSize[0], ras.header.cupsPageSize[1]);

  (*(ras.start_job))(&ras, cb, ctx);

  if (document)
  {
   /*
    * Render pages in the PDF...
    */

    if (pages > 1 && sheet_back && ras.header.Duplex)
    {
     /*
      * Setup the back page transform...
      */

      if (!strcmp(sheet_back, "flipped"))
      {
	if (ras.header.Tumble)
	  back_transform = CGAffineTransformMake(-1, 0, 0, 1, ras.header.cupsPageSize[0], 0);
	else
	  back_transform = CGAffineTransformMake(1, 0, 0, -1, 0, ras.header.cupsPageSize[1]);
      }
      else if (!strcmp(sheet_back, "manual-tumble") && ras.header.Tumble)
	back_transform = CGAffineTransformMake(-1, 0, 0, -1, ras.header.cupsPageSize[0], ras.header.cupsPageSize[1]);
      else if (!strcmp(sheet_back, "rotated") && !ras.header.Tumble)
	back_transform = CGAffineTransformMake(-1, 0, 0, -1, ras.header.cupsPageSize[0], ras.header.cupsPageSize[1]);
      else
	back_transform = CGAffineTransformMake(1, 0, 0, 1, 0, 0);
    }
    else
      back_transform = CGAffineTransformMake(1, 0, 0, 1, 0, 0);

    if (Verbosity > 1)
      fprintf(stderr, "DEBUG: back_transform=[%g %g %g %g %g %g]\n", back_transform.a, back_transform.b, back_transform.c, back_transform.d, back_transform.tx, back_transform.ty);

   /*
    * Draw all of the pages...
    */

    for (copy = 0; copy < ras.copies; copy ++)
    {
      for (page = 1; page <= pages; page ++)
      {
	unsigned	y,		/* Current line */
			band_starty = 0,/* Start line of band */
			band_endy = 0;	/* End line of band */
	unsigned char	*lineptr;	/* Pointer to line */

	pdf_page  = CGPDFDocumentGetPage(document, page + first - 1);
	transform = CGPDFPageGetDrawingTransform(pdf_page, kCGPDFCropBox,dest, 0, true);

	if (Verbosity > 1)
	  fprintf(stderr, "DEBUG: Printing copy %d/%d, page %d/%d, transform=[%g %g %g %g %g %g]\n", copy + 1, ras.copies, page, pages, transform.a, transform.b, transform.c, transform.d, transform.tx, transform.ty);

	(*(ras.start_page))(&ras, page, cb, ctx);

	for (y = ras.top; y <= ras.bottom; y ++)
	{
	  if (y > band_endy)
	  {
	   /*
	    * Draw the next band of raster data...
	    */

	    band_starty = y;
	    band_endy   = y + ras.band_height - 1;
	    if (band_endy > ras.bottom)
	      band_endy = ras.bottom;

	    if (Verbosity > 1)
	      fprintf(stderr, "DEBUG: Drawing band from %u to %u.\n", band_starty, band_endy);

	    CGContextSaveGState(context);
	      if (ras.header.cupsNumColors == 1)
		CGContextSetGrayFillColor(context, 1., 1.);
	      else
		CGContextSetRGBFillColor(context, 1., 1., 1., 1.);

	      CGContextSetCTM(context, CGAffineTransformIdentity);
	      CGContextFillRect(context, CGRectMake(0., 0., ras.header.cupsWidth, ras.band_height));
	    CGContextRestoreGState(context);

	    CGContextSaveGState(context);
	      if (Verbosity > 1)
		fprintf(stderr, "DEBUG: Band translate 0.0,%g\n", y / yscale);
	      CGContextTranslateCTM(context, 0.0, y / yscale);
	      if (!(page & 1) && ras.header.Duplex)
		CGContextConcatCTM(context, back_transform);
	      CGContextConcatCTM(context, transform);

	      CGContextClipToRect(context, CGPDFPageGetBoxRect(pdf_page, kCGPDFCropBox));
	      CGContextDrawPDFPage(context, pdf_page);
	    CGContextRestoreGState(context);
	  }

	 /*
	  * Prepare and write a line...
	  */

	  lineptr = ras.band_buffer + (y - band_starty) * band_size + ras.left * ras.band_bpp;
	  if (ras.band_bpp == 4)
	    pack_rgba(lineptr, ras.right - ras.left + 1);

	  (*(ras.write_line))(&ras, y, lineptr, cb, ctx);
	}

	(*(ras.end_page))(&ras, page, cb, ctx);

	impressions ++;
	fprintf(stderr, "ATTR: job-impressions-completed=%u\n", impressions);
	if (!ras.header.Duplex || !(page & 1))
	{
	  media_sheets ++;
	  fprintf(stderr, "ATTR: job-media-sheets-completed=%u\n", media_sheets);
	}
      }

      if (ras.copies > 1 && (pages & 1) && ras.header.Duplex)
      {
       /*
	* Duplex printing, add a blank back side image...
	*/

	unsigned	y;		/* Current line */

	if (Verbosity > 1)
	  fprintf(stderr, "DEBUG: Printing blank page %u for duplex.\n", pages + 1);

	memset(ras.band_buffer, 255, ras.header.cupsBytesPerLine);

	(*(ras.start_page))(&ras, page, cb, ctx);

	for (y = ras.top; y < ras.bottom; y ++)
	  (*(ras.write_line))(&ras, y, ras.band_buffer, cb, ctx);

	(*(ras.end_page))(&ras, page, cb, ctx);

	impressions ++;
	fprintf(stderr, "ATTR: job-impressions-completed=%u\n", impressions);
	if (!ras.header.Duplex || !(page & 1))
	{
	  media_sheets ++;
	  fprintf(stderr, "ATTR: job-media-sheets-completed=%u\n", media_sheets);
	}
      }
    }

    CGPDFDocumentRelease(document);
  }
  else
  {
   /*
    * Render copies of the image...
    */

    image_width  = CGImageGetWidth(image);
    image_height = CGImageGetHeight(image);

    if ((image_height < image_width && ras.header.cupsWidth < ras.header.cupsHeight) ||
	 (image_width < image_height && ras.header.cupsHeight < ras.header.cupsWidth))
    {
     /*
      * Rotate image 90 degrees...
      */

      image_rotation = 90;
    }
    else
    {
     /*
      * Leave image as-is...
      */

      image_rotation = 0;
    }

    if (Verbosity > 1)
      fprintf(stderr, "DEBUG: image_width=%u, image_height=%u, image_rotation=%d\n", (unsigned)image_width, (unsigned)image_height, image_rotation);

    if ((!strcmp(print_scaling, "auto") && ras.borderless) || !strcmp(print_scaling, "fill"))
    {
     /*
      * Scale to fill...
      */

      if (image_rotation)
      {
	image_xscale = ras.header.cupsPageSize[0] / (double)image_height;
	image_yscale = ras.header.cupsPageSize[1] / (double)image_width;
      }
      else
      {
	image_xscale = ras.header.cupsPageSize[0] / (double)image_width;
	image_yscale = ras.header.cupsPageSize[1] / (double)image_height;
      }

      if (image_xscale < image_yscale)
	image_xscale = image_yscale;
      else
	image_yscale = image_xscale;

    }
    else
    {
     /*
      * Scale to fit with 1/4" margins...
      */

      if (image_rotation)
      {
	image_xscale = (ras.header.cupsPageSize[0] - 36.0) / (double)image_height;
	image_yscale = (ras.header.cupsPageSize[1] - 36.0) / (double)image_width;
      }
      else
      {
	image_xscale = (ras.header.cupsPageSize[0] - 36.0) / (double)image_width;
	image_yscale = (ras.header.cupsPageSize[1] - 36.0) / (double)image_height;
      }

      if (image_xscale > image_yscale)
	image_xscale = image_yscale;
      else
	image_yscale = image_xscale;
    }

    if (image_rotation)
    {
      transform = CGAffineTransformMake(image_xscale, 0, 0, image_yscale, 0.5 * (ras.header.cupsPageSize[0] - image_xscale * image_height), 0.5 * (ras.header.cupsPageSize[1] - image_yscale * image_width));
    }
    else
    {
      transform = CGAffineTransformMake(image_xscale, 0, 0, image_yscale, 0.5 * (ras.header.cupsPageSize[0] - image_xscale * image_width), 0.5 * (ras.header.cupsPageSize[1] - image_yscale * image_height));
    }

   /*
    * Draw all of the copies...
    */

    for (copy = 0; copy < ras.copies; copy ++)
    {
      unsigned		y,		/* Current line */
			band_starty = 0,/* Start line of band */
			band_endy = 0;	/* End line of band */
      unsigned char	*lineptr;	/* Pointer to line */

      if (Verbosity > 1)
	fprintf(stderr, "DEBUG: Printing copy %d/%d, transform=[%g %g %g %g %g %g]\n", copy + 1, ras.copies, transform.a, transform.b, transform.c, transform.d, transform.tx, transform.ty);

      (*(ras.start_page))(&ras, 1, cb, ctx);

      for (y = ras.top; y <= ras.bottom; y ++)
      {
	if (y > band_endy)
	{
	 /*
	  * Draw the next band of raster data...
	  */

	  band_starty = y;
	  band_endy   = y + ras.band_height - 1;
	  if (band_endy > ras.bottom)
	    band_endy = ras.bottom;

	  if (Verbosity > 1)
	    fprintf(stderr, "DEBUG: Drawing band from %u to %u.\n", band_starty, band_endy);

	  CGContextSaveGState(context);
	    if (ras.header.cupsNumColors == 1)
	      CGContextSetGrayFillColor(context, 1., 1.);
	    else
	      CGContextSetRGBFillColor(context, 1., 1., 1., 1.);

	    CGContextSetCTM(context, CGAffineTransformIdentity);
	    CGContextFillRect(context, CGRectMake(0., 0., ras.header.cupsWidth, ras.band_height));
	  CGContextRestoreGState(context);

	  CGContextSaveGState(context);
	    if (Verbosity > 1)
	      fprintf(stderr, "DEBUG: Band translate 0.0,%g\n", y / yscale);
	    CGContextTranslateCTM(context, 0.0, y / yscale);
	    CGContextConcatCTM(context, transform);

	    if (image_rotation)
	      CGContextConcatCTM(context, CGAffineTransformMake(0, -1, 1, 0, 0, image_width));

	    CGContextDrawImage(context, CGRectMake(0, 0, image_width, image_height), image);
	  CGContextRestoreGState(context);
	}

       /*
	* Prepare and write a line...
	*/

	lineptr = ras.band_buffer + (y - band_starty) * band_size + ras.left * ras.band_bpp;
	if (ras.band_bpp == 4)
	  pack_rgba(lineptr, ras.right - ras.left + 1);

	(*(ras.write_line))(&ras, y, lineptr, cb, ctx);
      }

      (*(ras.end_page))(&ras, 1, cb, ctx);

      impressions ++;
      fprintf(stderr, "ATTR: job-impressions-completed=%u\n", impressions);
      media_sheets ++;
      fprintf(stderr, "ATTR: job-media-sheets-completed=%u\n", media_sheets);
    }

    CFRelease(image);
  }

  (*(ras.end_job))(&ras, cb, ctx);

 /*
  * Clean up...
  */

  CGContextRelease(context);

  return (0);
}


#else
/*
 * 'xform_document()' - Transform a file for printing.
 */

static int				/* O - 0 on success, 1 on error */
xform_document(
    const char       *filename,		/* I - File to transform */
    const char       *informat,		/* I - Input format (MIME media type) */
    const char       *outformat,	/* I - Output format (MIME media type) */
    const char       *resolutions,	/* I - Supported resolutions */
    const char       *sheet_back,	/* I - Back side transform */
    const char       *types,		/* I - Supported types */
    int              num_options,	/* I - Number of options */
    cups_option_t    *options,		/* I - Options */
    xform_writecb_t cb,		/* I - Write callback */
    void             *ctx)		/* I - Write context */
{
  fz_context		*context;	/* MuPDF context */
  fz_document		*document;	/* Document to print */
  fz_page		*pdf_page;	/* Page in PDF file */
  fz_pixmap		*pixmap;	/* Pixmap for band */
  fz_device		*device;	/* Device for rendering */
  fz_colorspace		*cs;		/* Quartz color space */
  xform_ctx_t	ras;		/* Raster info */
  size_t		max_raster;	/* Maximum raster memory to use */
  const char		*max_raster_env;/* IPPTRANSFORM_MAX_RASTER env var */
  unsigned		pages = 1;	/* Number of pages */
  int			color = 1;	/* Color PDF? */
  const char		*page_ranges;	/* "page-ranges" option */
  unsigned		first, last;	/* First and last page of range */
  const char		*print_scaling;	/* print-scaling option */
  unsigned		copy;		/* Current copy */
  unsigned		page;		/* Current page */
  unsigned		media_sheets = 0,
			impressions = 0;/* Page/sheet counters */
  size_t		band_size;	/* Size of band line */
  double		xscale, yscale;	/* Scaling factor */
  fz_rect		image_box;	/* Bounding box of content */
  fz_matrix	 	base_transform,	/* Base transform */
			image_transform,/* Transform for content ("page image") */
			transform,	/* Transform for page */
			back_transform;	/* Transform for back side */


 /*
  * Open the PDF file...
  */

  if ((context = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED)) == NULL)
  {
    fputs("ERROR: Unable to create context.\n", stderr);
    return (1);
  }

  fz_register_document_handlers(context);

  fz_try(context) document = fz_open_document(context, filename);
  fz_catch(context)
  {
    fprintf(stderr, "ERROR: Unable to open '%s': %s\n", filename, fz_caught_message(context));
    fz_drop_context(context);
    return (1);
  }

  if (fz_needs_password(context, document))
  {
    fputs("ERROR: Document is encrypted and cannot be unlocked.\n", stderr);
    fz_drop_document(context, document);
    fz_drop_context(context);
    return (1);
  }

 /*
  * Check page ranges...
  */

  if ((page_ranges = cupsGetOption("page-ranges", num_options, options)) != NULL)
  {
    if (sscanf(page_ranges, "%u-%u", &first, &last) != 2 || first > last)
    {
      fprintf(stderr, "ERROR: Bad \"page-ranges\" value '%s'.\n", page_ranges);

      fz_drop_document(context, document);
      fz_drop_context(context);

      return (1);
    }

    pages = (unsigned)fz_count_pages(context, document);
    if (first > pages)
    {
      fputs("ERROR: \"page-ranges\" value does not include any pages to print in the document.\n", stderr);

      fz_drop_document(context, document);
      fz_drop_context(context);

      return (1);
    }

    if (last > pages)
      last = pages;
  }
  else
  {
    first = 1;
    last  = (unsigned)fz_count_pages(context, document);
  }

  pages = last - first + 1;

 /*
  * Setup the raster context...
  */

  if (xform_setup(&ras, outformat, resolutions, sheet_back, types, color, 1, num_options, options))
  {
    fz_drop_document(context, document);
    fz_drop_context(context);

    return (1);
  }

  if (ras.header.cupsBitsPerPixel != 24)
  {
   /*
    * Grayscale output...
    */

    ras.band_bpp = 2; /* TODO: Update to not use alpha (Issue #93) */
    cs           = fz_device_gray(context);
  }
  else
  {
   /*
    * Color (sRGB) output...
    */

    ras.band_bpp = 4; /* TODO: Update to not use alpha (Issue #93) */
    cs           = fz_device_rgb(context);
  }

  max_raster     = XFORM_MAX_RASTER;
  max_raster_env = getenv("IPPTRANSFORM_MAX_RASTER");
  if (max_raster_env && strtol(max_raster_env, NULL, 10) > 0)
    max_raster = (size_t)strtol(max_raster_env, NULL, 10);

  band_size = ras.header.cupsWidth * ras.band_bpp;
  if ((ras.band_height = (unsigned)(max_raster / band_size)) < 1)
    ras.band_height = 1;
  else if (ras.band_height > ras.header.cupsHeight)
    ras.band_height = ras.header.cupsHeight;

  /* TODO: Update code to not use RGBA/GrayA pixmap now that MuPDF supports it (Issue #93) */
#  if HAVE_FZ_NEW_PIXMAP_5_ARG
  pixmap = fz_new_pixmap(context, cs, (int)ras.header.cupsWidth,  (int)ras.band_height, 1);
#  else
  pixmap = fz_new_pixmap(context, cs, (int)ras.header.cupsWidth,  (int)ras.band_height, NULL, 1);
  pixmap->flags = 0;
#  endif /* HAVE_FZ_NEW_PIXMAP_5_ARG */

  pixmap->xres = (int)ras.header.HWResolution[0];
  pixmap->yres = (int)ras.header.HWResolution[1];

  xscale = ras.header.HWResolution[0] / 72.0;
  yscale = ras.header.HWResolution[1] / 72.0;

  if (Verbosity > 1)
    fprintf(stderr, "DEBUG: xscale=%g, yscale=%g\n", xscale, yscale);
  fz_scale(&base_transform, xscale, yscale);

  if (Verbosity > 1)
    fprintf(stderr, "DEBUG: Band height=%u, page height=%u\n", ras.band_height, ras.header.cupsHeight);

  device = fz_new_draw_device(context, &base_transform, pixmap);

#  ifndef HAVE_FZ_NEW_PIXMAP_5_ARG /* Bug in MuPDF 1.11 */
  /* Don't anti-alias or interpolate when creating raster data */
  fz_set_aa_level(context, 0);
#  endif /* !HAVE_FZ_NEW_PIXMAP_5_ARG */

  fz_enable_device_hints(context, device, FZ_DONT_INTERPOLATE_IMAGES);

 /*
  * Setup the back page transform, if any...
  */

  if (sheet_back && ras.header.Duplex)
  {
    if (!strcmp(sheet_back, "flipped"))
    {
      if (ras.header.Tumble)
        back_transform = fz_make_matrix(-1, 0, 0, 1, ras.header.cupsPageSize[0], 0);
      else
        back_transform = fz_make_matrix(1, 0, 0, -1, 0, ras.header.cupsPageSize[1]);
    }
    else if (!strcmp(sheet_back, "manual-tumble") && ras.header.Tumble)
      back_transform = fz_make_matrix(-1, 0, 0, -1, ras.header.cupsPageSize[0], ras.header.cupsPageSize[1]);
    else if (!strcmp(sheet_back, "rotated") && !ras.header.Tumble)
      back_transform = fz_make_matrix(-1, 0, 0, -1, ras.header.cupsPageSize[0], ras.header.cupsPageSize[1]);
    else
      back_transform = fz_make_matrix(1, 0, 0, 1, 0, 0);
  }
  else
    back_transform = fz_make_matrix(1, 0, 0, 1, 0, 0);

  if (Verbosity > 1)
    fprintf(stderr, "DEBUG: cupsPageSize=[%g %g]\n", ras.header.cupsPageSize[0], ras.header.cupsPageSize[1]);
  if (Verbosity > 1)
    fprintf(stderr, "DEBUG: back_transform=[%g %g %g %g %g %g]\n", back_transform.a, back_transform.b, back_transform.c, back_transform.d, back_transform.e, back_transform.f);

 /*
  * Get print-scaling value...
  */

  if ((print_scaling = cupsGetOption("print-scaling", num_options, options)) == NULL)
    if ((print_scaling = getenv("PRINTER_PRINT_SCALING_DEFAULT")) == NULL)
      print_scaling = "auto";

 /*
  * Draw all of the pages...
  */

  (*(ras.start_job))(&ras, cb, ctx);

  for (copy = 0; copy < ras.copies; copy ++)
  {
    for (page = 1; page <= pages; page ++)
    {
      unsigned		y,		/* Current line */
			band_starty = 0,/* Start line of band */
			band_endy = 0;	/* End line of band */
      unsigned char	*lineptr;	/* Pointer to line */

      pdf_page = fz_load_page(context, document, (int)(page + first - 2));

      fz_bound_page(context, pdf_page, &image_box);

      fprintf(stderr, "DEBUG: image_box=[%g %g %g %g]\n", image_box.x0, image_box.y0, image_box.x1, image_box.y1);

      float image_width = image_box.x1 - image_box.x0;
      float image_height = image_box.y1 - image_box.y0;
      int image_rotation = 0;
      int is_image = strcmp(informat, "application/pdf") != 0;
      float image_xscale, image_yscale;

      if ((image_height < image_width && ras.header.cupsWidth < ras.header.cupsHeight) ||
	   (image_width < image_height && ras.header.cupsHeight < ras.header.cupsWidth))
      {
       /*
	* Rotate image/page 90 degrees...
	*/

	image_rotation = 90;
      }

      if ((!strcmp(print_scaling, "auto") && ras.borderless && is_image) || !strcmp(print_scaling, "fill"))
      {
       /*
	* Scale to fill...
	*/

	if (image_rotation)
	{
	  image_xscale = ras.header.cupsPageSize[0] / (double)image_height;
	  image_yscale = ras.header.cupsPageSize[1] / (double)image_width;
	}
	else
	{
	  image_xscale = ras.header.cupsPageSize[0] / (double)image_width;
	  image_yscale = ras.header.cupsPageSize[1] / (double)image_height;
	}

	if (image_xscale < image_yscale)
	  image_xscale = image_yscale;
	else
	  image_yscale = image_xscale;

      }
      else if ((!strcmp(print_scaling, "auto") && (is_image || (image_rotation == 0 && (image_width > ras.header.cupsPageSize[0] || image_height > ras.header.cupsPageSize[1])) || (image_rotation == 90 && (image_height > ras.header.cupsPageSize[1] || image_width > ras.header.cupsPageSize[1])))) || !strcmp(print_scaling, "fit"))
      {
       /*
	* Scale to fit...
	*/

	if (image_rotation)
	{
	  image_xscale = ras.header.cupsPageSize[0] / (double)image_height;
	  image_yscale = ras.header.cupsPageSize[1] / (double)image_width;
	}
	else
	{
	  image_xscale = ras.header.cupsPageSize[0] / (double)image_width;
	  image_yscale = ras.header.cupsPageSize[1] / (double)image_height;
	}

	if (image_xscale > image_yscale)
	  image_xscale = image_yscale;
	else
	  image_yscale = image_xscale;
      }
      else
      {
       /*
        * Do not scale...
	*/

        image_xscale = image_yscale = 1.0;
      }

      if (image_rotation)
      {
	image_transform = fz_make_matrix(image_xscale, 0, 0, image_yscale, 0.5 * (ras.header.cupsPageSize[0] - image_xscale * image_height), 0.5 * (ras.header.cupsPageSize[1] - image_yscale * image_width));
      }
      else
      {
	image_transform = fz_make_matrix(image_xscale, 0, 0, image_yscale, 0.5 * (ras.header.cupsPageSize[0] - image_xscale * image_width), 0.5 * (ras.header.cupsPageSize[1] - image_yscale * image_height));
      }

      if (Verbosity > 1)
        fprintf(stderr, "DEBUG: Printing copy %d/%d, page %d/%d, image_transform=[%g %g %g %g %g %g]\n", copy + 1, ras.copies, page, pages, image_transform.a, image_transform.b, image_transform.c, image_transform.d, image_transform.e, image_transform.f);

      (*(ras.start_page))(&ras, page, cb, ctx);

      for (y = ras.top; y <= ras.bottom; y ++)
      {
	if (y > band_endy)
	{
	 /*
	  * Draw the next band of raster data...
	  */

	  band_starty = y;
	  band_endy   = y + ras.band_height - 1;
	  if (band_endy > ras.bottom)
	    band_endy = ras.bottom;

	  if (Verbosity > 1)
	    fprintf(stderr, "DEBUG: Drawing band from %u to %u.\n", band_starty, band_endy);

          fz_clear_pixmap_with_value(context, pixmap, 0xff);

          transform = fz_identity;
	  fz_pre_translate(&transform, 0.0, -1.0 * y / yscale);
	  if (!(page & 1) && ras.header.Duplex)
	    fz_concat(&transform, &transform, &back_transform);

	  fz_concat(&transform, &transform, &image_transform);

          fprintf(stderr, "DEBUG: page transform=[%g %g %g %g %g %g]\n", transform.a, transform.b, transform.c, transform.d, transform.e, transform.f);

          fz_run_page(context, pdf_page, device, &transform, NULL);
	}

       /*
	* Prepare and write a line...
	*/

	lineptr = pixmap->samples + (y - band_starty) * band_size + ras.left * ras.band_bpp;
        if (ras.band_bpp == 4)
          pack_rgba(lineptr, ras.right - ras.left + 1);
        else
          pack_graya(lineptr, ras.right - ras.left + 1);

	(*(ras.write_line))(&ras, y, lineptr, cb, ctx);
      }

      (*(ras.end_page))(&ras, page, cb, ctx);

      impressions ++;
      fprintf(stderr, "ATTR: job-impressions-completed=%u\n", impressions);
      if (!ras.header.Duplex || !(page & 1))
      {
        media_sheets ++;
	fprintf(stderr, "ATTR: job-media-sheets-completed=%u\n", media_sheets);
      }
    }

    if (ras.copies > 1 && (pages & 1) && ras.header.Duplex)
    {
     /*
      * Duplex printing, add a blank back side image...
      */

      unsigned		y;		/* Current line */

      if (Verbosity > 1)
        fprintf(stderr, "DEBUG: Printing blank page %u for duplex.\n", pages + 1);

      memset(pixmap->samples, 255, ras.header.cupsBytesPerLine);

      (*(ras.start_page))(&ras, page, cb, ctx);

      for (y = ras.top; y < ras.bottom; y ++)
	(*(ras.write_line))(&ras, y, pixmap->samples, cb, ctx);

      (*(ras.end_page))(&ras, page, cb, ctx);

      impressions ++;
      fprintf(stderr, "ATTR: job-impressions-completed=%u\n", impressions);
      if (!ras.header.Duplex || !(page & 1))
      {
        media_sheets ++;
	fprintf(stderr, "ATTR: job-media-sheets-completed=%u\n", media_sheets);
      }
    }
  }

  (*(ras.end_job))(&ras, cb, ctx);

 /*
  * Clean up...
  */

  fz_drop_device(context, device);
  fz_drop_pixmap(context, pixmap);
  fz_drop_document(context, document);
  fz_drop_context(context);

  return (1);
}
#endif /* __APPLE__ */


/*
 * 'xform_setup()' - Setup a raster context for printing.
 */

static int				/* O - 0 on success, -1 on failure */
xform_setup(xform_ctx_t *ras,	/* I - Raster information */
            const char     *format,	/* I - Output format (MIME media type) */
	    const char     *resolutions,/* I - Supported resolutions */
	    const char     *sheet_back,	/* I - Back side transform */
	    const char     *types,	/* I - Supported types */
	    int            color,	/* I - Document contains color? */
            unsigned       pages,	/* I - Number of pages */
            int            num_options,	/* I - Number of options */
            cups_option_t  *options)	/* I - Options */
{
  const char	*copies,		/* "copies" option */
		*media,			/* "media" option */
		*media_col;		/* "media-col" option */
  pwg_media_t	*pwg_media = NULL;	/* PWG media value */
  const char	*print_color_mode,	/* "print-color-mode" option */
		*print_quality,		/* "print-quality" option */
		*printer_resolution,	/* "printer-resolution" option */
		*sides,			/* "sides" option */
		*type;			/* Raster type to use */
  int		draft = 0,		/* Draft quality? */
		xdpi, ydpi;		/* Resolution to use */
  cups_array_t	*res_array,		/* Resolutions in array */
		*type_array;		/* Types in array */


 /*
  * Initialize raster information...
  */

  memset(ras, 0, sizeof(xform_ctx_t));

  ras->format      = format;
  ras->num_options = num_options;
  ras->options     = options;

  if (!strcmp(format, "application/vnd.hp-pcl"))
    pcl_init(ras);
  else
    raster_init(ras);

 /*
  * Get the number of copies...
  */

  if ((copies = cupsGetOption("copies", num_options, options)) != NULL)
  {
    int temp = atoi(copies);		/* Copies value */

    if (temp < 1 || temp > 9999)
    {
      fprintf(stderr, "ERROR: Invalid \"copies\" value '%s'.\n", copies);
      return (-1);
    }

    ras->copies = (unsigned)temp;
  }
  else
    ras->copies = 1;

 /*
  * Figure out the media size...
  */

  if ((media = cupsGetOption("media", num_options, options)) != NULL)
  {
    if ((pwg_media = pwgMediaForPWG(media)) == NULL)
      pwg_media = pwgMediaForLegacy(media);

    if (!pwg_media)
    {
      fprintf(stderr, "ERROR: Unknown \"media\" value '%s'.\n", media);
      return (-1);
    }
  }
  else if ((media_col = cupsGetOption("media-col", num_options, options)) != NULL)
  {
    int			num_cols;	/* Number of collection values */
    cups_option_t	*cols;		/* Collection values */
    const char		*media_size_name,
			*media_size,	/* Collection attributes */
			*media_bottom_margin,
			*media_left_margin,
			*media_right_margin,
			*media_top_margin;

    num_cols = cupsParseOptions(media_col, 0, &cols);
    if ((media_size_name = cupsGetOption("media-size-name", num_cols, cols)) != NULL)
    {
      if ((pwg_media = pwgMediaForPWG(media_size_name)) == NULL)
      {
	fprintf(stderr, "ERROR: Unknown \"media-size-name\" value '%s'.\n", media_size_name);
	cupsFreeOptions(num_cols, cols);
	return (-1);
      }
    }
    else if ((media_size = cupsGetOption("media-size", num_cols, cols)) != NULL)
    {
      int		num_sizes;	/* Number of collection values */
      cups_option_t	*sizes;		/* Collection values */
      const char	*x_dim,		/* Collection attributes */
			*y_dim;

      num_sizes = cupsParseOptions(media_size, 0, &sizes);
      if ((x_dim = cupsGetOption("x-dimension", num_sizes, sizes)) != NULL && (y_dim = cupsGetOption("y-dimension", num_sizes, sizes)) != NULL)
      {
        pwg_media = pwgMediaForSize(atoi(x_dim), atoi(y_dim));
      }
      else
      {
        fprintf(stderr, "ERROR: Bad \"media-size\" value '%s'.\n", media_size);
	cupsFreeOptions(num_sizes, sizes);
	cupsFreeOptions(num_cols, cols);
	return (-1);
      }

      cupsFreeOptions(num_sizes, sizes);
    }

   /*
    * Check whether the media-col is for a borderless size...
    */

    if ((media_bottom_margin = cupsGetOption("media-bottom-margin", num_cols, cols)) != NULL && !strcmp(media_bottom_margin, "0") &&
        (media_left_margin = cupsGetOption("media-left-margin", num_cols, cols)) != NULL && !strcmp(media_left_margin, "0") &&
        (media_right_margin = cupsGetOption("media-right-margin", num_cols, cols)) != NULL && !strcmp(media_right_margin, "0") &&
        (media_top_margin = cupsGetOption("media-top-margin", num_cols, cols)) != NULL && !strcmp(media_top_margin, "0"))
      ras->borderless = 1;

    cupsFreeOptions(num_cols, cols);
  }

  if (!pwg_media)
  {
   /*
    * Use default size...
    */

    const char	*media_default = getenv("PRINTER_MEDIA_DEFAULT");
				/* "media-default" value */

    if (!media_default)
      media_default = "na_letter_8.5x11in";

    if ((pwg_media = pwgMediaForPWG(media_default)) == NULL)
    {
      fprintf(stderr, "ERROR: Unknown \"media-default\" value '%s'.\n", media_default);
      return (-1);
    }
  }

 /*
  * Map certain photo sizes (4x6, 5x7, 8x10) to borderless...
  */

  if ((pwg_media->width == 10160 && pwg_media->length == 15240) ||(pwg_media->width == 12700 && pwg_media->length == 17780) ||(pwg_media->width == 20320 && pwg_media->length == 25400))
    ras->borderless = 1;

 /*
  * Figure out the proper resolution, etc.
  */

  res_array = _cupsArrayNewStrings(resolutions, ',');

  if ((printer_resolution = cupsGetOption("printer-resolution", num_options, options)) != NULL && !cupsArrayFind(res_array, (void *)printer_resolution))
  {
    if (Verbosity)
      fprintf(stderr, "INFO: Unsupported \"printer-resolution\" value '%s'.\n", printer_resolution);
    printer_resolution = NULL;
  }

  if (!printer_resolution)
  {
    if ((print_quality = cupsGetOption("print-quality", num_options, options)) != NULL)
    {
      switch (atoi(print_quality))
      {
        case IPP_QUALITY_DRAFT :
	    draft              = 1;
	    printer_resolution = cupsArrayIndex(res_array, 0);
	    break;

        case IPP_QUALITY_NORMAL :
	    printer_resolution = cupsArrayIndex(res_array, cupsArrayCount(res_array) / 2);
	    break;

        case IPP_QUALITY_HIGH :
	    printer_resolution = cupsArrayIndex(res_array, cupsArrayCount(res_array) - 1);
	    break;

	default :
	    if (Verbosity)
	      fprintf(stderr, "INFO: Unsupported \"print-quality\" value '%s'.\n", print_quality);
	    break;
      }
    }
  }

  if (!printer_resolution)
    printer_resolution = cupsArrayIndex(res_array, cupsArrayCount(res_array) / 2);

  if (!printer_resolution)
  {
    fputs("ERROR: No \"printer-resolution\" or \"pwg-raster-document-resolution-supported\" value.\n", stderr);
    return (-1);
  }

 /*
  * Parse the "printer-resolution" value...
  */

  if (sscanf(printer_resolution, "%ux%udpi", &xdpi, &ydpi) != 2)
  {
    if (sscanf(printer_resolution, "%udpi", &xdpi) == 1)
    {
      ydpi = xdpi;
    }
    else
    {
      fprintf(stderr, "ERROR: Bad resolution value '%s'.\n", printer_resolution);
      return (-1);
    }
  }

  cupsArrayDelete(res_array);

 /*
  * Now figure out the color space to use...
  */

  if ((print_color_mode = cupsGetOption("print-color-mode", num_options, options)) == NULL)
    print_color_mode = getenv("PRINTER_PRINT_COLOR_MODE_DEFAULT");

  if (print_color_mode)
  {
    if (!strcmp(print_color_mode, "monochrome") || !strcmp(print_color_mode, "process-monochrome") || !strcmp(print_color_mode, "auto-monochrome"))
    {
      color = 0;
    }
    else if (!strcmp(print_color_mode, "bi-level") || !strcmp(print_color_mode, "process-bi-level"))
    {
      color = 0;
      draft = 1;
    }
  }

  type_array = _cupsArrayNewStrings(types, ',');

  if (color && cupsArrayFind(type_array, "srgb_8"))
    type = "srgb_8";
  else if (draft && cupsArrayFind(type_array, "black_1"))
    type = "black_1";
  else if (draft && cupsArrayFind(type_array, "sgray_1"))
    type = "sgray_1";
  else
    type = "sgray_8";

 /*
  * Initialize the raster header...
  */

  if (pages == 1)
    sides = "one-sided";
  else if ((sides = cupsGetOption("sides", num_options, options)) == NULL)
  {
    if ((sides = getenv("PRINTER_SIDES_DEFAULT")) == NULL)
      sides = "one-sided";
  }

  if (ras->copies > 1 && (pages & 1) && strcmp(sides, "one-sided"))
    pages ++;

  if (!cupsRasterInitPWGHeader(&(ras->header), pwg_media, type, xdpi, ydpi, sides, NULL))
  {
    fprintf(stderr, "ERROR: Unable to initialize raster context: %s\n", cupsRasterErrorString());
    return (-1);
  }

  if (pages > 1)
  {
    if (!cupsRasterInitPWGHeader(&(ras->back_header), pwg_media, type, xdpi, ydpi, sides, sheet_back))
    {
      fprintf(stderr, "ERROR: Unable to initialize back side raster context: %s\n", cupsRasterErrorString());
      return (-1);
    }
  }

  ras->header.cupsInteger[CUPS_RASTER_PWG_TotalPageCount]      = ras->copies * pages;
  ras->back_header.cupsInteger[CUPS_RASTER_PWG_TotalPageCount] = ras->copies * pages;

  return (0);
}
#endif /* 0 */
