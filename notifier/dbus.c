/*
 * "$Id$"
 *
 *   D-Bus notifier for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2008 by Apple Inc.
 *   Copyright (C) 2007 Red Hat, Inc.
 *   Copyright (C) 2007 Tim Waugh <twaugh@redhat.com>
 *   Copyright 1997-2005 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
 */

/*
 * Include necessary headers...
 */

#include <cups/cups.h>
#include <cups/string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <dbus/dbus.h>
#ifdef HAVE_DBUS_MESSAGE_ITER_INIT_APPEND
#  define dbus_message_append_iter_init dbus_message_iter_init_append
#  define dbus_message_iter_append_string(i,v) dbus_message_iter_append_basic(i, DBUS_TYPE_STRING, v)
#  define dbus_message_iter_append_uint32(i,v) dbus_message_iter_append_basic(i, DBUS_TYPE_UINT32, v)
#  define dbus_message_iter_append_boolean(i,v) dbus_message_iter_append_basic(i, DBUS_TYPE_BOOLEAN, v)
#endif /* HAVE_DBUS_MESSAGE_ITER_INIT_APPEND */


/*
 * D-Bus object: org.cups.cupsd.Notifier
 * D-Bus object path: /org/cups/cupsd/Notifier
 *
 * D-Bus interface name: org.cups.cupsd.Notifier
 *
 * Signals:
 *
 * ServerRestarted(STRING text)
 * Server has restarted.
 *
 * ServerStarted(STRING text)
 * Server has started.
 *
 * ServerStopped(STRING text)
 * Server has stopped.
 *
 * ServerAudit(STRING text)
 * Security-related event.
 *
 * PrinterRestarted(STRING text,
 *                  STRING printer-uri,
 *                  STRING printer-name,
 *                  UINT32 printer-state,
 *                  STRING printer-state-reasons,
 *                  BOOLEAN printer-is-accepting-jobs)
 * Printer has restarted.
 *
 * PrinterShutdown(STRING text,
 *                 STRING printer-uri,
 *                 STRING printer-name,
 *                 UINT32 printer-state,
 *                 STRING printer-state-reasons,
 *                 BOOLEAN printer-is-accepting-jobs)
 * Printer has shutdown.
 *
 * PrinterStopped(STRING text,
 *                STRING printer-uri,
 *                STRING printer-name,
 *                UINT32 printer-state,
 *                STRING printer-state-reasons,
 *                BOOLEAN printer-is-accepting-jobs)
 * Printer has stopped.
 *
 * PrinterStateChanged(STRING text,
 *                     STRING printer-uri,
 *                     STRING printer-name,
 *                     UINT32 printer-state,
 *                     STRING printer-state-reasons,
 *                     BOOLEAN printer-is-accepting-jobs)
 * Printer state has changed.
 *
 * PrinterFinishingsChanged(STRING text,
 *                          STRING printer-uri,
 *                          STRING printer-name,
 *                          UINT32 printer-state,
 *                          STRING printer-state-reasons,
 *                          BOOLEAN printer-is-accepting-jobs)
 * Printer's finishings-supported attribute has changed.
 *
 * PrinterMediaChanged(STRING text,
 *                     STRING printer-uri,
 *                     STRING printer-name,
 *                     UINT32 printer-state,
 *                     STRING printer-state-reasons,
 *                     BOOLEAN printer-is-accepting-jobs)
 * Printer's media-supported attribute has changed.
 *
 * PrinterAdded(STRING text,
 *              STRING printer-uri,
 *              STRING printer-name,
 *              UINT32 printer-state,
 *              STRING printer-state-reasons,
 *              BOOLEAN printer-is-accepting-jobs)
 * Printer has been added.
 *
 * PrinterDeleted(STRING text,
 *                STRING printer-uri,
 *                STRING printer-name,
 *                UINT32 printer-state,
 *                STRING printer-state-reasons,
 *                BOOLEAN printer-is-accepting-jobs)
 * Printer has been deleted.
 *
 * PrinterModified(STRING text,
 *                 STRING printer-uri,
 *                 STRING printer-name,
 *                 UINT32 printer-state,
 *                 STRING printer-state-reasons,
 *                 BOOLEAN printer-is-accepting-jobs)
 * Printer has been modified.
 *
 * text describes the event.
 * printer-state-reasons is a comma-separated list.
 * If printer-uri is "" in a Job* signal, the other printer-* parameters
 * must be ignored.
 * If the job name is not know, job-name will be "".
 */

/*
 * Constants...
 */

enum
{
  PARAMS_NONE,
  PARAMS_PRINTER,
  PARAMS_JOB
};


/*
 * Local functions...
 */

static int	acquire_lock(int *fd);


/*
 * 'main()' - Read events and send DBUS notifications.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  ipp_t			*msg;		/* Event message from scheduler */
  ipp_state_t		state;		/* IPP event state */
  struct sigaction	action;		/* POSIX sigaction data */
  DBusConnection	*con = NULL;	/* Connection to DBUS server */
  DBusError		error;		/* Error, if any */
  DBusMessage		*message;	/* Message to send */
  DBusMessageIter	iter;		/* Iterator for message data */
  int			lock_fd = -1;	/* Lock file descriptor */
  char			lock_filename[1024];
					/* Lock filename */


 /*
  * Don't buffer stderr...
  */

  setbuf(stderr, NULL);

 /*
  * Ignore SIGPIPE signals...
  */

  memset(&action, 0, sizeof(action));
  action.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &action, NULL);

 /*
  * Validate command-line options...
  */

  if (argc != 3)
  {
    fputs("Usage: dbus dbus:/// notify-user-data\n", stderr);
    return (1);
  }

  if (strcmp(argv[1], "dbus:"))
  {
    fprintf(stderr, "ERROR: Bad URI \"%s\"!\n", argv[1]);
    return (1);
  }

 /*
  * Loop forever until we run out of events...
  */

  for (;;)
  {
    ipp_attribute_t	*attr;		/* Current attribute */
    const char		*event;		/* Event name */
    const char		*signame = NULL;/* DBUS signal name */
    char		*printer_reasons = NULL;
					/* Printer reasons string */
    const char		*nul = "";	/* Empty string value */
    int			no = 0;		/* Boolean "no" value */
    int			params = PARAMS_NONE;
					/* What parameters to include? */


   /*
    * Get the next event...
    */

    msg = ippNew();
    while ((state = ippReadFile(0, msg)) != IPP_DATA)
    {
      if (state <= IPP_IDLE)
        break;
    }

    fprintf(stderr, "DEBUG: state=%d\n", state);

    if (state == IPP_ERROR)
      fputs("DEBUG: ippReadFile() returned IPP_ERROR!\n", stderr);

    if (state <= IPP_IDLE)
    {
     /*
      * Out of messages, free memory and then exit...
      */

      ippDelete(msg);
      break;
    }

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

      con = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
      if (!con)
	dbus_error_free(&error);
      else
	fputs("DEBUG: Connected to D-BUS\n", stderr);
    }

    if (!con)
      continue;

    if (lock_fd == -1 &&
        acquire_lock(&lock_fd, lock_filename, sizeof(lock_filename)))
      continue;

    attr = ippFindAttribute(msg, "notify-subscribed-event",
			     IPP_TAG_KEYWORD);
    if (!attr)
      continue;

    event = attr->values[0].string.text;
    if (!strncmp(event, "server-", 7))
    {
      const char *word2 = event + 7;	/* Second word */

      if (!strcmp(word2, "restarted"))
	signame = "ServerRestarted";
      else if (!strcmp(word2, "started"))
	signame = "ServerStarted";
      else if (!strcmp(word2, "stopped"))
	signame = "ServerStopped";
      else if (!strcmp(word2, "audit"))
	signame = "ServerAudit";
      else
	continue;
    }
    else if (!strncmp(event, "printer-", 8))
    {
      const char *word2 = event + 8;	/* Second word */

      params = PARAMS_PRINTER;
      if (!strcmp(word2, "restarted"))
	signame = "PrinterRestarted";
      else if (!strcmp(word2, "shutdown"))
	signame = "PrinterShutdown";
      else if (!strcmp(word2, "stopped"))
	signame = "PrinterStopped";
      else if (!strcmp(word2, "state-changed"))
	signame = "PrinterStateChanged";
      else if (!strcmp(word2, "finishings-changed"))
	signame = "PrinterFinishingsChanged";
      else if (!strcmp(word2, "media-changed"))
	signame = "PrinterMediaChanged";
      else if (!strcmp(word2, "added"))
	signame = "PrinterAdded";
      else if (!strcmp(word2, "deleted"))
	signame = "PrinterDeleted";
      else if (!strcmp(word2, "modified"))
	signame = "PrinterModified";
      else
	continue;
    }
    else if (!strncmp(event, "job-", 4))
    {
      const char *word2 = event + 4;	/* Second word */

      params = PARAMS_JOB;
      if (!strcmp(word2, "state"))
	signame = "JobState";
      else if (!strcmp(word2, "created"))
	signame = "JobCreated";
      else if (!strcmp(word2, "completed"))
	signame = "JobCompleted";
      else if (!strcmp(word2, "stopped"))
	signame = "JobStopped";
      else if (!strcmp(word2, "config-changed"))
	signame = "JobConfigChanged";
      else if (!strcmp(word2, "progress"))
	signame = "JobProgress";
      else
	continue;
    }
    else
      continue;

    /*
     * Create and send the new message...
     */
    
    fprintf(stderr, "DEBUG: %s\n", signame);
    message = dbus_message_new_signal("/org/cups/cupsd/Notifier",
				      "org.cups.cupsd.Notifier",
				      signame);

    dbus_message_append_iter_init(message, &iter);
    attr = ippFindAttribute(msg, "notify-text", IPP_TAG_TEXT);
    if (!attr)
      goto bail;
    dbus_message_iter_append_string(&iter, &(attr->values[0].string.text));

    if (params >= PARAMS_PRINTER)
    {
      char	*p;			/* Pointer into printer_reasons */
      size_t	reasons_length;		/* Required size of printer_reasons */
      int	i;			/* Looping var */
      int	have_printer_params = 1;/* Do we have printer URI? */

      /* STRING printer-uri or "" */
      attr = ippFindAttribute(msg, "notify-printer-uri", IPP_TAG_URI);
      if (attr)
	dbus_message_iter_append_string(&iter, &(attr->values[0].string.text));
      else
      {
	have_printer_params = 0;
	dbus_message_iter_append_string(&iter, &nul);
      }

      /* STRING printer-name */
      if (have_printer_params)
      {
	attr = ippFindAttribute(msg, "printer-name", IPP_TAG_NAME);
	if (attr)
	  dbus_message_iter_append_string(&iter,
					  &(attr->values[0].string.text));
	else
	  goto bail;
      }
      else
	dbus_message_iter_append_string(&iter, &nul);

      /* UINT32 printer-state */
      if (have_printer_params)
      {
	attr = ippFindAttribute(msg, "printer-state", IPP_TAG_ENUM);
	if (attr)
	  dbus_message_iter_append_uint32(&iter, &(attr->values[0].integer));
	else
	  goto bail;
      }
      else
	dbus_message_iter_append_uint32(&iter, &no);

      /* STRING printer-state-reasons */
      if (have_printer_params)
      {
	attr = ippFindAttribute(msg, "printer-state-reasons",
				IPP_TAG_KEYWORD);
	if (attr)
	{
	  for (reasons_length = 0, i = 0; i < attr->num_values; i++)
	    /* All need commas except the last, which needs a nul byte. */
	    reasons_length += 1 + strlen(attr->values[i].string.text);
	  printer_reasons = malloc(reasons_length);
	  if (!printer_reasons)
	    goto bail;
	  p = printer_reasons;
	  for (i = 0; i < attr->num_values; i++)
	  {
	    strcpy(p, attr->values[i].string.text);
	    p += strlen(p);
	    if (i)
	      *p++ = ',';
	  }
	  dbus_message_iter_append_string(&iter, &printer_reasons);
	}
	else
	  goto bail;
      }
      else
	dbus_message_iter_append_string(&iter, &nul);

      /* BOOL printer-is-accepting-jobs */
      if (have_printer_params)
      {
	attr = ippFindAttribute(msg, "printer-is-accepting-jobs",
				IPP_TAG_BOOLEAN);
	if (attr)
	  dbus_message_iter_append_boolean(&iter, &(attr->values[0].boolean));
	else
	  goto bail;
      }
      else
	dbus_message_iter_append_boolean(&iter, &no);
    }

    if (params >= PARAMS_JOB)
    {
      /* UINT32 job-id */
      attr = ippFindAttribute(msg, "notify-job-id", IPP_TAG_INTEGER);
      if (!attr)
	goto bail;
      dbus_message_iter_append_uint32(&iter, &(attr->values[0].integer));

      /* UINT32 job-state */
      attr = ippFindAttribute(msg, "job-state", IPP_TAG_ENUM);
      if (!attr)
	goto bail;
      dbus_message_iter_append_uint32(&iter, &(attr->values[0].integer));

      /* STRING job-state-reasons */
      attr = ippFindAttribute(msg, "job-state-reasons", IPP_TAG_KEYWORD);
      if (!attr)
	goto bail;
      dbus_message_iter_append_string(&iter, &(attr->values[0].string.text));

      /* STRING job-name or "" */
      attr = ippFindAttribute(msg, "job-name", IPP_TAG_NAME);
      if (attr)
	dbus_message_iter_append_string(&iter, &(attr->values[0].string.text));
      else
	dbus_message_iter_append_string(&iter, &nul);

      /* UINT32 job-impressions-completed */
      attr = ippFindAttribute(msg, "job-impressions-completed",
			      IPP_TAG_INTEGER);
      if (!attr)
	goto bail;
      dbus_message_iter_append_uint32(&iter, &(attr->values[0].integer));
    }

    dbus_connection_send(con, message, NULL);
    dbus_connection_flush(con);

   /*
    * Cleanup...
    */

    bail:

    if (printer_reasons)
      free(printer_reasons);

    dbus_message_unref(message);
    ippDelete(msg);
  }

 /*
  * Remove lock file...
  */

  if (lock_fd >= 0)
  {
    close(lock_fd);
    unlink(lock_filename);
  }

  return (0);
}


/*
 * 'acquire_lock()' - Acquire a lock so we only have a single notifier running.
 */

static int				/* O - 0 on success, -1 on failure */
acquire_lock(int    *fd,		/* O - Lock file descriptor */
             char   *lockfile,		/* I - Lock filename buffer */
	     size_t locksize)		/* I - Size of filename buffer */
{
  const char	*tmpdir;		/* Temporary directory */


 /*
  * Figure out where to put the lock file...
  */

  if ((tmpdir = getenv("TMPDIR")) == NULL)
    tmpdir = "/tmp";

  snprintf(lockfile, locksize, "%s/cups-dbus-notifier-lockfile", tmpdir);

 /*
  * Create the lock file and fail if it already exists...
  */

  if ((*fd = open(lockfile, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR)) < 0)
    return (-1);
  else
    return (0);
}


/*
 * End of "$Id$".
 */
