/*
 * Generic HP PCL printer command for ippeveprinter/CUPS.
 *
 * Copyright © 2019 by Apple Inc.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more
 * information.
 */

/*
 * Include necessary headers...
 */

#include "ippevecommon.h"
#include "dither.h"


/*
 * Local globals...
 */

static unsigned		pcl_bottom,	/* Bottom line */
			pcl_left,	/* Left offset in line */
			pcl_right,	/* Right offset in line */
			pcl_top,	/* Top line */
			pcl_blanks;	/* Number of blank lines to skip */
static unsigned char	pcl_white,	/* White color */
			*pcl_line,	/* Line buffer */
			*pcl_comp;	/* Compression buffer */

/*
 * Local functions...
 */

static void	pcl_end_page(cups_page_header2_t *header, unsigned page);
static void	pcl_start_page(cups_page_header2_t *header, unsigned page);
static int	pcl_to_pcl(const char *filename);
static void	pcl_write_line(cups_page_header2_t *header, unsigned y, const unsigned char *line);
static int	raster_to_pcl(const char *filename);


/*
 * 'main()' - Main entry for PCL printer command.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  const char		*content_type;	/* Content type to print */


 /*
  * Print it...
  */

  if (argc > 2)
  {
    fputs("ERROR: Too many arguments supplied, aborting.\n", stderr);
    return (1);
  }
  else if ((content_type = getenv("CONTENT_TYPE")) == NULL)
  {
    fputs("ERROR: CONTENT_TYPE environment variable not set, aborting.\n", stderr);
    return (1);
  }
  else if (!strcasecmp(content_type, "application/vnd.hp-pcl"))
  {
    return (pcl_to_pcl(argv[1]));
  }
  else if (!strcasecmp(content_type, "image/pwg-raster") || !strcasecmp(content_type, "image/urf"))
  {
    return (raster_to_pcl(argv[1]));
  }
  else
  {
    fprintf(stderr, "ERROR: CONTENT_TYPE %s not supported.\n", content_type);
    return (1);
  }
}


/*
 * 'pcl_end_page()' - End of PCL page.
 */

static void
pcl_end_page(
    cups_page_header2_t *header,	/* I - Page header */
    unsigned            page)		/* I - Current page */
{
 /*
  * End graphics...
  */

  fputs("\033*r0B", stdout);

 /*
  * Formfeed as needed...
  */

  if (!(header->Duplex && (page & 1)))
    putchar('\f');

 /*
  * Free the output buffers...
  */

  free(pcl_line);
  free(pcl_comp);
}


/*
 * 'pcl_start_page()' - Start a PCL page.
 */

static void
pcl_start_page(
    cups_page_header2_t *header,	/* I - Page header */
    unsigned            page)		/* I - Current page */
{
 /*
  * Setup margins to be 1/6" top and bottom and 1/4" or .135" on the
  * left and right.
  */

  pcl_top    = header->HWResolution[1] / 6;
  pcl_bottom = header->cupsHeight - header->HWResolution[1] / 6 - 1;

  if (header->PageSize[1] == 842)
  {
   /* A4 gets special side margins to expose an 8" print area */
    pcl_left  = (header->cupsWidth - 8 * header->HWResolution[0]) / 2;
    pcl_right = pcl_left + 8 * header->HWResolution[0] - 1;
  }
  else
  {
   /* All other sizes get 1/4" margins */
    pcl_left  = header->HWResolution[0] / 4;
    pcl_right = header->cupsWidth - header->HWResolution[0] / 4 - 1;
  }

  if (!header->Duplex || (page & 1))
  {
   /*
    * Set the media size...
    */

    printf("\033&l12D\033&k12H");	/* Set 12 LPI, 10 CPI */
    printf("\033&l0O");			/* Set portrait orientation */

    switch (header->PageSize[1])
    {
      case 540 : /* Monarch Envelope */
          printf("\033&l80A");
	  break;

      case 595 : /* A5 */
          printf("\033&l25A");
	  break;

      case 624 : /* DL Envelope */
          printf("\033&l90A");
	  break;

      case 649 : /* C5 Envelope */
          printf("\033&l91A");
	  break;

      case 684 : /* COM-10 Envelope */
          printf("\033&l81A");
	  break;

      case 709 : /* B5 Envelope */
          printf("\033&l100A");
	  break;

      case 756 : /* Executive */
          printf("\033&l1A");
	  break;

      case 792 : /* Letter */
          printf("\033&l2A");
	  break;

      case 842 : /* A4 */
          printf("\033&l26A");
	  break;

      case 1008 : /* Legal */
          printf("\033&l3A");
	  break;

      case 1191 : /* A3 */
          printf("\033&l27A");
	  break;

      case 1224 : /* Tabloid */
          printf("\033&l6A");
	  break;
    }

   /*
    * Set top margin and turn off perforation skip...
    */

    printf("\033&l%uE\033&l0L", 12 * pcl_top / header->HWResolution[1]);

    if (header->Duplex)
    {
      int mode = header->Duplex ? 1 + header->Tumble != 0 : 0;

      printf("\033&l%dS", mode);	/* Set duplex mode */
    }
  }
  else if (header->Duplex)
    printf("\033&a2G");			/* Print on back side */

 /*
  * Set graphics mode...
  */

  printf("\033*t%uR", header->HWResolution[0]);
					/* Set resolution */
  printf("\033*r%uS", pcl_right - pcl_left + 1);
					/* Set width */
  printf("\033*r%uT", pcl_bottom - pcl_top + 1);
					/* Set height */
  printf("\033&a0H\033&a%uV", 720 * pcl_top / header->HWResolution[1]);
					/* Set position */

  printf("\033*b2M");	/* Use PackBits compression */
  printf("\033*r1A");	/* Start graphics */

 /*
  * Allocate the output buffers...
  */

  pcl_white  = header->cupsBitsPerColor == 1 ? 0 : 255;
  pcl_blanks = 0;
  pcl_line   = malloc(header->cupsWidth / 8 + 1);
  pcl_comp   = malloc(2 * header->cupsBytesPerLine + 2);

  fprintf(stderr, "ATTR: job-impressions-completed=%d\n", page);
}


/*
 * 'pcl_to_pcl()' - Pass through PCL data.
 */

static int				/* O - Exit status */
pcl_to_pcl(const char *filename)	/* I - File to print or NULL for stdin */
{
  int		fd;			/* File to read from */
  char		buffer[65536];		/* Copy buffer */
  ssize_t	bytes;			/* Bytes to write */


 /*
  * Open the input file...
  */

  if (filename)
  {
    if ((fd = open(filename, O_RDONLY)) < 0)
    {
      fprintf(stderr, "ERROR: Unable to open \"%s\": %s\n", filename, strerror(errno));
      return (1);
    }
  }
  else
  {
    fd = 0;
  }

  fputs("ATTR: job-impressions=unknown\n", stderr);

 /*
  * Copy to stdout...
  */

  while ((bytes = read(fd, buffer, sizeof(buffer))) > 0)
    write(1, buffer, (size_t)bytes);

 /*
  * Close the input file...
  */

  if (fd > 0)
    close(fd);

  return (0);
}


/*
 * 'pcl_write_line()' - Write a line of raster data.
 */

static void
pcl_write_line(
    cups_page_header2_t *header,	/* I - Raster information */
    unsigned            y,		/* I - Line number */
    const unsigned char *line)		/* I - Pixels on line */
{
  unsigned	x;			/* Column number */
  unsigned char	bit,			/* Current bit */
		byte,			/* Current byte */
		*outptr,		/* Pointer into output buffer */
		*outend,		/* End of output buffer */
		*start,			/* Start of sequence */
		*compptr;		/* Pointer into compression buffer */
  unsigned	count;			/* Count of bytes for output */
  const unsigned char	*ditherline;	/* Pointer into dither table */


  if (line[0] == pcl_white && !memcmp(line, line + 1, header->cupsBytesPerLine - 1))
  {
   /*
    * Skip blank line...
    */

    pcl_blanks ++;
    return;
  }

  if (header->cupsBitsPerPixel == 1)
  {
   /*
    * B&W bitmap data can be used directly...
    */

    outend = (unsigned char *)line + (pcl_right + 7) / 8;
    outptr = (unsigned char *)line + pcl_left / 8;
  }
  else
  {
   /*
    * Dither 8-bit grayscale to B&W...
    */

    y &= 63;
    ditherline = threshold[y];

    for (x = pcl_left, bit = 128, byte = 0, outptr = pcl_line; x <= pcl_right; x ++, line ++)
    {
      if (*line <= ditherline[x & 63])
	byte |= bit;

      if (bit == 1)
      {
	*outptr++ = byte;
	byte      = 0;
	bit       = 128;
      }
      else
	bit >>= 1;
    }

    if (bit != 128)
      *outptr++ = byte;

    outend = outptr;
    outptr = pcl_line;
  }

 /*
  * Apply compression...
  */

  compptr = pcl_comp;

  while (outptr < outend)
  {
    if ((outptr + 1) >= outend)
    {
     /*
      * Single byte on the end...
      */

      *compptr++ = 0x00;
      *compptr++ = *outptr++;
    }
    else if (outptr[0] == outptr[1])
    {
     /*
      * Repeated sequence...
      */

      outptr ++;
      count = 2;

      while (outptr < (outend - 1) &&
	     outptr[0] == outptr[1] &&
	     count < 127)
      {
	outptr ++;
	count ++;
      }

      *compptr++ = (unsigned char)(257 - count);
      *compptr++ = *outptr++;
    }
    else
    {
     /*
      * Non-repeated sequence...
      */

      start = outptr;
      outptr ++;
      count = 1;

      while (outptr < (outend - 1) &&
	     outptr[0] != outptr[1] &&
	     count < 127)
      {
	outptr ++;
	count ++;
      }

      *compptr++ = (unsigned char)(count - 1);

      memcpy(compptr, start, count);
      compptr += count;
    }
  }

 /*
  * Output the line...
  */

  if (pcl_blanks > 0)
  {
   /*
    * Skip blank lines first...
    */

    printf("\033*b%dY", pcl_blanks);
    pcl_blanks = 0;
  }

  printf("\033*b%dW", (int)(compptr - pcl_comp));
  fwrite(pcl_comp, 1, (size_t)(compptr - pcl_comp), stdout);
}


/*
 * 'raster_to_pcl()' - Convert raster data to PCL.
 */

static int				/* O - Exit status */
raster_to_pcl(const char *filename)	/* I - File to print (NULL for stdin) */
{
  int			fd;		/* Input file */
  cups_raster_t		*ras;		/* Raster stream */
  cups_page_header2_t	header;		/* Page header */
  unsigned		page = 0,	/* Current page */
			y;		/* Current line */
  unsigned char		*line;		/* Line buffer */



 /*
  * Open the input file...
  */

  if (filename)
  {
    if ((fd = open(filename, O_RDONLY)) < 0)
    {
      fprintf(stderr, "ERROR: Unable to open \"%s\": %s\n", filename, strerror(errno));
      return (1);
    }
  }
  else
  {
    fd = 0;
  }

 /*
  * Open the raster stream and send pages...
  */

  if ((ras = cupsRasterOpen(fd, CUPS_RASTER_READ)) == NULL)
  {
    fputs("ERROR: Unable to read raster data, aborting.\n", stderr);
    return (1);
  }

  fputs("\033E", stdout);

  while (cupsRasterReadHeader2(ras, &header))
  {
    page ++;

    if (header.cupsColorSpace != CUPS_CSPACE_W && header.cupsColorSpace != CUPS_CSPACE_SW && header.cupsColorSpace != CUPS_CSPACE_K)
    {
      fputs("ERROR: Unsupported color space, aborting.\n", stderr);
      break;
    }
    else if (header.cupsBitsPerColor != 1 && header.cupsBitsPerColor != 8)
    {
      fputs("ERROR: Unsupported bit depth, aborting.\n", stderr);
      break;
    }

    line = malloc(header.cupsBytesPerLine);

    pcl_start_page(&header, page);
    for (y = 0; y < header.cupsHeight; y ++)
    {
      if (cupsRasterReadPixels(ras, line, header.cupsBytesPerLine))
        pcl_write_line(&header, y, line);
      else
        break;
    }
    pcl_end_page(&header, page);

    free(line);
  }

  cupsRasterClose(ras);

  fprintf(stderr, "ATTR: job-impressions=%d\n", page);

  return (0);
}
