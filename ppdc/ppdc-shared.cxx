//
// Shared data class for the CUPS PPD Compiler.
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
}
