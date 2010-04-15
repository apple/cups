/*
 * "$Id$"
 *
 *   Private PWG media API definitions for CUPS.
 *
 *   Copyright 2009-2010 by Apple Inc.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 */

#ifndef _CUPS_PWG_PRIVATE_H_
#  define _CUPS_PWG_PRIVATE_H_


/*
 * Include necessary headers...
 */

#  include <cups/cups.h>


/*
 * C++ magic...
 */

#  ifdef __cplusplus
extern "C" {
#  endif /* __cplusplus */


/*
 * Macros...
 */

/* Convert from points to 2540ths */
#  define _PWG_FROMPTS(n)	(int)((n) * 2540 / 72)
/* Convert from 2540ths to points */
#  define _PWG_TOPTS(n)		((n) * 72.0 / 2540.0)


/*
 * Types and structures...
 */

#  ifndef _CUPS_PPD_H_
typedef struct ppd_file_s ppd_file_t;
#  endif /* _CUPS_PPD_H_ */

typedef struct _pwg_media_s		/**** Common media size data ****/
{
  const char	*pwg,			/* PWG 5101.1 "self describing" name */
		*legacy,		/* IPP/ISO legacy name */
		*ppd;			/* Standard Adobe PPD name */
  int		width,			/* Width in 2540ths */
		length;			/* Length in 2540ths */
} _pwg_media_t;

typedef struct _pwg_map_s		/**** Map element - PPD to/from PWG */
{
  char		*pwg,			/* PWG media keyword */
		*ppd;			/* PPD option keyword */
} _pwg_map_t;

typedef struct _pwg_size_s		/**** Size element - PPD to/from PWG */
{
  _pwg_map_t	map;			/* Map element */
  int		width,			/* Width in 2540ths */
		length,			/* Length in 2540ths */
		left,			/* Left margin in 2540ths */
		bottom,			/* Bottom margin in 2540ths */
		right,			/* Right margin in 2540ths */
		top;			/* Top margin in 2540ths */
} _pwg_size_t;

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
  int		num_sources;		/* Number of media sources */
  _pwg_map_t	*sources;		/* Media sources */
  int		num_types;		/* Number of media types */
  _pwg_map_t	*types;			/* Media types */
} _pwg_t;


/*
 * Functions...
 */

extern _pwg_t		*_pwgCreateWithFile(const char *filename);
extern void		_pwgDestroy(_pwg_t *pwg);
extern void		_pwgGenerateSize(char *keyword, size_t keysize,
				         const char *prefix,
					 const char *ppdname,
					 int width, int length);
extern int		_pwgInitSize(_pwg_size_t *size, ipp_t *job,
				     int *margins_set);
extern _pwg_media_t	*_pwgMediaForLegacy(const char *legacy);
extern _pwg_media_t	*_pwgMediaForPWG(const char *pwg);
extern _pwg_media_t	*_pwgMediaForSize(int width, int length);
extern int		_pwgWriteFile(_pwg_t *pwg, const char *filename);


#  ifdef __cplusplus
}
#  endif /* __cplusplus */

#endif /* !_CUPS_PWG_PRIVATE_H_ */

/*
 * End of "$Id$".
 */
