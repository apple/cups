/*
 * "$Id: imagetops.c,v 1.1 1998/02/19 20:43:33 mike Exp $"
 *
 *   Image file to PostScript conversion program for espPrint, a collection
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
 *   $Log: imagetops.c,v $
 *   Revision 1.1  1998/02/19 20:43:33  mike
 *   Initial revision
 *
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


/*
 * Local functions...
 */

static void	usage(void);
static void	ps_hex(FILE *, ib_t *, int);
static void	ps_ascii85(FILE *, ib_t *, int, int);
static void	make_transfer_function(char *, float, float, float, float);
static void	print_prolog(FILE *, int, float *, int *, float *);


/*
 * 'main()' - Main entry...
 */

void
main(int  argc,		/* I - Number of command-line arguments */
     char *argv[])	/* I - Command-line arguments */
{
  int			i;		/* Looping var */
  FILE			*out;		/* Output file */
  gzFile		ppdfile;	/* PPD file */
  char			*pslevel,	/* Level of PostScript supported */
			*opt,		/* Current option character */
			*infile,	/* Input filename */
			*outfile,	/* Output filename */
			*printer;	/* Printer */
  PDInfoStruct		*info;		/* POD info */
  PDStatusStruct	*status;	/* POD status */
  time_t		mod_time;	/* Modification time */
  PDSizeTableStruct	*size;		/* Page size */
  image_t		*img;		/* Image to print */
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
  float			gammaval[4];
  int			brightness[4];
  int			x0, y0,		/* Corners of the page in image coords */
			x1, y1;
  ib_t			*row;
  int			y;
  int			colorspace;
  int			hue, sat;
  int			out_offset,
			out_length;


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
  gammaval[0]   = 0.0;
  gammaval[1]   = 0.0;
  gammaval[2]   = 0.0;
  gammaval[3]   = 0.0;
  brightness[0] = 100;
  brightness[1] = 100;
  brightness[2] = 100;
  brightness[3] = 100;

  for (i = 1; i < argc; i ++)
    if (argv[i][0] == '-')
      for (opt = argv[i] + 1; *opt != '\0'; opt ++)
        switch (*opt)
        {
          case 'P' : /* Specify the printer name */
              i ++;
              if (i >= argc)
                usage();

              printer = argv[i];
              break;

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

          case 'B' : /* Bits per pixel */
          case 'C' : /* Colorspace */
          case 'F' : /* Format */
          case 'H' : /* Pixel height */
          case 'R' : /* Resolution */
          case 'W' : /* Pixel width */
          case 'X' : /* Horizontal resolution */
          case 'Y' : /* Vertical resolution */
              i ++;
              if (i >= argc)
                usage();
              break;

          case 'M' : /* Model (PostScript Level) */
              i ++;
              if (i >= argc)
                usage();

              level = atoi(argv[i]);
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
    fputs("img2ps: Command-line args are:", stderr);
    for (i = 1; i < argc; i ++)
      fprintf(stderr, " %s", argv[i]);
    fputs("\n", stderr);
  };

 /*
  * Check for necessary args...
  */

  if (printer == NULL ||
      infile == NULL)
    usage();

 /*
  * Open the POD database files and get the printer definition record.
  */
  
  if (PDLocalReadInfo(printer, &info, &mod_time) < 0)
  {
    fprintf(stderr, "img2ps: Could not open required POD database files for printer \'%s\'.\n", 
            printer);
    fprintf(stderr, "        Are you sure all required POD files are properly installed?\n");

    PDPerror("img2ps");
    exit(ERR_BAD_ARG);
  };

  status = info->active_status;
  size   = PDFindPageSize(info, PD_SIZE_CURRENT);

 /*
  * Figure out what we need to generate...
  */

  if (strncasecmp(info->printer_class, "Color", 5) == 0)
    colorspace = IMAGE_RGB;
  else
    colorspace = IMAGE_WHITE;

 /*
  * See if we have a level 1 or 2 printer...
  */

  if (level == 0 && info->ppd_path[0] != '\0')
    if ((ppdfile = gzopen(info->ppd_path, "r")) != NULL)
    {
      if ((pslevel = PPDGetCap(ppdfile, "LanguageLevel")) != NULL)
        level = atoi(pslevel);
      gzclose(ppdfile);
    };

  if (strstr(info->printer_class, "Raster") != NULL)
    level = 2;

 /*
  * Open the input image to print...
  */

  if ((img = ImageOpen(infile, colorspace, IMAGE_WHITE, sat, hue)) == NULL)
    exit (ERR_FILE_CONVERT);

  colorspace = img->colorspace;

  if (Verbosity)
    fprintf(stderr, "img2ps: Original image is %dx%d pixels...\n",
            img->xsize, img->ysize);

 /*
  * Scale as necessary...
  */

  if (landscape)
  {
    xprint = (float)size->vertical_addr / (float)info->vertical_resolution;
    yprint = (float)size->horizontal_addr / (float)info->horizontal_resolution;
  }
  else
  {
    xprint = (float)size->horizontal_addr / (float)info->horizontal_resolution;
    yprint = (float)size->vertical_addr / (float)info->vertical_resolution;
  };

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
    
    xinches = (float)img->xsize / (float)xppi;
    yinches = (float)img->ysize / (float)yppi;

    if (rotation == 0)
    {
      xzoom = xinches / xprint;
      yzoom = yinches / yprint;
    }
    else if (rotation == 1)
    {
      xzoom = yinches / xprint;
      yzoom = xinches / yprint;
    }
    else
    {
      xzoom = xinches / xprint;
      yzoom = yinches / yprint;
      rotation = 0;

      if (xinches > xprint && xinches <= yprint)
      {
	xzoom = yinches / xprint;
	yzoom = xinches / yprint;
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
      xsize = xprint * xzoom;
      ysize = xsize * img->xsize / img->ysize;
      if (ysize > (yprint * yzoom))
      {
        ysize = yprint * yzoom;
        xsize = ysize * img->ysize / img->xsize;
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

    xinches = xsize;
    yinches = ysize;
  };

  if (rotation == 0)
  {
    xpages = ceil(xinches / xprint);
    ypages = ceil(yinches / yprint);
  }
  else
  {
    xpages = ceil(xinches / yprint);
    ypages = ceil(yinches / xprint);
  };

  if (Verbosity)
  {
    fprintf(stderr, "img2ps: Page size is %.1fx%.1f inches\n", xprint, yprint);
    fprintf(stderr, "img2ps: Output image is rotated %d degrees, %.1fx%.1f inches.\n",
            rotation * 90, xinches, yinches);
    fprintf(stderr, "img2ps: Output image to %dx%d pages...\n", xpages, ypages);
  };

 /*
  * Create the output stream...
  */

  if (outfile == NULL)
    out = stdout;
  else
    out = fopen(outfile, "w");

  if (out == NULL)
  {
    fprintf(stderr, "img2ps: Unable to create PostScript output to %s - %s\n",
            outfile == NULL ? "(stdout)" : outfile, strerror(errno));
    exit(ERR_TRANSMISSION);
  };

 /*
  * Output the pages...
  */

  fputs("%!PS-Adobe-3.0\n", out);
  fprintf(out, "%%%%BoundingBox: %.1f %.1f %.1f %.1f\n",
          72.0 * size->left_margin, 
          72.0 * (size->length - yprint - size->top_margin), 
          72.0 * (xprint + size->left_margin), 
          72.0 * (size->length - size->top_margin));
  fprintf(out, "%%%%LanguageLevel: %d\n", level);
  fputs("%%Creator: img2ps " SVERSION " Copyright 1993-1998 Easy Software Products\n", out);
  fprintf(out, "%%Pages: %d\n", xpages * ypages);
  fputs("%%EndComments\n\n", out);

  print_prolog(out, colorspace, gammaval, brightness, status->color_profile);

  row = malloc(img->xsize * abs(colorspace) + 3);

  for (xpage = 0; xpage < xpages; xpage ++)
    for (ypage = 0; ypage < ypages; ypage ++)
    {
      fprintf(out, "%%Page: %d\n", xpage * ypages + ypage + 1);
      fputs("gsave\n", out);

      x0 = img->xsize * xpage / xpages;
      x1 = img->xsize * (xpage + 1) / xpages - 1;
      y0 = img->ysize * ypage / ypages;
      y1 = img->ysize * (ypage + 1) / ypages - 1;

      xprint = xinches / xpages;
      yprint = yinches / ypages;

      if (rotation == 0)
      {
        if (flip)
	  fprintf(out, "\t%.1f %.1f translate\n",
        	  (size->width + xprint) * 36.0,
        	  (size->length + yprint) * 36.0);
        else
	  fprintf(out, "\t%.1f %.1f translate\n",
        	  (size->width - xprint) * 36.0,
        	  (size->length + yprint) * 36.0);
	fprintf(out, "\t%.3f %.3f scale\n\n",
	        xprint * 72.0 / (x1 - x0 + 1),
	        yprint * 72.0 / (y1 - y0 + 1));
      }
      else
      {
        if (flip)
	  fprintf(out, "\t%.1f %.1f translate\n",
        	  (size->width + yprint) * 36.0,
        	  (size->length - xprint) * 36.0);
        else
	  fprintf(out, "\t%.1f %.1f translate\n",
        	  (size->width - yprint) * 36.0,
        	  (size->length - xprint) * 36.0);
	fprintf(out, "\t%.3f %.3f scale\n\n",
	        yprint * 72.0 / (y1 - y0 + 1),
	        xprint * 72.0 / (x1 - x0 + 1));
      };

      if (level == 1)
	fprintf(out, "\t/picture %d string def\n", (x1 - x0 + 1) * abs(colorspace));

      if (rotation == 0)
      {
        if (flip)
	  fprintf(out, "\t%d %d 8 [-1 0 0 -1 1 1] ",
	 	  (x1 - x0 + 1), (y1 - y0 + 1));
        else
	  fprintf(out, "\t%d %d 8 [1 0 0 -1 0 1] ",
	 	  (x1 - x0 + 1), (y1 - y0 + 1));
      }
      else
      {
        if (flip)
	  fprintf(out, "\t%d %d 8 [0 -1 1 0 0 0] ",
	 	  (x1 - x0 + 1), (y1 - y0 + 1));
        else
	  fprintf(out, "\t%d %d 8 [0 1 1 0 0 0] ",
	 	  (x1 - x0 + 1), (y1 - y0 + 1));
      };

      if (level == 1)
      {
        if (colorspace == IMAGE_WHITE)
          fputs(" {currentfile picture readhex pop} image\n", out);
        else
          fputs(" {currentfile picture readhex pop} false 3 colorimage\n", out);

        for (y = y0; y <= y1; y ++)
        {
          ImageGetRow(img, x0, y, x1 - x0 + 1, row);
          ps_hex(out, row, (x1 - x0 + 1) * abs(colorspace));
        };
      }
      else
      {
        fprintf(out, " currentfile /ASCII85Decode filter false %d colorimage\n",
                abs(colorspace));

        for (y = y0, out_offset = 0; y <= y1; y ++)
        {
          ImageGetRow(img, x0, y, x1 - x0 + 1, row + out_offset);

          out_length = (x1 - x0 + 1) * abs(colorspace) + out_offset;
          out_offset = out_length & 3;

          ps_ascii85(out, row, out_length, y == y1);

          if (out_offset > 0)
            memcpy(row, row + out_length - out_offset, out_offset);
        };
      };

      fputs("grestore\n", out);
      fputs("showpage\n", out);
      fprintf(out, "%%EndPage: %d\n", xpage * ypages + ypage + 1);
    };

  fputs("%%EOF\n", out);

  ImageClose(img);

  if (out != stdout)
    fclose(out);
}


/*
 * 'ps_hex()' - Print binary data as a series of hexadecimal numbers.
 */

static void
ps_hex(FILE *prn,	/* I - File to print to */
       ib_t *data,	/* I - Data to print */
       int  length)	/* I - Number of bytes to print */
{
  static char	*hex = "0123456789ABCDEF";


  while (length > 0)
  {
   /*
    * Put the hex chars out to the file; note that we don't use fprintf()
    * for speed reasons...
    */

    putc(hex[*data >> 4], prn);
    putc(hex[*data & 15], prn);

    data ++;
    length --;
  };

  putc('\n', prn);
}


/*
 * 'ps_ascii85()' - Print binary data as a series of base-85 numbers.
 */

static void
ps_ascii85(FILE *prn,		/* I - File to print to */
	   ib_t *data,		/* I - Data to print */
	   int  length,		/* I - Number of bytes to print */
	   int  last_line)	/* I - Last line of raster data? */
{
  unsigned	b;		/* Binary data word */
  unsigned char	c[5];		/* ASCII85 encoded chars */
  int		col;		/* Current column */


  col = 0;
  while (length > 3)
  {
    b = (((((data[0] << 8) | data[1]) << 8) | data[2]) << 8) | data[3];

    if (b == 0)
      putc('z', prn);
    else
    {
      c[4] = (b % 85) + '!';
      b /= 85;
      c[3] = (b % 85) + '!';
      b /= 85;
      c[2] = (b % 85) + '!';
      b /= 85;
      c[1] = (b % 85) + '!';
      b /= 85;
      c[0] = b + '!';

      fwrite(c, 5, 1, prn);
    };

    data += 4;
    length -= 4;

    col = (col + 1) & 15;
    if (col == 0 && length > 0)
      putc('\n', prn);
  };

  if (last_line)
  {
    if (length > 0)
    {
      for (b = 0, col = length; col > 0; b = (b << 8) | data[0], data ++, col --);

      c[4] = (b % 85) + '!';
      b /= 85;
      c[3] = (b % 85) + '!';
      b /= 85;
      c[2] = (b % 85) + '!';
      b /= 85;
      c[1] = (b % 85) + '!';
      b /= 85;
      c[0] = b + '!';

      fwrite(c, length + 1, 1, prn);
    };

    fputs("~>\n", prn);
  }
  else
    putc('\n', prn);
}


/*
 * 'usage()' - Print usage message and exit.
 */

static void
usage(void)
{    
  fputs("usage: img2ps -P <printer-name> <filename> [-D] [-L <log-file>]\n", stderr);
  fputs("              [-O <output-file>] [-b <brightness-val(s)>] [-f]\n", stderr);
  fputs("              [-g <gamma-val(s)>] [-h <hue>] [-l] [-p <ppi>]\n", stderr);
  fputs("              [-r <rotation>] [-s <saturation]\n", stderr);

  exit(ERR_BAD_ARG);
}


/*
 * 'make_transfer_function()' - Make a transfer function given a gamma,
 *                              brightness, and color profile values.
 */

static void
make_transfer_function(char  *s,	/* O - Transfer function string */
                       float ig,	/* I - Image gamma */
                       float ib,	/* I - Image brightness */
                       float pg,	/* I - Profile gamma */
                       float pd)	/* I - Profile ink density */
{
  if (ig == 0.0)
    ig = LutDefaultGamma();

  if ((ig == 1.0 || ig == 0.0) &&
      (ib == 1.0 || ib == 0.0) &&
      (pg == 1.0 || pg == 0.0) &&
      (pd == 1.0 || pd == 0.0))
  {
    s[0] = '\0';
    return;
  };

  if (ig != 1.0 && ig != 0.0)
    sprintf(s, "%.4f exp ", 1.0 / ig);
  else
    s[0] = '\0';

  if (ib != 1.0 || ib != 0.0 ||
      pg != 1.0 || pg != 0.0 ||
      pd != 1.0 || pd != 0.0)
  {
    strcat(s, "neg 1 add ");

    if (ib != 1.0 && ib != 0.0)
      sprintf(s + strlen(s), "%.2f mul ", ib);

    if (pg != 1.0 && pg != 0.0)
      sprintf(s + strlen(s), "%.4f exp ", 1.0 / pg);

    if (pd != 1.0 && pd != 0.0)
      sprintf(s + strlen(s), "%.4f mul ", pd);

    strcat(s, "neg 1 add");
  };
}


/*
 * 'print_prolog()' - Print the output prolog...
 */

static void
print_prolog(FILE  *out,
             int   colorspace,
             float gammaval[4],
             int   brightness[4],
             float *color_profile)
{
  char	cyan[255],
	magenta[255],
	yellow[255],
	black[255];


  fputs("%%BeginProlog\n", out);

  make_transfer_function(black, gammaval[0], 100.0 / brightness[0],
                         color_profile[PD_PROFILE_KG],
                         color_profile[PD_PROFILE_KD]);

  if (colorspace == IMAGE_RGB)
  {
   /*
    * Color output...
    */

    make_transfer_function(cyan, gammaval[1], 100.0 / brightness[1],
                           color_profile[PD_PROFILE_BG],
                           color_profile[PD_PROFILE_CD]);
    make_transfer_function(magenta, gammaval[2], 100.0 / brightness[2],
                           color_profile[PD_PROFILE_BG],
                           color_profile[PD_PROFILE_MD]);
    make_transfer_function(yellow, gammaval[3], 100.0 / brightness[3],
                           color_profile[PD_PROFILE_BG],
                           color_profile[PD_PROFILE_YD]);

    fprintf(out, "{ %s } bind\n"
        	 "{ %s } bind\n"
        	 "{ %s } bind\n"
        	 "{ %s } bind\n"
        	 "setcolortransfer\n",
           cyan, magenta, yellow, black);
  }
  else
  {
   /*
    * B&W output...
    */

    fprintf(out, "{ %s } bind\n"
                 "settransfer\n",
            black);
  };

  fputs("%%EndProlog\n", out);
}


/*
 * End of "$Id: imagetops.c,v 1.1 1998/02/19 20:43:33 mike Exp $".
 */
