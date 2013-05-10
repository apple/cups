/*
 * "$Id$"
 *
 *   Private PPD definitions for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007-2008 by Apple Inc.
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

#  include "cups.h"


/*
 * C++ magic...
 */

#  ifdef __cplusplus
extern "C" {
#  endif /* __cplusplus */


/*
 * Structures...
 */

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


/*
 * Prototypes...
 */

extern void		_ppdFreeLanguages(cups_array_t *languages);
extern int		_ppdGet1284Values(const char *device_id,
			                  cups_option_t **values);
extern cups_encoding_t	_ppdGetEncoding(const char *name);
extern cups_array_t	*_ppdGetLanguages(ppd_file_t *ppd);
extern unsigned		_ppdHashName(const char *name);
extern ppd_attr_t	*_ppdLocalizedAttr(ppd_file_t *ppd, const char *keyword,
			                   const char *spec, const char *ll_CC);
extern char		*_ppdNormalizeMakeAndModel(const char *make_and_model,
			                           char *buffer,
						   size_t bufsize);
extern int		_ppdParseOptions(const char *s, int num_options,
			                 cups_option_t **options);


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
