/*
 * "$Id: usersys.c,v 1.3 2000/01/04 13:45:37 mike Exp $"
 *
 *   User, system, and password routines for the Common UNIX Printing
 *   System (CUPS).
 *
 *   Copyright 1997-2000 by Easy Software Products.
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
 */

/*
 * Include necessary headers...
 */

#include "cups.h"
#include <config.h>
#include <stdlib.h>
#include <ctype.h>


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
  return ("WindowsUser");
}


/*
 * 'cupsGetPassword()' - Get a password from the user...
 */

const char *				/* O - Password */
cupsGetPassword(const char *prompt)	/* I - Prompt string */
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


 /*
  * Rewind the password file...
  */

  setpwent();

 /*
  * Lookup the password entry for the current user.
  */

  if ((pwd = getpwuid(getuid())) == NULL)
    return ("unknown");			/* Unknown user! */

 /*
  * Rewind the password file again and return the username...
  */

  setpwent();

  return (pwd->pw_name);
}


/*
 * 'cupsGetPassword()' - Get a password from the user...
 */

const char *				/* O - Password */
cupsGetPassword(const char *prompt)	/* I - Prompt string */
{
  return (getpass(prompt));
}
#endif /* WIN32 || __EMX__ */


/*
 * 'cupsServer()' - Return the hostname of the default server...
 */

const char *				/* O - Server name */
cupsServer(void)
{
  FILE		*fp;			/* cupsd.conf file */
  char		*server;		/* Pointer to server name */
  static char	line[1024];		/* Line from file */


 /*
  * First see if the CUPS_SERVER environment variable is set...
  */

  if ((server = getenv("CUPS_SERVER")) != NULL)
    return (server);

 /*
  * Next check to see if we have a cupsd.conf file...
  */

  if ((fp = fopen(CUPS_SERVERROOT "/conf/cupsd.conf", "r")) == NULL)
    return ("localhost");

 /*
  * Read the cupsd.conf file and look for a ServerName line...
  */

  while (fgets(line, sizeof(line), fp) != NULL)
    if (strncmp(line, "ServerName ", 11) == 0)
    {
     /*
      * Got it!  Drop any trailing newline and find the name...
      */

      server = line + strlen(line) - 1;
      if (*server == '\n')
        *server = '\0';

      for (server = line + 11; isspace(*server); server ++);

      if (*server)
        return (server);
    }

 /*
  * Didn't see a ServerName line, so return "localhost"...
  */

  fclose(fp);

  return ("localhost");
}


/*
 * End of "$Id: usersys.c,v 1.3 2000/01/04 13:45:37 mike Exp $".
 */
