//
// "$Id$"
//
//   Variable class for the CUPS PPD Compiler.
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
//   ppdcVariable::ppdcVariable()  - Create a variable.
//   ppdcVariable::~ppdcVariable() - Destroy a variable.
//   ppdcVariable::set_value()     - Set the value of a variable.
//

//
// Include necessary headers...
//

#include "ppdc.h"


//
// 'ppdcVariable::ppdcVariable()' - Create a variable.
//

ppdcVariable::ppdcVariable(const char *n,	// I - Name of variable
                           const char *v)	// I - Value of variable
{
  name  = new ppdcString(n);
  value = new ppdcString(v);
}


//
// 'ppdcVariable::~ppdcVariable()' - Destroy a variable.
//

ppdcVariable::~ppdcVariable()
{
  name->release();
  value->release();
}


//
// 'ppdcVariable::set_value()' - Set the value of a variable.
//

void
ppdcVariable::set_value(const char *v)
{
  value->release();
  value = new ppdcString(v);
}


//
// End of "$Id$".
//
