/*
 * "$Id: image-bmp.c,v 1.3.2.2 2002/01/02 18:04:44 mike Exp $"
 *
 *   BMP image routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1993-2002 by Easy Software Products.
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
 *   ImageReadBMP() - Read a BMP image file.
 *   read_word()    - Read a 16-bit unsigned integer.
 *   read_dword()   - Read a 32-bit unsigned integer.
 *   read_long()    - Read a 32-bit signed integer.
 */

/*
 * Include necessary headers...
 */

#include "image.h"


/*
 * Constants for the bitmap compression...
 */

#  define BI_RGB       0             /* No compression - straight BGR data */
#  define BI_RLE8      1             /* 8-bit run-length compression */
#  define BI_RLE4      2             /* 4-bit run-length compression */
#  define BI_BITFIELDS 3             /* RGB bitmap with RGB masks */


/*
 * Local functions...
 */

static unsigned short read_word(FILE *fp);
static unsigned int   read_dword(FILE *fp);
static int            read_long(FILE *fp);


/*
 * 'ImageReadBMP()' - Read a BMP image file.
 */

int					/* O - Read status */
ImageReadBMP(image_t    *img,		/* IO - Image */
             FILE       *fp,		/* I - Image file */
             int        primary,	/* I - Primary choice for colorspace */
             int        secondary,	/* I - Secondary choice for colorspace */
             int        saturation,	/* I - Color saturation (%) */
             int        hue,		/* I - Color hue (degrees) */
	     const ib_t *lut)		/* I - Lookup table for gamma/brightness */
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
  ib_t		bit,			/* Bit in image */
		byte;			/* Byte in image */
  ib_t		*in,			/* Input pixels */
		*out,			/* Output pixels */
		*ptr;			/* Pointer into pixels */
  ib_t		colormap[256][4];	/* Colormap */


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

  fprintf(stderr, "offset = %d\n", offset);

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

 /*
  * Make sure the resolution info is valid...
  */

  if (img->xppi == 0)
    img->xppi = 128;
  if (img->yppi == 0)
    img->yppi = 128;

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

  fread(colormap, colors_used, 4, fp);

 /*
  * Setup image and buffers...
  */

  img->colorspace = primary;

  ImageSetMaxTiles(img, 0);

  in  = malloc(img->xsize * 3);
  bpp = ImageGetDepth(img);
  out = malloc(img->xsize * bpp);

 /*
  * Read the image data...
  */

  color = 0;
  count = 0;
  align = 0;

  for (y = img->ysize - 1; y >= 0; y --)
  {
    if (img->colorspace == IMAGE_RGB)
      ptr = out;
    else
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

    if (img->colorspace == IMAGE_RGB)
    {
      if (saturation != 100 || hue != 0)
	ImageRGBAdjust(out, img->xsize, saturation, hue);
    }
    else
    {
      if (saturation != 100 || hue != 0)
	ImageRGBAdjust(in, img->xsize, saturation, hue);

      switch (img->colorspace)
      {
	case IMAGE_WHITE :
	    ImageRGBToWhite(in, out, img->xsize);
	    break;
	case IMAGE_BLACK :
	    ImageRGBToBlack(in, out, img->xsize);
	    break;
	case IMAGE_CMY :
	    ImageRGBToCMY(in, out, img->xsize);
	    break;
	case IMAGE_CMYK :
	    ImageRGBToCMYK(in, out, img->xsize);
	    break;
      }
    }

    if (lut)
      ImageLut(out, img->xsize * bpp, lut);

    ImagePutRow(img, 0, y, img->xsize, out);
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
 * End of "$Id: image-bmp.c,v 1.3.2.2 2002/01/02 18:04:44 mike Exp $".
 */
