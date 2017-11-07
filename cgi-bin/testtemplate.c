/*
 * CGI template test program for CUPS.
 *
 * Copyright 2007-2011 by Apple Inc.
 * Copyright 2006 by Easy Software Products.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more information.
 */

/*
 * Include necessary headers...
 */

#include "cgi.h"


/*
 * 'main()' - Test the template code.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  int	i;				/* Looping var */
  char	*value;				/* Value in name=value */
  FILE	*out;				/* Where to send output */


 /*
  * Don't buffer stdout or stderr so that the mixed output is sane...
  */

  setbuf(stdout, NULL);
  setbuf(stderr, NULL);

 /*
  * Loop through the command-line, assigning variables for any args with
  * "name=value"...
  */

  out = stdout;

  for (i = 1; i < argc; i ++)
  {
    if (!strcmp(argv[i], "-o"))
    {
      i ++;
      if (i < argc)
      {
        out = fopen(argv[i], "w");
	if (!out)
	{
	  perror(argv[i]);
	  return (1);
	}
      }
    }
    else if (!strcmp(argv[i], "-e"))
    {
      i ++;

      if (i < argc)
      {
        if (!freopen(argv[i], "w", stderr))
	{
	  perror(argv[i]);
	  return (1);
	}
      }
    }
    else if (!strcmp(argv[i], "-q"))
      freopen("/dev/null", "w", stderr);
    else if ((value = strchr(argv[i], '=')) != NULL)
    {
      *value++ = '\0';
      cgiSetVariable(argv[i], value);
    }
    else
      cgiCopyTemplateFile(out, argv[i]);
  }

 /*
  * Return with no errors...
  */

  return (0);
}
