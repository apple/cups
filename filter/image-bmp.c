/*
 * "$Id: image-bmp.c 7221 2008-01-16 22:20:08Z mike $"
 *
 *   BMP image routines for CUPS.
 *
 *   Copyright 2007-2011 by Apple Inc.
 *   Copyright 1993-2007 by Easy Software Products.
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
 *   _cupsImageReadBMP() - Read a BMP image file.
 *   read_word()         - Read a 16-bit unsigned integer.
 *   read_dword()        - Read a 32-bit unsigned integer.
 *   read_long()         - Read a 32-bit signed integer.
 */

/*
 * Include necessary headers...
 */

#include "image-private.h"


/*
 * Constants for the bitmap compression...
 */

#  define BI_RGB	0		/* No compression - straight BGR data */
#  define BI_RLE8	1		/* 8-bit run-length compression */
#  define BI_RLE4	2		/* 4-bit run-length compression */
#  define BI_BITFIELDS	3		/* RGB bitmap with RGB masks */


/*
 * Local functions...
 */

static unsigned short	read_word(FILE *fp);
static unsigned int	read_dword(FILE *fp);
static int		read_long(FILE *fp);


/*
 * '_cupsImageReadBMP()' - Read a BMP image file.
 */

int					/* O - Read status */
_cupsImageReadBMP(
    cups_image_t    *img,		/* IO - cupsImage */
    FILE            *fp,		/* I - cupsImage file */
    cups_icspace_t  primary,		/* I - Primary choice for colorspace */
    cups_icspace_t  secondary,		/* I - Secondary choice for colorspace */
    int             saturation,		/* I - Color saturation (%) */
    int             hue,		/* I - Color hue (degrees) */
    const cups_ib_t *lut)		/* I - Lookup table for gamma/brightness */
{
  int		offset,			/* Offset to bitmap data */
		info_size,		/* Size of info header */
		planes,			/* Number of planes (always 1) */
		depth,			/* Depth of image (bits) */
		compression,		/* Type of compression */
		image_size,		/* Size of image in bytes */
		colors_used,		/* Number of colors used */
		colors_important,	/* Number of important colors */
		bpp,			/* Bytes per pixel */
		x, y,			/* Looping vars */
		color,			/* Color of RLE pixel */
		count,			/* Number of times to repeat */
		temp,			/* Temporary color */
		align;			/* Alignment bytes */
  cups_ib_t	bit,			/* Bit in image */
		byte;			/* Byte in image */
  cups_ib_t	*in,			/* Input pixels */
		*out,			/* Output pixels */
		*ptr;			/* Pointer into pixels */
  cups_ib_t	colormap[256][4];	/* Colormap */


  (void)secondary;

 /*
  * Get the header...
  */

  getc(fp);		/* Skip "BM" sync chars */
  getc(fp);
  read_dword(fp);	/* Skip size */
  read_word(fp);	/* Skip reserved stuff */
  read_word(fp);
  offset = read_dword(fp);

  fprintf(stderr, "DEBUG: offset = %d\n", offset);

  if (offset < 0)
  {
    fprintf(stderr, "DEBUG: Bad BMP offset %d\n", offset);
    fclose(fp);
    return (1);
  }

 /*
  * Then the bitmap information...
  */

  info_size        = read_dword(fp);
  img->xsize       = read_long(fp);
  img->ysize       = read_long(fp);
  planes           = read_word(fp);
  depth            = read_word(fp);
  compression      = read_dword(fp);
  image_size       = read_dword(fp);
  img->xppi        = read_long(fp) * 0.0254 + 0.5;
  img->yppi        = read_long(fp) * 0.0254 + 0.5;
  colors_used      = read_dword(fp);
  colors_important = read_dword(fp);

  if (img->xsize == 0 || img->xsize > CUPS_IMAGE_MAX_WIDTH ||
      img->ysize == 0 || img->ysize > CUPS_IMAGE_MAX_HEIGHT ||
      (depth != 1 && depth != 4 && depth != 8 && depth != 24))
  {
    fprintf(stderr, "DEBUG: Bad BMP dimensions %ux%ux%d\n",
            img->xsize, img->ysize, depth);
    fclose(fp);
    return (1);
  }

  if (colors_used < 0 || colors_used > 256)
  {
    fprintf(stderr, "DEBUG: Bad BMP colormap size %d\n", colors_used);
    fclose(fp);
    return (1);
  }

  if (img->xppi == 0 || img->yppi == 0)
  {
    fprintf(stderr, "DEBUG: Bad BMP resolution %dx%d PPI.\n",
            img->xppi, img->yppi);
    img->xppi = img->yppi = 128;
  }

 /*
  * Make sure the resolution info is valid...
  */

  fprintf(stderr, "info_size = %d, xsize = %d, ysize = %d, planes = %d, depth = %d\n",
          info_size, img->xsize, img->ysize, planes, depth);
  fprintf(stderr, "compression = %d, image_size = %d, xppi = %d, yppi = %d\n",
          compression, image_size, img->xppi, img->yppi);
  fprintf(stderr, "colors_used = %d, colors_important = %d\n", colors_used,
          colors_important);

  if (info_size > 40)
    for (info_size -= 40; info_size > 0; info_size --)
      getc(fp);

 /*
  * Get colormap...
  */

  if (colors_used == 0 && depth <= 8)
    colors_used = 1 << depth;

  if (colors_used > 0)
    fread(colormap, colors_used, 4, fp);
  else
    memset(colormap, 0, sizeof(colormap));

 /*
  * Setup image and buffers...
  */

  img->colorspace = (primary == CUPS_IMAGE_RGB_CMYK) ? CUPS_IMAGE_RGB : primary;

  cupsImageSetMaxTiles(img, 0);

  bpp = cupsImageGetDepth(img);

  if ((in = malloc(img->xsize * 3)) == NULL)
  {
    fputs("DEBUG: Unable to allocate memory!\n", stderr);
    fclose(fp);
    return (1);
  }

  if ((out = malloc(img->xsize * bpp)) == NULL)
  {
    fputs("DEBUG: Unable to allocate memory!\n", stderr);
    free(in);
    fclose(fp);
    return (1);
  }

 /*
  * Read the image data...
  */

  color = 0;
  count = 0;
  align = 0;

  for (y = img->ysize - 1; y >= 0; y --)
  {
    ptr = in;

    switch (depth)
    {
      case 1 : /* Bitmap */
          for (x = img->xsize, bit = 128, byte = 0; x > 0; x --)
	  {
	    if (bit == 128)
	      byte = getc(fp);

	    if (byte & bit)
	    {
	      *ptr++ = colormap[1][2];
	      *ptr++ = colormap[1][1];
	      *ptr++ = colormap[1][0];
	    }
	    else
	    {
	      *ptr++ = colormap[0][2];
	      *ptr++ = colormap[0][1];
	      *ptr++ = colormap[0][0];
	    }

	    if (bit > 1)
	      bit >>= 1;
	    else
	      bit = 128;
	  }

         /*
	  * Read remaining bytes to align to 32 bits...
	  */

	  for (temp = (img->xsize + 7) / 8; temp & 3; temp ++)
	    getc(fp);
          break;

      case 4 : /* 16-color */
          for (x = img->xsize, bit = 0xf0, temp = 0; x > 0; x --)
	  {
	   /*
	    * Get a new count as needed...
	    */

            if (compression != BI_RLE4 && count == 0)
	    {
	      count = 2;
	      color = -1;
            }

	    if (count == 0)
	    {
	      while (align > 0)
	      {
	        align --;
		getc(fp);
              }

	      if ((count = getc(fp)) == 0)
	      {
		if ((count = getc(fp)) == 0)
		{
		 /*
		  * End of line...
		  */

                  x ++;
		  continue;
		}
		else if (count == 1)
		{
		 /*
		  * End of image...
		  */

		  break;
		}
		else if (count == 2)
		{
		 /*
		  * Delta...
		  */

		  count = getc(fp) * getc(fp) * img->xsize;
		  color = 0;
		}
		else
		{
		 /*
		  * Absolute...
		  */

		  color = -1;
		  align = ((4 - (count & 3)) / 2) & 1;
		}
	      }
	      else
	        color = getc(fp);
            }

           /*
	    * Get a new color as needed...
	    */

	    count --;

            if (bit == 0xf0)
	    {
              if (color < 0)
		temp = getc(fp);
	      else
		temp = color;

             /*
	      * Copy the color value...
	      */

	      *ptr++ = colormap[temp >> 4][2];
	      *ptr++ = colormap[temp >> 4][1];
	      *ptr++ = colormap[temp >> 4][0];
	      bit    = 0x0f;
            }
	    else
	    {
             /*
	      * Copy the color value...
	      */

	      *ptr++ = colormap[temp & 15][2];
	      *ptr++ = colormap[temp & 15][1];
	      *ptr++ = colormap[temp & 15][0];
	      bit    = 0xf0;
	    }
	  }
          break;

      case 8 : /* 256-color */
          for (x = img->xsize; x > 0; x --)
	  {
	   /*
	    * Get a new count as needed...
	    */

            if (compression != BI_RLE8)
	    {
	      count = 1;
	      color = -1;
            }

	    if (count == 0)
	    {
	      while (align > 0)
	      {
	        align --;
		getc(fp);
              }

	      if ((count = getc(fp)) == 0)
	      {
		if ((count = getc(fp)) == 0)
		{
		 /*
		  * End of line...
		  */

                  x ++;
		  continue;
		}
		else if (count == 1)
		{
		 /*
		  * End of image...
		  */

		  break;
		}
		else if (count == 2)
		{
		 /*
		  * Delta...
		  */

		  count = getc(fp) * getc(fp) * img->xsize;
		  color = 0;
		}
		else
		{
		 /*
		  * Absolute...
		  */

		  color = -1;
		  align = (2 - (count & 1)) & 1;
		}
	      }
	      else
	        color = getc(fp);
            }

           /*
	    * Get a new color as needed...
	    */

            if (color < 0)
	      temp = getc(fp);
	    else
	      temp = color;

            count --;

           /*
	    * Copy the color value...
	    */

	    *ptr++ = colormap[temp][2];
	    *ptr++ = colormap[temp][1];
	    *ptr++ = colormap[temp][0];
	  }
          break;

      case 24 : /* 24-bit RGB */
          for (x = img->xsize; x > 0; x --, ptr += 3)
	  {
	    ptr[2] = getc(fp);
	    ptr[1] = getc(fp);
	    ptr[0] = getc(fp);
	  }

         /*
	  * Read remaining bytes to align to 32 bits...
	  */

	  for (temp = img->xsize * 3; temp & 3; temp ++)
	    getc(fp);
          break;
    }

    if (saturation != 100 || hue != 0)
      cupsImageRGBAdjust(in, img->xsize, saturation, hue);

    switch (img->colorspace)
    {
      default :
	  break;

      case CUPS_IMAGE_WHITE :
	  cupsImageRGBToWhite(in, out, img->xsize);
	  break;

      case CUPS_IMAGE_RGB :
	  cupsImageRGBToRGB(in, out, img->xsize);
	  break;

      case CUPS_IMAGE_BLACK :
	  cupsImageRGBToBlack(in, out, img->xsize);
	  break;

      case CUPS_IMAGE_CMY :
	  cupsImageRGBToCMY(in, out, img->xsize);
	  break;

      case CUPS_IMAGE_CMYK :
	  cupsImageRGBToCMYK(in, out, img->xsize);
	  break;
    }

    if (lut)
      cupsImageLut(out, img->xsize * bpp, lut);

    _cupsImagePutRow(img, 0, y, img->xsize, out);
  }

  fclose(fp);
  free(in);
  free(out);

  return (0);
}


/*
 * 'read_word()' - Read a 16-bit unsigned integer.
 */

static unsigned short     /* O - 16-bit unsigned integer */
read_word(FILE *fp)       /* I - File to read from */
{
  unsigned char b0, b1; /* Bytes from file */

  b0 = getc(fp);
  b1 = getc(fp);

  return ((b1 << 8) | b0);
}


/*
 * 'read_dword()' - Read a 32-bit unsigned integer.
 */

static unsigned int               /* O - 32-bit unsigned integer */
read_dword(FILE *fp)              /* I - File to read from */
{
  unsigned char b0, b1, b2, b3; /* Bytes from file */

  b0 = getc(fp);
  b1 = getc(fp);
  b2 = getc(fp);
  b3 = getc(fp);

  return ((((((b3 << 8) | b2) << 8) | b1) << 8) | b0);
}


/*
 * 'read_long()' - Read a 32-bit signed integer.
 */

static int                        /* O - 32-bit signed integer */
read_long(FILE *fp)               /* I - File to read from */
{
  unsigned char b0, b1, b2, b3; /* Bytes from file */

  b0 = getc(fp);
  b1 = getc(fp);
  b2 = getc(fp);
  b3 = getc(fp);

  return ((int)(((((b3 << 8) | b2) << 8) | b1) << 8) | b0);
}


/*
 * End of "$Id: image-bmp.c 7221 2008-01-16 22:20:08Z mike $".
 */
