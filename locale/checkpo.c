/*
 * Verify that translations in the .po file have the same number and type of
 * printf-style format strings.
 *
 * Copyright 2007-2017 by Apple Inc.
 * Copyright 1997-2007 by Easy Software Products, all rights reserved.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * missing or damaged, see the license at "http://www.cups.org/".
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
static cups_array_t     *cups_load_strings(const char *filename);
static int	        cups_read_strings(cups_file_t *fp, char *buffer, size_t bufsize, char **id, char **str);
static char	        *cups_scan_strings(char *buffer);
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
      po = cups_load_strings(argv[i]);
    else
      po = _cupsMessageLoad(argv[i], 1);

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

      if (!strncmp(msg->id, "ALERT:", 6) || !strncmp(msg->id, "CRIT:", 5) ||
          !strncmp(msg->id, "DEBUG:", 6) || !strncmp(msg->id, "DEBUG2:", 7) ||
          !strncmp(msg->id, "EMERG:", 6) || !strncmp(msg->id, "ERROR:", 6) ||
          !strncmp(msg->id, "INFO:", 5) || !strncmp(msg->id, "NOTICE:", 7) ||
          !strncmp(msg->id, "WARNING:", 8))
      {
        if (pass)
	{
	  pass = 0;
	  puts("FAIL");
	}

	printf("    Bad prefix on filter message \"%s\"\n",
	       abbreviate(msg->id, idbuf, sizeof(idbuf)));
      }

      idfmt = msg->id + strlen(msg->id) - 1;
      if (idfmt >= msg->id && *idfmt == '\n')
      {
        if (pass)
	{
	  pass = 0;
	  puts("FAIL");
	}

	printf("    Trailing newline in message \"%s\"\n",
	       abbreviate(msg->id, idbuf, sizeof(idbuf)));
      }

      for (; idfmt >= msg->id; idfmt --)
        if (!isspace(*idfmt & 255))
	  break;

      if (idfmt >= msg->id && *idfmt == '!')
      {
        if (pass)
	{
	  pass = 0;
	  puts("FAIL");
	}

	printf("    Exclamation in message \"%s\"\n",
	       abbreviate(msg->id, idbuf, sizeof(idbuf)));
      }

      if ((idfmt - 2) >= msg->id && !strncmp(idfmt - 2, "...", 3))
      {
        if (pass)
	{
	  pass = 0;
	  puts("FAIL");
	}

	printf("    Ellipsis in message \"%s\"\n",
	       abbreviate(msg->id, idbuf, sizeof(idbuf)));
      }


      if (!msg->str || !msg->str[0])
      {
        untranslated ++;
	continue;
      }
      else if (strchr(msg->id, '%'))
      {
        idfmts  = collect_formats(msg->id);
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
		 abbreviate(msg->id, idbuf, sizeof(idbuf)));
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
        if (*strfmt == '\\' &&
	    strfmt[1] != '\\' && strfmt[1] != 'n' && strfmt[1] != 'r' &&
	    strfmt[1] != 't' && strfmt[1] != '\"' && !isdigit(strfmt[1] & 255))
	{
	  if (pass)
	  {
	    pass = 0;
	    puts("FAIL");
	  }

	  printf("    Bad escape \\%c in filter message \"%s\"\n"
	         "      for \"%s\"\n", strfmt[1],
		 abbreviate(msg->str, strbuf, sizeof(strbuf)),
		 abbreviate(msg->id, idbuf, sizeof(idbuf)));
          break;
        }
    }

    if (pass)
    {
      if ((untranslated * 10) >= cupsArrayCount(po) &&
          strcmp(argv[i], "cups.pot"))
      {
       /*
        * Only allow 10% of messages to be untranslated before we fail...
	*/

        pass = 0;
        puts("FAIL");
	printf("    Too many untranslated messages (%d of %d)\n",
	       untranslated, cupsArrayCount(po));
      }
      else if (untranslated > 0)
        printf("PASS (%d of %d untranslated)\n", untranslated,
	       cupsArrayCount(po));
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
 * 'cups_load_strings()' - Load a .strings file into a _cups_msg_t array.
 */

static cups_array_t *                   /* O - CUPS array of _cups_msg_t values */
cups_load_strings(const char *filename) /* I - File to load */
{
  cups_file_t     *fp;                  /* .strings file */
  cups_array_t    *po;                  /* Localization array */
  _cups_message_t *m;                   /* Localization message */
  char		  buffer[8192],	        /* Message buffer */
                  *id,		        /* ID string */
                  *str;		        /* Translated message */


  if ((fp = cupsFileOpen(filename, "r")) == NULL)
    return (NULL);

  po = _cupsMessageNew(NULL);

  while (cups_read_strings(fp, buffer, sizeof(buffer), &id, &str))
  {
    if ((m = malloc(sizeof(_cups_message_t))) == NULL)
      break;

    m->id  = strdup(id);
    m->str = strdup(str);

    if (m->id && m->str)
      cupsArrayAdd(po, m);
    else
    {
      if (m->id)
        free(m->id);

      if (m->str)
        free(m->str);

      free(m);

      cupsArrayDelete(po);
      po = NULL;
      break;
    }
  }

  cupsFileClose(fp);

  return (po);
}


/*
 * 'cups_read_strings()' - Read a pair of strings from a .strings file.
 */

static int				/* O - 1 on success, 0 on failure */
cups_read_strings(cups_file_t *strings,	/* I - .strings file */
                  char        *buffer,	/* I - Line buffer */
                  size_t      bufsize,	/* I - Size of line buffer */
		  char        **id,	/* O - Pointer to ID string */
		  char        **str)	/* O - Pointer to translation string */
{
  char	*bufptr;			/* Pointer into buffer */


  while (cupsFileGets(strings, buffer, bufsize))
  {
    if (buffer[0] != '\"')
      continue;

    *id    = buffer + 1;
    bufptr = cups_scan_strings(buffer);

    if (*bufptr != '\"')
      continue;

    *bufptr++ = '\0';

    while (*bufptr && *bufptr != '\"')
      bufptr ++;

    if (!*bufptr)
      continue;

    *str   = bufptr + 1;
    bufptr = cups_scan_strings(bufptr);

    if (*bufptr != '\"')
      continue;

    *bufptr = '\0';

    return (1);
  }

  return (0);
}


/*
 * 'cups_scan_strings()' - Scan a quoted string.
 */

static char *				/* O - End of string */
cups_scan_strings(char *buffer)		/* I - Start of string */
{
  char	*bufptr;			/* Pointer into string */


  for (bufptr = buffer + 1; *bufptr && *bufptr != '\"'; bufptr ++)
  {
    if (*bufptr == '\\')
    {
      if (bufptr[1] >= '0' && bufptr[1] <= '3' &&
	  bufptr[2] >= '0' && bufptr[2] <= '7' &&
	  bufptr[3] >= '0' && bufptr[3] <= '7')
      {
       /*
	* Decode \nnn octal escape...
	*/

	*bufptr = (char)(((((bufptr[1] - '0') << 3) | (bufptr[2] - '0')) << 3) | (bufptr[3] - '0'));
	_cups_strcpy(bufptr + 1, bufptr + 4);
      }
      else
      {
       /*
	* Decode \C escape...
	*/

	_cups_strcpy(bufptr, bufptr + 1);
	if (*bufptr == 'n')
	  *bufptr = '\n';
	else if (*bufptr == 'r')
	  *bufptr = '\r';
	else if (*bufptr == 't')
	  *bufptr = '\t';
      }
    }
  }

  return (bufptr);
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
