/*
 * "$Id: auth.c,v 1.1.2.7 2004/06/29 01:04:30 mike Exp $"
 *
 *   Authentication functions for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2003 by Easy Software Products.
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
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   cupsDoAuthentication() - Authenticate a request...
 *   cups_local_auth()      - Get the local authorization certificate if
 *                            available/applicable...
 */

/*
 * Include necessary headers...
 */

#include "cups.h"
#include "ipp.h"
#include "language.h"
#include "string.h"
#include "debug.h"
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#if defined(WIN32) || defined(__EMX__)
#  include <io.h>
#else
#  include <unistd.h>
#endif /* WIN32 || __EMX__ */


/*
 * Local functions...
 */

static int	cups_local_auth(http_t *http);


/*
 * 'cupsDoAuthentication()' - Authenticate a request...
 */

int					/* O - 0 on success, -1 on error */
cupsDoAuthentication(http_t     *http,	/* I - HTTP connection to server */
                     const char *method,/* I - Request method (GET, POST, PUT) */
		     const char *resource)
					/* I - Resource path */
{
  const char	*password;		/* Password string */
  char		prompt[1024],		/* Prompt for user */
		realm[HTTP_MAX_VALUE],	/* realm="xyz" string */
		nonce[HTTP_MAX_VALUE],	/* nonce="xyz" string */
		encode[512];		/* Encoded username:password */


  DEBUG_printf(("cupsDoAuthentication(http=%p, method=\"%s\", resource=\"%s\")\n",
                http, method, resource));
  DEBUG_printf(("cupsDoAuthentication: digest_tries=%d, userpass=\"%s\"\n",
                http->digest_tries, http->userpass));

 /*
  * Clear the current authentication string...
  */

  http->authstring[0] = '\0';

 /*
  * See if we can do local authentication...
  */

  if (!cups_local_auth(http))
  {
    DEBUG_printf(("cupsDoAuthentication: authstring=\"%s\"\n", http->authstring));
    return (0);
  }

 /*
  * Nope, see if we should retry the current username:password...
  */

  if (http->digest_tries > 1 || !http->userpass[0])
  {
   /*
    * Nope - get a new password from the user...
    */

    snprintf(prompt, sizeof(prompt), "Password for %s on %s? ", cupsUser(),
             http->hostname);

    http->digest_tries  = strncasecmp(http->fields[HTTP_FIELD_WWW_AUTHENTICATE],
                                      "Basic", 5) == 0;
    http->userpass[0]   = '\0';

    if ((password = cupsGetPassword(prompt)) == NULL)
      return (-1);

    if (!password[0])
      return (-1);

    snprintf(http->userpass, sizeof(http->userpass), "%s:%s", cupsUser(),
             password);
  }
  else if (http->status == HTTP_UNAUTHORIZED)
    http->digest_tries ++;

 /*
  * Got a password; encode it for the server...
  */

  if (strncmp(http->fields[HTTP_FIELD_WWW_AUTHENTICATE], "Basic", 5) == 0)
  {
   /*
    * Basic authentication...
    */

    httpEncode64(encode, http->userpass);
    snprintf(http->authstring, sizeof(http->authstring), "Basic %s", encode);
  }
  else
  {
   /*
    * Digest authentication...
    */

    httpGetSubField(http, HTTP_FIELD_WWW_AUTHENTICATE, "realm", realm);
    httpGetSubField(http, HTTP_FIELD_WWW_AUTHENTICATE, "nonce", nonce);

    httpMD5(cupsUser(), realm, strchr(http->userpass, ':') + 1, encode);
    httpMD5Final(nonce, method, resource, encode);
    snprintf(http->authstring, sizeof(http->authstring),
	     "Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", "
	     "uri=\"%s\", response=\"%s\"", cupsUser(), realm, nonce,
	     resource, encode);
  }

  DEBUG_printf(("cupsDoAuthentication: authstring=\"%s\"\n", http->authstring));

  return (0);
}


/*
 * 'cups_local_auth()' - Get the local authorization certificate if
 *                       available/applicable...
 */

static int				/* O - 0 if available, -1 if not */
cups_local_auth(http_t *http)		/* I - HTTP connection to server */
{
#if defined(WIN32) || defined(__EMX__)
 /*
  * Currently WIN32 and OS-2 do not support the CUPS server...
  */

  return (-1);
#else
  int		pid;			/* Current process ID */
  FILE		*fp;			/* Certificate file */
  char		filename[1024],		/* Certificate filename */
		certificate[33];	/* Certificate string */
  const char	*root;			/* Server root directory */


  DEBUG_printf(("cups_local_auth(http=%p) hostaddr=%08x, hostname=\"%s\"\n",
                http, ntohl(http->hostaddr.sin_addr.s_addr), http->hostname));

 /*
  * See if we are accessing localhost...
  */

  if (!httpAddrLocalhost(&http->hostaddr) &&
      strcasecmp(http->hostname, "localhost") != 0)
  {
    DEBUG_puts("cups_local_auth: Not a local connection!");
    return (-1);
  }

 /*
  * Try opening a certificate file for this PID.  If that fails,
  * try the root certificate...
  */

  if ((root = getenv("CUPS_SERVERROOT")) == NULL)
    root = CUPS_SERVERROOT;

  pid = getpid();
  snprintf(filename, sizeof(filename), "%s/certs/%d", root, pid);
  if ((fp = fopen(filename, "r")) == NULL && pid > 0)
  {
    DEBUG_printf(("cups_local_auth: Unable to open file %s: %s\n",
                  filename, strerror(errno)));

    snprintf(filename, sizeof(filename), "%s/certs/0", root);
    fp = fopen(filename, "r");
  }

  if (fp == NULL)
  {
    DEBUG_printf(("cups_local_auth: Unable to open file %s: %s\n",
                  filename, strerror(errno)));
    return (-1);
  }

 /*
  * Read the certificate from the file...
  */

  fgets(certificate, sizeof(certificate), fp);
  fclose(fp);

 /*
  * Set the authorization string and return...
  */

  snprintf(http->authstring, sizeof(http->authstring), "Local %s", certificate);

  DEBUG_printf(("cups_local_auth: Returning authstring = \"%s\"\n",
                http->authstring));

  return (0);
#endif /* WIN32 || __EMX__ */
}


/*
 * End of "$Id: auth.c,v 1.1.2.7 2004/06/29 01:04:30 mike Exp $".
 */
