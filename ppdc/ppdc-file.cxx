//
// File class for the CUPS PPD Compiler.
//
// Copyright 2007-2010 by Apple Inc.
// Copyright 2002-2005 by Easy Software Products.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more information.
//

//
// Include necessary headers...
//

#include "ppdc-private.h"


//
// 'ppdcFile::ppdcFile()' - Create (open) a file.
//

ppdcFile::ppdcFile(const char  *f,		// I - File to open
                   cups_file_t *ffp)		// I - File pointer to use
{
  if (ffp)
  {
    fp = ffp;
    cupsFileRewind(fp);
  }
  else
    fp = cupsFileOpen(f, "r");

  close_on_delete = !ffp;
  filename        = f;
  line            = 1;

  if (!fp)
    _cupsLangPrintf(stderr, _("ppdc: Unable to open %s: %s"), f,
                    strerror(errno));
}


//
// 'ppdcFile::~ppdcFile()' - Delete (close) a file.
//

ppdcFile::~ppdcFile()
{
  if (close_on_delete && fp)
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
