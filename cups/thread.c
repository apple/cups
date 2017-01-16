/*
 * Threading primitives for CUPS.
 *
 * Copyright 2009-2016 by Apple Inc.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * missing or damaged, see the license at "http://www.cups.org/".
 *
 * This file is subject to the Apple OS-Developed Software exception.
 */

/*
 * Include necessary headers...
 */

#include "cups-private.h"
#include "thread-private.h"


#if defined(HAVE_PTHREAD_H)
/*
 * '_cupsCondBroadcast()' - Wake up waiting threads.
 */

void
_cupsCondBroadcast(_cups_cond_t *cond)	/* I - Condition */
{
  pthread_cond_broadcast(cond);
}


/*
 * '_cupsCondInit()' - Initialize a condition variable.
 */

void
_cupsCondInit(_cups_cond_t *cond)	/* I - Condition */
{
  pthread_cond_init(cond, NULL);
}


/*
 * '_cupsCondWait()' - Wait for a condition with optional timeout.
 */

void
_cupsCondWait(_cups_cond_t  *cond,	/* I - Condition */
              _cups_mutex_t *mutex,	/* I - Mutex */
	      double        timeout)	/* I - Timeout in seconds (0 or negative for none) */
{
  if (timeout > 0.0)
  {
    struct timespec abstime;		/* Timeout */

    abstime.tv_sec  = (long)timeout;
    abstime.tv_nsec = (long)(1000000000 * (timeout - (long)timeout));

    pthread_cond_timedwait(cond, mutex, &abstime);
  }
  else
    pthread_cond_wait(cond, mutex);
}


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
 * '_cupsThreadCancel()' - Cancel (kill) a thread.
 */

void
_cupsThreadCancel(_cups_thread_t thread)/* I - Thread ID */
{
  pthread_cancel(thread);
}


/*
 * '_cupsThreadCreate()' - Create a thread.
 */

_cups_thread_t				/* O - Thread ID */
_cupsThreadCreate(
    _cups_thread_func_t func,		/* I - Entry point */
    void                *arg)		/* I - Entry point context */
{
  pthread_t thread;

  if (pthread_create(&thread, NULL, (void *(*)(void *))func, arg))
    return (0);
  else
    return (thread);
}


/*
 * '_cupsThreadWait()' - Wait for a thread to exit.
 */

void *					/* O - Return value */
_cupsThreadWait(_cups_thread_t thread)	/* I - Thread ID */
{
  void	*ret;				/* Return value */


  if (pthread_join(thread, &ret))
    return (NULL);
  else
    return (ret);
}


#elif defined(WIN32)
#  include <process.h>


/*
 * '_cupsCondBroadcast()' - Wake up waiting threads.
 */

void
_cupsCondBroadcast(_cups_cond_t *cond)	/* I - Condition */
{
  // TODO: Implement me
}


/*
 * '_cupsCondInit()' - Initialize a condition variable.
 */

void
_cupsCondInit(_cups_cond_t *cond)	/* I - Condition */
{
  // TODO: Implement me
}


/*
 * '_cupsCondWait()' - Wait for a condition with optional timeout.
 */

void
_cupsCondWait(_cups_cond_t  *cond,	/* I - Condition */
              _cups_mutex_t *mutex,	/* I - Mutex */
	      double        timeout)	/* I - Timeout in seconds (0 or negative for none) */
{
  // TODO: Implement me
}


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
 * '_cupsThreadCancel()' - Cancel (kill) a thread.
 */

void
_cupsThreadCancel(_cups_thread_t thread)/* I - Thread ID */
{
  // TODO: Implement me
}


/*
 * '_cupsThreadCreate()' - Create a thread.
 */

_cups_thread_t				/* O - Thread ID */
_cupsThreadCreate(
    _cups_thread_func_t func,		/* I - Entry point */
    void                *arg)		/* I - Entry point context */
{
  return (_beginthreadex(NULL, 0, (LPTHREAD_START_ROUTINE)func, arg, 0, NULL));
}


/*
 * '_cupsThreadWait()' - Wait for a thread to exit.
 */

void *					/* O - Return value */
_cupsThreadWait(_cups_thread_t thread)	/* I - Thread ID */
{
  // TODO: Implement me
  (void)thread;

  return (NULL);
}


#else /* No threading */
/*
 * '_cupsCondBroadcast()' - Wake up waiting threads.
 */

void
_cupsCondBroadcast(_cups_cond_t *cond)	/* I - Condition */
{
  // TODO: Implement me
}


/*
 * '_cupsCondInit()' - Initialize a condition variable.
 */

void
_cupsCondInit(_cups_cond_t *cond)	/* I - Condition */
{
  // TODO: Implement me
}


/*
 * '_cupsCondWait()' - Wait for a condition with optional timeout.
 */

void
_cupsCondWait(_cups_cond_t  *cond,	/* I - Condition */
              _cups_mutex_t *mutex,	/* I - Mutex */
	      double        timeout)	/* I - Timeout in seconds (0 or negative for none) */
{
  // TODO: Implement me
}


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
 * '_cupsThreadCancel()' - Cancel (kill) a thread.
 */

void
_cupsThreadCancel(_cups_thread_t thread)/* I - Thread ID */
{
  (void)thread;
}


/*
 * '_cupsThreadCreate()' - Create a thread.
 */

_cups_thread_t				/* O - Thread ID */
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


/*
 * '_cupsThreadWait()' - Wait for a thread to exit.
 */

void *					/* O - Return value */
_cupsThreadWait(_cups_thread_t thread)	/* I - Thread ID */
{
  (void)thread;

  return (NULL);
}

#endif /* HAVE_PTHREAD_H */
