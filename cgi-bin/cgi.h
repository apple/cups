/*
 * "$Id: cgi.h,v 1.1 1997/05/08 19:55:53 mike Exp $"
 *
 *   CGI support library definitions.
 *
 * Revision History:
 *
 *   $Log: cgi.h,v $
 *   Revision 1.1  1997/05/08 19:55:53  mike
 *   Initial revision
 *
 */

#ifndef _CGI_H_
#  define _CGI_H_

#  include <stdio.h>
#  include <stdlib.h>
#  include <string.h>

/*
 * Prototypes...
 */

extern int	cgiInitialize(int need_content);
extern void	cgiAbort(char *title, char *format, ...);
extern char	*cgiGetVariable(char *name);
extern void	cgiSetVariable(char *name, char *value);

extern void	cgiCopyTemplateFile(FILE *out, char *template);

extern void	cgiStartHTML(FILE *out, char *title);
extern void	cgiEndHTML(FILE *out);

#  define cgiGetUser()	getenv("REMOTE_USER")
#  define cgiGetHost()	(getenv("REMOTE_HOST") == NULL ? getenv("REMOTE_ADDR") : getenv("REMOTE_HOST"))

#endif /* !_CGI_H_ */

/*
 * End of "$Id: cgi.h,v 1.1 1997/05/08 19:55:53 mike Exp $".
 */
