//
// "$Id: ppdc-message.cxx 1378 2009-04-08 03:17:45Z msweet $"
//
//   Shared message class for the CUPS PPD Compiler.
//
//   Copyright 2007-2009 by Apple Inc.
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
//   ppdcMessage::ppdcMessage()  - Create a shared message.
//   ppdcMessage::~ppdcMessage() - Destroy a shared message.
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


//
// End of "$Id: ppdc-message.cxx 1378 2009-04-08 03:17:45Z msweet $".
//
