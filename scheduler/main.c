/*
 * "$Id: main.c,v 1.1 1998/10/12 13:57:19 mike Exp $"
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
 *   Revision 1.1  1998/10/12 13:57:19  mike
 *   Initial revision
 *
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
  struct timeval	timeout;	/* select() timeout */


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

    for (i = 0, con = Connection; i < NumConnections; i ++, con ++)
#if 0
      if (con->bufused > 0 ||
          con->state == HTTP_GET_DATA ||
          con->state == HTTP_POST_DATA)
#else
      if (con->bufused > 0)
#endif /* 0 */
	break;

    if (i < NumConnections)
    {
      timeout.tv_sec  = 0;
      timeout.tv_usec = 0;
    }
    else
    {
      timeout.tv_sec  = 1;
      timeout.tv_usec = 0;
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

      if (FD_ISSET(Connection[i].fd, &output) ||
          Connection[i].state == HTTP_GET_DATA ||
	  Connection[i].state == HTTP_POST_DATA)
        if (!WriteConnection(Connection + i))
	{
	  con --;
	  i --;
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
 * End of "$Id: main.c,v 1.1 1998/10/12 13:57:19 mike Exp $".
 */
