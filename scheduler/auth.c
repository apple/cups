/*
 * "$Id: auth.c,v 1.41.2.1 2001/04/02 19:51:46 mike Exp $"
 *
 *   Authorization routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2001 by Easy Software Products, all rights reserved.
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
 * Contents:
 *
 *   AddLocation()        - Add a location for authorization.
 *   AddName()            - Add a name to a location...
 *   AllowHost()          - Add a host name that is allowed to access the
 *                          location.
 *   AllowIP()            - Add an IP address or network that is allowed to
 *                          access the location.
 *   CheckAuth()          - Check authorization masks.
 *   CopyLocation()       - Make a copy of a location...
 *   DeleteAllLocations() - Free all memory used for location authorization.
 *   DenyHost()           - Add a host name that is not allowed to access the
 *                          location.
 *   DenyIP()             - Add an IP address or network that is not allowed
 *                          to access the location.
 *   FindBest()           - Find the location entry that best matches the
 *                          resource.
 *   FindLocation()       - Find the named location.
 *   IsAuthorized()       - Check to see if the user is authorized...
 *   add_allow()          - Add an allow mask to the location.
 *   add_deny()           - Add a deny mask to the location.
 *   get_md5_passwd()     - Get an MD5 password.
 *   pam_func()           - PAM conversation function.
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"
#include <pwd.h>
#include <grp.h>
#ifdef HAVE_SHADOW_H
#  include <shadow.h>
#endif /* HAVE_SHADOW_H */
#ifdef HAVE_CRYPT_H
#  include <crypt.h>
#endif /* HAVE_CRYPT_H */
#if HAVE_LIBPAM
#  include <security/pam_appl.h>
#endif /* HAVE_LIBPAM */


/*
 * Local functions...
 */

static authmask_t	*add_allow(location_t *loc);
static authmask_t	*add_deny(location_t *loc);
static char		*get_md5_passwd(const char *username, const char *group,
			                char passwd[33]);
#if HAVE_LIBPAM
static int		pam_func(int, const struct pam_message **,
			         struct pam_response **, void *);
#endif /* HAVE_LIBPAM */


/*
 * 'AddLocation()' - Add a location for authorization.
 */

location_t *				/* O - Pointer to new location record */
AddLocation(const char *location)	/* I - Location path */
{
  location_t	*temp;			/* New location */


 /*
  * Try to allocate memory for the new location.
  */

  if (NumLocations == 0)
    temp = malloc(sizeof(location_t));
  else
    temp = realloc(Locations, sizeof(location_t) * (NumLocations + 1));

  if (temp == NULL)
    return (NULL);

  Locations = temp;
  temp      += NumLocations;
  NumLocations ++;

 /*
  * Initialize the record and copy the name over...
  */

  memset(temp, 0, sizeof(location_t));
  strncpy(temp->location, location, sizeof(temp->location) - 1);
  temp->length = strlen(temp->location);

  LogMessage(L_DEBUG, "AddLocation: added location \'%s\'", location);

 /*
  * Return the new record...
  */

  return (temp);
}


/*
 * 'AddName()' - Add a name to a location...
 */

void
AddName(location_t *loc,	/* I - Location to add to */
        char       *name)	/* I - Name to add */
{
  char	**temp;			/* Pointer to names array */


  if (loc->num_names == 0)
    temp = malloc(sizeof(char *));
  else
    temp = realloc(loc->names, (loc->num_names + 1) * sizeof(char *));

  if (temp == NULL)
  {
    LogMessage(L_ERROR, "Unable to add name to location %s: %s", loc->location,
               strerror(errno));
    return;
  }

  loc->names = temp;

  if ((temp[loc->num_names] = strdup(name)) == NULL)
  {
    LogMessage(L_ERROR, "Unable to duplicate name for location %s: %s",
               loc->location, strerror(errno));
    return;
  }

  loc->num_names ++;
}


/*
 * 'AllowHost()' - Add a host name that is allowed to access the location.
 */

void
AllowHost(location_t *loc,	/* I - Location to add to */
          char       *name)	/* I - Name of host or domain to add */
{
  authmask_t	*temp;		/* New host/domain mask */


  if ((temp = add_allow(loc)) == NULL)
    return;

  temp->type             = AUTH_NAME;
  temp->mask.name.name   = strdup(name);
  temp->mask.name.length = strlen(name);

  LogMessage(L_DEBUG, "AllowHost: %s allow %s", loc->location, name);
}


/*
 * 'AllowIP()' - Add an IP address or network that is allowed to access the
 *               location.
 */

void
AllowIP(location_t *loc,	/* I - Location to add to */
        unsigned   address[4],	/* I - IP address to add */
        unsigned   netmask[4])	/* I - Netmask of address */
{
  authmask_t	*temp;		/* New host/domain mask */


  if ((temp = add_allow(loc)) == NULL)
    return;

  temp->type = AUTH_IP;
  memcpy(temp->mask.ip.address, address, sizeof(address));
  memcpy(temp->mask.ip.netmask, netmask, sizeof(netmask));

  LogMessage(L_DEBUG, "AllowIP: %s allow %08x/%08x", loc->location,
             address, netmask);
}


/*
 * 'CheckAuth()' - Check authorization masks.
 */

int				/* O - 1 if mask matches, 0 otherwise */
CheckAuth(unsigned   ip[4],	/* I - Client address */
          char       *name,	/* I - Client hostname */
          int        name_len,	/* I - Length of hostname */
          int        num_masks, /* I - Number of masks */
          authmask_t *masks)	/* I - Masks */
{
  int	i;			/* Looping var */


  while (num_masks > 0)
  {
    switch (masks->type)
    {
      case AUTH_NAME :
         /*
	  * Check for exact name match...
	  */

          if (strcasecmp(name, masks->mask.name.name) == 0)
	    return (1);

         /*
	  * Check for domain match...
	  */

	  if (name_len >= masks->mask.name.length &&
	      masks->mask.name.name[0] == '.' &&
	      strcasecmp(name + name_len - masks->mask.name.length,
	                 masks->mask.name.name) == 0)
	    return (1);
          break;

      case AUTH_IP :
         /*
	  * Check for IP/network address match...
	  */

          for (i = 0; i < 4; i ++)
	    if ((ip[i] & masks->mask.ip.netmask[i]) != masks->mask.ip.address[i])
	      break;

	  if (i == 4)
	    return (1);
          break;
    }

    masks ++;
    num_masks --;
  }

  return (0);
}


/*
 * 'CopyLocation()' - Make a copy of a location...
 */

location_t *			/* O - New location */
CopyLocation(location_t **loc)	/* IO - Original location */
{
  int		i;		/* Looping var */
  int		locindex;	/* Index into Locations array */
  location_t	*temp;		/* New location */


 /*
  * Add the new location, updating the original location
  * pointer as needed...
  */

  locindex = *loc - Locations;

  if ((temp = AddLocation((*loc)->location)) == NULL)
    return (NULL);

  *loc = Locations + locindex;

 /*
  * Copy the information from the original location to the new one.
  */

  temp->limit = (*loc)->limit;
  temp->order_type = (*loc)->order_type;
  temp->type       = (*loc)->type;
  temp->level      = (*loc)->level;
  temp->satisfy    = (*loc)->satisfy;
  temp->encryption = (*loc)->encryption;

  if ((temp->num_names  = (*loc)->num_names) > 0)
  {
   /*
    * Copy the names array...
    */

    if ((temp->names = calloc(temp->num_names, sizeof(char *))) == NULL)
    {
      LogMessage(L_ERROR, "CopyLocation: Unable to allocate memory for %d names: %s",
                 temp->num_names, strerror(errno));
      NumLocations --;
      return (NULL);
    }

    for (i = 0; i < temp->num_names; i ++)
      if ((temp->names[i] = strdup((*loc)->names[i])) == NULL)
      {
	LogMessage(L_ERROR, "CopyLocation: Unable to copy name \"%s\": %s",
                   (*loc)->names[i], strerror(errno));

	NumLocations --;
	return (NULL);
      }
  }

  if ((temp->num_allow  = (*loc)->num_allow) > 0)
  {
   /*
    * Copy allow rules...
    */

    if ((temp->allow = calloc(temp->num_allow, sizeof(authmask_t))) == NULL)
    {
      LogMessage(L_ERROR, "CopyLocation: Unable to allocate memory for %d allow rules: %s",
                 temp->num_allow, strerror(errno));
      NumLocations --;
      return (NULL);
    }

    for (i = 0; i < temp->num_allow; i ++)
      switch (temp->allow[i].type = (*loc)->allow[i].type)
      {
        case AUTH_NAME :
	    temp->allow[i].mask.name.length = (*loc)->allow[i].mask.name.length;
	    temp->allow[i].mask.name.name   = strdup((*loc)->allow[i].mask.name.name);

            if (temp->allow[i].mask.name.name == NULL)
	    {
	      LogMessage(L_ERROR, "CopyLocation: Unable to copy allow name \"%s\": %s",
                	 (*loc)->allow[i].mask.name.name, strerror(errno));
	      NumLocations --;
	      return (NULL);
	    }
	    break;
	case AUTH_IP :
	    memcpy(&(temp->allow[i].mask.ip), &((*loc)->allow[i].mask.ip),
	           sizeof(ipmask_t));
	    break;
      }
  }

  if ((temp->num_deny  = (*loc)->num_deny) > 0)
  {
   /*
    * Copy deny rules...
    */

    if ((temp->deny = calloc(temp->num_deny, sizeof(authmask_t))) == NULL)
    {
      LogMessage(L_ERROR, "CopyLocation: Unable to allocate memory for %d deny rules: %s",
                 temp->num_deny, strerror(errno));
      NumLocations --;
      return (NULL);
    }

    for (i = 0; i < temp->num_deny; i ++)
      switch (temp->deny[i].type = (*loc)->deny[i].type)
      {
        case AUTH_NAME :
	    temp->deny[i].mask.name.length = (*loc)->deny[i].mask.name.length;
	    temp->deny[i].mask.name.name   = strdup((*loc)->deny[i].mask.name.name);

            if (temp->deny[i].mask.name.name == NULL)
	    {
	      LogMessage(L_ERROR, "CopyLocation: Unable to copy deny name \"%s\": %s",
                	 (*loc)->deny[i].mask.name.name, strerror(errno));
	      NumLocations --;
	      return (NULL);
	    }
	    break;
	case AUTH_IP :
	    memcpy(&(temp->deny[i].mask.ip), &((*loc)->deny[i].mask.ip),
	           sizeof(ipmask_t));
	    break;
      }
  }

  return (temp);
}


/*
 * 'DeleteAllLocations()' - Free all memory used for location authorization.
 */

void
DeleteAllLocations(void)
{
  int		i, j;		/* Looping vars */
  location_t	*loc;		/* Current location */
  authmask_t	*mask;		/* Current mask */


 /*
  * Free all of the allow/deny records first...
  */

  for (i = NumLocations, loc = Locations; i > 0; i --, loc ++)
  {
    for (j = loc->num_names - 1; j >= 0; j --)
      free(loc->names[j]);

    if (loc->num_names > 0)
      free(loc->names);

    for (j = loc->num_allow, mask = loc->allow; j > 0; j --, mask ++)
      if (mask->type == AUTH_NAME)
        free(mask->mask.name.name);

    if (loc->num_allow > 0)
      free(loc->allow);

    for (j = loc->num_deny, mask = loc->deny; j > 0; j --, mask ++)
      if (mask->type == AUTH_NAME)
        free(mask->mask.name.name);

    if (loc->num_deny > 0)
      free(loc->deny);
  }

 /*
  * Then free the location array...
  */

  if (NumLocations > 0)
    free(Locations);

  Locations    = NULL;
  NumLocations = 0;
}


/*
 * 'DenyHost()' - Add a host name that is not allowed to access the location.
 */

void
DenyHost(location_t *loc,	/* I - Location to add to */
         char       *name)	/* I - Name of host or domain to add */
{
  authmask_t	*temp;		/* New host/domain mask */


  if ((temp = add_deny(loc)) == NULL)
    return;

  temp->type             = AUTH_NAME;
  temp->mask.name.name   = strdup(name);
  temp->mask.name.length = strlen(name);

  LogMessage(L_DEBUG, "DenyHost: %s deny %s", loc->location, name);
}


/*
 * 'DenyIP()' - Add an IP address or network that is not allowed to access
 *              the location.
 */

void
DenyIP(location_t *loc,		/* I - Location to add to */
       unsigned   address[4],	/* I - IP address to add */
       unsigned   netmask[4])	/* I - Netmask of address */
{
  authmask_t	*temp;		/* New host/domain mask */


  if ((temp = add_deny(loc)) == NULL)
    return;

  temp->type = AUTH_IP;
  memcpy(temp->mask.ip.address, address, sizeof(address));
  memcpy(temp->mask.ip.netmask, netmask, sizeof(netmask));

  LogMessage(L_DEBUG, "DenyIP: %s deny %08x/%08x\n", loc->location,
             address, netmask);
}


/*
 * 'FindBest()' - Find the location entry that best matches the resource.
 */

location_t *			/* O - Location that matches */
FindBest(client_t *con)		/* I - Connection */
{
  int		i;		/* Looping var */
  location_t	*loc,		/* Current location */
		*best;		/* Best match for location so far */
  int		bestlen;	/* Length of best match */
  int		limit;		/* Limit field */
  static int	limits[] =	/* Map http_status_t to AUTH_LIMIT_xyz */
		{
		  AUTH_LIMIT_ALL,
		  AUTH_LIMIT_OPTIONS,
		  AUTH_LIMIT_GET,
		  AUTH_LIMIT_GET,
		  AUTH_LIMIT_HEAD,
		  AUTH_LIMIT_POST,
		  AUTH_LIMIT_POST,
		  AUTH_LIMIT_POST,
		  AUTH_LIMIT_PUT,
		  AUTH_LIMIT_PUT,
		  AUTH_LIMIT_DELETE,
		  AUTH_LIMIT_TRACE,
		  AUTH_LIMIT_ALL,
		  AUTH_LIMIT_ALL
		};


 /*
  * Loop through the list of locations to find a match...
  */

  limit   = limits[con->http.state];
  best    = NULL;
  bestlen = 0;

  for (i = NumLocations, loc = Locations; i > 0; i --, loc ++)
  {
    DEBUG_printf(("Location %s Limit %x\n", loc->location, loc->limit));

    if (loc->length > bestlen &&
        strncmp(con->uri, loc->location, loc->length) == 0 &&
	loc->location[0] == '/' &&
	(limit & loc->limit) != 0)
    {
      best    = loc;
      bestlen = loc->length;
    }
  }

 /*
  * Return the match, if any...
  */

  return (best);
}


/*
 * 'FindLocation()' - Find the named location.
 */

location_t *				/* O - Location that matches */
FindLocation(const char *location)	/* I - Connection */
{
  int		i;			/* Looping var */


 /*
  * Loop through the list of locations to find a match...
  */

  for (i = 0; i < NumLocations; i ++)
    if (strcasecmp(Locations[i].location, location) == 0)
      return (Locations + i);

  return (NULL);
}


/*
 * 'IsAuthorized()' - Check to see if the user is authorized...
 */

http_status_t			/* O - HTTP_OK if authorized or error code */
IsAuthorized(client_t *con)	/* I - Connection */
{
  int		i, j,		/* Looping vars */
		auth;		/* Authorization status */
  unsigned	address[4];	/* Authorization address */
  location_t	*best;		/* Best match for location so far */
  int		hostlen;	/* Length of hostname */
  struct passwd	*pw;		/* User password data */
  struct group	*grp;		/* Group data */
  char		nonce[HTTP_MAX_VALUE],
				/* Nonce value from client */
		md5[33];	/* MD5 password */
#if HAVE_LIBPAM
  pam_handle_t	*pamh;		/* PAM authentication handle */
  int		pamerr;		/* PAM error code */
  struct pam_conv pamdata;	/* PAM conversation data */
#else
  char		*pass;		/* Encrypted password */
#  ifdef HAVE_SHADOW_H
  struct spwd	*spw;		/* Shadow password data */
#  endif /* HAVE_SHADOW_H */
#endif /* HAVE_LIBPAM */
  static const char *states[] =	/* HTTP client states... */
		{
		  "WAITING",
		  "OPTIONS",
		  "GET",
		  "GET",
		  "HEAD",
		  "POST",
		  "POST",
		  "POST",
		  "PUT",
		  "PUT",
		  "DELETE",
		  "TRACE",
		  "CLOSE",
		  "STATUS"
		};


 /*
  * Find a matching location; if there is no match then access is
  * not authorized...
  */

  if ((best = FindBest(con)) == NULL)
  {
    DEBUG_printf(("FindBest(%s, %s) failed!\n", states[con->http.state],
                  con->uri));

    return (HTTP_FORBIDDEN);
  }

 /*
  * Check host/ip-based accesses...
  */

#ifdef AF_INET6
  if (con->http.hostaddr.addr.sa_family == AF_INET6)
  {
    address[0] = ntohl(con->http.hostaddr.ipv6.sin6_addr.s6_addr32[0]);
    address[1] = ntohl(con->http.hostaddr.ipv6.sin6_addr.s6_addr32[1]);
    address[2] = ntohl(con->http.hostaddr.ipv6.sin6_addr.s6_addr32[2]);
    address[3] = ntohl(con->http.hostaddr.ipv6.sin6_addr.s6_addr32[3]);
  }
  else
#endif /* AF_INET6 */
  if (con->http.hostaddr.addr.sa_family == AF_INET)
  {
    unsigned temp;		/* Temporary address variable */


   /*
    * Convert 32-bit IPv4 address to 128 bits...
    */

    temp = ntohl(con->http.hostaddr.ipv4.sin_addr.s_addr);

    address[3] = temp & 255;
    temp       >>= 8;
    address[2] = temp & 255;
    temp       >>= 8;
    address[1] = temp & 255;
    temp       >>= 8;
    address[0] = temp & 255;
  }
  else
    memset(address, 0, sizeof(address));

  hostlen = strlen(con->http.hostname);

  if (strcasecmp(con->http.hostname, "localhost") == 0)
  {
   /*
    * Access from localhost (127.0.0.1 or 0.0.0.1) is always allowed...
    */

    auth = AUTH_ALLOW;
  }
  else
  {
   /*
    * Do authorization checks on the domain/address...
    */

    switch (best->order_type)
    {
      default :
	  auth = AUTH_DENY;	/* anti-compiler-warning-code */
	  break;

      case AUTH_ALLOW : /* Order Deny,Allow */
          auth = AUTH_ALLOW;

          if (CheckAuth(address, con->http.hostname, hostlen,
	          	best->num_deny, best->deny))
	    auth = AUTH_DENY;

          if (CheckAuth(address, con->http.hostname, hostlen,
	        	best->num_allow, best->allow))
	    auth = AUTH_ALLOW;
	  break;

      case AUTH_DENY : /* Order Allow,Deny */
          auth = AUTH_DENY;

          if (CheckAuth(address, con->http.hostname, hostlen,
	        	best->num_allow, best->allow))
	    auth = AUTH_ALLOW;

          if (CheckAuth(address, con->http.hostname, hostlen,
	        	best->num_deny, best->deny))
	    auth = AUTH_DENY;
	  break;
    }
  }

  if (auth == AUTH_DENY && best->satisfy == AUTH_SATISFY_ALL)
    return (HTTP_FORBIDDEN);

#ifdef HAVE_LIBSSL
 /*
  * See if encryption is required...
  */

  if (best->encryption >= HTTP_ENCRYPT_REQUIRED && !con->http.tls)
    return (HTTP_UPGRADE_REQUIRED);
#endif /* HAVE_LIBSSL */

 /*
  * Now see what access level is required...
  */

  if (best->level == AUTH_ANON)		/* Anonymous access - allow it */
    return (HTTP_OK);

  DEBUG_printf(("IsAuthorized: username = \"%s\", password = \"%s\"\n",
		con->username, con->password));

  if (con->username[0] == '\0')
  {
    if (best->satisfy == AUTH_SATISFY_ALL || auth == AUTH_DENY)
      return (HTTP_UNAUTHORIZED);	/* Non-anonymous needs user/pass */
    else
      return (HTTP_OK);			/* unless overridden with Satisfy */
  }

 /*
  * Check the user's password...
  */

  pw = getpwnam(con->username);		/* Get the current password */
  endpwent();				/* Close the password file */

  if (pw == NULL)			/* No such user... */
  {
    LogMessage(L_WARN, "IsAuthorized: Unknown username \"%s\"; access denied.",
               con->username);
    return (HTTP_UNAUTHORIZED);
  }

  DEBUG_printf(("IsAuthorized: Checking \"%s\", address = %08x, hostname = \"%s\"\n",
                con->username, address, con->http.hostname));

  if (strcasecmp(con->http.hostname, "localhost") != 0 ||
      strncmp(con->http.fields[HTTP_FIELD_AUTHORIZATION], "Local", 5) != 0)
  {
   /*
    * Not doing local certificate-based authentication; check the password...
    */

    if (!con->password[0])
      return (HTTP_UNAUTHORIZED);

   /*
    * See if we are doing Digest or Basic authentication...
    */

    if (best->type == AUTH_BASIC)
    {
#if HAVE_LIBPAM
     /*
      * Only use PAM to do authentication.  This allows MD5 passwords, among
      * other things...
      */

      pamdata.conv        = pam_func;
      pamdata.appdata_ptr = con;

      pamerr = pam_start("cups", con->username, &pamdata, &pamh);
      if (pamerr != PAM_SUCCESS)
      {
	LogMessage(L_ERROR, "IsAuthorized: pam_start() returned %d (%s)!\n",
        	   pamerr, pam_strerror(pamh, pamerr));
	pam_end(pamh, 0);
	return (HTTP_UNAUTHORIZED);
      }

      pamerr = pam_authenticate(pamh, PAM_SILENT);
      if (pamerr != PAM_SUCCESS)
      {
	LogMessage(L_ERROR, "IsAuthorized: pam_authenticate() returned %d (%s)!\n",
        	   pamerr, pam_strerror(pamh, pamerr));
	pam_end(pamh, 0);
	return (HTTP_UNAUTHORIZED);
      }

      pamerr = pam_acct_mgmt(pamh, PAM_SILENT);
      if (pamerr != PAM_SUCCESS)
      {
	LogMessage(L_ERROR, "IsAuthorized: pam_acct_mgmt() returned %d (%s)!\n",
        	   pamerr, pam_strerror(pamh, pamerr));
	pam_end(pamh, 0);
	return (HTTP_UNAUTHORIZED);
      }

      pam_end(pamh, PAM_SUCCESS);
#else
#  ifdef HAVE_SHADOW_H
      spw = getspnam(con->username);
      endspent();

      if (spw == NULL && strcmp(pw->pw_passwd, "x") == 0)
      {					/* Don't allow blank passwords! */
	LogMessage(L_WARN, "IsAuthorized: Username \"%s\" has no shadow password; access denied.",
        	   con->username);
	return (HTTP_UNAUTHORIZED);		/* No such user or bad shadow file */
      }

#    ifdef DEBUG
      if (spw != NULL)
	printf("spw->sp_pwdp = \"%s\"\n", spw->sp_pwdp);
      else
	puts("spw = NULL");
#    endif /* DEBUG */

      if (spw != NULL && spw->sp_pwdp[0] == '\0' && pw->pw_passwd[0] == '\0')
#  else
      if (pw->pw_passwd[0] == '\0')		/* Don't allow blank passwords! */
#  endif /* HAVE_SHADOW_H */
      {					/* Don't allow blank passwords! */
	LogMessage(L_WARN, "IsAuthorized: Username \"%s\" has no password; access denied.",
        	   con->username);
	return (HTTP_UNAUTHORIZED);
      }

     /*
      * OK, the password isn't blank, so compare with what came from the client...
      */

      DEBUG_printf(("IsAuthorized: pw_passwd = %s, crypt = %s\n",
		    pw->pw_passwd, crypt(con->password, pw->pw_passwd)));

      pass = crypt(con->password, pw->pw_passwd);

      if (pass == NULL ||
	  strcmp(pw->pw_passwd, crypt(con->password, pw->pw_passwd)) != 0)
      {
#  ifdef HAVE_SHADOW_H
	if (spw != NULL)
	{
	  DEBUG_printf(("IsAuthorized: sp_pwdp = %s, crypt = %s\n",
			spw->sp_pwdp, crypt(con->password, spw->sp_pwdp)));

	  pass = crypt(con->password, spw->sp_pwdp);

	  if (pass == NULL ||
              strcmp(spw->sp_pwdp, crypt(con->password, spw->sp_pwdp)) != 0)
	    return (HTTP_UNAUTHORIZED);
	}
	else
#  endif /* HAVE_SHADOW_H */
	  return (HTTP_UNAUTHORIZED);
      }
#endif /* HAVE_LIBPAM */
    }
    else
    {
     /*
      * Do Digest authentication...
      */

      if (!httpGetSubField(&(con->http), HTTP_FIELD_WWW_AUTHENTICATE, "nonce",
                           nonce))
      {
        LogMessage(L_ERROR, "IsAuthorized: No nonce value for Digest authentication!");
        return (HTTP_UNAUTHORIZED);
      }

      if (strcmp(con->http.hostname, nonce) != 0)
      {
        LogMessage(L_ERROR, "IsAuthorized: Nonce value error!");
        LogMessage(L_ERROR, "IsAuthorized: Expected \"%s\",",
	           con->http.hostname);
        LogMessage(L_ERROR, "IsAuthorized: Got \"%s\"!", nonce);
        return (HTTP_UNAUTHORIZED);
      }

      if (!get_md5_passwd(con->username, best->names[0], md5))
      {
        LogMessage(L_ERROR, "IsAuthorized: No user:group for \"%s:%s\" in passwd.md5!",
	           con->username, best->names[0]);
        return (HTTP_UNAUTHORIZED);
      }

      httpMD5Final(nonce, states[con->http.state], con->uri, md5);

      if (strcmp(md5, con->password) != 0)
      {
        LogMessage(L_ERROR, "IsAuthorized: MD5s \"%s\" and \"%s\" don't match!",
	           md5, con->password);
        return (HTTP_UNAUTHORIZED);
      }
    }
  }

 /*
  * OK, the password is good.  See if we need normal user access, or group
  * access... (root always matches)
  */

  if (strcmp(con->username, "root") == 0)
    return (HTTP_OK);

  if (best->level == AUTH_USER)
  {
   /*
    * If there are no names associated with this location, then
    * any valid user is OK...
    */

    if (best->num_names == 0)
      return (HTTP_OK);

   /*
    * Otherwise check the user list and return OK if this user is
    * allowed...
    */

    for (i = 0; i < best->num_names; i ++)
      if (strcmp(con->username, best->names[i]) == 0)
        return (HTTP_OK);

    return (HTTP_UNAUTHORIZED);
  }

 /*
  * Check to see if this user is in any of the named groups...
  */

  for (i = 0; i < best->num_names; i ++)
  {
    grp = getgrnam(best->names[i]);
    endgrent();

    if (grp == NULL)			/* No group by that name??? */
    {
      LogMessage(L_WARN, "IsAuthorized: group name \"%s\" does not exist!",
        	 best->names[i]);
      return (HTTP_FORBIDDEN);
    }

    for (j = 0; grp->gr_mem[j] != NULL; j ++)
      if (strcmp(con->username, grp->gr_mem[j]) == 0)
	return (HTTP_OK);

   /*
    * Check to see if the default group ID matches for the user...
    */

    if (grp->gr_gid == pw->pw_gid)
      return (HTTP_OK);
  }

 /*
  * The user isn't part of the specified group, so deny access...
  */

  DEBUG_puts("IsAuthorized: user not in group!");

  return (HTTP_UNAUTHORIZED);
}


/*
 * 'add_allow()' - Add an allow mask to the location.
 */

static authmask_t *		/* O - New mask record */
add_allow(location_t *loc)	/* I - Location to add to */
{
  authmask_t	*temp;		/* New mask record */


 /*
  * Range-check...
  */

  if (loc == NULL)
    return (NULL);

 /*
  * Try to allocate memory for the record...
  */

  if (loc->num_allow == 0)
    temp = malloc(sizeof(authmask_t));
  else
    temp = realloc(loc->allow, sizeof(authmask_t) * (loc->num_allow + 1));

  if (temp == NULL)
    return (NULL);

  loc->allow = temp;
  temp       += loc->num_allow;
  loc->num_allow ++;

 /*
  * Clear the mask record and return...
  */

  memset(temp, 0, sizeof(authmask_t));
  return (temp);
}


/*
 * 'add_deny()' - Add a deny mask to the location.
 */

static authmask_t *		/* O - New mask record */
add_deny(location_t *loc)	/* I - Location to add to */
{
  authmask_t	*temp;		/* New mask record */


 /*
  * Range-check...
  */

  if (loc == NULL)
    return (NULL);

 /*
  * Try to allocate memory for the record...
  */

  if (loc->num_deny == 0)
    temp = malloc(sizeof(authmask_t));
  else
    temp = realloc(loc->deny, sizeof(authmask_t) * (loc->num_deny + 1));

  if (temp == NULL)
    return (NULL);

  loc->deny = temp;
  temp      += loc->num_deny;
  loc->num_deny ++;

 /*
  * Clear the mask record and return...
  */

  memset(temp, 0, sizeof(authmask_t));
  return (temp);
}


/*
 * 'get_md5_passwd()' - Get an MD5 password.
 */

static char *				/* O - MD5 password string */
get_md5_passwd(const char *username,	/* I - Username */
               const char *group,	/* I - Group */
               char       passwd[33])	/* O - MD5 password string */
{
  FILE	*fp;				/* passwd.md5 file */
  char	filename[1024],			/* passwd.md5 filename */
	line[256],			/* Line from file */
	tempuser[33],			/* User from file */
	tempgroup[33];			/* Group from file */


  snprintf(filename, sizeof(filename), "%s/passwd.md5", ServerRoot);
  if ((fp = fopen(filename, "r")) == NULL)
    return (NULL);

  while (fgets(line, sizeof(line), fp) != NULL)
  {
    if (sscanf(line, "%32[^:]:%32[^:]:%32s", tempuser, tempgroup, passwd) != 3)
      continue;

    if (strcmp(username, tempuser) == 0 &&
        strcmp(group, tempgroup) == 0)
    {
     /*
      * Found the password entry!
      */

      fclose(fp);
      return (passwd);
    }
  }

 /*
  * Didn't find a password entry - return NULL!
  */

  fclose(fp);
  return (NULL);
}


#if HAVE_LIBPAM
/*
 * 'pam_func()' - PAM conversation function.
 */

static int					/* O - Success or failure */
pam_func(int                      num_msg,	/* I - Number of messages */
         const struct pam_message **msg,	/* I - Messages */
         struct pam_response      **resp,	/* O - Responses */
         void                     *appdata_ptr)	/* I - Pointer to connection */
{
  int			i;			/* Looping var */
  struct pam_response	*replies;		/* Replies */
  client_t		*client;		/* Pointer client connection */


 /*
  * Allocate memory for the responses...
  */

  if ((replies = malloc(sizeof(struct pam_response) * num_msg)) == NULL)
    return (PAM_CONV_ERR);

 /*
  * Answer all of the messages...
  */

  client = (client_t *)appdata_ptr;

  for (i = 0; i < num_msg; i ++)
    switch (msg[i]->msg_style)
    {
      case PAM_PROMPT_ECHO_ON:
          replies[i].resp_retcode = PAM_SUCCESS;
          replies[i].resp         = strdup(client->username);
          break;

      case PAM_PROMPT_ECHO_OFF:
          replies[i].resp_retcode = PAM_SUCCESS;
          replies[i].resp         = strdup(client->password);
          break;

      case PAM_TEXT_INFO:
      case PAM_ERROR_MSG:
          replies[i].resp_retcode = PAM_SUCCESS;
          replies[i].resp         = NULL;
          break;

      default:
          free(replies);
          return (PAM_CONV_ERR);
    }

 /*
  * Return the responses back to PAM...
  */

  *resp = replies;

  return (PAM_SUCCESS);
}
#endif /* HAVE_LIBPAM */


/*
 * End of "$Id: auth.c,v 1.41.2.1 2001/04/02 19:51:46 mike Exp $".
 */
