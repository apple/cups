/*
 * "$Id$"
 *
 *   Private PPD definitions for CUPS.
 *
 *   Copyright 2007-2010 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   PostScript is a trademark of Adobe Systems, Inc.
 *
 *   This code and any derivative of it may be used and distributed
 *   freely under the terms of the GNU General Public License when
 *   used with GNU Ghostscript or its derivatives.  Use of the code
 *   (or any derivative of it) with software other than GNU
 *   GhostScript (or its derivatives) is governed by the CUPS license
 *   agreement.
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 */

#ifndef _CUPS_PPD_PRIVATE_H_
#  define _CUPS_PPD_PRIVATE_H_

/*
 * Include necessary headers...
 */

#  include <cups/cups.h>
#  include <cups/ppd.h>
#  include "pwg-private.h"


/*
 * C++ magic...
 */

#  ifdef __cplusplus
extern "C" {
#  endif /* __cplusplus */


/*
 * Types and structures...
 */

typedef enum _ppd_parse_e		/**** Selector for _ppdParseOptions ****/
{
  _PPD_PARSE_OPTIONS,			/* Parse only the options */
  _PPD_PARSE_PROPERTIES,		/* Parse only the properties */
  _PPD_PARSE_ALL			/* Parse everything */
} _ppd_parse_t;

typedef struct _ppd_cups_uiconst_s	/**** Constraint from cupsUIConstraints ****/
{
  ppd_option_t	*option;		/* Constrained option */
  ppd_choice_t	*choice;		/* Constrained choice or @code NULL@ */
  int		installable;		/* Installable option? */
} _ppd_cups_uiconst_t;

typedef struct _ppd_cups_uiconsts_s	/**** cupsUIConstraints ****/
{
  char		resolver[PPD_MAX_NAME];	/* Resolver name */
  int		installable,		/* Constrained against any installable options? */
		num_constraints;	/* Number of constraints */
  _ppd_cups_uiconst_t *constraints;	/* Constraints */
} _ppd_cups_uiconsts_t;

typedef enum _pwg_output_mode_e		/**** PWG output-mode indices ****/
{
  _PWG_OUTPUT_MODE_MONOCHROME = 0,	/* output-mode=monochrome */
  _PWG_OUTPUT_MODE_COLOR,		/* output-mode=color */
  _PWG_OUTPUT_MODE_MAX
} _pwg_output_mode_t;

typedef enum _pwg_print_quality_e	/**** PWG print-quality indices ****/
{
  _PWG_PRINT_QUALITY_DRAFT = 0,		/* print-quality=3 */
  _PWG_PRINT_QUALITY_NORMAL,		/* print-quality=4 */
  _PWG_PRINT_QUALITY_HIGH,		/* print-quality=5 */
  _PWG_PRINT_QUALITY_MAX
} _pwg_print_quality_t;

typedef struct _pwg_s			/**** PWG-PPD conversion data ****/
{
  int		num_bins;		/* Number of output bins */
  _pwg_map_t	*bins;			/* Output bins */
  int		num_sizes;		/* Number of media sizes */
  _pwg_size_t	*sizes;			/* Media sizes */
  int		custom_max_width,	/* Maximum custom width in 2540ths */
		custom_max_length,	/* Maximum custom length in 2540ths */
		custom_min_width,	/* Minimum custom width in 2540ths */
		custom_min_length;	/* Minimum custom length in 2540ths */
  char		*custom_max_keyword,	/* Maximum custom size PWG keyword */
		*custom_min_keyword,	/* Minimum custom size PWG keyword */
		custom_ppd_size[41];	/* Custom PPD size name */
  _pwg_size_t	custom_size;		/* Custom size record */
  char		*source_option;		/* PPD option for media source */
  int		num_sources;		/* Number of media sources */
  _pwg_map_t	*sources;		/* Media sources */
  int		num_types;		/* Number of media types */
  _pwg_map_t	*types;			/* Media types */
  int		num_presets[_PWG_OUTPUT_MODE_MAX][_PWG_PRINT_QUALITY_MAX];
					/* Number of output-mode/print-quality options */
  cups_option_t	*presets[_PWG_OUTPUT_MODE_MAX][_PWG_PRINT_QUALITY_MAX];
					/* output-mode/print-quality options */
  char		*sides_option,		/* PPD option for sides */
		*sides_1sided,		/* Choice for one-sided */
		*sides_2sided_long,	/* Choice for two-sided-long-edge */
		*sides_2sided_short;	/* Choice for two-sided-short-edge */
} _pwg_t;


/*
 * Prototypes...
 */

extern void		_ppdFreeLanguages(cups_array_t *languages);
extern cups_encoding_t	_ppdGetEncoding(const char *name);
extern cups_array_t	*_ppdGetLanguages(ppd_file_t *ppd);
extern unsigned		_ppdHashName(const char *name);
extern ppd_attr_t	*_ppdLocalizedAttr(ppd_file_t *ppd, const char *keyword,
			                   const char *spec, const char *ll_CC);
extern char		*_ppdNormalizeMakeAndModel(const char *make_and_model,
			                           char *buffer,
						   size_t bufsize);
extern int		_ppdParseOptions(const char *s, int num_options,
			                 cups_option_t **options,
					 _ppd_parse_t which);
extern _pwg_t		*_pwgCreateWithFile(const char *filename);
extern _pwg_t		*_pwgCreateWithPPD(ppd_file_t *ppd);
extern void		_pwgDestroy(_pwg_t *pwg);
extern const char	*_pwgGetBin(_pwg_t *pwg, const char *output_bin);
extern const char	*_pwgGetInputSlot(_pwg_t *pwg, ipp_t *job,
			                  const char *keyword);
extern const char	*_pwgGetMediaType(_pwg_t *pwg, ipp_t *job,
			                  const char *keyword);
extern const char	*_pwgGetOutputBin(_pwg_t *pwg, const char *keyword);
extern const char	*_pwgGetPageSize(_pwg_t *pwg, ipp_t *job,
			                 const char *keyword, int *exact);
extern _pwg_size_t	*_pwgGetSize(_pwg_t *pwg, const char *page_size);
extern const char	*_pwgGetSource(_pwg_t *pwg, const char *input_slot);
extern const char	*_pwgGetType(_pwg_t *pwg, const char *media_type);
extern const char	*_pwgInputSlotForSource(const char *media_source,
			                        char *name, size_t namesize);
extern const char	*_pwgMediaTypeForType(const char *media_type,
			                      char *name, size_t namesize);
extern const char	*_pwgPageSizeForMedia(_pwg_media_t *media,
			                      char *name, size_t namesize);
extern int		_pwgWriteFile(_pwg_t *pwg, const char *filename);


/*
 * C++ magic...
 */

#  ifdef __cplusplus
}
#  endif /* __cplusplus */
#endif /* !_CUPS_PPD_PRIVATE_H_ */

/*
 * End of "$Id$".
 */
