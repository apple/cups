/*
 * "$Id: image-colorspace.c,v 1.4 1998/03/05 16:58:38 mike Exp $"
 *
 *   Colorspace conversions for espPrint, a collection of printer drivers.
 *
 *   Copyright 1993-1998 by Easy Software Products
 *
 *   These coded instructions, statements, and computer programs contain
 *   unpublished proprietary information of Easy Software Products, and
 *   are protected by Federal copyright law.  They may not be disclosed
 *   to third parties or copied or duplicated in any form, in whole or
 *   in part, without the prior written consent of Easy Software Products.
 *
 * Contents:
 *
 * Revision History:
 *
 *   $Log: image-colorspace.c,v $
 *   Revision 1.4  1998/03/05 16:58:38  mike
 *   Removed RGB adjustments, as it was causing color shifts to occur.
 *
 *   Revision 1.3  1998/02/24  21:45:43  mike
 *   Fixed CMY conversion - don't adjust black...
 *
 *   Revision 1.2  1998/02/24  21:05:22  mike
 *   Added color adjustments for CMY.
 *
 *   Revision 1.1  1998/02/19  15:35:50  mike
 *   Initial revision
 *
 */

/*
 * Include necessary headers...
 */

#include "image.h"
#include <math.h>


/*
 * Local functions...
 */

static void	matrixmult(float a[3][3], float b[3][3], float c[3][3]);
static void	identmat(float matrix[3][3]);
static void	saturatemat(float mat[3][3], float sat);
static void	xrotatemat(float mat[3][3], float rs, float rc);
static void	yrotatemat(float mat[3][3], float rs, float rc);
static void	zrotatemat(float mat[3][3], float rs, float rc);
static void	huerotatemat(float mat[3][3], float rot);
#define 	min(a,b)	((a) < (b) ? (a) : (b))
#define		abs(a)		((a) < 0 ? -(a) : (a))


void
ImageWhiteToBlack(ib_t *in,
                  ib_t *out,
                  int  count)
{
  while (count > 0)
  {
    *out++ = 255 - *in++;
    count --;
  };
}


void
ImageWhiteToRGB(ib_t *in,
                ib_t *out,
                int  count)
{
  while (count > 0)
  {
    *out++ = *in;
    *out++ = *in;
    *out++ = *in++;
    count --;
  };
}


void
ImageWhiteToCMY(ib_t *in,
                ib_t *out,
                int  count)
{
  while (count > 0)
  {
    *out++ = 255 - *in;
    *out++ = 255 - *in;
    *out++ = 255 - *in++;
    count --;
  };
}


void
ImageWhiteToCMYK(ib_t *in,
                 ib_t *out,
                 int  count)
{
  while (count > 0)
  {
    *out++ = 0;
    *out++ = 0;
    *out++ = 0;
    *out++ = 255 - *in++;
    count --;
  };
}



void
ImageRGBToBlack(ib_t *in,
                ib_t *out,
                int count)
{
  while (count > 0)
  {
    *out++ = 255 - (31 * in[0] + 61 * in[1] + 8 * in[2]) / 100;
    in += 3;
    count --;
  };
}


void
ImageRGBToWhite(ib_t *in,
                ib_t *out,
                int  count)
{
  while (count > 0)
  {
    *out++ = (31 * in[0] + 61 * in[1] + 8 * in[2]) / 100;
    in += 3;
    count --;
  };
}


void
ImageRGBToCMY(ib_t *in,
              ib_t *out,
              int  count)
{
#if 0 /* This can do strange things */
  int	c, m, y, k;		/* CMYK values */


  while (count > 0)
  {
    c    = 255 - in[0];
    m    = 255 - in[1];
    y    = 255 - in[2];
    k    = min(c, min(m, y));

    *out++ = (255 - (in[1] + in[2]) / 8) * (c - k) / 255 + k;
    *out++ = (255 - (in[0] + in[2]) / 8) * (m - k) / 255 + k;
    *out++ = (255 - (in[0] + in[1]) / 8) * (y - k) / 255 + k;
    in += 3;
    count --;
  };
#else
  while (count > 0)
  {
    *out++ = 255 - *in++;
    *out++ = 255 - *in++;
    *out++ = 255 - *in++;
    count --;
  };
#endif /* 0 */
}


void
ImageRGBToCMYK(ib_t *in,
               ib_t *out,
               int  count)
{
  int	c, m, y, k,		/* CMYK values */
	diff,			/* Color differences */
	divk;			/* Black divisor */


  while (count > 0)
  {
    c    = 255 - in[0];
    m    = 255 - in[1];
    y    = 255 - in[2];
    diff = 255 - (abs(c - m) + abs(c - y) + abs(m - y)) / 3;
    k    = diff * min(c, min(m, y)) / 255;
    divk = 255 - k;

    if (divk == 0)
    {
      *out++ = 0;
      *out++ = 0;
      *out++ = 0;
      *out++ = k;
    }
    else
    {
#if 0 /* This can do strange things */
      *out++ = (255 - (in[1] + in[2]) / 8) * (c - k) / divk;
      *out++ = (255 - (in[0] + in[2]) / 8) * (m - k) / divk;
      *out++ = (255 - (in[0] + in[1]) / 8) * (y - k) / divk;
#else
      *out++ = 255 * (c - k) / divk;
      *out++ = 255 * (m - k) / divk;
      *out++ = 255 * (y - k) / divk;
#endif /* 0 */
      *out++ = k;
    };

    in += 3;
    count --;
  };
}


void
ImageLut(ib_t *lut,
         ib_t *pixels,
         int  count,
         int  colorspace)
{
  switch (colorspace)
  {
    case IMAGE_BLACK :
    case IMAGE_WHITE :
        while (count > 0)
        {
          *pixels++ = lut[*pixels];
          count --;
        };
        break;
    case IMAGE_CMY :
    case IMAGE_RGB :
        while (count > 0)
        {
          *pixels++ = lut[*pixels * 3 + 0];
          *pixels++ = lut[*pixels * 3 + 1];
          *pixels++ = lut[*pixels * 3 + 2];
          count --;
        };
        break;
    case IMAGE_CMYK :
        while (count > 0)
        {
          *pixels++ = lut[*pixels * 4 + 0];
          *pixels++ = lut[*pixels * 4 + 1];
          *pixels++ = lut[*pixels * 4 + 2];
          *pixels++ = lut[*pixels * 4 + 3];
          count --;
        };
        break;
  };
}


void
ImageRGBAdjust(ib_t *pixels,
               int  count,
               int  saturation,
               int  hue)
{
  int		i, j, k;
  float		mat[3][3];
  static int	last_saturation = 100,
		last_hue = 0;
  static int	lut[3][3][256];


  if (saturation != last_saturation ||
      hue != last_hue)
  {
    identmat(mat);
    saturatemat(mat, saturation * 0.01);
    huerotatemat(mat, (float)hue);

    for (i = 0; i < 3; i ++)
      for (j = 0; j < 3; j ++)
        for (k = 0; k < 256; k ++)
          lut[i][j][k] = mat[i][j] * k + 0.5;

    last_saturation = saturation;
    last_hue        = hue;
  };

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

    j = lut[0][1][pixels[0]] +
        lut[1][1][pixels[1]] +
        lut[2][1][pixels[2]];
    if (j < 0)
      pixels[1] = 0;
    else if (j > 255)
      pixels[1] = 255;
    else
      pixels[1] = j;

    k = lut[0][2][pixels[0]] +
        lut[1][2][pixels[1]] +
        lut[2][2][pixels[2]];
    if (k < 0)
      pixels[2] = 0;
    else if (k > 255)
      pixels[2] = 255;
    else
      pixels[2] = k;

    count --;
    pixels += 3;
  };
}


/* 
 *	matrixmult -	
 *		multiply two matricies
 */
static void
matrixmult(float a[3][3],
           float b[3][3],
           float c[3][3])
{
  int x, y;
  float temp[3][3];


  for(y = 0; y < 3; y ++)
    for(x = 0; x < 3; x ++)
      temp[y][x] = b[y][0] * a[0][x] +
                   b[y][1] * a[1][x] +
                   b[y][2] * a[2][x];

  memcpy(c, temp, sizeof(temp));
}


#define RLUM    (0.3086)
#define GLUM    (0.6094)
#define BLUM    (0.0820)

/* 
 *	identmat -	
 *		make an identity matrix
 */
static void
identmat(float matrix[3][3])
{
  matrix[0][0] = 1.0;
  matrix[0][1] = 0.0;
  matrix[0][2] = 0.0;
  matrix[1][0] = 0.0;
  matrix[1][1] = 1.0;
  matrix[1][2] = 0.0;
  matrix[2][0] = 0.0;
  matrix[2][1] = 0.0;
  matrix[2][2] = 1.0;
}


/* 
 *	saturatemat -	
 *		make a saturation marix
 */
static void
saturatemat(float mat[3][3],
            float sat)
{
  float mmat[3][3];
  float a, b, c, d, e, f, g, h, i;
  float rwgt, gwgt, bwgt;


  rwgt = RLUM;
  gwgt = GLUM;
  bwgt = BLUM;

  a = (1.0-sat)*rwgt + sat;
  b = (1.0-sat)*rwgt;
  c = (1.0-sat)*rwgt;
  d = (1.0-sat)*gwgt;
  e = (1.0-sat)*gwgt + sat;
  f = (1.0-sat)*gwgt;
  g = (1.0-sat)*bwgt;
  h = (1.0-sat)*bwgt;
  i = (1.0-sat)*bwgt + sat;

  mmat[0][0] = a;
  mmat[0][1] = b;
  mmat[0][2] = c;

  mmat[1][0] = d;
  mmat[1][1] = e;
  mmat[1][2] = f;

  mmat[2][0] = g;
  mmat[2][1] = h;
  mmat[2][2] = i;

  matrixmult(mmat, mat, mat);
}

/* 
 *	xrotate -	
 *		rotate about the x (red) axis
 */
static void
xrotatemat(float mat[3][3],
           float rs,
           float rc)
{
  float mmat[3][3];


  mmat[0][0] = 1.0;
  mmat[0][1] = 0.0;
  mmat[0][2] = 0.0;

  mmat[1][0] = 0.0;
  mmat[1][1] = rc;
  mmat[1][2] = rs;

  mmat[2][0] = 0.0;
  mmat[2][1] = -rs;
  mmat[2][2] = rc;

  matrixmult(mmat,mat,mat);
}

/* 
 *	yrotate -	
 *		rotate about the y (green) axis
 */
static void
yrotatemat(float mat[3][3],
           float rs,
           float rc)
{
  float mmat[3][3];


  mmat[0][0] = rc;
  mmat[0][1] = 0.0;
  mmat[0][2] = -rs;

  mmat[1][0] = 0.0;
  mmat[1][1] = 1.0;
  mmat[1][2] = 0.0;

  mmat[2][0] = rs;
  mmat[2][1] = 0.0;
  mmat[2][2] = rc;

  matrixmult(mmat,mat,mat);
}

/* 
 *	zrotate -	
 *		rotate about the z (blue) axis
 */
static void
zrotatemat(float mat[3][3],
           float rs,
           float rc)
{
  float mmat[3][3];


  mmat[0][0] = rc;
  mmat[0][1] = rs;
  mmat[0][2] = 0.0;

  mmat[1][0] = -rs;
  mmat[1][1] = rc;
  mmat[1][2] = 0.0;

  mmat[2][0] = 0.0;
  mmat[2][1] = 0.0;
  mmat[2][2] = 1.0;

  matrixmult(mmat,mat,mat);
}

/* 
 *	xformpnt -	
 *		transform a 3D point using a matrix
 */
static void
xformpnt(float matrix[3][3],
         float x,
         float y,
         float z,
         float *tx,
         float *ty,
         float *tz)
{
  *tx = x*matrix[0][0] + y*matrix[1][0] + z*matrix[2][0];
  *ty = x*matrix[0][1] + y*matrix[1][1] + z*matrix[2][1];
  *tz = x*matrix[0][2] + y*matrix[1][2] + z*matrix[2][2];
}

/* 
 *	zshear -	
 *		shear z using x and y.
 */
static void
zshearmat(float mat[3][3],
          float dx,
          float dy)
{
  float mmat[3][3];


  mmat[0][0] = 1.0;
  mmat[0][1] = 0.0;
  mmat[0][2] = dx;

  mmat[1][0] = 0.0;
  mmat[1][1] = 1.0;
  mmat[1][2] = dy;

  mmat[2][0] = 0.0;
  mmat[2][1] = 0.0;
  mmat[2][2] = 1.0;

  matrixmult(mmat,mat,mat);
}

/* 
 *	huerotatemat -	
 *		rotate the hue, while maintaining luminance.
 */
static void
huerotatemat(float mat[3][3],
             float rot)
{
  float mmat[3][3];
  float mag;
  float lx, ly, lz;
  float xrs, xrc;
  float yrs, yrc;
  float zrs, zrc;
  float zsx, zsy;

  identmat(mmat);

/* rotate the grey vector into positive Z */
  mag = sqrt(2.0);
  xrs = 1.0/mag;
  xrc = 1.0/mag;
  xrotatemat(mmat,xrs,xrc);
  mag = sqrt(3.0);
  yrs = -1.0/mag;
  yrc = sqrt(2.0)/mag;
  yrotatemat(mmat,yrs,yrc);

/* shear the space to make the luminance plane horizontal */
  xformpnt(mmat,RLUM,GLUM,BLUM,&lx,&ly,&lz);
  zsx = lx/lz;
  zsy = ly/lz;
  zshearmat(mmat,zsx,zsy);

/* rotate the hue */
  zrs = sin(rot*M_PI/180.0);
  zrc = cos(rot*M_PI/180.0);
  zrotatemat(mmat,zrs,zrc);

/* unshear the space to put the luminance plane back */
  zshearmat(mmat,-zsx,-zsy);

/* rotate the grey vector back into place */
  yrotatemat(mmat,-yrs,yrc);
  xrotatemat(mmat,-xrs,xrc);

  matrixmult(mmat,mat,mat);
}


/*
 * End of "$Id: image-colorspace.c,v 1.4 1998/03/05 16:58:38 mike Exp $".
 */
