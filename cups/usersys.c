/*
 * "$Id: usersys.c,v 1.14.2.2 2002/03/01 19:55:14 mike Exp $"
 *
 *   User, system, and password routines for the Common UNIX Printing
 *   System (CUPS).
 *
 *   Copyright 1997-2002 by Easy Software Products.
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
 *   cupsEncryption()    - Get the default encryption settings...
 *   cupsGetPassword()   - Get a password from the user...
 *   cupsServer()        - Return the hostname of the default server...
 *   cupsSetEncryption() - Set the encryption preference.
 *   cupsSetPasswordCB() - Set the password callback for CUPS.
 *   cupsSetServer()     - Set the default server name...
 *   cupsSetUser()       - Set the default user name...
 *   cupsUser()          - Return the current users name.
 *   cups_get_password() - Get a password from the user...
 *   cups_get_line()     - Get a line from a file...
 */

/*
 * Include necessary headers...
 */

#include "cups.h"
#include "string.h"
#include <stdlib.h>
#include <ctype.h>


/*
 * Local functions...
 */

static const char	*cups_get_password(const char *prompt);
static char		*cups_get_line(char *buf, int buflen, FILE *fp);


/*
 * Local globals...
 */

static http_encryption_t cups_encryption = (http_encryption_t)-1;
static char		cups_user[65] = "",
			cups_server[256] = "";
static const char	*(*cups_pwdcb)(const char *) = cups_get_password;


/*
 * 'cupsEncryption()' - Get the default encryption settings...
 */

http_encryption_t
cupsEncryption(void)
{
  FILE		*fp;			/* client.conf file */
  char		*encryption;		/* CUPS_ENCRYPTION variable */
  const char	*home;			/* Home directory of user */
  static char	line[1024];		/* Line from file */


 /*
  * First see if we have already set the encryption stuff...
  */

  if (cups_encryption == (http_encryption_t)-1)
  {
   /*
    * Then see if the CUPS_ENCRYPTION environment variable is set...
    */

    if ((encryption = getenv("CUPS_ENCRYPTION")) == NULL)
    {
     /*
      * Next check to see if we have a $HOME/.cupsrc or client.conf file...
      */

      if ((home = getenv("HOME")) != NULL)
      {
	snprintf(line, sizeof(line), "%s/.cupsrc", home);
	fp = fopen(line, "r");
      }
      else
	fp = NULL;

      if (fp == NULL)
      {
	if ((home = getenv("CUPS_SERVERROOT")) != NULL)
	{
	  snprintf(line, sizeof(line), "%s/client.conf", home);
	  fp = fopen(line, "r");
	}
	else
	  fp = fopen(CUPS_SERVERROOT "/client.conf", "r");
      }

      encryption = "IfRequested";

      if (fp != NULL)
      {
       /*
	* Read the config file and look for a ServerName line...
	*/

	while (cups_get_line(line, sizeof(line), fp) != NULL)
	  if (strncmp(line, "Encryption ", 11) == 0)
	  {
	   /*
	    * Got it!  Drop any trailing newline and find the name...
	    */

	    encryption = line + strlen(line) - 1;
	    if (*encryption == '\n')
              *encryption = '\0';

	    for (encryption = line + 11; isspace(*encryption); encryption ++);
	    break;
	  }

	fclose(fp);
      }
    }

   /*
    * Set the encryption preference...
    */

    if (strcasecmp(encryption, "never") == 0)
      cups_encryption = HTTP_ENCRYPT_NEVER;
    else if (strcasecmp(encryption, "always") == 0)
      cups_encryption = HTTP_ENCRYPT_ALWAYS;
    else if (strcasecmp(encryption, "required") == 0)
      cups_encryption = HTTP_ENCRYPT_REQUIRED;
    else
      cups_encryption = HTTP_ENCRYPT_IF_REQUESTED;
  }

  return (cups_encryption);
}


/*
 * 'cupsGetPassword()' - Get a password from the user...
 */

const char *				/* O - Password */
cupsGetPassword(const char *prompt)	/* I - Prompt string */
{
  return ((*cups_pwdcb)(prompt));
}


/*
 * 'cupsSetEncryption()' - Set the encryption preference.
 */

void
cupsSetEncryption(http_encryption_t e)	/* I - New encryption preference */
{
  cups_encryption = e;
}


/*
 * 'cupsServer()' - Return the hostname of the default server...
 */

const char *				/* O - Server name */
cupsServer(void)
{
  FILE		*fp;			/* client.conf file */
  char		*server;		/* Pointer to server name */
  const char	*home;			/* Home directory of user */
  static char	line[1024];		/* Line from file */


 /*
  * First see if we have already set the server name...
  */

  if (!cups_server[0])
  {
   /*
    * Then see if the CUPS_SERVER environment variable is set...
    */

    if ((server = getenv("CUPS_SERVER")) == NULL)
    {
     /*
      * Next check to see if we have a $HOME/.cupsrc or client.conf file...
      */

      if ((home = getenv("HOME")) != NULL)
      {
	snprintf(line, sizeof(line), "%s/.cupsrc", home);
	fp = fopen(line, "r");
      }
      else
	fp = NULL;

      if (fp == NULL)
      {
	if ((home = getenv("CUPS_SERVERROOT")) != NULL)
	{
	  snprintf(line, sizeof(line), "%s/client.conf", home);
	  fp = fopen(line, "r");
	}
	else
	  fp = fopen(CUPS_SERVERROOT "/client.conf", "r");
      }

      server = "localhost";

      if (fp != NULL)
      {
       /*
	* Read the config file and look for a ServerName line...
	*/

	while (cups_get_line(line, sizeof(line), fp) != NULL)
	  if (strncmp(line, "ServerName ", 11) == 0)
	  {
	   /*
	    * Got it!  Drop any trailing newline and find the name...
	    */

	    server = line + strlen(line) - 1;
	    if (*server == '\n')
              *server = '\0';

	    for (server = line + 11; isspace(*server); server ++);
	    break;
	  }

	fclose(fp);
      }
    }

   /*
    * Copy the server name over...
    */

    strncpy(cups_server, server, sizeof(cups_server) - 1);
    cups_server[sizeof(cups_server) - 1] = '\0';
  }

  return (cups_server);
}


/*
 * 'cupsSetPasswordCB()' - Set the password callback for CUPS.
 */

void
cupsSetPasswordCB(const char *(*cb)(const char *))	/* I - Callback function */
{
  if (cb == (const char *(*)(const char *))0)
    cups_pwdcb = cups_get_password;
  else
    cups_pwdcb = cb;
}


/*
 * 'cupsSetServer()' - Set the default server name...
 */

void
cupsSetServer(const char *server)	/* I - Server name */
{
  if (server)
  {
    strncpy(cups_server, server, sizeof(cups_server) - 1);
    cups_server[sizeof(cups_server) - 1] = '\0';
  }
  else
    cups_server[0] = '\0';
}


/*
 * 'cupsSetUser()' - Set the default user name...
 */

void
cupsSetUser(const char *user)		/* I - User name */
{
  if (user)
  {
    strncpy(cups_user, user, sizeof(cups_user) - 1);
    cups_user[sizeof(cups_user) - 1] = '\0';
  }
  else
    cups_user[0] = '\0';
}


#if defined(WIN32) || defined(__EMX__)
/*
 * WIN32 and OS/2 username and password stuff...
 */

/*
 * 'cupsUser()' - Return the current user's name.
 */

const char *				/* O - User name */
cupsUser(void)
{
  if (!cups_user[0])
    strcpy(cups_user, "WindowsUser");

  return (cups_user);
}


/*
 * 'cups_get_password()' - Get a password from the user...
 */

static const char *			/* O - Password */
cups_get_password(const char *prompt)	/* I - Prompt string */
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


  if (!cups_user[0])
  {
   /*
    * Rewind the password file...
    */

    setpwent();

   /*
    * Lookup the password entry for the current user.
    */

    if ((pwd = getpwuid(getuid())) == NULL)
      strcpy(cups_user, "unknown");	/* Unknown user! */
    else
    {
     /*
      * Copy the username...
      */

      setpwent();

      strncpy(cups_user, pwd->pw_name, sizeof(cups_user) - 1);
      cups_user[sizeof(cups_user) - 1] = '\0';
    }

   /*
    * Rewind the password file again...
    */

    setpwent();
  }

  return (cups_user);
}


/*
 * 'cups_get_password()' - Get a password from the user...
 */

static const char *			/* O - Password */
cups_get_password(const char *prompt)	/* I - Prompt string */
{
  return (getpass(prompt));
}
#endif /* WIN32 || __EMX__ */


/*
 * 'cups_get_line()' - Get a line from a file.
 */

static char *			/* O - Line from file */
cups_get_line(char *buf,	/* I - Line buffer */
              int  buflen,	/* I - Size of line buffer */
	      FILE *fp)		/* I - File to read from */
{
  char	*bufptr;		/* Pointer to end of buffer */


 /*
  * Get the line from a file...
  */

  if (fgets(buf, buflen, fp) == NULL)
    return (NULL);

 /*
  * Remove all trailing whitespace...
  */

  bufptr = buf + strlen(buf) - 1;
  if (bufptr < buf)
    return (NULL);

  while (isspace(*bufptr) && bufptr >= buf)
    *bufptr-- = '\0';

  return (buf);
}


/*
 * End of "$Id: usersys.c,v 1.14.2.2 2002/03/01 19:55:14 mike Exp $".
 */
