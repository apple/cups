/*
 * "$Id: imagetoraster.c,v 1.10 1999/04/01 18:25:04 mike Exp $"
 *
 *   Image file to raster filter for the Common UNIX Printing System (CUPS).
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

/*
 * Include necessary headers...
 */

#include "common.h"
#include "image.h"
#include <cups/raster.h>
#include <math.h>


/*
 * Globals...
 */

int	Flip = 0,		/* Flip/mirror pages */
	Collate = 0,		/* Collate copies? */
	Copies = 1;		/* Number of copies */
int	FloydDither[16][16] =	/* Traditional Floyd ordered dither */
	{
	  { 0,   128, 32,  160, 8,   136, 40,  168,
	    2,   130, 34,  162, 10,  138, 42,  170 },
	  { 192, 64,  224, 96,  200, 72,  232, 104,
	    194, 66,  226, 98,  202, 74,  234, 106 },
	  { 48,  176, 16,  144, 56,  184, 24,  152,
	    50,  178, 18,  146, 58,  186, 26,  154 },
	  { 240, 112, 208, 80,  248, 120, 216, 88,
	    242, 114, 210, 82,  250, 122, 218, 90 },
	  { 12,  140, 44,  172, 4,   132, 36,  164,
	    14,  142, 46,  174, 6,   134, 38,  166 },
	  { 204, 76,  236, 108, 196, 68,  228, 100,
	    206, 78,  238, 110, 198, 70,  230, 102 },
	  { 60,  188, 28,  156, 52,  180, 20,  148,
	    62,  190, 30,  158, 54,  182, 22,  150 },
	  { 252, 124, 220, 92,  244, 116, 212, 84,
	    254, 126, 222, 94,  246, 118, 214, 86 },
	  { 3,   131, 35,  163, 11,  139, 43,  171,
	    1,   129, 33,  161, 9,   137, 41,  169 },
	  { 195, 67,  227, 99,  203, 75,  235, 107,
	    193, 65,  225, 97,  201, 73,  233, 105 },
	  { 51,  179, 19,  147, 59,  187, 27,  155,
	    49,  177, 17,  145, 57,  185, 25,  153 },
	  { 243, 115, 211, 83,  251, 123, 219, 91,
	    241, 113, 209, 81,  249, 121, 217, 89 },
	  { 15,  143, 47,  175, 7,   135, 39,  167,
	    13,  141, 45,  173, 5,   133, 37,  165 },
	  { 207, 79,  239, 111, 199, 71,  231, 103,
	    205, 77,  237, 109, 197, 69,  229, 101 },
	  { 63,  191, 31,  159, 55,  183, 23,  151,
	    61,  189, 29,  157, 53,  181, 21,  149 },
	  { 254, 127, 223, 95,  247, 119, 215, 87,
	    253, 125, 221, 93,  245, 117, 213, 85 }
	};


/*
 * Local functions...
 */

static void	make_lut(ib_t *, int, float, float);


/*
 * 'main()' - Main entry...
 */

int			/* O - Exit status */
main(int  argc,		/* I - Number of command-line arguments */
     char *argv[])	/* I - Command-line arguments */
{
  int		i;		/* Looping var */
  image_t	*img;		/* Image to print */
  float		xprint,		/* Printable area */
		yprint,
		xinches,	/* Total size in inches */
		yinches;
  float		xsize,		/* Total size in points */
		ysize;
  int		xpages,		/* # x pages */
		ypages,		/* # y pages */
		xpage,		/* Current x page */
		ypage,		/* Current y page */
		xtemp,		/* Bitmap width in pixels */
		ytemp,		/* Bitmap height in pixels */
		page;		/* Current page number */
  int		x0, y0,		/* Corners of the page in image coords */
		x1, y1;
  ppd_file_t	*ppd;		/* PPD file */
  ppd_choice_t	*choice;	/* PPD option choice */
  char		*resolution,	/* Output resolution */
		*media_type;	/* Media type */
  ppd_profile_t	*profile;	/* Color profile */
  cups_raster_t	*ras;		/* Raster stream */
  cups_page_header_t header;	/* Page header */
  int		num_options;	/* Number of print options */
  cups_option_t	*options;	/* Print options */
  char		*val;		/* Option value */
  int		slowcollate;	/* Collate copies the slow way */
  float		g;		/* Gamma correction value */
  float		b;		/* Brightness factor */
  float		zoom;		/* Zoom facter */
  int		ppi;		/* Pixels-per-inch */
  int		hue, sat;	/* Hue and saturation adjustment */
  izoom_t	*z;		/* ImageZoom buffer */
  int		primary,	/* Primary image colorspace */
		secondary;	/* Secondary image colorspace */
  ib_t		*row,		/* Current row */
		*rowptr,	/* Pointer into row */
		*r0,		/* Top row */
		*r1,		/* Bottom row */
		bitmask;	/* Current mask for pixel */
  int		bitoffset;	/* Current offset in line */
  int		x,		/* Current X coordinate on page */
		y,		/* Current Y coordinate on page */
		iy,		/* Current Y coordinate in image */
		last_iy,	/* Previous Y coordinate in image */
		yerr0,		/* Top Y error value */
		yerr1,		/* Bottom Y error value */
		blank,		/* Blank value */
		*dither;	/* Pointer into dither array */
  ib_t		lut[256],	/* Gamma/brightness LUT */
		onpixels[256],	/* On-pixel LUT */
		offpixels[256];	/* Off-pixel LUT */
  static ib_t	bitmasks[8] =	/* Bit masks */
		{ 128, 64, 32, 16, 8, 4, 2, 1 };
  static int	planes[] =	/* Number of planes for each colorspace */
		{ 1, 3, 4, 1, 3, 3, 4, 4, 4, 6 };


  if (argc != 7)
  {
    fputs("ERROR: imagetoraster job-id user title copies options file\n", stderr);
    return (1);
  }

 /*
  * Process command-line options and write the prolog...
  */

  zoom = 0.0;
  ppi  = 0;
  hue  = 0;
  sat  = 100;
  g    = 1.0;
  b    = 1.0;

  options     = NULL;
  num_options = cupsParseOptions(argv[5], 0, &options);

  ppd = SetCommonOptions(num_options, options);

  ppdMarkDefaults(ppd);
  cupsMarkOptions(ppd, num_options, options);

  if ((val = cupsGetOption("copies", num_options, options)) != NULL)
    Copies = atoi(val);

  if ((val = cupsGetOption("multiple-document-handling", num_options, options)) != NULL)
  {
   /*
    * This IPP attribute is unnecessarily complicated...
    *
    *   single-document, separate-documents-collated-copies, and
    *   single-document-new-sheet all require collated copies.
    *
    *   separate-documents-collated-copies allows for uncollated copies.
    */

    Collate = strcmp(val, "separate-documents-collated-copies") != 0;
  }

  if ((val = cupsGetOption("Collate", num_options, options)) != NULL &&
      strcmp(val, "True") == 0)
    Collate = 1;

  if ((val = cupsGetOption("gamma", num_options, options)) != NULL)
    g = atoi(val) * 0.001f;

  if ((val = cupsGetOption("brightness", num_options, options)) != NULL)
    b = atoi(val) * 0.01f;

  if ((val = cupsGetOption("scaling", num_options, options)) != NULL)
    zoom = atoi(val) * 0.01;

  if ((val = cupsGetOption("ppi", num_options, options)) != NULL)
    ppi = atoi(val);

  if ((val = cupsGetOption("saturation", num_options, options)) != NULL)
    sat = atoi(val);

  if ((val = cupsGetOption("hue", num_options, options)) != NULL)
    hue = atoi(val);

 /*
  * Set the needed options in the page header...
  */

  memset(&header, 0, sizeof(header));

  if ((choice = ppdFindMarkedChoice(ppd, "ColorModel")) != NULL)
  {
    if (choice->num_data > 1)
    {
      header.cupsColorOrder = (cups_order_t)choice->data[0];
      header.cupsColorSpace = (cups_cspace_t)choice->data[1];
    }
    else
    {
      header.cupsColorOrder = CUPS_ORDER_CHUNKED;
      header.cupsColorSpace = CUPS_CSPACE_RGB;
    }
  }
  else
  {
    header.cupsColorOrder = CUPS_ORDER_CHUNKED;
    header.cupsColorSpace = CUPS_CSPACE_CMYK;
  }

  if ((choice = ppdFindMarkedChoice(ppd, "InputSlot")) != NULL)
    header.MediaPosition = choice->data[0];

  if ((choice = ppdFindMarkedChoice(ppd, "MediaType")) != NULL)
  {
    media_type = choice->choice;

    strcpy(header.MediaType, media_type);
  }
  else
    media_type = "";

  if ((choice = ppdFindMarkedChoice(ppd, "Resolution")) != NULL)
  {
    resolution = choice->choice;

    if (sscanf(resolution, "%dx%d", header.HWResolution + 0,
               header.HWResolution + 1) == 1)
      header.HWResolution[1] = header.HWResolution[0];

    if (choice->num_data > 0)
      header.cupsBitsPerColor = choice->data[0];
    else
      header.cupsBitsPerColor = 1;
  }
  else
  {
    resolution = "";
    header.HWResolution[0]  = 100;
    header.HWResolution[1]  = 100;
    header.cupsBitsPerColor = 8;
  }

 /*
  * Choose the appropriate colorspace and color profile...
  */

  switch (header.cupsColorSpace)
  {
    case CUPS_CSPACE_W :
        primary   = IMAGE_WHITE;
	secondary = IMAGE_WHITE;
        header.cupsBitsPerPixel = header.cupsBitsPerColor;
	break;

    case CUPS_CSPACE_RGB :
    case CUPS_CSPACE_RGBA :
        primary   = IMAGE_RGB;
	secondary = IMAGE_RGB;

	if (header.cupsColorOrder == CUPS_ORDER_CHUNKED)
	{
	  if (header.cupsBitsPerColor >= 8)
            header.cupsBitsPerPixel = header.cupsBitsPerColor * 3;
	  else
            header.cupsBitsPerPixel = header.cupsBitsPerColor * 4;
	}
	else
	  header.cupsBitsPerPixel = header.cupsBitsPerColor;
	break;

    case CUPS_CSPACE_K :
        primary   = IMAGE_BLACK;
	secondary = IMAGE_BLACK;
        header.cupsBitsPerPixel = header.cupsBitsPerColor;
	break;

    default :
        if (header.cupsBitsPerColor > 1)
	{
          primary   = IMAGE_CMYK;
	  secondary = IMAGE_CMYK;

	  if (header.cupsColorOrder == CUPS_ORDER_CHUNKED)
            header.cupsBitsPerPixel = header.cupsBitsPerColor * 4;
	  else
	    header.cupsBitsPerPixel = header.cupsBitsPerColor;
	  break;
	}

    case CUPS_CSPACE_CMY :
    case CUPS_CSPACE_YMC :
        primary   = IMAGE_CMY;
	secondary = IMAGE_CMY;

	if (header.cupsColorOrder == CUPS_ORDER_CHUNKED)
	{
	  if (header.cupsBitsPerColor >= 8)
            header.cupsBitsPerPixel = 24;
	  else
            header.cupsBitsPerPixel = header.cupsBitsPerColor * 4;
	}
	else
	  header.cupsBitsPerPixel = header.cupsBitsPerColor;
	break;

    case CUPS_CSPACE_KCMYcm :
	if (header.cupsBitsPerPixel == 1)
	{
          primary   = IMAGE_CMY;
	  secondary = IMAGE_CMY;

	  if (header.cupsColorOrder == CUPS_ORDER_CHUNKED)
	    header.cupsBitsPerPixel = 8;
	  else
	    header.cupsBitsPerPixel = 1;
	}
	else
	{
          primary   = IMAGE_CMYK;
	  secondary = IMAGE_CMYK;

	  if (header.cupsColorOrder == CUPS_ORDER_CHUNKED)
	  {
	    if (header.cupsBitsPerPixel >= 8)
	      header.cupsBitsPerPixel = header.cupsBitsPerColor * 6;
	    else
	      header.cupsBitsPerPixel = header.cupsBitsPerColor * 8;
	  }
	  else
	    header.cupsBitsPerPixel = header.cupsBitsPerColor;
	}
	break;
  }

 /*
  * Find a color profile matching the current options...
  */

  if (ppd != NULL)
  {
    for (i = 0, profile = ppd->profiles; i < ppd->num_profiles; i ++, profile ++)
      if ((strcmp(profile->resolution, resolution) == 0 ||
           profile->resolution[0] == '-') &&
          (strcmp(profile->media_type, media_type) == 0 ||
           profile->media_type[0] == '-'))
	break;

   /*
    * If we found a color profile, use it!
    */

    if (i < ppd->num_profiles)
    {
      fputs("Setting color profile!\n", stderr);
      ImageSetProfile(profile->density, profile->matrix);
    }
  }

 /*
  * Create a gamma/brightness LUT...
  */

  make_lut(lut, primary, g, b);

 /*
  * Open the input image to print...
  */

  fputs("INFO: Loading image file...\n", stderr);

  if ((img = ImageOpen(argv[6], primary, secondary, sat, hue, lut)) == NULL)
  {
    fputs("ERROR: Unable to open image file for printing!\n", stderr);
    ppdClose(ppd);
    return (1);
  }

 /*
  * Scale as necessary...
  */

  xprint = (PageRight - PageLeft) / 72.0;
  yprint = (PageTop - PageBottom) / 72.0;

  if (zoom == 0.0 && ppi == 0)
    ppi = img->xppi;

  if (ppi > 0)
  {
   /*
    * Scale the image as neccesary to match the desired pixels-per-inch.
    */
    
    xinches = (float)img->xsize / (float)ppi;
    yinches = (float)img->ysize / (float)ppi;
  }
  else
  {
   /*
    * Scale percentage of page size...
    */

    xsize = xprint * zoom;
    ysize = xsize * img->ysize / img->xsize;

    if (ysize > (yprint * zoom))
    {
      ysize = yprint * zoom;
      xsize = ysize * img->xsize / img->ysize;
    }

    xinches = xsize;
    yinches = ysize;
  }

  xpages = ceil(xinches / xprint);
  ypages = ceil(yinches / yprint);

 /*
  * Compute the bitmap size...
  */

  xprint = xinches / xpages;
  yprint = yinches / ypages;

  if (ppd != NULL && ppd->variable_sizes)
  {
    header.cupsWidth   = xprint * header.HWResolution[0];
    header.cupsHeight  = yprint * header.HWResolution[1];
    header.PageSize[0] = header.cupsWidth;
    header.PageSize[1] = header.cupsHeight;
  }
  else
  {
    header.cupsWidth   = (PageRight - PageLeft) * header.HWResolution[0] / 72.0;
    header.cupsHeight  = (PageTop - PageBottom) * header.HWResolution[1] / 72.0;
    header.PageSize[0] = PageWidth * header.HWResolution[0] / 72.0;
    header.PageSize[1] = PageLength * header.HWResolution[1] / 72.0;
  }

  switch (header.cupsColorOrder)
  {
    case CUPS_ORDER_CHUNKED :
        header.cupsBytesPerLine = (header.cupsBitsPerPixel *
	                           header.cupsWidth + 7) / 8;
        break;

    case CUPS_ORDER_BANDED :
        header.cupsBytesPerLine = (header.cupsBitsPerPixel *
	                           header.cupsWidth + 7) / 8 *
				  planes[header.cupsColorSpace];
        break;

    case CUPS_ORDER_PLANAR :
        header.cupsBytesPerLine = (header.cupsBitsPerPixel *
	                           header.cupsWidth + 7) / 8;
        break;
  }

 /*
  * See if we need to collate, and if so how we need to do it...
  */

  if (xpages == 1 && ypages == 1)
    Collate = 0;

  slowcollate = Collate && ppdFindOption(ppd, "Collate") == NULL;

  if (Copies > 1 && !slowcollate)
  {
    header.Collate   = (cups_bool_t)Collate;
    header.NumCopies = Copies;

    Copies = 1;
  }

 /*
  * Create the dithering lookup tables...
  */

  onpixels[0]  = 0;
  offpixels[0] = 0;

  switch (header.cupsBitsPerColor)
  {
    case 2 :
	for (i = 1; i < 64; i ++)
	  onpixels[i] = 0;
	for (; i < 128; i ++)
	  onpixels[i] = 1;
	for (; i < 192; i ++)
	  onpixels[i] = 2;
	for (; i < 256; i ++)
	  onpixels[i] = 3;

	for (i = 1; i < 96; i ++)
	  offpixels[i] = 1;
	for (; i < 224; i ++)
	  offpixels[i] = 2;
	for (; i < 256; i ++)
	  offpixels[i] = 3;
        break;
    case 4 :
        for (i = 1; i < 256; i ++)
        {
          onpixels[i]  = i / 16;
          onpixels[i]  |= onpixels[i] << 4;
          offpixels[i] = i / 17 + 1;
          offpixels[i] |= offpixels[i] << 4;
        }
        break;
  }

 /*
  * Output the pages...
  */

  fprintf(stderr, "DEBUG: cupsWidth = %d\n", header.cupsWidth);
  fprintf(stderr, "DEBUG: cupsHeight = %d\n", header.cupsHeight);
  fprintf(stderr, "DEBUG: cupsBitsPerColor = %d\n", header.cupsBitsPerColor);
  fprintf(stderr, "DEBUG: cupsBitsPerPixel = %d\n", header.cupsBitsPerPixel);
  fprintf(stderr, "DEBUG: cupsBytesPerLine = %d\n", header.cupsBytesPerLine);
  fprintf(stderr, "DEBUG: cupsColorOrder = %d\n", header.cupsColorOrder);
  fprintf(stderr, "DEBUG: cupsColorSpace = %d\n", header.cupsColorSpace);

  row   = malloc(header.cupsBytesPerLine);
  ras   = cupsRasterOpen(1, CUPS_RASTER_WRITE);
  blank = img->colorspace < 0 ? 0 : ~0;

  for (i = 0, page = 1; i < Copies; i ++)
    for (xpage = 0; xpage < xpages; xpage ++)
      for (ypage = 0; ypage < ypages; ypage ++, page ++)
      {
        fprintf(stderr, "INFO: Formatting page %d...\n", page);

	if (!(Orientation & 1))
	{
	  x0 = img->xsize * xpage / xpages;
	  x1 = img->xsize * (xpage + 1) / xpages - 1;
	  y0 = img->ysize * ypage / ypages;
	  y1 = img->ysize * (ypage + 1) / ypages - 1;
	}
	else
	{
	  x0 = img->xsize * ypage / ypages;
	  x1 = img->xsize * (ypage + 1) / ypages - 1;
	  y0 = img->ysize * xpage / xpages;
	  y1 = img->ysize * (xpage + 1) / xpages - 1;
	}

	xtemp = header.HWResolution[0] * xinches / xpages;
	ytemp = header.HWResolution[1] * yinches / ypages;

	z = ImageZoomAlloc(img, x0, y0, x1, y1, xtemp, ytemp, Orientation & 1);

        cupsRasterWriteHeader(ras, &header);

	memset(row, blank, header.cupsBytesPerLine);

        if (header.cupsHeight > z->ysize && Orientation < 2)
	  for (y = header.cupsHeight - z->ysize; y > 0; y --)
	  {
	    if (cupsRasterWritePixels(ras, row, header.cupsBytesPerLine) <
	            header.cupsBytesPerLine)
	    {
	      fputs("ERROR: Unable to write raster data to driver!\n", stderr);
	      ImageClose(img);
	      exit(1);
	    }
          }

	for (y = z->ysize, yerr0 = z->ysize, yerr1 = 0, iy = 0, last_iy = -2;
             y > 0;
             y --)
	{
	  if (iy != last_iy)
	  {
	    if (header.cupsBitsPerColor == 8)
	    {
              if ((iy - last_iy) > 1)
        	ImageZoomFill(z, iy);
              ImageZoomFill(z, iy + z->yincr);
            }
            else
              ImageZoomQFill(z, iy);

            last_iy = iy;
	  }

    	  memset(row, blank, header.cupsBytesPerLine);

          switch (header.cupsBitsPerColor)
          {
            case 1 :
                if (Orientation == 1 || Orientation == 2)
                  bitoffset = header.cupsBitsPerPixel *
                              (header.cupsWidth - z->xsize);
                else
		  bitoffset = 0;

        	if (img->colorspace == IMAGE_RGB ||
		    img->colorspace == IMAGE_CMY)
		  bitoffset ++;

                bitmask = bitmasks[bitoffset & 7];
                dither  = FloydDither[y & 15];

                for (x = z->xsize * z->depth, rowptr = row + bitoffset / 8,
                         r0 = z->rows[z->row];
        	     x > 0;
        	     x --, r0 ++)
        	{
        	  if (*r0 > dither[x & 7])
        	    *rowptr ^= bitmask;

        	  if (img->colorspace == IMAGE_RGB ||
		      img->colorspace == IMAGE_CMY)
		  {
		    if (bitmask == 16)
        	      bitmask = 8;
		    else if (bitmask > 1)
        	      bitmask >>= 1;
        	    else
        	    {
        	      bitmask = 64;
        	      rowptr ++;
        	    }
		  }
		  else
		  {
		    if (bitmask > 1)
        	      bitmask >>= 1;
        	    else
        	    {
        	      bitmask = 128;
        	      rowptr ++;
        	    }
		  }
        	}

        	if (img->colorspace == IMAGE_CMYK)
        	  for (rowptr = row, x = header.cupsBytesPerLine; x > 0; x --, rowptr ++)
        	  {
        	    if ((*rowptr & 0xe0) == 0xe0)
        	      *rowptr ^= 0xf0;
        	    if ((*rowptr & 0x0e) == 0x0e)
        	      *rowptr ^= 0x0f;
        	  }
                break;
            case 2 :
                if (Orientation == 1 || Orientation == 2)
                  bitoffset = header.cupsBitsPerPixel *
                              (header.cupsWidth - z->xsize);
                else
		  bitoffset = 0;

        	if (img->colorspace == IMAGE_RGB ||
		    img->colorspace == IMAGE_CMY)
		  bitoffset += 2;

                bitmask = 0xc0 >> (bitoffset & 7);
                dither  = FloydDither[y & 15];

                for (x = z->xsize * z->depth, rowptr = row + bitoffset / 8,
                         r0 = z->rows[z->row];
        	     x > 0;
        	     x --, r0 ++)
        	{
        	  if (*r0 > dither[x & 7])
        	    *rowptr ^= (bitmask & onpixels[*r0]);
        	  else
        	    *rowptr ^= (bitmask & offpixels[*r0]);

        	  if (bitmask > 3)
        	    bitmask >>= 2;
        	  else
        	  {
		    if (img->colorspace == IMAGE_RGB ||
		        img->colorspace == IMAGE_CMY)
        	      bitmask = 0xc0;
		    else
        	      bitmask = 0x30;

        	    rowptr ++;
        	  }
        	}
                break;
            case 4 :
                if (Orientation == 1 || Orientation == 2)
                  bitoffset = header.cupsBitsPerPixel *
                              (header.cupsWidth - z->xsize);
                else
		  bitoffset = 0;

                bitmask = 0xf0 >> (bitoffset & 7);
                dither  = FloydDither[y & 15];

                for (x = z->xsize * z->depth, rowptr = row + bitoffset / 8,
                         r0 = z->rows[z->row];
        	     x > 0;
        	     x --, r0 ++)
        	{
        	  if (*r0 > dither[x & 7])
        	    *rowptr ^= (bitmask & onpixels[*r0]);
        	  else
        	    *rowptr ^= (bitmask & offpixels[*r0]);

        	  if (bitmask == 0xf0)
        	    bitmask = 0x0f;
        	  else
        	  {
        	    bitmask = 0xf0;
        	    rowptr ++;
        	  }
        	}
                break;
            case 8 :
                if (Orientation == 1 || Orientation == 2)
                  bitoffset = header.cupsBitsPerPixel *
                              (header.cupsWidth - z->xsize);
                else
		  bitoffset = 0;

                for (x = z->xsize * z->depth,
        		 rowptr = row + bitoffset,
        		 r0 = z->rows[z->row ^ 1], r1 = z->rows[z->row];
        	     x > 0;
        	     x --, rowptr ++, r0 ++, r1 ++)
        	  if (*r0 == *r1)
                    *rowptr = *r0;
        	  else
                    *rowptr = (*r0 * yerr0 + *r1 * yerr1) / z->ysize;
                break;
          }

	  if (cupsRasterWritePixels(ras, row, header.cupsBytesPerLine) <
	          header.cupsBytesPerLine)
	  {
            fputs("ERROR: Unable to write raster data to driver!\n", stderr);
	    ImageClose(img);
	    exit(1);
	  }

	  iy    += z->ystep;
	  yerr0 -= z->ymod;
	  yerr1 += z->ymod;
	  if (yerr0 <= 0)
	  {
            yerr0 += z->ysize;
            yerr1 -= z->ysize;
            iy    += z->yincr;
	  }
	}

	memset(row, blank, header.cupsBytesPerLine);

        if (header.cupsHeight > z->ysize && Orientation >= 2)
	  for (y = header.cupsHeight - z->ysize; y > 0; y --)
	  {
	    if (cupsRasterWritePixels(ras, row, header.cupsBytesPerLine) <
	            header.cupsBytesPerLine)
	    {
	      fputs("ERROR: Unable to write raster data to driver!\n", stderr);
	      ImageClose(img);
	      exit(1);
	    }
          }

        ImageZoomFree(z);
      }

 /*
  * Close files...
  */

  free(row);
  cupsRasterClose(ras);
  ImageClose(img);
  ppdClose(ppd);

  return (0);
}


/*
 * 'make_lut()' - Make a lookup table given gamma and brightness values.
 */

static void
make_lut(ib_t  *lut,		/* I - Lookup table */
	 int   colorspace,	/* I - Colorspace */
         float g,		/* I - Image gamma */
         float b)		/* I - Image brightness */
{
  int	i;			/* Looping var */
  int	v;			/* Current value */


  g = 1.0 / g;
  b = 1.0 / b;

  for (i = 0; i < 256; i ++)
  {
    if (colorspace < 0)
      v = 255.0 * b * (1.0 - pow(1.0 - (float)i / 255.0, g)) + 0.5;
    else
      v = 255.0 * (1.0 - b * (1.0 - pow((float)i / 255.0, g))) + 0.5;

    if (v < 0)
      *lut++ = 0;
    else if (v > 255)
      *lut++ = 255;
    else
      *lut++ = v;
  }
}


/*
 * End of "$Id: imagetoraster.c,v 1.10 1999/04/01 18:25:04 mike Exp $".
 */
