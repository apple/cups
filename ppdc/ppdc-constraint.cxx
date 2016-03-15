//
// "$Id: ppdc-constraint.cxx 1378 2009-04-08 03:17:45Z msweet $"
//
//   Contraint class for the CUPS PPD Compiler.
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
//   ppdcConstraint::ppdcConstraint()  - Create a constraint.
//   ppdcConstraint::~ppdcConstraint() - Destroy a constraint.
//

//
// Include necessary headers...
//

#include "ppdc-private.h"


//
// 'ppdcConstraint::ppdcConstraint()' - Create a constraint.
//

ppdcConstraint::ppdcConstraint(const char *o1,	// I - First option
                               const char *c1,	// I - First choice
			       const char *o2,	// I - Second option
			       const char *c2)	// I - Second choice
  : ppdcShared()
{
  PPDC_NEW;

  option1 = new ppdcString(o1);
  choice1 = new ppdcString(c1);
  option2 = new ppdcString(o2);
  choice2 = new ppdcString(c2);
}


//
// 'ppdcConstraint::~ppdcConstraint()' - Destroy a constraint.
//

ppdcConstraint::~ppdcConstraint()
{
  PPDC_DELETE;

  option1->release();
  choice1->release();
  option2->release();
  choice2->release();
}


//
// End of "$Id: ppdc-constraint.cxx 1378 2009-04-08 03:17:45Z msweet $".
//
