/*
 * "$Id$"
 *
 *   Bit packing routines for CUPS.
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
 *   cupsPackHorizontal()    - Pack pixels horizontally...
 *   cupsPackHorizontal2()   - Pack 2-bit pixels horizontally...
 *   cupsPackHorizontalBit() - Pack pixels horizontally by bit...
 *   cupsPackVertical()      - Pack pixels vertically...
 */

/*
 * Include necessary headers...
 */

#include "driver.h"


/*
 * 'cupsPackHorizontal()' - Pack pixels horizontally...
 */

void
cupsPackHorizontal(const unsigned char *ipixels,/* I - Input pixels */
        	   unsigned char       *obytes,	/* O - Output bytes */
        	   int                 width,	/* I - Number of pixels */
        	   const unsigned char clearto,	/* I - Initial value of bytes */
		   const int           step)	/* I - Step value between pixels */
{
  register unsigned char	b;		/* Current byte */


 /*
  * Do whole bytes first...
  */

  while (width > 7)
  {
    b = clearto;

    if (*ipixels)
      b ^= 0x80;
    ipixels += step;
    if (*ipixels)
      b ^= 0x40;
    ipixels += step;
    if (*ipixels)
      b ^= 0x20;
    ipixels += step;
    if (*ipixels)
      b ^= 0x10;
    ipixels += step;
    if (*ipixels)
      b ^= 0x08;
    ipixels += step;
    if (*ipixels)
      b ^= 0x04;
    ipixels += step;
    if (*ipixels)
      b ^= 0x02;
    ipixels += step;
    if (*ipixels)
      b ^= 0x01;
    ipixels += step;

    *obytes++ = b;

    width -= 8;
  }

 /*
  * Then do the last N bytes (N < 8)...
  */

  b = clearto;

  switch (width)
  {
    case 7 :
	if (ipixels[6 * step])
	  b ^= 0x02;
    case 6 :
	if (ipixels[5 * step])
	  b ^= 0x04;
    case 5 :
	if (ipixels[4 * step])
	  b ^= 0x08;
    case 4 :
	if (ipixels[3 * step])
	  b ^= 0x10;
    case 3 :
	if (ipixels[2 * step])
	  b ^= 0x20;
    case 2 :
	if (ipixels[1 * step])
	  b ^= 0x40;
    case 1 :
	if (ipixels[0])
	  b ^= 0x80;
        *obytes = b;
        break;
  }
}


/*
 * 'cupsPackHorizontal2()' - Pack 2-bit pixels horizontally...
 */

void
cupsPackHorizontal2(const unsigned char *ipixels,	/* I - Input pixels */
        	    unsigned char       *obytes,	/* O - Output bytes */
        	    int                 width,		/* I - Number of pixels */
		    const int           step)		/* I - Stepping value */
{
  register unsigned char	b;			/* Current byte */


 /*
  * Do whole bytes first...
  */

  while (width > 3)
  {
    b = *ipixels;
    ipixels += step;
    b = (b << 2) | *ipixels;
    ipixels += step;
    b = (b << 2) | *ipixels;
    ipixels += step;
    b = (b << 2) | *ipixels;
    ipixels += step;

    *obytes++ = b;

    width -= 4;
  }

 /*
  * Then do the last N bytes (N < 4)...
  */

  b = 0;

  switch (width)
  {
    case 3 :
	b = ipixels[2 * step];
    case 2 :
	b = (b << 2) | ipixels[step];
    case 1 :
	b = (b << 2) | ipixels[0];
        *obytes = b << (8 - 2 * width);
        break;
  }
}


/*
 * 'cupsPackHorizontalBit()' - Pack pixels horizontally by bit...
 */

void
cupsPackHorizontalBit(const unsigned char *ipixels,	/* I - Input pixels */
                      unsigned char       *obytes,	/* O - Output bytes */
                      int                 width,	/* I - Number of pixels */
                      const unsigned char clearto,	/* I - Initial value of bytes */
		      const unsigned char bit)		/* I - Bit to check */
{
  register unsigned char	b;			/* Current byte */


 /*
  * Do whole bytes first...
  */

  while (width > 7)
  {
    b = clearto;

    if (*ipixels++ & bit)
      b ^= 0x80;
    if (*ipixels++ & bit)
      b ^= 0x40;
    if (*ipixels++ & bit)
      b ^= 0x20;
    if (*ipixels++ & bit)
      b ^= 0x10;
    if (*ipixels++ & bit)
      b ^= 0x08;
    if (*ipixels++ & bit)
      b ^= 0x04;
    if (*ipixels++ & bit)
      b ^= 0x02;
    if (*ipixels++ & bit)
      b ^= 0x01;

    *obytes++ = b;

    width -= 8;
  }

 /*
  * Then do the last N bytes (N < 8)...
  */

  b = clearto;

  switch (width)
  {
    case 7 :
	if (ipixels[6] & bit)
	  b ^= 0x02;
    case 6 :
	if (ipixels[5] & bit)
	  b ^= 0x04;
    case 5 :
	if (ipixels[4] & bit)
	  b ^= 0x08;
    case 4 :
	if (ipixels[3] & bit)
	  b ^= 0x10;
    case 3 :
	if (ipixels[2] & bit)
	  b ^= 0x20;
    case 2 :
	if (ipixels[1] & bit)
	  b ^= 0x40;
    case 1 :
	if (ipixels[0] & bit)
	  b ^= 0x80;
        *obytes = b;
        break;
  }
}


/*
 * 'cupsPackVertical()' - Pack pixels vertically...
 */

void
cupsPackVertical(const unsigned char *ipixels,	/* I - Input pixels */
                 unsigned char       *obytes,	/* O - Output bytes */
                 int                 width,	/* I - Number of input pixels */
                 const unsigned char bit,	/* I - Output bit */
                 const int           step)	/* I - Number of bytes between columns */
{
 /*
  * Loop through the entire array...
  */

  while (width > 7)
  {
    if (*ipixels++)
      *obytes ^= bit;
    obytes += step;
    if (*ipixels++)
      *obytes ^= bit;
    obytes += step;
    if (*ipixels++)
      *obytes ^= bit;
    obytes += step;
    if (*ipixels++)
      *obytes ^= bit;
    obytes += step;
    if (*ipixels++)
      *obytes ^= bit;
    obytes += step;
    if (*ipixels++)
      *obytes ^= bit;
    obytes += step;
    if (*ipixels++)
      *obytes ^= bit;
    obytes += step;
    if (*ipixels++)
      *obytes ^= bit;
    obytes += step;

    width -= 8;
  }

  while (width > 0)
  {
    if (*ipixels++)
      *obytes ^= bit;

    obytes += step;
    width --;
  }
}


/*
 * End of "$Id$".
 */
