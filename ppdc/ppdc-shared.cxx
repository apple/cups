//
// "$Id: ppdc-shared.cxx 1556 2009-06-10 19:02:58Z msweet $"
//
//   Shared data class for the CUPS PPD Compiler.
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
//   ppdcShared::ppdcShared()  - Create shared data.
//   ppdcShared::~ppdcShared() - Destroy shared data.
//   ppdcShared::release()     - Decrement the use count and delete as needed.
//   ppdcShared::retain()      - Increment the use count for this data.
//

//
// Include necessary headers...
//

#include "ppdc-private.h"


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
  DEBUG_printf(("%s: %p release use=%d", class_name(), this, use));

  use --;

#ifdef DEBUG
  if (use < 0)
  {
    fprintf(stderr, "ERROR: Over-release of %s: %p\n", class_name(), this);
    abort();
  }
#endif /* DEBUG */

  if (use == 0)
    delete this;
}


//
// 'ppdcShared::retain()' - Increment the use count for this data.
//

void
ppdcShared::retain()
{
  use ++;

  DEBUG_printf(("%s: %p retain use=%d", class_name(), this, use));
}


//
// End of "$Id: ppdc-shared.cxx 1556 2009-06-10 19:02:58Z msweet $".
//
