/*
 * "$Id: main.c,v 1.4 1998/10/16 18:28:01 mike Exp $"
 *
 *   for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-1998 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Easy Software Products and are protected by Federal
 *   copyright law.  Distribution and use rights are outlined in the file
 *   "LICENSE" which should have been included with this file.  If this
 *   file is missing or damaged please contact Easy Software Products
 *   at:
 *
 *       Attn: CUPS Licensing Information
 *       Easy Software Products
 *       44145 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 *   main()           - Main entry for the CUPS scheduler.
 *   sighup_handler() - Handle 'hangup' signals to reconfigure the scheduler.
 *   usage()          - Show scheduler usage.
 */

/*
 * Include necessary headers...
 */

#define _MAIN_C_
#include "cupsd.h"


/*
 * Local functions...
 */

static void	sighup_handler(int sig);
static void	usage(void);


/*
 * 'main()' - Main entry for the CUPS scheduler.
 */

int				/* O - Exit status */
main(int  argc,			/* I - Number of command-line arguments */
     char *argv[])		/* I - Command-line arguments */
{
  int			i;		/* Looping var */
  char			*opt;		/* Option character */
  int			bytes;		/* Number of bytes read */
  fd_set		input,		/* Input set for select() */
			output;		/* Output set for select() */
  client_t		*con;		/* Current client */
  listener_t		*lis;		/* Current listener */
  time_t		activity;	/* Activity timer */
  struct timeval	timeout;	/* select() timeout */


 /*
  * Check for command-line arguments...
  */

  for (i = 1; i < argc; i ++)
    if (argv[i][0] == '-')
      for (opt = argv[i] + 1; *opt != '\0'; opt ++)
        switch (*opt)
	{
	  case 'c' : /* Configuration file */
	      i ++;
	      if (i >= argc)
	        usage();

	      strncpy(ConfigurationFile, argv[i], sizeof(ConfigurationFile) - 1);
	      ConfigurationFile[sizeof(ConfigurationFile) - 1] = '\0';
	      break;

	  default : /* Unknown option */
              fprintf(stderr, "cupsd: Unknown option \'%c\' - aborting!\n", *opt);
	      usage();
	      break;
	}
    else
    {
      fprintf(stderr, "cupsd: Unknown argument \'%s\' - aborting!\n", argv[i]);
      usage();
    }

 /*
  * Set the timezone to GMT...
  */

  putenv("TZ=GMT");
  tzset();

 /*
  * Catch hangup signals...
  */

  sigset(SIGHUP, sighup_handler);

 /*
  * Loop forever...
  */

  while (TRUE)
  {
   /*
    * Check if we need to load the server configuration file...
    */

    if (NeedReload)
    {
      if (NumClients > 0)
      {
        for (i = NumClients, con = Clients; i > 0; i --, con ++)
	  if (con->state == HTTP_WAITING)
	  {
	    CloseClient(con);
	    con --;
	  }
	  else
	    con->keep_alive = 0;

	for (i = 0; i < NumListeners; i ++)
	  FD_CLR(Listeners[i].fd, &InputSet);
      }
      else if (!ReadConfiguration())
      {
        fprintf(stderr, "cupsd: Unable to read configuration file \'%s\' - exiting!",
	        ConfigurationFile);
        exit(1);
      }
    }

   /*
    * Check for available input or ready output.  If select() returns
    * 0 or -1, something bad happened and we should exit immediately.
    *
    * Note that we at least have the listening socket open at all
    * times.
    */

    input  = InputSet;
    output = OutputSet;

    timeout.tv_sec  = 1;
    timeout.tv_usec = 0;

    for (i = NumClients, con = Clients; i > 0; i --, con ++)
      if (con->bufused > 0)
      {
        timeout.tv_sec  = 0;
	break;
      }

    if ((i = select(100, &input, &output, NULL, &timeout)) < 0)
    {
      if (errno == EINTR)
        continue;

      fprintf(stderr, "cupsd: select() failed - %s\n", strerror(errno));
      break;
    }

    for (i = NumListeners, lis = Listeners; i > 0; i --, lis ++)
      if (FD_ISSET(lis->fd, &input))
        AcceptClient(lis);

    for (i = NumClients, con = Clients; i > 0; i --, con ++)
    {
     /*
      * Read data as needed...
      */

      if (FD_ISSET(con->fd, &input))
      {
        if ((bytes = recv(con->fd, con->buf + con->bufused,
                          MAX_BUFFER - con->bufused, 0)) <= 0)
        {
	  fprintf(stderr, "cupsd: Lost client #%d\n", con->fd);
          CloseClient(con);
	  con --;
	  continue;
	}

        con->bufused += bytes;
      }

     /*
      * Process the input buffer...
      */

      if (con->bufused > 0)
        if (!ReadClient(con))
	{
	  con --;
	  continue;
	}

     /*
      * Write data as needed...
      */

      if (FD_ISSET(con->fd, &output) ||
          FD_ISSET(con->file, &output))
        if (!WriteClient(con))
	{
	  con --;
	  continue;
	}

     /*
      * Check the activity and close old clients...
      */

      activity = time(NULL) - 30;
      if (con->activity < activity)
      {
        CloseClient(con);
        con --;
        continue;
      }
    }
  }

 /*
  * If we get here something very bad happened and we need to exit
  * immediately.
  */

  CloseAllClients();
  StopListening();

  return (1);
}


/*
 * 'sighup_handler()' - Handle 'hangup' signals to reconfigure the scheduler.
 */

static void
sighup_handler(int sig)	/* I - Signal number */
{
  NeedReload = TRUE;
}


/*
 * 'usage()' - Show scheduler usage.
 */

static void
usage(void)
{
  fputs("Usage: cupsd [-c config-file]", stderr);
  exit(1);
}


/*
 * End of "$Id: main.c,v 1.4 1998/10/16 18:28:01 mike Exp $".
 */
