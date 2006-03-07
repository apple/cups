/*
 * "$Id$"
 *
 *   IEEE-1394 backend for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2002 by Easy Software Products, all rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or
 *   without modification, are permitted provided that the
 *   following conditions are met:
 *
 *     1. Redistributions of source code must retain the above
 *	  copyright notice, this list of conditions and the
 *	  following disclaimer.
 *
 *     2. Redistributions in binary form must reproduce the
 *	  above copyright notice, this list of conditions and
 *	  the following disclaimer in the documentation and/or
 *	  other materials provided with the distribution.
 *
 *     3. All advertising materials mentioning features or use
 *	  of this software must display the following
 *	  acknowledgement:
 *
 *	    This product includes software developed by Easy
 *	    Software Products.
 *
 *     4. The name of Easy Software Products may not be used to
 *	  endorse or promote products derived from this software
 *	  without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS
 *   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
 *   BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *   MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *   DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS
 *   BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 *   OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 *   DAMAGE.
 *
 * Contents:
 *
 *   main()         - Send a file to the printer.
 *   list_devices() - List all known printer devices...
 */

/*
 * Include necessary headers.
 */

#include "ieee1394.h"


/*
 * Local functions...
 */

void	list_devices(void);


/*
 * 'main()' - Send a file to the printer.
 *
 * Usage:
 *
 *    printer-uri job-id user title copies options [file]
 */

int			/* O - Exit status */
main(int  argc,		/* I - Number of command-line arguments (6 or 7) */
     char *argv[])	/* I - Command-line arguments */
{
  ieee1394_dev_t dev;		/* Printer device */
  int		fp;		/* Print file */
  int		copies;		/* Number of copies to print */
  int		rbytes;		/* Number of bytes read from device */
  size_t	nbytes,		/* Number of bytes read from file */
		tbytes;		/* Total number of bytes written */
  char		buffer[8192];	/* Input/output buffer */
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;	/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */


 /*
  * Make sure status messages are not buffered...
  */

  setbuf(stderr, NULL);

 /*
  * Check command-line...
  */

  if (argc == 1)
  {
    list_devices();

    return (0);
  }
  else if (argc < 6 || argc > 7)
  {
    fprintf(stderr, "Usage: %s job-id user title copies options [file]\n",
            argv[0]);
    return (1);
  }

 /*
  * If we have 7 arguments, print the file named on the command-line.
  * Otherwise, send stdin instead...
  */

  if (argc == 6)
  {
    fp     = 0;
    copies = 1;
  }
  else
  {
   /*
    * Try to open the print file...
    */

    if ((fp = open(argv[6], O_RDONLY)) < 0)
    {
      perror("ERROR: unable to open print file");
      return (1);
    }

    copies = atoi(argv[4]);
  }

 /*
  * Try to open the printer device...
  */

  fputs("STATE: +connecting-to-device\n", stderr);

  do
  {
    if ((dev = ieee1394_open(argv[0])) == NULL)
    {
      fputs("INFO: Firewire printer busy; will retry in 30 seconds...\n", stderr);
      sleep(30);
    }
  }
  while (dev == NULL);

  fputs("STATE: -connecting-to-device\n", stderr);

 /*
  * Now that we are "connected" to the port, ignore SIGTERM so that we
  * can finish out any page data the driver sends (e.g. to eject the
  * current page...  Only ignore SIGTERM if we are printing data from
  * stdin (otherwise you can't cancel raw jobs...)
  */

  if (argc < 7)
  {
#ifdef HAVE_SIGSET /* Use System V signals over POSIX to avoid bugs */
    sigset(SIGTERM, SIG_IGN);
#elif defined(HAVE_SIGACTION)
    memset(&action, 0, sizeof(action));

    sigemptyset(&action.sa_mask);
    action.sa_handler = SIG_IGN;
    sigaction(SIGTERM, &action, NULL);
#else
    signal(SIGTERM, SIG_IGN);
#endif /* HAVE_SIGSET */
  }

 /*
  * Finally, send the print file...
  */

  while (copies > 0)
  {
    copies --;

    if (fp != 0)
    {
      fputs("PAGE: 1 1\n", stderr);
      lseek(fp, 0, SEEK_SET);
    }

    tbytes = 0;
    while ((nbytes = read(fp, buffer, sizeof(buffer))) > 0)
    {
     /*
      * Write the print data to the printer...
      */

      tbytes += nbytes;

      if (ieee1394_write(dev, buffer, nbytes) < 0)
      {
	perror("ERROR: Unable to send print file to printer");
	break;
      }

      if ((rbytes = ieee1394_read(dev, buffer, sizeof(buffer))) > 0)
        fprintf(stderr, "INFO: Read %d bytes from printer...\n", rbytes);

      if (argc > 6)
	fprintf(stderr, "INFO: Sending print file, %lu bytes...\n",
	        (unsigned long)tbytes);
    }
  }

 /*
  * Close the printer device and input file and return...
  */

  ieee1394_close(dev);

  if (fp != 0)
    close(fp);

  fputs("INFO: Ready to print.\n", stderr);

  return (0);
}


/*
 * 'list_devices()' - List all known devices...
 */

void
list_devices(void)
{
  int			i,		/* Looping var */
			num_info;	/* Number of devices */
  ieee1394_info_t	*info;		/* Devices... */


 /*
  * Get the available devices...
  */

  info = ieee1394_list(&num_info);

 /*
  * List them as needed...
  */

  if (num_info > 0)
  {
    for (i = 0; i < num_info; i ++)
      printf("direct %s \"%s\" \"%s\"\n", info[i].uri,
             info[i].make_model, info[i].description);

    free(info);
  }
}


/*
 * End of "$Id$".
 */
