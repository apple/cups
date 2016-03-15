/*
 * "$Id: cupsaddsmb.c 10996 2013-05-29 11:51:34Z msweet $"
 *
 *   "cupsaddsmb" command for CUPS.
 *
 *   Copyright 2007-2012 by Apple Inc.
 *   Copyright 2001-2006 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
 *   main()        - Export printers on the command-line.
 *   export_dest() - Export a destination to SAMBA.
 *   usage()       - Show program usage and exit...
 */

/*
 * Include necessary headers...
 */

#include <cups/cups-private.h>
#include <cups/adminutil.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>


/*
 * Local globals...
 */

int		Verbosity = 0;
const char	*SAMBAUser,
		*SAMBAPassword,
		*SAMBAServer;


/*
 * Local functions...
 */

int	export_dest(http_t *http, const char *dest);
void	usage(void) __attribute__((noreturn));


/*
 * 'main()' - Export printers on the command-line.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  int		i, j;			/* Looping vars */
  int		status;			/* Status from export_dest() */
  int		export_all;		/* Export all printers? */
  http_t	*http;			/* Connection to server */
  int		num_dests;		/* Number of printers */
  cups_dest_t	*dests;			/* Printers */


  _cupsSetLocale(argv);

 /*
  * Parse command-line arguments...
  */

  export_all    = 0;
  http          = NULL;
  SAMBAUser     = cupsUser();
  SAMBAPassword = NULL;
  SAMBAServer   = NULL;

  for (i = 1; i < argc; i ++)
    if (!strcmp(argv[i], "-E"))
    {
#ifdef HAVE_SSL
      cupsSetEncryption(HTTP_ENCRYPT_REQUIRED);
#else
      _cupsLangPrintf(stderr,
	              _("%s: Sorry, no encryption support."),
	              argv[0]);
#endif /* HAVE_SSL */
    }
    else if (!strcmp(argv[i], "-H"))
    {
      i ++;
      if (i >= argc)
        usage();

      SAMBAServer = argv[i];
    }
    else if (!strcmp(argv[i], "-U"))
    {
      char	*sep;			/* Separator for password */


      i ++;
      if (i >= argc)
        usage();

      SAMBAUser = argv[i];

      if ((sep = strchr(argv[i], '%')) != NULL)
      {
       /*
        * Nul-terminate the username at the first % and point the
	* password at the rest...
	*/

        *sep++ = '\0';

        SAMBAPassword = sep;
      }
    }
    else if (!strcmp(argv[i], "-a"))
      export_all = 1;
    else if (!strcmp(argv[i], "-h"))
    {
      i ++;
      if (i >= argc)
        usage();

      cupsSetServer(argv[i]);
    }
    else if (!strcmp(argv[i], "-v"))
      Verbosity = 1;
    else if (argv[i][0] != '-')
    {
      if (!http)
      {
       /*
	* Connect to the server...
	*/

	if ((http = httpConnectEncrypt(cupsServer(), ippPort(),
                                       cupsEncryption())) == NULL)
	{
	  _cupsLangPrintf(stderr, _("%s: Unable to connect to server."),
	                  argv[0]);
	  exit(1);
	}
      }

      if (SAMBAServer == NULL)
      {
	SAMBAServer = cupsServer();

	if (SAMBAServer[0] == '/')	/* Use localhost instead of domain socket */
	  SAMBAServer = "localhost";
      }

      if ((status = export_dest(http, argv[i])) != 0)
	return (status);
    }
    else
      usage();

 /*
  * Connect to the server...
  */

  if ((http = httpConnectEncrypt(cupsServer(), ippPort(),
                                 cupsEncryption())) == NULL)
  {
    _cupsLangPrintf(stderr, _("%s: Unable to connect to server."), argv[0]);
    exit(1);
  }

 /*
  * See if the user specified "-a"...
  */

  if (export_all)
  {
   /*
    * Export all printers...
    */

    if (SAMBAServer == NULL)
    {
      SAMBAServer = cupsServer();

      if (SAMBAServer[0] == '/')	/* Use localhost instead of domain socket */
	SAMBAServer = "localhost";
    }

    num_dests = cupsGetDests2(http, &dests);

    for (j = 0, status = 0; j < num_dests; j ++)
      if (!dests[j].instance)
      {
        if ((status = export_dest(http, dests[j].name)) != 0)
	  break;
      }

    cupsFreeDests(num_dests, dests);

    if (status)
      return (status);
  }

  return (0);
}


/*
 * 'export_dest()' - Export a destination to SAMBA.
 */

int					/* O - 0 on success, non-zero on error */
export_dest(http_t     *http,		/* I - Connection to server */
            const char *dest)		/* I - Destination to export */
{
  int		status;			/* Status of export */
  char		ppdfile[1024],		/* PPD file for printer drivers */
		prompt[1024];		/* Password prompt */
  int		tries;			/* Number of tries */


 /*
  * Get the Windows PPD file for the printer...
  */

  if (!cupsAdminCreateWindowsPPD(http, dest, ppdfile, sizeof(ppdfile)))
  {
    _cupsLangPrintf(stderr,
                    _("cupsaddsmb: No PPD file for printer \"%s\" - %s"),
        	    dest, cupsLastErrorString());
    return (1);
  }

 /*
  * Try to export it...
  */

  for (status = 0, tries = 0; !status && tries < 3; tries ++)
  {
   /*
    * Get the password, as needed...
    */

    if (!SAMBAPassword)
    {
      snprintf(prompt, sizeof(prompt),
               _cupsLangString(cupsLangDefault(),
	                       _("Password for %s required to access %s via "
			         "SAMBA: ")),
	       SAMBAUser, SAMBAServer);

      if ((SAMBAPassword = cupsGetPassword(prompt)) == NULL)
	break;
    }

    status = cupsAdminExportSamba(dest, ppdfile, SAMBAServer,
                                  SAMBAUser, SAMBAPassword,
				  Verbosity ? stderr : NULL);

    if (!status && cupsLastError() == IPP_NOT_FOUND)
      break;
  }

  unlink(ppdfile);

  return (!status);
}


/*
 * 'usage()' - Show program usage and exit...
 */

void
usage(void)
{
  _cupsLangPuts(stdout, _("Usage: cupsaddsmb [options] printer1 ... printerN"));
  _cupsLangPuts(stdout, _("       cupsaddsmb [options] -a"));
  _cupsLangPuts(stdout, "");
  _cupsLangPuts(stdout, _("Options:"));
  _cupsLangPuts(stdout, _("  -E                      Encrypt the connection."));
  _cupsLangPuts(stdout, _("  -H samba-server         Use the named SAMBA "
                          "server."));
  _cupsLangPuts(stdout, _("  -U username             Specify username."));
  _cupsLangPuts(stdout, _("  -a                      Export all printers."));
  _cupsLangPuts(stdout, _("  -h server[:port]        Specify server address."));
  _cupsLangPuts(stdout, _("  -v                      Be verbose."));

  exit(1);
}


/*
 * End of "$Id: cupsaddsmb.c 10996 2013-05-29 11:51:34Z msweet $".
 */
