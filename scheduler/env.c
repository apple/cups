/*
 * "$Id$"
 *
 *   Environment management routines for the Common UNIX Printing System (CUPS).
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
 * Contents:
 *
 *   cupsdClearEnv() - Clear common environment variables.
 *   cupsdInitEnv()  - Initialize the current environment with standard
 *                     variables.
 *   cupsdLoadEnv()  - Copy common environment variables into an array.
 *   cupsdSetEnv()   - Set a common environment variable.
 *   cupsdSetEnvf()  - Set a formatted common environment variable.
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"


/*
 * Local globals...
 */

static int	num_common_env = 0;	/* Number of common env vars */
static char	*common_env[100];	/* Common env vars */


/*
 * 'cupsdClearEnv()' - Clear common environment variables.
 */

void
cupsdClearEnv(void)
{
  int	i;				/* Looping var */


  for (i = 0; i < num_common_env; i ++)
    cupsdClearString(common_env + i);

  num_common_env = 0;
}


/*
 * 'cupsdInitEnv()' - Initialize the current environment with standard variables.
 */

void
cupsdInitEnv(void)
{
 /*
  * Clear existing environment variables...
  */

  cupsdClearEnv();

#ifdef __APPLE__
 /*
  * Add special voodoo magic for MacOS X - this allows MacOS X programs
  * to access their bundle resources properly...
  */

  cupsdSetString(common_env, "<CFProcessPath>");
  num_common_env = 1;
#endif	/* __APPLE__ */

 /*
  * Set common variables...
  */

  cupsdSetEnv("CUPS_CACHEDIR", CacheDir);
  cupsdSetEnv("CUPS_DATADIR", DataDir);
  cupsdSetEnv("CUPS_DOCROOT", DocumentRoot);
  cupsdSetEnv("CUPS_FONTPATH", FontPath);
  cupsdSetEnv("CUPS_REQUESTROOT", RequestRoot);
  cupsdSetEnv("CUPS_SERVERBIN", ServerBin);
  cupsdSetEnv("CUPS_SERVERROOT", ServerRoot);
  cupsdSetEnv("CUPS_STATEDIR", StateDir);
  cupsdSetEnv("DYLD_LIBRARY_PATH", NULL);
  cupsdSetEnv("LD_ASSUME_KERNEL", NULL);
  cupsdSetEnv("LD_LIBRARY_PATH", NULL);
  cupsdSetEnv("LD_PRELOAD", NULL);
  cupsdSetEnv("NLSPATH", NULL);
  cupsdSetEnvf("PATH", "%s/filter:/bin:/usr/bin", ServerBin);
  cupsdSetEnv("SHLIB_PATH", NULL);
  cupsdSetEnv("SOFTWARE", CUPS_MINIMAL);
  cupsdSetEnv("TMPDIR", TempDir);
  cupsdSetEnv("TZ", NULL);
  cupsdSetEnv("USER", "root");
  cupsdSetEnv("VG_ARGS", NULL);
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
 /*
  * Check for room...
  */

  if (num_common_env >= (int)(sizeof(common_env) / sizeof(common_env[0])))
  {
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "cupsdSetEnv: Too many environment variables set!");
    return;
  }

 /*
  * If "value" is NULL, try getting value from current environment...
  */

  if (!value)
    value = getenv(name);

  if (!value)
    return;

 /*
  * Set the new environment variable...
  */

  cupsdSetStringf(common_env + num_common_env, "%s=%s", name, value);

  cupsdLogMessage(CUPSD_LOG_DEBUG, "cupsdSetEnv: %s\n",
                  common_env[num_common_env]);

  num_common_env ++;
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
 * End of "$Id$".
 */
