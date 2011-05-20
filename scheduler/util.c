/*
 * "$Id: util.c 7621 2008-06-06 18:55:35Z mike $"
 *
 *   Mini-daemon utility functions for CUPS.
 *
 *   Copyright 2007-2011 by Apple Inc.
 *   Copyright 1997-2005 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
 *   cupsdCompareNames()       - Compare two names.
 *   cupsdCreateStringsArray() - Create a CUPS array of strings.
 *   cupsdExec()               - Run a program with the correct environment.
 *   cupsdPipeCommand()        - Read output from a command.
 *   cupsdSendIPPGroup()       - Send a group tag.
 *   cupsdSendIPPHeader()      - Send the IPP response header.
 *   cupsdSendIPPInteger()     - Send an integer attribute.
 *   cupsdSendIPPString()      - Send a string attribute.
 *   cupsdSendIPPTrailer()     - Send the end-of-message tag.
 */

/*
 * Include necessary headers...
 */

#include "util.h"
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef __APPLE__
#  include <libgen.h>
extern char **environ;
#endif /* __APPLE__ */


/*
 * 'cupsdCompareNames()' - Compare two names.
 *
 * This function basically does a _cups_strcasecmp() of the two strings,
 * but is also aware of numbers so that "a2" < "a100".
 */

int					/* O - Result of comparison */
cupsdCompareNames(const char *s,	/* I - First string */
                  const char *t)	/* I - Second string */
{
  int		diff,			/* Difference between digits */
		digits;			/* Number of digits */


 /*
  * Loop through both names, returning only when a difference is
  * seen.  Also, compare whole numbers rather than just characters, too!
  */

  while (*s && *t)
  {
    if (isdigit(*s & 255) && isdigit(*t & 255))
    {
     /*
      * Got a number; start by skipping leading 0's...
      */

      while (*s == '0')
        s ++;
      while (*t == '0')
        t ++;

     /*
      * Skip equal digits...
      */

      while (isdigit(*s & 255) && *s == *t)
      {
        s ++;
	t ++;
      }

     /*
      * Bounce out if *s and *t aren't both digits...
      */

      if (isdigit(*s & 255) && !isdigit(*t & 255))
        return (1);
      else if (!isdigit(*s & 255) && isdigit(*t & 255))
        return (-1);
      else if (!isdigit(*s & 255) || !isdigit(*t & 255))
        continue;

      if (*s < *t)
        diff = -1;
      else
        diff = 1;

     /*
      * Figure out how many more digits there are...
      */

      digits = 0;
      s ++;
      t ++;

      while (isdigit(*s & 255))
      {
        digits ++;
	s ++;
      }

      while (isdigit(*t & 255))
      {
        digits --;
	t ++;
      }

     /*
      * Return if the number or value of the digits is different...
      */

      if (digits < 0)
        return (-1);
      else if (digits > 0)
        return (1);
      else if (diff)
        return (diff);
    }
    else if (tolower(*s) < tolower(*t))
      return (-1);
    else if (tolower(*s) > tolower(*t))
      return (1);
    else
    {
      s ++;
      t ++;
    }
  }

 /*
  * Return the results of the final comparison...
  */

  if (*s)
    return (1);
  else if (*t)
    return (-1);
  else
    return (0);
}


/*
 * 'cupsdCreateStringsArray()' - Create a CUPS array of strings.
 */

cups_array_t *				/* O - CUPS array */
cupsdCreateStringsArray(const char *s)	/* I - Comma-delimited strings */
{
  if (!s || !*s)
    return (NULL);
  else
    return (_cupsArrayNewStrings(s));
}


/*
 * 'cupsdExec()' - Run a program with the correct environment.
 *
 * On Mac OS X, we need to update the CFProcessPath environment variable that
 * is passed in the environment so the child can access its bundled resources.
 */

int					/* O - exec() status */
cupsdExec(const char *command,		/* I - Full path to program */
          char       **argv)		/* I - Command-line arguments */
{
#ifdef __APPLE__
  int	i, j;				/* Looping vars */
  char	*envp[500],			/* Array of environment variables */
	cfprocesspath[1024],		/* CFProcessPath environment variable */
	linkpath[1024];			/* Link path for symlinks... */
  int	linkbytes;			/* Bytes for link path */


 /*
  * Some Mac OS X programs are bundled and need the CFProcessPath environment
  * variable defined.  If the command is a symlink, resolve the link and point
  * to the resolved location, otherwise, use the command path itself.
  */

  if ((linkbytes = readlink(command, linkpath, sizeof(linkpath) - 1)) > 0)
  {
   /*
    * Yes, this is a symlink to the actual program, nul-terminate and
    * use it...
    */

    linkpath[linkbytes] = '\0';

    if (linkpath[0] == '/')
      snprintf(cfprocesspath, sizeof(cfprocesspath), "CFProcessPath=%s",
	       linkpath);
    else
      snprintf(cfprocesspath, sizeof(cfprocesspath), "CFProcessPath=%s/%s",
	       dirname((char *)command), linkpath);
  }
  else
    snprintf(cfprocesspath, sizeof(cfprocesspath), "CFProcessPath=%s", command);

  envp[0] = cfprocesspath;

 /*
  * Copy the rest of the environment except for any CFProcessPath that may
  * already be there...
  */

  for (i = 1, j = 0;
       environ[j] && i < (int)(sizeof(envp) / sizeof(envp[0]) - 1);
       j ++)
    if (strncmp(environ[j], "CFProcessPath=", 14))
      envp[i ++] = environ[j];

  envp[i] = NULL;

 /*
  * Use execve() to run the program...
  */

  return (execve(command, argv, envp));

#else
 /*
  * On other operating systems, just call execv() to use the same environment
  * variables as the parent...
  */

  return (execv(command, argv));
#endif /* __APPLE__ */
}


/*
 * 'cupsdPipeCommand()' - Read output from a command.
 */

cups_file_t *				/* O - CUPS file or NULL on error */
cupsdPipeCommand(int        *pid,	/* O - Process ID or 0 on error */
                 const char *command,	/* I - Command to run */
                 char       **argv,	/* I - Arguments to pass to command */
		 int        user)	/* I - User to run as or 0 for current */
{
  int	fd,				/* Temporary file descriptor */
	fds[2];				/* Pipe file descriptors */


 /*
  * First create the pipe...
  */

  if (pipe(fds))
  {
    *pid = 0;
    return (NULL);
  }

 /*
  * Set the "close on exec" flag on each end of the pipe...
  */

  if (fcntl(fds[0], F_SETFD, fcntl(fds[0], F_GETFD) | FD_CLOEXEC))
  {
    close(fds[0]);
    close(fds[1]);

    *pid = 0;

    return (NULL);
  }

  if (fcntl(fds[1], F_SETFD, fcntl(fds[1], F_GETFD) | FD_CLOEXEC))
  {
    close(fds[0]);
    close(fds[1]);

    *pid = 0;

    return (NULL);
  }

 /*
  * Then run the command...
  */

  if ((*pid = fork()) < 0)
  {
   /*
    * Unable to fork!
    */

    *pid = 0;
    close(fds[0]);
    close(fds[1]);

    return (NULL);
  }
  else if (!*pid)
  {
   /*
    * Child comes here...
    */

    if (!getuid() && user)
      setuid(user);			/* Run as restricted user */

    if ((fd = open("/dev/null", O_RDONLY)) > 0)
    {
      dup2(fd, 0);			/* </dev/null */
      close(fd);
    }

    dup2(fds[1], 1);			/* >pipe */
    close(fds[1]);

    cupsdExec(command, argv);
    exit(errno);
  }

 /*
  * Parent comes here, open the input side of the pipe...
  */

  close(fds[1]);

  return (cupsFileOpenFd(fds[0], "r"));
}


/*
 * 'cupsdSendIPPGroup()' - Send a group tag.
 */

void
cupsdSendIPPGroup(ipp_tag_t group_tag)	/* I - Group tag */
{
 /*
  * Send IPP group tag (1 byte)...
  */

  putchar(group_tag);
}


/*
 * 'cupsdSendIPPHeader()' - Send the IPP response header.
 */

void
cupsdSendIPPHeader(
    ipp_status_t status_code,		/* I - Status code */
    int          request_id)		/* I - Request ID */
{
 /*
  * Send IPP/1.1 response header: version number (2 bytes), status code
  * (2 bytes), and request ID (4 bytes)...
  *
  * TODO: Add version number (IPP/2.x and IPP/1.0) support.
  */

  putchar(1);
  putchar(1);

  putchar(status_code >> 8);
  putchar(status_code);

  putchar(request_id >> 24);
  putchar(request_id >> 16);
  putchar(request_id >> 8);
  putchar(request_id);
}


/*
 * 'cupsdSendIPPInteger()' - Send an integer attribute.
 */

void
cupsdSendIPPInteger(
    ipp_tag_t  value_tag,		/* I - Value tag */
    const char *name,			/* I - Attribute name */
    int        value)			/* I - Attribute value */
{
  size_t	len;			/* Length of attribute name */


 /*
  * Send IPP integer value: value tag (1 byte), name length (2 bytes),
  * name string (without nul), value length (2 bytes), and value (4 bytes)...
  */

  putchar(value_tag);

  len = strlen(name);
  putchar(len >> 8);
  putchar(len);

  fputs(name, stdout);

  putchar(0);
  putchar(4);

  putchar(value >> 24);
  putchar(value >> 16);
  putchar(value >> 8);
  putchar(value);
}


/*
 * 'cupsdSendIPPString()' - Send a string attribute.
 */

void
cupsdSendIPPString(
    ipp_tag_t  value_tag,		/* I - Value tag */
    const char *name,			/* I - Attribute name */
    const char *value)			/* I - Attribute value */
{
  size_t	len;			/* Length of attribute name */


 /*
  * Send IPP string value: value tag (1 byte), name length (2 bytes),
  * name string (without nul), value length (2 bytes), and value string
  * (without nul)...
  */

  putchar(value_tag);

  len = strlen(name);
  putchar(len >> 8);
  putchar(len);

  fputs(name, stdout);

  len = strlen(value);
  putchar(len >> 8);
  putchar(len);

  fputs(value, stdout);
}


/*
 * 'cupsdSendIPPTrailer()' - Send the end-of-message tag.
 */

void
cupsdSendIPPTrailer(void)
{
  putchar(IPP_TAG_END);
  fflush(stdout);
}


/*
 * End of "$Id: util.c 7621 2008-06-06 18:55:35Z mike $".
 */
