/*
 * "$Id: imagetops.c,v 1.36.2.14 2003/04/10 14:13:50 mike Exp $"
 *
 *   Image file to PostScript filter for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1993-2003 by Easy Software Products.
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
 *   main()       - Main entry...
 *   ps_hex()     - Print binary data as a series of hexadecimal numbers.
 *   ps_ascii85() - Print binary data as a series of base-85 numbers.
 */

/*
 * Include necessary headers...
 */

#include "common.h"
#include "image.h"
#include <math.h>


/*
 * Globals...
 */

int	Flip = 0,		/* Flip/mirror pages */
	XPosition = 0,		/* Horizontal position on page */
	YPosition = 0,		/* Vertical position on page */
	Collate = 0,		/* Collate copies? */
	Copies = 1;		/* Number of copies */


/*
 * Local functions...
 */

static void	ps_hex(ib_t *, int, int);
static void	ps_ascii85(ib_t *, int, int);


/*
 * 'main()' - Main entry...
 */

int			/* O - Exit status */
main(int  argc,		/* I - Number of command-line arguments */
     char *argv[])	/* I - Command-line arguments */
{
  image_t	*img;		/* Image to print */
  float		xprint,		/* Printable area */
		yprint,
		xinches,	/* Total size in inches */
		yinches;
  float		xsize,		/* Total size in points */
		ysize,
		xsize2,
		ysize2;
  float		aspect;		/* Aspect ratio */
  int		xpages,		/* # x pages */
		ypages,		/* # y pages */
		xpage,		/* Current x page */
		ypage,		/* Current y page */
		page,		/* Current page number */
		pos;		/* Position on page */
  int		x0, y0,		/* Corners of the page in image coords */
		x1, y1;
  ib_t		*row;		/* Current row */
  int		y;		/* Current Y coordinate in image */
  int		colorspace;	/* Output colorspace */
  int		out_offset,	/* Offset into output buffer */
		out_length;	/* Length of output buffer */
  ppd_file_t	*ppd;		/* PPD file */
  ppd_choice_t	*choice;	/* PPD option choice */
  int		num_options;	/* Number of print options */
  cups_option_t	*options;	/* Print options */
  const char	*val;		/* Option value */
  int		slowcollate;	/* Collate copies the slow way */
  float		g;		/* Gamma correction value */
  float		b;		/* Brightness factor */
  float		zoom;		/* Zoom facter */
  int		xppi, yppi;	/* Pixels-per-inch */
  int		hue, sat;	/* Hue and saturation adjustment */
  int		realcopies;	/* Real copies being printed */
  float		left, top;	/* Left and top of image */
  char		filename[1024];	/* Name of file to print */
  time_t	curtime;	/* Current time */
  struct tm	*curtm;		/* Current date */
  char		curdate[255];	/* Current date string */


 /*
  * Make sure status messages are not buffered...
  */

  setbuf(stderr, NULL);

 /*
  * Check command-line...
  */

  if (argc < 6 || argc > 7)
  {
    fputs("ERROR: imagetops job-id user title copies options [file]\n", stderr);
    return (1);
  }

  fprintf(stderr, "INFO: %s %s %s %s %s %s %s\n", argv[0], argv[1], argv[2],
          argv[3], argv[4], argv[5], argv[6] ? argv[6] : "(null)");

 /*
  * Copy stdin as needed...
  */

  if (argc == 6)
  {
    int		fd;		/* File to write to */
    char	buffer[8192];	/* Buffer to read into */
    int		bytes;		/* # of bytes to read */


    if ((fd = cupsTempFd(filename, sizeof(filename))) < 0)
    {
      perror("ERROR: Unable to copy image file");
      return (1);
    }

    fprintf(stderr, "DEBUG: imagetoraster - copying to temp print file \"%s\"\n",
            filename);

    while ((bytes = fread(buffer, 1, sizeof(buffer), stdin)) > 0)
      write(fd, buffer, bytes);

    close(fd);
  }
  else
    strlcpy(filename, argv[6], sizeof(filename));

 /*
  * Process command-line options and write the prolog...
  */

  zoom = 0.0;
  xppi = 0;
  yppi = 0;
  hue  = 0;
  sat  = 100;
  g    = 1.0;
  b    = 1.0;

  Copies = atoi(argv[4]);

  options     = NULL;
  num_options = cupsParseOptions(argv[5], 0, &options);

  ppd = SetCommonOptions(num_options, options, 1);

  if ((val = cupsGetOption("multiple-document-handling", num_options, options)) != NULL)
  {
   /*
    * This IPP attribute is unnecessarily complicated...
    *
    *   single-document, separate-documents-collated-copies, and
    *   single-document-new-sheet all require collated copies.
    *
    *   separate-documents-uncollated-copies allows for uncollated copies.
    */

    Collate = strcasecmp(val, "separate-documents-uncollated-copies") != 0;
  }

  if ((val = cupsGetOption("Collate", num_options, options)) != NULL &&
      strcasecmp(val, "True") == 0)
    Collate = 1;

  if ((val = cupsGetOption("gamma", num_options, options)) != NULL)
    g = atoi(val) * 0.001f;

  if ((val = cupsGetOption("brightness", num_options, options)) != NULL)
    b = atoi(val) * 0.01f;

  if ((val = cupsGetOption("scaling", num_options, options)) != NULL)
    zoom = atoi(val) * 0.01;

  if ((val = cupsGetOption("ppi", num_options, options)) != NULL)
    if (sscanf(val, "%dx%d", &xppi, &yppi) < 2)
      yppi = xppi;

  if ((val = cupsGetOption("position", num_options, options)) != NULL)
  {
    if (strcasecmp(val, "center") == 0)
    {
      XPosition = 0;
      YPosition = 0;
    }
    else if (strcasecmp(val, "top") == 0)
    {
      XPosition = 0;
      YPosition = 1;
    }
    else if (strcasecmp(val, "left") == 0)
    {
      XPosition = -1;
      YPosition = 0;
    }
    else if (strcasecmp(val, "right") == 0)
    {
      XPosition = 1;
      YPosition = 0;
    }
    else if (strcasecmp(val, "top-left") == 0)
    {
      XPosition = -1;
      YPosition = 1;
    }
    else if (strcasecmp(val, "top-right") == 0)
    {
      XPosition = 1;
      YPosition = 1;
    }
    else if (strcasecmp(val, "bottom") == 0)
    {
      XPosition = 0;
      YPosition = -1;
    }
    else if (strcasecmp(val, "bottom-left") == 0)
    {
      XPosition = -1;
      YPosition = -1;
    }
    else if (strcasecmp(val, "bottom-right") == 0)
    {
      XPosition = 1;
      YPosition = -1;
    }
  }

  if ((val = cupsGetOption("saturation", num_options, options)) != NULL)
    sat = atoi(val);

  if ((val = cupsGetOption("hue", num_options, options)) != NULL)
    hue = atoi(val);

  if ((val = cupsGetOption("mirror", num_options, options)) != NULL &&
      strcasecmp(val, "True") == 0)
    Flip = 1;

 /*
  * Open the input image to print...
  */

  colorspace = ColorDevice ? IMAGE_RGB_CMYK : IMAGE_WHITE;

  img = ImageOpen(filename, colorspace, IMAGE_WHITE, sat, hue, NULL);

  if (argc == 6)
    unlink(filename);

  if (img == NULL)
  {
    fputs("ERROR: Unable to open image file for printing!\n", stderr);
    ppdClose(ppd);
    return (1);
  }

  colorspace = img->colorspace;

 /*
  * Scale as necessary...
  */

  xprint = (PageRight - PageLeft) / 72.0;
  yprint = (PageTop - PageBottom) / 72.0;

  if (zoom == 0.0 && xppi == 0)
  {
    xppi = img->xppi;
    yppi = img->yppi;
  }

  if (yppi == 0)
    yppi = xppi;

  fprintf(stderr, "DEBUG: Before scaling: xprint=%.1f, yprint=%.1f, xppi=%d, yppi=%d, zoom=%.2f\n",
          xprint, yprint, xppi, yppi, zoom);

  if (xppi > 0)
  {
   /*
    * Scale the image as neccesary to match the desired pixels-per-inch.
    */
    
    xinches = (float)img->xsize / (float)xppi;
    yinches = (float)img->ysize / (float)yppi;

    fprintf(stderr, "DEBUG: Image size is %.1f x %.1f inches...\n",
            xinches, yinches);

    if ((val = cupsGetOption("natural-scaling", num_options, options)) != NULL)
    {
      xinches = xinches * atoi(val) / 100;
      yinches = yinches * atoi(val) / 100;
    }

    if (cupsGetOption("orientation-requested", num_options, options) == NULL &&
        cupsGetOption("landscape", num_options, options) == NULL)
    {
     /*
      * Rotate the image if it will fit landscape but not portrait...
      */

      fputs("DEBUG: Auto orientation...\n", stderr);

      if ((xinches > xprint || yinches > yprint) &&
          xinches <= yprint && yinches <= xprint)
      {
       /*
	* Rotate the image as needed...
	*/

        fputs("DEBUG: Using landscape orientation...\n", stderr);

	Orientation = (Orientation + 1) & 3;
	xsize       = yprint;
	yprint      = xprint;
	xprint      = xsize;

	xsize       = PageLeft;
	PageLeft    = PageBottom;
	PageBottom  = PageWidth - PageRight;
	PageRight   = PageTop;
	PageTop     = PageLength - xsize;

	xsize       = PageWidth;
	PageWidth   = PageLength;
	PageLength  = xsize;
      }
    }
  }
  else
  {
   /*
    * Scale percentage of page size...
    */

    aspect = (float)img->yppi / (float)img->xppi;

    fprintf(stderr, "DEBUG: img->xppi = %d, img->yppi = %d, aspect = %f\n",
            img->xppi, img->yppi, aspect);

    xsize = xprint * zoom;
    ysize = xsize * img->ysize / img->xsize / aspect;

    if (ysize > (yprint * zoom))
    {
      ysize = yprint * zoom;
      xsize = ysize * img->xsize * aspect / img->ysize;
    }

    xsize2 = yprint * zoom;
    ysize2 = xsize2 * img->ysize / img->xsize / aspect;

    if (ysize2 > (xprint * zoom))
    {
      ysize2 = xprint * zoom;
      xsize2 = ysize2 * img->xsize * aspect / img->ysize;
    }

    fprintf(stderr, "DEBUG: Portrait size is %.2f x %.2f inches\n", xsize, ysize);
    fprintf(stderr, "DEBUG: Landscape size is %.2f x %.2f inches\n", xsize2, ysize2);

    if (cupsGetOption("orientation-requested", num_options, options) == NULL &&
        cupsGetOption("landscape", num_options, options) == NULL)
    {
     /*
      * Choose the rotation with the largest area, but prefer
      * portrait if they are equal...
      */

      fputs("DEBUG: Auto orientation...\n", stderr);

      if ((xsize * ysize) < (xsize2 * xsize2))
      {
       /*
	* Do landscape orientation...
	*/

        fputs("DEBUG: Using landscape orientation...\n", stderr);

	Orientation = 1;
	xinches     = xsize2;
	yinches     = ysize2;
	xprint      = (PageTop - PageBottom) / 72.0;
	yprint      = (PageRight - PageLeft) / 72.0;
      }
      else
      {
       /*
	* Do portrait orientation...
	*/

        fputs("DEBUG: Using portrait orientation...\n", stderr);

	Orientation = 0;
	xinches     = xsize;
	yinches     = ysize;
      }
    }
    else if (Orientation & 1)
    {
      fputs("DEBUG: Using landscape orientation...\n", stderr);

      xinches     = xsize2;
      yinches     = ysize2;
      xprint      = (PageTop - PageBottom) / 72.0;
      yprint      = (PageRight - PageLeft) / 72.0;
    }
    else
    {
      fputs("DEBUG: Using portrait orientation...\n", stderr);

      xinches     = xsize;
      yinches     = ysize;
      xprint      = (PageRight - PageLeft) / 72.0;
      yprint      = (PageTop - PageBottom) / 72.0;
    }
  }

 /*
  * Compute the number of pages to print and the size of the image on each
  * page...
  */

  xpages = ceil(xinches / xprint);
  ypages = ceil(yinches / yprint);

  xprint = xinches / xpages;
  yprint = yinches / ypages;

 /*
  * Update the page size for custom sizes...
  */

  if ((choice = ppdFindMarkedChoice(ppd, "PageSize")) != NULL &&
      strcasecmp(choice->choice, "Custom") == 0)
  {
    float	width,		/* New width in points */
		length;		/* New length in points */
    char	s[255];		/* New custom page size... */


    if (Orientation & 1)
    {
      width  = yprint * 72.0;
      length = xprint * 72.0;
    }
    else
    {
      width  = xprint * 72.0;
      length = yprint * 72.0;
    }

   /*
    * Add margins to page size...
    */

    width  += ppd->custom_margins[0] + ppd->custom_margins[2];
    length += ppd->custom_margins[1] + ppd->custom_margins[3];

   /*
    * Enforce minimums...
    */

    if (width < ppd->custom_min[0])
      width = ppd->custom_min[0];

    if (length < ppd->custom_min[1])
      length = ppd->custom_min[1];

   /*
    * Set the new custom size...
    */

    sprintf(s, "Custom.%.0fx%.0f", width, length);
    ppdMarkOption(ppd, "PageSize", s);

    fprintf(stderr, "DEBUG: Updated custom page size to %.2f x %.2f inches...\n",
            width / 72.0, length / 72.0);

   /*
    * Update page variables...
    */

    PageWidth  = width;
    PageLength = length;
    PageLeft   = ppd->custom_margins[0];
    PageRight  = width - ppd->custom_margins[2];
    PageBottom = ppd->custom_margins[1];
    PageTop    = length - ppd->custom_margins[3];

    UpdatePageVars();
  }

 /*
  * See if we need to collate, and if so how we need to do it...
  */

  if (xpages == 1 && ypages == 1)
    Collate = 0;

  slowcollate = Collate && ppdFindOption(ppd, "Collate") == NULL;

  if (Copies > 1 && !slowcollate)
  {
    realcopies = Copies;
    Copies     = 1;
  }
  else
    realcopies = 1;

 /*
  * Write any "exit server" options that have been selected...
  */

  ppdEmit(ppd, stdout, PPD_ORDER_EXIT);

 /*
  * Write any JCL commands that are needed to print PostScript code...
  */

  ppdEmitJCL(ppd, stdout, atoi(argv[1]), argv[2], argv[3]);

 /*
  * Start sending the document with any commands needed...
  */

  curtime = time(NULL);
  curtm   = localtime(&curtime);

  puts("%!PS-Adobe-3.0");
  printf("%%%%BoundingBox: %.0f %.0f %.0f %.0f\n", PageLeft, PageBottom,
         PageRight, PageTop);
  printf("%%%%LanguageLevel: %d\n", LanguageLevel);
  printf("%%%%Pages: %d\n", xpages * ypages * Copies);
  puts("%%DocumentData: Clean7Bit");
  puts("%%DocumentNeededResources: font Helvetica-Bold");
  puts("%%Creator: imagetops/" CUPS_SVERSION);
  strftime(curdate, sizeof(curdate), CUPS_STRFTIME_FORMAT, curtm);
  printf("%%%%CreationDate: %s\n", curdate);
  printf("%%%%Title: %s\n", argv[3]);
  printf("%%%%For: %s\n", argv[2]);
  if (Orientation & 1)
    puts("%%Orientation: Landscape");
  else
    puts("%%Orientation: Portrait");
  puts("%%EndComments");
  puts("%%BeginProlog");

  if (ppd != NULL && ppd->patches != NULL)
    puts(ppd->patches);

  ppdEmit(ppd, stdout, PPD_ORDER_DOCUMENT);
  ppdEmit(ppd, stdout, PPD_ORDER_ANY);
  ppdEmit(ppd, stdout, PPD_ORDER_PROLOG);

  if (g != 1.0 || b != 1.0)
    printf("{ neg 1 add dup 0 lt { pop 1 } { %.3f exp neg 1 add } "
           "ifelse %.3f mul } bind settransfer\n", g, b);

  WriteLabelProlog(cupsGetOption("page-label", num_options, options),
                   PageBottom, PageTop, PageWidth);

  if (realcopies > 1)
  {
    if (ppd == NULL || ppd->language_level == 1)
      printf("/#copies %d def\n", realcopies);
    else
      printf("<</NumCopies %d>>setpagedevice\n", realcopies);
  }

  puts("%%EndProlog");

 /*
  * Output the pages...
  */

  row = malloc(img->xsize * abs(colorspace) + 3);

  for (page = 1; Copies > 0; Copies --)
    for (xpage = 0; xpage < xpages; xpage ++)
      for (ypage = 0; ypage < ypages; ypage ++, page ++)
      {
        if (ppd && ppd->num_filters == 0)
          fprintf(stderr, "PAGE: %d %d\n", page, realcopies);

	fprintf(stderr, "INFO: Printing page %d...\n", page);

        printf("%%%%Page: %d %d\n", page, page);

        ppdEmit(ppd, stdout, PPD_ORDER_PAGE);

	puts("gsave");

	if (Flip)
	  printf("%.0f 0 translate -1 1 scale\n", PageWidth);

	switch (Orientation)
	{
	  case 1 : /* Landscape */
              printf("%.0f 0 translate 90 rotate\n", PageLength);
              break;
	  case 2 : /* Reverse Portrait */
              printf("%.0f %.0f translate 180 rotate\n", PageWidth, PageLength);
              break;
	  case 3 : /* Reverse Landscape */
              printf("0 %.0f translate -90 rotate\n", PageWidth);
              break;
	}

	x0 = img->xsize * xpage / xpages;
	x1 = img->xsize * (xpage + 1) / xpages - 1;
	y0 = img->ysize * ypage / ypages;
	y1 = img->ysize * (ypage + 1) / ypages - 1;

        switch (Orientation)
	{
	  default : /* Portrait */
	      pos = XPosition;
	      break;
	  case 1 : /* Landscape */
	      pos = YPosition;
	      break;
	  case 2 : /* Reverse Portrait */
	      pos = -XPosition;
	      break;
	  case 3 : /* Reverse Lanscape */
	      pos = -YPosition;
	      break;
	}

        switch (pos)
	{
	  case -1 :
	      left = PageLeft;
	      break;
	  default :
	      left = PageLeft + (PageWidth - xprint * 72.0) * 0.5;
	      break;
	  case 1 :
	      left = PageRight - xprint * 72.0;
	      break;
	}

        switch (Orientation)
	{
	  default : /* Portrait */
	      pos = YPosition;
	      break;
	  case 1 : /* Landscape */
	      pos = -XPosition;
	      break;
	  case 2 : /* Reverse Portrait */
	      pos = -YPosition;
	      break;
	  case 3 : /* Reverse Lanscape */
	      pos = XPosition;
	      break;
	}

        switch (pos)
	{
	  case -1 :
	      top = PageBottom + 72.0 * yprint;
	      break;
	  default :
	      top = PageBottom + (PageLength + yprint * 72.0) * 0.5;
	      break;
	  case 1 :
	      top = PageTop;
	      break;
	}

        printf("%.1f %.1f translate\n", left, top);

	printf("%.3f %.3f scale\n\n",
	       xprint * 72.0 / (x1 - x0 + 1),
	       yprint * 72.0 / (y1 - y0 + 1));

	if (LanguageLevel == 1)
	{
	  printf("/picture %d string def\n", (x1 - x0 + 1) * abs(colorspace));
	  printf("%d %d 8[1 0 0 -1 0 1]", (x1 - x0 + 1), (y1 - y0 + 1));

          if (colorspace == IMAGE_WHITE)
            puts("{currentfile picture readhexstring pop} image");
          else
            printf("{currentfile picture readhexstring pop} false %d colorimage\n",
	           abs(colorspace));

          for (y = y0; y <= y1; y ++)
          {
            ImageGetRow(img, x0, y, x1 - x0 + 1, row);
            ps_hex(row, (x1 - x0 + 1) * abs(colorspace), y == y1);
          }
	}
	else
	{
          switch (colorspace)
	  {
	    case IMAGE_WHITE :
                puts("/DeviceGray setcolorspace");
		break;
            case IMAGE_RGB :
                puts("/DeviceRGB setcolorspace");
		break;
            case IMAGE_CMYK :
                puts("/DeviceCMYK setcolorspace");
		break;
          }

          printf("<<"
                 "/ImageType 1"
		 "/Width %d"
		 "/Height %d"
		 "/BitsPerComponent 8",
		 x1 - x0 + 1, y1 - y0 + 1);

          switch (colorspace)
	  {
	    case IMAGE_WHITE :
                fputs("/Decode[0 1]", stdout);
		break;
            case IMAGE_RGB :
                fputs("/Decode[0 1 0 1 0 1]", stdout);
		break;
            case IMAGE_CMYK :
                fputs("/Decode[0 1 0 1 0 1 0 1]", stdout);
		break;
          }

          fputs("/DataSource currentfile /ASCII85Decode filter", stdout);

          if (((x1 - x0 + 1) / xprint) < 100.0)
            fputs("/Interpolate true", stdout);

          puts("/ImageMatrix[1 0 0 -1 0 1]>>image");

          for (y = y0, out_offset = 0; y <= y1; y ++)
          {
            ImageGetRow(img, x0, y, x1 - x0 + 1, row + out_offset);

            out_length = (x1 - x0 + 1) * abs(colorspace) + out_offset;
            out_offset = out_length & 3;

            ps_ascii85(row, out_length, y == y1);

            if (out_offset > 0)
              memcpy(row, row + out_length - out_offset, out_offset);
          }
	}

	puts("grestore");
	WriteLabels(Orientation);
	puts("showpage");
      }

  puts("%%EOF");

 /*
  * End the job with the appropriate JCL command or CTRL-D otherwise.
  */

  if (ppd != NULL && ppd->jcl_end)
    fputs(ppd->jcl_end, stdout);
  else
    putchar(0x04);

 /*
  * Close files...
  */

  ImageClose(img);
  ppdClose(ppd);

  return (0);
}


/*
 * 'ps_hex()' - Print binary data as a series of hexadecimal numbers.
 */

static void
ps_hex(ib_t *data,		/* I - Data to print */
       int  length,		/* I - Number of bytes to print */
       int  last_line)		/* I - Last line of raster data? */
{
  static int	col = 0;	/* Current column */
  static char	*hex = "0123456789ABCDEF";
				/* Hex digits */


  while (length > 0)
  {
   /*
    * Put the hex chars out to the file; note that we don't use printf()
    * for speed reasons...
    */

    putchar(hex[*data >> 4]);
    putchar(hex[*data & 15]);

    data ++;
    length --;

    col += 2;
    if (col > 78)
    {
      putchar('\n');
      col = 0;
    }
  }

  if (last_line && col)
  {
    putchar('\n');
    col = 0;
  }
}


/*
 * 'ps_ascii85()' - Print binary data as a series of base-85 numbers.
 */

static void
ps_ascii85(ib_t *data,		/* I - Data to print */
	   int  length,		/* I - Number of bytes to print */
	   int  last_line)	/* I - Last line of raster data? */
{
  unsigned	b;		/* Binary data word */
  unsigned char	c[5];		/* ASCII85 encoded chars */
  static int	col = 0;	/* Current column */


  while (length > 3)
  {
    b = (((((data[0] << 8) | data[1]) << 8) | data[2]) << 8) | data[3];

    if (b == 0)
    {
      putchar('z');
      col ++;
    }
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

      fwrite(c, 5, 1, stdout);
      col += 5;
    }

    data += 4;
    length -= 4;

    if (col >= 75)
    {
      putchar('\n');
      col = 0;
    }
  }

  if (last_line)
  {
    if (length > 0)
    {
      memset(data + length, 0, 4 - length);
      b = (((((data[0] << 8) | data[1]) << 8) | data[2]) << 8) | data[3];

      c[4] = (b % 85) + '!';
      b /= 85;
      c[3] = (b % 85) + '!';
      b /= 85;
      c[2] = (b % 85) + '!';
      b /= 85;
      c[1] = (b % 85) + '!';
      b /= 85;
      c[0] = b + '!';

      fwrite(c, length + 1, 1, stdout);
    }

    puts("~>");
    col = 0;
  }
}


/*
 * End of "$Id: imagetops.c,v 1.36.2.14 2003/04/10 14:13:50 mike Exp $".
 */
