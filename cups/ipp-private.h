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

typedef union _ipp_request_u		/**** Request Header ****/
{
  struct				/* Any Header */
  {
    ipp_uchar_t	version[2];		/* Protocol version number */
    int		op_status;		/* Operation ID or status code*/
    int		request_id;		/* Request ID */
  }		any;

  struct				/* Operation Header */
  {
    ipp_uchar_t	version[2];		/* Protocol version number */
    ipp_op_t	operation_id;		/* Operation ID */
    int		request_id;		/* Request ID */
  }		op;

  struct				/* Status Header */
  {
    ipp_uchar_t	version[2];		/* Protocol version number */
    ipp_status_t status_code;		/* Status code */
    int		request_id;		/* Request ID */
  }		status;

  /**** New in CUPS 1.1.19 ****/
  struct				/* Event Header @since CUPS 1.1.19/macOS 10.3@ */
  {
    ipp_uchar_t	version[2];		/* Protocol version number */
    ipp_status_t status_code;		/* Status code */
    int		request_id;		/* Request ID */
  }		event;
} _ipp_request_t;

typedef union _ipp_value_u		/**** Attribute Value ****/
{
  int		integer;		/* Integer/enumerated value */

  char		boolean;		/* Boolean value */

  ipp_uchar_t	date[11];		/* Date/time value */

  struct
  {
    int		xres,			/* Horizontal resolution */
		yres;			/* Vertical resolution */
    ipp_res_t	units;			/* Resolution units */
  }		resolution;		/* Resolution value */

  struct
  {
    int		lower,			/* Lower value */
		upper;			/* Upper value */
  }		range;			/* Range of integers value */

  struct
  {
    char	*language;		/* Language code */
    char	*text;			/* String */
  }		string;			/* String with language value */

  struct
  {
    int		length;			/* Length of attribute */
    void	*data;			/* Data in attribute */
  }		unknown;		/* Unknown attribute type */

/**** New in CUPS 1.1.19 ****/
  ipp_t		*collection;		/* Collection value @since CUPS 1.1.19/macOS 10.3@ */
} _ipp_value_t;

struct _ipp_attribute_s			/**** IPP attribute ****/
{
  ipp_attribute_t *next;		/* Next attribute in list */
  ipp_tag_t	group_tag,		/* Job/Printer/Operation group tag */
		value_tag;		/* What type of value is it? */
  char		*name;			/* Name of attribute */
  int		num_values;		/* Number of values */
  _ipp_value_t	values[1];		/* Values */
};

struct _ipp_s				/**** IPP Request/Response/Notification ****/
{
  ipp_state_t		state;		/* State of request */
  _ipp_request_t	request;	/* Request header */
  ipp_attribute_t	*attrs;		/* Attributes */
  ipp_attribute_t	*last;		/* Last attribute in list */
  ipp_attribute_t	*current;	/* Current attribute (for read/write) */
  ipp_tag_t		curtag;		/* Current attribute group tag */

/**** New in CUPS 1.2 ****/
  ipp_attribute_t	*prev;		/* Previous attribute (for read) @since CUPS 1.2/macOS 10.5@ */

/**** New in CUPS 1.4.4 ****/
  int			use;		/* Use count @since CUPS 1.4.4/macOS 10.6.?@ */
/**** New in CUPS 2.0 ****/
  int			atend,		/* At end of list? */
			curindex;	/* Current attribute index for hierarchical search */
};

typedef struct _ipp_option_s		/**** Attribute mapping data ****/
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

typedef int (*_ipp_fattr_cb_t)(_ipp_file_t *f, void *user_data, const char *attr);
					/**** File Attribute (Filter) Callback ****/
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
  _ipp_fattr_cb_t attrcb;		/* Attribute (filter) callback */
  _ipp_ferror_cb_t errorcb;		/* Error callback */
  _ipp_ftoken_cb_t tokencb;		/* Token callback */
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
extern const char	*_ippCheckOptions(void) _CUPS_PRIVATE;
#endif /* DEBUG */
extern _ipp_option_t	*_ippFindOption(const char *name) _CUPS_PRIVATE;

/* ipp-file.c */
extern ipp_t		*_ippFileParse(_ipp_vars_t *v, const char *filename, void *user_data) _CUPS_PRIVATE;
extern int		_ippFileReadToken(_ipp_file_t *f, char *token, size_t tokensize) _CUPS_PRIVATE;

/* ipp-vars.c */
extern void		_ippVarsDeinit(_ipp_vars_t *v) _CUPS_PRIVATE;
extern void		_ippVarsExpand(_ipp_vars_t *v, char *dst, const char *src, size_t dstsize) _CUPS_NONNULL(1,2,3) _CUPS_PRIVATE;
extern const char	*_ippVarsGet(_ipp_vars_t *v, const char *name) _CUPS_PRIVATE;
extern void		_ippVarsInit(_ipp_vars_t *v, _ipp_fattr_cb_t attrcb, _ipp_ferror_cb_t errorcb, _ipp_ftoken_cb_t tokencb) _CUPS_PRIVATE;
extern const char	*_ippVarsPasswordCB(const char *prompt, http_t *http, const char *method, const char *resource, void *user_data) _CUPS_PRIVATE;
extern int		_ippVarsSet(_ipp_vars_t *v, const char *name, const char *value) _CUPS_PRIVATE;


/*
 * C++ magic...
 */

#  ifdef __cplusplus
}
#  endif /* __cplusplus */
#endif /* !_CUPS_IPP_H_ */
