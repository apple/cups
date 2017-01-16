//
// Option choice class for the CUPS PPD Compiler.
//
// Copyright 2007-2009 by Apple Inc.
// Copyright 2002-2005 by Easy Software Products.
//
// These coded instructions, statements, and computer programs are the
// property of Apple Inc. and are protected by Federal copyright
// law.  Distribution and use rights are outlined in the file "LICENSE.txt"
// which should have been included with this file.  If this file is
// missing or damaged, see the license at "http://www.cups.org/".
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
