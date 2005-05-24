/*
 * "$Id$"
 *
 *   Subscription routines for the Common UNIX Printing System (CUPS) scheduler.
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
 *   cupsdAddEvent()               - Add an event to the global event cache.
 *   cupsdAddSubscription()        - Add a new subscription object.
 *   cupsdDeleteAllEvents()        - Delete all cached events.
 *   cupsdDeleteAllSubscriptions() - Delete all subscriptions.
 *   cupsdDeleteSubscription()     - Delete a subscription object.
 *   cupsdFindSubscription()       - Find a subscription by ID.
 *   cupsdExpireSubscriptions()    - Expire old subscription objects.
 *   cupsdLoadAllSubscriptions()   - Load all subscriptions from the .conf file.
 *   cupsdSaveAllSubscriptions()   - Save all subscriptions to the .conf file.
 *   cupsdSendNotification()       - Send a notification for the specified event.
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"


/*
 * Local functions...
 */

static void	cupsd_delete_event(cupsd_event_t *event);


/*
 * 'cupsdAddEvent()' - Add an event to the global event cache.
 */

void
cupsdAddEvent(
    cupsd_eventmask_t event,		/* I - Event */
    printer_t         *dest,		/* I - Printer associated with event */
    job_t             *job,		/* I - Job associated with event */
    const char        *text,		/* I - Notification text */
    ...)				/* I - Additional arguments as needed */
{
  va_list		ap;		/* Pointer to additional arguments */
  char			ftext[1024];	/* Formatted text buffer */
  cupsd_event_t		*temp;		/* New event pointer */
  int			i;		/* Looping var */
  cupsd_subscription_t	*sub;		/* Current subscription */


 /*
  * Return if we aren't keeping events...
  */

  if (MaxEvents <= 0)
  {
    LogMessage(L_WARN, "cupsdAddEvent: Discarding %s event since MaxEvents is %d!",
               cupsdEventName(event), MaxEvents);
    return;
  }

 /*
  * Allocate memory for the event cache as needed...
  */

  if (!Events)
  {
    Events    = calloc(MaxEvents, sizeof(cupsd_event_t *));
    NumEvents = 0;

    if (!Events)
    {
      LogMessage(L_CRIT, "Unable to allocate memory for event cache - %s",
        	 strerror(errno));
      return;
    }
  }

 /*
  * Then loop through the subscriptions and add the event to the corresponding
  * caches...
  */

  for (i = 0, temp = NULL; i < NumSubscriptions; i ++)
  {
   /*
    * Check if this subscription requires this event...
    */

    sub = Subscriptions[i];

    if ((sub->mask & event) != 0 &&
        (sub->dest == dest || !sub->dest) &&
	(sub->job == job || !sub->job))
    {
     /*
      * Need this event...
      */

      if (!temp)
      {
       /*
	* Create the new event record...
	*/

	if ((temp = (cupsd_event_t *)calloc(1, sizeof(cupsd_event_t))) == NULL)
	{
	  LogMessage(L_CRIT, "Unable to allocate memory for event - %s",
        	     strerror(errno));
	  return;
	}

	temp->event = event;
	temp->time  = time(NULL);
	temp->attrs = ippNew();
	temp->job   = job;
	temp->dest  = dest;

        ippAddInteger(temp->attrs, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_INTEGER,
	              "notify-subscription-id", sub->id);

	ippAddString(temp->attrs, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_KEYWORD,
	             "notify-subscribed-event", NULL, cupsdEventName(event));

        ippAddInteger(temp->attrs, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_INTEGER,
	              "printer-up-time", time(NULL));

        va_start(ap, text);
	vsnprintf(ftext, sizeof(ftext), text, ap);
	va_end(ap);

	ippAddString(temp->attrs, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_KEYWORD,
	             "notify-text", NULL, ftext);

        if (dest)
	{
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
	                "printer-is-accepting-jobs", dest->accepting);
        }

        if (job)
	{
	  ippAddInteger(temp->attrs, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_INTEGER,
	                "job-id", job->id);
	  ippAddInteger(temp->attrs, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_ENUM,
	                "job-state", (int)job->state);

	  switch (job->state->values[0].integer)
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

	    case IPP_JOB_CANCELLED :
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
	* Purge an old event as needed...
	*/

	if (NumEvents >= MaxEvents)
	{
	 /*
	  * Purge the oldest event in the cache...
	  */

	  cupsd_delete_event(Events[0]);

	  NumEvents --;

	  memmove(Events, Events + 1, NumEvents * sizeof(cupsd_event_t *));
	}

       /*
	* Add the new event to the main cache...
	*/

	Events[NumEvents] = temp;
	NumEvents ++;
      }

     /*
      * Send the notification for this subscription...
      */

      cupsdSendNotification(sub, temp);
    }
  }

  if (temp)
    cupsdSaveAllSubscriptions();
  else
    LogMessage(L_DEBUG2, "Discarding unused %s event...",
               cupsdEventName(event));
}


/*
 * 'cupsdAddSubscription()' - Add a new subscription object.
 */

cupsd_subscription_t *			/* O - New subscription object */
cupsdAddSubscription(
    unsigned   mask,			/* I - Event mask */
    printer_t  *dest,			/* I - Printer, if any */
    job_t      *job,			/* I - Job, if any */
    const char *uri)			/* I - notify-recipient-uri, if any */
{
  cupsd_subscription_t	*temp;		/* New subscription object */


  if (!Subscriptions)
  {
   /*
    * Allocate memory for the subscription array...
    */

    Subscriptions    = calloc(MaxSubscriptions, sizeof(cupsd_subscription_t *));
    NumSubscriptions = 0;

    if (!Subscriptions)
    {
      LogMessage(L_CRIT, "Unable to allocate memory for subscriptions - %s",
        	 strerror(errno));
      return (NULL);
    }
  }

 /*
  * Limit the number of subscriptions...
  */

  if (NumSubscriptions >= MaxSubscriptions)
    return (NULL);

 /*
  * Allocate memory for this subscription...
  */

  if ((temp = calloc(1, sizeof(cupsd_subscription_t))) == NULL)
  {
    LogMessage(L_CRIT, "Unable to allocate memory for subscription object - %s",
               strerror(errno));
    return (NULL);
  }

 /*
  * Fill in common data...
  */

  temp->id             = NextSubscriptionId;
  temp->mask           = mask;
  temp->job            = job;
  temp->dest           = dest;
  temp->first_event_id = 1;
  temp->next_event_id  = 1;

  SetString(&(temp->recipient), uri);

 /*
  * Add the subscription to the array...
  */

  Subscriptions[NumSubscriptions] = temp;
  NumSubscriptions ++;
  NextSubscriptionId ++;

  return (temp);
}


/*
 * 'cupsdDeleteAllEvents()' - Delete all cached events.
 */

void
cupsdDeleteAllEvents(void)
{
  int	i;				/* Looping var */


  if (MaxEvents <= 0 || !Events)
    return;

  for (i = 0; i < NumEvents; i ++)
    cupsd_delete_event(Events[i]);

  free(Events);
  Events = NULL;
}


/*
 * 'cupsdDeleteAllSubscriptions()' - Delete all subscriptions.
 */

void
cupsdDeleteAllSubscriptions(void)
{
  int	i;				/* Looping var */


  if (!Subscriptions)
    return;

  for (i = 0; i < NumSubscriptions; i ++)
    cupsdDeleteSubscription(Subscriptions[i], 0);

  free(Subscriptions);
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
  int	i;				/* Looping var */


 /*
  * Free memory...
  */

  if (sub->recipient)
    free(sub->recipient);

  if (sub->events)
    free(sub->events);

  free(sub);

 /*
  * Remove subscription from array...
  */

  for (i = 0; i < NumSubscriptions; i ++)
    if (Subscriptions[i] == sub)
    {
     /*
      * Remove from array and stop...
      */

      NumSubscriptions --;

      if (i < NumSubscriptions)
        memmove(Subscriptions + i, Subscriptions + i + 1,
	        (NumSubscriptions - i) * sizeof(cupsd_subscription_t *));
      break;
    }

 /*
  * Update the subscriptions as needed...
  */

  if (update)
    cupsdSaveAllSubscriptions();
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

    case CUPSD_EVENT_PRINTER_STATE_CHANGED :
        return ("printer-state-changed");

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
    printer_t *dest,			/* I - Printer, if any */
    job_t     *job)			/* I - Job, if any */
{
  int			i;		/* Looping var */
  cupsd_subscription_t	*sub;		/* Current subscription */
  int			update;		/* Update subscriptions.conf? */
  time_t		curtime;	/* Current time */


  curtime = time(NULL);

  for (i = 0, update = 0; i < NumSubscriptions; i ++)
  {
    sub = Subscriptions[i];

    if (sub->expire <= curtime ||
        (dest && sub->dest == dest) ||
	(job && sub->job == job))
    {
      LogMessage(L_INFO, "Subscription %d has expired...", sub->id);

      cupsdDeleteSubscription(sub, 0);

      update = 1;
    }
  }

  if (update)
    cupsdSaveAllSubscriptions();
}


/*
 * 'cupsdFindSubscription()' - Find a subscription by ID.
 */

cupsd_subscription_t *			/* O - Subscription object */
cupsdFindSubscription(int id)		/* I - Subscription ID */
{
  int			left,		/* Left side of binary search */
			center,		/* Center of binary search */
			right;		/* Right side of binary search */
  cupsd_subscription_t	*sub;		/* Current subscription */


 /*
  * Return early if we have no subscriptions...
  */

  if (NumSubscriptions == 0)
    return (NULL);

 /*
  * Otherwise do a binary search for the subscription ID...
  */

  for (left = 0, right = NumSubscriptions - 1; (right - left) > 1;)
  {
    center = (left + right) / 2;
    sub    = Subscriptions[center];

    if (sub->id == id)
      return (sub);
    else if (sub->id < id)
      left = center;
    else
      right = center;
  }

  if (Subscriptions[left]->id == id)
    return (Subscriptions[left]);
  else if (Subscriptions[right]->id == id)
    return (Subscriptions[right]);
  else
    return (NULL);
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
			name[256],	/* Parameter name */
			*value,		/* Pointer to value */
			*valueptr;	/* Pointer into value */
  cupsd_subscription_t	*sub;		/* Current subscription */
  int			hex;		/* Non-zero if reading hex data */
  int			delete_sub;	/* Delete subscription? */


 /*
  * Open the subscriptions.conf file...
  */

  snprintf(line, sizeof(line), "%s/subscriptions.conf", ServerRoot);
  if ((fp = cupsFileOpen(line, "r")) == NULL)
  {
    LogMessage(L_ERROR, "LoadAllSubscriptions: Unable to open %s - %s", line,
               strerror(errno));
    return;
  }

 /*
  * Read all of the lines from the file...
  */

  linenum    = 0;
  sub        = NULL;
  delete_sub = 0;

  while (cupsFileGetConf(fp, line, sizeof(line), &value, &linenum))
  {
    if (!strcasecmp(name, "<Subscription"))
    {
     /*
      * <Subscription #>
      */

      if (!sub && value && isdigit(value[0] & 255))
      {
        sub     = cupsdAddSubscription(CUPSD_EVENT_NONE, NULL, NULL, NULL);
	sub->id = atoi(value);
      }
      else
      {
        LogMessage(L_ERROR, "Syntax error on line %d of subscriptions.conf.",
	           linenum);
        return;
      }
    }
    else if (!strcasecmp(name, "</Subscription>"))
    {
      if (!sub)
      {
        LogMessage(L_ERROR, "Syntax error on line %d of subscriptions.conf.",
	           linenum);
        return;
      }

      if (delete_sub)
        cupsdDeleteSubscription(sub, 0);

      sub        = NULL;
      delete_sub = 0;
    }
    else if (!sub)
    {
      LogMessage(L_ERROR, "Syntax error on line %d of subscriptions.conf.",
	         linenum);
      return;
    }
    else if (!strcasecmp(name, "Events"))
    {
     /*
      * Events name
      * Events name name name ...
      */

      if (!value)
      {
	LogMessage(L_ERROR, "Syntax error on line %d of subscriptions.conf.",
	           linenum);
	return;
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
	  LogMessage(L_ERROR, "Unknown event name \'%s\' on line %d of subscriptions.conf.",
	             value, linenum);
	  return;
	}

	value = valueptr;
      }
    }
    else if (!strcasecmp(name, "Recipient"))
    {
     /*
      * Recipient uri
      */

      if (value)
	SetString(&sub->recipient, value);
      else
      {
	LogMessage(L_ERROR, "Syntax error on line %d of subscriptions.conf.",
	           linenum);
	return;
      }
    }
    else if (!strcasecmp(name, "JobId"))
    {
     /*
      * JobId #
      */

      if (value && isdigit(*value & 255))
      {
        if ((sub->job = FindJob(atoi(value))) == NULL)
	{
	  LogMessage(L_ERROR, "Job %s not found on line %d of subscriptions.conf.",
	             value, linenum);
	  delete_sub = 1;
	}
      }
      else
      {
	LogMessage(L_ERROR, "Syntax error on line %d of subscriptions.conf.",
	           linenum);
	return;
      }
    }
    else if (!strcasecmp(name, "PrinterName"))
    {
     /*
      * PrinterName name
      */

      if (value)
      {
        if ((sub->dest = FindDest(value)) == NULL)
	{
	  LogMessage(L_ERROR, "Printer \'%s\' not found on line %d of subscriptions.conf.",
	             value, linenum);
	  delete_sub = 1;
	}
      }
      else
      {
	LogMessage(L_ERROR, "Syntax error on line %d of subscriptions.conf.",
	           linenum);
	return;
      }
    }
    else if (!strcasecmp(name, "UserData"))
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
	        sub->user_data[i] = (valueptr[0] - '0') << 4;
	      else
	        sub->user_data[i] = (tolower(valueptr[0]) - 'a' + 10) << 4;

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
	    sub->user_data[i] = *valueptr++;
	}

	if (*valueptr)
	{
	  LogMessage(L_ERROR, "Bad UserData \'%s\' on line %d of subscriptions.conf.",
	             value, linenum);
	}
	else
	  sub->user_data_len = i;
      }
      else
      {
	LogMessage(L_ERROR, "Syntax error on line %d of subscriptions.conf.",
	           linenum);
	return;
      }
    }
    else if (!strcasecmp(name, "LeaseTime"))
    {
     /*
      * LeaseTime #
      */

      if (value && isdigit(*value & 255))
        sub->lease = atoi(value);
      else
      {
	LogMessage(L_ERROR, "Syntax error on line %d of subscriptions.conf.",
	           linenum);
	return;
      }
    }
    else if (!strcasecmp(name, "Interval"))
    {
     /*
      * Interval #
      */

      if (value && isdigit(*value & 255))
        sub->interval = atoi(value);
      else
      {
	LogMessage(L_ERROR, "Syntax error on line %d of subscriptions.conf.",
	           linenum);
	return;
      }
    }
    else if (!strcasecmp(name, "ExpirationTime"))
    {
     /*
      * ExpirationTime #
      */

      if (value && isdigit(*value & 255))
        sub->expire = atoi(value);
      else
      {
	LogMessage(L_ERROR, "Syntax error on line %d of subscriptions.conf.",
	           linenum);
	return;
      }
    }
    else if (!strcasecmp(name, "NextEventId"))
    {
     /*
      * NextEventId #
      */

      if (value && isdigit(*value & 255))
        sub->next_event_id = sub->first_event_id = atoi(value);
      else
      {
	LogMessage(L_ERROR, "Syntax error on line %d of subscriptions.conf.",
	           linenum);
	return;
      }
    }
    else
    {
     /*
      * Something else we don't understand...
      */

      LogMessage(L_ERROR, "Unknown configuration directive %s on line %d of subscriptions.conf.",
	         name, linenum);
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
  int			i, j;		/* Looping vars */
  cups_file_t		*fp;		/* subscriptions.conf file */
  char			temp[1024];	/* Temporary string */
  char			backup[1024];	/* subscriptions.conf.O file */
  cupsd_subscription_t	*sub;		/* Current subscription */
  time_t		curtime;	/* Current time */
  struct tm		*curdate;	/* Current date */
  unsigned		mask;		/* Current event mask */
  const char		*name;		/* Current event name */
  int			hex;		/* Non-zero if we are writing hex data */


 /*
  * Create the subscriptions.conf file...
  */

  snprintf(temp, sizeof(temp), "%s/subscriptions.conf", ServerRoot);
  snprintf(backup, sizeof(backup), "%s/subscriptions.conf.O", ServerRoot);

  if (rename(temp, backup))
    LogMessage(L_ERROR, "Unable to backup subscriptions.conf - %s",
               strerror(errno));

  if ((fp = cupsFileOpen(temp, "w")) == NULL)
  {
    LogMessage(L_ERROR, "Unable to save subscriptions.conf - %s",
               strerror(errno));

    if (rename(backup, temp))
      LogMessage(L_ERROR, "Unable to restore subscriptions.conf - %s",
                 strerror(errno));
    return;
  }
  else
    LogMessage(L_INFO, "Saving subscriptions.conf...");

 /*
  * Restrict access to the file...
  */

  fchown(cupsFileNumber(fp), getuid(), Group);
  fchmod(cupsFileNumber(fp), ConfigFilePerm);

 /*
  * Write a small header to the file...
  */

  curtime = time(NULL);
  curdate = localtime(&curtime);
  strftime(temp, sizeof(temp) - 1, CUPS_STRFTIME_FORMAT, curdate);

  cupsFilePuts(fp, "# Subscription configuration file for " CUPS_SVERSION "\n");
  cupsFilePrintf(fp, "# Written by cupsd on %s\n", temp);

 /*
  * Write every subscription known to the system...
  */

  for (i = 0; i < NumSubscriptions; i ++)
  {
    sub = Subscriptions[i];

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

    if (sub->recipient)
      cupsFilePrintf(fp, "Recipient %s\n", sub->recipient);
    if (sub->job)
      cupsFilePrintf(fp, "JobId %d\n", sub->job->id);
    if (sub->dest)
      cupsFilePrintf(fp, "PrinterName %s\n", sub->dest->name);

    if (sub->user_data_len > 0)
    {
      cupsFilePuts(fp, "UserData ");

      for (j = 0, hex = 0; j < sub->user_data_len; j ++)
      {
        if (sub->user_data[j] < ' ' ||
	    sub->user_data[j] > 0x7f ||
	    sub->user_data[j] == '<')
	{
	  if (!hex)
	  {
	    cupsFilePrintf(fp, "<%02X", sub->user_data[j]);
	    hex = 1;
	  }
	  else
	    cupsFilePrintf(fp, "%02X", sub->user_data[j]);
	}
	else
	{
	  if (hex)
	  {
	    cupsFilePrintf(fp, ">%c", sub->user_data[j]);
	    hex = 0;
	  }
	  else
	    cupsFilePutChar(fp, sub->user_data[j]);
	}
      }

      if (hex)
        cupsFilePuts(fp, ">\n");
      else
        cupsFilePutChar(fp, '\n');
    }

    cupsFilePrintf(fp, "LeaseTime %d\n", sub->lease);
    cupsFilePrintf(fp, "Interval %d\n", sub->interval);
    cupsFilePrintf(fp, "ExpirationTime %ld\n", (long)sub->expire);
    cupsFilePrintf(fp, "NextEventId %d\n", sub->next_event_id);

    cupsFilePuts(fp, "</Subscription>\n");
  }

  cupsFileClose(fp);
}


/*
 * 'cupsdSendNotification()' - Send a notification for the specified event.
 */

void
cupsdSendNotification(
    cupsd_subscription_t *sub,		/* I - Subscription object */
    cupsd_event_t        *event)	/* I - Event to send */
{
  LogMessage(L_DEBUG, "cupsdSendNotification(sub=%p(%d), event=%p(%s))\n",
             sub, sub->id, event, cupsdEventName(event->event));

 /*
  * Add the event to the subscription.  Since the events array is
  * always MaxEvents in length, and since we will have already
  * removed an event from the subscription cache if we hit the
  * event cache limit, we don't need to check for overflow here...
  */

  sub->events[sub->num_events] = event;
  sub->num_events ++;

 /*
  * Deliver the event...
  */

  /**** TODO ****/

 /*
  * Bump the event sequence number...
  */

  sub->next_event_id ++;
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
  int			i;		/* Looping var */
  cupsd_subscription_t	*sub;		/* Current subscription */


 /*
  * Loop through the subscriptions and look for the event in the cache...
  */

  for (i = 0; i < NumSubscriptions; i ++)
  {
   /*
    * Only check the first event in the subscription cache, since the
    * caller will only delete the oldest event in the cache...
    */

    sub = Subscriptions[i];

    if (sub->num_events > 0 && sub->events[0] == event)
    {
     /*
      * Remove this event...
      */

      sub->num_events --;
      sub->first_event_id ++;

      if (sub->num_events > 0)
      {
       /*
        * Shift other events upward in cache...
	*/

        memmove(sub->events, sub->events + 1,
	        sub->num_events * sizeof(cupsd_event_t *));
      }
    }
  }

 /*
  * Free memory...
  */

  ippDelete(event->attrs);
  free(event);
}


/*
 * End of "$Id$".
 */
