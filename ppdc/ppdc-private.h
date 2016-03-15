//
// "$Id: ppdc-private.h 1992 2010-03-24 14:32:08Z msweet $"
//
//   Private definitions for the CUPS PPD Compiler.
//
//   Copyright 2009-2010 by Apple Inc.
//
//   These coded instructions, statements, and computer programs are the
//   property of Apple Inc. and are protected by Federal copyright
//   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
//   which should have been included with this file.  If this file is
//   file is missing or damaged, see the license at "http://www.cups.org/".
//

#ifndef _PPDC_PRIVATE_H_
#  define _PPDC_PRIVATE_H_

//
// Include necessary headers...
//

#  include "ppdc.h"
#  include <cups/cups-private.h>


//
// Macros...
//

#  define PPDC_NEW		DEBUG_printf(("%s: %p new", class_name(), this))
#  define PPDC_NEWVAL(s)	DEBUG_printf(("%s(\"%s\"): %p new", class_name(), s, this))
#  define PPDC_DELETE		DEBUG_printf(("%s: %p delete", class_name(), this))
#  define PPDC_DELETEVAL(s)	DEBUG_printf(("%s(\"%s\"): %p delete", class_name(), s, this))


#endif // !_PPDC_PRIVATE_H_

//
// End of "$Id: ppdc-private.h 1992 2010-03-24 14:32:08Z msweet $".
//
