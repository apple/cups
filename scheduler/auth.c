/*
 * "$Id: auth.c,v 1.36 2000/11/03 14:13:27 mike Exp $"
 *
 *   Authorization routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2000 by Easy Software Products, all rights reserved.
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
 *   AllowHost()          - Add a host name that is allowed to access the
 *                          location.
 *   AllowIP()            - Add an IP address or network that is allowed to
 *                          access the location.
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
        unsigned   address,	/* I - IP address to add */
        unsigned   netmask)	/* I - Netmask of address */
{
  authmask_t	*temp;		/* New host/domain mask */


  if ((temp = add_allow(loc)) == NULL)
    return;

  temp->type            = AUTH_IP;
  temp->mask.ip.address = address;
  temp->mask.ip.netmask = netmask;

  LogMessage(L_DEBUG, "AllowIP: %s allow %08x/%08x", loc->location,
             address, netmask);
}


/*
 * 'CheckAuth()' - Check authorization masks.
 */

int				/* O - 1 if mask matches, 0 otherwise */
CheckAuth(unsigned   ip,	/* I - Client address */
          char       *name,	/* I - Client hostname */
          int        name_len,	/* I - Length of hostname */
          int        num_masks, /* I - Number of masks */
          authmask_t *masks)	/* I - Masks */
{
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

          if ((ip & masks->mask.ip.netmask) == masks->mask.ip.address)
	    return (1);
          break;
    }

    masks ++;
    num_masks --;
  }

  return (0);
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
       unsigned   address,	/* I - IP address to add */
       unsigned   netmask)	/* I - Netmask of address */
{
  authmask_t	*temp;		/* New host/domain mask */


  if ((temp = add_deny(loc)) == NULL)
    return;

  temp->type            = AUTH_IP;
  temp->mask.ip.address = address;
  temp->mask.ip.netmask = netmask;

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


 /*
  * Loop through the list of locations to find a match...
  */

  best    = NULL;
  bestlen = 0;

  for (i = NumLocations, loc = Locations; i > 0; i --, loc ++)
    if (loc->length > bestlen &&
        strncmp(con->uri, loc->location, loc->length) == 0 &&
	loc->location[0] == '/')
    {
      best    = loc;
      bestlen = loc->length;
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
  int		i,		/* Looping var */
		auth;		/* Authorization status */
  unsigned	address;	/* Authorization address */
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
    return (HTTP_FORBIDDEN);

 /*
  * Check host/ip-based accesses...
  */

  address = ntohl(con->http.hostaddr.sin_addr.s_addr);
  hostlen = strlen(con->http.hostname);

  if (address == 0x7f000001 || strcasecmp(con->http.hostname, "localhost") == 0)
  {
   /*
    * Access from localhost (127.0.0.1) is always allowed...
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

  if (auth == AUTH_DENY)
    return (HTTP_FORBIDDEN);

 /*
  * Now see what access level is required...
  */

  if (best->level == AUTH_ANON)		/* Anonymous access - allow it */
    return (HTTP_OK);

  DEBUG_printf(("IsAuthorized: username = \"%s\", password = \"%s\"\n",
		con->username, con->password));

  if (con->username[0] == '\0')
    return (HTTP_UNAUTHORIZED);		/* Non-anonymous needs user/pass */

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

  if ((address != 0x7f000001 &&
       strcasecmp(con->http.hostname, "localhost") != 0) ||
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

      if (!get_md5_passwd(con->username, best->group_name, md5))
      {
        LogMessage(L_ERROR, "IsAuthorized: No user:group of \"%s:%s\" in passwd.md5!",
	           con->username, best->group_name);
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

  if (best->level == AUTH_USER || strcmp(con->username, "root") == 0)
    return (HTTP_OK);

 /*
  * Check to see if this user is in the specified group...
  */

  grp = getgrnam(best->group_name);
  endgrent();

  if (grp == NULL)			/* No group by that name??? */
  {
    LogMessage(L_WARN, "IsAuthorized: group name \"%s\" does not exist!",
               best->group_name);
    return (HTTP_FORBIDDEN);
  }

  for (i = 0; grp->gr_mem[i] != NULL; i ++)
    if (strcmp(con->username, grp->gr_mem[i]) == 0)
      return (HTTP_OK);

 /*
  * Check to see if the default group ID matches for the user...
  */

  if (grp->gr_gid == pw->pw_gid)
    return (HTTP_OK);

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
 * End of "$Id: auth.c,v 1.36 2000/11/03 14:13:27 mike Exp $".
 */
