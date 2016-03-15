/*
 * "$Id: auth.c 12604 2015-05-06 01:43:05Z msweet $"
 *
 * Authorization routines for the CUPS scheduler.
 *
 * Copyright 2007-2015 by Apple Inc.
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
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"
#include <grp.h>
#ifdef HAVE_SHADOW_H
#  include <shadow.h>
#endif /* HAVE_SHADOW_H */
#ifdef HAVE_CRYPT_H
#  include <crypt.h>
#endif /* HAVE_CRYPT_H */
#if HAVE_LIBPAM
#  ifdef HAVE_PAM_PAM_APPL_H
#    include <pam/pam_appl.h>
#  else
#    include <security/pam_appl.h>
#  endif /* HAVE_PAM_PAM_APPL_H */
#endif /* HAVE_LIBPAM */
#ifdef HAVE_MEMBERSHIP_H
#  include <membership.h>
#endif /* HAVE_MEMBERSHIP_H */
#ifdef HAVE_AUTHORIZATION_H
#  include <Security/AuthorizationTags.h>
#  ifdef HAVE_SECBASEPRIV_H
#    include <Security/SecBasePriv.h>
#  else
extern const char *cssmErrorString(int error);
#  endif /* HAVE_SECBASEPRIV_H */
#endif /* HAVE_AUTHORIZATION_H */
#ifdef HAVE_SYS_PARAM_H
#  include <sys/param.h>
#endif /* HAVE_SYS_PARAM_H */
#ifdef HAVE_SYS_UCRED_H
#  include <sys/ucred.h>
typedef struct xucred cupsd_ucred_t;
#  define CUPSD_UCRED_UID(c) (c).cr_uid
#else
#  ifndef __OpenBSD__
typedef struct ucred cupsd_ucred_t;
#  else
typedef struct sockpeercred cupsd_ucred_t;
#  endif
#  define CUPSD_UCRED_UID(c) (c).uid
#endif /* HAVE_SYS_UCRED_H */


/*
 * Local functions...
 */

#ifdef HAVE_AUTHORIZATION_H
static int		check_authref(cupsd_client_t *con, const char *right);
#endif /* HAVE_AUTHORIZATION_H */
static int		compare_locations(cupsd_location_t *a,
			                  cupsd_location_t *b);
static cupsd_authmask_t	*copy_authmask(cupsd_authmask_t *am, void *data);
#if !HAVE_LIBPAM
static char		*cups_crypt(const char *pw, const char *salt);
#endif /* !HAVE_LIBPAM */
static void		free_authmask(cupsd_authmask_t *am, void *data);
#if HAVE_LIBPAM
static int		pam_func(int, const struct pam_message **,
			         struct pam_response **, void *);
#else
static void		to64(char *s, unsigned long v, int n);
#endif /* HAVE_LIBPAM */


/*
 * Local structures...
 */

#if HAVE_LIBPAM
typedef struct cupsd_authdata_s		/**** Authentication data ****/
{
  char	username[HTTP_MAX_VALUE],	/* Username string */
	password[HTTP_MAX_VALUE];	/* Password string */
} cupsd_authdata_t;
#endif /* HAVE_LIBPAM */


/*
 * 'cupsdAddIPMask()' - Add an IP address authorization mask.
 */

int					/* O  - 1 on success, 0 on failure */
cupsdAddIPMask(
    cups_array_t   **masks,		/* IO - Masks array (created as needed) */
    const unsigned address[4],		/* I  - IP address */
    const unsigned netmask[4])		/* I  - IP netmask */
{
  cupsd_authmask_t	temp;		/* New host/domain mask */


  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "cupsdAddIPMask(masks=%p(%p), address=%x:%x:%x:%x, "
		  "netmask=%x:%x:%x:%x)",
		  masks, *masks,
		  address[0], address[1], address[2], address[3],
		  netmask[0], netmask[1], netmask[2], netmask[3]);

  temp.type = CUPSD_AUTH_IP;
  memcpy(temp.mask.ip.address, address, sizeof(temp.mask.ip.address));
  memcpy(temp.mask.ip.netmask, netmask, sizeof(temp.mask.ip.netmask));

 /*
  * Create the masks array as needed and add...
  */

  if (!*masks)
    *masks = cupsArrayNew3(NULL, NULL, NULL, 0,
			   (cups_acopy_func_t)copy_authmask,
			   (cups_afree_func_t)free_authmask);

  return (cupsArrayAdd(*masks, &temp));
}


/*
 * 'cupsdAddLocation()' - Add a location for authorization.
 */

void
cupsdAddLocation(cupsd_location_t *loc)	/* I - Location to add */
{
 /*
  * Make sure the locations array is created...
  */

  if (!Locations)
    Locations = cupsArrayNew3((cups_array_func_t)compare_locations, NULL,
                              (cups_ahash_func_t)NULL, 0,
			      (cups_acopy_func_t)NULL,
			      (cups_afree_func_t)cupsdFreeLocation);

  if (Locations)
  {
    cupsArrayAdd(Locations, loc);

    cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdAddLocation: Added location \"%s\"",
                    loc->location ? loc->location : "(null)");
  }
}


/*
 * 'cupsdAddName()' - Add a name to a location...
 */

void
cupsdAddName(cupsd_location_t *loc,	/* I - Location to add to */
             char             *name)	/* I - Name to add */
{
  cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdAddName(loc=%p, name=\"%s\")",
                  loc, name);

  if (!loc->names)
    loc->names = cupsArrayNew3(NULL, NULL, NULL, 0,
                               (cups_acopy_func_t)_cupsStrAlloc,
                               (cups_afree_func_t)_cupsStrFree);

  if (!cupsArrayAdd(loc->names, name))
  {
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "Unable to duplicate name for location %s: %s",
                    loc->location ? loc->location : "nil", strerror(errno));
    return;
  }
}


/*
 * 'cupsdAddNameMask()' - Add a host or interface name authorization mask.
 */

int					/* O  - 1 on success, 0 on failure */
cupsdAddNameMask(cups_array_t **masks,	/* IO - Masks array (created as needed) */
                 char         *name)	/* I  - Host or interface name */
{
  cupsd_authmask_t	temp;		/* New host/domain mask */
  char			ifname[32],	/* Interface name */
			*ifptr;		/* Pointer to end of name */


  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "cupsdAddNameMask(masks=%p(%p), name=\"%s\")",
                  masks, *masks, name);

  if (!_cups_strcasecmp(name, "@LOCAL"))
  {
   /*
    * Deny *interface*...
    */

    temp.type           = CUPSD_AUTH_INTERFACE;
    temp.mask.name.name = (char *)"*";
  }
  else if (!_cups_strncasecmp(name, "@IF(", 4))
  {
   /*
    * Deny *interface*...
    */

    strlcpy(ifname, name + 4, sizeof(ifname));

    ifptr = ifname + strlen(ifname) - 1;

    if (ifptr >= ifname && *ifptr == ')')
    {
      ifptr --;
      *ifptr = '\0';
    }

    temp.type             = CUPSD_AUTH_INTERFACE;
    temp.mask.name.name   = ifname;
  }
  else
  {
   /*
    * Deny name...
    */

    if (*name == '*')
      name ++;

    temp.type             = CUPSD_AUTH_NAME;
    temp.mask.name.name   = (char *)name;
  }

 /*
  * Set the name length...
  */

  temp.mask.name.length = strlen(temp.mask.name.name);

 /*
  * Create the masks array as needed and add...
  */

  if (!*masks)
    *masks = cupsArrayNew3(NULL, NULL, NULL, 0,
			   (cups_acopy_func_t)copy_authmask,
			   (cups_afree_func_t)free_authmask);

  return (cupsArrayAdd(*masks, &temp));
}


/*
 * 'cupsdAuthorize()' - Validate any authorization credentials.
 */

void
cupsdAuthorize(cupsd_client_t *con)	/* I - Client connection */
{
  int		type;			/* Authentication type */
  const char	*authorization;		/* Pointer into Authorization string */
  char		*ptr,			/* Pointer into string */
		username[HTTP_MAX_VALUE],
					/* Username string */
		password[HTTP_MAX_VALUE];
					/* Password string */
  cupsd_cert_t	*localuser;		/* Certificate username */


 /*
  * Locate the best matching location so we know what kind of
  * authentication to expect...
  */

  con->best = cupsdFindBest(con->uri, httpGetState(con->http));
  con->type = CUPSD_AUTH_NONE;

  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "[Client %d] con->uri=\"%s\", con->best=%p(%s)",
                  con->number, con->uri, con->best,
                  con->best ? con->best->location : "");

  if (con->best && con->best->type != CUPSD_AUTH_NONE)
  {
    if (con->best->type == CUPSD_AUTH_DEFAULT)
      type = cupsdDefaultAuthType();
    else
      type = con->best->type;
  }
  else
    type = cupsdDefaultAuthType();

 /*
  * Decode the Authorization string...
  */

  authorization = httpGetField(con->http, HTTP_FIELD_AUTHORIZATION);

  cupsdLogMessage(CUPSD_LOG_DEBUG2, "[Client %d] Authorization=\"%s\"",
                  con->number, authorization);

  username[0] = '\0';
  password[0] = '\0';

#ifdef HAVE_GSSAPI
  con->gss_uid = 0;
#endif /* HAVE_GSSAPI */

#ifdef HAVE_AUTHORIZATION_H
  if (con->authref)
  {
    AuthorizationFree(con->authref, kAuthorizationFlagDefaults);
    con->authref = NULL;
  }
#endif /* HAVE_AUTHORIZATION_H */

  if (!*authorization)
  {
   /*
    * No authorization data provided, return early...
    */

    cupsdLogMessage(CUPSD_LOG_DEBUG,
                    "[Client %d] No authentication data provided.",
                    con->number);
    return;
  }
#ifdef HAVE_AUTHORIZATION_H
  else if (!strncmp(authorization, "AuthRef ", 8) &&
           httpAddrLocalhost(httpGetAddress(con->http)))
  {
    OSStatus		status;		/* Status */
    char		authdata[HTTP_MAX_VALUE];
					/* Nonce value from client */
    int			authlen;	/* Auth string length */
    AuthorizationItemSet *authinfo;	/* Authorization item set */

   /*
    * Get the Authorization Services data...
    */

    authorization += 8;
    while (isspace(*authorization & 255))
      authorization ++;

    authlen = sizeof(authdata);
    httpDecode64_2(authdata, &authlen, authorization);

    if (authlen != kAuthorizationExternalFormLength)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
	              "[Client %d] External Authorization reference size is "
	              "incorrect.", con->number);
      return;
    }

    if ((status = AuthorizationCreateFromExternalForm((AuthorizationExternalForm *)authdata, &con->authref)) != 0)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
		      "[Client %d] AuthorizationCreateFromExternalForm "
		      "returned %d (%s)", con->number, (int)status,
		      cssmErrorString(status));
      return;
    }

    username[0] = '\0';

    if (!AuthorizationCopyInfo(con->authref, kAuthorizationEnvironmentUsername,
			       &authinfo))
    {
      if (authinfo->count == 1 && authinfo->items[0].value &&
          authinfo->items[0].valueLength >= 2)
      {
        strlcpy(username, authinfo->items[0].value, sizeof(username));

        cupsdLogMessage(CUPSD_LOG_DEBUG,
		        "[Client %d] Authorized as \"%s\" using AuthRef",
		        con->number, username);
      }

      AuthorizationFreeItemSet(authinfo);
    }

    if (!username[0])
    {
     /*
      * No username in AuthRef, grab username using peer credentials...
      */

      struct passwd	*pwd;		/* Password entry for this user */
      cupsd_ucred_t	peercred;	/* Peer credentials */
      socklen_t		peersize;	/* Size of peer credentials */

      peersize = sizeof(peercred);

      if (getsockopt(httpGetFd(con->http), 0, LOCAL_PEERCRED, &peercred, &peersize))
      {
        cupsdLogMessage(CUPSD_LOG_ERROR,
                        "[Client %d] Unable to get peer credentials - %s",
                        con->number, strerror(errno));
        return;
      }

      if ((pwd = getpwuid(CUPSD_UCRED_UID(peercred))) == NULL)
      {
        cupsdLogMessage(CUPSD_LOG_ERROR,
                        "[Client %d] Unable to find UID %d for peer "
                        "credentials.", con->number,
                        (int)CUPSD_UCRED_UID(peercred));
        return;
      }

      strlcpy(username, pwd->pw_name, sizeof(username));

      cupsdLogMessage(CUPSD_LOG_DEBUG,
		      "[Client %d] Authorized as \"%s\" using "
		      "AuthRef + PeerCred", con->number, username);
    }

    con->type = CUPSD_AUTH_BASIC;
  }
#endif /* HAVE_AUTHORIZATION_H */
#if defined(SO_PEERCRED) && defined(AF_LOCAL)
  else if (!strncmp(authorization, "PeerCred ", 9) &&
           con->http->hostaddr->addr.sa_family == AF_LOCAL && con->best)
  {
   /*
    * Use peer credentials from domain socket connection...
    */

    struct passwd	*pwd;		/* Password entry for this user */
    cupsd_ucred_t	peercred;	/* Peer credentials */
    socklen_t		peersize;	/* Size of peer credentials */
#ifdef HAVE_AUTHORIZATION_H
    const char		*name;		/* Authorizing name */
    int			no_peer = 0;	/* Don't allow peer credentials? */

   /*
    * See if we should allow peer credentials...
    */

    for (name = (char *)cupsArrayFirst(con->best->names);
         name;
         name = (char *)cupsArrayNext(con->best->names))
    {
      if (!_cups_strncasecmp(name, "@AUTHKEY(", 9) ||
          !_cups_strcasecmp(name, "@SYSTEM"))
      {
       /* Normally don't want peer credentials if we need an auth key... */
	no_peer = 1;
      }
      else if (!_cups_strcasecmp(name, "@OWNER"))
      {
       /* but if @OWNER is present then we allow it... */
        no_peer = 0;
        break;
      }
    }

    if (no_peer)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
		      "[Client %d] PeerCred authentication not allowed for "
		      "resource per AUTHKEY policy.", con->number);
      return;
    }
#endif /* HAVE_AUTHORIZATION_H */

    if ((pwd = getpwnam(authorization + 9)) == NULL)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "[Client %d] User \"%s\" does not exist.", con->number,
                      authorization + 9);
      return;
    }

    peersize = sizeof(peercred);

#  ifdef __APPLE__
    if (getsockopt(httpGetFd(con->http), 0, LOCAL_PEERCRED, &peercred, &peersize))
#  else
    if (getsockopt(httpGetFd(con->http), SOL_SOCKET, SO_PEERCRED, &peercred, &peersize))
#  endif /* __APPLE__ */
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "[Client %d] Unable to get peer credentials - %s",
                      con->number, strerror(errno));
      return;
    }

    if (pwd->pw_uid != CUPSD_UCRED_UID(peercred))
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "[Client %d] Invalid peer credentials for \"%s\" - got "
                      "%d, expected %d!", con->number, authorization + 9,
		      CUPSD_UCRED_UID(peercred), pwd->pw_uid);
#  ifdef HAVE_SYS_UCRED_H
      cupsdLogMessage(CUPSD_LOG_DEBUG, "[Client %d] cr_version=%d",
                      con->number, peercred.cr_version);
      cupsdLogMessage(CUPSD_LOG_DEBUG, "[Client %d] cr_uid=%d",
                      con->number, peercred.cr_uid);
      cupsdLogMessage(CUPSD_LOG_DEBUG, "[Client %d] cr_ngroups=%d",
                      con->number, peercred.cr_ngroups);
      cupsdLogMessage(CUPSD_LOG_DEBUG, "[Client %d] cr_groups[0]=%d",
                      con->number, peercred.cr_groups[0]);
#  endif /* HAVE_SYS_UCRED_H */
      return;
    }

    strlcpy(username, authorization + 9, sizeof(username));

#  ifdef HAVE_GSSAPI
    con->gss_uid = CUPSD_UCRED_UID(peercred);
#  endif /* HAVE_GSSAPI */

    cupsdLogMessage(CUPSD_LOG_DEBUG,
                    "[Client %d] Authorized as %s using PeerCred", con->number,
		    username);

    con->type = CUPSD_AUTH_BASIC;
  }
#endif /* SO_PEERCRED && AF_LOCAL */
  else if (!strncmp(authorization, "Local", 5) &&
	   httpAddrLocalhost(httpGetAddress(con->http)))
  {
   /*
    * Get Local certificate authentication data...
    */

    authorization += 5;
    while (isspace(*authorization & 255))
      authorization ++;

    if ((localuser = cupsdFindCert(authorization)) == NULL)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "[Client %d] Local authentication certificate not found.",
                      con->number);
      return;
    }

    strlcpy(username, localuser->username, sizeof(username));
    con->type = localuser->type;

    cupsdLogMessage(CUPSD_LOG_DEBUG,
		    "[Client %d] Authorized as %s using Local", con->number,
		    username);
  }
  else if (!strncmp(authorization, "Basic", 5))
  {
   /*
    * Get the Basic authentication data...
    */

    int	userlen;			/* Username:password length */


    authorization += 5;
    while (isspace(*authorization & 255))
      authorization ++;

    userlen = sizeof(username);
    httpDecode64_2(username, &userlen, authorization);

   /*
    * Pull the username and password out...
    */

    if ((ptr = strchr(username, ':')) == NULL)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR, "[Client %d] Missing Basic password.",
                      con->number);
      return;
    }

    *ptr++ = '\0';

    if (!username[0])
    {
     /*
      * Username must not be empty...
      */

      cupsdLogMessage(CUPSD_LOG_ERROR, "[Client %d] Empty Basic username.",
                      con->number);
      return;
    }

    if (!*ptr)
    {
     /*
      * Password must not be empty...
      */

      cupsdLogMessage(CUPSD_LOG_ERROR, "[Client %d] Empty Basic password.",
                      con->number);
      return;
    }

    strlcpy(password, ptr, sizeof(password));

   /*
    * Validate the username and password...
    */

    switch (type)
    {
      default :
      case CUPSD_AUTH_BASIC :
          {
#if HAVE_LIBPAM
	   /*
	    * Only use PAM to do authentication.  This supports MD5
	    * passwords, among other things...
	    */

	    pam_handle_t	*pamh;	/* PAM authentication handle */
	    int			pamerr;	/* PAM error code */
	    struct pam_conv	pamdata;/* PAM conversation data */
	    cupsd_authdata_t	data;	/* Authentication data */


            strlcpy(data.username, username, sizeof(data.username));
	    strlcpy(data.password, password, sizeof(data.password));

#  ifdef __sun
	    pamdata.conv        = (int (*)(int, struct pam_message **,
	                                   struct pam_response **,
					   void *))pam_func;
#  else
	    pamdata.conv        = pam_func;
#  endif /* __sun */
	    pamdata.appdata_ptr = &data;

	    pamerr = pam_start("cups", username, &pamdata, &pamh);
	    if (pamerr != PAM_SUCCESS)
	    {
	      cupsdLogMessage(CUPSD_LOG_ERROR,
	                      "[Client %d] pam_start() returned %d (%s)",
        	              con->number, pamerr, pam_strerror(pamh, pamerr));
	      return;
	    }

#  ifdef HAVE_PAM_SET_ITEM
#    ifdef PAM_RHOST
	    pamerr = pam_set_item(pamh, PAM_RHOST, con->http->hostname);
	    if (pamerr != PAM_SUCCESS)
	      cupsdLogMessage(CUPSD_LOG_WARN,
	                      "[Client %d] pam_set_item(PAM_RHOST) "
			      "returned %d (%s)", con->number, pamerr,
			      pam_strerror(pamh, pamerr));
#    endif /* PAM_RHOST */

#    ifdef PAM_TTY
	    pamerr = pam_set_item(pamh, PAM_TTY, "cups");
	    if (pamerr != PAM_SUCCESS)
	      cupsdLogMessage(CUPSD_LOG_WARN,
	                      "[Client %d] pam_set_item(PAM_TTY) "
			      "returned %d (%s)!", con->number, pamerr,
			      pam_strerror(pamh, pamerr));
#    endif /* PAM_TTY */
#  endif /* HAVE_PAM_SET_ITEM */

	    pamerr = pam_authenticate(pamh, PAM_SILENT);
	    if (pamerr != PAM_SUCCESS)
	    {
	      cupsdLogMessage(CUPSD_LOG_ERROR,
	                      "[Client %d] pam_authenticate() returned %d (%s)",
        	              con->number, pamerr, pam_strerror(pamh, pamerr));
	      pam_end(pamh, 0);
	      return;
	    }

#  ifdef HAVE_PAM_SETCRED
            pamerr = pam_setcred(pamh, PAM_ESTABLISH_CRED | PAM_SILENT);
	    if (pamerr != PAM_SUCCESS)
	      cupsdLogMessage(CUPSD_LOG_WARN,
	                      "[Client %d] pam_setcred() returned %d (%s)",
	                      con->number, pamerr,
			      pam_strerror(pamh, pamerr));
#  endif /* HAVE_PAM_SETCRED */

	    pamerr = pam_acct_mgmt(pamh, PAM_SILENT);
	    if (pamerr != PAM_SUCCESS)
	    {
	      cupsdLogMessage(CUPSD_LOG_ERROR,
	                      "[Client %d] pam_acct_mgmt() returned %d (%s)",
        	              con->number, pamerr, pam_strerror(pamh, pamerr));
	      pam_end(pamh, 0);
	      return;
	    }

	    pam_end(pamh, PAM_SUCCESS);

#else
           /*
	    * Use normal UNIX password file-based authentication...
	    */

            char		*pass;	/* Encrypted password */
            struct passwd	*pw;	/* User password data */
#  ifdef HAVE_SHADOW_H
            struct spwd		*spw;	/* Shadow password data */
#  endif /* HAVE_SHADOW_H */


	    pw = getpwnam(username);	/* Get the current password */
	    endpwent();			/* Close the password file */

	    if (!pw)
	    {
	     /*
	      * No such user...
	      */

	      cupsdLogMessage(CUPSD_LOG_ERROR,
	                      "[Client %d] Unknown username \"%s\".",
        	              con->number, username);
	      return;
	    }

#  ifdef HAVE_SHADOW_H
	    spw = getspnam(username);
	    endspent();

	    if (!spw && !strcmp(pw->pw_passwd, "x"))
	    {
	     /*
	      * Don't allow blank passwords!
	      */

	      cupsdLogMessage(CUPSD_LOG_ERROR,
	                      "[Client %d] Username \"%s\" has no shadow "
			      "password.", con->number, username);
	      return;
	    }

	    if (spw && !spw->sp_pwdp[0] && !pw->pw_passwd[0])
#  else
	    if (!pw->pw_passwd[0])
#  endif /* HAVE_SHADOW_H */
	    {
	     /*
	      * Don't allow blank passwords!
	      */

	      cupsdLogMessage(CUPSD_LOG_ERROR,
	                      "[Client %d] Username \"%s\" has no password.",
	                      con->number, username);
	      return;
	    }

	   /*
	    * OK, the password isn't blank, so compare with what came from the
	    * client...
	    */

	    pass = cups_crypt(password, pw->pw_passwd);

	    cupsdLogMessage(CUPSD_LOG_DEBUG2,
	                    "[Client %d] pw_passwd=\"%s\", crypt=\"%s\"",
		            con->number, pw->pw_passwd, pass);

	    if (!pass || strcmp(pw->pw_passwd, pass))
	    {
#  ifdef HAVE_SHADOW_H
	      if (spw)
	      {
		pass = cups_crypt(password, spw->sp_pwdp);

		cupsdLogMessage(CUPSD_LOG_DEBUG2,
	                	"[Client %d] sp_pwdp=\"%s\", crypt=\"%s\"",
				con->number, spw->sp_pwdp, pass);

		if (pass == NULL || strcmp(spw->sp_pwdp, pass))
		{
	          cupsdLogMessage(CUPSD_LOG_ERROR,
		                  "[Client %d] Authentication failed for user "
		                  "\"%s\".", con->number, username);
		  return;
        	}
	      }
	      else
#  endif /* HAVE_SHADOW_H */
	      {
		cupsdLogMessage(CUPSD_LOG_ERROR,
		        	"[Client %d] Authentication failed for user "
		        	"\"%s\".", con->number, username);
		return;
              }
	    }
#endif /* HAVE_LIBPAM */
          }

	  cupsdLogMessage(CUPSD_LOG_DEBUG,
			  "[Client %d] Authorized as %s using Basic",
			  con->number, username);
          break;
    }

    con->type = type;
  }
#ifdef HAVE_GSSAPI
  else if (!strncmp(authorization, "Negotiate", 9))
  {
    int			len;		/* Length of authorization string */
    gss_ctx_id_t	context;	/* Authorization context */
    OM_uint32		major_status,	/* Major status code */
			minor_status;	/* Minor status code */
    gss_buffer_desc	input_token = GSS_C_EMPTY_BUFFER,
					/* Input token from string */
			output_token = GSS_C_EMPTY_BUFFER;
					/* Output token for username */
    gss_name_t		client_name;	/* Client name */


#  ifdef __APPLE__
   /*
    * If the weak-linked GSSAPI/Kerberos library is not present, don't try
    * to use it...
    */

    if (&gss_init_sec_context == NULL)
    {
      cupsdLogMessage(CUPSD_LOG_WARN,
                      "[Client %d] GSSAPI/Kerberos authentication failed "
                      "because the Kerberos framework is not present.",
                      con->number);
      return;
    }
#  endif /* __APPLE__ */

   /*
    * Find the start of the Kerberos input token...
    */

    authorization += 9;
    while (isspace(*authorization & 255))
      authorization ++;

    if (!*authorization)
    {
      cupsdLogMessage(CUPSD_LOG_DEBUG2,
		      "[Client %d] No authentication data specified.",
		      con->number);
      return;
    }

   /*
    * Decode the authorization string to get the input token...
    */

    len                = (int)strlen(authorization);
    input_token.value  = malloc((size_t)len);
    input_token.value  = httpDecode64_2(input_token.value, &len,
					authorization);
    input_token.length = (size_t)len;

   /*
    * Accept the input token to get the authorization info...
    */

    context      = GSS_C_NO_CONTEXT;
    client_name  = GSS_C_NO_NAME;
    major_status = gss_accept_sec_context(&minor_status,
					  &context,
					  ServerCreds,
					  &input_token,
					  GSS_C_NO_CHANNEL_BINDINGS,
					  &client_name,
					  NULL,
					  &output_token,
					  NULL,
					  NULL,
					  NULL);

    if (output_token.length > 0)
      gss_release_buffer(&minor_status, &output_token);

    if (GSS_ERROR(major_status))
    {
      cupsdLogGSSMessage(CUPSD_LOG_DEBUG, major_status, minor_status,
			 "[Client %d] Error accepting GSSAPI security context",
			 con->number);

      if (context != GSS_C_NO_CONTEXT)
	gss_delete_sec_context(&minor_status, &context, GSS_C_NO_BUFFER);
      return;
    }

    con->have_gss = 1;

   /*
    * Get the username associated with the client's credentials...
    */

    if (major_status == GSS_S_CONTINUE_NEEDED)
      cupsdLogGSSMessage(CUPSD_LOG_DEBUG, major_status, minor_status,
			 "[Client %d] Credentials not complete", con->number);
    else if (major_status == GSS_S_COMPLETE)
    {
      major_status = gss_display_name(&minor_status, client_name,
				      &output_token, NULL);

      if (GSS_ERROR(major_status))
      {
	cupsdLogGSSMessage(CUPSD_LOG_DEBUG, major_status, minor_status,
			   "[Client %d] Error getting username", con->number);
	gss_release_name(&minor_status, &client_name);
	gss_delete_sec_context(&minor_status, &context, GSS_C_NO_BUFFER);
	return;
      }

      strlcpy(username, output_token.value, sizeof(username));

      cupsdLogMessage(CUPSD_LOG_DEBUG,
		      "[Client %d] Authorized as %s using Negotiate",
		      con->number, username);

      gss_release_name(&minor_status, &client_name);
      gss_release_buffer(&minor_status, &output_token);

      con->type = CUPSD_AUTH_NEGOTIATE;
    }

    gss_delete_sec_context(&minor_status, &context, GSS_C_NO_BUFFER);

#  if defined(SO_PEERCRED) && defined(AF_LOCAL)
   /*
    * Get the client's UID if we are printing locally - that allows a backend
    * to run as the correct user to get Kerberos credentials of its own.
    */

    if (httpAddrFamily(con->http->hostaddr) == AF_LOCAL)
    {
      cupsd_ucred_t	peercred;	/* Peer credentials */
      socklen_t		peersize;	/* Size of peer credentials */

      peersize = sizeof(peercred);

#    ifdef __APPLE__
      if (getsockopt(httpGetFd(con->http), 0, LOCAL_PEERCRED, &peercred, &peersize))
#    else
      if (getsockopt(httpGetFd(con->http), SOL_SOCKET, SO_PEERCRED, &peercred,
                     &peersize))
#    endif /* __APPLE__ */
      {
	cupsdLogMessage(CUPSD_LOG_ERROR,
	                "[Client %d] Unable to get peer credentials - %s",
			con->number, strerror(errno));
      }
      else
      {
	cupsdLogMessage(CUPSD_LOG_DEBUG,
			"[Client %d] Using credentials for UID %d.",
			con->number, CUPSD_UCRED_UID(peercred));
        con->gss_uid = CUPSD_UCRED_UID(peercred);
      }
    }
#  endif /* SO_PEERCRED && AF_LOCAL */
  }
#endif /* HAVE_GSSAPI */
  else
  {
    char	scheme[256];		/* Auth scheme... */


    if (sscanf(authorization, "%255s", scheme) != 1)
      strlcpy(scheme, "UNKNOWN", sizeof(scheme));

    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "[Client %d] Bad authentication data \"%s ...\"",
                    con->number, scheme);
    return;
  }

 /*
  * If we get here, then we were able to validate the username and
  * password - copy the validated username and password to the client
  * data and return...
  */

  strlcpy(con->username, username, sizeof(con->username));
  strlcpy(con->password, password, sizeof(con->password));
}


/*
 * 'cupsdCheckAccess()' - Check whether the given address is allowed to
 *                        access a location.
 */

int					/* O - 1 if allowed, 0 otherwise */
cupsdCheckAccess(
    unsigned         ip[4],		/* I - Client address */
    const char       *name,		/* I - Client hostname */
    size_t           namelen,		/* I - Length of hostname */
    cupsd_location_t *loc)		/* I - Location to check */
{
  int	allow;				/* 1 if allowed, 0 otherwise */


  if (!_cups_strcasecmp(name, "localhost"))
  {
   /*
    * Access from localhost (127.0.0.1 or ::1) is always allowed...
    */

    return (1);
  }
  else
  {
   /*
    * Do authorization checks on the domain/address...
    */

    switch (loc->order_type)
    {
      default :
	  allow = 0;	/* anti-compiler-warning-code */
	  break;

      case CUPSD_AUTH_ALLOW : /* Order Deny,Allow */
          allow = 1;

          if (cupsdCheckAuth(ip, name, namelen, loc->deny))
	    allow = 0;

          if (cupsdCheckAuth(ip, name, namelen, loc->allow))
	    allow = 1;
	  break;

      case CUPSD_AUTH_DENY : /* Order Allow,Deny */
          allow = 0;

          if (cupsdCheckAuth(ip, name, namelen, loc->allow))
	    allow = 1;

          if (cupsdCheckAuth(ip, name, namelen, loc->deny))
	    allow = 0;
	  break;
    }
  }

  return (allow);
}


/*
 * 'cupsdCheckAuth()' - Check authorization masks.
 */

int					/* O - 1 if mask matches, 0 otherwise */
cupsdCheckAuth(unsigned     ip[4],	/* I - Client address */
	       const char   *name,	/* I - Client hostname */
	       size_t       name_len,	/* I - Length of hostname */
	       cups_array_t *masks)	/* I - Masks */
{
  int			i;		/* Looping var */
  cupsd_authmask_t	*mask;		/* Current mask */
  cupsd_netif_t		*iface;		/* Network interface */
  unsigned		netip4;		/* IPv4 network address */
#ifdef AF_INET6
  unsigned		netip6[4];	/* IPv6 network address */
#endif /* AF_INET6 */


  for (mask = (cupsd_authmask_t *)cupsArrayFirst(masks);
       mask;
       mask = (cupsd_authmask_t *)cupsArrayNext(masks))
  {
    switch (mask->type)
    {
      case CUPSD_AUTH_INTERFACE :
         /*
	  * Check for a match with a network interface...
	  */

          netip4 = htonl(ip[3]);

#ifdef AF_INET6
          netip6[0] = htonl(ip[0]);
          netip6[1] = htonl(ip[1]);
          netip6[2] = htonl(ip[2]);
          netip6[3] = htonl(ip[3]);
#endif /* AF_INET6 */

	  cupsdNetIFUpdate();

          if (!strcmp(mask->mask.name.name, "*"))
	  {
#ifdef __APPLE__
           /*
	    * Allow Back-to-My-Mac addresses...
	    */

	    if ((ip[0] & 0xff000000) == 0xfd000000)
	      return (1);
#endif /* __APPLE__ */

	   /*
	    * Check against all local interfaces...
	    */

	    for (iface = (cupsd_netif_t *)cupsArrayFirst(NetIFList);
		 iface;
		 iface = (cupsd_netif_t *)cupsArrayNext(NetIFList))
	    {
	     /*
	      * Only check local interfaces...
	      */

	      if (!iface->is_local)
	        continue;

              if (iface->address.addr.sa_family == AF_INET)
	      {
	       /*
	        * Check IPv4 address...
		*/

        	if ((netip4 & iface->mask.ipv4.sin_addr.s_addr) ==
	            (iface->address.ipv4.sin_addr.s_addr &
		     iface->mask.ipv4.sin_addr.s_addr))
		  return (1);
              }
#ifdef AF_INET6
	      else
	      {
	       /*
	        * Check IPv6 address...
		*/

        	for (i = 0; i < 4; i ++)
		  if ((netip6[i] & iface->mask.ipv6.sin6_addr.s6_addr32[i]) !=
		      (iface->address.ipv6.sin6_addr.s6_addr32[i] &
		       iface->mask.ipv6.sin6_addr.s6_addr32[i]))
		    break;

		if (i == 4)
		  return (1);
              }
#endif /* AF_INET6 */
	    }
	  }
	  else
	  {
	   /*
	    * Check the named interface...
	    */

	    for (iface = (cupsd_netif_t *)cupsArrayFirst(NetIFList);
	         iface;
		 iface = (cupsd_netif_t *)cupsArrayNext(NetIFList))
	    {
              if (strcmp(mask->mask.name.name, iface->name))
                continue;

              if (iface->address.addr.sa_family == AF_INET)
	      {
	       /*
		* Check IPv4 address...
		*/

        	if ((netip4 & iface->mask.ipv4.sin_addr.s_addr) ==
	            (iface->address.ipv4.sin_addr.s_addr &
		     iface->mask.ipv4.sin_addr.s_addr))
		  return (1);
              }
#ifdef AF_INET6
	      else
	      {
	       /*
		* Check IPv6 address...
		*/

        	for (i = 0; i < 4; i ++)
		  if ((netip6[i] & iface->mask.ipv6.sin6_addr.s6_addr32[i]) !=
		      (iface->address.ipv6.sin6_addr.s6_addr32[i] &
		       iface->mask.ipv6.sin6_addr.s6_addr32[i]))
		    break;

		if (i == 4)
		  return (1);
              }
#endif /* AF_INET6 */
	    }
	  }
	  break;

      case CUPSD_AUTH_NAME :
         /*
	  * Check for exact name match...
	  */

          if (!_cups_strcasecmp(name, mask->mask.name.name))
	    return (1);

         /*
	  * Check for domain match...
	  */

	  if (name_len >= mask->mask.name.length &&
	      mask->mask.name.name[0] == '.' &&
	      !_cups_strcasecmp(name + name_len - mask->mask.name.length,
	                  mask->mask.name.name))
	    return (1);
          break;

      case CUPSD_AUTH_IP :
         /*
	  * Check for IP/network address match...
	  */

          for (i = 0; i < 4; i ++)
	    if ((ip[i] & mask->mask.ip.netmask[i]) !=
	            mask->mask.ip.address[i])
	      break;

	  if (i == 4)
	    return (1);
          break;
    }
  }

  return (0);
}


/*
 * 'cupsdCheckGroup()' - Check for a user's group membership.
 */

int					/* O - 1 if user is a member, 0 otherwise */
cupsdCheckGroup(
    const char    *username,		/* I - User name */
    struct passwd *user,		/* I - System user info */
    const char    *groupname)		/* I - Group name */
{
  int		i;			/* Looping var */
  struct group	*group;			/* System group info */
#ifdef HAVE_MBR_UID_TO_UUID
  uuid_t	useruuid,		/* UUID for username */
		groupuuid;		/* UUID for groupname */
  int		is_member;		/* True if user is a member of group */
#endif /* HAVE_MBR_UID_TO_UUID */


  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "cupsdCheckGroup(username=\"%s\", user=%p, groupname=\"%s\")",
                  username, user, groupname);

 /*
  * Validate input...
  */

  if (!username || !groupname)
    return (0);

 /*
  * Check to see if the user is a member of the named group...
  */

  group = getgrnam(groupname);
  endgrent();

  if (group != NULL)
  {
   /*
    * Group exists, check it...
    */

    for (i = 0; group->gr_mem[i]; i ++)
      if (!_cups_strcasecmp(username, group->gr_mem[i]))
	return (1);
  }

 /*
  * Group doesn't exist or user not in group list, check the group ID
  * against the user's group ID...
  */

  if (user && group && group->gr_gid == user->pw_gid)
    return (1);

#ifdef HAVE_MBR_UID_TO_UUID
 /*
  * Check group membership through MacOS X membership API...
  */

  if (user && !mbr_uid_to_uuid(user->pw_uid, useruuid))
  {
    if (group)
    {
     /*
      * Map group name to UUID and check membership...
      */

      if (!mbr_gid_to_uuid(group->gr_gid, groupuuid))
        if (!mbr_check_membership(useruuid, groupuuid, &is_member))
	  if (is_member)
	    return (1);
    }
    else if (groupname[0] == '#')
    {
     /*
      * Use UUID directly and check for equality (user UUID) and
      * membership (group UUID)...
      */

      if (!uuid_parse((char *)groupname + 1, groupuuid))
      {
        if (!uuid_compare(useruuid, groupuuid))
	  return (1);
	else if (!mbr_check_membership(useruuid, groupuuid, &is_member))
	  if (is_member)
	    return (1);
      }

      return (0);
    }
  }
  else if (groupname[0] == '#')
    return (0);
#endif /* HAVE_MBR_UID_TO_UUID */

 /*
  * If we get this far, then the user isn't part of the named group...
  */

  return (0);
}


/*
 * 'cupsdCopyLocation()' - Make a copy of a location...
 */

cupsd_location_t *			/* O - New location */
cupsdCopyLocation(
    cupsd_location_t *loc)		/* I - Original location */
{
  cupsd_location_t	*temp;		/* New location */


 /*
  * Make a copy of the original location...
  */

  if ((temp = calloc(1, sizeof(cupsd_location_t))) == NULL)
    return (NULL);

 /*
  * Copy the information from the original location to the new one.
  */

  if (!loc)
    return (temp);

  if (loc->location)
    temp->location = _cupsStrAlloc(loc->location);

  temp->length     = loc->length;
  temp->limit      = loc->limit;
  temp->order_type = loc->order_type;
  temp->type       = loc->type;
  temp->level      = loc->level;
  temp->satisfy    = loc->satisfy;
  temp->encryption = loc->encryption;

  if (loc->names)
  {
    if ((temp->names = cupsArrayDup(loc->names)) == NULL)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "Unable to allocate memory for %d names: %s",
		      cupsArrayCount(loc->names), strerror(errno));

      cupsdFreeLocation(temp);
      return (NULL);
    }
  }

  if (loc->allow)
  {
   /*
    * Copy allow rules...
    */

    if ((temp->allow = cupsArrayDup(loc->allow)) == NULL)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "Unable to allocate memory for %d allow rules: %s",
                      cupsArrayCount(loc->allow), strerror(errno));
      cupsdFreeLocation(temp);
      return (NULL);
    }
  }

  if (loc->deny)
  {
   /*
    * Copy deny rules...
    */

    if ((temp->deny = cupsArrayDup(loc->deny)) == NULL)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "Unable to allocate memory for %d deny rules: %s",
                      cupsArrayCount(loc->deny), strerror(errno));
      cupsdFreeLocation(temp);
      return (NULL);
    }
  }

  return (temp);
}


/*
 * 'cupsdDeleteAllLocations()' - Free all memory used for location authorization.
 */

void
cupsdDeleteAllLocations(void)
{
 /*
  * Free the location array, which will free all of the locations...
  */

  cupsArrayDelete(Locations);
  Locations = NULL;
}


/*
 * 'cupsdFindBest()' - Find the location entry that best matches the resource.
 */

cupsd_location_t *			/* O - Location that matches */
cupsdFindBest(const char   *path,	/* I - Resource path */
              http_state_t state)	/* I - HTTP state/request */
{
  char			uri[HTTP_MAX_URI],
					/* URI in request... */
			*uriptr;	/* Pointer into URI */
  cupsd_location_t	*loc,		/* Current location */
			*best;		/* Best match for location so far */
  size_t		bestlen;	/* Length of best match */
  int			limit;		/* Limit field */
  static const int	limits[] =	/* Map http_status_t to CUPSD_AUTH_LIMIT_xyz */
		{
		  CUPSD_AUTH_LIMIT_ALL,
		  CUPSD_AUTH_LIMIT_OPTIONS,
		  CUPSD_AUTH_LIMIT_GET,
		  CUPSD_AUTH_LIMIT_GET,
		  CUPSD_AUTH_LIMIT_HEAD,
		  CUPSD_AUTH_LIMIT_POST,
		  CUPSD_AUTH_LIMIT_POST,
		  CUPSD_AUTH_LIMIT_POST,
		  CUPSD_AUTH_LIMIT_PUT,
		  CUPSD_AUTH_LIMIT_PUT,
		  CUPSD_AUTH_LIMIT_DELETE,
		  CUPSD_AUTH_LIMIT_TRACE,
		  CUPSD_AUTH_LIMIT_ALL,
		  CUPSD_AUTH_LIMIT_ALL,
		  CUPSD_AUTH_LIMIT_ALL,
		  CUPSD_AUTH_LIMIT_ALL
		};


 /*
  * First copy the connection URI to a local string so we have drop
  * any .ppd extension from the pathname in /printers or /classes
  * URIs...
  */

  strlcpy(uri, path, sizeof(uri));

  if (!strncmp(uri, "/printers/", 10) ||
      !strncmp(uri, "/classes/", 9))
  {
   /*
    * Check if the URI has .ppd on the end...
    */

    uriptr = uri + strlen(uri) - 4; /* len > 4 if we get here... */

    if (!strcmp(uriptr, ".ppd"))
      *uriptr = '\0';
  }

  if ((uriptr = strchr(uri, '?')) != NULL)
    *uriptr = '\0';		/* Drop trailing query string */

  if ((uriptr = uri + strlen(uri) - 1) > uri && *uriptr == '/')
    *uriptr = '\0';		/* Remove trailing '/' */

 /*
  * Loop through the list of locations to find a match...
  */

  limit   = limits[state];
  best    = NULL;
  bestlen = 0;

  cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdFindBest: uri = \"%s\", limit=%x...", uri, limit);


  for (loc = (cupsd_location_t *)cupsArrayFirst(Locations);
       loc;
       loc = (cupsd_location_t *)cupsArrayNext(Locations))
  {
    cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdFindBest: Location %s(%d) Limit %x", loc->location ? loc->location : "(null)", (int)loc->length, loc->limit);

    if (!strncmp(uri, "/printers/", 10) || !strncmp(uri, "/classes/", 9))
    {
     /*
      * Use case-insensitive comparison for queue names...
      */

      if (loc->length > bestlen && loc->location &&
          !_cups_strncasecmp(uri, loc->location, loc->length) &&
	  loc->location[0] == '/' &&
	  (limit & loc->limit) != 0)
      {
	best    = loc;
	bestlen = loc->length;
      }
    }
    else
    {
     /*
      * Use case-sensitive comparison for other URIs...
      */

      if (loc->length > bestlen && loc->location &&
          !strncmp(uri, loc->location, loc->length) &&
	  loc->location[0] == '/' &&
	  (limit & loc->limit) != 0)
      {
	best    = loc;
	bestlen = loc->length;
      }
    }
  }

 /*
  * Return the match, if any...
  */

  cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdFindBest: best = %s",
                  best ? best->location : "NONE");

  return (best);
}


/*
 * 'cupsdFindLocation()' - Find the named location.
 */

cupsd_location_t *			/* O - Location that matches */
cupsdFindLocation(const char *location)	/* I - Connection */
{
  cupsd_location_t	key;		/* Search key */


  key.location = (char *)location;

  return ((cupsd_location_t *)cupsArrayFind(Locations, &key));
}


/*
 * 'cupsdFreeLocation()' - Free all memory used by a location.
 */

void
cupsdFreeLocation(cupsd_location_t *loc)/* I - Location to free */
{
  cupsArrayDelete(loc->names);
  cupsArrayDelete(loc->allow);
  cupsArrayDelete(loc->deny);

  _cupsStrFree(loc->location);
  free(loc);
}


/*
 * 'cupsdIsAuthorized()' - Check to see if the user is authorized...
 */

http_status_t				/* O - HTTP_OK if authorized or error code */
cupsdIsAuthorized(cupsd_client_t *con,	/* I - Connection */
                  const char     *owner)/* I - Owner of object */
{
  int			i,		/* Looping vars */
			auth,		/* Authorization status */
			type;		/* Type of authentication */
  http_addr_t		*hostaddr = httpGetAddress(con->http);
					/* Client address */
  const char		*hostname = httpGetHostname(con->http, NULL, 0);
					/* Client hostname */
  unsigned		address[4];	/* Authorization address */
  cupsd_location_t	*best;		/* Best match for location so far */
  size_t		hostlen;	/* Length of hostname */
  char			*name,		/* Current username */
			username[256],	/* Username to authorize */
			ownername[256],	/* Owner name to authorize */
			*ptr;		/* Pointer into username */
  struct passwd		*pw;		/* User password data */
  static const char * const levels[] =	/* Auth levels */
		{
		  "ANON",
		  "USER",
		  "GROUP"
		};
  static const char * const types[] =	/* Auth types */
		{
		  "None",
		  "Basic",
		  "Negotiate"
		};


  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "cupsdIsAuthorized: con->uri=\"%s\", con->best=%p(%s)",
                  con->uri, con->best, con->best ? con->best->location ?
                			   con->best->location : "(null)" : "");
  if (owner)
    cupsdLogMessage(CUPSD_LOG_DEBUG2,
                    "cupsdIsAuthorized: owner=\"%s\"", owner);

 /*
  * If there is no "best" authentication rule for this request, then
  * access is allowed from the local system and denied from other
  * addresses...
  */

  if (!con->best)
  {
    if (httpAddrLocalhost(httpGetAddress(con->http)) ||
        !strcmp(hostname, ServerName) ||
	cupsArrayFind(ServerAlias, (void *)hostname))
      return (HTTP_OK);
    else
      return (HTTP_FORBIDDEN);
  }

  best = con->best;

  if ((type = best->type) == CUPSD_AUTH_DEFAULT)
    type = cupsdDefaultAuthType();

  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "cupsdIsAuthorized: level=CUPSD_AUTH_%s, type=%s, "
		  "satisfy=CUPSD_AUTH_SATISFY_%s, num_names=%d",
                  levels[best->level], types[type],
	          best->satisfy ? "ANY" : "ALL", cupsArrayCount(best->names));

  if (best->limit == CUPSD_AUTH_LIMIT_IPP)
    cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdIsAuthorized: op=%x(%s)",
                    best->op, ippOpString(best->op));

 /*
  * Check host/ip-based accesses...
  */

#ifdef AF_INET6
  if (httpAddrFamily(hostaddr) == AF_INET6)
  {
   /*
    * Copy IPv6 address...
    */

    address[0] = ntohl(hostaddr->ipv6.sin6_addr.s6_addr32[0]);
    address[1] = ntohl(hostaddr->ipv6.sin6_addr.s6_addr32[1]);
    address[2] = ntohl(hostaddr->ipv6.sin6_addr.s6_addr32[2]);
    address[3] = ntohl(hostaddr->ipv6.sin6_addr.s6_addr32[3]);
  }
  else
#endif /* AF_INET6 */
  if (con->http->hostaddr->addr.sa_family == AF_INET)
  {
   /*
    * Copy IPv4 address...
    */

    address[0] = 0;
    address[1] = 0;
    address[2] = 0;
    address[3] = ntohl(hostaddr->ipv4.sin_addr.s_addr);
  }
  else
    memset(address, 0, sizeof(address));

  hostlen = strlen(hostname);

  auth = cupsdCheckAccess(address, hostname, hostlen, best)
             ? CUPSD_AUTH_ALLOW : CUPSD_AUTH_DENY;

  cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdIsAuthorized: auth=CUPSD_AUTH_%s...",
                  auth ? "DENY" : "ALLOW");

  if (auth == CUPSD_AUTH_DENY && best->satisfy == CUPSD_AUTH_SATISFY_ALL)
    return (HTTP_FORBIDDEN);

#ifdef HAVE_SSL
 /*
  * See if encryption is required...
  */

  if ((best->encryption >= HTTP_ENCRYPT_REQUIRED && !con->http->tls &&
      _cups_strcasecmp(hostname, "localhost") &&
      !httpAddrLocalhost(hostaddr) &&
      best->satisfy == CUPSD_AUTH_SATISFY_ALL) &&
      !(type == CUPSD_AUTH_NEGOTIATE ||
        (type == CUPSD_AUTH_NONE &&
         cupsdDefaultAuthType() == CUPSD_AUTH_NEGOTIATE)))
  {
    cupsdLogMessage(CUPSD_LOG_DEBUG,
                    "cupsdIsAuthorized: Need upgrade to TLS...");
    return (HTTP_UPGRADE_REQUIRED);
  }
#endif /* HAVE_SSL */

 /*
  * Now see what access level is required...
  */

  if (best->level == CUPSD_AUTH_ANON ||	/* Anonymous access - allow it */
      (type == CUPSD_AUTH_NONE && cupsArrayCount(best->names) == 0))
    return (HTTP_OK);

  if (!con->username[0] && type == CUPSD_AUTH_NONE &&
      best->limit == CUPSD_AUTH_LIMIT_IPP)
  {
   /*
    * Check for unauthenticated username...
    */

    ipp_attribute_t	*attr;		/* requesting-user-name attribute */


    attr = ippFindAttribute(con->request, "requesting-user-name", IPP_TAG_NAME);
    if (attr)
    {
      cupsdLogMessage(CUPSD_LOG_DEBUG,
                      "cupsdIsAuthorized: requesting-user-name=\"%s\"",
                      attr->values[0].string.text);
      strlcpy(username, attr->values[0].string.text, sizeof(username));
    }
    else if (best->satisfy == CUPSD_AUTH_SATISFY_ALL || auth == CUPSD_AUTH_DENY)
      return (HTTP_UNAUTHORIZED);	/* Non-anonymous needs user/pass */
    else
      return (HTTP_OK);			/* unless overridden with Satisfy */
  }
  else
  {
    cupsdLogMessage(CUPSD_LOG_DEBUG, "cupsdIsAuthorized: username=\"%s\"",
	            con->username);

#ifdef HAVE_AUTHORIZATION_H
    if (!con->username[0] && !con->authref)
#else
    if (!con->username[0])
#endif /* HAVE_AUTHORIZATION_H */
    {
      if (best->satisfy == CUPSD_AUTH_SATISFY_ALL || auth == CUPSD_AUTH_DENY)
	return (HTTP_UNAUTHORIZED);	/* Non-anonymous needs user/pass */
      else
	return (HTTP_OK);		/* unless overridden with Satisfy */
    }


    if (con->type != type && type != CUPSD_AUTH_NONE &&
#ifdef HAVE_GSSAPI
        (type != CUPSD_AUTH_NEGOTIATE || con->gss_uid <= 0) &&
#endif /* HAVE_GSSAPI */
        con->type != CUPSD_AUTH_BASIC)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR, "Authorized using %s, expected %s.",
                      types[con->type], types[type]);

      return (HTTP_UNAUTHORIZED);
    }

    strlcpy(username, con->username, sizeof(username));
  }

 /*
  * OK, got a username.  See if we need normal user access, or group
  * access... (root always matches)
  */

  if (!strcmp(username, "root"))
    return (HTTP_OK);

 /*
  * Strip any @domain or @KDC from the username and owner...
  */

  if ((ptr = strchr(username, '@')) != NULL)
    *ptr = '\0';

  if (owner)
  {
    strlcpy(ownername, owner, sizeof(ownername));

    if ((ptr = strchr(ownername, '@')) != NULL)
      *ptr = '\0';
  }
  else
    ownername[0] = '\0';

 /*
  * Get the user info...
  */

  if (username[0])
  {
    pw = getpwnam(username);
    endpwent();
  }
  else
    pw = NULL;

  if (best->level == CUPSD_AUTH_USER)
  {
   /*
    * If there are no names associated with this location, then
    * any valid user is OK...
    */

    if (cupsArrayCount(best->names) == 0)
      return (HTTP_OK);

   /*
    * Otherwise check the user list and return OK if this user is
    * allowed...
    */

    cupsdLogMessage(CUPSD_LOG_DEBUG2,
                    "cupsdIsAuthorized: Checking user membership...");

#ifdef HAVE_AUTHORIZATION_H
   /*
    * If an authorization reference was supplied it must match a right name...
    */

    if (con->authref)
    {
      for (name = (char *)cupsArrayFirst(best->names);
           name;
	   name = (char *)cupsArrayNext(best->names))
      {
	if (!_cups_strncasecmp(name, "@AUTHKEY(", 9) && check_authref(con, name + 9))
	  return (HTTP_OK);
	else if (!_cups_strcasecmp(name, "@SYSTEM") && SystemGroupAuthKey &&
		 check_authref(con, SystemGroupAuthKey))
	  return (HTTP_OK);
      }

      return (HTTP_FORBIDDEN);
    }
#endif /* HAVE_AUTHORIZATION_H */

    for (name = (char *)cupsArrayFirst(best->names);
	 name;
	 name = (char *)cupsArrayNext(best->names))
    {
      if (!_cups_strcasecmp(name, "@OWNER") && owner &&
          !_cups_strcasecmp(username, ownername))
	return (HTTP_OK);
      else if (!_cups_strcasecmp(name, "@SYSTEM"))
      {
        for (i = 0; i < NumSystemGroups; i ++)
	  if (cupsdCheckGroup(username, pw, SystemGroups[i]))
	    return (HTTP_OK);
      }
      else if (name[0] == '@')
      {
        if (cupsdCheckGroup(username, pw, name + 1))
          return (HTTP_OK);
      }
      else if (!_cups_strcasecmp(username, name))
        return (HTTP_OK);
    }

    return (con->username[0] ? HTTP_FORBIDDEN : HTTP_UNAUTHORIZED);
  }

 /*
  * Check to see if this user is in any of the named groups...
  */

  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "cupsdIsAuthorized: Checking group membership...");

 /*
  * Check to see if this user is in any of the named groups...
  */

  for (name = (char *)cupsArrayFirst(best->names);
       name;
       name = (char *)cupsArrayNext(best->names))
  {
    cupsdLogMessage(CUPSD_LOG_DEBUG2,
                    "cupsdIsAuthorized: Checking group \"%s\" membership...",
                    name);

    if (!_cups_strcasecmp(name, "@SYSTEM"))
    {
      for (i = 0; i < NumSystemGroups; i ++)
	if (cupsdCheckGroup(username, pw, SystemGroups[i]))
	  return (HTTP_OK);
    }
    else if (cupsdCheckGroup(username, pw, name))
      return (HTTP_OK);
  }

 /*
  * The user isn't part of the specified group, so deny access...
  */

  cupsdLogMessage(CUPSD_LOG_DEBUG,
                  "cupsdIsAuthorized: User not in group(s)!");

  return (con->username[0] ? HTTP_FORBIDDEN : HTTP_UNAUTHORIZED);
}


/*
 * 'cupsdNewLocation()' - Create a new location for authorization.
 *
 * Note: Still need to call cupsdAddLocation() to add it to the list of global
 * locations.
 */

cupsd_location_t *			/* O - Pointer to new location record */
cupsdNewLocation(const char *location)	/* I - Location path */
{
  cupsd_location_t	*temp;		/* New location */


 /*
  * Try to allocate memory for the new location.
  */

  if ((temp = calloc(1, sizeof(cupsd_location_t))) == NULL)
    return (NULL);

 /*
  * Initialize the record and copy the name over...
  */

  if ((temp->location = _cupsStrAlloc(location)) == NULL)
  {
    free(temp);
    return (NULL);
  }

  temp->length = strlen(temp->location);

 /*
  * Return the new record...
  */

  return (temp);
}


#ifdef HAVE_AUTHORIZATION_H
/*
 * 'check_authref()' - Check if an authorization services reference has the
 *		       supplied right.
 */

static int				/* O - 1 if right is valid, 0 otherwise */
check_authref(cupsd_client_t *con,	/* I - Connection */
	      const char     *right)	/* I - Right name */
{
  OSStatus		status;		/* OS Status */
  AuthorizationItem	authright;	/* Authorization right */
  AuthorizationRights	authrights;	/* Authorization rights */
  AuthorizationFlags	authflags;	/* Authorization flags */


 /*
  * Check to see if the user is allowed to perform the task...
  */

  if (!con->authref)
    return (0);

  authright.name        = right;
  authright.valueLength = 0;
  authright.value       = NULL;
  authright.flags       = 0;

  authrights.count = 1;
  authrights.items = &authright;

  authflags = kAuthorizationFlagDefaults |
	      kAuthorizationFlagExtendRights;

  if ((status = AuthorizationCopyRights(con->authref, &authrights,
					kAuthorizationEmptyEnvironment,
					authflags, NULL)) != 0)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR,
		    "AuthorizationCopyRights(\"%s\") returned %d (%s)",
		    authright.name, (int)status, cssmErrorString(status));
    return (0);
  }

  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "AuthorizationCopyRights(\"%s\") succeeded!",
		  authright.name);

  return (1);
}
#endif /* HAVE_AUTHORIZATION_H */


/*
 * 'compare_locations()' - Compare two locations.
 */

static int				/* O - Result of comparison */
compare_locations(cupsd_location_t *a,	/* I - First location */
                  cupsd_location_t *b)	/* I - Second location */
{
  return (strcmp(b->location, a->location));
}


/*
 * 'copy_authmask()' - Copy function for auth masks.
 */

static cupsd_authmask_t	*		/* O - New auth mask */
copy_authmask(cupsd_authmask_t *mask,	/* I - Existing auth mask */
              void             *data)	/* I - User data (unused) */
{
  cupsd_authmask_t	*temp;		/* New auth mask */


  (void)data;

  if ((temp = malloc(sizeof(cupsd_authmask_t))) != NULL)
  {
    memcpy(temp, mask, sizeof(cupsd_authmask_t));

    if (temp->type == CUPSD_AUTH_NAME || temp->type == CUPSD_AUTH_INTERFACE)
    {
     /*
      * Make a copy of the name...
      */

      if ((temp->mask.name.name = _cupsStrAlloc(temp->mask.name.name)) == NULL)
      {
       /*
        * Failed to make copy...
	*/

        free(temp);
	temp = NULL;
      }
    }
  }

  return (temp);
}


#if !HAVE_LIBPAM
/*
 * 'cups_crypt()' - Encrypt the password using the DES or MD5 algorithms,
 *                  as needed.
 */

static char *				/* O - Encrypted password */
cups_crypt(const char *pw,		/* I - Password string */
           const char *salt)		/* I - Salt (key) string */
{
  if (!strncmp(salt, "$1$", 3))
  {
   /*
    * Use MD5 passwords without the benefit of PAM; this is for
    * Slackware Linux, and the algorithm was taken from the
    * old shadow-19990827/lib/md5crypt.c source code... :(
    */

    int			i;		/* Looping var */
    unsigned long	n;		/* Output number */
    int			pwlen;		/* Length of password string */
    const char		*salt_end;	/* End of "salt" data for MD5 */
    char		*ptr;		/* Pointer into result string */
    _cups_md5_state_t	state;		/* Primary MD5 state info */
    _cups_md5_state_t	state2;		/* Secondary MD5 state info */
    unsigned char	digest[16];	/* MD5 digest result */
    static char		result[120];	/* Final password string */


   /*
    * Get the salt data between dollar signs, e.g. $1$saltdata$md5.
    * Get a maximum of 8 characters of salt data after $1$...
    */

    for (salt_end = salt + 3; *salt_end && (salt_end - salt) < 11; salt_end ++)
      if (*salt_end == '$')
        break;

   /*
    * Compute the MD5 sum we need...
    */

    pwlen = strlen(pw);

    _cupsMD5Init(&state);
    _cupsMD5Append(&state, (unsigned char *)pw, pwlen);
    _cupsMD5Append(&state, (unsigned char *)salt, salt_end - salt);

    _cupsMD5Init(&state2);
    _cupsMD5Append(&state2, (unsigned char *)pw, pwlen);
    _cupsMD5Append(&state2, (unsigned char *)salt + 3, salt_end - salt - 3);
    _cupsMD5Append(&state2, (unsigned char *)pw, pwlen);
    _cupsMD5Finish(&state2, digest);

    for (i = pwlen; i > 0; i -= 16)
      _cupsMD5Append(&state, digest, i > 16 ? 16 : i);

    for (i = pwlen; i > 0; i >>= 1)
      _cupsMD5Append(&state, (unsigned char *)((i & 1) ? "" : pw), 1);

    _cupsMD5Finish(&state, digest);

    for (i = 0; i < 1000; i ++)
    {
      _cupsMD5Init(&state);

      if (i & 1)
        _cupsMD5Append(&state, (unsigned char *)pw, pwlen);
      else
        _cupsMD5Append(&state, digest, 16);

      if (i % 3)
        _cupsMD5Append(&state, (unsigned char *)salt + 3, salt_end - salt - 3);

      if (i % 7)
        _cupsMD5Append(&state, (unsigned char *)pw, pwlen);

      if (i & 1)
        _cupsMD5Append(&state, digest, 16);
      else
        _cupsMD5Append(&state, (unsigned char *)pw, pwlen);

      _cupsMD5Finish(&state, digest);
    }

   /*
    * Copy the final sum to the result string and return...
    */

    memcpy(result, salt, (size_t)(salt_end - salt));
    ptr = result + (salt_end - salt);
    *ptr++ = '$';

    for (i = 0; i < 5; i ++, ptr += 4)
    {
      n = ((((unsigned)digest[i] << 8) | (unsigned)digest[i + 6]) << 8);

      if (i < 4)
        n |= (unsigned)digest[i + 12];
      else
        n |= (unsigned)digest[5];

      to64(ptr, n, 4);
    }

    to64(ptr, (unsigned)digest[11], 2);
    ptr += 2;
    *ptr = '\0';

    return (result);
  }
  else
  {
   /*
    * Use the standard crypt() function...
    */

    return (crypt(pw, salt));
  }
}
#endif /* !HAVE_LIBPAM */


/*
 * 'free_authmask()' - Free function for auth masks.
 */

static void
free_authmask(cupsd_authmask_t *mask,	/* I - Auth mask to free */
              void             *data)	/* I - User data (unused) */
{
  (void)data;

  if (mask->type == CUPSD_AUTH_NAME || mask->type == CUPSD_AUTH_INTERFACE)
    _cupsStrFree(mask->mask.name.name);

  free(mask);
}


#if HAVE_LIBPAM
/*
 * 'pam_func()' - PAM conversation function.
 */

static int				/* O - Success or failure */
pam_func(
    int                      num_msg,	/* I - Number of messages */
    const struct pam_message **msg,	/* I - Messages */
    struct pam_response      **resp,	/* O - Responses */
    void                     *appdata_ptr)
					/* I - Pointer to connection */
{
  int			i;		/* Looping var */
  struct pam_response	*replies;	/* Replies */
  cupsd_authdata_t	*data;		/* Pointer to auth data */


 /*
  * Allocate memory for the responses...
  */

  if ((replies = malloc(sizeof(struct pam_response) * (size_t)num_msg)) == NULL)
    return (PAM_CONV_ERR);

 /*
  * Answer all of the messages...
  */

  DEBUG_printf(("pam_func: appdata_ptr = %p\n", appdata_ptr));

  data = (cupsd_authdata_t *)appdata_ptr;

  for (i = 0; i < num_msg; i ++)
  {
    DEBUG_printf(("pam_func: Message = \"%s\"\n", msg[i]->msg));

    switch (msg[i]->msg_style)
    {
      case PAM_PROMPT_ECHO_ON:
          DEBUG_printf(("pam_func: PAM_PROMPT_ECHO_ON, returning \"%s\"...\n",
	                data->username));
          replies[i].resp_retcode = PAM_SUCCESS;
          replies[i].resp         = strdup(data->username);
          break;

      case PAM_PROMPT_ECHO_OFF:
          DEBUG_printf(("pam_func: PAM_PROMPT_ECHO_OFF, returning \"%s\"...\n",
	                data->password));
          replies[i].resp_retcode = PAM_SUCCESS;
          replies[i].resp         = strdup(data->password);
          break;

      case PAM_TEXT_INFO:
          DEBUG_puts("pam_func: PAM_TEXT_INFO...");
          replies[i].resp_retcode = PAM_SUCCESS;
          replies[i].resp         = NULL;
          break;

      case PAM_ERROR_MSG:
          DEBUG_puts("pam_func: PAM_ERROR_MSG...");
          replies[i].resp_retcode = PAM_SUCCESS;
          replies[i].resp         = NULL;
          break;

      default:
          DEBUG_printf(("pam_func: Unknown PAM message %d...\n",
	                msg[i]->msg_style));
          free(replies);
          return (PAM_CONV_ERR);
    }
  }

 /*
  * Return the responses back to PAM...
  */

  *resp = replies;

  return (PAM_SUCCESS);
}
#else


/*
 * 'to64()' - Base64-encode an integer value...
 */

static void
to64(char          *s,			/* O - Output string */
     unsigned long v,			/* I - Value to encode */
     int           n)			/* I - Number of digits */
{
  const char	*itoa64 = "./0123456789"
                          "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                          "abcdefghijklmnopqrstuvwxyz";


  for (; n > 0; n --, v >>= 6)
    *s++ = itoa64[v & 0x3f];
}
#endif /* HAVE_LIBPAM */


/*
 * End of "$Id: auth.c 12604 2015-05-06 01:43:05Z msweet $".
 */
