/*
 * "$Id: auth.c 7720 2008-07-11 22:46:21Z mike $"
 *
 *   Authentication functions for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007-2010 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products.
 *
 *   This file contains Kerberos support code, copyright 2006 by
 *   Jelmer Vernooij.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   cupsDoAuthentication() - Authenticate a request.
 *   cups_get_gssname()     - Get GSSAPI name for authentication.
 *   cups_gss_printf()      - Show error messages from GSSAPI...
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

#if HAVE_AUTHORIZATION_H
#  include <Security/Authorization.h>
#  ifdef HAVE_SECBASEPRIV_H
#    include <Security/SecBasePriv.h>
#  else
extern const char *cssmErrorString(int error);
#  endif /* HAVE_SECBASEPRIV_H */
#endif /* HAVE_AUTHORIZATION_H */

#if defined(SO_PEERCRED) && defined(AF_LOCAL)
#  include <pwd.h>
#endif /* SO_PEERCRED && AF_LOCAL */


/*
 * Local functions...
 */

#ifdef HAVE_GSSAPI
static gss_name_t cups_get_gssname(http_t *http, const char *service_name);
#  ifdef DEBUG
static void	cups_gss_printf(OM_uint32 major_status, OM_uint32 minor_status,
				const char *message);
#  else
#    define	cups_gss_printf(major, minor, message)
#  endif /* DEBUG */
#endif /* HAVE_GSSAPI */
static int	cups_local_auth(http_t *http);


/*
 * 'cupsDoAuthentication()' - Authenticate a request.
 *
 * This function should be called in response to a @code HTTP_UNAUTHORIZED@
 * status, prior to resubmitting your request.
 *
 * @since CUPS 1.1.20/Mac OS X 10.4@
 */

int					/* O - 0 on success, -1 on error */
cupsDoAuthentication(
    http_t     *http,			/* I - Connection to server or @code CUPS_HTTP_DEFAULT@ */
    const char *method,			/* I - Request method ("GET", "POST", "PUT") */
    const char *resource)		/* I - Resource path */
{
  const char	*password;		/* Password string */
  char		prompt[1024],		/* Prompt for user */
		realm[HTTP_MAX_VALUE],	/* realm="xyz" string */
		nonce[HTTP_MAX_VALUE];	/* nonce="xyz" string */
  int		localauth;		/* Local authentication result */
  _cups_globals_t *cg;			/* Global data */


  DEBUG_printf(("cupsDoAuthentication(http=%p, method=\"%s\", resource=\"%s\")",
                http, method, resource));

  if (!http)
    http = _cupsConnect();

  if (!http || !method || !resource)
    return (-1);

  DEBUG_printf(("2cupsDoAuthentication: digest_tries=%d, userpass=\"%s\"",
                http->digest_tries, http->userpass));
  DEBUG_printf(("2cupsDoAuthentication: WWW-Authenticate=\"%s\"",
                httpGetField(http, HTTP_FIELD_WWW_AUTHENTICATE)));

 /*
  * Clear the current authentication string...
  */

  httpSetAuthString(http, NULL, NULL);

 /*
  * See if we can do local authentication...
  */

  if (http->digest_tries < 3)
  {
    if ((localauth = cups_local_auth(http)) == 0)
    {
      DEBUG_printf(("2cupsDoAuthentication: authstring=\"%s\"",
                    http->authstring));
  
      if (http->status == HTTP_UNAUTHORIZED)
	http->digest_tries ++;
  
      return (0);
    }
    else if (localauth == -1)
    {
      http->status = HTTP_AUTHORIZATION_CANCELED;
      return (-1);			/* Error or canceled */
    }
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

    cg = _cupsGlobals();

    if (!cg->lang_default)
      cg->lang_default = cupsLangDefault();

    snprintf(prompt, sizeof(prompt),
             _cupsLangString(cg->lang_default, _("Password for %s on %s? ")),
	     cupsUser(),
	     http->hostname[0] == '/' ? "localhost" : http->hostname);

    http->digest_tries  = strncasecmp(http->fields[HTTP_FIELD_WWW_AUTHENTICATE],
                                      "Digest", 5) != 0;
    http->userpass[0]   = '\0';

    if ((password = cupsGetPassword2(prompt, http, method, resource)) == NULL)
    {
      http->status = HTTP_AUTHORIZATION_CANCELED;
      return (-1);
    }

    if (!password[0])
    {
      http->status = HTTP_AUTHORIZATION_CANCELED;
      return (-1);
    }

    snprintf(http->userpass, sizeof(http->userpass), "%s:%s", cupsUser(),
             password);
  }
  else if (http->status == HTTP_UNAUTHORIZED)
    http->digest_tries ++;

  if (http->status == HTTP_UNAUTHORIZED && http->digest_tries >= 3)
  {
    DEBUG_printf(("1cupsDoAuthentication: Too many authentication tries (%d)",
		  http->digest_tries));

    http->status = HTTP_AUTHORIZATION_CANCELED;
    return (-1);
  }

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
    char		*gss_service_name;
					/* GSS service name */
#  ifdef USE_SPNEGO
    const char		*authorization;
					/* Pointer into Authorization string */
#  endif /* USE_SPNEGO */


#  ifdef __APPLE__
   /*
    * If the weak-linked GSSAPI/Kerberos library is not present, don't try
    * to use it...
    */

    if (gss_init_sec_context == NULL)
    {
      DEBUG_puts("1cupsDoAuthentication: Weak-linked GSSAPI/Kerberos framework "
                 "is not present");
      http->status = HTTP_AUTHORIZATION_CANCELED;

      return (-1);
    }
#  endif /* __APPLE__ */

    if (http->gssname == GSS_C_NO_NAME)
    {
      if ((gss_service_name = getenv("CUPS_GSSSERVICENAME")) == NULL)
	gss_service_name = CUPS_DEFAULT_GSSSERVICENAME;
      else
	DEBUG_puts("2cupsDoAuthentication: GSS service name set via "
	           "environment variable");

      http->gssname = cups_get_gssname(http, gss_service_name);
    }

#  ifdef USE_SPNEGO /* We don't implement SPNEGO just yet... */
   /*
    * Find the start of the Kerberos input token...
    */

    authorization = httpGetField(http, HTTP_FIELD_WWW_AUTHENTICATE);

    authorization += 9;
    while (*authorization && isspace(*authorization & 255))
      authorization ++;

    if (*authorization)
    {
     /*
      * Decode the authorization string to get the input token...
      */

      int len = strlen(authorization);

      input_token.value  = malloc(len);
      input_token.value  = httpDecode64_2(input_token.value, &len,
					  authorization);
      input_token.length = len;

#    ifdef DEBUG
      {
        char *ptr = (char *)input_token.value;
	int left = len;

        fputs("input_token=", stdout);
	while (left > 0)
	{
	  if (*ptr < ' ')
	    printf("\\%03o", *ptr & 255);
	  else
	    putchar(*ptr);
	  ptr ++;
	  left --;
	}
	putchar('\n');
      }
#    endif /* DEBUG */
    }
#  endif /* USE_SPNEGO */

    if (http->gssctx != GSS_C_NO_CONTEXT)
    {
      gss_delete_sec_context(&minor_status, &http->gssctx, GSS_C_NO_BUFFER);
      http->gssctx = GSS_C_NO_CONTEXT;
    }

    major_status  = gss_init_sec_context(&minor_status, GSS_C_NO_CREDENTIAL,
					 &http->gssctx,
					 http->gssname, http->gssmech,
#ifdef GSS_C_DELEG_POLICY_FLAG
					 GSS_C_DELEG_POLICY_FLAG |
#endif /* GSS_C_DELEG_POLICY_FLAG */
					 GSS_C_MUTUAL_FLAG | GSS_C_INTEG_FLAG,
					 GSS_C_INDEFINITE,
					 GSS_C_NO_CHANNEL_BINDINGS,
					 &input_token, &http->gssmech,
					 &output_token, NULL, NULL);

    if (input_token.value)
      free(input_token.value);

    if (GSS_ERROR(major_status))
    {
      cups_gss_printf(major_status, minor_status,
		      "cupsDoAuthentication: Unable to initialize security "
		      "context");
      http->status = HTTP_AUTHORIZATION_CANCELED;

      return (-1);
    }

    if (major_status == GSS_S_CONTINUE_NEEDED)
      cups_gss_printf(major_status, minor_status,
		      "cupsDoAuthentication: Continuation needed!");

    if (output_token.length > 0 && output_token.length <= 65536)
    {
     /*
      * Allocate the authorization string since Windows KDCs can have
      * arbitrarily large credentials...
      */

      int authsize = 10 +				/* "Negotiate " */
                     output_token.length * 4 / 3 + 1 +	/* Base64 */
		     1;					/* nul */

      httpSetAuthString(http, NULL, NULL);

      if ((http->authstring = malloc(authsize)) == NULL)
      {
        http->authstring = http->_authstring;
	authsize         = sizeof(http->_authstring);
      }

      strcpy(http->authstring, "Negotiate ");
      httpEncode64_2(http->authstring + 10, authsize - 10, output_token.value,
		     output_token.length);
 
      gss_release_buffer(&minor_status, &output_token);
    }
    else
    {
      DEBUG_printf(("1cupsDoAuthentication: Kerberos credentials too large - "
                    "%d bytes!", (int)output_token.length));
      http->status = HTTP_AUTHORIZATION_CANCELED;
      gss_release_buffer(&minor_status, &output_token);

      return (-1);
    }
#endif /* HAVE_GSSAPI */
  }
  else if (strncmp(http->fields[HTTP_FIELD_WWW_AUTHENTICATE], "Digest", 6))
  {
   /*
    * Basic authentication...
    */

    char	encode[256];		/* Base64 buffer */


    httpEncode64_2(encode, sizeof(encode), http->userpass,
                   (int)strlen(http->userpass));
    httpSetAuthString(http, "Basic", encode);
  }
  else
  {
   /*
    * Digest authentication...
    */

    char	encode[33],		/* MD5 buffer */
		digest[1024];		/* Digest auth data */


    httpGetSubField(http, HTTP_FIELD_WWW_AUTHENTICATE, "realm", realm);
    httpGetSubField(http, HTTP_FIELD_WWW_AUTHENTICATE, "nonce", nonce);

    httpMD5(cupsUser(), realm, strchr(http->userpass, ':') + 1, encode);
    httpMD5Final(nonce, method, resource, encode);
    snprintf(digest, sizeof(digest),
	     "username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s\", "
	     "response=\"%s\"", cupsUser(), realm, nonce, resource, encode);
    httpSetAuthString(http, "Digest", digest);
  }

  DEBUG_printf(("1cupsDoAuthentication: authstring=\"%s\"", http->authstring));

  return (0);
}


#ifdef HAVE_GSSAPI
/*
 * 'cups_get_gssname()' - Get CUPS service credentials for authentication.
 */

static gss_name_t			/* O - Server name */
cups_get_gssname(
    http_t     *http,			/* I - Connection to server */
    const char *service_name)		/* I - Service name */
{
  gss_buffer_desc token = GSS_C_EMPTY_BUFFER;
					/* Service token */
  OM_uint32	  major_status,		/* Major status code */
		  minor_status;		/* Minor status code */
  gss_name_t	  server_name;		/* Server name */
  char		  buf[1024],		/* Name buffer */
		  fqdn[HTTP_MAX_URI];	/* Server name buffer */


  DEBUG_printf(("7cups_get_gssname(http=%p, service_name=\"%s\")", http,
                service_name));


 /*
  * Get the hostname...
  */

  httpGetHostname(http, fqdn, sizeof(fqdn));

  if (!strcmp(fqdn, "localhost"))
    httpGetHostname(NULL, fqdn, sizeof(fqdn));

 /*
  * Get a server name we can use for authentication purposes...
  */

  snprintf(buf, sizeof(buf), "%s@%s", service_name, fqdn);

  DEBUG_printf(("9cups_get_gssname: Looking up %s...", buf));

  token.value  = buf;
  token.length = strlen(buf);
  server_name  = GSS_C_NO_NAME;
  major_status = gss_import_name(&minor_status, &token,
	 			 GSS_C_NT_HOSTBASED_SERVICE,
				 &server_name);

  if (GSS_ERROR(major_status))
  {
    cups_gss_printf(major_status, minor_status,
                    "cups_get_gssname: gss_import_name() failed");
    return (NULL);
  }

  return (server_name);
}


#  ifdef DEBUG
/*
 * 'cups_gss_printf()' - Show debug error messages from GSSAPI...
 */

static void
cups_gss_printf(OM_uint32  major_status,/* I - Major status code */
		OM_uint32  minor_status,/* I - Minor status code */
		const char *message)	/* I - Prefix for error message */
{
  OM_uint32	err_major_status,	/* Major status code for display */
		err_minor_status;	/* Minor status code for display */
  OM_uint32	msg_ctx;		/* Message context */
  gss_buffer_desc major_status_string = GSS_C_EMPTY_BUFFER,
					/* Major status message */
		minor_status_string = GSS_C_EMPTY_BUFFER;
					/* Minor status message */


  msg_ctx          = 0;
  err_major_status = gss_display_status(&err_minor_status,
	                        	major_status,
					GSS_C_GSS_CODE,
					GSS_C_NO_OID,
					&msg_ctx,
					&major_status_string);

  if (!GSS_ERROR(err_major_status))
    gss_display_status(&err_minor_status, minor_status, GSS_C_MECH_CODE,
		       GSS_C_NULL_OID, &msg_ctx, &minor_status_string);

  DEBUG_printf(("1%s: %s, %s", message, (char *)major_status_string.value,
	        (char *)minor_status_string.value));

  gss_release_buffer(&err_minor_status, &major_status_string);
  gss_release_buffer(&err_minor_status, &minor_status_string);
}
#  endif /* DEBUG */
#endif /* HAVE_GSSAPI */


/*
 * 'cups_local_auth()' - Get the local authorization certificate if
 *                       available/applicable...
 */

static int				/* O - 0 if available */
					/*     1 if not available */
					/*    -1 error */
cups_local_auth(http_t *http)		/* I - HTTP connection to server */
{
#if defined(WIN32) || defined(__EMX__)
 /*
  * Currently WIN32 and OS-2 do not support the CUPS server...
  */

  return (1);
#else
  int			pid;		/* Current process ID */
  FILE			*fp;		/* Certificate file */
  char			trc[16],	/* Try Root Certificate parameter */
			filename[1024],	/* Certificate filename */
			certificate[33];/* Certificate string */
  _cups_globals_t *cg = _cupsGlobals();	/* Global data */
#  if defined(HAVE_AUTHORIZATION_H)
  OSStatus		status;		/* Status */
  AuthorizationItem	auth_right;	/* Authorization right */
  AuthorizationRights	auth_rights;	/* Authorization rights */
  AuthorizationFlags	auth_flags;	/* Authorization flags */
  AuthorizationExternalForm auth_extrn;	/* Authorization ref external */
  char			auth_key[1024];	/* Buffer */
  char			buffer[1024];	/* Buffer */
#  endif /* HAVE_AUTHORIZATION_H */


  DEBUG_printf(("7cups_local_auth(http=%p) hostaddr=%s, hostname=\"%s\"",
                http, httpAddrString(http->hostaddr, filename, sizeof(filename)), http->hostname));

 /*
  * See if we are accessing localhost...
  */

  if (!httpAddrLocalhost(http->hostaddr) &&
      strcasecmp(http->hostname, "localhost") != 0)
  {
    DEBUG_puts("8cups_local_auth: Not a local connection!");
    return (1);
  }

#  if defined(HAVE_AUTHORIZATION_H)
 /*
  * Delete any previous authorization reference...
  */
  
  if (http->auth_ref)
  {
    AuthorizationFree(http->auth_ref, kAuthorizationFlagDefaults);
    http->auth_ref = NULL;
  }

  if (!getenv("GATEWAY_INTERFACE") &&
      httpGetSubField2(http, HTTP_FIELD_WWW_AUTHENTICATE, "authkey", 
		       auth_key, sizeof(auth_key)))
  {
    status = AuthorizationCreate(NULL, kAuthorizationEmptyEnvironment, 
				 kAuthorizationFlagDefaults, &http->auth_ref);
    if (status != errAuthorizationSuccess)
    {
      DEBUG_printf(("8cups_local_auth: AuthorizationCreate() returned %d (%s)",
		    (int)status, cssmErrorString(status)));
      return (-1);
    }

    auth_right.name        = auth_key;
    auth_right.valueLength = 0;
    auth_right.value       = NULL;
    auth_right.flags       = 0;

    auth_rights.count = 1;
    auth_rights.items = &auth_right;

    auth_flags = kAuthorizationFlagDefaults | 
		 kAuthorizationFlagPreAuthorize |
		 kAuthorizationFlagInteractionAllowed | 
		 kAuthorizationFlagExtendRights;

    status = AuthorizationCopyRights(http->auth_ref, &auth_rights, 
				     kAuthorizationEmptyEnvironment, 
				     auth_flags, NULL);
    if (status == errAuthorizationSuccess)
      status = AuthorizationMakeExternalForm(http->auth_ref, &auth_extrn);

    if (status == errAuthorizationSuccess)
    {
     /*
      * Set the authorization string and return...
      */

      httpEncode64_2(buffer, sizeof(buffer), (void *)&auth_extrn, 
		     sizeof(auth_extrn));

      httpSetAuthString(http, "AuthRef", buffer);

      DEBUG_printf(("8cups_local_auth: Returning authstring=\"%s\"",
		    http->authstring));
      return (0);
    }
    else if (status == errAuthorizationCanceled)
      return (-1);

    DEBUG_printf(("9cups_local_auth: AuthorizationCopyRights() returned %d (%s)",
		  (int)status, cssmErrorString(status)));

  /*
   * Fall through to try certificates...
   */
  }
#  endif /* HAVE_AUTHORIZATION_H */

#  if defined(SO_PEERCRED) && defined(AF_LOCAL)
 /*
  * See if we can authenticate using the peer credentials provided over a
  * domain socket; if so, specify "PeerCred username" as the authentication
  * information...
  */

#    ifdef HAVE_GSSAPI
  if (strncmp(http->fields[HTTP_FIELD_WWW_AUTHENTICATE], "Negotiate", 9) &&
#    else
  if (
#    endif /* HAVE_GSSAPI */
      http->hostaddr->addr.sa_family == AF_LOCAL &&
      !getenv("GATEWAY_INTERFACE"))	/* Not via CGI programs... */
  {
   /*
    * Verify that the current cupsUser() matches the current UID...
    */

    struct passwd	*pwd;		/* Password information */
    const char		*username;	/* Current username */

    username = cupsUser();

    if ((pwd = getpwnam(username)) != NULL && pwd->pw_uid == getuid())
    {
      httpSetAuthString(http, "PeerCred", username);

      DEBUG_printf(("8cups_local_auth: Returning authstring=\"%s\"",
		    http->authstring));

      return (0);
    }
  }
#  endif /* SO_PEERCRED && AF_LOCAL */

 /*
  * Try opening a certificate file for this PID.  If that fails,
  * try the root certificate...
  */

  pid = getpid();
  snprintf(filename, sizeof(filename), "%s/certs/%d", cg->cups_statedir, pid);
  if ((fp = fopen(filename, "r")) == NULL && pid > 0)
  {
   /*
    * No certificate for this PID; see if we can get the root certificate...
    */

    DEBUG_printf(("9cups_local_auth: Unable to open file %s: %s",
                  filename, strerror(errno)));

#ifdef HAVE_GSSAPI
    if (!strncmp(http->fields[HTTP_FIELD_WWW_AUTHENTICATE], "Negotiate", 9))
    {
     /*
      * Kerberos required, don't try the root certificate...
      */

      return (1);
    }
#endif /* HAVE_GSSAPI */

    if (!httpGetSubField2(http, HTTP_FIELD_WWW_AUTHENTICATE, "trc", trc,
	                  sizeof(trc)))
    {
     /*
      * Scheduler doesn't want us to use the root certificate...
      */

      return (1);
    }

    snprintf(filename, sizeof(filename), "%s/certs/0", cg->cups_statedir);
    fp = fopen(filename, "r");
  }

  if (fp)
  {
   /*
    * Read the certificate from the file...
    */

    fgets(certificate, sizeof(certificate), fp);
    fclose(fp);

   /*
    * Set the authorization string and return...
    */

    httpSetAuthString(http, "Local", certificate);

    DEBUG_printf(("8cups_local_auth: Returning authstring=\"%s\"",
		  http->authstring));

    return (0);
  }

  return (1);
#endif /* WIN32 || __EMX__ */
}


/*
 * End of "$Id: auth.c 7720 2008-07-11 22:46:21Z mike $".
 */
