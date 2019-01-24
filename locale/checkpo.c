/*
 * Verify that translations in the .po file have the same number and type of
 * printf-style format strings.
 *
 * Copyright 2007-2017 by Apple Inc.
 * Copyright 1997-2007 by Easy Software Products, all rights reserved.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more information.
 *
 * Usage:
 *
 *   checkpo filename.{po,strings} [... filenameN.{po,strings}]
 *
 * Compile with:
 *
 *   gcc -o checkpo checkpo.c `cups-config --libs`
 */

#include <cups/cups-private.h>


/*
 * Local functions...
 */

static char		*abbreviate(const char *s, char *buf, int bufsize);
static cups_array_t	*collect_formats(const char *id);
static void		free_formats(cups_array_t *fmts);


/*
 * 'main()' - Validate .po and .strings files.
 */

int					/* O - Exit code */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  int			i;		/* Looping var */
  cups_array_t		*po;		/* .po file */
  _cups_message_t	*msg;		/* Current message */
  cups_array_t		*idfmts,	/* Format strings in msgid */
			*strfmts;	/* Format strings in msgstr */
  char			*idfmt,		/* Current msgid format string */
			*strfmt;	/* Current msgstr format string */
  int			fmtidx;		/* Format index */
  int			status,		/* Exit status */
			pass,		/* Pass/fail status */
			untranslated;	/* Untranslated messages */
  char			idbuf[80],	/* Abbreviated msgid */
			strbuf[80];	/* Abbreviated msgstr */


  if (argc < 2)
  {
    puts("Usage: checkpo filename.{po,strings} [... filenameN.{po,strings}]");
    return (1);
  }

 /*
  * Check every .po or .strings file on the command-line...
  */

  for (i = 1, status = 0; i < argc; i ++)
  {
   /*
    * Use the CUPS .po loader to get the message strings...
    */

    if (strstr(argv[i], ".strings"))
      po = _cupsMessageLoad(argv[i], _CUPS_MESSAGE_STRINGS);
    else
      po = _cupsMessageLoad(argv[i], _CUPS_MESSAGE_PO | _CUPS_MESSAGE_EMPTY);

    if (!po)
    {
      perror(argv[i]);
      return (1);
    }

    if (i > 1)
      putchar('\n');
    printf("%s: ", argv[i]);
    fflush(stdout);

   /*
    * Scan every message for a % string and then match them up with
    * the corresponding string in the translation...
    */

    pass         = 1;
    untranslated = 0;

    for (msg = (_cups_message_t *)cupsArrayFirst(po);
         msg;
	 msg = (_cups_message_t *)cupsArrayNext(po))
    {
     /*
      * Make sure filter message prefixes are not translated...
      */

      if (!strncmp(msg->msg, "ALERT:", 6) || !strncmp(msg->msg, "CRIT:", 5) ||
          !strncmp(msg->msg, "DEBUG:", 6) || !strncmp(msg->msg, "DEBUG2:", 7) ||
          !strncmp(msg->msg, "EMERG:", 6) || !strncmp(msg->msg, "ERROR:", 6) ||
          !strncmp(msg->msg, "INFO:", 5) || !strncmp(msg->msg, "NOTICE:", 7) ||
          !strncmp(msg->msg, "WARNING:", 8))
      {
        if (pass)
	{
	  pass = 0;
	  puts("FAIL");
	}

	printf("    Bad prefix on filter message \"%s\"\n",
	       abbreviate(msg->msg, idbuf, sizeof(idbuf)));
      }

      idfmt = msg->msg + strlen(msg->msg) - 1;
      if (idfmt >= msg->msg && *idfmt == '\n')
      {
        if (pass)
	{
	  pass = 0;
	  puts("FAIL");
	}

	printf("    Trailing newline in message \"%s\"\n",
	       abbreviate(msg->msg, idbuf, sizeof(idbuf)));
      }

      for (; idfmt >= msg->msg; idfmt --)
        if (!isspace(*idfmt & 255))
	  break;

      if (idfmt >= msg->msg && *idfmt == '!')
      {
        if (pass)
	{
	  pass = 0;
	  puts("FAIL");
	}

	printf("    Exclamation in message \"%s\"\n",
	       abbreviate(msg->msg, idbuf, sizeof(idbuf)));
      }

      if ((idfmt - 2) >= msg->msg && !strncmp(idfmt - 2, "...", 3))
      {
        if (pass)
	{
	  pass = 0;
	  puts("FAIL");
	}

	printf("    Ellipsis in message \"%s\"\n",
	       abbreviate(msg->msg, idbuf, sizeof(idbuf)));
      }

      if (!msg->str || !msg->str[0])
      {
        untranslated ++;
	continue;
      }
      else if (strchr(msg->msg, '%'))
      {
        idfmts  = collect_formats(msg->msg);
	strfmts = collect_formats(msg->str);
	fmtidx  = 0;

        for (strfmt = (char *)cupsArrayFirst(strfmts);
	     strfmt;
	     strfmt = (char *)cupsArrayNext(strfmts))
	{
	  if (isdigit(strfmt[1] & 255) && strfmt[2] == '$')
	  {
	   /*
	    * Handle positioned format stuff...
	    */

            fmtidx = strfmt[1] - '1';
            strfmt += 3;
	    if ((idfmt = (char *)cupsArrayIndex(idfmts, fmtidx)) != NULL)
	      idfmt ++;
	  }
	  else
	  {
	   /*
	    * Compare against the current format...
	    */

	    idfmt = (char *)cupsArrayIndex(idfmts, fmtidx);
          }

	  fmtidx ++;

	  if (!idfmt || strcmp(strfmt, idfmt))
	    break;
	}

        if (cupsArrayCount(strfmts) != cupsArrayCount(idfmts) || strfmt)
	{
	  if (pass)
	  {
	    pass = 0;
	    puts("FAIL");
	  }

	  printf("    Bad translation string \"%s\"\n        for \"%s\"\n",
	         abbreviate(msg->str, strbuf, sizeof(strbuf)),
		 abbreviate(msg->msg, idbuf, sizeof(idbuf)));
          fputs("    Translation formats:", stdout);
	  for (strfmt = (char *)cupsArrayFirst(strfmts);
	       strfmt;
	       strfmt = (char *)cupsArrayNext(strfmts))
	    printf(" %s", strfmt);
          fputs("\n    Original formats:", stdout);
	  for (idfmt = (char *)cupsArrayFirst(idfmts);
	       idfmt;
	       idfmt = (char *)cupsArrayNext(idfmts))
	    printf(" %s", idfmt);
          putchar('\n');
          putchar('\n');
	}

	free_formats(idfmts);
	free_formats(strfmts);
      }

     /*
      * Only allow \\, \n, \r, \t, \", and \### character escapes...
      */

      for (strfmt = msg->str; *strfmt; strfmt ++)
      {
        if (*strfmt == '\\')
        {
          strfmt ++;

          if (*strfmt != '\\' && *strfmt != 'n' && *strfmt != 'r' && *strfmt != 't' && *strfmt != '\"' && !isdigit(*strfmt & 255))
	  {
	    if (pass)
	    {
	      pass = 0;
	      puts("FAIL");
	    }

	    printf("    Bad escape \\%c in filter message \"%s\"\n"
		   "      for \"%s\"\n", strfmt[1],
		   abbreviate(msg->str, strbuf, sizeof(strbuf)),
		   abbreviate(msg->msg, idbuf, sizeof(idbuf)));
	    break;
	  }
	}
      }
    }

    if (pass)
    {
      int count = cupsArrayCount(po);	/* Total number of messages */

      if (untranslated >= (count / 10) && strcmp(argv[i], "cups.pot"))
      {
       /*
        * Only allow 10% of messages to be untranslated before we fail...
	*/

        pass = 0;
        puts("FAIL");
	printf("    Too many untranslated messages (%d of %d or %.1f%% are translated)\n", count - untranslated, count, 100.0 - 100.0 * untranslated / count);
      }
      else if (untranslated > 0)
        printf("PASS (%d of %d or %.1f%% are translated)\n", count - untranslated, count, 100.0 - 100.0 * untranslated / count);
      else
        puts("PASS");
    }

    if (!pass)
      status = 1;

    _cupsMessageFree(po);
  }

  return (status);
}


/*
 * 'abbreviate()' - Abbreviate a message string as needed.
 */

static char *				/* O - Abbreviated string */
abbreviate(const char *s,		/* I - String to abbreviate */
           char       *buf,		/* I - Buffer */
	   int        bufsize)		/* I - Size of buffer */
{
  char	*bufptr;			/* Pointer into buffer */


  for (bufptr = buf, bufsize -= 4; *s && bufsize > 0; s ++)
  {
    if (*s == '\n')
    {
      if (bufsize < 2)
        break;

      *bufptr++ = '\\';
      *bufptr++ = 'n';
      bufsize -= 2;
    }
    else if (*s == '\t')
    {
      if (bufsize < 2)
        break;

      *bufptr++ = '\\';
      *bufptr++ = 't';
      bufsize -= 2;
    }
    else if (*s >= 0 && *s < ' ')
    {
      if (bufsize < 4)
        break;

      sprintf(bufptr, "\\%03o", *s);
      bufptr += 4;
      bufsize -= 4;
    }
    else
    {
      *bufptr++ = *s;
      bufsize --;
    }
  }

  if (*s)
    memcpy(bufptr, "...", 4);
  else
    *bufptr = '\0';

  return (buf);
}


/*
 * 'collect_formats()' - Collect all of the format strings in the msgid.
 */

static cups_array_t *			/* O - Array of format strings */
collect_formats(const char *id)		/* I - msgid string */
{
  cups_array_t	*fmts;			/* Array of format strings */
  char		buf[255],		/* Format string buffer */
		*bufptr;		/* Pointer into format string */


  fmts = cupsArrayNew(NULL, NULL);

  while ((id = strchr(id, '%')) != NULL)
  {
    if (id[1] == '%')
    {
     /*
      * Skip %%...
      */

      id += 2;
      continue;
    }

    for (bufptr = buf; *id && bufptr < (buf + sizeof(buf) - 1); id ++)
    {
      *bufptr++ = *id;

      if (strchr("CDEFGIOSUXcdeifgopsux", *id))
      {
        id ++;
        break;
      }
    }

    *bufptr = '\0';
    cupsArrayAdd(fmts, strdup(buf));
  }

  return (fmts);
}


/*
 * 'free_formats()' - Free all of the format strings.
 */

static void
free_formats(cups_array_t *fmts)	/* I - Array of format strings */
{
  char	*s;				/* Current string */


  for (s = (char *)cupsArrayFirst(fmts); s; s = (char *)cupsArrayNext(fmts))
    free(s);

  cupsArrayDelete(fmts);
}
