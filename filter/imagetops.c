/*
 * "$Id: imagetops.c,v 1.19 1999/11/01 16:53:43 mike Exp $"
 *
 *   Image file to PostScript filter for the Common UNIX Printing System (CUPS).
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
	Collate = 0,		/* Collate copies? */
	Copies = 1;		/* Number of copies */


/*
 * Local functions...
 */

static void	ps_hex(ib_t *, int);
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
		ysize;
  int		xpages,		/* # x pages */
		ypages,		/* # y pages */
		xpage,		/* Current x page */
		ypage,		/* Current y page */
		page;		/* Current page number */
  int		x0, y0,		/* Corners of the page in image coords */
		x1, y1;
  ib_t		*row;		/* Current row */
  int		y;		/* Current Y coordinate in image */
  int		colorspace;	/* Output colorspace */
  int		out_offset,	/* Offset into output buffer */
		out_length;	/* Length of output buffer */
  ppd_file_t	*ppd;		/* PPD file */
  int		num_options;	/* Number of print options */
  cups_option_t	*options;	/* Print options */
  const char	*val;		/* Option value */
  int		slowcollate;	/* Collate copies the slow way */
  float		g;		/* Gamma correction value */
  float		b;		/* Brightness factor */
  float		zoom;		/* Zoom facter */
  int		ppi;		/* Pixels-per-inch */
  int		hue, sat;	/* Hue and saturation adjustment */
  int		realcopies;	/* Real copies being printed */


  if (argc != 7)
  {
    fputs("ERROR: imagetops job-id user title copies options file\n", stderr);
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

  Copies = atoi(argv[4]);

  options     = NULL;
  num_options = cupsParseOptions(argv[5], 0, &options);

  ppd = SetCommonOptions(num_options, options, 1);

  ppdMarkDefaults(ppd);
  cupsMarkOptions(ppd, num_options, options);

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

    Collate = strcasecmp(val, "separate-documents-collated-copies") != 0;
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
    ppi = atoi(val);

  if ((val = cupsGetOption("saturation", num_options, options)) != NULL)
    sat = atoi(val);

  if ((val = cupsGetOption("hue", num_options, options)) != NULL)
    hue = atoi(val);

 /*
  * Open the input image to print...
  */

  colorspace = ColorDevice ? IMAGE_RGB : IMAGE_WHITE;

  if ((img = ImageOpen(argv[6], colorspace, IMAGE_WHITE, sat, hue, NULL)) == NULL)
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
  * See if we need to collate, and if so how we need to do it...
  */

  if (xpages == 1 && ypages == 1)
    Collate = 0;

  slowcollate = Collate && ppdFindOption(ppd, "Collate") == NULL;

 /*
  * Write any "exit server" options that have been selected...
  */

  ppdEmit(ppd, stdout, PPD_ORDER_EXIT);

 /*
  * Write any JCL commands that are needed to print PostScript code...
  */

  if (ppd != NULL && ppd->jcl_begin && ppd->jcl_ps)
  {
    fputs(ppd->jcl_begin, stdout);
    ppdEmit(ppd, stdout, PPD_ORDER_JCL);
    fputs(ppd->jcl_ps, stdout);
  }

 /*
  * Start sending the document with any commands needed...
  */

  puts("%!");

  if (ppd != NULL && ppd->patches != NULL)
    puts(ppd->patches);

  ppdEmit(ppd, stdout, PPD_ORDER_DOCUMENT);
  ppdEmit(ppd, stdout, PPD_ORDER_ANY);
  ppdEmit(ppd, stdout, PPD_ORDER_PROLOG);

  if (g != 1.0 || b != 1.0)
    printf("{ neg 1 add dup 0 lt { pop 1 } { %.3f exp neg 1 add } "
           "ifelse %.3f mul } bind settransfer\n", g, b);

  if (Copies > 1 && !slowcollate)
  {
    printf("/#copies %d def\n", Copies);
    realcopies = Copies;
    Copies     = 1;
  }
  else
    realcopies = 1;

 /*
  * Output the pages...
  */

  xprint = xinches / xpages;
  yprint = yinches / ypages;
  row    = malloc(img->xsize * abs(colorspace) + 3);

  for (page = 1; Copies > 0; Copies --)
    for (xpage = 0; xpage < xpages; xpage ++)
      for (ypage = 0; ypage < ypages; ypage ++, page ++)
      {
        fprintf(stderr, "PAGE: %d %d\n", page, realcopies);
        fprintf(stderr, "INFO: Printing page %d...\n", page);

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

        printf("%.1f %.1f translate\n", PageLeft, PageBottom + 72.0 * yprint);
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
            puts("{currentfile picture readhexstring pop} false 3 colorimage");

          for (y = y0; y <= y1; y ++)
          {
            ImageGetRow(img, x0, y, x1 - x0 + 1, row);
            ps_hex(row, (x1 - x0 + 1) * abs(colorspace));
          }
	}
	else
	{
          if (colorspace == IMAGE_WHITE)
            puts("/DeviceGray setcolorspace");
          else
            puts("/DeviceRGB setcolorspace");

          printf("<<"
                 "/ImageType 1"
		 "/Width %d"
		 "/Height %d"
		 "/BitsPerComponent 8",
		 x1 - x0 + 1, y1 - y0 + 1);

          if (colorspace == IMAGE_WHITE)
            fputs("/Decode[0 1]", stdout);
          else
            fputs("/Decode[0 1 0 1 0 1]", stdout);

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
	puts("showpage");
      }

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
ps_hex(ib_t *data,	/* I - Data to print */
       int  length)	/* I - Number of bytes to print */
{
  int		col;
  static char	*hex = "0123456789ABCDEF";


  col = 0;

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

    col = (col + 1) & 31;
    if (col == 0 && length > 0)
      putchar('\n');
  }

  putchar('\n');
}


/*
 * 'ps_ascii85()' - Print binary data as a series of base-85 numbers.
 */

static void
ps_ascii85(ib_t *data,		/* I - Data to print */
	   int  length,		/* I - Number of bytes to print */
	   int  last_line)	/* I - Last line of raster data? */
{
  int		i;		/* Looping var */
  unsigned	b;		/* Binary data word */
  unsigned char	c[5];		/* ASCII85 encoded chars */


  while (length > 3)
  {
    b = (((((data[0] << 8) | data[1]) << 8) | data[2]) << 8) | data[3];

    if (b == 0)
      putchar('z');
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
    }

    data += 4;
    length -= 4;
  }

  if (last_line)
  {
    if (length > 0)
    {
      for (b = 0, i = length; i > 0; b = (b << 8) | data[0], data ++, i --);

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
  }
}


/*
 * End of "$Id: imagetops.c,v 1.19 1999/11/01 16:53:43 mike Exp $".
 */
