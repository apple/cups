/*
 * "$Id: policy.c 7673 2008-06-18 22:31:26Z mike $"
 *
 *   Policy routines for the CUPS scheduler.
 *
 *   Copyright 2007-2011 by Apple Inc.
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
 *   AddPolicy()              - Add a policy to the system.
 *   cupsdAddPolicyOp()       - Add an operation to a policy.
 *   cupsdCheckPolicy()       - Check the IPP operation and username against a
 *                              policy.
 *   cupsdDeleteAllPolicies() - Delete all policies in memory.
 *   cupsdFindPolicy()        - Find a named policy.
 *   cupsdFindPolicyOp()      - Find a policy operation.
 *   cupsdGetPrivateAttrs()   - Get the private attributes for the current
 *                              request.
 *   compare_ops()            - Compare two operations.
 *   compare_policies()       - Compare two policies.
 *   free_policy()            - Free the memory used by a policy.
 *   hash_op()                - Generate a lookup hash for the operation.
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"
#include <pwd.h>


/*
 * Local functions...
 */

static int	compare_ops(cupsd_location_t *a, cupsd_location_t *b);
static int	compare_policies(cupsd_policy_t *a, cupsd_policy_t *b);
static void	free_policy(cupsd_policy_t *p);
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
    Policies = cupsArrayNew3((cups_array_func_t)compare_policies, NULL,
			     (cups_ahash_func_t)NULL, 0,
			     (cups_acopy_func_t)NULL,
			     (cups_afree_func_t)free_policy);

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
  cupsd_location_t	*temp;		/* New policy operation */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdAddPolicyOp(p=%p, po=%p, op=%x(%s))",
                  p, po, op, ippOpString(op));

  if (!p)
    return (NULL);

  if (!p->ops)
    p->ops = cupsArrayNew3((cups_array_func_t)compare_ops, NULL,
                           (cups_ahash_func_t)hash_op, 128,
			   (cups_acopy_func_t)NULL,
			   (cups_afree_func_t)cupsdFreeLocation);

  if (!p->ops)
    return (NULL);

  if ((temp = cupsdCopyLocation(po)) != NULL)
  {
    temp->op    = op;
    temp->limit = CUPSD_AUTH_LIMIT_IPP;

    cupsArrayAdd(p->ops, temp);
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
  cupsd_printer_t	*printer;	/* Current printer */


  if (!Policies)
    return;

 /*
  * First clear the policy pointers for all printers...
  */

  for (printer = (cupsd_printer_t *)cupsArrayFirst(Printers);
       printer;
       printer = (cupsd_printer_t *)cupsArrayNext(Printers))
    printer->op_policy_ptr = NULL;

  DefaultPolicyPtr = NULL;

 /*
  * Then free all of the policies...
  */

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
 * 'cupsdGetPrivateAttrs()' - Get the private attributes for the current
 *                            request.
 */

cups_array_t *				/* O - Array or NULL for no restrictions */
cupsdGetPrivateAttrs(
    cupsd_policy_t  *policy,		/* I - Policy */
    cupsd_client_t  *con,		/* I - Client connection */
    cupsd_printer_t *printer,		/* I - Printer, if any */
    const char      *owner)		/* I - Owner of object */
{
  char		*name;			/* Current name in access list */
  cups_array_t	*access_ptr,		/* Access array */
		*attrs_ptr;		/* Attributes array */
  const char	*username;		/* Username associated with request */
  ipp_attribute_t *attr;		/* Attribute from request */
  struct passwd	*pw;			/* User info */


#ifdef DEBUG
  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "cupsdGetPrivateAttrs(policy=%p(%s), con=%p(%d), "
		  "printer=%p(%s), owner=\"%s\")", policy, policy->name, con,
		  con->http.fd, printer, printer ? printer->name : "", owner);
#endif /* DEBUG */

 /*
  * Get the access and attributes lists that correspond to the request...
  */

#ifdef DEBUG
  cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdGetPrivateAttrs: %s",
                  ippOpString(con->request->request.op.operation_id));
#endif /* DEBUG */

  switch (con->request->request.op.operation_id)
  {
    case IPP_GET_SUBSCRIPTIONS :
    case IPP_GET_SUBSCRIPTION_ATTRIBUTES :
    case IPP_GET_NOTIFICATIONS :
        access_ptr = policy->sub_access;
	attrs_ptr  = policy->sub_attrs;
	break;

    default :
        access_ptr = policy->job_access;
	attrs_ptr  = policy->job_attrs;
        break;
  }

 /*
  * If none of the attributes are private, return NULL now...
  */

  if ((name = (char *)cupsArrayFirst(attrs_ptr)) != NULL &&
      !_cups_strcasecmp(name, "none"))
  {
#ifdef DEBUG
    cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdGetPrivateAttrs: Returning NULL.");
#endif /* DEBUG */

    return (NULL);
  }

 /*
  * Otherwise check the user against the access list...
  */

  if (con->username[0])
    username = con->username;
  else if ((attr = ippFindAttribute(con->request, "requesting-user-name",
                                    IPP_TAG_NAME)) != NULL)
    username = attr->values[0].string.text;
  else
    username = "anonymous";

  if (username[0])
  {
    pw = getpwnam(username);
    endpwent();
  }
  else
    pw = NULL;

#ifdef DEBUG
  cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdGetPrivateAttrs: username=\"%s\"",
                  username);
#endif /* DEBUG */

 /*
  * Otherwise check the user against the access list...
  */

  for (name = (char *)cupsArrayFirst(access_ptr);
       name;
       name = (char *)cupsArrayNext(access_ptr))
  {
#ifdef DEBUG
    cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdGetPrivateAttrs: name=%s", name);
#endif /* DEBUG */

    if (printer && !_cups_strcasecmp(name, "@ACL"))
    {
      char	*acl;			/* Current ACL user/group */

      for (acl = (char *)cupsArrayFirst(printer->users);
	   acl;
	   acl = (char *)cupsArrayNext(printer->users))
      {
	if (acl[0] == '@')
	{
	 /*
	  * Check group membership...
	  */

	  if (cupsdCheckGroup(username, pw, acl + 1))
	    break;
	}
	else if (acl[0] == '#')
	{
	 /*
	  * Check UUID...
	  */

	  if (cupsdCheckGroup(username, pw, acl))
	    break;
	}
	else if (!_cups_strcasecmp(username, acl))
	  break;
      }
    }
    else if (owner && !_cups_strcasecmp(name, "@OWNER") &&
             !_cups_strcasecmp(username, owner))
    {
#ifdef DEBUG
      cupsdLogMessage(CUPSD_LOG_DEBUG2,
		      "cupsdGetPrivateAttrs: Returning NULL.");
#endif /* DEBUG */

      return (NULL);
    }
    else if (!_cups_strcasecmp(name, "@SYSTEM"))
    {
      int i;				/* Looping var */

      for (i = 0; i < NumSystemGroups; i ++)
	if (cupsdCheckGroup(username, pw, SystemGroups[i]))
	{
#ifdef DEBUG
	  cupsdLogMessage(CUPSD_LOG_DEBUG2,
	                  "cupsdGetPrivateAttrs: Returning NULL.");
#endif /* DEBUG */

	  return (NULL);
	}
    }
    else if (name[0] == '@')
    {
      if (cupsdCheckGroup(username, pw, name + 1))
      {
#ifdef DEBUG
        cupsdLogMessage(CUPSD_LOG_DEBUG2,
	                "cupsdGetPrivateAttrs: Returning NULL.");
#endif /* DEBUG */

	return (NULL);
      }
    }
    else if (!_cups_strcasecmp(username, name))
    {
#ifdef DEBUG
      cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdGetPrivateAttrs: Returning NULL.");
#endif /* DEBUG */

      return (NULL);
    }
  }

 /*
  * No direct access, so return private attributes list...
  */

#ifdef DEBUG
  cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdGetPrivateAttrs: Returning list.");
#endif /* DEBUG */

  return (attrs_ptr);
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
  return (_cups_strcasecmp(a->name, b->name));
}


/*
 * 'free_policy()' - Free the memory used by a policy.
 */

static void
free_policy(cupsd_policy_t *p)		/* I - Policy to free */
{
  cupsArrayDelete(p->job_access);
  cupsArrayDelete(p->job_attrs);
  cupsArrayDelete(p->sub_access);
  cupsArrayDelete(p->sub_attrs);
  cupsArrayDelete(p->ops);
  cupsdClearString(&p->name);
  free(p);
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
 * End of "$Id: policy.c 7673 2008-06-18 22:31:26Z mike $".
 */
