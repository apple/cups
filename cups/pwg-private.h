/*
 * "$Id: pwg-private.h 11826 2014-04-23 00:38:21Z msweet $"
 *
 *   Private PWG media API definitions for CUPS.
 *
 *   Copyright 2009-2013 by Apple Inc.
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
 * Deprecated stuff for prior users of the private PWG media API...
 */

#  ifndef _CUPS_NO_DEPRECATED
typedef struct pwg_map_s _pwg_map_t;
typedef struct pwg_media_s _pwg_media_t;
typedef struct pwg_size_s _pwg_size_t;
#  endif /* _CUPS_NO_DEPRECATED */


/*
 * Functions...
 */

extern void		_pwgGenerateSize(char *keyword, size_t keysize,
				         const char *prefix,
					 const char *name,
					 int width, int length)
					 _CUPS_INTERNAL_MSG("Use pwgFormatSizeName instead.");
extern int		_pwgInitSize(pwg_size_t *size, ipp_t *job,
				     int *margins_set)
				     _CUPS_INTERNAL_MSG("Use pwgInitSize instead.");
extern pwg_media_t	*_pwgMediaForLegacy(const char *legacy)
			    _CUPS_INTERNAL_MSG("Use pwgMediaForLegacy instead.");
extern pwg_media_t	*_pwgMediaForPPD(const char *ppd)
			    _CUPS_INTERNAL_MSG("Use pwgMediaForPPD instead.");
extern pwg_media_t	*_pwgMediaForPWG(const char *pwg)
			    _CUPS_INTERNAL_MSG("Use pwgMediaForPWG instead.");
extern pwg_media_t	*_pwgMediaForSize(int width, int length)
			    _CUPS_INTERNAL_MSG("Use pwgMediaForSize instead.");
extern const pwg_media_t *_pwgMediaTable(size_t *num_media);
extern pwg_media_t *_pwgMediaNearSize(int width, int length, int epsilon);

#  ifdef __cplusplus
}
#  endif /* __cplusplus */

#endif /* !_CUPS_PWG_PRIVATE_H_ */

/*
 * End of "$Id: pwg-private.h 11826 2014-04-23 00:38:21Z msweet $".
 */
