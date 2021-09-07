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
    value = strdup(v);
  }
  else
    value = NULL;
}


//
// 'ppdcString::~ppdcString()' - Destroy a shared string.
//

ppdcString::~ppdcString()
{
  PPDC_DELETEVAL(value);
  free(value);
}
