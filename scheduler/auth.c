/*
 * "$Id: auth.c,v 1.10 1999/05/10 21:35:40 mike Exp $"
 *
 *   Authorization routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-1999 by Easy Software Products, all rights reserved.
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
 *       44145 Airport View Drive, Suite 204
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
 *   IsAuthorized()       - Check to see if the user is authorized...
 *   add_allow()          - Add an allow mask to the location.
 *   add_deny()           - Add a deny mask to the location.
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
#include <crypt.h>


/*
 * Local functions...
 */

static authmask_t	*add_allow(location_t *loc);
static authmask_t	*add_deny(location_t *loc);
static int		check_auth(unsigned ip, char *name, int namelen,
				   int num_masks, authmask_t *masks);


/*
 * 'AddLocation()' - Add a location for authorization.
 */

location_t *			/* O - Pointer to new location record */
AddLocation(char *location)	/* I - Location path */
{
  location_t	*temp;		/* New location */


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

  LogMessage(LOG_DEBUG, "AddLocation: added location \'%s\'", location);

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

  LogMessage(LOG_DEBUG, "AllowHost: %s allow %s", loc->location, name);
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

  LogMessage(LOG_DEBUG, "AllowIP: %s allow %08x/%08x", loc->location,
             address, netmask);
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

  LogMessage(LOG_DEBUG, "DenyHost: %s deny %s", loc->location, name);
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

  LogMessage(LOG_DEBUG, "DenyIP: %s deny %08x/%08x\n", loc->location,
             address, netmask);
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
  location_t	*loc,		/* Current location */
		*best;		/* Best match for location so far */
  int		bestlen,	/* Length of best match */
		hostlen;	/* Length of hostname */
  struct passwd	*pw;		/* User password data */
#ifdef HAVE_SHADOW_H
  struct spwd	*spw;		/* Shadow password data */
#endif /* HAVE_SHADOW_H */
  struct group	*grp;		/* Group data */


 /*
  * First loop through the list of locations to find a match...
  */

  best    = NULL;
  bestlen = 0;

  for (i = NumLocations, loc = Locations; i > 0; i --, loc ++)
    if (loc->length > bestlen &&
        strncmp(con->uri, loc->location, loc->length) == 0)
    {
      best    = loc;
      bestlen = loc->length;
    }

 /*
  * If there is no match, the access is not authorized...
  */

  if (best == NULL)
    return (HTTP_FORBIDDEN);

 /*
  * Check host/ip-based accesses...
  */

  auth    = best->order_type;
  address = ntohl(con->http.hostaddr.sin_addr.s_addr);
  hostlen = strlen(con->http.hostname);

  switch (auth)
  {
    case AUTH_ALLOW : /* Order Deny,Allow */
        if (check_auth(address, con->http.hostname, hostlen,
	               best->num_deny, best->deny))
	  auth = AUTH_DENY;

        if (check_auth(address, con->http.hostname, hostlen,
	               best->num_allow, best->allow))
	  auth = AUTH_ALLOW;
	break;

    case AUTH_DENY : /* Order Allow,Deny */
        if (check_auth(address, con->http.hostname, hostlen,
	               best->num_allow, best->allow))
	  auth = AUTH_ALLOW;

        if (check_auth(address, con->http.hostname, hostlen,
	               best->num_deny, best->deny))
	  auth = AUTH_DENY;
	break;
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

  if (con->username[0] == '\0' || con->password[0] == '\0')
    return (HTTP_UNAUTHORIZED);		/* Non-anonymous needed user/pass */

 /*
  * Check the user's password...
  */

  pw = getpwnam(con->username);		/* Get the current password */
  endpwent();				/* Close the password file */

  if (pw == NULL)			/* No such user... */
  {
    LogMessage(LOG_WARN, "IsAuthorized: Unknown username \"%s\"; access denied.",
               con->username);
    return (HTTP_UNAUTHORIZED);
  }

  if (pw->pw_passwd[0] == '\0')		/* Don't allow blank passwords! */
  {
    LogMessage(LOG_WARN, "IsAuthorized: Username \"%s\" has no password; access denied.",
               con->username);
    return (HTTP_UNAUTHORIZED);
  }

#ifdef HAVE_SHADOW_H
  spw = getspnam(con->username);
  endspent();

  if (spw == NULL)			/* No such user or damaged shadow file */
    return (HTTP_UNAUTHORIZED);

  if (spw->sp_pwdp[0] == '\0')		/* Don't allow blank passwords! */
  {
    LogMessage(LOG_WARN, "IsAuthorized: Username \"%s\" has no password; access denied.",
               con->username);
    return (HTTP_UNAUTHORIZED);
  }
#endif /* HAVE_SHADOW_H */

 /*
  * OK, the password isn't blank, so compare with what came from the client...
  */

  DEBUG_printf(("IsAuthorized: pw_passwd = %s, crypt = %s\n",
		pw->pw_passwd, crypt(con->password, pw->pw_passwd)));

  if (strcmp(pw->pw_passwd, crypt(con->password, pw->pw_passwd)) != 0)
  {
#ifdef HAVE_SHADOW_H
    if (spw != NULL)
    {
      DEBUG_printf(("IsAuthorized: sp_pwdp = %s, crypt = %s\n",
		    spw->sp_pwdp, crypt(con->password, spw->sp_pwdp)));

      if (strcmp(spw->sp_pwdp, crypt(con->password, spw->sp_pwdp)) != 0)
	return (HTTP_UNAUTHORIZED);
    }
    else
#endif /* HAVE_SHADOW_H */
      return (HTTP_UNAUTHORIZED);
  }

 /*
  * OK, the password is good.  See if we need normal user access, or group
  * access...
  */

  if (best->level == AUTH_USER)
    return (HTTP_OK);

 /*
  * Check to see if this user is in the specified group...
  */

  grp = getgrnam(best->group_name);
  endgrent();

  if (grp == NULL)			/* No group by that name??? */
  {
    LogMessage(LOG_WARN, "IsAuthorized: group name \"%s\" does not exist!",
               best->group_name);
    return (HTTP_UNAUTHORIZED);
  }

  for (i = 0; grp->gr_mem[i] != NULL; i ++)
    if (strcmp(con->username, grp->gr_mem[i]) == 0)
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
 * 'check_auth()' - Check authorization masks.
 */

static int			/* O - 1 if mask matches, 0 otherwise */
check_auth(unsigned   ip,	/* I - Client address */
           char       *name,	/* I - Client hostname */
	   int        name_len,	/* I - Length of hostname */
           int        num_masks,/* I - Number of masks */
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
 * End of "$Id: auth.c,v 1.10 1999/05/10 21:35:40 mike Exp $".
 */
