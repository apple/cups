/*
 * "$Id: policy.h,v 1.1.2.6 2004/06/29 13:15:11 mike Exp $"
 *
 *   Policy definitions for the Common UNIX Printing System (CUPS)
 *   scheduler.
 *
 *   Copyright 1997-2004 by Easy Software Products, all rights reserved.
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
 *       Hollywood, Maryland 20636-3142 USA
 *
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 */

/*
 * "Any" operation code...
 */

#define IPP_ANY_OPERATION	(ipp_op_t)0


/*
 * Access levels...
 */

#define POLICY_LEVEL_ANON	0	/* Anonymous access */
#define POLICY_LEVEL_NONE	1	/* Do not support operation */
#define POLICY_LEVEL_USER	2	/* Must have a valid username/password */
#define POLICY_LEVEL_GROUP	3	/* Must also be in a named group */


/*
 * IPP operation policy structures...
 */

typedef struct
{
  ipp_op_t	op;			/* Operation */
  int		level,			/* Authentication level required */
		num_names;		/* Number of names */
  char		**names;		/* Names */
} policyop_t;

typedef struct
{
  char		*name;			/* Policy name */
  int		default_result;		/* Default policy result */
  int		num_ops;		/* Number of operations */
  policyop_t	*ops;			/* Operations */
} policy_t;


/*
 * Globals...
 */

VAR int			NumPolicies	VALUE(0);
					/* Number of policies */
VAR policy_t		*Policies	VALUE(NULL);
					/* Policies */


/*
 * Prototypes...
 */

extern policy_t		*AddPolicy(const char *policy);
extern policyop_t	*AddPolicyOp(policy_t *p, ipp_op_t op);
extern void		AddPolicyOpName(policyop_t *po, const char *name);
extern int		CheckPolicy(policy_t *p, ipp_op_t op,
			            const char *name, const char *owner);
extern void		DeleteAllPolicies(void);
extern policy_t		*FindPolicy(const char *policy);
extern policyop_t	*FindPolicyOp(policy_t *p, ipp_op_t op);


/*
 * End of "$Id: policy.h,v 1.1.2.6 2004/06/29 13:15:11 mike Exp $".
 */
