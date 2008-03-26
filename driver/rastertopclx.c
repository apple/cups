/*
 * "$Id$"
 *
 *   Advanced HP Page Control Language and Raster Transfer Language
 *   filter for CUPS.
 *
 *   Copyright 2007-2008 by Apple Inc.
 *   Copyright 1993-2005 by Easy Software Products
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
 *   StartPage()    - Start a page of graphics.
 *   EndPage()      - Finish a page of graphics.
 *   Shutdown()     - Shutdown a printer.
 *   CancelJob()    - Cancel the current job...
 *   CompressData() - Compress a line of graphics.
 *   OutputLine()   - Output the specified number of lines of graphics.
 *   ReadLine()     - Read graphics from the page stream.
 *   main()         - Main entry and processing of driver.
 */

/*
 * Include necessary headers...
 */

#include "driver.h"
#include "pcl-common.h"
#include <signal.h>


/*
 * Output modes...
 */

typedef enum
{
  OUTPUT_BITMAP,			/* Output bitmap data from RIP */
  OUTPUT_INVERBIT,			/* Output inverted bitmap data */
  OUTPUT_RGB,				/* Output 24-bit RGB data from RIP */
  OUTPUT_DITHERED			/* Output dithered data */
} pcl_output_t;


/*
 * Globals...
 */

cups_rgb_t	*RGB;			/* RGB color separation data */
cups_cmyk_t	*CMYK;			/* CMYK color separation data */
unsigned char	*PixelBuffer,		/* Pixel buffer */
		*CMYKBuffer,		/* CMYK buffer */
		*OutputBuffers[6],	/* Output buffers */
		*DotBuffers[6],		/* Bit buffers */
		*CompBuffer,		/* Compression buffer */
		*SeedBuffer,		/* Mode 3 seed buffers */
		BlankValue;		/* The blank value */
short		*InputBuffer;		/* Color separation buffer */
cups_lut_t	*DitherLuts[6];		/* Lookup tables for dithering */
cups_dither_t	*DitherStates[6];	/* Dither state tables */
int		PrinterPlanes,		/* Number of color planes */
		SeedInvalid,		/* Contents of seed buffer invalid? */
		DotBits[6],		/* Number of bits per color */
		DotBufferSizes[6],	/* Size of one row of color dots */
		DotBufferSize,		/* Size of complete line */
		OutputFeed,		/* Number of lines to skip */
		Page;			/* Current page number */
pcl_output_t	OutputMode;		/* Output mode - see OUTPUT_ consts */
const int	ColorOrders[7][7] =	/* Order of color planes */
		{
		  { 0, 0, 0, 0, 0, 0, 0 },	/* Black */
		  { 0, 0, 0, 0, 0, 0, 0 },
		  { 0, 1, 2, 0, 0, 0, 0 },	/* CMY */
		  { 3, 0, 1, 2, 0, 0, 0 },	/* KCMY */
		  { 0, 0, 0, 0, 0, 0, 0 },
		  { 5, 0, 1, 2, 3, 4, 0 },	/* KCMYcm */
		  { 5, 0, 1, 2, 3, 4, 6 }	/* KCMYcmk */
		};
int		Canceled;		/* Is the job canceled? */


/*
 * Prototypes...
 */

void	StartPage(ppd_file_t *ppd, cups_page_header2_t *header, int job_id,
	          const char *user, const char *title, int num_options,
		  cups_option_t *options);
void	EndPage(ppd_file_t *ppd, cups_page_header2_t *header);
void	Shutdown(ppd_file_t *ppd, int job_id, const char *user,
	         const char *title, int num_options, cups_option_t *options);

void	CancelJob(int sig);
void	CompressData(unsigned char *line, int length, int plane, int pend,
	             int type);
void	OutputLine(ppd_file_t *ppd, cups_page_header2_t *header);
int	ReadLine(cups_raster_t *ras, cups_page_header2_t *header);


/*
 * 'StartPage()' - Start a page of graphics.
 */

void
StartPage(ppd_file_t         *ppd,	/* I - PPD file */
          cups_page_header2_t *header,	/* I - Page header */
	  int                job_id,	/* I - Job ID */
	  const char         *user,	/* I - User printing job */
	  const char         *title,	/* I - Title of job */
	  int                num_options,
					/* I - Number of command-line options */
	  cups_option_t      *options)	/* I - Command-line options */
{
  int		i;			/* Temporary/looping var */
  int		plane;			/* Current plane */
  char		s[255];			/* Temporary value */
  const char	*colormodel;		/* Color model string */
  char		resolution[PPD_MAX_NAME],
					/* Resolution string */
		spec[PPD_MAX_NAME];	/* PPD attribute name */
  ppd_attr_t	*attr;			/* Attribute from PPD file */
  ppd_choice_t	*choice;		/* Selected option */
  const int	*order;			/* Order to use */
  int		xorigin,		/* X origin of page */
		yorigin;		/* Y origin of page */
  static const float default_lut[2] =	/* Default dithering lookup table */
		{
		  0.0,
		  1.0
		};


 /*
  * Debug info...
  */

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

#ifdef __APPLE__
 /*
  * MacOS X 10.2.x doesn't set most of the page device attributes, so check
  * the options and set them accordingly...
  */

  if (ppdIsMarked(ppd, "Duplex", "DuplexNoTumble"))
  {
    header->Duplex = CUPS_TRUE;
    header->Tumble = CUPS_FALSE;
  }
  else if (ppdIsMarked(ppd, "Duplex", "DuplexTumble"))
  {
    header->Duplex = CUPS_TRUE;
    header->Tumble = CUPS_TRUE;
  }

  fprintf(stderr, "DEBUG: num_options=%d\n", num_options);

  for (i = 0; i < num_options; i ++)
    fprintf(stderr, "DEBUG: options[%d]=[\"%s\" \"%s\"]\n", i,
            options[i].name, options[i].value);
#endif /* __APPLE__ */

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
    case CUPS_CSPACE_CMY :
        colormodel = "CMY";
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
    strcpy(header->MediaType, "PLAIN");

 /*
  * Get the dithering parameters...
  */

  BlankValue = 0x00;

  if (header->cupsBitsPerColor == 1)
  {
   /*
    * Use raw bitmap mode...
    */

    switch (header->cupsColorSpace)
    {
      case CUPS_CSPACE_K :
          OutputMode    = OUTPUT_BITMAP;
	  PrinterPlanes = 1;
	  break;
      case CUPS_CSPACE_W :
          OutputMode    = OUTPUT_INVERBIT;
	  PrinterPlanes = 1;
	  break;
      default :
      case CUPS_CSPACE_RGB :
          OutputMode    = OUTPUT_INVERBIT;
	  PrinterPlanes = 3;
	  break;
      case CUPS_CSPACE_CMY :
          OutputMode    = OUTPUT_BITMAP;
	  PrinterPlanes = 3;
	  break;
      case CUPS_CSPACE_CMYK :
          OutputMode    = OUTPUT_BITMAP;
	  PrinterPlanes = 4;
	  break;
    }

    if (OutputMode == OUTPUT_INVERBIT)
      BlankValue = 0xff;

    DotBufferSize = header->cupsBytesPerLine;

    memset(DitherLuts, 0, sizeof(DitherLuts));
    memset(DitherStates, 0, sizeof(DitherStates));
  }
  else if (header->cupsColorSpace == CUPS_CSPACE_RGB &&
           (ppd->model_number & PCL_RASTER_RGB24))
  {
   /*
    * Use 24-bit RGB output mode...
    */

    OutputMode    = OUTPUT_RGB;
    PrinterPlanes = 3;
    DotBufferSize = header->cupsBytesPerLine;

    if (header->cupsCompression == 10)
      BlankValue = 0xff;

    memset(DitherLuts, 0, sizeof(DitherLuts));
    memset(DitherStates, 0, sizeof(DitherStates));
  }
  else if ((header->cupsColorSpace == CUPS_CSPACE_K ||
            header->cupsColorSpace == CUPS_CSPACE_W) &&
           (ppd->model_number & PCL_RASTER_RGB24) &&
	   header->cupsCompression == 10)
  {
   /*
    * Use 24-bit RGB output mode for grayscale/black output...
    */

    OutputMode    = OUTPUT_RGB;
    PrinterPlanes = 1;
    DotBufferSize = header->cupsBytesPerLine;

    if (header->cupsColorSpace == CUPS_CSPACE_W)
      BlankValue = 0xff;

    memset(DitherLuts, 0, sizeof(DitherLuts));
    memset(DitherStates, 0, sizeof(DitherStates));
  }
  else
  {
   /*
    * Use dithered output mode...
    */

    OutputMode = OUTPUT_DITHERED;

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

    CMYK = cupsCMYKLoad(ppd, colormodel, header->MediaType, resolution);

    if (RGB)
      fputs("DEBUG: Loaded RGB separation from PPD.\n", stderr);

    if (CMYK)
      fputs("DEBUG: Loaded CMYK separation from PPD.\n", stderr);
    else
    {
      fputs("DEBUG: Loading default K separation.\n", stderr);
      CMYK = cupsCMYKNew(1);
    }

    PrinterPlanes = CMYK->num_channels;

   /*
    * Use dithered mode...
    */

    switch (PrinterPlanes)
    {
      case 1 : /* K */
          DitherLuts[0] = cupsLutLoad(ppd, colormodel, header->MediaType,
	                              resolution, "Black");
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
    }

    for (plane = 0; plane < PrinterPlanes; plane ++)
    {
      if (!DitherLuts[plane])
        DitherLuts[plane] = cupsLutNew(2, default_lut);

      if (DitherLuts[plane][4095].pixel > 1)
	DotBits[plane] = 2;
      else
	DotBits[plane] = 1;

      DitherStates[plane] = cupsDitherNew(header->cupsWidth);

      if (!DitherLuts[plane])
	DitherLuts[plane] = cupsLutNew(2, default_lut);
    }
  }

  fprintf(stderr, "DEBUG: PrinterPlanes = %d\n", PrinterPlanes);

 /*
  * Initialize the printer...
  */

  if ((attr = ppdFindAttr(ppd, "cupsInitialNulls", NULL)) != NULL)
    for (i = atoi(attr->value); i > 0; i --)
      putchar(0);

  if (Page == 1 && (ppd->model_number & PCL_PJL))
  {
    pjl_escape();

   /*
    * PJL job setup...
    */

    pjl_set_job(job_id, user, title);

    if ((attr = ppdFindAttr(ppd, "cupsPJL", "StartJob")) != NULL)
      pjl_write(ppd, attr->value, NULL, job_id, user, title, num_options,
                options);

    snprintf(spec, sizeof(spec), "RENDERMODE.%s", colormodel);
    if ((attr = ppdFindAttr(ppd, "cupsPJL", spec)) != NULL)
      printf("@PJL SET RENDERMODE=%s\r\n", attr->value);

    snprintf(spec, sizeof(spec), "COLORSPACE.%s", colormodel);
    if ((attr = ppdFindAttr(ppd, "cupsPJL", spec)) != NULL)
      printf("@PJL SET COLORSPACE=%s\r\n", attr->value);

    snprintf(spec, sizeof(spec), "RENDERINTENT.%s", colormodel);
    if ((attr = ppdFindAttr(ppd, "cupsPJL", spec)) != NULL)
      printf("@PJL SET RENDERINTENT=%s\r\n", attr->value);

    if ((attr = ppdFindAttr(ppd, "cupsPJL", "Duplex")) != NULL)
    {
      sprintf(s, "%d", header->Duplex);
      pjl_write(ppd, attr->value, s, job_id, user, title, num_options, options);
    }

    if ((attr = ppdFindAttr(ppd, "cupsPJL", "Tumble")) != NULL)
    {
      sprintf(s, "%d", header->Tumble);
      pjl_write(ppd, attr->value, s, job_id, user, title, num_options, options);
    }

    if ((attr = ppdFindAttr(ppd, "cupsPJL", "MediaClass")) != NULL)
      pjl_write(ppd, attr->value, header->MediaClass, job_id, user, title,
                num_options, options);

    if ((attr = ppdFindAttr(ppd, "cupsPJL", "MediaColor")) != NULL)
      pjl_write(ppd, attr->value, header->MediaColor, job_id, user, title,
                num_options, options);

    if ((attr = ppdFindAttr(ppd, "cupsPJL", "MediaType")) != NULL)
      pjl_write(ppd, attr->value, header->MediaType, job_id, user, title,
                num_options, options);

    if ((attr = ppdFindAttr(ppd, "cupsPJL", "OutputType")) != NULL)
      pjl_write(ppd, attr->value, header->OutputType, job_id, user, title,
                num_options, options);

    if ((attr = ppdFindAttr(ppd, "cupsPJL", "cupsBooklet")) != NULL &&
        (choice = ppdFindMarkedChoice(ppd, "cupsBooklet")) != NULL)
      pjl_write(ppd, attr->value, choice->choice, job_id, user, title,
                num_options, options);

    if ((attr = ppdFindAttr(ppd, "cupsPJL", "Jog")) != NULL)
    {
      sprintf(s, "%d", header->Jog);
      pjl_write(ppd, attr->value, s, job_id, user, title, num_options, options);
    }

    if ((attr = ppdFindAttr(ppd, "cupsPJL", "cupsPunch")) != NULL &&
        (choice = ppdFindMarkedChoice(ppd, "cupsPunch")) != NULL)
      pjl_write(ppd, attr->value, choice->choice, job_id, user, title,
                num_options, options);

    if ((attr = ppdFindAttr(ppd, "cupsPJL", "cupsStaple")) != NULL &&
        (choice = ppdFindMarkedChoice(ppd, "cupsStaple")) != NULL)
      pjl_write(ppd, attr->value, choice->choice, job_id, user, title,
                num_options, options);

    if ((attr = ppdFindAttr(ppd, "cupsPJL", "cupsRET")) != NULL &&
        (choice = ppdFindMarkedChoice(ppd, "cupsRET")) != NULL)
      pjl_write(ppd, attr->value, choice->choice, job_id, user, title,
                num_options, options);

    if ((attr = ppdFindAttr(ppd, "cupsPJL", "cupsTonerSave")) != NULL &&
        (choice = ppdFindMarkedChoice(ppd, "cupsTonerSave")) != NULL)
      pjl_write(ppd, attr->value, choice->choice, job_id, user, title,
                num_options, options);

    if (ppd->model_number & PCL_PJL_PAPERWIDTH)
    {
      printf("@PJL SET PAPERLENGTH=%d\r\n", header->PageSize[1] * 10);
      printf("@PJL SET PAPERWIDTH=%d\r\n", header->PageSize[0] * 10);
    }

    if (ppd->model_number & PCL_PJL_RESOLUTION)
      printf("@PJL SET RESOLUTION=%d\r\n", header->HWResolution[0]);

    if (ppd->model_number & PCL_PJL_HPGL2)
      pjl_enter_language("HPGL2");
    else if (ppd->model_number & PCL_PJL_PCL3GUI)
      pjl_enter_language("PCL3GUI");
    else
      pjl_enter_language("PCL");
  }

  if (Page == 1)
  {
    pcl_reset();
  }

  if (ppd->model_number & PCL_PJL_HPGL2)
  {
    if (Page == 1)
    {
     /*
      * HP-GL/2 initialization...
      */

      printf("IN;");
      printf("MG\"%d %s %s\";", job_id, user, title);
    }

   /*
    * Set media size, position, type, etc...
    */

    printf("BP5,0;");
    printf("PS%.0f,%.0f;",
	   header->cupsHeight * 1016.0 / header->HWResolution[1],
	   header->cupsWidth * 1016.0 / header->HWResolution[0]);
    printf("PU;");
    printf("PA0,0");

    printf("MT%d;", header->cupsMediaType);

    if (header->CutMedia == CUPS_CUT_PAGE)
      printf("EC;");
    else
      printf("EC0;");

   /*
    * Set graphics mode...
    */

    pcl_set_pcl_mode(0);
    pcl_set_negative_motion();
  }
  else
  {
   /*
    * Set media size, position, type, etc...
    */

    if (!header->Duplex || (Page & 1))
    {
      pcl_set_media_size(ppd, header->PageSize[0], header->PageSize[1]);

      if (header->MediaPosition)
        pcl_set_media_source(header->MediaPosition);

      pcl_set_media_type(header->cupsMediaType);

      if (ppdFindAttr(ppd, "cupsPJL", "Duplex") == NULL)
        pcl_set_duplex(header->Duplex, header->Tumble);

     /*
      * Set the number of copies...
      */

      if (!ppd->manual_copies)
	pcl_set_copies(header->NumCopies);

     /*
      * Set the output order/bin...
      */

      if (ppdFindAttr(ppd, "cupsPJL", "Jog") == NULL && header->Jog)
        printf("\033&l%dG", header->Jog);
    }
    else
    {
     /*
      * Print on the back side...
      */

      printf("\033&a2G");
    }

    if (header->Duplex && (ppd->model_number & PCL_RASTER_CRD))
    {
     /*
      * Reload the media...
      */

      pcl_set_media_source(-2);
    }

   /*
    * Set the units for cursor positioning and go to the top of the form.
    */

    printf("\033&u%dD", header->HWResolution[0]);
    printf("\033*p0Y\033*p0X");
  }

  if ((attr = cupsFindAttr(ppd, "cupsPCLQuality", colormodel,
                           header->MediaType, resolution, spec,
			   sizeof(spec))) != NULL)
  {
   /*
    * Set the print quality...
    */

    if (ppd->model_number & PCL_PJL_HPGL2)
      printf("QM%d", atoi(attr->value));
    else
      printf("\033*o%dM", atoi(attr->value));
  }

 /*
  * Enter graphics mode...
  */

  if (ppd->model_number & PCL_RASTER_CRD)
  {
   /*
    * Use configure raster data command...
    */

    if (OutputMode == OUTPUT_RGB)
    {
     /*
      * Send 12-byte configure raster data command with horizontal and
      * vertical resolutions as well as a color count...
      */

      if ((attr = cupsFindAttr(ppd, "cupsPCLCRDMode", colormodel,
                               header->MediaType, resolution, spec,
			       sizeof(spec))) != NULL)
        i = atoi(attr->value);
      else
        i = 31;

      printf("\033*g12W");
      putchar(6);			/* Format 6 */
      putchar(i);			/* Set pen mode */
      putchar(0x00);			/* Number components */
      putchar(0x01);			/* (1 for RGB) */

      putchar(header->HWResolution[0] >> 8);
      putchar(header->HWResolution[0]);
      putchar(header->HWResolution[1] >> 8);
      putchar(header->HWResolution[1]);

      putchar(header->cupsCompression);	/* Compression mode 3 or 10 */
      putchar(0x01);			/* Portrait orientation */
      putchar(0x20);			/* Bits per pixel (32 = RGB) */
      putchar(0x01);			/* Planes per pixel (1 = chunky RGB) */
    }
    else
    {
     /*
      * Send the configure raster data command with horizontal and
      * vertical resolutions as well as a color count...
      */

      printf("\033*g%dW", PrinterPlanes * 6 + 2);
      putchar(2);			/* Format 2 */
      putchar(PrinterPlanes);		/* Output planes */

      order = ColorOrders[PrinterPlanes - 1];

      for (i = 0; i < PrinterPlanes; i ++)
      {
        plane = order[i];

	putchar(header->HWResolution[0] >> 8);
	putchar(header->HWResolution[0]);
	putchar(header->HWResolution[1] >> 8);
	putchar(header->HWResolution[1]);
	putchar(0);
	putchar(1 << DotBits[plane]);
      }
    }
  }
  else if ((ppd->model_number & PCL_RASTER_CID) && OutputMode == OUTPUT_RGB)
  {
   /*
    * Use configure image data command...
    */

    pcl_set_simple_resolution(header->HWResolution[0]);
					/* Set output resolution */

    cupsWritePrintData("\033*v6W\0\3\0\10\10\10", 11);
					/* 24-bit RGB */
  }
  else
  {
   /*
    * Use simple raster commands...
    */

    pcl_set_simple_resolution(header->HWResolution[0]);
					/* Set output resolution */

    if (PrinterPlanes == 3)
      pcl_set_simple_cmy();
    else if (PrinterPlanes == 4)
      pcl_set_simple_kcmy();
  }

  if ((attr = ppdFindAttr(ppd, "cupsPCLOrigin", "X")) != NULL)
    xorigin = atoi(attr->value);
  else
    xorigin = 0;

  if ((attr = ppdFindAttr(ppd, "cupsPCLOrigin", "Y")) != NULL)
    yorigin = atoi(attr->value);
  else
    yorigin = 120;

  printf("\033&a%dH\033&a%dV", xorigin, yorigin);
  printf("\033*r%dS", header->cupsWidth);
  printf("\033*r%dT", header->cupsHeight);
  printf("\033*r1A");

  if (header->cupsCompression && header->cupsCompression != 10)
    printf("\033*b%dM", header->cupsCompression);

  OutputFeed = 0;

 /*
  * Allocate memory for the page...
  */

  PixelBuffer = malloc(header->cupsBytesPerLine);

  if (OutputMode == OUTPUT_DITHERED)
  {
    InputBuffer      = malloc(header->cupsWidth * PrinterPlanes * 2);
    OutputBuffers[0] = malloc(PrinterPlanes * header->cupsWidth);

    for (i = 1; i < PrinterPlanes; i ++)
      OutputBuffers[i] = OutputBuffers[0] + i * header->cupsWidth;

    if (RGB)
      CMYKBuffer = malloc(header->cupsWidth * PrinterPlanes);

    for (plane = 0, DotBufferSize = 0; plane < PrinterPlanes; plane ++)
    {
      DotBufferSizes[plane] = (header->cupsWidth + 7) / 8 * DotBits[plane];
      DotBufferSize         += DotBufferSizes[plane];
    }

    DotBuffers[0] = malloc(DotBufferSize);
    for (plane = 1; plane < PrinterPlanes; plane ++)
      DotBuffers[plane] = DotBuffers[plane - 1] + DotBufferSizes[plane - 1];
  }

  if (header->cupsCompression)
    CompBuffer = malloc(DotBufferSize * 4);

  if (header->cupsCompression >= 3)
    SeedBuffer = malloc(DotBufferSize);

  SeedInvalid = 1;

  fprintf(stderr, "BlankValue=%d\n", BlankValue);
}


/*
 * 'EndPage()' - Finish a page of graphics.
 */

void
EndPage(ppd_file_t         *ppd,	/* I - PPD file */
        cups_page_header2_t *header)	/* I - Page header */
{
  int	plane;				/* Current plane */


 /*
  * End graphics mode...
  */

  if (ppd->model_number & PCL_RASTER_END_COLOR)
    printf("\033*rC");			/* End color GFX */
  else
    printf("\033*r0B");			/* End B&W GFX */

 /*
  * Output a page eject sequence...
  */

  if (ppd->model_number & PCL_PJL_HPGL2)
  {
     pcl_set_hpgl_mode(0);		/* Back to HP-GL/2 mode */
     printf("PG;");			/* Eject the current page */
  }
  else if (!(header->Duplex && (Page & 1)))
    printf("\014");			/* Eject current page */

 /*
  * Free memory for the page...
  */

  free(PixelBuffer);

  if (OutputMode == OUTPUT_DITHERED)
  {
    for (plane = 0; plane < PrinterPlanes; plane ++)
    {
      cupsDitherDelete(DitherStates[plane]);
      cupsLutDelete(DitherLuts[plane]);
    }

    free(DotBuffers[0]);
    free(InputBuffer);
    free(OutputBuffers[0]);

    cupsCMYKDelete(CMYK);

    if (RGB)
    {
      cupsRGBDelete(RGB);
      free(CMYKBuffer);
    }
  }

  if (header->cupsCompression)
    free(CompBuffer);

  if (header->cupsCompression >= 3)
    free(SeedBuffer);
}


/*
 * 'Shutdown()' - Shutdown a printer.
 */

void
Shutdown(ppd_file_t         *ppd,	/* I - PPD file */
	 int                job_id,	/* I - Job ID */
	 const char         *user,	/* I - User printing job */
	 const char         *title,	/* I - Title of job */
	 int                num_options,/* I - Number of command-line options */
	 cups_option_t      *options)	/* I - Command-line options */
{
  ppd_attr_t	*attr;			/* Attribute from PPD file */


  if ((attr = ppdFindAttr(ppd, "cupsPCL", "EndJob")) != NULL)
  {
   /*
    * Tell the printer how many pages were in the job...
    */

    putchar(0x1b);
    printf(attr->value, Page);
  }
  else
  {
   /*
    * Return the printer to the default state...
    */

    pcl_reset();
  }

  if (ppd->model_number & PCL_PJL)
  {
    pjl_escape();

    if ((attr = ppdFindAttr(ppd, "cupsPJL", "EndJob")) != NULL)
      pjl_write(ppd, attr->value, NULL, job_id, user, title, num_options,
                options);
    else
      printf("@PJL EOJ\r\n");

    pjl_escape();
  }
}


/*
 * 'CancelJob()' - Cancel the current job...
 */

void
CancelJob(int sig)			/* I - Signal */
{
  (void)sig;

  Canceled = 1;
}


/*
 * 'CompressData()' - Compress a line of graphics.
 */

void
CompressData(unsigned char *line,	/* I - Data to compress */
             int           length,	/* I - Number of bytes */
	     int           plane,	/* I - Color plane */
	     int           pend,	/* I - End character for data */
	     int           type)	/* I - Type of compression */
{
  unsigned char	*line_ptr,		/* Current byte pointer */
        	*line_end,		/* End-of-line byte pointer */
        	*comp_ptr,		/* Pointer into compression buffer */
        	*start,			/* Start of compression sequence */
		*seed;			/* Seed buffer pointer */
  int           count,			/* Count of bytes for output */
		offset,			/* Offset of bytes for output */
		temp;			/* Temporary count */
  int		r, g, b;		/* RGB deltas for mode 10 compression */


  switch (type)
  {
    default :
       /*
	* Do no compression; with a mode-0 only printer, we can compress blank
	* lines...
	*/

	line_ptr = line;

        if (cupsCheckBytes(line, length))
          line_end = line;		/* Blank line */
        else
	  line_end = line + length;	/* Non-blank line */
	break;

    case 1 :
       /*
        * Do run-length encoding...
        */

	line_end = line + length;
	for (line_ptr = line, comp_ptr = CompBuffer;
	     line_ptr < line_end;
	     comp_ptr += 2, line_ptr += count)
	{
	  for (count = 1;
               (line_ptr + count) < line_end &&
	           line_ptr[0] == line_ptr[count] &&
        	   count < 256;
               count ++);

	  comp_ptr[0] = count - 1;
	  comp_ptr[1] = line_ptr[0];
	}

        line_ptr = CompBuffer;
        line_end = comp_ptr;
	break;

    case 2 :
       /*
        * Do TIFF pack-bits encoding...
        */

	line_ptr = line;
	line_end = line + length;
	comp_ptr = CompBuffer;

	while (line_ptr < line_end)
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

        line_ptr = CompBuffer;
        line_end = comp_ptr;
	break;

    case 3 :
       /*
	* Do delta-row compression...
	*/

	line_ptr = line;
	line_end = line + length;

	comp_ptr = CompBuffer;
	seed     = SeedBuffer + plane * length;

	while (line_ptr < line_end)
        {
         /*
          * Find the next non-matching sequence...
          */

          start = line_ptr;

	  if (SeedInvalid)
	  {
	   /*
	    * The seed buffer is invalid, so do the next 8 bytes, max...
	    */

	    offset = 0;

	    if ((count = line_end - line_ptr) > 8)
	      count = 8;

	    line_ptr += count;
	  }
	  else
	  {
	   /*
	    * The seed buffer is valid, so compare against it...
	    */

            while (*line_ptr == *seed &&
                   line_ptr < line_end)
            {
              line_ptr ++;
              seed ++;
            }

            if (line_ptr == line_end)
              break;

            offset = line_ptr - start;

           /*
            * Find up to 8 non-matching bytes...
            */

            start = line_ptr;
            count = 0;
            while (*line_ptr != *seed &&
                   line_ptr < line_end &&
                   count < 8)
            {
              line_ptr ++;
              seed ++;
              count ++;
            }
	  }

         /*
          * Place mode 3 compression data in the buffer; see HP manuals
          * for details...
          */

          if (offset >= 31)
          {
           /*
            * Output multi-byte offset...
            */

            *comp_ptr++ = ((count - 1) << 5) | 31;

            offset -= 31;
            while (offset >= 255)
            {
              *comp_ptr++ = 255;
              offset    -= 255;
            }

            *comp_ptr++ = offset;
          }
          else
          {
           /*
            * Output single-byte offset...
            */

            *comp_ptr++ = ((count - 1) << 5) | offset;
          }

          memcpy(comp_ptr, start, count);
          comp_ptr += count;
        }

	line_ptr = CompBuffer;
	line_end = comp_ptr;

        memcpy(SeedBuffer + plane * length, line, length);
	break;

    case 10 :
       /*
        * Mode 10 "near lossless" RGB compression...
	*/

	line_ptr = line;
	line_end = line + length;

	comp_ptr = CompBuffer;
	seed     = SeedBuffer;

        if (PrinterPlanes == 1)
	{
	 /*
	  * Do grayscale compression to RGB...
	  */

	  while (line_ptr < line_end)
          {
           /*
            * Find the next non-matching sequence...
            */

            start = line_ptr;
            while (line_ptr < line_end &&
	           *line_ptr == *seed)
            {
              line_ptr ++;
              seed ++;
            }

            if (line_ptr == line_end)
              break;

            offset = line_ptr - start;

           /*
            * Find non-matching grayscale pixels...
            */

            start = line_ptr;
            while (line_ptr < line_end &&
	           *line_ptr != *seed)
            {
              line_ptr ++;
              seed ++;
            }

            count = line_ptr - start;

#if 0
            fprintf(stderr, "DEBUG: offset=%d, count=%d, comp_ptr=%p(%d of %d)...\n",
	            offset, count, comp_ptr, comp_ptr - CompBuffer,
		    BytesPerLine * 5);
#endif /* 0 */

           /*
            * Place mode 10 compression data in the buffer; each sequence
	    * starts with a command byte that looks like:
	    *
	    *     CMD SRC SRC OFF OFF CNT CNT CNT
	    *
	    * For the purpose of this driver, CMD and SRC are always 0.
	    *
	    * If the offset >= 3 then additional offset bytes follow the
	    * first command byte, each byte == 255 until the last one.
	    *
	    * If the count >= 7, then additional count bytes follow each
	    * group of pixels, each byte == 255 until the last one.
	    *
	    * The offset and count are in RGB tuples (not bytes, as for
	    * Mode 3 and 9)...
            */

            if (offset >= 3)
            {
             /*
              * Output multi-byte offset...
              */

              if (count > 7)
		*comp_ptr++ = 0x1f;
	      else
		*comp_ptr++ = 0x18 | (count - 1);

              offset -= 3;
              while (offset >= 255)
              {
        	*comp_ptr++ = 255;
        	offset      -= 255;
              }

              *comp_ptr++ = offset;
            }
            else
            {
             /*
              * Output single-byte offset...
              */

              if (count > 7)
		*comp_ptr++ = (offset << 3) | 0x07;
	      else
		*comp_ptr++ = (offset << 3) | (count - 1);
            }

	    temp = count - 8;
	    seed -= count;

            while (count > 0)
	    {
	      if (count <= temp)
	      {
	       /*
		* This is exceedingly lame...  The replacement counts
		* are intermingled with the data...
		*/

        	if (temp >= 255)
        	  *comp_ptr++ = 255;
        	else
        	  *comp_ptr++ = temp;

        	temp -= 255;
	      }

             /*
	      * Get difference between current and see pixels...
	      */

              r = *start - *seed;
	      g = r;
	      b = ((*start & 0xfe) - (*seed & 0xfe)) / 2;

              if (r < -16 || r > 15 || g < -16 || g > 15 || b < -16 || b > 15)
	      {
	       /*
		* Pack 24-bit RGB into 23 bits...  Lame...
		*/

                g = *start;

		*comp_ptr++ = g >> 1;

		if (g & 1)
		  *comp_ptr++ = 0x80 | (g >> 1);
		else
		  *comp_ptr++ = g >> 1;

		if (g & 1)
		  *comp_ptr++ = 0x80 | (g >> 1);
		else
		  *comp_ptr++ = g >> 1;
              }
	      else
	      {
	       /*
		* Pack 15-bit RGB difference...
		*/

        	*comp_ptr++ = 0x80 | ((r << 2) & 0x7c) | ((g >> 3) & 0x03);
		*comp_ptr++ = ((g << 5) & 0xe0) | (b & 0x1f);
	      }

              count --;
	      start ++;
	      seed ++;
            }

           /*
	    * Make sure we have the ending count if the replacement count
	    * was exactly 8 + 255n...
	    */

	    if (temp == 0)
	      *comp_ptr++ = 0;
          }
	}
	else
	{
	 /*
	  * Do RGB compression...
	  */

	  while (line_ptr < line_end)
          {
           /*
            * Find the next non-matching sequence...
            */

            start = line_ptr;
            while (line_ptr[0] == seed[0] &&
                   line_ptr[1] == seed[1] &&
                   line_ptr[2] == seed[2] &&
                   (line_ptr + 2) < line_end)
            {
              line_ptr += 3;
              seed += 3;
            }

            if (line_ptr == line_end)
              break;

            offset = (line_ptr - start) / 3;

           /*
            * Find non-matching RGB tuples...
            */

            start = line_ptr;
            while ((line_ptr[0] != seed[0] ||
                    line_ptr[1] != seed[1] ||
                    line_ptr[2] != seed[2]) &&
                   (line_ptr + 2) < line_end)
            {
              line_ptr += 3;
              seed += 3;
            }

            count = (line_ptr - start) / 3;

           /*
            * Place mode 10 compression data in the buffer; each sequence
	    * starts with a command byte that looks like:
	    *
	    *     CMD SRC SRC OFF OFF CNT CNT CNT
	    *
	    * For the purpose of this driver, CMD and SRC are always 0.
	    *
	    * If the offset >= 3 then additional offset bytes follow the
	    * first command byte, each byte == 255 until the last one.
	    *
	    * If the count >= 7, then additional count bytes follow each
	    * group of pixels, each byte == 255 until the last one.
	    *
	    * The offset and count are in RGB tuples (not bytes, as for
	    * Mode 3 and 9)...
            */

            if (offset >= 3)
            {
             /*
              * Output multi-byte offset...
              */

              if (count > 7)
		*comp_ptr++ = 0x1f;
	      else
		*comp_ptr++ = 0x18 | (count - 1);

              offset -= 3;
              while (offset >= 255)
              {
        	*comp_ptr++ = 255;
        	offset      -= 255;
              }

              *comp_ptr++ = offset;
            }
            else
            {
             /*
              * Output single-byte offset...
              */

              if (count > 7)
		*comp_ptr++ = (offset << 3) | 0x07;
	      else
		*comp_ptr++ = (offset << 3) | (count - 1);
            }

	    temp = count - 8;
	    seed -= count * 3;

            while (count > 0)
	    {
	      if (count <= temp)
	      {
	       /*
		* This is exceedingly lame...  The replacement counts
		* are intermingled with the data...
		*/

        	if (temp >= 255)
        	  *comp_ptr++ = 255;
        	else
        	  *comp_ptr++ = temp;

        	temp -= 255;
	      }

             /*
	      * Get difference between current and see pixels...
	      */

              r = start[0] - seed[0];
	      g = start[1] - seed[1];
	      b = ((start[2] & 0xfe) - (seed[2] & 0xfe)) / 2;

              if (r < -16 || r > 15 || g < -16 || g > 15 || b < -16 || b > 15)
	      {
	       /*
		* Pack 24-bit RGB into 23 bits...  Lame...
		*/

		*comp_ptr++ = start[0] >> 1;

		if (start[0] & 1)
		  *comp_ptr++ = 0x80 | (start[1] >> 1);
		else
		  *comp_ptr++ = start[1] >> 1;

		if (start[1] & 1)
		  *comp_ptr++ = 0x80 | (start[2] >> 1);
		else
		  *comp_ptr++ = start[2] >> 1;
              }
	      else
	      {
	       /*
		* Pack 15-bit RGB difference...
		*/

        	*comp_ptr++ = 0x80 | ((r << 2) & 0x7c) | ((g >> 3) & 0x03);
		*comp_ptr++ = ((g << 5) & 0xe0) | (b & 0x1f);
	      }

              count --;
	      start += 3;
	      seed += 3;
            }

           /*
	    * Make sure we have the ending count if the replacement count
	    * was exactly 8 + 255n...
	    */

	    if (temp == 0)
	      *comp_ptr++ = 0;
          }
        }

	line_ptr = CompBuffer;
	line_end = comp_ptr;

        memcpy(SeedBuffer, line, length);
	break;
  }

 /*
  * Set the length of the data and write a raster plane...
  */

  printf("\033*b%d%c", (int)(line_end - line_ptr), pend);
  cupsWritePrintData(line_ptr, line_end - line_ptr);
}


/*
 * 'OutputLine()' - Output the specified number of lines of graphics.
 */

void
OutputLine(ppd_file_t         *ppd,	/* I - PPD file */
           cups_page_header2_t *header)	/* I - Page header */
{
  int			i, j;		/* Looping vars */
  int			plane;		/* Current plane */
  unsigned char		bit;		/* Current bit */
  int			bytes;		/* Number of bytes/plane */
  int			width;		/* Width of line in pixels */
  const int		*order;		/* Order to use */
  unsigned char		*ptr;		/* Pointer into buffer */


 /*
  * Output whitespace as needed...
  */

  if (OutputFeed > 0)
  {
    if (header->cupsCompression < 3)
    {
     /*
      * Send blank raster lines...
      */

      while (OutputFeed > 0)
      {
	printf("\033*b0W");
	OutputFeed --;
      }
    }
    else
    {
     /*
      * Send Y offset command and invalidate the seed buffer...
      */

      printf("\033*b%dY", OutputFeed);
      OutputFeed  = 0;
      SeedInvalid = 1;
    }
  }

 /*
  * Write bitmap data as needed...
  */

  switch (OutputMode)
  {
    case OUTPUT_BITMAP :		/* Send 1-bit bitmap data... */
	order = ColorOrders[PrinterPlanes - 1];
	bytes = header->cupsBytesPerLine / PrinterPlanes;

	for (i = 0; i < PrinterPlanes; i ++)
	{
	  plane = order[i];

	  CompressData(PixelBuffer + i * bytes, bytes, plane,
	               (i < (PrinterPlanes - 1)) ? 'V' : 'W',
		       header->cupsCompression);
        }
        break;

    case OUTPUT_INVERBIT :		/* Send inverted 1-bit bitmap data... */
	order = ColorOrders[PrinterPlanes - 1];
	bytes = header->cupsBytesPerLine / PrinterPlanes;

        for (i = header->cupsBytesPerLine, ptr = PixelBuffer;
	     i > 0;
	     i --, ptr ++)
	  *ptr = ~*ptr;

	for (i = 0; i < PrinterPlanes; i ++)
	{
	  plane = order[i];

	  CompressData(PixelBuffer + i * bytes, bytes, plane,
	               (i < (PrinterPlanes - 1)) ? 'V' : 'W',
		       header->cupsCompression);
        }
        break;

    case OUTPUT_RGB :			/* Send 24-bit RGB data... */
        if (PrinterPlanes == 1 && !BlankValue)
	{
	 /*
	  * Invert black to grayscale...
	  */

          for (i = header->cupsBytesPerLine, ptr = PixelBuffer;
	       i > 0;
	       i --, ptr ++)
	    *ptr = ~*ptr;
	}

       /*
	* Compress the output...
	*/

	CompressData(PixelBuffer, header->cupsBytesPerLine, 0, 'W',
	             header->cupsCompression);
        break;

    default :
	order = ColorOrders[PrinterPlanes - 1];
	width = header->cupsWidth;

	for (i = 0, j = 0; i < PrinterPlanes; i ++)
	{
	  plane = order[i];
	  bytes = DotBufferSizes[plane] / DotBits[plane];

	  for (bit = 1, ptr = DotBuffers[plane];
	       bit <= DotBits[plane];
	       bit <<= 1, ptr += bytes, j ++)
	  {
	    cupsPackHorizontalBit(OutputBuffers[plane], DotBuffers[plane],
	                          width, 0, bit);
            CompressData(ptr, bytes, j,
	                 i == (PrinterPlanes - 1) &&
			     bit == DotBits[plane] ? 'W' : 'V',
			 header->cupsCompression);
          }
	}
	break;
  }

 /*
  * The seed buffer, if any, now should contain valid data...
  */

  SeedInvalid = 0;
}


/*
 * 'ReadLine()' - Read graphics from the page stream.
 */

int					/* O - Number of lines (0 if blank) */
ReadLine(cups_raster_t      *ras,	/* I - Raster stream */
         cups_page_header2_t *header)	/* I - Page header */
{
  int	plane,				/* Current color plane */
	width;				/* Width of line */


 /*
  * Read raster data...
  */

  cupsRasterReadPixels(ras, PixelBuffer, header->cupsBytesPerLine);

 /*
  * See if it is blank; if so, return right away...
  */

  if (cupsCheckValue(PixelBuffer, header->cupsBytesPerLine, BlankValue))
    return (0);

 /*
  * If we aren't dithering, return immediately...
  */

  if (OutputMode != OUTPUT_DITHERED)
    return (1);

 /*
  * Perform the color separation...
  */

  width = header->cupsWidth;

  switch (header->cupsColorSpace)
  {
    case CUPS_CSPACE_W :
        if (RGB)
	{
	  cupsRGBDoGray(RGB, PixelBuffer, CMYKBuffer, width);

	  if (RGB->num_channels == 1)
	    cupsCMYKDoBlack(CMYK, CMYKBuffer, InputBuffer, width);
	  else
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

	  if (RGB->num_channels == 1)
	    cupsCMYKDoBlack(CMYK, CMYKBuffer, InputBuffer, width);
	  else
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
    cupsDitherLine(DitherStates[plane], DitherLuts[plane], InputBuffer + plane,
                   PrinterPlanes, OutputBuffers[plane]);

 /*
  * Return 1 to indicate that we have non-blank output...
  */

  return (1);
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
  cups_page_header2_t	header;		/* Page header from file */
  int			y;		/* Current line */
  ppd_file_t		*ppd;		/* PPD file */
  int			job_id;		/* Job ID */
  int			num_options;	/* Number of options */
  cups_option_t		*options;	/* Options */
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;		/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */


 /*
  * Make sure status messages are not buffered...
  */

  setbuf(stderr, NULL);

 /*
  * Check command-line...
  */

  if (argc < 6 || argc > 7)
  {
    fputs("ERROR: rastertopclx job-id user title copies options [file]\n", stderr);
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
  * Register a signal handler to eject the current page if the
  * job is cancelled.
  */

  Canceled = 0;

#ifdef HAVE_SIGSET /* Use System V signals over POSIX to avoid bugs */
  sigset(SIGTERM, CancelJob);
#elif defined(HAVE_SIGACTION)
  memset(&action, 0, sizeof(action));

  sigemptyset(&action.sa_mask);
  action.sa_handler = CancelJob;
  sigaction(SIGTERM, &action, NULL);
#else
  signal(SIGTERM, CancelJob);
#endif /* HAVE_SIGSET */

 /*
  * Process pages as needed...
  */

  job_id = atoi(argv[1]);

  Page = 0;

  while (cupsRasterReadHeader2(ras, &header))
  {
   /*
    * Write a status message with the page number and number of copies.
    */

    if (Canceled)
      break;

    Page ++;

    fprintf(stderr, "PAGE: %d %d\n", Page, header.NumCopies);
    fprintf(stderr, "INFO: Starting page %d...\n", Page);

    StartPage(ppd, &header, atoi(argv[1]), argv[2], argv[3],
              num_options, options);

    for (y = 0; y < (int)header.cupsHeight; y ++)
    {
     /*
      * Let the user know how far we have progressed...
      */

      if (Canceled)
	break;

      if ((y & 127) == 0)
        fprintf(stderr, "INFO: Printing page %d, %d%% complete...\n", Page,
	        100 * y / header.cupsHeight);

     /*
      * Read and write a line of graphics or whitespace...
      */

      if (ReadLine(ras, &header))
        OutputLine(ppd, &header);
      else
        OutputFeed ++;
    }

   /*
    * Eject the page...
    */

    fprintf(stderr, "INFO: Finished page %d...\n", Page);

    EndPage(ppd, &header);

    if (Canceled)
      break;
  }

  Shutdown(ppd, job_id, argv[2], argv[3], num_options, options);

  cupsFreeOptions(num_options, options);

  cupsRasterClose(ras);

  if (fd != 0)
    close(fd);

  if (Page == 0)
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
