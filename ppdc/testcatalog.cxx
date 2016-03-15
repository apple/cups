//
// "$Id: testcatalog.cxx 1378 2009-04-08 03:17:45Z msweet $"
//
//   Test program for message catalog class.
//
//   Copyright 2008 by Apple Inc.
//
//   These coded instructions, statements, and computer programs are the
//   property of Apple Inc. and are protected by Federal copyright
//   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
//   which should have been included with this file.  If this file is
//   file is missing or damaged, see the license at "http://www.cups.org/".
//
// Contents:
//
//   main() - Open a message catalog 
//

//
// Include necessary headers...
//

#include "ppdc-private.h"


//
// 'main()' - Open a message catalog 
//

int					// O - Exit status
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  ppdcCatalog	*catalog;		// Message catalog
  ppdcMessage	*m;			// Current message


  if (argc != 2)
  {
    puts("Usage: testcatalog filename");
    return (1);
  }

  // Scan the command-line...
  catalog = new ppdcCatalog(NULL, argv[1]);

  printf("%s: %d messages\n", argv[1], catalog->messages->count);

  for (m = (ppdcMessage *)catalog->messages->first();
       m;
       m = (ppdcMessage *)catalog->messages->next())
    printf("%s: %s\n", m->id->value, m->string->value);

  catalog->release();

  // Return with no errors.
  return (0);
}


//
// End of "$Id: testcatalog.cxx 1378 2009-04-08 03:17:45Z msweet $".
//
