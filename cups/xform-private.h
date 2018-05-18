/*
 * Private transform API definitions for CUPS.
 *
 * Copyright © 2016-2018 by Apple Inc.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more
 * information.
 */

#ifndef _CUPS_XFORM_PRIVATE_H_
#  define _CUPS_XFORM_PRIVATE_H_


/*
 * Include necessary headers...
 */

#  include <cups/cups.h>
#  include <cups/raster.h>


/*
 * C++ magic...
 */

#  ifdef __cplusplus
extern "C" {
#  endif /* __cplusplus */


/*
 * Constants...
 */

/**** Input/Output MIME media types ****/
#  define XFORM_FORMAT_APPLE_RASTER	"image/urf"
#  define XFORM_FORMAT_JPEG		"image/jpeg"
#  define XFORM_FORMAT_PCL		"application/vnd.hp-pcl"
#  define XFORM_FORMAT_PDF		"application/pdf"
#  define XFORM_FORMAT_PNG		"image/png"
#  define XFORM_FORMAT_POSTSCRIPT	"application/postscript"
#  define XFORM_FORMAT_PWG_RASTER	"image/pwg-raster"
#  define XFORM_FORMAT_TEXT		"text/plain"

typedef enum xform_duplex_e		/**** 2-Sided Capabilities ****/
{
  XFORM_DUPLEX_NONE,			/* No 2-sided support */
  XFORM_DUPLEX_NORMAL,			/* 2-sided support, normal back side orientation ('normal') */
  XFORM_DUPLEX_LONG_TUMBLE,		/* 2-sided support, rotate back side 180 degrees for long edge ('manual-tumble') */
  XFORM_DUPLEX_SHORT_TUMBLE,		/* 2-sided support, rotate back side 180 degrees for short edge ('rotated') */
  XFORM_DUPLEX_MIRRORED			/* 2-sided support, mirror back side ('flippped') */
} xform_duplex_t;

typedef enum xform_loglevel_e		/**** Logging Levels ****/
{
  XFORM_LOGLEVEL_DEBUG,			/* Debugging message */
  XFORM_LOGLEVEL_INFO,			/* Informational message */
  XFORM_LOGLEVEL_ERROR,			/* Error message */
  XFORM_LOGLEVEL_ATTR			/* Attribute message */
} xform_loglevel_t;

/*
 * Local types...
 */

typedef struct xform_margins_s		/**** Output margins ****/
{
  int		bottom,			/* Bottom margin in hundredths of millimeters */
		left,			/* Left margin in hundredths of millimeters */
		right,			/* Right margin in hundredths of millimeters */
		top;			/* Top margin in hundredths of millimeters */
} xform_margins_t;

typedef struct xform_size_s		/**** Output size ****/
{
  int		width,			/* Width in hundredths of millimeters */
		length;			/* Length in hundredths of millimeters */
} xform_size_t;

typedef struct xform_capabilities_s	/**** Output Capabilities ****/
{
  int		mixed;			/* Supports pages with different colorspaces and sizes? */
  cups_cspace_t	color,			/* Colorspace for printing color documents */
		monochrome,		/* Colorspace for printing B&W documents */
		photo;			/* Colorspace for printing photos */
  unsigned	draft_bits,		/* Bits per color for printing draft quality */
		normal_bits,		/* Bits per color for printing normal quality */
		high_bits;		/* Bits per color for printing high/best/photo quality */
  unsigned	draft_resolution[2],	/* Draft resolution */
		normal_resolution[2],	/* Normal resolution */
		high_resolution[2];	/* High/best/photo resolution */
  xform_duplex_t duplex;		/* 2-sided capabilities */
  xform_margins_t margins,		/* Default margins */
  xform_size_t	size;			/* Default size */
  xform_margins_t max_margins;		/* Maximum margins */
  xform_size_t	max_size;		/* Maximum size */
  xform_margins_t min_margins;		/* Minimum margins */
  xform_size_t	min_size;		/* Minimum size */
} xform_capabilities_t;

typedef struct _xform_ctx_s xform_ctx_t;/**** Transform context ****/

typedef void (*xform_logcb_t)(void *user_data, xform_loglevel_t level, const char *message);
					/**** Logging callback ****/

typedef ssize_t (*xform_writecb_t)(void *user_data, const unsigned char *buffer, size_t length);
					/**** Output callback ****/


/*
 * Functions...
 */

extern void		xformDelete(xform_ctx_t *ctx);
extern xform_ctx_t	*xformNew(const char *outformat, xform_capabilities_t *outcaps);
extern int		xformRun(xform_ctx_t *ctx, const char *infile, const char *informat);
extern void		xformSetLogCallback(xform_ctx_t *ctx, xform_logcb_t logcb, void *logdata);
extern void		xformSetOptions(xform_ctx_t *ctx, int num_options, cups_option_t *options);
extern void		xformSetWriteCallback(xform_ctx_t *ctx, xform_writecb_t writecb, void *writedata);


#  ifdef __cplusplus
}
#  endif /* __cplusplus */

#endif /* !_CUPS_XFORM_PRIVATE_H_ */
