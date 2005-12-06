/*
 * "$Id$"
 *
 *   CGI support library definitions.
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
 */

#ifndef _CUPS_CGI_H_
#  define _CUPS_CGI_H_

#  include <stdio.h>
#  include <stdlib.h>
#  include <time.h>
#  include <sys/stat.h>

#  ifdef WIN32
#    include <direct.h>
#    include <io.h>
#  else
#    include <unistd.h>
#  endif /* WIN32 */

#  include <cups/cups.h>
#  include "help-index.h"


/*
 * Types...
 */

typedef struct cgi_file_s		/**** Uploaded file data ****/
{
  char		tempfile[1024],		/* Temporary file containing data */
		*name,			/* Variable name */
		*filename,		/* Original filename */
		*mimetype;		/* MIME media type */
  size_t	filesize;		/* Size of uploaded file */
} cgi_file_t;


/*
 * Prototypes...
 */

extern void		cgiAbort(const char *title, const char *stylesheet,
			         const char *format, ...);
extern int		cgiCheckVariables(const char *names);
extern void		*cgiCompileSearch(const char *query);
extern void		cgiCopyTemplateFile(FILE *out, const char *tmpl);
extern void		cgiCopyTemplateLang(FILE *out, const char *directory,
			                    const char *tmpl, const char *lang);
extern int		cgiDoSearch(void *search, const char *text);
extern void		cgiEndHTML(void);
extern char		*cgiFormEncode(char *dst, const char *src, size_t dstsize);
extern void		cgiFreeSearch(void *search);
extern const char	*cgiGetArray(const char *name, int element);
extern void		cgiGetAttributes(ipp_t *request, const char *directory,
			                 const char *tmpl, const char *lang);
extern char		*cgiGetCookie(const char *name, char *buf, int buflen);
extern const cgi_file_t	*cgiGetFile(void);
#  define cgiGetHost()	(getenv("REMOTE_HOST") == NULL ? getenv("REMOTE_ADDR") : getenv("REMOTE_HOST"))
extern int		cgiGetSize(const char *name);
#  define cgiGetUser()	getenv("REMOTE_USER")
extern char		*cgiGetTemplateDir(void);
extern const char	*cgiGetVariable(const char *name);
extern int		cgiInitialize(void);
extern int		cgiIsPOST(void);
extern char		*cgiRewriteURL(const char *uri, char *url, int urlsize,
			               const char *newresource);
extern void		cgiSetArray(const char *name, int element,
			            const char *value);
extern void		cgiSetCookie(const char *name, const char *value,
			             const char *path, const char *domain,
				     time_t expires, int secure);
extern int		cgiSetIPPVars(ipp_t *, const char *, const char *,
			              const char *, int);
extern void		cgiSetServerVersion(void);
extern void		cgiSetSize(const char *name, int size);
extern void		cgiSetVariable(const char *name, const char *value);
extern void		cgiStartHTML(const char *title);


#endif /* !_CUPS_CGI_H_ */

/*
 * End of "$Id$".
 */
