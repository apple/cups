/*
 * "$Id: image-gif.c,v 1.8.2.7 2002/12/13 15:54:34 mike Exp $"
 *
 *   GIF image routines for the Common UNIX Printing System (CUPS).
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
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   ImageReadGIF()   - Read a GIF image file.
 *   gif_read_cmap()  - Read the colormap from a GIF file...
 *   gif_get_block()  - Read a GIF data block...
 *   gif_get_code()   - Get a LZW code from the file...
 *   gif_read_lzw()   - Read a byte from the LZW stream...
 *   gif_read_image() - Read a GIF image stream...
 */

/*
 * Include necessary headers...
 */

#include "image.h"


/*
 * GIF definitions...
 */

#define GIF_INTERLACE	0x40
#define GIF_COLORMAP	0x80

typedef ib_t	gif_cmap_t[256][4];


/*
 * Local globals...
 */

static int	gif_eof = 0;		/* Did we hit EOF? */


/*
 * Local functions...
 */

static int	gif_read_cmap(FILE *fp, int ncolors, gif_cmap_t cmap,
		              int *gray);
static int	gif_get_block(FILE *fp, unsigned char *buffer);
static int	gif_get_code (FILE *fp, int code_size, int first_time);
static int	gif_read_lzw(FILE *fp, int first_time, int input_code_size);
static int	gif_read_image(FILE *fp, image_t *img, gif_cmap_t cmap,
		               int interlace);


/*
 * 'ImageReadGIF()' - Read a GIF image file.
 */

int					/* O - Read status */
ImageReadGIF(image_t    *img,		/* IO - Image */
             FILE       *fp,		/* I - Image file */
             int        primary,	/* I - Primary choice for colorspace */
             int        secondary,	/* I - Secondary choice for colorspace */
             int        saturation,	/* I - Color saturation (%) */
             int        hue,		/* I - Color hue (degrees) */
	     const ib_t *lut)		/* I - Lookup table for gamma/brightness */
{
  unsigned char	buf[1024];	/* Input buffer */
  gif_cmap_t	cmap;		/* Colormap */
  int		i,		/* Looping var */
		bpp,		/* Bytes per pixel */
		gray,		/* Grayscale image? */
		ncolors,	/* Bits per pixel */
		transparent;	/* Transparent color index */


 /*
  * GIF files are either grayscale or RGB - no CMYK...
  */

  if (primary == IMAGE_RGB_CMYK)
    primary = IMAGE_RGB;

 /*
  * Read the header; we already know it is a GIF file...
  */

  fread(buf, 13, 1, fp);

  img->xsize = (buf[7] << 8) | buf[6];
  img->ysize = (buf[9] << 8) | buf[8];
  ncolors    = 2 << (buf[10] & 0x07);
  gray       = primary == IMAGE_BLACK || primary == IMAGE_WHITE;

  if (buf[10] & GIF_COLORMAP)
    if (gif_read_cmap(fp, ncolors, cmap, &gray))
    {
      fclose(fp);
      return (-1);
    }

  transparent = -1;

  for (;;)
  {
    switch (getc(fp))
    {
      case ';' :	/* End of image */
          fclose(fp);
          return (-1);		/* Early end of file */

      case '!' :	/* Extension record */
          buf[0] = getc(fp);
          if (buf[0] == 0xf9)	/* Graphic Control Extension */
          {
            gif_get_block(fp, buf);
            if (buf[0] & 1)	/* Get transparent color index */
              transparent = buf[3];
          }

          while (gif_get_block(fp, buf) != 0);
          break;

      case ',' :	/* Image data */
          fread(buf, 9, 1, fp);

          if (buf[8] & GIF_COLORMAP)
          {
            ncolors = 2 << (buf[8] & 0x07);
            gray = primary == IMAGE_BLACK || primary == IMAGE_WHITE;

	    if (gif_read_cmap(fp, ncolors, cmap, &gray))
	    {
              fclose(fp);
	      return (-1);
	    }
	  }

          if (transparent >= 0)
          {
           /*
            * Make transparent color white...
            */

            cmap[transparent][0] = 255;
            cmap[transparent][1] = 255;
            cmap[transparent][2] = 255;
          }

	  if (gray)
	  {
	    switch (secondary)
	    {
              case IMAGE_CMYK :
        	  for (i = ncolors - 1; i >= 0; i --)
        	    ImageWhiteToCMYK(cmap[i], cmap[i], 1);
        	  break;
              case IMAGE_CMY :
        	  for (i = ncolors - 1; i >= 0; i --)
        	    ImageWhiteToCMY(cmap[i], cmap[i], 1);
        	  break;
              case IMAGE_BLACK :
        	  for (i = ncolors - 1; i >= 0; i --)
        	    ImageWhiteToBlack(cmap[i], cmap[i], 1);
        	  break;
              case IMAGE_WHITE :
        	  break;
              case IMAGE_RGB :
        	  for (i = ncolors - 1; i >= 0; i --)
        	    ImageWhiteToRGB(cmap[i], cmap[i], 1);
        	  break;
	    }

            img->colorspace = secondary;
	  }
	  else
	  {
	    if (hue != 0 || saturation != 100)
              for (i = ncolors - 1; i >= 0; i --)
        	ImageRGBAdjust(cmap[i], 1, saturation, hue);

	    switch (primary)
	    {
              case IMAGE_CMYK :
        	  for (i = ncolors - 1; i >= 0; i --)
        	    ImageRGBToCMYK(cmap[i], cmap[i], 1);
        	  break;
              case IMAGE_CMY :
        	  for (i = ncolors - 1; i >= 0; i --)
        	    ImageRGBToCMY(cmap[i], cmap[i], 1);
        	  break;
              case IMAGE_BLACK :
        	  for (i = ncolors - 1; i >= 0; i --)
        	    ImageRGBToBlack(cmap[i], cmap[i], 1);
        	  break;
              case IMAGE_WHITE :
        	  for (i = ncolors - 1; i >= 0; i --)
        	    ImageRGBToWhite(cmap[i], cmap[i], 1);
        	  break;
              case IMAGE_RGB :
        	  break;
	    }

            img->colorspace = primary;
	  }

          if (lut)
	  {
	    bpp = ImageGetDepth(img);

            for (i = ncolors - 1; i >= 0; i --)
              ImageLut(cmap[i], bpp, lut);
	  }

          img->xsize = (buf[5] << 8) | buf[4];
          img->ysize = (buf[7] << 8) | buf[6];

         /*
	  * Check the dimensions of the image; since the dimensions are
	  * a 16-bit integer we just need to check for 0...
	  */

          if (img->xsize == 0 || img->ysize == 0)
	  {
	    fprintf(stderr, "ERROR: Bad GIF image dimensions: %dx%d\n",
	            img->xsize, img->ysize);
	    fclose(fp);
	    return (1);
	  }

	  i = gif_read_image(fp, img, cmap, buf[8] & GIF_INTERLACE);
          fclose(fp);
          return (i);
    }
  }
}


/*
 * 'gif_read_cmap()' - Read the colormap from a GIF file...
 */

static int				/* O - -1 on error, 0 on success */
gif_read_cmap(FILE       *fp,		/* I - File to read from */
  	      int        ncolors,	/* I - Number of colors in file */
	      gif_cmap_t cmap,		/* O - Colormap information */
	      int        *gray)		/* IO - Is the image grayscale? */
{
  int	i;				/* Looping var */


 /*
  * Read the colormap...
  */

  for (i = 0; i < ncolors; i ++)
    if (fread(cmap[i], 3, 1, fp) < 1)
      return (-1);

 /*
  * Check to see if the colormap is a grayscale ramp...
  */

  for (i = 0; i < ncolors; i ++)
    if (cmap[i][0] != cmap[i][1] || cmap[i][1] != cmap[i][2])
      break;

  if (i == ncolors)
  {
    *gray = 1;
    return (0);
  }

 /*
  * If this needs to be a grayscale image, convert the RGB values to
  * luminance values...
  */

  if (*gray)
    for (i = 0; i < ncolors; i ++)
      cmap[i][0] = (cmap[i][0] * 31 + cmap[i][1] * 61 + cmap[i][2] * 8) / 100;

  return (0);
}


/*
 * 'gif_get_block()' - Read a GIF data block...
 */

static int				/* O - Number characters read */
gif_get_block(FILE          *fp,	/* I - File to read from */
	      unsigned char *buf)	/* I - Input buffer */
{
  int	count;				/* Number of character to read */


 /*
  * Read the count byte followed by the data from the file...
  */

  if ((count = getc(fp)) == EOF)
  {
    gif_eof = 1;
    return (-1);
  }
  else if (count == 0)
    gif_eof = 1;
  else if (fread(buf, 1, count, fp) < count)
  {
    gif_eof = 1;
    return (-1);
  }
  else
    gif_eof = 0;

  return (count);
}


/*
 * 'gif_get_code()' - Get a LZW code from the file...
 */

static int			/* O - LZW code */
gif_get_code(FILE *fp,		/* I - File to read from */
	     int  code_size,	/* I - Size of code in bits */
	     int  first_time)	/* I - 1 = first time, 0 = not first time */
{
  unsigned		i, j,		/* Looping vars */
			ret;		/* Return value */
  int			count;		/* Number of bytes read */
  static unsigned char	buf[280];	/* Input buffer */
  static unsigned	curbit,		/* Current bit */
			lastbit,	/* Last bit in buffer */
			done,		/* Done with this buffer? */
			last_byte;	/* Last byte in buffer */
  static unsigned char	bits[8] =	/* Bit masks for codes */
			{
			  0x01, 0x02, 0x04, 0x08,
			  0x10, 0x20, 0x40, 0x80
			};


  if (first_time)
  {
   /*
    * Just initialize the input buffer...
    */

    curbit  = 0;
    lastbit = 0;
    done    = 0;

    return (0);
  }


  if ((curbit + code_size) >= lastbit)
  {
   /*
    * Don't have enough bits to hold the code...
    */

    if (done)
      return (-1);	/* Sorry, no more... */

   /*
    * Move last two bytes to front of buffer...
    */

    if (last_byte > 1)
    {
      buf[0]    = buf[last_byte - 2];
      buf[1]    = buf[last_byte - 1];
      last_byte = 2;
    }
    else if (last_byte == 1)
    {
      buf[0]    = buf[last_byte - 1];
      last_byte = 1;
    }

   /*
    * Read in another buffer...
    */

    if ((count = gif_get_block (fp, buf + last_byte)) <= 0)
    {
     /*
      * Whoops, no more data!
      */

      done = 1;
      return (-1);
    }

   /*
    * Update buffer state...
    */

    curbit    = (curbit - lastbit) + 8 * last_byte;
    last_byte += count;
    lastbit   = last_byte * 8;
  }

  ret = 0;
  for (ret = 0, i = curbit + code_size - 1, j = code_size;
       j > 0;
       i --, j --)
    ret = (ret << 1) | ((buf[i / 8] & bits[i & 7]) != 0);

  curbit += code_size;

  return ret;
}


/*
 * 'gif_read_lzw()' - Read a byte from the LZW stream...
 */

static int				/* I - Byte from stream */
gif_read_lzw(FILE *fp,			/* I - File to read from */
	     int  first_time,		/* I - 1 = first time, 0 = not first time */
 	     int  input_code_size)	/* I - Code size in bits */
{
  int		i,			/* Looping var */
		code,			/* Current code */
		incode;			/* Input code */
  static short	fresh = 0,		/* 1 = empty buffers */
		code_size,		/* Current code size */
		set_code_size,		/* Initial code size set */
		max_code,		/* Maximum code used */
		max_code_size,		/* Maximum code size */
		firstcode,		/* First code read */
		oldcode,		/* Last code read */
		clear_code,		/* Clear code for LZW input */
		end_code,		/* End code for LZW input */
		table[2][4096],		/* String table */
		stack[8192],		/* Output stack */
		*sp;			/* Current stack pointer */


  if (first_time)
  {
   /*
    * Setup LZW state...
    */

    set_code_size = input_code_size;
    code_size     = set_code_size + 1;
    clear_code    = 1 << set_code_size;
    end_code      = clear_code + 1;
    max_code_size = 2 * clear_code;
    max_code      = clear_code + 2;

   /*
    * Initialize input buffers...
    */

    gif_get_code(fp, 0, 1);

   /*
    * Wipe the decompressor table...
    */

    fresh = 1;

    for (i = 0; i < clear_code; i ++)
    {
      table[0][i] = 0;
      table[1][i] = i;
    }

    for (; i < 4096; i ++)
      table[0][i] = table[1][0] = 0;

    sp = stack;

    return (0);
  }
  else if (fresh)
  {
    fresh = 0;

    do
      firstcode = oldcode = gif_get_code(fp, code_size, 0);
    while (firstcode == clear_code);

    return (firstcode);
  }

  if (sp > stack)
    return (*--sp);

  while ((code = gif_get_code (fp, code_size, 0)) >= 0)
  {
    if (code == clear_code)
    {
      for (i = 0; i < clear_code; i ++)
      {
	table[0][i] = 0;
	table[1][i] = i;
      }

      for (; i < 4096; i ++)
	table[0][i] = table[1][i] = 0;

      code_size     = set_code_size + 1;
      max_code_size = 2 * clear_code;
      max_code      = clear_code + 2;

      sp = stack;

      firstcode = oldcode = gif_get_code(fp, code_size, 0);

      return (firstcode);
    }
    else if (code == end_code)
    {
      unsigned char	buf[260];


      if (!gif_eof)
        while (gif_get_block(fp, buf) > 0);

      return (-2);
    }

    incode = code;

    if (code >= max_code)
    {
      *sp++ = firstcode;
      code  = oldcode;
    }

    while (code >= clear_code)
    {
      *sp++ = table[1][code];
      if (code == table[0][code])
	return (255);

      code = table[0][code];
    }

    *sp++ = firstcode = table[1][code];
    code  = max_code;

    if (code < 4096)
    {
      table[0][code] = oldcode;
      table[1][code] = firstcode;
      max_code ++;

      if (max_code >= max_code_size && max_code_size < 4096)
      {
	max_code_size *= 2;
	code_size ++;
      }
    }

    oldcode = incode;

    if (sp > stack)
      return (*--sp);
  }

  return (code);
}


/*
 * 'gif_read_image()' - Read a GIF image stream...
 */

static int				/* I - 0 = success, -1 = failure */
gif_read_image(FILE       *fp,		/* I - Input file */
	       image_t    *img,		/* I - Image pointer */
	       gif_cmap_t cmap,		/* I - Colormap */
	       int        interlace)	/* I - Non-zero = interlaced image */
{
  unsigned char	code_size;		/* Code size */
  ib_t		*pixels,		/* Pixel buffer */
		*temp;			/* Current pixel */
  int		xpos,			/* Current X position */
		ypos,			/* Current Y position */
		pass;			/* Current pass */
  int		pixel;			/* Current pixel */
  int		bpp;			/* Bytes per pixel */
  static int	xpasses[4] = { 8, 8, 4, 2 },
		ypasses[5] = { 0, 4, 2, 1, 999999 };


  bpp       = ImageGetDepth(img);
  pixels    = calloc(bpp, img->xsize);
  xpos      = 0;
  ypos      = 0;
  pass      = 0;
  code_size = getc(fp);

  if (gif_read_lzw(fp, 1, code_size) < 0)
    return (-1);

  temp = pixels;
  while ((pixel = gif_read_lzw(fp, 0, code_size)) >= 0)
  {
    switch (bpp)
    {
      case 4 :
          temp[3] = cmap[pixel][3];
      case 3 :
          temp[2] = cmap[pixel][2];
      case 2 :
          temp[1] = cmap[pixel][1];
      default :
          temp[0] = cmap[pixel][0];
    }

    xpos ++;
    temp += bpp;
    if (xpos == img->xsize)
    {
      ImagePutRow(img, 0, ypos, img->xsize, pixels);

      xpos = 0;
      temp = pixels;

      if (interlace)
      {
        ypos += xpasses[pass];

        if (ypos >= img->ysize)
	{
	  pass ++;

          ypos = ypasses[pass];
	}
      }
      else
	ypos ++;
    }

    if (ypos >= img->ysize)
      break;
  }

  free(pixels);

  return (0);
}


/*
 * End of "$Id: image-gif.c,v 1.8.2.7 2002/12/13 15:54:34 mike Exp $".
 */
