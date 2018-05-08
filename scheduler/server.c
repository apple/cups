/*
 * Server start/stop routines for the CUPS scheduler.
 *
 * Copyright 2007-2018 by Apple Inc.
 * Copyright 1997-2006 by Easy Software Products, all rights reserved.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * missing or damaged, see the license at "http://www.cups.org/".
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

static int		started = 0;	/* Did we start the server already? */


/*
 * 'cupsdStartServer()' - Start the server.
 */

void
cupsdStartServer(void)
{
 /*
  * Create the default security profile...
  */

  DefaultProfile = cupsdCreateProfile(0, 1);

#ifdef HAVE_SANDBOX_H
  if (!DefaultProfile && UseSandboxing && Sandboxing != CUPSD_SANDBOXING_OFF)
  {
   /*
    * Failure to create the sandbox profile means something really bad has
    * happened and we need to shutdown immediately.
    */

    return;
  }
#endif /* HAVE_SANDBOX_H */

 /*
  * Start color management (as needed)...
  */

  cupsdStartColor();

 /*
  * Startup all the networking stuff...
  */

  cupsdStartListening();
  cupsdStartBrowsing();

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

  cupsdSetBusyState(0);
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
  * Stop color management (as needed)...
  */

  cupsdStopColor();

 /*
  * Close all network clients...
  */

  cupsdCloseAllClients();
  cupsdStopListening();
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

 /*
  * Close all log files...
  */

  if (AccessFile != NULL)
  {
    if (AccessFile != LogStderr)
      cupsFileClose(AccessFile);

    AccessFile = NULL;
  }

  if (ErrorFile != NULL)
  {
    if (ErrorFile != LogStderr)
      cupsFileClose(ErrorFile);

    ErrorFile = NULL;
  }

  if (PageFile != NULL)
  {
    if (PageFile != LogStderr)
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
