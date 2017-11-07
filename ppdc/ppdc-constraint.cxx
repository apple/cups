//
// Contraint class for the CUPS PPD Compiler.
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
