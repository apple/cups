//
// Shared message class for the CUPS PPD Compiler.
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
// 'ppdcMessage::ppdcMessage()' - Create a shared message.
//

ppdcMessage::ppdcMessage(const char *i,	// I - ID
                         const char *s)	// I - Text
  : ppdcShared()
{
  PPDC_NEW;

  id     = new ppdcString(i);
  string = new ppdcString(s);
}


//
// 'ppdcMessage::~ppdcMessage()' - Destroy a shared message.
//

ppdcMessage::~ppdcMessage()
{
  PPDC_DELETE;

  id->release();
  string->release();
}
