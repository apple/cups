/*
 * "$Id: lppasswd.c,v 1.6 2000/07/17 19:44:35 mike Exp $"
 *
 *   MD5 password program for the Common UNIX Printing System (CUPS).
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
 *   main()  - Add, change, or delete passwords from the MD5 password file.
 *   usage() - Show program usage.
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <grp.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <cups/cups.h>
#include <cups/md5.h>
#include <cups/string.h>


/*
 * Operations...
 */

#define ADD	0
#define CHANGE	1
#define DELETE	2


/*
 * Local functions...
 */

void	usage(FILE *fp);


/*
 * 'main()' - Add, change, or delete passwords from the MD5 password file.
 */

int				/* O - Exit status */
main(int  argc,			/* I - Number of command-line arguments */
     char *argv[])		/* I - Command-line arguments */
{
  int		i;		/* Looping var */
  char		*opt;		/* Option pointer */
  const char	*username;	/* Pointer to username */
  const char	*groupname;	/* Pointer to group name */
  int		op;		/* Operation (add, change, delete) */
  struct group	*group;		/* System group */
  const char	*passwd;	/* Password string */
  FILE		*infile,	/* Input file */
		*outfile;	/* Output file */
  char		line[256],	/* Line from file */
		userline[17],	/* User from line */
		groupline[17],	/* Group from line */
		md5line[33],	/* MD5-sum from line */
		md5new[33];	/* New MD5 sum */
  const char	*root;		/* CUPS server root directory */
  char		passwdmd5[1024],/* passwd.md5 file */
		passwdold[1024];/* passwd.old file */


 /*
  * Find the server directory...
  */

  if ((root = getenv("CUPS_SERVERROOT")) != NULL)
  {
    snprintf(passwdmd5, sizeof(passwdmd5), "%s/passwd.md5", root);
    snprintf(passwdold, sizeof(passwdold), "%s/passwd.old", root);
  }
  else
  {
    strcpy(passwdmd5, CUPS_SERVERROOT "/passwd.md5");
    strcpy(passwdold, CUPS_SERVERROOT "/passwd.old");
  }

 /*
  * Find the default system group: "sys", "system", or "root"...
  */

  group = getgrnam("sys");
  endgrent();

  if (group != NULL)
    groupname = "sys";
  else
  {
    group = getgrnam("system");
    endgrent();

    if (group != NULL)
      groupname = "system";
    else
    {
      group = getgrnam("root");
      endgrent();

      if (group != NULL)
        groupname = "root";
      else
        groupname = "unknown";
    }
  }

  username = NULL;
  op       = CHANGE;

 /*
  * Parse command-line options...
  */

  for (i = 1; i < argc; i ++)
    if (argv[i][0] == '-')
      for (opt = argv[i] + 1; *opt; opt ++)
        switch (*opt)
	{
	  case 'a' : /* Add */
	      op = ADD;
	      break;
	  case 'x' : /* Delete */
	      op = DELETE;
	      break;
	  case 'g' : /* Group */
	      i ++;
	      if (i >= argc)
	        usage(stderr);

              groupname = argv[i];
	      break;
	  case 'h' : /* Help */
	      usage(stdout);
	      break;
	  default : /* Bad option */
	      usage(stderr);
	      break;
	}
    else if (!username)
      username = argv[i];
    else
      usage(stderr);

 /*
  * See if we are trying to add or delete a password when we aren't logged in
  * as root...
  */

  if (getuid() && (op != CHANGE || username))
  {
    fputs("lppasswd: Only root can add or delete passwords!\n", stderr);
    return (1);
  }

 /*
  * Fill in missing info...
  */

  if (!username)
    username = cupsUser();

 /*
  * Try locking the password file...
  */

  if (access(passwdold, 0) == 0)
  {
    fputs("lppasswd: Password file busy!\n", stderr);
    return (1);
  }

  if (rename(passwdmd5, passwdold))
    if (errno != ENOENT && op != ADD)
    {
      perror("lppasswd: Unable to rename password file");
      return (1);
    }

 /*
  * Open the existing password file and create a new one...
  */

  infile = fopen(passwdold, "r");
  if (infile == NULL && op != ADD)
  {
    fputs("lppasswd: No password file to add to or delete from!\n", stderr);
    return (1);
  }

  if ((outfile = fopen(passwdmd5, "w")) == NULL)
  {
    perror("lppasswd: Unable to create password file");
    rename(passwdold, passwdmd5);
    return (1);
  }

  fchmod(fileno(outfile), 0400);

 /*
  * Read lines from the password file; the format is:
  *
  *   username:group:MD5-sum
  */

  if (infile)
  {
    while (fgets(line, sizeof(line), infile) != NULL)
    {
      if (sscanf(line, "%16[^:]:%16[^:]:%32s", userline, groupline, md5line) != 3)
        continue;

      if (strcmp(username, userline) == 0 &&
          strcmp(groupname, groupline) == 0)
	break;

      fputs(line, outfile);
    }

    while (fgets(line, sizeof(line), infile) != NULL)
      fputs(line, outfile);
  }
  else
  {
    userline[0]  = '\0';
    groupline[0] = '\0';
    md5line[0]   = '\0';
  }

  if (op == CHANGE &&
      (strcmp(username, userline) != 0 ||
       strcmp(groupname, groupline) != 0))
    fprintf(stderr, "lppasswd: user \"%s\" and group \"%s\" do not exist.\n",
            username, groupname);
  else if (op != DELETE)
  {
    if (op == CHANGE && getuid())
    {
      if ((passwd  = cupsGetPassword("Enter old password:")) == NULL)
      {
       /*
	* Close the files and remove the old password file...
	*/

	fclose(infile);
	fclose(outfile);

	unlink(passwdold);

	return (0);
      }

      if (strcmp(httpMD5(username, "CUPS", passwd, md5new), md5line) != 0)
      {
	fputs("lppasswd: Sorry, password doesn't match!\n", stderr);

       /*
	* Close the files and remove the old password file...
	*/

	fclose(infile);
	fclose(outfile);

        rename(passwdold, passwdmd5);

	return (1);
      }
    }

    if ((passwd  = cupsGetPassword("Enter password:")) == NULL)
    {
     /*
      * Close the files and remove the old password file...
      */

      fclose(infile);
      fclose(outfile);

      rename(passwdold, passwdmd5);

      return (0);
    }

    strcpy(line, passwd);
      
    if ((passwd  = cupsGetPassword("Enter password again:")) == NULL)
    {
     /*
      * Close the files and remove the old password file...
      */

      fclose(infile);
      fclose(outfile);

      rename(passwdold, passwdmd5);

      return (0);
    }

    if (strcmp(passwd, line) != 0)
    {
      fputs("lppasswd: Sorry, passwords don't match!\n", stderr);

     /*
      * Close the files and remove the old password file...
      */

      fclose(infile);
      fclose(outfile);

      rename(passwdold, passwdmd5);

      return (1);
    }

   /*
    * Check that the password contains at least one letter and number.
    */

    for (passwd = line; *passwd; passwd ++)
      if (isdigit(*passwd))
        break;

    if (*passwd)
      for (passwd = line; *passwd; passwd ++)
	if (isalpha(*passwd))
          break;

   /*
    * Only allow passwords that are at least 6 chars, have a letter and
    * a number, and don't contain the username.
    */

    if (strlen(line) < 6 || strstr(line, username) != NULL || !*passwd)
    {
      fputs("lppasswd: Sorry, password must be at least 6 characters long, cannot contain\n"
            "          your username, and must contain at least one letter and number.\n", stderr);

     /*
      * Close the files and remove the old password file...
      */

      fclose(infile);
      fclose(outfile);

      rename(passwdold, passwdmd5);

      return (1);
    }

    fprintf(outfile, "%s:%s:%s\n", username, groupname,
            httpMD5(username, "CUPS", passwd, md5new));
  }

 /*
  * Close the files and remove the old password file...
  */

  fclose(infile);
  fclose(outfile);

  unlink(passwdold);

  return (0);
}


/*
 * 'usage()' - Show program usage.
 */

void
usage(FILE *fp)		/* I - File to send usage to */
{
  if (getuid())
  {
    fputs("Usage: lppasswd [-g groupname]\n", fp);
  }
  else
  {
    fputs("Usage: lppasswd [-g groupname] [username]\n", fp);
    fputs("       lppasswd [-g groupname] -a [username]\n", fp);
    fputs("       lppasswd [-g groupname] -x [username]\n", fp);
  }

  exit(1);
}


/*
 * End of "$Id: lppasswd.c,v 1.6 2000/07/17 19:44:35 mike Exp $".
 */
