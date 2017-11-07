//
// Shared string class for the CUPS PPD Compiler.
//
// Copyright 2007-2012 by Apple Inc.
// Copyright 2002-2005 by Easy Software Products.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more information.
//

//
// Include necessary headers...
//

#include "ppdc-private.h"


//
// 'ppdcString::ppdcString()' - Create a shared string.
//

ppdcString::ppdcString(const char *v)	// I - String
  : ppdcShared()
{
  PPDC_NEWVAL(v);

  if (v)
  {
    size_t vlen = strlen(v);

    value = new char[vlen + 1];
    memcpy(value, v, vlen + 1);
  }
  else
    value = 0;
}


//
// 'ppdcString::~ppdcString()' - Destroy a shared string.
//

ppdcString::~ppdcString()
{
  PPDC_DELETEVAL(value);

  if (value)
    delete[] value;
}
