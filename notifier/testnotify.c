/*
 * "$Id: testnotify.c 10996 2013-05-29 11:51:34Z msweet $"
 *
 *   Test notifier for CUPS.
 *
 *   Copyright 2007-2011 by Apple Inc.
 *   Copyright 1997-2005 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
 *   main()             - Main entry for the test notifier.
 *   print_attributes() - Print the attributes in a request...
 */

/*
 * Include necessary headers...
 */

#include <cups/cups-private.h>


/*
 * Local functions...
 */

void	print_attributes(ipp_t *ipp, int indent);


/*
 * 'main()' - Main entry for the test notifier.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  int		i;			/* Looping var */
  ipp_t		*event;			/* Event from scheduler */
  ipp_state_t	state;			/* IPP event state */


  setbuf(stderr, NULL);

  fprintf(stderr, "DEBUG: argc=%d\n", argc);
  for (i = 0; i < argc; i ++)
    fprintf(stderr, "DEBUG: argv[%d]=\"%s\"\n", i, argv[i]);
  fprintf(stderr, "DEBUG: TMPDIR=\"%s\"\n", getenv("TMPDIR"));

  for (;;)
  {
    event = ippNew();
    while ((state = ippReadFile(0, event)) != IPP_DATA)
    {
      if (state <= IPP_IDLE)
        break;
    }

    if (state == IPP_ERROR)
      fputs("DEBUG: ippReadFile() returned IPP_ERROR!\n", stderr);

    if (state <= IPP_IDLE)
    {
      ippDelete(event);
      return (0);
    }

    print_attributes(event, 4);
    ippDelete(event);

   /*
    * If the recipient URI is "testnotify://nowait", then we exit after each
    * event...
    */

    if (!strcmp(argv[1], "testnotify://nowait"))
      return (0);
  }
}


/*
 * 'print_attributes()' - Print the attributes in a request...
 */

void
print_attributes(ipp_t *ipp,		/* I - IPP request */
                 int   indent)		/* I - Indentation */
{
  ipp_tag_t		group;		/* Current group */
  ipp_attribute_t	*attr;		/* Current attribute */
  char			buffer[1024];	/* Value buffer */


  for (group = IPP_TAG_ZERO, attr = ipp->attrs; attr; attr = attr->next)
  {
    if ((attr->group_tag == IPP_TAG_ZERO && indent <= 8) || !attr->name)
    {
      group = IPP_TAG_ZERO;
      fputc('\n', stderr);
      continue;
    }

    if (group != attr->group_tag)
    {
      group = attr->group_tag;

      fprintf(stderr, "DEBUG: %*s%s:\n\n", indent - 4, "", ippTagString(group));
    }

    ippAttributeString(attr, buffer, sizeof(buffer));

    fprintf(stderr, "DEBUG: %*s%s (%s%s) %s", indent, "", attr->name,
            attr->num_values > 1 ? "1setOf " : "",
	    ippTagString(attr->value_tag), buffer);
  }
}


/*
 * End of "$Id: testnotify.c 10996 2013-05-29 11:51:34Z msweet $".
 */
