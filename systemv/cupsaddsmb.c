/*
 * "$Id$"
 *
 *   "cupsaddsmb" command for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2001-2006 by Easy Software Products.
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
 *   main()             - Export printers on the command-line.
 *   convert_ppd()      - Convert a PPD file to a form usable by any of the
 *                        Windows PostScript printer drivers.
 *   do_samba_command() - Do a SAMBA command, asking for a password as needed.
 *   export_dest()      - Export a destination to SAMBA.
 *   ppd_gets()         - Get a CR and/or LF-terminated line.
 *   usage()            - Show program usage and exit...
 *   write_option()     - Write a CUPS option to a PPD file.
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <cups/string.h>
#include <cups/cups.h>
#include <cups/i18n.h>
#include <cups/debug.h>
#include <errno.h>
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

int	convert_ppd(const char *src, char *dst, int dstsize, ipp_t *info);
int	do_samba_command(const char *command, const char *address,
	                 const char *subcommand);
int	export_dest(const char *dest);
char	*ppd_gets(FILE *fp, char *buf, int  buflen);
void	usage(void);
int	write_option(FILE *dstfp, int order, const char *name,
	             const char *text, const char *attrname,
	             ipp_attribute_t *suppattr, ipp_attribute_t *defattr,
		     int defval, int valcount);


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
  int		num_dests;		/* Number of printers */
  cups_dest_t	*dests;			/* Printers */


 /*
  * Parse command-line arguments...
  */

  export_all = 0;

  SAMBAUser     = cupsUser();
  SAMBAPassword = NULL;
  SAMBAServer   = NULL;

  for (i = 1; i < argc; i ++)
    if (!strcmp(argv[i], "-a"))
      export_all = 1;
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
    else if (!strcmp(argv[i], "-H"))
    {
      i ++;
      if (i >= argc)
        usage();

      SAMBAServer = argv[i];
    }
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
      if (SAMBAServer == NULL)
	SAMBAServer = cupsServer();

      if ((status = export_dest(argv[i])) != 0)
	return (status);
    }
    else
      usage();

 /*
  * See if the user specified "-a"...
  */

  if (export_all)
  {
   /*
    * Export all printers...
    */

    if (SAMBAServer == NULL)
      SAMBAServer = cupsServer();

    num_dests = cupsGetDests(&dests);

    for (j = 0, status = 0; j < num_dests; j ++)
      if (!dests[j].instance)
      {
        if ((status = export_dest(dests[j].name)) != 0)
	  break;
      }

    cupsFreeDests(num_dests, dests);

    if (status)
      return (status);
  }

  return (0);
}


/*
 * 'convert_ppd()' - Convert a PPD file to a form usable by any of the
 *                   Windows PostScript printer drivers.
 */

int					/* O - 0 on success, 1 on failure */
convert_ppd(const char *src,		/* I - Source (original) PPD */
            char       *dst,		/* O - Destination PPD */
	    int        dstsize,		/* I - Size of destination buffer */
	    ipp_t      *info)		/* I - Printer attributes */
{
  FILE			*srcfp,		/* Source file */
			*dstfp;		/* Destination file */
  int			dstfd;		/* Destination file descriptor */
  ipp_attribute_t	*suppattr,	/* IPP -supported attribute */
			*defattr;	/* IPP -default attribute */
  char			line[256],	/* Line from PPD file */
			junk[256],	/* Extra junk to throw away */
			*ptr,		/* Pointer into line */
			option[41],	/* Option */
			choice[41];	/* Choice */
  int			jcloption,	/* In a JCL option? */
			linenum;	/* Current line number */
  time_t		curtime;	/* Current time */
  struct tm		*curdate;	/* Current date */


 /*
  * Open the original PPD file...
  */

  if ((srcfp = fopen(src, "rb")) == NULL)
    return (1);

 /*
  * Create a temporary output file using the destination buffer...
  */

  if ((dstfd = cupsTempFd(dst, dstsize)) < 0)
  {
    fclose(srcfp);

    return (1);
  }

  if ((dstfp = fdopen(dstfd, "w")) == NULL)
  {
   /*
    * Unable to convert to FILE *...
    */

    close(dstfd);

    fclose(srcfp);

    return (1);
  }

 /*
  * Write a new header explaining that this isn't the original PPD...
  */

  fputs("*PPD-Adobe: \"4.3\"\n", dstfp);

  curtime = time(NULL);
  curdate = gmtime(&curtime);

  fprintf(dstfp, "*%% Modified on %04d%02d%02d%02d%02d%02d+0000 by cupsaddsmb\n",
          curdate->tm_year + 1900, curdate->tm_mon + 1, curdate->tm_mday,
          curdate->tm_hour, curdate->tm_min, curdate->tm_sec);

 /*
  * Read the existing PPD file, converting all PJL commands to CUPS
  * job ticket comments...
  */

  jcloption = 0;
  linenum   = 0;

  while (ppd_gets(srcfp, line, sizeof(line)) != NULL)
  {
    linenum ++;

    if (!strncmp(line, "*PPD-Adobe:", 11))
    {
     /*
      * Already wrote the PPD header...
      */

      continue;
    }
    else if (!strncmp(line, "*JCLBegin:", 10) ||
             !strncmp(line, "*JCLToPSInterpreter:", 20) ||
	     !strncmp(line, "*JCLEnd:", 8) ||
	     !strncmp(line, "*Protocols:", 11))
    {
     /*
      * Don't use existing JCL keywords; we'll create our own, below...
      */

      fprintf(dstfp, "*%% Commented out by cupsaddsmb...\n*%%%s", line + 1);
      continue;
    }
    else if (!strncmp(line, "*JCLOpenUI", 10))
    {
      jcloption = 1;
      fputs(line, dstfp);
    }
    else if (!strncmp(line, "*JCLCloseUI", 11))
    {
      jcloption = 0;
      fputs(line, dstfp);
    }
    else if (jcloption &&
             strncmp(line, "*End", 4) &&
             strncmp(line, "*Default", 8) &&
             strncmp(line, "*OrderDependency", 16))
    {
      if ((ptr = strchr(line, ':')) == NULL)
      {
        _cupsLangPrintf(stderr, NULL,
	                _("cupsaddsmb: Missing value on line %d!\n"), linenum);
        fclose(srcfp);
        fclose(dstfp);
        close(dstfd);
	unlink(dst);
	return (1);
      }

      if ((ptr = strchr(ptr, '\"')) == NULL)
      {
        _cupsLangPrintf(stderr, NULL,
	                _("cupsaddsmb: Missing double quote on line %d!\n"),
	        	linenum);
        fclose(srcfp);
        fclose(dstfp);
        close(dstfd);
	unlink(dst);
	return (1);
      }

      if (sscanf(line, "*%40s%*[ \t]%40[^/]", option, choice) != 2)
      {
        _cupsLangPrintf(stderr, NULL,
	                _("cupsaddsmb: Bad option + choice on line %d!\n"),
	        	linenum);
        fclose(srcfp);
        fclose(dstfp);
        close(dstfd);
	unlink(dst);
	return (1);
      }

      if (strchr(ptr + 1, '\"') == NULL)
      {
       /*
        * Skip remaining...
	*/

	while (ppd_gets(srcfp, junk, sizeof(junk)) != NULL)
	{
	  linenum ++;

	  if (!strncmp(junk, "*End", 4))
	    break;
	}
      }

      snprintf(ptr + 1, sizeof(line) - (ptr - line + 1),
               "%%cupsJobTicket: %s=%s\n\"\n*End\n", option, choice);

      fprintf(dstfp, "*%% Changed by cupsaddsmb...\n%s", line);
    }
    else
      fputs(line, dstfp);
  }

  fclose(srcfp);

 /*
  * Now add the CUPS-specific attributes and options...
  */

  fputs("\n*% CUPS Job Ticket support and options...\n", dstfp);
  fputs("*Protocols: PJL\n", dstfp);
  fputs("*JCLBegin: \"%!PS-Adobe-3.0<0A>\"\n", dstfp);
  fputs("*JCLToPSInterpreter: \"\"\n", dstfp);
  fputs("*JCLEnd: \"\"\n", dstfp);

  fputs("\n*OpenGroup: CUPS/CUPS Options\n\n", dstfp);

  if ((defattr = ippFindAttribute(info, "job-hold-until-default",
                                  IPP_TAG_ZERO)) != NULL &&
      (suppattr = ippFindAttribute(info, "job-hold-until-supported",
                                   IPP_TAG_ZERO)) != NULL)
    write_option(dstfp, 10, "cupsJobHoldUntil", "Hold Until", "job-hold-until",
                 suppattr, defattr, 0, 1);

  if ((defattr = ippFindAttribute(info, "job-priority-default",
                                  IPP_TAG_INTEGER)) != NULL &&
      (suppattr = ippFindAttribute(info, "job-priority-supported",
                                   IPP_TAG_RANGE)) != NULL)
    write_option(dstfp, 11, "cupsJobPriority", "Priority", "job-priority",
                 suppattr, defattr, 0, 1);

  if ((defattr = ippFindAttribute(info, "job-sheets-default",
                                  IPP_TAG_ZERO)) != NULL &&
      (suppattr = ippFindAttribute(info, "job-sheets-supported",
                                   IPP_TAG_ZERO)) != NULL)
  {
    write_option(dstfp, 20, "cupsJobSheetsStart", "Start Banner",
                 "job-sheets", suppattr, defattr, 0, 2);
    write_option(dstfp, 21, "cupsJobSheetsEnd", "End Banner",
                 "job-sheets", suppattr, defattr, 1, 2);
  }

  fputs("*CloseGroup: CUPS\n", dstfp);

  fclose(dstfp);
  close(dstfd);

  return (0);
}


/*
 * 'do_samba_command()' - Do a SAMBA command, asking for
 *                        a password as needed.
 */

int					/* O - Status of command */
do_samba_command(const char *command,	/* I - Command to run */
                 const char *address,	/* I - Address for command */
                 const char *subcmd)	/* I - Sub-command */
{
  int		status;			/* Status of command */
  char		temp[4096];		/* Command/prompt string */
  int		pid;			/* Process ID of child */


  DEBUG_printf(("do_samba_command(command=\"%s\", address=\"%s\", subcmd=\"%s\")\n",
        	command, address, subcmd));
  DEBUG_printf(("SAMBAUser=\"%s\", SAMBAPassword=\"%s\"\n", SAMBAUser,
                SAMBAPassword));

  for (status = 1; status; )
  {
    if (!SAMBAPassword)
    {
      snprintf(temp, sizeof(temp),
               _("Password for %s required to access %s via SAMBA: "),
	       SAMBAUser, SAMBAServer);

      if ((SAMBAPassword = cupsGetPassword(temp)) == NULL)
	break;
    }

    snprintf(temp, sizeof(temp), "%s%%%s", SAMBAUser, SAMBAPassword);

    if (Verbosity)
      _cupsLangPrintf(stdout, NULL,
                      _("Running command: %s %s -N -U \'%s%%%s\' -c \'%s\'\n"),
        	      command, address, SAMBAUser, SAMBAPassword, subcmd);

    if ((pid = fork()) == 0)
    {
     /*
      * Child goes here, redirect stdin/out/err and execute the command...
      */

      close(0);
      open("/dev/null", O_RDONLY);

      if (!Verbosity)
      {
        close(1);
	open("/dev/null", O_WRONLY);
	close(2);
	dup(1);
      }

      execlp(command, command, address, "-N", "-U", temp, "-c", subcmd,
             (char *)0);
      exit(errno);
    }
    else if (pid < 0)
    {
      status = -1;

      _cupsLangPrintf(stderr, NULL, _("cupsaddsmb: Unable to run \"%s\": %s\n"),
                      command, strerror(errno));
    }
    else
    {
     /*
      * Wait for the process to complete...
      */

      while (wait(&status) != pid);
    }

    DEBUG_printf(("status=%d\n", status));

    if (Verbosity)
      _cupsLangPuts(stdout, NULL, "\n");

    if (status)
    {
      if (SAMBAPassword[0])
        SAMBAPassword = NULL;
      else
        break;
    }
  }

  return (status);
}


/*
 * 'export_dest()' - Export a destination to SAMBA.
 */

int					/* O - 0 on success, non-zero on error */
export_dest(const char *dest)		/* I - Destination to export */
{
  int			status;		/* Status of smbclient/rpcclient commands */
  const char		*ppdfile;	/* PPD file for printer drivers */
  char			newppd[1024],	/* New PPD file for printer drivers */
			file[1024],	/* File to test for */
			address[1024],	/* Address for command */
			uri[1024],	/* Printer URI */
			subcmd[1024];	/* Sub-command */
  const char		*datadir;	/* CUPS_DATADIR */
  http_t		*http;		/* Connection to server */
  cups_lang_t		*language;	/* Default language */
  ipp_t			*request,	/* IPP request */
			*response;	/* IPP response */
  static const char	*pattrs[] =	/* Printer attributes we want */
			{
			  "job-hold-until-supported",
			  "job-hold-until-default",
			  "job-sheets-supported",
			  "job-sheets-default",
			  "job-priority-supported",
			  "job-priority-default"
			};


 /*
  * Get the location of the printer driver files...
  */

  if ((datadir = getenv("CUPS_DATADIR")) == NULL)
    datadir = CUPS_DATADIR;

  language = cupsLangDefault();

 /*
  * Open a connection to the scheduler...
  */

  if ((http = httpConnectEncrypt(cupsServer(), ippPort(), cupsEncryption())) == NULL)
  {
    _cupsLangPrintf(stderr, language,
                    _("cupsaddsmb: Unable to connect to server \"%s\" for "
		      "%s - %s\n"),
        	    cupsServer(), dest, strerror(errno));
    return (1);
  }

 /*
  * Get the PPD file...
  */

  if ((ppdfile = cupsGetPPD2(http, dest)) == NULL)
  {
    _cupsLangPrintf(stderr, language,
                    _("cupsaddsmb: No PPD file for printer \"%s\" - "
		      "skipping!\n"),
        	    dest);
    httpClose(http);
    return (0);
  }

 /*
  * Append the supported banner pages to the PPD file...
  */

  request = ippNew();
  request->request.op.operation_id = IPP_GET_PRINTER_ATTRIBUTES;
  request->request.op.request_id   = 1;

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  httpAssembleURIf(uri, sizeof(uri), "ipp", NULL, "localhost", 0,
                   "/printers/%s", dest);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);

  ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                "requested-attributes", sizeof(pattrs) / sizeof(pattrs[0]),
		NULL, pattrs);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/")) != NULL)
  {
    if (response->request.status.status_code > IPP_OK_CONFLICT)
    {
      _cupsLangPrintf(stderr, language,
                      _("cupsaddsmb: get-printer-attributes failed for "
		        "\"%s\": %s\n"),
        	      dest,
		      ippErrorString(response->request.status.status_code));
      ippDelete(response);
      cupsLangFree(language);
      httpClose(http);
      unlink(ppdfile);
      return (2);
    }
  }
  else
  {
    _cupsLangPrintf(stderr, language,
                    _("cupsaddsmb: get-printer-attributes failed for "
		      "\"%s\": %s\n"),
        	    dest, ippErrorString(cupsLastError()));
    cupsLangFree(language);
    httpClose(http);
    unlink(ppdfile);
    return (2);
  }

 /*
  * Convert the PPD file to the Windows driver format...
  */

  if (convert_ppd(ppdfile, newppd, sizeof(newppd), response))
  {
    _cupsLangPrintf(stderr, language,
                    _("cupsaddsmb: Unable to convert PPD file for %s - %s\n"),
        	    dest, strerror(errno));
    ippDelete(response);
    cupsLangFree(language);
    httpClose(http);
    unlink(ppdfile);
    return (3);
  }

  ippDelete(response);
  cupsLangFree(language);
  httpClose(http);

 /*
  * Remove the old PPD and point to the new one...
  */

  unlink(ppdfile);

  ppdfile = newppd;

 /*
  * See which drivers are available; the new CUPS v6 and Adobe drivers
  * depend on the Windows 2k PS driver, so copy that driver first:
  *
  * Files:
  *
  *     ps5ui.dll
  *     pscript.hlp
  *     pscript.ntf
  *     pscript5.dll
  */

  snprintf(file, sizeof(file), "%s/drivers/pscript5.dll", datadir);
  if (!access(file, 0))
  {
   /*
    * Windows 2k driver is installed; do the smbclient commands needed
    * to copy the Win2k drivers over...
    */

    snprintf(address, sizeof(address), "//%s/print$", SAMBAServer);

    snprintf(subcmd, sizeof(subcmd),
             "mkdir W32X86;"
	     "put %s W32X86/%s.ppd;"
	     "put %s/drivers/ps5ui.dll W32X86/ps5ui.dll;"
	     "put %s/drivers/pscript.hlp W32X86/pscript.hlp;"
	     "put %s/drivers/pscript.ntf W32X86/pscript.ntf;"
	     "put %s/drivers/pscript5.dll W32X86/pscript5.dll",
	     ppdfile, dest, datadir, datadir, datadir, datadir);

    if ((status = do_samba_command("smbclient", address, subcmd)) != 0)
    {
      _cupsLangPrintf(stderr, language,
                      _("cupsaddsmb: Unable to copy Windows 2000 printer "
		        "driver files (%d)!\n"),
                      status);
      unlink(ppdfile);
      return (4);
    }

   /*
    * See if we also have the CUPS driver files; if so, use them!
    */

    snprintf(file, sizeof(file), "%s/drivers/cupsps6.dll", datadir);
    if (!access(file, 0))
    {
     /*
      * Copy the CUPS driver files over...
      */

      snprintf(subcmd, sizeof(subcmd),
               "put %s/drivers/cups6.ini W32X86/cups6.ini;"
               "put %s/drivers/cupsps6.dll W32X86/cupsps6.dll;"
	       "put %s/drivers/cupsui6.dll W32X86/cupsui6.dll",
	       datadir, datadir, datadir);

      if ((status = do_samba_command("smbclient", address, subcmd)) != 0)
      {
	_cupsLangPrintf(stderr, language,
	                _("cupsaddsmb: Unable to copy CUPS printer driver "
			  "files (%d)!\n"),
        		status);
	unlink(ppdfile);
	return (4);
      }
      
     /*
      * Do the rpcclient command needed for the CUPS drivers...
      */

      snprintf(subcmd, sizeof(subcmd),
               "adddriver \"Windows NT x86\" \"%s:"
	       "pscript5.dll:%s.ppd:ps5ui.dll:pscript.hlp:NULL:RAW:"
	       "pscript5.dll,%s.ppd,ps5ui.dll,pscript.hlp,pscript.ntf,"
	       "cups6.ini,cupsps6.dll,cupsui6.dll\"",
	       dest, dest, dest);
    }
    else
    {
     /*
      * Don't have the CUPS drivers, so just use the standard Windows
      * drivers...
      */

      snprintf(subcmd, sizeof(subcmd),
               "adddriver \"Windows NT x86\" \"%s:"
	       "pscript5.dll:%s.ppd:ps5ui.dll:pscript.hlp:NULL:RAW:"
	       "pscript5.dll,%s.ppd,ps5ui.dll,pscript.hlp,pscript.ntf\"",
	       dest, dest, dest);
    }

    if ((status = do_samba_command("rpcclient", SAMBAServer, subcmd)) != 0)
    {
      _cupsLangPrintf(stderr, language,
                      _("cupsaddsmb: Unable to install Windows 2000 printer "
		        "driver files (%d)!\n"),
        	      status);
      unlink(ppdfile);
      return (5);
    }
  }

  snprintf(file, sizeof(file), "%s/drivers/ADOBEPS4.DRV", datadir);
  if (!access(file, 0))
  {
   /*
    * Do the smbclient commands needed for the Adobe Win9x drivers...
    */

    snprintf(address, sizeof(address), "//%s/print$", SAMBAServer);

    snprintf(subcmd, sizeof(subcmd),
             "mkdir WIN40;"
	     "put %s WIN40/%s.PPD;"
	     "put %s/drivers/ADFONTS.MFM WIN40/ADFONTS.MFM;"
	     "put %s/drivers/ADOBEPS4.DRV WIN40/ADOBEPS4.DRV;"
	     "put %s/drivers/ADOBEPS4.HLP WIN40/ADOBEPS4.HLP;"
	     "put %s/drivers/ICONLIB.DLL WIN40/ICONLIB.DLL;"
	     "put %s/drivers/PSMON.DLL WIN40/PSMON.DLL;",
	     ppdfile, dest, datadir, datadir, datadir, datadir, datadir);

    if ((status = do_samba_command("smbclient", address, subcmd)) != 0)
    {
      _cupsLangPrintf(stderr, language,
                      _("cupsaddsmb: Unable to copy Windows 9x printer "
		        "driver files (%d)!\n"),
        	      status);
      unlink(ppdfile);
      return (6);
    }

   /*
    * Do the rpcclient commands needed for the Adobe Win9x drivers...
    */

    snprintf(subcmd, sizeof(subcmd),
	     "adddriver \"Windows 4.0\" \"%s:ADOBEPS4.DRV:%s.PPD:NULL:"
	     "ADOBEPS4.HLP:PSMON.DLL:RAW:"
	     "ADOBEPS4.DRV,%s.PPD,ADOBEPS4.HLP,PSMON.DLL,ADFONTS.MFM,"
	     "ICONLIB.DLL\"",
	     dest, dest, dest);

    if ((status = do_samba_command("rpcclient", SAMBAServer, subcmd)) != 0)
    {
      _cupsLangPrintf(stderr, language,
                      _("cupsaddsmb: Unable to install Windows 9x printer "
		        "driver files (%d)!\n"),
        	      status);
      unlink(ppdfile);
      return (7);
    }
  }

  unlink(ppdfile);

 /*
  * Finally, associate the drivers we just added with the queue...
  */

  snprintf(subcmd, sizeof(subcmd), "setdriver %s %s", dest, dest);

  if ((status = do_samba_command("rpcclient", SAMBAServer, subcmd)) != 0)
  {
    _cupsLangPrintf(stderr, language,
                    _("cupsaddsmb: Unable to set Windows printer driver (%d)!\n"),
        	    status);
    return (8);
  }

  return (0);
}


/*
 * 'ppd_gets()' - Get a CR and/or LF-terminated line.
 */

char *					/* O - Line read or NULL on eof/error */
ppd_gets(FILE *fp,			/* I - File to read from*/
         char *buf,			/* O - String buffer */
	 int  buflen)			/* I - Size of string buffer */
{
  int		ch;			/* Character from file */
  char		*ptr,			/* Current position in line buffer */
		*end;			/* End of line buffer */


 /*
  * Range check input...
  */

  if (!fp || !buf || buflen < 2 || feof(fp))
    return (NULL);

 /*
  * Now loop until we have a valid line...
  */

  for (ptr = buf, end = buf + buflen - 1; ptr < end ;)
  {
    if ((ch = getc(fp)) == EOF)
    {
      if (ptr == buf)
        return (NULL);
      else
        break;
    }

    *ptr++ = ch;

    if (ch == '\r')
    {
     /*
      * Check for CR LF...
      */

      if ((ch = getc(fp)) != '\n')
        ungetc(ch, fp);
      else if (ptr < end)
        *ptr++ = ch;

      break;
    }
    else if (ch == '\n')
    {
     /*
      * Line feed ends a line...
      */

      break;
    }
  }

  *ptr = '\0';

  return (buf);
}


/*
 * 'usage()' - Show program usage and exit...
 */

void
usage(void)
{
  _cupsLangPuts(stdout, NULL,
                _("Usage: cupsaddsmb [options] printer1 ... printerN\n"
		  "       cupsaddsmb [options] -a\n"
		  "\n"
		  "Options:\n"
		  "  -H samba-server  Use the named SAMBA server\n"
		  "  -U samba-user    Authenticate using the named SAMBA user\n"
		  "  -a               Export all printers\n"
		  "  -h cups-server   Use the named CUPS server\n"
		  "  -v               Be verbose (show commands)\n"));
  exit(1);
}


/*
 * 'write_option()' - Write a CUPS option to a PPD file.
 */

int					/* O - 0 on success, 1 on failure */
write_option(FILE            *dstfp,	/* I - PPD file */
             int             order,	/* I - Order dependency */
             const char      *name,	/* I - Option name */
	     const char      *text,	/* I - Option text */
             const char      *attrname,	/* I - Attribute name */
             ipp_attribute_t *suppattr,	/* I - IPP -supported attribute */
	     ipp_attribute_t *defattr,	/* I - IPP -default attribute */
	     int             defval,	/* I - Default value number */
	     int             valcount)	/* I - Number of values */
{
  int	i;				/* Looping var */


  if (!dstfp || !name || !text || !suppattr || !defattr)
    return (1);

  fprintf(dstfp, "*JCLOpenUI *%s/%s: PickOne\n"
                 "*OrderDependency: %d JCLSetup *%s\n",
          name, text, order, name);

  if (defattr->value_tag == IPP_TAG_INTEGER)
  {
   /*
    * Do numeric options with a range or list...
    */

    fprintf(dstfp, "*Default%s: %d\n", name, defattr->values[defval].integer);

    if (suppattr->value_tag == IPP_TAG_RANGE)
    {
     /*
      * List each number in the range...
      */

      for (i = suppattr->values[0].range.lower;
           i <= suppattr->values[0].range.upper;
	   i ++)
      {
        fprintf(dstfp, "*%s %d: \"", name, i);

        if (valcount == 1)
	  fprintf(dstfp, "%%cupsJobTicket: %s=%d\n\"\n*End\n", attrname, i);
        else if (defval == 0)
	  fprintf(dstfp, "%%cupsJobTicket: %s=%d\"\n", attrname, i);
        else if (defval < (valcount - 1))
	  fprintf(dstfp, ",%d\"\n", i);
        else
	  fprintf(dstfp, ",%d\n\"\n*End\n", i);
      }
    }
    else
    {
     /*
      * List explicit numbers...
      */

      for (i = 0; i < suppattr->num_values; i ++)
      {
        fprintf(dstfp, "*%s %d: \"", name, suppattr->values[i].integer);

        if (valcount == 1)
	  fprintf(dstfp, "%%cupsJobTicket: %s=%d\n\"\n*End\n", attrname,
	          suppattr->values[i].integer);
        else if (defval == 0)
	  fprintf(dstfp, "%%cupsJobTicket: %s=%d\"\n", attrname,
	          suppattr->values[i].integer);
        else if (defval < (valcount - 1))
	  fprintf(dstfp, ",%d\"\n", suppattr->values[i].integer);
        else
	  fprintf(dstfp, ",%d\n\"\n*End\n", suppattr->values[i].integer);
      }
    }
  }
  else
  {
   /*
    * Do text options with a list...
    */

    fprintf(dstfp, "*Default%s: %s\n", name,
            defattr->values[defval].string.text);

    for (i = 0; i < suppattr->num_values; i ++)
    {
      fprintf(dstfp, "*%s %s: \"", name, suppattr->values[i].string.text);

      if (valcount == 1)
	fprintf(dstfp, "%%cupsJobTicket: %s=%s\n\"\n*End\n", attrname,
	        suppattr->values[i].string.text);
      else if (defval == 0)
	fprintf(dstfp, "%%cupsJobTicket: %s=%s\"\n", attrname,
	        suppattr->values[i].string.text);
      else if (defval < (valcount - 1))
	fprintf(dstfp, ",%s\"\n", suppattr->values[i].string.text);
      else
	fprintf(dstfp, ",%s\n\"\n*End\n", suppattr->values[i].string.text);
    }
  }

  fprintf(dstfp, "*JCLCloseUI: *%s\n\n", name);

  return (0);
}


/*
 * End of "$Id$".
 */
