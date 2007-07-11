/*
 * "$Id$"
 *
 *   Browsing test program for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
 *   main()  - Simulate one or more remote printers.
 *   usage() - Show program usage...
 */

/*
 * Include necessary headers...
 */

#include <cups/cups.h>
#include <cups/string.h>
#include <stdlib.h>
#include <errno.h>


/*
 * Local functions...
 */

static void	usage(void);


/*
 * 'main()' - Simulate one or more remote printers.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  int		i,			/* Looping var */
		printer,		/* Current printer */
		num_printers,		/* Number of printers */
		pclass,			/* Current printer class */
		num_pclasses,		/* Number of printer classes */
		server,			/* Current server */
		num_servers,		/* Number of servers */
		count,			/* Number of printers sent this cycle */
		interval,		/* Browse Interval */
		lease,			/* Browse lease-duration */
		continuous,		/* Run continuously? */
		port,			/* Browse port */
		sock,			/* Browse socket */
		val,			/* Socket option value */
		seconds,		/* Seconds until next cycle */
		verbose;		/* Verbose output? */
  const char	*options;		/* Options for URIs */
  time_t	curtime;		/* Current UNIX time */
  struct tm	*curdate;		/* Current date and time */
  struct sockaddr_in addr;		/* Broadcast address */
  char		packet[1540];		/* Data packet */
  static const char * const names[26] =	/* Printer names */
		{
		  "alpha",
		  "bravo",
		  "charlie",
		  "delta",
		  "echo",
		  "foxtrot",
		  "golf",
		  "hotel",
		  "india",
		  "juliet",
		  "kilo",
		  "lima",
		  "mike",
		  "november",
		  "oscar",
		  "papa",
		  "quebec",
		  "romeo",
		  "sierra",
		  "tango",
		  "uniform",
		  "victor",
		  "wiskey",
		  "x-ray",
		  "yankee",
		  "zulu"
		};


 /*
  * Process command-line arguments...
  */

  num_printers = 10;
  num_pclasses = 5;
  num_servers  = 1;
  interval     = 30;
  lease        = 60;
  port         = 0;
  verbose      = 0;
  continuous   = 0;
  options      = NULL;

  for (i = 1; i < argc; i ++)
  {
    if (!strcmp(argv[i], "-c"))
      continuous = 1;
    else if (!strcmp(argv[i], "-i"))
    {
      i ++;
      if (i < argc)
        interval = atoi(argv[i]);
      else
        usage();

      continuous = 1;
    }
    else if (!strcmp(argv[i], "-l"))
    {
      i ++;
      if (i < argc)
        lease = atoi(argv[i]);
      else
        usage();
    }
    else if (!strcmp(argv[i], "-o"))
    {
      i ++;
      if (i < argc)
        options = argv[i];
      else
        usage();
    }
    else if (!strcmp(argv[i], "-C"))
    {
      i ++;
      if (i < argc)
        num_pclasses = atoi(argv[i]);
      else
        usage();
    }
    else if (!strcmp(argv[i], "-p"))
    {
      i ++;
      if (i < argc)
        num_printers = atoi(argv[i]);
      else
        usage();
    }
    else if (!strcmp(argv[i], "-s"))
    {
      i ++;
      if (i < argc)
        num_servers = atoi(argv[i]);
      else
        usage();
    }
    else if (!strcmp(argv[i], "-v"))
      verbose = 1;
    else if (isdigit(argv[i][0] & 255))
    {
      port = atoi(argv[i]);
    }
    else
      usage();
  }

  if ((num_printers <= 0 && num_pclasses <= 0) || num_servers <= 0 ||
      interval <= 0 || lease < 1 || port <= 0)
    usage();

 /*
  * Open a broadcast socket...
  */

  if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
  {
    perror("Unable to open broadcast socket");
    return (1);
  }

 /*
  * Set the "broadcast" flag...
  */

  val = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &val, sizeof(val)))
  {
    perror("Unable to put socket in broadcast mode");

    close(sock);
    return (1);
  }

 /*
  * Broadcast to 127.0.0.1 (localhost)
  */

  memset(&addr, 0, sizeof(addr));
  addr.sin_addr.s_addr = htonl(0x7f000001);
  addr.sin_family      = AF_INET;
  addr.sin_port        = htons(port);

 /*
  * Send virtual printers continuously until we are stopped.
  */

  for (;;)
  {
   /*
    * Start a new cycle of N printers...
    */

    printf("Sending %d printers from %d servers...\n", num_printers,
           num_servers);

    count   = num_servers * (num_printers + num_pclasses) / interval + 1;
    curtime = time(NULL);
    curdate = localtime(&curtime);
    seconds = interval;

    for (i = 0, printer = 0; printer < num_printers; printer ++)
    {
      for (server = 0; server < num_servers; server ++, i ++)
      {
        if (i == count)
	{
	  seconds --;
	  i = 0;
	  sleep(1);
	  curtime = time(NULL);
	  curdate = localtime(&curtime);
	}

        snprintf(packet, sizeof(packet),
	         "%x %x ipp://testserver-%d/printers/%s-%d \"Server Room %d\" "
		 "\"Test Printer %d\" \"Acme Blazer 2000\"%s%s "
		 "lease-duration=%d\n",
                 CUPS_PRINTER_REMOTE, IPP_PRINTER_IDLE, server + 1,
		 names[printer % 26], printer / 26 + 1, server + 1,
		 printer + 1, options ? " ipp-options=" : "",
		 options ? options : "", lease);

        if (verbose)
	  printf("[%02d:%02d:%02d] %s", curdate->tm_hour, curdate->tm_min,
	         curdate->tm_sec, packet);

        if (sendto(sock, packet, strlen(packet), 0,
	           (struct sockaddr *)&addr, sizeof(addr)) < 0)
	  perror("Unabled to send packet");
      }
    }


    for (i = 0, pclass = 0; pclass < num_pclasses; pclass ++)
    {
      for (server = 0; server < num_servers; server ++, i ++)
      {
        if (i == count)
	{
	  seconds --;
	  i = 0;
	  sleep(1);
	  curtime = time(NULL);
	  curdate = localtime(&curtime);
	}

        snprintf(packet, sizeof(packet),
	         "%x %x ipp://testserver-%d/classes/class-%s-%d \"Server Room %d\" "
		 "\"Test Class %d\" \"Acme Blazer 2000\"%s%s "
		 "lease-duration=%d\n",
                 CUPS_PRINTER_REMOTE | CUPS_PRINTER_CLASS, IPP_PRINTER_IDLE,
		 server + 1, names[pclass % 26], pclass / 26 + 1, server + 1,
		 pclass + 1, options ? " ipp-options=" : "",
		 options ? options : "", lease);

        if (verbose)
	  printf("[%02d:%02d:%02d] %s", curdate->tm_hour, curdate->tm_min,
	         curdate->tm_sec, packet);

        if (sendto(sock, packet, strlen(packet), 0,
	           (struct sockaddr *)&addr, sizeof(addr)) < 0)
	  perror("Unabled to send packet");
      }
    }

    if (!continuous)
      break;

   /*
    * Sleep for any remaining time...
    */

    if (seconds > 0) 
      sleep(seconds);
  }

  return (0);
}


/*
 * 'usage()' - Show program usage...
 */

static void
usage(void)
{
  puts("Usage: testdirsvc [-c] [-i interval] [-l lease-duration] "
       "[-o ipp-options] [-p printers] "
       "[-C classes] [-s servers] [-v] port");
  exit(0);
}


/*
 * End of "$Id$".
 */
