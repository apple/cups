/*
 * "$Id$"
 *
 *   Authentication functions for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2006 by Easy Software Products.
 *
 *   This file contains Kerberos support code, copyright 2006 by
 *   Jelmer Vernooij.
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
 *       Hollywood, Maryland 20636 USA
 *
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   cupsDoAuthentication() - Authenticate a request.
 *   DEBUG_gss_printf()     - Show debug error messages from GSSAPI...
 *   cups_get_gss_creds()   - Get CUPS service credentials for authentication.
 *   cups_local_auth()      - Get the local authorization certificate if
 *                            available/applicable...
 */

/*
 * Include necessary headers...
 */

#include "globals.h"
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

#ifdef HAVE_GSSAPI
#  ifdef DEBUG
static void	DEBUG_gss_printf(OM_uint32 major_status, OM_uint32 minor_status,
				 const char *message);
#  endif /* DEBUG  */
static gss_name_t cups_get_gss_creds(http_t *http, const char *service_name);
#endif /* HAVE_GSSAPI */
static int	cups_local_auth(http_t *http);


/*
 * 'cupsDoAuthentication()' - Authenticate a request.
 *
 * This function should be called in response to a HTTP_UNAUTHORIZED
 * status, prior to resubmitting your request.
 *
 * @since CUPS 1.1.20@
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
		encode[2048];		/* Encoded username:password */


  DEBUG_printf(("cupsDoAuthentication(http=%p, method=\"%s\", resource=\"%s\")\n",
                http, method, resource));
  DEBUG_printf(("cupsDoAuthentication: digest_tries=%d, userpass=\"%s\"\n",
                http->digest_tries, http->userpass));

 /*
  * Clear the current authentication string...
  */

  http->_authstring[0] = '\0';

  if (http->authstring && http->authstring != http->_authstring)
  {
    free(http->authstring);
    http->authstring = http->_authstring;
  }

 /*
  * See if we can do local authentication...
  */

  if (http->digest_tries < 3 && !cups_local_auth(http))
  {
    DEBUG_printf(("cupsDoAuthentication: authstring=\"%s\"\n", http->authstring));

    if (http->status == HTTP_UNAUTHORIZED)
      http->digest_tries ++;

    return (0);
  }

 /*
  * Nope, see if we should retry the current username:password...
  */

  if ((http->digest_tries > 1 || !http->userpass[0]) &&
      strncmp(http->fields[HTTP_FIELD_WWW_AUTHENTICATE], "Negotiate", 9))
  {
   /*
    * Nope - get a new password from the user...
    */

    snprintf(prompt, sizeof(prompt), _("Password for %s on %s? "), cupsUser(),
             http->hostname[0] == '/' ? "localhost" : http->hostname);

    http->digest_tries  = strncasecmp(http->fields[HTTP_FIELD_WWW_AUTHENTICATE],
                                      "Digest", 5) != 0;
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

  if (!strncmp(http->fields[HTTP_FIELD_WWW_AUTHENTICATE], "Negotiate", 9))
  {
#ifdef HAVE_GSSAPI
   /*
    * Kerberos authentication...
    */

    OM_uint32		minor_status,	/* Minor status code */
			major_status;	/* Major status code */
    gss_buffer_desc	output_token = GSS_C_EMPTY_BUFFER,
					/* Output token */
			input_token = GSS_C_EMPTY_BUFFER;
					/* Input token */


    http->gssname = cups_get_gss_creds(http, "HTTP");
    major_status  = gss_init_sec_context(&minor_status, GSS_C_NO_CREDENTIAL,
					 &http->gssctx,
					 http->gssname, http->gssmech,
					 GSS_C_MUTUAL_FLAG, GSS_C_INDEFINITE,
					 GSS_C_NO_CHANNEL_BINDINGS,
					 &input_token, &http->gssmech,
					 &output_token, NULL, NULL);

    if (GSS_ERROR(major_status))
    {
#  ifdef DEBUG
      DEBUG_gss_printf(major_status, minor_status,
		       "Unable to initialise security context");
#  endif /* DEBUG */
      return (-1);
    }

#  ifdef DEBUG
    if (major_status == GSS_S_CONTINUE_NEEDED)
      DEBUG_gss_printf(major_status, minor_status, "Continuation needed!");
#  endif /* DEBUG */

    httpEncode64_2(encode, sizeof(encode), output_token.value,
                   output_token.length);

    http->authstring = malloc(strlen(encode) + 20);
    sprintf(http->authstring, "Negotiate %s", encode); /* Safe because allocated */

   /*
    * Copy back what we can to _authstring for backwards compatibility...
    */

    strlcpy(http->_authstring, http->authstring, sizeof(http->_authstring));
#endif /* HAVE_GSSAPI */
  }
  else if (strncmp(http->fields[HTTP_FIELD_WWW_AUTHENTICATE], "Digest", 6))
  {
   /*
    * Basic authentication...
    */

    httpEncode64_2(encode, sizeof(encode), http->userpass,
                   strlen(http->userpass));
    snprintf(http->_authstring, sizeof(http->_authstring), "Basic %s", encode);
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
    snprintf(http->_authstring, sizeof(http->_authstring),
	     "Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", "
	     "uri=\"%s\", response=\"%s\"", cupsUser(), realm, nonce,
	     resource, encode);
  }

  DEBUG_printf(("cupsDoAuthentication: authstring=\"%s\"\n", http->authstring));

  return (0);
}


#ifdef HAVE_GSSAPI
#  ifdef DEBUG
/*
 * 'DEBUG_gss_printf()' - Show debug error messages from GSSAPI...
 */

static void
DEBUG_gss_printf(OM_uint32 major_status,/* I - Major status code */
                 OM_uint32 minor_status,/* I - Minor status code */
		 const char *message)	/* I - Prefix for error message */
{
  OM_uint32	err_major_status,	/* Major status code for display */
		err_minor_status;	/* Minor status code for display */
  OM_uint32	msg_ctx;		/* Message context */
  gss_buffer_desc major_status_string,	/* Major status message */
		minor_status_string;	/* Minor status message */


  msg_ctx             = 0;
  major_status_string = GSS_C_EMPTY_BUFFER;
  minor_status_string = GSS_C_EMPTY_BUFFER;
  err_major_status    = gss_display_status(&err_minor_status,
	                        	   major_status,
					   GSS_C_GSS_CODE,
					   GSS_C_NO_OID,
					   &msg_ctx,
					   &major_status_string);

  if (!GSS_ERROR(err_major_status))
    err_major_status = gss_display_status(&err_minor_status,
	                        	  minor_status,
					  GSS_C_MECH_CODE,
					  GSS_C_NULL_OID,
					  &msg_ctx,
					  &minor_status_string);

  printf("%s: %s, %s\n", message, (char *)major_status_string.value,
	 (char *)minor_status_string.value);

  gss_release_buffer(&err_minor_status, &major_status_string);
  gss_release_buffer(&err_minor_status, &minor_status_string);
}
#  endif /* DEBUG */


/*
 * 'cups_get_gss_creds()' - Get CUPS service credentials for authentication.
 */

static gss_name_t			/* O - Server name */
cups_get_gss_creds(
    http_t     *http,			/* I - Connection to server */
    const char *service_name)		/* I - Service name */
{
  gss_buffer_desc token = GSS_C_EMPTY_BUFFER;
					/* Service token */
  OM_uint32	major_status,		/* Major status code */
		minor_status;		/* Minor status code */
  gss_name_t	server_name;		/* Server name */
  char		buf[1024],		/* Name buffer */
		fqdn[HTTP_MAX_URI];	/* Server name buffer */


 /*
  * Get a server name we can use for authentication purposes...
  */

  snprintf(buf, sizeof(buf), "%s@%s", service_name,
	   httpGetHostname(http, fqdn, sizeof(fqdn)));

  token.value  = buf;
  token.length = strlen(buf) + 1;
  server_name  = GSS_C_NO_NAME;
  major_status = gss_import_name(&minor_status, &token,
	 			 GSS_C_NT_HOSTBASED_SERVICE,
				 &server_name);

 /*
  * Clear the service token after we are done to avoid exposing information...
  */

  memset(&token, 0, sizeof(token));

  if (GSS_ERROR(major_status))
  {
#  ifdef DEBUG
    DEBUG_gss_printf(major_status, minor_status, "gss_import_name() failed");
#  endif /* DEBUG */

    return (NULL);
  }

  return (server_name);
}
#endif /* HAVE_GSSAPI */


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
  _cups_globals_t *cg = _cupsGlobals();	/* Global data */


  DEBUG_printf(("cups_local_auth(http=%p) hostaddr=%s, hostname=\"%s\"\n",
                http, httpAddrString(http->hostaddr, filename, sizeof(filename)), http->hostname));

 /*
  * See if we are accessing localhost...
  */

  if (!httpAddrLocalhost(http->hostaddr) &&
      strcasecmp(http->hostname, "localhost") != 0)
  {
    DEBUG_puts("cups_local_auth: Not a local connection!");
    return (-1);
  }

 /*
  * Try opening a certificate file for this PID.  If that fails,
  * try the root certificate...
  */

  pid = getpid();
  snprintf(filename, sizeof(filename), "%s/certs/%d", cg->cups_statedir, pid);
  if ((fp = fopen(filename, "r")) == NULL && pid > 0)
  {
    DEBUG_printf(("cups_local_auth: Unable to open file %s: %s\n",
                  filename, strerror(errno)));

    snprintf(filename, sizeof(filename), "%s/certs/0", cg->cups_statedir);
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

  http->authstring = malloc(strlen(certificate) + 10);
  sprintf(http->authstring, "Local %s", certificate);

  /* Copy back to _authstring for backwards compatibility */
  strlcpy(http->_authstring, http->authstring, sizeof(http->_authstring));

  DEBUG_printf(("cups_local_auth: Returning authstring = \"%s\"\n",
                http->authstring));

  return (0);
#endif /* WIN32 || __EMX__ */
}


/*
 * End of "$Id$".
 */
