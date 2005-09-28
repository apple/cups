/*
 * "$Id$"
 *
 *   Authentication certificate routines for the Common UNIX
 *   Printing System (CUPS).
 *
 *   Copyright 1997-2005 by Easy Software Products.
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
 *       Hollywood, Maryland 20636 USA
 *
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 *   cupsdAddCert()        - Add a certificate.
 *   cupsdDeleteCert()     - Delete a single certificate.
 *   cupsdDeleteAllCerts() - Delete all certificates...
 *   cupsdFindCert()       - Find a certificate.
 *   cupsdInitCerts()      - Initialize the certificate "system" and root
 *                      certificate.
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"


/*
 * 'cupsdAddCert()' - Add a certificate.
 */

void
cupsdAddCert(int        pid,			/* I - Process ID */
        const char *username)		/* I - Username */
{
  int		i;			/* Looping var */
  cupsd_cert_t	*cert;			/* Current certificate */
  int		fd;			/* Certificate file */
  char		filename[1024];		/* Certificate filename */
  static const char hex[] = "0123456789ABCDEF";
					/* Hex constants... */


  cupsdLogMessage(L_DEBUG2, "cupsdAddCert: adding certificate for pid %d", pid);

 /*
  * Allocate memory for the certificate...
  */

  if ((cert = calloc(sizeof(cupsd_cert_t), 1)) == NULL)
    return;

 /*
  * Fill in the certificate information...
  */

  cert->pid = pid;
  strlcpy(cert->username, username, sizeof(cert->username));

  for (i = 0; i < 32; i ++)
    cert->certificate[i] = hex[random() & 15];

 /*
  * Save the certificate to a file readable only by the User and Group
  * (or root and SystemGroup for PID == 0)...
  */

  snprintf(filename, sizeof(filename), "%s/certs/%d", StateDir, pid);
  unlink(filename);

  if ((fd = open(filename, O_WRONLY | O_CREAT | O_EXCL, 0400)) < 0)
  {
    cupsdLogMessage(L_ERROR, "cupsdAddCert: Unable to create certificate file %s - %s",
               filename, strerror(errno));
    free(cert);
    return;
  }

  if (pid == 0)
  {
   /*
    * Root certificate...
    */

    fchmod(fd, 0440);
    fchown(fd, RunUser, SystemGroupIDs[0]);

    RootCertTime = time(NULL);
  }
  else
  {
   /*
    * CGI certificate...
    */

    fchmod(fd, 0400);
    fchown(fd, User, Group);
  }

  DEBUG_printf(("ADD pid=%d, username=%s, cert=%s\n", pid, username,
                cert->certificate));

  write(fd, cert->certificate, strlen(cert->certificate));
  close(fd);

 /*
  * Insert the certificate at the front of the list...
  */

  cert->next = Certs;
  Certs      = cert;
}


/*
 * 'cupsdDeleteCert()' - Delete a single certificate.
 */

void
cupsdDeleteCert(int pid)			/* I - Process ID */
{
  cupsd_cert_t	*cert,			/* Current certificate */
		*prev;			/* Previous certificate */
  char		filename[1024];		/* Certificate file */


  for (prev = NULL, cert = Certs; cert != NULL; prev = cert, cert = cert->next)
    if (cert->pid == pid)
    {
     /*
      * Remove this certificate from the list...
      */

      cupsdLogMessage(L_DEBUG2, "cupsdDeleteCert: removing certificate for pid %d", pid);

      DEBUG_printf(("DELETE pid=%d, username=%s, cert=%s\n", cert->pid,
                    cert->username, cert->certificate));

      if (prev == NULL)
        Certs = cert->next;
      else
        prev->next = cert->next;

      free(cert);

     /*
      * Delete the file and return...
      */

      snprintf(filename, sizeof(filename), "%s/certs/%d", StateDir, pid);
      if (unlink(filename))
	cupsdLogMessage(L_ERROR, "cupsdDeleteCert: Unable to remove %s!\n", filename);

      return;
    }
}


/*
 * 'cupsdDeleteAllCerts()' - Delete all certificates...
 */

void
cupsdDeleteAllCerts(void)
{
  cupsd_cert_t	*cert,			/* Current certificate */
		*next;			/* Next certificate */
  char		filename[1024];		/* Certificate file */


 /*
  * Loop through each certificate, deleting them...
  */

  for (cert = Certs; cert != NULL; cert = next)
  {
   /*
    * Delete the file...
    */

    snprintf(filename, sizeof(filename), "%s/certs/%d", StateDir, cert->pid);
    if (unlink(filename))
      cupsdLogMessage(L_ERROR, "cupsdDeleteAllCerts: Unable to remove %s!\n", filename);

   /*
    * Free memory...
    */

    next = cert->next;
    free(cert);
  }

  Certs = NULL;
}


/*
 * 'cupsdFindCert()' - Find a certificate.
 */

const char *				/* O - Matching username or NULL */
cupsdFindCert(const char *certificate)	/* I - Certificate */
{
  cupsd_cert_t	*cert;			/* Current certificate */


  DEBUG_printf(("cupsdFindCert(certificate=%s)\n", certificate));
  for (cert = Certs; cert != NULL; cert = cert->next)
    if (strcasecmp(certificate, cert->certificate) == 0)
    {
      DEBUG_printf(("    returning %s...\n", cert->username));
      return (cert->username);
    }

  DEBUG_puts("    certificate not found!");

  return (NULL);
}


/*
 * 'cupsdInitCerts()' - Initialize the certificate "system" and root certificate.
 */

void
cupsdInitCerts(void)
{
  cups_file_t	*fp;			/* /dev/random file */
  unsigned	seed;			/* Seed for random number generator */
  struct timeval tod;			/* Time of day */


 /*
  * Initialize the random number generator using the random device or
  * the current time, as available...
  */

  if ((fp = cupsFileOpen("/dev/urandom", "rb")) == NULL)
  {
   /*
    * Get the time in usecs and use it as the initial seed...
    */

    gettimeofday(&tod, NULL);

    seed = (unsigned)(tod.tv_sec + tod.tv_usec);
  }
  else
  {
   /*
    * Read 4 random characters from the random device and use
    * them as the seed...
    */

    seed = cupsFileGetChar(fp);
    seed = (seed << 8) | cupsFileGetChar(fp);
    seed = (seed << 8) | cupsFileGetChar(fp);
    seed = (seed << 8) | cupsFileGetChar(fp);

    cupsFileClose(fp);
  }

  srandom(seed);

 /*
  * Create a root certificate and return...
  */

  if (!RunUser)
    cupsdAddCert(0, "root");
}


/*
 * End of "$Id$".
 */
