/*
 * "$Id: policy.c,v 1.1.2.1 2002/04/14 12:58:54 mike Exp $"
 *
 *   Policy routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2002 by Easy Software Products, all rights reserved.
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


/*
 * 'AddPolicy()' - Add a policy to the system.
 */

policy_t *				/* O - Policy */
AddPolicy(const char *policy)		/* I - Name of policy */
{
}


/*
 * 'AddPolicyOp()' - Add an operation to a policy.
 */

policyop_t *				/* O - New policy operation */
AddPolicyOp(policy_t *p,		/* I - Policy */
            ipp_op_t op)		/* I - IPP operation code */
{
}


/*
 * 'AddPolicyOpName()' - Add a name to a policy operation.
 */

void
AddPolicyOpName(policyop_t *po,		/* I - Policy operation */
                const char *name)	/* I - Name to add */
{
}


/*
 * 'CheckPolicy()' - Check the IPP operation and username against a policy.
 */

int					/* I - 1 if OK, 0 otherwise */
CheckPolicy(policy_t   *p,		/* I - Policy */
            ipp_op_t   op,		/* I - IPP operation */
	    const char *name)		/* I - Authenticated username */
{
}


/*
 * 'DeleteAllPolicies()' - Delete all policies in memory.
 */

void
DeleteAllPolicies(void)
{
}


/*
 * 'FindPolicy()' - Find a named policy.
 */

policy_t *				/* O - Policy */
FindPolicy(const char *policy)		/* I - Name of policy */
{
}


/*
 * 'FindPolicyOp()' - Find a policy operation.
 */

policyop_t *				/* O - Policy operation */
FindPolicyOp(policy_t *p,		/* I - Policy */
             ipp_op_t op)		/* I - IPP operation */
{
}


/*
 * End of "$Id: policy.c,v 1.1.2.1 2002/04/14 12:58:54 mike Exp $".
 */
