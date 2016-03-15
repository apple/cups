/*
 * "$Id: subscriptions.c 13040 2016-01-11 20:29:13Z msweet $"
 *
 * Subscription routines for the CUPS scheduler.
 *
 * Copyright 2007-2014 by Apple Inc.
 * Copyright 1997-2007 by Easy Software Products, all rights reserved.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * file is missing or damaged, see the license at "http://www.cups.org/".
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"
#ifdef HAVE_DBUS
#  include <dbus/dbus.h>
#  ifdef HAVE_DBUS_MESSAGE_ITER_INIT_APPEND
#    define dbus_message_append_iter_init dbus_message_iter_init_append
#    define dbus_message_iter_append_string(i,v) dbus_message_iter_append_basic(i, DBUS_TYPE_STRING, &(v))
#    define dbus_message_iter_append_uint32(i,v) dbus_message_iter_append_basic(i, DBUS_TYPE_UINT32, &(v))
#  endif /* HAVE_DBUS_MESSAGE_ITER_INIT_APPEND */
#endif /* HAVE_DBUS */


/*
 * Local functions...
 */

static int	cupsd_compare_subscriptions(cupsd_subscription_t *first,
		                            cupsd_subscription_t *second,
		                            void *unused);
static void	cupsd_delete_event(cupsd_event_t *event);
#ifdef HAVE_DBUS
static void	cupsd_send_dbus(cupsd_eventmask_t event, cupsd_printer_t *dest,
		                cupsd_job_t *job);
#endif /* HAVE_DBUS */
static void	cupsd_send_notification(cupsd_subscription_t *sub,
		                        cupsd_event_t *event);
static void	cupsd_start_notifier(cupsd_subscription_t *sub);
static void	cupsd_update_notifier(void);


/*
 * 'cupsdAddEvent()' - Add an event to the global event cache.
 */

void
cupsdAddEvent(
    cupsd_eventmask_t event,		/* I - Event */
    cupsd_printer_t   *dest,		/* I - Printer associated with event */
    cupsd_job_t       *job,		/* I - Job associated with event */
    const char        *text,		/* I - Notification text */
    ...)				/* I - Additional arguments as needed */
{
  va_list		ap;		/* Pointer to additional arguments */
  char			ftext[1024];	/* Formatted text buffer */
  ipp_attribute_t	*attr;		/* Printer/job attribute */
  cupsd_event_t		*temp;		/* New event pointer */
  cupsd_subscription_t	*sub;		/* Current subscription */


  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "cupsdAddEvent(event=%s, dest=%p(%s), job=%p(%d), text=\"%s\", ...)",
		  cupsdEventName(event), dest, dest ? dest->name : "",
		  job, job ? job->id : 0, text);

 /*
  * Keep track of events with any OS-supplied notification mechanisms...
  */

  LastEvent |= event;

#ifdef HAVE_DBUS
  cupsd_send_dbus(event, dest, job);
#endif /* HAVE_DBUS */

 /*
  * Return if we aren't keeping events...
  */

  if (MaxEvents <= 0)
  {
    cupsdLogMessage(CUPSD_LOG_WARN,
                    "cupsdAddEvent: Discarding %s event since MaxEvents is %d!",
                    cupsdEventName(event), MaxEvents);
    return;
  }

 /*
  * Then loop through the subscriptions and add the event to the corresponding
  * caches...
  */

  for (temp = NULL, sub = (cupsd_subscription_t *)cupsArrayFirst(Subscriptions);
       sub;
       sub = (cupsd_subscription_t *)cupsArrayNext(Subscriptions))
  {
   /*
    * Check if this subscription requires this event...
    */

    if ((sub->mask & event) != 0 && (sub->dest == dest || !sub->dest || sub->job == job))
    {
     /*
      * Need this event, so create a new event record...
      */

      if ((temp = (cupsd_event_t *)calloc(1, sizeof(cupsd_event_t))) == NULL)
      {
	cupsdLogMessage(CUPSD_LOG_CRIT,
	                "Unable to allocate memory for event - %s",
        	        strerror(errno));
	return;
      }

      temp->event = event;
      temp->time  = time(NULL);
      temp->attrs = ippNew();
      temp->job   = job;

      if (dest)
        temp->dest = dest;
      else if (job)
        temp->dest = dest = cupsdFindPrinter(job->dest);

     /*
      * Add common event notification attributes...
      */

      ippAddString(temp->attrs, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_CHARSET,
                   "notify-charset", NULL, "utf-8");

      ippAddString(temp->attrs, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_LANGUAGE,
                   "notify-natural-language", NULL, "en-US");

      ippAddInteger(temp->attrs, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_INTEGER,
	            "notify-subscription-id", sub->id);

      ippAddInteger(temp->attrs, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_INTEGER,
	            "notify-sequence-number", sub->next_event_id);

      ippAddString(temp->attrs, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_KEYWORD,
	           "notify-subscribed-event", NULL, cupsdEventName(event));

      if (sub->user_data_len > 0)
        ippAddOctetString(temp->attrs, IPP_TAG_EVENT_NOTIFICATION,
	                  "notify-user-data", sub->user_data,
			  sub->user_data_len);

      ippAddInteger(temp->attrs, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_INTEGER,
	            "printer-up-time", time(NULL));

      va_start(ap, text);
      vsnprintf(ftext, sizeof(ftext), text, ap);
      va_end(ap);

      ippAddString(temp->attrs, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_TEXT,
	           "notify-text", NULL, ftext);

      if (dest)
      {
       /*
	* Add printer attributes...
	*/

	ippAddString(temp->attrs, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_URI,
	             "notify-printer-uri", NULL, dest->uri);

	ippAddString(temp->attrs, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_NAME,
	             "printer-name", NULL, dest->name);

	ippAddInteger(temp->attrs, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_ENUM,
	              "printer-state", dest->state);

	if (dest->num_reasons == 0)
	  ippAddString(temp->attrs, IPP_TAG_EVENT_NOTIFICATION,
	               IPP_TAG_KEYWORD, "printer-state-reasons", NULL,
		       dest->state == IPP_PRINTER_STOPPED ? "paused" : "none");
	else
	  ippAddStrings(temp->attrs, IPP_TAG_EVENT_NOTIFICATION,
	                IPP_TAG_KEYWORD, "printer-state-reasons",
			dest->num_reasons, NULL,
			(const char * const *)dest->reasons);

	ippAddBoolean(temp->attrs, IPP_TAG_EVENT_NOTIFICATION,
	              "printer-is-accepting-jobs", (char)dest->accepting);
      }

      if (job)
      {
       /*
	* Add job attributes...
	*/

	ippAddInteger(temp->attrs, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_INTEGER,
	              "notify-job-id", job->id);
	ippAddInteger(temp->attrs, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_ENUM,
	              "job-state", job->state_value);

        if ((attr = ippFindAttribute(job->attrs, "job-name",
	                             IPP_TAG_NAME)) != NULL)
	  ippAddString(temp->attrs, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_NAME,
	               "job-name", NULL, attr->values[0].string.text);

	switch (job->state_value)
	{
	  case IPP_JOB_PENDING :
              if (dest && dest->state == IPP_PRINTER_STOPPED)
        	ippAddString(temp->attrs, IPP_TAG_EVENT_NOTIFICATION,
		             IPP_TAG_KEYWORD, "job-state-reasons", NULL,
			     "printer-stopped");
              else
        	ippAddString(temp->attrs, IPP_TAG_EVENT_NOTIFICATION,
		             IPP_TAG_KEYWORD, "job-state-reasons", NULL,
			     "none");
              break;

	  case IPP_JOB_HELD :
              if (ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_KEYWORD) != NULL ||
		  ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_NAME) != NULL)
        	ippAddString(temp->attrs, IPP_TAG_EVENT_NOTIFICATION,
		             IPP_TAG_KEYWORD, "job-state-reasons", NULL,
			     "job-hold-until-specified");
              else
        	ippAddString(temp->attrs, IPP_TAG_EVENT_NOTIFICATION,
		             IPP_TAG_KEYWORD, "job-state-reasons", NULL,
			     "job-incoming");
              break;

	  case IPP_JOB_PROCESSING :
              ippAddString(temp->attrs, IPP_TAG_EVENT_NOTIFICATION,
		           IPP_TAG_KEYWORD, "job-state-reasons", NULL,
			   "job-printing");
              break;

	  case IPP_JOB_STOPPED :
              ippAddString(temp->attrs, IPP_TAG_EVENT_NOTIFICATION,
		           IPP_TAG_KEYWORD, "job-state-reasons", NULL,
			   "job-stopped");
              break;

	  case IPP_JOB_CANCELED :
              ippAddString(temp->attrs, IPP_TAG_EVENT_NOTIFICATION,
		           IPP_TAG_KEYWORD, "job-state-reasons", NULL,
			   "job-canceled-by-user");
              break;

	  case IPP_JOB_ABORTED :
              ippAddString(temp->attrs, IPP_TAG_EVENT_NOTIFICATION,
		           IPP_TAG_KEYWORD, "job-state-reasons", NULL,
			   "aborted-by-system");
              break;

	  case IPP_JOB_COMPLETED :
              ippAddString(temp->attrs, IPP_TAG_EVENT_NOTIFICATION,
		           IPP_TAG_KEYWORD, "job-state-reasons", NULL,
			   "job-completed-successfully");
              break;
	}

	ippAddInteger(temp->attrs, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_INTEGER,
	              "job-impressions-completed",
		      job->sheets ? job->sheets->values[0].integer : 0);
      }

     /*
      * Send the notification for this subscription...
      */

      cupsd_send_notification(sub, temp);
    }
  }

  if (temp)
    cupsdMarkDirty(CUPSD_DIRTY_SUBSCRIPTIONS);
  else
    cupsdLogMessage(CUPSD_LOG_DEBUG, "Discarding unused %s event...",
                    cupsdEventName(event));
}


/*
 * 'cupsdAddSubscription()' - Add a new subscription object.
 */

cupsd_subscription_t *			/* O - New subscription object */
cupsdAddSubscription(
    unsigned        mask,		/* I - Event mask */
    cupsd_printer_t *dest,		/* I - Printer, if any */
    cupsd_job_t     *job,		/* I - Job, if any */
    const char      *uri,		/* I - notify-recipient-uri, if any */
    int             sub_id)		/* I - notify-subscription-id or 0 */
{
  cupsd_subscription_t	*temp;		/* New subscription object */


  cupsdLogMessage(CUPSD_LOG_DEBUG,
                  "cupsdAddSubscription(mask=%x, dest=%p(%s), job=%p(%d), "
		  "uri=\"%s\")",
                  mask, dest, dest ? dest->name : "", job, job ? job->id : 0,
		  uri ? uri : "(null)");

  if (!Subscriptions)
    Subscriptions = cupsArrayNew((cups_array_func_t)cupsd_compare_subscriptions,
                                 NULL);

  if (!Subscriptions)
  {
    cupsdLogMessage(CUPSD_LOG_CRIT,
                    "Unable to allocate memory for subscriptions - %s",
        	    strerror(errno));
    return (NULL);
  }

 /*
  * Limit the number of subscriptions...
  */

  if (MaxSubscriptions > 0 && cupsArrayCount(Subscriptions) >= MaxSubscriptions)
  {
    cupsdLogMessage(CUPSD_LOG_DEBUG,
                    "cupsdAddSubscription: Reached MaxSubscriptions %d "
		    "(count=%d)", MaxSubscriptions,
		    cupsArrayCount(Subscriptions));
    return (NULL);
  }

  if (MaxSubscriptionsPerJob > 0 && job)
  {
    int	count;				/* Number of job subscriptions */

    for (temp = (cupsd_subscription_t *)cupsArrayFirst(Subscriptions),
             count = 0;
         temp;
	 temp = (cupsd_subscription_t *)cupsArrayNext(Subscriptions))
      if (temp->job == job)
        count ++;

    if (count >= MaxSubscriptionsPerJob)
    {
      cupsdLogMessage(CUPSD_LOG_DEBUG,
		      "cupsdAddSubscription: Reached MaxSubscriptionsPerJob %d "
		      "for job #%d (count=%d)", MaxSubscriptionsPerJob,
		      job->id, count);
      return (NULL);
    }
  }

  if (MaxSubscriptionsPerPrinter > 0 && dest)
  {
    int	count;				/* Number of printer subscriptions */

    for (temp = (cupsd_subscription_t *)cupsArrayFirst(Subscriptions),
             count = 0;
         temp;
	 temp = (cupsd_subscription_t *)cupsArrayNext(Subscriptions))
      if (temp->dest == dest)
        count ++;

    if (count >= MaxSubscriptionsPerPrinter)
    {
      cupsdLogMessage(CUPSD_LOG_DEBUG,
		      "cupsdAddSubscription: Reached "
		      "MaxSubscriptionsPerPrinter %d for %s (count=%d)",
		      MaxSubscriptionsPerPrinter, dest->name, count);
      return (NULL);
    }
  }

 /*
  * Allocate memory for this subscription...
  */

  if ((temp = calloc(1, sizeof(cupsd_subscription_t))) == NULL)
  {
    cupsdLogMessage(CUPSD_LOG_CRIT,
                    "Unable to allocate memory for subscription object - %s",
                    strerror(errno));
    return (NULL);
  }

 /*
  * Fill in common data...
  */

  if (sub_id)
  {
    temp->id = sub_id;

    if (sub_id >= NextSubscriptionId)
      NextSubscriptionId = sub_id + 1;
  }
  else
  {
    temp->id = NextSubscriptionId;

    NextSubscriptionId ++;
  }

  temp->mask           = mask;
  temp->dest           = dest;
  temp->job            = job;
  temp->pipe           = -1;
  temp->first_event_id = 1;
  temp->next_event_id  = 1;

  cupsdSetString(&(temp->recipient), uri);

 /*
  * Add the subscription to the array...
  */

  cupsArrayAdd(Subscriptions, temp);

 /*
  * For RSS subscriptions, run the notifier immediately...
  */

  if (uri && !strncmp(uri, "rss:", 4))
    cupsd_start_notifier(temp);

  return (temp);
}


/*
 * 'cupsdDeleteAllSubscriptions()' - Delete all subscriptions.
 */

void
cupsdDeleteAllSubscriptions(void)
{
  cupsd_subscription_t	*sub;		/* Subscription */


  if (!Subscriptions)
    return;

  for (sub = (cupsd_subscription_t *)cupsArrayFirst(Subscriptions);
       sub;
       sub = (cupsd_subscription_t *)cupsArrayNext(Subscriptions))
    cupsdDeleteSubscription(sub, 0);

  cupsArrayDelete(Subscriptions);
  Subscriptions = NULL;
}


/*
 * 'cupsdDeleteSubscription()' - Delete a subscription object.
 */

void
cupsdDeleteSubscription(
    cupsd_subscription_t *sub,		/* I - Subscription object */
    int                  update)	/* I - 1 = update subscriptions.conf */
{
 /*
  * Close the pipe to the notifier as needed...
  */

  if (sub->pipe >= 0)
    close(sub->pipe);

 /*
  * Remove subscription from array...
  */

  cupsArrayRemove(Subscriptions, sub);

 /*
  * Free memory...
  */

  cupsdClearString(&(sub->owner));
  cupsdClearString(&(sub->recipient));

  cupsArrayDelete(sub->events);

  free(sub);

 /*
  * Update the subscriptions as needed...
  */

  if (update)
    cupsdMarkDirty(CUPSD_DIRTY_SUBSCRIPTIONS);
}


/*
 * 'cupsdEventName()' - Return a single event name.
 */

const char *				/* O - Event name */
cupsdEventName(
    cupsd_eventmask_t event)		/* I - Event value */
{
  switch (event)
  {
    default :
        return (NULL);

    case CUPSD_EVENT_PRINTER_RESTARTED :
        return ("printer-restarted");

    case CUPSD_EVENT_PRINTER_SHUTDOWN :
        return ("printer-shutdown");

    case CUPSD_EVENT_PRINTER_STOPPED :
        return ("printer-stopped");

    case CUPSD_EVENT_PRINTER_FINISHINGS_CHANGED :
        return ("printer-finishings-changed");

    case CUPSD_EVENT_PRINTER_MEDIA_CHANGED :
        return ("printer-media-changed");

    case CUPSD_EVENT_PRINTER_ADDED :
        return ("printer-added");

    case CUPSD_EVENT_PRINTER_DELETED :
        return ("printer-deleted");

    case CUPSD_EVENT_PRINTER_MODIFIED :
        return ("printer-modified");

    case CUPSD_EVENT_PRINTER_QUEUE_ORDER_CHANGED :
        return ("printer-queue-order-changed");

    case CUPSD_EVENT_PRINTER_STATE :
    case CUPSD_EVENT_PRINTER_STATE_CHANGED :
        return ("printer-state-changed");

    case CUPSD_EVENT_PRINTER_CONFIG :
    case CUPSD_EVENT_PRINTER_CONFIG_CHANGED :
        return ("printer-config-changed");

    case CUPSD_EVENT_PRINTER_CHANGED :
        return ("printer-changed");

    case CUPSD_EVENT_JOB_CREATED :
        return ("job-created");

    case CUPSD_EVENT_JOB_COMPLETED :
        return ("job-completed");

    case CUPSD_EVENT_JOB_STOPPED :
        return ("job-stopped");

    case CUPSD_EVENT_JOB_CONFIG_CHANGED :
        return ("job-config-changed");

    case CUPSD_EVENT_JOB_PROGRESS :
        return ("job-progress");

    case CUPSD_EVENT_JOB_STATE :
    case CUPSD_EVENT_JOB_STATE_CHANGED :
        return ("job-state-changed");

    case CUPSD_EVENT_SERVER_RESTARTED :
        return ("server-restarted");

    case CUPSD_EVENT_SERVER_STARTED :
        return ("server-started");

    case CUPSD_EVENT_SERVER_STOPPED :
        return ("server-stopped");

    case CUPSD_EVENT_SERVER_AUDIT :
        return ("server-audit");

    case CUPSD_EVENT_ALL :
        return ("all");
  }
}


/*
 * 'cupsdEventValue()' - Return the event mask value for a name.
 */

cupsd_eventmask_t			/* O - Event mask value */
cupsdEventValue(const char *name)	/* I - Name of event */
{
  if (!strcmp(name, "all"))
    return (CUPSD_EVENT_ALL);
  else if (!strcmp(name, "printer-restarted"))
    return (CUPSD_EVENT_PRINTER_RESTARTED);
  else if (!strcmp(name, "printer-shutdown"))
    return (CUPSD_EVENT_PRINTER_SHUTDOWN);
  else if (!strcmp(name, "printer-stopped"))
    return (CUPSD_EVENT_PRINTER_STOPPED);
  else if (!strcmp(name, "printer-finishings-changed"))
    return (CUPSD_EVENT_PRINTER_FINISHINGS_CHANGED);
  else if (!strcmp(name, "printer-media-changed"))
    return (CUPSD_EVENT_PRINTER_MEDIA_CHANGED);
  else if (!strcmp(name, "printer-added"))
    return (CUPSD_EVENT_PRINTER_ADDED);
  else if (!strcmp(name, "printer-deleted"))
    return (CUPSD_EVENT_PRINTER_DELETED);
  else if (!strcmp(name, "printer-modified"))
    return (CUPSD_EVENT_PRINTER_MODIFIED);
  else if (!strcmp(name, "printer-queue-order-changed"))
    return (CUPSD_EVENT_PRINTER_QUEUE_ORDER_CHANGED);
  else if (!strcmp(name, "printer-state-changed"))
    return (CUPSD_EVENT_PRINTER_STATE_CHANGED);
  else if (!strcmp(name, "printer-config-changed"))
    return (CUPSD_EVENT_PRINTER_CONFIG_CHANGED);
  else if (!strcmp(name, "printer-changed"))
    return (CUPSD_EVENT_PRINTER_CHANGED);
  else if (!strcmp(name, "job-created"))
    return (CUPSD_EVENT_JOB_CREATED);
  else if (!strcmp(name, "job-completed"))
    return (CUPSD_EVENT_JOB_COMPLETED);
  else if (!strcmp(name, "job-stopped"))
    return (CUPSD_EVENT_JOB_STOPPED);
  else if (!strcmp(name, "job-config-changed"))
    return (CUPSD_EVENT_JOB_CONFIG_CHANGED);
  else if (!strcmp(name, "job-progress"))
    return (CUPSD_EVENT_JOB_PROGRESS);
  else if (!strcmp(name, "job-state-changed"))
    return (CUPSD_EVENT_JOB_STATE_CHANGED);
  else if (!strcmp(name, "server-restarted"))
    return (CUPSD_EVENT_SERVER_RESTARTED);
  else if (!strcmp(name, "server-started"))
    return (CUPSD_EVENT_SERVER_STARTED);
  else if (!strcmp(name, "server-stopped"))
    return (CUPSD_EVENT_SERVER_STOPPED);
  else if (!strcmp(name, "server-audit"))
    return (CUPSD_EVENT_SERVER_AUDIT);
  else
    return (CUPSD_EVENT_NONE);
}


/*
 * 'cupsdExpireSubscriptions()' - Expire old subscription objects.
 */

void
cupsdExpireSubscriptions(
    cupsd_printer_t *dest,		/* I - Printer, if any */
    cupsd_job_t     *job)		/* I - Job, if any */
{
  cupsd_subscription_t	*sub;		/* Current subscription */
  int			update;		/* Update subscriptions.conf? */
  time_t		curtime;	/* Current time */


  curtime = time(NULL);
  update  = 0;

  cupsdLogMessage(CUPSD_LOG_INFO, "Expiring subscriptions...");

  for (sub = (cupsd_subscription_t *)cupsArrayFirst(Subscriptions);
       sub;
       sub = (cupsd_subscription_t *)cupsArrayNext(Subscriptions))
    if ((!sub->job && !dest && sub->expire && sub->expire <= curtime) ||
        (dest && sub->dest == dest) ||
	(job && sub->job == job))
    {
      cupsdLogMessage(CUPSD_LOG_INFO, "Subscription %d has expired...",
                      sub->id);

      cupsdDeleteSubscription(sub, 0);

      update = 1;
    }

  if (update)
    cupsdMarkDirty(CUPSD_DIRTY_SUBSCRIPTIONS);
}


/*
 * 'cupsdFindSubscription()' - Find a subscription by ID.
 */

cupsd_subscription_t *			/* O - Subscription object */
cupsdFindSubscription(int id)		/* I - Subscription ID */
{
  cupsd_subscription_t	sub;		/* Subscription template */


  sub.id = id;

  return ((cupsd_subscription_t *)cupsArrayFind(Subscriptions, &sub));
}


/*
 * 'cupsdLoadAllSubscriptions()' - Load all subscriptions from the .conf file.
 */

void
cupsdLoadAllSubscriptions(void)
{
  int			i;		/* Looping var */
  cups_file_t		*fp;		/* subscriptions.conf file */
  int			linenum;	/* Current line number */
  char			line[1024],	/* Line from file */
			*value,		/* Pointer to value */
			*valueptr;	/* Pointer into value */
  cupsd_subscription_t	*sub;		/* Current subscription */
  int			hex;		/* Non-zero if reading hex data */
  int			delete_sub;	/* Delete subscription? */


 /*
  * Open the subscriptions.conf file...
  */

  snprintf(line, sizeof(line), "%s/subscriptions.conf", ServerRoot);
  if ((fp = cupsdOpenConfFile(line)) == NULL)
    return;

 /*
  * Read all of the lines from the file...
  */

  linenum    = 0;
  sub        = NULL;
  delete_sub = 0;

  while (cupsFileGetConf(fp, line, sizeof(line), &value, &linenum))
  {
    if (!_cups_strcasecmp(line, "NextSubscriptionId") && value)
    {
     /*
      * NextSubscriptionId NNN
      */

      i = atoi(value);
      if (i >= NextSubscriptionId && i > 0)
        NextSubscriptionId = i;
    }
    else if (!_cups_strcasecmp(line, "<Subscription"))
    {
     /*
      * <Subscription #>
      */

      if (!sub && value && isdigit(value[0] & 255))
      {
        sub = cupsdAddSubscription(CUPSD_EVENT_NONE, NULL, NULL, NULL,
	                           atoi(value));
      }
      else
      {
        cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Syntax error on line %d of subscriptions.conf.",
	                linenum);
        break;
      }
    }
    else if (!_cups_strcasecmp(line, "</Subscription>"))
    {
      if (!sub)
      {
        cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Syntax error on line %d of subscriptions.conf.",
	                linenum);
        break;
      }

      if (delete_sub)
        cupsdDeleteSubscription(sub, 0);

      sub        = NULL;
      delete_sub = 0;
    }
    else if (!sub)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "Syntax error on line %d of subscriptions.conf.",
	              linenum);
    }
    else if (!_cups_strcasecmp(line, "Events"))
    {
     /*
      * Events name
      * Events name name name ...
      */

      if (!value)
      {
	cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Syntax error on line %d of subscriptions.conf.",
	                linenum);
	break;
      }

      while (*value)
      {
       /*
        * Separate event names...
	*/

        for (valueptr = value; !isspace(*valueptr) && *valueptr; valueptr ++);

	while (isspace(*valueptr & 255))
	  *valueptr++ = '\0';

       /*
        * See if the name exists...
	*/

        if ((sub->mask |= cupsdEventValue(value)) == CUPSD_EVENT_NONE)
	{
	  cupsdLogMessage(CUPSD_LOG_ERROR,
	                  "Unknown event name \'%s\' on line %d of subscriptions.conf.",
	                  value, linenum);
	  break;
	}

	value = valueptr;
      }
    }
    else if (!_cups_strcasecmp(line, "Owner"))
    {
     /*
      * Owner
      */

      if (value)
	cupsdSetString(&sub->owner, value);
      else
      {
	cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Syntax error on line %d of subscriptions.conf.",
	                linenum);
	break;
      }
    }
    else if (!_cups_strcasecmp(line, "Recipient"))
    {
     /*
      * Recipient uri
      */

      if (value)
	cupsdSetString(&sub->recipient, value);
      else
      {
	cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Syntax error on line %d of subscriptions.conf.",
	                linenum);
	break;
      }
    }
    else if (!_cups_strcasecmp(line, "JobId"))
    {
     /*
      * JobId #
      */

      if (value && isdigit(*value & 255))
      {
        if ((sub->job = cupsdFindJob(atoi(value))) == NULL)
	{
	  cupsdLogMessage(CUPSD_LOG_ERROR,
	                  "Job %s not found on line %d of subscriptions.conf.",
	                  value, linenum);
	  delete_sub = 1;
	}
      }
      else
      {
	cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Syntax error on line %d of subscriptions.conf.",
	                linenum);
	break;
      }
    }
    else if (!_cups_strcasecmp(line, "PrinterName"))
    {
     /*
      * PrinterName name
      */

      if (value)
      {
        if ((sub->dest = cupsdFindDest(value)) == NULL)
	{
	  cupsdLogMessage(CUPSD_LOG_ERROR,
	                  "Printer \'%s\' not found on line %d of subscriptions.conf.",
	                  value, linenum);
	  delete_sub = 1;
	}
      }
      else
      {
	cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Syntax error on line %d of subscriptions.conf.",
	                linenum);
	break;
      }
    }
    else if (!_cups_strcasecmp(line, "UserData"))
    {
     /*
      * UserData encoded-string
      */

      if (value)
      {
        for (i = 0, valueptr = value, hex = 0; i < 63 && *valueptr; i ++)
	{
	  if (*valueptr == '<' && !hex)
	  {
	    hex = 1;
	    valueptr ++;
	  }

	  if (hex)
	  {
	    if (isxdigit(valueptr[0]) && isxdigit(valueptr[1]))
	    {
	      if (isdigit(valueptr[0]))
	        sub->user_data[i] = (unsigned char)((valueptr[0] - '0') << 4);
	      else
	        sub->user_data[i] = (unsigned char)((tolower(valueptr[0]) - 'a' + 10) << 4);

	      if (isdigit(valueptr[1]))
	        sub->user_data[i] |= valueptr[1] - '0';
	      else
	        sub->user_data[i] |= tolower(valueptr[1]) - 'a' + 10;

              valueptr += 2;

	      if (*valueptr == '>')
	      {
	        hex = 0;
		valueptr ++;
	      }
	    }
	    else
	      break;
	  }
	  else
	    sub->user_data[i] = (unsigned char)*valueptr++;
	}

	if (*valueptr)
	{
	  cupsdLogMessage(CUPSD_LOG_ERROR,
	                  "Bad UserData \'%s\' on line %d of subscriptions.conf.",
	                  value, linenum);
	}
	else
	  sub->user_data_len = i;
      }
      else
      {
	cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Syntax error on line %d of subscriptions.conf.",
	                linenum);
	break;
      }
    }
    else if (!_cups_strcasecmp(line, "LeaseDuration"))
    {
     /*
      * LeaseDuration #
      */

      if (value && isdigit(*value & 255))
      {
        sub->lease  = atoi(value);
        sub->expire = sub->lease ? time(NULL) + sub->lease : 0;
      }
      else
      {
	cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Syntax error on line %d of subscriptions.conf.",
	                linenum);
	break;
      }
    }
    else if (!_cups_strcasecmp(line, "Interval"))
    {
     /*
      * Interval #
      */

      if (value && isdigit(*value & 255))
        sub->interval = atoi(value);
      else
      {
	cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Syntax error on line %d of subscriptions.conf.",
	                linenum);
	break;
      }
    }
    else if (!_cups_strcasecmp(line, "ExpirationTime"))
    {
     /*
      * ExpirationTime #
      */

      if (value && isdigit(*value & 255))
        sub->expire = atoi(value);
      else
      {
	cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Syntax error on line %d of subscriptions.conf.",
	                linenum);
	break;
      }
    }
    else if (!_cups_strcasecmp(line, "NextEventId"))
    {
     /*
      * NextEventId #
      */

      if (value && isdigit(*value & 255))
        sub->next_event_id = sub->first_event_id = atoi(value);
      else
      {
	cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Syntax error on line %d of subscriptions.conf.",
	                linenum);
	break;
      }
    }
    else
    {
     /*
      * Something else we don't understand...
      */

      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "Unknown configuration directive %s on line %d of subscriptions.conf.",
	              line, linenum);
    }
  }

  cupsFileClose(fp);
}


/*
 * 'cupsdSaveAllSubscriptions()' - Save all subscriptions to the .conf file.
 */

void
cupsdSaveAllSubscriptions(void)
{
  int			i;		/* Looping var */
  cups_file_t		*fp;		/* subscriptions.conf file */
  char			filename[1024],	/* subscriptions.conf filename */
			temp[1024];	/* Temporary string */
  cupsd_subscription_t	*sub;		/* Current subscription */
  time_t		curtime;	/* Current time */
  struct tm		*curdate;	/* Current date */
  unsigned		mask;		/* Current event mask */
  const char		*name;		/* Current event name */
  int			hex;		/* Non-zero if we are writing hex data */


 /*
  * Create the subscriptions.conf file...
  */

  snprintf(filename, sizeof(filename), "%s/subscriptions.conf", ServerRoot);

  if ((fp = cupsdCreateConfFile(filename, ConfigFilePerm)) == NULL)
    return;

  cupsdLogMessage(CUPSD_LOG_INFO, "Saving subscriptions.conf...");

 /*
  * Write a small header to the file...
  */

  curtime = time(NULL);
  curdate = localtime(&curtime);
  strftime(temp, sizeof(temp) - 1, "%Y-%m-%d %H:%M", curdate);

  cupsFilePuts(fp, "# Subscription configuration file for " CUPS_SVERSION "\n");
  cupsFilePrintf(fp, "# Written by cupsd on %s\n", temp);

  cupsFilePrintf(fp, "NextSubscriptionId %d\n", NextSubscriptionId);

 /*
  * Write every subscription known to the system...
  */

  for (sub = (cupsd_subscription_t *)cupsArrayFirst(Subscriptions);
       sub;
       sub = (cupsd_subscription_t *)cupsArrayNext(Subscriptions))
  {
    cupsFilePrintf(fp, "<Subscription %d>\n", sub->id);

    if ((name = cupsdEventName((cupsd_eventmask_t)sub->mask)) != NULL)
    {
     /*
      * Simple event list...
      */

      cupsFilePrintf(fp, "Events %s\n", name);
    }
    else
    {
     /*
      * Complex event list...
      */

      cupsFilePuts(fp, "Events");

      for (mask = 1; mask < CUPSD_EVENT_ALL; mask <<= 1)
        if (sub->mask & mask)
	  cupsFilePrintf(fp, " %s", cupsdEventName((cupsd_eventmask_t)mask));

      cupsFilePuts(fp, "\n");
    }

    if (sub->owner)
      cupsFilePrintf(fp, "Owner %s\n", sub->owner);
    if (sub->recipient)
      cupsFilePrintf(fp, "Recipient %s\n", sub->recipient);
    if (sub->job)
      cupsFilePrintf(fp, "JobId %d\n", sub->job->id);
    if (sub->dest)
      cupsFilePrintf(fp, "PrinterName %s\n", sub->dest->name);

    if (sub->user_data_len > 0)
    {
      cupsFilePuts(fp, "UserData ");

      for (i = 0, hex = 0; i < sub->user_data_len; i ++)
      {
        if (sub->user_data[i] < ' ' ||
	    sub->user_data[i] > 0x7f ||
	    sub->user_data[i] == '<')
	{
	  if (!hex)
	  {
	    cupsFilePrintf(fp, "<%02X", sub->user_data[i]);
	    hex = 1;
	  }
	  else
	    cupsFilePrintf(fp, "%02X", sub->user_data[i]);
	}
	else
	{
	  if (hex)
	  {
	    cupsFilePrintf(fp, ">%c", sub->user_data[i]);
	    hex = 0;
	  }
	  else
	    cupsFilePutChar(fp, sub->user_data[i]);
	}
      }

      if (hex)
        cupsFilePuts(fp, ">\n");
      else
        cupsFilePutChar(fp, '\n');
    }

    cupsFilePrintf(fp, "LeaseDuration %d\n", sub->lease);
    cupsFilePrintf(fp, "Interval %d\n", sub->interval);
    cupsFilePrintf(fp, "ExpirationTime %ld\n", (long)sub->expire);
    cupsFilePrintf(fp, "NextEventId %d\n", sub->next_event_id);

    cupsFilePuts(fp, "</Subscription>\n");
  }

  cupsdCloseCreatedConfFile(fp, filename);
}


/*
 * 'cupsdStopAllNotifiers()' - Stop all notifier processes.
 */

void
cupsdStopAllNotifiers(void)
{
  cupsd_subscription_t	*sub;		/* Current subscription */


 /*
  * See if we have started any notifiers...
  */

  if (!NotifierStatusBuffer)
    return;

 /*
  * Yes, kill any processes that are left...
  */

  for (sub = (cupsd_subscription_t *)cupsArrayFirst(Subscriptions);
       sub;
       sub = (cupsd_subscription_t *)cupsArrayNext(Subscriptions))
    if (sub->pid)
    {
      cupsdEndProcess(sub->pid, 0);

      close(sub->pipe);
      sub->pipe = -1;
    }

 /*
  * Close the status pipes...
  */

  if (NotifierPipes[0] >= 0)
  {
    cupsdRemoveSelect(NotifierPipes[0]);

    cupsdStatBufDelete(NotifierStatusBuffer);

    close(NotifierPipes[0]);
    close(NotifierPipes[1]);

    NotifierPipes[0] = -1;
    NotifierPipes[1] = -1;
    NotifierStatusBuffer = NULL;
  }
}


/*
 * 'cupsd_compare_subscriptions()' - Compare two subscriptions.
 */

static int				/* O - Result of comparison */
cupsd_compare_subscriptions(
    cupsd_subscription_t *first,	/* I - First subscription object */
    cupsd_subscription_t *second,	/* I - Second subscription object */
    void                 *unused)	/* I - Unused user data pointer */
{
  (void)unused;

  return (first->id - second->id);
}


/*
 * 'cupsd_delete_event()' - Delete a single event...
 *
 * Oldest events must be deleted first, otherwise the subscription cache
 * flushing code will not work properly.
 */

static void
cupsd_delete_event(cupsd_event_t *event)/* I - Event to delete */
{
 /*
  * Free memory...
  */

  ippDelete(event->attrs);
  free(event);
}


#ifdef HAVE_DBUS
/*
 * 'cupsd_send_dbus()' - Send a DBUS notification...
 */

static void
cupsd_send_dbus(cupsd_eventmask_t event,/* I - Event to send */
                cupsd_printer_t   *dest,/* I - Destination, if any */
                cupsd_job_t       *job)	/* I - Job, if any */
{
  DBusError		error;		/* Error, if any */
  DBusMessage		*message;	/* Message to send */
  DBusMessageIter	iter;		/* Iterator for message data */
  const char		*what;		/* What to send */
  static DBusConnection	*con = NULL;	/* Connection to DBUS server */


 /*
  * Figure out what to send, if anything...
  */

  if (event & CUPSD_EVENT_PRINTER_ADDED)
    what = "PrinterAdded";
  else if (event & CUPSD_EVENT_PRINTER_DELETED)
    what = "PrinterRemoved";
  else if (event & CUPSD_EVENT_PRINTER_CHANGED)
    what = "QueueChanged";
  else if (event & CUPSD_EVENT_JOB_CREATED)
    what = "JobQueuedLocal";
  else if ((event & CUPSD_EVENT_JOB_STATE) && job &&
           job->state_value == IPP_JOB_PROCESSING)
    what = "JobStartedLocal";
  else
    return;

 /*
  * Verify connection to DBUS server...
  */

  if (con && !dbus_connection_get_is_connected(con))
  {
    dbus_connection_unref(con);
    con = NULL;
  }

  if (!con)
  {
    dbus_error_init(&error);

    con = dbus_bus_get(getuid() ? DBUS_BUS_SESSION : DBUS_BUS_SYSTEM, &error);
    if (!con)
    {
      dbus_error_free(&error);
      return;
    }
  }

 /*
  * Create and send the new message...
  */

  message = dbus_message_new_signal("/com/redhat/PrinterSpooler",
				    "com.redhat.PrinterSpooler", what);

  dbus_message_append_iter_init(message, &iter);
  if (dest)
    dbus_message_iter_append_string(&iter, dest->name);
  if (job)
  {
    dbus_message_iter_append_uint32(&iter, job->id);
    dbus_message_iter_append_string(&iter, job->username);
  }

  dbus_connection_send(con, message, NULL);
  dbus_connection_flush(con);
  dbus_message_unref(message);
}
#endif /* HAVE_DBUS */


/*
 * 'cupsd_send_notification()' - Send a notification for the specified event.
 */

static void
cupsd_send_notification(
    cupsd_subscription_t *sub,		/* I - Subscription object */
    cupsd_event_t        *event)	/* I - Event to send */
{
  ipp_state_t	state;			/* IPP event state */


  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "cupsd_send_notification(sub=%p(%d), event=%p(%s))",
                  sub, sub->id, event, cupsdEventName(event->event));

 /*
  * Allocate the events array as needed...
  */

  if (!sub->events)
  {
    sub->events = cupsArrayNew3((cups_array_func_t)NULL, NULL,
                                (cups_ahash_func_t)NULL, 0,
				(cups_acopy_func_t)NULL,
				(cups_afree_func_t)cupsd_delete_event);

    if (!sub->events)
    {
      cupsdLogMessage(CUPSD_LOG_CRIT,
                      "Unable to allocate memory for subscription #%d!",
                      sub->id);
      return;
    }
  }

 /*
  * Purge an old event as needed...
  */

  if (cupsArrayCount(sub->events) >= MaxEvents)
  {
   /*
    * Purge the oldest event in the cache...
    */

    cupsArrayRemove(sub->events, cupsArrayFirst(sub->events));

    sub->first_event_id ++;
  }

 /*
  * Add the event to the subscription.  Since the events array is
  * always MaxEvents in length, and since we will have already
  * removed an event from the subscription cache if we hit the
  * event cache limit, we don't need to check for overflow here...
  */

  cupsArrayAdd(sub->events, event);

 /*
  * Deliver the event...
  */

  if (sub->recipient)
  {
    for (;;)
    {
      if (sub->pipe < 0)
	cupsd_start_notifier(sub);

      cupsdLogMessage(CUPSD_LOG_DEBUG2, "sub->pipe=%d", sub->pipe);

      if (sub->pipe < 0)
	break;

      event->attrs->state = IPP_IDLE;

      while ((state = ippWriteFile(sub->pipe, event->attrs)) != IPP_DATA)
        if (state == IPP_ERROR)
	  break;

      if (state == IPP_ERROR)
      {
        if (errno == EPIPE)
	{
	 /*
	  * Notifier died, try restarting it...
	  */

          cupsdLogMessage(CUPSD_LOG_WARN,
	                  "Notifier for subscription %d (%s) went away, "
			  "retrying!",
			  sub->id, sub->recipient);
	  cupsdEndProcess(sub->pid, 0);

	  close(sub->pipe);
	  sub->pipe = -1;
          continue;
	}

        cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Unable to send event for subscription %d (%s)!",
			sub->id, sub->recipient);
      }

     /*
      * If we get this far, break out of the loop...
      */

      break;
    }
  }

 /*
  * Bump the event sequence number...
  */

  sub->next_event_id ++;
}


/*
 * 'cupsd_start_notifier()' - Start a notifier subprocess...
 */

static void
cupsd_start_notifier(
    cupsd_subscription_t *sub)		/* I - Subscription object */
{
  int	pid;				/* Notifier process ID */
  int	fds[2];				/* Pipe file descriptors */
  char	*argv[4],			/* Command-line arguments */
	*envp[MAX_ENV],			/* Environment variables */
	user_data[128],			/* Base-64 encoded user data */
	scheme[256],			/* notify-recipient-uri scheme */
	*ptr,				/* Pointer into scheme */
	command[1024];			/* Notifier command */


 /*
  * Extract the scheme name from the recipient URI and point to the
  * notifier program...
  */

  strlcpy(scheme, sub->recipient, sizeof(scheme));
  if ((ptr = strchr(scheme, ':')) != NULL)
    *ptr = '\0';

  snprintf(command, sizeof(command), "%s/notifier/%s", ServerBin, scheme);

 /*
  * Base-64 encode the user data...
  */

  httpEncode64_2(user_data, sizeof(user_data), (char *)sub->user_data,
                 sub->user_data_len);

 /*
  * Setup the argument array...
  */

  argv[0] = command;
  argv[1] = sub->recipient;
  argv[2] = user_data;
  argv[3] = NULL;

 /*
  * Setup the environment...
  */

  cupsdLoadEnv(envp, (int)(sizeof(envp) / sizeof(envp[0])));

 /*
  * Create pipes as needed...
  */

  if (!NotifierStatusBuffer)
  {
   /*
    * Create the status pipe...
    */

    if (cupsdOpenPipe(NotifierPipes))
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "Unable to create pipes for notifier status - %s",
		      strerror(errno));
      return;
    }

    NotifierStatusBuffer = cupsdStatBufNew(NotifierPipes[0], "[Notifier]");

    cupsdAddSelect(NotifierPipes[0], (cupsd_selfunc_t)cupsd_update_notifier,
                   NULL, NULL);
  }

  if (cupsdOpenPipe(fds))
  {
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "Unable to create pipes for notifier %s - %s",
		    scheme, strerror(errno));
    return;
  }

 /*
  * Make sure the delivery pipe is non-blocking...
  */

  fcntl(fds[1], F_SETFL, fcntl(fds[1], F_GETFL) | O_NONBLOCK);

 /*
  * Create the notifier process...
  */

  if (cupsdStartProcess(command, argv, envp, fds[0], -1, NotifierPipes[1],
			-1, -1, 0, DefaultProfile, NULL, &pid) < 0)
  {
   /*
    * Error - can't fork!
    */

    cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to fork for notifier %s - %s",
                    scheme, strerror(errno));

    cupsdClosePipe(fds);
  }
  else
  {
   /*
    * Fork successful - return the PID...
    */

    cupsdLogMessage(CUPSD_LOG_DEBUG, "Notifier %s started - PID = %d",
                    scheme, pid);

    sub->pid    = pid;
    sub->pipe   = fds[1];
    sub->status = 0;

    close(fds[0]);
  }
}


/*
 * 'cupsd_update_notifier()' - Read messages from notifiers.
 */

void
cupsd_update_notifier(void)
{
  char		message[1024];		/* Pointer to message text */
  int		loglevel;		/* Log level for message */


  while (cupsdStatBufUpdate(NotifierStatusBuffer, &loglevel,
                            message, sizeof(message)))
  {
    if (loglevel == CUPSD_LOG_INFO)
      cupsdLogMessage(CUPSD_LOG_INFO, "%s", message);

    if (!strchr(NotifierStatusBuffer->buffer, '\n'))
      break;
  }
}


/*
 * End of "$Id: subscriptions.c 13040 2016-01-11 20:29:13Z msweet $".
 */
