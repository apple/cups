/*
 * "$Id$"
 *
 *   PWG media name API definitions for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2009 by Apple Inc.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 */

#ifndef _CUPS_PWGMEDIA_H_
#  define _CUPS_PWGMEDIA_H_


/*
 * C++ magic...
 */

#  ifdef __cplusplus
extern "C" {
#  endif /* __cplusplus */


/*
 * Types and structures...
 */

typedef struct _cups_pwg_media_s	/**** Common media size data ****/
{
  const char	*pwg,			/* PWG 5101.1 "self describing" name */
		*legacy;		/* IPP/ISO legacy name */
  double	width,			/* Width in points */
		length;			/* Length in points */
} _cups_pwg_media_t;


/*
 * Functions...
 */

extern _cups_pwg_media_t	*_cupsPWGMediaByLegacy(const char *legacy);
extern _cups_pwg_media_t	*_cupsPWGMediaByName(const char *pwg);
extern _cups_pwg_media_t	*_cupsPWGMediaBySize(double width,
				                     double length);


#  ifdef __cplusplus
}
#  endif /* __cplusplus */

#endif /* !_CUPS_PWGMEDIA_H_ */

/*
 * End of "$Id$".
 */
