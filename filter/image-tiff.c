/*
 * "$Id: image-tiff.c,v 1.3 1998/07/28 20:51:43 mike Exp $"
 *
 *   TIFF file routines for espPrint, a collection of printer drivers.
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
 *   $Log: image-tiff.c,v $
 *   Revision 1.3  1998/07/28 20:51:43  mike
 *   Fixed default orientation code.
 *
 *   Revision 1.2  1998/03/19  17:00:21  mike
 *   Added check for units in resolution (physical) chunk; if undefined
 *   assume meters instead of centimeters...
 *
 *   Revision 1.1  1998/02/19  20:44:58  mike
 *   Initial revision
 *
 */

/*
 * Include necessary headers...
 */

#include "image.h"
#include <tiff.h>	/* TIFF image definitions */
#include <tiffio.h>
#include <tiffiop.h>


int
ImageReadTIFF(image_t *img,
              FILE    *fp,
              int     primary,
              int     secondary,
              int     saturation,
              int     hue)
{
  TIFF		*tif;
  TIFFDirectory	*td;
  uint32	width, height;
  uint16	photometric,
		orientation,
		resunit;
  float		xres,
		yres;
  int		bpp,
		x, y,
		xstart, ystart,
		xdir, ydir,
		xcount, ycount,
		pstep,
		scanwidth,
		r, g, b, k;
  ib_t		*in,
		*out,
		*p,
		*scanline,
		*scanptr,
		bit,
		pixel,
		zero,
		one;


#ifdef hpux
  lseek(fileno(fp), 0, SEEK_SET); /* Work around "feature" in HPUX stdio */
#endif /* hpux */

  if ((tif = TIFFFdOpen(fileno(fp), "", "r")) == NULL)
  {
    fclose(fp);
    return (-1);
  };

  if (!TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width) ||
      !TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height) ||
      !TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, &photometric))
  {
    fclose(fp);
    return (-1);
  };

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
    };
  };

  td = &(tif->tif_dir);

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
  };

  scanwidth = TIFFScanlineSize(tif);
  scanline  = _TIFFmalloc(scanwidth);

  if (orientation < ORIENTATION_LEFTTOP)
  {
    if (td->td_samplesperpixel > 1)
      pstep = xdir * 3;
    else
      pstep = xdir;

    in  = malloc(img->xsize * 3 + 3);
    out = malloc(img->xsize * bpp);
  }
  else
  {
    if (td->td_samplesperpixel > 1)
      pstep = ydir * 3;
    else
      pstep = ydir;

    in  = malloc(img->ysize * 3 + 3);
    out = malloc(img->ysize * bpp);
  };

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
        };

        if (orientation < ORIENTATION_LEFTTOP)
        {
         /*
          * Row major order...
          */

          for (y = ystart, ycount = img->ysize;
               ycount > 0;
               ycount --, y += ydir)
          {
            if (td->td_bitspersample == 1)
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
        	};
              };
            }
            else if (td->td_bitspersample == 2)
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
        	};
              };
            }
            else if (td->td_bitspersample == 4)
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
        	};
              };
            }
            else if (xdir < 0 || zero)
            {
              TIFFReadScanline(tif, scanline, y, 0);

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
              };
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
	      };

              ImagePutRow(img, 0, y, img->xsize, out);
	    };
          };
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
            if (td->td_bitspersample == 1)
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
        	};
              };
            }
            else if (td->td_bitspersample == 2)
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
        	};
              };
            }
            else if (td->td_bitspersample == 4)
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
        	};
              };
            }
            else if (ydir < 0 || zero)
            {
              TIFFReadScanline(tif, scanline, x, 0);

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
              };
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
	      };

              ImagePutCol(img, x, 0, img->ysize, out);
	    };
          };
        };
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
            if (td->td_bitspersample == 1)
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
        	};
              };
            }
            else if (td->td_bitspersample == 2)
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
              };
            }
            else if (td->td_bitspersample == 4)
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
                };
              };
            }
            else if (xdir < 0)
            {
              TIFFReadScanline(tif, scanline, y, 0);

              for (xcount = img->xsize, p = in + xstart * 3, scanptr = scanline;
                   xcount > 0;
                   xcount --, p += pstep, scanptr += 3)
              {
                p[0] = scanptr[0];
                p[1] = scanptr[1];
                p[2] = scanptr[2];
              };
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
	      };

              ImagePutRow(img, 0, y, img->xsize, out);
	    };
          };
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
            if (td->td_bitspersample == 1)
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
        	};
              };
            }
            else if (td->td_bitspersample == 2)
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
              };
            }
            else if (td->td_bitspersample == 4)
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
                };
              };
            }
            else if (ydir < 0)
            {
              TIFFReadScanline(tif, scanline, x, 0);

              for (ycount = img->ysize, p = in + ystart * 3, scanptr = scanline;
                   ycount > 0;
                   ycount --, p += pstep, scanptr += 3)
              {
                p[0] = scanptr[0];
                p[1] = scanptr[1];
                p[2] = scanptr[2];
              };
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
	      };

              ImagePutCol(img, x, 0, img->ysize, out);
	    };
          };
        };
        break;

    case PHOTOMETRIC_SEPARATED :
	if (td->td_inkset == INKSET_CMYK)
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
              if (td->td_bitspersample == 1)
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
                  };

        	  if (bit == 0xf0)
                    bit = 0x0f;
        	  else
        	  {
                    bit = 0xf0;
                    scanptr ++;
        	  };
        	};
              }
              else if (td->td_bitspersample == 2)
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
                  };
        	};
              }
              else if (td->td_bitspersample == 4)
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
                  };
        	};
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
        	  };
        	};
              };

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
		};

        	ImagePutRow(img, 0, y, img->xsize, out);
	      };
            };
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
              if (td->td_bitspersample == 1)
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
                  };

        	  if (bit == 0xf0)
                    bit = 0x0f;
        	  else
        	  {
                    bit = 0xf0;
                    scanptr ++;
        	  };
        	};
              }
              else if (td->td_bitspersample == 2)
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
                  };
        	};
              }
              else if (td->td_bitspersample == 4)
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
                  };
        	};
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
        	  };
        	};
              };

              if ((saturation != 100 || hue != 0) && bpp > 1)
        	ImageRGBAdjust(in, img->ysize, saturation, hue);

              if (img->colorspace == IMAGE_RGB)
        	ImagePutCol(img, x, 0, img->ysize, in);
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
		};

        	ImagePutCol(img, x, 0, img->ysize, out);
	      };
            };
          };

          break;
	};

    default :
	_TIFFfree(scanline);
	free(in);
	free(out);

	TIFFClose(tif);
	return (-1);
  };

  _TIFFfree(scanline);
  free(in);
  free(out);

  TIFFClose(tif);
  return (0);
}


/*
 * End of "$Id: image-tiff.c,v 1.3 1998/07/28 20:51:43 mike Exp $".
 */
