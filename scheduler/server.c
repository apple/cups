/*
 * "$Id$"
 *
 *   Server start/stop routines for the Common UNIX Printing System (CUPS).
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
 *   cupsdStartServer() - Start the server.
 *   cupsdStopServer()  - Stop the server.
 */

/*
 * Include necessary headers...
 */

#include <cups/http-private.h>
#include "cupsd.h"
#include <grp.h>


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
#ifdef HAVE_LIBSSL
  int			i;		/* Looping var */
  struct timeval	curtime;	/* Current time in microseconds */
  unsigned char		data[1024];	/* Seed data */
#endif /* HAVE_LIBSSL */


#ifdef HAVE_LIBSSL
 /*
  * Initialize the encryption libraries...
  */

  SSL_library_init();
  SSL_load_error_strings();

 /*
  * Using the current time is a dubious random seed, but on some systems
  * it is the best we can do (on others, this seed isn't even used...)
  */

  gettimeofday(&curtime, NULL);
  srand(curtime.tv_sec + curtime.tv_usec);

  for (i = 0; i < sizeof(data); i ++)
    data[i] = rand(); /* Yes, this is a poor source of random data... */

  RAND_seed(&data, sizeof(data));
#elif defined(HAVE_GNUTLS)
 /*
  * Initialize the encryption libraries...
  */

  gnutls_global_init();
#endif /* HAVE_LIBSSL */

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

    cupsdLogMessage(CUPSD_LOG_DEBUG2,
                    "cupsdStartServer: Adding fd %d to InputSet...",
		    CGIPipes[0]);
    FD_SET(CGIPipes[0], InputSet);
  }

  started = 1;
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
  cupsdSaveRemoteCache();

  if (Clients != NULL)
  {
    free(Clients);
    Clients = NULL;
  }

#if defined(HAVE_SSL) && defined(HAVE_CDSASSL)
 /*
  * Free all of the certificates...
  */

  if (ServerCertificatesArray)
  {
    CFRelease(ServerCertificatesArray);
    ServerCertificatesArray = NULL;
  }
#endif /* HAVE_SSL && HAVE_CDSASSL */

 /*
  * Close the pipe for CGI processes...
  */

  if (CGIPipes[0] >= 0)
  {
    cupsdLogMessage(CUPSD_LOG_DEBUG2,
                    "cupsdStopServer: Removing fd %d from InputSet...",
                    CGIPipes[0]);

    FD_CLR(CGIPipes[0], InputSet);

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

  started = 0;
}


/*
 * End of "$Id$".
 */
