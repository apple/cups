/*
 * "$Id: cgi.h,v 1.5 1999/09/10 15:44:12 mike Exp $"
 *
 *   CGI support library definitions.
 *
 *   Copyright 1997-1999 by Easy Software Products, All Rights Reserved.
 */

#ifndef _CGI_H_
#  define _CGI_H_

#  include <stdio.h>
#  include <stdlib.h>
#  include <string.h>

/*
 * Prototypes...
 */

extern int		cgiInitialize(void);
extern void		cgiAbort(const char *title, const char *stylesheet,
			         const char *format, ...);
extern int		cgiCheckVariables(const char *names);
extern const char	*cgiGetArray(const char *name, int element);
extern int		cgiGetSize(const char *name);
extern const char	*cgiGetVariable(const char *name);
extern void		cgiSetArray(const char *name, int element,
			            const char *value);
extern void		cgiSetVariable(const char *name, const char *value);
extern void		cgiCopyTemplateFile(FILE *out, const char *template);

extern void		cgiStartHTML(FILE *out, const char *author,
			             const char *stylesheet,
			             const char *keywords,
			             const char *description,
				     const char *title, ...);
extern void		cgiEndHTML(FILE *out);

extern FILE		*cgiEMailOpen(const char *from, const char *to,
			              const char *cc, const char *subject,
				      int multipart);
extern void		cgiEMailPart(FILE *out, const char *type,
			             const char *encoding);
extern void		cgiEMailClose(FILE *out);


#  define cgiGetUser()	getenv("REMOTE_USER")
#  define cgiGetHost()	(getenv("REMOTE_HOST") == NULL ? getenv("REMOTE_ADDR") : getenv("REMOTE_HOST"))

#endif /* !_CGI_H_ */

/*
 * End of "$Id: cgi.h,v 1.5 1999/09/10 15:44:12 mike Exp $".
 */
