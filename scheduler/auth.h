/*
 * "$Id: auth.h,v 1.2 1998/10/16 18:28:01 mike Exp $"
 *
 *   Authorization definitions for the Common UNIX Printing System (CUPS)
 *   scheduler.
 *
 *   Copyright 1997-1998 by Easy Software Products, all rights reserved.
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
 *       44145 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 */

/*
 * HTTP authorization types and levels...
 */

#define AUTH_ANON		0	/* Anonymous access */
#define AUTH_USER		1	/* Must have a valid username/password */
#define AUTH_GROUP		2	/* Must also be in a named group */

#define AUTH_ALLOW		0	/* Allow access */
#define AUTH_DENY		1	/* Deny access */

#define AUTH_NAME		0	/* Authorize host by name */
#define AUTH_IP			1	/* Authorize host by IP */


/*
 * HTTP authorization structures...
 */

typedef struct
{
  unsigned	address,		/* IP address */
		netmask;		/* IP netmask */
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
} mask_t;

typedef struct
{
  char		location[1024];		/* Location of resource */
  int		length,			/* Length of location string */
		order_type,		/* Allow or Deny */
		level;			/* Access level required */
  char		group_name[16];		/* User group name */
  int		num_allow;		/* Number of Allow lines */
  mask_t	*allow;			/* Allow lines */
  int		num_deny;		/* Number of Deny lines */
  mask_t	*deny;			/* Deny lines */
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

extern location_t	*AddLocation(char *location);
extern void		AllowHost(location_t *loc, char *name);
extern void		AllowIP(location_t *loc, unsigned address,
			        unsigned netmask);
extern void		DeleteAllLocations(void);
extern void		DenyHost(location_t *loc, char *name);
extern void		DenyIP(location_t *loc, unsigned address,
			       unsigned netmask);
extern int		IsAuthorized(client_t *con);


/*
 * End of "$Id: auth.h,v 1.2 1998/10/16 18:28:01 mike Exp $".
 */
