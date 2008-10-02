/*
 * "$Id: testnotify.c 6649 2007-07-11 21:46:42Z mike $"
 *
 *   Test notifier for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007 by Apple Inc.
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

#include <cups/cups.h>
#include <cups/language.h>
#include <cups/string.h>


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
  int			i;		/* Looping var */
  ipp_tag_t		group;		/* Current group */
  ipp_attribute_t	*attr;		/* Current attribute */
  ipp_value_t		*val;		/* Current value */
  static const char * const tags[] =	/* Value/group tag strings */
			{
			  "reserved-00",
			  "operation-attributes-tag",
			  "job-attributes-tag",
			  "end-of-attributes-tag",
			  "printer-attributes-tag",
			  "unsupported-attributes-tag",
			  "subscription-attributes-tag",
			  "event-attributes-tag",
			  "reserved-08",
			  "reserved-09",
			  "reserved-0A",
			  "reserved-0B",
			  "reserved-0C",
			  "reserved-0D",
			  "reserved-0E",
			  "reserved-0F",
			  "unsupported",
			  "default",
			  "unknown",
			  "no-value",
			  "reserved-14",
			  "not-settable",
			  "delete-attr",
			  "admin-define",
			  "reserved-18",
			  "reserved-19",
			  "reserved-1A",
			  "reserved-1B",
			  "reserved-1C",
			  "reserved-1D",
			  "reserved-1E",
			  "reserved-1F",
			  "reserved-20",
			  "integer",
			  "boolean",
			  "enum",
			  "reserved-24",
			  "reserved-25",
			  "reserved-26",
			  "reserved-27",
			  "reserved-28",
			  "reserved-29",
			  "reserved-2a",
			  "reserved-2b",
			  "reserved-2c",
			  "reserved-2d",
			  "reserved-2e",
			  "reserved-2f",
			  "octetString",
			  "dateTime",
			  "resolution",
			  "rangeOfInteger",
			  "begCollection",
			  "textWithLanguage",
			  "nameWithLanguage",
			  "endCollection",
			  "reserved-38",
			  "reserved-39",
			  "reserved-3a",
			  "reserved-3b",
			  "reserved-3c",
			  "reserved-3d",
			  "reserved-3e",
			  "reserved-3f",
			  "reserved-40",
			  "textWithoutLanguage",
			  "nameWithoutLanguage",
			  "reserved-43",
			  "keyword",
			  "uri",
			  "uriScheme",
			  "charset",
			  "naturalLanguage",
			  "mimeMediaType",
			  "memberName"
			};


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

      fprintf(stderr, "DEBUG: %*s%s:\n\n", indent - 4, "", tags[group]);
    }

    fprintf(stderr, "DEBUG: %*s%s (", indent, "", attr->name);
    if (attr->num_values > 1)
      fputs("1setOf ", stderr);
    fprintf(stderr, "%s):", tags[attr->value_tag]);

    switch (attr->value_tag)
    {
      case IPP_TAG_ENUM :
      case IPP_TAG_INTEGER :
          for (i = 0, val = attr->values; i < attr->num_values; i ++, val ++)
	    fprintf(stderr, " %d", val->integer);
          fputc('\n', stderr);
          break;

      case IPP_TAG_BOOLEAN :
          for (i = 0, val = attr->values; i < attr->num_values; i ++, val ++)
	    fprintf(stderr, " %s", val->boolean ? "true" : "false");
          fputc('\n', stderr);
          break;

      case IPP_TAG_RANGE :
          for (i = 0, val = attr->values; i < attr->num_values; i ++, val ++)
	    fprintf(stderr, " %d-%d", val->range.lower, val->range.upper);
	  fputc('\n', stderr);
          break;

      case IPP_TAG_DATE :
          {
	    time_t	vtime;		/* Date/Time value */
	    struct tm	*vdate;		/* Date info */
	    char	vstring[256];	/* Formatted time */

	    for (i = 0, val = attr->values; i < attr->num_values; i ++, val ++)
	    {
	      vtime = ippDateToTime(val->date);
	      vdate = localtime(&vtime);
	      strftime(vstring, sizeof(vstring), "%c", vdate);
	      fprintf(stderr, " (%s)", vstring);
	    }
          }
	  fputc('\n', stderr);
          break;

      case IPP_TAG_RESOLUTION :
          for (i = 0, val = attr->values; i < attr->num_values; i ++, val ++)
	    fprintf(stderr, " %dx%d%s", val->resolution.xres,
	            val->resolution.yres,
	            val->resolution.units == IPP_RES_PER_INCH ? "dpi" : "dpc");
	  fputc('\n', stderr);
          break;

      case IPP_TAG_STRING :
      case IPP_TAG_TEXTLANG :
      case IPP_TAG_NAMELANG :
      case IPP_TAG_TEXT :
      case IPP_TAG_NAME :
      case IPP_TAG_KEYWORD :
      case IPP_TAG_URI :
      case IPP_TAG_URISCHEME :
      case IPP_TAG_CHARSET :
      case IPP_TAG_LANGUAGE :
      case IPP_TAG_MIMETYPE :
          for (i = 0, val = attr->values; i < attr->num_values; i ++, val ++)
	    fprintf(stderr, " \"%s\"", val->string.text);
	  fputc('\n', stderr);
          break;

      case IPP_TAG_BEGIN_COLLECTION :
	  fputc('\n', stderr);

          for (i = 0, val = attr->values; i < attr->num_values; i ++, val ++)
	  {
	    if (i)
	      fputc('\n', stderr);
	    print_attributes(val->collection, indent + 4);
	  }
          break;

      default :
          fprintf(stderr, "UNKNOWN (%d values)\n", attr->num_values);
          break;
    }
  }
}


/*
 * End of "$Id: testnotify.c 6649 2007-07-11 21:46:42Z mike $".
 */
