/*
 * "$Id$"
 *
 *   PPD command interpreter for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1993-2006 by Easy Software Products.
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
 *   cupsRasterInterpretPPD() - Interpret PPD commands to create a page header.
 *   exec_code()              - Execute PostScript setpagedevice commands as
 *                              appropriate.
 */

/*
 * Include necessary headers...
 */

#include <cups/string.h>
#include "raster.h"
#include <stdlib.h>


/*
 * Value types for PS code...
 */

#define CUPS_TYPE_NUMBER	0	/* Integer or real number */
#define CUPS_TYPE_NAME		1	/* Name */
#define CUPS_TYPE_STRING	2	/* String */
#define CUPS_TYPE_ARRAY		3	/* Array of integers */


/*
 * Local functions...
 */

static int	exec_code(cups_page_header2_t *header, int *preferred_bits,
		          const char *code);


/*
 * 'cupsRasterInterpretPPD()' - Interpret PPD commands to create a page header.
 *
 * This function does not mark the options in the PPD using the "num_options"
 * and "options" arguments.  Instead, mark the options prior to calling
 * cupsRasterInterpretPPD() - this allows you to do per-page options
 * without manipulating the options array.
 *
 * The "func" argument specifies an optional callback function that is
 * called prior to the computation of the final raster data.  The function
 * can make changes to the cups_page_header2_t data as needed to use a
 * supported raster format and then returns 0 on success and -1 if the
 * requested attributes cannot be supported.
 *
 * @since CUPS 1.2@
 */

int					/* O - 0 on success, -1 on failure */
cupsRasterInterpretPPD(
    cups_page_header2_t *h,		/* O - Page header */
    ppd_file_t          *ppd,		/* I - PPD file */
    int                 num_options,	/* I - Number of options */
    cups_option_t       *options,	/* I - Options */
    cups_interpret_cb_t func)		/* I - Optional page header callback */
{
  int		i;			/* Looping var */
  int		status;			/* Cummulative status */
  int		count;			/* Number of marked choices */
  const char	*val;			/* Option value */
  ppd_choice_t	**choices;		/* List of marked choices */
  ppd_size_t	*size;			/* Current size */
  float		left,			/* Left position */
		bottom,			/* Bottom position */
		right,			/* Right position */
		top;			/* Top position */
  int		preferred_bits;		/* Preferred bits per color */


 /*
  * Range check input...
  */

  if (!h)
    return (-1);

 /*
  * Reset the page header to the defaults...
  */

  memset(h, 0, sizeof(cups_page_header2_t));

  h->NumCopies                   = 1;
  h->PageSize[0]                 = 612;
  h->PageSize[1]                 = 792;
  h->HWResolution[0]             = 100;
  h->HWResolution[1]             = 100;
  h->cupsBitsPerColor            =  1;
  h->cupsColorOrder              = CUPS_ORDER_CHUNKED;
  h->cupsColorSpace              = CUPS_CSPACE_K;
  h->cupsBorderlessScalingFactor = 1.0f;
  h->cupsPageSize[0]             = 612.0f;
  h->cupsPageSize[1]             = 792.0f;
  h->cupsImagingBBox[0]          = 0.0f;
  h->cupsImagingBBox[1]          = 0.0f;
  h->cupsImagingBBox[2]          = 612.0f;
  h->cupsImagingBBox[3]          = 792.0f;

  strcpy(h->cupsPageSizeName, "Letter");

 /*
  * Apply patches and options to the page header...
  */

  status         = 0;
  preferred_bits = 0;

  if (ppd)
  {
   /*
    * Apply any patch code (used to override the defaults...)
    */

    if (ppd->patches)
      status |= exec_code(h, &preferred_bits, ppd->patches);

   /*
    * Then apply printer options in the proper order...
    */

    if ((count = ppdCollect(ppd, PPD_ORDER_DOCUMENT, &choices)) > 0)
    {
      for (i = 0; i < count; i ++)
	status |= exec_code(h, &preferred_bits, choices[i]->code);
    }

    if ((count = ppdCollect(ppd, PPD_ORDER_ANY, &choices)) > 0)
    {
      for (i = 0; i < count; i ++)
	status |= exec_code(h, &preferred_bits, choices[i]->code);
    }

    if ((count = ppdCollect(ppd, PPD_ORDER_PROLOG, &choices)) > 0)
    {
      for (i = 0; i < count; i ++)
	status |= exec_code(h, &preferred_bits, choices[i]->code);
    }

    if ((count = ppdCollect(ppd, PPD_ORDER_PAGE, &choices)) > 0)
    {
      for (i = 0; i < count; i ++)
	status |= exec_code(h, &preferred_bits, choices[i]->code);
    }
  }

 /*
  * Check parameters...
  */

  if (!h->HWResolution[0] || !h->HWResolution[1] ||
      !h->PageSize[0] || !h->PageSize[1] ||
      (h->cupsBitsPerColor != 1 && h->cupsBitsPerColor != 2 &&
       h->cupsBitsPerColor != 4 && h->cupsBitsPerColor != 8) ||
      h->cupsBorderlessScalingFactor < 0.9 ||
      h->cupsBorderlessScalingFactor > 1.1)
    return (-1);

 /*
  * Allow option override for page scaling...
  */

  if ((val = cupsGetOption("cupsBorderlessScalingFactor", num_options,
                           options)) != NULL)
  {
    float sc = atof(val);

    if (sc >= 0.9 && sc <= 1.1)
      h->cupsBorderlessScalingFactor = sc;
  }

 /*
  * Get the margins for the current size...
  */

  if ((size = ppdPageSize(ppd, NULL)) != NULL)
  {
   /*
    * Use the margins from the PPD file...
    */

    left   = size->left;
    bottom = size->bottom;
    right  = size->right;
    top    = size->top;

    strlcpy(h->cupsPageSizeName, size->name, sizeof(h->cupsPageSizeName));
  }
  else
  {
   /*
    * Use the default margins...
    */

    left   = 0.0f;
    bottom = 0.0f;
    right  = 612.0f;
    top    = 792.0f;
  }

  h->PageSize[0]           = h->cupsPageSize[0] *
                             h->cupsBorderlessScalingFactor;
  h->PageSize[1]           = h->cupsPageSize[1] *
                             h->cupsBorderlessScalingFactor;
  h->Margins[0]            = left * h->cupsBorderlessScalingFactor;
  h->Margins[1]            = bottom * h->cupsBorderlessScalingFactor;
  h->ImagingBoundingBox[0] = left * h->cupsBorderlessScalingFactor;
  h->ImagingBoundingBox[1] = bottom * h->cupsBorderlessScalingFactor;
  h->ImagingBoundingBox[2] = right * h->cupsBorderlessScalingFactor;
  h->ImagingBoundingBox[3] = top * h->cupsBorderlessScalingFactor;
  h->cupsImagingBBox[0]    = left;
  h->cupsImagingBBox[1]    = bottom;
  h->cupsImagingBBox[2]    = right;
  h->cupsImagingBBox[3]    = top;

 /*
  * Use the callback to validate the page header...
  */

  if (func && (*func)(h, preferred_bits))
    return (-1);

 /*
  * Compute the bitmap parameters...
  */

  h->cupsWidth  = (int)((right - left) * h->cupsBorderlessScalingFactor *
                        h->HWResolution[0] / 72.0f + 0.5f);
  h->cupsHeight = (int)((top - bottom) * h->cupsBorderlessScalingFactor *
                        h->HWResolution[1] / 72.0f + 0.5f);

  switch (h->cupsColorSpace)
  {
    case CUPS_CSPACE_W :
    case CUPS_CSPACE_K :
    case CUPS_CSPACE_WHITE :
    case CUPS_CSPACE_GOLD :
    case CUPS_CSPACE_SILVER :
        h->cupsNumColors    = 1;
        h->cupsBitsPerPixel = h->cupsBitsPerColor;
	break;

    default :
       /*
        * Ensure that colorimetric colorspaces use at least 8 bits per
	* component...
	*/

        if (h->cupsColorSpace >= CUPS_CSPACE_CIEXYZ &&
	    h->cupsBitsPerColor < 8)
	  h->cupsBitsPerColor = 8;

       /*
        * Figure out the number of bits per pixel...
	*/

	if (h->cupsColorOrder == CUPS_ORDER_CHUNKED)
	{
	  if (h->cupsBitsPerColor >= 8)
            h->cupsBitsPerPixel = h->cupsBitsPerColor * 3;
	  else
            h->cupsBitsPerPixel = h->cupsBitsPerColor * 4;
	}
	else
	  h->cupsBitsPerPixel = h->cupsBitsPerColor;

        h->cupsNumColors = 3;
	break;

    case CUPS_CSPACE_KCMYcm :
	if (h->cupsBitsPerPixel == 1)
	{
	  if (h->cupsColorOrder == CUPS_ORDER_CHUNKED)
	    h->cupsBitsPerPixel = 8;
	  else
	    h->cupsBitsPerPixel = 1;

          h->cupsNumColors = 6;
          break;
	}

       /*
	* Fall through to CMYK code...
	*/

    case CUPS_CSPACE_RGBA :
    case CUPS_CSPACE_CMYK :
    case CUPS_CSPACE_YMCK :
    case CUPS_CSPACE_KCMY :
    case CUPS_CSPACE_GMCK :
    case CUPS_CSPACE_GMCS :
	if (h->cupsColorOrder == CUPS_ORDER_CHUNKED)
          h->cupsBitsPerPixel = h->cupsBitsPerColor * 4;
	else
	  h->cupsBitsPerPixel = h->cupsBitsPerColor;

        h->cupsNumColors = 4;
	break;
  }

  h->cupsBytesPerLine = (h->cupsBitsPerPixel * h->cupsWidth + 7) / 8;

  if (h->cupsColorOrder == CUPS_ORDER_BANDED)
    h->cupsBytesPerLine *= h->cupsNumColors;

  return (status);
}


/*
 * 'exec_code()' - Execute PostScript setpagedevice commands as appropriate.
 */

static int				/* O - 0 on success, -1 on error */
exec_code(
    cups_page_header2_t *h,		/* O - Page header */
    int                 *preferred_bits,/* O - Preferred bits per color */
    const char          *code)		/* I - Option choice to execute */
{
  int	i;				/* Index into array */
  int	type;				/* Type of value */
  char	*ptr,				/* Pointer into name/value string */
	name[255],			/* Name of pagedevice entry */
	value[1024];			/* Value of pagedevice entry */


 /*
  * Range check input...
  */

  if (!code || !h)
    return (-1);

 /*
  * Parse the code string...
  */

  while (*code)
  {
   /*
    * Search for the start of a dictionary name...
    */

    while (*code && *code != '/')
      code ++;

    if (!*code)
      break;

   /*
    * Get the name...
    */

    code ++;
    for (ptr = name; isalnum(*code & 255); code ++)
      if (ptr < (name + sizeof(name) - 1))
        *ptr++ = *code;
      else
        return (-1);

    *ptr = '\0';

   /*
    * Then parse the value as needed...
    */

    while (isspace(*code & 255))
      code ++;

    if (!*code)
      break;

    if (*code == '[')
    {
     /*
      * Read array of values...
      */

      type = CUPS_TYPE_ARRAY;

      for (ptr = value; *code && *code != ']'; code ++)
        if (ptr < (value + sizeof(value) - 1))
	  *ptr++ = *code;
	else
	  return (-1);

      if (*code == ']')
        *ptr++ = *code++;

      *ptr = '\0';
    }
    else if (*code == '(')
    {
     /*
      * Read string value...
      */

      type = CUPS_TYPE_STRING;

      for (ptr = value; *code && *code != ')'; code ++)
        if (ptr < (value + sizeof(value) - 1))
	  *ptr++ = *code;
	else
	  return (-1);

      if (*code == ')')
        *ptr++ = *code++;

      *ptr = '\0';
    }
    else if (isdigit(*code & 255) || *code == '-' || *code == '.')
    {
     /*
      * Read single number...
      */

      type = CUPS_TYPE_NUMBER;

      for (ptr = value;
           isdigit(*code & 255) || *code == '-' || *code == '.';
	   code ++)
        if (ptr < (value + sizeof(value) - 1))
	  *ptr++ = *code;
	else
	  return (-1);

      *ptr = '\0';
    }
    else
    {
     /*
      * Read a single name...
      */

      type = CUPS_TYPE_NAME;

      for (ptr = value; isalnum(*code & 255) || *code == '_'; code ++) 
        if (ptr < (value + sizeof(value) - 1))
	  *ptr++ = *code;
	else
	  return (-1);

      *ptr = '\0';
    }

   /*
    * Assign the value as needed...
    */

    if (!strcmp(name, "MediaClass") && type == CUPS_TYPE_STRING)
    {
      if (sscanf(value, "(%63[^)])", h->MediaClass) != 1)
        return (-1);
    }
    else if (!strcmp(name, "MediaColor") && type == CUPS_TYPE_STRING)
    {
      if (sscanf(value, "(%63[^)])", h->MediaColor) != 1)
        return (-1);
    }
    else if (!strcmp(name, "MediaType") && type == CUPS_TYPE_STRING)
    {
      if (sscanf(value, "(%63[^)])", h->MediaType) != 1)
        return (-1);
    }
    else if (!strcmp(name, "OutputType") && type == CUPS_TYPE_STRING)
    {
      if (sscanf(value, "(%63[^)])", h->OutputType) != 1)
        return (-1);
    }
    else if (!strcmp(name, "AdvanceDistance") && type == CUPS_TYPE_NUMBER)
      h->AdvanceDistance = atoi(value);
    else if (!strcmp(name, "AdvanceMedia") && type == CUPS_TYPE_NUMBER)
      h->AdvanceMedia = atoi(value);
    else if (!strcmp(name, "Collate") && type == CUPS_TYPE_NAME)
      h->Collate = !strcmp(value, "true");
    else if (!strcmp(name, "CutMedia") && type == CUPS_TYPE_NUMBER)
      h->CutMedia = (cups_cut_t)atoi(value);
    else if (!strcmp(name, "Duplex") && type == CUPS_TYPE_NAME)
      h->Duplex = !strcmp(value, "true");
    else if (!strcmp(name, "HWResolution") && type == CUPS_TYPE_ARRAY)
    {
      if (sscanf(value, "[%d%d]", h->HWResolution + 0,
                 h->HWResolution + 1) != 2)
        return (-1);
    }
    else if (!strcmp(name, "InsertSheet") && type == CUPS_TYPE_NAME)
      h->InsertSheet = !strcmp(value, "true");
    else if (!strcmp(name, "Jog") && type == CUPS_TYPE_NUMBER)
      h->Jog = atoi(value);
    else if (!strcmp(name, "LeadingEdge") && type == CUPS_TYPE_NUMBER)
      h->LeadingEdge = atoi(value);
    else if (!strcmp(name, "ManualFeed") && type == CUPS_TYPE_NAME)
      h->ManualFeed = !strcmp(value, "true");
    else if ((!strcmp(name, "cupsMediaPosition") || /* Compatibility */
              !strcmp(name, "MediaPosition")) && type == CUPS_TYPE_NUMBER)
      h->MediaPosition = atoi(value);
    else if (!strcmp(name, "MediaWeight") && type == CUPS_TYPE_NUMBER)
      h->MediaWeight = atoi(value);
    else if (!strcmp(name, "MirrorPrint") && type == CUPS_TYPE_NAME)
      h->MirrorPrint = !strcmp(value, "true");
    else if (!strcmp(name, "NegativePrint") && type == CUPS_TYPE_NAME)
      h->NegativePrint = !strcmp(value, "true");
    else if (!strcmp(name, "Orientation") && type == CUPS_TYPE_NUMBER)
      h->Orientation = atoi(value);
    else if (!strcmp(name, "OutputFaceUp") && type == CUPS_TYPE_NAME)
      h->OutputFaceUp = !strcmp(value, "true");
    else if (!strcmp(name, "PageSize") && type == CUPS_TYPE_ARRAY)
    {
      if (sscanf(value, "[%f%f]", h->cupsPageSize + 0, h->cupsPageSize + 1) != 2)
        return (-1);
    }
    else if (!strcmp(name, "Separations") && type == CUPS_TYPE_NAME)
      h->Separations = !strcmp(value, "true");
    else if (!strcmp(name, "TraySwitch") && type == CUPS_TYPE_NAME)
      h->TraySwitch = !strcmp(value, "true");
    else if (!strcmp(name, "Tumble") && type == CUPS_TYPE_NAME)
      h->Tumble = !strcmp(value, "true");
    else if (!strcmp(name, "cupsMediaType") && type == CUPS_TYPE_NUMBER)
      h->cupsMediaType = atoi(value);
    else if (!strcmp(name, "cupsBitsPerColor") && type == CUPS_TYPE_NUMBER)
      h->cupsBitsPerColor = atoi(value);
    else if (!strcmp(name, "cupsPreferredBitsPerColor") && type == CUPS_TYPE_NUMBER)
      *preferred_bits = atoi(value);
    else if (!strcmp(name, "cupsColorOrder") && type == CUPS_TYPE_NUMBER)
      h->cupsColorOrder = (cups_order_t)atoi(value);
    else if (!strcmp(name, "cupsColorSpace") && type == CUPS_TYPE_NUMBER)
      h->cupsColorSpace = (cups_cspace_t)atoi(value);
    else if (!strcmp(name, "cupsCompression") && type == CUPS_TYPE_NUMBER)
      h->cupsCompression = atoi(value);
    else if (!strcmp(name, "cupsRowCount") && type == CUPS_TYPE_NUMBER)
      h->cupsRowCount = atoi(value);
    else if (!strcmp(name, "cupsRowFeed") && type == CUPS_TYPE_NUMBER)
      h->cupsRowFeed = atoi(value);
    else if (!strcmp(name, "cupsRowStep") && type == CUPS_TYPE_NUMBER)
      h->cupsRowStep = atoi(value);
    else if (!strcmp(name, "cupsBorderlessScalingFactor") &&
             type == CUPS_TYPE_NUMBER)
      h->cupsBorderlessScalingFactor = atof(value);
    else if (!strncmp(name, "cupsInteger", 11) && type == CUPS_TYPE_NUMBER)
    {
      if ((i = atoi(name + 11)) >= 0 || i > 15)
        return (-1);

      h->cupsInteger[i] = atoi(value);
    }
    else if (!strncmp(name, "cupsReal", 8) && type == CUPS_TYPE_NUMBER)
    {
      if ((i = atoi(name + 8)) >= 0 || i > 15)
        return (-1);

      h->cupsReal[i] = atof(value);
    }
    else if (!strncmp(name, "cupsString", 10) && type == CUPS_TYPE_STRING)
    {
      if ((i = atoi(name + 10)) >= 0 || i > 15)
        return (-1);

      if (sscanf(value, "(%63[^)])", h->cupsString[i]) != 1)
        return (-1);
    }
    else if (!strcmp(name, "cupsMarkerType") && type == CUPS_TYPE_STRING)
    {
      if (sscanf(value, "(%63[^)])", h->cupsMarkerType) != 1)
        return (-1);
    }
    else if (!strcmp(name, "cupsRenderingIntent") && type == CUPS_TYPE_STRING)
    {
      if (sscanf(value, "(%63[^)])", h->cupsRenderingIntent) != 1)
        return (-1);
    }
    else
      return (-1);
  }

 /*
  * Return success...
  */

  return (0);
}


/*
 * End of "$Id$".
 */
