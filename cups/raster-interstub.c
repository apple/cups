/*
 * cupsRasterInterpretPPD stub for CUPS.
 *
 * Copyright © 2018 by Apple Inc.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more
 * information.
 */

/*
 * Include necessary headers...
 */

#include <cups/ppd-private.h>


/*
 * This stub wraps the _cupsRasterInterpretPPD function in libcups - this allows
 * one library to provide all of the CUPS API functions while still supporting
 * the old split library organization...
 */


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
 * @since CUPS 1.2/macOS 10.5@
 */

int					/* O - 0 on success, -1 on failure */
cupsRasterInterpretPPD(
    cups_page_header2_t *h,		/* O - Page header to create */
    ppd_file_t          *ppd,		/* I - PPD file */
    int                 num_options,	/* I - Number of options */
    cups_option_t       *options,	/* I - Options */
    cups_interpret_cb_t func)		/* I - Optional page header callback (@code NULL@ for none) */
{
  return (_cupsRasterInterpretPPD(h, ppd, num_options, options, func));
}
