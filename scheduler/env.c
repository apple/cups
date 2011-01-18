/*
 * "$Id: env.c 9459 2011-01-11 03:48:42Z mike $"
 *
 *   Environment management routines for the CUPS scheduler.
 *
 *   Copyright 2007-2011 by Apple Inc.
 *   Copyright 1997-2006 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
 *   cupsdInitEnv()   - Initialize the current environment with standard
 *                      variables.
 *   cupsdLoadEnv()   - Copy common environment variables into an array.
 *   cupsdSetEnv()    - Set a common environment variable.
 *   cupsdSetEnvf()   - Set a formatted common environment variable.
 *   cupsdUpdateEnv() - Update the environment for the configured directories.
 *   clear_env()      - Clear common environment variables.
 *   find_env()       - Find a common environment variable.
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"


/*
 * Local globals...
 */

static int	num_common_env = 0;	/* Number of common env vars */
static char	*common_env[MAX_ENV];	/* Common env vars */


/*
 * Local functions...
 */

static void	clear_env(void);
static int	find_env(const char *name);


/*
 * 'cupsdInitEnv()' - Initialize the current environment with standard variables.
 */

void
cupsdInitEnv(void)
{
 /*
  * Clear existing environment variables...
  */

  clear_env();

#if defined(__APPLE__)
 /*
  * Add special voodoo magic for MacOS X - this allows MacOS X 
  * programs to access their bundle resources properly...
  *
  * This string is replaced in cupsdStartProcess()...
  */

  cupsdSetString(common_env, "<CFProcessPath>");
  num_common_env = 1;
#endif	/* __APPLE__ */
}


/*
 * 'cupsdLoadEnv()' - Copy common environment variables into an array.
 */

int					/* O - Number of environment variables */
cupsdLoadEnv(char *envp[],		/* I - Environment array */
             int  envmax)		/* I - Maximum number of elements */
{
  int	i;				/* Looping var */


 /*
  * Leave room for a NULL pointer at the end...
  */

  envmax --;

 /*
  * Copy pointers to the environment...
  */

  for (i = 0; i < num_common_env && i < envmax; i ++)
    envp[i] = common_env[i];

 /*
  * NULL terminate the environment array and return the number of
  * elements we added...
  */

  envp[i] = NULL;

  return (i);
}


/*
 * 'cupsdSetEnv()' - Set a common environment variable.
 */

void
cupsdSetEnv(const char *name,		/* I - Name of variable */
            const char *value)		/* I - Value of variable */
{
  int	i;				/* Index into environent array */


 /*
  * If "value" is NULL, try getting value from current environment...
  */

  if (!value)
    value = getenv(name);

  if (!value)
    return;

 /*
  * See if this variable has already been defined...
  */

  if ((i = find_env(name)) < 0)
  {
   /*
    * Check for room...
    */

    if (num_common_env >= (int)(sizeof(common_env) / sizeof(common_env[0])))
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "cupsdSetEnv: Too many environment variables set!");
      return;
    }

    i = num_common_env;
    num_common_env ++;
  }

 /*
  * Set the new environment variable...
  */

  cupsdSetStringf(common_env + i, "%s=%s", name, value);

  cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdSetEnv: %s", common_env[i]);
}


/*
 * 'cupsdSetEnvf()' - Set a formatted common environment variable.
 */

void
cupsdSetEnvf(const char *name,		/* I - Name of variable */
             const char *value,		/* I - Printf-style value of variable */
	     ...)			/* I - Additional args as needed */
{
  char		v[4096];		/* Formatting string value */
  va_list	ap;			/* Argument pointer */


 /*
  * Format the value string...
  */

  va_start(ap, value);
  vsnprintf(v, sizeof(v), value, ap);
  va_end(ap);

 /*
  * Set the env variable...
  */

  cupsdSetEnv(name, v);
}


/*
 * 'cupsdUpdateEnv()' - Update the environment for the configured directories.
 */

void
cupsdUpdateEnv(void)
{
 /*
  * Set common variables...
  */

#define set_if_undefined(name,value) if (find_env(name) < 0) cupsdSetEnv(name,value)

  set_if_undefined("CUPS_CACHEDIR", CacheDir);
  set_if_undefined("CUPS_DATADIR", DataDir);
  set_if_undefined("CUPS_DOCROOT", DocumentRoot);
  set_if_undefined("CUPS_FONTPATH", FontPath);
  set_if_undefined("CUPS_REQUESTROOT", RequestRoot);
  set_if_undefined("CUPS_SERVERBIN", ServerBin);
  set_if_undefined("CUPS_SERVERROOT", ServerRoot);
  set_if_undefined("CUPS_STATEDIR", StateDir);
  set_if_undefined("DYLD_LIBRARY_PATH", NULL);
  set_if_undefined("HOME", TempDir);
  set_if_undefined("LD_ASSUME_KERNEL", NULL);
  set_if_undefined("LD_LIBRARY_PATH", NULL);
  set_if_undefined("LD_PRELOAD", NULL);
  set_if_undefined("NLSPATH", NULL);
  if (find_env("PATH") < 0)
    cupsdSetEnvf("PATH", "%s/filter:" CUPS_BINDIR ":" CUPS_SBINDIR
			 ":/bin:/usr/bin", ServerBin);
  set_if_undefined("SERVER_ADMIN", ServerAdmin);
  set_if_undefined("SHLIB_PATH", NULL);
  set_if_undefined("SOFTWARE", CUPS_MINIMAL);
  set_if_undefined("TMPDIR", TempDir);
  set_if_undefined("TZ", NULL);
  set_if_undefined("USER", "root");
  set_if_undefined("VG_ARGS", NULL);
}


/*
 * 'clear_env()' - Clear common environment variables.
 */

static void
clear_env(void)
{
  int	i;				/* Looping var */


  for (i = 0; i < num_common_env; i ++)
    cupsdClearString(common_env + i);

  num_common_env = 0;
}


/*
 * 'find_env()' - Find a common environment variable.
 */

static int				/* O - Index or -1 if not found */
find_env(const char *name)		/* I - Variable name */
{
  int		i;			/* Looping var */
  size_t	namelen;		/* Length of name */


  for (i = 0, namelen = strlen(name); i < num_common_env; i ++)
    if (!strncmp(common_env[i], name, namelen) && common_env[i][namelen] == '=')
      return (i);

  return (-1);
}


/*
 * End of "$Id: env.c 9459 2011-01-11 03:48:42Z mike $".
 */
