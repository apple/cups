/*
 * Private PPD definitions for CUPS.
 *
 * Copyright © 2007-2019 by Apple Inc.
 * Copyright © 1997-2007 by Easy Software Products, all rights reserved.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more
 * information.
 *
 * PostScript is a trademark of Adobe Systems, Inc.
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
 * Constants...
 */

#  define _PPD_CACHE_VERSION	9	/* Version number in cache file */


/*
 * Types and structures...
 */

typedef struct _ppd_globals_s		/**** CUPS PPD global state data ****/
{
  /* ppd.c */
  ppd_status_t		ppd_status;	/* Status of last ppdOpen*() */
  int			ppd_line;	/* Current line number */
  ppd_conform_t		ppd_conform;	/* Level of conformance required */

  /* ppd-util.c */
  char			ppd_filename[HTTP_MAX_URI];
					/* PPD filename */
} _ppd_globals_t;

typedef enum _ppd_localization_e	/**** Selector for _ppdOpen ****/
{
  _PPD_LOCALIZATION_DEFAULT,		/* Load only the default localization */
  _PPD_LOCALIZATION_ICC_PROFILES,	/* Load only the color profile localization */
  _PPD_LOCALIZATION_NONE,		/* Load no localizations */
  _PPD_LOCALIZATION_ALL			/* Load all localizations */
} _ppd_localization_t;

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

typedef enum _pwg_print_color_mode_e	/**** PWG print-color-mode indices ****/
{
  _PWG_PRINT_COLOR_MODE_MONOCHROME = 0,	/* print-color-mode=monochrome */
  _PWG_PRINT_COLOR_MODE_COLOR,		/* print-color-mode=color */
  /* Other values are not supported by CUPS yet. */
  _PWG_PRINT_COLOR_MODE_MAX
} _pwg_print_color_mode_t;

typedef enum _pwg_print_quality_e	/**** PWG print-quality values ****/
{
  _PWG_PRINT_QUALITY_DRAFT = 0,		/* print-quality=3 */
  _PWG_PRINT_QUALITY_NORMAL,		/* print-quality=4 */
  _PWG_PRINT_QUALITY_HIGH,		/* print-quality=5 */
  _PWG_PRINT_QUALITY_MAX
} _pwg_print_quality_t;

typedef struct _pwg_finishings_s	/**** PWG finishings mapping data ****/
{
  ipp_finishings_t	value;		/* finishings value */
  int			num_options;	/* Number of options to apply */
  cups_option_t		*options;	/* Options to apply */
} _pwg_finishings_t;

struct _ppd_cache_s			/**** PPD cache and PWG conversion data ****/
{
  int		num_bins;		/* Number of output bins */
  pwg_map_t	*bins;			/* Output bins */
  int		num_sizes;		/* Number of media sizes */
  pwg_size_t	*sizes;			/* Media sizes */
  int		custom_max_width,	/* Maximum custom width in 2540ths */
		custom_max_length,	/* Maximum custom length in 2540ths */
		custom_min_width,	/* Minimum custom width in 2540ths */
		custom_min_length;	/* Minimum custom length in 2540ths */
  char		*custom_max_keyword,	/* Maximum custom size PWG keyword */
		*custom_min_keyword,	/* Minimum custom size PWG keyword */
		custom_ppd_size[41];	/* Custom PPD size name */
  pwg_size_t	custom_size;		/* Custom size record */
  char		*source_option;		/* PPD option for media source */
  int		num_sources;		/* Number of media sources */
  pwg_map_t	*sources;		/* Media sources */
  int		num_types;		/* Number of media types */
  pwg_map_t	*types;			/* Media types */
  int		num_presets[_PWG_PRINT_COLOR_MODE_MAX][_PWG_PRINT_QUALITY_MAX];
					/* Number of print-color-mode/print-quality options */
  cups_option_t	*presets[_PWG_PRINT_COLOR_MODE_MAX][_PWG_PRINT_QUALITY_MAX];
					/* print-color-mode/print-quality options */
  char		*sides_option,		/* PPD option for sides */
		*sides_1sided,		/* Choice for one-sided */
		*sides_2sided_long,	/* Choice for two-sided-long-edge */
		*sides_2sided_short;	/* Choice for two-sided-short-edge */
  char		*product;		/* Product value */
  cups_array_t	*filters,		/* cupsFilter/cupsFilter2 values */
		*prefilters;		/* cupsPreFilter values */
  int		single_file;		/* cupsSingleFile value */
  cups_array_t	*finishings;		/* cupsIPPFinishings values */
  cups_array_t	*templates;		/* cupsFinishingTemplate values */
  int		max_copies,		/* cupsMaxCopies value */
		account_id,		/* cupsJobAccountId value */
		accounting_user_id;	/* cupsJobAccountingUserId value */
  char		*password;		/* cupsJobPassword value */
  cups_array_t	*mandatory;		/* cupsMandatory value */
  char		*charge_info_uri;	/* cupsChargeInfoURI value */
  cups_array_t	*strings;		/* Localization strings */
  cups_array_t	*support_files;		/* Support files - ICC profiles, etc. */
};


/*
 * Prototypes...
 */

extern int		_cupsConvertOptions(ipp_t *request, ppd_file_t *ppd, _ppd_cache_t *pc, ipp_attribute_t *media_col_sup, ipp_attribute_t *doc_handling_sup, ipp_attribute_t *print_color_mode_sup, const char *user, const char *format, int copies, int num_options, cups_option_t *options) _CUPS_PRIVATE;
extern int		_cupsRasterExecPS(cups_page_header2_t *h, int *preferred_bits, const char *code) _CUPS_NONNULL(3) _CUPS_PRIVATE;
extern int		_cupsRasterInterpretPPD(cups_page_header2_t *h, ppd_file_t *ppd, int num_options, cups_option_t *options, cups_interpret_cb_t func) _CUPS_PRIVATE;

extern _ppd_cache_t	*_ppdCacheCreateWithFile(const char *filename,
			                         ipp_t **attrs) _CUPS_PRIVATE;
extern _ppd_cache_t	*_ppdCacheCreateWithPPD(ppd_file_t *ppd) _CUPS_PRIVATE;
extern void		_ppdCacheDestroy(_ppd_cache_t *pc) _CUPS_PRIVATE;
extern const char	*_ppdCacheGetBin(_ppd_cache_t *pc,
			                 const char *output_bin) _CUPS_PRIVATE;
extern int		_ppdCacheGetFinishingOptions(_ppd_cache_t *pc,
			                             ipp_t *job,
			                             ipp_finishings_t value,
			                             int num_options,
			                             cups_option_t **options) _CUPS_PRIVATE;
extern int		_ppdCacheGetFinishingValues(ppd_file_t *ppd, _ppd_cache_t *pc, int max_values, int *values) _CUPS_PRIVATE;
extern const char	*_ppdCacheGetInputSlot(_ppd_cache_t *pc, ipp_t *job,
			                       const char *keyword) _CUPS_PRIVATE;
extern const char	*_ppdCacheGetMediaType(_ppd_cache_t *pc, ipp_t *job,
			                       const char *keyword) _CUPS_PRIVATE;
extern const char	*_ppdCacheGetOutputBin(_ppd_cache_t *pc,
			                       const char *keyword) _CUPS_PRIVATE;
extern const char	*_ppdCacheGetPageSize(_ppd_cache_t *pc, ipp_t *job,
			                      const char *keyword, int *exact) _CUPS_PRIVATE;
extern pwg_size_t	*_ppdCacheGetSize(_ppd_cache_t *pc,
			                  const char *page_size) _CUPS_PRIVATE;
extern const char	*_ppdCacheGetSource(_ppd_cache_t *pc,
			                    const char *input_slot) _CUPS_PRIVATE;
extern const char	*_ppdCacheGetType(_ppd_cache_t *pc,
			                  const char *media_type) _CUPS_PRIVATE;
extern int		_ppdCacheWriteFile(_ppd_cache_t *pc,
			                   const char *filename, ipp_t *attrs) _CUPS_PRIVATE;
extern char		*_ppdCreateFromIPP(char *buffer, size_t bufsize, ipp_t *response) _CUPS_PRIVATE;
extern void		_ppdFreeLanguages(cups_array_t *languages) _CUPS_PRIVATE;
extern cups_encoding_t	_ppdGetEncoding(const char *name) _CUPS_PRIVATE;
extern cups_array_t	*_ppdGetLanguages(ppd_file_t *ppd) _CUPS_PRIVATE;
extern _ppd_globals_t	*_ppdGlobals(void) _CUPS_PRIVATE;
extern unsigned		_ppdHashName(const char *name) _CUPS_PRIVATE;
extern ppd_attr_t	*_ppdLocalizedAttr(ppd_file_t *ppd, const char *keyword,
			                   const char *spec, const char *ll_CC) _CUPS_PRIVATE;
extern char		*_ppdNormalizeMakeAndModel(const char *make_and_model,
			                           char *buffer,
						   size_t bufsize) _CUPS_PRIVATE;
extern ppd_file_t	*_ppdOpen(cups_file_t *fp,
				  _ppd_localization_t localization) _CUPS_PRIVATE;
extern ppd_file_t	*_ppdOpenFile(const char *filename,
				      _ppd_localization_t localization) _CUPS_PRIVATE;
extern int		_ppdParseOptions(const char *s, int num_options,
			                 cups_option_t **options,
					 _ppd_parse_t which) _CUPS_PRIVATE;
extern const char	*_pwgInputSlotForSource(const char *media_source,
			                        char *name, size_t namesize) _CUPS_PRIVATE;
extern const char	*_pwgMediaTypeForType(const char *media_type,
					      char *name, size_t namesize) _CUPS_PRIVATE;
extern const char	*_pwgPageSizeForMedia(pwg_media_t *media,
			                      char *name, size_t namesize) _CUPS_PRIVATE;


/*
 * C++ magic...
 */

#  ifdef __cplusplus
}
#  endif /* __cplusplus */
#endif /* !_CUPS_PPD_PRIVATE_H_ */
