/*
 * "$Id: config.h,v 1.1 1999/12/21 02:26:49 mike Exp $"
 *
 *   Configuration file for the Common UNIX Printing System (CUPS) under WIN32.
 *
 *   Copyright 1997-1999 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Easy Software Products and are protected by Federal
 *   copyright law.  Distribution and use rights are outlined in the file
 *   "LICENSE.txt" which should have been included with this file.  If this
 *   file is missing or damaged please contact Easy Software Products
 *   at:
 *
 *       Attn: CUPS Licensing Information
 *       Easy Software Products
 *       44141 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 */

/*
 * Version of software...
 */

#define CUPS_SVERSION	"CUPS v1.0.3"

/*
 * Where are files stored?
 */

#define CUPS_LOCALEDIR	"C:/CUPS/locale"
#define CUPS_SERVERROOT	"C:/CUPS/var"
#define CUPS_DATADIR	"C:/CUPS/share"

/*
 * Do we have various image libraries?
 */

#undef HAVE_LIBPNG
#undef HAVE_LIBZ
#undef HAVE_LIBJPEG
#undef HAVE_LIBTIFF

/*
 * Does this machine store words in big-endian (MSB-first) order?
 */

#undef WORDS_BIGENDIAN

/*
 * Which directory functions and headers do we use?
 */

#undef HAVE_DIRENT_H
#undef HAVE_SYS_DIR_H
#undef HAVE_SYS_NDIR_H
#undef HAVE_NDIR_H

/*
 * Do we have PAM stuff?
 */

#ifndef HAVE_LIBPAM
#define HAVE_LIBPAM 0
#endif /* !HAVE_LIBPAM */

/*
 * Do we have <shadow.h>?
 */

#undef HAVE_SHADOW_H

/*
 * Do we have <crypt.h>?
 */

#undef HAVE_CRYPT_H

/*
 * Do we have the strXXX() functions?
 */

#define HAVE_STRDUP
#define HAVE_STRCASECMP
#define HAVE_STRNCASECMP

/*
 * Do we have the (v)snprintf() functions?
 */

#undef HAVE_SNPRINTF
#undef HAVE_VSNPRINTF

/*
 * What signal functions to use?
 */

#undef HAVE_SIGSET
#undef HAVE_SIGACTION

/*
 * What wait functions to use?
 */

#undef HAVE_WAITPID
#undef HAVE_WAIT3

/*
 * End of "$Id: config.h,v 1.1 1999/12/21 02:26:49 mike Exp $".
 */
