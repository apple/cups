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
 *   cupsdAddPolicy()         - Add a policy to the system.
 *   cupsdAddPolicyOp()       - Add an operation to a policy.
 *   cupsdCheckPolicy()       - Check the IPP operation and username against
 *                              a policy.
 *   cupsdDeleteAllPolicies() - Delete all policies in memory.
 *   cupsdFindPolicy()        - Find a named policy.
 *   cupsdFindPolicyOp()      - Find a policy operation.
 *   check_op()               - Check the current operation.
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"
#include <grp.h>
#ifdef HAVE_USERSEC_H
#  include <usersec.h>
#endif /* HAVE_USERSEC_H */


/*
 * Local functions...
 */

static int	check_op(location_t *po, int allow_deny, const char *name,
		         const char *owner);


/*
 * 'AddPolicy()' - Add a policy to the system.
 */

cupsd_policy_t *			/* O - Policy */
cupsdAddPolicy(const char *policy)	/* I - Name of policy */
{
  cupsd_policy_t	*temp,		/* Pointer to policy */
			**tempa;	/* Pointer to policy array */


  if (policy == NULL)
    return (NULL);

  if (NumPolicies == 0)
    tempa = malloc(sizeof(cupsd_policy_t *));
  else
    tempa = realloc(Policies, sizeof(cupsd_policy_t *) * (NumPolicies + 1));

  if (tempa == NULL)
    return (NULL);

  Policies = tempa;
  tempa    += NumPolicies;

  if ((temp = calloc(1, sizeof(cupsd_policy_t))) != NULL)
  {
    temp->name = strdup(policy);
    *tempa     = temp;

    NumPolicies ++;
  }

  return (temp);
}


/*
 * 'cupsdAddPolicyOp()' - Add an operation to a policy.
 */

location_t *				/* O - New policy operation */
cupsdAddPolicyOp(cupsd_policy_t *p,	/* I - Policy */
                 location_t     *po,	/* I - Policy operation to copy */
                 ipp_op_t       op)	/* I - IPP operation code */
{
  int		i;			/* Looping var */
  location_t	*temp,			/* New policy operation */
		**tempa;		/* New policy operation array */
  char		name[1024];		/* Interface name */


  if (p == NULL)
    return (NULL);

  if (p->num_ops == 0)
    tempa = malloc(sizeof(location_t *));
  else
    tempa = realloc(p->ops, sizeof(location_t *) * (p->num_ops + 1));

  if (tempa == NULL)
    return (NULL);

  p->ops = tempa;

  if ((temp = calloc(1, sizeof(location_t))) != NULL)
  {
    p->ops            = tempa;
    tempa[p->num_ops] = temp;
    p->num_ops ++;

    temp->op    = op;
    temp->limit = AUTH_LIMIT_IPP;

    if (po)
    {
     /*
      * Copy the specified policy to the new one...
      */

      temp->order_type = po->order_type;
      temp->type       = po->order_type;
      temp->level      = po->level;
      temp->satisfy    = po->satisfy;
      temp->encryption = po->encryption;

      for (i = 0; i < po->num_names; i ++)
        AddName(temp, po->names[i]);

      for (i = 0; i < po->num_allow; i ++)
        switch (po->allow[i].type)
	{
	  case AUTH_IP :
	      AllowIP(temp, po->allow[i].mask.ip.address,
	              po->allow[i].mask.ip.netmask);
	      break;

          case AUTH_INTERFACE :
	      snprintf(name, sizeof(name), "@IF(%s)",
	               po->allow[i].mask.name.name);
              AllowHost(temp, name);
	      break;

          default :
              AllowHost(temp, po->allow[i].mask.name.name);
	      break;
        }

      for (i = 0; i < po->num_deny; i ++)
        switch (po->deny[i].type)
	{
	  case AUTH_IP :
	      DenyIP(temp, po->deny[i].mask.ip.address,
	              po->deny[i].mask.ip.netmask);
	      break;

          case AUTH_INTERFACE :
	      snprintf(name, sizeof(name), "@IF(%s)",
	               po->deny[i].mask.name.name);
              DenyHost(temp, name);
	      break;

          default :
              DenyHost(temp, po->deny[i].mask.name.name);
	      break;
        }
    }
  }

  return (temp);
}


/*
 * 'cupsdCheckPolicy()' - Check the IPP operation and username against a policy.
 */

int					/* I - 1 if OK, 0 otherwise */
cupsdCheckPolicy(cupsd_policy_t *p,	/* I - Policy */
                 client_t       *con,	/* I - Client connection */
	         const char     *owner)	/* I - Owner of object */
{
  ipp_op_t	op;			/* IPP operation */
  const char	*name;			/* Username */
  int		authenticated;		/* Authenticated? */
  ipp_attribute_t *attr;		/* IPP attribute */
  int		status;			/* Status */
  location_t	*po;			/* Current policy operation */


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

  if ((po = cupsdFindPolicyOp(p, op)) == NULL)
  {
    LogMessage(L_DEBUG2, "CheckPolicy: No matching operation, returning 0!");
    return (0);
  }

 /*
  * Check the policy against the current user, etc.
  */

  if (po->type && !authenticated)
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
 * 'cupsdDeleteAllPolicies()' - Delete all policies in memory.
 */

void
cupsdDeleteAllPolicies(void)
{
  int			i, j;		/* Looping vars */
  cupsd_policy_t	**p;		/* Current policy */
  location_t		**po;		/* Current policy op */


  if (NumPolicies == 0)
    return;

  for (i = NumPolicies, p = Policies; i > 0; i --, p ++)
  {
    for (j = (*p)->num_ops, po = (*p)->ops; j > 0; j --, po ++)
    {
      cupsdDeleteLocation(*po);
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
 * 'cupsdFindPolicy()' - Find a named policy.
 */

cupsd_policy_t *			/* O - Policy */
cupsdFindPolicy(const char *policy)	/* I - Name of policy */
{
  int		i;			/* Looping var */
  cupsd_policy_t	**p;		/* Current policy */


 /*
  * Range check...
  */

  if (policy == NULL)
    return (NULL);

 /*
  * Check the operation against the available policies...
  */

  for (i = NumPolicies, p = Policies; i > 0; i --, p ++)
    if (!strcasecmp(policy, (*p)->name))
      return (*p);

  return (NULL);
}


/*
 * 'cupsdFindPolicyOp()' - Find a policy operation.
 */

location_t *				/* O - Policy operation */
cupsdFindPolicyOp(cupsd_policy_t *p,	/* I - Policy */
                  ipp_op_t       op)	/* I - IPP operation */
{
  int		i;			/* Looping var */
  location_t	**po;			/* Current policy operation */


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


#if 0
/*
 * 'check_op()' - Check the current operation.
 */

static int				/* O - 1 if match, 0 if not */
check_op(location_t *po,		/* I - Policy operation */
         int        allow_deny,		/* I - POLICY_ALLOW or POLICY_DENY */
         const char *name,		/* I - User name */
	 const char *owner)		/* I - Owner name */
{
  int		i, j;			/* Looping vars */
  policyname_t	*pn;			/* Current policy name */
  struct passwd	*pw;			/* User's password entry */


  pw = getpwnam(name);
  endpwent();

  for (i = po->num_names, pn = po->names; i > 0; i --, pn ++)
  {
    if (pn->allow_deny != allow_deny)
      continue;

    if (!strcasecmp(pn->name, "@OWNER"))
    {
      if (owner && !strcasecmp(name, owner))
        return (1);
    }
    else if (!strcasecmp(pn->name, "@SYSTEM"))
    {
      for (j = 0; j < NumSystemGroups; j ++)
        if (cupsdCheckGroup(name, pw, SystemGroups[j]))
          return (1);
    }
    else if (pn->name[0] == '@')
    {
      if (cupsdCheckGroup(name, pw, pn->name + 1))
        return (1);
    }
    else if (!strcasecmp(name, pn->name))
      return (1);
  }

  return (0);
}
#endif /* 0 */

/*
 * End of "$Id$".
 */
