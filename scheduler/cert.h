/*
 * Authentication certificate definitions for the CUPS scheduler.
 *
 * Copyright 2007-2012 by Apple Inc.
 * Copyright 1997-2005 by Easy Software Products.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more information.
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
  int		type;			/* AuthType for username */
} cupsd_cert_t;


/*
 * Globals...
 */

VAR cupsd_cert_t	*Certs		/* List of certificates */
				VALUE(NULL);
VAR time_t		RootCertTime	/* Root certificate update time */
				VALUE(0);


/*
 * Prototypes...
 */

extern void		cupsdAddCert(int pid, const char *username, int type);
extern void		cupsdDeleteCert(int pid);
extern void		cupsdDeleteAllCerts(void);
extern cupsd_cert_t	*cupsdFindCert(const char *certificate);
extern void		cupsdInitCerts(void);
