//
// "$Id$"
//
//   Shared font class for the CUPS PPD Compiler.
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
//   ppdcFont::ppdcFont()  - Create a shared font.
//   ppdcFont::~ppdcFont() - Destroy a shared font.
//

//
// Include necessary headers...
//

#include "ppdc.h"


//
// 'ppdcFont::ppdcFont()' - Create a shared font.
//

ppdcFont::ppdcFont(const char     *n,		// I - Name of font
                   const char     *e,		// I - Font encoding
		   const char     *v,		// I - Font version
		   const char     *c,		// I - Font charset
        	   ppdcFontStatus s)		// I - Font status
  : ppdcShared()
{
  name     = new ppdcString(n);
  encoding = new ppdcString(e);
  version  = new ppdcString(v);
  charset  = new ppdcString(c);
  status   = s;
}


//
// 'ppdcFont::~ppdcFont()' - Destroy a shared font.
//

ppdcFont::~ppdcFont()
{
  name->release();
  encoding->release();
  version->release();
  charset->release();
}


//
// End of "$Id$".
//
