//
// "$Id$"
//
//   Group class for the CUPS PPD Compiler.
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
//   ppdcGroup::ppdcGroup()   - Create a new group.
//   ppdcGroup::ppdcGroup()   - Copy a new group.
//   ppdcGroup::~ppdcGroup()  - Destroy a group.
//   ppdcGroup::find_option() - Find an option in a group.
//

//
// Include necessary headers...
//

#include "ppdc.h"


//
// 'ppdcGroup::ppdcGroup()' - Create a new group.
//

ppdcGroup::ppdcGroup(const char *n,	// I - Name of group
                     const char *t)	// I - Text of group
{
  name    = new ppdcString(n);
  text    = new ppdcString(t);
  options = new ppdcArray();
}


//
// 'ppdcGroup::ppdcGroup()' - Copy a new group.
//

ppdcGroup::ppdcGroup(ppdcGroup *g)	// I - Group template
{
  ppdcOption	*o;			// Current option


  g->name->get();
  g->text->get();

  name = g->name;
  text = g->text;

  options = new ppdcArray();
  for (o = (ppdcOption *)g->options->first(); o; o = (ppdcOption *)g->options->next())
    options->add(new ppdcOption(o));
}


//
// 'ppdcGroup::~ppdcGroup()' - Destroy a group.
//

ppdcGroup::~ppdcGroup()
{
  name->release();
  text->release();
  delete options;
}


//
// 'ppdcGroup::find_option()' - Find an option in a group.
//

ppdcOption *
ppdcGroup::find_option(const char *n)	// I - Name of option
{
  ppdcOption	*o;			// Current option


  for (o = (ppdcOption *)options->first(); o; o = (ppdcOption *)options->next())
    if (!strcasecmp(n, o->name->value))
      return (o);

  return (0);
}


//
// End of "$Id$".
//
