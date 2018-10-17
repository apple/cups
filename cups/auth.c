/*
 * Authentication functions for CUPS.
 *
 * Copyright © 2007-2018 by Apple Inc.
 * Copyright © 1997-2007 by Easy Software Products.
 *
 * This file contains Kerberos support code, copyright 2006 by
 * Jelmer Vernooij.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * missing or damaged, see the license at "http://www.cups.org/".
 *
 * This file is subject to the Apple OS-Developed Software exception.
 */

/*
 * Include necessary headers...
 */

#include "cups-private.h"
#include <fcntl.h>
#include <sys/stat.h>
#if defined(_WIN32) || defined(__EMX__)
#  include <io.h>
#else
#  include <unistd.h>
#endif /* _WIN32 || __EMX__ */

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

static const char	*cups_auth_find(const char *www_authenticate, const char *scheme);
static const char	*cups_auth_param(const char *scheme, const char *name, char *value, size_t valsize);
static const char	*cups_auth_scheme(const char *www_authenticate, char *scheme, size_t schemesize);

#ifdef HAVE_GSSAPI
#  ifdef HAVE_GSS_ACQUIRE_CRED_EX_F
#    ifdef HAVE_GSS_GSSAPI_SPI_H
#      include <GSS/gssapi_spi.h>
#    else
#      define GSS_AUTH_IDENTITY_TYPE_1 1
#      define gss_acquire_cred_ex_f __ApplePrivate_gss_acquire_cred_ex_f
typedef struct gss_auth_identity /* @private@ */
{
  uint32_t type;
  uint32_t flags;
  char *username;
  char *realm;
  char *password;
  gss_buffer_t *credentialsRef;
} gss_auth_identity_desc;
extern OM_uint32 gss_acquire_cred_ex_f(gss_status_id_t, const gss_name_t,
				       OM_uint32, OM_uint32, const gss_OID,
				       gss_cred_usage_t, gss_auth_identity_t,
				       void *, void (*)(void *, OM_uint32,
				                        gss_status_id_t,
							gss_cred_id_t,
							gss_OID_set,
							OM_uint32));
#    endif /* HAVE_GSS_GSSAPI_SPI_H */
#    include <dispatch/dispatch.h>
typedef struct _cups_gss_acquire_s	/* Acquire callback data */
{
  dispatch_semaphore_t	sem;		/* Synchronization semaphore */
  OM_uint32		major;		/* Returned status code */
  gss_cred_id_t		creds;		/* Returned credentials */
} _cups_gss_acquire_t;

static void	cups_gss_acquire(void *ctx, OM_uint32 major,
		                 gss_status_id_t status,
				 gss_cred_id_t creds, gss_OID_set oids,
				 OM_uint32 time_rec);
#  endif /* HAVE_GSS_ACQUIRE_CRED_EX_F */
static gss_name_t cups_gss_getname(http_t *http, const char *service_name);
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
 * This function should be called in response to a @code HTTP_STATUS_UNAUTHORIZED@
 * status, prior to resubmitting your request.
 *
 * @since CUPS 1.1.20/macOS 10.4@
 */

int					/* O - 0 on success, -1 on error */
cupsDoAuthentication(
    http_t     *http,			/* I - Connection to server or @code CUPS_HTTP_DEFAULT@ */
    const char *method,			/* I - Request method ("GET", "POST", "PUT") */
    const char *resource)		/* I - Resource path */
{
  const char	*password,		/* Password string */
		*www_auth,		/* WWW-Authenticate header */
		*schemedata;		/* Scheme-specific data */
  char		scheme[256],		/* Scheme name */
		prompt[1024];		/* Prompt for user */
  int		localauth;		/* Local authentication result */
  _cups_globals_t *cg;			/* Global data */


  DEBUG_printf(("cupsDoAuthentication(http=%p, method=\"%s\", resource=\"%s\")", (void *)http, method, resource));

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

      if (http->status == HTTP_STATUS_UNAUTHORIZED)
	http->digest_tries ++;

      return (0);
    }
    else if (localauth == -1)
    {
      http->status = HTTP_STATUS_CUPS_AUTHORIZATION_CANCELED;
      return (-1);			/* Error or canceled */
    }
  }

 /*
  * Nope, loop through the authentication schemes to find the first we support.
  */

  www_auth = httpGetField(http, HTTP_FIELD_WWW_AUTHENTICATE);

  for (schemedata = cups_auth_scheme(www_auth, scheme, sizeof(scheme)); schemedata; schemedata = cups_auth_scheme(schemedata + strlen(scheme), scheme, sizeof(scheme)))
  {
   /*
    * Check the scheme name...
    */

#ifdef HAVE_GSSAPI
    if (!_cups_strcasecmp(scheme, "Negotiate"))
    {
     /*
      * Kerberos authentication...
      */

      if (_cupsSetNegotiateAuthString(http, method, resource))
      {
	http->status = HTTP_STATUS_CUPS_AUTHORIZATION_CANCELED;
	return (-1);
      }

      break;
    }
    else
#endif /* HAVE_GSSAPI */
    if (_cups_strcasecmp(scheme, "Basic") && _cups_strcasecmp(scheme, "Digest"))
      continue;				/* Not supported (yet) */

   /*
    * See if we should retry the current username:password...
    */

    if ((http->digest_tries > 1 || !http->userpass[0]) && (!_cups_strcasecmp(scheme, "Basic") || (!_cups_strcasecmp(scheme, "Digest"))))
    {
     /*
      * Nope - get a new password from the user...
      */

      char default_username[HTTP_MAX_VALUE];
					/* Default username */

      cg = _cupsGlobals();

      if (!cg->lang_default)
	cg->lang_default = cupsLangDefault();

      if (cups_auth_param(schemedata, "username", default_username, sizeof(default_username)))
	cupsSetUser(default_username);

      snprintf(prompt, sizeof(prompt), _cupsLangString(cg->lang_default, _("Password for %s on %s? ")), cupsUser(), http->hostname[0] == '/' ? "localhost" : http->hostname);

      http->digest_tries  = _cups_strncasecmp(scheme, "Digest", 6) != 0;
      http->userpass[0]   = '\0';

      if ((password = cupsGetPassword2(prompt, http, method, resource)) == NULL)
      {
	http->status = HTTP_STATUS_CUPS_AUTHORIZATION_CANCELED;
	return (-1);
      }

      snprintf(http->userpass, sizeof(http->userpass), "%s:%s", cupsUser(), password);
    }
    else if (http->status == HTTP_STATUS_UNAUTHORIZED)
      http->digest_tries ++;

    if (http->status == HTTP_STATUS_UNAUTHORIZED && http->digest_tries >= 3)
    {
      DEBUG_printf(("1cupsDoAuthentication: Too many authentication tries (%d)", http->digest_tries));

      http->status = HTTP_STATUS_CUPS_AUTHORIZATION_CANCELED;
      return (-1);
    }

   /*
    * Got a password; encode it for the server...
    */

    if (!_cups_strcasecmp(scheme, "Basic"))
    {
     /*
      * Basic authentication...
      */

      char	encode[256];		/* Base64 buffer */

      httpEncode64_2(encode, sizeof(encode), http->userpass, (int)strlen(http->userpass));
      httpSetAuthString(http, "Basic", encode);
      break;
    }
    else if (!_cups_strcasecmp(scheme, "Digest"))
    {
     /*
      * Digest authentication...
      */

      char nonce[HTTP_MAX_VALUE];	/* nonce="xyz" string */

      cups_auth_param(schemedata, "algorithm", http->algorithm, sizeof(http->algorithm));
      cups_auth_param(schemedata, "opaque", http->opaque, sizeof(http->opaque));
      cups_auth_param(schemedata, "nonce", nonce, sizeof(nonce));
      cups_auth_param(schemedata, "realm", http->realm, sizeof(http->realm));

      if (_httpSetDigestAuthString(http, nonce, method, resource))
        break;
    }
  }

  if (http->authstring)
  {
    DEBUG_printf(("1cupsDoAuthentication: authstring=\"%s\"", http->authstring));

    return (0);
  }
  else
  {
    DEBUG_printf(("1cupsDoAuthentication: Unknown auth type: \"%s\"", www_auth));
    http->status = HTTP_STATUS_CUPS_AUTHORIZATION_CANCELED;

    return (-1);
  }
}


#ifdef HAVE_GSSAPI
/*
 * '_cupsSetNegotiateAuthString()' - Set the Kerberos authentication string.
 */

int					/* O - 0 on success, -1 on error */
_cupsSetNegotiateAuthString(
    http_t     *http,			/* I - Connection to server */
    const char *method,			/* I - Request method ("GET", "POST", "PUT") */
    const char *resource)		/* I - Resource path */
{
  OM_uint32	minor_status,		/* Minor status code */
		major_status;		/* Major status code */
  gss_buffer_desc output_token = GSS_C_EMPTY_BUFFER;
					/* Output token */


  (void)method;
  (void)resource;

#  ifdef __APPLE__
 /*
  * If the weak-linked GSSAPI/Kerberos library is not present, don't try
  * to use it...
  */

  if (&gss_init_sec_context == NULL)
  {
    DEBUG_puts("1_cupsSetNegotiateAuthString: Weak-linked GSSAPI/Kerberos "
               "framework is not present");
    return (-1);
  }
#  endif /* __APPLE__ */

  if (http->gssname == GSS_C_NO_NAME)
  {
    http->gssname = cups_gss_getname(http, _cupsGSSServiceName());
  }

  if (http->gssctx != GSS_C_NO_CONTEXT)
  {
    gss_delete_sec_context(&minor_status, &http->gssctx, GSS_C_NO_BUFFER);
    http->gssctx = GSS_C_NO_CONTEXT;
  }

  major_status = gss_init_sec_context(&minor_status, GSS_C_NO_CREDENTIAL,
				      &http->gssctx,
				      http->gssname, http->gssmech,
				      GSS_C_MUTUAL_FLAG | GSS_C_INTEG_FLAG,
				      GSS_C_INDEFINITE,
				      GSS_C_NO_CHANNEL_BINDINGS,
				      GSS_C_NO_BUFFER, &http->gssmech,
				      &output_token, NULL, NULL);

#  ifdef HAVE_GSS_ACQUIRE_CRED_EX_F
  if (major_status == GSS_S_NO_CRED)
  {
   /*
    * Ask the user for credentials...
    */

    char		prompt[1024],	/* Prompt for user */
			userbuf[256];	/* Kerberos username */
    const char		*username,	/* Username string */
			*password;	/* Password string */
    _cups_gss_acquire_t	data;		/* Callback data */
    gss_auth_identity_desc identity;	/* Kerberos user identity */
    _cups_globals_t	*cg = _cupsGlobals();
					/* Per-thread global data */

    if (!cg->lang_default)
      cg->lang_default = cupsLangDefault();

    snprintf(prompt, sizeof(prompt),
             _cupsLangString(cg->lang_default, _("Password for %s on %s? ")),
	     cupsUser(), http->gsshost);

    if ((password = cupsGetPassword2(prompt, http, method, resource)) == NULL)
      return (-1);

   /*
    * Try to acquire credentials...
    */

    username = cupsUser();
    if (!strchr(username, '@'))
    {
      snprintf(userbuf, sizeof(userbuf), "%s@%s", username, http->gsshost);
      username = userbuf;
    }

    identity.type           = GSS_AUTH_IDENTITY_TYPE_1;
    identity.flags          = 0;
    identity.username       = (char *)username;
    identity.realm          = (char *)"";
    identity.password       = (char *)password;
    identity.credentialsRef = NULL;

    data.sem   = dispatch_semaphore_create(0);
    data.major = 0;
    data.creds = NULL;

    if (data.sem)
    {
      major_status = gss_acquire_cred_ex_f(NULL, GSS_C_NO_NAME, 0, GSS_C_INDEFINITE, GSS_KRB5_MECHANISM, GSS_C_INITIATE, (gss_auth_identity_t)&identity, &data, cups_gss_acquire);

      if (major_status == GSS_S_COMPLETE)
      {
	dispatch_semaphore_wait(data.sem, DISPATCH_TIME_FOREVER);
	major_status = data.major;
      }

      dispatch_release(data.sem);

      if (major_status == GSS_S_COMPLETE)
      {
        OM_uint32	release_minor;	/* Minor status from releasing creds */

	major_status = gss_init_sec_context(&minor_status, data.creds,
					    &http->gssctx,
					    http->gssname, http->gssmech,
					    GSS_C_MUTUAL_FLAG | GSS_C_INTEG_FLAG,
					    GSS_C_INDEFINITE,
					    GSS_C_NO_CHANNEL_BINDINGS,
					    GSS_C_NO_BUFFER, &http->gssmech,
					    &output_token, NULL, NULL);
        gss_release_cred(&release_minor, &data.creds);
      }
    }
  }
#  endif /* HAVE_GSS_ACQUIRED_CRED_EX_F */

  if (GSS_ERROR(major_status))
  {
    cups_gss_printf(major_status, minor_status,
		    "_cupsSetNegotiateAuthString: Unable to initialize "
		    "security context");
    return (-1);
  }

#  ifdef DEBUG
  else if (major_status == GSS_S_CONTINUE_NEEDED)
    cups_gss_printf(major_status, minor_status,
		    "_cupsSetNegotiateAuthString: Continuation needed!");
#  endif /* DEBUG */

  if (output_token.length > 0 && output_token.length <= 65536)
  {
   /*
    * Allocate the authorization string since Windows KDCs can have
    * arbitrarily large credentials...
    */

    int authsize = 10 +			/* "Negotiate " */
		   (int)output_token.length * 4 / 3 + 1 + 1;
		   			/* Base64 + nul */

    httpSetAuthString(http, NULL, NULL);

    if ((http->authstring = malloc((size_t)authsize)) == NULL)
    {
      http->authstring = http->_authstring;
      authsize         = sizeof(http->_authstring);
    }

    strlcpy(http->authstring, "Negotiate ", (size_t)authsize);
    httpEncode64_2(http->authstring + 10, authsize - 10, output_token.value,
		   (int)output_token.length);

    gss_release_buffer(&minor_status, &output_token);
  }
  else
  {
    DEBUG_printf(("1_cupsSetNegotiateAuthString: Kerberos credentials too "
                  "large - %d bytes!", (int)output_token.length));
    gss_release_buffer(&minor_status, &output_token);

    return (-1);
  }

  return (0);
}
#endif /* HAVE_GSSAPI */


/*
 * 'cups_auth_find()' - Find the named WWW-Authenticate scheme.
 *
 * The "www_authenticate" parameter points to the current position in the header.
 *
 * Returns @code NULL@ if the auth scheme is not present.
 */

static const char *				/* O - Start of matching scheme or @code NULL@ if not found */
cups_auth_find(const char *www_authenticate,	/* I - Pointer into WWW-Authenticate header */
               const char *scheme)		/* I - Authentication scheme */
{
  size_t	schemelen = strlen(scheme);	/* Length of scheme */


  DEBUG_printf(("8cups_auth_find(www_authenticate=\"%s\", scheme=\"%s\"(%d))", www_authenticate, scheme, (int)schemelen));

  while (*www_authenticate)
  {
   /*
    * Skip leading whitespace and commas...
    */

    DEBUG_printf(("9cups_auth_find: Before whitespace: \"%s\"", www_authenticate));
    while (isspace(*www_authenticate & 255) || *www_authenticate == ',')
      www_authenticate ++;
    DEBUG_printf(("9cups_auth_find: After whitespace: \"%s\"", www_authenticate));

   /*
    * See if this is "Scheme" followed by whitespace or the end of the string.
    */

    if (!strncmp(www_authenticate, scheme, schemelen) && (isspace(www_authenticate[schemelen] & 255) || www_authenticate[schemelen] == ',' || !www_authenticate[schemelen]))
    {
     /*
      * Yes, this is the start of the scheme-specific information...
      */

      DEBUG_printf(("9cups_auth_find: Returning \"%s\".", www_authenticate));

      return (www_authenticate);
    }

   /*
    * Skip the scheme name or param="value" string...
    */

    while (!isspace(*www_authenticate & 255) && *www_authenticate)
    {
      if (*www_authenticate == '\"')
      {
       /*
        * Skip quoted value...
        */

        www_authenticate ++;
        while (*www_authenticate && *www_authenticate != '\"')
          www_authenticate ++;

        DEBUG_printf(("9cups_auth_find: After quoted: \"%s\"", www_authenticate));
      }

      www_authenticate ++;
    }

    DEBUG_printf(("9cups_auth_find: After skip: \"%s\"", www_authenticate));
  }

  DEBUG_puts("9cups_auth_find: Returning NULL.");

  return (NULL);
}


/*
 * 'cups_auth_param()' - Copy the value for the named authentication parameter,
 *                       if present.
 */

static const char *				/* O - Parameter value or @code NULL@ if not present */
cups_auth_param(const char *scheme,		/* I - Pointer to auth data */
                const char *name,		/* I - Name of parameter */
                char       *value,		/* I - Value buffer */
                size_t     valsize)		/* I - Size of value buffer */
{
  char		*valptr = value,		/* Pointer into value buffer */
		*valend = value + valsize - 1;	/* Pointer to end of buffer */
  size_t	namelen = strlen(name);		/* Name length */
  int		param;				/* Is this a parameter? */


  DEBUG_printf(("8cups_auth_param(scheme=\"%s\", name=\"%s\", value=%p, valsize=%d)", scheme, name, (void *)value, (int)valsize));

  while (!isspace(*scheme & 255) && *scheme)
    scheme ++;

  while (*scheme)
  {
    while (isspace(*scheme & 255) || *scheme == ',')
      scheme ++;

    if (!strncmp(scheme, name, namelen) && scheme[namelen] == '=')
    {
     /*
      * Found the parameter, copy the value...
      */

      scheme += namelen + 1;
      if (*scheme == '\"')
      {
        scheme ++;

	while (*scheme && *scheme != '\"')
	{
	  if (valptr < valend)
	    *valptr++ = *scheme;

	  scheme ++;
	}
      }
      else
      {
	while (*scheme && strchr("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~+/=", *scheme))
	{
	  if (valptr < valend)
	    *valptr++ = *scheme;

	  scheme ++;
	}
      }

      *valptr = '\0';

      DEBUG_printf(("9cups_auth_param: Returning \"%s\".", value));

      return (value);
    }

   /*
    * Skip the param=value string...
    */

    param = 0;

    while (!isspace(*scheme & 255) && *scheme)
    {
      if (*scheme == '=')
        param = 1;
      else if (*scheme == '\"')
      {
       /*
        * Skip quoted value...
        */

        scheme ++;
        while (*scheme && *scheme != '\"')
          scheme ++;
      }

      scheme ++;
    }

   /*
    * If this wasn't a parameter, we are at the end of this scheme's
    * parameters...
    */

    if (!param)
      break;
  }

  *value = '\0';

  DEBUG_puts("9cups_auth_param: Returning NULL.");

  return (NULL);
}


/*
 * 'cups_auth_scheme()' - Get the (next) WWW-Authenticate scheme.
 *
 * The "www_authenticate" parameter points to the current position in the header.
 *
 * Returns @code NULL@ if there are no (more) auth schemes present.
 */

static const char *				/* O - Start of scheme or @code NULL@ if not found */
cups_auth_scheme(const char *www_authenticate,	/* I - Pointer into WWW-Authenticate header */
		 char       *scheme,		/* I - Scheme name buffer */
		 size_t     schemesize)		/* I - Size of buffer */
{
  const char	*start;				/* Start of scheme data */
  char		*sptr = scheme,			/* Pointer into scheme buffer */
		*send = scheme + schemesize - 1;/* End of scheme buffer */
  int		param;				/* Is this a parameter? */


  DEBUG_printf(("8cups_auth_scheme(www_authenticate=\"%s\", scheme=%p, schemesize=%d)", www_authenticate, (void *)scheme, (int)schemesize));

  while (*www_authenticate)
  {
   /*
    * Skip leading whitespace and commas...
    */

    while (isspace(*www_authenticate & 255) || *www_authenticate == ',')
      www_authenticate ++;

   /*
    * Parse the scheme name or param="value" string...
    */

    for (sptr = scheme, start = www_authenticate, param = 0; *www_authenticate && *www_authenticate != ',' && !isspace(*www_authenticate & 255); www_authenticate ++)
    {
      if (*www_authenticate == '=')
        param = 1;
      else if (!param && sptr < send)
        *sptr++ = *www_authenticate;
      else if (*www_authenticate == '\"')
      {
       /*
        * Skip quoted value...
        */

        www_authenticate ++;
        while (*www_authenticate && *www_authenticate != '\"')
          www_authenticate ++;
      }
    }

    if (sptr > scheme && !param)
    {
      *sptr = '\0';

      DEBUG_printf(("9cups_auth_scheme: Returning \"%s\".", start));

      return (start);
    }
  }

  *scheme = '\0';

  DEBUG_puts("9cups_auth_scheme: Returning NULL.");

  return (NULL);
}


#ifdef HAVE_GSSAPI
#  ifdef HAVE_GSS_ACQUIRE_CRED_EX_F
/*
 * 'cups_gss_acquire()' - Kerberos credentials callback.
 */
static void
cups_gss_acquire(
    void            *ctx,		/* I - Caller context */
    OM_uint32       major,		/* I - Major error code */
    gss_status_id_t status,		/* I - Status (unused) */
    gss_cred_id_t   creds,		/* I - Credentials (if any) */
    gss_OID_set     oids,		/* I - Mechanism OIDs (unused) */
    OM_uint32       time_rec)		/* I - Timestamp (unused) */
{
  uint32_t		min;		/* Minor error code */
  _cups_gss_acquire_t	*data;		/* Callback data */


  (void)status;
  (void)time_rec;

  data        = (_cups_gss_acquire_t *)ctx;
  data->major = major;
  data->creds = creds;

  gss_release_oid_set(&min, &oids);
  dispatch_semaphore_signal(data->sem);
}
#  endif /* HAVE_GSS_ACQUIRE_CRED_EX_F */


/*
 * 'cups_gss_getname()' - Get CUPS service credentials for authentication.
 */

static gss_name_t			/* O - Server name */
cups_gss_getname(
    http_t     *http,			/* I - Connection to server */
    const char *service_name)		/* I - Service name */
{
  gss_buffer_desc token = GSS_C_EMPTY_BUFFER;
					/* Service token */
  OM_uint32	  major_status,		/* Major status code */
		  minor_status;		/* Minor status code */
  gss_name_t	  server_name;		/* Server name */
  char		  buf[1024];		/* Name buffer */


  DEBUG_printf(("7cups_gss_getname(http=%p, service_name=\"%s\")", http,
                service_name));


 /*
  * Get the hostname...
  */

  if (!http->gsshost[0])
  {
    httpGetHostname(http, http->gsshost, sizeof(http->gsshost));

    if (!strcmp(http->gsshost, "localhost"))
    {
      if (gethostname(http->gsshost, sizeof(http->gsshost)) < 0)
      {
	DEBUG_printf(("1cups_gss_getname: gethostname() failed: %s",
		      strerror(errno)));
	http->gsshost[0] = '\0';
	return (NULL);
      }

      if (!strchr(http->gsshost, '.'))
      {
       /*
	* The hostname is not a FQDN, so look it up...
	*/

	struct hostent	*host;		/* Host entry to get FQDN */

	if ((host = gethostbyname(http->gsshost)) != NULL && host->h_name)
	{
	 /*
	  * Use the resolved hostname...
	  */

	  strlcpy(http->gsshost, host->h_name, sizeof(http->gsshost));
	}
	else
	{
	  DEBUG_printf(("1cups_gss_getname: gethostbyname(\"%s\") failed.",
			http->gsshost));
	  http->gsshost[0] = '\0';
	  return (NULL);
	}
      }
    }
  }

 /*
  * Get a service name we can use for authentication purposes...
  */

  snprintf(buf, sizeof(buf), "%s@%s", service_name, http->gsshost);

  DEBUG_printf(("8cups_gss_getname: Looking up \"%s\".", buf));

  token.value  = buf;
  token.length = strlen(buf);
  server_name  = GSS_C_NO_NAME;
  major_status = gss_import_name(&minor_status, &token,
	 			 GSS_C_NT_HOSTBASED_SERVICE,
				 &server_name);

  if (GSS_ERROR(major_status))
  {
    cups_gss_printf(major_status, minor_status,
                    "cups_gss_getname: gss_import_name() failed");
    return (NULL);
  }

  return (server_name);
}


#  ifdef DEBUG
/*
 * 'cups_gss_printf()' - Show debug error messages from GSSAPI.
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
 *                       available/applicable.
 */

static int				/* O - 0 if available */
					/*     1 if not available */
					/*    -1 error */
cups_local_auth(http_t *http)		/* I - HTTP connection to server */
{
#if defined(_WIN32) || defined(__EMX__)
 /*
  * Currently _WIN32 and OS-2 do not support the CUPS server...
  */

  return (1);
#else
  int			pid;		/* Current process ID */
  FILE			*fp;		/* Certificate file */
  char			trc[16],	/* Try Root Certificate parameter */
			filename[1024];	/* Certificate filename */
  const char		*www_auth,	/* WWW-Authenticate header */
			*schemedata;	/* Data for the named auth scheme */
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


  DEBUG_printf(("7cups_local_auth(http=%p) hostaddr=%s, hostname=\"%s\"", (void *)http, httpAddrString(http->hostaddr, filename, sizeof(filename)), http->hostname));

 /*
  * See if we are accessing localhost...
  */

  if (!httpAddrLocalhost(http->hostaddr) && _cups_strcasecmp(http->hostname, "localhost") != 0)
  {
    DEBUG_puts("8cups_local_auth: Not a local connection!");
    return (1);
  }

  www_auth = httpGetField(http, HTTP_FIELD_WWW_AUTHENTICATE);

#  if defined(HAVE_AUTHORIZATION_H)
 /*
  * Delete any previous authorization reference...
  */

  if (http->auth_ref)
  {
    AuthorizationFree(http->auth_ref, kAuthorizationFlagDefaults);
    http->auth_ref = NULL;
  }

  if (!getenv("GATEWAY_INTERFACE") && (schemedata = cups_auth_find(www_auth, "AuthRef")) != NULL && cups_auth_param(schemedata, "key", auth_key, sizeof(auth_key)))
  {
    status = AuthorizationCreate(NULL, kAuthorizationEmptyEnvironment, kAuthorizationFlagDefaults, &http->auth_ref);
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

#  ifdef HAVE_GSSAPI
  if (cups_auth_find(www_auth, "Negotiate"))
    return (1);
#  endif /* HAVE_GSSAPI */

#  if defined(SO_PEERCRED) && defined(AF_LOCAL)
 /*
  * See if we can authenticate using the peer credentials provided over a
  * domain socket; if so, specify "PeerCred username" as the authentication
  * information...
  */

  if (http->hostaddr->addr.sa_family == AF_LOCAL &&
      !getenv("GATEWAY_INTERFACE") &&	/* Not via CGI programs... */
      cups_auth_find(www_auth, "PeerCred"))
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

  if ((schemedata = cups_auth_find(www_auth, "Local")) == NULL)
    return (1);

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

    DEBUG_printf(("9cups_local_auth: Unable to open file \"%s\": %s", filename, strerror(errno)));

    if (!cups_auth_param(schemedata, "trc", trc, sizeof(trc)))
    {
     /*
      * Scheduler doesn't want us to use the root certificate...
      */

      return (1);
    }

    snprintf(filename, sizeof(filename), "%s/certs/0", cg->cups_statedir);
    if ((fp = fopen(filename, "r")) == NULL)
      DEBUG_printf(("9cups_local_auth: Unable to open file \"%s\": %s", filename, strerror(errno)));
  }

  if (fp)
  {
   /*
    * Read the certificate from the file...
    */

    char	certificate[33],	/* Certificate string */
		*certptr;		/* Pointer to certificate string */

    certptr = fgets(certificate, sizeof(certificate), fp);
    fclose(fp);

    if (certptr)
    {
     /*
      * Set the authorization string and return...
      */

      httpSetAuthString(http, "Local", certificate);

      DEBUG_printf(("8cups_local_auth: Returning authstring=\"%s\"",
		    http->authstring));

      return (0);
    }
  }

  return (1);
#endif /* _WIN32 || __EMX__ */
}
