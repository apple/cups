/*
 * "$Id: policy.h 6895 2007-08-30 00:09:27Z mike $"
 *
 *   Policy definitions for the Common UNIX Printing System (CUPS)
 *   scheduler.
 *
 *   Copyright 2007 by Apple Inc.
 *   Copyright 1997-2005 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 */


/*
 * Policy structure...
 */

typedef struct
{
  char			*name;		/* Policy name */
  cups_array_t		*ops;		/* Operations */
} cupsd_policy_t;


/*
 * Globals...
 */

VAR cups_array_t	*Policies	VALUE(NULL);
					/* Policies */


/*
 * Prototypes...
 */

extern cupsd_policy_t	*cupsdAddPolicy(const char *policy);
extern cupsd_location_t	*cupsdAddPolicyOp(cupsd_policy_t *p,
			                  cupsd_location_t *po,
			                  ipp_op_t op);
extern http_status_t	cupsdCheckPolicy(cupsd_policy_t *p, cupsd_client_t *con,
				         const char *owner);
extern void		cupsdDeleteAllPolicies(void);
extern cupsd_policy_t	*cupsdFindPolicy(const char *policy);
extern cupsd_location_t	*cupsdFindPolicyOp(cupsd_policy_t *p, ipp_op_t op);


/*
 * End of "$Id: policy.h 6895 2007-08-30 00:09:27Z mike $".
 */
