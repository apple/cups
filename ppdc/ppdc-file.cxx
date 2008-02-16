//
// "$Id$"
//
//   File class for the CUPS PPD Compiler.
//
//   Copyright 2007 by Apple Inc.
//   Copyright 2002-2005 by Easy Software Products.
//
//   These coded instructions, statements, and computer programs are the
//   property of Apple Inc. and are protected by Federal copyright
//   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
//   which should have been included with this file.  If this file is
//   file is missing or damaged, see the license at "http://www.cups.org/".
//
// Contents:
//
//   ppdcFile::ppdcFile()  - Create (open) a file.
//   ppdcFile::~ppdcFile() - Delete (close) a file.
//   ppdcFile::get()       - Get a character from a file.
//   ppdcFile::peek()      - Look at the next character from a file.
//

//
// Include necessary headers...
//

#include "ppdc.h"


//
// 'ppdcFile::ppdcFile()' - Create (open) a file.
//

ppdcFile::ppdcFile(const char *f)		// I - File to open
{
  fp       = cupsFileOpen(f, "r");
  filename = f;
  line     = 1;

  if (!fp)
    fprintf(stderr, "ppdc: Unable to open %s: %s\n", f, strerror(errno));
}


//
// 'ppdcFile::~ppdcFile()' - Delete (close) a file.
//

ppdcFile::~ppdcFile()
{
  if (fp)
    cupsFileClose(fp);
}


//
// 'ppdcFile::get()' - Get a character from a file.
//

int
ppdcFile::get()
{
  int	ch;					// Character from file


  // Return EOF if there is no open file...
  if (!fp)
    return (EOF);

  // Get the character...
  ch = cupsFileGetChar(fp);

  // Update the line number as needed...
  if (ch == '\n')
    line ++;

  // Return the character...
  return (ch);
}


//
// 'ppdcFile::peek()' - Look at the next character from a file.
//

int					// O - Next character in file
ppdcFile::peek()
{
  // Return immediaely if there is no open file...
  if (!fp)
    return (EOF);

  // Otherwise return the next character without advancing...
  return (cupsFilePeekChar(fp));
}


//
// End of "$Id$".
//
