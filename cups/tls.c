/*
 * "$Id: tls.c 12136 2014-08-29 15:19:40Z msweet $"
 *
 * TLS routines for CUPS.
 *
 * Copyright 2007-2014 by Apple Inc.
 * Copyright 1997-2007 by Easy Software Products, all rights reserved.
 *
 * This file contains Kerberos support code, copyright 2006 by
 * Jelmer Vernooij.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * This file is subject to the Apple OS-Developed Software exception.
 */

/*
 * Include necessary headers...
 */

#include "cups-private.h"
#include <fcntl.h>
#include <math.h>
#ifdef WIN32
#  include <tchar.h>
#else
#  include <signal.h>
#  include <sys/time.h>
#  include <sys/resource.h>
#endif /* WIN32 */
#ifdef HAVE_POLL
#  include <poll.h>
#endif /* HAVE_POLL */


/*
 * Local functions...
 */

#ifdef HAVE_SSL
#  ifdef HAVE_GNUTLS
#    include "tls-gnutls.c"
#  elif defined(HAVE_CDSASSL)
#    include "tls-darwin.c"
#  elif defined(HAVE_SSPISSL)
#    include "tls-sspi.c"
#  endif /* HAVE_GNUTLS */
#else
/* Stubs for when TLS is not supported/available */
int
httpCopyCredentials(http_t *http, cups_array_t **credentials)
{
  (void)http;
  if (credentials)
    *credentials = NULL;
  return (-1);
}
int
httpCredentialsAreValidForName(cups_array_t *credentials, const char *common_name)
{
  (void)credentials;
  (void)common_name;
  return (1);
}
time_t
httpCredentialsGetExpiration(cups_array_t *credentials)
{
  (void)credentials;
  return (INT_MAX);
}
http_trust_t
httpCredentialsGetTrust(cups_array_t *credentials, const char *common_name)
{
  (void)credentials;
  (void)common_name;
  return (HTTP_TRUST_OK);
}
size_t
httpCredentialsString(cups_array_t *credentials, char *buffer, size_t bufsize)
{
  (void)credentials;
  (void)bufsize;
  if (buffer)
    *buffer = '\0';
  return (0);
}
int
httpLoadCredentials(const char *path, cups_array_t **credentials, const char *common_name)
{
  (void)path;
  (void)credentials;
  (void)common_name;
  return (-1);
}
int
httpSaveCredentials(const char *path, cups_array_t *credentials, const char *common_name)
{
  (void)path;
  (void)credentials;
  (void)common_name;
  return (-1);
}
#endif /* HAVE_SSL */


/*
 * End of "$Id: tls.c 12136 2014-08-29 15:19:40Z msweet $".
 */
