/*
 * "$Id$"
 *
 *   Configuration routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2006 by Easy Software Products, all rights reserved.
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
 *   cupsdReadConfiguration() - Read the cupsd.conf file.
 *   check_permissions()      - Fix the mode and ownership of a file or
 *                              directory.
 *   get_address()            - Get an address + port number from a line.
 *   get_addr_and_mask()      - Get an IP address and netmask.
 *   parse_aaa()              - Parse authentication, authorization, and
 *                              access control lines.
 *   parse_groups()           - Parse system group names in a string.
 *   parse_protocols()        - Parse browse protocols in a string.
 *   read_configuration()     - Read a configuration file.
 *   read_location()          - Read a <Location path> definition.
 *   read_policy()            - Read a <Policy name> definition.
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"
#include <stdarg.h>
#include <grp.h>
#include <sys/utsname.h>
#include <cups/dir.h>

#ifdef HAVE_VSYSLOG
#  include <syslog.h>
#endif /* HAVE_VSYSLOG */


/*
 * Possibly missing network definitions...
 */

#ifndef INADDR_NONE
#  define INADDR_NONE	0xffffffff
#endif /* !INADDR_NONE */


/*
 * Configuration variable structure...
 */

typedef enum
{
  CUPSD_VARTYPE_INTEGER,		/* Integer option */
  CUPSD_VARTYPE_STRING,			/* String option */
  CUPSD_VARTYPE_BOOLEAN			/* Boolean option */
} cupsd_vartype_t;

typedef struct
{
  char			*name;		/* Name of variable */
  void			*ptr;		/* Pointer to variable */
  cupsd_vartype_t	type;		/* Type (int, string, address) */
} cupsd_var_t;


/*
 * Local globals...
 */

static cupsd_var_t	variables[] =
{
  { "AccessLog",		&AccessLog,		CUPSD_VARTYPE_STRING },
  { "AutoPurgeJobs", 		&JobAutoPurge,		CUPSD_VARTYPE_BOOLEAN },
  { "BrowseInterval",		&BrowseInterval,	CUPSD_VARTYPE_INTEGER },
#ifdef HAVE_LDAP
  { "BrowseLDAPBindDN",		&BrowseLDAPBindDN,	CUPSD_VARTYPE_STRING },
  { "BrowseLDAPDN",		&BrowseLDAPDN,		CUPSD_VARTYPE_STRING },
  { "BrowseLDAPPassword",	&BrowseLDAPPassword,	CUPSD_VARTYPE_STRING },
  { "BrowseLDAPServer",		&BrowseLDAPServer,	CUPSD_VARTYPE_STRING },
#endif /* HAVE_LDAP */
  { "BrowseLocalOptions",	&BrowseLocalOptions,	CUPSD_VARTYPE_STRING },
  { "BrowsePort",		&BrowsePort,		CUPSD_VARTYPE_INTEGER },
  { "BrowseRemoteOptions",	&BrowseRemoteOptions,	CUPSD_VARTYPE_STRING },
  { "BrowseShortNames",		&BrowseShortNames,	CUPSD_VARTYPE_BOOLEAN },
  { "BrowseTimeout",		&BrowseTimeout,		CUPSD_VARTYPE_INTEGER },
  { "Browsing",			&Browsing,		CUPSD_VARTYPE_BOOLEAN },
  { "CacheDir",			&CacheDir,		CUPSD_VARTYPE_STRING },
  { "Classification",		&Classification,	CUPSD_VARTYPE_STRING },
  { "ClassifyOverride",		&ClassifyOverride,	CUPSD_VARTYPE_BOOLEAN },
  { "ConfigFilePerm",		&ConfigFilePerm,	CUPSD_VARTYPE_INTEGER },
  { "DataDir",			&DataDir,		CUPSD_VARTYPE_STRING },
  { "DefaultCharset",		&DefaultCharset,	CUPSD_VARTYPE_STRING },
  { "DefaultLanguage",		&DefaultLanguage,	CUPSD_VARTYPE_STRING },
  { "DefaultLeaseDuration",	&DefaultLeaseDuration,	CUPSD_VARTYPE_INTEGER },
  { "DefaultPolicy",		&DefaultPolicy,		CUPSD_VARTYPE_STRING },
  { "DefaultShared",		&DefaultShared,		CUPSD_VARTYPE_BOOLEAN },
  { "DocumentRoot",		&DocumentRoot,		CUPSD_VARTYPE_STRING },
  { "ErrorLog",			&ErrorLog,		CUPSD_VARTYPE_STRING },
  { "FileDevice",		&FileDevice,		CUPSD_VARTYPE_BOOLEAN },
  { "FilterLimit",		&FilterLimit,		CUPSD_VARTYPE_INTEGER },
  { "FilterNice",		&FilterNice,		CUPSD_VARTYPE_INTEGER },
  { "FontPath",			&FontPath,		CUPSD_VARTYPE_STRING },
  { "HideImplicitMembers",	&HideImplicitMembers,	CUPSD_VARTYPE_BOOLEAN },
  { "ImplicitClasses",		&ImplicitClasses,	CUPSD_VARTYPE_BOOLEAN },
  { "ImplicitAnyClasses",	&ImplicitAnyClasses,	CUPSD_VARTYPE_BOOLEAN },
  { "JobRetryLimit",		&JobRetryLimit,		CUPSD_VARTYPE_INTEGER },
  { "JobRetryInterval",		&JobRetryInterval,	CUPSD_VARTYPE_INTEGER },
  { "KeepAliveTimeout",		&KeepAliveTimeout,	CUPSD_VARTYPE_INTEGER },
  { "KeepAlive",		&KeepAlive,		CUPSD_VARTYPE_BOOLEAN },
  { "LimitRequestBody",		&MaxRequestSize,	CUPSD_VARTYPE_INTEGER },
  { "ListenBackLog",		&ListenBackLog,		CUPSD_VARTYPE_INTEGER },
  { "LogFilePerm",		&LogFilePerm,		CUPSD_VARTYPE_INTEGER },
  { "MaxActiveJobs",		&MaxActiveJobs,		CUPSD_VARTYPE_INTEGER },
  { "MaxClients",		&MaxClients,		CUPSD_VARTYPE_INTEGER },
  { "MaxClientsPerHost",	&MaxClientsPerHost,	CUPSD_VARTYPE_INTEGER },
  { "MaxCopies",		&MaxCopies,		CUPSD_VARTYPE_INTEGER },
  { "MaxEvents",		&MaxEvents,		CUPSD_VARTYPE_INTEGER },
  { "MaxJobs",			&MaxJobs,		CUPSD_VARTYPE_INTEGER },
  { "MaxJobsPerPrinter",	&MaxJobsPerPrinter,	CUPSD_VARTYPE_INTEGER },
  { "MaxJobsPerUser",		&MaxJobsPerUser,	CUPSD_VARTYPE_INTEGER },
  { "MaxLeaseDuration",		&MaxLeaseDuration,	CUPSD_VARTYPE_INTEGER },
  { "MaxLogSize",		&MaxLogSize,		CUPSD_VARTYPE_INTEGER },
  { "MaxPrinterHistory",	&MaxPrinterHistory,	CUPSD_VARTYPE_INTEGER },
  { "MaxRequestSize",		&MaxRequestSize,	CUPSD_VARTYPE_INTEGER },
  { "MaxSubscriptions",		&MaxSubscriptions,	CUPSD_VARTYPE_INTEGER },
  { "MaxSubscriptionsPerJob",	&MaxSubscriptionsPerJob,	CUPSD_VARTYPE_INTEGER },
  { "MaxSubscriptionsPerPrinter",&MaxSubscriptionsPerPrinter,	CUPSD_VARTYPE_INTEGER },
  { "MaxSubscriptionsPerUser",	&MaxSubscriptionsPerUser,	CUPSD_VARTYPE_INTEGER },
  { "PageLog",			&PageLog,		CUPSD_VARTYPE_STRING },
  { "PreserveJobFiles",		&JobFiles,		CUPSD_VARTYPE_BOOLEAN },
  { "PreserveJobHistory",	&JobHistory,		CUPSD_VARTYPE_BOOLEAN },
  { "Printcap",			&Printcap,		CUPSD_VARTYPE_STRING },
  { "PrintcapGUI",		&PrintcapGUI,		CUPSD_VARTYPE_STRING },
  { "ReloadTimeout",		&ReloadTimeout,		CUPSD_VARTYPE_INTEGER },
  { "RemoteRoot",		&RemoteRoot,		CUPSD_VARTYPE_STRING },
  { "RequestRoot",		&RequestRoot,		CUPSD_VARTYPE_STRING },
  { "RIPCache",			&RIPCache,		CUPSD_VARTYPE_STRING },
  { "RootCertDuration",		&RootCertDuration,	CUPSD_VARTYPE_INTEGER },
  { "ServerAdmin",		&ServerAdmin,		CUPSD_VARTYPE_STRING },
  { "ServerBin",		&ServerBin,		CUPSD_VARTYPE_STRING },
#ifdef HAVE_SSL
  { "ServerCertificate",	&ServerCertificate,	CUPSD_VARTYPE_STRING },
#  if defined(HAVE_LIBSSL) || defined(HAVE_GNUTLS)
  { "ServerKey",		&ServerKey,		CUPSD_VARTYPE_STRING },
#  endif /* HAVE_LIBSSL || HAVE_GNUTLS */
#endif /* HAVE_SSL */
#ifdef HAVE_LAUNCHD
  { "LaunchdTimeout",		&LaunchdTimeout,	CUPSD_VARTYPE_INTEGER },
  { "LaunchdConf",		&LaunchdConf,		CUPSD_VARTYPE_STRING },
#endif /* HAVE_LAUNCHD */
  { "ServerName",		&ServerName,		CUPSD_VARTYPE_STRING },
  { "ServerRoot",		&ServerRoot,		CUPSD_VARTYPE_STRING },
  { "StateDir",			&StateDir,		CUPSD_VARTYPE_STRING },
  { "TempDir",			&TempDir,		CUPSD_VARTYPE_STRING },
  { "Timeout",			&Timeout,		CUPSD_VARTYPE_INTEGER },
  { "UseNetworkDefault",	&UseNetworkDefault,	CUPSD_VARTYPE_BOOLEAN }
};
#define NUM_VARS	(sizeof(variables) / sizeof(variables[0]))


static unsigned		ones[4] =
			{
			  0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff
			};
static unsigned		zeros[4] =
			{
			  0x00000000, 0x00000000, 0x00000000, 0x00000000
			};


/*
 * Local functions...
 */
static int		check_permissions(const char *filename,
			                  const char *suffix, int mode,
					  int user, int group, int is_dir,
					  int create_dir);
static http_addrlist_t	*get_address(const char *value, int defport);
static int		get_addr_and_mask(const char *value, unsigned *ip,
			                  unsigned *mask);
static int		parse_aaa(cupsd_location_t *loc, char *line,
			          char *value, int linenum);
static int		parse_groups(const char *s);
static int		parse_protocols(const char *s);
static int		read_configuration(cups_file_t *fp);
static int		read_location(cups_file_t *fp, char *name, int linenum);
static int		read_policy(cups_file_t *fp, char *name, int linenum);


/*
 * 'cupsdReadConfiguration()' - Read the cupsd.conf file.
 */

int					/* O - 1 on success, 0 otherwise */
cupsdReadConfiguration(void)
{
  int		i;			/* Looping var */
  cups_file_t	*fp;			/* Configuration file */
  int		status;			/* Return status */
  char		temp[1024],		/* Temporary buffer */
		*slash;			/* Directory separator */
  cups_lang_t	*language;		/* Language */
  struct passwd	*user;			/* Default user */
  struct group	*group;			/* Default group */
  char		*old_serverroot,	/* Old ServerRoot */
		*old_requestroot;	/* Old RequestRoot */
  const char	*tmpdir;		/* TMPDIR environment variable */
  struct stat	tmpinfo;		/* Temporary directory info */


 /*
  * Save the old root paths...
  */

  old_serverroot = NULL;
  cupsdSetString(&old_serverroot, ServerRoot);
  old_requestroot = NULL;
  cupsdSetString(&old_requestroot, RequestRoot);

 /*
  * Reset the server configuration data...
  */

  cupsdDeleteAllLocations();

  if (NumBrowsers > 0)
  {
    free(Browsers);
    Browsers = NULL;

    NumBrowsers = 0;
  }

  if (NumPolled > 0)
  {
    free(Polled);

    NumPolled = 0;
  }

  if (NumRelays > 0)
  {
    for (i = 0; i < NumRelays; i ++)
      if (Relays[i].from.type == AUTH_NAME)
	free(Relays[i].from.mask.name.name);

    free(Relays);

    NumRelays = 0;
  }

  cupsdDeleteAllListeners();

 /*
  * String options...
  */

  cupsdSetString(&ServerName, httpGetHostname(NULL, temp, sizeof(temp)));
  cupsdSetStringf(&ServerAdmin, "root@%s", temp);
  cupsdSetString(&ServerBin, CUPS_SERVERBIN);
  cupsdSetString(&RequestRoot, CUPS_REQUESTS);
  cupsdSetString(&CacheDir, CUPS_CACHEDIR);
  cupsdSetString(&DataDir, CUPS_DATADIR);
  cupsdSetString(&DocumentRoot, CUPS_DOCROOT);
  cupsdSetString(&AccessLog, CUPS_LOGDIR "/access_log");
  cupsdSetString(&ErrorLog, CUPS_LOGDIR "/error_log");
  cupsdSetString(&PageLog, CUPS_LOGDIR "/page_log");
  cupsdSetString(&Printcap, CUPS_DEFAULT_PRINTCAP);
  cupsdSetString(&PrintcapGUI, "/usr/bin/glpoptions");
  cupsdSetString(&FontPath, CUPS_FONTPATH);
  cupsdSetString(&RemoteRoot, "remroot");
  cupsdSetString(&ServerHeader, "CUPS/1.2");
  cupsdSetString(&StateDir, CUPS_STATEDIR);

  if (!strcmp(CUPS_DEFAULT_PRINTCAP, "/etc/printers.conf"))
    PrintcapFormat = PRINTCAP_SOLARIS;
  else
    PrintcapFormat = PRINTCAP_BSD;

  strlcpy(temp, ConfigurationFile, sizeof(temp));
  if ((slash = strrchr(temp, '/')) != NULL)
    *slash = '\0';

  cupsdSetString(&ServerRoot, temp);

  cupsdClearString(&Classification);
  ClassifyOverride  = 0;

#ifdef HAVE_SSL
#  ifdef HAVE_CDSASSL
  cupsdSetString(&ServerCertificate, "/Library/Keychains/System.keychain");
#  else
  cupsdSetString(&ServerCertificate, "ssl/server.crt");
  cupsdSetString(&ServerKey, "ssl/server.key");
#  endif /* HAVE_CDSASSL */
#endif /* HAVE_SSL */

  language = cupsLangDefault();

  if (!strcmp(language->language, "C") || !strcmp(language->language, "POSIX"))
    cupsdSetString(&DefaultLanguage, "en");
  else
    cupsdSetString(&DefaultLanguage, language->language);

  cupsdSetString(&DefaultCharset, _cupsEncodingName(language->encoding));

  cupsdSetString(&RIPCache, "8m");

  cupsdSetString(&TempDir, NULL);

 /*
  * Find the default user...
  */

  if ((user = getpwnam(CUPS_DEFAULT_USER)) != NULL)
    User = user->pw_uid;
  else
  {
   /*
    * Use the (historical) NFS nobody user ID (-2 as a 16-bit twos-
    * complement number...)
    */

    User = 65534;
  }

  endpwent();

 /*
  * Find the default group...
  */

  group = getgrnam(CUPS_DEFAULT_GROUP);
  endgrent();

  if (group)
    Group = group->gr_gid;
  else
  {
   /*
    * Fallback to group "nobody"...
    */

    group = getgrnam("nobody");
    endgrent();

    if (group)
      Group = group->gr_gid;
    else
    {
     /*
      * Use the (historical) NFS nobody group ID (-2 as a 16-bit twos-
      * complement number...)
      */

      Group = 65534;
    }
  }

 /*
  * Numeric options...
  */

  ConfigFilePerm        = CUPS_DEFAULT_CONFIG_FILE_PERM;
  DefaultAuthType       = AUTH_BASIC;
#ifdef HAVE_SSL
  DefaultEncryption     = HTTP_ENCRYPT_REQUIRED;
#endif /* HAVE_SSL */
  JobRetryLimit         = 5;
  JobRetryInterval      = 300;
  FileDevice            = FALSE;
  FilterLevel           = 0;
  FilterLimit           = 0;
  FilterNice            = 0;
  HostNameLookups       = FALSE;
  ImplicitClasses       = CUPS_DEFAULT_IMPLICIT_CLASSES;
  ImplicitAnyClasses    = FALSE;
  HideImplicitMembers   = TRUE;
  KeepAlive             = TRUE;
  KeepAliveTimeout      = DEFAULT_KEEPALIVE;
  ListenBackLog         = SOMAXCONN;
  LogFilePerm           = CUPS_DEFAULT_LOG_FILE_PERM;
  LogLevel              = CUPSD_LOG_ERROR;
  MaxClients            = 100;
  MaxClientsPerHost     = 0;
  MaxLogSize            = 1024 * 1024;
  MaxPrinterHistory     = 10;
  MaxRequestSize        = 0;
  ReloadTimeout	        = 60;
  RootCertDuration      = 300;
  Timeout               = DEFAULT_TIMEOUT;
  NumSystemGroups       = 0;

  BrowseInterval        = DEFAULT_INTERVAL;
  BrowsePort            = ippPort();
  BrowseLocalProtocols  = parse_protocols(CUPS_DEFAULT_BROWSE_LOCAL_PROTOCOLS);
  BrowseRemoteProtocols = parse_protocols(CUPS_DEFAULT_BROWSE_REMOTE_PROTOCOLS);
  BrowseShortNames      = CUPS_DEFAULT_BROWSE_SHORT_NAMES;
  BrowseTimeout         = DEFAULT_TIMEOUT;
  Browsing              = CUPS_DEFAULT_BROWSING;
  DefaultShared         = CUPS_DEFAULT_DEFAULT_SHARED;

  cupsdClearString(&BrowseLocalOptions);
  cupsdClearString(&BrowseRemoteOptions);

#ifdef HAVE_LDAP
  cupsdClearString(&BrowseLDAPBindDN);
  cupsdClearString(&BrowseLDAPDN);
  cupsdClearString(&BrowseLDAPPassword);
  cupsdClearString(&BrowseLDAPServer);
#endif /* HAVE_LDAP */

  JobHistory          = DEFAULT_HISTORY;
  JobFiles            = DEFAULT_FILES;
  JobAutoPurge        = 0;
  MaxJobs             = 500;
  MaxActiveJobs       = 0;
  MaxJobsPerUser      = 0;
  MaxJobsPerPrinter   = 0;
  MaxCopies           = 100;

  cupsdDeleteAllPolicies();
  cupsdClearString(&DefaultPolicy);

  MaxSubscriptions           = 100;
  MaxSubscriptionsPerJob     = 0;
  MaxSubscriptionsPerPrinter = 0;
  MaxSubscriptionsPerUser    = 0;
  DefaultLeaseDuration       = 86400;
  MaxLeaseDuration           = 0;

#ifdef HAVE_LAUNCHD
  LaunchdTimeout = DEFAULT_TIMEOUT + 10;
  cupsdSetString(&LaunchdConf, CUPS_DEFAULT_LAUNCHD_CONF);
#endif /* HAVE_LAUNCHD */

 /*
  * Read the configuration file...
  */

  if ((fp = cupsFileOpen(ConfigurationFile, "r")) == NULL)
    return (0);

  status = read_configuration(fp);

  cupsFileClose(fp);

  if (!status)
    return (0);

  RunUser = getuid();

 /*
  * Use the default system group if none was supplied in cupsd.conf...
  */

  if (NumSystemGroups == 0)
  {
    if (!parse_groups(CUPS_DEFAULT_SYSTEM_GROUPS))
    {
     /*
      * Find the group associated with GID 0...
      */

      group = getgrgid(0);
      endgrent();

      if (group != NULL)
	cupsdSetString(&SystemGroups[0], group->gr_name);
      else
	cupsdSetString(&SystemGroups[0], "unknown");

      SystemGroupIDs[0] = 0;
      NumSystemGroups   = 1;
    }
  }

 /*
  * Get the access control list for browsing...
  */

  BrowseACL = cupsdFindLocation("CUPS_INTERNAL_BROWSE_ACL");

 /*
  * Open the system log for cupsd if necessary...
  */

#ifdef HAVE_VSYSLOG
  if (!strcmp(AccessLog, "syslog") ||
      !strcmp(ErrorLog, "syslog") ||
      !strcmp(PageLog, "syslog"))
    openlog("cupsd", LOG_PID | LOG_NOWAIT | LOG_NDELAY, LOG_LPR);
#endif /* HAVE_VSYSLOG */

 /*
  * Log the configuration file that was used...
  */

  cupsdLogMessage(CUPSD_LOG_INFO, "Loaded configuration file \"%s\"",
                  ConfigurationFile);

 /*
  * Validate the Group and SystemGroup settings - they cannot be the same,
  * otherwise the CGI programs will be able to authenticate as root without
  * a password!
  */

  if (!RunUser)
  {
    for (i = 0; i < NumSystemGroups; i ++)
      if (Group == SystemGroupIDs[i])
        break;

    if (i < NumSystemGroups)
    {
     /*
      * Log the error and reset the group to a safe value...
      */

      cupsdLogMessage(CUPSD_LOG_NOTICE,
                      "Group and SystemGroup cannot use the same groups!");
      cupsdLogMessage(CUPSD_LOG_INFO, "Resetting Group to \"nobody\"...");

      group = getgrnam("nobody");
      endgrent();

      if (group != NULL)
	Group = group->gr_gid;
      else
      {
       /*
	* Use the (historical) NFS nobody group ID (-2 as a 16-bit twos-
	* complement number...)
	*/

	Group = 65534;
      }
    }
  }

 /*
  * Check that we have at least one listen/port line; if not, report this
  * as an error and exit!
  */

  if (cupsArrayCount(Listeners) == 0)
  {
   /*
    * No listeners!
    */

    cupsdLogMessage(CUPSD_LOG_EMERG,
                    "No valid Listen or Port lines were found in the configuration file!");

   /*
    * Commit suicide...
    */

    cupsdEndProcess(getpid(), 0);
  }

 /*
  * Set the default locale using the language and charset...
  */

  cupsdSetStringf(&DefaultLocale, "%s.%s", DefaultLanguage, DefaultCharset);

 /*
  * Update all relative filenames to include the full path from ServerRoot...
  */

  if (DocumentRoot[0] != '/')
    cupsdSetStringf(&DocumentRoot, "%s/%s", ServerRoot, DocumentRoot);

  if (RequestRoot[0] != '/')
    cupsdSetStringf(&RequestRoot, "%s/%s", ServerRoot, RequestRoot);

  if (ServerBin[0] != '/')
    cupsdSetStringf(&ServerBin, "%s/%s", ServerRoot, ServerBin);

  if (StateDir[0] != '/')
    cupsdSetStringf(&StateDir, "%s/%s", ServerRoot, StateDir);

  if (CacheDir[0] != '/')
    cupsdSetStringf(&CacheDir, "%s/%s", ServerRoot, CacheDir);

#ifdef HAVE_SSL
  if (ServerCertificate[0] != '/')
    cupsdSetStringf(&ServerCertificate, "%s/%s", ServerRoot, ServerCertificate);

  if (!strncmp(ServerRoot, ServerCertificate, strlen(ServerRoot)))
  {
    chown(ServerCertificate, RunUser, Group);
    chmod(ServerCertificate, 0600);
  }

#  if defined(HAVE_LIBSSL) || defined(HAVE_GNUTLS)
  if (ServerKey[0] != '/')
    cupsdSetStringf(&ServerKey, "%s/%s", ServerRoot, ServerKey);

  if (!strncmp(ServerRoot, ServerKey, strlen(ServerRoot)))
  {
    chown(ServerKey, RunUser, Group);
    chmod(ServerKey, 0600);
  }
#  endif /* HAVE_LIBSSL || HAVE_GNUTLS */
#endif /* HAVE_SSL */

 /*
  * Make sure that directories and config files are owned and
  * writable by the user and group in the cupsd.conf file...
  */

  check_permissions(CacheDir, NULL, 0775, RunUser, Group, 1, 1);
/*  check_permissions(CacheDir, "ppd", 0755, RunUser, Group, 1, 1);*/

  check_permissions(StateDir, NULL, 0755, RunUser, Group, 1, 1);
  check_permissions(StateDir, "certs", RunUser ? 0711 : 0511, User,
                    SystemGroupIDs[0], 1, 1);

  check_permissions(ServerRoot, NULL, 0755, RunUser, Group, 1, 0);
  check_permissions(ServerRoot, "ppd", 0755, RunUser, Group, 1, 1);
  check_permissions(ServerRoot, "ssl", 0700, RunUser, Group, 1, 0);
  check_permissions(ServerRoot, "cupsd.conf", ConfigFilePerm, RunUser, Group,
                    0, 0);
  check_permissions(ServerRoot, "classes.conf", 0600, RunUser, Group, 0, 0);
  check_permissions(ServerRoot, "printers.conf", 0600, RunUser, Group, 0, 0);
  check_permissions(ServerRoot, "passwd.md5", 0600, User, Group, 0, 0);

 /*
  * Update TempDir to the default if it hasn't been set already...
  */

  if (!TempDir)
  {
    if ((tmpdir = getenv("TMPDIR")) != NULL)
    {
     /*
      * TMPDIR is defined, see if it is OK for us to use...
      */

      if (stat(tmpdir, &tmpinfo))
        cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to access TMPDIR (%s): %s",
	                tmpdir, strerror(errno));
      else if (!S_ISDIR(tmpinfo.st_mode))
        cupsdLogMessage(CUPSD_LOG_ERROR, "TMPDIR (%s) is not a directory!",
	                tmpdir);
      else if ((tmpinfo.st_uid != User || !(tmpinfo.st_mode & S_IWUSR)) &&
               (tmpinfo.st_gid != Group || !(tmpinfo.st_mode & S_IWGRP)) &&
	       !(tmpinfo.st_mode & S_IWOTH))
        cupsdLogMessage(CUPSD_LOG_ERROR,
	                "TMPDIR (%s) has the wrong permissions!", tmpdir);
      else
        cupsdSetString(&TempDir, tmpdir);

      if (!TempDir)
        cupsdLogMessage(CUPSD_LOG_INFO, "Using default TempDir of %s/tmp...",
	                RequestRoot);
    }

    if (!TempDir)
      cupsdSetStringf(&TempDir, "%s/tmp", RequestRoot);
  }

 /*
  * Make sure the request and temporary directories have the right
  * permissions...
  */

  check_permissions(RequestRoot, NULL, 0710, RunUser, Group, 1, 1);

  if (!strncmp(TempDir, RequestRoot, strlen(RequestRoot)) ||
      access(TempDir, 0))
  {
   /*
    * Update ownership and permissions if the CUPS temp directory
    * is under the spool directory or does not exist...
    */

    check_permissions(TempDir, NULL, 01770, RunUser, Group, 1, 1);
  }

  if (!strncmp(TempDir, RequestRoot, strlen(RequestRoot)))
  {
   /*
    * Clean out the temporary directory...
    */

    cups_dir_t		*dir;		/* Temporary directory */
    cups_dentry_t	*dent;		/* Directory entry */
    char		tempfile[1024];	/* Temporary filename */


    if ((dir = cupsDirOpen(TempDir)) != NULL)
    {
      cupsdLogMessage(CUPSD_LOG_INFO,
                      "Cleaning out old temporary files in \"%s\"...", TempDir);

      while ((dent = cupsDirRead(dir)) != NULL)
      {
        snprintf(tempfile, sizeof(tempfile), "%s/%s", TempDir, dent->filename);

	if (unlink(tempfile))
	  cupsdLogMessage(CUPSD_LOG_ERROR,
	                  "Unable to remove temporary file \"%s\" - %s",
	                  tempfile, strerror(errno));
        else
	  cupsdLogMessage(CUPSD_LOG_DEBUG, "Removed temporary file \"%s\"...",
	                  tempfile);
      }

      cupsDirClose(dir);
    }
    else
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "Unable to open temporary directory \"%s\" - %s",
                      TempDir, strerror(errno));
  }

 /*
  * Setup environment variables...
  */

  cupsdInitEnv();

 /*
  * Check the MaxClients setting, and then allocate memory for it...
  */

  if (MaxClients > (MaxFDs / 3) || MaxClients <= 0)
  {
    if (MaxClients > 0)
      cupsdLogMessage(CUPSD_LOG_INFO, "MaxClients limited to 1/3 (%d) of the file descriptor limit (%d)...",
                 MaxFDs / 3, MaxFDs);

    MaxClients = MaxFDs / 3;
  }

  cupsdLogMessage(CUPSD_LOG_INFO, "Configured for up to %d clients.",
                  MaxClients);

 /*
  * Check the MaxActiveJobs setting; limit to 1/3 the available
  * file descriptors, since we need a pipe for each job...
  */

  if (MaxActiveJobs > (MaxFDs / 3))
    MaxActiveJobs = MaxFDs / 3;

  if (Classification && strcasecmp(Classification, "none") == 0)
    cupsdClearString(&Classification);

  if (Classification)
    cupsdLogMessage(CUPSD_LOG_INFO, "Security set to \"%s\"", Classification);

 /*
  * Update the MaxClientsPerHost value, as needed...
  */

  if (MaxClientsPerHost <= 0)
    MaxClientsPerHost = MaxClients;

  if (MaxClientsPerHost > MaxClients)
    MaxClientsPerHost = MaxClients;

  cupsdLogMessage(CUPSD_LOG_INFO,
                  "Allowing up to %d client connections per host.",
                  MaxClientsPerHost);

 /*
  * Update the default policy, as needed...
  */

  if (DefaultPolicy)
    DefaultPolicyPtr = cupsdFindPolicy(DefaultPolicy);
  else
    DefaultPolicyPtr = NULL;

  if (!DefaultPolicyPtr)
  {
    cupsd_policy_t	*p;		/* New policy */
    cupsd_location_t	*po;		/* New policy operation */


    if (DefaultPolicy)
      cupsdLogMessage(CUPSD_LOG_ERROR, "Default policy \"%s\" not found!",
                      DefaultPolicy);

    cupsdSetString(&DefaultPolicy, "default");

    if ((DefaultPolicyPtr = cupsdFindPolicy("default")) != NULL)
      cupsdLogMessage(CUPSD_LOG_INFO,
                      "Using policy \"default\" as the default!");
    else
    {
      cupsdLogMessage(CUPSD_LOG_INFO,
                      "Creating CUPS default administrative policy:");

      DefaultPolicyPtr = p = cupsdAddPolicy("default");

      cupsdLogMessage(CUPSD_LOG_INFO, "<Policy default>");
      cupsdLogMessage(CUPSD_LOG_INFO,
                      "<Limit Send-Document Send-URI Cancel-Job Hold-Job "
                      "Release-Job Restart-Job Purge-Jobs "
		      "Set-Job-Attributes Create-Job-Subscription "
		      "Renew-Subscription Cancel-Subscription "
		      "Get-Notifications Reprocess-Job Cancel-Current-Job "
		      "Suspend-Current-Job Resume-Job CUPS-Move-Job "
		      "CUPS-Authenticate-Job>");
      cupsdLogMessage(CUPSD_LOG_INFO, "Order Deny,Allow");

      po = cupsdAddPolicyOp(p, NULL, IPP_SEND_DOCUMENT);
      po->order_type = AUTH_ALLOW;
      po->level      = AUTH_USER;

      cupsdAddName(po, "@OWNER");
      cupsdAddName(po, "@SYSTEM");
      cupsdLogMessage(CUPSD_LOG_INFO, "Require user @OWNER @SYSTEM");

      cupsdAddPolicyOp(p, po, IPP_SEND_URI);
      cupsdAddPolicyOp(p, po, IPP_CANCEL_JOB);
      cupsdAddPolicyOp(p, po, IPP_HOLD_JOB);
      cupsdAddPolicyOp(p, po, IPP_RELEASE_JOB);
      cupsdAddPolicyOp(p, po, IPP_RESTART_JOB);
      cupsdAddPolicyOp(p, po, IPP_PURGE_JOBS);
      cupsdAddPolicyOp(p, po, IPP_SET_JOB_ATTRIBUTES);
      cupsdAddPolicyOp(p, po, IPP_CREATE_JOB_SUBSCRIPTION);
      cupsdAddPolicyOp(p, po, IPP_RENEW_SUBSCRIPTION);
      cupsdAddPolicyOp(p, po, IPP_CANCEL_SUBSCRIPTION);
      cupsdAddPolicyOp(p, po, IPP_GET_NOTIFICATIONS);
      cupsdAddPolicyOp(p, po, IPP_REPROCESS_JOB);
      cupsdAddPolicyOp(p, po, IPP_CANCEL_CURRENT_JOB);
      cupsdAddPolicyOp(p, po, IPP_SUSPEND_CURRENT_JOB);
      cupsdAddPolicyOp(p, po, IPP_RESUME_JOB);
      cupsdAddPolicyOp(p, po, CUPS_MOVE_JOB);
      cupsdAddPolicyOp(p, po, CUPS_AUTHENTICATE_JOB);

      cupsdLogMessage(CUPSD_LOG_INFO, "</Limit>");

      cupsdLogMessage(CUPSD_LOG_INFO,
                      "<Limit Pause-Printer Resume-Printer "
                      "Set-Printer-Attributes Enable-Printer "
		      "Disable-Printer Pause-Printer-After-Current-Job "
		      "Hold-New-Jobs Release-Held-New-Jobs "
		      "Deactivate-Printer Activate-Printer Restart-Printer "
		      "Shutdown-Printer Startup-Printer Promote-Job "
		      "Schedule-Job-After CUPS-Add-Printer "
		      "CUPS-Delete-Printer CUPS-Add-Class CUPS-Delete-Class "
		      "CUPS-Accept-Jobs CUPS-Reject-Jobs CUPS-Set-Default>");
      cupsdLogMessage(CUPSD_LOG_INFO, "Order Deny,Allow");
      cupsdLogMessage(CUPSD_LOG_INFO, "AuthType Basic");

      po = cupsdAddPolicyOp(p, NULL, IPP_PAUSE_PRINTER);
      po->order_type = AUTH_ALLOW;
      po->type       = AUTH_BASIC;
      po->level      = AUTH_USER;

      cupsdAddName(po, "@SYSTEM");
      cupsdLogMessage(CUPSD_LOG_INFO, "Require user @SYSTEM");

      cupsdAddPolicyOp(p, po, IPP_RESUME_PRINTER);
      cupsdAddPolicyOp(p, po, IPP_SET_PRINTER_ATTRIBUTES);
      cupsdAddPolicyOp(p, po, IPP_ENABLE_PRINTER);
      cupsdAddPolicyOp(p, po, IPP_DISABLE_PRINTER);
      cupsdAddPolicyOp(p, po, IPP_PAUSE_PRINTER_AFTER_CURRENT_JOB);
      cupsdAddPolicyOp(p, po, IPP_HOLD_NEW_JOBS);
      cupsdAddPolicyOp(p, po, IPP_RELEASE_HELD_NEW_JOBS);
      cupsdAddPolicyOp(p, po, IPP_DEACTIVATE_PRINTER);
      cupsdAddPolicyOp(p, po, IPP_ACTIVATE_PRINTER);
      cupsdAddPolicyOp(p, po, IPP_RESTART_PRINTER);
      cupsdAddPolicyOp(p, po, IPP_SHUTDOWN_PRINTER);
      cupsdAddPolicyOp(p, po, IPP_STARTUP_PRINTER);
      cupsdAddPolicyOp(p, po, IPP_PROMOTE_JOB);
      cupsdAddPolicyOp(p, po, IPP_SCHEDULE_JOB_AFTER);
      cupsdAddPolicyOp(p, po, CUPS_ADD_PRINTER);
      cupsdAddPolicyOp(p, po, CUPS_DELETE_PRINTER);
      cupsdAddPolicyOp(p, po, CUPS_ADD_CLASS);
      cupsdAddPolicyOp(p, po, CUPS_DELETE_CLASS);
      cupsdAddPolicyOp(p, po, CUPS_ACCEPT_JOBS);
      cupsdAddPolicyOp(p, po, CUPS_REJECT_JOBS);
      cupsdAddPolicyOp(p, po, CUPS_SET_DEFAULT);

      cupsdLogMessage(CUPSD_LOG_INFO, "</Limit>");

      cupsdLogMessage(CUPSD_LOG_INFO, "<Limit All>");
      cupsdLogMessage(CUPSD_LOG_INFO, "Order Deny,Allow");

      po = cupsdAddPolicyOp(p, NULL, IPP_ANY_OPERATION);
      po->order_type = AUTH_ALLOW;

      cupsdLogMessage(CUPSD_LOG_INFO, "</Limit>");
      cupsdLogMessage(CUPSD_LOG_INFO, "</Policy>");
    }
  }

  cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdReadConfiguration: NumPolicies=%d",
                  NumPolicies);
  for (i = 0; i < NumPolicies; i ++)
    cupsdLogMessage(CUPSD_LOG_DEBUG2,
                    "cupsdReadConfiguration: Policies[%d]=\"%s\"", i,
                    Policies[i]->name);

 /*
  * If we are doing a full reload or the server root has changed, flush
  * the jobs, printers, etc. and start from scratch...
  */

  if (NeedReload == RELOAD_ALL ||
      !old_serverroot || !ServerRoot || strcmp(old_serverroot, ServerRoot) ||
      !old_requestroot || !RequestRoot || strcmp(old_requestroot, RequestRoot))
  {
    mime_type_t	*type;			/* Current type */
    char	mimetype[MIME_MAX_SUPER + MIME_MAX_TYPE];
					/* MIME type name */


    cupsdLogMessage(CUPSD_LOG_INFO, "Full reload is required.");

   /*
    * Free all memory...
    */

    cupsdDeleteAllSubscriptions();
    cupsdFreeAllJobs();
    cupsdDeleteAllClasses();
    cupsdDeleteAllPrinters();

    DefaultPrinter = NULL;

    if (MimeDatabase != NULL)
      mimeDelete(MimeDatabase);

    if (NumMimeTypes)
    {
      for (i = 0; i < NumMimeTypes; i ++)
	_cupsStrFree(MimeTypes[i]);

      free(MimeTypes);
    }

   /*
    * Read the MIME type and conversion database...
    */

    snprintf(temp, sizeof(temp), "%s/filter", ServerBin);

    MimeDatabase = mimeLoad(ServerRoot, temp);

    if (!MimeDatabase)
    {
      cupsdLogMessage(CUPSD_LOG_EMERG,
                      "Unable to load MIME database from \'%s\'!", ServerRoot);
      exit(errno);
    }

    cupsdLogMessage(CUPSD_LOG_INFO,
                    "Loaded MIME database from \'%s\': %d types, %d filters...",
                    ServerRoot, mimeNumTypes(MimeDatabase),
		    mimeNumFilters(MimeDatabase));

   /*
    * Create a list of MIME types for the document-format-supported
    * attribute...
    */

    NumMimeTypes = mimeNumTypes(MimeDatabase);
    if (!mimeType(MimeDatabase, "application", "octet-stream"))
      NumMimeTypes ++;

    MimeTypes = calloc(NumMimeTypes, sizeof(const char *));

    for (i = 0, type = mimeFirstType(MimeDatabase);
         type;
	 i ++, type = mimeNextType(MimeDatabase))
    {
      snprintf(mimetype, sizeof(mimetype), "%s/%s", type->super, type->type);

      MimeTypes[i] = _cupsStrAlloc(mimetype);
    }

    if (i < NumMimeTypes)
      MimeTypes[i] = _cupsStrAlloc("application/octet-stream");

    if (LogLevel == CUPSD_LOG_DEBUG2)
    {
      mime_filter_t	*filter;	/* Current filter */


      for (type = mimeFirstType(MimeDatabase);
           type;
	   type = mimeNextType(MimeDatabase))
	cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdReadConfiguration: type %s/%s",
		        type->super, type->type);

      for (filter = mimeFirstFilter(MimeDatabase);
           filter;
	   filter = mimeNextFilter(MimeDatabase))
	cupsdLogMessage(CUPSD_LOG_DEBUG2,
	                "cupsdReadConfiguration: filter %s/%s to %s/%s %d %s",
		        filter->src->super, filter->src->type,
		        filter->dst->super, filter->dst->type,
		        filter->cost, filter->filter);
    }

   /*
    * Load banners...
    */

    snprintf(temp, sizeof(temp), "%s/banners", DataDir);
    cupsdLoadBanners(temp);

   /*
    * Load printers and classes...
    */

    cupsdLoadAllPrinters();
    cupsdLoadAllClasses();
    cupsdLoadRemoteCache();
    cupsdWritePrintcap();

    cupsdCreateCommonData();

   /*
    * Load queued jobs...
    */

    cupsdLoadAllJobs();

   /*
    * Load subscriptions...
    */

    cupsdLoadAllSubscriptions();

    cupsdLogMessage(CUPSD_LOG_INFO, "Full reload complete.");
  }
  else
  {
   /*
    * Not a full reload, so recreate the common printer attributes...
    */

    cupsdCreateCommonData();

   /*
    * Update all printers as needed...
    */

    cupsdUpdatePrinters();
    cupsdWritePrintcap();

    cupsdLogMessage(CUPSD_LOG_INFO, "Partial reload complete.");
  }

 /*
  * Reset the reload state...
  */

  NeedReload = RELOAD_NONE;

  cupsdClearString(&old_serverroot);
  cupsdClearString(&old_requestroot);

  return (1);
}


/*
 * 'check_permissions()' - Fix the mode and ownership of a file or directory.
 */

static int				/* O - 0 on success, -1 on error */
check_permissions(const char *filename,	/* I - File/directory name */
                  const char *suffix,	/* I - Additional file/directory name */
                  int        mode,	/* I - Permissions */
		  int        user,	/* I - Owner */
		  int        group,	/* I - Group */
		  int        is_dir,	/* I - 1 = directory, 0 = file */
		  int        create_dir)/* I - 1 = create directory, 0 = not */
{
  int		dir_created = 0;	/* Did we create a directory? */
  char		pathname[1024];		/* File name with prefix */
  struct stat	fileinfo;		/* Stat buffer */


 /*
  * Prepend the given root to the filename before testing it...
  */

  if (suffix)
  {
    snprintf(pathname, sizeof(pathname), "%s/%s", filename, suffix);
    filename = pathname;
  }

 /*
  * See if we can stat the file/directory...
  */

  if (stat(filename, &fileinfo))
  {
    if (errno == ENOENT && create_dir)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR, "Creating missing directory \"%s\"",
                      filename);

      if (mkdir(filename, mode))
      {
        cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Unable to create directory \"%s\" - %s", filename,
		        strerror(errno));
        return (-1);
      }

      dir_created = 1;
    }
    else
      return (-1);
  }

 /*
  * Make sure it's a regular file...
  */

  if (!dir_created && !is_dir && !S_ISREG(fileinfo.st_mode))
  {
    cupsdLogMessage(CUPSD_LOG_ERROR, "\"%s\" is not a regular file!", filename);
    return (-1);
  }

  if (!dir_created && is_dir && !S_ISDIR(fileinfo.st_mode))
  {
    cupsdLogMessage(CUPSD_LOG_ERROR, "\"%s\" is not a directory!", filename);
    return (-1);
  }

 /*
  * Fix owner, group, and mode as needed...
  */

  if (dir_created || fileinfo.st_uid != user || fileinfo.st_gid != group)
  {
    cupsdLogMessage(CUPSD_LOG_WARN, "Repairing ownership of \"%s\"", filename);

    if (chown(filename, user, group))
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "Unable to change ownership of \"%s\" - %s", filename,
		      strerror(errno));
      return (-1);
    }
  }

  if (dir_created || (fileinfo.st_mode & 07777) != mode)
  {
    cupsdLogMessage(CUPSD_LOG_WARN, "Repairing access permissions of \"%s\"", filename);

    if (chmod(filename, mode))
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "Unable to change permissions of \"%s\" - %s", filename,
		      strerror(errno));
      return (-1);
    }
  }

 /*
  * Everything is OK...
  */

  return (0);
}


/*
 * 'get_address()' - Get an address + port number from a line.
 */

static http_addrlist_t *		/* O - Pointer to list if address good, NULL if bad */
get_address(const char  *value,		/* I - Value string */
	    int         defport)	/* I - Default port */
{
  char			buffer[1024],	/* Hostname + port number buffer */
			defpname[255],	/* Default port name */
			*hostname,	/* Hostname or IP */
			*portname;	/* Port number or name */
  http_addrlist_t	*addrlist;	/* Address list */


 /*
  * Check for an empty value...
  */

  if (!*value)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR, "Bad (empty) address!");
    return (NULL);
  }

 /*
  * Grab a hostname and port number; if there is no colon and the port name
  * is only digits, then we have a port number by itself...
  */

  strlcpy(buffer, value, sizeof(buffer));

  if ((portname = strrchr(buffer, ':')) != NULL && !strchr(portname, ']'))
  {
    *portname++ = '\0';
    hostname = buffer;
  }
  else
  {
    for (portname = buffer; isdigit(*portname & 255); portname ++);

    if (*portname)
    {
     /*
      * Use the default port...
      */

      sprintf(defpname, "%d", defport);
      portname = defpname;
      hostname = buffer;
    }
    else
    {
     /*
      * The buffer contains just a port number...
      */

      portname = buffer;
      hostname = NULL;
    }
  }

  if (hostname && !strcmp(hostname, "*"))
    hostname = NULL;

 /*
  * Now lookup the address using httpAddrGetList()...
  */

  if ((addrlist = httpAddrGetList(hostname, AF_UNSPEC, portname)) == NULL)
    cupsdLogMessage(CUPSD_LOG_ERROR, "Hostname lookup for \"%s\" failed!",
                    hostname ? hostname : "(nil)");

  return (addrlist);
}


/*
 * 'get_addr_and_mask()' - Get an IP address and netmask.
 */

static int				/* O - 1 on success, 0 on failure */
get_addr_and_mask(const char *value,	/* I - String from config file */
                  unsigned   *ip,	/* O - Address value */
		  unsigned   *mask)	/* O - Mask value */
{
  int		i, j,			/* Looping vars */
		family,			/* Address family */
		ipcount;		/* Count of fields in address */
  unsigned	ipval;			/* Value */
  const char	*maskval,		/* Pointer to start of mask value */
		*ptr,			/* Pointer into value */
		*ptr2;			/* ... */


 /*
  * Get the address...
  */

  ip[0]   = ip[1]   = ip[2]   = ip[2]   = 0x00000000;
  mask[0] = mask[1] = mask[2] = mask[3] = 0xffffffff;

  if ((maskval = strchr(value, '/')) != NULL)
    maskval ++;
  else
    maskval = value + strlen(value);

#ifdef AF_INET6
 /*
  * Check for an IPv6 address...
  */

  if (*value == '[')
  {
   /*
    * Parse hexadecimal IPv6 address...
    */

    family  = AF_INET6;

    for (i = 0, ptr = value + 1; *ptr && i < 8; i ++)
    {
      if (*ptr == ']')
        break;
      else if (!strncmp(ptr, "::", 2))
      {
        for (ptr2 = strchr(ptr + 2, ':'), j = 0;
	     ptr2;
	     ptr2 = strchr(ptr2 + 1, ':'), j ++);

        i = 7 - j;
	ptr ++;
      }
      else if (isxdigit(*ptr & 255))
      {
        ipval = strtoul(ptr, (char **)&ptr, 16);

	if (ipval > 0xffff)
	  return (0);

        if (i & 1)
          ip[i / 2] |= ipval;
	else
          ip[i / 2] |= ipval << 16;
      }
      else
        return (0);

      while (*ptr == ':')
        ptr ++;
    }

    if (*ptr != ']')
      return (0);

    ptr ++;

    if (*ptr && *ptr != '/')
      return (0);
  }
  else
#endif /* AF_INET6 */
  {
   /*
    * Parse dotted-decimal IPv4 address...
    */

    unsigned val[4];			/* IPv4 address values */


    family  = AF_INET;
    ipcount = sscanf(value, "%u.%u.%u.%u", val + 0, val + 1, val + 2, val + 3);

   /*
    * Range check the IP numbers...
    */

    for (i = 0; i < ipcount; i ++)
      if (val[i] > 255)
        return (0);

   /*
    * Make sure the trailing values are zeroed, as some C libraries like
    * glibc apparently like to fill the unused arguments with garbage...
    */

    for (i = ipcount; i < 4; i ++)
      val[i] = 0;

   /*
    * Merge everything into a 32-bit IPv4 address in ip[3]...
    */

    ip[3] = (((((val[0] << 8) | val[1]) << 8) | val[2]) << 8) | val[3];

    if (ipcount < 4)
      mask[3] = (0xffffffff << (32 - 8 * ipcount)) & 0xffffffff;
  }

  if (*maskval)
  {
   /*
    * Get the netmask value(s)...
    */

    memset(mask, 0, sizeof(unsigned) * 4);

    if (strchr(maskval, '.'))
    {
     /*
      * Get dotted-decimal mask...
      */

      if (family != AF_INET)
        return (0);

      if (sscanf(maskval, "%u.%u.%u.%u", mask + 0, mask + 1, mask + 2, mask + 3) != 4)
        return (0);

      mask[3] |= ((((mask[0] << 8) | mask[1]) << 8) | mask[2]) << 8;
      mask[0] = mask[1] = mask[2] = 0;
    }
    else
    {
     /*
      * Get address/bits format...
      */

      i = atoi(maskval);

#ifdef AF_INET6
      if (family == AF_INET6)
      {
        if (i > 128)
	  return (0);

        i = 128 - i;

	if (i <= 96)
	  mask[0] = 0xffffffff;
	else
	  mask[0] = (0xffffffff << (i - 96)) & 0xffffffff;

	if (i <= 64)
	  mask[1] = 0xffffffff;
	else if (i >= 96)
	  mask[1] = 0;
	else
	  mask[1] = (0xffffffff << (i - 64)) & 0xffffffff;

	if (i <= 32)
	  mask[2] = 0xffffffff;
	else if (i >= 64)
	  mask[2] = 0;
	else
	  mask[2] = (0xffffffff << (i - 32)) & 0xffffffff;

	if (i == 0)
	  mask[3] = 0xffffffff;
	else if (i >= 32)
	  mask[3] = 0;
	else
	  mask[3] = (0xffffffff << i) & 0xffffffff;
      }
      else
#endif /* AF_INET6 */
      {
        if (i > 32)
	  return (0);

        mask[0] = 0xffffffff;
        mask[1] = 0xffffffff;
        mask[2] = 0xffffffff;

	if (i < 32)
          mask[3] = (0xffffffff << (32 - i)) & 0xffffffff;
	else
	  mask[3] = 0xffffffff;
      }
    }
  }

  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "get_addr_and_mask(value=\"%s\", "
                  "ip=[%08x:%08x:%08x:%08x], mask=[%08x:%08x:%08x:%08x]",
             value, ip[0], ip[1], ip[2], ip[3], mask[0], mask[1], mask[2],
	     mask[3]);

 /*
  * Check for a valid netmask; no fallback like in CUPS 1.1.x!
  */

  if ((ip[0] & ~mask[0]) != 0 ||
      (ip[1] & ~mask[1]) != 0 ||
      (ip[2] & ~mask[2]) != 0 ||
      (ip[3] & ~mask[3]) != 0)
    return (0);

  return (1);
}


/*
 * 'parse_aaa()' - Parse authentication, authorization, and access control lines.
 */

static int				/* O - 1 on success, 0 on failure */
parse_aaa(cupsd_location_t *loc,	/* I - Location */
          char             *line,	/* I - Line from file */
	  char             *value,	/* I - Start of value data */
	  int              linenum)	/* I - Current line number */
{
  char		*valptr;		/* Pointer into value */
  unsigned	ip[4],			/* IP address components */
 		mask[4];		/* IP netmask components */


  if (!strcasecmp(line, "Encryption"))
  {
   /*
    * "Encryption xxx" - set required encryption level...
    */

    if (!strcasecmp(value, "never"))
      loc->encryption = HTTP_ENCRYPT_NEVER;
    else if (!strcasecmp(value, "always"))
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "Encryption value \"%s\" on line %d is invalid in this "
		      "context. Using \"required\" instead.", value, linenum);

      loc->encryption = HTTP_ENCRYPT_REQUIRED;
    }
    else if (!strcasecmp(value, "required"))
      loc->encryption = HTTP_ENCRYPT_REQUIRED;
    else if (!strcasecmp(value, "ifrequested"))
      loc->encryption = HTTP_ENCRYPT_IF_REQUESTED;
    else
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "Unknown Encryption value %s on line %d.", value, linenum);
      return (0);
    }
  }
  else if (!strcasecmp(line, "Order"))
  {
   /*
    * "Order Deny,Allow" or "Order Allow,Deny"...
    */

    if (!strncasecmp(value, "deny", 4))
      loc->order_type = AUTH_ALLOW;
    else if (!strncasecmp(value, "allow", 5))
      loc->order_type = AUTH_DENY;
    else
    {
      cupsdLogMessage(CUPSD_LOG_ERROR, "Unknown Order value %s on line %d.",
	              value, linenum);
      return (0);
    }
  }
  else if (!strcasecmp(line, "Allow") || !strcasecmp(line, "Deny"))
  {
   /*
    * Allow [From] host/ip...
    * Deny [From] host/ip...
    */

    if (!strncasecmp(value, "from", 4))
    {
     /*
      * Strip leading "from"...
      */

      value += 4;

      while (isspace(*value & 255))
	value ++;
    }

   /*
    * Figure out what form the allow/deny address takes:
    *
    *    All
    *    None
    *    *.domain.com
    *    .domain.com
    *    host.domain.com
    *    nnn.*
    *    nnn.nnn.*
    *    nnn.nnn.nnn.*
    *    nnn.nnn.nnn.nnn
    *    nnn.nnn.nnn.nnn/mm
    *    nnn.nnn.nnn.nnn/mmm.mmm.mmm.mmm
    */

    if (!strcasecmp(value, "all"))
    {
     /*
      * All hosts...
      */

      if (!strcasecmp(line, "Allow"))
	cupsdAllowIP(loc, zeros, zeros);
      else
	cupsdDenyIP(loc, zeros, zeros);
    }
    else if (!strcasecmp(value, "none"))
    {
     /*
      * No hosts...
      */

      if (!strcasecmp(line, "Allow"))
	cupsdAllowIP(loc, ones, zeros);
      else
	cupsdDenyIP(loc, ones, zeros);
    }
#ifdef AF_INET6
    else if (value[0] == '*' || value[0] == '.' || 
             (!isdigit(value[0] & 255) && value[0] != '['))
#else
    else if (value[0] == '*' || value[0] == '.' || !isdigit(value[0] & 255))
#endif /* AF_INET6 */
    {
     /*
      * Host or domain name...
      */

      if (value[0] == '*')
	value ++;

      if (!strcasecmp(line, "Allow"))
	cupsdAllowHost(loc, value);
      else
	cupsdDenyHost(loc, value);
    }
    else
    {
     /*
      * One of many IP address forms...
      */

      if (!get_addr_and_mask(value, ip, mask))
      {
        cupsdLogMessage(CUPSD_LOG_ERROR, "Bad netmask value %s on line %d.",
	                value, linenum);
        return (0);
      }

      if (!strcasecmp(line, "Allow"))
	cupsdAllowIP(loc, ip, mask);
      else
	cupsdDenyIP(loc, ip, mask);
    }
  }
  else if (!strcasecmp(line, "AuthType"))
  {
   /*
    * AuthType {none,basic,digest,basicdigest}
    */

    if (!strcasecmp(value, "none"))
    {
      loc->type  = AUTH_NONE;
      loc->level = AUTH_ANON;
    }
    else if (!strcasecmp(value, "basic"))
    {
      loc->type = AUTH_BASIC;

      if (loc->level == AUTH_ANON)
	loc->level = AUTH_USER;
    }
    else if (!strcasecmp(value, "digest"))
    {
      loc->type = AUTH_DIGEST;

      if (loc->level == AUTH_ANON)
	loc->level = AUTH_USER;
    }
    else if (!strcasecmp(value, "basicdigest"))
    {
      loc->type = AUTH_BASICDIGEST;

      if (loc->level == AUTH_ANON)
	loc->level = AUTH_USER;
    }
    else
    {
      cupsdLogMessage(CUPSD_LOG_WARN,
                      "Unknown authorization type %s on line %d.",
	              value, linenum);
      return (0);
    }
  }
  else if (!strcasecmp(line, "AuthClass"))
  {
   /*
    * AuthClass anonymous, user, system, group
    */

    if (!strcasecmp(value, "anonymous"))
    {
      loc->type  = AUTH_NONE;
      loc->level = AUTH_ANON;

      cupsdLogMessage(CUPSD_LOG_WARN,
                      "\"AuthClass %s\" is deprecated; consider removing "
		      "it from line %d.",
	              value, linenum);
    }
    else if (!strcasecmp(value, "user"))
    {
      loc->level = AUTH_USER;

      cupsdLogMessage(CUPSD_LOG_WARN,
                      "\"AuthClass %s\" is deprecated; consider using "
		      "\"Require valid-user\" on line %d.",
	              value, linenum);
    }
    else if (!strcasecmp(value, "group"))
    {
      loc->level = AUTH_GROUP;

      cupsdLogMessage(CUPSD_LOG_WARN,
                      "\"AuthClass %s\" is deprecated; consider using "
		      "\"Require @groupname\" on line %d.",
	              value, linenum);
    }
    else if (!strcasecmp(value, "system"))
    {
      loc->level = AUTH_GROUP;

      cupsdAddName(loc, "@SYSTEM");

      cupsdLogMessage(CUPSD_LOG_WARN,
                      "\"AuthClass %s\" is deprecated; consider using "
		      "\"Require @SYSTEM\" on line %d.",
	              value, linenum);
    }
    else
    {
      cupsdLogMessage(CUPSD_LOG_WARN,
                      "Unknown authorization class %s on line %d.",
	              value, linenum);
      return (0);
    }
  }
  else if (!strcasecmp(line, "AuthGroupName"))
  {
    cupsdAddName(loc, value);

    cupsdLogMessage(CUPSD_LOG_WARN,
                    "\"AuthGroupName %s\" directive is deprecated; consider "
		    "using \"Require @%s\" on line %d.",
		    value, value, linenum);
  }
  else if (!strcasecmp(line, "Require"))
  {
   /*
    * Apache synonym for AuthClass and AuthGroupName...
    *
    * Get initial word:
    *
    *     Require valid-user
    *     Require group names
    *     Require user names
    */

    for (valptr = value; !isspace(*valptr & 255) && *valptr; valptr ++);

    if (*valptr)
      *valptr++ = '\0';

    if (!strcasecmp(value, "valid-user") ||
        !strcasecmp(value, "user"))
      loc->level = AUTH_USER;
    else if (!strcasecmp(value, "group"))
      loc->level = AUTH_GROUP;
    else
    {
      cupsdLogMessage(CUPSD_LOG_WARN, "Unknown Require type %s on line %d.",
	              value, linenum);
      return (0);
    }

   /*
    * Get the list of names from the line...
    */

    for (value = valptr; *value;)
    {
      while (isspace(*value & 255))
	value ++;

      if (*value == '\"' || *value == '\'')
      {
       /*
	* Grab quoted name...
	*/

        for (valptr = value + 1; *valptr != *value && *valptr; valptr ++);

	value ++;
      }
      else
      {
       /*
	* Grab literal name.
	*/

        for (valptr = value; !isspace(*valptr & 255) && *valptr; valptr ++);
      }

      if (*valptr)
	*valptr++ = '\0';

      cupsdAddName(loc, value);

      for (value = valptr; isspace(*value & 255); value ++);
    }
  }
  else if (!strcasecmp(line, "Satisfy"))
  {
    if (!strcasecmp(value, "all"))
      loc->satisfy = AUTH_SATISFY_ALL;
    else if (!strcasecmp(value, "any"))
      loc->satisfy = AUTH_SATISFY_ANY;
    else
    {
      cupsdLogMessage(CUPSD_LOG_WARN, "Unknown Satisfy value %s on line %d.",
                      value, linenum);
      return (0);
    }
  }
  else
    return (0);

  return (1);
}


/*
 * 'parse_groups()' - Parse system group names in a string.
 */

static int				/* O - 1 on success, 0 on failure */
parse_groups(const char *s)		/* I - Space-delimited groups */
{
  int		status;			/* Return status */
  char		value[1024],		/* Value string */
		*valstart,		/* Pointer into value */
		*valend,		/* End of value */
		quote;			/* Quote character */
  struct group	*group;			/* Group */


 /*
  * Make a copy of the string and parse out the groups...
  */

  strlcpy(value, s, sizeof(value));

  status   = 1;
  valstart = value;

  while (*valstart && NumSystemGroups < MAX_SYSTEM_GROUPS)
  {
    if (*valstart == '\'' || *valstart == '\"')
    {
     /*
      * Scan quoted name...
      */

      quote = *valstart++;

      for (valend = valstart; *valend; valend ++)
	if (*valend == quote)
	  break;
    }
    else
    {
     /*
      * Scan space or comma-delimited name...
      */

      for (valend = valstart; *valend; valend ++)
	if (isspace(*valend) || *valend == ',')
	  break;
    }

    if (*valend)
      *valend++ = '\0';

    group = getgrnam(valstart);
    if (group)
    {
      cupsdSetString(SystemGroups + NumSystemGroups, valstart);
      SystemGroupIDs[NumSystemGroups] = group->gr_gid;

      NumSystemGroups ++;
    }
    else
      status = 0;

    endgrent();

    valstart = valend;

    while (*valstart == ',' || isspace(*valstart))
      valstart ++;
  }

  return (status);
}


/*
 * 'parse_protocols()' - Parse browse protocols in a string.
 */

static int				/* O - Browse protocol bits */
parse_protocols(const char *s)		/* I - Space-delimited protocols */
{
  int	protocols;			/* Browse protocol bits */
  char	value[1024],			/* Value string */
	*valstart,			/* Pointer into value */
	*valend;			/* End of value */


 /*
  * Loop through the value string,...
  */

  strlcpy(value, s, sizeof(value));

  protocols = 0;

  for (valstart = value; *valstart;)
  {
   /*
    * Get the current space/comma-delimited protocol name...
    */

    for (valend = valstart; *valend; valend ++)
      if (isspace(*valend & 255) || *valend == ',')
	break;

    if (*valend)
      *valend++ = '\0';

   /*
    * Add the protocol to the bitmask...
    */

    if (!strcasecmp(valstart, "cups"))
      protocols |= BROWSE_CUPS;
    else if (!strcasecmp(valstart, "slp"))
      protocols |= BROWSE_SLP;
    else if (!strcasecmp(valstart, "ldap"))
      protocols |= BROWSE_LDAP;
    else if (!strcasecmp(valstart, "dnssd") || !strcasecmp(valstart, "bonjour"))
      protocols |= BROWSE_DNSSD;
    else if (!strcasecmp(valstart, "all"))
      protocols |= BROWSE_ALL;
    else
      return (-1);

    for (valstart = valend; *valstart; valstart ++)
      if (!isspace(*valstart & 255) || *valstart != ',')
	break;
  }

  return (protocols);
}


/*
 * 'read_configuration()' - Read a configuration file.
 */

static int				/* O - 1 on success, 0 on failure */
read_configuration(cups_file_t *fp)	/* I - File to read from */
{
  int			i;		/* Looping var */
  int			linenum;	/* Current line number */
  char			line[HTTP_MAX_BUFFER],
					/* Line from file */
			temp[HTTP_MAX_BUFFER],
					/* Temporary buffer for value */
			temp2[HTTP_MAX_BUFFER],
					/* Temporary buffer 2 for value */
			*ptr,		/* Pointer into line/temp */
			*value,		/* Pointer to value */
			*valueptr;	/* Pointer into value */
  int			valuelen;	/* Length of value */
  cupsd_var_t		*var;		/* Current variable */
  http_addrlist_t	*addrlist,	/* Address list */
			*addr;		/* Current address */
  unsigned		ip[4],		/* Address value */
			mask[4];	/* Netmask value */
  cupsd_dirsvc_relay_t	*relay;		/* Relay data */
  cupsd_dirsvc_poll_t	*pollp;		/* Polling data */
  cupsd_location_t	*location;	/* Browse location */
  cups_file_t		*incfile;	/* Include file */
  char			incname[1024];	/* Include filename */
  struct group		*group;		/* Group */


 /*
  * Loop through each line in the file...
  */

  linenum = 0;

  while (cupsFileGetConf(fp, line, sizeof(line), &value, &linenum))
  {
   /*
    * Decode the directive...
    */

    if (!strcasecmp(line, "Include"))
    {
     /*
      * Include filename
      */

      if (value[0] == '/')
        strlcpy(incname, value, sizeof(incname));
      else
        snprintf(incname, sizeof(incname), "%s/%s", ServerRoot, value);

      if ((incfile = cupsFileOpen(incname, "rb")) == NULL)
        cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Unable to include config file \"%s\" - %s",
	                incname, strerror(errno));
      else
      {
        read_configuration(incfile);
	cupsFileClose(incfile);
      }
    }
    else if (!strcasecmp(line, "<Location"))
    {
     /*
      * <Location path>
      */

      if (value)
      {
	linenum = read_location(fp, value, linenum);
	if (linenum == 0)
	  return (0);
      }
      else
      {
        cupsdLogMessage(CUPSD_LOG_ERROR, "Syntax error on line %d.",
	           linenum);
        return (0);
      }
    }
    else if (!strcasecmp(line, "<Policy"))
    {
     /*
      * <Policy name>
      */

      if (value)
      {
	linenum = read_policy(fp, value, linenum);
	if (linenum == 0)
	  return (0);
      }
      else
      {
        cupsdLogMessage(CUPSD_LOG_ERROR, "Syntax error on line %d.", linenum);
        return (0);
      }
    }
    else if (!strcasecmp(line, "FaxRetryInterval"))
    {
      if (value)
      {
        JobRetryInterval = atoi(value);
	cupsdLogMessage(CUPSD_LOG_WARN,
	                "FaxRetryInterval is deprecated; use "
			"JobRetryInterval on line %d.", linenum);
      }
      else
      {
        cupsdLogMessage(CUPSD_LOG_ERROR, "Syntax error on line %d.", linenum);
        return (0);
      }
    }
    else if (!strcasecmp(line, "FaxRetryLimit"))
    {
      if (value)
      {
        JobRetryLimit = atoi(value);
	cupsdLogMessage(CUPSD_LOG_WARN,
	                "FaxRetryLimit is deprecated; use "
			"JobRetryLimit on line %d.", linenum);
      }
      else
      {
        cupsdLogMessage(CUPSD_LOG_ERROR, "Syntax error on line %d.", linenum);
        return (0);
      }
    }
    else if (!strcasecmp(line, "Port") || !strcasecmp(line, "Listen")
#ifdef HAVE_SSL
             || !strcasecmp(line, "SSLPort") || !strcasecmp(line, "SSLListen")
#endif /* HAVE_SSL */
	     )
    {
     /*
      * Add listening address(es) to the list...
      */

      cupsd_listener_t	*lis;		/* New listeners array */


     /*
      * Get the address list...
      */

      addrlist = get_address(value, IPP_PORT);

      if (!addrlist)
      {
        cupsdLogMessage(CUPSD_LOG_ERROR, "Bad %s address %s at line %d.", line,
	                value, linenum);
        continue;
      }

     /*
      * Add each address...
      */

      for (addr = addrlist; addr; addr = addr->next)
      {
       /*
        * Allocate another listener...
	*/

        if (!Listeners)
	  Listeners = cupsArrayNew(NULL, NULL);

	if (!Listeners)
	{
          cupsdLogMessage(CUPSD_LOG_ERROR,
	                  "Unable to allocate %s at line %d - %s.",
	                  line, linenum, strerror(errno));
          break;
	}

        if ((lis = calloc(1, sizeof(cupsd_listener_t))) == NULL)
	{
          cupsdLogMessage(CUPSD_LOG_ERROR,
	                  "Unable to allocate %s at line %d - %s.",
	                  line, linenum, strerror(errno));
          break;
	}

        cupsArrayAdd(Listeners, lis);

       /*
        * Copy the current address and log it...
	*/

	memcpy(&(lis->address), &(addr->addr), sizeof(lis->address));
	lis->fd = -1;

#ifdef HAVE_SSL
        if (!strcasecmp(line, "SSLPort") || !strcasecmp(line, "SSLListen"))
          lis->encryption = HTTP_ENCRYPT_ALWAYS;
#endif /* HAVE_SSL */

	httpAddrString(&lis->address, temp, sizeof(temp));

#ifdef AF_INET6
        if (lis->address.addr.sa_family == AF_INET6)
          cupsdLogMessage(CUPSD_LOG_INFO, "Listening to %s:%d (IPv6)", temp,
                          ntohs(lis->address.ipv6.sin6_port));
	else
#endif /* AF_INET6 */
#ifdef AF_LOCAL
        if (lis->address.addr.sa_family == AF_LOCAL)
          cupsdLogMessage(CUPSD_LOG_INFO, "Listening to %s (Domain)", temp);
	else
#endif /* AF_LOCAL */
	cupsdLogMessage(CUPSD_LOG_INFO, "Listening to %s:%d (IPv4)", temp,
                        ntohs(lis->address.ipv4.sin_port));
      }

     /*
      * Free the list...
      */

      httpAddrFreeList(addrlist);
    }
    else if (!strcasecmp(line, "BrowseAddress"))
    {
     /*
      * Add a browse address to the list...
      */

      cupsd_dirsvc_addr_t	*dira;	/* New browse address array */


      if (NumBrowsers == 0)
        dira = malloc(sizeof(cupsd_dirsvc_addr_t));
      else
        dira = realloc(Browsers, (NumBrowsers + 1) * sizeof(cupsd_dirsvc_addr_t));

      if (!dira)
      {
        cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Unable to allocate BrowseAddress at line %d - %s.",
	                linenum, strerror(errno));
        continue;
      }

      Browsers = dira;
      dira     += NumBrowsers;

      memset(dira, 0, sizeof(cupsd_dirsvc_addr_t));

      if (!strcasecmp(value, "@LOCAL"))
      {
       /*
	* Send browse data to all local interfaces...
	*/

	strcpy(dira->iface, "*");
	NumBrowsers ++;
      }
      else if (!strncasecmp(value, "@IF(", 4))
      {
       /*
	* Send browse data to the named interface...
	*/

	strlcpy(dira->iface, value + 4, sizeof(Browsers[0].iface));

        ptr = dira->iface + strlen(dira->iface) - 1;
        if (*ptr == ')')
	  *ptr = '\0';

	NumBrowsers ++;
      }
      else if ((addrlist = get_address(value, BrowsePort)) != NULL)
      {
       /*
        * Only IPv4 addresses are supported...
        */

	for (addr = addrlist; addr; addr = addr->next)
	  if (addr->addr.addr.sa_family == AF_INET)
	    break;	    

	if (addr)
	{
	  memcpy(&(dira->to), &(addrlist->addr), sizeof(dira->to));
	  httpAddrString(&(dira->to), temp, sizeof(temp));

	  cupsdLogMessage(CUPSD_LOG_INFO,
	                  "Sending browsing info to %s:%d (IPv4)",
			  temp, ntohs(dira->to.ipv4.sin_port));
  
	  NumBrowsers ++;
	}
	else
	  cupsdLogMessage(CUPSD_LOG_ERROR, "Bad BrowseAddress %s at line %d.",
			  value, linenum);

	httpAddrFreeList(addrlist);
      }
      else
        cupsdLogMessage(CUPSD_LOG_ERROR, "Bad BrowseAddress %s at line %d.",
	                value, linenum);
    }
    else if (!strcasecmp(line, "BrowseOrder"))
    {
     /*
      * "BrowseOrder Deny,Allow" or "BrowseOrder Allow,Deny"...
      */

      if ((location = cupsdFindLocation("CUPS_INTERNAL_BROWSE_ACL")) == NULL)
        location = cupsdAddLocation("CUPS_INTERNAL_BROWSE_ACL");

      if (location == NULL)
        cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Unable to initialize browse access control list!");
      else if (!strncasecmp(value, "deny", 4))
        location->order_type = AUTH_ALLOW;
      else if (!strncasecmp(value, "allow", 5))
        location->order_type = AUTH_DENY;
      else
        cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Unknown BrowseOrder value %s on line %d.",
	                value, linenum);
    }
    else if (!strcasecmp(line, "BrowseProtocols") ||
             !strcasecmp(line, "BrowseLocalProtocols") ||
             !strcasecmp(line, "BrowseRemoteProtocols"))
    {
     /*
      * "BrowseProtocols name [... name]"
      * "BrowseLocalProtocols name [... name]"
      * "BrowseRemoteProtocols name [... name]"
      */

      int protocols = parse_protocols(value);

      if (protocols < 0)
      {
	cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Unknown browse protocol \"%s\" on line %d.",
	                value, linenum);
        break;
      }

      if (strcasecmp(line, "BrowseLocalProtocols"))
        BrowseRemoteProtocols = protocols;
      if (strcasecmp(line, "BrowseRemoteProtocols"))
        BrowseLocalProtocols = protocols;
    }
    else if (!strcasecmp(line, "BrowseAllow") ||
             !strcasecmp(line, "BrowseDeny"))
    {
     /*
      * BrowseAllow [From] host/ip...
      * BrowseDeny [From] host/ip...
      */

      if ((location = cupsdFindLocation("CUPS_INTERNAL_BROWSE_ACL")) == NULL)
        location = cupsdAddLocation("CUPS_INTERNAL_BROWSE_ACL");

      if (location == NULL)
        cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Unable to initialize browse access control list!");
      else
      {
	if (!strncasecmp(value, "from ", 5))
	{
	 /*
          * Strip leading "from"...
	  */

	  value += 5;

	  while (isspace(*value))
	    value ++;
	}

       /*
	* Figure out what form the allow/deny address takes:
	*
	*    All
	*    None
	*    *.domain.com
	*    .domain.com
	*    host.domain.com
	*    nnn.*
	*    nnn.nnn.*
	*    nnn.nnn.nnn.*
	*    nnn.nnn.nnn.nnn
	*    nnn.nnn.nnn.nnn/mm
	*    nnn.nnn.nnn.nnn/mmm.mmm.mmm.mmm
	*/

	if (!strcasecmp(value, "all"))
	{
	 /*
          * All hosts...
	  */

          if (!strcasecmp(line, "BrowseAllow"))
	    cupsdAllowIP(location, zeros, zeros);
	  else
	    cupsdDenyIP(location, zeros, zeros);
	}
	else if (!strcasecmp(value, "none"))
	{
	 /*
          * No hosts...
	  */

          if (!strcasecmp(line, "BrowseAllow"))
	    cupsdAllowIP(location, ones, zeros);
	  else
	    cupsdDenyIP(location, ones, zeros);
	}
#ifdef AF_INET6
	else if (value[0] == '*' || value[0] == '.' || 
        	 (!isdigit(value[0] & 255) && value[0] != '['))
#else
	else if (value[0] == '*' || value[0] == '.' || !isdigit(value[0] & 255))
#endif /* AF_INET6 */
	{
	 /*
          * Host or domain name...
	  */

	  if (value[0] == '*')
	    value ++;

          if (!strcasecmp(line, "BrowseAllow"))
	    cupsdAllowHost(location, value);
	  else
	    cupsdDenyHost(location, value);
	}
	else
	{
	 /*
          * One of many IP address forms...
	  */

          if (!get_addr_and_mask(value, ip, mask))
	  {
            cupsdLogMessage(CUPSD_LOG_ERROR, "Bad netmask value %s on line %d.",
	                    value, linenum);
	    break;
	  }

          if (!strcasecmp(line, "BrowseAllow"))
	    cupsdAllowIP(location, ip, mask);
	  else
	    cupsdDenyIP(location, ip, mask);
	}
      }
    }
    else if (!strcasecmp(line, "BrowseRelay"))
    {
     /*
      * BrowseRelay [from] source [to] destination
      */

      if (NumRelays == 0)
        relay = malloc(sizeof(cupsd_dirsvc_relay_t));
      else
        relay = realloc(Relays, (NumRelays + 1) * sizeof(cupsd_dirsvc_relay_t));

      if (!relay)
      {
        cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Unable to allocate BrowseRelay at line %d - %s.",
	                linenum, strerror(errno));
        continue;
      }

      Relays = relay;
      relay  += NumRelays;

      memset(relay, 0, sizeof(cupsd_dirsvc_relay_t));

      if (!strncasecmp(value, "from ", 5))
      {
       /*
        * Strip leading "from"...
	*/

	value += 5;

	while (isspace(*value))
	  value ++;
      }

     /*
      * Figure out what form the from address takes:
      *
      *    *.domain.com
      *    .domain.com
      *    host.domain.com
      *    nnn.*
      *    nnn.nnn.*
      *    nnn.nnn.nnn.*
      *    nnn.nnn.nnn.nnn
      *    nnn.nnn.nnn.nnn/mm
      *    nnn.nnn.nnn.nnn/mmm.mmm.mmm.mmm
      */

#ifdef AF_INET6
      if (value[0] == '*' || value[0] == '.' || 
          (!isdigit(value[0] & 255) && value[0] != '['))
#else
      if (value[0] == '*' || value[0] == '.' || !isdigit(value[0] & 255))
#endif /* AF_INET6 */
      {
       /*
        * Host or domain name...
	*/

	if (value[0] == '*')
	  value ++;

        strlcpy(temp, value, sizeof(temp));
	if ((ptr = strchr(temp, ' ')) != NULL)
	  *ptr = '\0';

        relay->from.type             = AUTH_NAME;
	relay->from.mask.name.name   = strdup(temp);
	relay->from.mask.name.length = strlen(temp);
      }
      else
      {
       /*
        * One of many IP address forms...
	*/

        if (!get_addr_and_mask(value, ip, mask))
	{
          cupsdLogMessage(CUPSD_LOG_ERROR, "Bad netmask value %s on line %d.",
	                  value, linenum);
	  break;
	}

        relay->from.type = AUTH_IP;
	memcpy(relay->from.mask.ip.address, ip,
	       sizeof(relay->from.mask.ip.address));
	memcpy(relay->from.mask.ip.netmask, mask,
	       sizeof(relay->from.mask.ip.netmask));
      }

     /*
      * Skip value and trailing whitespace...
      */

      for (; *value; value ++)
	if (isspace(*value))
	  break;

      while (isspace(*value))
        value ++;

      if (!strncasecmp(value, "to ", 3))
      {
       /*
        * Strip leading "to"...
	*/

	value += 3;

	while (isspace(*value))
	  value ++;
      }

     /*
      * Get "to" address and port...
      */

      if ((addrlist = get_address(value, BrowsePort)) != NULL)
      {
       /*
        * Only IPv4 addresses are supported...
        */

	for (addr = addrlist; addr; addr = addr->next)
	  if (addr->addr.addr.sa_family == AF_INET)
	    break;	    

	if (addr)
	{
	  memcpy(&(relay->to), &(addrlist->addr), sizeof(relay->to));
  
	  httpAddrString(&(relay->to), temp, sizeof(temp));
  
	  if (relay->from.type == AUTH_IP)
	    snprintf(temp2, sizeof(temp2), "%u.%u.%u.%u/%u.%u.%u.%u",
		     relay->from.mask.ip.address[0],
		     relay->from.mask.ip.address[1],
		     relay->from.mask.ip.address[2],
		     relay->from.mask.ip.address[3],
		     relay->from.mask.ip.netmask[0],
		     relay->from.mask.ip.netmask[1],
		     relay->from.mask.ip.netmask[2],
		     relay->from.mask.ip.netmask[3]);
	  else
	    strlcpy(temp2, relay->from.mask.name.name, sizeof(temp2));
  
	  cupsdLogMessage(CUPSD_LOG_INFO, "Relaying from %s to %s:%d (IPv4)",
			  temp, temp2, ntohs(relay->to.ipv4.sin_port));
  
	  NumRelays ++;
	}
	else
	  cupsdLogMessage(CUPSD_LOG_ERROR, "Bad relay address %s at line %d.",
	                  value, linenum);

	httpAddrFreeList(addrlist);
      }
      else
      {
        if (relay->from.type == AUTH_NAME)
	  free(relay->from.mask.name.name);

        cupsdLogMessage(CUPSD_LOG_ERROR, "Bad relay address %s at line %d.",
	                value, linenum);
      }
    }
    else if (!strcasecmp(line, "BrowsePoll"))
    {
     /*
      * BrowsePoll address[:port]
      */

      char		*portname;	/* Port name */
      int		portnum;	/* Port number */
      struct servent	*service;	/* Service */


     /*
      * Extract the port name from the address...
      */

      if ((portname = strrchr(value, ':')) != NULL && !strchr(portname, ']'))
      {
        *portname++ = '\0';

        if (isdigit(*portname & 255))
	  portnum = atoi(portname);
	else if ((service = getservbyname(portname, NULL)) != NULL)
	  portnum = ntohs(service->s_port);
	else
	{
	  cupsdLogMessage(CUPSD_LOG_ERROR, "Lookup of service \"%s\" failed!",
	                  portname);
          continue;
	}
      }
      else
        portnum = ippPort();

     /*
      * Add the poll entry...
      */

      if (NumPolled == 0)
        pollp = malloc(sizeof(cupsd_dirsvc_poll_t));
      else
        pollp = realloc(Polled, (NumPolled + 1) * sizeof(cupsd_dirsvc_poll_t));

      if (!pollp)
      {
        cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Unable to allocate BrowsePoll at line %d - %s.",
	                linenum, strerror(errno));
        continue;
      }

      Polled = pollp;
      pollp   += NumPolled;

      NumPolled ++;
      memset(pollp, 0, sizeof(cupsd_dirsvc_poll_t));

      strlcpy(pollp->hostname, value, sizeof(pollp->hostname));
      pollp->port = portnum;

      cupsdLogMessage(CUPSD_LOG_INFO, "Polling %s:%d", pollp->hostname,
	              pollp->port);
    }
    else if (!strcasecmp(line, "DefaultAuthType"))
    {
     /*
      * DefaultAuthType {basic,digest,basicdigest}
      */

      if (!strcasecmp(value, "basic"))
	DefaultAuthType = AUTH_BASIC;
      else if (!strcasecmp(value, "digest"))
	DefaultAuthType = AUTH_DIGEST;
      else if (!strcasecmp(value, "basicdigest"))
	DefaultAuthType = AUTH_BASICDIGEST;
      else
      {
	cupsdLogMessage(CUPSD_LOG_WARN,
	                "Unknown default authorization type %s on line %d.",
	                value, linenum);
	return (0);
      }
    }
#ifdef HAVE_SSL
    else if (!strcasecmp(line, "DefaultEncryption"))
    {
     /*
      * DefaultEncryption {Never,IfRequested,Required}
      */

      if (!value || !strcasecmp(value, "never"))
	DefaultEncryption = HTTP_ENCRYPT_NEVER;
      else if (!strcasecmp(value, "required"))
	DefaultEncryption = HTTP_ENCRYPT_REQUIRED;
      else if (!strcasecmp(value, "ifrequested"))
	DefaultEncryption = HTTP_ENCRYPT_IF_REQUESTED;
      else
      {
	cupsdLogMessage(CUPSD_LOG_WARN,
	                "Unknown default encryption %s on line %d.",
	                value, linenum);
	return (0);
      }
    }
#endif /* HAVE_SSL */
    else if (!strcasecmp(line, "User"))
    {
     /*
      * User ID to run as...
      */

      if (value && isdigit(value[0] & 255))
      {
        int uid = atoi(value);

	if (!uid)
	  cupsdLogMessage(CUPSD_LOG_ERROR,
	                  "Will not use User 0 as specified on line %d "
			  "for security reasons.  You must use a non-"
			  "privileged account instead.",
	                  linenum);
        else
	  User = atoi(value);
      }
      else if (value)
      {
        struct passwd *p;	/* Password information */

        endpwent();
	p = getpwnam(value);

	if (p)
	{
	  if (!p->pw_uid)
	    cupsdLogMessage(CUPSD_LOG_ERROR,
	                    "Will not use User %s (UID=0) as specified on line "
			    "%d for security reasons.  You must use a non-"
			    "privileged account instead.",
	                    value, linenum);
	  else
	    User = p->pw_uid;
	}
	else
	  cupsdLogMessage(CUPSD_LOG_ERROR,
	                  "Unknown User \"%s\" on line %d, ignoring!",
	                  value, linenum);
      }
      else
	cupsdLogMessage(CUPSD_LOG_ERROR,
	                "User directive on line %d missing the username!",
	                linenum);
    }
    else if (!strcasecmp(line, "Group"))
    {
     /*
      * Group ID to run as...
      */

      if (isdigit(value[0]))
        Group = atoi(value);
      else
      {
        endgrent();
	group = getgrnam(value);

	if (group != NULL)
	  Group = group->gr_gid;
	else
	  cupsdLogMessage(CUPSD_LOG_ERROR,
	                  "Unknown Group \"%s\" on line %d, ignoring!",
	                  value, linenum);
      }
    }
    else if (!strcasecmp(line, "SystemGroup"))
    {
     /*
      * SystemGroup (admin) group(s)...
      */

      if (!parse_groups(value))
	cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Unknown SystemGroup \"%s\" on line %d, ignoring!",
	                value, linenum);
    }
    else if (!strcasecmp(line, "HostNameLookups"))
    {
     /*
      * Do hostname lookups?
      */

      if (!strcasecmp(value, "off"))
        HostNameLookups = 0;
      else if (!strcasecmp(value, "on"))
        HostNameLookups = 1;
      else if (!strcasecmp(value, "double"))
        HostNameLookups = 2;
      else
	cupsdLogMessage(CUPSD_LOG_WARN, "Unknown HostNameLookups %s on line %d.",
	                value, linenum);
    }
    else if (!strcasecmp(line, "LogLevel"))
    {
     /*
      * Amount of logging to do...
      */

      if (!strcasecmp(value, "debug2"))
        LogLevel = CUPSD_LOG_DEBUG2;
      else if (!strcasecmp(value, "debug"))
        LogLevel = CUPSD_LOG_DEBUG;
      else if (!strcasecmp(value, "info"))
        LogLevel = CUPSD_LOG_INFO;
      else if (!strcasecmp(value, "notice"))
        LogLevel = CUPSD_LOG_NOTICE;
      else if (!strcasecmp(value, "warn"))
        LogLevel = CUPSD_LOG_WARN;
      else if (!strcasecmp(value, "error"))
        LogLevel = CUPSD_LOG_ERROR;
      else if (!strcasecmp(value, "crit"))
        LogLevel = CUPSD_LOG_CRIT;
      else if (!strcasecmp(value, "alert"))
        LogLevel = CUPSD_LOG_ALERT;
      else if (!strcasecmp(value, "emerg"))
        LogLevel = CUPSD_LOG_EMERG;
      else if (!strcasecmp(value, "none"))
        LogLevel = CUPSD_LOG_NONE;
      else
        cupsdLogMessage(CUPSD_LOG_WARN, "Unknown LogLevel %s on line %d.",
	                value, linenum);
    }
    else if (!strcasecmp(line, "PrintcapFormat"))
    {
     /*
      * Format of printcap file?
      */

      if (!strcasecmp(value, "bsd"))
        PrintcapFormat = PRINTCAP_BSD;
      else if (!strcasecmp(value, "solaris"))
        PrintcapFormat = PRINTCAP_SOLARIS;
      else
	cupsdLogMessage(CUPSD_LOG_WARN, "Unknown PrintcapFormat %s on line %d.",
	                value, linenum);
    }
    else if (!strcasecmp(line, "ServerTokens"))
    {
     /*
      * Set the string used for the Server header...
      */

      struct utsname plat;		/* Platform info */


      uname(&plat);

      if (!strcasecmp(value, "ProductOnly"))
	cupsdSetString(&ServerHeader, "CUPS");
      else if (!strcasecmp(value, "Major"))
	cupsdSetString(&ServerHeader, "CUPS/1");
      else if (!strcasecmp(value, "Minor"))
	cupsdSetString(&ServerHeader, "CUPS/1.2");
      else if (!strcasecmp(value, "Minimal"))
	cupsdSetString(&ServerHeader, CUPS_MINIMAL);
      else if (!strcasecmp(value, "OS"))
	cupsdSetStringf(&ServerHeader, CUPS_MINIMAL " (%s)", plat.sysname);
      else if (!strcasecmp(value, "Full"))
	cupsdSetStringf(&ServerHeader, CUPS_MINIMAL " (%s) IPP/1.1",
	                plat.sysname);
      else if (!strcasecmp(value, "None"))
	cupsdClearString(&ServerHeader);
      else
	cupsdLogMessage(CUPSD_LOG_WARN, "Unknown ServerTokens %s on line %d.",
                        value, linenum);
    }
    else if (!strcasecmp(line, "PassEnv"))
    {
     /*
      * PassEnv variable [... variable]
      */

      for (; *value;)
      {
        for (valuelen = 0; value[valuelen]; valuelen ++)
	  if (isspace(value[valuelen]) || value[valuelen] == ',')
	    break;

        if (value[valuelen])
        {
	  value[valuelen] = '\0';
	  valuelen ++;
	}

        cupsdSetEnv(value, NULL);

        for (value += valuelen; *value; value ++)
	  if (!isspace(*value) || *value != ',')
	    break;
      }
    }
    else if (!strcasecmp(line, "SetEnv"))
    {
     /*
      * SetEnv variable value
      */

      for (valueptr = value; *valueptr && !isspace(*valueptr & 255); valueptr ++);

      if (*valueptr)
      {
       /*
        * Found a value...
	*/

        while (isspace(*valueptr & 255))
	  *valueptr++ = '\0';

        cupsdSetEnv(value, valueptr);
      }
      else
        cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Missing value for SetEnv directive on line %d.",
	                linenum);
    }
    else
    {
     /*
      * Find a simple variable in the list...
      */

      for (i = NUM_VARS, var = variables; i > 0; i --, var ++)
        if (!strcasecmp(line, var->name))
	  break;

      if (i == 0)
      {
       /*
        * Unknown directive!  Output an error message and continue...
	*/

        cupsdLogMessage(CUPSD_LOG_ERROR, "Unknown directive %s on line %d.",
	                line, linenum);
        continue;
      }

      switch (var->type)
      {
        case CUPSD_VARTYPE_INTEGER :
	    if (!value)
	      cupsdLogMessage(CUPSD_LOG_ERROR,
	                      "Missing integer value for %s on line %d!",
			      line, linenum);
	    else
	    {
	      int	n;		/* Number */
	      char	*units;		/* Units */


              n = strtol(value, &units, 0);

	      if (units && *units)
	      {
        	if (tolower(units[0] & 255) == 'g')
		  n *= 1024 * 1024 * 1024;
        	else if (tolower(units[0] & 255) == 'm')
		  n *= 1024 * 1024;
		else if (tolower(units[0] & 255) == 'k')
		  n *= 1024;
		else if (tolower(units[0] & 255) == 't')
		  n *= 262144;
	      }

	      *((int *)var->ptr) = n;
	    }
	    break;

	case CUPSD_VARTYPE_BOOLEAN :
	    if (!value)
	      cupsdLogMessage(CUPSD_LOG_ERROR,
	                      "Missing boolean value for %s on line %d!",
			      line, linenum);
            else if (!strcasecmp(value, "true") ||
	             !strcasecmp(value, "on") ||
		     !strcasecmp(value, "enabled") ||
		     !strcasecmp(value, "yes") ||
		     atoi(value) != 0)
              *((int *)var->ptr) = TRUE;
	    else if (!strcasecmp(value, "false") ||
	             !strcasecmp(value, "off") ||
		     !strcasecmp(value, "disabled") ||
		     !strcasecmp(value, "no") ||
		     !strcasecmp(value, "0"))
              *((int *)var->ptr) = FALSE;
	    else
              cupsdLogMessage(CUPSD_LOG_ERROR,
	                      "Unknown boolean value %s on line %d.",
	                      value, linenum);
	    break;

	case CUPSD_VARTYPE_STRING :
	    cupsdSetString((char **)var->ptr, value);
	    break;
      }
    }
  }

  return (1);
}


/*
 * 'read_location()' - Read a <Location path> definition.
 */

static int				/* O - New line number or 0 on error */
read_location(cups_file_t *fp,		/* I - Configuration file */
              char        *location,	/* I - Location name/path */
	      int         linenum)	/* I - Current line number */
{
  cupsd_location_t	*loc,		/* New location */
			*parent;	/* Parent location */
  char			line[HTTP_MAX_BUFFER],
					/* Line buffer */
			*value,		/* Value for directive */
			*valptr;	/* Pointer into value */


  if ((parent = cupsdAddLocation(location)) == NULL)
    return (0);

  parent->limit = AUTH_LIMIT_ALL;
  loc           = parent;

  while (cupsFileGetConf(fp, line, sizeof(line), &value, &linenum))
  {
   /*
    * Decode the directive...
    */

    if (!strcasecmp(line, "</Location>"))
      return (linenum);
    else if (!strcasecmp(line, "<Limit") ||
             !strcasecmp(line, "<LimitExcept"))
    {
      if (!value)
      {
        cupsdLogMessage(CUPSD_LOG_ERROR, "Syntax error on line %d.", linenum);
        return (0);
      }
      
      if ((loc = cupsdCopyLocation(&parent)) == NULL)
        return (0);

      loc->limit = 0;
      while (*value)
      {
        for (valptr = value; !isspace(*valptr & 255) && *valptr; valptr ++);

	if (*valptr)
	  *valptr++ = '\0';

        if (!strcmp(value, "ALL"))
	  loc->limit = AUTH_LIMIT_ALL;
	else if (!strcmp(value, "GET"))
	  loc->limit |= AUTH_LIMIT_GET;
	else if (!strcmp(value, "HEAD"))
	  loc->limit |= AUTH_LIMIT_HEAD;
	else if (!strcmp(value, "OPTIONS"))
	  loc->limit |= AUTH_LIMIT_OPTIONS;
	else if (!strcmp(value, "POST"))
	  loc->limit |= AUTH_LIMIT_POST;
	else if (!strcmp(value, "PUT"))
	  loc->limit |= AUTH_LIMIT_PUT;
	else if (!strcmp(value, "TRACE"))
	  loc->limit |= AUTH_LIMIT_TRACE;
	else
	  cupsdLogMessage(CUPSD_LOG_WARN, "Unknown request type %s on line %d!",
	                  value, linenum);

        for (value = valptr; isspace(*value & 255); value ++);
      }

      if (!strcasecmp(line, "<LimitExcept"))
        loc->limit = AUTH_LIMIT_ALL ^ loc->limit;

      parent->limit &= ~loc->limit;
    }
    else if (!strcasecmp(line, "</Limit>"))
      loc = parent;
    else if (!parse_aaa(loc, line, value, linenum))
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "Unknown Location directive %s on line %d.",
	              line, linenum);
      return (0);
    }
  }

  cupsdLogMessage(CUPSD_LOG_ERROR,
                  "Unexpected end-of-file at line %d while reading location!",
                  linenum);

  return (0);
}


/*
 * 'read_policy()' - Read a <Policy name> definition.
 */

static int				/* O - New line number or 0 on error */
read_policy(cups_file_t *fp,		/* I - Configuration file */
            char        *policy,	/* I - Location name/path */
	    int         linenum)	/* I - Current line number */
{
  int			i;		/* Looping var */
  cupsd_policy_t	*pol;		/* Policy */
  cupsd_location_t	*op;		/* Policy operation */
  int			num_ops;	/* Number of IPP operations */
  ipp_op_t		ops[100];	/* Operations */
  char			line[HTTP_MAX_BUFFER],
					/* Line buffer */
			*value,		/* Value for directive */
			*valptr;	/* Pointer into value */


 /*
  * Create the policy...
  */

  if ((pol = cupsdAddPolicy(policy)) == NULL)
    return (0);

 /*
  * Read from the file...
  */

  op      = NULL;
  num_ops = 0;

  while (cupsFileGetConf(fp, line, sizeof(line), &value, &linenum))
  {
   /*
    * Decode the directive...
    */

    if (!strcasecmp(line, "</Policy>"))
    {
      if (op)
        cupsdLogMessage(CUPSD_LOG_WARN,
	                "Missing </Limit> before </Policy> on line %d!",
	                linenum);

      return (linenum);
    }
    else if (!strcasecmp(line, "<Limit") && !op)
    {
      if (!value)
      {
        cupsdLogMessage(CUPSD_LOG_ERROR, "Syntax error on line %d.", linenum);
        return (0);
      }
      
     /*
      * Scan for IPP operation names...
      */

      num_ops = 0;

      while (*value)
      {
        for (valptr = value; !isspace(*valptr & 255) && *valptr; valptr ++);

	if (*valptr)
	  *valptr++ = '\0';

        if (num_ops < (int)(sizeof(ops) / sizeof(ops[0])))
	{
	  if (!strcasecmp(value, "All"))
	    ops[num_ops] = IPP_ANY_OPERATION;
	  else if ((ops[num_ops] = ippOpValue(value)) == IPP_BAD_OPERATION)
	    cupsdLogMessage(CUPSD_LOG_ERROR,
	                    "Bad IPP operation name \"%s\" on line %d!",
	                    value, linenum);
          else
	    num_ops ++;
	}
	else
	  cupsdLogMessage(CUPSD_LOG_ERROR,
	                  "Too many operations listed on line %d!",
	                  linenum);

        for (value = valptr; isspace(*value & 255); value ++);
      }

     /*
      * If none are specified, apply the policy to all operations...
      */

      if (num_ops == 0)
      {
        ops[0]  = IPP_ANY_OPERATION;
	num_ops = 1;
      }

     /*
      * Add a new policy for the first operation...
      */

      op = cupsdAddPolicyOp(pol, NULL, ops[0]);
    }
    else if (!strcasecmp(line, "</Limit>") && op)
    {
     /*
      * Finish the current operation limit...
      */

      if (num_ops > 1)
      {
       /*
        * Copy the policy to the other operations...
	*/

        for (i = 1; i < num_ops; i ++)
	  cupsdAddPolicyOp(pol, op, ops[i]);
      }

      op = NULL;
    }
    else if (!op)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "Missing <Limit ops> directive before %s on line %d.",
                      line, linenum);
      return (0);
    }
    else if (!parse_aaa(op, line, value, linenum))
    {
      if (op)
	cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Unknown Policy Limit directive %s on line %d.",
	                line, linenum);
      else
	cupsdLogMessage(CUPSD_LOG_ERROR,
	                "Unknown Policy directive %s on line %d.",
	                line, linenum);

      return (0);
    }
  }

  cupsdLogMessage(CUPSD_LOG_ERROR,
                  "Unexpected end-of-file at line %d while reading policy \"%s\"!",
                  linenum, policy);

  return (0);
}


/*
 * End of "$Id$".
 */
