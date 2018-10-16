/*
 * Private threading definitions for CUPS.
 *
 * Copyright 2009-2017 by Apple Inc.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more information.
 */

#ifndef _CUPS_THREAD_PRIVATE_H_
#  define _CUPS_THREAD_PRIVATE_H_

/*
 * Include necessary headers...
 */

#  include "config.h"
#  include <cups/versioning.h>


/*
 * C++ magic...
 */

#  ifdef __cplusplus
extern "C" {
#  endif /* __cplusplus */


#  ifdef HAVE_PTHREAD_H			/* POSIX threading */
#    include <pthread.h>
typedef void *(*_cups_thread_func_t)(void *arg);
typedef pthread_t _cups_thread_t;
typedef pthread_cond_t _cups_cond_t;
typedef pthread_mutex_t _cups_mutex_t;
typedef pthread_rwlock_t _cups_rwlock_t;
typedef pthread_key_t	_cups_threadkey_t;
#    define _CUPS_COND_INITIALIZER PTHREAD_COND_INITIALIZER
#    define _CUPS_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER
#    define _CUPS_RWLOCK_INITIALIZER PTHREAD_RWLOCK_INITIALIZER
#    define _CUPS_THREADKEY_INITIALIZER 0
#    define _cupsThreadGetData(k) pthread_getspecific(k)
#    define _cupsThreadSetData(k,p) pthread_setspecific(k,p)

#  elif defined(_WIN32)			/* Windows threading */
#    include <winsock2.h>
#    include <windows.h>
typedef void *(__stdcall *_cups_thread_func_t)(void *arg);
typedef int _cups_thread_t;
typedef char _cups_cond_t;		/* TODO: Implement Win32 conditional */
typedef struct _cups_mutex_s
{
  int			m_init;		/* Flag for on-demand initialization */
  CRITICAL_SECTION	m_criticalSection;
					/* Win32 Critical Section */
} _cups_mutex_t;
typedef _cups_mutex_t _cups_rwlock_t;	/* TODO: Implement Win32 reader/writer lock */
typedef DWORD	_cups_threadkey_t;
#    define _CUPS_COND_INITIALIZER 0
#    define _CUPS_MUTEX_INITIALIZER { 0, 0 }
#    define _CUPS_RWLOCK_INITIALIZER { 0, 0 }
#    define _CUPS_THREADKEY_INITIALIZER 0
#    define _cupsThreadGetData(k) TlsGetValue(k)
#    define _cupsThreadSetData(k,p) TlsSetValue(k,p)

#  else					/* No threading */
typedef void	*(*_cups_thread_func_t)(void *arg);
typedef int	_cups_thread_t;
typedef char	_cups_cond_t;
typedef char	_cups_mutex_t;
typedef char	_cups_rwlock_t;
typedef void	*_cups_threadkey_t;
#    define _CUPS_COND_INITIALIZER 0
#    define _CUPS_MUTEX_INITIALIZER 0
#    define _CUPS_RWLOCK_INITIALIZER 0
#    define _CUPS_THREADKEY_INITIALIZER (void *)0
#    define _cupsThreadGetData(k) k
#    define _cupsThreadSetData(k,p) k=p
#  endif /* HAVE_PTHREAD_H */


/*
 * Functions...
 */

extern void	_cupsCondBroadcast(_cups_cond_t *cond) _CUPS_PRIVATE;
extern void	_cupsCondInit(_cups_cond_t *cond) _CUPS_PRIVATE;
extern void	_cupsCondWait(_cups_cond_t *cond, _cups_mutex_t *mutex, double timeout) _CUPS_PRIVATE;
extern void	_cupsMutexInit(_cups_mutex_t *mutex) _CUPS_PRIVATE;
extern void	_cupsMutexLock(_cups_mutex_t *mutex) _CUPS_PRIVATE;
extern void	_cupsMutexUnlock(_cups_mutex_t *mutex) _CUPS_PRIVATE;
extern void	_cupsRWInit(_cups_rwlock_t *rwlock) _CUPS_PRIVATE;
extern void	_cupsRWLockRead(_cups_rwlock_t *rwlock) _CUPS_PRIVATE;
extern void	_cupsRWLockWrite(_cups_rwlock_t *rwlock) _CUPS_PRIVATE;
extern void	_cupsRWUnlock(_cups_rwlock_t *rwlock) _CUPS_PRIVATE;
extern void	_cupsThreadCancel(_cups_thread_t thread) _CUPS_PRIVATE;
extern _cups_thread_t _cupsThreadCreate(_cups_thread_func_t func, void *arg) _CUPS_PRIVATE;
extern void     _cupsThreadDetach(_cups_thread_t thread) _CUPS_PRIVATE;
extern void	*_cupsThreadWait(_cups_thread_t thread) _CUPS_PRIVATE;

#  ifdef __cplusplus
}
#  endif /* __cplusplus */
#endif /* !_CUPS_THREAD_PRIVATE_H_ */
