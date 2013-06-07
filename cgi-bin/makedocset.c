/*
 * "$Id: makedocset.c 3833 2012-05-23 22:51:18Z msweet $"
 *
 *   Xcode documentation set generator.
 *
 *   Copyright 2007-2012 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Usage:
 *
 *   makedocset directory *.tokens
 *
 * Contents:
 *
 *   main()                   - Test the help index code.
 *   compare_html()           - Compare the titles of two HTML files.
 *   compare_sections()       - Compare the names of two help sections.
 *   compare_sections_files() - Compare the number of files and section names.
 *   write_index()            - Write an index file for the CUPS help.
 *   write_info()             - Write the Info.plist file.
 *   write_nodes()            - Write the Nodes.xml file.
 */

/*
 * Include necessary headers...
 */

#include "cgi-private.h"
#include <errno.h>


/*
 * Local structures...
 */

typedef struct _cups_html_s		/**** Help file ****/
{
  char		*path;			/* Path to help file */
  char		*title;			/* Title of help file */
} _cups_html_t;

typedef struct _cups_section_s		/**** Help section ****/
{
  char		*name;			/* Section name */
  cups_array_t	*files;			/* Files in this section */
} _cups_section_t;


/*
 * Local functions...
 */

static int	compare_html(_cups_html_t *a, _cups_html_t *b);
static int	compare_sections(_cups_section_t *a, _cups_section_t *b);
static int	compare_sections_files(_cups_section_t *a, _cups_section_t *b);
static void	write_index(const char *path, help_index_t *hi);
static void	write_info(const char *path, const char *revision);
static void	write_nodes(const char *path, help_index_t *hi);


/*
 * 'main()' - Test the help index code.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  int		i;			/* Looping var */
  char		path[1024],		/* Path to documentation */
		line[1024];		/* Line from file */
  help_index_t	*hi;			/* Help index */
  cups_file_t	*tokens,		/* Tokens.xml file */
		*fp;			/* Current file */


  if (argc < 4)
  {
    puts("Usage: makedocset directory revision *.tokens");
    return (1);
  }

 /*
  * Index the help documents...
  */

  snprintf(path, sizeof(path), "%s/Contents/Resources/Documentation", argv[1]);
  if ((hi = helpLoadIndex(NULL, path)) == NULL)
  {
    fputs("makedocset: Unable to index help files!\n", stderr);
    return (1);
  }

  snprintf(path, sizeof(path), "%s/Contents/Resources/Documentation/index.html",
           argv[1]);
  write_index(path, hi);

  snprintf(path, sizeof(path), "%s/Contents/Resources/Nodes.xml", argv[1]);
  write_nodes(path, hi);

 /*
  * Write the Info.plist file...
  */

  snprintf(path, sizeof(path), "%s/Contents/Info.plist", argv[1]);
  write_info(path, argv[2]);

 /*
  * Merge the Tokens.xml files...
  */

  snprintf(path, sizeof(path), "%s/Contents/Resources/Tokens.xml", argv[1]);
  if ((tokens = cupsFileOpen(path, "w")) == NULL)
  {
    fprintf(stderr, "makedocset: Unable to create \"%s\": %s\n", path,
	    strerror(errno));
    return (1);
  }

  cupsFilePuts(tokens, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
  cupsFilePuts(tokens, "<Tokens version=\"1.0\">\n");

  for (i = 3; i < argc; i ++)
  {
    if ((fp = cupsFileOpen(argv[i], "r")) == NULL)
    {
      fprintf(stderr, "makedocset: Unable to open \"%s\": %s\n", argv[i],
	      strerror(errno));
      return (1);
    }

    if (!cupsFileGets(fp, line, sizeof(line)) || strncmp(line, "<?xml ", 6) ||
        !cupsFileGets(fp, line, sizeof(line)) || strncmp(line, "<Tokens ", 8))
    {
      fprintf(stderr, "makedocset: Bad Tokens.xml file \"%s\"!\n", argv[i]);
      return (1);
    }

    while (cupsFileGets(fp, line, sizeof(line)))
    {
      if (strcmp(line, "</Tokens>"))
        cupsFilePrintf(tokens, "%s\n", line);
    }

    cupsFileClose(fp);
  }

  cupsFilePuts(tokens, "</Tokens>\n");

  cupsFileClose(tokens);

 /*
  * Return with no errors...
  */

  return (0);
}


/*
 * 'compare_html()' - Compare the titles of two HTML files.
 */

static int				/* O - Result of comparison */
compare_html(_cups_html_t *a,		/* I - First file */
             _cups_html_t *b)		/* I - Second file */
{
  return (_cups_strcasecmp(a->title, b->title));
}


/*
 * 'compare_sections()' - Compare the names of two help sections.
 */

static int				/* O - Result of comparison */
compare_sections(_cups_section_t *a,	/* I - First section */
                 _cups_section_t *b)	/* I - Second section */
{
  return (_cups_strcasecmp(a->name, b->name));
}


/*
 * 'compare_sections_files()' - Compare the number of files and section names.
 */

static int				/* O - Result of comparison */
compare_sections_files(
    _cups_section_t *a,			/* I - First section */
    _cups_section_t *b)			/* I - Second section */
{
  int	ret = cupsArrayCount(b->files) - cupsArrayCount(a->files);

  if (ret)
    return (ret);
  else
    return (_cups_strcasecmp(a->name, b->name));
}


/*
 * 'write_index()' - Write an index file for the CUPS help.
 */

static void
write_index(const char   *path,		/* I - File to write */
            help_index_t *hi)		/* I - Index of files */
{
  cups_file_t		*fp;		/* Output file */
  help_node_t		*node;		/* Current help node */
  _cups_section_t	*section,	/* Current section */
			key;		/* Section search key */
  _cups_html_t		*html;		/* Current HTML file */
  cups_array_t		*sections,	/* Sections in index */
			*sections_files,/* Sections sorted by size */
			*columns[3];	/* Columns in final HTML file */
  int			column,		/* Current column */
			lines[3],	/* Number of lines in each column */
			min_column,	/* Smallest column */
			min_lines;	/* Smallest number of lines */


 /*
  * Build an array of sections and their files.
  */

  sections = cupsArrayNew((cups_array_func_t)compare_sections, NULL);

  for (node = (help_node_t *)cupsArrayFirst(hi->nodes);
       node;
       node = (help_node_t *)cupsArrayNext(hi->nodes))
  {
    if (node->anchor)
      continue;

    key.name = node->section ? node->section : "Miscellaneous";
    if ((section = (_cups_section_t *)cupsArrayFind(sections, &key)) == NULL)
    {
      section        = (_cups_section_t *)calloc(1, sizeof(_cups_section_t));
      section->name  = key.name;
      section->files = cupsArrayNew((cups_array_func_t)compare_html, NULL);

      cupsArrayAdd(sections, section);
    }

    html = (_cups_html_t *)calloc(1, sizeof(_cups_html_t));
    html->path  = node->filename;
    html->title = node->text;

    cupsArrayAdd(section->files, html);
  }

 /*
  * Build a sorted list of sections based on the number of files in each section
  * and the section name...
  */

  sections_files = cupsArrayNew((cups_array_func_t)compare_sections_files,
                                NULL);
  for (section = (_cups_section_t *)cupsArrayFirst(sections);
       section;
       section = (_cups_section_t *)cupsArrayNext(sections))
    cupsArrayAdd(sections_files, section);

 /*
  * Then build three columns to hold everything, trying to balance the number of
  * lines in each column...
  */

  for (column = 0; column < 3; column ++)
  {
    columns[column] = cupsArrayNew((cups_array_func_t)compare_sections, NULL);
    lines[column]   = 0;
  }

  for (section = (_cups_section_t *)cupsArrayFirst(sections_files);
       section;
       section = (_cups_section_t *)cupsArrayNext(sections_files))
  {
    for (min_column = 0, min_lines = lines[0], column = 1;
         column < 3;
	 column ++)
    {
      if (lines[column] < min_lines)
      {
        min_column = column;
        min_lines  = lines[column];
      }
    }

    cupsArrayAdd(columns[min_column], section);
    lines[min_column] += cupsArrayCount(section->files) + 2;
  }

 /*
  * Write the HTML file...
  */

  if ((fp = cupsFileOpen(path, "w")) == NULL)
  {
    fprintf(stderr, "makedocset: Unable to create %s: %s\n", path,
            strerror(errno));
    exit(1);
  }

  cupsFilePuts(fp, "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 "
                   "Transitional//EN\" "
		   "\"http://www.w3.org/TR/html4/loose.dtd\">\n"
		   "<html>\n"
		   "<head>\n"
		   "<title>CUPS Documentation</title>\n"
		   "<link rel='stylesheet' type='text/css' "
		   "href='cups-printable.css'>\n"
		   "</head>\n"
		   "<body>\n"
		   "<h1 class='title'>CUPS Documentation</h1>\n"
		   "<table width='100%' summary=''>\n"
		   "<tr>\n");

  for (column = 0; column < 3; column ++)
  {
    if (column)
      cupsFilePuts(fp, "<td>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</td>\n");

    cupsFilePuts(fp, "<td valign='top' width='33%'>");
    for (section = (_cups_section_t *)cupsArrayFirst(columns[column]);
         section;
	 section = (_cups_section_t *)cupsArrayNext(columns[column]))
    {
      cupsFilePrintf(fp, "<h2 class='title'>%s</h2>\n", section->name);
      for (html = (_cups_html_t *)cupsArrayFirst(section->files);
           html;
	   html = (_cups_html_t *)cupsArrayNext(section->files))
	cupsFilePrintf(fp, "<p class='compact'><a href='%s'>%s</a></p>\n",
	               html->path, html->title);
    }
    cupsFilePuts(fp, "</td>\n");
  }
  cupsFilePuts(fp, "</tr>\n"
                   "</table>\n"
		   "</body>\n"
		   "</html>\n");
  cupsFileClose(fp);
}


/*
 * 'write_info()' - Write the Info.plist file.
 */

static void
write_info(const char *path,		/* I - File to write */
           const char *revision)	/* I - Subversion revision number */
{
  cups_file_t	*fp;			/* File */


  if ((fp = cupsFileOpen(path, "w")) == NULL)
  {
    fprintf(stderr, "makedocset: Unable to create %s: %s\n", path,
            strerror(errno));
    exit(1);
  }

  cupsFilePrintf(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		     "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
		     "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
		     "<plist version=\"1.0\">\n"
		     "<dict>\n"
		     "\t<key>CFBundleIdentifier</key>\n"
		     "\t<string>org.cups.docset</string>\n"
		     "\t<key>CFBundleName</key>\n"
		     "\t<string>CUPS Documentation</string>\n"
		     "\t<key>CFBundleVersion</key>\n"
		     "\t<string>%d.%d.%s</string>\n"
		     "\t<key>CFBundleShortVersionString</key>\n"
		     "\t<string>%d.%d.%d</string>\n"
		     "\t<key>DocSetFeedName</key>\n"
		     "\t<string>cups.org</string>\n"
		     "\t<key>DocSetFeedURL</key>\n"
		     "\t<string>http://www.cups.org/org.cups.docset.atom"
		     "</string>\n"
		     "\t<key>DocSetPublisherIdentifier</key>\n"
		     "\t<string>org.cups</string>\n"
		     "\t<key>DocSetPublisherName</key>\n"
		     "\t<string>CUPS</string>\n"
		     "</dict>\n"
		     "</plist>\n",
		     CUPS_VERSION_MAJOR, CUPS_VERSION_MINOR, revision,
		     CUPS_VERSION_MAJOR, CUPS_VERSION_MINOR, CUPS_VERSION_PATCH);

  cupsFileClose(fp);
}


/*
 * 'write_nodes()' - Write the Nodes.xml file.
 */

static void
write_nodes(const char   *path,		/* I - File to write */
            help_index_t *hi)		/* I - Index of files */
{
  cups_file_t	*fp;			/* Output file */
  int		id;			/* Current node ID */
  help_node_t	*node;			/* Current help node */
  int		subnodes;		/* Currently in Subnodes for file? */
  int		needclose;		/* Need to close the current node? */


  if ((fp = cupsFileOpen(path, "w")) == NULL)
  {
    fprintf(stderr, "makedocset: Unable to create %s: %s\n", path,
            strerror(errno));
    exit(1);
  }

  cupsFilePuts(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		   "<DocSetNodes version=\"1.0\">\n"
		   "<TOC>\n"
		   "<Node id=\"0\">\n"
		   "<Name>CUPS Documentation</Name>\n"
		   "<Path>Documentation/index.html</Path>\n"
		   "</Node>\n");

  for (node = (help_node_t *)cupsArrayFirst(hi->nodes), id = 1, subnodes = 0,
           needclose = 0;
       node;
       node = (help_node_t *)cupsArrayNext(hi->nodes), id ++)
  {
    if (node->anchor)
    {
      if (!subnodes)
      {
        cupsFilePuts(fp, "<Subnodes>\n");
	subnodes = 1;
      }

      cupsFilePrintf(fp, "<Node id=\"%d\">\n"
                         "<Path>Documentation/%s</Path>\n"
			 "<Anchor>%s</Anchor>\n"
			 "<Name>%s</Name>\n"
			 "</Node>\n", id, node->filename, node->anchor,
		     node->text);
    }
    else
    {
      if (subnodes)
      {
        cupsFilePuts(fp, "</Subnodes>\n");
	subnodes = 0;
      }

      if (needclose)
        cupsFilePuts(fp, "</Node>\n");

      cupsFilePrintf(fp, "<Node id=\"%d\">\n"
                         "<Path>Documentation/%s</Path>\n"
			 "<Name>%s</Name>\n", id, node->filename, node->text);
      needclose = 1;
    }
  }

  if (subnodes)
    cupsFilePuts(fp, "</Subnodes>\n");

  if (needclose)
    cupsFilePuts(fp, "</Node>\n");

  cupsFilePuts(fp, "</TOC>\n"
		   "</DocSetNodes>\n");

  cupsFileClose(fp);
}


/*
 * End of "$Id: makedocset.c 3833 2012-05-23 22:51:18Z msweet $".
 */
