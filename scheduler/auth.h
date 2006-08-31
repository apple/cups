/*
 * "$Id$"
 *
 *   Authorization definitions for the Common UNIX Printing System (CUPS)
 *   scheduler.
 *
 *   Copyright 1997-2006 by Easy Software Products, all rights reserved.
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

/*
 * Include necessary headers...
 */

#include <pwd.h>


/*
 * HTTP authorization types and levels...
 */

#define AUTH_NONE		0	/* No authentication */
#define AUTH_BASIC		1	/* Basic authentication */
#define AUTH_DIGEST		2	/* Digest authentication */
#define AUTH_BASICDIGEST	3	/* Basic authentication w/passwd.md5 */
#define AUTH_KERBEROS		4	/* Kerberos authentication */

#define AUTH_ANON		0	/* Anonymous access */
#define AUTH_USER		1	/* Must have a valid username/password */
#define AUTH_GROUP		2	/* Must also be in a named group */

#define AUTH_ALLOW		0	/* Allow access */
#define AUTH_DENY		1	/* Deny access */

#define AUTH_NAME		0	/* Authorize host by name */
#define AUTH_IP			1	/* Authorize host by IP */
#define AUTH_INTERFACE		2	/* Authorize host by interface */

#define AUTH_SATISFY_ALL	0	/* Satisfy both address and auth */
#define AUTH_SATISFY_ANY	1	/* Satisfy either address or auth */

#define AUTH_LIMIT_DELETE	1	/* Limit DELETE requests */
#define AUTH_LIMIT_GET		2	/* Limit GET requests */
#define AUTH_LIMIT_HEAD		4	/* Limit HEAD requests */
#define AUTH_LIMIT_OPTIONS	8	/* Limit OPTIONS requests */
#define AUTH_LIMIT_POST		16	/* Limit POST requests */
#define AUTH_LIMIT_PUT		32	/* Limit PUT requests */
#define AUTH_LIMIT_TRACE	64	/* Limit TRACE requests */
#define AUTH_LIMIT_ALL		127	/* Limit all requests */
#define AUTH_LIMIT_IPP		128	/* Limit IPP requests */

#define IPP_ANY_OPERATION	(ipp_op_t)0
					/* Any IPP operation */
#define IPP_BAD_OPERATION	(ipp_op_t)-1
					/* No IPP operation */


/*
 * HTTP access control structures...
 */

typedef struct
{
  unsigned	address[4],		/* IP address */
		netmask[4];		/* IP netmask */
} cupsd_ipmask_t;

typedef struct
{
  int		length;			/* Length of name */
  char		*name;			/* Name string */
} cupsd_namemask_t;

typedef struct
{
  int		type;			/* Mask type */
  union
  {
    cupsd_namemask_t	name;		/* Host/Domain name */
    cupsd_ipmask_t	ip;		/* IP address/network */
  }		mask;			/* Mask data */
} cupsd_authmask_t;

typedef struct
{
  char			*location;	/* Location of resource */
  ipp_op_t		op;		/* IPP operation */
  int			limit,		/* Limit for these types of requests */
			length,		/* Length of location string */
			order_type,	/* Allow or Deny */
			type,		/* Type of authentication */
			level,		/* Access level required */
			satisfy;	/* Satisfy any or all limits? */
  int			num_names;	/* Number of names */
  char			**names;	/* User or group names */
  int			num_allow;	/* Number of Allow lines */
  cupsd_authmask_t	*allow;		/* Allow lines */
  int			num_deny;	/* Number of Deny lines */
  cupsd_authmask_t	*deny;		/* Deny lines */
  http_encryption_t	encryption;	/* To encrypt or not to encrypt... */
} cupsd_location_t;

typedef struct cupsd_client_s cupsd_client_t;


/*
 * Globals...
 */

VAR cups_array_t	*Locations	VALUE(NULL);
					/* Authorization locations */
VAR int			DefaultAuthType	VALUE(AUTH_BASIC);
					/* Default AuthType, if not specified */
#ifdef HAVE_SSL
VAR http_encryption_t	DefaultEncryption VALUE(HTTP_ENCRYPT_REQUIRED);
					/* Default encryption for authentication */
#endif /* HAVE_SSL */


/*
 * Prototypes...
 */

extern cupsd_location_t	*cupsdAddLocation(const char *location);
extern void		cupsdAddName(cupsd_location_t *loc, char *name);
extern void		cupsdAllowHost(cupsd_location_t *loc, char *name);
extern void		cupsdAllowIP(cupsd_location_t *loc, unsigned address[4],
			             unsigned netmask[4]);
extern void		cupsdAuthorize(cupsd_client_t *con);
extern int		cupsdCheckAuth(unsigned ip[4], char *name, int namelen,
				       int num_masks, cupsd_authmask_t *masks);
extern int		cupsdCheckGroup(const char *username,
			                struct passwd *user,
			                const char *groupname);
extern cupsd_location_t	*cupsdCopyLocation(cupsd_location_t **loc);
extern void		cupsdDeleteAllLocations(void);
extern void		cupsdDeleteLocation(cupsd_location_t *loc);
extern void		cupsdDenyHost(cupsd_location_t *loc, char *name);
extern void		cupsdDenyIP(cupsd_location_t *loc, unsigned address[4],
			            unsigned netmask[4]);
extern cupsd_location_t	*cupsdFindBest(const char *path, http_state_t state);
extern cupsd_location_t	*cupsdFindLocation(const char *location);
extern http_status_t	cupsdIsAuthorized(cupsd_client_t *con, const char *owner);


/*
 * End of "$Id$".
 */
