/*
  Copyright 1993-2001 by Easy Software Products
  Copyright (C) 1994, 1995, 1998 Aladdin Enterprises.  All rights reserved.

  This file is part of GNU Ghostscript.

  GNU Ghostscript is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY.  No author or distributor accepts responsibility
  to anyone for the consequences of using it or for whether it serves any
  particular purpose or works at all, unless he says so in writing.  Refer
  to the GNU General Public License for full details.

  Everyone is granted permission to copy, modify and redistribute GNU
  Ghostscript, but only under the conditions described in the GNU General
  Public License.  A copy of this license is supposed to have been given
  to you along with GNU Ghostscript so you can know your rights and
  responsibilities.  It should be in a file named COPYING.  Among other
  things, the copyright notice and this notice must be preserved on all
  copies.

  Aladdin Enterprises supports the work of the GNU Project, but is not
  affiliated with the Free Software Foundation or the GNU Project.  GNU
  Ghostscript, as distributed by Aladdin Enterprises, does not require any
  GNU software to build or run it.
*/

#include <config.h>
#ifdef HAVE_LIBJPEG
/*$Id: sjpegerr.c,v 1.5 2001/01/22 15:03:56 mike Exp $ */
/* IJG error message table for Ghostscript. */
#include "stdio_.h"
#include "jpeglib.h"

/*
 * MRS - these are normally found in jversion.h, however it seems that
 * many vendors (Red Hat, SGI, etc.) do not distribute it...
 *
 * The following definitions come from the 6B distribution...
 */

#define JVERSION	"6b  27-Mar-1998"
#define JCOPYRIGHT	"Copyright (C) 1998, Thomas G. Lane"

/*
 * This file exists solely to hold the rather large IJG error message string
 * table (now about 4K, and likely to grow in future releases).  The table
 * is large enough that we don't want it to be in the default data segment
 * in a 16-bit executable.
 *
 * In IJG version 5 and earlier, under Borland C, this is accomplished simply
 * by compiling this one file in "huge" memory model rather than "large".
 * The string constants will then go into a private far data segment.
 * In less brain-damaged architectures, this file is simply compiled normally,
 * and we pay only the price of one extra function call.
 *
 * In IJG version 5a and later, under Borland C, this is accomplished by making
 * each string be a separate variable that's explicitly declared "far".
 * (What a crock...)
 *
 * This must be a separate file to avoid duplicate-symbol errors, since we
 * use the IJG message code names as variables rather than as enum constants.
 */

#if JPEG_LIB_VERSION <= 50	/**************** *************** */

#include "jerror.h"		/* get error codes */
#define JMAKE_MSG_TABLE
#include "jerror.h"		/* create message string table */

#define jpeg_std_message_table jpeg_message_table

#else	/* JPEG_LIB_VERSION >= 51 */ /**************** *************** */

/* Create a static const char[] variable for each message string. */

#define JMESSAGE(code,string)	static const char code[] = string;

#include "jerror.h"

/* Now build an array of pointers to same. */

#define JMESSAGE(code,string)	code ,

static const char *const jpeg_std_message_table[] =
{
#include "jerror.h"
    NULL
};

#endif	/* JPEG_LIB_VERSION */ /**************** *************** */

/*
 * Return a pointer to the message table.
 * It is unsafe to do much more than this within the "huge" environment.
 */

const char *const *
gs_jpeg_message_table(void)
{
    return jpeg_std_message_table;
}
#endif /* HAVE_LIBJPEG */
