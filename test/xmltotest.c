/*
 * "$Id: xmltotest.c 3643 2012-02-13 16:35:48Z msweet $"
 *
 *   IANA XML registration to test file generator for CUPS.
 *
 *   Copyright 2011-2012 by Apple Inc.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Usage:
 *
 *   ./xmltotest [--ref standard] {--job|--printer} [XML file/URL] >file.test
 *
 *   If not specified, loads the XML registrations from:
 *
 *     http://www.iana.org/assignments/ipp-registrations/ipp-registrations.xml
 *
 *   "Standard" is of the form "rfcNNNN" or "pwgNNNN.N".
 *
 * Contents:
 *
 *   main()	    - Process command-line arguments.
 *   compare_reg()  - Compare two registrations.
 *   load_xml()     - Load the XML registration file or URL.
 *   match_xref()   - Compare the xref against the named standard.
 *   new_reg()	    - Create a new registration record.
 *   usage()	    - Show usage message.
 *   write_expect() - Write an EXPECT test for an attribute.
 */


#include <config.h>
#include <cups/cups.h>
#include <unistd.h>
#include <fcntl.h>

#ifdef HAVE_MXML_H
#  include <mxml.h>
/*
 * Local types...
 */

typedef struct _cups_reg_s		/**** Registration data ****/
{
  char	*name,				/* Attribute name */
	*member,			/* Member attribute name */
	*sub_member,			/* Sub-member attribute name */
	*syntax;			/* Attribute syntax */
} _cups_reg_t;


/*
 * Local functions...
 */

static int		compare_reg(_cups_reg_t *a, _cups_reg_t *b);
static mxml_node_t	*load_xml(const char *reg_file);
static int		match_xref(mxml_node_t *xref, const char *standard);
static _cups_reg_t	*new_reg(mxml_node_t *name, mxml_node_t *member,
			         mxml_node_t *sub_member, mxml_node_t *syntax);
static int		usage(void);
static void		write_expect(_cups_reg_t *reg, ipp_tag_t group);


/*
 * 'main()' - Process command-line arguments.
 */

int
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  int		i;			/* Looping var */
  const char	*reg_file = NULL,	/* Registration file/URL to use */
		*reg_standard = NULL;	/* Which standard to extract */
  mxml_node_t	*reg_xml,		/* Registration XML data */
		*reg_2,			/* ipp-registrations-2 */
		*reg_record,		/* <record> */
		*reg_collection,	/* <collection> */
		*reg_name,		/* <name> */
		*reg_member,		/* <member_attribute> */
		*reg_sub_member,	/* <sub-member_attribute> */
		*reg_syntax,		/* <syntax> */
		*reg_xref;		/* <xref> */
  cups_array_t	*attrs;			/* Attribute registrations */
  _cups_reg_t	*current;		/* Current attribute registration */
  ipp_tag_t	group = IPP_TAG_ZERO,	/* Which attributes to test */
		reg_group;		/* Group for registration */


 /*
  * Parse command-line...
  */

  for (i = 1; i < argc; i ++)
  {
    if (!strcmp(argv[i], "--job") && group == IPP_TAG_ZERO)
      group = IPP_TAG_JOB;
    else if (!strcmp(argv[i], "--ref"))
    {
      i ++;
      if (i >= argc)
        return (usage());

      reg_standard = argv[i];
    }
    else if (!strcmp(argv[i], "--printer") && group == IPP_TAG_ZERO)
      group = IPP_TAG_PRINTER;
    else if (argv[i][0] == '-' || reg_file)
      return (usage());
    else
      reg_file = argv[i];
  }

  if (group == IPP_TAG_ZERO)
    return (usage());

 /*
  * Read registrations...
  */

  if (!reg_file)
    reg_file = "http://www.iana.org/assignments/ipp-registrations/"
	       "ipp-registrations.xml";

  if ((reg_xml = load_xml(reg_file)) == NULL)
    return (1);

 /*
  * Scan registrations for attributes...
  */

  if ((reg_2 = mxmlFindElement(reg_xml, reg_xml, "registry", "id",
                               "ipp-registrations-2",
                               MXML_DESCEND)) == NULL)
  {
    fprintf(stderr, "xmltotest: No IPP attribute registrations in \"%s\".\n",
            reg_file);
    return (1);
  }

  attrs = cupsArrayNew((cups_array_func_t)compare_reg, NULL);

  for (reg_record = mxmlFindElement(reg_2, reg_2, "record", NULL, NULL,
				    MXML_DESCEND);
       reg_record;
       reg_record = mxmlFindElement(reg_record, reg_2, "record", NULL, NULL,
                                    MXML_NO_DESCEND))
  {
   /*
    * Get the values from the current record...
    */

    reg_collection = mxmlFindElement(reg_record, reg_record, "collection",
                                     NULL, NULL, MXML_DESCEND);
    reg_name       = mxmlFindElement(reg_record, reg_record, "name", NULL, NULL,
                                     MXML_DESCEND);
    reg_member     = mxmlFindElement(reg_record, reg_record, "member_attribute",
                                     NULL, NULL, MXML_DESCEND);
    reg_sub_member = mxmlFindElement(reg_record, reg_record,
                                     "sub-member_attribute", NULL, NULL,
                                     MXML_DESCEND);
    reg_syntax     = mxmlFindElement(reg_record, reg_record, "syntax", NULL,
                                     NULL, MXML_DESCEND);
    reg_xref       = mxmlFindElement(reg_record, reg_record, "xref", NULL, NULL,
                                     MXML_DESCEND);

    if (!reg_collection || !reg_name || !reg_syntax || !reg_xref)
      continue;

   /*
    * Filter based on group and standard...
    */

    if (!strcmp(reg_collection->child->value.opaque, "Printer Description"))
      reg_group = IPP_TAG_PRINTER;
    else if (!strcmp(reg_collection->child->value.opaque, "Job Description"))
      reg_group = IPP_TAG_JOB;
    else if (!strcmp(reg_collection->child->value.opaque, "Job Template"))
    {
      if (strstr(reg_name->child->value.opaque, "-default") ||
          strstr(reg_name->child->value.opaque, "-supported"))
	reg_group = IPP_TAG_PRINTER;
      else
	reg_group = IPP_TAG_JOB;
    }
    else
      reg_group = IPP_TAG_ZERO;

    if (reg_group != group)
      continue;

    if (reg_standard && !match_xref(reg_xref, reg_standard))
      continue;

   /*
    * Add the record to the array...
    */

    if ((current = new_reg(reg_name, reg_member, reg_sub_member,
                           reg_syntax)) != NULL)
      cupsArrayAdd(attrs, current);
  }

 /*
  * Write out a test for all of the selected attributes...
  */

  puts("{");

  if (group == IPP_TAG_PRINTER)
  {
    puts("\tOPERATION Get-Printer-Attributes");
    puts("\tGROUP operation-attributes-tag");
    puts("\tATTR charset attributes-charset utf-8");
    puts("\tATTR naturalLanguage attributes-natural-language en");
    puts("\tATTR uri printer-uri $uri");
    puts("\tATTR name requesting-user-name $user");
    puts("\tATTR keyword requested-attributes all,media-col-database");
    puts("");
    puts("\tSTATUS successful-ok");
    puts("\tSTATUS successful-ok-ignored-or-substituted-attributes");
    puts("");
  }
  else
  {
    puts("\tOPERATION Get-Job-Attributes");
    puts("\tGROUP operation-attributes-tag");
    puts("\tATTR charset attributes-charset utf-8");
    puts("\tATTR naturalLanguage attributes-natural-language en");
    puts("\tATTR uri printer-uri $uri");
    puts("\tATTR integer job-id $job-id");
    puts("\tATTR name requesting-user-name $user");
    puts("");
    puts("\tSTATUS successful-ok");
    puts("");
  }

  for (current = cupsArrayFirst(attrs);
       current;
       current = cupsArrayNext(attrs))
    write_expect(current, group);

  puts("}");

  return (0);
}


/*
 * 'compare_reg()' - Compare two registrations.
 */

static int				/* O - Result of comparison */
compare_reg(_cups_reg_t *a,		/* I - First registration */
            _cups_reg_t *b)		/* I - Second registration */
{
  int	retval;				/* Return value */


  if ((retval = strcmp(a->name, b->name)) != 0)
    return (retval);

  if (a->member && b->member)
    retval = strcmp(a->member, b->member);
  else if (a->member)
    retval = 1;
  else if (b->member)
    retval = -1;

  if (retval)
    return (retval);

  if (a->sub_member && b->sub_member)
    retval = strcmp(a->sub_member, b->sub_member);
  else if (a->sub_member)
    retval = 1;
  else if (b->sub_member)
    retval = -1;

  return (retval);
}


/*
 * 'load_xml()' - Load the XML registration file or URL.
 */

static mxml_node_t *			/* O - XML file or NULL */
load_xml(const char *reg_file)		/* I - Filename or URL */
{
  mxml_node_t		*xml;		/* XML file */
  char			scheme[256],	/* Scheme */
			userpass[256],	/* Username and password */
			hostname[256],	/* Hostname */
			resource[1024],	/* Resource path */
			filename[1024];	/* Temporary file */
  int			port,		/* Port number */
			fd;		/* File descriptor */


  if (httpSeparateURI(HTTP_URI_CODING_ALL, reg_file, scheme, sizeof(scheme),
                      userpass, sizeof(userpass), hostname, sizeof(hostname),
                      &port, resource, sizeof(resource)) < HTTP_URI_OK)
  {
    fprintf(stderr, "xmltotest: Bad URI or filename \"%s\".\n", reg_file);
    return (NULL);
  }

  if (!strcmp(scheme, "file"))
  {
   /*
    * Local file...
    */

    if ((fd = open(resource, O_RDONLY)) < 0)
    {
      fprintf(stderr, "xmltotest: Unable to open \"%s\": %s\n", resource,
              strerror(errno));
      return (NULL);
    }

    filename[0] = '\0';
  }
  else if (strcmp(scheme, "http") && strcmp(scheme, "https"))
  {
    fprintf(stderr, "xmltotest: Unsupported URI scheme \"%s\".\n", scheme);
    return (NULL);
  }
  else
  {
    http_t		*http;		/* HTTP connection */
    http_encryption_t	encryption;	/* Encryption to use */
    http_status_t	status;		/* Status of HTTP GET */

    if (!strcmp(scheme, "https") || port == 443)
      encryption = HTTP_ENCRYPT_ALWAYS;
    else
      encryption = HTTP_ENCRYPT_IF_REQUESTED;

    if ((http = httpConnectEncrypt(hostname, port, encryption)) == NULL)
    {
      fprintf(stderr, "xmltotest: Unable to connect to \"%s\": %s\n", hostname,
              cupsLastErrorString());
      return (NULL);
    }

    if ((fd = cupsTempFd(filename, sizeof(filename))) < 0)
    {
      fprintf(stderr, "xmltotest: Unable to create temporary file: %s\n",
              strerror(errno));
      httpClose(http);
      return (NULL);
    }

    status = cupsGetFd(http, resource, fd);
    httpClose(http);

    if (status != HTTP_OK)
    {
      fprintf(stderr, "mxmltotest: Unable to get \"%s\": %d\n", reg_file,
              status);
      close(fd);
      unlink(filename);
      return (NULL);
    }

    lseek(fd, 0, SEEK_SET);
  }

 /*
  * Load the XML file...
  */

  xml = mxmlLoadFd(NULL, fd, MXML_OPAQUE_CALLBACK);

  close(fd);

  if (filename[0])
    unlink(filename);

  return (xml);
}


/*
 * 'match_xref()' - Compare the xref against the named standard.
 */

static int				/* O - 1 if match, 0 if not */
match_xref(mxml_node_t *xref,		/* I - <xref> node */
           const char  *standard)	/* I - Name of standard */
{
  const char	*data;			/* "data" attribute */
  char		s[256];			/* String to look for */


  if ((data = mxmlElementGetAttr(xref, "data")) == NULL)
    return (1);

  if (!strcmp(data, standard))
    return (1);

  if (!strncmp(standard, "pwg", 3))
  {
    snprintf(s, sizeof(s), "-%s.pdf", standard + 3);
    return (strstr(data, s) != NULL);
  }
  else
    return (0);
}


/*
 * 'new_reg()' - Create a new registration record.
 */

static _cups_reg_t *			/* O - New record */
new_reg(mxml_node_t *name,		/* I - Attribute name */
        mxml_node_t *member,		/* I - Member attribute, if any */
        mxml_node_t *sub_member,	/* I - Sub-member attribute, if any */
        mxml_node_t *syntax)		/* I - Syntax */
{
  _cups_reg_t	*reg;			/* New record */


  if ((reg = calloc(1, sizeof(_cups_reg_t))) != NULL)
  {
    reg->name   = name->child->value.opaque;
    reg->syntax = syntax->child->value.opaque;

    if (member)
      reg->member = member->child->value.opaque;

    if (sub_member)
      reg->sub_member = sub_member->child->value.opaque;
  }

  return (reg);
}


/*
 * 'usage()' - Show usage message.
 */

static int				/* O - Exit status */
usage(void)
{
  puts("Usage ./xmltotest [--ref standard] {--job|--printer} [XML file/URL] "
       ">file.test");
  return (1);
}


/*
 * 'write_expect()' - Write an EXPECT test for an attribute.
 */

static void
write_expect(_cups_reg_t *reg,		/* I - Registration information */
             ipp_tag_t   group)		/* I - Attribute group tag */
{
  const char	*syntax;		/* Pointer into syntax string */
  int		single = 1,		/* Single valued? */
		skip = 0;		/* Skip characters? */


  printf("\tEXPECT ?%s OF-TYPE ", reg->name);

  syntax = reg->syntax;

  while (*syntax)
  {
    if (!strncmp(syntax, "1setOf", 6))
    {
      single = 0;
      syntax += 6;

      while (isspace(*syntax & 255))
        syntax ++;

      if (*syntax == '(')
        syntax ++;
    }
    else if (!strncmp(syntax, "type1", 5) || !strncmp(syntax, "type2", 5) ||
             !strncmp(syntax, "type3", 5))
      syntax += 5;
    else if (*syntax == '(')
    {
      skip = 1;
      syntax ++;
    }
    else if (*syntax == ')')
    {
      skip = 0;
      syntax ++;
    }
    else if (!skip && (*syntax == '|' || isalpha(*syntax & 255)))
      putchar(*syntax++);
    else
      syntax ++;
  }

  if (single)
    printf(" IN-GROUP %s COUNT 1\n", ippTagString(group));
  else
    printf(" IN-GROUP %s\n", ippTagString(group));
}


#else /* !HAVE_MXML */
int
main(void)
{
  return (1);
}
#endif /* HAVE_MXML */


/*
 * End of "$Id: xmltotest.c 3643 2012-02-13 16:35:48Z msweet $".
 */
