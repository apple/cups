/*
 * "$Id: testimage.c 4485 2005-01-03 19:30:00Z mike $"
 *
 *   Image library test program for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1993-2005 by Easy Software Products.
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
 *       Hollywood, Maryland 20636 USA
 *
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   main() - Main entry...
 */

/*
 * Include necessary headers...
 */

#include "image.h"


/*
 * 'main()' - Main entry...
 */

int				/* O - Exit status */
main(int  argc,			/* I - Number of command-line arguments */
     char *argv[])		/* I - Command-line arguments */
{
  image_t	*img;		/* Image to print */
  int		primary;	/* Primary image colorspace */
  FILE		*out;		/* Output PPM/PGM file */
  ib_t		*line;		/* Line from file */
  int		y;		/* Current line */


  if (argc != 3)
  {
    puts("Usage: testimage filename.ext filename.[ppm|pgm]");
    return (1);
  }

  if (strstr(argv[2], ".ppm") != NULL)
    primary = IMAGE_RGB;
  else
    primary = IMAGE_WHITE;

  img = ImageOpen(argv[1], primary, IMAGE_WHITE, 100, 0, NULL);

  if (!img)
  {
    perror(argv[1]);
    return (1);
  }

  out = fopen(argv[2], "wb");

  if (!out)
  {
    perror(argv[2]);
    ImageClose(img);
    return (1);
  }

  line = calloc(img->xsize, img->colorspace);

  fprintf(out, "P%d\n%d\n%d\n255\n", img->colorspace == IMAGE_WHITE ? 5 : 6,
          img->xsize, img->ysize);

  for (y = 0; y < img->ysize; y ++)
  {
    ImageGetRow(img, 0, y, img->xsize, line);
    fwrite(line, img->xsize, img->colorspace, out);
  }

  ImageClose(img);
  fclose(out);

  return (0);
}


/*
 * End of "$Id: testimage.c 4485 2005-01-03 19:30:00Z mike $".
 */
