/*
 * "$Id: usersys.c 8498 2009-04-13 17:03:15Z mike $"
 *
 *   User, system, and password routines for the Common UNIX Printing
 *   System (CUPS).
 *
 *   Copyright 2007-2009 by Apple Inc.
 *   Copyright 1997-2006 by Easy Software Products.
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
 *   cupsEncryption()        - Get the default encryption settings.
 *   cupsGetPassword()       - Get a password from the user.
 *   cupsServer()            - Return the hostname/address of the default
 *                             server.
 *   cupsSetEncryption()     - Set the encryption preference.
 *   cupsSetPasswordCB()     - Set the password callback for CUPS.
 *   cupsSetPasswordCB2()    - Set the advanced password callback for CUPS.
 *   cupsSetServer()         - Set the default server name.
 *   cupsSetUser()           - Set the default user name.
 *   cupsUser()              - Return the current user's name.
 *   _cupsGetPassword()      - Get a password from the user.
 *   _cupsSetDefaults()      - Set the default server, port, and encryption.
 *   cups_read_client_conf() - Read a client.conf file.
 */

/*
 * Include necessary headers...
 */

#include "http-private.h"
#include "globals.h"
#include <stdlib.h>
#include <sys/stat.h>
#ifdef WIN32
#  include <windows.h>
#endif /* WIN32 */
#include "debug.h"


/*
 * Local functions...
 */

static void	cups_read_client_conf(cups_file_t *fp,
		                      _cups_globals_t *cg,
		                      const char *cups_encryption,
				      const char *cups_server);


/*
 * 'cupsEncryption()' - Get the default encryption settings.
 *
 * The default encryption setting comes from the CUPS_ENCRYPTION
 * environment variable, then the ~/.cups/client.conf file, and finally the
 * /etc/cups/client.conf file. If not set, the default is
 * @code HTTP_ENCRYPT_IF_REQUESTED@.
 */

http_encryption_t			/* O - Encryption settings */
cupsEncryption(void)
{
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


  if (cg->encryption == (http_encryption_t)-1)
    _cupsSetDefaults();

  return (cg->encryption);
}


/*
 * 'cupsGetPassword()' - Get a password from the user.
 *
 * Uses the current password callback function. Returns @code NULL@ if the
 * user does not provide a password.
 */

const char *				/* O - Password */
cupsGetPassword(const char *prompt)	/* I - Prompt string */
{
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


  return ((cg->password_cb)(prompt, NULL, NULL, NULL, cg->password_data));
}


/*
 * 'cupsGetPassword2()' - Get a password from the user using the advanced
 *                        callback.
 *
 * Uses the current password callback function. Returns @code NULL@ if the
 * user does not provide a password.
 *
 * @since CUPS 1.4/Mac OS X 10.6@
 */

const char *				/* O - Password */
cupsGetPassword2(const char *prompt,	/* I - Prompt string */
		 http_t     *http,	/* I - Connection to server or @code CUPS_HTTP_DEFAULT@ */
		 const char *method,	/* I - Request method ("GET", "POST", "PUT") */
		 const char *resource)	/* I - Resource path */
{
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


  if (!http)
    http = _cupsConnect();

  return ((cg->password_cb)(prompt, http, method, resource, cg->password_data));
}


/*
 * 'cupsServer()' - Return the hostname/address of the default server.
 *
 * The returned value can be a fully-qualified hostname, a numeric
 * IPv4 or IPv6 address, or a domain socket pathname.
 */

const char *				/* O - Server name */
cupsServer(void)
{
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


  if (!cg->server[0])
    _cupsSetDefaults();

  return (cg->server);
}


/*
 * 'cupsSetEncryption()' - Set the encryption preference.
 */

void
cupsSetEncryption(http_encryption_t e)	/* I - New encryption preference */
{
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


  cg->encryption = e;

  if (cg->http)
    httpEncryption(cg->http, e);
}


/*
 * 'cupsSetPasswordCB()' - Set the password callback for CUPS.
 *
 * Pass @code NULL@ to restore the default (console) password callback.
 */

void
cupsSetPasswordCB(cups_password_cb_t cb)/* I - Callback function */
{
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


  if (cb == (cups_password_cb_t)0)
    cg->password_cb = (cups_password_cb2_t)_cupsGetPassword;
  else
    cg->password_cb = (cups_password_cb2_t)cb;

  cg->password_data = NULL;
}


/*
 * 'cupsSetPasswordCB2()' - Set the advanced password callback for CUPS.
 *
 * Pass @code NULL@ to restore the default (console) password callback.
 *
 * @since CUPS 1.4/Mac OS X 10.6@
 */

void
cupsSetPasswordCB2(
    cups_password_cb2_t cb,		/* I - Callback function */
    void                *user_data)	/* I - User data pointer */
{
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


  if (cb == (cups_password_cb2_t)0)
    cg->password_cb = (cups_password_cb2_t)_cupsGetPassword;
  else
    cg->password_cb = cb;

  cg->password_data = user_data;
}


/*
 * 'cupsSetServer()' - Set the default server name.
 *
 * The "server" string can be a fully-qualified hostname, a numeric
 * IPv4 or IPv6 address, or a domain socket pathname. Pass @code NULL@ to
 * restore the default server name.
 */

void
cupsSetServer(const char *server)	/* I - Server name */
{
  char		*port;			/* Pointer to port */
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


  if (server)
  {
    strlcpy(cg->server, server, sizeof(cg->server));

    if (cg->server[0] != '/' && (port = strrchr(cg->server, ':')) != NULL &&
        !strchr(port, ']') && isdigit(port[1] & 255))
    {
      *port++ = '\0';

      cg->ipp_port = atoi(port);
    }

    if (cg->server[0] == '/')
      strcpy(cg->servername, "localhost");
    else
      strlcpy(cg->servername, cg->server, sizeof(cg->servername));
  }
  else
  {
    cg->server[0]     = '\0';
    cg->servername[0] = '\0';
  }

  if (cg->http)
  {
    httpClose(cg->http);
    cg->http = NULL;
  }
}


/*
 * 'cupsSetUser()' - Set the default user name.
 *
 * Pass @code NULL@ to restore the default user name.
 */

void
cupsSetUser(const char *user)		/* I - User name */
{
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


  if (user)
    strlcpy(cg->user, user, sizeof(cg->user));
  else
    cg->user[0] = '\0';
}


#if defined(WIN32)
/*
 * WIN32 username and password stuff.
 */

/*
 * 'cupsUser()' - Return the current user's name.
 */

const char *				/* O - User name */
cupsUser(void)
{
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


  if (!cg->user[0])
  {
    DWORD	size;		/* Size of string */


    size = sizeof(cg->user);
    if (!GetUserName(cg->user, &size))
    {
     /*
      * Use the default username...
      */

      strcpy(cg->user, "unknown");
    }
  }

  return (cg->user);
}


/*
 * '_cupsGetPassword()' - Get a password from the user.
 */

const char *				/* O - Password */
_cupsGetPassword(const char *prompt)	/* I - Prompt string */
{
  return (NULL);
}
#else
/*
 * UNIX username and password stuff...
 */

#  include <pwd.h>

/*
 * 'cupsUser()' - Return the current user's name.
 */

const char *				/* O - User name */
cupsUser(void)
{
  struct passwd	*pwd;			/* User/password entry */
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


  if (!cg->user[0])
  {
   /*
    * Rewind the password file...
    */

    setpwent();

   /*
    * Lookup the password entry for the current user.
    */

    if ((pwd = getpwuid(getuid())) == NULL)
      strcpy(cg->user, "unknown");	/* Unknown user! */
    else
    {
     /*
      * Copy the username...
      */

      setpwent();

      strlcpy(cg->user, pwd->pw_name, sizeof(cg->user));
    }

   /*
    * Rewind the password file again...
    */

    setpwent();
  }

  return (cg->user);
}


/*
 * '_cupsGetPassword()' - Get a password from the user.
 */

const char *				/* O - Password */
_cupsGetPassword(const char *prompt)	/* I - Prompt string */
{
  return (getpass(prompt));
}
#endif /* WIN32 */


/*
 * '_cupsSetDefaults()' - Set the default server, port, and encryption.
 */

void
_cupsSetDefaults(void)
{
  cups_file_t	*fp;			/* File */
  const char	*home,			/* Home directory of user */
		*cups_encryption,	/* CUPS_ENCRYPTION env var */
		*cups_server;		/* CUPS_SERVER env var */
  char		filename[1024];		/* Filename */
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


  DEBUG_puts("_cupsSetDefaults()");

 /*
  * First collect environment variables...
  */

  cups_encryption = getenv("CUPS_ENCRYPTION");
  cups_server     = getenv("CUPS_SERVER");

 /*
  * Then, if needed, the .cups/client.conf or .cupsrc file in the home
  * directory...
  */

  if ((cg->encryption == (http_encryption_t)-1 || !cg->server[0] ||
       !cg->ipp_port) && (home = getenv("HOME")) != NULL)
  {
   /*
    * Look for ~/.cups/client.conf or ~/.cupsrc...
    */

    snprintf(filename, sizeof(filename), "%s/.cups/client.conf", home);
    if ((fp = cupsFileOpen(filename, "r")) == NULL)
    {
      snprintf(filename, sizeof(filename), "%s/.cupsrc", home);
      fp = cupsFileOpen(filename, "r");
    }

    if (fp)
    {
      cups_read_client_conf(fp, cg, cups_encryption, cups_server);
      cupsFileClose(fp);
    }
  }

  if (cg->encryption == (http_encryption_t)-1 || !cg->server[0] ||
      !cg->ipp_port)
  {
   /*
    * Look for CUPS_SERVERROOT/client.conf...
    */

    snprintf(filename, sizeof(filename), "%s/client.conf", cg->cups_serverroot);
    if ((fp = cupsFileOpen(filename, "r")) != NULL)
    {
      cups_read_client_conf(fp, cg, cups_encryption, cups_server);
      cupsFileClose(fp);
    }
  }

 /*
  * If we still have things that aren't set, use the compiled in defaults...
  */

  if (cg->encryption == (http_encryption_t)-1)
    cg->encryption = HTTP_ENCRYPT_IF_REQUESTED;

  if (!cg->server[0])
  {
    if (!cups_server)
    {
#ifdef CUPS_DEFAULT_DOMAINSOCKET
     /*
      * If we are compiled with domain socket support, only use the
      * domain socket if it exists and has the right permissions...
      */

      struct stat	sockinfo;	/* Domain socket information */

      if (!stat(CUPS_DEFAULT_DOMAINSOCKET, &sockinfo) &&
	  (sockinfo.st_mode & S_IRWXO) == S_IRWXO)
	cups_server = CUPS_DEFAULT_DOMAINSOCKET;
      else
#endif /* CUPS_DEFAULT_DOMAINSOCKET */
      cups_server = "localhost";
    }

    cupsSetServer(cups_server);
  }

  if (!cg->ipp_port)
  {
    const char		*ipp_port;	/* IPP_PORT environment variable */
    struct servent	*service;	/* Port number info */  


    if ((ipp_port = getenv("IPP_PORT")) != NULL)
    {
      if ((cg->ipp_port = atoi(ipp_port)) <= 0)
        cg->ipp_port = CUPS_DEFAULT_IPP_PORT;
    }
    else if ((service = getservbyname("ipp", NULL)) == NULL ||
             service->s_port <= 0)
      cg->ipp_port = CUPS_DEFAULT_IPP_PORT;
    else
      cg->ipp_port = ntohs(service->s_port);
  }
}


/*
 * 'cups_read_client_conf()' - Read a client.conf file.
 */

static void
cups_read_client_conf(
    cups_file_t     *fp,		/* I - File to read */
    _cups_globals_t *cg,		/* I - Global data */
    const char      *cups_encryption,	/* I - CUPS_ENCRYPTION env var */
    const char      *cups_server)	/* I - CUPS_SERVER env var */
{
  int	linenum;			/* Current line number */
  char	line[1024],			/* Line from file */
        *value,				/* Pointer into line */
	encryption[1024],		/* Encryption value */
	server_name[1024];		/* ServerName value */


 /*
  * Read from the file...
  */

  linenum = 0;
  while (cupsFileGetConf(fp, line, sizeof(line), &value, &linenum))
  {
    if (!cups_encryption && cg->encryption == (http_encryption_t)-1 &&
        !strcasecmp(line, "Encryption") && value)
    {
      strlcpy(encryption, value, sizeof(encryption));
      cups_encryption = encryption;
    }
    else if (!cups_server && (!cg->server[0] || !cg->ipp_port) &&
             !strcasecmp(line, "ServerName") && value)
    {
      strlcpy(server_name, value, sizeof(server_name));
      cups_server = server_name;
    }
  }

 /*
  * Set values...
  */

  if (cg->encryption == (http_encryption_t)-1 && cups_encryption)
  {
    if (!strcasecmp(cups_encryption, "never"))
      cg->encryption = HTTP_ENCRYPT_NEVER;
    else if (!strcasecmp(cups_encryption, "always"))
      cg->encryption = HTTP_ENCRYPT_ALWAYS;
    else if (!strcasecmp(cups_encryption, "required"))
      cg->encryption = HTTP_ENCRYPT_REQUIRED;
    else
      cg->encryption = HTTP_ENCRYPT_IF_REQUESTED;
  }

  if ((!cg->server[0] || !cg->ipp_port) && cups_server)
  {
    if (!cg->server[0])
    {
     /*
      * Copy server name...
      */

      strlcpy(cg->server, cups_server, sizeof(cg->server));

      if (cg->server[0] != '/' && (value = strrchr(cg->server, ':')) != NULL &&
	  !strchr(value, ']') && isdigit(value[1] & 255))
        *value++ = '\0';
      else
        value = NULL;

      if (cg->server[0] == '/')
	strcpy(cg->servername, "localhost");
      else
	strlcpy(cg->servername, cg->server, sizeof(cg->servername));
    }
    else if (cups_server[0] != '/' &&
             (value = strrchr(cups_server, ':')) != NULL &&
	     !strchr(value, ']') && isdigit(value[1] & 255))
      value ++;
    else
      value = NULL;

    if (!cg->ipp_port && value)
      cg->ipp_port = atoi(value);
  }
}


/*
 * End of "$Id: usersys.c 8498 2009-04-13 17:03:15Z mike $".
 */
