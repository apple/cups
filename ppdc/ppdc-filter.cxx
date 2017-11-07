//
// Filter class for the CUPS PPD Compiler.
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
// 'ppdcFilter::ppdcFilter()' - Create a filter.
//

ppdcFilter::ppdcFilter(const char *t,	// I - MIME type
		       const char *p,	// I - Filter program
		       int        c)	// I - Relative cost
  : ppdcShared()
{
  PPDC_NEW;

  mime_type = new ppdcString(t);
  program   = new ppdcString(p);
  cost      = c;
}


//
// 'ppdcFilter::~ppdcFilter()' - Destroy a filter.
//

ppdcFilter::~ppdcFilter()
{
  PPDC_DELETE;

  mime_type->release();
  program->release();
}
