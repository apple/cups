//
// Private definitions for the CUPS PPD Compiler.
//
// Copyright 2009-2010 by Apple Inc.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more information.
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

#  ifdef PPDC_DEBUG
#    define PPDC_NEW		DEBUG_printf(("%s: %p new", class_name(), this))
#    define PPDC_NEWVAL(s)	DEBUG_printf(("%s(\"%s\"): %p new", class_name(), s, this))
#    define PPDC_DELETE		DEBUG_printf(("%s: %p delete", class_name(), this))
#    define PPDC_DELETEVAL(s)	DEBUG_printf(("%s(\"%s\"): %p delete", class_name(), s, this))
#  else
#    define PPDC_NEW
#    define PPDC_NEWVAL(s)
#    define PPDC_DELETE
#    define PPDC_DELETEVAL(s)
#  endif /* PPDC_DEBUG */

#endif // !_PPDC_PRIVATE_H_
