/*
 * "$Id: image-tiff.c,v 1.6 1999/03/24 18:01:45 mike Exp $"
 *
 *   TIFF file routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1993-1999 by Easy Software Products.
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
 */

/*
 * Include necessary headers...
 */

#include "image.h"

#ifdef HAVE_LIBTIFF
#  include <tiff.h>	/* TIFF image definitions */
#  include <tiffio.h>


int
ImageReadTIFF(image_t *img,
              FILE    *fp,
              int     primary,
              int     secondary,
              int     saturation,
              int     hue)
{
  TIFF		*tif;
  uint32	width, height;
  uint16	photometric,
		orientation,
		resunit,
		samples,
		bits,
		inkset;
  float		xres,
		yres;
  uint16	*redcmap,
		*greencmap,
		*bluecmap;
  int		c,
		num_colors,
		bpp,
		x, y,
		xstart, ystart,
		xdir, ydir,
		xcount, ycount,
		pstep,
		scanwidth,
		r, g, b, k,
		alpha;
  ib_t		*in,
		*out,
		*p,
		*scanline,
		*scanptr,
		bit,
		pixel,
		zero,
		one;


#ifdef __hpux
  lseek(fileno(fp), 0, SEEK_SET); /* Work around "feature" in HPUX stdio */
#endif /* hpux */

  if ((tif = TIFFFdOpen(fileno(fp), "", "r")) == NULL)
  {
    fclose(fp);
    return (-1);
  }

  if (!TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width) ||
      !TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height) ||
      !TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, &photometric) ||
      !TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &samples) ||
      !TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &bits))
  {
    fclose(fp);
    return (-1);
  }

  if (!TIFFGetField(tif, TIFFTAG_ORIENTATION, &orientation))
    orientation = 0;

  if (TIFFGetField(tif, TIFFTAG_XRESOLUTION, &xres) &&
      TIFFGetField(tif, TIFFTAG_YRESOLUTION, &yres) &&
      TIFFGetField(tif, TIFFTAG_RESOLUTIONUNIT, &resunit))
  {
    if (resunit == RESUNIT_INCH)
    {
      img->xppi = xres;
      img->yppi = yres;
    }
    else if (resunit == RESUNIT_CENTIMETER)
    {
      img->xppi = xres * 2.54;
      img->yppi = yres * 2.54;
    }
    else
    {
      img->xppi = xres * 0.0254;
      img->yppi = yres * 0.0254;
    }
  }

  if (samples == 2 || (samples == 4 && photometric == PHOTOMETRIC_RGB))
    alpha = 1;
  else
    alpha = 0;

  img->xsize = width;
  img->ysize = height;
  if (photometric == PHOTOMETRIC_MINISBLACK ||
      photometric == PHOTOMETRIC_MINISWHITE)
    img->colorspace = secondary;
  else
    img->colorspace = primary;

  bpp = ImageGetDepth(img);

  ImageSetMaxTiles(img, 0);

  switch (orientation)
  {
    case ORIENTATION_TOPRIGHT :
    case ORIENTATION_RIGHTTOP :
        xstart = img->xsize - 1;
        xdir   = -1;
        ystart = 0;
        ydir   = 1;
        break;
    default :
    case ORIENTATION_TOPLEFT :
    case ORIENTATION_LEFTTOP :
        xstart = 0;
        xdir   = 1;
        ystart = 0;
        ydir   = 1;
        break;
    case ORIENTATION_BOTLEFT :
    case ORIENTATION_LEFTBOT :
        xstart = 0;
        xdir   = 1;
        ystart = img->ysize - 1;
        ydir   = -1;
        break;
    case ORIENTATION_BOTRIGHT :
    case ORIENTATION_RIGHTBOT :
        xstart = img->xsize - 1;
        xdir   = -1;
        ystart = img->ysize - 1;
        ydir   = -1;
        break;
  }

  scanwidth = TIFFScanlineSize(tif);
  scanline  = _TIFFmalloc(scanwidth);

  if (orientation < ORIENTATION_LEFTTOP)
  {
    if (samples > 1 || photometric == PHOTOMETRIC_PALETTE)
      pstep = xdir * 3;
    else
      pstep = xdir;

    in  = malloc(img->xsize * 3 + 3);
    out = malloc(img->xsize * bpp);
  }
  else
  {
    if (samples > 1 || photometric == PHOTOMETRIC_PALETTE)
      pstep = ydir * 3;
    else
      pstep = ydir;

    in  = malloc(img->ysize * 3 + 3);
    out = malloc(img->ysize * bpp);
  }

  switch (photometric)
  {
    case PHOTOMETRIC_MINISWHITE :
    case PHOTOMETRIC_MINISBLACK :
        if (photometric == PHOTOMETRIC_MINISBLACK)
        {
          zero = 0;
          one  = 255;
        }
        else
        {
          zero = 255;
          one  = 0;
        }

        if (orientation < ORIENTATION_LEFTTOP)
        {
         /*
          * Row major order...
          */

          for (y = ystart, ycount = img->ysize;
               ycount > 0;
               ycount --, y += ydir)
          {
            if (bits == 1)
            {
              TIFFReadScanline(tif, scanline, y, 0);
              for (xcount = img->xsize, scanptr = scanline, p = in + xstart, bit = 128;
                   xcount > 0;
                   xcount --, p += pstep)
              {
        	if (*scanptr & bit)
                  *p = one;
                else
                  *p = zero;

        	if (bit > 1)
                  bit >>= 1;
        	else
        	{
                  bit = 128;
                  scanptr ++;
        	}
              }
            }
            else if (bits == 2)
            {
              TIFFReadScanline(tif, scanline, y, 0);
              for (xcount = img->xsize, scanptr = scanline, p = in + xstart, bit = 0xc0;
                   xcount > 0;
                   xcount --, p += pstep)
              {
                pixel = *scanptr & bit;
                while (pixel > 3)
                  pixel >>= 2;
                *p = (255 * pixel / 3) ^ zero;

        	if (bit > 3)
                  bit >>= 2;
        	else
        	{
                  bit = 0xc0;
                  scanptr ++;
        	}
              }
            }
            else if (bits == 4)
            {
              TIFFReadScanline(tif, scanline, y, 0);
              for (xcount = img->xsize, scanptr = scanline, p = in + xstart, bit = 0xf0;
                   xcount > 0;
                   xcount --, p += pstep)
              {
                if (bit == 0xf0)
                {
                  *p = (255 * ((*scanptr & 0xf0) >> 4) / 15) ^ zero;
                  bit = 0x0f;
                }
                else
        	{
                  *p = (255 * (*scanptr & 0x0f) / 15) ^ zero;
                  bit = 0xf0;
                  scanptr ++;
        	}
              }
            }
            else if (xdir < 0 || zero || alpha)
            {
              TIFFReadScanline(tif, scanline, y, 0);

              if (alpha)
	      {
        	if (zero)
        	{
                  for (xcount = img->xsize, p = in + xstart, scanptr = scanline;
                       xcount > 0;
                       xcount --, p += pstep, scanptr += 2)
                    *p = (scanptr[1] * (255 - scanptr[0]) +
		          (255 - scanptr[1]) * 255) / 255;
        	}
        	else
        	{
                  for (xcount = img->xsize, p = in + xstart, scanptr = scanline;
                       xcount > 0;
                       xcount --, p += pstep, scanptr += 2)
                    *p = (scanptr[1] * scanptr[0] +
		          (255 - scanptr[1]) * 255) / 255;
        	}
	      }
	      else
	      {
        	if (zero)
        	{
                  for (xcount = img->xsize, p = in + xstart, scanptr = scanline;
                       xcount > 0;
                       xcount --, p += pstep, scanptr ++)
                    *p = 255 - *scanptr;
        	}
        	else
        	{
                  for (xcount = img->xsize, p = in + xstart, scanptr = scanline;
                       xcount > 0;
                       xcount --, p += pstep, scanptr ++)
                    *p = *scanptr;
        	}
              }
            }
            else
              TIFFReadScanline(tif, in, y, 0);

            if (img->colorspace == IMAGE_WHITE)
              ImagePutRow(img, 0, y, img->xsize, in);
            else
            {
	      switch (img->colorspace)
	      {
		case IMAGE_RGB :
		    ImageWhiteToRGB(in, out, img->xsize);
		    break;
		case IMAGE_BLACK :
		    ImageWhiteToBlack(in, out, img->xsize);
		    break;
		case IMAGE_CMY :
		    ImageWhiteToCMY(in, out, img->xsize);
		    break;
		case IMAGE_CMYK :
		    ImageWhiteToCMYK(in, out, img->xsize);
		    break;
	      }

              ImagePutRow(img, 0, y, img->xsize, out);
	    }
          }
        }
        else
        {
         /*
          * Column major order...
          */

          for (x = xstart, xcount = img->xsize;
               xcount > 0;
               xcount --, x += xdir)
          {
            if (bits == 1)
            {
              TIFFReadScanline(tif, scanline, x, 0);
              for (ycount = img->ysize, scanptr = scanline, p = in + ystart, bit = 128;
                   ycount > 0;
                   ycount --, p += ydir)
              {
        	if (*scanptr & bit)
                  *p = one;
                else
                  *p = zero;

        	if (bit > 1)
                  bit >>= 1;
        	else
        	{
                  bit = 128;
                  scanptr ++;
        	}
              }
            }
            else if (bits == 2)
            {
              TIFFReadScanline(tif, scanline, x, 0);
              for (ycount = img->ysize, scanptr = scanline, p = in + ystart, bit = 0xc0;
                   ycount > 0;
                   ycount --, p += ydir)
              {
                pixel = *scanptr & 0xc0;
                while (pixel > 3)
                  pixel >>= 2;

                *p = (255 * pixel / 3) ^ zero;

        	if (bit > 3)
                  bit >>= 2;
        	else
        	{
                  bit = 0xc0;
                  scanptr ++;
        	}
              }
            }
            else if (bits == 4)
            {
              TIFFReadScanline(tif, scanline, x, 0);
              for (ycount = img->ysize, scanptr = scanline, p = in + ystart, bit = 0xf0;
                   ycount > 0;
                   ycount --, p += ydir)
              {
        	if (bit == 0xf0)
        	{
                  *p = (255 * ((*scanptr & 0xf0) >> 4) / 15) ^ zero;
                  bit = 0x0f;
                }
        	else
        	{
                  *p = (255 * (*scanptr & 0x0f) / 15) ^ zero;
                  bit = 0xf0;
                  scanptr ++;
        	}
              }
            }
            else if (ydir < 0 || zero || alpha)
            {
              TIFFReadScanline(tif, scanline, x, 0);

              if (alpha)
	      {
		if (zero)
        	{
                  for (ycount = img->ysize, p = in + ystart, scanptr = scanline;
                       ycount > 0;
                       ycount --, p += ydir, scanptr += 2)
                    *p = (scanptr[1] * (255 - scanptr[0]) +
		          (255 - scanptr[1]) * 255) / 255;
        	}
        	else
        	{
                  for (ycount = img->ysize, p = in + ystart, scanptr = scanline;
                       ycount > 0;
                       ycount --, p += ydir, scanptr += 2)
                    *p = (scanptr[1] * scanptr[0] +
		          (255 - scanptr[1]) * 255) / 255;
        	}
              }
	      else
	      {
		if (zero)
        	{
                  for (ycount = img->ysize, p = in + ystart, scanptr = scanline;
                       ycount > 0;
                       ycount --, p += ydir, scanptr ++)
                    *p = 255 - *scanptr;
        	}
        	else
        	{
                  for (ycount = img->ysize, p = in + ystart, scanptr = scanline;
                       ycount > 0;
                       ycount --, p += ydir, scanptr ++)
                    *p = *scanptr;
        	}
	      }
            }
            else
              TIFFReadScanline(tif, in, x, 0);

            if (img->colorspace == IMAGE_WHITE)
              ImagePutCol(img, x, 0, img->ysize, in);
            else
            {
	      switch (img->colorspace)
	      {
		case IMAGE_RGB :
		    ImageWhiteToRGB(in, out, img->ysize);
		    break;
		case IMAGE_BLACK :
		    ImageWhiteToBlack(in, out, img->ysize);
		    break;
		case IMAGE_CMY :
		    ImageWhiteToCMY(in, out, img->ysize);
		    break;
		case IMAGE_CMYK :
		    ImageWhiteToCMYK(in, out, img->ysize);
		    break;
	      }

              ImagePutCol(img, x, 0, img->ysize, out);
	    }
          }
        }
        break;

    case PHOTOMETRIC_PALETTE :
	if (!TIFFGetField(tif, TIFFTAG_COLORMAP, &redcmap, &greencmap, &bluecmap))
	{
	  fclose(fp);
	  return (-1);
	}

        num_colors = 1 << bits;

        for (c = 0; c < num_colors; c ++)
	{
	  redcmap[c]   >>= 8;
	  greencmap[c] >>= 8;
	  bluecmap[c]  >>= 8;
	}

        if (orientation < ORIENTATION_LEFTTOP)
        {
         /*
          * Row major order...
          */

          for (y = ystart, ycount = img->ysize;
               ycount > 0;
               ycount --, y += ydir)
          {
            if (bits == 1)
            {
              TIFFReadScanline(tif, scanline, y, 0);
              for (xcount = img->xsize, scanptr = scanline,
	               p = in + xstart * 3, bit = 128;
                   xcount > 0;
                   xcount --, p += pstep)
              {
        	if (*scanptr & bit)
		{
                  p[0] = redcmap[1];
                  p[1] = greencmap[1];
                  p[2] = bluecmap[1];
		}
                else
		{
                  p[0] = redcmap[0];
                  p[1] = greencmap[0];
                  p[2] = bluecmap[0];
		}

        	if (bit > 1)
                  bit >>= 1;
        	else
        	{
                  bit = 128;
                  scanptr ++;
        	}
              }
            }
            else if (bits == 2)
            {
              TIFFReadScanline(tif, scanline, y, 0);
              for (xcount = img->xsize, scanptr = scanline,
	               p = in + xstart * 3, bit = 0xc0;
                   xcount > 0;
                   xcount --, p += pstep)
              {
                pixel = *scanptr & bit;
                while (pixel > 3)
                  pixel >>= 2;

                p[0] = redcmap[pixel];
                p[1] = greencmap[pixel];
                p[2] = bluecmap[pixel];

        	if (bit > 3)
                  bit >>= 2;
        	else
        	{
                  bit = 0xc0;
                  scanptr ++;
        	}
              }
            }
            else if (bits == 4)
            {
              TIFFReadScanline(tif, scanline, y, 0);
              for (xcount = img->xsize, scanptr = scanline,
	               p = in + 3 * xstart, bit = 0xf0;
                   xcount > 0;
                   xcount --, p += pstep)
              {
                if (bit == 0xf0)
                {
		  pixel = (*scanptr & 0xf0) >> 4;
                  p[0]  = redcmap[pixel];
                  p[1]  = greencmap[pixel];
                  p[2]  = bluecmap[pixel];
                  bit   = 0x0f;
                }
                else
        	{
		  pixel = *scanptr++ & 0x0f;
                  p[0]  = redcmap[pixel];
                  p[1]  = greencmap[pixel];
                  p[2]  = bluecmap[pixel];
                  bit   = 0xf0;
        	}
              }
            }
            else
            {
              TIFFReadScanline(tif, scanline, y, 0);

              for (xcount = img->xsize, p = in + 3 * xstart, scanptr = scanline;
                   xcount > 0;
                   xcount --, p += pstep)
              {
	        p[0] = redcmap[*scanptr];
	        p[1] = greencmap[*scanptr];
	        p[2] = bluecmap[*scanptr++];
	      }
            }

            if (img->colorspace == IMAGE_RGB)
              ImagePutRow(img, 0, y, img->xsize, in);
            else
            {
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

              ImagePutRow(img, 0, y, img->xsize, out);
	    }
          }
        }
        else
        {
         /*
          * Column major order...
          */

          for (x = xstart, xcount = img->xsize;
               xcount > 0;
               xcount --, x += xdir)
          {
            if (bits == 1)
            {
              TIFFReadScanline(tif, scanline, x, 0);
              for (ycount = img->ysize, scanptr = scanline,
	               p = in + 3 * ystart, bit = 128;
                   ycount > 0;
                   ycount --, p += ydir)
              {
        	if (*scanptr & bit)
		{
                  p[0] = redcmap[1];
                  p[1] = greencmap[1];
                  p[2] = bluecmap[1];
		}
                else
		{
                  p[0] = redcmap[0];
                  p[1] = greencmap[0];
                  p[2] = bluecmap[0];
		}

        	if (bit > 1)
                  bit >>= 1;
        	else
        	{
                  bit = 128;
                  scanptr ++;
        	}
              }
            }
            else if (bits == 2)
            {
              TIFFReadScanline(tif, scanline, x, 0);
              for (ycount = img->ysize, scanptr = scanline,
	               p = in + 3 * ystart, bit = 0xc0;
                   ycount > 0;
                   ycount --, p += ydir)
              {
                pixel = *scanptr & 0xc0;
                while (pixel > 3)
                  pixel >>= 2;

                p[0] = redcmap[pixel];
                p[1] = greencmap[pixel];
                p[2] = bluecmap[pixel];

        	if (bit > 3)
                  bit >>= 2;
        	else
        	{
                  bit = 0xc0;
                  scanptr ++;
        	}
              }
            }
            else if (bits == 4)
            {
              TIFFReadScanline(tif, scanline, x, 0);
              for (ycount = img->ysize, scanptr = scanline,
	               p = in + 3 * ystart, bit = 0xf0;
                   ycount > 0;
                   ycount --, p += ydir)
              {
                if (bit == 0xf0)
                {
		  pixel = (*scanptr & 0xf0) >> 4;
                  p[0]  = redcmap[pixel];
                  p[1]  = greencmap[pixel];
                  p[2]  = bluecmap[pixel];
                  bit   = 0x0f;
                }
                else
        	{
		  pixel = *scanptr++ & 0x0f;
                  p[0]  = redcmap[pixel];
                  p[1]  = greencmap[pixel];
                  p[2]  = bluecmap[pixel];
                  bit   = 0xf0;
        	}
              }
            }
            else
            {
              TIFFReadScanline(tif, scanline, x, 0);

              for (ycount = img->ysize, p = in + 3 * ystart, scanptr = scanline;
                   ycount > 0;
                   ycount --, p += ydir)
              {
	        p[0] = redcmap[*scanptr];
	        p[1] = greencmap[*scanptr];
	        p[2] = bluecmap[*scanptr++];
	      }
            }

            if (img->colorspace == IMAGE_RGB)
              ImagePutCol(img, x, 0, img->ysize, in);
            else
            {
	      switch (img->colorspace)
	      {
		case IMAGE_WHITE :
		    ImageRGBToWhite(in, out, img->ysize);
		    break;
		case IMAGE_BLACK :
		    ImageRGBToBlack(in, out, img->ysize);
		    break;
		case IMAGE_CMY :
		    ImageRGBToCMY(in, out, img->ysize);
		    break;
		case IMAGE_CMYK :
		    ImageRGBToCMYK(in, out, img->ysize);
		    break;
	      }

              ImagePutCol(img, x, 0, img->ysize, out);
	    }
          }
        }
        break;

    case PHOTOMETRIC_RGB :
        if (orientation < ORIENTATION_LEFTTOP)
        {
         /*
          * Row major order...
          */

          for (y = ystart, ycount = img->ysize;
               ycount > 0;
               ycount --, y += ydir)
          {
            if (bits == 1)
            {
              TIFFReadScanline(tif, scanline, y, 0);
              for (xcount = img->xsize, scanptr = scanline, p = in + xstart * 3, bit = 0xf0;
                   xcount > 0;
                   xcount --, p += pstep)
              {
        	if (*scanptr & bit & 0x88)
                  p[0] = 255;
                else
                  p[0] = 0;

        	if (*scanptr & bit & 0x44)
                  p[1] = 255;
                else
                  p[1] = 0;

        	if (*scanptr & bit & 0x22)
                  p[2] = 255;
                else
                  p[2] = 0;

        	if (bit == 0xf0)
                  bit = 0x0f;
        	else
        	{
                  bit = 0xf0;
                  scanptr ++;
        	}
              }
            }
            else if (bits == 2)
            {
              TIFFReadScanline(tif, scanline, y, 0);
              for (xcount = img->xsize, scanptr = scanline, p = in + xstart * 3;
                   xcount > 0;
                   xcount --, p += pstep, scanptr ++)
              {
                pixel = *scanptr >> 2;
                p[0] = 255 * (pixel & 3) / 3;
                pixel >>= 2;
                p[1] = 255 * (pixel & 3) / 3;
                pixel >>= 2;
                p[2] = 255 * (pixel & 3) / 3;
              }
            }
            else if (bits == 4)
            {
              TIFFReadScanline(tif, scanline, y, 0);
              for (xcount = img->xsize, scanptr = scanline, p = in + xstart * 3;
                   xcount > 0;
                   xcount -= 2, p += 2 * pstep, scanptr += 3)
              {
                pixel = scanptr[0];
                p[1] = 255 * (pixel & 15) / 15;
                pixel >>= 4;
                p[0] = 255 * (pixel & 15) / 15;
                pixel = scanptr[1];
                p[2] = 255 * ((pixel >> 4) & 15) / 15;

                if (xcount > 1)
                {
                  p[pstep + 0] = 255 * (pixel & 15) / 15;
                  pixel = scanptr[2];
                  p[pstep + 2] = 255 * (pixel & 15) / 15;
                  pixel >>= 4;
                  p[pstep + 1] = 255 * (pixel & 15) / 15;
                }
              }
            }
            else if (xdir < 0 || alpha)
            {
              TIFFReadScanline(tif, scanline, y, 0);

              if (alpha)
	      {
        	for (xcount = img->xsize, p = in + xstart * 3, scanptr = scanline;
                     xcount > 0;
                     xcount --, p += pstep, scanptr += 4)
        	{
                  p[0] = (scanptr[0] * scanptr[3] + 255 * (255 - scanptr[3])) / 255;
                  p[1] = (scanptr[1] * scanptr[3] + 255 * (255 - scanptr[3])) / 255;
                  p[2] = (scanptr[2] * scanptr[3] + 255 * (255 - scanptr[3])) / 255;
        	}
              }
	      else
              {
	      	for (xcount = img->xsize, p = in + xstart * 3, scanptr = scanline;
                     xcount > 0;
                     xcount --, p += pstep, scanptr += 3)
        	{
                  p[0] = scanptr[0];
                  p[1] = scanptr[1];
                  p[2] = scanptr[2];
        	}
	      }
            }
            else
              TIFFReadScanline(tif, in, y, 0);

            if ((saturation != 100 || hue != 0) && bpp > 1)
              ImageRGBAdjust(in, img->xsize, saturation, hue);

            if (img->colorspace == IMAGE_RGB)
              ImagePutRow(img, 0, y, img->xsize, in);
            else
            {
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

              ImagePutRow(img, 0, y, img->xsize, out);
	    }
          }
        }
        else
        {
         /*
          * Column major order...
          */

          for (x = xstart, xcount = img->xsize;
               xcount > 0;
               xcount --, x += xdir)
          {
            if (bits == 1)
            {
              TIFFReadScanline(tif, scanline, x, 0);
              for (ycount = img->ysize, scanptr = scanline, p = in + ystart * 3, bit = 0xf0;
                   ycount > 0;
                   ycount --, p += pstep)
              {
        	if (*scanptr & bit & 0x88)
                  p[0] = one;
                else
                  p[0] = zero;

        	if (*scanptr & bit & 0x44)
                  p[1] = one;
                else
                  p[1] = zero;

        	if (*scanptr & bit & 0x22)
                  p[2] = one;
                else
                  p[2] = zero;

        	if (bit == 0xf0)
                  bit = 0x0f;
        	else
        	{
                  bit = 0xf0;
                  scanptr ++;
        	}
              }
            }
            else if (bits == 2)
            {
              TIFFReadScanline(tif, scanline, x, 0);
              for (ycount = img->ysize, scanptr = scanline, p = in + ystart * 3;
                   ycount > 0;
                   ycount --, p += pstep, scanptr ++)
              {
                pixel = *scanptr >> 2;
                p[0] = 255 * (pixel & 3) / 3;
                pixel >>= 2;
                p[1] = 255 * (pixel & 3) / 3;
                pixel >>= 2;
                p[2] = 255 * (pixel & 3) / 3;
              }
            }
            else if (bits == 4)
            {
              TIFFReadScanline(tif, scanline, x, 0);
              for (ycount = img->ysize, scanptr = scanline, p = in + ystart * 3;
                   ycount > 0;
                   ycount -= 2, p += 2 * pstep, scanptr += 3)
              {
                pixel = scanptr[0];
                p[1] = 255 * (pixel & 15) / 15;
                pixel >>= 4;
                p[0] = 255 * (pixel & 15) / 15;
                pixel = scanptr[1];
                p[2] = 255 * ((pixel >> 4) & 15) / 15;

                if (ycount > 1)
                {
                  p[pstep + 0] = 255 * (pixel & 15) / 15;
                  pixel = scanptr[2];
                  p[pstep + 2] = 255 * (pixel & 15) / 15;
                  pixel >>= 4;
                  p[pstep + 1] = 255 * (pixel & 15) / 15;
                }
              }
            }
            else if (ydir < 0 || alpha)
            {
              TIFFReadScanline(tif, scanline, x, 0);

              if (alpha)
	      {
		for (ycount = img->ysize, p = in + ystart * 3, scanptr = scanline;
                     ycount > 0;
                     ycount --, p += pstep, scanptr += 4)
        	{
                  p[0] = (scanptr[0] * scanptr[3] + 255 * (255 - scanptr[3])) / 255;
                  p[1] = (scanptr[1] * scanptr[3] + 255 * (255 - scanptr[3])) / 255;
                  p[2] = (scanptr[2] * scanptr[3] + 255 * (255 - scanptr[3])) / 255;
        	}
              }
	      else
	      {
		for (ycount = img->ysize, p = in + ystart * 3, scanptr = scanline;
                     ycount > 0;
                     ycount --, p += pstep, scanptr += 3)
        	{
                  p[0] = scanptr[0];
                  p[1] = scanptr[1];
                  p[2] = scanptr[2];
        	}
	      }
            }
            else
              TIFFReadScanline(tif, in, x, 0);

            if ((saturation != 100 || hue != 0) && bpp > 1)
              ImageRGBAdjust(in, img->ysize, saturation, hue);

            if (img->colorspace == IMAGE_RGB)
              ImagePutCol(img, x, 0, img->ysize, in);
            else
            {
	      switch (img->colorspace)
	      {
		case IMAGE_WHITE :
		    ImageRGBToWhite(in, out, img->ysize);
		    break;
		case IMAGE_BLACK :
		    ImageRGBToBlack(in, out, img->ysize);
		    break;
		case IMAGE_CMY :
		    ImageRGBToCMY(in, out, img->ysize);
		    break;
		case IMAGE_CMYK :
		    ImageRGBToCMYK(in, out, img->ysize);
		    break;
	      }

              ImagePutCol(img, x, 0, img->ysize, out);
	    }
          }
        }
        break;

    case PHOTOMETRIC_SEPARATED :
        TIFFGetField(tif, TIFFTAG_INKSET, &inkset);

	if (inkset == INKSET_CMYK)
	{
          if (orientation < ORIENTATION_LEFTTOP)
          {
           /*
            * Row major order...
            */

            for (y = ystart, ycount = img->ysize;
        	 ycount > 0;
        	 ycount --, y += ydir)
            {
              if (bits == 1)
              {
        	TIFFReadScanline(tif, scanline, y, 0);
        	for (xcount = img->xsize, scanptr = scanline, p = in + xstart * 3, bit = 0xf0;
                     xcount > 0;
                     xcount --, p += pstep)
        	{
        	  if (*scanptr & bit & 0x11)
        	  {
                    p[0] = 0;
                    p[1] = 0;
                    p[2] = 0;
                  }
                  else
                  {
        	    if (*scanptr & bit & 0x88)
                      p[0] = 0;
                    else
                      p[0] = 255;

        	    if (*scanptr & bit & 0x44)
                      p[1] = 0;
                    else
                      p[1] = 255;

        	    if (*scanptr & bit & 0x22)
                      p[2] = 0;
                    else
                      p[2] = 255;
                  }

        	  if (bit == 0xf0)
                    bit = 0x0f;
        	  else
        	  {
                    bit = 0xf0;
                    scanptr ++;
        	  }
        	}
              }
              else if (bits == 2)
              {
        	TIFFReadScanline(tif, scanline, y, 0);
        	for (xcount = img->xsize, scanptr = scanline, p = in + xstart * 3;
                     xcount > 0;
                     xcount --, p += pstep, scanptr ++)
        	{
        	  pixel = *scanptr;
        	  k     = 255 * (pixel & 3) / 3;
        	  if (k == 255)
        	  {
        	    p[0] = 0;
        	    p[1] = 0;
        	    p[2] = 0;
        	  }
        	  else
        	  {
                    pixel >>= 2;
                    b = 255 - 255 * (pixel & 3) / 3 - k;
                    if (b < 0)
                      p[2] = 0;
                    else if (b < 256)
                      p[2] = b;
                    else
                      p[2] = 255;

                    pixel >>= 2;
                    g = 255 - 255 * (pixel & 3) / 3 - k;
                    if (g < 0)
                      p[1] = 0;
                    else if (g < 256)
                      p[1] = g;
                    else
                      p[1] = 255;

                    pixel >>= 2;
                    r = 255 - 255 * (pixel & 3) / 3 - k;
                    if (r < 0)
                      p[0] = 0;
                    else if (r < 256)
                      p[0] = r;
                    else
                      p[0] = 255;
                  }
        	}
              }
              else if (bits == 4)
              {
        	TIFFReadScanline(tif, scanline, y, 0);
        	for (xcount = img->xsize, scanptr = scanline, p = in + xstart * 3;
                     xcount > 0;
                     xcount --, p += pstep, scanptr += 2)
        	{
        	  pixel = scanptr[1];
        	  k     = 255 * (pixel & 15) / 15;
        	  if (k == 255)
        	  {
        	    p[0] = 0;
        	    p[1] = 0;
        	    p[2] = 0;
        	  }
        	  else
        	  {
                    pixel >>= 4;
                    b = 255 - 255 * (pixel & 15) / 15 - k;
                    if (b < 0)
                      p[2] = 0;
                    else if (b < 256)
                      p[2] = b;
                    else
                      p[2] = 255;

                    pixel = scanptr[0];
                    g = 255 - 255 * (pixel & 15) / 15 - k;
                    if (g < 0)
                      p[1] = 0;
                    else if (g < 256)
                      p[1] = g;
                    else
                      p[1] = 255;

                    pixel >>= 4;
                    r = 255 - 255 * (pixel & 15) / 15 - k;
                    if (r < 0)
                      p[0] = 0;
                    else if (r < 256)
                      p[0] = r;
                    else
                      p[0] = 255;
                  }
        	}
              }
              else
              {
        	TIFFReadScanline(tif, scanline, y, 0);

        	for (xcount = img->xsize, p = in + xstart * 3, scanptr = scanline;
                     xcount > 0;
                     xcount --, p += pstep, scanptr += 4)
        	{
        	  k = scanptr[3];
        	  if (k == 255)
        	  {
        	    p[0] = 0;
        	    p[1] = 0;
        	    p[2] = 0;
        	  }
        	  else
        	  {
                    r = 255 - scanptr[0] - k;
                    if (r < 0)
                      p[0] = 0;
                    else if (r < 256)
                      p[0] = r;
                    else
                      p[0] = 255;

                    g = 255 - scanptr[1] - k;
                    if (g < 0)
                      p[1] = 0;
                    else if (g < 256)
                      p[1] = g;
                    else
                      p[1] = 255;

                    b = 255 - scanptr[2] - k;
                    if (b < 0)
                      p[2] = 0;
                    else if (b < 256)
                      p[2] = b;
                    else
                      p[2] = 255;
        	  }
        	}
              }

              if ((saturation != 100 || hue != 0) && bpp > 1)
        	ImageRGBAdjust(in, img->xsize, saturation, hue);

              if (img->colorspace == IMAGE_RGB)
        	ImagePutRow(img, 0, y, img->xsize, in);
              else
              {
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

        	ImagePutRow(img, 0, y, img->xsize, out);
	      }
            }
          }
          else
          {
           /*
            * Column major order...
            */

            for (x = xstart, xcount = img->xsize;
        	 xcount > 0;
        	 xcount --, x += xdir)
            {
              if (bits == 1)
              {
        	TIFFReadScanline(tif, scanline, x, 0);
        	for (ycount = img->ysize, scanptr = scanline, p = in + xstart * 3, bit = 0xf0;
                     ycount > 0;
                     ycount --, p += pstep)
        	{
        	  if (*scanptr & bit & 0x11)
        	  {
                    p[0] = 0;
                    p[1] = 0;
                    p[2] = 0;
                  }
                  else
                  {
        	    if (*scanptr & bit & 0x88)
                      p[0] = 0;
                    else
                      p[0] = 255;

        	    if (*scanptr & bit & 0x44)
                      p[1] = 0;
                    else
                      p[1] = 255;

        	    if (*scanptr & bit & 0x22)
                      p[2] = 0;
                    else
                      p[2] = 255;
                  }

        	  if (bit == 0xf0)
                    bit = 0x0f;
        	  else
        	  {
                    bit = 0xf0;
                    scanptr ++;
        	  }
        	}
              }
              else if (bits == 2)
              {
        	TIFFReadScanline(tif, scanline, x, 0);
        	for (ycount = img->ysize, scanptr = scanline, p = in + xstart * 3;
                     ycount > 0;
                     ycount --, p += pstep, scanptr ++)
        	{
        	  pixel = *scanptr;
        	  k     = 255 * (pixel & 3) / 3;
        	  if (k == 255)
        	  {
        	    p[0] = 0;
        	    p[1] = 0;
        	    p[2] = 0;
        	  }
        	  else
        	  {
                    pixel >>= 2;
                    b = 255 - 255 * (pixel & 3) / 3 - k;
                    if (b < 0)
                      p[2] = 0;
                    else if (b < 256)
                      p[2] = b;
                    else
                      p[2] = 255;

                    pixel >>= 2;
                    g = 255 - 255 * (pixel & 3) / 3 - k;
                    if (g < 0)
                      p[1] = 0;
                    else if (g < 256)
                      p[1] = g;
                    else
                      p[1] = 255;

                    pixel >>= 2;
                    r = 255 - 255 * (pixel & 3) / 3 - k;
                    if (r < 0)
                      p[0] = 0;
                    else if (r < 256)
                      p[0] = r;
                    else
                      p[0] = 255;
                  }
        	}
              }
              else if (bits == 4)
              {
        	TIFFReadScanline(tif, scanline, x, 0);
        	for (ycount = img->ysize, scanptr = scanline, p = in + xstart * 3;
                     ycount > 0;
                     ycount --, p += pstep, scanptr += 2)
        	{
        	  pixel = scanptr[1];
        	  k     = 255 * (pixel & 15) / 15;
        	  if (k == 255)
        	  {
        	    p[0] = 0;
        	    p[1] = 0;
        	    p[2] = 0;
        	  }
        	  else
        	  {
                    pixel >>= 4;
                    b = 255 - 255 * (pixel & 15) / 15 - k;
                    if (b < 0)
                      p[2] = 0;
                    else if (b < 256)
                      p[2] = b;
                    else
                      p[2] = 255;

                    pixel = scanptr[0];
                    g = 255 - 255 * (pixel & 15) / 15 - k;
                    if (g < 0)
                      p[1] = 0;
                    else if (g < 256)
                      p[1] = g;
                    else
                      p[1] = 255;

                    pixel >>= 4;
                    r = 255 - 255 * (pixel & 15) / 15 - k;
                    if (r < 0)
                      p[0] = 0;
                    else if (r < 256)
                      p[0] = r;
                    else
                      p[0] = 255;
                  }
        	}
              }
              else
              {
        	TIFFReadScanline(tif, scanline, x, 0);

        	for (ycount = img->ysize, p = in + xstart * 3, scanptr = scanline;
                     ycount > 0;
                     ycount --, p += pstep, scanptr += 4)
        	{
        	  k = scanptr[3];
        	  if (k == 255)
        	  {
        	    p[0] = 0;
        	    p[1] = 0;
        	    p[2] = 0;
        	  }
        	  else
        	  {
                    r = 255 - scanptr[0] - k;
                    if (r < 0)
                      p[0] = 0;
                    else if (r < 256)
                      p[0] = r;
                    else
                      p[0] = 255;

                    g = 255 - scanptr[1] - k;
                    if (g < 0)
                      p[1] = 0;
                    else if (g < 256)
                      p[1] = g;
                    else
                      p[1] = 255;

                    b = 255 - scanptr[2] - k;
                    if (b < 0)
                      p[2] = 0;
                    else if (b < 256)
                      p[2] = b;
                    else
                      p[2] = 255;
        	  }
        	}
              }

              if ((saturation != 100 || hue != 0) && bpp > 1)
        	ImageRGBAdjust(in, img->ysize, saturation, hue);

              if (img->colorspace == IMAGE_RGB)
        	ImagePutCol(img, x, 0, img->ysize, in);
              else
              {
		switch (img->colorspace)
		{
		  case IMAGE_WHITE :
		      ImageRGBToWhite(in, out, img->ysize);
		      break;
		  case IMAGE_BLACK :
		      ImageRGBToBlack(in, out, img->ysize);
		      break;
		  case IMAGE_CMY :
		      ImageRGBToCMY(in, out, img->ysize);
		      break;
		  case IMAGE_CMYK :
		      ImageRGBToCMYK(in, out, img->ysize);
		      break;
		}

        	ImagePutCol(img, x, 0, img->ysize, out);
	      }
            }
          }

          break;
	}

    default :
	_TIFFfree(scanline);
	free(in);
	free(out);

	TIFFClose(tif);
	return (-1);
  }

  _TIFFfree(scanline);
  free(in);
  free(out);

  TIFFClose(tif);
  return (0);
}


#endif /* HAVE_LIBTIFF */


/*
 * End of "$Id: image-tiff.c,v 1.6 1999/03/24 18:01:45 mike Exp $".
 */
