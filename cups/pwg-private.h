/*
 * "$Id$"
 *
 *   Private PWG media API definitions for CUPS.
 *
 *   Copyright 2009-2012 by Apple Inc.
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
#  define _PWG_FROMPTS(n)	(int)(((n) * 2540 + 36) / 72)
/* Convert from 2540ths to points */
#  define _PWG_TOPTS(n)		((n) * 72.0 / 2540.0)


/*
 * Types and structures...
 */

typedef struct _pwg_map_s		/**** Map element - PPD to/from PWG */
{
  char		*pwg,			/* PWG media keyword */
		*ppd;			/* PPD option keyword */
} _pwg_map_t;

typedef struct _pwg_media_s		/**** Common media size data ****/
{
  const char	*pwg,			/* PWG 5101.1 "self describing" name */
		*legacy,		/* IPP/ISO legacy name */
		*ppd;			/* Standard Adobe PPD name */
  int		width,			/* Width in 2540ths */
		length;			/* Length in 2540ths */
} _pwg_media_t;

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


/*
 * Functions...
 */

extern void		_pwgGenerateSize(char *keyword, size_t keysize,
				         const char *prefix,
					 const char *name,
					 int width, int length);
extern int		_pwgInitSize(_pwg_size_t *size, ipp_t *job,
				     int *margins_set);
extern _pwg_media_t	*_pwgMediaForLegacy(const char *legacy);
extern _pwg_media_t	*_pwgMediaForPPD(const char *ppd);
extern _pwg_media_t	*_pwgMediaForPWG(const char *pwg);
extern _pwg_media_t	*_pwgMediaForSize(int width, int length);


#  ifdef __cplusplus
}
#  endif /* __cplusplus */

#endif /* !_CUPS_PWG_PRIVATE_H_ */

/*
 * End of "$Id$".
 */
