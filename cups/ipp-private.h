/*
 * Private IPP definitions for CUPS.
 *
 * Copyright © 2007-2018 by Apple Inc.
 * Copyright © 1997-2006 by Easy Software Products.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more
 * information.
 */

#ifndef _CUPS_IPP_PRIVATE_H_
#  define _CUPS_IPP_PRIVATE_H_

/*
 * Include necessary headers...
 */

#  include <cups/cups.h>


/*
 * C++ magic...
 */

#  ifdef __cplusplus
extern "C" {
#  endif /* __cplusplus */


/*
 * Constants...
 */

#  define IPP_BUF_SIZE	(IPP_MAX_LENGTH + 2)
					/* Size of buffer */


/*
 * Structures...
 */

typedef struct				/**** Attribute mapping data ****/
{
  int		multivalue;		/* Option has multiple values? */
  const char	*name;			/* Option/attribute name */
  ipp_tag_t	value_tag;		/* Value tag for this attribute */
  ipp_tag_t	group_tag;		/* Group tag for this attribute */
  ipp_tag_t	alt_group_tag;		/* Alternate group tag for this
					 * attribute */
  const ipp_op_t *operations;		/* Allowed operations for this attr */
} _ipp_option_t;

typedef struct _ipp_file_s _ipp_file_t;/**** File Parser ****/
typedef struct _ipp_vars_s _ipp_vars_t;/**** Variables ****/

typedef int (*_ipp_ferror_cb_t)(_ipp_file_t *f, void *user_data, const char *error);
					/**** File Parser Error Callback ****/
typedef int (*_ipp_ftoken_cb_t)(_ipp_file_t *f, _ipp_vars_t *v, void *user_data, const char *token);
					/**** File Parser Token Callback ****/

struct _ipp_vars_s			/**** Variables ****/
{
  char		*uri,			/* URI for printer */
		scheme[64],		/* Scheme from URI */
		username[256],		/* Username from URI */
		*password,		/* Password from URI (if any) */
		host[256],		/* Hostname from URI */
		portstr[32],		/* Port number string */
		resource[1024];		/* Resource path from URI */
  int 		port;			/* Port number from URI */
  int		num_vars;		/* Number of variables */
  cups_option_t	*vars;			/* Array of variables */
  int		password_tries;		/* Number of retries for password */
};

struct _ipp_file_s			/**** File Parser */
{
  const char		*filename;	/* Filename */
  cups_file_t		*fp;		/* File pointer */
  int			linenum;	/* Current line number */
  ipp_t			*attrs;		/* Attributes */
  ipp_tag_t		group_tag;	/* Current group for new attributes */
};


/*
 * Prototypes for private functions...
 */

/* encode.c */
#ifdef DEBUG
extern const char	*_ippCheckOptions(void);
#endif /* DEBUG */
extern _ipp_option_t	*_ippFindOption(const char *name);

/* ipp-file.c */
extern ipp_t		*_ippFileParse(_ipp_vars_t *v, const char *filename, _ipp_ftoken_cb_t tokencb, _ipp_ferror_cb_t errorcb, void *user_data);
extern int		_ippFileReadToken(_ipp_file_t *f, char *token, size_t tokensize);

/* ipp-vars.c */
extern void		_ippVarsDeinit(_ipp_vars_t *v);
extern void		_ippVarsExpand(_ipp_vars_t *v, char *dst, const char *src, size_t dstsize) __attribute__((nonnull(1,2,3)));
extern const char	*_ippVarsGet(_ipp_vars_t *v, const char *name);
extern void		_ippVarsInit(_ipp_vars_t *v);
extern const char	*_ippVarsPasswordCB(const char *prompt, http_t *http, const char *method, const char *resource, void *user_data);
extern int		_ippVarsSet(_ipp_vars_t *v, const char *name, const char *value);


/*
 * C++ magic...
 */

#  ifdef __cplusplus
}
#  endif /* __cplusplus */
#endif /* !_CUPS_IPP_H_ */
