/*
 * "$Id: imagetoraster.c,v 1.6 1998/08/10 16:20:08 mike Exp $"
 *
 *   Image file to STIFF conversion program for espPrint, a collection
 *   of printer drivers.
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
 *   $Log: imagetoraster.c,v $
 *   Revision 1.6  1998/08/10 16:20:08  mike
 *   Fixed scaling problems.
 *
 *   Revision 1.5  1998/07/28  20:48:30  mike
 *   Updated size/page computation code to work properly.
 *
 *   Revision 1.4  1998/07/28  18:49:15  mike
 *   Fixed bug in rotation code - was rotating variable size media as well...
 *
 *   Revision 1.3  1998/04/02  21:06:54  mike
 *   Fixed problem with dither array (off by one).
 *
 *   Revision 1.2  1998/02/24  21:06:28  mike
 *   Updated to handle variable media sizes and adjust the output size
 *   accordingly.
 *
 *   Revision 1.1  1998/02/19  20:18:34  mike
 *   Initial revision
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

#include <pod.h>
#include <ppd.h>
#include <printutil.h>
#include <printstiff.h>
#include <errorcodes.h>

#include "image.h"


/*
 * Constants...
 */

#ifndef FALSE				/* Boolean stuff */
#  define FALSE 0
#  define TRUE  (!FALSE)
#endif /* !FALSE */


/*
 * Globals...
 */

int	Verbosity = 0;
int	FloydDither[16][16] =
{
  { 0, 128, 32, 160, 8, 136, 40, 168, 2, 130, 34, 162, 10, 138, 42, 170 },
  { 192, 64, 224, 96, 200, 72, 232, 104, 194, 66, 226, 98, 202, 74, 234, 106 },
  { 48, 176, 16, 144, 56, 184, 24, 152, 50, 178, 18, 146, 58, 186, 26, 154 },
  { 240, 112, 208, 80, 248, 120, 216, 88, 242, 114, 210, 82, 250, 122, 218, 90 },
  { 12, 140, 44, 172, 4, 132, 36, 164, 14, 142, 46, 174, 6, 134, 38, 166 },
  { 204, 76, 236, 108, 196, 68, 228, 100, 206, 78, 238, 110, 198, 70, 230, 102 },
  { 60, 188, 28, 156, 52, 180, 20, 148, 62, 190, 30, 158, 54, 182, 22, 150 },
  { 252, 124, 220, 92, 244, 116, 212, 84, 254, 126, 222, 94, 246, 118, 214, 86 },
  { 3, 131, 35, 163, 11, 139, 43, 171, 1, 129, 33, 161, 9, 137, 41, 169 },
  { 195, 67, 227, 99, 203, 75, 235, 107, 193, 65, 225, 97, 201, 73, 233, 105 },
  { 51, 179, 19, 147, 59, 187, 27, 155, 49, 177, 17, 145, 57, 185, 25, 153 },
  { 243, 115, 211, 83, 251, 123, 219, 91, 241, 113, 209, 81, 249, 121, 217, 89 },
  { 15, 143, 47, 175, 7, 135, 39, 167, 13, 141, 45, 173, 5, 133, 37, 165 },
  { 207, 79, 239, 111, 199, 71, 231, 103, 205, 77, 237, 109, 197, 69, 229, 101 },
  { 63, 191, 31, 159, 55, 183, 23, 151, 61, 189, 29, 157, 53, 181, 21, 149 },
  { 254, 127, 223, 95, 247, 119, 215, 87, 253, 125, 221, 93, 245, 117, 213, 85 }
};


/*
 * Local functions...
 */

static void	usage(void);
static void	make_lut(ib_t *, int, float, float, float, float);


/*
 * 'main()' - Main entry...
 */

int
main(int  argc,		/* I - Number of command-line arguments */
     char *argv[])	/* I - Command-line arguments */
{
  int			i;		/* Looping var */
  char			*opt,		/* Current option character */
			*infile,	/* Input filename */
			*outfile,	/* Output filename */
			*printer;	/* Printer */
  STStream		*st;		/* output image stream */
  PSTImageHeader	header;		/* Output image header */
  PDInfoStruct		*info;		/* POD info */
  PDStatusStruct	*status;	/* POD status */
  time_t		mod_time;	/* Modification time */
  PDSizeTableStruct	*size;		/* Page size */
  image_t		*img;		/* Image to print */
  izoom_t		*z;		/* ImageZoom buffer */
  int			bits,		/* Bits-per-channel */
			icolorspace,	/* Image colorspace */
                        scolorspace,	/* STIFF colorspace */
                        variable,	/* Non-zero if page is variable size */
			width,		/* Width (pixels) of each image */
			height,		/* Height (pixels) of each image */
			xdpi,		/* Horizontal resolution */
			ydpi,		/* Vertical resolution */
			copies;		/* Number of copies */
  float			xzoom,		/* X zoom facter */
			yzoom,		/* Y zoom facter */
			xprint,
			yprint,
			xinches,
			yinches;
  float			xsize,
			ysize,
			xtemp,
			ytemp;
  int			xpages,		/* X pages */
			ypages,		/* Y pages */
			xpage,		/* Current x page */
			ypage;		/* Current y page */
  int			level,		/* PostScript "level" */
			flip,		/* Flip */
			rotation,	/* Rotation */
			landscape,	/* Landscape orientation? */
			xppi, yppi;	/* Pixels-per-inch */
  float			profile[6];
  float			gammaval[4];
  int			brightness[4];
  int			x0, y0,		/* Corners of the page in image coords */
			x1, y1;
  ib_t			*row,
			*rowptr,
			*r0,
			*r1,
			bitmask;
  int			bwidth,
			bpp,
			bitoffset;
  int			x,
			y,
			iy,
			last_iy,
			yerr0,
			yerr1,
			blank,
			*dither;
  int			hue, sat;
  ib_t			luts[1024],
			onpixels[256],
			offpixels[256];
  static ib_t		bitmasks[8] = { 128, 64, 32, 16, 8, 4, 2, 1 };


 /*
  * Process any command-line args...
  */

  infile        = NULL;
  outfile       = NULL;
  printer       = NULL;
  level         = 0;
  rotation      = -1;
  xzoom = yzoom = 0.0;
  flip          = 0;
  xppi          = 0;
  yppi          = 0;
  hue           = 0;
  sat           = 100;
  landscape     = 0;
  profile[0]    = 1.0;
  profile[1]    = 1.0;
  profile[2]    = 1.0;
  profile[3]    = 1.0;
  profile[4]    = 1.0;
  profile[5]    = 1.0;
  gammaval[0]   = 0.0;
  gammaval[1]   = 0.0;
  gammaval[2]   = 0.0;
  gammaval[3]   = 0.0;
  brightness[0] = 100;
  brightness[1] = 100;
  brightness[2] = 100;
  brightness[3] = 100;
  bits          = 1;
  scolorspace   = ST_TYPE_K;
  width         = 850;
  height        = 1100;
  variable      = 1;
  xdpi          = 100;
  ydpi          = 100;
  copies        = 1;

  for (i = 1; i < argc; i ++)
    if (argv[i][0] == '-')
      for (opt = argv[i] + 1; *opt != '\0'; opt ++)
        switch (*opt)
        {
          case 'L' : /* Log file */
              i ++;
              if (i >= argc)
                usage();

              freopen(argv[i], "w", stderr);
              break;

          case 'O' : /* Output file */
              i ++;
              if (i >= argc || outfile != NULL)
                usage();

              outfile = argv[i];
              break;

          case 'P' : /* Specify the printer name */
              i ++;
              if (i >= argc)
                usage();

              printer = argv[i];

	     /*
	      * Open the POD database files and get the printer definition record.
	      */

	      if (PDLocalReadInfo(printer, &info, &mod_time) < 0)
	      {
		fprintf(stderr, "img2stiff: Could not open required POD database files for printer \'%s\'.\n", 
        		printer);
		fprintf(stderr, "img2stiff: Are you sure all required POD files are properly installed?\n");

		PDPerror("img2stiff");
		exit(ERR_BAD_ARG);
	      };

	      status   = info->active_status;
	      size     = PDFindPageSize(info, PD_SIZE_CURRENT);
	      variable = (status->media_size == PD_SIZE_VARIABLE);

	     /*
	      * Figure out what we need to generate...
	      */

              width  = size->horizontal_addr;
              height = size->vertical_addr;
              xdpi   = info->horizontal_resolution;
              ydpi   = info->vertical_resolution;

              memcpy(profile, status->color_profile, sizeof(profile));

	      switch (PD_GET_DEPTH_CODE(status->number_of_colors))
	      {
	        case PD_DATA_DEPTH1 :
	            bits = 1;
	            break;
	        case PD_DATA_DEPTH2 :
	            bits = 2;
	            break;
	        case PD_DATA_DEPTH4 :
	            bits = 4;
	            break;
	        case PD_DATA_DEPTH8 :
	            bits = 8;
	            break;
	      };

	      switch (PD_GET_COLORSPACE_CODE(status->number_of_colors))
	      {
	        case PD_DATA_K :
	            scolorspace = ST_TYPE_K;
	            break;
	        case PD_DATA_CMY :
	        case PD_DATA_YMC :
	            scolorspace = ST_TYPE_CMY;
	            break;
	        case PD_DATA_CMYK :
	        case PD_DATA_YMCK :
	        case PD_DATA_KCMY :
	            scolorspace = ST_TYPE_CMYK;
	            break;
	        case PD_DATA_W :
	            scolorspace = ST_TYPE_W;
	            break;
	        case PD_DATA_RGB :
	            scolorspace = ST_TYPE_RGB;
	            break;
	      };
              break;

          case 'B' : /* Bits per pixel */
              i ++;
              if (i >= argc)
                usage();

              bits = atoi(argv[i]);

              if (bits != 1 && bits != 2 && bits != 4 && bits != 8)
                usage();
              break;

          case 'F' : /* Format */
              i ++;
              if (i >= argc)
                usage();
              break;

          case 'C' : /* Colorspace */
              i ++;
              if (i >= argc)
                usage();

              if (strcasecmp(argv[i], "k") == 0)
                scolorspace = ST_TYPE_K;
              else if (strcasecmp(argv[i], "w") == 0)
                scolorspace = ST_TYPE_W;
              else if (strcasecmp(argv[i], "rgb") == 0)
                scolorspace = ST_TYPE_RGB;
              else if (strcasecmp(argv[i], "cmy") == 0)
                scolorspace = ST_TYPE_CMY;
              else if (strcasecmp(argv[i], "cmyk") == 0)
                scolorspace = ST_TYPE_CMYK;
              else if (strcasecmp(argv[i], "kcmy") == 0)
                scolorspace = ST_TYPE_CMYK;
              else if (strcasecmp(argv[i], "ymc") == 0)
                scolorspace = ST_TYPE_CMY;
              else if (strcasecmp(argv[i], "ymck") == 0)
                scolorspace = ST_TYPE_CMYK;
              else
                usage();
              break;

          case 'X' : /* X resolution */
              i ++;
              if (i >= argc)
                usage();

              xdpi = atoi(argv[i]);
              break;

          case 'Y' : /* Y resolution */
              i ++;
              if (i >= argc)
                usage();

              ydpi = atoi(argv[i]);
              break;

          case 'R' : /* Resolution */
              i ++;
              if (i >= argc)
                usage();

              xdpi = ydpi = atoi(argv[i]);
              break;

          case 'W' : /* Pixel width */
              i ++;
              if (i >= argc)
                usage();

              width    = atoi(argv[i]);
	      variable = 0;
              break;

          case 'H' : /* Pixel height */
              i ++;
              if (i >= argc)
                usage();

              height   = atoi(argv[i]);
	      variable = 0;
              break;

          case 'M' : /* Model (PostScript Level) */
              i ++;
              if (i >= argc)
                usage();
              break;

          case 'l' : /* Landscape */
              landscape = 1;
              break;

          case 'f' : /* Flip the image */
              flip = 1;
              break;

          case 'r' : /* Rotate */
              i ++;
              if (i >= argc)
                usage();

              rotation = (atoi(argv[i]) % 180) / 90;
              break;

          case 'z' : /* Page zoom */
              i ++;
              if (i >= argc)
                usage();

              if (sscanf(argv[i], "%f,%f", &xzoom, &yzoom) == 1)
                yzoom = xzoom;

              if (strchr(argv[i], '.') == NULL)
              {
                xzoom *= 0.01;
                yzoom *= 0.01;
              };
              break;

          case 'p' : /* Scale to pixels/inch */
              i ++;
              if (i >= argc)
                usage();

              if (sscanf(argv[i], "%d,%d", &xppi, &yppi) == 1)
                yppi = xppi;
              break;

          case 'n' : /* Number of copies */
              i ++;
              if (i >= argc)
                usage();

              copies = atoi(argv[i]);
              break;

          case 'D' : /* Produce debugging messages */
              Verbosity ++;
              break;

          case 'h' : /* Color Hue */
              i ++;
              if (i >= argc)
                usage();

              hue = atoi(argv[i]);
              break;

          case 's' : /* Color Saturation */
              i ++;
              if (i >= argc)
                usage();

              sat = atoi(argv[i]);
              break;

          case 'g' :	/* Gamma correction */
	      i ++;
	      if (i < argc)
	        switch (sscanf(argv[i], "%f,%f,%f,%f", gammaval + 0,
	                       gammaval + 1, gammaval + 2, gammaval + 3))
	        {
	          case 1 :
	              gammaval[1] = gammaval[0];
	          case 2 :
	              gammaval[2] = gammaval[1];
	              gammaval[3] = gammaval[1];
	              break;
	        };
	      break;

          case 'b' :	/* Brightness */
	      i ++;
	      if (i < argc)
	        switch (sscanf(argv[i], "%d,%d,%d,%d", brightness + 0,
	                brightness + 1, brightness + 2, brightness + 3))
	        {
	          case 1 :
	              brightness[1] = brightness[0];
	          case 2 :
	              brightness[2] = brightness[1];
	              brightness[3] = brightness[1];
	              break;
	        };
	      break;

          default :
              usage();
              break;
        }
    else if (infile != NULL)
      usage();
    else
      infile = argv[i];

  if (Verbosity)
  {
    fputs("img2stiff: Command-line args are:", stderr);
    for (i = 1; i < argc; i ++)
      fprintf(stderr, " %s", argv[i]);
    fputs("\n", stderr);
  };

 /*
  * Check for necessary args...
  */

  if (infile == NULL)
    usage();

 /*
  * Figure out the image colorspace...
  */

  blank = 0;

  switch (scolorspace)
  {
    case ST_TYPE_K :
        icolorspace = IMAGE_BLACK;
        break;
    case ST_TYPE_CMY :
        icolorspace = IMAGE_CMY;
        break;
    case ST_TYPE_CMYK :
        if (bits == 1)
          icolorspace = IMAGE_CMY;
        else
          icolorspace = IMAGE_CMYK;
        break;
    case ST_TYPE_W :
        if (bits == 8)
          icolorspace = IMAGE_WHITE;
	else
          icolorspace = IMAGE_BLACK;

        blank = 255;
        break;
    case ST_TYPE_RGB :
        if (bits == 8)
          icolorspace = IMAGE_RGB;
	else
          icolorspace = IMAGE_CMY;

        blank = 255;
        break;
  };

 /*
  * Open the input image to print...
  */

  if ((img = ImageOpen(infile, icolorspace, icolorspace, sat, hue)) == NULL)
    exit (ERR_FILE_CONVERT);

  if (Verbosity)
    fprintf(stderr, "img2stiff: Original image is %dx%d pixels...\n",
            img->xsize, img->ysize);

 /*
  * Scale as necessary...
  */

  xprint = (float)width / (float)xdpi;
  yprint = (float)height / (float)ydpi;

  if (rotation >= 0 && landscape)
    rotation = 1 - (rotation & 1);

  if (xzoom == 0.0 && xppi == 0)
  {
    xppi = img->xppi;
    yppi = img->yppi;
  };

  if (xppi > 0)
  {
   /*
    * Scale the image as neccesary to match the desired pixels-per-inch.
    */
    

    if (rotation == 0)
    {
      xinches = (float)img->xsize / (float)xppi;
      yinches = (float)img->ysize / (float)yppi;
    }
    else if (rotation == 1)
    {
      xinches = (float)img->ysize / (float)yppi;
      yinches = (float)img->xsize / (float)xppi;
    }
    else
    {
      xinches  = (float)img->xsize / (float)xppi;
      yinches  = (float)img->ysize / (float)yppi;
      rotation = 0;

      if (xinches > xprint && xinches <= yprint)
      {
	xinches  = (float)img->ysize / (float)yppi;
	yinches  = (float)img->xsize / (float)xppi;
        rotation = 1;
      };
    };
  }
  else
  {
   /*
    * Scale percentage of page size...
    */

    if (rotation == 0)
    {
      xsize = xprint * xzoom;
      ysize = xsize * img->ysize / img->xsize;

      if (ysize > (yprint * yzoom))
      {
        ysize = yprint * yzoom;
        xsize = ysize * img->xsize / img->ysize;
      };
    }
    else if (rotation == 1)
    {
      ysize = xprint * yzoom;
      xsize = ysize * img->xsize / img->ysize;

      if (xsize > (yprint * xzoom))
      {
        xsize = yprint * xzoom;
        ysize = xsize * img->ysize / img->xsize;
      };
    }
    else
    {
      xsize = xprint * xzoom;
      ysize = xsize * img->ysize / img->xsize;

      if (ysize > (yprint * yzoom))
      {
        ysize = yprint * yzoom;
        xsize = ysize * img->xsize / img->ysize;
      };

      ytemp = xprint * yzoom;
      xtemp = ytemp * img->xsize / img->ysize;

      if (xtemp > (yprint * xzoom))
      {
        xtemp = yprint * xzoom;
        ytemp = xtemp * img->ysize / img->xsize;
      };

      if ((xsize * ysize) < (xtemp * ytemp))
      {
        xsize = xtemp;
        ysize = ytemp;

        rotation = 1;
      }
      else
        rotation = 0;
    };

    if (rotation)
    {
      xinches = ysize;
      yinches = xsize;
    }
    else
    {
      xinches = xsize;
      yinches = ysize;
    };
  };

  xpages = ceil(xinches / xprint);
  ypages = ceil(yinches / yprint);

  if (Verbosity)
  {
    fprintf(stderr, "img2stiff: Page size is %.1fx%.1f inches\n", xprint, yprint);
    fprintf(stderr, "img2stiff: Output image is rotated %d degrees, %.1fx%.1f inches.\n",
            rotation * 90, xinches, yinches);
    fprintf(stderr, "img2stiff: Output image to %dx%d pages...\n", xpages, ypages);
  };

 /*
  * Create the output stream...
  */

  if (outfile == NULL)
    st = STOpen(1, ST_WRITE);
  else
    st = STOpen(open(outfile, O_WRONLY | O_TRUNC | O_CREAT, 0666), ST_WRITE);

  if (st == NULL)
  {
    fprintf(stderr, "img2stiff: Unable to create STIFF output to %s - %s\n",
            outfile == NULL ? "(stdout)" : outfile, strerror(errno));
    exit(ERR_TRANSMISSION);
  };

 /*
  * Create the lookup tables...
  */

  switch (img->colorspace)
  {
    case IMAGE_WHITE :
    case IMAGE_BLACK :
        make_lut(luts, img->colorspace, gammaval[0], 100.0 / brightness[0],
                 profile[PD_PROFILE_KG], profile[PD_PROFILE_KD]);
        break;
    case IMAGE_RGB :
    case IMAGE_CMY :
        make_lut(luts + 0, img->colorspace, gammaval[1], 100.0 / brightness[1],
                 profile[PD_PROFILE_BG], profile[PD_PROFILE_CD]);
        make_lut(luts + 1, img->colorspace, gammaval[2], 100.0 / brightness[2],
                 profile[PD_PROFILE_BG], profile[PD_PROFILE_MD]);
        make_lut(luts + 2, img->colorspace, gammaval[3], 100.0 / brightness[3],
                 profile[PD_PROFILE_BG], profile[PD_PROFILE_YD]);
        break;
    case IMAGE_CMYK :
        make_lut(luts + 0, img->colorspace, gammaval[1], 100.0 / brightness[1],
                 profile[PD_PROFILE_BG], profile[PD_PROFILE_CD]);
        make_lut(luts + 1, img->colorspace, gammaval[2], 100.0 / brightness[2],
                 profile[PD_PROFILE_BG], profile[PD_PROFILE_MD]);
        make_lut(luts + 2, img->colorspace, gammaval[3], 100.0 / brightness[3],
                 profile[PD_PROFILE_BG], profile[PD_PROFILE_YD]);
        make_lut(luts + 3, img->colorspace, gammaval[0], 100.0 / brightness[0],
                 profile[PD_PROFILE_KG], profile[PD_PROFILE_KD]);
        break;
  };

  onpixels[0]  = 0;
  offpixels[0] = 0;

  switch (bits)
  {
    case 2 :
	for (i = 1; i < 64; i ++)
	  onpixels[i] = 0;
	for (; i < 170; i ++)
	  onpixels[i] = 1;
	for (; i < 234; i ++)
	  onpixels[i] = 2;
	for (; i < 256; i ++)
	  onpixels[i] = 3;

	for (i = 1; i < 117; i ++)
	  offpixels[i] = 1;
	for (; i < 202; i ++)
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
        };
        break;
  };

 /*
  * Output the pages...
  */

  header.type            = scolorspace;
  header.plane           = ST_PLANE_PACKED;
  header.resUnit	 = PST_RES_UNIT_INCH;
  header.xRes		 = xdpi;
  header.yRes		 = ydpi;
  header.thresholding	 = PST_THRESHOLD_NONE;
  header.compression	 = PST_COMPRESSION_NONE;
  header.pageNumbers[0]	 = 0;
  header.pageNumbers[1]	 = xpages * ypages * copies;
  header.dateTime	 = NULL;
  header.hostComputer	 = NULL;
  header.software	 = "img2stiff - ESP Print " SVERSION;
  header.docName	 = infile;
  header.targetPrinter	 = printer;
  header.driverOptions	 = NULL;
  header.bitsPerSample   = bits;
  header.samplesPerPixel = ImageGetDepth(img);

  if ((bits == 1 || bits == 2) && header.samplesPerPixel == 3)
    header.samplesPerPixel = 4;

  if (variable)
  {
    width  = xdpi * xinches / xpages;
    height = ydpi * yinches / ypages;

    fprintf(stderr, "img2stiff: Set variable size to %dx%d pixels...\n",
            width, height);
  };

  bpp    = header.bitsPerSample * header.samplesPerPixel;
  bwidth = (width * bpp + 7) / 8;

  header.width    = width;
  header.height   = height;
  header.imgbytes = header.height * bwidth;

  row = (ib_t *)malloc(bwidth);

  for (i = 0; i < copies; i ++)
    for (xpage = 0; xpage < xpages; xpage ++)
      for (ypage = 0; ypage < ypages; ypage ++)
      {
	if (rotation == 0)
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
	};

	xtemp = xdpi * xinches / xpages;
	ytemp = ydpi * yinches / ypages;

	z = ImageZoomAlloc(img, x0, y0, x1, y1, xtemp, ytemp, rotation);

	header.pageNumbers[0] ++;
	PSTWriteImageHeader(st, &header,
                            header.pageNumbers[0] == header.pageNumbers[1]);

	if (Verbosity)
	{
	  fprintf(stderr, "img2stiff: Starting page %d\n",
        	  header.pageNumbers[0]);
	  fprintf(stderr, "img2stiff: type = %04x, bitsPerSample = %d, samplesPerPixel = %d\n",
        	  header.type, header.bitsPerSample, header.samplesPerPixel);
	  fprintf(stderr, "img2stiff: xRes = %d, yRes = %d, width = %d, height = %d\n",
        	  header.xRes, header.yRes, header.width, header.height);
	  fprintf(stderr, "img2stiff: (x0, y0) = (%d, %d), (x1, y1) = (%d, %d)\n",
	          x0, y0, x1, y1);
	  fprintf(stderr, "img2stiff: image area = %.0fx%.0f pixels\n",
	          xtemp, ytemp);
	};

	memset(row, blank, bwidth);

        if (header.height > z->ysize)
	  for (y = (header.height - z->ysize) / 2; y > 0; y --)
	  {
	    if (Verbosity > 1)
	      fprintf(stderr, "img2stiff: blanking line %d\n", y);

	    if (STWrite(st, row, bwidth) < bwidth)
	    {
	      ImageClose(img);
	      exit(ERR_TRANSMISSION);
	    };
          };

	for (y = z->ysize, yerr0 = z->ysize, yerr1 = 0, iy = 0, last_iy = -2;
             y > 0;
             y --)
	{
	  if (Verbosity > 1)
	    fprintf(stderr, "img2stiff: generating line %d\n", y);

	  if (iy != last_iy)
	  {
	    if (bits == 8)
	    {
              if ((iy - last_iy) > 1)
        	ImageZoomFill(z, iy, luts);
              ImageZoomFill(z, iy + z->yincr, luts);
            }
            else
              ImageZoomQFill(z, iy, luts);

            last_iy = iy;
	  };

          switch (bits)
          {
            case 1 :
    		memset(row, 0, bwidth);

                bitoffset = header.samplesPerPixel *
                            ((header.width - z->xsize) / 2);
                bitmask   = bitmasks[bitoffset & 7];
                dither    = FloydDither[y & 15];

                for (x = z->xsize * z->depth, rowptr = row + bitoffset / 8,
                         r0 = z->rows[z->row];
        	     x > 0;
        	     x --, r0 ++)
        	{
        	  if (*r0 > dither[x & 7])
        	    *rowptr |= bitmask;

        	  if ((bitmask == 32 || bitmask == 2) &&
        	      (icolorspace == IMAGE_RGB || icolorspace == IMAGE_CMY))
        	    bitmask >>= 1;

        	  if (bitmask > 1)
        	    bitmask >>= 1;
        	  else
        	  {
        	    bitmask = 128;
        	    rowptr ++;
        	  };
        	};

        	if (scolorspace == ST_TYPE_CMYK)
        	  for (rowptr = row, x = bwidth; x > 0; x --, rowptr ++)
        	  {
        	    if ((*rowptr & 0xe0) == 0xe0)
        	      *rowptr ^= 0xf0;
        	    if ((*rowptr & 0x0e) == 0x0e)
        	      *rowptr ^= 0x0f;
        	  }
		else if (blank == 255)
        	  for (rowptr = row, x = bwidth; x > 0; x --, rowptr ++)
         	    *rowptr = ~*rowptr;
                break;
            case 2 :
    		memset(row, 0, bwidth);

                bitoffset = 2 * header.samplesPerPixel *
                            ((header.width - z->xsize) / 2);
                bitmask   = 0xc0 >> (bitoffset & 7);
                dither    = FloydDither[y & 15];

                for (x = z->xsize * z->depth, rowptr = row + bitoffset / 8,
                         r0 = z->rows[z->row];
        	     x > 0;
        	     x --, r0 ++)
        	{
        	  if (*r0 > dither[x & 7])
        	    *rowptr |= (bitmask & onpixels[*r0]);
        	  else
        	    *rowptr |= (bitmask & offpixels[*r0]);

        	  if (bitmask > 3)
        	    bitmask >>= 2;
        	  else
        	  {
        	    bitmask = 0xc0;
        	    rowptr ++;
        	  };
        	};

		if (blank == 255)
        	  for (rowptr = row, x = bwidth; x > 0; x --, rowptr ++)
         	    *rowptr = ~*rowptr;
                break;
            case 4 :
    		memset(row, 0, bwidth);

                bitoffset = 4 * header.samplesPerPixel *
                            ((header.width - z->xsize) / 2);
                bitmask   = 0xf0 >> (bitoffset & 7);
                dither    = FloydDither[y & 15];

                for (x = z->xsize * z->depth, rowptr = row + bitoffset / 8,
                         r0 = z->rows[z->row];
        	     x > 0;
        	     x --, r0 ++)
        	{
        	  if (*r0 > dither[x & 7])
        	    *rowptr |= (bitmask & onpixels[*r0]);
        	  else
        	    *rowptr |= (bitmask & offpixels[*r0]);

        	  if (bitmask == 0xf0)
        	    bitmask = 0x0f;
        	  else
        	  {
        	    bitmask = 0xf0;
        	    rowptr ++;
        	  };
        	};

		if (blank == 255)
        	  for (rowptr = row, x = bwidth; x > 0; x --, rowptr ++)
         	    *rowptr = ~*rowptr;
                break;
            case 8 :
                bitoffset = header.samplesPerPixel *
                            ((header.width - z->xsize) / 2);

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
          };

	  if (STWrite(st, row, bwidth) < bwidth)
	  {
	    ImageClose(img);
	    exit(ERR_TRANSMISSION);
	  };

	  iy    += z->ystep;
	  yerr0 -= z->ymod;
	  yerr1 += z->ymod;
	  if (yerr0 <= 0)
	  {
            yerr0 += z->ysize;
            yerr1 -= z->ysize;
            iy    += z->yincr;
	  };
	};

	memset(row, blank, bwidth);

        if (header.height > z->ysize)
	  for (y = (header.height + z->ysize) / 2; y < header.height; y ++)
	  {
	    if (Verbosity > 1)
	      fprintf(stderr, "img2stiff: blanking line %d\n", y);

	    if (STWrite(st, row, bwidth) < bwidth)
	    {
	      ImageClose(img);
	      exit(ERR_TRANSMISSION);
	    };
          };

        ImageZoomFree(z);

	if (Verbosity)
	  fputs("img2stiff: done with this page...\n", stderr);
      };

  ImageClose(img);
  free(row);

  STClose(st);

  if (Verbosity)
    fputs("img2stiff: Exiting with no errors!\n", stderr);

  return (NO_ERROR);
}


/*
 * 'usage()' - Print usage message and exit.
 */

static void
usage(void)
{    
  fputs("usage: img2stiff -P <printer-name> <filename> [-D] [-L <log-file>]\n", stderr);
  fputs("              [-O <output-file>] [-b <brightness-val(s)>] [-f]\n", stderr);
  fputs("              [-g <gamma-val(s)>] [-h <hue>] [-l] [-p <ppi>]\n", stderr);
  fputs("              [-r <rotation>] [-s <saturation]\n", stderr);

  exit(ERR_BAD_ARG);
}


/*
 * 'make_lut()' - Make a lookup table given gamma, brightness, and color
 *                profile values.
 */

static void
make_lut(ib_t  *lut,		/* I - Lookup table */
	 int   colorspace,	/* I - Colorspace */
         float ig,		/* I - Image gamma */
         float ib,		/* I - Image brightness */
         float pg,		/* I - Profile gamma */
         float pd)		/* I - Profile ink density */
{
  int	i;			/* Looping var */
  float	v;			/* Current value */


  if (ig == 0.0)
    ig = LutDefaultGamma();

  ig = 1.0 / ig;
  pg = 1.0 / pg;

  for (i = 0; i < 256; i ++)
  {
    if (colorspace < 0)
      v = 1.0 - pow(1.0 - (float)i / 255.0, ig);
    else
      v = 1.0 - pow((float)i / 255.0, ig);

    v = pd * pow(v * ib, pg);

    if (colorspace < 0)
    {
      *lut = 255.0 * v + 0.5;
      lut -= colorspace;
    }
    else
    {
      *lut = 255.5 - 255.0 * v;
      lut += colorspace;
    };
  };
}


/*
 * End of "$Id: imagetoraster.c,v 1.6 1998/08/10 16:20:08 mike Exp $".
 */
