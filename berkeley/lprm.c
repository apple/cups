/*
 * "$Id: lprm.c 7261 2008-01-28 23:09:31Z mike $"
 *
 *   "lprm" command for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007 by Apple Inc.
 *   Copyright 1997-2006 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
 *   main() - Parse options and cancel jobs.
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>

#include <cups/cups.h>
#include <cups/i18n.h>
#include <cups/string.h>


/*
 * 'main()' - Parse options and cancel jobs.
 */

int				/* O - Exit status */
main(int  argc,			/* I - Number of command-line arguments */
     char *argv[])		/* I - Command-line arguments */
{
  int		i;		/* Looping var */
  int		job_id;		/* Job ID */
  const char	*name;		/* Destination printer */
  char		*instance;	/* Pointer to instance name */
  cups_dest_t	*dest,		/* Destination */
		*defdest;	/* Default destination */
  int		did_cancel;	/* Did we cancel something? */


  _cupsSetLocale(argv);

 /*
  * Setup to cancel individual print jobs...
  */

  did_cancel = 0;
  defdest    = cupsGetNamedDest(CUPS_HTTP_DEFAULT, NULL, NULL);
  name       = defdest ? defdest->name : NULL;

 /*
  * Process command-line arguments...
  */

  for (i = 1; i < argc; i ++)
    if (argv[i][0] == '-' && argv[i][1] != '\0')
      switch (argv[i][1])
      {
        case 'E' : /* Encrypt */
#ifdef HAVE_SSL
	    cupsSetEncryption(HTTP_ENCRYPT_REQUIRED);
#else
            _cupsLangPrintf(stderr,
	                    _("%s: Sorry, no encryption support compiled in!\n"),
	                    argv[0]);
#endif /* HAVE_SSL */
	    break;

        case 'P' : /* Cancel jobs on a printer */
	    if (argv[i][2])
	      name = argv[i] + 2;
	    else
	    {
	      i ++;
	      name = argv[i];
	    }

	    if ((instance = strchr(name, '/')) != NULL)
	      *instance = '\0';

	    if ((dest = cupsGetNamedDest(CUPS_HTTP_DEFAULT, name,
	                                 NULL)) == NULL)
	    {
	      _cupsLangPrintf(stderr,
	                      _("%s: Error - unknown destination \"%s\"!\n"),
			      argv[0], name);
              goto error;
	    }

	    cupsFreeDests(1, dest);
	    break;

        case 'U' : /* Username */
	    if (argv[i][2] != '\0')
	      cupsSetUser(argv[i] + 2);
	    else
	    {
	      i ++;
	      if (i >= argc)
	      {
	        _cupsLangPrintf(stderr,
		                _("%s: Error - expected username after "
				  "\'-U\' option!\n"),
		        	argv[0]);
	        goto error;
	      }

              cupsSetUser(argv[i]);
	    }
	    break;
	    
        case 'h' : /* Connect to host */
	    if (argv[i][2] != '\0')
              cupsSetServer(argv[i] + 2);
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
	        _cupsLangPrintf(stderr,
		        	_("%s: Error - expected hostname after "
			          "\'-h\' option!\n"),
				argv[0]);
		goto error;
              }
	      else
                cupsSetServer(argv[i]);
	    }

            if (defdest)
	      cupsFreeDests(1, defdest);

	    defdest = cupsGetNamedDest(CUPS_HTTP_DEFAULT, NULL, NULL);
	    name    = defdest ? defdest->name : NULL;
	    break;

	default :
	    _cupsLangPrintf(stderr,
	                    _("%s: Error - unknown option \'%c\'!\n"),
			    argv[0], argv[i][1]);
            goto error;
      }
    else
    {
     /*
      * Cancel a job or printer...
      */

      if ((dest = cupsGetNamedDest(CUPS_HTTP_DEFAULT, argv[i], NULL)) != NULL)
        cupsFreeDests(1, dest);

      if (dest)
      {
        name   = argv[i];
        job_id = 0;
      }
      else if (isdigit(argv[i][0] & 255))
      {
        name   = NULL;
        job_id = atoi(argv[i]);
      }
      else if (!strcmp(argv[i], "-"))
      {
       /*
        * Cancel all jobs
        */

        job_id = -1;
      }
      else
      {
	_cupsLangPrintf(stderr,
			_("%s: Error - unknown destination \"%s\"!\n"),
			argv[0], argv[i]);
	goto error;
      }

      if (cupsCancelJob2(CUPS_HTTP_DEFAULT, name, job_id, 0) != IPP_OK)
      {
        _cupsLangPrintf(stderr, "%s: %s\n", argv[0], cupsLastErrorString());
	goto error;
      }

      did_cancel = 1;
    }

 /*
  * If nothing has been canceled yet, cancel the current job on the specified
  * (or default) printer...
  */

  if (!did_cancel && cupsCancelJob2(CUPS_HTTP_DEFAULT, name, 0, 0) != IPP_OK)
    {
      _cupsLangPrintf(stderr, "%s: %s\n", argv[0], cupsLastErrorString());
      goto error;
    }

  if (defdest)
    cupsFreeDests(1, defdest);

  return (0);

 /*
  * If we get here there was an error, so clean up...
  */

  error:

  if (defdest)
    cupsFreeDests(1, defdest);

  return (1);
}


/*
 * End of "$Id: lprm.c 7261 2008-01-28 23:09:31Z mike $".
 */
