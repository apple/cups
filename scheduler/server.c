/*
 * "$Id: server.c 7927 2008-09-10 22:05:29Z mike $"
 *
 *   Server start/stop routines for the CUPS scheduler.
 *
 *   Copyright 2007-2010 by Apple Inc.
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
 *   cupsdStartServer() - Start the server.
 *   cupsdStopServer()  - Stop the server.
 */

/*
 * Include necessary headers...
 */

#include <cups/http-private.h>
#include "cupsd.h"
#include <grp.h>
#ifdef HAVE_NOTIFY_H
#  include <notify.h>
#endif /* HAVE_NOTIFY_H */


/*
 * Local globals...
 */

static int	started = 0;


/*
 * 'cupsdStartServer()' - Start the server.
 */

void
cupsdStartServer(void)
{
 /*
  * Create the default security profile...
  */

  DefaultProfile = cupsdCreateProfile(0);

 /*
  * Startup all the networking stuff...
  */

  cupsdStartListening();
  cupsdStartBrowsing();
  cupsdStartPolling();

 /*
  * Create a pipe for CGI processes...
  */

  if (cupsdOpenPipe(CGIPipes))
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "cupsdStartServer: Unable to create pipes for CGI status!");
  else
  {
    CGIStatusBuffer = cupsdStatBufNew(CGIPipes[0], "[CGI]");

    cupsdAddSelect(CGIPipes[0], (cupsd_selfunc_t)cupsdUpdateCGI, NULL, NULL);
  }

 /*
  * Mark that the server has started and printers and jobs may be changed...
  */

  LastEvent = CUPSD_EVENT_PRINTER_CHANGED | CUPSD_EVENT_JOB_STATE_CHANGED |
              CUPSD_EVENT_SERVER_STARTED;
  started   = 1;

  cupsdSetBusyState();
}


/*
 * 'cupsdStopServer()' - Stop the server.
 */

void
cupsdStopServer(void)
{
  if (!started)
    return;

 /*
  * Close all network clients and stop all jobs...
  */

  cupsdCloseAllClients();
  cupsdStopListening();
  cupsdStopPolling();
  cupsdStopBrowsing();
  cupsdStopAllNotifiers();
  cupsdDeleteAllCerts();

  if (Clients)
  {
    cupsArrayDelete(Clients);
    Clients = NULL;
  }

 /*
  * Close the pipe for CGI processes...
  */

  if (CGIPipes[0] >= 0)
  {
    cupsdRemoveSelect(CGIPipes[0]);

    cupsdStatBufDelete(CGIStatusBuffer);
    close(CGIPipes[1]);

    CGIPipes[0] = -1;
    CGIPipes[1] = -1;
  }

#ifdef HAVE_NOTIFY_POST
 /*
  * Send one last notification as the server shuts down.
  */

  cupsdLogMessage(CUPSD_LOG_DEBUG,
                  "notify_post(\"com.apple.printerListChange\") last");
  notify_post("com.apple.printerListChange");
#endif /* HAVE_NOTIFY_POST */

 /*
  * Close all log files...
  */

  if (AccessFile != NULL)
  {
    cupsFileClose(AccessFile);

    AccessFile = NULL;
  }

  if (ErrorFile != NULL)
  {
    cupsFileClose(ErrorFile);

    ErrorFile = NULL;
  }

  if (PageFile != NULL)
  {
    cupsFileClose(PageFile);

    PageFile = NULL;
  }

 /*
  * Delete the default security profile...
  */

  cupsdDestroyProfile(DefaultProfile);
  DefaultProfile = NULL;

 /*
  * Write out any dirty files...
  */

  if (DirtyFiles)
    cupsdCleanDirty();

  started = 0;
}


/*
 * End of "$Id: server.c 7927 2008-09-10 22:05:29Z mike $".
 */
