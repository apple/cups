/*
 * "$Id$"
 *
 *   Policy routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007-2008 by Apple Inc.
 *   Copyright 1997-2006 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
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
 * Local functions...
 */

static int	compare_ops(cupsd_location_t *a, cupsd_location_t *b);
static int	compare_policies(cupsd_policy_t *a, cupsd_policy_t *b);
static int	hash_op(cupsd_location_t *op);


/*
 * 'AddPolicy()' - Add a policy to the system.
 */

cupsd_policy_t *			/* O - Policy */
cupsdAddPolicy(const char *policy)	/* I - Name of policy */
{
  cupsd_policy_t	*temp;		/* Pointer to policy */


  if (!policy)
    return (NULL);

  if (!Policies)
    Policies = cupsArrayNew((cups_array_func_t)compare_policies, NULL);

  if (!Policies)
    return (NULL);

  if ((temp = calloc(1, sizeof(cupsd_policy_t))) != NULL)
  {
    cupsdSetString(&temp->name, policy);
    cupsArrayAdd(Policies, temp);
  }

  return (temp);
}


/*
 * 'cupsdAddPolicyOp()' - Add an operation to a policy.
 */

cupsd_location_t *			/* O - New policy operation */
cupsdAddPolicyOp(cupsd_policy_t   *p,	/* I - Policy */
                 cupsd_location_t *po,	/* I - Policy operation to copy */
                 ipp_op_t         op)	/* I - IPP operation code */
{
  int			i;		/* Looping var */
  cupsd_location_t	*temp;		/* New policy operation */
  char			name[1024];	/* Interface name */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdAddPolicyOp(p=%p, po=%p, op=%x(%s))",
                  p, po, op, ippOpString(op));

  if (!p)
    return (NULL);

  if (!p->ops)
    p->ops = cupsArrayNew2((cups_array_func_t)compare_ops, NULL,
                           (cups_ahash_func_t)hash_op, 128);

  if (!p->ops)
    return (NULL);

  if ((temp = calloc(1, sizeof(cupsd_location_t))) != NULL)
  {
    temp->op    = op;
    temp->limit = CUPSD_AUTH_LIMIT_IPP;

    cupsArrayAdd(p->ops, temp);

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
	  case CUPSD_AUTH_IP :
	      cupsdAllowIP(temp, po->allow[i].mask.ip.address,
	                   po->allow[i].mask.ip.netmask);
	      break;

          case CUPSD_AUTH_INTERFACE :
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
	  case CUPSD_AUTH_IP :
	      cupsdDenyIP(temp, po->deny[i].mask.ip.address,
	                  po->deny[i].mask.ip.netmask);
	      break;

          case CUPSD_AUTH_INTERFACE :
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

http_status_t				/* I - 1 if OK, 0 otherwise */
cupsdCheckPolicy(cupsd_policy_t *p,	/* I - Policy */
                 cupsd_client_t *con,	/* I - Client connection */
	         const char     *owner)	/* I - Owner of object */
{
  cupsd_location_t	*po;		/* Current policy operation */


 /*
  * Range check...
  */

  if (!p || !con)
  {
    cupsdLogMessage(CUPSD_LOG_CRIT, "cupsdCheckPolicy: p=%p, con=%p!", p, con);

    return ((http_status_t)0);
  }

 /*
  * Find a match for the operation...
  */

  if ((po = cupsdFindPolicyOp(p, con->request->request.op.operation_id)) == NULL)
  {
    cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdCheckPolicy: No matching operation, returning 0!");
    return ((http_status_t)0);
  }

  con->best = po;

 /*
  * Return the status of the check...
  */

  return (cupsdIsAuthorized(con, owner));
}


/*
 * 'cupsdDeleteAllPolicies()' - Delete all policies in memory.
 */

void
cupsdDeleteAllPolicies(void)
{
  cupsd_policy_t	*p;		/* Current policy */
  cupsd_location_t	*po;		/* Current policy op */


  if (!Policies)
    return;

  for (p = (cupsd_policy_t *)cupsArrayFirst(Policies);
       p;
       p = (cupsd_policy_t *)cupsArrayNext(Policies))
  {
    for (po = (cupsd_location_t *)cupsArrayFirst(p->ops);
         po;
	 po = (cupsd_location_t *)cupsArrayNext(p->ops))
      cupsdDeleteLocation(po);

    cupsArrayDelete(p->ops);
    cupsdClearString(&p->name);
    free(p);
  }

  cupsArrayDelete(Policies);

  Policies = NULL;
}


/*
 * 'cupsdFindPolicy()' - Find a named policy.
 */

cupsd_policy_t *			/* O - Policy */
cupsdFindPolicy(const char *policy)	/* I - Name of policy */
{
  cupsd_policy_t	key;		/* Search key */


 /*
  * Range check...
  */

  if (!policy)
    return (NULL);

 /*
  * Look it up...
  */

  key.name = (char *)policy;
  return ((cupsd_policy_t *)cupsArrayFind(Policies, &key));
}


/*
 * 'cupsdFindPolicyOp()' - Find a policy operation.
 */

cupsd_location_t *			/* O - Policy operation */
cupsdFindPolicyOp(cupsd_policy_t *p,	/* I - Policy */
                  ipp_op_t       op)	/* I - IPP operation */
{
  cupsd_location_t	key,		/* Search key... */
			*po;		/* Current policy operation */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdFindPolicyOp(p=%p, op=%x(%s))",
                  p, op, ippOpString(op));

 /*
  * Range check...
  */

  if (!p)
    return (NULL);

 /*
  * Check the operation against the available policies...
  */

  key.op = op;
  if ((po = (cupsd_location_t *)cupsArrayFind(p->ops, &key)) != NULL)
  {
    cupsdLogMessage(CUPSD_LOG_DEBUG2,
		    "cupsdFindPolicyOp: Found exact match...");
    return (po);
  }

  key.op = IPP_ANY_OPERATION;
  if ((po = (cupsd_location_t *)cupsArrayFind(p->ops, &key)) != NULL)
  {
    cupsdLogMessage(CUPSD_LOG_DEBUG2,
		    "cupsdFindPolicyOp: Found wildcard match...");
    return (po);
  }

  cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdFindPolicyOp: No match found!");

  return (NULL);
}


/*
 * 'compare_ops()' - Compare two operations.
 */

static int				/* O - Result of comparison */
compare_ops(cupsd_location_t *a,	/* I - First operation */
            cupsd_location_t *b)	/* I - Second operation */
{
  return (a->op - b->op);
}


/*
 * 'compare_policies()' - Compare two policies.
 */

static int				/* O - Result of comparison */
compare_policies(cupsd_policy_t *a,	/* I - First policy */
                 cupsd_policy_t *b)	/* I - Second policy */
{
  return (strcasecmp(a->name, b->name));
}


/*
 * 'hash_op()' - Generate a lookup hash for the operation.
 */

static int				/* O - Hash value */
hash_op(cupsd_location_t *op)		/* I - Operation */
{
  return (((op->op >> 6) & 0x40) | (op->op & 0x3f));
}


/*
 * End of "$Id$".
 */
