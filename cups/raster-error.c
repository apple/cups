/*
 * Raster error handling for CUPS.
 *
 * Copyright © 2007-2018 by Apple Inc.
 * Copyright © 2007 by Easy Software Products.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more
 * information.
 */

/*
 * Include necessary headers...
 */

#include "cups-private.h"
#include "raster-private.h"
#include "debug-internal.h"


/*
 * '_cupsRasterAddError()' - Add an error message to the error buffer.
 */

void
_cupsRasterAddError(const char *f,	/* I - Printf-style error message */
                    ...)		/* I - Additional arguments as needed */
{
  _cups_globals_t	*cg = _cupsGlobals();
					/* Thread globals */
  _cups_raster_error_t	*buf = &cg->raster_error;
					/* Error buffer */
  va_list	ap;			/* Pointer to additional arguments */
  char		s[2048];		/* Message string */
  ssize_t	bytes;			/* Bytes in message string */


  DEBUG_printf(("_cupsRasterAddError(f=\"%s\", ...)", f));

  va_start(ap, f);
  bytes = vsnprintf(s, sizeof(s), f, ap);
  va_end(ap);

  if (bytes <= 0)
    return;

  DEBUG_printf(("1_cupsRasterAddError: %s", s));

  bytes ++;

  if ((size_t)bytes >= sizeof(s))
    return;

  if (bytes > (ssize_t)(buf->end - buf->current))
  {
   /*
    * Allocate more memory...
    */

    char	*temp;			/* New buffer */
    size_t	size;			/* Size of buffer */


    size = (size_t)(buf->end - buf->start + 2 * bytes + 1024);

    if (buf->start)
      temp = realloc(buf->start, size);
    else
      temp = malloc(size);

    if (!temp)
      return;

   /*
    * Update pointers...
    */

    buf->end     = temp + size;
    buf->current = temp + (buf->current - buf->start);
    buf->start   = temp;
  }

 /*
  * Append the message to the end of the current string...
  */

  memcpy(buf->current, s, (size_t)bytes);
  buf->current += bytes - 1;
}


/*
 * '_cupsRasterClearError()' - Clear the error buffer.
 */

void
_cupsRasterClearError(void)
{
  _cups_globals_t	*cg = _cupsGlobals();
					/* Thread globals */
  _cups_raster_error_t	*buf = &cg->raster_error;
					/* Error buffer */


  buf->current = buf->start;

  if (buf->start)
    *(buf->start) = '\0';
}


/*
 * '_cupsRasterErrorString()' - Return the last error from a raster function.
 *
 * If there are no recent errors, NULL is returned.
 *
 * @since CUPS 1.3/macOS 10.5@
 */

const char *				/* O - Last error */
_cupsRasterErrorString(void)
{
  _cups_globals_t	*cg = _cupsGlobals();
					/* Thread globals */
  _cups_raster_error_t	*buf = &cg->raster_error;
					/* Error buffer */


  if (buf->current == buf->start)
    return (NULL);
  else
    return (buf->start);
}
