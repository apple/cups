/*
 * "$Id: interpret.c 12748 2015-06-24 15:58:40Z msweet $"
 *
 * PPD command interpreter for CUPS.
 *
 * Copyright 2007-2015 by Apple Inc.
 * Copyright 1993-2007 by Easy Software Products.
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

#include <cups/raster-private.h>


/*
 * Stack values for the PostScript mini-interpreter...
 */

typedef enum
{
  CUPS_PS_NAME,
  CUPS_PS_NUMBER,
  CUPS_PS_STRING,
  CUPS_PS_BOOLEAN,
  CUPS_PS_NULL,
  CUPS_PS_START_ARRAY,
  CUPS_PS_END_ARRAY,
  CUPS_PS_START_DICT,
  CUPS_PS_END_DICT,
  CUPS_PS_START_PROC,
  CUPS_PS_END_PROC,
  CUPS_PS_CLEARTOMARK,
  CUPS_PS_COPY,
  CUPS_PS_DUP,
  CUPS_PS_INDEX,
  CUPS_PS_POP,
  CUPS_PS_ROLL,
  CUPS_PS_SETPAGEDEVICE,
  CUPS_PS_STOPPED,
  CUPS_PS_OTHER
} _cups_ps_type_t;

typedef struct
{
  _cups_ps_type_t	type;		/* Object type */
  union
  {
    int		boolean;		/* Boolean value */
    char	name[64];		/* Name value */
    double	number;			/* Number value */
    char	other[64];		/* Other operator */
    char	string[64];		/* Sring value */
  }			value;		/* Value */
} _cups_ps_obj_t;

typedef struct
{
  int			num_objs,	/* Number of objects on stack */
			alloc_objs;	/* Number of allocated objects */
  _cups_ps_obj_t	*objs;		/* Objects in stack */
} _cups_ps_stack_t;


/*
 * Local functions...
 */

static int		cleartomark_stack(_cups_ps_stack_t *st);
static int		copy_stack(_cups_ps_stack_t *st, int count);
static void		delete_stack(_cups_ps_stack_t *st);
static void		error_object(_cups_ps_obj_t *obj);
static void		error_stack(_cups_ps_stack_t *st, const char *title);
static _cups_ps_obj_t	*index_stack(_cups_ps_stack_t *st, int n);
static _cups_ps_stack_t	*new_stack(void);
static _cups_ps_obj_t	*pop_stack(_cups_ps_stack_t *st);
static _cups_ps_obj_t	*push_stack(_cups_ps_stack_t *st,
			            _cups_ps_obj_t *obj);
static int		roll_stack(_cups_ps_stack_t *st, int c, int s);
static _cups_ps_obj_t	*scan_ps(_cups_ps_stack_t *st, char **ptr);
static int		setpagedevice(_cups_ps_stack_t *st,
			                cups_page_header2_t *h,
			                int *preferred_bits);
#ifdef DEBUG
static void		DEBUG_object(const char *prefix, _cups_ps_obj_t *obj);
static void		DEBUG_stack(const char *prefix, _cups_ps_stack_t *st);
#endif /* DEBUG */


/*
 * 'cupsRasterInterpretPPD()' - Interpret PPD commands to create a page header.
 *
 * This function is used by raster image processing (RIP) filters like
 * cgpdftoraster and imagetoraster when writing CUPS raster data for a page.
 * It is not used by raster printer driver filters which only read CUPS
 * raster data.
 *
 *
 * @code cupsRasterInterpretPPD@ does not mark the options in the PPD using
 * the "num_options" and "options" arguments.  Instead, mark the options with
 * @code cupsMarkOptions@ and @code ppdMarkOption@ prior to calling it -
 * this allows for per-page options without manipulating the options array.
 *
 * The "func" argument specifies an optional callback function that is
 * called prior to the computation of the final raster data.  The function
 * can make changes to the @link cups_page_header2_t@ data as needed to use a
 * supported raster format and then returns 0 on success and -1 if the
 * requested attributes cannot be supported.
 *
 *
 * @code cupsRasterInterpretPPD@ supports a subset of the PostScript language.
 * Currently only the @code [@, @code ]@, @code <<@, @code >>@, @code {@,
 * @code }@, @code cleartomark@, @code copy@, @code dup@, @code index@,
 * @code pop@, @code roll@, @code setpagedevice@, and @code stopped@ operators
 * are supported.
 *
 * @since CUPS 1.2/OS X 10.5@
 */

int					/* O - 0 on success, -1 on failure */
cupsRasterInterpretPPD(
    cups_page_header2_t *h,		/* O - Page header to create */
    ppd_file_t          *ppd,		/* I - PPD file */
    int                 num_options,	/* I - Number of options */
    cups_option_t       *options,	/* I - Options */
    cups_interpret_cb_t func)		/* I - Optional page header callback (@code NULL@ for none) */
{
  int		status;			/* Cummulative status */
  char		*code;			/* Code to run */
  const char	*val;			/* Option value */
  ppd_size_t	*size;			/* Current size */
  float		left,			/* Left position */
		bottom,			/* Bottom position */
		right,			/* Right position */
		top,			/* Top position */
		temp1, temp2;		/* Temporary variables for swapping */
  int		preferred_bits;		/* Preferred bits per color */


 /*
  * Range check input...
  */

  _cupsRasterClearError();

  if (!h)
  {
    _cupsRasterAddError("Page header cannot be NULL!\n");
    return (-1);
  }

 /*
  * Reset the page header to the defaults...
  */

  memset(h, 0, sizeof(cups_page_header2_t));

  h->NumCopies                   = 1;
  h->PageSize[0]                 = 612;
  h->PageSize[1]                 = 792;
  h->HWResolution[0]             = 100;
  h->HWResolution[1]             = 100;
  h->cupsBitsPerColor            = 1;
  h->cupsColorOrder              = CUPS_ORDER_CHUNKED;
  h->cupsColorSpace              = CUPS_CSPACE_K;
  h->cupsBorderlessScalingFactor = 1.0f;
  h->cupsPageSize[0]             = 612.0f;
  h->cupsPageSize[1]             = 792.0f;
  h->cupsImagingBBox[0]          = 0.0f;
  h->cupsImagingBBox[1]          = 0.0f;
  h->cupsImagingBBox[2]          = 612.0f;
  h->cupsImagingBBox[3]          = 792.0f;

  strlcpy(h->cupsPageSizeName, "Letter", sizeof(h->cupsPageSizeName));

#ifdef __APPLE__
 /*
  * cupsInteger0 is also used for the total page count on OS X; set an
  * uncommon default value so we can tell if the driver is using cupsInteger0.
  */

  h->cupsInteger[0] = 0x80000000;
#endif /* __APPLE__ */

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
      status |= _cupsRasterExecPS(h, &preferred_bits, ppd->patches);

   /*
    * Then apply printer options in the proper order...
    */

    if ((code = ppdEmitString(ppd, PPD_ORDER_DOCUMENT, 0.0)) != NULL)
    {
      status |= _cupsRasterExecPS(h, &preferred_bits, code);
      free(code);
    }

    if ((code = ppdEmitString(ppd, PPD_ORDER_ANY, 0.0)) != NULL)
    {
      status |= _cupsRasterExecPS(h, &preferred_bits, code);
      free(code);
    }

    if ((code = ppdEmitString(ppd, PPD_ORDER_PROLOG, 0.0)) != NULL)
    {
      status |= _cupsRasterExecPS(h, &preferred_bits, code);
      free(code);
    }

    if ((code = ppdEmitString(ppd, PPD_ORDER_PAGE, 0.0)) != NULL)
    {
      status |= _cupsRasterExecPS(h, &preferred_bits, code);
      free(code);
    }
  }

 /*
  * Allow option override for page scaling...
  */

  if ((val = cupsGetOption("cupsBorderlessScalingFactor", num_options,
                           options)) != NULL)
  {
    double sc = atof(val);		/* Scale factor */

    if (sc >= 0.1 && sc <= 2.0)
      h->cupsBorderlessScalingFactor = (float)sc;
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

    h->cupsPageSize[0] = size->width;
    h->cupsPageSize[1] = size->length;
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

 /*
  * Handle orientation...
  */

  switch (h->Orientation)
  {
    case CUPS_ORIENT_0 :
    default :
        /* Do nothing */
        break;

    case CUPS_ORIENT_90 :
        temp1              = h->cupsPageSize[0];
        h->cupsPageSize[0] = h->cupsPageSize[1];
        h->cupsPageSize[1] = temp1;

        temp1  = left;
        temp2  = right;
        left   = h->cupsPageSize[0] - top;
        right  = h->cupsPageSize[0] - bottom;
        bottom = h->cupsPageSize[1] - temp1;
        top    = h->cupsPageSize[1] - temp2;
        break;

    case CUPS_ORIENT_180 :
        temp1  = left;
        temp2  = bottom;
        left   = h->cupsPageSize[0] - right;
        right  = h->cupsPageSize[0] - temp1;
        bottom = h->cupsPageSize[1] - top;
        top    = h->cupsPageSize[1] - temp2;
        break;

    case CUPS_ORIENT_270 :
        temp1              = h->cupsPageSize[0];
        h->cupsPageSize[0] = h->cupsPageSize[1];
        h->cupsPageSize[1] = temp1;

        temp1  = left;
        temp2  = right;
        left   = bottom;
        right  = top;
        bottom = h->cupsPageSize[1] - temp2;
        top    = h->cupsPageSize[1] - temp1;
        break;
  }

  if (left > right)
  {
    temp1 = left;
    left  = right;
    right = temp1;
  }

  if (bottom > top)
  {
    temp1  = bottom;
    bottom = top;
    top    = temp1;
  }

  h->PageSize[0]           = (unsigned)(h->cupsPageSize[0] *
                                        h->cupsBorderlessScalingFactor);
  h->PageSize[1]           = (unsigned)(h->cupsPageSize[1] *
                                        h->cupsBorderlessScalingFactor);
  h->Margins[0]            = (unsigned)(left *
                                        h->cupsBorderlessScalingFactor);
  h->Margins[1]            = (unsigned)(bottom *
                                        h->cupsBorderlessScalingFactor);
  h->ImagingBoundingBox[0] = (unsigned)(left *
                                        h->cupsBorderlessScalingFactor);
  h->ImagingBoundingBox[1] = (unsigned)(bottom *
                                        h->cupsBorderlessScalingFactor);
  h->ImagingBoundingBox[2] = (unsigned)(right *
                                        h->cupsBorderlessScalingFactor);
  h->ImagingBoundingBox[3] = (unsigned)(top *
                                        h->cupsBorderlessScalingFactor);
  h->cupsImagingBBox[0]    = (float)left;
  h->cupsImagingBBox[1]    = (float)bottom;
  h->cupsImagingBBox[2]    = (float)right;
  h->cupsImagingBBox[3]    = (float)top;

 /*
  * Use the callback to validate the page header...
  */

  if (func && (*func)(h, preferred_bits))
  {
    _cupsRasterAddError("Page header callback returned error.\n");
    return (-1);
  }

 /*
  * Check parameters...
  */

  if (!h->HWResolution[0] || !h->HWResolution[1] ||
      !h->PageSize[0] || !h->PageSize[1] ||
      (h->cupsBitsPerColor != 1 && h->cupsBitsPerColor != 2 &&
       h->cupsBitsPerColor != 4 && h->cupsBitsPerColor != 8 &&
       h->cupsBitsPerColor != 16) ||
      h->cupsBorderlessScalingFactor < 0.1 ||
      h->cupsBorderlessScalingFactor > 2.0)
  {
    _cupsRasterAddError("Page header uses unsupported values.\n");
    return (-1);
  }

 /*
  * Compute the bitmap parameters...
  */

  h->cupsWidth  = (unsigned)((right - left) * h->cupsBorderlessScalingFactor *
                        h->HWResolution[0] / 72.0f + 0.5f);
  h->cupsHeight = (unsigned)((top - bottom) * h->cupsBorderlessScalingFactor *
                        h->HWResolution[1] / 72.0f + 0.5f);

  switch (h->cupsColorSpace)
  {
    case CUPS_CSPACE_W :
    case CUPS_CSPACE_K :
    case CUPS_CSPACE_WHITE :
    case CUPS_CSPACE_GOLD :
    case CUPS_CSPACE_SILVER :
    case CUPS_CSPACE_SW :
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
	if (h->cupsBitsPerColor == 1)
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
    case CUPS_CSPACE_RGBW :
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

    case CUPS_CSPACE_DEVICE1 :
    case CUPS_CSPACE_DEVICE2 :
    case CUPS_CSPACE_DEVICE3 :
    case CUPS_CSPACE_DEVICE4 :
    case CUPS_CSPACE_DEVICE5 :
    case CUPS_CSPACE_DEVICE6 :
    case CUPS_CSPACE_DEVICE7 :
    case CUPS_CSPACE_DEVICE8 :
    case CUPS_CSPACE_DEVICE9 :
    case CUPS_CSPACE_DEVICEA :
    case CUPS_CSPACE_DEVICEB :
    case CUPS_CSPACE_DEVICEC :
    case CUPS_CSPACE_DEVICED :
    case CUPS_CSPACE_DEVICEE :
    case CUPS_CSPACE_DEVICEF :
        h->cupsNumColors = h->cupsColorSpace - CUPS_CSPACE_DEVICE1 + 1;

        if (h->cupsColorOrder == CUPS_ORDER_CHUNKED)
          h->cupsBitsPerPixel = h->cupsBitsPerColor * h->cupsNumColors;
	else
	  h->cupsBitsPerPixel = h->cupsBitsPerColor;
	break;
  }

  h->cupsBytesPerLine = (h->cupsBitsPerPixel * h->cupsWidth + 7) / 8;

  if (h->cupsColorOrder == CUPS_ORDER_BANDED)
    h->cupsBytesPerLine *= h->cupsNumColors;

  return (status);
}


/*
 * '_cupsRasterExecPS()' - Execute PostScript code to initialize a page header.
 */

int					/* O - 0 on success, -1 on error */
_cupsRasterExecPS(
    cups_page_header2_t *h,		/* O - Page header */
    int                 *preferred_bits,/* O - Preferred bits per color */
    const char          *code)		/* I - PS code to execute */
{
  int			error = 0;	/* Error condition? */
  _cups_ps_stack_t	*st;		/* PostScript value stack */
  _cups_ps_obj_t	*obj;		/* Object from top of stack */
  char			*codecopy,	/* Copy of code */
			*codeptr;	/* Pointer into copy of code */


  DEBUG_printf(("_cupsRasterExecPS(h=%p, preferred_bits=%p, code=\"%s\")\n",
                h, preferred_bits, code));

 /*
  * Copy the PostScript code and create a stack...
  */

  if ((codecopy = strdup(code)) == NULL)
  {
    _cupsRasterAddError("Unable to duplicate code string.\n");
    return (-1);
  }

  if ((st = new_stack()) == NULL)
  {
    _cupsRasterAddError("Unable to create stack.\n");
    free(codecopy);
    return (-1);
  }

 /*
  * Parse the PS string until we run out of data...
  */

  codeptr = codecopy;

  while ((obj = scan_ps(st, &codeptr)) != NULL)
  {
#ifdef DEBUG
    DEBUG_printf(("_cupsRasterExecPS: Stack (%d objects)", st->num_objs));
    DEBUG_object("_cupsRasterExecPS", obj);
#endif /* DEBUG */

    switch (obj->type)
    {
      default :
          /* Do nothing for regular values */
	  break;

      case CUPS_PS_CLEARTOMARK :
          pop_stack(st);

	  if (cleartomark_stack(st))
	    _cupsRasterAddError("cleartomark: Stack underflow.\n");

#ifdef DEBUG
          DEBUG_puts("1_cupsRasterExecPS:    dup");
	  DEBUG_stack("_cupsRasterExecPS", st);
#endif /* DEBUG */
          break;

      case CUPS_PS_COPY :
          pop_stack(st);
	  if ((obj = pop_stack(st)) != NULL)
	  {
	    copy_stack(st, (int)obj->value.number);

#ifdef DEBUG
            DEBUG_puts("_cupsRasterExecPS: copy");
	    DEBUG_stack("_cupsRasterExecPS", st);
#endif /* DEBUG */
          }
          break;

      case CUPS_PS_DUP :
          pop_stack(st);
	  copy_stack(st, 1);

#ifdef DEBUG
          DEBUG_puts("_cupsRasterExecPS: dup");
	  DEBUG_stack("_cupsRasterExecPS", st);
#endif /* DEBUG */
          break;

      case CUPS_PS_INDEX :
          pop_stack(st);
	  if ((obj = pop_stack(st)) != NULL)
	  {
	    index_stack(st, (int)obj->value.number);

#ifdef DEBUG
            DEBUG_puts("_cupsRasterExecPS: index");
	    DEBUG_stack("_cupsRasterExecPS", st);
#endif /* DEBUG */
          }
          break;

      case CUPS_PS_POP :
          pop_stack(st);
          pop_stack(st);

#ifdef DEBUG
          DEBUG_puts("_cupsRasterExecPS: pop");
	  DEBUG_stack("_cupsRasterExecPS", st);
#endif /* DEBUG */
          break;

      case CUPS_PS_ROLL :
          pop_stack(st);
	  if ((obj = pop_stack(st)) != NULL)
	  {
            int		c;		/* Count */


            c = (int)obj->value.number;

	    if ((obj = pop_stack(st)) != NULL)
	    {
	      roll_stack(st, (int)obj->value.number, c);

#ifdef DEBUG
              DEBUG_puts("_cupsRasterExecPS: roll");
	      DEBUG_stack("_cupsRasterExecPS", st);
#endif /* DEBUG */
            }
	  }
          break;

      case CUPS_PS_SETPAGEDEVICE :
          pop_stack(st);
	  setpagedevice(st, h, preferred_bits);

#ifdef DEBUG
          DEBUG_puts("_cupsRasterExecPS: setpagedevice");
	  DEBUG_stack("_cupsRasterExecPS", st);
#endif /* DEBUG */
          break;

      case CUPS_PS_START_PROC :
      case CUPS_PS_END_PROC :
      case CUPS_PS_STOPPED :
          pop_stack(st);
	  break;

      case CUPS_PS_OTHER :
          _cupsRasterAddError("Unknown operator \"%s\".\n", obj->value.other);
	  error = 1;
          DEBUG_printf(("_cupsRasterExecPS: Unknown operator \"%s\".", obj->value.other));
          break;
    }

    if (error)
      break;
  }

 /*
  * Cleanup...
  */

  free(codecopy);

  if (st->num_objs > 0)
  {
    error_stack(st, "Stack not empty:");

#ifdef DEBUG
    DEBUG_puts("_cupsRasterExecPS: Stack not empty");
    DEBUG_stack("_cupsRasterExecPS", st);
#endif /* DEBUG */

    delete_stack(st);

    return (-1);
  }

  delete_stack(st);

 /*
  * Return success...
  */

  return (0);
}


/*
 * 'cleartomark_stack()' - Clear to the last mark ([) on the stack.
 */

static int				/* O - 0 on success, -1 on error */
cleartomark_stack(_cups_ps_stack_t *st)	/* I - Stack */
{
  _cups_ps_obj_t	*obj;		/* Current object on stack */


  while ((obj = pop_stack(st)) != NULL)
    if (obj->type == CUPS_PS_START_ARRAY)
      break;

  return (obj ? 0 : -1);
}


/*
 * 'copy_stack()' - Copy the top N stack objects.
 */

static int				/* O - 0 on success, -1 on error */
copy_stack(_cups_ps_stack_t *st,	/* I - Stack */
           int              c)		/* I - Number of objects to copy */
{
  int	n;				/* Index */


  if (c < 0)
    return (-1);
  else if (c == 0)
    return (0);

  if ((n = st->num_objs - c) < 0)
    return (-1);

  while (c > 0)
  {
    if (!push_stack(st, st->objs + n))
      return (-1);

    n ++;
    c --;
  }

  return (0);
}


/*
 * 'delete_stack()' - Free memory used by a stack.
 */

static void
delete_stack(_cups_ps_stack_t *st)	/* I - Stack */
{
  free(st->objs);
  free(st);
}


/*
 * 'error_object()' - Add an object's value to the current error message.
 */

static void
error_object(_cups_ps_obj_t *obj)	/* I - Object to add */
{
  switch (obj->type)
  {
    case CUPS_PS_NAME :
	_cupsRasterAddError(" /%s", obj->value.name);
	break;

    case CUPS_PS_NUMBER :
	_cupsRasterAddError(" %g", obj->value.number);
	break;

    case CUPS_PS_STRING :
	_cupsRasterAddError(" (%s)", obj->value.string);
	break;

    case CUPS_PS_BOOLEAN :
	if (obj->value.boolean)
	  _cupsRasterAddError(" true");
	else
	  _cupsRasterAddError(" false");
	break;

    case CUPS_PS_NULL :
	_cupsRasterAddError(" null");
	break;

    case CUPS_PS_START_ARRAY :
	_cupsRasterAddError(" [");
	break;

    case CUPS_PS_END_ARRAY :
	_cupsRasterAddError(" ]");
	break;

    case CUPS_PS_START_DICT :
	_cupsRasterAddError(" <<");
	break;

    case CUPS_PS_END_DICT :
	_cupsRasterAddError(" >>");
	break;

    case CUPS_PS_START_PROC :
	_cupsRasterAddError(" {");
	break;

    case CUPS_PS_END_PROC :
	_cupsRasterAddError(" }");
	break;

    case CUPS_PS_COPY :
	_cupsRasterAddError(" --copy--");
        break;

    case CUPS_PS_CLEARTOMARK :
	_cupsRasterAddError(" --cleartomark--");
        break;

    case CUPS_PS_DUP :
	_cupsRasterAddError(" --dup--");
        break;

    case CUPS_PS_INDEX :
	_cupsRasterAddError(" --index--");
        break;

    case CUPS_PS_POP :
	_cupsRasterAddError(" --pop--");
        break;

    case CUPS_PS_ROLL :
	_cupsRasterAddError(" --roll--");
        break;

    case CUPS_PS_SETPAGEDEVICE :
	_cupsRasterAddError(" --setpagedevice--");
        break;

    case CUPS_PS_STOPPED :
	_cupsRasterAddError(" --stopped--");
        break;

    case CUPS_PS_OTHER :
	_cupsRasterAddError(" --%s--", obj->value.other);
	break;
  }
}


/*
 * 'error_stack()' - Add a stack to the current error message...
 */

static void
error_stack(_cups_ps_stack_t *st,	/* I - Stack */
            const char       *title)	/* I - Title string */
{
  int			c;		/* Looping var */
  _cups_ps_obj_t	*obj;		/* Current object on stack */


  _cupsRasterAddError("%s", title);

  for (obj = st->objs, c = st->num_objs; c > 0; c --, obj ++)
    error_object(obj);

  _cupsRasterAddError("\n");
}


/*
 * 'index_stack()' - Copy the Nth value on the stack.
 */

static _cups_ps_obj_t	*		/* O - New object */
index_stack(_cups_ps_stack_t *st,	/* I - Stack */
            int              n)		/* I - Object index */
{
  if (n < 0 || (n = st->num_objs - n - 1) < 0)
    return (NULL);

  return (push_stack(st, st->objs + n));
}


/*
 * 'new_stack()' - Create a new stack.
 */

static _cups_ps_stack_t	*		/* O - New stack */
new_stack(void)
{
  _cups_ps_stack_t	*st;		/* New stack */


  if ((st = calloc(1, sizeof(_cups_ps_stack_t))) == NULL)
    return (NULL);

  st->alloc_objs = 32;

  if ((st->objs = calloc(32, sizeof(_cups_ps_obj_t))) == NULL)
  {
    free(st);
    return (NULL);
  }
  else
    return (st);
}


/*
 * 'pop_stock()' - Pop the top object off the stack.
 */

static _cups_ps_obj_t	*		/* O - Object */
pop_stack(_cups_ps_stack_t *st)		/* I - Stack */
{
  if (st->num_objs > 0)
  {
    st->num_objs --;

    return (st->objs + st->num_objs);
  }
  else
    return (NULL);
}


/*
 * 'push_stack()' - Push an object on the stack.
 */

static _cups_ps_obj_t	*		/* O - New object */
push_stack(_cups_ps_stack_t *st,	/* I - Stack */
           _cups_ps_obj_t   *obj)	/* I - Object */
{
  _cups_ps_obj_t	*temp;		/* New object */


  if (st->num_objs >= st->alloc_objs)
  {


    st->alloc_objs += 32;

    if ((temp = realloc(st->objs, (size_t)st->alloc_objs *
                                  sizeof(_cups_ps_obj_t))) == NULL)
      return (NULL);

    st->objs = temp;
    memset(temp + st->num_objs, 0, 32 * sizeof(_cups_ps_obj_t));
  }

  temp = st->objs + st->num_objs;
  st->num_objs ++;

  memcpy(temp, obj, sizeof(_cups_ps_obj_t));

  return (temp);
}


/*
 * 'roll_stack()' - Rotate stack objects.
 */

static int				/* O - 0 on success, -1 on error */
roll_stack(_cups_ps_stack_t *st,	/* I - Stack */
	   int              c,		/* I - Number of objects */
           int              s)		/* I - Amount to shift */
{
  _cups_ps_obj_t	*temp;		/* Temporary array of objects */
  int			n;		/* Index into array */


  DEBUG_printf(("3roll_stack(st=%p, s=%d, c=%d)", st, s, c));

 /*
  * Range check input...
  */

  if (c < 0)
    return (-1);
  else if (c == 0)
    return (0);

  if ((n = st->num_objs - c) < 0)
    return (-1);

  s %= c;

  if (s == 0)
    return (0);

 /*
  * Copy N objects and move things around...
  */

  if (s < 0)
  {
   /*
    * Shift down...
    */

    s = -s;

    if ((temp = calloc((size_t)s, sizeof(_cups_ps_obj_t))) == NULL)
      return (-1);

    memcpy(temp, st->objs + n, (size_t)s * sizeof(_cups_ps_obj_t));
    memmove(st->objs + n, st->objs + n + s, (size_t)(c - s) * sizeof(_cups_ps_obj_t));
    memcpy(st->objs + n + c - s, temp, (size_t)s * sizeof(_cups_ps_obj_t));
  }
  else
  {
   /*
    * Shift up...
    */

    if ((temp = calloc((size_t)s, sizeof(_cups_ps_obj_t))) == NULL)
      return (-1);

    memcpy(temp, st->objs + n + c - s, (size_t)s * sizeof(_cups_ps_obj_t));
    memmove(st->objs + n + s, st->objs + n, (size_t)(c - s) * sizeof(_cups_ps_obj_t));
    memcpy(st->objs + n, temp, (size_t)s * sizeof(_cups_ps_obj_t));
  }

  free(temp);

  return (0);
}


/*
 * 'scan_ps()' - Scan a string for the next PS object.
 */

static _cups_ps_obj_t	*		/* O  - New object or NULL on EOF */
scan_ps(_cups_ps_stack_t *st,		/* I  - Stack */
        char             **ptr)		/* IO - String pointer */
{
  _cups_ps_obj_t	obj;		/* Current object */
  char			*start,		/* Start of object */
			*cur,		/* Current position */
			*valptr,	/* Pointer into value string */
			*valend;	/* End of value string */
  int			parens;		/* Parenthesis nesting level */


 /*
  * Skip leading whitespace...
  */

  for (cur = *ptr; *cur; cur ++)
  {
    if (*cur == '%')
    {
     /*
      * Comment, skip to end of line...
      */

      for (cur ++; *cur && *cur != '\n' && *cur != '\r'; cur ++);

      if (!*cur)
        cur --;
    }
    else if (!isspace(*cur & 255))
      break;
  }

  if (!*cur)
  {
    *ptr = NULL;

    return (NULL);
  }

 /*
  * See what we have...
  */

  memset(&obj, 0, sizeof(obj));

  switch (*cur)
  {
    case '(' :				/* (string) */
        obj.type = CUPS_PS_STRING;
	start    = cur;

	for (cur ++, parens = 1, valptr = obj.value.string,
	         valend = obj.value.string + sizeof(obj.value.string) - 1;
             *cur;
	     cur ++)
	{
	  if (*cur == ')' && parens == 1)
	    break;

          if (*cur == '(')
	    parens ++;
	  else if (*cur == ')')
	    parens --;

          if (valptr >= valend)
	  {
	    *ptr = start;

	    return (NULL);
	  }

	  if (*cur == '\\')
	  {
	   /*
	    * Decode escaped character...
	    */

	    cur ++;

            if (*cur == 'b')
	      *valptr++ = '\b';
	    else if (*cur == 'f')
	      *valptr++ = '\f';
	    else if (*cur == 'n')
	      *valptr++ = '\n';
	    else if (*cur == 'r')
	      *valptr++ = '\r';
	    else if (*cur == 't')
	      *valptr++ = '\t';
	    else if (*cur >= '0' && *cur <= '7')
	    {
	      int ch = *cur - '0';

              if (cur[1] >= '0' && cur[1] <= '7')
	      {
	        cur ++;
		ch = (ch << 3) + *cur - '0';
	      }

              if (cur[1] >= '0' && cur[1] <= '7')
	      {
	        cur ++;
		ch = (ch << 3) + *cur - '0';
	      }

	      *valptr++ = (char)ch;
	    }
	    else if (*cur == '\r')
	    {
	      if (cur[1] == '\n')
	        cur ++;
	    }
	    else if (*cur != '\n')
	      *valptr++ = *cur;
	  }
	  else
	    *valptr++ = *cur;
	}

	if (*cur != ')')
	{
	  *ptr = start;

	  return (NULL);
	}

	cur ++;
        break;

    case '[' :				/* Start array */
        obj.type = CUPS_PS_START_ARRAY;
	cur ++;
        break;

    case ']' :				/* End array */
        obj.type = CUPS_PS_END_ARRAY;
	cur ++;
        break;

    case '<' :				/* Start dictionary or hex string */
        if (cur[1] == '<')
	{
	  obj.type = CUPS_PS_START_DICT;
	  cur += 2;
	}
	else
	{
          obj.type = CUPS_PS_STRING;
	  start    = cur;

	  for (cur ++, valptr = obj.value.string,
	           valend = obj.value.string + sizeof(obj.value.string) - 1;
               *cur;
	       cur ++)
	  {
	    int	ch;			/* Current character */



            if (*cur == '>')
	      break;
	    else if (valptr >= valend || !isxdigit(*cur & 255))
	    {
	      *ptr = start;
	      return (NULL);
	    }

            if (*cur >= '0' && *cur <= '9')
	      ch = (*cur - '0') << 4;
	    else
	      ch = (tolower(*cur) - 'a' + 10) << 4;

	    if (isxdigit(cur[1] & 255))
	    {
	      cur ++;

              if (*cur >= '0' && *cur <= '9')
		ch |= *cur - '0';
	      else
		ch |= tolower(*cur) - 'a' + 10;
            }

	    *valptr++ = (char)ch;
          }

          if (*cur != '>')
	  {
	    *ptr = start;
	    return (NULL);
	  }

	  cur ++;
	}
        break;

    case '>' :				/* End dictionary? */
        if (cur[1] == '>')
	{
	  obj.type = CUPS_PS_END_DICT;
	  cur += 2;
	}
	else
	{
	  obj.type           = CUPS_PS_OTHER;
	  obj.value.other[0] = *cur;

	  cur ++;
	}
        break;

    case '{' :				/* Start procedure */
        obj.type = CUPS_PS_START_PROC;
	cur ++;
        break;

    case '}' :				/* End procedure */
        obj.type = CUPS_PS_END_PROC;
	cur ++;
        break;

    case '-' :				/* Possible number */
    case '+' :
        if (!isdigit(cur[1] & 255) && cur[1] != '.')
	{
	  obj.type           = CUPS_PS_OTHER;
	  obj.value.other[0] = *cur;

	  cur ++;
	  break;
	}

    case '0' :				/* Number */
    case '1' :
    case '2' :
    case '3' :
    case '4' :
    case '5' :
    case '6' :
    case '7' :
    case '8' :
    case '9' :
    case '.' :
        obj.type = CUPS_PS_NUMBER;

        start = cur;
	for (cur ++; *cur; cur ++)
	  if (!isdigit(*cur & 255))
	    break;

        if (*cur == '#')
	{
	 /*
	  * Integer with radix...
	  */

          obj.value.number = strtol(cur + 1, &cur, atoi(start));
	  break;
	}
	else if (strchr(".Ee()<>[]{}/%", *cur) || isspace(*cur & 255))
	{
	 /*
	  * Integer or real number...
	  */

	  obj.value.number = _cupsStrScand(start, &cur, localeconv());
          break;
	}
	else
	  cur = start;

    default :				/* Operator/variable name */
        start = cur;

	if (*cur == '/')
	{
	  obj.type = CUPS_PS_NAME;
          valptr   = obj.value.name;
          valend   = obj.value.name + sizeof(obj.value.name) - 1;
	  cur ++;
	}
	else
	{
	  obj.type = CUPS_PS_OTHER;
          valptr   = obj.value.other;
          valend   = obj.value.other + sizeof(obj.value.other) - 1;
	}

	while (*cur)
	{
	  if (strchr("()<>[]{}/%", *cur) || isspace(*cur & 255))
	    break;
	  else if (valptr < valend)
	    *valptr++ = *cur++;
	  else
	  {
	    *ptr = start;
	    return (NULL);
	  }
	}

        if (obj.type == CUPS_PS_OTHER)
	{
          if (!strcmp(obj.value.other, "true"))
	  {
	    obj.type          = CUPS_PS_BOOLEAN;
	    obj.value.boolean = 1;
	  }
	  else if (!strcmp(obj.value.other, "false"))
	  {
	    obj.type          = CUPS_PS_BOOLEAN;
	    obj.value.boolean = 0;
	  }
	  else if (!strcmp(obj.value.other, "null"))
	    obj.type = CUPS_PS_NULL;
	  else if (!strcmp(obj.value.other, "cleartomark"))
	    obj.type = CUPS_PS_CLEARTOMARK;
	  else if (!strcmp(obj.value.other, "copy"))
	    obj.type = CUPS_PS_COPY;
	  else if (!strcmp(obj.value.other, "dup"))
	    obj.type = CUPS_PS_DUP;
	  else if (!strcmp(obj.value.other, "index"))
	    obj.type = CUPS_PS_INDEX;
	  else if (!strcmp(obj.value.other, "pop"))
	    obj.type = CUPS_PS_POP;
	  else if (!strcmp(obj.value.other, "roll"))
	    obj.type = CUPS_PS_ROLL;
	  else if (!strcmp(obj.value.other, "setpagedevice"))
	    obj.type = CUPS_PS_SETPAGEDEVICE;
	  else if (!strcmp(obj.value.other, "stopped"))
	    obj.type = CUPS_PS_STOPPED;
	}
	break;
  }

 /*
  * Save the current position in the string and return the new object...
  */

  *ptr = cur;

  return (push_stack(st, &obj));
}


/*
 * 'setpagedevice()' - Simulate the PostScript setpagedevice operator.
 */

static int				/* O - 0 on success, -1 on error */
setpagedevice(
    _cups_ps_stack_t    *st,		/* I - Stack */
    cups_page_header2_t *h,		/* O - Page header */
    int                 *preferred_bits)/* O - Preferred bits per color */
{
  int			i;		/* Index into array */
  _cups_ps_obj_t	*obj,		/* Current object */
			*end;		/* End of dictionary */
  const char		*name;		/* Attribute name */


 /*
  * Make sure we have a dictionary on the stack...
  */

  if (st->num_objs == 0)
    return (-1);

  obj = end = st->objs + st->num_objs - 1;

  if (obj->type != CUPS_PS_END_DICT)
    return (-1);

  obj --;

  while (obj > st->objs)
  {
    if (obj->type == CUPS_PS_START_DICT)
      break;

    obj --;
  }

  if (obj < st->objs)
    return (-1);

 /*
  * Found the start of the dictionary, empty the stack to this point...
  */

  st->num_objs = (int)(obj - st->objs);

 /*
  * Now pull /name and value pairs from the dictionary...
  */

  DEBUG_puts("3setpagedevice: Dictionary:");

  for (obj ++; obj < end; obj ++)
  {
   /*
    * Grab the name...
    */

    if (obj->type != CUPS_PS_NAME)
      return (-1);

    name = obj->value.name;
    obj ++;

#ifdef DEBUG
    DEBUG_printf(("4setpagedevice: /%s ", name));
    DEBUG_object("setpagedevice", obj);
#endif /* DEBUG */

   /*
    * Then grab the value...
    */

    if (!strcmp(name, "MediaClass") && obj->type == CUPS_PS_STRING)
      strlcpy(h->MediaClass, obj->value.string, sizeof(h->MediaClass));
    else if (!strcmp(name, "MediaColor") && obj->type == CUPS_PS_STRING)
      strlcpy(h->MediaColor, obj->value.string, sizeof(h->MediaColor));
    else if (!strcmp(name, "MediaType") && obj->type == CUPS_PS_STRING)
      strlcpy(h->MediaType, obj->value.string, sizeof(h->MediaType));
    else if (!strcmp(name, "OutputType") && obj->type == CUPS_PS_STRING)
      strlcpy(h->OutputType, obj->value.string, sizeof(h->OutputType));
    else if (!strcmp(name, "AdvanceDistance") && obj->type == CUPS_PS_NUMBER)
      h->AdvanceDistance = (unsigned)obj->value.number;
    else if (!strcmp(name, "AdvanceMedia") && obj->type == CUPS_PS_NUMBER)
      h->AdvanceMedia = (unsigned)obj->value.number;
    else if (!strcmp(name, "Collate") && obj->type == CUPS_PS_BOOLEAN)
      h->Collate = (unsigned)obj->value.boolean;
    else if (!strcmp(name, "CutMedia") && obj->type == CUPS_PS_NUMBER)
      h->CutMedia = (cups_cut_t)(unsigned)obj->value.number;
    else if (!strcmp(name, "Duplex") && obj->type == CUPS_PS_BOOLEAN)
      h->Duplex = (unsigned)obj->value.boolean;
    else if (!strcmp(name, "HWResolution") && obj->type == CUPS_PS_START_ARRAY)
    {
      if (obj[1].type == CUPS_PS_NUMBER && obj[2].type == CUPS_PS_NUMBER &&
          obj[3].type == CUPS_PS_END_ARRAY)
      {
        h->HWResolution[0] = (unsigned)obj[1].value.number;
	h->HWResolution[1] = (unsigned)obj[2].value.number;
	obj += 3;
      }
      else
        return (-1);
    }
    else if (!strcmp(name, "InsertSheet") && obj->type == CUPS_PS_BOOLEAN)
      h->InsertSheet = (unsigned)obj->value.boolean;
    else if (!strcmp(name, "Jog") && obj->type == CUPS_PS_NUMBER)
      h->Jog = (unsigned)obj->value.number;
    else if (!strcmp(name, "LeadingEdge") && obj->type == CUPS_PS_NUMBER)
      h->LeadingEdge = (unsigned)obj->value.number;
    else if (!strcmp(name, "ManualFeed") && obj->type == CUPS_PS_BOOLEAN)
      h->ManualFeed = (unsigned)obj->value.boolean;
    else if ((!strcmp(name, "cupsMediaPosition") ||
              !strcmp(name, "MediaPosition")) && obj->type == CUPS_PS_NUMBER)
    {
     /*
      * cupsMediaPosition is supported for backwards compatibility only.
      * We added it back in the Ghostscript 5.50 days to work around a
      * bug in Ghostscript WRT handling of MediaPosition and setpagedevice.
      *
      * All new development should set MediaPosition...
      */

      h->MediaPosition = (unsigned)obj->value.number;
    }
    else if (!strcmp(name, "MediaWeight") && obj->type == CUPS_PS_NUMBER)
      h->MediaWeight = (unsigned)obj->value.number;
    else if (!strcmp(name, "MirrorPrint") && obj->type == CUPS_PS_BOOLEAN)
      h->MirrorPrint = (unsigned)obj->value.boolean;
    else if (!strcmp(name, "NegativePrint") && obj->type == CUPS_PS_BOOLEAN)
      h->NegativePrint = (unsigned)obj->value.boolean;
    else if (!strcmp(name, "NumCopies") && obj->type == CUPS_PS_NUMBER)
      h->NumCopies = (unsigned)obj->value.number;
    else if (!strcmp(name, "Orientation") && obj->type == CUPS_PS_NUMBER)
      h->Orientation = (unsigned)obj->value.number;
    else if (!strcmp(name, "OutputFaceUp") && obj->type == CUPS_PS_BOOLEAN)
      h->OutputFaceUp = (unsigned)obj->value.boolean;
    else if (!strcmp(name, "PageSize") && obj->type == CUPS_PS_START_ARRAY)
    {
      if (obj[1].type == CUPS_PS_NUMBER && obj[2].type == CUPS_PS_NUMBER &&
          obj[3].type == CUPS_PS_END_ARRAY)
      {
        h->cupsPageSize[0] = (float)obj[1].value.number;
	h->cupsPageSize[1] = (float)obj[2].value.number;

        h->PageSize[0] = (unsigned)obj[1].value.number;
	h->PageSize[1] = (unsigned)obj[2].value.number;

	obj += 3;
      }
      else
        return (-1);
    }
    else if (!strcmp(name, "Separations") && obj->type == CUPS_PS_BOOLEAN)
      h->Separations = (unsigned)obj->value.boolean;
    else if (!strcmp(name, "TraySwitch") && obj->type == CUPS_PS_BOOLEAN)
      h->TraySwitch = (unsigned)obj->value.boolean;
    else if (!strcmp(name, "Tumble") && obj->type == CUPS_PS_BOOLEAN)
      h->Tumble = (unsigned)obj->value.boolean;
    else if (!strcmp(name, "cupsMediaType") && obj->type == CUPS_PS_NUMBER)
      h->cupsMediaType = (unsigned)obj->value.number;
    else if (!strcmp(name, "cupsBitsPerColor") && obj->type == CUPS_PS_NUMBER)
      h->cupsBitsPerColor = (unsigned)obj->value.number;
    else if (!strcmp(name, "cupsPreferredBitsPerColor") &&
             obj->type == CUPS_PS_NUMBER)
      *preferred_bits = (int)obj->value.number;
    else if (!strcmp(name, "cupsColorOrder") && obj->type == CUPS_PS_NUMBER)
      h->cupsColorOrder = (cups_order_t)(unsigned)obj->value.number;
    else if (!strcmp(name, "cupsColorSpace") && obj->type == CUPS_PS_NUMBER)
      h->cupsColorSpace = (cups_cspace_t)(unsigned)obj->value.number;
    else if (!strcmp(name, "cupsCompression") && obj->type == CUPS_PS_NUMBER)
      h->cupsCompression = (unsigned)obj->value.number;
    else if (!strcmp(name, "cupsRowCount") && obj->type == CUPS_PS_NUMBER)
      h->cupsRowCount = (unsigned)obj->value.number;
    else if (!strcmp(name, "cupsRowFeed") && obj->type == CUPS_PS_NUMBER)
      h->cupsRowFeed = (unsigned)obj->value.number;
    else if (!strcmp(name, "cupsRowStep") && obj->type == CUPS_PS_NUMBER)
      h->cupsRowStep = (unsigned)obj->value.number;
    else if (!strcmp(name, "cupsBorderlessScalingFactor") &&
             obj->type == CUPS_PS_NUMBER)
      h->cupsBorderlessScalingFactor = (float)obj->value.number;
    else if (!strncmp(name, "cupsInteger", 11) && obj->type == CUPS_PS_NUMBER)
    {
      if ((i = atoi(name + 11)) < 0 || i > 15)
        return (-1);

      h->cupsInteger[i] = (unsigned)obj->value.number;
    }
    else if (!strncmp(name, "cupsReal", 8) && obj->type == CUPS_PS_NUMBER)
    {
      if ((i = atoi(name + 8)) < 0 || i > 15)
        return (-1);

      h->cupsReal[i] = (float)obj->value.number;
    }
    else if (!strncmp(name, "cupsString", 10) && obj->type == CUPS_PS_STRING)
    {
      if ((i = atoi(name + 10)) < 0 || i > 15)
        return (-1);

      strlcpy(h->cupsString[i], obj->value.string, sizeof(h->cupsString[i]));
    }
    else if (!strcmp(name, "cupsMarkerType") && obj->type == CUPS_PS_STRING)
      strlcpy(h->cupsMarkerType, obj->value.string, sizeof(h->cupsMarkerType));
    else if (!strcmp(name, "cupsPageSizeName") && obj->type == CUPS_PS_STRING)
      strlcpy(h->cupsPageSizeName, obj->value.string,
              sizeof(h->cupsPageSizeName));
    else if (!strcmp(name, "cupsRenderingIntent") &&
             obj->type == CUPS_PS_STRING)
      strlcpy(h->cupsRenderingIntent, obj->value.string,
              sizeof(h->cupsRenderingIntent));
    else
    {
     /*
      * Ignore unknown name+value...
      */

      DEBUG_printf(("4setpagedevice: Unknown name (\"%s\") or value...\n", name));

      while (obj[1].type != CUPS_PS_NAME && obj < end)
        obj ++;
    }
  }

  return (0);
}


#ifdef DEBUG
/*
 * 'DEBUG_object()' - Print an object's value...
 */

static void
DEBUG_object(const char *prefix,	/* I - Prefix string */
             _cups_ps_obj_t *obj)	/* I - Object to print */
{
  switch (obj->type)
  {
    case CUPS_PS_NAME :
	DEBUG_printf(("4%s: /%s\n", prefix, obj->value.name));
	break;

    case CUPS_PS_NUMBER :
	DEBUG_printf(("4%s: %g\n", prefix, obj->value.number));
	break;

    case CUPS_PS_STRING :
	DEBUG_printf(("4%s: (%s)\n", prefix, obj->value.string));
	break;

    case CUPS_PS_BOOLEAN :
	if (obj->value.boolean)
	  DEBUG_printf(("4%s: true", prefix));
	else
	  DEBUG_printf(("4%s: false", prefix));
	break;

    case CUPS_PS_NULL :
	DEBUG_printf(("4%s: null", prefix));
	break;

    case CUPS_PS_START_ARRAY :
	DEBUG_printf(("4%s: [", prefix));
	break;

    case CUPS_PS_END_ARRAY :
	DEBUG_printf(("4%s: ]", prefix));
	break;

    case CUPS_PS_START_DICT :
	DEBUG_printf(("4%s: <<", prefix));
	break;

    case CUPS_PS_END_DICT :
	DEBUG_printf(("4%s: >>", prefix));
	break;

    case CUPS_PS_START_PROC :
	DEBUG_printf(("4%s: {", prefix));
	break;

    case CUPS_PS_END_PROC :
	DEBUG_printf(("4%s: }", prefix));
	break;

    case CUPS_PS_CLEARTOMARK :
	DEBUG_printf(("4%s: --cleartomark--", prefix));
        break;

    case CUPS_PS_COPY :
	DEBUG_printf(("4%s: --copy--", prefix));
        break;

    case CUPS_PS_DUP :
	DEBUG_printf(("4%s: --dup--", prefix));
        break;

    case CUPS_PS_INDEX :
	DEBUG_printf(("4%s: --index--", prefix));
        break;

    case CUPS_PS_POP :
	DEBUG_printf(("4%s: --pop--", prefix));
        break;

    case CUPS_PS_ROLL :
	DEBUG_printf(("4%s: --roll--", prefix));
        break;

    case CUPS_PS_SETPAGEDEVICE :
	DEBUG_printf(("4%s: --setpagedevice--", prefix));
        break;

    case CUPS_PS_STOPPED :
	DEBUG_printf(("4%s: --stopped--", prefix));
        break;

    case CUPS_PS_OTHER :
	DEBUG_printf(("4%s: --%s--", prefix, obj->value.other));
	break;
  }
}


/*
 * 'DEBUG_stack()' - Print a stack...
 */

static void
DEBUG_stack(const char       *prefix,	/* I - Prefix string */
            _cups_ps_stack_t *st)	/* I - Stack */
{
  int			c;		/* Looping var */
  _cups_ps_obj_t	*obj;		/* Current object on stack */


  for (obj = st->objs, c = st->num_objs; c > 0; c --, obj ++)
    DEBUG_object(prefix, obj);
}
#endif /* DEBUG */


/*
 * End of "$Id: interpret.c 12748 2015-06-24 15:58:40Z msweet $".
 */
