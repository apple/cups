/*
 * "$Id$"
 *
 *   Authentication certificate definitions for the Common UNIX
 *   Printing System (CUPS).
 *
 *   Copyright 2007-2008 by Apple Inc.
 *   Copyright 1997-2005 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 */

/*
 * Certificate structure...
 */

typedef struct cupsd_cert_s
{
  struct cupsd_cert_s *next;		/* Next certificate in list */
  int		pid;			/* Process ID (0 for root certificate) */
  char		certificate[33];	/* 32 hex characters, or 128 bits */
  char		username[33];		/* Authenticated username */
#ifdef HAVE_GSSAPI
  krb5_ccache	ccache;			/* Kerberos credential cache */
#endif /* HAVE_GSSAPI */
} cupsd_cert_t;


/*
 * Globals...
 */

VAR cupsd_cert_t	*Certs;		/* List of certificates */
VAR time_t		RootCertTime;	/* Root certificate update time */


/*
 * Prototypes...
 */

extern void		cupsdAddCert(int pid, const char *username,
			             void *ccache);
extern void		cupsdDeleteCert(int pid);
extern void		cupsdDeleteAllCerts(void);
extern const char	*cupsdFindCert(const char *certificate);
extern void		cupsdInitCerts(void);


/*
 * End of "$Id$".
 */
