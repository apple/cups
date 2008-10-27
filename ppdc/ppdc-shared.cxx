//
// "$Id$"
//
//   Shared data class for the CUPS PPD Compiler.
//
//   Copyright 2007-2008 by Apple Inc.
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
//   ppdcShared::ppdcShared()  - Create shared data.
//   ppdcShared::~ppdcShared() - Destroy shared data.
//   ppdcShared::release()     - Decrement the use count and delete as needed.
//   ppdcShared::retain()      - Increment the use count for this data.
//

//
// Include necessary headers...
//

#include "ppdc.h"


//
// 'ppdcShared::ppdcShared()' - Create shared data.
//

ppdcShared::ppdcShared()
{
  use = 1;
}


//
// 'ppdcShared::~ppdcShared()' - Destroy shared data.
//

ppdcShared::~ppdcShared()
{
}


//
// 'ppdcShared::release()' - Decrement the use count and delete as needed.
//

void
ppdcShared::release(void)
{
  use --;
  if (!use)
    delete this;
}


//
// 'ppdcShared::retain()' - Increment the use count for this data.
//

void
ppdcShared::retain()
{
  use ++;
}


//
// End of "$Id$".
//
