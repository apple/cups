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
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"


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

cupsd_location_t *				/* O - New policy operation */
cupsdAddPolicyOp(cupsd_policy_t *p,	/* I - Policy */
                 cupsd_location_t     *po,	/* I - Policy operation to copy */
                 ipp_op_t       op)	/* I - IPP operation code */
{
  int		i;			/* Looping var */
  cupsd_location_t	*temp,			/* New policy operation */
		**tempa;		/* New policy operation array */
  char		name[1024];		/* Interface name */


  cupsdLogMessage(L_DEBUG2, "cupsdAddPolicyOp(p=%p, po=%p, op=%x(%s))",
             p, po, op, ippOpString(op));

  if (p == NULL)
    return (NULL);

  if (p->num_ops == 0)
    tempa = malloc(sizeof(cupsd_location_t *));
  else
    tempa = realloc(p->ops, sizeof(cupsd_location_t *) * (p->num_ops + 1));

  if (tempa == NULL)
    return (NULL);

  p->ops = tempa;

  if ((temp = calloc(1, sizeof(cupsd_location_t))) != NULL)
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
      temp->type       = po->type;
      temp->level      = po->level;
      temp->satisfy    = po->satisfy;
      temp->encryption = po->encryption;

      for (i = 0; i < po->num_names; i ++)
        cupsdAddName(temp, po->names[i]);

      for (i = 0; i < po->num_allow; i ++)
        switch (po->allow[i].type)
	{
	  case AUTH_IP :
	      cupsdAllowIP(temp, po->allow[i].mask.ip.address,
	              po->allow[i].mask.ip.netmask);
	      break;

          case AUTH_INTERFACE :
	      snprintf(name, sizeof(name), "@IF(%s)",
	               po->allow[i].mask.name.name);
              cupsdAllowHost(temp, name);
	      break;

          default :
              cupsdAllowHost(temp, po->allow[i].mask.name.name);
	      break;
        }

      for (i = 0; i < po->num_deny; i ++)
        switch (po->deny[i].type)
	{
	  case AUTH_IP :
	      cupsdDenyIP(temp, po->deny[i].mask.ip.address,
	              po->deny[i].mask.ip.netmask);
	      break;

          case AUTH_INTERFACE :
	      snprintf(name, sizeof(name), "@IF(%s)",
	               po->deny[i].mask.name.name);
              cupsdDenyHost(temp, name);
	      break;

          default :
              cupsdDenyHost(temp, po->deny[i].mask.name.name);
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
                 cupsd_client_t       *con,	/* I - Client connection */
	         const char     *owner)	/* I - Owner of object */
{
  cupsd_location_t	*po;			/* Current policy operation */


 /*
  * Range check...
  */

  if (!p || !con)
  {
    cupsdLogMessage(L_CRIT, "cupsdCheckPolicy: p=%p, con=%p!", p, con);

    return (0);
  }

 /*
  * Find a match for the operation...
  */

  if ((po = cupsdFindPolicyOp(p, con->request->request.op.operation_id)) == NULL)
  {
    cupsdLogMessage(L_DEBUG2, "cupsdCheckPolicy: No matching operation, returning 0!");
    return (0);
  }

  con->best = po;

 /*
  * Return the status of the check...
  */

  return (cupsdIsAuthorized(con, owner) == HTTP_OK);
}


/*
 * 'cupsdDeleteAllPolicies()' - Delete all policies in memory.
 */

void
cupsdDeleteAllPolicies(void)
{
  int			i, j;		/* Looping vars */
  cupsd_policy_t	**p;		/* Current policy */
  cupsd_location_t		**po;		/* Current policy op */


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
  int			i;		/* Looping var */
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

cupsd_location_t *				/* O - Policy operation */
cupsdFindPolicyOp(cupsd_policy_t *p,	/* I - Policy */
                  ipp_op_t       op)	/* I - IPP operation */
{
  int		i;			/* Looping var */
  cupsd_location_t	**po;			/* Current policy operation */


  cupsdLogMessage(L_DEBUG2, "cupsdFindPolicyOp(p=%p, op=%x(%s))\n",
             p, op, ippOpString(op));

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
    {
      cupsdLogMessage(L_DEBUG2, "cupsdFindPolicyOp: Found exact match...");
      return (*po);
    }

  for (i = p->num_ops, po = p->ops; i > 0; i --, po ++)
    if ((*po)->op == IPP_ANY_OPERATION)
    {
      cupsdLogMessage(L_DEBUG2, "cupsdFindPolicyOp: Found wildcard match...");
      return (*po);
    }

  cupsdLogMessage(L_DEBUG2, "cupsdFindPolicyOp: No match found!");

  return (NULL);
}


/*
 * End of "$Id$".
 */
