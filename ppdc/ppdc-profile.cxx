//
// Color profile class for the CUPS PPD Compiler.
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
// 'ppdcProfile::ppdcProfile()' - Create a color profile.
//

ppdcProfile::ppdcProfile(const char  *r,	// I - Resolution name
                         const char  *m,	// I - Media type name
			 float       d,		// I - Density
			 float       g,		// I - Gamma
			 const float *p)	// I - 3x3 transform matrix
  : ppdcShared()
{
  PPDC_NEW;

  resolution = new ppdcString(r);
  media_type = new ppdcString(m);
  density    = d;
  gamma      = g;

  memcpy(profile, p, sizeof(profile));
}


//
// 'ppdcProfile::~ppdcProfile()' - Destroy a color profile.
//

ppdcProfile::~ppdcProfile()
{
  PPDC_DELETE;

  resolution->release();
  media_type->release();
}
