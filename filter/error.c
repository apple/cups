/*
 * "$Id: error.c 12748 2015-06-24 15:58:40Z msweet $"
 *
 * Raster error handling for CUPS.
 *
 * Copyright 2007-2015 by Apple Inc.
 * Copyright 2007 by Easy Software Products.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * This file is subject to the Apple OS-Developed Software exception.
 */

/*
 * Include necessary headers...
 */

#include <cups/raster-private.h>


/*
 * Local structures...
 */

typedef struct _cups_raster_error_s	/**** Error buffer structure ****/
{
  char	*start,				/* Start of buffer */
	*current,			/* Current position in buffer */
	*end;				/* End of buffer */
} _cups_raster_error_t;


/*
 * Local functions...
 */

static _cups_raster_error_t	*get_error_buffer(void);


/*
 * '_cupsRasterAddError()' - Add an error message to the error buffer.
 */

void
_cupsRasterAddError(const char *f,	/* I - Printf-style error message */
                    ...)		/* I - Additional arguments as needed */
{
  _cups_raster_error_t	*buf = get_error_buffer();
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
  _cups_raster_error_t	*buf = get_error_buffer();
					/* Error buffer */


  buf->current = buf->start;

  if (buf->start)
    *(buf->start) = '\0';
}


/*
 * 'cupsRasterErrorString()' - Return the last error from a raster function.
 *
 * If there are no recent errors, NULL is returned.
 *
 * @since CUPS 1.3/OS X 10.5@
 */

const char *				/* O - Last error */
cupsRasterErrorString(void)
{
  _cups_raster_error_t	*buf = get_error_buffer();
					/* Error buffer */


  if (buf->current == buf->start)
    return (NULL);
  else
    return (buf->start);
}


#ifdef HAVE_PTHREAD_H
/*
 * Implement per-thread globals...
 */

#  include <pthread.h>


/*
 * Local globals...
 */

static pthread_key_t	raster_key = 0;	/* Thread local storage key */
static pthread_once_t	raster_key_once = PTHREAD_ONCE_INIT;
					/* One-time initialization object */


/*
 * Local functions...
 */

static void	raster_init(void);
static void	raster_destructor(void *value);


/*
 * 'get_error_buffer()' - Return a pointer to thread local storage.
 */

_cups_raster_error_t *			/* O - Pointer to error buffer */
get_error_buffer(void)
{
  _cups_raster_error_t *buf;		/* Pointer to error buffer */


 /*
  * Initialize the global data exactly once...
  */

  DEBUG_puts("3get_error_buffer()");

  pthread_once(&raster_key_once, raster_init);

 /*
  * See if we have allocated the data yet...
  */

  if ((buf = (_cups_raster_error_t *)pthread_getspecific(raster_key))
          == NULL)
  {
    DEBUG_puts("4get_error_buffer: allocating memory for thread.");

   /*
    * No, allocate memory as set the pointer for the key...
    */

    buf = calloc(1, sizeof(_cups_raster_error_t));
    pthread_setspecific(raster_key, buf);

    DEBUG_printf(("4get_error_buffer: buf=%p", buf));
  }

 /*
  * Return the pointer to the data...
  */

  return (buf);
}


/*
 * 'raster_init()' - Initialize error buffer once.
 */

static void
raster_init(void)
{
  pthread_key_create(&raster_key, raster_destructor);

  DEBUG_printf(("3raster_init(): raster_key=%x(%u)", (unsigned)raster_key, (unsigned)raster_key));
}


/*
 * 'raster_destructor()' - Free memory allocated by get_error_buffer().
 */

static void
raster_destructor(void *value)		/* I - Data to free */
{
  _cups_raster_error_t *buf = (_cups_raster_error_t *)value;
					/* Error buffer */


  DEBUG_printf(("3raster_destructor(value=%p)", value));

  if (buf->start)
    free(buf->start);

  free(value);
}


#else
/*
 * Implement static globals...
 */

/*
 * 'get_error_buffer()' - Return a pointer to thread local storage.
 */

_cups_raster_error_t *			/* O - Pointer to error buffer */
get_error_buffer(void)
{
  static _cups_raster_error_t buf = { 0, 0, 0 };
					/* Error buffer */


  return (&buf);
}
#endif /* HAVE_PTHREAD_H */


/*
 * End of "$Id: error.c 12748 2015-06-24 15:58:40Z msweet $".
 */
