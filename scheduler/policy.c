/*
 * "$Id: policy.c,v 1.1.2.5 2004/06/29 13:15:11 mike Exp $"
 *
 *   Policy routines for the Common UNIX Printing System (CUPS).
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
 *
 * Contents:
 *
 *   AddPolicy()         - Add a policy to the system.
 *   AddPolicyOp()       - Add an operation to a policy.
 *   AddPolicyOpName()   - Add a name to a policy operation.
 *   CheckPolicy()       - Check the IPP operation and username against a policy.
 *   DeleteAllPolicies() - Delete all policies in memory.
 *   FindPolicy()        - Find a named policy.
 *   FindPolicyOp()      - Find a policy operation.
 *   validate_user()     - Validate the user for the request.
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"
#include <pwd.h>
#include <grp.h>
#ifdef HAVE_USERSEC_H
#  include <usersec.h>
#endif /* HAVE_USERSEC_H */


/*
 * Local functions...
 */

static int	check_group(const char *, const char *);


/*
 * 'AddPolicy()' - Add a policy to the system.
 */

policy_t *				/* O - Policy */
AddPolicy(const char *policy)		/* I - Name of policy */
{
  policy_t	*temp;			/* Pointer to policy */


  if (policy == NULL)
    return (NULL);

  if (NumPolicies == 0)
    temp = malloc(sizeof(policy_t));
  else
    temp = realloc(Policies, sizeof(policy_t) * (NumPolicies + 1));

  if (temp != NULL)
  {
    Policies = temp;
    temp     += NumPolicies;
    NumPolicies ++;

    memset(temp, 0, sizeof(policy_t));
    temp->name           = strdup(policy);
    temp->default_result = 1;
  }

  return (temp);
}


/*
 * 'AddPolicyOp()' - Add an operation to a policy.
 */

policyop_t *				/* O - New policy operation */
AddPolicyOp(policy_t *p,		/* I - Policy */
            ipp_op_t op)		/* I - IPP operation code */
{
  policyop_t	*temp;			/* New policy operation */


  if (p == NULL)
    return (NULL);

  if (p->num_ops == 0)
    temp = malloc(sizeof(policyop_t));
  else
    temp = realloc(p->ops, sizeof(policyop_t) * (p->num_ops + 1));

  if (temp != NULL)
  {
    p->ops = temp;
    temp   += p->num_ops;
    p->num_ops ++;

    memset(temp, 0, sizeof(policyop_t));
    temp->op = op;
  }

  return (temp);
}


/*
 * 'AddPolicyOpName()' - Add a name to a policy operation.
 */

void
AddPolicyOpName(policyop_t *po,		/* I - Policy operation */
                const char *name)	/* I - Name to add */
{
  char		**temp;			/* New name array */


  if (po == NULL || name == NULL)
    return;

  if (po->num_names == 0)
    temp = malloc(sizeof(char *));
  else
    temp = realloc(po->names, sizeof(char *) * (po->num_names + 1));

  if (temp != NULL)
  {
    po->names = temp;
    temp      += po->num_names;
    po->num_names ++;

    *temp = strdup(name);
  }
}


/*
 * 'CheckPolicy()' - Check the IPP operation and username against a policy.
 */

int					/* I - 1 if OK, 0 otherwise */
CheckPolicy(policy_t   *p,		/* I - Policy */
            ipp_op_t   op,		/* I - IPP operation */
	    const char *name,		/* I - Authenticated username */
	    const char *owner)		/* I - Owner of object */
{
  int		i, j;			/* Looping vars */
  policyop_t	*po;			/* Current policy operation */
  char		**pn;			/* Current policy name */


 /*
  * Range check...
  */

  if (p == NULL)
    return (0);

 /*
  * Check the operation against the available policies...
  */

  for (i = p->num_ops, po = p->ops; i > 0; i --, po ++)
    if (po->op == op)
      switch (po->level)
      {
        case POLICY_LEVEL_ANON :
	    return (1);

        case POLICY_LEVEL_NONE :
	    return (0);

        case POLICY_LEVEL_USER :
	    if (name == NULL || !*name)
	      return (0);
	    else if (po->num_names == 0)
	      return (1);
	    else if (owner != NULL && strcmp(name, owner) == 0)
	      return (1);

	    for (j = po->num_names, pn = po->names; j > 0; j --, pn ++)
	      if (strcmp(name, *pn) == 0)
	        return (1);

	    return (0);

        case POLICY_LEVEL_GROUP :
	    if (name == NULL || !*name)
	      return (0);
	    else if (po->num_names == 0)
	      return (1);
	    else if (owner != NULL && strcmp(name, owner) == 0)
	      return (1);

	    for (j = po->num_names, pn = po->names; j > 0; j --, pn ++)
	      if (check_group(name, *pn))
	        return (1);

	    return (0);
      }

 /*
  * If none of the operations matched, then return the default
  * result...
  */

  return (p->default_result);
}


/*
 * 'DeleteAllPolicies()' - Delete all policies in memory.
 */

void
DeleteAllPolicies(void)
{
  int		i, j, k;	/* Looping vars */
  policy_t	*p;		/* Current policy */
  policyop_t	*po;		/* Current policy operation */
  char		**pn;		/* Current policy name */


  if (NumPolicies == 0)
    return;

  for (i = NumPolicies, p = Policies; i > 0; i --, p ++)
  {
    for (j = p->num_ops, po = p->ops; j > 0; j --, po ++)
    {
      for (k = po->num_names, pn = po->names; k > 0; k --, pn ++)
        free(*pn);

      if (po->num_names > 0)
        free(po->names);
    }

    if (p->num_ops > 0)
      free(p->ops);
  }

  free(Policies);

  NumPolicies = 0;
  Policies    = NULL;
}


/*
 * 'FindPolicy()' - Find a named policy.
 */

policy_t *				/* O - Policy */
FindPolicy(const char *policy)		/* I - Name of policy */
{
  int		i;			/* Looping var */
  policy_t	*p;			/* Current policy */


 /*
  * Range check...
  */

  if (policy == NULL)
    return (NULL);

 /*
  * Check the operation against the available policies...
  */

  for (i = NumPolicies, p = Policies; i > 0; i --, p ++)
    if (strcasecmp(policy, p->name) == 0)
      return (p);

  return (NULL);
}


/*
 * 'FindPolicyOp()' - Find a policy operation.
 */

policyop_t *				/* O - Policy operation */
FindPolicyOp(policy_t *p,		/* I - Policy */
             ipp_op_t op)		/* I - IPP operation */
{
  int		i;			/* Looping var */
  policyop_t	*po;			/* Current policy operation */


 /*
  * Range check...
  */

  if (p == NULL)
    return (NULL);

 /*
  * Check the operation against the available policies...
  */

  for (i = p->num_ops, po = p->ops; i > 0; i --, po ++)
    if (po->op == op)
      return (po);

  return (NULL);
}


/*
 * 'validate_user()' - Validate the user for the request.
 */

static int				/* O - 1 if permitted, 0 otherwise */
check_group(const char *username,	/* I - Authenticated username */
            const char *groupname)	/* I - Group name */
{
  int			i;		/* Looping var */
  struct passwd		*user;		/* User info */
  struct group		*group;		/* System group info */
  char			junk[33];	/* MD5 password (not used) */


  LogMessage(L_DEBUG2, "check_group(%s, %s)\n", username, groupname);

 /*
  * Validate input...
  */

  if (username == NULL || groupname == NULL)
    return (0);

 /*
  * Check to see if the user is a member of the named group...
  */

  user = getpwnam(username);
  endpwent();

  group = getgrnam(groupname);
  endgrent();

  if (group != NULL)
  {
   /*
    * Group exists, check it...
    */

    for (i = 0; group->gr_mem[i]; i ++)
      if (strcmp(username, group->gr_mem[i]) == 0)
	return (1);
  }

 /*
  * Group doesn't exist or user not in group list, check the group ID
  * against the user's group ID...
  */

  if (user != NULL && group != NULL && group->gr_gid == user->pw_gid)
    return (1);

 /*
  * Username not found, group not found, or user is not part of the
  * system group...  Check for a user and group in the MD5 password
  * file...
  */

  if (GetMD5Passwd(username, groupname, junk) != NULL)
    return (1);

 /*
  * If we get this far, then the user isn't part of the named group...
  */

  return (0);
}


/*
 * End of "$Id: policy.c,v 1.1.2.5 2004/06/29 13:15:11 mike Exp $".
 */
