/*
 * "$Id: main.c,v 1.3 1998/10/13 18:24:15 mike Exp $"
 *
 *   Main entry for the CUPS test program.
 *
 * Contents:
 *
 *   main() - Main entry for the PCDS executive.
 *
 * Revision History:
 *
 *   $Log: main.c,v $
 *   Revision 1.3  1998/10/13 18:24:15  mike
 *   Added activity timeout code.
 *   Added Basic authorization code.
 *   Fixed problem with main loop that would cause a core dump.
 *
 *   Revision 1.2  1998/10/12  15:31:08  mike
 *   Switched from stdio files to file descriptors.
 *   Added FD_CLOEXEC flags to all non-essential files.
 *   Added pipe_command() function.
 *   Added write checks for all writes.
 *
 *   Revision 1.1  1998/10/12  13:57:19  mike
 *   Initial revision
 */

/*
 * Include necessary headers...
 */

#define _MAIN_C_
#include "cupsd.h"


/*
 * 'main()' - Main entry for the PCDS executive.
 */

int				/* O - Exit status */
main(int  argc,			/* I - Number of command-line arguments */
     char *argv[])		/* I - Command-line arguments */
{
  int			i;		/* Looping var */
  int			bytes;		/* Number of bytes read */
  fd_set		input,		/* Input set for select() */
			output;		/* Output set for select() */
  connection_t		*con;		/* Current connection */
  time_t		activity;	/* Activity timer */
  struct timeval	timeout;	/* select() timeout */


 /*
  * Set the timezone to GMT...
  */

  putenv("TZ=GMT");
  tzset();

 /*
  * Initialize 'ipp' socket for external connections...
  */

  StartListening();

 /*
  * Loop forever...
  */

  while (TRUE)
  {
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

    for (i = 0, con = Connection; i < NumConnections; i ++, con ++)
      if (con->bufused > 0)
      {
        timeout.tv_sec  = 0;
	break;
      }

    if ((i = select(100, &input, &output, NULL, &timeout)) < 0)
      break;

    if (FD_ISSET(Listener, &input))
      AcceptConnection();

    for (i = 0, con = Connection; i < NumConnections; i ++, con ++)
    {
     /*
      * Read data as needed...
      */

      if (FD_ISSET(con->fd, &input))
      {
        if ((bytes = recv(con->fd, con->buf + con->bufused,
                          MAX_BUFFER - con->bufused, 0)) <= 0)
        {
          CloseConnection(con);
	  con --;
	  i --;
	  continue;
	}

        con->bufused += bytes;
      }

     /*
      * Process the input buffer...
      */

      if (con->bufused > 0)
        if (!ReadConnection(con))
	{
	  con --;
	  i --;
	  continue;
	}

     /*
      * Write data as needed...
      */

      if (FD_ISSET(con->fd, &output) ||
          FD_ISSET(con->file, &output))
        if (!WriteConnection(con))
	{
	  con --;
	  i --;
	  continue;
	}

     /*
      * Check the activity and close old connections...
      */

      activity = time(NULL) - 30;
      if (con->activity < activity)
      {
        CloseConnection(con);
        con --;
        i --;
        continue;
      }
    }
  }

 /*
  * If we get here something very bad happened and we need to exit
  * immediately.
  */

  while (NumConnections > 0)
    CloseConnection(Connection);

#ifdef WIN32
  closesocket(Listener);
#else
  close(Listener);
#endif /* WIN32 */

  return (0);
}

/*
 * End of "$Id: main.c,v 1.3 1998/10/13 18:24:15 mike Exp $".
 */
