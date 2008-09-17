/*
 * "$Id: image-colorspace.c 7720 2008-07-11 22:46:21Z mike $"
 *
 *   Colorspace conversions for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007-2008 by Apple Inc.
 *   Copyright 1993-2006 by Easy Software Products.
 *
 *   The color saturation/hue matrix stuff is provided thanks to Mr. Paul
 *   Haeberli at "http://www.sgi.com/grafica/matrix/index.html".
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
 *   cupsImageCMYKToBlack()         - Convert CMYK data to black.
 *   cupsImageCMYKToCMY()           - Convert CMYK colors to CMY.
 *   cupsImageCMYKToCMYK()          - Convert CMYK colors to CMYK.
 *   cupsImageCMYKToRGB()           - Convert CMYK colors to device-dependent
 *                                    RGB.
 *   cupsImageCMYKToWhite()         - Convert CMYK colors to luminance.
 *   cupsImageLut()                 - Adjust all pixel values with the given
 *                                    LUT.
 *   cupsImageRGBAdjust()           - Adjust the hue and saturation of the
 *                                    given RGB colors.
 *   cupsImageRGBToBlack()          - Convert RGB data to black.
 *   cupsImageRGBToCMY()            - Convert RGB colors to CMY.
 *   cupsImageRGBToCMYK()           - Convert RGB colors to CMYK.
 *   cupsImageRGBToRGB()            - Convert RGB colors to device-dependent
 *                                    RGB.
 *   cupsImageRGBToWhite()          - Convert RGB colors to luminance.
 *   cupsImageSetProfile()          - Set the device color profile.
 *   cupsImageSetRasterColorSpace() - Set the destination colorspace.
 *   cupsImageWhiteToBlack()        - Convert luminance colors to black.
 *   cupsImageWhiteToCMY()          - Convert luminance colors to CMY.
 *   cupsImageWhiteToCMYK()         - Convert luminance colors to CMYK.
 *   cupsImageWhiteToRGB()          - Convert luminance data to RGB.
 *   cupsImageWhiteToWhite()        - Convert luminance colors to device-
 *                                    dependent luminance.
 *   cielab()                       - Map CIE Lab transformation...
 *   huerotate()                    - Rotate the hue, maintaining luminance.
 *   ident()                        - Make an identity matrix.
 *   mult()                         - Multiply two matrices.
 *   rgb_to_lab()                   - Convert an RGB color to CIE Lab.
 *   rgb_to_xyz()                   - Convert an RGB color to CIE XYZ.
 *   saturate()                     - Make a saturation matrix.
 *   xform()                        - Transform a 3D point using a matrix...
 *   xrotate()                      - Rotate about the x (red) axis...
 *   yrotate()                      - Rotate about the y (green) axis...
 *   zrotate()                      - Rotate about the z (blue) axis...
 *   zshear()                       - Shear z using x and y...
 */

/*
 * Include necessary headers...
 */

#include "image-private.h"


/*
 * Define some math constants that are required...
 */

#ifndef M_PI
#  define M_PI		3.14159265358979323846
#endif /* !M_PI */

#ifndef M_SQRT2
#  define M_SQRT2	1.41421356237309504880
#endif /* !M_SQRT2 */

#ifndef M_SQRT1_2
#  define M_SQRT1_2	0.70710678118654752440
#endif /* !M_SQRT1_2 */

/*
 * CIE XYZ whitepoint...
 */

#define D65_X	(0.412453 + 0.357580 + 0.180423)
#define D65_Y	(0.212671 + 0.715160 + 0.072169)
#define D65_Z	(0.019334 + 0.119193 + 0.950227)


/*
 * Lookup table structure...
 */

typedef int cups_clut_t[3][256];


/*
 * Local globals...
 */

static int		cupsImageHaveProfile = 0;
					/* Do we have a color profile? */
static int		*cupsImageDensity;
					/* Ink/marker density LUT */
static cups_clut_t	*cupsImageMatrix;
					/* Color transform matrix LUT */
static cups_cspace_t	cupsImageColorSpace = CUPS_CSPACE_RGB;
					/* Destination colorspace */


/*
 * Local functions...
 */

static float	cielab(float x, float xn);
static void	huerotate(float [3][3], float);
static void	ident(float [3][3]);
static void	mult(float [3][3], float [3][3], float [3][3]);
static void	rgb_to_lab(cups_ib_t *val);
static void	rgb_to_xyz(cups_ib_t *val);
static void	saturate(float [3][3], float);
static void	xform(float [3][3], float, float, float, float *, float *, float *);
static void	xrotate(float [3][3], float, float);
static void	yrotate(float [3][3], float, float);
static void	zrotate(float [3][3], float, float);
static void	zshear(float [3][3], float, float);


/*
 * 'cupsImageCMYKToBlack()' - Convert CMYK data to black.
 */

void
cupsImageCMYKToBlack(
    const cups_ib_t *in,		/* I - Input pixels */
    cups_ib_t       *out,		/* I - Output pixels */
    int             count)		/* I - Number of pixels */
{
  int	k;				/* Black value */


  if (cupsImageHaveProfile)
    while (count > 0)
    {
      k = (31 * in[0] + 61 * in[1] + 8 * in[2]) / 100 + in[3];

      if (k < 255)
        *out++ = cupsImageDensity[k];
      else
        *out++ = cupsImageDensity[255];

      in += 4;
      count --;
    }
  else
    while (count > 0)
    {
      k = (31 * in[0] + 61 * in[1] + 8 * in[2]) / 100 + in[3];

      if (k < 255)
        *out++ = k;
      else
        *out++ = 255;

      in += 4;
      count --;
    }
}


/*
 * 'cupsImageCMYKToCMY()' - Convert CMYK colors to CMY.
 */

void
cupsImageCMYKToCMY(
    const cups_ib_t *in,		/* I - Input pixels */
    cups_ib_t       *out,		/* I - Output pixels */
    int             count)		/* I - Number of pixels */
{
  int	c, m, y, k;			/* CMYK values */
  int	cc, cm, cy;			/* Calibrated CMY values */


  if (cupsImageHaveProfile)
    while (count > 0)
    {
      c = *in++;
      m = *in++;
      y = *in++;
      k = *in++;

      cc = cupsImageMatrix[0][0][c] +
           cupsImageMatrix[0][1][m] +
	   cupsImageMatrix[0][2][y] + k;
      cm = cupsImageMatrix[1][0][c] +
           cupsImageMatrix[1][1][m] +
	   cupsImageMatrix[1][2][y] + k;
      cy = cupsImageMatrix[2][0][c] +
           cupsImageMatrix[2][1][m] +
	   cupsImageMatrix[2][2][y] + k;

      if (cc < 0)
        *out++ = 0;
      else if (cc > 255)
        *out++ = cupsImageDensity[255];
      else
        *out++ = cupsImageDensity[cc];

      if (cm < 0)
        *out++ = 0;
      else if (cm > 255)
        *out++ = cupsImageDensity[255];
      else
        *out++ = cupsImageDensity[cm];

      if (cy < 0)
        *out++ = 0;
      else if (cy > 255)
        *out++ = cupsImageDensity[255];
      else
        *out++ = cupsImageDensity[cy];

      count --;
    }
  else
    while (count > 0)
    {
      c = *in++;
      m = *in++;
      y = *in++;
      k = *in++;

      c += k;
      m += k;
      y += k;

      if (c < 255)
        *out++ = c;
      else
        *out++ = 255;

      if (m < 255)
        *out++ = y;
      else
        *out++ = 255;

      if (y < 255)
        *out++ = y;
      else
        *out++ = 255;

      count --;
    }
}


/*
 * 'cupsImageCMYKToCMYK()' - Convert CMYK colors to CMYK.
 */

void
cupsImageCMYKToCMYK(
    const cups_ib_t *in,		/* I - Input pixels */
    cups_ib_t       *out,		/* I - Output pixels */
    int             count)		/* I - Number of pixels */
{
  int	c, m, y, k;			/* CMYK values */
  int	cc, cm, cy;			/* Calibrated CMY values */


  if (cupsImageHaveProfile)
    while (count > 0)
    {
      c = *in++;
      m = *in++;
      y = *in++;
      k = *in++;

      cc = (cupsImageMatrix[0][0][c] +
            cupsImageMatrix[0][1][m] +
	    cupsImageMatrix[0][2][y]);
      cm = (cupsImageMatrix[1][0][c] +
            cupsImageMatrix[1][1][m] +
	    cupsImageMatrix[1][2][y]);
      cy = (cupsImageMatrix[2][0][c] +
            cupsImageMatrix[2][1][m] +
	    cupsImageMatrix[2][2][y]);

      if (cc < 0)
        *out++ = 0;
      else if (cc > 255)
        *out++ = cupsImageDensity[255];
      else
        *out++ = cupsImageDensity[cc];

      if (cm < 0)
        *out++ = 0;
      else if (cm > 255)
        *out++ = cupsImageDensity[255];
      else
        *out++ = cupsImageDensity[cm];

      if (cy < 0)
        *out++ = 0;
      else if (cy > 255)
        *out++ = cupsImageDensity[255];
      else
        *out++ = cupsImageDensity[cy];

      *out++ = cupsImageDensity[k];

      count --;
    }
  else if (in != out)
  {
    while (count > 0)
    {
      *out++ = *in++;
      *out++ = *in++;
      *out++ = *in++;
      *out++ = *in++;

      count --;
    }
  }
}


/*
 * 'cupsImageCMYKToRGB()' - Convert CMYK colors to device-dependent RGB.
 */

void
cupsImageCMYKToRGB(
    const cups_ib_t *in,		/* I - Input pixels */
    cups_ib_t       *out,		/* I - Output pixels */
    int             count)		/* I - Number of pixels */
{
  int	c, m, y, k;			/* CMYK values */
  int	cr, cg, cb;			/* Calibrated RGB values */


  if (cupsImageHaveProfile)
  {
    while (count > 0)
    {
      c = *in++;
      m = *in++;
      y = *in++;
      k = *in++;

      cr = cupsImageMatrix[0][0][c] +
           cupsImageMatrix[0][1][m] +
           cupsImageMatrix[0][2][y] + k;
      cg = cupsImageMatrix[1][0][c] +
           cupsImageMatrix[1][1][m] +
	   cupsImageMatrix[1][2][y] + k;
      cb = cupsImageMatrix[2][0][c] +
           cupsImageMatrix[2][1][m] +
	   cupsImageMatrix[2][2][y] + k;

      if (cr < 0)
        *out++ = 255;
      else if (cr > 255)
        *out++ = 255 - cupsImageDensity[255];
      else
        *out++ = 255 - cupsImageDensity[cr];

      if (cg < 0)
        *out++ = 255;
      else if (cg > 255)
        *out++ = 255 - cupsImageDensity[255];
      else
        *out++ = 255 - cupsImageDensity[cg];

      if (cb < 0)
        *out++ = 255;
      else if (cb > 255)
        *out++ = 255 - cupsImageDensity[255];
      else
        *out++ = 255 - cupsImageDensity[cb];

      count --;
    }
  }
  else
  {
    while (count > 0)
    {
      c = 255 - *in++;
      m = 255 - *in++;
      y = 255 - *in++;
      k = *in++;

      c -= k;
      m -= k;
      y -= k;

      if (c > 0)
	*out++ = c;
      else
        *out++ = 0;

      if (m > 0)
	*out++ = m;
      else
        *out++ = 0;

      if (y > 0)
	*out++ = y;
      else
        *out++ = 0;

      if (cupsImageColorSpace == CUPS_CSPACE_CIELab ||
          cupsImageColorSpace >= CUPS_CSPACE_ICC1)
        rgb_to_lab(out - 3);
      else if (cupsImageColorSpace == CUPS_CSPACE_CIEXYZ)
        rgb_to_xyz(out - 3);

      count --;
    }
  }
}


/*
 * 'cupsImageCMYKToWhite()' - Convert CMYK colors to luminance.
 */

void
cupsImageCMYKToWhite(
    const cups_ib_t *in,		/* I - Input pixels */
    cups_ib_t       *out,		/* I - Output pixels */
    int             count)		/* I - Number of pixels */
{
  int	w;				/* White value */


  if (cupsImageHaveProfile)
  {
    while (count > 0)
    {
      w = 255 - (31 * in[0] + 61 * in[1] + 8 * in[2]) / 100 - in[3];

      if (w > 0)
        *out++ = cupsImageDensity[w];
      else
        *out++ = cupsImageDensity[0];

      in += 4;
      count --;
    }
  }
  else
  {
    while (count > 0)
    {
      w = 255 - (31 * in[0] + 61 * in[1] + 8 * in[2]) / 100 - in[3];

      if (w > 0)
        *out++ = w;
      else
        *out++ = 0;

      in += 4;
      count --;
    }
  }
}


/*
 * 'cupsImageLut()' - Adjust all pixel values with the given LUT.
 */

void
cupsImageLut(cups_ib_t       *pixels,	/* IO - Input/output pixels */
             int             count,	/* I  - Number of pixels/bytes to adjust */
             const cups_ib_t *lut)	/* I  - Lookup table */
{
  while (count > 0)
  {
    *pixels = lut[*pixels];
    pixels ++;
    count --;
  }
}


/*
 * 'cupsImageRGBAdjust()' - Adjust the hue and saturation of the given RGB colors.
 */

void
cupsImageRGBAdjust(cups_ib_t *pixels,	/* IO - Input/output pixels */
        	   int       count,	/* I - Number of pixels to adjust */
        	   int       saturation,/* I - Color saturation (%) */
        	   int       hue)	/* I - Color hue (degrees) */
{
  int			i, j, k;	/* Looping vars */
  float			mat[3][3];	/* Color adjustment matrix */
  static int		last_sat = 100,	/* Last saturation used */
			last_hue = 0;	/* Last hue used */
  static cups_clut_t	*lut = NULL;	/* Lookup table for matrix */


  if (saturation != last_sat || hue != last_hue || !lut)
  {
   /*
    * Build the color adjustment matrix...
    */

    ident(mat);
    saturate(mat, saturation * 0.01);
    huerotate(mat, (float)hue);

   /*
    * Allocate memory for the lookup table...
    */

    if (lut == NULL)
      lut = calloc(3, sizeof(cups_clut_t));

    if (lut == NULL)
      return;

   /*
    * Convert the matrix into a 3x3 array of lookup tables...
    */

    for (i = 0; i < 3; i ++)
      for (j = 0; j < 3; j ++)
        for (k = 0; k < 256; k ++)
          lut[i][j][k] = mat[i][j] * k + 0.5;

   /*
    * Save the saturation and hue to compare later...
    */

    last_sat = saturation;
    last_hue = hue;
  }

 /*
  * Adjust each pixel in the given buffer.
  */

  while (count > 0)
  {
    i = lut[0][0][pixels[0]] +
        lut[1][0][pixels[1]] +
        lut[2][0][pixels[2]];
    if (i < 0)
      pixels[0] = 0;
    else if (i > 255)
      pixels[0] = 255;
    else
      pixels[0] = i;

    i = lut[0][1][pixels[0]] +
        lut[1][1][pixels[1]] +
        lut[2][1][pixels[2]];
    if (i < 0)
      pixels[1] = 0;
    else if (i > 255)
      pixels[1] = 255;
    else
      pixels[1] = i;

    i = lut[0][2][pixels[0]] +
        lut[1][2][pixels[1]] +
        lut[2][2][pixels[2]];
    if (i < 0)
      pixels[2] = 0;
    else if (i > 255)
      pixels[2] = 255;
    else
      pixels[2] = i;

    count --;
    pixels += 3;
  }
}


/*
 * 'cupsImageRGBToBlack()' - Convert RGB data to black.
 */

void
cupsImageRGBToBlack(
    const cups_ib_t *in,		/* I - Input pixels */
    cups_ib_t       *out,		/* I - Output pixels */
    int             count)		/* I - Number of pixels */
{
  if (cupsImageHaveProfile)
    while (count > 0)
    {
      *out++ = cupsImageDensity[255 - (31 * in[0] + 61 * in[1] + 8 * in[2]) / 100];
      in += 3;
      count --;
    }
  else
    while (count > 0)
    {
      *out++ = 255 - (31 * in[0] + 61 * in[1] + 8 * in[2]) / 100;
      in += 3;
      count --;
    }
}


/*
 * 'cupsImageRGBToCMY()' - Convert RGB colors to CMY.
 */

void
cupsImageRGBToCMY(
    const cups_ib_t *in,		/* I - Input pixels */
    cups_ib_t       *out,		/* I - Output pixels */
    int             count)		/* I - Number of pixels */
{
  int	c, m, y, k;			/* CMYK values */
  int	cc, cm, cy;			/* Calibrated CMY values */


  if (cupsImageHaveProfile)
    while (count > 0)
    {
      c = 255 - *in++;
      m = 255 - *in++;
      y = 255 - *in++;
      k = min(c, min(m, y));
      c -= k;
      m -= k;
      y -= k;

      cc = cupsImageMatrix[0][0][c] +
           cupsImageMatrix[0][1][m] +
	   cupsImageMatrix[0][2][y] + k;
      cm = cupsImageMatrix[1][0][c] +
           cupsImageMatrix[1][1][m] +
	   cupsImageMatrix[1][2][y] + k;
      cy = cupsImageMatrix[2][0][c] +
           cupsImageMatrix[2][1][m] +
	   cupsImageMatrix[2][2][y] + k;

      if (cc < 0)
        *out++ = 0;
      else if (cc > 255)
        *out++ = cupsImageDensity[255];
      else
        *out++ = cupsImageDensity[cc];

      if (cm < 0)
        *out++ = 0;
      else if (cm > 255)
        *out++ = cupsImageDensity[255];
      else
        *out++ = cupsImageDensity[cm];

      if (cy < 0)
        *out++ = 0;
      else if (cy > 255)
        *out++ = cupsImageDensity[255];
      else
        *out++ = cupsImageDensity[cy];

      count --;
    }
  else
    while (count > 0)
    {
      c    = 255 - in[0];
      m    = 255 - in[1];
      y    = 255 - in[2];
      k    = min(c, min(m, y));

      *out++ = (255 - in[1] / 4) * (c - k) / 255 + k;
      *out++ = (255 - in[2] / 4) * (m - k) / 255 + k;
      *out++ = (255 - in[0] / 4) * (y - k) / 255 + k;
      in += 3;
      count --;
    }
}


/*
 * 'cupsImageRGBToCMYK()' - Convert RGB colors to CMYK.
 */

void
cupsImageRGBToCMYK(
    const cups_ib_t *in,		/* I - Input pixels */
    cups_ib_t       *out,		/* I - Output pixels */
    int             count)		/* I - Number of pixels */
{
  int	c, m, y, k,			/* CMYK values */
	km;				/* Maximum K value */
  int	cc, cm, cy;			/* Calibrated CMY values */


  if (cupsImageHaveProfile)
    while (count > 0)
    {
      c = 255 - *in++;
      m = 255 - *in++;
      y = 255 - *in++;
      k = min(c, min(m, y));

      if ((km = max(c, max(m, y))) > k)
        k = k * k * k / (km * km);

      c -= k;
      m -= k;
      y -= k;

      cc = (cupsImageMatrix[0][0][c] +
            cupsImageMatrix[0][1][m] +
	    cupsImageMatrix[0][2][y]);
      cm = (cupsImageMatrix[1][0][c] +
            cupsImageMatrix[1][1][m] +
	    cupsImageMatrix[1][2][y]);
      cy = (cupsImageMatrix[2][0][c] +
            cupsImageMatrix[2][1][m] +
	    cupsImageMatrix[2][2][y]);

      if (cc < 0)
        *out++ = 0;
      else if (cc > 255)
        *out++ = cupsImageDensity[255];
      else
        *out++ = cupsImageDensity[cc];

      if (cm < 0)
        *out++ = 0;
      else if (cm > 255)
        *out++ = cupsImageDensity[255];
      else
        *out++ = cupsImageDensity[cm];

      if (cy < 0)
        *out++ = 0;
      else if (cy > 255)
        *out++ = cupsImageDensity[255];
      else
        *out++ = cupsImageDensity[cy];

      *out++ = cupsImageDensity[k];

      count --;
    }
  else
    while (count > 0)
    {
      c = 255 - *in++;
      m = 255 - *in++;
      y = 255 - *in++;
      k = min(c, min(m, y));

      if ((km = max(c, max(m, y))) > k)
        k = k * k * k / (km * km);

      c -= k;
      m -= k;
      y -= k;

      *out++ = c;
      *out++ = m;
      *out++ = y;
      *out++ = k;

      count --;
    }
}


/*
 * 'cupsImageRGBToRGB()' - Convert RGB colors to device-dependent RGB.
 */

void
cupsImageRGBToRGB(
    const cups_ib_t *in,		/* I - Input pixels */
    cups_ib_t       *out,		/* I - Output pixels */
    int             count)		/* I - Number of pixels */
{
  int	c, m, y, k;			/* CMYK values */
  int	cr, cg, cb;			/* Calibrated RGB values */


  if (cupsImageHaveProfile)
  {
    while (count > 0)
    {
      c = 255 - *in++;
      m = 255 - *in++;
      y = 255 - *in++;
      k = min(c, min(m, y));
      c -= k;
      m -= k;
      y -= k;

      cr = cupsImageMatrix[0][0][c] +
           cupsImageMatrix[0][1][m] +
           cupsImageMatrix[0][2][y] + k;
      cg = cupsImageMatrix[1][0][c] +
           cupsImageMatrix[1][1][m] +
	   cupsImageMatrix[1][2][y] + k;
      cb = cupsImageMatrix[2][0][c] +
           cupsImageMatrix[2][1][m] +
	   cupsImageMatrix[2][2][y] + k;

      if (cr < 0)
        *out++ = 255;
      else if (cr > 255)
        *out++ = 255 - cupsImageDensity[255];
      else
        *out++ = 255 - cupsImageDensity[cr];

      if (cg < 0)
        *out++ = 255;
      else if (cg > 255)
        *out++ = 255 - cupsImageDensity[255];
      else
        *out++ = 255 - cupsImageDensity[cg];

      if (cb < 0)
        *out++ = 255;
      else if (cb > 255)
        *out++ = 255 - cupsImageDensity[255];
      else
        *out++ = 255 - cupsImageDensity[cb];

      count --;
    }
  }
  else
  {
    if (in != out)
      memcpy(out, in, count * 3);

    if (cupsImageColorSpace == CUPS_CSPACE_CIELab ||
        cupsImageColorSpace >= CUPS_CSPACE_ICC1)
    {
      while (count > 0)
      {
        rgb_to_lab(out);

	out += 3;
	count --;
      }
    }
    else if (cupsImageColorSpace == CUPS_CSPACE_CIEXYZ)
    {
      while (count > 0)
      {
        rgb_to_xyz(out);

	out += 3;
	count --;
      }
    }
  }
}


/*
 * 'cupsImageRGBToWhite()' - Convert RGB colors to luminance.
 */

void
cupsImageRGBToWhite(
    const cups_ib_t *in,		/* I - Input pixels */
    cups_ib_t       *out,		/* I - Output pixels */
    int             count)		/* I - Number of pixels */
{
  if (cupsImageHaveProfile)
  {
    while (count > 0)
    {
      *out++ = 255 - cupsImageDensity[255 - (31 * in[0] + 61 * in[1] + 8 * in[2]) / 100];
      in += 3;
      count --;
    }
  }
  else
  {
    while (count > 0)
    {
      *out++ = (31 * in[0] + 61 * in[1] + 8 * in[2]) / 100;
      in += 3;
      count --;
    }
  }
}


/*
 * 'cupsImageSetProfile()' - Set the device color profile.
 */

void
cupsImageSetProfile(float d,		/* I - Ink/marker density */
                    float g,		/* I - Ink/marker gamma */
                    float matrix[3][3])	/* I - Color transform matrix */
{
  int	i, j, k;			/* Looping vars */
  float	m;				/* Current matrix value */
  int	*im;				/* Pointer into cupsImageMatrix */


 /*
  * Allocate memory for the profile data...
  */

  if (cupsImageMatrix == NULL)
    cupsImageMatrix = calloc(3, sizeof(cups_clut_t));

  if (cupsImageMatrix == NULL)
    return;

  if (cupsImageDensity == NULL)
    cupsImageDensity = calloc(256, sizeof(int));

  if (cupsImageDensity == NULL)
    return;

 /*
  * Populate the profile lookup tables...
  */

  cupsImageHaveProfile  = 1;

  for (i = 0, im = cupsImageMatrix[0][0]; i < 3; i ++)
    for (j = 0; j < 3; j ++)
      for (k = 0, m = matrix[i][j]; k < 256; k ++)
        *im++ = (int)(k * m + 0.5);

  for (k = 0, im = cupsImageDensity; k < 256; k ++)
    *im++ = 255.0 * d * pow((float)k / 255.0, g) + 0.5;
}


/*
 * 'cupsImageSetRasterColorSpace()' - Set the destination colorspace.
 */

void
cupsImageSetRasterColorSpace(
    cups_cspace_t cs)			/* I - Destination colorspace */
{
 /*
  * Set the destination colorspace...
  */

  cupsImageColorSpace = cs;

 /*
  * Don't use color profiles in colorimetric colorspaces...
  */

  if (cs == CUPS_CSPACE_CIEXYZ ||
      cs == CUPS_CSPACE_CIELab ||
      cs >= CUPS_CSPACE_ICC1)
    cupsImageHaveProfile = 0;
}


/*
 * 'cupsImageWhiteToBlack()' - Convert luminance colors to black.
 */

void
cupsImageWhiteToBlack(
    const cups_ib_t *in,		/* I - Input pixels */
    cups_ib_t       *out,		/* I - Output pixels */
    int             count)		/* I - Number of pixels */
{
  if (cupsImageHaveProfile)
    while (count > 0)
    {
      *out++ = cupsImageDensity[255 - *in++];
      count --;
    }
  else
    while (count > 0)
    {
      *out++ = 255 - *in++;
      count --;
    }
}


/*
 * 'cupsImageWhiteToCMY()' - Convert luminance colors to CMY.
 */

void
cupsImageWhiteToCMY(
    const cups_ib_t *in,		/* I - Input pixels */
    cups_ib_t       *out,		/* I - Output pixels */
    int             count)		/* I - Number of pixels */
{
  if (cupsImageHaveProfile)
    while (count > 0)
    {
      out[0] = cupsImageDensity[255 - *in++];
      out[1] = out[0];
      out[2] = out[0];
      out += 3;
      count --;
    }
  else
    while (count > 0)
    {
      *out++ = 255 - *in;
      *out++ = 255 - *in;
      *out++ = 255 - *in++;
      count --;
    }
}


/*
 * 'cupsImageWhiteToCMYK()' - Convert luminance colors to CMYK.
 */

void
cupsImageWhiteToCMYK(
    const cups_ib_t *in,		/* I - Input pixels */
    cups_ib_t       *out,		/* I - Output pixels */
    int             count)		/* I - Number of pixels */
{
  if (cupsImageHaveProfile)
    while (count > 0)
    {
      *out++ = 0;
      *out++ = 0;
      *out++ = 0;
      *out++ = cupsImageDensity[255 - *in++];
      count --;
    }
  else
    while (count > 0)
    {
      *out++ = 0;
      *out++ = 0;
      *out++ = 0;
      *out++ = 255 - *in++;
      count --;
    }
}


/*
 * 'cupsImageWhiteToRGB()' - Convert luminance data to RGB.
 */

void
cupsImageWhiteToRGB(
    const cups_ib_t *in,		/* I - Input pixels */
    cups_ib_t       *out,		/* I - Output pixels */
    int             count)		/* I - Number of pixels */
{
  if (cupsImageHaveProfile)
  {
    while (count > 0)
    {
      out[0] = 255 - cupsImageDensity[255 - *in++];
      out[1] = out[0];
      out[2] = out[0];
      out += 3;
      count --;
    }
  }
  else
  {
    while (count > 0)
    {
      *out++ = *in;
      *out++ = *in;
      *out++ = *in++;

      if (cupsImageColorSpace == CUPS_CSPACE_CIELab ||
          cupsImageColorSpace >= CUPS_CSPACE_ICC1)
        rgb_to_lab(out - 3);
      else if (cupsImageColorSpace == CUPS_CSPACE_CIEXYZ)
        rgb_to_xyz(out - 3);

      count --;
    }
  }
}


/*
 * 'cupsImageWhiteToWhite()' - Convert luminance colors to device-dependent
 *                             luminance.
 */

void
cupsImageWhiteToWhite(
    const cups_ib_t *in,		/* I - Input pixels */
    cups_ib_t       *out,		/* I - Output pixels */
    int             count)		/* I - Number of pixels */
{
  if (cupsImageHaveProfile)
    while (count > 0)
    {
      *out++ = 255 - cupsImageDensity[255 - *in++];
      count --;
    }
  else if (in != out)
    memcpy(out, in, count);
}


/*
 * 'cielab()' - Map CIE Lab transformation...
 */

static float				/* O - Adjusted color value */
cielab(float x,				/* I - Raw color value */
       float xn)			/* I - Whitepoint color value */
{
  float x_xn;				/* Fraction of whitepoint */


  x_xn = x / xn;

  if (x_xn > 0.008856)
    return (cbrt(x_xn));
  else
    return (7.787 * x_xn + 16.0 / 116.0);
}


/* 
 * 'huerotate()' - Rotate the hue, maintaining luminance.
 */

static void
huerotate(float mat[3][3],		/* I - Matrix to append to */
          float rot)			/* I - Hue rotation in degrees */
{
  float hmat[3][3];			/* Hue matrix */
  float lx, ly, lz;			/* Luminance vector */
  float xrs, xrc;			/* X rotation sine/cosine */
  float yrs, yrc;			/* Y rotation sine/cosine */
  float zrs, zrc;			/* Z rotation sine/cosine */
  float zsx, zsy;			/* Z shear x/y */


 /*
  * Load the identity matrix...
  */

  ident(hmat);

 /*
  * Rotate the grey vector into positive Z...
  */

  xrs = M_SQRT1_2;
  xrc = M_SQRT1_2;
  xrotate(hmat,xrs,xrc);

  yrs = -1.0 / sqrt(3.0);
  yrc = -M_SQRT2 * yrs;
  yrotate(hmat,yrs,yrc);

 /*
  * Shear the space to make the luminance plane horizontal...
  */

  xform(hmat, 0.3086, 0.6094, 0.0820, &lx, &ly, &lz);
  zsx = lx / lz;
  zsy = ly / lz;
  zshear(hmat, zsx, zsy);

 /*
  * Rotate the hue...
  */

  zrs = sin(rot * M_PI / 180.0);
  zrc = cos(rot * M_PI / 180.0);

  zrotate(hmat, zrs, zrc);

 /*
  * Unshear the space to put the luminance plane back...
  */

  zshear(hmat, -zsx, -zsy);

 /*
  * Rotate the grey vector back into place...
  */

  yrotate(hmat, -yrs, yrc);
  xrotate(hmat, -xrs, xrc);

 /*
  * Append it to the current matrix...
  */

  mult(hmat, mat, mat);
}


/* 
 * 'ident()' - Make an identity matrix.
 */

static void
ident(float mat[3][3])			/* I - Matrix to identify */
{
  mat[0][0] = 1.0;
  mat[0][1] = 0.0;
  mat[0][2] = 0.0;
  mat[1][0] = 0.0;
  mat[1][1] = 1.0;
  mat[1][2] = 0.0;
  mat[2][0] = 0.0;
  mat[2][1] = 0.0;
  mat[2][2] = 1.0;
}


/* 
 * 'mult()' - Multiply two matrices.
 */

static void
mult(float a[3][3],			/* I - First matrix */
     float b[3][3],			/* I - Second matrix */
     float c[3][3])			/* I - Destination matrix */
{
  int	x, y;				/* Looping vars */
  float	temp[3][3];			/* Temporary matrix */


 /*
  * Multiply a and b, putting the result in temp...
  */

  for (y = 0; y < 3; y ++)
    for (x = 0; x < 3; x ++)
      temp[y][x] = b[y][0] * a[0][x] +
                   b[y][1] * a[1][x] +
                   b[y][2] * a[2][x];

 /*
  * Copy temp to c (that way c can be a pointer to a or b).
  */

  memcpy(c, temp, sizeof(temp));
}


/*
 * 'rgb_to_lab()' - Convert an RGB color to CIE Lab.
 */

static void
rgb_to_lab(cups_ib_t *val)		/* IO - Color value */
{
  float	r,				/* Red value */
	g,				/* Green value */
	b,				/* Blue value */
	ciex,				/* CIE X value */
	ciey,				/* CIE Y value */
	ciez,				/* CIE Z value */
	ciey_yn,			/* Normalized luminance */
	ciel,				/* CIE L value */
	ciea,				/* CIE a value */
	cieb;				/* CIE b value */


 /*
  * Convert sRGB to linear RGB...
  */

  r = pow((val[0] / 255.0 + 0.055) / 1.055, 2.4);
  g = pow((val[1] / 255.0 + 0.055) / 1.055, 2.4);
  b = pow((val[2] / 255.0 + 0.055) / 1.055, 2.4);

 /*
  * Convert to CIE XYZ...
  */

  ciex = 0.412453 * r + 0.357580 * g + 0.180423 * b; 
  ciey = 0.212671 * r + 0.715160 * g + 0.072169 * b;
  ciez = 0.019334 * r + 0.119193 * g + 0.950227 * b;

 /*
  * Normalize and convert to CIE Lab...
  */

  ciey_yn = ciey / D65_Y;

  if (ciey_yn > 0.008856)
    ciel = 116 * cbrt(ciey_yn) - 16;
  else
    ciel = 903.3 * ciey_yn;

  ciel = ciel;
  ciea = 500 * (cielab(ciex, D65_X) - cielab(ciey, D65_Y));
  cieb = 200 * (cielab(ciey, D65_Y) - cielab(ciez, D65_Z));

 /*
  * Scale the L value and bias the a and b values by 128 so that all
  * numbers are from 0 to 255.
  */

  ciel = ciel * 2.55 + 0.5;
  ciea += 128.5;
  cieb += 128.5;

 /*
  * Output 8-bit values...
  */

  if (ciel < 0.0)
    val[0] = 0;
  else if (ciel < 255.0)
    val[0] = (int)ciel;
  else
    val[0] = 255;

  if (ciea < 0.0)
    val[1] = 0;
  else if (ciea < 255.0)
    val[1] = (int)ciea;
  else
    val[1] = 255;

  if (cieb < 0.0)
    val[2] = 0;
  else if (cieb < 255.0)
    val[2] = (int)cieb;
  else
    val[2] = 255;
}


/*
 * 'rgb_to_xyz()' - Convert an RGB color to CIE XYZ.
 */

static void
rgb_to_xyz(cups_ib_t *val)		/* IO - Color value */
{
  float	r,				/* Red value */
	g,				/* Green value */
	b,				/* Blue value */
	ciex,				/* CIE X value */
	ciey,				/* CIE Y value */
	ciez;				/* CIE Z value */


 /*
  * Convert sRGB to linear RGB...
  */

  r = pow((val[0] / 255.0 + 0.055) / 1.055, 2.4);
  g = pow((val[1] / 255.0 + 0.055) / 1.055, 2.4);
  b = pow((val[2] / 255.0 + 0.055) / 1.055, 2.4);

 /*
  * Convert to CIE XYZ...
  */

  ciex = 0.412453 * r + 0.357580 * g + 0.180423 * b; 
  ciey = 0.212671 * r + 0.715160 * g + 0.072169 * b;
  ciez = 0.019334 * r + 0.119193 * g + 0.950227 * b;

 /*
  * Encode as 8-bit XYZ...
  */

  if (ciex < 0.0f)
    val[0] = 0;
  else if (ciex < 1.1f)
    val[0] = (int)(231.8181f * ciex + 0.5);
  else
    val[0] = 255;

  if (ciey < 0.0f)
    val[1] = 0;
  else if (ciey < 1.1f)
    val[1] = (int)(231.8181f * ciey + 0.5);
  else
    val[1] = 255;

  if (ciez < 0.0f)
    val[2] = 0;
  else if (ciez < 1.1f)
    val[2] = (int)(231.8181f * ciez + 0.5);
  else
    val[2] = 255;
}


/* 
 * 'saturate()' - Make a saturation matrix.
 */

static void
saturate(float mat[3][3],		/* I - Matrix to append to */
         float sat)			/* I - Desired color saturation */
{
  float	smat[3][3];			/* Saturation matrix */


  smat[0][0] = (1.0 - sat) * 0.3086 + sat;
  smat[0][1] = (1.0 - sat) * 0.3086;
  smat[0][2] = (1.0 - sat) * 0.3086;
  smat[1][0] = (1.0 - sat) * 0.6094;
  smat[1][1] = (1.0 - sat) * 0.6094 + sat;
  smat[1][2] = (1.0 - sat) * 0.6094;
  smat[2][0] = (1.0 - sat) * 0.0820;
  smat[2][1] = (1.0 - sat) * 0.0820;
  smat[2][2] = (1.0 - sat) * 0.0820 + sat;

  mult(smat, mat, mat);
}


/* 
 * 'xform()' - Transform a 3D point using a matrix...
 */

static void
xform(float mat[3][3],			/* I - Matrix */
      float x,				/* I - Input X coordinate */
      float y,				/* I - Input Y coordinate */
      float z,				/* I - Input Z coordinate */
      float *tx,			/* O - Output X coordinate */
      float *ty,			/* O - Output Y coordinate */
      float *tz)			/* O - Output Z coordinate */
{
  *tx = x * mat[0][0] + y * mat[1][0] + z * mat[2][0];
  *ty = x * mat[0][1] + y * mat[1][1] + z * mat[2][1];
  *tz = x * mat[0][2] + y * mat[1][2] + z * mat[2][2];
}


/* 
 * 'xrotate()' - Rotate about the x (red) axis...
 */

static void
xrotate(float mat[3][3],		/* I - Matrix */
        float rs,			/* I - Rotation angle sine */
        float rc)			/* I - Rotation angle cosine */
{
  float rmat[3][3];			/* I - Rotation matrix */


  rmat[0][0] = 1.0;
  rmat[0][1] = 0.0;
  rmat[0][2] = 0.0;

  rmat[1][0] = 0.0;
  rmat[1][1] = rc;
  rmat[1][2] = rs;

  rmat[2][0] = 0.0;
  rmat[2][1] = -rs;
  rmat[2][2] = rc;

  mult(rmat, mat, mat);
}


/* 
 * 'yrotate()' - Rotate about the y (green) axis...
 */

static void
yrotate(float mat[3][3],		/* I - Matrix */
        float rs,			/* I - Rotation angle sine */
        float rc)			/* I - Rotation angle cosine */
{
  float rmat[3][3];			/* I - Rotation matrix */


  rmat[0][0] = rc;
  rmat[0][1] = 0.0;
  rmat[0][2] = -rs;

  rmat[1][0] = 0.0;
  rmat[1][1] = 1.0;
  rmat[1][2] = 0.0;

  rmat[2][0] = rs;
  rmat[2][1] = 0.0;
  rmat[2][2] = rc;

  mult(rmat,mat,mat);
}


/* 
 * 'zrotate()' - Rotate about the z (blue) axis...
 */

static void
zrotate(float mat[3][3],		/* I - Matrix */
        float rs,			/* I - Rotation angle sine */
        float rc)			/* I - Rotation angle cosine */
{
  float rmat[3][3];			/* I - Rotation matrix */


  rmat[0][0] = rc;
  rmat[0][1] = rs;
  rmat[0][2] = 0.0;

  rmat[1][0] = -rs;
  rmat[1][1] = rc;
  rmat[1][2] = 0.0;

  rmat[2][0] = 0.0;
  rmat[2][1] = 0.0;
  rmat[2][2] = 1.0;

  mult(rmat,mat,mat);
}


/* 
 * 'zshear()' - Shear z using x and y...
 */

static void
zshear(float mat[3][3],			/* I - Matrix */
       float dx,			/* I - X shear */
       float dy)			/* I - Y shear */
{
  float smat[3][3];			/* Shear matrix */


  smat[0][0] = 1.0;
  smat[0][1] = 0.0;
  smat[0][2] = dx;

  smat[1][0] = 0.0;
  smat[1][1] = 1.0;
  smat[1][2] = dy;

  smat[2][0] = 0.0;
  smat[2][1] = 0.0;
  smat[2][2] = 1.0;

  mult(smat, mat, mat);
}


/*
 * End of "$Id: image-colorspace.c 7720 2008-07-11 22:46:21Z mike $".
 */
