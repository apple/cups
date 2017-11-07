/*
 * Private CGI definitions for CUPS.
 *
 * Copyright 2007-2011 by Apple Inc.
 * Copyright 1997-2006 by Easy Software Products.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more information.
 */

/*
 * Include necessary headers...
 */

#include "cgi.h"
#include <cups/debug-private.h>
#include <cups/language-private.h>
#include <cups/string-private.h>
#include <cups/ipp-private.h>	/* TODO: Update so we don't need this */


/*
 * Limits...
 */

#define CUPS_PAGE_MAX	100		/* Maximum items per page */
