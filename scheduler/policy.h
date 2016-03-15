/*
 * "$Id: policy.h 10996 2013-05-29 11:51:34Z msweet $"
 *
 *   Policy definitions for the CUPS scheduler.
 *
 *   Copyright 2007-2010 by Apple Inc.
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
  cups_array_t		*job_access,	/* Private users/groups for jobs */
			*job_attrs,	/* Private attributes for jobs */
			*sub_access,	/* Private users/groups for subscriptions */
			*sub_attrs,	/* Private attributes for subscriptions */
			*ops;		/* Operations */
} cupsd_policy_t;

typedef struct cupsd_printer_s cupsd_printer_t;


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
extern cups_array_t	*cupsdGetPrivateAttrs(cupsd_policy_t *p,
			                      cupsd_client_t *con,
					      cupsd_printer_t *printer,
			                      const char *owner);


/*
 * End of "$Id: policy.h 10996 2013-05-29 11:51:34Z msweet $".
 */
