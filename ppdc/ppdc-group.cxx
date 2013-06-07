//
// "$Id: ppdc-group.cxx 3275 2011-05-20 07:26:13Z msweet $"
//
//   Group class for the CUPS PPD Compiler.
//
//   Copyright 2007-2011 by Apple Inc.
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
//   ppdcGroup::ppdcGroup()   - Copy a new group.
//   ppdcGroup::~ppdcGroup()  - Destroy a group.
//   ppdcGroup::find_option() - Find an option in a group.
//

//
// Include necessary headers...
//

#include "ppdc-private.h"


//
// 'ppdcGroup::ppdcGroup()' - Create a new group.
//

ppdcGroup::ppdcGroup(const char *n,	// I - Name of group
                     const char *t)	// I - Text of group
{
  PPDC_NEWVAL(n);

  name    = new ppdcString(n);
  text    = new ppdcString(t);
  options = new ppdcArray();
}


//
// 'ppdcGroup::ppdcGroup()' - Copy a new group.
//

ppdcGroup::ppdcGroup(ppdcGroup *g)	// I - Group template
{
  PPDC_NEWVAL(g->name->value);

  g->name->retain();
  g->text->retain();

  name = g->name;
  text = g->text;

  options = new ppdcArray();
  for (ppdcOption *o = (ppdcOption *)g->options->first();
       o;
       o = (ppdcOption *)g->options->next())
    options->add(new ppdcOption(o));
}


//
// 'ppdcGroup::~ppdcGroup()' - Destroy a group.
//

ppdcGroup::~ppdcGroup()
{
  PPDC_DELETEVAL(name ? name->value : NULL);

  name->release();
  text->release();
  options->release();

  name = text = 0;
  options = 0;
}


//
// 'ppdcGroup::find_option()' - Find an option in a group.
//

ppdcOption *
ppdcGroup::find_option(const char *n)	// I - Name of option
{
  ppdcOption	*o;			// Current option


  for (o = (ppdcOption *)options->first(); o; o = (ppdcOption *)options->next())
    if (!_cups_strcasecmp(n, o->name->value))
      return (o);

  return (0);
}


//
// End of "$Id: ppdc-group.cxx 3275 2011-05-20 07:26:13Z msweet $".
//
