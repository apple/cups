/*
 * "$Id$"
 *
 *   Global variable access routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2005 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Easy Software Products and are protected by Federal
 *   copyright law.  Distribution and use rights are outlined in the file
 *   "LICENSE.txt" which should have been included with this file.  If this
 *   file is missing or damaged please contact Easy Software Products
 *   at:
 *
 *       Attn: CUPS Licensing Information
 *       Easy Software Products
 *       44141 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636 USA
 *
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   _cupsGlobals()	  - Return a pointer to thread local storage.
 *   globals_init()       - Initialize globals once.
 *   globals_destructor() - Free memory allocated by _cupsGlobals().
 */

/*
 * Include necessary headers...
 */

#include "globals.h"
#include "http-private.h"
#include <stdlib.h>


#ifdef HAVE_PTHREAD_H
/*
 * Implement per-thread globals...
 */

/*
 * Local globals...
 */

static pthread_key_t	globals_key = -1;
					/* Thread local storage key */
static pthread_once_t	globals_key_once = PTHREAD_ONCE_INIT;
					/* One-time initialization object */


/*
 * Local functions...
 */

static void	globals_init();
static void	globals_destructor(void *value);


/*
 * '_cupsGlobals()' - Return a pointer to thread local storage
 */

_cups_globals_t *			/* O - Pointer to global data */
_cupsGlobals(void)
{
  _cups_globals_t *globals;		/* Pointer to global data */


 /* 
  * Initialize the global data exactly once...
  */

  pthread_once(&globals_key_once, globals_init);

 /*
  * See if we have allocated the data yet...
  */

  if ((globals = (_cups_globals_t *)pthread_getspecific(globals_key)) == NULL)
  {
   /*
    * No, allocate memory as set the pointer for the key...
    */

    globals = calloc(1, sizeof(_cups_globals_t));
    pthread_setspecific(globals_key, globals);

   /*
    * Initialize variables that have non-zero values
    */

    globals->encryption  = (http_encryption_t)-1;
    globals->password_cb = _cupsGetPassword;
  }

 /*
  * Return the pointer to the data...
  */

  return (globals);
}


/*
 * 'globals_init()' - Initialize globals once.
 */

static void
globals_init()
{
  pthread_key_create(&globals_key, globals_destructor);
}


/*
 * 'globals_destructor()' - Free memory allocated by _cupsGlobals().
 */

static void
globals_destructor(void *value)		/* I - Data to free */
{
  free(value);
}


#else
/*
 * Implement static globals...
 */

/*
 * '_cupsGlobals()' - Return a pointer to thread local storage.
 */

_cups_globals_t *			/* O - Pointer to global data */
_cupsGlobals(void)
{
  static _cups_globals_t globals;	/* Global data */
  static int		initialized = 0;/* Global data initialized? */


 /*
  * Initialize global data as needed...
  */

  if (!initialized)
  {
    initialized = 1;

   /*
    * Initialize global variables...
    */

    memset(&globals, 0, sizeof(globals));

    globals.encryption  = (http_encryption_t)-1;
    globals.password_cb = _cupsGetPassword;
  }

  return (&globals);
}
#endif /* HAVE_PTHREAD_H */


/*
 * End of "$Id$".
 */
