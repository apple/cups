//
// Variable class for the CUPS PPD Compiler.
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
// 'ppdcVariable::ppdcVariable()' - Create a variable.
//

ppdcVariable::ppdcVariable(const char *n,	// I - Name of variable
                           const char *v)	// I - Value of variable
  : ppdcShared()
{
  PPDC_NEW;

  name  = new ppdcString(n);
  value = new ppdcString(v);
}


//
// 'ppdcVariable::~ppdcVariable()' - Destroy a variable.
//

ppdcVariable::~ppdcVariable()
{
  PPDC_DELETE;

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
