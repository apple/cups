/*
 * "$Id: globals.c 7870 2008-08-27 18:14:10Z mike $"
 *
 *   Global variable access routines for CUPS.
 *
 *   Copyright 2007-2010 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products, all rights reserved.
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
 *   _cupsGlobalLock()    - Lock the global mutex.
 *   _cupsGlobals()       - Return a pointer to thread local storage
 *   _cupsGlobalUnlock()  - Unlock the global mutex.
 *   DllMain()            - Main entry for library.
 *   cups_globals_alloc() - Allocate and initialize global data.
 *   cups_globals_free()  - Free global data.
 *   cups_globals_init()  - Initialize environment variables.
 */

/*
 * Include necessary headers...
 */

#include "cups-private.h"


/*
 * Local globals...
 */


static _cups_threadkey_t cups_globals_key = _CUPS_THREADKEY_INITIALIZER;
					/* Thread local storage key */
#ifdef HAVE_PTHREAD_H
static pthread_once_t	cups_globals_key_once = PTHREAD_ONCE_INIT;
					/* One-time initialization object */
#endif /* HAVE_PTHREAD_H */
static _cups_mutex_t	cups_global_mutex = _CUPS_MUTEX_INITIALIZER;
					/* Global critical section */


/*
 * Local functions...
 */

static _cups_globals_t	*cups_globals_alloc(void);
static void		cups_globals_free(_cups_globals_t *g);
#ifdef HAVE_PTHREAD_H
static void		cups_globals_init(void);
#endif /* HAVE_PTHREAD_H */


/*
 * '_cupsGlobalLock()' - Lock the global mutex.
 */

void
_cupsGlobalLock(void)
{
#ifdef HAVE_PTHREAD_H
  pthread_mutex_lock(&cups_global_mutex);
#elif defined(WIN32)
  EnterCriticalSection(&cups_global_mutex.m_criticalSection);
#endif /* HAVE_PTHREAD_H */
}


/*
 * '_cupsGlobals()' - Return a pointer to thread local storage
 */

_cups_globals_t *			/* O - Pointer to global data */
_cupsGlobals(void)
{
  _cups_globals_t *cg;			/* Pointer to global data */


#ifdef HAVE_PTHREAD_H
 /*
  * Initialize the global data exactly once...
  */

  pthread_once(&cups_globals_key_once, cups_globals_init);
#endif /* HAVE_PTHREAD_H */

 /*
  * See if we have allocated the data yet...
  */

  if ((cg = (_cups_globals_t *)_cupsThreadGetData(cups_globals_key)) == NULL)
  {
   /*
    * No, allocate memory as set the pointer for the key...
    */

    if ((cg = cups_globals_alloc()) != NULL)
      _cupsThreadSetData(cups_globals_key, cg);
  }

 /*
  * Return the pointer to the data...
  */

  return (cg);
}


/*
 * '_cupsGlobalUnlock()' - Unlock the global mutex.
 */

void
_cupsGlobalUnlock(void)
{
#ifdef HAVE_PTHREAD_H
  pthread_mutex_unlock(&cups_global_mutex);
#elif defined(WIN32)
  LeaveCriticalSection(&cups_global_mutex.m_criticalSection);
#endif /* HAVE_PTHREAD_H */
}


#ifdef WIN32
/*
 * 'DllMain()' - Main entry for library.
 */

BOOL WINAPI				/* O - Success/failure */
DllMain(HINSTANCE hinst,		/* I - DLL module handle */
        DWORD     reason,		/* I - Reason */
        LPVOID    reserved)		/* I - Unused */
{
  _cups_globals_t *cg;			/* Global data */


  (void)hinst;
  (void)reserved;

  switch (reason)
  {
    case DLL_PROCESS_ATTACH :		/* Called on library initialization */
        InitializeCriticalSection(&cups_global_mutex.m_criticalSection);

        if ((cups_globals_key = TlsAlloc()) == TLS_OUT_OF_INDEXES)
          return (FALSE);
        break;

    case DLL_THREAD_DETACH :		/* Called when a thread terminates */
        if ((cg = (_cups_globals_t *)TlsGetValue(cups_globals_key)) != NULL)
          cups_globals_free(cg);
        break;

    case DLL_PROCESS_DETACH :		/* Called when library is unloaded */
        if ((cg = (_cups_globals_t *)TlsGetValue(cups_globals_key)) != NULL)
          cups_globals_free(cg);

        TlsFree(cups_globals_key);
        DeleteCriticalSection(&cups_global_mutex.m_criticalSection);
        break;

    default:
        break;
  }

  return (TRUE);
}
#endif /* WIN32 */


/*
 * 'cups_globals_alloc()' - Allocate and initialize global data.
 */

static _cups_globals_t *		/* O - Pointer to global data */
cups_globals_alloc(void)
{
  _cups_globals_t *cg = malloc(sizeof(_cups_globals_t));
					/* Pointer to global data */
#ifdef WIN32
  HKEY		key;			/* Registry key */
  DWORD		size;			/* Size of string */
  static char	installdir[1024],	/* Install directory */
		confdir[1024],		/* Server root directory */
		localedir[1024];	/* Locale directory */
#endif /* WIN32 */


  if (!cg)
    return (NULL);

 /*
  * Clear the global storage and set the default encryption and password
  * callback values...
  */

  memset(cg, 0, sizeof(_cups_globals_t));
  cg->encryption    = (http_encryption_t)-1;
  cg->password_cb   = (cups_password_cb2_t)_cupsGetPassword;
  cg->any_root      = 1;
  cg->expired_certs = 1;
  cg->expired_root  = 1;

 /*
  * Then set directories as appropriate...
  */

#ifdef WIN32
 /*
  * Open the registry...
  */

  strcpy(installdir, "C:/Program Files/cups.org");

  if (!RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\cups.org", 0, KEY_READ,
                    &key))
  {
   /*
    * Grab the installation directory...
    */

    size = sizeof(installdir);
    RegQueryValueEx(key, "installdir", NULL, NULL, installdir, &size);
    RegCloseKey(key);
  }

  snprintf(confdir, sizeof(confdir), "%s/conf", installdir);
  snprintf(localedir, sizeof(localedir), "%s/locale", installdir);

  if ((cg->cups_datadir = getenv("CUPS_DATADIR")) == NULL)
    cg->cups_datadir = installdir;

  if ((cg->cups_serverbin = getenv("CUPS_SERVERBIN")) == NULL)
    cg->cups_serverbin = installdir;

  if ((cg->cups_serverroot = getenv("CUPS_SERVERROOT")) == NULL)
    cg->cups_serverroot = confdir;

  if ((cg->cups_statedir = getenv("CUPS_STATEDIR")) == NULL)
    cg->cups_statedir = confdir;

  if ((cg->localedir = getenv("LOCALEDIR")) == NULL)
    cg->localedir = localedir;

#else
#  ifdef HAVE_GETEUID
  if ((geteuid() != getuid() && getuid()) || getegid() != getgid())
#  else
  if (!getuid())
#  endif /* HAVE_GETEUID */
  {
   /*
    * When running setuid/setgid, don't allow environment variables to override
    * the directories...
    */

    cg->cups_datadir    = CUPS_DATADIR;
    cg->cups_serverbin  = CUPS_SERVERBIN;
    cg->cups_serverroot = CUPS_SERVERROOT;
    cg->cups_statedir   = CUPS_STATEDIR;
    cg->localedir       = CUPS_LOCALEDIR;
  }
  else
  {
   /*
    * Allow directories to be overridden by environment variables.
    */

    if ((cg->cups_datadir = getenv("CUPS_DATADIR")) == NULL)
      cg->cups_datadir = CUPS_DATADIR;

    if ((cg->cups_serverbin = getenv("CUPS_SERVERBIN")) == NULL)
      cg->cups_serverbin = CUPS_SERVERBIN;

    if ((cg->cups_serverroot = getenv("CUPS_SERVERROOT")) == NULL)
      cg->cups_serverroot = CUPS_SERVERROOT;

    if ((cg->cups_statedir = getenv("CUPS_STATEDIR")) == NULL)
      cg->cups_statedir = CUPS_STATEDIR;

    if ((cg->localedir = getenv("LOCALEDIR")) == NULL)
      cg->localedir = CUPS_LOCALEDIR;
  }
#endif /* WIN32 */

  return (cg);
}


/*
 * 'cups_globals_free()' - Free global data.
 */

static void
cups_globals_free(_cups_globals_t *cg)	/* I - Pointer to global data */
{
  _cups_buffer_t	*buffer,	/* Current read/write buffer */
			*next;		/* Next buffer */


  if (cg->last_status_message)
    _cupsStrFree(cg->last_status_message);

  for (buffer = cg->cups_buffers; buffer; buffer = next)
  {
    next = buffer->next;
    free(buffer);
  }

  cupsArrayDelete(cg->leg_size_lut);
  cupsArrayDelete(cg->ppd_size_lut);
  cupsArrayDelete(cg->pwg_size_lut);

  httpClose(cg->http);

  _httpFreeCredentials(cg->tls_credentials);

  cupsFileClose(cg->stdio_files[0]);
  cupsFileClose(cg->stdio_files[1]);
  cupsFileClose(cg->stdio_files[2]);

  cupsFreeOptions(cg->cupsd_num_settings, cg->cupsd_settings);

  free(cg);
}


#ifdef HAVE_PTHREAD_H
/*
 * 'cups_globals_init()' - Initialize environment variables.
 */

static void
cups_globals_init(void)
{
 /*
  * Register the global data for this thread...
  */

  pthread_key_create(&cups_globals_key, (void (*)(void *))cups_globals_free);
}
#endif /* HAVE_PTHREAD_H */


/*
 * End of "$Id: globals.c 7870 2008-08-27 18:14:10Z mike $".
 */
