/*
 * "$Id$"
 *
 *   Policy definitions for the Common UNIX Printing System (CUPS)
 *   scheduler.
 *
 *   Copyright 1997-2005 by Easy Software Products, all rights reserved.
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
 * Policy structure...
 */

typedef struct
{
  char		*name;			/* Policy name */
  int		num_ops;		/* Number of operations */
  cupsd_location_t	**ops;			/* Operations */
} cupsd_policy_t;


/*
 * Globals...
 */

VAR int			NumPolicies	VALUE(0);
					/* Number of policies */
VAR cupsd_policy_t	**Policies	VALUE(NULL);
					/* Policies */


/*
 * Prototypes...
 */

extern cupsd_policy_t	*cupsdAddPolicy(const char *policy);
extern cupsd_location_t	*cupsdAddPolicyOp(cupsd_policy_t *p, cupsd_location_t *po,
			                  ipp_op_t op);
extern int		cupsdCheckPolicy(cupsd_policy_t *p, cupsd_client_t *con,
				         const char *owner);
extern void		cupsdDeleteAllPolicies(void);
extern cupsd_policy_t	*cupsdFindPolicy(const char *policy);
extern cupsd_location_t	*cupsdFindPolicyOp(cupsd_policy_t *p, ipp_op_t op);


/*
 * End of "$Id$".
 */
