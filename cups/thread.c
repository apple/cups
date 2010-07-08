/*
 * "$Id$"
 *
 *   Threading primitives for CUPS.
 *
 *   Copyright 2009-2010 by Apple Inc.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
 *   _cupsMutexLock()    - Lock a mutex.
 *   _cupsMutexUnlock()  - Unlock a mutex.
 *   _cupsThreadCreate() - Create a thread.
 */

/*
 * Include necessary headers...
 */

#include "cups-private.h"
#include "thread-private.h"


#if defined(HAVE_PTHREAD_H)
/*
 * '_cupsMutexLock()' - Lock a mutex.
 */

void
_cupsMutexLock(_cups_mutex_t *mutex)	/* I - Mutex */
{
  pthread_mutex_lock(mutex);
}


/*
 * '_cupsMutexUnlock()' - Unlock a mutex.
 */

void
_cupsMutexUnlock(_cups_mutex_t *mutex)	/* I - Mutex */
{
  pthread_mutex_unlock(mutex);
}


/*
 * '_cupsThreadCreate()' - Create a thread.
 */

int					/* O - 0 on failure, 1 on success */	
_cupsThreadCreate(
    _cups_thread_func_t func,		/* I - Entry point */
    void                *arg)		/* I - Entry point context */
{
  pthread_t thread;

  return (pthread_create(&thread, NULL, (void *(*)(void *))func, arg) == 0);
}


#elif defined(WIN32)
#  include <process.h>


/*
 * '_cupsMutexLock()' - Lock a mutex.
 */

void
_cupsMutexLock(_cups_mutex_t *mutex)	/* I - Mutex */
{
  if (!mutex->m_init)
  {
    _cupsGlobalLock();

    if (!mutex->m_init)
    {
      InitializeCriticalSection(&mutex->m_criticalSection);
      mutex->m_init = 1;
    }

    _cupsGlobalUnlock();
  }

  EnterCriticalSection(&mutex->m_criticalSection);
}


/*
 * '_cupsMutexUnlock()' - Unlock a mutex.
 */

void
_cupsMutexUnlock(_cups_mutex_t *mutex)	/* I - Mutex */
{
  LeaveCriticalSection(&mutex->m_criticalSection);
}


/*
 * '_cupsThreadCreate()' - Create a thread.
 */

int					/* O - 0 on failure, 1 on success */	
_cupsThreadCreate(
    _cups_thread_func_t func,		/* I - Entry point */
    void                *arg)		/* I - Entry point context */
{
  return (_beginthreadex(NULL, 0, (LPTHREAD_START_ROUTINE) func, arg, 0, NULL)
	      != 0);
}


#else
/*
 * '_cupsMutexLock()' - Lock a mutex.
 */

void
_cupsMutexLock(_cups_mutex_t *mutex)	/* I - Mutex */
{
}


/*
 * '_cupsMutexUnlock()' - Unlock a mutex.
 */

void
_cupsMutexUnlock(_cups_mutex_t *mutex)	/* I - Mutex */
{
}
#endif /* HAVE_PTHREAD_H */


/*
 * End of "$Id$".
 */
