/*
 * "$Id$"
 *
 *   Policy routines for the Common UNIX Printing System (CUPS).
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
 *
 * Contents:
 *
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

static int	check_group(const char *name, const char *group);
static int	check_op(policyop_t *po, int allow_deny, const char *name,
		         const char *owner);


/*
 * 'AddPolicy()' - Add a policy to the system.
 */

policy_t *				/* O - Policy */
AddPolicy(const char *policy)		/* I - Name of policy */
{
  policy_t	*temp,			/* Pointer to policy */
		**tempa;		/* Pointer to policy array */


  if (policy == NULL)
    return (NULL);

  if (NumPolicies == 0)
    tempa = malloc(sizeof(policy_t *));
  else
    tempa = realloc(Policies, sizeof(policy_t *) * (NumPolicies + 1));

  if (tempa == NULL)
    return (NULL);

  Policies = tempa;
  tempa    += NumPolicies;

  if ((temp = calloc(1, sizeof(policy_t))) != NULL)
  {
    temp->name = strdup(policy);
    *tempa     = temp;

    NumPolicies ++;
  }

  return (temp);
}


/*
 * 'AddPolicyOp()' - Add an operation to a policy.
 */

policyop_t *				/* O - New policy operation */
AddPolicyOp(policy_t   *p,		/* I - Policy */
            policyop_t *po,		/* I - Policy operation to copy */
            ipp_op_t   op)		/* I - IPP operation code */
{
  int		i;			/* Looping var */
  policyop_t	*temp,			/* New policy operation */
		**tempa;		/* New policy operation array */


  if (p == NULL)
    return (NULL);

  if (p->num_ops == 0)
    tempa = malloc(sizeof(policyop_t *));
  else
    tempa = realloc(p->ops, sizeof(policyop_t *) * (p->num_ops + 1));

  if (tempa == NULL)
    return (NULL);

  p->ops = tempa;

  if ((temp = calloc(1, sizeof(policyop_t))) != NULL)
  {
    p->ops            = tempa;
    tempa[p->num_ops] = temp;
    p->num_ops ++;

    temp->op = op;

    if (po)
    {
     /*
      * Copy the specified policy to the new one...
      */

      temp->order_type   = po->order_type;
      temp->authenticate = po->authenticate;
      for (i = 0; i < po->num_names; i ++)
        AddPolicyOpName(temp, po->names[i].allow_deny, po->names[i].name);
    }
  }

  return (temp);
}


/*
 * 'AddPolicyOpName()' - Add a name to a policy operation.
 */

void
AddPolicyOpName(policyop_t *po,		/* I - Policy operation */
                int        allow_deny,	/* I - POLICY_ALLOW or POLICY_DENY */
                const char *name)	/* I - Name to add */
{
  policyname_t	*temp;			/* New name array */


  if (po == NULL || name == NULL)
    return;

  if (po->num_names == 0)
    temp = malloc(sizeof(policyname_t));
  else
    temp = realloc(po->names, sizeof(policyname_t) * (po->num_names + 1));

  if (temp != NULL)
  {
    po->names = temp;
    temp      += po->num_names;
    po->num_names ++;

    temp->allow_deny = allow_deny;
    temp->name       = strdup(name);
  }
}


/*
 * 'CheckPolicy()' - Check the IPP operation and username against a policy.
 */

int					/* I - 1 if OK, 0 otherwise */
CheckPolicy(policy_t   *p,		/* I - Policy */
            client_t   *con,		/* I - Client connection */
	    const char *owner)		/* I - Owner of object */
{
  ipp_op_t	op;			/* IPP operation */
  const char	*name;			/* Username */
  int		authenticated;		/* Authenticated? */
  ipp_attribute_t *attr;		/* IPP attribute */
  int		status;			/* Status */
  policyop_t	*po;			/* Current policy operation */


 /*
  * Range check...
  */

  if (!p || !con)
  {
    LogMessage(L_CRIT, "CheckPolicy: p=%p, con=%p!", p, con);

    return (0);
  }

 /*
  * Collect info from the request...
  */

  op = con->request->request.op.operation_id;

  if (con->username[0])
  {
    name          = con->username;
    authenticated = 1;
  }
  else if ((attr = ippFindAttribute(con->request, "requesting-user-name",
                                    IPP_TAG_NAME)) != NULL)
  {
    name          = attr->values[0].string.text;
    authenticated = 0;
  }
  else
  {
    name          = "anonymous";
    authenticated = 0;
  }

  LogMessage(L_DEBUG2, "CheckPolicy: op=%04x, name=\"%s\", authenticated=%d, owner=\"%s\"",
             op, name, authenticated, owner ? owner : "");

 /*
  * Find a match for the operation...
  */

  if ((po = FindPolicyOp(p, op)) == NULL)
  {
    LogMessage(L_DEBUG2, "CheckPolicy: No matching operation, returning 0!");
    return (0);
  }

 /*
  * Check the policy against the current user, etc.
  */

  if (po->authenticate && !authenticated)
  {
    LogMessage(L_DEBUG2, "CheckPolicy: Operation requires authentication, returning 0!");
    return (0);
  }

  switch (status = po->order_type)
  {
    default :
    case POLICY_ALLOW :
        if (check_op(po, POLICY_DENY, name, owner))
	  status = POLICY_DENY;
        if (check_op(po, POLICY_ALLOW, name, owner))
	  status = POLICY_ALLOW;
	break;

    case POLICY_DENY :
        if (check_op(po, POLICY_ALLOW, name, owner))
	  status = POLICY_ALLOW;
        if (check_op(po, POLICY_DENY, name, owner))
	  status = POLICY_DENY;
	break;
  }

 /*
  * Return the status of the check...
  */

  LogMessage(L_DEBUG2, "CheckPolicy: Returning %d...", !status);

  return (!status);
}


/*
 * 'DeleteAllPolicies()' - Delete all policies in memory.
 */

void
DeleteAllPolicies(void)
{
  int		i, j, k;		/* Looping vars */
  policy_t	**p;			/* Current policy */
  policyop_t	**po;			/* Current policy operation */
  policyname_t	*pn;			/* Current policy name */


  if (NumPolicies == 0)
    return;

  for (i = NumPolicies, p = Policies; i > 0; i --, p ++)
  {
    for (j = (*p)->num_ops, po = (*p)->ops; j > 0; j --, po ++)
    {
      for (k = (*po)->num_names, pn = (*po)->names; k > 0; k --, pn ++)
        free(pn->name);

      if ((*po)->num_names > 0)
        free((*po)->names);

      free(*po);
    }

    if ((*p)->num_ops > 0)
      free((*p)->ops);

    free(*p);
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
  policy_t	**p;			/* Current policy */


 /*
  * Range check...
  */

  if (policy == NULL)
    return (NULL);

 /*
  * Check the operation against the available policies...
  */

  for (i = NumPolicies, p = Policies; i > 0; i --, p ++)
    if (strcasecmp(policy, (*p)->name) == 0)
      return (*p);

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
  policyop_t	**po;			/* Current policy operation */


 /*
  * Range check...
  */

  if (p == NULL)
    return (NULL);

 /*
  * Check the operation against the available policies...
  */

  for (i = p->num_ops, po = p->ops; i > 0; i --, po ++)
    if ((*po)->op == op)
      return (*po);

  for (i = p->num_ops, po = p->ops; i > 0; i --, po ++)
    if ((*po)->op == IPP_ANY_OPERATION)
      return (*po);

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
      if (strcasecmp(username, group->gr_mem[i]) == 0)
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


#if 0 //// OLD OLD OLD OLD OLD
  if (strcasecmp(username, owner) != 0 && strcasecmp(username, "root") != 0)
  {
   /*
    * Not the owner or root; check to see if the user is a member of the
    * system group...
    */

    user = getpwnam(username);
    endpwent();

    for (i = 0, j = 0, group = NULL; i < NumSystemGroups; i ++)
    {
      group = getgrnam(SystemGroups[i]);
      endgrent();

      if (group != NULL)
      {
	for (j = 0; group->gr_mem[j]; j ++)
          if (strcasecmp(username, group->gr_mem[j]) == 0)
	    break;

        if (group->gr_mem[j])
	  break;
      }
      else
	j = 0;
    }

    if (user == NULL || group == NULL ||
        (group->gr_mem[j] == NULL && group->gr_gid != user->pw_gid))
    {
     /*
      * Username not found, group not found, or user is not part of the
      * system group...  Check for a user and group in the MD5 password
      * file...
      */

      for (i = 0; i < NumSystemGroups; i ++)
        if (GetMD5Passwd(username, SystemGroups[i], junk) != NULL)
	  return (1);

     /*
      * Nope, not an MD5 user, either.  Return 0 indicating no-go...
      */

      return (0);
    }
  }

  return (1);
#endif //// 0
}


/*
 * 'check_op()' - Check the current operation.
 */

static int				/* O - 1 if match, 0 if not */
check_op(policyop_t *po,		/* I - Policy operation */
         int        allow_deny,		/* I - POLICY_ALLOW or POLICY_DENY */
         const char *name,		/* I - User name */
	 const char *owner)		/* I - Owner name */
{
  int		i;			/* Looping vars */
  policyname_t	*pn;			/* Current policy name */


  for (i = po->num_names, pn = po->names; i > 0; i --, pn ++)
  {
    if (pn->allow_deny != allow_deny)
      continue;

    if (!strcasecmp(pn->name, "@OWNER"))
    {
      if (owner && !strcasecmp(name, owner))
        return (1);
    }
    else if (pn->name[0] == '@')
    {
      if (check_group(name, pn->name + 1))
        return (1);
    }
    else if (!strcasecmp(name, pn->name))
      return (1);
  }

  return (0);
}


/*
 * End of "$Id$".
 */
