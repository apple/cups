/*
 * "$Id: lppasswd.c,v 1.11 2001/02/14 16:06:43 mike Exp $"
 *
 *   MD5 password program for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2001 by Easy Software Products.
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
 *   main()    - Add, change, or delete passwords from the MD5 password file.
 *   usage()   - Show program usage.
 *   xstrdup() - strdup() function with NULL checking...
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
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

static void	usage(FILE *fp);
static char	*xstrdup(const char *);


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
		passwdold[1024],/* passwd.old file */
		passwdnew[1024];/* passwd.tmp file */
  char		*newpass,	/* new password */
  		*oldpass;	/* old password */
  int		flag;		/* Password check flags... */
  int		fd;		/* Password file descriptor */


 /*
  * Find the server directory...
  *
  * Don't use CUPS_SERVERROOT unless we're run by the
  * super user.
  */

  if (!getuid() && (root = getenv("CUPS_SERVERROOT")) != NULL)
  {
    snprintf(passwdmd5, sizeof(passwdmd5), "%s/passwd.md5", root);
    snprintf(passwdold, sizeof(passwdold), "%s/passwd.old", root);
    snprintf(passwdnew, sizeof(passwdnew), "%s/passwd.new", root);
  }
  else
  {
    strcpy(passwdmd5, CUPS_SERVERROOT "/passwd.md5");
    strcpy(passwdold, CUPS_SERVERROOT "/passwd.old");
    strcpy(passwdnew, CUPS_SERVERROOT "/passwd.new");
  }

 /*
  * Find the default system group: "sys", "system", or "root"...
  */

  if (getgrnam("sys"))
    groupname = "sys";
  else if (getgrnam("system"))
    groupname = "system";
  else if (getgrnam("root"))
    groupname = "root";
  else
    groupname = "unknown";

  endgrent();

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

  oldpass = newpass = NULL;

 /*
  * Obtain old and new password _before_ locking the database
  * to keep users from locking the file indefinitely.
  */

  if (op == CHANGE && getuid())
  {
    if ((passwd = cupsGetPassword("Enter old password:")) == NULL)
      return (1);

    oldpass = xstrdup(passwd);
  }

 /*
  * Now get the new password
  */

  if ((passwd = cupsGetPassword("Enter password:")) == NULL)
    return (1);

  newpass = xstrdup(passwd);

  if ((passwd = cupsGetPassword("Enter password again:")) == NULL)
    return (1);

  if (strcmp(passwd, newpass) != 0)
  {
    fputs("lppasswd: Sorry, passwords don't match!\n", stderr);
    return (1);
  }

 /*
  * Check that the password contains at least one letter and number.
  */

  flag = 0;

  for (passwd = newpass; *passwd; passwd ++)
    if (isdigit(*passwd))
      flag |= 1;
    else if (isalpha(*passwd))
      flag |= 2;

 /*
  * Only allow passwords that are at least 6 chars, have a letter and
  * a number, and don't contain the username.
  */

  if (strlen(newpass) < 6 || strstr(newpass, username) != NULL || flag != 3)
  {
    fputs("lppasswd: Sorry, password rejected.\n"
	  "Your password must be at least 6 characters long, cannot contain\n"
	  "your username, and must contain at least one letter and number.\n",
	  stderr);
    return (1);
  }

  outfile = infile = NULL;

  /*
   * Open the output file.
   */

  if ((fd = open(passwdnew, O_WRONLY|O_CREAT|O_EXCL, 0400)) < 0)
  {
    if (errno == EEXIST)
      fputs("lppasswd: Password file busy!\n", stderr);
    else
      perror("lppasswd: Unable to open passwd file");

    return (1);
  }

  if ((outfile = fdopen(fd, "w")) == NULL)
  {
    perror("lppasswd: Unable to open passwd file");
    goto fail_out;
  }

 /*
  * Open the existing password file and create a new one...
  */

  infile = fopen(passwdmd5, "r");
  if (infile == NULL && errno != ENOENT && op != ADD)
  {
    fputs("lppasswd: No password file to add to or delete from!\n", stderr);
    goto fail_out;
  }

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
    if (oldpass &&
        strcmp(httpMD5(username, "CUPS", oldpass, md5new), md5line) != 0)
    {
      fputs("lppasswd: Sorry, password doesn't match!\n", stderr);
      goto fail_out;
    }

    fprintf(outfile, "%s:%s:%s\n", username, groupname,
            httpMD5(username, "CUPS", newpass, md5new));
  }

 /*
  * Close the files and remove the old password file...
  */

  if (infile)
    fclose(infile);

  fclose(outfile);

 /*
  * Save old passwd file
  */

  unlink(passwdold);
  link(passwdmd5, passwdold);

 /*
  * Install new password file
  */

  if (rename(passwdnew, passwdmd5) < 0)
  {
    perror("lppasswd: failed to rename passwd file");
    unlink(passwdnew);
    return (1);
  }

  return (0);

 /*
  * This is where all errors die...
  */

fail_out:

  if (infile)
    fclose(infile);

  if (outfile)
    fclose(outfile);

  unlink(passwdnew);

  return (1);
}


/*
 * 'usage()' - Show program usage.
 */

static void
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
 * 'xstrdup()' - strdup() function with NULL checking...
 */

static char *			/* O - New string */
xstrdup(const char *in)		/* I - String to duplicate */
{
  char	*out;			/* New string */


  if (in == NULL)
    return (NULL);

  if ((out = strdup(in)) == NULL)
  {
    perror("lppasswd: Out of memory!");
    exit(1);
  }

  return (out);
}


/*
 * End of "$Id: lppasswd.c,v 1.11 2001/02/14 16:06:43 mike Exp $".
 */
