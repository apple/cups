/*
 * "$Id: cert.h,v 1.3.2.3 2004/06/29 13:15:10 mike Exp $"
 *
 *   Authentication certificate definitions for the Common UNIX
 *   Printing System (CUPS).
 *
 *   Copyright 1997-2004 by Easy Software Products.
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
 *       Hollywood, Maryland 20636-3142 USA
 *
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 */

/*
 * Certificate structure...
 */

typedef struct cert_str
{
  struct cert_str *next;	/* Next certificate in list */
  int		pid;		/* Process ID (0 for root certificate) */
  char		certificate[33];/* 32 hex characters, or 128 bits */
  char		username[33];	/* Authenticated username */
} cert_t;


/*
 * Globals...
 */

VAR cert_t	*Certs;		/* List of certificates */
VAR time_t	RootCertTime;	/* Root certificate update time */


/*
 * Prototypes...
 */

extern void		AddCert(int pid, const char *username);
extern void		DeleteCert(int pid);
extern void		DeleteAllCerts(void);
extern const char	*FindCert(const char *certificate);
extern void		InitCerts(void);


/*
 * End of "$Id: cert.h,v 1.3.2.3 2004/06/29 13:15:10 mike Exp $".
 */
