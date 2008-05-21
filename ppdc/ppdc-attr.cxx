//
// "$Id$"
//
//   Attribute class for the CUPS PPD Compiler.
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
//   ppdcAttr::ppdcAttr()  - Create an attribute.
//   ppdcAttr::~ppdcAttr() - Destroy an attribute.
//

//
// Include necessary headers...
//

#include "ppdc.h"


//
// 'ppdcAttr::ppdcAttr()' - Create an attribute.
//

ppdcAttr::ppdcAttr(const char *n,	// I - Name
                   const char *s,	// I - Spec string
		   const char *t,	// I - Human-readable text
		   const char *v,	// I - Value
		   bool       loc)	// I - Localize this attribute?
  : ppdcShared()
{
  name        = new ppdcString(n);
  selector    = new ppdcString(s);
  text        = new ppdcString(t);
  value       = new ppdcString(v);
  localizable = loc;
}


//
// 'ppdcAttr::~ppdcAttr()' - Destroy an attribute.
//

ppdcAttr::~ppdcAttr()
{
  name->release();
  selector->release();
  text->release();
  value->release();
}


//
// End of "$Id$".
//
