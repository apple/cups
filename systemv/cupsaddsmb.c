/*
 * "$Id: cupsaddsmb.c,v 1.2 2001/11/09 17:19:44 mike Exp $"
 *
 *   "cupsaddsmb" command for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2001 by Easy Software Products.
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
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 *   main()             - Export printers on the command-line.
 *   do_samba_command() - Do a SAMBA command, asking for a password as needed.
 *   export_dest()      - Export a destination to SAMBA.
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <cups/cups.h>
#include <cups/string.h>
#include <errno.h>


/*
 * Local globals...
 */

int	Verbosity = 0;


/*
 * Local functions...
 */

int	do_samba_command(const char *, const char *, const char *);
int	export_dest(const char *);


/*
 * 'main()' - Export printers on the command-line.
 */

int			/* O - Exit status */
main(int  argc,		/* I - Number of command-line arguments */
     char *argv[])	/* I - Command-line arguments */
{
  int	i, j;		/* Looping vars */
  int	status;		/* Status from export_dest() */
  int	num_printers;	/* Number of printers */
  char	**printers;	/* Printers */


  for (i = 1; i < argc; i ++)
    if (strcmp(argv[i], "-a") == 0)
    {
      num_printers = cupsGetPrinters(&printers);

      for (j = 0, status = 0; j < num_printers; j ++)
        if ((status = export_dest(printers[j])) != 0)
	  break;

      for (j = 0; j < num_printers; j ++)
        free(printers[j]);

      if (num_printers)
        free(printers);

      if (status)
        return (status);
    }
    else if (strcmp(argv[i], "-U") == 0)
    {
      i ++;
      if (i >= argc)
      {
	puts("Usage: cupsaddsmb [-a] [-U user] [-v] [printer1 ... printerN]");
	return (1);
      }

      cupsSetUser(argv[i]);
    }
    else if (strcmp(argv[i], "-v") == 0)
      Verbosity = 1;
    else if (argv[i][0] != '-')
    {
      if ((status = export_dest(argv[i])) != 0)
	return (status);
    }
    else
    {
      puts("Usage: cupsaddsmb [-a] [-U user] [-v] [printer1 ... printerN]");
      return (1);
    }

  return (0);
}


/*
 * 'do_samba_command()' - Do a SAMBA command, asking for
 *                        a password as needed.
 */

int					/* O - Status of command */
do_samba_command(const char *command,	/* I - Command to run */
                 const char *args,	/* I - Argument(s) for command */
                 const char *filename)	/* I - File to use as input */
{
  int		status;			/* Status of command */
  char		temp[1024];		/* Command/prompt string */
  static const char *p = NULL;		/* Password data */


  for (status = 1;;)
  {
    if (p)
      snprintf(temp, sizeof(temp), "%s -N -U \'%s%%%s\' %s <%s",
               command, cupsUser(), p, args, filename ? filename : "/dev/null");
    else
      snprintf(temp, sizeof(temp), "%s -N -U \'%s\' %s <%s",
               command, cupsUser(), args, filename ? filename : "/dev/null");

    if (Verbosity)
    {
      printf("Running the following command:\n\n    %s\n", temp);

      if (filename)
      {
        char cat[1024];


        puts("\nwith the following input:\n");

        snprintf(cat, sizeof(cat), "cat %s", filename);
        system(cat);
      }
    }
    else
    {
      strncat(temp, " >/dev/null 2>/dev/null", sizeof(temp) - 1);
      temp[sizeof(temp) - 1] = '\0';
    }

    if ((status = system(temp)) != 0)
    {
      if (Verbosity)
        puts("");

      snprintf(temp, sizeof(temp),
               "Password for %s required to access %s via SAMBA: ",
	       cupsUser(), cupsServer());

      if ((p = cupsGetPassword(temp)) == NULL)
        break;
    }
    else
    {
      if (Verbosity)
        puts("");

      break;
    }
  }

  return (status);
}


/*
 * 'export_dest()' - Export a destination to SAMBA.
 */

int				/* O - 0 on success, non-zero on error */
export_dest(const char *dest)	/* I - Destination to export */
{
  int		status;		/* Status of smbclient/rpcclient commands */
  FILE		*fp;		/* File pointer for temp file */
  char		tempfile[1024];	/* Temporary file for print commands */
  const char	*ppdfile;	/* PPD file for printer drivers */
  char		command[1024];	/* Command to run */
  const char	*datadir;	/* CUPS_DATADIR */


  /* Get a temporary file for our smbclient and rpcclient commands... */
  cupsTempFile(tempfile, sizeof(tempfile));

  if ((datadir = getenv("CUPS_DATADIR")) == NULL)
    datadir = CUPS_DATADIR;

  /* Get the PPD file... */
  if ((ppdfile = cupsGetPPD(dest)) == NULL)
  {
    fprintf(stderr, "Warning: No PPD file for printer \"%s\"!\n", dest);
    return (1);
  }

  /* Write the smbclient commands needed for the Windows drivers... */
  if ((fp = fopen(tempfile, "w")) == NULL)
  {
    fprintf(stderr, "ERROR: Unable to create temporary file \"%s\" for export - %s\n",
	    tempfile, strerror(errno));
    unlink(ppdfile);
    return (2);
  }

  fputs("mkdir W32X86\n", fp);
  fprintf(fp, "put %s W32X86/%s.PPD\n", ppdfile, dest);
  fprintf(fp, "put %s/drivers/ADOBEPS5.DLL W32X86/ADOBEPS5.DLL\n",
          datadir);
  fprintf(fp, "put %s/drivers/ADOBEPSU.DLL W32X86/ADOBEPSU.DLL\n",
          datadir);
  fprintf(fp, "put %s/drivers/ADOBEPSU.HLP W32X86/ADOBEPSU.HLP\n",
          datadir);
  fputs("mkdir WIN40\n", fp);
  fprintf(fp, "put %s WIN40/%s.PPD\n", ppdfile, dest);
  fprintf(fp, "put %s/drivers/ADFONTS.MFM WIN40/ADFONTS.MFM\n",
          datadir);
  fprintf(fp, "put %s/drivers/ADOBEPS4.DRV WIN40/ADOBEPS4.DRV\n",
          datadir);
  fprintf(fp, "put %s/drivers/ADOBEPS4.HLP WIN40/ADOBEPS4.HLP\n",
          datadir);
  fprintf(fp, "put %s/drivers/DEFPRTR2.PPD WIN40/DEFPRTR2.PPD\n",
          datadir);
  fprintf(fp, "put %s/drivers/ICONLIB.DLL WIN40/ICONLIB.DLL\n",
          datadir);
  fprintf(fp, "put %s/drivers/PSMON.DLL WIN40/PSMON.DLL\n",
          datadir);
  fputs("quit\n", fp);

  fclose(fp);

  /* Run the smbclient command to copy the Windows drivers... */
  snprintf(command, sizeof(command), "smbclient //%s/print\\$", cupsServer());

  if ((status = do_samba_command(command, "", tempfile)) != 0)
  {
    fprintf(stderr, "ERROR: Unable to copy Windows printer driver files (%d)!\n",
            status);
    unlink(ppdfile);
    unlink(tempfile);
    return (3);
  }

  unlink(ppdfile);

  /* Write the rpcclient commands needed for the Windows drivers... */
  if ((fp = fopen(tempfile, "w")) == NULL)
  {
    fprintf(stderr, "ERROR: Unable to create temporary file \"%s\" for export - %s\n",
            tempfile, strerror(errno));
    unlink(tempfile);
    return (4);
  }

  fprintf(fp, "adddriver \"Windows NT x86\" "
              "\"%s:ADOBEPS5.DLL:%s.PPD:ADOBEPSU.DLL:ADOBEPSU.HLP:"
	      "NULL:RAW:NULL\"\n",
          dest, dest);
  fprintf(fp, "addprinter %s %s \"%s\" \"\"\n", dest, dest, dest);

 /*
  * MRS: For some reason, SAMBA doesn't like to install Win9x drivers
  *      with aux files.  They are currently commented out but further
  *      investigation is required...
  */

  fprintf(fp, "adddriver \"Windows 4.0\" "
              "\"%s:ADOBEPS4.DRV:%s.PPD:NULL:ADOBEPS4.HLP:"
	      "PSMON.DLL:RAW:NULL\"\n",
	      /*"PSMON.DLL:RAW:ADFONTS.MFM,DEFPRTR2.PPD,ICONLIB.DLL\"\n",*/
	      dest, dest);
  fputs("quit\n", fp);

  fclose(fp);

  /* Run the rpcclient command to install the Windows drivers... */
  if ((status = do_samba_command("rpcclient", cupsServer(), tempfile)) != 0)
  {
    fprintf(stderr, "ERROR: Unable to install Windows printer driver files (%d)!\n",
            status);
    unlink(tempfile);
    return (5);
  }

  unlink(tempfile);

  return (0);
}


/*
 * End of "$Id: cupsaddsmb.c,v 1.2 2001/11/09 17:19:44 mike Exp $".
 */
