/*
 * "$Id$"
 *
 *   Advanced EPSON ESC/P raster driver for CUPS.
 *
 *   Copyright 2007 by Apple Inc.
 *   Copyright 1993-2005 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
 *   Setup()           - Prepare the printer for graphics output.
 *   StartPage()       - Start a page of graphics.
 *   EndPage()         - Finish a page of graphics.
 *   Shutdown()        - Shutdown a printer.
 *   CompressData()    - Compress a line of graphics.
 *   OutputBand()      - Output a band of graphics.
 *   ProcessLine()     - Read graphics from the page stream and output
 *                       as needed.
 *   main()            - Main entry and processing of driver.
 */

/*
 * Include necessary headers...
 */

#include "driver.h"
#include <cups/string.h>
#include "data/escp.h"


/*
 * Softweave data...
 */

typedef struct cups_weave_str
{
  struct cups_weave_str	*prev,			/* Previous band */
			*next;			/* Next band */
  int			x, y,			/* Column/Line on the page */
			plane,			/* Color plane */
			dirty,			/* Is this buffer dirty? */
			row,			/* Row in the buffer */
			count;			/* Max rows this pass */
  unsigned char		*buffer;		/* Data buffer */
} cups_weave_t;


/*
 * Globals...
 */

cups_rgb_t	*RGB;			/* RGB color separation data */
cups_cmyk_t	*CMYK;			/* CMYK color separation data */
unsigned char	*PixelBuffer,		/* Pixel buffer */
		*CMYKBuffer,		/* CMYK buffer */
		*OutputBuffers[7],	/* Output buffers */
		*DotBuffers[7],		/* Dot buffers */
		*CompBuffer;		/* Compression buffer */
short		*InputBuffer;		/* Color separation buffer */
cups_weave_t	*DotAvailList,		/* Available buffers */
		*DotUsedList,		/* Used buffers */
		*DotBands[128][7];	/* Buffers in use */
int		DotBufferSize,		/* Size of dot buffers */
		DotRowMax,		/* Maximum row number in buffer */
		DotColStep,		/* Step for each output column */
		DotRowStep,		/* Step for each output line */
		DotRowFeed,		/* Amount to feed for interleave */
		DotRowCount,		/* Number of rows to output */
		DotRowOffset[7],	/* Offset for each color on print head */
		DotRowCurrent,		/* Current row */
		DotSize;		/* Dot size (Pro 5000 only) */
int		PrinterPlanes,		/* # of color planes */
		BitPlanes,		/* # of bit planes per color */
		PrinterTop,		/* Top of page */
		PrinterLength;		/* Length of page */
cups_lut_t	*DitherLuts[7];		/* Lookup tables for dithering */
cups_dither_t	*DitherStates[7];	/* Dither state tables */
int		OutputFeed;		/* Number of lines to skip */


/*
 * Prototypes...
 */

void	Setup(ppd_file_t *);
void	StartPage(ppd_file_t *, cups_page_header_t *);
void	EndPage(ppd_file_t *, cups_page_header_t *);
void	Shutdown(ppd_file_t *);

void	AddBand(cups_weave_t *band);
void	CompressData(ppd_file_t *, const unsigned char *, const int,
	             int, int, const int, const int, const int,
		     const int);
void	OutputBand(ppd_file_t *, cups_page_header_t *,
	           cups_weave_t *band);
void	ProcessLine(ppd_file_t *, cups_raster_t *,
	            cups_page_header_t *, const int y);


/*
 * 'Setup()' - Prepare a printer for graphics output.
 */

void
Setup(ppd_file_t *ppd)		/* I - PPD file */
{
 /*
  * Some EPSON printers need an additional command issued at the
  * beginning of each job to exit from USB "packet" mode...
  */

  if (ppd->model_number & ESCP_USB)
    cupsWritePrintData("\000\000\000\033\001@EJL 1284.4\n@EJL     \n\033@", 29);
}


/*
 * 'StartPage()' - Start a page of graphics.
 */

void
StartPage(ppd_file_t         *ppd,	/* I - PPD file */
          cups_page_header_t *header)	/* I - Page header */
{
  int		i, y;			/* Looping vars */
  int		subrow,			/* Current subrow */
		modrow,			/* Subrow modulus */
		plane;			/* Current color plane */
  unsigned char	*ptr;			/* Pointer into dot buffer */
  int		bands;			/* Number of bands to allocate */
  int		units;			/* Units for resolution */
  cups_weave_t	*band;			/* Current band */
  const char	*colormodel;		/* Color model string */
  char		resolution[PPD_MAX_NAME],
					/* Resolution string */
		spec[PPD_MAX_NAME];	/* PPD attribute name */
  ppd_attr_t	*attr;			/* Attribute from PPD file */
  const float	default_lut[2] =	/* Default dithering lookup table */
		{
		  0.0,
		  1.0
		};


  fprintf(stderr, "DEBUG: StartPage...\n");
  fprintf(stderr, "DEBUG: MediaClass = \"%s\"\n", header->MediaClass);
  fprintf(stderr, "DEBUG: MediaColor = \"%s\"\n", header->MediaColor);
  fprintf(stderr, "DEBUG: MediaType = \"%s\"\n", header->MediaType);
  fprintf(stderr, "DEBUG: OutputType = \"%s\"\n", header->OutputType);

  fprintf(stderr, "DEBUG: AdvanceDistance = %d\n", header->AdvanceDistance);
  fprintf(stderr, "DEBUG: AdvanceMedia = %d\n", header->AdvanceMedia);
  fprintf(stderr, "DEBUG: Collate = %d\n", header->Collate);
  fprintf(stderr, "DEBUG: CutMedia = %d\n", header->CutMedia);
  fprintf(stderr, "DEBUG: Duplex = %d\n", header->Duplex);
  fprintf(stderr, "DEBUG: HWResolution = [ %d %d ]\n", header->HWResolution[0],
          header->HWResolution[1]);
  fprintf(stderr, "DEBUG: ImagingBoundingBox = [ %d %d %d %d ]\n",
          header->ImagingBoundingBox[0], header->ImagingBoundingBox[1],
          header->ImagingBoundingBox[2], header->ImagingBoundingBox[3]);
  fprintf(stderr, "DEBUG: InsertSheet = %d\n", header->InsertSheet);
  fprintf(stderr, "DEBUG: Jog = %d\n", header->Jog);
  fprintf(stderr, "DEBUG: LeadingEdge = %d\n", header->LeadingEdge);
  fprintf(stderr, "DEBUG: Margins = [ %d %d ]\n", header->Margins[0],
          header->Margins[1]);
  fprintf(stderr, "DEBUG: ManualFeed = %d\n", header->ManualFeed);
  fprintf(stderr, "DEBUG: MediaPosition = %d\n", header->MediaPosition);
  fprintf(stderr, "DEBUG: MediaWeight = %d\n", header->MediaWeight);
  fprintf(stderr, "DEBUG: MirrorPrint = %d\n", header->MirrorPrint);
  fprintf(stderr, "DEBUG: NegativePrint = %d\n", header->NegativePrint);
  fprintf(stderr, "DEBUG: NumCopies = %d\n", header->NumCopies);
  fprintf(stderr, "DEBUG: Orientation = %d\n", header->Orientation);
  fprintf(stderr, "DEBUG: OutputFaceUp = %d\n", header->OutputFaceUp);
  fprintf(stderr, "DEBUG: PageSize = [ %d %d ]\n", header->PageSize[0],
          header->PageSize[1]);
  fprintf(stderr, "DEBUG: Separations = %d\n", header->Separations);
  fprintf(stderr, "DEBUG: TraySwitch = %d\n", header->TraySwitch);
  fprintf(stderr, "DEBUG: Tumble = %d\n", header->Tumble);
  fprintf(stderr, "DEBUG: cupsWidth = %d\n", header->cupsWidth);
  fprintf(stderr, "DEBUG: cupsHeight = %d\n", header->cupsHeight);
  fprintf(stderr, "DEBUG: cupsMediaType = %d\n", header->cupsMediaType);
  fprintf(stderr, "DEBUG: cupsBitsPerColor = %d\n", header->cupsBitsPerColor);
  fprintf(stderr, "DEBUG: cupsBitsPerPixel = %d\n", header->cupsBitsPerPixel);
  fprintf(stderr, "DEBUG: cupsBytesPerLine = %d\n", header->cupsBytesPerLine);
  fprintf(stderr, "DEBUG: cupsColorOrder = %d\n", header->cupsColorOrder);
  fprintf(stderr, "DEBUG: cupsColorSpace = %d\n", header->cupsColorSpace);
  fprintf(stderr, "DEBUG: cupsCompression = %d\n", header->cupsCompression);
  fprintf(stderr, "DEBUG: cupsRowCount = %d\n", header->cupsRowCount);
  fprintf(stderr, "DEBUG: cupsRowFeed = %d\n", header->cupsRowFeed);
  fprintf(stderr, "DEBUG: cupsRowStep = %d\n", header->cupsRowStep);

 /*
  * Figure out the color model and spec strings...
  */

  switch (header->cupsColorSpace)
  {
    case CUPS_CSPACE_K :
        colormodel = "Black";
	break;
    case CUPS_CSPACE_W :
        colormodel = "Gray";
	break;
    default :
    case CUPS_CSPACE_RGB :
        colormodel = "RGB";
	break;
    case CUPS_CSPACE_CMYK :
        colormodel = "CMYK";
	break;
  }

  if (header->HWResolution[0] != header->HWResolution[1])
    snprintf(resolution, sizeof(resolution), "%dx%ddpi",
             header->HWResolution[0], header->HWResolution[1]);
  else
    snprintf(resolution, sizeof(resolution), "%ddpi",
             header->HWResolution[0]);

  if (!header->MediaType[0])
    strcpy(header->MediaType, "Plain");

 /*
  * Load the appropriate color profiles...
  */

  RGB  = NULL;
  CMYK = NULL;

  fputs("DEBUG: Attempting to load color profiles using the following values:\n", stderr);
  fprintf(stderr, "DEBUG: ColorModel = %s\n", colormodel);
  fprintf(stderr, "DEBUG: MediaType = %s\n", header->MediaType);
  fprintf(stderr, "DEBUG: Resolution = %s\n", resolution);

  if (header->cupsColorSpace == CUPS_CSPACE_RGB ||
      header->cupsColorSpace == CUPS_CSPACE_W)
    RGB = cupsRGBLoad(ppd, colormodel, header->MediaType, resolution);
  else
    RGB = NULL;

  CMYK = cupsCMYKLoad(ppd, colormodel, header->MediaType, resolution);

  if (RGB)
    fputs("DEBUG: Loaded RGB separation from PPD.\n", stderr);

  if (CMYK)
    fputs("DEBUG: Loaded CMYK separation from PPD.\n", stderr);
  else
  {
    fputs("DEBUG: Loading default CMYK separation.\n", stderr);
    CMYK = cupsCMYKNew(4);
  }

  PrinterPlanes = CMYK->num_channels;

  fprintf(stderr, "DEBUG: PrinterPlanes = %d\n", PrinterPlanes);

 /*
  * Get the dithering parameters...
  */

  switch (PrinterPlanes)
  {
    case 1 : /* K */
        DitherLuts[0] = cupsLutLoad(ppd, colormodel, header->MediaType,
	                            resolution, "Black");
        break;

    case 2 : /* Kk */
        DitherLuts[0] = cupsLutLoad(ppd, colormodel, header->MediaType,
	                            resolution, "Black");
        DitherLuts[1] = cupsLutLoad(ppd, colormodel, header->MediaType,
	                            resolution, "LightBlack");
        break;

    case 3 : /* CMY */
        DitherLuts[0] = cupsLutLoad(ppd, colormodel, header->MediaType,
	                            resolution, "Cyan");
        DitherLuts[1] = cupsLutLoad(ppd, colormodel, header->MediaType,
	                            resolution, "Magenta");
        DitherLuts[2] = cupsLutLoad(ppd, colormodel, header->MediaType,
	                            resolution, "Yellow");
        break;

    case 4 : /* CMYK */
        DitherLuts[0] = cupsLutLoad(ppd, colormodel, header->MediaType,
	                            resolution, "Cyan");
        DitherLuts[1] = cupsLutLoad(ppd, colormodel, header->MediaType,
	                            resolution, "Magenta");
        DitherLuts[2] = cupsLutLoad(ppd, colormodel, header->MediaType,
	                            resolution, "Yellow");
        DitherLuts[3] = cupsLutLoad(ppd, colormodel, header->MediaType,
	                            resolution, "Black");
        break;

    case 6 : /* CcMmYK */
        DitherLuts[0] = cupsLutLoad(ppd, colormodel, header->MediaType,
	                            resolution, "Cyan");
        DitherLuts[1] = cupsLutLoad(ppd, colormodel, header->MediaType,
	                            resolution, "LightCyan");
        DitherLuts[2] = cupsLutLoad(ppd, colormodel, header->MediaType,
	                            resolution, "Magenta");
        DitherLuts[3] = cupsLutLoad(ppd, colormodel, header->MediaType,
	                            resolution, "LightMagenta");
        DitherLuts[4] = cupsLutLoad(ppd, colormodel, header->MediaType,
	                            resolution, "Yellow");
        DitherLuts[5] = cupsLutLoad(ppd, colormodel, header->MediaType,
	                            resolution, "Black");
        break;

    case 7 : /* CcMmYKk */
        DitherLuts[0] = cupsLutLoad(ppd, colormodel, header->MediaType,
	                            resolution, "Cyan");
        DitherLuts[1] = cupsLutLoad(ppd, colormodel, header->MediaType,
	                            resolution, "LightCyan");
        DitherLuts[2] = cupsLutLoad(ppd, colormodel, header->MediaType,
	                            resolution, "Magenta");
        DitherLuts[3] = cupsLutLoad(ppd, colormodel, header->MediaType,
	                            resolution, "LightMagenta");
        DitherLuts[4] = cupsLutLoad(ppd, colormodel, header->MediaType,
	                            resolution, "Yellow");
        DitherLuts[5] = cupsLutLoad(ppd, colormodel, header->MediaType,
	                            resolution, "Black");
        DitherLuts[6] = cupsLutLoad(ppd, colormodel, header->MediaType,
	                            resolution, "LightBlack");
        break;
  }

  for (plane = 0; plane < PrinterPlanes; plane ++)
  {
    DitherStates[plane] = cupsDitherNew(header->cupsWidth);

    if (!DitherLuts[plane])
      DitherLuts[plane] = cupsLutNew(2, default_lut);
  }

  if (DitherLuts[0][4095].pixel > 1)
    BitPlanes = 2;
  else
    BitPlanes = 1;

 /*
  * Initialize the printer...
  */

  printf("\033@");

  if (ppd->model_number & ESCP_REMOTE)
  {
   /*
    * Go into remote mode...
    */

    cupsWritePrintData("\033(R\010\000\000REMOTE1", 13);

   /*
    * Disable status reporting...
    */

    cupsWritePrintData("ST\002\000\000\000", 6);

   /*
    * Enable borderless printing...
    */

    if ((attr = ppdFindAttr(ppd, "cupsESCPFP", NULL)) != NULL && attr->value)
    {
     /*
      * Set horizontal offset...
      */

      i = atoi(attr->value);

      cupsWritePrintData("FP\003\000\000", 5);
      putchar(i & 255);
      putchar(i >> 8);
    }

   /*
    * Set media type...
    */

    if (header->cupsMediaType)
    {
      sprintf(spec, "%d", header->cupsMediaType);

      if ((attr = ppdFindAttr(ppd, "cupsESCPSN0", spec)) != NULL && attr->value)
      {
       /*
        * Set feed sequence...
	*/

	cupsWritePrintData("SN\003\000\000\000", 6);
	putchar(atoi(attr->value));
      }

      if ((attr = ppdFindAttr(ppd, "cupsESCPSN1", spec)) != NULL && attr->value)
      {
       /*
        * Set platten gap...
	*/

	cupsWritePrintData("SN\003\000\000\001", 6);
	putchar(atoi(attr->value));
      }

      if ((attr = ppdFindAttr(ppd, "cupsESCPSN2", spec)) != NULL && attr->value)
      {
       /*
        * Paper feeding/ejecting sequence...
	*/

	cupsWritePrintData("SN\003\000\000\002", 6);
	putchar(atoi(attr->value));
      }

      if ((attr = ppdFindAttr(ppd, "cupsESCPSN6", spec)) != NULL && attr->value)
      {
       /*
        * Eject delay...
	*/

        cupsWritePrintData("SN\003\000\000\006", 6);
        putchar(atoi(attr->value));
      }

      if ((attr = ppdFindAttr(ppd, "cupsESCPMT", spec)) != NULL && attr->value)
      {
       /*
        * Set media type.
	*/

	cupsWritePrintData("MT\003\000\000\000", 6);
        putchar(atoi(attr->value));
      }

      if ((attr = ppdFindAttr(ppd, "cupsESCPPH", spec)) != NULL && attr->value)
      {
       /*
        * Set paper thickness.
        */

	cupsWritePrintData("PH\002\000\000", 5);
        putchar(atoi(attr->value));
      }
    }

    sprintf(spec, "%d", header->MediaPosition);

    if (header->MediaPosition)
    {
      if ((attr = ppdFindAttr(ppd, "cupsESCPPC", spec)) != NULL && attr->value)
      {
       /*
	* Paper check.
	*/

	cupsWritePrintData("PC\002\000\000", 5);
        putchar(atoi(attr->value));
      }

      if ((attr = ppdFindAttr(ppd, "cupsESCPPP", spec)) != NULL && attr->value)
      {
       /*
	* Paper path.
	*/

        int a, b;

        a = b = 0;
        sscanf(attr->value, "%d%d", &a, &b);

	cupsWritePrintData("PP\003\000\000", 5);
        putchar(a);
        putchar(b);
      }

      if ((attr = ppdFindAttr(ppd, "cupsESCPEX", spec)) != NULL && attr->value)
      {
       /*
	* Set media position.
	*/

	cupsWritePrintData("EX\006\000\000\000\000\000\005", 9);
        putchar(atoi(attr->value));
      }
    }

    if ((attr = ppdFindAttr(ppd, "cupsESCPMS", spec)) != NULL && attr->value)
    {
     /*
      * Set media size...
      */

      cupsWritePrintData("MS\010\000\000", 5);
      putchar(atoi(attr->value));

      switch (header->PageSize[1])
      {
        case 1191 :	/* A3 */
	    putchar(0x01);
	    putchar(0x00);
	    putchar(0x00);
	    putchar(0x00);
	    putchar(0x00);
	    putchar(0x00);
	    break;
	case 1032 :	/* B4 */
	    putchar(0x02);
	    putchar(0x00);
	    putchar(0x00);
	    putchar(0x00);
	    putchar(0x00);
	    putchar(0x00);
	    break;
	case 842 :	/* A4 */
	    putchar(0x03);
	    putchar(0x00);
	    putchar(0x00);
	    putchar(0x00);
	    putchar(0x00);
	    putchar(0x00);
	    break;
	case 595 :	/* A4.Transverse */
	    putchar(0x03);
	    putchar(0x01);
	    putchar(0x00);
	    putchar(0x00);
	    putchar(0x00);
	    putchar(0x00);
	    break;
	case 729 :	/* B5 */
	    putchar(0x04);
	    putchar(0x00);
	    putchar(0x00);
	    putchar(0x00);
	    putchar(0x00);
	    putchar(0x00);
	    break;
	case 516 :	/* B5.Transverse */
	    putchar(0x04);
	    putchar(0x01);
	    putchar(0x00);
	    putchar(0x00);
	    putchar(0x00);
	    putchar(0x00);
	    break;
	case 1369 :	/* Super A3/B */
	    putchar(0x20);
	    putchar(0x00);
	    putchar(0x00);
	    putchar(0x00);
	    putchar(0x00);
	    putchar(0x00);
	    break;
	case 792 :	/* Letter */
	    putchar(0x08);
	    putchar(0x00);
	    putchar(0x00);
	    putchar(0x00);
	    putchar(0x00);
	    putchar(0x00);
	    break;
	case 612 :	/* Letter.Transverse */
	    putchar(0x08);
	    putchar(0x01);
	    putchar(0x00);
	    putchar(0x00);
	    putchar(0x00);
	    putchar(0x00);
	    break;
	case 1004 :	/* Legal */
	    putchar(0x0a);
	    putchar(0x00);
	    putchar(0x00);
	    putchar(0x00);
	    putchar(0x00);
	    putchar(0x00);
	    break;
	case 1224 :	/* Tabloid */
	    putchar(0x2d);
	    putchar(0x00);
	    putchar(0x00);
	    putchar(0x00);
	    putchar(0x00);
	    putchar(0x00);
	    break;
	default :	/* Custom size */
	    putchar(0xff);
	    putchar(0xff);
	    i = 360 * header->PageSize[0] / 72;
	    putchar(i);
	    putchar(i >> 8);
	    i = 360 * header->PageSize[1] / 72;
	    putchar(i);
	    putchar(i >> 8);
	    break;
      }
    }

    sprintf(spec, "%d", header->CutMedia);

    if ((attr = ppdFindAttr(ppd, "cupsESCPAC", spec)) != NULL && attr->value)
    {
     /*
      * Enable/disable cutter.
      */

      cupsWritePrintData("AC\002\000\000", 5);
      putchar(atoi(attr->value));

      if ((attr = ppdFindAttr(ppd, "cupsESCPSN80", header->MediaType)) != NULL && attr->value)
      {
       /*
	* Cutting method...
	*/

	cupsWritePrintData("SN\003\000\000\200", 6);
	putchar(atoi(attr->value));
      }

      if ((attr = ppdFindAttr(ppd, "cupsESCPSN81", header->MediaType)) != NULL && attr->value)
      {
       /*
	* Cutting pressure...
	*/

	cupsWritePrintData("SN\003\000\000\201", 6);
	putchar(atoi(attr->value));
      }
    }

    if ((attr = ppdFindAttr(ppd, "cupsESCPCO", spec)) != NULL && attr->value)
    {
     /*
      * Enable/disable cutter.
      */

      cupsWritePrintData("CO\010\000\000\000", 6);
      putchar(atoi(attr->value));
      cupsWritePrintData("\000\000\000\000\000", 5);
    }

   /*
    * Exit remote mode...
    */

    cupsWritePrintData("\033\000\000\000", 4);
  }

 /*
  * Enter graphics mode...
  */

  cupsWritePrintData("\033(G\001\000\001", 6);

 /*
  * Set the line feed increment...
  */

  /* TODO: get this from the PPD file... */
  for (units = 1440; units < header->HWResolution[0]; units *= 2);

  if (ppd->model_number & ESCP_EXT_UNITS)
  {
    cupsWritePrintData("\033(U\005\000", 5);
    putchar(units / header->HWResolution[1]);
    putchar(units / header->HWResolution[1]);
    putchar(units / header->HWResolution[0]);
    putchar(units);
    putchar(units >> 8);
  }
  else
  {
    cupsWritePrintData("\033(U\001\000", 5);
    putchar(3600 / header->HWResolution[1]);
  }

 /*
  * Set the page length...
  */

  PrinterLength = header->PageSize[1] * header->HWResolution[1] / 72;

  if (ppd->model_number & ESCP_PAGE_SIZE)
  {
   /*
    * Set page size (expands bottom margin)...
    */

    cupsWritePrintData("\033(S\010\000", 5);

    i = header->PageSize[0] * header->HWResolution[1] / 72;
    putchar(i);
    putchar(i >> 8);
    putchar(i >> 16);
    putchar(i >> 24);

    i = header->PageSize[1] * header->HWResolution[1] / 72;
    putchar(i);
    putchar(i >> 8);
    putchar(i >> 16);
    putchar(i >> 24);
  }
  else
  {
    cupsWritePrintData("\033(C\002\000", 5);
    putchar(PrinterLength & 255);
    putchar(PrinterLength >> 8);
  }

 /*
  * Set the top and bottom margins...
  */

  PrinterTop = (int)((ppd->sizes[1].length - ppd->sizes[1].top) *
                     header->HWResolution[1] / 72.0);

  if (ppd->model_number & ESCP_EXT_MARGINS)
  {
    cupsWritePrintData("\033(c\010\000", 5);

    putchar(PrinterTop);
    putchar(PrinterTop >> 8);
    putchar(PrinterTop >> 16);
    putchar(PrinterTop >> 24);

    putchar(PrinterLength);
    putchar(PrinterLength >> 8);
    putchar(PrinterLength >> 16);
    putchar(PrinterLength >> 24);
  }
  else
  {
    cupsWritePrintData("\033(c\004\000", 5);

    putchar(PrinterTop & 255);
    putchar(PrinterTop >> 8);

    putchar(PrinterLength & 255);
    putchar(PrinterLength >> 8);
  }

 /*
  * Set the top position...
  */

  cupsWritePrintData("\033(V\002\000\000\000", 7);

 /*
  * Enable unidirectional printing depending on the mode...
  */

  if ((attr = cupsFindAttr(ppd, "cupsESCPDirection", colormodel,
                           header->MediaType, resolution, spec,
			   sizeof(spec))) != NULL)
    printf("\033U%c", atoi(attr->value));

 /*
  * Enable/disable microweaving as needed...
  */

  if ((attr = cupsFindAttr(ppd, "cupsESCPMicroWeave", colormodel,
                           header->MediaType, resolution, spec,
			   sizeof(spec))) != NULL)
    printf("\033(i\001%c%c", 0, atoi(attr->value));

 /*
  * Set the dot size and print speed as needed...
  */

  if ((attr = cupsFindAttr(ppd, "cupsESCPDotSize", colormodel,
                           header->MediaType, resolution, spec,
			   sizeof(spec))) != NULL)
    printf("\033(e\002%c%c%c", 0, 0, atoi(attr->value));

  if (ppd->model_number & ESCP_ESCK)
  {
   /*
    * Set the print mode...
    */

    if (PrinterPlanes == 1)
    {
     /*
      * Fast black printing.
      */

      cupsWritePrintData("\033(K\002\000\000\001", 7);
    }
    else
    {
     /*
      * Color printing.
      */

      cupsWritePrintData("\033(K\002\000\000\002", 7);
    }
  }

 /*
  * Get softweave settings from header...
  */

  if (header->cupsRowCount <= 1)
  {
    DotRowCount = 1;
    DotColStep  = 1;
    DotRowStep  = 1;
    DotRowFeed  = 1;
  }
  else
  {
    DotRowCount = header->cupsRowCount;
    DotRowFeed  = header->cupsRowFeed;
    DotRowStep  = header->cupsRowStep % 100;
    DotColStep  = header->cupsRowStep / 100;

    if (DotColStep == 0)
      DotColStep ++;
  }

 /*
  * Setup softweave parameters...
  */

  DotRowCurrent = 0;
  DotRowMax     = DotRowCount * DotRowStep;
  DotBufferSize = (header->cupsWidth / DotColStep * BitPlanes + 7) / 8;

  fprintf(stderr, "DEBUG: DotBufferSize = %d\n", DotBufferSize);
  fprintf(stderr, "DEBUG: DotColStep = %d\n", DotColStep);
  fprintf(stderr, "DEBUG: DotRowMax = %d\n", DotRowMax);
  fprintf(stderr, "DEBUG: DotRowStep = %d\n", DotRowStep);
  fprintf(stderr, "DEBUG: DotRowFeed = %d\n", DotRowFeed);
  fprintf(stderr, "DEBUG: DotRowCount = %d\n", DotRowCount);

  DotAvailList  = NULL;
  DotUsedList   = NULL;
  DotBuffers[0] = NULL;

  fprintf(stderr, "DEBUG: model_number = %x\n", ppd->model_number);

  if (DotRowMax > 1)
  {
   /*
    * Compute offsets for the color jets on the print head...
    */

    bands = DotRowStep * DotColStep * PrinterPlanes * 4;

    memset(DotRowOffset, 0, sizeof(DotRowOffset));

    if (PrinterPlanes == 1)
    {
     /*
      * Use full height of print head...
      */

      if ((attr = ppdFindAttr(ppd, "cupsESCPBlack", resolution)) != NULL &&
          attr->value)
      {
       /*
        * Use custom black head data...
	*/

        sscanf(attr->value, "%d%d", &DotRowCount, &DotRowStep);
      }
    }
    else if (ppd->model_number & ESCP_STAGGER)
    {
     /*
      * Use staggered print head...
      */

      fputs("DEBUG: Offset head detected...\n", stderr);

      if ((attr = ppdFindAttr(ppd, "cupsESCPOffsets", resolution)) != NULL &&
          attr->value)
      {
       /*
        * Use only 1/3 of the print head when printing color...
	*/

        sscanf(attr->value, "%d%d%d%d", DotRowOffset + 0,
	       DotRowOffset + 1, DotRowOffset + 2, DotRowOffset + 3);
      }
    }

    for (i = 0; i < PrinterPlanes; i ++)
      fprintf(stderr, "DEBUG: DotRowOffset[%d] = %d\n", i, DotRowOffset[i]);

   /*
    * Allocate bands...
    */

    for (i = 0; i < bands; i ++)
    {
      band         = (cups_weave_t *)calloc(1, sizeof(cups_weave_t));
      band->next   = DotAvailList;
      DotAvailList = band;

      band->buffer = calloc(DotRowCount, DotBufferSize);
    }

    fputs("DEBUG: Pointer list at start of page...\n", stderr);

    for (band = DotAvailList; band != NULL; band = band->next)
      fprintf(stderr, "DEBUG: %p\n", band);

    fputs("DEBUG: ----END----\n", stderr);

   /*
    * Fill the initial bands...
    */

    modrow = DotColStep * DotRowStep;

    if (DotRowFeed == 0)
    {
     /*
      * Automatically compute the optimal feed value...
      */

      DotRowFeed = DotRowCount / DotColStep - DotRowStep;

      while ((((DotRowFeed % 2) == 0) == ((DotRowCount % 2) == 0) ||
              ((DotRowFeed % 3) == 0) == ((DotRowCount % 3) == 0) ||
              ((DotRowFeed % 5) == 0) == ((DotRowCount % 5) == 0)) &&
	     DotRowFeed > 1)
	DotRowFeed --;

      if (DotRowFeed < 1)
	DotRowFeed = 1;

      fprintf(stderr, "DEBUG: Auto DotRowFeed = %d, modrow=%d...\n",
              DotRowFeed, modrow);
    }

    memset(DotBands, 0, sizeof(DotBands));

    for (i = modrow, subrow = modrow - 1, y = DotRowFeed;
	 i > 0;
	 i --, y += DotRowFeed)
    {
      while (DotBands[subrow][0])
      {
       /*
        * This subrow is already used, move to another one...
	*/

	subrow = (subrow + 1) % modrow;
      }

      for (plane = 0; plane < PrinterPlanes; plane ++)
      {
       /*
        * Pull the next available band from the list...
	*/

        band                    = DotAvailList;
	DotAvailList            = DotAvailList->next;
	DotBands[subrow][plane] = band;

       /*
        * Start the band in the first few passes, with the number of rows
	* varying to allow for a nice interleaved pattern...
	*/

        band->x     = subrow / DotRowStep;
        band->y     = (subrow % DotRowStep) + DotRowOffset[plane];
	band->plane = plane;
	band->row   = 0;
	band->count = DotRowCount - y / DotRowStep;

        if (band->count < 1)
	  band->count = 1;
	else if (band->count > DotRowCount)
	  band->count = DotRowCount;

	fprintf(stderr, "DEBUG: DotBands[%d][%d] = %p, x = %d, y = %d, plane = %d, count = %d\n",
	        subrow, plane, band, band->x, band->y, band->plane, band->count);
      }

      subrow = (subrow + DotRowFeed) % modrow;
    }
  }
  else
  {
   /*
    * Allocate memory for a single line of graphics...
    */

    ptr = calloc(PrinterPlanes, DotBufferSize);

    for (plane = 0; plane < PrinterPlanes; plane ++, ptr += DotBufferSize)
      DotBuffers[plane] = ptr;
  }

 /*
  * Set the output resolution...
  */

  cupsWritePrintData("\033(D\004\000", 5);
  putchar(units);
  putchar(units >> 8);
  putchar(units * DotRowStep / header->HWResolution[1]);
  putchar(units * DotColStep / header->HWResolution[0]);

 /*
  * Set the top of form...
  */

  OutputFeed = 0;

 /*
  * Allocate buffers as needed...
  */

  PixelBuffer      = malloc(header->cupsBytesPerLine);
  InputBuffer      = malloc(header->cupsWidth * PrinterPlanes * 2);
  OutputBuffers[0] = malloc(PrinterPlanes * header->cupsWidth);

  for (i = 1; i < PrinterPlanes; i ++)
    OutputBuffers[i] = OutputBuffers[0] + i * header->cupsWidth;

  if (RGB)
    CMYKBuffer = malloc(header->cupsWidth * PrinterPlanes);

  CompBuffer = malloc(10 * DotBufferSize * DotRowMax);
}


/*
 * 'EndPage()' - Finish a page of graphics.
 */

void
EndPage(ppd_file_t         *ppd,	/* I - PPD file */
        cups_page_header_t *header)	/* I - Page header */
{
  int		i;			/* Looping var */
  cups_weave_t	*band,			/* Current band */
		*next;			/* Next band in list */
  int		plane;			/* Current plane */
  int		subrow;			/* Current subrow */
  int		subrows;		/* Number of subrows */


 /*
  * Output the last bands of print data as necessary...
  */

  if (DotRowMax > 1)
  {
   /*
    * Move the remaining bands to the used or avail lists...
    */

    subrows = DotRowStep * DotColStep;

    for (subrow = 0; subrow < subrows; subrow ++)
      for (plane = 0; plane < PrinterPlanes; plane ++)
      {
        if (DotBands[subrow][plane]->dirty)
	{
	 /*
	  * Insert into the used list...
	  */

          DotBands[subrow][plane]->count = DotBands[subrow][plane]->row;

          AddBand(DotBands[subrow][plane]);
	}
	else
	{
	 /*
	  * Nothing here, so move it to the available list...
	  */

	  DotBands[subrow][plane]->next = DotAvailList;
	  DotAvailList                  = DotBands[subrow][plane];
	}

	DotBands[subrow][plane] = NULL;
      }

   /*
    * Loop until all bands are written...
    */

    fputs("DEBUG: Pointer list at end of page...\n", stderr);

    for (band = DotUsedList; band != NULL; band = band->next)
      fprintf(stderr, "DEBUG: %p (used)\n", band);
    for (band = DotAvailList; band != NULL; band = band->next)
      fprintf(stderr, "DEBUG: %p (avail)\n", band);

    fputs("DEBUG: ----END----\n", stderr);

    for (band = DotUsedList; band != NULL; band = next)
    {
      next = band->next;

      OutputBand(ppd, header, band);

      fprintf(stderr, "DEBUG: freeing used band %p, prev = %p, next = %p\n",
              band, band->prev, band->next);

      free(band->buffer);
      free(band);
    }

   /*
    * Free memory for the available bands, if any...
    */

    for (band = DotAvailList; band != NULL; band = next)
    {
      next = band->next;

      fprintf(stderr, "DEBUG: freeing avail band %p, prev = %p,  next = %p\n",
              band, band->prev, band->next);

      free(band->buffer);
      free(band);
    }
  }
  else
    free(DotBuffers[0]);

 /*
  * Output a page eject sequence...
  */

  putchar(12);

 /*
  * Free memory for the page...
  */

  for (i = 0; i < PrinterPlanes; i ++)
  {
    cupsDitherDelete(DitherStates[i]);
    cupsLutDelete(DitherLuts[i]);
  }

  free(OutputBuffers[0]);

  free(PixelBuffer);
  free(InputBuffer);
  free(CompBuffer);

  cupsCMYKDelete(CMYK);

  if (RGB)
  {
    cupsRGBDelete(RGB);
    free(CMYKBuffer);
  }
}


/*
 * 'Shutdown()' - Shutdown a printer.
 */

void
Shutdown(ppd_file_t *ppd)		/* I - PPD file */
{
 /*
  * Reset the printer...
  */

  printf("\033@");

  if (ppd->model_number & ESCP_REMOTE)
  {
   /*
    * Go into remote mode...
    */

    cupsWritePrintData("\033(R\010\000\000REMOTE1", 13);

   /*
    * Load defaults...
    */

    cupsWritePrintData("LD\000\000", 4);

   /*
    * Exit remote mode...
    */

    cupsWritePrintData("\033\000\000\000", 4);
  }
}


/*
 * 'AddBand()' - Add a band of data to the used list.
 */

void
AddBand(cups_weave_t *band)			/* I - Band to add */
{
  cups_weave_t	*current,			/* Current band */
		*prev;				/* Previous band */


  if (band->count < 1)
    return;

  for (current = DotUsedList, prev = NULL;
       current != NULL;
       prev = current, current = current->next)
    if (band->y < current->y ||
        (band->y == current->y && band->x < current->x) ||
	(band->y == current->y && band->x == current->x &&
	 band->plane < current->plane))
      break;

  if (current != NULL)
  {
   /*
    * Insert the band...
    */

    band->next    = current;
    band->prev    = prev;
    current->prev = band;

    if (prev != NULL)
      prev->next = band;
    else
      DotUsedList = band;
  }
  else if (prev != NULL)
  {
   /*
    * Append the band to the end...
    */

    band->prev = prev;
    prev->next = band;
    band->next = NULL;
  }
  else
  {
   /*
    * First band in list...
    */

    DotUsedList = band;
    band->prev  = NULL;
    band->next  = NULL;
  }
}


/*
 * 'CompressData()' - Compress a line of graphics.
 */

void
CompressData(ppd_file_t          *ppd,	/* I - PPD file information */
             const unsigned char *line,	/* I - Data to compress */
             const int           length,/* I - Number of bytes */
	     int                 plane,	/* I - Color plane */
	     int                 type,	/* I - Type of compression */
	     const int           rows,	/* I - Number of lines to write */
	     const int           xstep,	/* I - Spacing between columns */
	     const int           ystep,	/* I - Spacing between lines */
	     const int           offset)/* I - Head offset */
{
  register const unsigned char *line_ptr,
					/* Current byte pointer */
        	*line_end,		/* End-of-line byte pointer */
        	*start;			/* Start of compression sequence */
  register unsigned char *comp_ptr;	/* Pointer into compression buffer */
  register int  count;			/* Count of bytes for output */
  register int	bytes;			/* Number of bytes per row */
  static int	ctable[7][7] =		/* Colors */
		{
		  {  0,  0,  0,  0,  0,  0,  0 },	/* K */
		  {  0, 16,  0,  0,  0,  0,  0 },	/* Kk */
		  {  2,  1,  4,  0,  0,  0,  0 },	/* CMY */
		  {  2,  1,  4,  0,  0,  0,  0 },	/* CMYK */
		  {  0,  0,  0,  0,  0,  0,  0 },
		  {  2, 18,  1, 17,  4,  0,  0 },	/* CcMmYK */
		  {  2, 18,  1, 17,  4,  0, 16 },	/* CcMmYKk */
		};


  switch (type)
  {
    case 0 :
       /*
	* Do no compression...
	*/

	line_ptr = (const unsigned char *)line;
	line_end = (const unsigned char *)line + length;
	break;

    default :
       /*
        * Do TIFF pack-bits encoding...
        */

	line_ptr = (const unsigned char *)line;
	line_end = (const unsigned char *)line + length;
	comp_ptr = CompBuffer;

	while (line_ptr < line_end && (comp_ptr - CompBuffer) < length)
	{
	  if ((line_ptr + 1) >= line_end)
	  {
	   /*
	    * Single byte on the end...
	    */

	    *comp_ptr++ = 0x00;
	    *comp_ptr++ = *line_ptr++;
	  }
	  else if (line_ptr[0] == line_ptr[1])
	  {
	   /*
	    * Repeated sequence...
	    */

	    line_ptr ++;
	    count = 2;

	    while (line_ptr < (line_end - 1) &&
        	   line_ptr[0] == line_ptr[1] &&
        	   count < 127)
	    {
              line_ptr ++;
              count ++;
	    }

	    *comp_ptr++ = 257 - count;
	    *comp_ptr++ = *line_ptr++;
	  }
	  else
	  {
	   /*
	    * Non-repeated sequence...
	    */

	    start    = line_ptr;
	    line_ptr ++;
	    count    = 1;

	    while (line_ptr < (line_end - 1) &&
        	   line_ptr[0] != line_ptr[1] &&
        	   count < 127)
	    {
              line_ptr ++;
              count ++;
	    }

	    *comp_ptr++ = count - 1;

	    memcpy(comp_ptr, start, count);
	    comp_ptr += count;
	  }
	}

        if ((comp_ptr - CompBuffer) < length)
	{
          line_ptr = (const unsigned char *)CompBuffer;
          line_end = (const unsigned char *)comp_ptr;
	}
	else
	{
	  type     = 0;
	  line_ptr = (const unsigned char *)line;
	  line_end = (const unsigned char *)line + length;
	}
	break;
  }

 /*
  * Position the print head...
  */

  putchar(0x0d);

  if (offset)
  {
    if (BitPlanes == 1)
      cupsWritePrintData("\033(\\\004\000\240\005", 7);
    else
      printf("\033\\");

    putchar(offset);
    putchar(offset >> 8);
  }

 /*
  * Send the graphics...
  */

  bytes = length / rows;

  if (ppd->model_number & ESCP_RASTER_ESCI)
  {
   /*
    * Send graphics with ESC i command.
    */

    printf("\033i");
    putchar(ctable[PrinterPlanes - 1][plane]);
    putchar(type != 0);
    putchar(BitPlanes);
    putchar(bytes & 255);
    putchar(bytes >> 8);
    putchar(rows & 255);
    putchar(rows >> 8);
  }
  else
  {
   /*
    * Set the color if necessary...
    */

    if (PrinterPlanes > 1)
    {
      plane = ctable[PrinterPlanes - 1][plane];

      if (plane & 0x10)
	printf("\033(r%c%c%c%c", 2, 0, 1, plane & 0x0f);
      else
	printf("\033r%c", plane);
    }

   /*
    * Send graphics with ESC . command.
    */

    bytes *= 8;

    printf("\033.");
    putchar(type != 0);
    putchar(ystep);
    putchar(xstep);
    putchar(rows);
    putchar(bytes & 255);
    putchar(bytes >> 8);
  }

  cupsWritePrintData(line_ptr, line_end - line_ptr);
}


/*
 * 'OutputBand()' - Output a band of graphics.
 */

void
OutputBand(ppd_file_t         *ppd,	/* I - PPD file */
           cups_page_header_t *header,	/* I - Page header */
           cups_weave_t       *band)	/* I - Current band */
{
  int	xstep,				/* Spacing between columns */
	ystep;				/* Spacing between rows */


 /*
  * Interleaved ESC/P2 graphics...
  */

  OutputFeed    = band->y - DotRowCurrent;
  DotRowCurrent = band->y;

  fprintf(stderr, "DEBUG: Printing band %p, x = %d, y = %d, plane = %d, count = %d, OutputFeed = %d\n",
          band, band->x, band->y, band->plane, band->count, OutputFeed);

 /*
  * Compute step values...
  */

  xstep = 3600 * DotColStep / header->HWResolution[0];
  ystep = 3600 * DotRowStep / header->HWResolution[1];

 /*
  * Output the band...
  */

  if (OutputFeed > 0)
  {
    cupsWritePrintData("\033(v\002\000", 5);
    putchar(OutputFeed & 255);
    putchar(OutputFeed >> 8);

    OutputFeed = 0;
  }

  CompressData(ppd, band->buffer, band->count * DotBufferSize, band->plane,
	       header->cupsCompression, band->count, xstep, ystep, band->x);

 /*
  * Clear the band...
  */

  memset(band->buffer, 0, band->count * DotBufferSize);
  band->dirty = 0;

 /*
  * Flush the output buffers...
  */

  fflush(stdout);
}


/*
 * 'ProcessLine()' - Read graphics from the page stream and output as needed.
 */

void
ProcessLine(ppd_file_t         *ppd,	/* I - PPD file */
            cups_raster_t      *ras,	/* I - Raster stream */
            cups_page_header_t *header,	/* I - Page header */
            const int          y)	/* I - Current scanline */
{
  int		plane,			/* Current color plane */
		width,			/* Width of line */
		subwidth,		/* Width of interleaved row */
		subrow,			/* Subrow for interleaved output */
		offset,			/* Offset to current line */
		pass,			/* Pass number */
		xstep,			/* X step value */
		ystep;			/* Y step value */
  cups_weave_t	*band;			/* Current band */


 /*
  * Read a row of graphics...
  */

  if (!cupsRasterReadPixels(ras, PixelBuffer, header->cupsBytesPerLine))
    return;

 /*
  * Perform the color separation...
  */

  offset   = 0;
  width    = header->cupsWidth;
  subwidth = header->cupsWidth / DotColStep;
  xstep    = 3600 / header->HWResolution[0];
  ystep    = 3600 / header->HWResolution[1];

  switch (header->cupsColorSpace)
  {
    case CUPS_CSPACE_W :
        if (RGB)
	{
	  cupsRGBDoGray(RGB, PixelBuffer, CMYKBuffer, width);
	  cupsCMYKDoCMYK(CMYK, CMYKBuffer, InputBuffer, width);
	}
	else
          cupsCMYKDoGray(CMYK, PixelBuffer, InputBuffer, width);
	break;

    case CUPS_CSPACE_K :
        cupsCMYKDoBlack(CMYK, PixelBuffer, InputBuffer, width);
	break;

    default :
    case CUPS_CSPACE_RGB :
        if (RGB)
	{
	  cupsRGBDoRGB(RGB, PixelBuffer, CMYKBuffer, width);
	  cupsCMYKDoCMYK(CMYK, CMYKBuffer, InputBuffer, width);
	}
	else
          cupsCMYKDoRGB(CMYK, PixelBuffer, InputBuffer, width);
	break;

    case CUPS_CSPACE_CMYK :
        cupsCMYKDoCMYK(CMYK, PixelBuffer, InputBuffer, width);
	break;
  }

 /*
  * Dither the pixels...
  */

  for (plane = 0; plane < PrinterPlanes; plane ++)
  {
    cupsDitherLine(DitherStates[plane], DitherLuts[plane], InputBuffer + plane,
                   PrinterPlanes, OutputBuffers[plane]);

    if (DotRowMax == 1)
    {
     /*
      * Handle microweaved output...
      */

      if (cupsCheckBytes(OutputBuffers[plane], width))
	continue;

      if (BitPlanes == 1)
	cupsPackHorizontal(OutputBuffers[plane], DotBuffers[plane],
	                   width, 0, 1);
      else
	cupsPackHorizontal2(OutputBuffers[plane], DotBuffers[plane],
                	    width, 1);

      if (OutputFeed > 0)
      {
	cupsWritePrintData("\033(v\002\000", 5);
	putchar(OutputFeed & 255);
	putchar(OutputFeed >> 8);
	OutputFeed = 0;
      }

      CompressData(ppd, DotBuffers[plane], DotBufferSize, plane, 1, 1,
                   xstep, ystep, 0);
      fflush(stdout);
    }
    else
    {
     /*
      * Handle softweaved output...
      */

      for (pass = 0, subrow = y % DotRowStep;
           pass < DotColStep;
	   pass ++, subrow += DotRowStep)
      {
       /*
	* See if we need to output the band...
	*/

        band   = DotBands[subrow][plane];
	offset = band->row * DotBufferSize;

        if (BitPlanes == 1)
	  cupsPackHorizontal(OutputBuffers[plane] + pass,
	                     band->buffer + offset, subwidth, 0, DotColStep);
        else
	  cupsPackHorizontal2(OutputBuffers[plane] + pass,
	                      band->buffer + offset, subwidth, DotColStep);

        band->row ++;
	band->dirty |= !cupsCheckBytes(band->buffer + offset, DotBufferSize);
	if (band->row >= band->count)
	{
	  if (band->dirty)
	  {
	   /*
	    * Dirty band needs to be added to the used list...
	    */

	    AddBand(band);

           /*
	    * Then find a new band...
	    */

	    if (DotAvailList == NULL)
	    {
	      OutputBand(ppd, header, DotUsedList);

	      DotBands[subrow][plane] = DotUsedList;
	      DotUsedList->x          = band->x;
	      DotUsedList->y          = band->y + band->count * DotRowStep;
	      DotUsedList->plane      = band->plane;
	      DotUsedList->row        = 0;
	      DotUsedList->count      = DotRowCount;
	      DotUsedList             = DotUsedList->next;
	    }
	    else
	    {
	      DotBands[subrow][plane] = DotAvailList;
	      DotAvailList->x         = band->x;
	      DotAvailList->y         = band->y + band->count * DotRowStep;
	      DotAvailList->plane     = band->plane;
	      DotAvailList->row       = 0;
	      DotAvailList->count     = DotRowCount;
	      DotAvailList            = DotAvailList->next;
	    }
	  }
	  else
	  {
	   /*
	    * This band isn't dirty, so reuse it...
	    */

            fprintf(stderr, "DEBUG: Blank band %p, x = %d, y = %d, plane = %d, count = %d\n",
	            band, band->x, band->y, band->plane, band->count);

	    band->y     += band->count * DotRowStep;
	    band->row   = 0;
	    band->count = DotRowCount;
	  }
        }
      }
    }
  }

  if (DotRowMax == 1)
    OutputFeed ++;
}


/*
 * 'main()' - Main entry and processing of driver.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  int			fd;		/* File descriptor */
  cups_raster_t		*ras;		/* Raster stream for printing */
  cups_page_header_t	header;		/* Page header from file */
  int			page;		/* Current page */
  int			y;		/* Current line */
  ppd_file_t		*ppd;		/* PPD file */
  int			num_options;	/* Number of options */
  cups_option_t		*options;	/* Options */


 /*
  * Make sure status messages are not buffered...
  */

  setbuf(stderr, NULL);

 /*
  * Check command-line...
  */

  if (argc < 6 || argc > 7)
  {
    fputs("ERROR: rastertoescpx job-id user title copies options [file]\n", stderr);
    return (1);
  }

  num_options = cupsParseOptions(argv[5], 0, &options);

 /*
  * Open the PPD file...
  */

  ppd = ppdOpenFile(getenv("PPD"));

  if (!ppd)
  {
    fputs("ERROR: Unable to open PPD file!\n", stderr);
    return (1);
  }

  ppdMarkDefaults(ppd);
  cupsMarkOptions(ppd, num_options, options);

 /*
  * Open the page stream...
  */

  if (argc == 7)
  {
    if ((fd = open(argv[6], O_RDONLY)) == -1)
    {
      perror("ERROR: Unable to open raster file - ");
      return (1);
    }
  }
  else
    fd = 0;

  ras = cupsRasterOpen(fd, CUPS_RASTER_READ);

 /*
  * Initialize the print device...
  */

  Setup(ppd);

 /*
  * Process pages as needed...
  */

  page = 0;

  while (cupsRasterReadHeader(ras, &header))
  {
    page ++;

    fprintf(stderr, "PAGE: %d 1\n", page);
    fprintf(stderr, "INFO: Starting page %d...\n", page);

    StartPage(ppd, &header);

    for (y = 0; y < header.cupsHeight; y ++)
    {
      if ((y & 127) == 0)
        fprintf(stderr, "INFO: Printing page %d, %d%% complete...\n", page,
	        100 * y / header.cupsHeight);

      ProcessLine(ppd, ras, &header, y);
    }

    fprintf(stderr, "INFO: Finished page %d...\n", page);

    EndPage(ppd, &header);
  }

  Shutdown(ppd);

  cupsFreeOptions(num_options, options);

  cupsRasterClose(ras);

  if (fd != 0)
    close(fd);

  if (page == 0)
  {
    fputs("ERROR: No pages found!\n", stderr);
    return (1);
  }
  else
  {
    fputs("INFO: Ready to print.\n", stderr);
    return (0);
  }
}


/*
 * End of "$Id$".
 */
