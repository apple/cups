/*
 * Private PWG media API definitions for CUPS.
 *
 * Copyright 2009-2016 by Apple Inc.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * missing or damaged, see the license at "http://www.cups.org/".
 *
 * This file is subject to the Apple OS-Developed Software exception.
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
extern const pwg_media_t *_pwgMediaTable(size_t *num_media);
extern pwg_media_t *_pwgMediaNearSize(int width, int length, int epsilon);

#  ifdef __cplusplus
}
#  endif /* __cplusplus */

#endif /* !_CUPS_PWG_PRIVATE_H_ */
