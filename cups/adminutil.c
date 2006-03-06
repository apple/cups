/*
 * "$Id$"
 *
 *   Administration utility API definitions for the Common UNIX Printing
 *   System (CUPS).
 *
 *   MANY OF THE FUNCTIONS IN THIS HEADER ARE PRIVATE AND SUBJECT TO
 *   CHANGE AT ANY TIME.  USE AT YOUR OWN RISK.
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
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   cupsAdminCreateWindowsPPD()   - Create the Windows PPD file for a printer.
 *   cupsAdminExportSamba()        - Export a printer to Samba.
 *   _cupsAdminGetServerSettings() - Get settings from the server.
 *   _cupsAdminSetServerSettings() - Set settings on the server.
 *   do_samba_command()            - Do a SAMBA command.
 *   write_option()                - Write a CUPS option to a PPD file.
 */

/*
 * Include necessary headers...
 */

#include "adminutil.h"
#include "globals.h"
#include "debug.h"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>


/*
 * Local functions...
 */

static int	do_samba_command(const char *command, const char *address,
		                 const char *subcommand, const char *username,
				 const char *password, FILE *logfile);
static void	write_option(cups_file_t *dstfp, int order, const char *name,
	        	     const char *text, const char *attrname,
	        	     ipp_attribute_t *suppattr,
			     ipp_attribute_t *defattr, int defval,
			     int valcount);


/*
 * 'cupsAdminCreateWindowsPPD()' - Create the Windows PPD file for a printer.
 */

char *					/* O - PPD file or NULL */
cupsAdminCreateWindowsPPD(
    http_t     *http,			/* I - Connection to server */
    const char *dest,			/* I - Printer or class */
    char       *buffer,			/* I - Filename buffer */
    int        bufsize)			/* I - Size of filename buffer */
{
  const char		*src;		/* Source PPD filename */
  cups_file_t		*srcfp,		/* Source PPD file */
			*dstfp;		/* Destination PPD file */
  ipp_t			*request,	/* IPP request */
			*response;	/* IPP response */
  ipp_attribute_t	*suppattr,	/* IPP -supported attribute */
			*defattr;	/* IPP -default attribute */
  char			line[256],	/* Line from PPD file */
			junk[256],	/* Extra junk to throw away */
			*ptr,		/* Pointer into line */
			uri[1024],	/* Printer URI */
			option[41],	/* Option */
			choice[41];	/* Choice */
  int			jcloption,	/* In a JCL option? */
			linenum;	/* Current line number */
  time_t		curtime;	/* Current time */
  struct tm		*curdate;	/* Current date */
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
  * Range check the input...
  */

  if (buffer)
    *buffer = '\0';

  if (!http || !dest || !buffer || bufsize < 2)
    return (NULL);

 /*
  * Get the PPD file...
  */

  if ((src = cupsGetPPD2(http, dest)) == NULL)
    return (NULL);

 /*
  * Get the supported banner pages, etc. for the printer...
  */

  request = ippNewRequest(IPP_GET_PRINTER_ATTRIBUTES);

  httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                   "localhost", 0, "/printers/%s", dest);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);

  ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                "requested-attributes", sizeof(pattrs) / sizeof(pattrs[0]),
		NULL, pattrs);

 /*
  * Do the request and get back a response...
  */

  response = cupsDoRequest(http, request, "/");
  if (!response || cupsLastError() > IPP_OK_CONFLICT)
  {
    unlink(src);
    return (NULL);
  }

 /*
  * Open the original PPD file...
  */

  if ((srcfp = cupsFileOpen(src, "rb")) == NULL)
    return (NULL);

 /*
  * Create a temporary output file using the destination buffer...
  */

  if ((dstfp = cupsTempFile2(buffer, bufsize)) < 0)
  {
    cupsFileClose(srcfp);

    unlink(src);

    return (NULL);
  }

 /*
  * Write a new header explaining that this isn't the original PPD...
  */

  cupsFilePuts(dstfp, "*PPD-Adobe: \"4.3\"\n");

  curtime = time(NULL);
  curdate = gmtime(&curtime);

  cupsFilePrintf(dstfp, "*%% Modified on %04d%02d%02d%02d%02d%02d+0000 "
                        "for CUPS Windows Driver\n",
        	 curdate->tm_year + 1900, curdate->tm_mon + 1, curdate->tm_mday,
        	 curdate->tm_hour, curdate->tm_min, curdate->tm_sec);

 /*
  * Read the existing PPD file, converting all PJL commands to CUPS
  * job ticket comments...
  */

  jcloption = 0;
  linenum   = 0;

  while (cupsFileGets(srcfp, line, sizeof(line)))
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

      cupsFilePrintf(dstfp, "*%% Commented out for CUPS Windows Driver...\n"
                            "*%%%s", line + 1);
      continue;
    }
    else if (!strncmp(line, "*JCLOpenUI", 10))
    {
      jcloption = 1;
      cupsFilePuts(dstfp, line);
    }
    else if (!strncmp(line, "*JCLCloseUI", 11))
    {
      jcloption = 0;
      cupsFilePuts(dstfp, line);
    }
    else if (jcloption &&
             strncmp(line, "*End", 4) &&
             strncmp(line, "*Default", 8) &&
             strncmp(line, "*OrderDependency", 16))
    {
      if ((ptr = strchr(line, ':')) == NULL)
      {
        snprintf(line, sizeof(line), _("Missing value on line %d!\n"), linenum);
        _cupsSetError(IPP_DOCUMENT_FORMAT_ERROR, line);

        cupsFileClose(srcfp);
        cupsFileClose(dstfp);

	unlink(src);
	unlink(buffer);

        *buffer = '\0';

	return (NULL);
      }

      if ((ptr = strchr(ptr, '\"')) == NULL)
      {
        snprintf(line, sizeof(line), _("Missing double quote on line %d!\n"),
	         linenum);
        _cupsSetError(IPP_DOCUMENT_FORMAT_ERROR, line);

        cupsFileClose(srcfp);
        cupsFileClose(dstfp);

	unlink(src);
	unlink(buffer);

        *buffer = '\0';

	return (NULL);
      }

      if (sscanf(line, "*%40s%*[ \t]%40[^/]", option, choice) != 2)
      {
        snprintf(line, sizeof(line), _("Bad option + choice on line %d!\n"),
	         linenum);
        _cupsSetError(IPP_DOCUMENT_FORMAT_ERROR, line);

        cupsFileClose(srcfp);
        cupsFileClose(dstfp);

	unlink(src);
	unlink(buffer);

        *buffer = '\0';

	return (NULL);
      }

      if (strchr(ptr + 1, '\"') == NULL)
      {
       /*
        * Skip remaining...
	*/

	while (cupsFileGets(srcfp, junk, sizeof(junk)) != NULL)
	{
	  linenum ++;

	  if (!strncmp(junk, "*End", 4))
	    break;
	}
      }

      snprintf(ptr + 1, sizeof(line) - (ptr - line + 1),
               "%%cupsJobTicket: %s=%s\n\"\n*End\n", option, choice);

      cupsFilePrintf(dstfp, "*%% Changed for CUPS Windows Driver...\n%s", line);
    }
    else
      cupsFilePuts(dstfp, line);
  }

  cupsFileClose(srcfp);
  unlink(src);

 /*
  * Now add the CUPS-specific attributes and options...
  */

  cupsFilePuts(dstfp, "\n*% CUPS Job Ticket support and options...\n");
  cupsFilePuts(dstfp, "*Protocols: PJL\n");
  cupsFilePuts(dstfp, "*JCLBegin: \"%!PS-Adobe-3.0<0A>\"\n");
  cupsFilePuts(dstfp, "*JCLToPSInterpreter: \"\"\n");
  cupsFilePuts(dstfp, "*JCLEnd: \"\"\n");

  cupsFilePuts(dstfp, "\n*OpenGroup: CUPS/CUPS Options\n\n");

  if ((defattr = ippFindAttribute(response, "job-hold-until-default",
                                  IPP_TAG_ZERO)) != NULL &&
      (suppattr = ippFindAttribute(response, "job-hold-until-supported",
                                   IPP_TAG_ZERO)) != NULL)
    write_option(dstfp, 10, "cupsJobHoldUntil", "Hold Until", "job-hold-until",
                 suppattr, defattr, 0, 1);

  if ((defattr = ippFindAttribute(response, "job-priority-default",
                                  IPP_TAG_INTEGER)) != NULL &&
      (suppattr = ippFindAttribute(response, "job-priority-supported",
                                   IPP_TAG_RANGE)) != NULL)
    write_option(dstfp, 11, "cupsJobPriority", "Priority", "job-priority",
                 suppattr, defattr, 0, 1);

  if ((defattr = ippFindAttribute(response, "job-sheets-default",
                                  IPP_TAG_ZERO)) != NULL &&
      (suppattr = ippFindAttribute(response, "job-sheets-supported",
                                   IPP_TAG_ZERO)) != NULL)
  {
    write_option(dstfp, 20, "cupsJobSheetsStart", "Start Banner",
                 "job-sheets", suppattr, defattr, 0, 2);
    write_option(dstfp, 21, "cupsJobSheetsEnd", "End Banner",
                 "job-sheets", suppattr, defattr, 1, 2);
  }

  cupsFilePuts(dstfp, "*CloseGroup: CUPS\n");
  cupsFileClose(dstfp);

  ippDelete(response);

  return (buffer);
}


/*
 * 'cupsAdminExportSamba()' - Export a printer to Samba.
 */

int					/* O - 1 on success, 0 on failure */
cupsAdminExportSamba(
    const char *dest,			/* I - Destination to export */
    const char *ppd,			/* I - PPD file */
    const char *samba_server,		/* I - Samba server */
    const char *samba_user,		/* I - Samba username */
    const char *samba_password,		/* I - Samba password */
    FILE       *logfile)		/* I - Log file, if any */
{
  int			status;		/* Status of smbclient/rpcclient commands */
  int			have_drivers;	/* Have drivers? */
  char			file[1024],	/* File to test for */
			address[1024],	/* Address for command */
			subcmd[1024];	/* Sub-command */
  _cups_globals_t	*cg = _cupsGlobals();
					/* Global data */


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

  have_drivers = 0;

  snprintf(file, sizeof(file), "%s/drivers/pscript5.dll", cg->cups_datadir);
  if (!access(file, 0))
  {
    have_drivers |= 1;

   /*
    * Windows 2k driver is installed; do the smbclient commands needed
    * to copy the Win2k drivers over...
    */

    snprintf(address, sizeof(address), "//%s/print$", samba_server);

    snprintf(subcmd, sizeof(subcmd),
             "mkdir W32X86;"
	     "put %s W32X86/%s.ppd;"
	     "put %s/drivers/ps5ui.dll W32X86/ps5ui.dll;"
	     "put %s/drivers/pscript.hlp W32X86/pscript.hlp;"
	     "put %s/drivers/pscript.ntf W32X86/pscript.ntf;"
	     "put %s/drivers/pscript5.dll W32X86/pscript5.dll",
	     ppd, dest, cg->cups_datadir, cg->cups_datadir,
	     cg->cups_datadir, cg->cups_datadir);

    if ((status = do_samba_command("smbclient", address, subcmd,
                                   samba_user, samba_password, logfile)) != 0)
    {
      if (logfile)
	_cupsLangPrintf(logfile,
                	_("Unable to copy Windows 2000 printer "
		          "driver files (%d)!\n"),
                	status);
      return (0);
    }

   /*
    * See if we also have the CUPS driver files; if so, use them!
    */

    snprintf(file, sizeof(file), "%s/drivers/cupsps6.dll", cg->cups_datadir);
    if (!access(file, 0))
    {
     /*
      * Copy the CUPS driver files over...
      */

      snprintf(subcmd, sizeof(subcmd),
               "put %s/drivers/cups6.ini W32X86/cups6.ini;"
               "put %s/drivers/cupsps6.dll W32X86/cupsps6.dll;"
	       "put %s/drivers/cupsui6.dll W32X86/cupsui6.dll",
	       cg->cups_datadir, cg->cups_datadir, cg->cups_datadir);

      if ((status = do_samba_command("smbclient", address, subcmd,
                                     samba_user, samba_password, logfile)) != 0)
      {
        if (logfile)
	  _cupsLangPrintf(logfile,
	                  _("Unable to copy CUPS printer driver files (%d)!\n"),
        		  status);
	return (0);
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

    if ((status = do_samba_command("rpcclient", samba_server, subcmd,
                                   samba_user, samba_password, logfile)) != 0)
    {
      if (logfile)
	_cupsLangPrintf(logfile,
                	_("Unable to install Windows 2000 printer "
		          "driver files (%d)!\n"),
        		status);
      return (0);
    }
  }

  snprintf(file, sizeof(file), "%s/drivers/ADOBEPS4.DRV", cg->cups_datadir);
  if (!access(file, 0))
  {
    have_drivers |= 2;

   /*
    * Do the smbclient commands needed for the Adobe Win9x drivers...
    */

    snprintf(address, sizeof(address), "//%s/print$", samba_server);

    snprintf(subcmd, sizeof(subcmd),
             "mkdir WIN40;"
	     "put %s WIN40/%s.PPD;"
	     "put %s/drivers/ADFONTS.MFM WIN40/ADFONTS.MFM;"
	     "put %s/drivers/ADOBEPS4.DRV WIN40/ADOBEPS4.DRV;"
	     "put %s/drivers/ADOBEPS4.HLP WIN40/ADOBEPS4.HLP;"
	     "put %s/drivers/ICONLIB.DLL WIN40/ICONLIB.DLL;"
	     "put %s/drivers/PSMON.DLL WIN40/PSMON.DLL;",
	     ppd, dest, cg->cups_datadir, cg->cups_datadir,
	     cg->cups_datadir, cg->cups_datadir, cg->cups_datadir);

    if ((status = do_samba_command("smbclient", address, subcmd,
                                   samba_user, samba_password, logfile)) != 0)
    {
      if (logfile)
	_cupsLangPrintf(logfile,
                	_("Unable to copy Windows 9x printer "
		          "driver files (%d)!\n"),
        		status);
      return (0);
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

    if ((status = do_samba_command("rpcclient", samba_server, subcmd,
                                   samba_user, samba_password, logfile)) != 0)
    {
      if (logfile)
	_cupsLangPrintf(logfile,
                	_("Unable to install Windows 9x printer "
		          "driver files (%d)!\n"),
        		status);
      return (0);
    }
  }

  if (logfile)
  {
    if (have_drivers == 0)
      _cupsLangPuts(logfile,
                    _("No Windows printer drivers are installed!\n"));
    else if (have_drivers == 2)
      _cupsLangPuts(logfile,
                    _("Warning, no Windows 2000 printer drivers "
		      "are installed!\n"));
  }

  if (have_drivers == 0)
    return (0);

 /*
  * Finally, associate the drivers we just added with the queue...
  */

  snprintf(subcmd, sizeof(subcmd), "setdriver %s %s", dest, dest);

  if ((status = do_samba_command("rpcclient", samba_server, subcmd,
                                 samba_user, samba_password, logfile)) != 0)
  {
    if (logfile)
      _cupsLangPrintf(logfile,
                      _("Unable to set Windows printer driver (%d)!\n"),
        	      status);
    return (0);
  }

  return (1);
}


/*
 * '_cupsAdminGetServerSettings()' - Get settings from the server.
 */

int					/* O - 1 on success, 0 on failure */
_cupsAdminGetServerSettings(
    http_t        *http,		/* I - Connection to server */
    int           *num_settings,	/* O - Number of settings */
    cups_option_t **settings)		/* O - Settings */
{
  return (1);
}


/*
 * '_cupsAdminSetServerSettings()' - Set settings on the server.
 */

int					/* O - 1 on success, 0 on failure */
_cupsAdminSetServerSettings(
    http_t        *http,		/* I - Connection to server */
    int           num_settings,		/* I - Number of settings */
    cups_option_t *settings)		/* I - Settings */
{
  return (1);
}


/*
 * 'do_samba_command()' - Do a SAMBA command.
 */

static int				/* O - Status of command */
do_samba_command(const char *command,	/* I - Command to run */
                 const char *address,	/* I - Address for command */
                 const char *subcmd,	/* I - Sub-command */
		 const char *username,	/* I - Samba user */
		 const char *password,	/* I - Samba password */
		 FILE *logfile)		/* I - Optional log file */
{
  int		status;			/* Status of command */
  char		temp[256];		/* Username+password string */
  int		pid;			/* Process ID of child */


  snprintf(temp, sizeof(temp), "%s%%%s", username, password);

  if (logfile)
    _cupsLangPrintf(logfile,
                    _("Running command: %s %s -N -U \'%s%%%s\' -c \'%s\'\n"),
        	    command, address, username, password, subcmd);

  if ((pid = fork()) == 0)
  {
   /*
    * Child goes here, redirect stdin/out/err and execute the command...
    */

    close(0);
    open("/dev/null", O_RDONLY);

    close(1);

    if (logfile)
      dup(fileno(logfile));
    else
      open("/dev/null", O_WRONLY);

    close(2);
    dup(1);

    execlp(command, command, address, "-N", "-U", temp, "-c", subcmd,
           (char *)0);
    exit(errno);
  }
  else if (pid < 0)
  {
    status = -1;

    _cupsLangPrintf(stderr, _("cupsaddsmb: Unable to run \"%s\": %s\n"),
                    command, strerror(errno));
  }
  else
  {
   /*
    * Wait for the process to complete...
    */

    while (wait(&status) != pid);
  }

  if (logfile)
    _cupsLangPuts(logfile, "\n");

  DEBUG_printf(("status=%d\n", status));

  return (status);
}


/*
 * 'write_option()' - Write a CUPS option to a PPD file.
 */

static void
write_option(cups_file_t     *dstfp,	/* I - PPD file */
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


  cupsFilePrintf(dstfp, "*JCLOpenUI *%s/%s: PickOne\n"
                        "*OrderDependency: %d JCLSetup *%s\n",
                 name, text, order, name);

  if (defattr->value_tag == IPP_TAG_INTEGER)
  {
   /*
    * Do numeric options with a range or list...
    */

    cupsFilePrintf(dstfp, "*Default%s: %d\n", name,
                   defattr->values[defval].integer);

    if (suppattr->value_tag == IPP_TAG_RANGE)
    {
     /*
      * List each number in the range...
      */

      for (i = suppattr->values[0].range.lower;
           i <= suppattr->values[0].range.upper;
	   i ++)
      {
        cupsFilePrintf(dstfp, "*%s %d: \"", name, i);

        if (valcount == 1)
	  cupsFilePrintf(dstfp, "%%cupsJobTicket: %s=%d\n\"\n*End\n",
	                 attrname, i);
        else if (defval == 0)
	  cupsFilePrintf(dstfp, "%%cupsJobTicket: %s=%d\"\n", attrname, i);
        else if (defval < (valcount - 1))
	  cupsFilePrintf(dstfp, ",%d\"\n", i);
        else
	  cupsFilePrintf(dstfp, ",%d\n\"\n*End\n", i);
      }
    }
    else
    {
     /*
      * List explicit numbers...
      */

      for (i = 0; i < suppattr->num_values; i ++)
      {
        cupsFilePrintf(dstfp, "*%s %d: \"", name, suppattr->values[i].integer);

        if (valcount == 1)
	  cupsFilePrintf(dstfp, "%%cupsJobTicket: %s=%d\n\"\n*End\n", attrname,
	          suppattr->values[i].integer);
        else if (defval == 0)
	  cupsFilePrintf(dstfp, "%%cupsJobTicket: %s=%d\"\n", attrname,
	          suppattr->values[i].integer);
        else if (defval < (valcount - 1))
	  cupsFilePrintf(dstfp, ",%d\"\n", suppattr->values[i].integer);
        else
	  cupsFilePrintf(dstfp, ",%d\n\"\n*End\n", suppattr->values[i].integer);
      }
    }
  }
  else
  {
   /*
    * Do text options with a list...
    */

    cupsFilePrintf(dstfp, "*Default%s: %s\n", name,
                   defattr->values[defval].string.text);

    for (i = 0; i < suppattr->num_values; i ++)
    {
      cupsFilePrintf(dstfp, "*%s %s: \"", name,
                     suppattr->values[i].string.text);

      if (valcount == 1)
	cupsFilePrintf(dstfp, "%%cupsJobTicket: %s=%s\n\"\n*End\n", attrname,
	        suppattr->values[i].string.text);
      else if (defval == 0)
	cupsFilePrintf(dstfp, "%%cupsJobTicket: %s=%s\"\n", attrname,
	        suppattr->values[i].string.text);
      else if (defval < (valcount - 1))
	cupsFilePrintf(dstfp, ",%s\"\n", suppattr->values[i].string.text);
      else
	cupsFilePrintf(dstfp, ",%s\n\"\n*End\n",
	               suppattr->values[i].string.text);
    }
  }

  cupsFilePrintf(dstfp, "*JCLCloseUI: *%s\n\n", name);
}


/*
 * End of "$Id$".
 */
