/*
 * "$Id: image-colorspace.c,v 1.19 2000/08/29 21:23:08 mike Exp $"
 *
 *   Colorspace conversions for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1993-2000 by Easy Software Products.
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
 * Contents:
 *
 *   ImageWhiteToWhite() - Convert luminance colors to device-dependent
 *   ImageWhiteToRGB()   - Convert luminance data to RGB.
 *   ImageWhiteToBlack() - Convert luminance colors to black.
 *   ImageWhiteToCMY()   - Convert luminance colors to CMY.
 *   ImageWhiteToCMYK()  - Convert luminance colors to CMYK.
 *   ImageRGBToBlack()   - Convert RGB data to black.
 *   ImageRGBToCMY()     - Convert RGB colors to CMY.
 *   ImageRGBToCMYK()    - Convert RGB colors to CMYK.
 *   ImageRGBToWhite()   - Convert RGB colors to luminance.
 *   ImageRGBToRGB()     - Convert RGB colors to device-dependent RGB.
 *   ImageLut()          - Adjust all pixel values with the given LUT.
 *   ImageRGBAdjust()    - Adjust the hue and saturation of the given RGB
 *                         colors.
 *   huerotate()         - Rotate the hue, maintaining luminance.
 *   ident()             - Make an identity matrix.
 *   mult()              - Multiply two matrices.
 *   saturate()          - Make a saturation matrix.
 *   xform()             - Transform a 3D point using a matrix...
 *   xrotate()           - Rotate about the x (red) axis...
 *   yrotate()           - Rotate about the y (green) axis...
 *   zrotate()           - Rotate about the z (blue) axis...
 *   zshear()            - Shear z using x and y...
 */

/*
 * Include necessary headers...
 */

#include "image.h"
#include <math.h>


/*
 * Globals...
 */

extern int	ImageHaveProfile;
extern int	ImageDensity[256];
extern int	ImageMatrix[3][3][256];

/*
 * Local functions...
 */

static void	huerotate(float [3][3], float);
static void	ident(float [3][3]);
static void	mult(float [3][3], float [3][3], float [3][3]);
static void	saturate(float [3][3], float);
static void	xform(float [3][3], float, float, float, float *, float *, float *);
static void	xrotate(float [3][3], float, float);
static void	yrotate(float [3][3], float, float);
static void	zrotate(float [3][3], float, float);
static void	zshear(float [3][3], float, float);


/*
 * 'ImageWhiteToWhite()' - Convert luminance colors to device-dependent
 *                         luminance.
 */

void
ImageWhiteToWhite(const ib_t *in,	/* I - Input pixels */
                  ib_t       *out,	/* I - Output pixels */
                  int        count)	/* I - Number of pixels */
{
  if (ImageHaveProfile)
    while (count > 0)
    {
      *out++ = 255 - ImageDensity[255 - *in++];
      count --;
    }
  else if (in != out)
    memcpy(out, in, count);
}


/*
 * 'ImageWhiteToRGB()' - Convert luminance data to RGB.
 */

void
ImageWhiteToRGB(const ib_t *in,		/* I - Input pixels */
                ib_t       *out,	/* I - Output pixels */
                int        count)	/* I - Number of pixels */
{
  if (ImageHaveProfile)
    while (count > 0)
    {
      out[0] = 255 - ImageDensity[255 - *in++];
      out[1] = out[0];
      out[2] = out[0];
      out += 3;
      count --;
    }
  else
    while (count > 0)
    {
      *out++ = *in;
      *out++ = *in;
      *out++ = *in++;
      count --;
    }
}


/*
 * 'ImageWhiteToBlack()' - Convert luminance colors to black.
 */

void
ImageWhiteToBlack(const ib_t *in,	/* I - Input pixels */
                  ib_t       *out,	/* I - Output pixels */
                  int        count)	/* I - Number of pixels */
{
  if (ImageHaveProfile)
    while (count > 0)
    {
      *out++ = ImageDensity[255 - *in++];
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
 * 'ImageWhiteToCMY()' - Convert luminance colors to CMY.
 */

void
ImageWhiteToCMY(const ib_t *in,		/* I - Input pixels */
                ib_t       *out,	/* I - Output pixels */
                int        count)	/* I - Number of pixels */
{
  if (ImageHaveProfile)
    while (count > 0)
    {
      out[0] = ImageDensity[255 - *in++];
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
 * 'ImageWhiteToCMYK()' - Convert luminance colors to CMYK.
 */

void
ImageWhiteToCMYK(const ib_t *in,	/* I - Input pixels */
                 ib_t       *out,	/* I - Output pixels */
                 int        count)	/* I - Number of pixels */
{
  if (ImageHaveProfile)
    while (count > 0)
    {
      *out++ = 0;
      *out++ = 0;
      *out++ = 0;
      *out++ = ImageDensity[255 - *in++];
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
 * 'ImageRGBToBlack()' - Convert RGB data to black.
 */

void
ImageRGBToBlack(const ib_t *in,		/* I - Input pixels */
                ib_t       *out,	/* I - Output pixels */
                int        count)	/* I - Number of pixels */
{
  if (ImageHaveProfile)
    while (count > 0)
    {
      *out++ = ImageDensity[255 - (31 * in[0] + 61 * in[1] + 8 * in[2]) / 100];
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
 * 'ImageRGBToCMY()' - Convert RGB colors to CMY.
 */

void
ImageRGBToCMY(const ib_t *in,	/* I - Input pixels */
              ib_t       *out,	/* I - Output pixels */
              int        count)	/* I - Number of pixels */
{
  int	c, m, y, k;		/* CMYK values */
  int	cc, cm, cy;		/* Calibrated CMY values */


  if (ImageHaveProfile)
    while (count > 0)
    {
      c = 255 - *in++;
      m = 255 - *in++;
      y = 255 - *in++;
      k = min(c, min(m, y));
      c -= k;
      m -= k;
      y -= k;

      cc = ImageMatrix[0][0][c] +
           ImageMatrix[0][1][m] +
	   ImageMatrix[0][2][y] + k;
      cm = ImageMatrix[1][0][c] +
           ImageMatrix[1][1][m] +
	   ImageMatrix[1][2][y] + k;
      cy = ImageMatrix[2][0][c] +
           ImageMatrix[2][1][m] +
	   ImageMatrix[2][2][y] + k;

      if (cc < 0)
        *out++ = 0;
      else if (cc > 255)
        *out++ = ImageDensity[255];
      else
        *out++ = ImageDensity[cc];

      if (cm < 0)
        *out++ = 0;
      else if (cm > 255)
        *out++ = ImageDensity[255];
      else
        *out++ = ImageDensity[cm];

      if (cy < 0)
        *out++ = 0;
      else if (cy > 255)
        *out++ = ImageDensity[255];
      else
        *out++ = ImageDensity[cy];

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
 * 'ImageRGBToCMYK()' - Convert RGB colors to CMYK.
 */

void
ImageRGBToCMYK(const ib_t *in,	/* I - Input pixels */
               ib_t       *out,	/* I - Output pixels */
               int        count)/* I - Number of pixels */
{
  int	c, m, y, k,		/* CMYK values */
	km;			/* Maximum K value */
  int	cc, cm, cy;		/* Calibrated CMY values */


  if (ImageHaveProfile)
    while (count > 0)
    {
      c = 255 - *in++;
      m = 255 - *in++;
      y = 255 - *in++;
      k = min(c, min(m, y));

      if ((km = max(c, max(m, y))) > k)
        k = k * k * k / km / km;

      c -= k;
      m -= k;
      y -= k;

      cc = (ImageMatrix[0][0][c] +
            ImageMatrix[0][1][m] +
	    ImageMatrix[0][2][y]);
      cm = (ImageMatrix[1][0][c] +
            ImageMatrix[1][1][m] +
	    ImageMatrix[1][2][y]);
      cy = (ImageMatrix[2][0][c] +
            ImageMatrix[2][1][m] +
	    ImageMatrix[2][2][y]);

      if (cc < 0)
        *out++ = 0;
      else if (cc > 255)
        *out++ = ImageDensity[255];
      else
        *out++ = ImageDensity[cc];

      if (cm < 0)
        *out++ = 0;
      else if (cm > 255)
        *out++ = ImageDensity[255];
      else
        *out++ = ImageDensity[cm];

      if (cy < 0)
        *out++ = 0;
      else if (cy > 255)
        *out++ = ImageDensity[255];
      else
        *out++ = ImageDensity[cy];

      *out++ = ImageDensity[k];

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
        k = k * k / km;

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
 * 'ImageRGBToWhite()' - Convert RGB colors to luminance.
 */

void
ImageRGBToWhite(const ib_t *in,		/* I - Input pixels */
                ib_t       *out,	/* I - Output pixels */
                int        count)	/* I - Number of pixels */
{
  if (ImageHaveProfile)
    while (count > 0)
    {
      *out++ = 255 - ImageDensity[255 - (31 * in[0] + 61 * in[1] + 8 * in[2]) / 100];
      in += 3;
      count --;
    }
  else
    while (count > 0)
    {
      *out++ = (31 * in[0] + 61 * in[1] + 8 * in[2]) / 100;
      in += 3;
      count --;
    }
}


/*
 * 'ImageRGBToRGB()' - Convert RGB colors to device-dependent RGB.
 */

void
ImageRGBToRGB(const ib_t *in,	/* I - Input pixels */
              ib_t       *out,	/* I - Output pixels */
              int        count)	/* I - Number of pixels */
{
  int	c, m, y, k;		/* CMYK values */
  int	cr, cg, cb;		/* Calibrated RGB values */


  if (ImageHaveProfile)
    while (count > 0)
    {
      c = 255 - *in++;
      m = 255 - *in++;
      y = 255 - *in++;
      k = min(c, min(m, y));
      c -= k;
      m -= k;
      y -= k;

      cr = ImageMatrix[0][0][c] +
           ImageMatrix[0][1][m] +
           ImageMatrix[0][2][y] + k;
      cg = ImageMatrix[1][0][c] +
           ImageMatrix[1][1][m] +
	   ImageMatrix[1][2][y] + k;
      cb = ImageMatrix[2][0][c] +
           ImageMatrix[2][1][m] +
	   ImageMatrix[2][2][y] + k;

      if (cr < 0)
        *out++ = 255;
      else if (cr > 255)
        *out++ = 255 - ImageDensity[255];
      else
        *out++ = 255 - ImageDensity[cr];

      if (cg < 0)
        *out++ = 255;
      else if (cg > 255)
        *out++ = 255 - ImageDensity[255];
      else
        *out++ = 255 - ImageDensity[cg];

      if (cb < 0)
        *out++ = 255;
      else if (cb > 255)
        *out++ = 255 - ImageDensity[255];
      else
        *out++ = 255 - ImageDensity[cb];

      count --;
    }
  else if (in != out)
    memcpy(out, in, count * 3);
}


/*
 * 'ImageLut()' - Adjust all pixel values with the given LUT.
 */

void
ImageLut(ib_t       *pixels,	/* IO - Input/output pixels */
         int        count,	/* I - Number of pixels/bytes to adjust */
         const ib_t *lut)	/* I - Lookup table */
{
  while (count > 0)
  {
    *pixels = lut[*pixels];
    pixels ++;
    count --;
  }
}


/*
 * 'ImageRGBAdjust()' - Adjust the hue and saturation of the given RGB colors.
 */

void
ImageRGBAdjust(ib_t *pixels,	/* IO - Input/output pixels */
               int  count,	/* I - Number of pixels to adjust */
               int  saturation,	/* I - Color saturation (%) */
               int  hue)	/* I - Color hue (degrees) */
{
  int		i, j, k;	/* Looping vars */
  float		mat[3][3];	/* Color adjustment matrix */
  static int	last_sat = 100,	/* Last saturation used */
		last_hue = 0;	/* Last hue used */
  static int	lut[3][3][256];	/* Lookup table for matrix */


  if (saturation != last_sat ||
      hue != last_hue)
  {
   /*
    * Build the color adjustment matrix...
    */

    ident(mat);
    saturate(mat, saturation * 0.01);
    huerotate(mat, (float)hue);

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
 * The color saturation/hue matrix stuff is provided thanks to Mr. Paul
 * Haeberli at "http://www.sgi.com/grafica/matrix/index.html".
 */

/* 
 * 'huerotate()' - Rotate the hue, maintaining luminance.
 */

static void
huerotate(float mat[3][3],	/* I - Matrix to append to */
          float rot)		/* I - Hue rotation in degrees */
{
  float hmat[3][3];		/* Hue matrix */
  float lx, ly, lz;		/* Luminance vector */
  float xrs, xrc;		/* X rotation sine/cosine */
  float yrs, yrc;		/* Y rotation sine/cosine */
  float zrs, zrc;		/* Z rotation sine/cosine */
  float zsx, zsy;		/* Z shear x/y */


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
ident(float mat[3][3])	/* I - Matrix to identify */
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
mult(float a[3][3],	/* I - First matrix */
     float b[3][3],	/* I - Second matrix */
     float c[3][3])	/* I - Destination matrix */
{
  int	x, y;		/* Looping vars */
  float	temp[3][3];	/* Temporary matrix */


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
 * 'saturate()' - Make a saturation matrix.
 */

static void
saturate(float mat[3][3],	/* I - Matrix to append to */
         float sat)		/* I - Desired color saturation */
{
  float	smat[3][3];		/* Saturation matrix */


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
xform(float mat[3][3],	/* I - Matrix */
      float x,		/* I - Input X coordinate */
      float y,		/* I - Input Y coordinate */
      float z,		/* I - Input Z coordinate */
      float *tx,	/* O - Output X coordinate */
      float *ty,	/* O - Output Y coordinate */
      float *tz)	/* O - Output Z coordinate */
{
  *tx = x * mat[0][0] + y * mat[1][0] + z * mat[2][0];
  *ty = x * mat[0][1] + y * mat[1][1] + z * mat[2][1];
  *tz = x * mat[0][2] + y * mat[1][2] + z * mat[2][2];
}


/* 
 * 'xrotate()' - Rotate about the x (red) axis...
 */

static void
xrotate(float mat[3][3],	/* I - Matrix */
        float rs,		/* I - Rotation angle sine */
        float rc)		/* I - Rotation angle cosine */
{
  float rmat[3][3];		/* I - Rotation matrix */


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
yrotate(float mat[3][3],	/* I - Matrix */
        float rs,		/* I - Rotation angle sine */
        float rc)		/* I - Rotation angle cosine */
{
  float rmat[3][3];		/* I - Rotation matrix */


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
zrotate(float mat[3][3],	/* I - Matrix */
        float rs,		/* I - Rotation angle sine */
        float rc)		/* I - Rotation angle cosine */
{
  float rmat[3][3];		/* I - Rotation matrix */


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
zshear(float mat[3][3],	/* I - Matrix */
       float dx,	/* I - X shear */
       float dy)	/* I - Y shear */
{
  float smat[3][3];	/* Shear matrix */


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
 * End of "$Id: image-colorspace.c,v 1.19 2000/08/29 21:23:08 mike Exp $".
 */
