/*
 * "$Id$"
 *
 *   Dithering routines for CUPS.
 *
 *   Copyright 2007 by Apple Inc.
 *   Copyright 1993-2005 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
 *   cupsDitherDelete() - Free a dithering buffer.
 *   cupsDitherLine()   - Dither a line of pixels...
 *   cupsDitherNew()    - Create a dithering buffer.
 */

/*
 * Include necessary headers.
 */

#include "driver.h"


/*
 * Random number function to use, in order of preference...
 */

#ifdef HAVE_RANDOM
#  define RANDOM_FUNCTION	random
#elif defined(HAVE_MRAND48)
#  define RANDOM_FUNCTION	mrand48
#elif defined(HAVE_LRAND48)
#  define RANDOM_FUNCTION	lrand48
#else
#  define RANDOM_FUNCTION	rand
#endif /* HAVE_RANDOM */


/*
 * 'cupsDitherDelete()' - Free a dithering buffer.
 *
 * Returns 0 on success, -1 on failure.
 */

void
cupsDitherDelete(cups_dither_t *d)	/* I - Dithering buffer */
{
  if (d != NULL)
    free(d);
}


/*
 * 'cupsDitherLine()' - Dither a line of pixels...
 */

void
cupsDitherLine(cups_dither_t    *d,	/* I - Dither data */
               const cups_lut_t *lut,	/* I - Lookup table */
	       const short      *data,	/* I - Separation data */
	       int              num_channels,
					/* I - Number of components */
	       unsigned char    *p)	/* O - Pixels */
{
  register int	x,			/* Horizontal position in line... */
		pixel,			/* Current adjusted pixel... */
		e,			/* Current error */
		e0,e1,e2;		/* Error values */
  register int	errval0,		/* First half of error value */
  		errval1,		/* Second half of error value */
		errbase,		/* Base multiplier */
		errbase0,		/* Base multiplier for large values */
		errbase1,		/* Base multiplier for small values */
		errrange;		/* Range of random multiplier */
  register int	*p0,			/* Error buffer pointers... */
		*p1;
  static char	logtable[16384];	/* Error magnitude for randomness */
  static char	loginit = 0;		/* Has the table been initialized? */


  if (!loginit)
  {
   /*
    * Initialize a logarithmic table for the magnitude of randomness
    * that is introduced.
    */

    loginit = 1;

    logtable[0] = 0;
    for (x = 1; x < 2049; x ++)
      logtable[x] = (int)(log(x / 16.0) / log(2.0) + 1.0);
    for (; x < 16384; x ++)
      logtable[x] = logtable[2049];
  }

  if (d->row == 0)
  {
   /*
    * Dither from left to right:
    *
    *       e0   ==        p0[0]
    *    e1 e2   == p1[-1] p1[0]
    */

    p0 = d->errors + 2;
    p1 = d->errors + 2 + d->width + 4;
    e0 = p0[0];
    e1 = 0;
    e2 = 0;

   /*
    * Error diffuse each output pixel...
    */

    for (x = d->width;
	 x > 0;
	 x --, p0 ++, p1 ++, p ++, data += num_channels)
    {
     /*
      * Skip blank pixels...
      */

      if (*data == 0)
      {
        *p     = 0;
	e0     = p0[1];
	p1[-1] = e1;
	e1     = e2;
	e2     = 0;
	continue;
      }

     /*
      * Compute the net pixel brightness and brightness error.  Set a dot
      * if necessary...
      */

      pixel = lut[*data].intensity + e0 / 128;

      if (pixel > CUPS_MAX_LUT)
	pixel = CUPS_MAX_LUT;
      else if (pixel < 0)
	pixel = 0;

      *p = lut[pixel].pixel;
      e  = lut[pixel].error;

     /*
      * Set the randomness factor...
      */

      if (e > 0)
        errrange = logtable[e];
      else
        errrange = logtable[-e];

      errbase  = 8 - errrange;
      errrange = errrange * 2 + 1;

     /*
      * Randomize the error value.
      */

      if (errrange > 1)
      {
        errbase0 = errbase + (RANDOM_FUNCTION() % errrange);
        errbase1 = errbase + (RANDOM_FUNCTION() % errrange);
      }
      else
        errbase0 = errbase1 = errbase;

     /*
      *       X   7/16 =    X  e0
      * 3/16 5/16 1/16 =    e1 e2
      */

      errval0 = errbase0 * e;
      errval1 = (16 - errbase0) * e;
      e0      = p0[1] + 7 * errval0;
      e1      = e2 + 5 * errval1;

      errval0 = errbase1 * e;
      errval1 = (16 - errbase1) * e;
      e2      = errval0;
      p1[-1]  = e1 + 3 * errval1;
    }
  }
  else
  {
   /*
    * Dither from right to left:
    *
    *    e0      == p0[0]
    *    e2 e1   == p1[0] p1[1]
    */

    p0   = d->errors + d->width + 1 + d->width + 4;
    p1   = d->errors + d->width + 1;
    p    += d->width - 1;
    data += num_channels * (d->width - 1);
    e0   = p0[0];
    e1   = 0;
    e2   = 0;

   /*
    * Error diffuse each output pixel...
    */

    for (x = d->width;
	 x > 0;
	 x --, p0 --, p1 --, p --, data -= num_channels)
    {
     /*
      * Skip blank pixels...
      */

      if (*data == 0)
      {
        *p    = 0;
	e0    = p0[-1];
	p1[1] = e1;
	e1    = e2;
	e2    = 0;
	continue;
      }

     /*
      * Compute the net pixel brightness and brightness error.  Set a dot
      * if necessary...
      */

      pixel = lut[*data].intensity + e0 / 128;

      if (pixel > CUPS_MAX_LUT)
	pixel = CUPS_MAX_LUT;
      else if (pixel < 0)
	pixel = 0;

      *p = lut[pixel].pixel;
      e  = lut[pixel].error;

     /*
      * Set the randomness factor...
      */

      if (e > 0)
        errrange = logtable[e];
      else
        errrange = logtable[-e];

      errbase  = 8 - errrange;
      errrange = errrange * 2 + 1;

     /*
      * Randomize the error value.
      */

      if (errrange > 1)
      {
        errbase0 = errbase + (RANDOM_FUNCTION() % errrange);
        errbase1 = errbase + (RANDOM_FUNCTION() % errrange);
      }
      else
        errbase0 = errbase1 = errbase;

     /*
      *       X   7/16 =    X  e0
      * 3/16 5/16 1/16 =    e1 e2
      */

      errval0 = errbase0 * e;
      errval1 = (16 - errbase0) * e;
      e0      = p0[-1] + 7 * errval0;
      e1      = e2 + 5 * errval1;

      errval0 = errbase1 * e;
      errval1 = (16 - errbase1) * e;
      e2      = errval0;
      p1[1]   = e1 + 3 * errval1;
    }
  }

 /*
  * Update to the next row...
  */

  d->row = 1 - d->row;
}


/*
 * 'cupsDitherNew()' - Create an error-diffusion dithering buffer.
 */

cups_dither_t *			/* O - New state array */
cupsDitherNew(int width)	/* I - Width of output in pixels */
{
  cups_dither_t	*d;		/* New dithering buffer */


  if ((d = (cups_dither_t *)calloc(1, sizeof(cups_dither_t) +
                                   2 * (width + 4) *
				       sizeof(int))) == NULL)
    return (NULL);

  d->width = width;

  return (d);
}


/*
 * End of "$Id$".
 */
