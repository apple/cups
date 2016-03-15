/*
 * "$Id: tls.c 3755 2012-03-30 05:59:14Z msweet $"
 *
 *   TLS support code for the CUPS scheduler.
 *
 *   Copyright 2012 by Apple Inc.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 */

#include "cupsd.h"

#ifdef HAVE_SSL
#  ifdef HAVE_CDSASSL
#    include "tls-darwin.c"
#  elif defined(HAVE_GNUTLS)
#    include "tls-gnutls.c"
#  elif defined(HAVE_LIBSSL)
#    include "tls-openssl.c"
#  endif /* HAVE_CDSASSL */
#endif /* HAVE_SSL */


/*
 * End of "$Id: tls.c 3755 2012-03-30 05:59:14Z msweet $".
 */
