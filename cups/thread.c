/*
 * "$Id: thread.c 11642 2014-02-27 15:57:59Z msweet $"
 *
 *   Threading primitives for CUPS.
 *
 *   Copyright 2009-2012 by Apple Inc.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   _cupsMutexInit()    - Initialize a mutex.
 *   _cupsMutexLock()    - Lock a mutex.
 *   _cupsMutexUnlock()  - Unlock a mutex.
 *   _cupsRWInit()       - Initialize a reader/writer lock.
 *   _cupsRWLockRead()   - Acquire a reader/writer lock for reading.
 *   _cupsRWLockWrite()  - Acquire a reader/writer lock for writing.
 *   _cupsRWUnlock()     - Release a reader/writer lock.
 *   _cupsThreadCreate() - Create a thread.
 */

/*
 * Include necessary headers...
 */

#include "cups-private.h"
#include "thread-private.h"


#if defined(HAVE_PTHREAD_H)
/*
 * '_cupsMutexInit()' - Initialize a mutex.
 */

void
_cupsMutexInit(_cups_mutex_t *mutex)	/* I - Mutex */
{
  pthread_mutex_init(mutex, NULL);
}


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
 * '_cupsRWInit()' - Initialize a reader/writer lock.
 */

void
_cupsRWInit(_cups_rwlock_t *rwlock)	/* I - Reader/writer lock */
{
  pthread_rwlock_init(rwlock, NULL);
}


/*
 * '_cupsRWLockRead()' - Acquire a reader/writer lock for reading.
 */

void
_cupsRWLockRead(_cups_rwlock_t *rwlock)	/* I - Reader/writer lock */
{
  pthread_rwlock_rdlock(rwlock);
}


/*
 * '_cupsRWLockWrite()' - Acquire a reader/writer lock for writing.
 */

void
_cupsRWLockWrite(_cups_rwlock_t *rwlock)/* I - Reader/writer lock */
{
  pthread_rwlock_wrlock(rwlock);
}


/*
 * '_cupsRWUnlock()' - Release a reader/writer lock.
 */

void
_cupsRWUnlock(_cups_rwlock_t *rwlock)	/* I - Reader/writer lock */
{
  pthread_rwlock_unlock(rwlock);
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
 * '_cupsMutexInit()' - Initialize a mutex.
 */

void
_cupsMutexInit(_cups_mutex_t *mutex)	/* I - Mutex */
{
  InitializeCriticalSection(&mutex->m_criticalSection);
  mutex->m_init = 1;
}


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
 * '_cupsRWInit()' - Initialize a reader/writer lock.
 */

void
_cupsRWInit(_cups_rwlock_t *rwlock)	/* I - Reader/writer lock */
{
  _cupsMutexInit((_cups_mutex_t *)rwlock);
}


/*
 * '_cupsRWLockRead()' - Acquire a reader/writer lock for reading.
 */

void
_cupsRWLockRead(_cups_rwlock_t *rwlock)	/* I - Reader/writer lock */
{
  _cupsMutexLock((_cups_mutex_t *)rwlock);
}


/*
 * '_cupsRWLockWrite()' - Acquire a reader/writer lock for writing.
 */

void
_cupsRWLockWrite(_cups_rwlock_t *rwlock)/* I - Reader/writer lock */
{
  _cupsMutexLock((_cups_mutex_t *)rwlock);
}


/*
 * '_cupsRWUnlock()' - Release a reader/writer lock.
 */

void
_cupsRWUnlock(_cups_rwlock_t *rwlock)	/* I - Reader/writer lock */
{
  _cupsMutexUnlock((_cups_mutex_t *)rwlock);
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
 * '_cupsMutexInit()' - Initialize a mutex.
 */

void
_cupsMutexInit(_cups_mutex_t *mutex)	/* I - Mutex */
{
  (void)mutex;
}


/*
 * '_cupsMutexLock()' - Lock a mutex.
 */

void
_cupsMutexLock(_cups_mutex_t *mutex)	/* I - Mutex */
{
  (void)mutex;
}


/*
 * '_cupsMutexUnlock()' - Unlock a mutex.
 */

void
_cupsMutexUnlock(_cups_mutex_t *mutex)	/* I - Mutex */
{
  (void)mutex;
}


/*
 * '_cupsRWInit()' - Initialize a reader/writer lock.
 */

void
_cupsRWInit(_cups_rwlock_t *rwlock)	/* I - Reader/writer lock */
{
  (void)rwlock;
}


/*
 * '_cupsRWLockRead()' - Acquire a reader/writer lock for reading.
 */

void
_cupsRWLockRead(_cups_rwlock_t *rwlock)	/* I - Reader/writer lock */
{
  (void)rwlock;
}


/*
 * '_cupsRWLockWrite()' - Acquire a reader/writer lock for writing.
 */

void
_cupsRWLockWrite(_cups_rwlock_t *rwlock)/* I - Reader/writer lock */
{
  (void)rwlock;
}


/*
 * '_cupsRWUnlock()' - Release a reader/writer lock.
 */

void
_cupsRWUnlock(_cups_rwlock_t *rwlock)	/* I - Reader/writer lock */
{
  (void)rwlock;
}


/*
 * '_cupsThreadCreate()' - Create a thread.
 */

int					/* O - 0 on failure, 1 on success */
_cupsThreadCreate(
    _cups_thread_func_t func,		/* I - Entry point */
    void                *arg)		/* I - Entry point context */
{
  fputs("DEBUG: CUPS was compiled without threading support, no thread "
        "created.\n", stderr);

  (void)func;
  (void)arg;

  return (0);
}
#endif /* HAVE_PTHREAD_H */


/*
 * End of "$Id: thread.c 11642 2014-02-27 15:57:59Z msweet $".
 */
