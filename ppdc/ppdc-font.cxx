//
// Shared font class for the CUPS PPD Compiler.
//
// Copyright 2007-2009 by Apple Inc.
// Copyright 2002-2005 by Easy Software Products.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more information.
//

//
// Include necessary headers...
//

#include "ppdc-private.h"


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
  PPDC_NEW;

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
  PPDC_DELETE;

  name->release();
  encoding->release();
  version->release();
  charset->release();
}
