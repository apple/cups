//
// "$Id$"
//
//   Filter class for the CUPS PPD Compiler.
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
//   ppdcFilter::ppdcFilter()  - Create a filter.
//   ppdcFilter::~ppdcFilter() - Destroy a filter.
//

//
// Include necessary headers...
//

#include "ppdc.h"


//
// 'ppdcFilter::ppdcFilter()' - Create a filter.
//

ppdcFilter::ppdcFilter(const char *t,	// I - MIME type
		       const char *p,	// I - Filter program
		       int        c)	// I - Relative cost
{
  mime_type = new ppdcString(t);
  program   = new ppdcString(p);
  cost      = c;
}


//
// 'ppdcFilter::~ppdcFilter()' - Destroy a filter.
//

ppdcFilter::~ppdcFilter()
{
  mime_type->release();
  program->release();
}


//
// End of "$Id$".
//
