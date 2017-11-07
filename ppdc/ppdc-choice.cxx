//
// Option choice class for the CUPS PPD Compiler.
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
// 'ppdcChoice::ppdcChoice()' - Create a new option choice.
//

ppdcChoice::ppdcChoice(const char *n,	// I - Name of choice
                       const char *t,	// I - Text of choice
		       const char *c)	// I - Code of choice
  : ppdcShared()
{
  PPDC_NEW;

  name = new ppdcString(n);
  text = new ppdcString(t);
  code = new ppdcString(c);
}


//
// 'ppdcChoice::~ppdcChoice()' - Destroy an option choice.
//

ppdcChoice::~ppdcChoice()
{
  PPDC_DELETE;

  name->release();
  text->release();
  code->release();
}
