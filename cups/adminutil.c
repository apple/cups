/*
 * "$Id$"
 *
 *   Administration utility API definitions for the Common UNIX Printing
 *   System (CUPS).
 *
 *   Copyright 2007 by Apple Inc.
 *   Copyright 2001-2007 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   cupsAdminCreateWindowsPPD()   - Create the Windows PPD file for a printer.
 *   cupsAdminExportSamba()        - Export a printer to Samba.
 *   cupsAdminGetServerSettings()  - Get settings from the server.
 *   _cupsAdminGetServerSettings() - Get settings from the server (private).
 *   cupsAdminSetServerSettings()  - Set settings on the server.
 *   _cupsAdminSetServerSettings() - Set settings on the server (private).
 *   do_samba_command()            - Do a SAMBA command.
 *   get_cupsd_conf()              - Get the current cupsd.conf file.
 *   invalidate_cupsd_cache()      - Invalidate the cached cupsd.conf settings.
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
#include <sys/stat.h>
#ifdef WIN32
#else
#  include <unistd.h>
#  include <sys/wait.h>
#endif /* WIN32 */


/*
 * Local functions...
 */

extern int		_cupsAdminGetServerSettings(http_t *http,
				                    int *num_settings,
			                            cups_option_t **settings);
extern int		_cupsAdminSetServerSettings(http_t *http,
			                            int num_settings,
			                            cups_option_t *settings);
static int		do_samba_command(const char *command,
			                 const char *address,
			                 const char *subcommand,
					 const char *authfile,
					 FILE *logfile);
static http_status_t	get_cupsd_conf(http_t *http, _cups_globals_t *cg,
			               time_t last_update, char *name,
				       int namelen, int *remote);
static void		invalidate_cupsd_cache(_cups_globals_t *cg);
static void		write_option(cups_file_t *dstfp, int order,
			             const char *name, const char *text,
				     const char *attrname,
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
  cups_lang_t		*language;	/* Current language */
  char			line[256],	/* Line from PPD file */
			junk[256],	/* Extra junk to throw away */
			*ptr,		/* Pointer into line */
			uri[1024],	/* Printer URI */
			option[41],	/* Option */
			choice[41];	/* Choice */
  int			jcloption,	/* In a JCL option? */
			jclorder,	/* Next JCL order dependency */
			linenum;	/* Current line number */
  time_t		curtime;	/* Current time */
  struct tm		*curdate;	/* Current date */
  static const char * const pattrs[] =	/* Printer attributes we want */
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

  if ((dstfp = cupsTempFile2(buffer, bufsize)) == NULL)
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
  jclorder  = 0;
  linenum   = 0;
  language  = cupsLangDefault();

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
                            "*%%%s\n", line + 1);
      continue;
    }
    else if (!strncmp(line, "*JCLOpenUI", 10))
    {
      jcloption = 1;
      cupsFilePrintf(dstfp, "%s\n", line);
    }
    else if (!strncmp(line, "*JCLCloseUI", 11))
    {
      jcloption = 0;
      cupsFilePrintf(dstfp, "%s\n", line);
    }
    else if (jcloption && !strncmp(line, "*OrderDependency:", 17))
    {
      for (ptr = line + 17; *ptr && isspace(*ptr & 255); ptr ++);

      ptr = strchr(ptr, ' ');

      if (ptr)
      {
	cupsFilePrintf(dstfp, "*OrderDependency: %d%s\n", jclorder, ptr);
	jclorder ++;
      }
      else
        cupsFilePrintf(dstfp, "%s\n", line);
    }
    else if (jcloption &&
             strncmp(line, "*End", 4) &&
             strncmp(line, "*Default", 8))
    {
      if ((ptr = strchr(line, ':')) == NULL)
      {
        snprintf(line, sizeof(line),
	          _cupsLangString(language, _("Missing value on line %d!")),
		  linenum);
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
        snprintf(line, sizeof(line),
	         _cupsLangString(language,
		                 _("Missing double quote on line %d!")),
	         linenum);
        _cupsSetError(IPP_DOCUMENT_FORMAT_ERROR, line);

        cupsFileClose(srcfp);
        cupsFileClose(dstfp);

	unlink(src);
	unlink(buffer);

        *buffer = '\0';

	return (NULL);
      }

      if (sscanf(line, "*%40s%*[ \t]%40[^:/]", option, choice) != 2)
      {
        snprintf(line, sizeof(line),
	         _cupsLangString(language,
		                 _("Bad option + choice on line %d!")),
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
               "%%cupsJobTicket: %s=%s\n\"\n*End", option, choice);

      cupsFilePrintf(dstfp, "*%% Changed for CUPS Windows Driver...\n%s\n",
                     line);
    }
    else
      cupsFilePrintf(dstfp, "%s\n", line);
  }

  cupsFileClose(srcfp);
  unlink(src);

  if (linenum == 0)
  {
    snprintf(line, sizeof(line),
             _cupsLangString(language, _("Empty PPD file!")),
             linenum);
    _cupsSetError(IPP_DOCUMENT_FORMAT_ERROR, line);

    cupsFileClose(dstfp);
    unlink(buffer);

    *buffer = '\0';

    return (NULL);
  }

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
    write_option(dstfp, jclorder ++, "cupsJobHoldUntil", "Hold Until",
                 "job-hold-until", suppattr, defattr, 0, 1);

  if ((defattr = ippFindAttribute(response, "job-priority-default",
                                  IPP_TAG_INTEGER)) != NULL &&
      (suppattr = ippFindAttribute(response, "job-priority-supported",
                                   IPP_TAG_RANGE)) != NULL)
    write_option(dstfp, jclorder ++, "cupsJobPriority", "Priority",
                 "job-priority", suppattr, defattr, 0, 1);

  if ((defattr = ippFindAttribute(response, "job-sheets-default",
                                  IPP_TAG_ZERO)) != NULL &&
      (suppattr = ippFindAttribute(response, "job-sheets-supported",
                                   IPP_TAG_ZERO)) != NULL)
  {
    write_option(dstfp, jclorder ++, "cupsJobSheetsStart", "Start Banner",
                 "job-sheets", suppattr, defattr, 0, 2);
    write_option(dstfp, jclorder ++, "cupsJobSheetsEnd", "End Banner",
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
  int			status;		/* Status of Samba commands */
  int			have_drivers;	/* Have drivers? */
  char			file[1024],	/* File to test for */
			authfile[1024],	/* Temporary authentication file */
			address[1024],	/* Address for command */
			subcmd[1024],	/* Sub-command */
			message[1024];	/* Error message */
  cups_file_t		*fp;		/* Authentication file */
  cups_lang_t		*language;	/* Current language */
  _cups_globals_t	*cg = _cupsGlobals();
					/* Global data */


 /*
  * Range check input...
  */

  if (!dest || !ppd || !samba_server || !samba_user || !samba_password)
  {
    _cupsSetError(IPP_INTERNAL_ERROR, NULL);
    return (0);
  }

 /*
  * Create a temporary authentication file for Samba...
  */

  if ((fp = cupsTempFile2(authfile, sizeof(authfile))) == NULL)
  {
    _cupsSetError(IPP_INTERNAL_ERROR, strerror(errno));
    return (0);
  }

  cupsFilePrintf(fp, "username = %s\n", samba_user);
  cupsFilePrintf(fp, "password = %s\n", samba_password);
  cupsFileClose(fp);

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
  language     = cupsLangDefault();

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
                                   authfile, logfile)) != 0)
    {
      snprintf(message, sizeof(message),
               _cupsLangString(language,
	                       _("Unable to copy Windows 2000 printer "
	                         "driver files (%d)!")), status);

      _cupsSetError(IPP_INTERNAL_ERROR, message);

      if (logfile)
	_cupsLangPrintf(logfile, "%s\n", message);

      unlink(authfile);

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
                                     authfile, logfile)) != 0)
      {
	snprintf(message, sizeof(message),
        	 _cupsLangString(language,
	                         _("Unable to copy CUPS printer driver "
				   "files (%d)!")), status);

	_cupsSetError(IPP_INTERNAL_ERROR, message);

	if (logfile)
	  _cupsLangPrintf(logfile, "%s\n", message);

        unlink(authfile);

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
                                   authfile, logfile)) != 0)
    {
      snprintf(message, sizeof(message),
               _cupsLangString(language,
                	       _("Unable to install Windows 2000 printer "
		        	 "driver files (%d)!")), status);

      _cupsSetError(IPP_INTERNAL_ERROR, message);

      if (logfile)
	_cupsLangPrintf(logfile, "%s\n", message);

      unlink(authfile);

      return (0);
    }
  }

 /*
  * See if we have the Win9x PS driver...
  */

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
                                   authfile, logfile)) != 0)
    {
      snprintf(message, sizeof(message),
               _cupsLangString(language,
                	       _("Unable to copy Windows 9x printer "
		        	 "driver files (%d)!")), status);

      _cupsSetError(IPP_INTERNAL_ERROR, message);

      if (logfile)
	_cupsLangPrintf(logfile, "%s\n", message);

      unlink(authfile);

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
                                   authfile, logfile)) != 0)
    {
      snprintf(message, sizeof(message),
               _cupsLangString(language,
                	       _("Unable to install Windows 9x printer "
		        	 "driver files (%d)!")), status);

      _cupsSetError(IPP_INTERNAL_ERROR, message);

      if (logfile)
	_cupsLangPrintf(logfile, "%s\n", message);

      unlink(authfile);

      return (0);
    }
  }

 /*
  * See if we have the 64-bit Windows PS driver...
  *
  * Files:
  *
  *     x64/ps5ui.dll
  *     x64/pscript.hlp
  *     x64/pscript.ntf
  *     x64/pscript5.dll
  */

  snprintf(file, sizeof(file), "%s/drivers/x64/pscript5.dll", cg->cups_datadir);
  if (!access(file, 0))
  {
    have_drivers |= 4;

   /*
    * 64-bit Windows driver is installed; do the smbclient commands needed
    * to copy the Win64 drivers over...
    */

    snprintf(address, sizeof(address), "//%s/print$", samba_server);

    snprintf(subcmd, sizeof(subcmd),
             "mkdir x64;"
	     "put %s x64/%s.ppd;"
	     "put %s/drivers/x64/ps5ui.dll x64/ps5ui.dll;"
	     "put %s/drivers/x64/pscript.hlp x64/pscript.hlp;"
	     "put %s/drivers/x64/pscript.ntf x64/pscript.ntf;"
	     "put %s/drivers/x64/pscript5.dll x64/pscript5.dll",
	     ppd, dest, cg->cups_datadir, cg->cups_datadir,
	     cg->cups_datadir, cg->cups_datadir);

    if ((status = do_samba_command("smbclient", address, subcmd,
                                   authfile, logfile)) != 0)
    {
      snprintf(message, sizeof(message),
               _cupsLangString(language,
	                       _("Unable to copy 64-bit Windows printer "
	                         "driver files (%d)!")), status);

      _cupsSetError(IPP_INTERNAL_ERROR, message);

      if (logfile)
	_cupsLangPrintf(logfile, "%s\n", message);

      unlink(authfile);

      return (0);
    }

   /*
    * See if we also have the CUPS driver files; if so, use them!
    */

    snprintf(file, sizeof(file), "%s/drivers/x64/cupsps6.dll", cg->cups_datadir);
    if (!access(file, 0))
    {
     /*
      * Copy the CUPS driver files over...
      */

      snprintf(subcmd, sizeof(subcmd),
               "put %s/drivers/x64/cups6.ini x64/cups6.ini;"
               "put %s/drivers/x64/cupsps6.dll x64/cupsps6.dll;"
	       "put %s/drivers/x64/cupsui6.dll x64/cupsui6.dll",
	       cg->cups_datadir, cg->cups_datadir, cg->cups_datadir);

      if ((status = do_samba_command("smbclient", address, subcmd,
                                     authfile, logfile)) != 0)
      {
	snprintf(message, sizeof(message),
        	 _cupsLangString(language,
	                         _("Unable to copy 64-bit CUPS printer driver "
				   "files (%d)!")), status);

	_cupsSetError(IPP_INTERNAL_ERROR, message);

	if (logfile)
	  _cupsLangPrintf(logfile, "%s\n", message);

        unlink(authfile);

	return (0);
      }
      
     /*
      * Do the rpcclient command needed for the CUPS drivers...
      */

      snprintf(subcmd, sizeof(subcmd),
               "adddriver \"Windows x64\" \"%s:"
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
               "adddriver \"Windows x64\" \"%s:"
	       "pscript5.dll:%s.ppd:ps5ui.dll:pscript.hlp:NULL:RAW:"
	       "pscript5.dll,%s.ppd,ps5ui.dll,pscript.hlp,pscript.ntf\"",
	       dest, dest, dest);
    }

    if ((status = do_samba_command("rpcclient", samba_server, subcmd,
                                   authfile, logfile)) != 0)
    {
      snprintf(message, sizeof(message),
               _cupsLangString(language,
                	       _("Unable to install Windows 2000 printer "
		        	 "driver files (%d)!")), status);

      _cupsSetError(IPP_INTERNAL_ERROR, message);

      if (logfile)
	_cupsLangPrintf(logfile, "%s\n", message);

      unlink(authfile);

      return (0);
    }
  }

  if (logfile && !(have_drivers & 1))
  {
    if (!have_drivers)
      strlcpy(message,
              _cupsLangString(language,
                	      _("No Windows printer drivers are installed!")),
              sizeof(message));
    else
      strlcpy(message,
              _cupsLangString(language,
                	      _("Warning, no Windows 2000 printer drivers "
				"are installed!")),
              sizeof(message));

    _cupsSetError(IPP_NOT_FOUND, message);
    _cupsLangPrintf(logfile, "%s\n", message);
  }

  if (have_drivers == 0)
  {
    _cupsSetError(IPP_NOT_FOUND, message);
    return (0);
  }

 /*
  * Finally, associate the drivers we just added with the queue...
  */

  snprintf(subcmd, sizeof(subcmd), "setdriver %s %s", dest, dest);

  if ((status = do_samba_command("rpcclient", samba_server, subcmd,
                                 authfile, logfile)) != 0)
  {
    snprintf(message, sizeof(message),
             _cupsLangString(language,
        		     _("Unable to set Windows printer driver (%d)!")),
        		     status);

    _cupsSetError(IPP_INTERNAL_ERROR, message);

    if (logfile)
      _cupsLangPrintf(logfile, "%s\n", message);

    unlink(authfile);

    return (0);
  }

  unlink(authfile);

  return (1);
}


/*
 * 'cupsAdminGetServerSettings()' - Get settings from the server.
 *
 * The returned settings should be freed with cupsFreeOptions() when
 * you are done with them.
 *
 * @since CUPS 1.3@
 */

int					/* O - 1 on success, 0 on failure */
cupsAdminGetServerSettings(
    http_t        *http,		/* I - Connection to server */
    int           *num_settings,	/* O - Number of settings */
    cups_option_t **settings)		/* O - Settings */
{
  return (_cupsAdminGetServerSettings(http, num_settings, settings));
}


/*
 * '_cupsAdminGetServerSettings()' - Get settings from the server.
 *
 * The returned settings should be freed with cupsFreeOptions() when
 * you are done with them.
 *
 * @since CUPS 1.2@
 */

int					/* O - 1 on success, 0 on failure */
_cupsAdminGetServerSettings(
    http_t        *http,		/* I - Connection to server */
    int           *num_settings,	/* O - Number of settings */
    cups_option_t **settings)		/* O - Settings */
{
  int		i;			/* Looping var */
  cups_file_t	*cupsd;			/* cupsd.conf file */
  char		cupsdconf[1024];	/* cupsd.conf filename */
  int		remote;			/* Remote cupsd.conf file? */
  http_status_t	status;			/* Status of getting cupsd.conf */
  char		line[1024],		/* Line from cupsd.conf file */
		*value;			/* Value on line */
  cups_option_t	*setting;		/* Current setting */
  _cups_globals_t *cg = _cupsGlobals();	/* Global data */


 /*
  * Range check input...
  */

  if (!http || !num_settings || !settings)
  {
    _cupsSetError(IPP_INTERNAL_ERROR, NULL);

    if (num_settings)
      *num_settings = 0;

    if (settings)
      *settings = NULL;

    return (0);
  }

  *num_settings = 0;
  *settings     = NULL;

 /*
  * Get the cupsd.conf file...
  */

  if ((status = get_cupsd_conf(http, cg, cg->cupsd_update, cupsdconf,
                               sizeof(cupsdconf), &remote)) == HTTP_OK)
  {
    if ((cupsd = cupsFileOpen(cupsdconf, "r")) == NULL)
    {
      char	message[1024];		/* Message string */


      snprintf(message, sizeof(message),
               _cupsLangString(cupsLangDefault(), _("open of %s failed: %s")),
               cupsdconf, strerror(errno));
      _cupsSetError(IPP_INTERNAL_ERROR, message);
    }
  }
  else
    cupsd = NULL;

  if (cupsd)
  {
   /*
    * Read the file, keeping track of what settings are enabled...
    */

    int		remote_access = 0,	/* Remote access allowed? */
		remote_admin = 0,	/* Remote administration allowed? */
		remote_any = 0,		/* Remote access from anywhere allowed? */
		browsing = 1,		/* Browsing enabled? */
		browse_allow = 1,	/* Browse address set? */
		browse_address = 0,	/* Browse address set? */
		cancel_policy = 1,	/* Cancel-job policy set? */
		debug_logging = 0;	/* LogLevel debug set? */
    int		linenum = 0,		/* Line number in file */
		in_location = 0,	/* In a location section? */
		in_policy = 0,		/* In a policy section? */
		in_cancel_job = 0,	/* In a cancel-job section? */
		in_admin_location = 0;	/* In the /admin location? */


    invalidate_cupsd_cache(cg);

    cg->cupsd_update = time(NULL);
    httpGetHostname(http, cg->cupsd_hostname, sizeof(cg->cupsd_hostname));

    while (cupsFileGetConf(cupsd, line, sizeof(line), &value, &linenum))
    {
      if (!value && strncmp(line, "</", 2))
        continue;

      if (!strcasecmp(line, "Port") || !strcasecmp(line, "Listen"))
      {
	char	*port;			/* Pointer to port number, if any */


	if ((port = strrchr(value, ':')) != NULL)
	  *port = '\0';
	else if (isdigit(*value & 255))
	{
	 /*
	  * Listen on a port number implies remote access...
	  */

	  remote_access = 1;
	  continue;
	}

	if (strcasecmp(value, "localhost") && strcmp(value, "127.0.0.1")
#ifdef AF_LOCAL
            && *value != '/'
#endif /* AF_LOCAL */
#ifdef AF_INET6
            && strcmp(value, "::1")
#endif /* AF_INET6 */
	    )
	  remote_access = 1;
      }
      else if (!strcasecmp(line, "Browsing"))
      {
	browsing = !strcasecmp(value, "yes") || !strcasecmp(value, "on") ||
	           !strcasecmp(value, "true");
      }
      else if (!strcasecmp(line, "BrowseAddress"))
      {
	browse_address = 1;
      }
      else if (!strcasecmp(line, "BrowseAllow"))
      {
	browse_allow = 1;
      }
      else if (!strcasecmp(line, "BrowseOrder"))
      {
	browse_allow = !strncasecmp(value, "deny,", 5);
      }
      else if (!strcasecmp(line, "LogLevel"))
      {
	debug_logging = !strncasecmp(value, "debug", 5);
      }
      else if (!strcasecmp(line, "<Policy") && !strcasecmp(value, "default"))
      {
	in_policy = 1;
      }
      else if (!strcasecmp(line, "</Policy>"))
      {
	in_policy = 0;
      }
      else if (!strcasecmp(line, "<Limit") && in_policy)
      {
       /*
	* See if the policy limit is for the Cancel-Job operation...
	*/

	char	*valptr;		/* Pointer into value */


	while (*value)
	{
	  for (valptr = value; !isspace(*valptr & 255) && *valptr; valptr ++);

	  if (*valptr)
	    *valptr++ = '\0';

          if (!strcasecmp(value, "cancel-job") || !strcasecmp(value, "all"))
	  {
	    in_cancel_job = 1;
	    break;
	  }

          for (value = valptr; isspace(*value & 255); value ++);
	}
      }
      else if (!strcasecmp(line, "</Limit>"))
      {
	in_cancel_job = 0;
      }
      else if (!strcasecmp(line, "Require") && in_cancel_job)
      {
	cancel_policy = 0;
      }
      else if (!strcasecmp(line, "<Location"))
      {
        in_admin_location = !strcasecmp(value, "/admin");
	in_location       = 1;
      }
      else if (!strcasecmp(line, "</Location>"))
      {
	in_admin_location = 0;
	in_location       = 0;
      }
      else if (!strcasecmp(line, "Allow") && in_admin_location &&
               strcasecmp(value, "localhost") && strcasecmp(value, "127.0.0.1")
#ifdef AF_LOCAL
	       && *value != '/'
#endif /* AF_LOCAL */
#ifdef AF_INET6
	       && strcmp(value, "::1")
#endif /* AF_INET6 */
	       )
      {
	remote_admin = 1;

	if (!strcasecmp(value, "all"))
	  remote_any = 1;
      }
      else if (line[0] != '<' && !in_location && !in_policy)
        cg->cupsd_num_settings = cupsAddOption(line, value,
	                                       cg->cupsd_num_settings,
					       &(cg->cupsd_settings));
    }

    cupsFileClose(cupsd);

    cg->cupsd_num_settings = cupsAddOption(CUPS_SERVER_DEBUG_LOGGING,
                                           debug_logging ? "1" : "0",
					   cg->cupsd_num_settings,
					   &(cg->cupsd_settings));

    cg->cupsd_num_settings = cupsAddOption(CUPS_SERVER_REMOTE_ADMIN,
                                           (remote_access && remote_admin) ?
					       "1" : "0",
					   cg->cupsd_num_settings,
					   &(cg->cupsd_settings));

    cg->cupsd_num_settings = cupsAddOption(CUPS_SERVER_REMOTE_ANY,
                                           remote_any ? "1" : "0",
					   cg->cupsd_num_settings,
					   &(cg->cupsd_settings));

    cg->cupsd_num_settings = cupsAddOption(CUPS_SERVER_REMOTE_PRINTERS,
                                           (browsing && browse_allow) ?
					       "1" : "0",
					   cg->cupsd_num_settings,
					   &(cg->cupsd_settings));

    cg->cupsd_num_settings = cupsAddOption(CUPS_SERVER_SHARE_PRINTERS,
                                           (remote_access && browsing &&
					    browse_address) ? "1" : "0",
					   cg->cupsd_num_settings,
					   &(cg->cupsd_settings));

    cg->cupsd_num_settings = cupsAddOption(CUPS_SERVER_USER_CANCEL_ANY,
                                           cancel_policy ? "1" : "0",
					   cg->cupsd_num_settings,
					   &(cg->cupsd_settings));
  }
  else if (status != HTTP_NOT_MODIFIED)
    invalidate_cupsd_cache(cg);

 /*
  * Remove any temporary files and copy the settings array...
  */

  if (remote)
    unlink(cupsdconf);

  for (i = cg->cupsd_num_settings, setting = cg->cupsd_settings;
       i > 0;
       i --, setting ++)
    *num_settings = cupsAddOption(setting->name, setting->value,
                                  *num_settings, settings);

  return (cg->cupsd_num_settings > 0);
}


/*
 * 'cupsAdminSetServerSettings()' - Set settings on the server.
 *
 * @since CUPS 1.3@
 */

int					/* O - 1 on success, 0 on failure */
cupsAdminSetServerSettings(
    http_t        *http,		/* I - Connection to server */
    int           num_settings,		/* I - Number of settings */
    cups_option_t *settings)		/* I - Settings */
{
  return (_cupsAdminSetServerSettings(http, num_settings, settings));
}


/*
 * '_cupsAdminSetServerSettings()' - Set settings on the server.
 *
 * @since CUPS 1.2@
 */

int					/* O - 1 on success, 0 on failure */
_cupsAdminSetServerSettings(
    http_t        *http,		/* I - Connection to server */
    int           num_settings,		/* I - Number of settings */
    cups_option_t *settings)		/* I - Settings */
{
  int		i;			/* Looping var */
  http_status_t status;			/* GET/PUT status */
  const char	*server_port_env;	/* SERVER_PORT env var */
  int		server_port;		/* IPP port for server */
  cups_file_t	*cupsd;			/* cupsd.conf file */
  char		cupsdconf[1024];	/* cupsd.conf filename */
  int		remote;			/* Remote cupsd.conf file? */
  char		tempfile[1024];		/* Temporary new cupsd.conf */
  cups_file_t	*temp;			/* Temporary file */
  char		line[1024],		/* Line from cupsd.conf file */
		*value;			/* Value on line */
  int		linenum,		/* Line number in file */
		in_location,		/* In a location section? */
		in_policy,		/* In a policy section? */
		in_default_policy,	/* In the default policy section? */
		in_cancel_job,		/* In a cancel-job section? */
		in_admin_location,	/* In the /admin location? */
		in_conf_location,	/* In the /admin/conf location? */
		in_root_location;	/* In the / location? */
  const char	*val;			/* Setting value */
  int		remote_printers,	/* Show remote printers */
		share_printers,		/* Share local printers */
		remote_admin,		/* Remote administration allowed? */
		remote_any,		/* Remote access from anywhere? */
		user_cancel_any,	/* Cancel-job policy set? */
		debug_logging;		/* LogLevel debug set? */
  int		wrote_port_listen,	/* Wrote the port/listen lines? */
		wrote_browsing,		/* Wrote the browsing lines? */
		wrote_policy,		/* Wrote the policy? */
		wrote_loglevel,		/* Wrote the LogLevel line? */
		wrote_admin_location,	/* Wrote the /admin location? */
		wrote_conf_location,	/* Wrote the /admin/conf location? */
		wrote_root_location;	/* Wrote the / location? */
  int		indent;			/* Indentation */
  int		cupsd_num_settings;	/* New number of settings */
  int		old_remote_printers,	/* Show remote printers */
		old_share_printers,	/* Share local printers */
		old_remote_admin,	/* Remote administration allowed? */
		old_user_cancel_any,	/* Cancel-job policy set? */
		old_debug_logging;	/* LogLevel debug set? */
  cups_option_t	*cupsd_settings,	/* New settings */
		*setting;		/* Current setting */
  _cups_globals_t *cg = _cupsGlobals();	/* Global data */


 /*
  * Range check input...
  */

  if (!http || !num_settings || !settings)
  {
    _cupsSetError(IPP_INTERNAL_ERROR, NULL);

    return (0);
  }

 /*
  * Get the cupsd.conf file...
  */

  if ((status = get_cupsd_conf(http, cg, 0, cupsdconf, sizeof(cupsdconf),
                               &remote)) == HTTP_OK)
  {
    if ((cupsd = cupsFileOpen(cupsdconf, "r")) == NULL)
    {
      _cupsSetError(IPP_INTERNAL_ERROR, strerror(errno));
      return (0);
    }
  }
  else
    return (0);

 /*
  * Get current settings...
  */

  if (!_cupsAdminGetServerSettings(http, &cupsd_num_settings,
                                   &cupsd_settings))
    return (0);

  if ((val = cupsGetOption(CUPS_SERVER_DEBUG_LOGGING, cupsd_num_settings,
                           cupsd_settings)) != NULL)
    old_debug_logging = atoi(val);
  else
    old_debug_logging = 0;

  if ((val = cupsGetOption(CUPS_SERVER_REMOTE_ADMIN, cupsd_num_settings,
                           cupsd_settings)) != NULL)
    old_remote_admin = atoi(val);
  else
    old_remote_admin = 0;

  if ((val = cupsGetOption(CUPS_SERVER_REMOTE_ANY, cupsd_num_settings,
                           cupsd_settings)) != NULL)
    remote_any = atoi(val);
  else
    remote_any = 0;

  if ((val = cupsGetOption(CUPS_SERVER_REMOTE_PRINTERS, cupsd_num_settings,
                           cupsd_settings)) != NULL)
    old_remote_printers = atoi(val);
  else
    old_remote_printers = 1;

  if ((val = cupsGetOption(CUPS_SERVER_SHARE_PRINTERS, cupsd_num_settings,
                           cupsd_settings)) != NULL)
    old_share_printers = atoi(val);
  else
    old_share_printers = 0;

  if ((val = cupsGetOption(CUPS_SERVER_USER_CANCEL_ANY, cupsd_num_settings,
                           cupsd_settings)) != NULL)
    old_user_cancel_any = atoi(val);
  else
    old_user_cancel_any = 0;

  cupsFreeOptions(cupsd_num_settings, cupsd_settings);

 /*
  * Get basic settings...
  */

  if ((val = cupsGetOption(CUPS_SERVER_DEBUG_LOGGING, num_settings,
                           settings)) != NULL)
  {
    debug_logging = atoi(val);

    if (debug_logging == old_debug_logging)
    {
     /*
      * No change to this setting...
      */

      debug_logging = -1;
    }
  }
  else
    debug_logging = -1;

  if ((val = cupsGetOption(CUPS_SERVER_REMOTE_ANY, num_settings,
                           settings)) != NULL)
    remote_any = atoi(val);

  if ((val = cupsGetOption(CUPS_SERVER_REMOTE_ADMIN, num_settings,
                           settings)) != NULL)
  {
    remote_admin = atoi(val);

    if (remote_admin == old_remote_admin && remote_any < 0)
    {
     /*
      * No change to this setting...
      */

      remote_admin = -1;
    }
  }
  else
    remote_admin = -1;

  if ((val = cupsGetOption(CUPS_SERVER_REMOTE_PRINTERS, num_settings,
                           settings)) != NULL)
  {
    remote_printers = atoi(val);

    if (remote_printers == old_remote_printers)
    {
     /*
      * No change to this setting...
      */

      remote_printers = -1;
    }
  }
  else
    remote_printers = -1;

  if ((val = cupsGetOption(CUPS_SERVER_SHARE_PRINTERS, num_settings,
                           settings)) != NULL)
  {
    share_printers = atoi(val);

    if (share_printers == old_share_printers && remote_any < 0)
    {
     /*
      * No change to this setting...
      */

      share_printers = -1;
    }
  }
  else
    share_printers = -1;

  if ((val = cupsGetOption(CUPS_SERVER_USER_CANCEL_ANY, num_settings,
                           settings)) != NULL)
  {
    user_cancel_any = atoi(val);

    if (user_cancel_any == old_user_cancel_any)
    {
     /*
      * No change to this setting...
      */

      user_cancel_any = -1;
    }
  }
  else
    user_cancel_any = -1;

 /*
  * Create a temporary file for the new cupsd.conf file...
  */

  if ((temp = cupsTempFile2(tempfile, sizeof(tempfile))) == NULL)
  {
    cupsFileClose(cupsd);

    if (remote)
      unlink(cupsdconf);

    _cupsSetError(IPP_INTERNAL_ERROR, strerror(errno));
    return (0);
  }

 /*
  * Copy the old file to the new, making changes along the way...
  */

  cupsd_num_settings   = 0;
  in_admin_location    = 0;
  in_cancel_job        = 0;
  in_conf_location     = 0;
  in_default_policy    = 0;
  in_location          = 0;
  in_policy            = 0;
  in_root_location     = 0;
  linenum              = 0;
  wrote_admin_location = 0;
  wrote_browsing       = 0;
  wrote_conf_location  = 0;
  wrote_loglevel       = 0;
  wrote_policy         = 0;
  wrote_port_listen    = 0;
  wrote_root_location  = 0;
  indent               = 0;

  if ((server_port_env = getenv("SERVER_PORT")) != NULL)
  {
    if ((server_port = atoi(server_port_env)) <= 0)
      server_port = ippPort();
  }
  else
    server_port = ippPort();

  if (server_port <= 0)
    server_port = IPP_PORT;

  while (cupsFileGetConf(cupsd, line, sizeof(line), &value, &linenum))
  {
    if ((!strcasecmp(line, "Port") || !strcasecmp(line, "Listen")) &&
        (share_printers >= 0 || remote_admin >= 0))
    {
      if (!wrote_port_listen)
      {
        wrote_port_listen = 1;

	if (share_printers > 0 || remote_admin > 0)
	{
	  cupsFilePuts(temp, "# Allow remote access\n");
	  cupsFilePrintf(temp, "Port %d\n", server_port);
	}
	else
	{
	  cupsFilePuts(temp, "# Only listen for connections from the local "
	                     "machine.\n");
	  cupsFilePrintf(temp, "Listen localhost:%d\n", server_port);
	}

#ifdef CUPS_DEFAULT_DOMAINSOCKET
        if ((!value || strcmp(CUPS_DEFAULT_DOMAINSOCKET, value)) &&
	    !access(CUPS_DEFAULT_DOMAINSOCKET, 0))
          cupsFilePuts(temp, "Listen " CUPS_DEFAULT_DOMAINSOCKET "\n");
#endif /* CUPS_DEFAULT_DOMAINSOCKET */
      }
      else if (value && value[0] == '/'
#ifdef CUPS_DEFAULT_DOMAINSOCKET
               && strcmp(CUPS_DEFAULT_DOMAINSOCKET, value)
#endif /* CUPS_DEFAULT_DOMAINSOCKET */
               )
        cupsFilePrintf(temp, "Listen %s\n", value);
    }
    else if ((!strcasecmp(line, "Browsing") ||
              !strcasecmp(line, "BrowseAddress") ||
              !strcasecmp(line, "BrowseAllow") ||
              !strcasecmp(line, "BrowseDeny") ||
              !strcasecmp(line, "BrowseLocalProtocols") ||
              !strcasecmp(line, "BrowseRemoteProtocols") ||
              !strcasecmp(line, "BrowseOrder")) &&
	     (remote_printers >= 0 || share_printers >= 0))
    {
      if (!wrote_browsing)
      {
	int new_remote_printers = (remote_printers > 0 ||
				   (remote_printers == -1 &&
				    old_remote_printers > 0));
	int new_share_printers = (share_printers > 0 ||
				  (share_printers == -1 &&
				   old_share_printers > 0));

        wrote_browsing = 1;

        if (new_remote_printers || new_share_printers)
	{
	  const char *localp = cupsGetOption("BrowseLocalProtocols",
					     num_settings, settings);
	  const char *remotep = cupsGetOption("BrowseRemoteProtocols",
					      num_settings, settings);

	  if (new_remote_printers && new_share_printers)
	    cupsFilePuts(temp,
	                 "# Enable printer sharing and shared printers.\n");
	  else if (new_remote_printers)
	    cupsFilePuts(temp,
	                 "# Show shared printers on the local network.\n");
	  else
	    cupsFilePuts(temp,
	                 "# Share local printers on the local network.\n");

	  cupsFilePuts(temp, "Browsing On\n");
	  cupsFilePuts(temp, "BrowseOrder allow,deny\n");

	  if (new_remote_printers)
	  {
	    cupsFilePuts(temp, "BrowseAllow all\n");

	    if (!remotep || !*remotep)
	      cupsFilePuts(temp, "BrowseRemoteProtocols "
	                         CUPS_DEFAULT_BROWSE_REMOTE_PROTOCOLS "\n");
            else if (remotep)
	      cupsFilePrintf(temp, "BrowseRemoteProtocols %s\n", remotep);
          }
	  else
	    cupsFilePuts(temp, "BrowseRemoteProtocols\n");

	  if (new_share_printers)
	  {
	    cupsFilePuts(temp, "BrowseAddress @LOCAL\n");

	    if (!localp || !*localp)
	      cupsFilePuts(temp, "BrowseLocalProtocols "
	                         CUPS_DEFAULT_BROWSE_LOCAL_PROTOCOLS "\n");
            else if (localp)
	      cupsFilePrintf(temp, "BrowseLocalProtocols %s\n", localp);
	  }
	  else
	    cupsFilePuts(temp, "BrowseLocalProtocols\n");
        }
	else
	{
	  cupsFilePuts(temp,
	               "# Disable printer sharing and shared printers.\n");
	  cupsFilePuts(temp, "Browsing Off\n");
	}
      }
    }
    else if (!strcasecmp(line, "LogLevel") && debug_logging >= 0)
    {
      wrote_loglevel = 1;

      if (debug_logging)
      {
        cupsFilePuts(temp,
	             "# Show troubleshooting information in error_log.\n");
	cupsFilePuts(temp, "LogLevel debug\n");
      }
      else
      {
        cupsFilePuts(temp, "# Show general information in error_log.\n");
	cupsFilePuts(temp, "LogLevel info\n");
      }
    }
    else if (!strcasecmp(line, "<Policy"))
    {
      in_default_policy = !strcasecmp(value, "default");
      in_policy         = 1;

      cupsFilePrintf(temp, "%s %s>\n", line, value);
      indent += 2;
    }
    else if (!strcasecmp(line, "</Policy>"))
    {
      indent -= 2;
      if (!wrote_policy && in_default_policy)
      {
	wrote_policy = 1;

        if (!user_cancel_any)
	  cupsFilePuts(temp, "  # Only the owner or an administrator can "
	                     "cancel a job...\n"
	                     "  <Limit Cancel-Job>\n"
	                     "    Order deny,allow\n"
			     "    Require user @OWNER "
			     CUPS_DEFAULT_PRINTADMIN_AUTH "\n"
			     "  </Limit>\n");
      }

      in_policy         = 0;
      in_default_policy = 0;

      cupsFilePuts(temp, "</Policy>\n");
    }
    else if (!strcasecmp(line, "<Location"))
    {
      in_location = 1;
      indent += 2;
      if (!strcmp(value, "/admin"))
	in_admin_location = 1;
      if (!strcmp(value, "/admin/conf"))
	in_conf_location = 1;
      else if (!strcmp(value, "/"))
	in_root_location = 1;

      cupsFilePrintf(temp, "%s %s>\n", line, value);
    }
    else if (!strcasecmp(line, "</Location>"))
    {
      in_location = 0;
      indent -= 2;
      if (in_admin_location && remote_admin >= 0)
      {
	wrote_admin_location = 1;

	if (remote_admin)
          cupsFilePuts(temp, "  # Allow remote administration...\n");
	else if (remote_admin == 0)
          cupsFilePuts(temp, "  # Restrict access to the admin pages...\n");

        cupsFilePuts(temp, "  Order allow,deny\n");

	if (remote_admin)
	  cupsFilePrintf(temp, "  Allow %s\n",
	                 remote_any > 0 ? "all" : "@LOCAL");
	else
	  cupsFilePuts(temp, "  Allow localhost\n");
      }
      else if (in_conf_location && remote_admin >= 0)
      {
	wrote_conf_location = 1;

	if (remote_admin)
          cupsFilePuts(temp, "  # Allow remote access to the configuration "
	                     "files...\n");
	else
          cupsFilePuts(temp, "  # Restrict access to the configuration "
	                     "files...\n");

        cupsFilePuts(temp, "  Order allow,deny\n");

	if (remote_admin)
	  cupsFilePrintf(temp, "  Allow %s\n",
	                 remote_any > 0 ? "all" : "@LOCAL");
	else
	  cupsFilePuts(temp, "  Allow localhost\n");
      }
      else if (in_root_location && (remote_admin >= 0 || share_printers >= 0))
      {
	wrote_root_location = 1;

	if (remote_admin > 0 && share_printers > 0)
          cupsFilePuts(temp, "  # Allow shared printing and remote "
	                     "administration...\n");
	else if (remote_admin > 0)
          cupsFilePuts(temp, "  # Allow remote administration...\n");
	else if (share_printers > 0)
          cupsFilePuts(temp, "  # Allow shared printing...\n");
	else
          cupsFilePuts(temp, "  # Restrict access to the server...\n");

        cupsFilePuts(temp, "  Order allow,deny\n");

	if (remote_admin > 0 || share_printers > 0)
	  cupsFilePrintf(temp, "  Allow %s\n",
	                 remote_any > 0 ? "all" : "@LOCAL");
	else
	  cupsFilePuts(temp, "  Allow localhost\n");
      }

      in_admin_location = 0;
      in_conf_location  = 0;
      in_root_location  = 0;

      cupsFilePuts(temp, "</Location>\n");
    }
    else if (!strcasecmp(line, "<Limit") && in_default_policy)
    {
     /*
      * See if the policy limit is for the Cancel-Job operation...
      */

      char	*valptr;		/* Pointer into value */


      indent += 2;

      if (!strcasecmp(value, "cancel-job") && user_cancel_any >= 0)
      {
       /*
	* Don't write anything for this limit section...
	*/

	in_cancel_job = 2;
      }
      else
      {
	cupsFilePrintf(temp, "  %s", line);

	while (*value)
	{
	  for (valptr = value; !isspace(*valptr & 255) && *valptr; valptr ++);

	  if (*valptr)
	    *valptr++ = '\0';

          if (!strcasecmp(value, "cancel-job") && user_cancel_any >= 0)
	  {
	   /*
	    * Write everything except for this definition...
	    */

	    in_cancel_job = 1;
	  }
	  else
	    cupsFilePrintf(temp, " %s", value);

          for (value = valptr; isspace(*value & 255); value ++);
	}

	cupsFilePuts(temp, ">\n");
      }
    }
    else if (!strcasecmp(line, "</Limit>") && in_cancel_job)
    {
      indent -= 2;

      if (in_cancel_job == 1)
        cupsFilePuts(temp, "  </Limit>\n");

      wrote_policy = 1;

      if (!user_cancel_any)
	cupsFilePuts(temp, "  # Only the owner or an administrator can cancel "
	                   "a job...\n"
	                   "  <Limit Cancel-Job>\n"
	                   "    Order deny,allow\n"
	                   "    Require user @OWNER "
			   CUPS_DEFAULT_PRINTADMIN_AUTH "\n"
			   "  </Limit>\n");

      in_cancel_job = 0;
    }
    else if ((((in_admin_location || in_conf_location || in_root_location) &&
               remote_admin >= 0) ||
              (in_root_location && share_printers >= 0)) &&
             (!strcasecmp(line, "Allow") || !strcasecmp(line, "Deny") ||
	      !strcasecmp(line, "Order")))
      continue;
    else if (in_cancel_job == 2)
      continue;
    else if (!strcasecmp(line, "<Limit")  && value)
      cupsFilePrintf(temp, "  %s %s>\n", line, value);
    else if (line[0] == '<')
    {
      if (value)
      {
        cupsFilePrintf(temp, "%*s%s %s>\n", indent, "", line, value);
	indent += 2;
      }
      else
      {
	if (line[1] == '/')
	  indent -= 2;

	cupsFilePrintf(temp, "%*s%s\n", indent, "", line);
      }
    }
    else if (!in_policy && !in_location &&
             (val = cupsGetOption(line, num_settings, settings)) != NULL)
    {
     /*
      * Replace this directive's value with the new one...
      */

      cupsd_num_settings = cupsAddOption(line, val, cupsd_num_settings,
                                         &cupsd_settings);

     /*
      * Write the new value in its place, without indentation since we
      * only support setting root directives, not in sections...
      */

      cupsFilePrintf(temp, "%s %s\n", line, val);
    }
    else if (value)
    {
      if (!in_policy && !in_location)
      {
       /*
        * Record the non-policy, non-location directives that we find
	* in the server settings, since we cache this info and record it
	* in _cupsAdminGetServerSettings()...
	*/

	cupsd_num_settings = cupsAddOption(line, value, cupsd_num_settings,
                                           &cupsd_settings);
      }

      cupsFilePrintf(temp, "%*s%s %s\n", indent, "", line, value);
    }
    else
      cupsFilePrintf(temp, "%*s%s\n", indent, "", line);
  }

 /*
  * Write any missing info...
  */

  if (!wrote_browsing && (remote_printers >= 0 || share_printers >= 0))
  {
    if (remote_printers > 0 || share_printers > 0)
    {
      if (remote_printers > 0 && share_printers > 0)
	cupsFilePuts(temp, "# Enable printer sharing and shared printers.\n");
      else if (remote_printers > 0)
	cupsFilePuts(temp, "# Show shared printers on the local network.\n");
      else
	cupsFilePuts(temp, "# Share local printers on the local network.\n");

      cupsFilePuts(temp, "Browsing On\n");
      cupsFilePuts(temp, "BrowseOrder allow,deny\n");

      if (remote_printers > 0)
	cupsFilePuts(temp, "BrowseAllow all\n");

      if (share_printers > 0)
	cupsFilePuts(temp, "BrowseAddress @LOCAL\n");
    }
    else
    {
      cupsFilePuts(temp, "# Disable printer sharing and shared printers.\n");
      cupsFilePuts(temp, "Browsing Off\n");
    }
  }

  if (!wrote_loglevel && debug_logging >= 0)
  {
    if (debug_logging)
    {
      cupsFilePuts(temp, "# Show troubleshooting information in error_log.\n");
      cupsFilePuts(temp, "LogLevel debug\n");
    }
    else
    {
      cupsFilePuts(temp, "# Show general information in error_log.\n");
      cupsFilePuts(temp, "LogLevel info\n");
    }
  }

  if (!wrote_port_listen && (share_printers >= 0 || remote_admin >= 0))
  {
    if (share_printers > 0 || remote_admin > 0)
    {
      cupsFilePuts(temp, "# Allow remote access\n");
      cupsFilePrintf(temp, "Port %d\n", ippPort());
    }
    else
    {
      cupsFilePuts(temp,
                   "# Only listen for connections from the local machine.\n");
      cupsFilePrintf(temp, "Listen localhost:%d\n", ippPort());
    }

#ifdef CUPS_DEFAULT_DOMAINSOCKET
    if (!access(CUPS_DEFAULT_DOMAINSOCKET, 0))
      cupsFilePuts(temp, "Listen " CUPS_DEFAULT_DOMAINSOCKET "\n");
#endif /* CUPS_DEFAULT_DOMAINSOCKET */
  }

  if (!wrote_root_location && (remote_admin >= 0 || share_printers >= 0))
  {
    if (remote_admin > 0 && share_printers > 0)
      cupsFilePuts(temp,
                   "# Allow shared printing and remote administration...\n");
    else if (remote_admin > 0)
      cupsFilePuts(temp, "# Allow remote administration...\n");
    else if (share_printers > 0)
      cupsFilePuts(temp, "# Allow shared printing...\n");
    else
      cupsFilePuts(temp, "# Restrict access to the server...\n");

    cupsFilePuts(temp, "<Location />\n"
                       "  Order allow,deny\n");

    if (remote_admin > 0 || share_printers > 0)
      cupsFilePrintf(temp, "  Allow %s\n", remote_any > 0 ? "all" : "@LOCAL");
    else
      cupsFilePuts(temp, "  Allow localhost\n");

    cupsFilePuts(temp, "</Location>\n");
  }

  if (!wrote_admin_location && remote_admin >= 0)
  {
    if (remote_admin)
      cupsFilePuts(temp, "# Allow remote administration...\n");
    else
      cupsFilePuts(temp, "# Restrict access to the admin pages...\n");

    cupsFilePuts(temp, "<Location /admin>\n"
                       "  Order allow,deny\n");

    if (remote_admin)
      cupsFilePrintf(temp, "  Allow %s\n", remote_any > 0 ? "all" : "@LOCAL");
    else
      cupsFilePuts(temp, "  Allow localhost\n");

    cupsFilePuts(temp, "</Location>\n");
  }

  if (!wrote_conf_location && remote_admin >= 0)
  {
    if (remote_admin)
      cupsFilePuts(temp,
                   "# Allow remote access to the configuration files...\n");
    else
      cupsFilePuts(temp, "# Restrict access to the configuration files...\n");

    cupsFilePuts(temp, "<Location /admin/conf>\n"
                       "  AuthType Basic\n"
                       "  Require user @SYSTEM\n"
                       "  Order allow,deny\n");

    if (remote_admin)
      cupsFilePrintf(temp, "  Allow %s\n", remote_any > 0 ? "all" : "@LOCAL");
    else
      cupsFilePuts(temp, "  Allow localhost\n");

    cupsFilePuts(temp, "</Location>\n");
  }

  if (!wrote_policy && user_cancel_any >= 0)
  {
    cupsFilePuts(temp, "<Policy default>\n"
                       "  # Job-related operations must be done by the owner "
		       "or an administrator...\n"
                       "  <Limit Send-Document Send-URI Hold-Job Release-Job "
		       "Restart-Job Purge-Jobs Set-Job-Attributes "
		       "Create-Job-Subscription Renew-Subscription "
		       "Cancel-Subscription Get-Notifications Reprocess-Job "
		       "Cancel-Current-Job Suspend-Current-Job Resume-Job "
		       "CUPS-Move-Job>\n"
                       "    Require user @OWNER @SYSTEM\n"
                       "    Order deny,allow\n"
                       "  </Limit>\n"
                       "  # All administration operations require an "
		       "administrator to authenticate...\n"
		       "  <Limit Pause-Printer Resume-Printer "
                       "Set-Printer-Attributes Enable-Printer "
		       "Disable-Printer Pause-Printer-After-Current-Job "
		       "Hold-New-Jobs Release-Held-New-Jobs Deactivate-Printer "
		       "Activate-Printer Restart-Printer Shutdown-Printer "
		       "Startup-Printer Promote-Job Schedule-Job-After "
		       "CUPS-Add-Printer CUPS-Delete-Printer "
		       "CUPS-Add-Class CUPS-Delete-Class "
		       "CUPS-Accept-Jobs CUPS-Reject-Jobs "
		       "CUPS-Set-Default CUPS-Add-Device CUPS-Delete-Device>\n"
                       "    AuthType Basic\n"
		       "    Require user @SYSTEM\n"
                       "    Order deny,allow\n"
                       "</Limit>\n");

    if (!user_cancel_any)
      cupsFilePuts(temp, "  # Only the owner or an administrator can cancel "
                         "a job...\n"
	                 "  <Limit Cancel-Job>\n"
	                 "    Order deny,allow\n"
	                 "    Require user @OWNER "
			 CUPS_DEFAULT_PRINTADMIN_AUTH "\n"
			 "  </Limit>\n");

    cupsFilePuts(temp, "  <Limit All>\n"
                       "  Order deny,allow\n"
                       "  </Limit>\n"
		       "</Policy>\n");
  }

  for (i = num_settings, setting = settings; i > 0; i --, setting ++)
    if (setting->name[0] != '_' &&
        !cupsGetOption(setting->name, cupsd_num_settings, cupsd_settings))
    {
     /*
      * Add this directive to the list of directives we have written...
      */

      cupsd_num_settings = cupsAddOption(setting->name, setting->value,
                                         cupsd_num_settings, &cupsd_settings);

     /*
      * Write the new value, without indentation since we only support
      * setting root directives, not in sections...
      */

      cupsFilePrintf(temp, "%s %s\n", setting->name, setting->value);
    }

  cupsFileClose(cupsd);
  cupsFileClose(temp);

 /*
  * Upload the configuration file to the server...
  */

  status = cupsPutFile(http, "/admin/conf/cupsd.conf", tempfile);

  if (status == HTTP_CREATED)
  {
   /*
    * Updated OK, add the basic settings...
    */

    if (debug_logging >= 0)
      cupsd_num_settings = cupsAddOption(CUPS_SERVER_DEBUG_LOGGING,
                                	 debug_logging ? "1" : "0",
					 cupsd_num_settings, &cupsd_settings);
    else
      cupsd_num_settings = cupsAddOption(CUPS_SERVER_DEBUG_LOGGING,
                                	 old_debug_logging ? "1" : "0",
					 cupsd_num_settings, &cupsd_settings);

    if (remote_admin >= 0)
      cupsd_num_settings = cupsAddOption(CUPS_SERVER_REMOTE_ADMIN,
                                	 remote_admin ? "1" : "0",
					 cupsd_num_settings, &cupsd_settings);
    else
      cupsd_num_settings = cupsAddOption(CUPS_SERVER_REMOTE_ADMIN,
                                	 old_remote_admin ? "1" : "0",
					 cupsd_num_settings, &cupsd_settings);

    cupsd_num_settings = cupsAddOption(CUPS_SERVER_REMOTE_ANY,
                                       remote_any ? "1" : "0",
				       cupsd_num_settings, &cupsd_settings);

    if (remote_printers >= 0)
      cupsd_num_settings = cupsAddOption(CUPS_SERVER_REMOTE_PRINTERS,
                                	 remote_printers ? "1" : "0",
					 cupsd_num_settings, &cupsd_settings);
    else
      cupsd_num_settings = cupsAddOption(CUPS_SERVER_REMOTE_PRINTERS,
                                	 old_remote_printers ? "1" : "0",
					 cupsd_num_settings, &cupsd_settings);

    if (share_printers >= 0)
      cupsd_num_settings = cupsAddOption(CUPS_SERVER_SHARE_PRINTERS,
                                	 share_printers ? "1" : "0",
					 cupsd_num_settings, &cupsd_settings);
    else
      cupsd_num_settings = cupsAddOption(CUPS_SERVER_SHARE_PRINTERS,
                                	 old_share_printers ? "1" : "0",
					 cupsd_num_settings, &cupsd_settings);

    if (user_cancel_any >= 0)
      cupsd_num_settings = cupsAddOption(CUPS_SERVER_USER_CANCEL_ANY,
                                	 user_cancel_any ? "1" : "0",
					 cupsd_num_settings, &cupsd_settings);
    else
      cupsd_num_settings = cupsAddOption(CUPS_SERVER_USER_CANCEL_ANY,
                                	 old_user_cancel_any ? "1" : "0",
					 cupsd_num_settings, &cupsd_settings);

   /*
    * Save the new values...
    */

    invalidate_cupsd_cache(cg);

    cg->cupsd_num_settings = cupsd_num_settings;
    cg->cupsd_settings     = cupsd_settings;
    cg->cupsd_update       = time(NULL);

    httpGetHostname(http, cg->cupsd_hostname, sizeof(cg->cupsd_hostname));
  }
  else
    cupsFreeOptions(cupsd_num_settings, cupsd_settings);

 /*
  * Remote our temp files and return...
  */

  if (remote)
    unlink(cupsdconf);

  unlink(tempfile);

  return (status == HTTP_CREATED);
}


/*
 * 'do_samba_command()' - Do a SAMBA command.
 */

static int				/* O - Status of command */
do_samba_command(const char *command,	/* I - Command to run */
                 const char *address,	/* I - Address for command */
                 const char *subcmd,	/* I - Sub-command */
		 const char *authfile,	/* I - Samba authentication file */
		 FILE *logfile)		/* I - Optional log file */
{
#ifdef WIN32
  return (1);				/* Always fail on Windows... */

#else
  int		status;			/* Status of command */
  int		pid;			/* Process ID of child */


  if (logfile)
    _cupsLangPrintf(logfile,
                    _("Running command: %s %s -N -A %s -c \'%s\'\n"),
        	    command, address, authfile, subcmd);

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

    execlp(command, command, address, "-N", "-A", authfile, "-c", subcmd,
           (char *)0);
    exit(errno);
  }
  else if (pid < 0)
  {
    status = -1;

    if (logfile)
      _cupsLangPrintf(logfile, _("Unable to run \"%s\": %s\n"),
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

  if (WIFEXITED(status))
    return (WEXITSTATUS(status));
  else
    return (-WTERMSIG(status));
#endif /* WIN32 */
}


/*
 * 'get_cupsd_conf()' - Get the current cupsd.conf file.
 */

static http_status_t			/* O - Status of request */
get_cupsd_conf(
    http_t          *http,		/* I - Connection to server */
    _cups_globals_t *cg,		/* I - Global data */
    time_t          last_update,	/* I - Last update time for file */
    char            *name,		/* I - Filename buffer */
    int             namesize,		/* I - Size of filename buffer */
    int             *remote)		/* O - Remote file? */
{
  int		fd;			/* Temporary file descriptor */
#ifndef WIN32
  struct stat	info;			/* cupsd.conf file information */
#endif /* WIN32 */
  http_status_t	status;			/* Status of getting cupsd.conf */
  char		host[HTTP_MAX_HOST];	/* Hostname for connection */


 /*
  * See if we already have the data we need...
  */

  httpGetHostname(http, host, sizeof(host));

  if (strcasecmp(cg->cupsd_hostname, host))
    invalidate_cupsd_cache(cg);

  snprintf(name, namesize, "%s/cupsd.conf", cg->cups_serverroot);
  *remote = 0;

#ifndef WIN32
  if (!strcasecmp(host, "localhost") && !access(name, R_OK))
  {
   /*
    * Read the local file rather than using HTTP...
    */

    if (stat(name, &info))
    {
      char	message[1024];		/* Message string */


      snprintf(message, sizeof(message),
               _cupsLangString(cupsLangDefault(), _("stat of %s failed: %s")),
               name, strerror(errno));
      _cupsSetError(IPP_INTERNAL_ERROR, message);

      *name = '\0';

      return (HTTP_SERVER_ERROR);
    }
    else if (last_update && info.st_mtime <= last_update)
      status = HTTP_NOT_MODIFIED;
    else
      status = HTTP_OK;
  }
  else
#endif /* !WIN32 */
  {
   /*
    * Read cupsd.conf via a HTTP GET request...
    */

    if ((fd = cupsTempFd(name, namesize)) < 0)
    {
      *name = '\0';

      _cupsSetError(IPP_INTERNAL_ERROR, strerror(errno));

      invalidate_cupsd_cache(cg);

      return (HTTP_SERVER_ERROR);
    }

    *remote = 1;

    httpClearFields(http);

    if (last_update)
      httpSetField(http, HTTP_FIELD_IF_MODIFIED_SINCE,
                   httpGetDateString(last_update));

    status = cupsGetFd(http, "/admin/conf/cupsd.conf", fd);

    close(fd);

    if (status != HTTP_OK)
    {
      unlink(name);
      *name = '\0';
    }
  }

  return (status);
}


/*
 * 'invalidate_cupsd_cache()' - Invalidate the cached cupsd.conf settings.
 */

static void
invalidate_cupsd_cache(
    _cups_globals_t *cg)		/* I - Global data */
{
  cupsFreeOptions(cg->cupsd_num_settings, cg->cupsd_settings);

  cg->cupsd_hostname[0]  = '\0';
  cg->cupsd_update       = 0;
  cg->cupsd_num_settings = 0;
  cg->cupsd_settings     = NULL;
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
