//
// "$Id: ppdc-profile.cxx 1378 2009-04-08 03:17:45Z msweet $"
//
//   Color profile class for the CUPS PPD Compiler.
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
//   ppdcProfile::ppdcProfile()  - Create a color profile.
//   ppdcProfile::~ppdcProfile() - Destroy a color profile.
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


//
// End of "$Id: ppdc-profile.cxx 1378 2009-04-08 03:17:45Z msweet $".
//
