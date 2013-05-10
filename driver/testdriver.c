/*
 * "$Id$"
 *
 *   Sample/test driver interface program for the Common UNIX Printing
 *   System (CUPS).
 *
 *   This program handles listing and installing both static PPD files
 *   in CUPS_DATADIR/model and dynamically generated PPD files using
 *   the driver helper programs in CUPS_SERVERBIN/driver.
 *
 *   Copyright 1997-2006 by Easy Software Products.
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
 *       Hollywood, Maryland 20636 USA
 *
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 *   main()      - Enumerate or display PPD files.
 *   cat_ppd()   - Display a PPD file.
 *   list_ppds() - List PPDs.
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <string.h>


/*
 * Local functions...
 */

static int	cat_ppd(const char *uri);
static int	list_ppds(const char *name);


/*
 * Sample data...
 */

static const char *models[][2] =
		{
		  { "foojet.ppd", "Foo Printer" },
		  { "barjet.ppd", "Bar Printer" },
		  { "foobar.ppd", "Foo/Bar Multifunction Printer" }
		};


/*
 * 'main()' - Enumerate or display PPD files.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  if (argc == 2 && !strcmp(argv[1], "list"))
    return (list_ppds(argv[0]));
  else if (argc == 3 && !strcmp(argv[1], "cat"))
    return (cat_ppd(argv[2]));

  fprintf(stderr, "ERROR: Usage: %s cat URI\n", argv[0]);
  fprintf(stderr, "ERROR: Usage: %s list\n", argv[0]);
  return (1);
}


/*
 * 'cat_ppd()' - Display a PPD file.
 */

static int				/* O - Exit status */
cat_ppd(const char *uri)		/* I - PPD URI */
{
  int		i;			/* Looping var */
  const char	*name;			/* Pointer to name in URI */


  if ((name = strchr(uri, ':')) == NULL)
  {
    fprintf(stderr, "ERROR: Bad URI \"%s\"!\n", uri);
    return (1);
  }

  name ++;

  for (i = 0 ; i < (int)(sizeof(models) / sizeof(models[0])); i ++)
    if (!strcmp(name, models[i][0]))
    {
     /*
      * Actually display the PPD file...
      */
      puts("*PPD-Adobe: \"4.3\"");

      puts("*LanguageEncoding: ISOLatin1");
      puts("*LanguageVersion: English");
      puts("*Manufacturer: \"Test\"");
      puts("*FileVersion: \"1.0\"");
      puts("*FormatVersion: \"4.3\"");
      puts("*PSVersion: \"(3010) 1\"");
      printf("*PCFileName: \"%s\"\n", models[i][0]);

      printf("*Product: \"(%s)\"\n", models[i][1]);
      printf("*ModelName: \"Test %s\"\n", models[i][1]);
      printf("*NickName: \"Test %s\"\n", models[i][1]);
      printf("*ShortNickName: \"Test %s\"\n", models[i][1]);

      puts("*OpenUI PageSize: PickOne"); 
      puts("*OrderDependency: 10 AnySetup *PageSetup");
      puts("*DefaultPageSize: Letter");
      puts("*PageSize Letter: \"<</PageSize[612 792]>>setpagedevice\"");
      puts("*PageSize A4: \"<</PageSize[585 842]>>setpagedevice\"");
      puts("*CloseUI: *PageSize");

      puts("*OpenUI PageRegion: PickOne"); 
      puts("*OrderDependency: 10 AnySetup *PageRegion");
      puts("*DefaultPageRegion: Letter");
      puts("*PageRegion Letter: \"<</PageRegion[612 792]>>setpagedevice\"");
      puts("*PageRegion A4: \"<</PageRegion[585 842]>>setpagedevice\"");
      puts("*CloseUI: *PageRegion");

      puts("*DefaultImageableArea: Letter");
      puts("*ImageableArea Letter: \"0 0 612 792\"");
      puts("*ImageableArea A4: \"0 0 595 842\"");

      puts("*DefaultPaperDimension: Letter");
      puts("*PaperDimension Letter: \"612 792\"");
      puts("*PaperDimension A4: \"595 842\"");

      return (0);
    }

  fprintf(stderr, "ERROR: Unknown URI \"%s\"!\n", uri);
  return (1);
}


/*
 * 'list_ppds()' - List PPDs.
 */

static int				/* O - Exit status */
list_ppds(const char *name)		/* I - Program name */
{
  int		i;			/* Looping var */
  const char	*base;			/* Base name of program */


  if ((base = strrchr(name, '/')) != NULL)
    base ++;
  else
    base = name;

  for (i = 0; i < (int)(sizeof(models) / sizeof(models[0])); i ++)
    printf("\"%s:%s\" en \"Test\" \"Test %s\" \"1284 device id\"\n",
           base, models[i][0], models[i][1]);

  return (0);
}


/*
 * End of "$Id$".
 */
