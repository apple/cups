/*
 * "$Id: auth.h,v 1.16.2.2 2001/12/26 16:52:50 mike Exp $"
 *
 *   Authorization definitions for the Common UNIX Printing System (CUPS)
 *   scheduler.
 *
 *   Copyright 1997-2001 by Easy Software Products, all rights reserved.
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
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 */

/*
 * HTTP authorization types and levels...
 */

#define AUTH_NONE		0	/* No authentication */
#define AUTH_BASIC		1	/* Basic authentication */
#define AUTH_DIGEST		2	/* Digest authentication */
#define AUTH_BASICDIGEST	3	/* Basic authentication w/passwd.md5 */

#define AUTH_ANON		0	/* Anonymous access */
#define AUTH_USER		1	/* Must have a valid username/password */
#define AUTH_GROUP		2	/* Must also be in a named group */

#define AUTH_ALLOW		0	/* Allow access */
#define AUTH_DENY		1	/* Deny access */

#define AUTH_NAME		0	/* Authorize host by name */
#define AUTH_IP			1	/* Authorize host by IP */

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


/*
 * HTTP access control structures...
 */

typedef struct
{
  unsigned	address[4],		/* IP address */
		netmask[4];		/* IP netmask */
} ipmask_t;

typedef struct
{
  int		length;			/* Length of name */
  char		*name;			/* Name string */
} namemask_t;

typedef struct
{
  int		type;			/* Mask type */
  union
  {
    namemask_t	name;			/* Host/Domain name */
    ipmask_t	ip;			/* IP address/network */
  }		mask;			/* Mask data */
} authmask_t;

typedef struct
{
  char		location[HTTP_MAX_URI];	/* Location of resource */
  int		limit,			/* Limit for these types of requests */
		length,			/* Length of location string */
		order_type,		/* Allow or Deny */
		type,			/* Type of authentication */
		level,			/* Access level required */
		satisfy;		/* Satisfy any or all limits? */
  int		num_names;		/* Number of names */
  char		**names;		/* User or group names */
  int		num_allow;		/* Number of Allow lines */
  authmask_t	*allow;			/* Allow lines */
  int		num_deny;		/* Number of Deny lines */
  authmask_t	*deny;			/* Deny lines */
  http_encryption_t encryption;		/* To encrypt or not to encrypt... */
} location_t;


/*
 * Globals...
 */

VAR int			NumLocations	VALUE(0);
					/* Number of authorization locations */
VAR location_t		*Locations	VALUE(NULL);
					/* Authorization locations */


/*
 * Prototypes...
 */

extern location_t	*AddLocation(const char *location);
extern void		AddName(location_t *loc, char *name);
extern void		AllowHost(location_t *loc, char *name);
extern void		AllowIP(location_t *loc, unsigned address[4],
			        unsigned netmask[4]);
extern int		CheckAuth(unsigned ip[4], char *name, int namelen,
				  int num_masks, authmask_t *masks);
extern location_t	*CopyLocation(location_t **loc);
extern void		DeleteAllLocations(void);
extern void		DenyHost(location_t *loc, char *name);
extern void		DenyIP(location_t *loc, unsigned address[4],
			       unsigned netmask[4]);
extern location_t	*FindBest(client_t *con, http_state_t state);
extern location_t	*FindLocation(const char *location);
extern http_status_t	IsAuthorized(client_t *con);


/*
 * End of "$Id: auth.h,v 1.16.2.2 2001/12/26 16:52:50 mike Exp $".
 */
