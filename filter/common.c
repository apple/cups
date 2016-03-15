/*
 * "$Id: common.c 11558 2014-02-06 18:33:34Z msweet $"
 *
 * Common filter routines for CUPS.
 *
 * Copyright 2007-2014 by Apple Inc.
 * Copyright 1997-2006 by Easy Software Products.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * This file is subject to the Apple OS-Developed Software exception.
 */

/*
 * Include necessary headers...
 */

#include "common.h"
#include <locale.h>


/*
 * Globals...
 */

int	Orientation = 0,		/* 0 = portrait, 1 = landscape, etc. */
	Duplex = 0,			/* Duplexed? */
	LanguageLevel = 1,		/* Language level of printer */
	ColorDevice = 1;		/* Do color text? */
float	PageLeft = 18.0f,		/* Left margin */
	PageRight = 594.0f,		/* Right margin */
	PageBottom = 36.0f,		/* Bottom margin */
	PageTop = 756.0f,		/* Top margin */
	PageWidth = 612.0f,		/* Total page width */
	PageLength = 792.0f;		/* Total page length */


/*
 * 'SetCommonOptions()' - Set common filter options for media size, etc.
 */

ppd_file_t *				/* O - PPD file */
SetCommonOptions(
    int           num_options,		/* I - Number of options */
    cups_option_t *options,		/* I - Options */
    int           change_size)		/* I - Change page size? */
{
  ppd_file_t	*ppd;			/* PPD file */
  ppd_size_t	*pagesize;		/* Current page size */
  const char	*val;			/* Option value */


#ifdef LC_TIME
  setlocale(LC_TIME, "");
#endif /* LC_TIME */

  ppd = ppdOpenFile(getenv("PPD"));

  ppdMarkDefaults(ppd);
  cupsMarkOptions(ppd, num_options, options);

  if ((pagesize = ppdPageSize(ppd, NULL)) != NULL)
  {
    PageWidth  = pagesize->width;
    PageLength = pagesize->length;
    PageTop    = pagesize->top;
    PageBottom = pagesize->bottom;
    PageLeft   = pagesize->left;
    PageRight  = pagesize->right;

    fprintf(stderr, "DEBUG: Page = %.0fx%.0f; %.0f,%.0f to %.0f,%.0f\n",
            PageWidth, PageLength, PageLeft, PageBottom, PageRight, PageTop);
  }

  if (ppd != NULL)
  {
    ColorDevice   = ppd->color_device;
    LanguageLevel = ppd->language_level;
  }

  if ((val = cupsGetOption("landscape", num_options, options)) != NULL)
  {
    if (_cups_strcasecmp(val, "no") != 0 && _cups_strcasecmp(val, "off") != 0 &&
        _cups_strcasecmp(val, "false") != 0)
    {
      if (ppd && ppd->landscape > 0)
        Orientation = 1;
      else
        Orientation = 3;
    }
  }
  else if ((val = cupsGetOption("orientation-requested", num_options, options)) != NULL)
  {
   /*
    * Map IPP orientation values to 0 to 3:
    *
    *   3 = 0 degrees   = 0
    *   4 = 90 degrees  = 1
    *   5 = -90 degrees = 3
    *   6 = 180 degrees = 2
    */

    Orientation = atoi(val) - 3;
    if (Orientation >= 2)
      Orientation ^= 1;
  }

  if ((val = cupsGetOption("page-left", num_options, options)) != NULL)
  {
    switch (Orientation & 3)
    {
      case 0 :
          PageLeft = (float)atof(val);
	  break;
      case 1 :
          PageBottom = (float)atof(val);
	  break;
      case 2 :
          PageRight = PageWidth - (float)atof(val);
	  break;
      case 3 :
          PageTop = PageLength - (float)atof(val);
	  break;
    }
  }

  if ((val = cupsGetOption("page-right", num_options, options)) != NULL)
  {
    switch (Orientation & 3)
    {
      case 0 :
          PageRight = PageWidth - (float)atof(val);
	  break;
      case 1 :
          PageTop = PageLength - (float)atof(val);
	  break;
      case 2 :
          PageLeft = (float)atof(val);
	  break;
      case 3 :
          PageBottom = (float)atof(val);
	  break;
    }
  }

  if ((val = cupsGetOption("page-bottom", num_options, options)) != NULL)
  {
    switch (Orientation & 3)
    {
      case 0 :
          PageBottom = (float)atof(val);
	  break;
      case 1 :
          PageLeft = (float)atof(val);
	  break;
      case 2 :
          PageTop = PageLength - (float)atof(val);
	  break;
      case 3 :
          PageRight = PageWidth - (float)atof(val);
	  break;
    }
  }

  if ((val = cupsGetOption("page-top", num_options, options)) != NULL)
  {
    switch (Orientation & 3)
    {
      case 0 :
          PageTop = PageLength - (float)atof(val);
	  break;
      case 1 :
          PageRight = PageWidth - (float)atof(val);
	  break;
      case 2 :
          PageBottom = (float)atof(val);
	  break;
      case 3 :
          PageLeft = (float)atof(val);
	  break;
    }
  }

  if (change_size)
    UpdatePageVars();

  if (ppdIsMarked(ppd, "Duplex", "DuplexNoTumble") ||
      ppdIsMarked(ppd, "Duplex", "DuplexTumble") ||
      ppdIsMarked(ppd, "JCLDuplex", "DuplexNoTumble") ||
      ppdIsMarked(ppd, "JCLDuplex", "DuplexTumble") ||
      ppdIsMarked(ppd, "EFDuplex", "DuplexNoTumble") ||
      ppdIsMarked(ppd, "EFDuplex", "DuplexTumble") ||
      ppdIsMarked(ppd, "KD03Duplex", "DuplexNoTumble") ||
      ppdIsMarked(ppd, "KD03Duplex", "DuplexTumble"))
    Duplex = 1;

  return (ppd);
}


/*
 * 'UpdatePageVars()' - Update the page variables for the orientation.
 */

void
UpdatePageVars(void)
{
  float		temp;			/* Swapping variable */


  switch (Orientation & 3)
  {
    case 0 : /* Portait */
        break;

    case 1 : /* Landscape */
	temp       = PageLeft;
	PageLeft   = PageBottom;
	PageBottom = temp;

	temp       = PageRight;
	PageRight  = PageTop;
	PageTop    = temp;

	temp       = PageWidth;
	PageWidth  = PageLength;
	PageLength = temp;
	break;

    case 2 : /* Reverse Portrait */
	temp       = PageWidth - PageLeft;
	PageLeft   = PageWidth - PageRight;
	PageRight  = temp;

	temp       = PageLength - PageBottom;
	PageBottom = PageLength - PageTop;
	PageTop    = temp;
        break;

    case 3 : /* Reverse Landscape */
	temp       = PageWidth - PageLeft;
	PageLeft   = PageWidth - PageRight;
	PageRight  = temp;

	temp       = PageLength - PageBottom;
	PageBottom = PageLength - PageTop;
	PageTop    = temp;

	temp       = PageLeft;
	PageLeft   = PageBottom;
	PageBottom = temp;

	temp       = PageRight;
	PageRight  = PageTop;
	PageTop    = temp;

	temp       = PageWidth;
	PageWidth  = PageLength;
	PageLength = temp;
	break;
  }
}


/*
 * 'WriteCommon()' - Write common procedures...
 */

void
WriteCommon(void)
{
  puts("% x y w h ESPrc - Clip to a rectangle.\n"
       "userdict/ESPrc/rectclip where{pop/rectclip load}\n"
       "{{newpath 4 2 roll moveto 1 index 0 rlineto 0 exch rlineto\n"
       "neg 0 rlineto closepath clip newpath}bind}ifelse put");
  puts("% x y w h ESPrf - Fill a rectangle.\n"
       "userdict/ESPrf/rectfill where{pop/rectfill load}\n"
       "{{gsave newpath 4 2 roll moveto 1 index 0 rlineto 0 exch rlineto\n"
       "neg 0 rlineto closepath fill grestore}bind}ifelse put");
  puts("% x y w h ESPrs - Stroke a rectangle.\n"
       "userdict/ESPrs/rectstroke where{pop/rectstroke load}\n"
       "{{gsave newpath 4 2 roll moveto 1 index 0 rlineto 0 exch rlineto\n"
       "neg 0 rlineto closepath stroke grestore}bind}ifelse put");
}


/*
 * 'WriteLabelProlog()' - Write the prolog with the classification
 *                        and page label.
 */

void
WriteLabelProlog(const char *label,	/* I - Page label */
		 float      bottom,	/* I - Bottom position in points */
		 float      top,	/* I - Top position in points */
		 float      width)	/* I - Width in points */
{
  const char	*classification;	/* CLASSIFICATION environment variable */
  const char	*ptr;			/* Temporary string pointer */


 /*
  * First get the current classification...
  */

  if ((classification = getenv("CLASSIFICATION")) == NULL)
    classification = "";
  if (strcmp(classification, "none") == 0)
    classification = "";

 /*
  * If there is nothing to show, bind an empty 'write labels' procedure
  * and return...
  */

  if (!classification[0] && (label == NULL || !label[0]))
  {
    puts("userdict/ESPwl{}bind put");
    return;
  }

 /*
  * Set the classification + page label string...
  */

  printf("userdict");
  if (strcmp(classification, "confidential") == 0)
    printf("/ESPpl(CONFIDENTIAL");
  else if (strcmp(classification, "classified") == 0)
    printf("/ESPpl(CLASSIFIED");
  else if (strcmp(classification, "secret") == 0)
    printf("/ESPpl(SECRET");
  else if (strcmp(classification, "topsecret") == 0)
    printf("/ESPpl(TOP SECRET");
  else if (strcmp(classification, "unclassified") == 0)
    printf("/ESPpl(UNCLASSIFIED");
  else
  {
    printf("/ESPpl(");

    for (ptr = classification; *ptr; ptr ++)
      if (*ptr < 32 || *ptr > 126)
        printf("\\%03o", *ptr);
      else if (*ptr == '_')
        putchar(' ');
      else
      {
	if (*ptr == '(' || *ptr == ')' || *ptr == '\\')
	  putchar('\\');

	putchar(*ptr);
      }
  }

  if (label)
  {
    if (classification[0])
      printf(" - ");

   /*
    * Quote the label string as needed...
    */

    for (ptr = label; *ptr; ptr ++)
      if (*ptr < 32 || *ptr > 126)
        printf("\\%03o", *ptr);
      else
      {
	if (*ptr == '(' || *ptr == ')' || *ptr == '\\')
	  putchar('\\');

	putchar(*ptr);
      }
  }

  puts(")put");

 /*
  * Then get a 14 point Helvetica-Bold font...
  */

  puts("userdict/ESPpf /Helvetica-Bold findfont 14 scalefont put");

 /*
  * Finally, the procedure to write the labels on the page...
  */

  puts("userdict/ESPwl{");
  puts("  ESPpf setfont");
  printf("  ESPpl stringwidth pop dup 12 add exch -0.5 mul %.0f add\n",
         width * 0.5f);
  puts("  1 setgray");
  printf("  dup 6 sub %.0f 3 index 20 ESPrf\n", bottom - 2.0);
  printf("  dup 6 sub %.0f 3 index 20 ESPrf\n", top - 18.0);
  puts("  0 setgray");
  printf("  dup 6 sub %.0f 3 index 20 ESPrs\n", bottom - 2.0);
  printf("  dup 6 sub %.0f 3 index 20 ESPrs\n", top - 18.0);
  printf("  dup %.0f moveto ESPpl show\n", bottom + 2.0);
  printf("  %.0f moveto ESPpl show\n", top - 14.0);
  puts("pop");
  puts("}bind put");
}


/*
 * 'WriteLabels()' - Write the actual page labels.
 */

void
WriteLabels(int orient)	/* I - Orientation of the page */
{
  float	width,		/* Width of page */
	length;		/* Length of page */


  puts("gsave");

  if ((orient ^ Orientation) & 1)
  {
    width  = PageLength;
    length = PageWidth;
  }
  else
  {
    width  = PageWidth;
    length = PageLength;
  }

  switch (orient & 3)
  {
    case 1 : /* Landscape */
        printf("%.1f 0.0 translate 90 rotate\n", length);
        break;
    case 2 : /* Reverse Portrait */
        printf("%.1f %.1f translate 180 rotate\n", width, length);
        break;
    case 3 : /* Reverse Landscape */
        printf("0.0 %.1f translate -90 rotate\n", width);
        break;
  }

  puts("ESPwl");
  puts("grestore");
}


/*
 * 'WriteTextComment()' - Write a DSC text comment.
 */

void
WriteTextComment(const char *name,	/* I - Comment name ("Title", etc.) */
                 const char *value)	/* I - Comment value */
{
  int	len;				/* Current line length */


 /*
  * DSC comments are of the form:
  *
  *   %%name: value
  *
  * The name and value must be limited to 7-bit ASCII for most printers,
  * so we escape all non-ASCII and ASCII control characters as described
  * in the Adobe Document Structuring Conventions specification.
  */

  printf("%%%%%s: (", name);
  len = 5 + (int)strlen(name);

  while (*value)
  {
    if (*value < ' ' || *value >= 127)
    {
     /*
      * Escape this character value...
      */

      if (len >= 251)			/* Keep line < 254 chars */
        break;

      printf("\\%03o", *value & 255);
      len += 4;
    }
    else if (*value == '\\')
    {
     /*
      * Escape the backslash...
      */

      if (len >= 253)			/* Keep line < 254 chars */
        break;

      putchar('\\');
      putchar('\\');
      len += 2;
    }
    else
    {
     /*
      * Put this character literally...
      */

      if (len >= 254)			/* Keep line < 254 chars */
        break;

      putchar(*value);
      len ++;
    }

    value ++;
  }

  puts(")");
}


/*
 * End of "$Id: common.c 11558 2014-02-06 18:33:34Z msweet $".
 */
