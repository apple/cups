/*
 * "$Id$"
 *
 *   Configuration routines for the Common UNIX Printing System (CUPS).
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
 *   cupsdReadConfiguration()  - Read the cupsd.conf file.
 *   get_address()        - Get an address + port number from a line.
 *   get_addr_and_mask()  - Get an IP address and netmask.
 *   parse_aaa()          - Parse authentication, authorization, and
 *                          access control lines.
 *   read_configuration() - Read a configuration file.
 *   read_location()      - Read a <Location path> definition.
 *   read_policy()        - Read a <Policy name> definition.
 *   CDSAGetServerCerts() - Convert a keychain name into the CFArrayRef
 *                          required by SSLSetCertificate.
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"
#include <stdarg.h>
#include <grp.h>
#include <sys/utsname.h>
#include <cups/dir.h>

#ifdef HAVE_DOMAINSOCKETS
#  include <sys/un.h>
#endif /* HAVE_DOMAINSOCKETS */

#ifdef HAVE_CDSASSL
#  include <Security/SecureTransport.h>
#  include <Security/SecIdentitySearch.h>
#endif /* HAVE_CDSASSL */

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

typedef struct
{
  char	*name;		/* Name of variable */
  void	*ptr;		/* Pointer to variable */
  int	type;		/* Type (int, string, address) */
} var_t;

#define VAR_INTEGER	0
#define VAR_STRING	1
#define VAR_BOOLEAN	2


/*
 * Local globals...
 */

static var_t	variables[] =
{
  { "AccessLog",		&AccessLog,		VAR_STRING },
  { "AutoPurgeJobs", 		&JobAutoPurge,		VAR_BOOLEAN },
  { "BrowseInterval",		&BrowseInterval,	VAR_INTEGER },
  { "BrowseLocalOptions",	&BrowseLocalOptions,	VAR_STRING },
  { "BrowsePort",		&BrowsePort,		VAR_INTEGER },
  { "BrowseRemoteOptions",	&BrowseRemoteOptions,	VAR_STRING },
  { "BrowseShortNames",		&BrowseShortNames,	VAR_BOOLEAN },
  { "BrowseTimeout",		&BrowseTimeout,		VAR_INTEGER },
  { "Browsing",			&Browsing,		VAR_BOOLEAN },
  { "CacheDir",			&CacheDir,		VAR_STRING },
  { "Classification",		&Classification,	VAR_STRING },
  { "ClassifyOverride",		&ClassifyOverride,	VAR_BOOLEAN },
  { "ConfigFilePerm",		&ConfigFilePerm,	VAR_INTEGER },
  { "DataDir",			&DataDir,		VAR_STRING },
  { "DefaultCharset",		&DefaultCharset,	VAR_STRING },
  { "DefaultLanguage",		&DefaultLanguage,	VAR_STRING },
  { "DefaultPolicy",		&DefaultPolicy,		VAR_STRING },
  { "DocumentRoot",		&DocumentRoot,		VAR_STRING },
  { "ErrorLog",			&ErrorLog,		VAR_STRING },
  { "FaxRetryLimit",		&FaxRetryLimit,		VAR_INTEGER },
  { "FaxRetryInterval",		&FaxRetryInterval,	VAR_INTEGER },
  { "FileDevice",		&FileDevice,		VAR_BOOLEAN },
  { "FilterLimit",		&FilterLimit,		VAR_INTEGER },
  { "FilterNice",		&FilterNice,		VAR_INTEGER },
  { "FontPath",			&FontPath,		VAR_STRING },
  { "HideImplicitMembers",	&HideImplicitMembers,	VAR_BOOLEAN },
  { "ImplicitClasses",		&ImplicitClasses,	VAR_BOOLEAN },
  { "ImplicitAnyClasses",	&ImplicitAnyClasses,	VAR_BOOLEAN },
  { "KeepAliveTimeout",		&KeepAliveTimeout,	VAR_INTEGER },
  { "KeepAlive",		&KeepAlive,		VAR_BOOLEAN },
  { "LimitRequestBody",		&MaxRequestSize,	VAR_INTEGER },
  { "ListenBackLog",		&ListenBackLog,		VAR_INTEGER },
  { "LogFilePerm",		&LogFilePerm,		VAR_INTEGER },
  { "MaxActiveJobs",		&MaxActiveJobs,		VAR_INTEGER },
  { "MaxClients",		&MaxClients,		VAR_INTEGER },
  { "MaxClientsPerHost",	&MaxClientsPerHost,	VAR_INTEGER },
  { "MaxCopies",		&MaxCopies,		VAR_INTEGER },
  { "MaxJobs",			&MaxJobs,		VAR_INTEGER },
  { "MaxJobsPerPrinter",	&MaxJobsPerPrinter,	VAR_INTEGER },
  { "MaxJobsPerUser",		&MaxJobsPerUser,	VAR_INTEGER },
  { "MaxLogSize",		&MaxLogSize,		VAR_INTEGER },
  { "MaxPrinterHistory",	&MaxPrinterHistory,	VAR_INTEGER },
  { "MaxRequestSize",		&MaxRequestSize,	VAR_INTEGER },
  { "PageLog",			&PageLog,		VAR_STRING },
  { "PreserveJobFiles",		&JobFiles,		VAR_BOOLEAN },
  { "PreserveJobHistory",	&JobHistory,		VAR_BOOLEAN },
  { "Printcap",			&Printcap,		VAR_STRING },
  { "PrintcapGUI",		&PrintcapGUI,		VAR_STRING },
  { "ReloadTimeout",		&ReloadTimeout,		VAR_INTEGER },
  { "RemoteRoot",		&RemoteRoot,		VAR_STRING },
  { "RequestRoot",		&RequestRoot,		VAR_STRING },
  { "RIPCache",			&RIPCache,		VAR_STRING },
  { "RunAsUser", 		&RunAsUser,		VAR_BOOLEAN },
  { "RootCertDuration",		&RootCertDuration,	VAR_INTEGER },
  { "ServerAdmin",		&ServerAdmin,		VAR_STRING },
  { "ServerBin",		&ServerBin,		VAR_STRING },
#ifdef HAVE_SSL
  { "ServerCertificate",	&ServerCertificate,	VAR_STRING },
#  if defined(HAVE_LIBSSL) || defined(HAVE_GNUTLS)
  { "ServerKey",		&ServerKey,		VAR_STRING },
#  endif /* HAVE_LIBSSL || HAVE_GNUTLS */
#endif /* HAVE_SSL */
  { "ServerName",		&ServerName,		VAR_STRING },
  { "ServerRoot",		&ServerRoot,		VAR_STRING },
  { "StateDir",			&StateDir,		VAR_STRING },
  { "TempDir",			&TempDir,		VAR_STRING },
  { "Timeout",			&Timeout,		VAR_INTEGER }
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

#ifdef HAVE_CDSASSL
CFArrayRef CDSAGetServerCerts();
#endif /* HAVE_CDSASSL */


/*
 * Local functions...
 */

static int	get_address(const char *value, unsigned defaddress, int defport,
		            int deffamily, http_addr_t *address);
static int	get_addr_and_mask(const char *value, unsigned *ip,
		                  unsigned *mask);
static int	parse_aaa(cupsd_location_t *loc, char *line, char *value, int linenum);
static int	read_configuration(cups_file_t *fp);
static int	read_location(cups_file_t *fp, char *name, int linenum);
static int	read_policy(cups_file_t *fp, char *name, int linenum);


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
  char		type[MIME_MAX_SUPER + MIME_MAX_TYPE];
					/* MIME type name */
  char		*language;		/* Language string */
  struct passwd	*user;			/* Default user */
  struct group	*group;			/* Default group */
  char		*old_serverroot,	/* Old ServerRoot */
		*old_requestroot;	/* Old RequestRoot */


 /*
  * Shutdown the server...
  */

  cupsdStopServer();

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

  if (NumListeners > 0)
  {
#ifdef HAVE_DOMAINSOCKETS
    int i;				/* Looping var */
    cupsd_listener_t	*lis;			/* Current listening socket */

    for (i = NumListeners, lis = Listeners; i > 0; i --, lis ++)
      if (lis->address.sin_family == AF_LOCAL)
	cupsdClearString((char **)&lis->address.sin_addr);
#endif /* HAVE_DOMAINSOCKETS */

    free(Listeners);

    NumListeners = 0;
  }

 /*
  * String options...
  */

  cupsdSetString(&ServerName, httpGetHostname(temp, sizeof(temp)));
  cupsdSetStringf(&ServerAdmin, "root@%s", temp);
  cupsdSetString(&ServerBin, CUPS_SERVERBIN);
  cupsdSetString(&RequestRoot, CUPS_REQUESTS);
  cupsdSetString(&CacheDir, CUPS_CACHEDIR);
  cupsdSetString(&DataDir, CUPS_DATADIR);
  cupsdSetString(&DocumentRoot, CUPS_DOCROOT);
  cupsdSetString(&AccessLog, CUPS_LOGDIR "/access_log");
  cupsdSetString(&ErrorLog, CUPS_LOGDIR "/error_log");
  cupsdSetString(&PageLog, CUPS_LOGDIR "/page_log");
  cupsdSetString(&Printcap, "/etc/printcap");
  cupsdSetString(&PrintcapGUI, "/usr/bin/glpoptions");
  cupsdSetString(&FontPath, CUPS_FONTPATH);
  cupsdSetString(&RemoteRoot, "remroot");
  cupsdSetString(&ServerHeader, "CUPS/1.1");
  cupsdSetString(&StateDir, CUPS_STATEDIR);

  strlcpy(temp, ConfigurationFile, sizeof(temp));
  if ((slash = strrchr(temp, '/')) != NULL)
    *slash = '\0';

  cupsdSetString(&ServerRoot, temp);

  cupsdClearString(&Classification);
  ClassifyOverride  = 0;

#ifdef HAVE_SSL
#  ifdef HAVE_CDSASSL
  cupsdSetString(&ServerCertificate, "/var/root/Library/Keychains/CUPS");
#  else
  cupsdSetString(&ServerCertificate, "ssl/server.crt");
  cupsdSetString(&ServerKey, "ssl/server.key");
#  endif /* HAVE_CDSASSL */
#endif /* HAVE_SSL */

  if ((language = DEFAULT_LANGUAGE) == NULL)
    language = "en";
  else if (!strcmp(language, "C") || !strcmp(language, "POSIX"))
    language = "en";

  cupsdSetString(&DefaultLanguage, language);
  cupsdSetString(&DefaultCharset, DEFAULT_CHARSET);

  cupsdSetString(&RIPCache, "8m");

  if (getenv("TMPDIR") == NULL)
    cupsdSetString(&TempDir, CUPS_REQUESTS "/tmp");
  else
    cupsdSetString(&TempDir, getenv("TMPDIR"));

 /*
  * Find the default system group: "sys", "system", or "root"...
  */

  group = getgrnam(CUPS_DEFAULT_GROUP);
  endgrent();

  NumSystemGroups = 0;

  if (group != NULL)
  {
   /*
    * Found the group, use it!
    */

    cupsdSetString(&SystemGroups[0], CUPS_DEFAULT_GROUP);

    SystemGroupIDs[0] = group->gr_gid;
  }
  else
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
  }

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
  * Find the default group (nobody)...
  */

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

 /*
  * Numeric options...
  */

  ConfigFilePerm      = 0640;
  DefaultAuthType     = AUTH_BASIC;
  FaxRetryLimit       = 5;
  FaxRetryInterval    = 300;
  FileDevice          = FALSE;
  FilterLevel         = 0;
  FilterLimit         = 0;
  FilterNice          = 0;
  HostNameLookups     = FALSE;
  ImplicitClasses     = TRUE;
  ImplicitAnyClasses  = FALSE;
  HideImplicitMembers = TRUE;
  KeepAlive           = TRUE;
  KeepAliveTimeout    = DEFAULT_KEEPALIVE;
  ListenBackLog       = SOMAXCONN;
  LogFilePerm         = 0644;
  LogLevel            = L_ERROR;
  MaxClients          = 100;
  MaxClientsPerHost   = 0;
  MaxLogSize          = 1024 * 1024;
  MaxPrinterHistory   = 10;
  MaxRequestSize      = 0;
  ReloadTimeout	      = 60;
  RootCertDuration    = 300;
  RunAsUser           = FALSE;
  Timeout             = DEFAULT_TIMEOUT;

  BrowseInterval        = DEFAULT_INTERVAL;
  BrowsePort            = ippPort();
  BrowseLocalProtocols  = BROWSE_CUPS;
  BrowseRemoteProtocols = BROWSE_CUPS;
  BrowseShortNames      = TRUE;
  BrowseTimeout         = DEFAULT_TIMEOUT;
  Browsing              = TRUE;

  cupsdClearString(&BrowseLocalOptions);
  cupsdClearString(&BrowseRemoteOptions);

  JobHistory          = DEFAULT_HISTORY;
  JobFiles            = DEFAULT_FILES;
  JobAutoPurge        = 0;
  MaxJobs             = 500;
  MaxActiveJobs       = 0;
  MaxJobsPerUser      = 0;
  MaxJobsPerPrinter   = 0;
  MaxCopies           = 100;

  cupsdClearString(&DefaultPolicy);

 /*
  * Read the configuration file...
  */

  if ((fp = cupsFileOpen(ConfigurationFile, "r")) == NULL)
    return (0);

  status = read_configuration(fp);

  cupsFileClose(fp);

  if (!status)
    return (0);

  if (RunAsUser)
    RunUser = User;
  else
    RunUser = getuid();

 /*
  * Use the default system group if none was supplied in cupsd.conf...
  */

  if (NumSystemGroups == 0)
    NumSystemGroups ++;

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

  cupsdLogMessage(L_INFO, "Loaded configuration file \"%s\"", ConfigurationFile);

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

      cupsdLogMessage(L_NOTICE, "Group and SystemGroup cannot use the same groups!");
      cupsdLogMessage(L_INFO, "Resetting Group to \"nobody\"...");

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

  if (NumListeners == 0)
  {
   /*
    * No listeners!
    */

    cupsdLogMessage(L_EMERG, "No valid Listen or Port lines were found in the configuration file!");

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

#  if defined(HAVE_LIBSSL) || defined(HAVE_GNUTLS)
  chown(ServerCertificate, RunUser, Group);
  chmod(ServerCertificate, ConfigFilePerm);

  if (ServerKey[0] != '/')
    cupsdSetStringf(&ServerKey, "%s/%s", ServerRoot, ServerKey);

  chown(ServerKey, RunUser, Group);
  chmod(ServerKey, ConfigFilePerm);
#  endif /* HAVE_LIBSSL || HAVE_GNUTLS */
#endif /* HAVE_SSL */

 /*
  * Make sure that directories and config files are owned and
  * writable by the user and group in the cupsd.conf file...
  */

  chown(CacheDir, RunUser, Group);
  chmod(CacheDir, 0775);

  snprintf(temp, sizeof(temp), "%s/ppd", CacheDir);
  if (access(temp, 0))
    mkdir(temp, 0755);
  chown(temp, RunUser, Group);
  chmod(temp, 0755);

  chown(StateDir, RunUser, Group);
  chmod(StateDir, 0775);

  snprintf(temp, sizeof(temp), "%s/certs", StateDir);
  if (access(temp, 0))
    mkdir(temp, 0510);
  chown(temp, User, SystemGroupIDs[0]);
  if (RunUser)
    chmod(temp, 0710);
  else
    chmod(temp, 0510);

  chown(ServerRoot, RunUser, Group);
  chmod(ServerRoot, 0755);

  snprintf(temp, sizeof(temp), "%s/ppd", ServerRoot);
  if (access(temp, 0))
    mkdir(temp, 0755);
  chown(temp, RunUser, Group);
  chmod(temp, 0755);

  snprintf(temp, sizeof(temp), "%s/ssl", ServerRoot);
  if (access(temp, 0))
    mkdir(temp, 0700);
  chown(temp, RunUser, Group);
  chmod(temp, 0700);

  snprintf(temp, sizeof(temp), "%s/cupsd.conf", ServerRoot);
  chown(temp, RunUser, Group);
  chmod(temp, ConfigFilePerm);

  snprintf(temp, sizeof(temp), "%s/classes.conf", ServerRoot);
  chown(temp, RunUser, Group);
  chmod(temp, 0600);

  snprintf(temp, sizeof(temp), "%s/printers.conf", ServerRoot);
  chown(temp, RunUser, Group);
  chmod(temp, 0600);

  snprintf(temp, sizeof(temp), "%s/passwd.md5", ServerRoot);
  chown(temp, User, Group);
  chmod(temp, 0600);

 /*
  * Make sure the request and temporary directories have the right
  * permissions...
  */

  chown(RequestRoot, RunUser, Group);
  chmod(RequestRoot, 0710);

  if (!strncmp(TempDir, RequestRoot, strlen(RequestRoot)) ||
      access(TempDir, 0))
  {
   /*
    * Update ownership and permissions if the CUPS temp directory
    * is under the spool directory or does not exist...
    */

    if (access(TempDir, 0))
      mkdir(TempDir, 01770);

    chown(TempDir, RunUser, Group);
    chmod(TempDir, 01770);
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
      cupsdLogMessage(L_INFO, "Cleaning out old temporary files in \"%s\"...",
                 TempDir);

      while ((dent = cupsDirRead(dir)) != NULL)
      {
        snprintf(tempfile, sizeof(tempfile), "%s/%s", TempDir, dent->filename);

	if (unlink(tempfile))
	  cupsdLogMessage(L_ERROR, "Unable to remove temporary file \"%s\" - %s",
	             tempfile, strerror(errno));
        else
	  cupsdLogMessage(L_DEBUG, "Removed temporary file \"%s\"...", tempfile);
      }

      cupsDirClose(dir);
    }
    else
      cupsdLogMessage(L_ERROR, "Unable to open temporary directory \"%s\" - %s",
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
      cupsdLogMessage(L_INFO, "MaxClients limited to 1/3 (%d) of the file descriptor limit (%d)...",
                 MaxFDs / 3, MaxFDs);

    MaxClients = MaxFDs / 3;
  }

  if ((Clients = calloc(sizeof(cupsd_client_t), MaxClients)) == NULL)
  {
    cupsdLogMessage(L_ERROR, "cupsdReadConfiguration: Unable to allocate memory for %d clients: %s",
               MaxClients, strerror(errno));
    exit(1);
  }
  else
    cupsdLogMessage(L_INFO, "Configured for up to %d clients.", MaxClients);

 /*
  * Check the MaxActiveJobs setting; limit to 1/3 the available
  * file descriptors, since we need a pipe for each job...
  */

  if (MaxActiveJobs > (MaxFDs / 3))
    MaxActiveJobs = MaxFDs / 3;

  if (Classification && strcasecmp(Classification, "none") == 0)
    cupsdClearString(&Classification);

  if (Classification)
    cupsdLogMessage(L_INFO, "Security set to \"%s\"", Classification);

 /*
  * Update the MaxClientsPerHost value, as needed...
  */

  if (MaxClientsPerHost <= 0)
    MaxClientsPerHost = MaxClients;

  if (MaxClientsPerHost > MaxClients)
    MaxClientsPerHost = MaxClients;

  cupsdLogMessage(L_INFO, "Allowing up to %d client connections per host.",
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
    cupsd_location_t		*po;		/* New policy operation */


    if (DefaultPolicy)
      cupsdLogMessage(L_ERROR, "Default policy \"%s\" not found!", DefaultPolicy);

    if ((DefaultPolicyPtr = cupsdFindPolicy("default")) != NULL)
      cupsdLogMessage(L_INFO, "Using policy \"default\" as the default!");
    else
    {
      cupsdLogMessage(L_INFO, "Creating CUPS default administrative policy:");

      DefaultPolicyPtr = p = cupsdAddPolicy("default");

      cupsdLogMessage(L_INFO, "<Policy default>");
      cupsdLogMessage(L_INFO, "<Limit Send-Document Send-URI Cancel-Job Hold-Job "
                         "Release-Job Restart-Job Purge-Jobs "
			 "Set-Job-Attributes Create-Job-Subscription "
			 "Renew-Subscription Cancel-Subscription "
			 "Get-Notifications Reprocess-Job Cancel-Current-Job "
			 "Suspend-Current-Job Resume-Job CUPS-Move-Job "
			 "CUPS-Authenticate-Job>");
      cupsdLogMessage(L_INFO, "Order Deny,Allow");

      po = cupsdAddPolicyOp(p, NULL, IPP_SEND_DOCUMENT);
      po->order_type = AUTH_ALLOW;
      po->level      = AUTH_USER;

      cupsdAddName(po, "@OWNER");
      cupsdAddName(po, "@SYSTEM");
      cupsdLogMessage(L_INFO, "Require user @OWNER @SYSTEM");

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

      cupsdLogMessage(L_INFO, "</Limit>");

      cupsdLogMessage(L_INFO, "<Limit Pause-Printer Resume-Printer "
                         "Set-Printer-Attributes Enable-Printer "
			 "Disable-Printer Pause-Printer-After-Current-Job "
			 "Hold-New-Jobs Release-Held-New-Jobs "
			 "Deactivate-Printer Activate-Printer Restart-Printer "
			 "Shutdown-Printer Startup-Printer Promote-Job "
			 "Schedule-Job-After CUPS-Add-Printer "
			 "CUPS-Delete-Printer CUPS-Add-Class CUPS-Delete-Class "
			 "CUPS-Accept-Jobs CUPS-Reject-Jobs CUPS-Set-Default>");
      cupsdLogMessage(L_INFO, "Order Deny,Allow");
      cupsdLogMessage(L_INFO, "AuthType Basic");

      po = cupsdAddPolicyOp(p, NULL, IPP_PAUSE_PRINTER);
      po->order_type = AUTH_ALLOW;
      po->type       = AUTH_BASIC;
      po->level      = AUTH_USER;

      cupsdAddName(po, "@SYSTEM");
      cupsdLogMessage(L_INFO, "Require user @SYSTEM");

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

      cupsdLogMessage(L_INFO, "</Limit>");

      cupsdLogMessage(L_INFO, "<Limit All>");
      cupsdLogMessage(L_INFO, "Order Deny,Allow");

      po = cupsdAddPolicyOp(p, NULL, IPP_ANY_OPERATION);
      po->order_type = AUTH_ALLOW;

      cupsdLogMessage(L_INFO, "</Limit>");
      cupsdLogMessage(L_INFO, "</Policy>");
    }
  }

 /*
  * If we are doing a full reload or the server root has changed, flush
  * the jobs, printers, etc. and start from scratch...
  */

  if (NeedReload == RELOAD_ALL ||
      !old_serverroot || !ServerRoot || strcmp(old_serverroot, ServerRoot) ||
      !old_requestroot || !RequestRoot || strcmp(old_requestroot, RequestRoot))
  {
    cupsdLogMessage(L_INFO, "Full reload is required.");

   /*
    * Free all memory...
    */

    cupsdFreeAllJobs();
    cupsdDeleteAllClasses();
    cupsdDeleteAllPrinters();

    DefaultPrinter = NULL;

    if (MimeDatabase != NULL)
      mimeDelete(MimeDatabase);

    if (NumMimeTypes)
    {
      for (i = 0; i < NumMimeTypes; i ++)
	free((void *)MimeTypes[i]);

      free(MimeTypes);
    }

   /*
    * Read the MIME type and conversion database...
    */

    snprintf(temp, sizeof(temp), "%s/filter", ServerBin);

    MimeDatabase = mimeLoad(ServerRoot, temp);

    if (!MimeDatabase)
    {
      cupsdLogMessage(L_EMERG, "Unable to load MIME database from \'%s\'!",
                 ServerRoot);
      exit(errno);
    }

    cupsdLogMessage(L_INFO, "Loaded MIME database from \'%s\': %d types, %d filters...",
               ServerRoot, MimeDatabase->num_types, MimeDatabase->num_filters);

   /*
    * Create a list of MIME types for the document-format-supported
    * attribute...
    */

    NumMimeTypes = MimeDatabase->num_types;
    if (!mimeType(MimeDatabase, "application", "octet-stream"))
      NumMimeTypes ++;

    MimeTypes = calloc(NumMimeTypes, sizeof(const char *));

    for (i = 0; i < MimeDatabase->num_types; i ++)
    {
      snprintf(type, sizeof(type), "%s/%s", MimeDatabase->types[i]->super,
               MimeDatabase->types[i]->type);

      MimeTypes[i] = strdup(type);
    }

    if (i < NumMimeTypes)
      MimeTypes[i] = strdup("application/octet-stream");

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

    cupsdCreateCommonData();

   /*
    * Load queued jobs...
    */

    cupsdLoadAllJobs();

    cupsdLogMessage(L_INFO, "Full reload complete.");
  }
  else
  {
    cupsdCreateCommonData();

    cupsdLogMessage(L_INFO, "Partial reload complete.");
  }

 /*
  * Reset the reload state...
  */

  NeedReload = RELOAD_NONE;

  cupsdClearString(&old_serverroot);
  cupsdClearString(&old_requestroot);

 /*
  * Startup the server and return...
  */

  cupsdStartServer();

  return (1);
}


/*
 * 'get_address()' - Get an address + port number from a line.
 */

static int				/* O - 1 if address good, 0 if bad */
get_address(const char  *value,		/* I - Value string */
            unsigned    defaddress,	/* I - Default address */
	    int         defport,	/* I - Default port */
	    int         deffamily,	/* I - Default family */
            http_addr_t *address)	/* O - Socket address */
{
  char			hostname[256],	/* Hostname or IP */
			portname[256],	/* Port number or name */
			*ptr;		/* Pointer into hostname string */
  struct hostent	*host;		/* Host address */
  struct servent	*port;		/* Port number */  


 /*
  * Check for an empty value...
  */

  if (!*value)
  {
    cupsdLogMessage(L_ERROR, "Bad (empty) address!");
    return (0);
  }

 /*
  * Initialize the socket address to the defaults...
  */

  memset(address, 0, sizeof(http_addr_t));

#ifdef AF_INET6
  if (deffamily == AF_INET6)
  {
    address->ipv6.sin6_family            = AF_INET6;
    address->ipv6.sin6_addr.s6_addr32[0] = htonl(defaddress);
    address->ipv6.sin6_addr.s6_addr32[1] = htonl(defaddress);
    address->ipv6.sin6_addr.s6_addr32[2] = htonl(defaddress);
    address->ipv6.sin6_addr.s6_addr32[3] = htonl(defaddress);
    address->ipv6.sin6_port              = htons(defport);
  }
  else
#endif /* AF_INET6 */
  {
    address->ipv4.sin_family      = AF_INET;
    address->ipv4.sin_addr.s_addr = htonl(defaddress);
    address->ipv4.sin_port        = htons(defport);
  }

#ifdef AF_LOCAL
 /*
  * If the address starts with a "/", it is a domain socket...
  */

  if (*value == '/')
  {
    if (strlen(value) >= sizeof(address->un.sun_path))
    {
      cupsdLogMessage(L_ERROR, "Domain socket name \"%s\" too long!", value);
      return (0);
    }

    address->un.sun_family = AF_LOCAL;
    strcpy(address->un.sun_path, value);

    return (1);
  }
#endif /* AF_LOCAL */

 /*
  * Try to grab a hostname and port number...
  */

  strlcpy(hostname, value, sizeof(hostname));

  if ((ptr = strrchr(hostname, ':')) != NULL)
  {
   /*
    * Copy hostname and port separately...
    */

    *ptr++ = '\0';

    strlcpy(portname, ptr, sizeof(portname));
  }
  else if (isdigit(value[0] & 255))
  {
   /*
    * Port number...
    */

    hostname[0] = '\0';
    strlcpy(portname, value, sizeof(portname));
  }
  else
  {
   /*
    * Hostname by itself...
    */

    portname[0] = '\0';
  }

 /*
  * Decode the hostname and port number as needed...
  */

  if (hostname[0] && strcmp(hostname, "*"))
  {
    if ((host = httpGetHostByName(hostname)) == NULL)
    {
      cupsdLogMessage(L_ERROR, "httpGetHostByName(\"%s\") failed - %s!", hostname,
                 hstrerror(h_errno));
      return (0);
    }

    httpAddrLoad(host, defport, 0, address);
  }

  if (portname[0] != '\0')
  {
    if (isdigit(portname[0] & 255))
    {
#ifdef AF_INET6
      if (address->addr.sa_family == AF_INET6)
        address->ipv6.sin6_port = htons(atoi(portname));
      else
#endif /* AF_INET6 */
      address->ipv4.sin_port = htons(atoi(portname));
    }
    else
    {
      if ((port = getservbyname(portname, NULL)) == NULL)
      {
        cupsdLogMessage(L_ERROR, "getservbyname(\"%s\") failed - %s!", portname,
                   strerror(errno));
        return (0);
      }
      else
      {
#ifdef AF_INET6
	if (address->addr.sa_family == AF_INET6)
          address->ipv6.sin6_port = htons(port->s_port);
	else
#endif /* AF_INET6 */
	address->ipv4.sin_port = htons(port->s_port);
      }
    }
  }

  return (1);
}


/*
 * 'get_addr_and_mask()' - Get an IP address and netmask.
 */

static int				/* O - 1 on success, 0 on failure */
get_addr_and_mask(const char *value,	/* I - String from config file */
                  unsigned   *ip,	/* O - Address value */
		  unsigned   *mask)	/* O - Mask value */
{
  int		i,			/* Looping var */
		family,			/* Address family */
		ipcount;		/* Count of fields in address */
  const char	*maskval,		/* Pointer to start of mask value */
		*ptr;			/* Pointer into value */
  static unsigned netmasks[4][4] =	/* Standard netmasks... */
  {
    { 0xffffffff, 0x00000000, 0x00000000, 0x00000000 },
    { 0xffffffff, 0xffffffff, 0x00000000, 0x00000000 },
    { 0xffffffff, 0xffffffff, 0xffffffff, 0x00000000 },
    { 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff }
  };


 /*
  * Get the address...
  */

  memset(ip, 0, sizeof(unsigned) * 4);

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

    for (i = 0, ptr = value + 1; *ptr && i < 4; i ++)
    {
      if (*ptr == ']')
        break;
      else if (*ptr == ':')
        ip[i] = 0;
      else
        ip[i] = strtoul(ptr, (char **)&ptr, 16);

      if (*ptr == ':' || *ptr == ']')
        ptr ++;
    }

    ipcount = i;

    if (*ptr && *ptr != '/')
      return (0);
  }
  else
#endif /* AF_INET6 */
  {
   /*
    * Parse dotted-decimal IPv4 address...
    */

    family  = AF_INET;
    ipcount = sscanf(value, "%u.%u.%u.%u", ip + 0, ip + 1, ip + 2, ip + 3);

    ip[3] |= ((((ip[0] << 8) | ip[1]) << 8) | ip[2]) << 8;
    ip[0] = ip[1] = ip[2] = 0;
  }

  if (*maskval)
  {
   /*
    * Get the netmask value(s)...
    */

    memset(mask, 0, sizeof(unsigned) * 4);

#ifdef AF_INET6
    if (maskval[1] == '[')
    {
     /*
      * Get hexadecimal mask value...
      */

      for (i = 0, ptr = maskval + 1; *ptr && i < 4; i ++)
      {
	if (*ptr == ']')
	  break;
	else if (*ptr == ':')
          mask[i] = 0;
	else
          mask[i] = strtoul(ptr, (char **)&ptr, 16);

	if (*ptr == ':' || *ptr == ']')
          ptr ++;
      }

      if (*ptr)
	return (0);
    }
    else
#endif /* AF_INET6 */
    if (strchr(maskval, '.'))
    {
     /*
      * Get dotted-decimal mask...
      */

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
        i = 32 - i;

        mask[0] = 0xffffffff;
        mask[1] = 0xffffffff;
        mask[2] = 0xffffffff;

	if (i > 0)
          mask[3] = (0xffffffff << i) & 0xffffffff;
	else
	  mask[3] = 0xffffffff;
      }
    }
  }
  else
    memcpy(mask, netmasks[ipcount - 1], sizeof(unsigned) * 4);

  cupsdLogMessage(L_DEBUG2, "get_addr_and_mask(value=\"%s\", "
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
parse_aaa(cupsd_location_t *loc,		/* I - Location */
          char       *line,		/* I - Line from file */
	  char       *value,		/* I - Start of value data */
	  int        linenum)		/* I - Current line number */
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
      cupsdLogMessage(L_ERROR, "Encryption value \"%s\" on line %d is invalid in this context. "
	                  "Using \"required\" instead.", value, linenum);

      loc->encryption = HTTP_ENCRYPT_REQUIRED;
    }
    else if (!strcasecmp(value, "required"))
      loc->encryption = HTTP_ENCRYPT_REQUIRED;
    else if (!strcasecmp(value, "ifrequested"))
      loc->encryption = HTTP_ENCRYPT_IF_REQUESTED;
    else
    {
      cupsdLogMessage(L_ERROR, "Unknown Encryption value %s on line %d.",
	         value, linenum);
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
      cupsdLogMessage(L_ERROR, "Unknown Order value %s on line %d.",
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
    else if (value[0] == '*' || value[0] == '.' || !isdigit(value[0] & 255))
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
        cupsdLogMessage(L_ERROR, "Bad netmask value %s on line %d.",
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
      cupsdLogMessage(L_WARN, "Unknown authorization type %s on line %d.",
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
    }
    else if (!strcasecmp(value, "user"))
      loc->level = AUTH_USER;
    else if (!strcasecmp(value, "group"))
      loc->level = AUTH_GROUP;
    else if (!strcasecmp(value, "system"))
    {
      loc->level = AUTH_GROUP;

      cupsdAddName(loc, "@SYSTEM");
    }
    else
    {
      cupsdLogMessage(L_WARN, "Unknown authorization class %s on line %d.",
	         value, linenum);
      return (0);
    }
  }
  else if (!strcasecmp(line, "AuthGroupName"))
    cupsdAddName(loc, value);
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
      cupsdLogMessage(L_WARN, "Unknown Require type %s on line %d.",
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
      cupsdLogMessage(L_WARN, "Unknown Satisfy value %s on line %d.", value,
	         linenum);
      return (0);
    }
  }
  else
    return (0);

  return (1);
}


/*
 * 'read_configuration()' - Read a configuration file.
 */

static int				/* O - 1 on success, 0 on failure */
read_configuration(cups_file_t *fp)	/* I - File to read from */
{
  int		i;			/* Looping var */
  int		linenum;		/* Current line number */
  char		line[HTTP_MAX_BUFFER],	/* Line from file */
		temp[HTTP_MAX_BUFFER],	/* Temporary buffer for value */
		temp2[HTTP_MAX_BUFFER],	/* Temporary buffer 2 for value */
		*ptr,			/* Pointer into line/temp */
		*value;			/* Pointer to value */
  int		valuelen;		/* Length of value */
  var_t		*var;			/* Current variable */
  unsigned	ip[4],			/* Address value */
		mask[4];		/* Netmask value */
  cupsd_dirsvc_relay_t *relay;		/* Relay data */
  cupsd_dirsvc_poll_t	*poll;			/* Polling data */
  http_addr_t	polladdr;		/* Polling address */
  cupsd_location_t	*location;		/* Browse location */
  cups_file_t	*incfile;		/* Include file */
  char		incname[1024];		/* Include filename */
  struct group	*group;			/* Group */


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
        cupsdLogMessage(L_ERROR, "Unable to include config file \"%s\" - %s",
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
        cupsdLogMessage(L_ERROR, "Syntax error on line %d.",
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
        cupsdLogMessage(L_ERROR, "Syntax error on line %d.",
	           linenum);
        return (0);
      }
    }
    else if (!strcasecmp(line, "Port") || !strcasecmp(line, "Listen"))
    {
     /*
      * Add a listening address to the list...
      */

      cupsd_listener_t	*lis;		/* New listeners array */


      if (NumListeners == 0)
        lis = malloc(sizeof(cupsd_listener_t));
      else
        lis = realloc(Listeners, (NumListeners + 1) * sizeof(cupsd_listener_t));

      if (!lis)
      {
        cupsdLogMessage(L_ERROR, "Unable to allocate %s at line %d - %s.",
	           line, linenum, strerror(errno));
        continue;
      }

      Listeners = lis;
      lis      += NumListeners;

      memset(lis, 0, sizeof(cupsd_listener_t));

#if defined(AF_INET6) && !defined(__OpenBSD__)
      if (get_address(value, INADDR_ANY, IPP_PORT, AF_INET6, &(lis->address)))
#else
      if (get_address(value, INADDR_ANY, IPP_PORT, AF_INET, &(lis->address)))
#endif /* AF_INET6  && !__OpenBSD__ */
      {
        httpAddrString(&(lis->address), temp, sizeof(temp));

#ifdef AF_INET6
        if (lis->address.addr.sa_family == AF_INET6)
          cupsdLogMessage(L_INFO, "Listening to %s:%d (IPv6)", temp,
                     ntohs(lis->address.ipv6.sin6_port));
	else
#endif /* AF_INET6 */
#ifdef AF_LOCAL
        if (lis->address.addr.sa_family == AF_LOCAL)
          cupsdLogMessage(L_INFO, "Listening to %s (Domain)", temp);
	else
#endif /* AF_LOCAL */
	  cupsdLogMessage(L_INFO, "Listening to %s:%d", temp,
                     ntohs(lis->address.ipv4.sin_port));

	NumListeners ++;
      }
      else
        cupsdLogMessage(L_ERROR, "Bad %s address %s at line %d.", line,
	           value, linenum);
    }
#ifdef HAVE_SSL
    else if (!strcasecmp(line, "SSLPort") || !strcasecmp(line, "SSLListen"))
    {
     /*
      * Add a listening address to the list...
      */

      cupsd_listener_t	*lis;		/* New listeners array */


      if (NumListeners == 0)
        lis = malloc(sizeof(cupsd_listener_t));
      else
        lis = realloc(Listeners, (NumListeners + 1) * sizeof(cupsd_listener_t));

      if (!lis)
      {
        cupsdLogMessage(L_ERROR, "Unable to allocate %s at line %d - %s.",
	           line, linenum, strerror(errno));
        continue;
      }

      Listeners = lis;
      lis      += NumListeners;

      if (get_address(value, INADDR_ANY, IPP_PORT, AF_INET, &(lis->address)))
      {
        httpAddrString(&(lis->address), temp, sizeof(temp));

#ifdef AF_INET6
        if (lis->address.addr.sa_family == AF_INET6)
          cupsdLogMessage(L_INFO, "Listening to %s:%d (IPv6)", temp,
                     ntohs(lis->address.ipv6.sin6_port));
	else
#endif /* AF_INET6 */
        cupsdLogMessage(L_INFO, "Listening to %s:%d", temp,
                   ntohs(lis->address.ipv4.sin_port));
        lis->encryption = HTTP_ENCRYPT_ALWAYS;
	NumListeners ++;
      }
      else
        cupsdLogMessage(L_ERROR, "Bad %s address %s at line %d.", line,
	           value, linenum);
    }
#endif /* HAVE_SSL */
    else if (!strcasecmp(line, "BrowseAddress"))
    {
     /*
      * Add a browse address to the list...
      */

      cupsd_dirsvc_addr_t	*dira;		/* New browse address array */


      if (NumBrowsers == 0)
        dira = malloc(sizeof(cupsd_dirsvc_addr_t));
      else
        dira = realloc(Browsers, (NumBrowsers + 1) * sizeof(cupsd_dirsvc_addr_t));

      if (!dira)
      {
        cupsdLogMessage(L_ERROR, "Unable to allocate BrowseAddress at line %d - %s.",
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
      else if (get_address(value, INADDR_NONE, BrowsePort, AF_INET, &(dira->to)))
      {
        httpAddrString(&(dira->to), temp, sizeof(temp));

#ifdef AF_INET6
        if (dira->to.addr.sa_family == AF_INET6)
          cupsdLogMessage(L_INFO, "Sending browsing info to %s:%d (IPv6)", temp,
                     ntohs(dira->to.ipv6.sin6_port));
	else
#endif /* AF_INET6 */
        cupsdLogMessage(L_INFO, "Sending browsing info to %s:%d", temp,
                   ntohs(dira->to.ipv4.sin_port));

	NumBrowsers ++;
      }
      else
        cupsdLogMessage(L_ERROR, "Bad BrowseAddress %s at line %d.", value,
	           linenum);
    }
    else if (!strcasecmp(line, "BrowseOrder"))
    {
     /*
      * "BrowseOrder Deny,Allow" or "BrowseOrder Allow,Deny"...
      */

      if ((location = cupsdFindLocation("CUPS_INTERNAL_BROWSE_ACL")) == NULL)
        location = cupsdAddLocation("CUPS_INTERNAL_BROWSE_ACL");

      if (location == NULL)
        cupsdLogMessage(L_ERROR, "Unable to initialize browse access control list!");
      else if (!strncasecmp(value, "deny", 4))
        location->order_type = AUTH_ALLOW;
      else if (!strncasecmp(value, "allow", 5))
        location->order_type = AUTH_DENY;
      else
        cupsdLogMessage(L_ERROR, "Unknown BrowseOrder value %s on line %d.",
	           value, linenum);
    }
    else if (!strcasecmp(line, "BrowseProtocols") ||
             !strcasecmp(line, "BrowseLocalProtocols") ||
             !strcasecmp(line, "BrowseRemoteProtocols"))
    {
     /*
      * "BrowseProtocol name [... name]"
      */

      if (strcasecmp(line, "BrowseLocalProtocols"))
        BrowseRemoteProtocols = 0;
      if (strcasecmp(line, "BrowseRemoteProtocols"))
        BrowseLocalProtocols = 0;

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

        if (!strcasecmp(value, "cups"))
	{
	  if (strcasecmp(line, "BrowseLocalProtocols"))
	    BrowseRemoteProtocols |= BROWSE_CUPS;
	  if (strcasecmp(line, "BrowseRemoteProtocols"))
	    BrowseLocalProtocols |= BROWSE_CUPS;
	}
        else if (!strcasecmp(value, "slp"))
	{
	  if (strcasecmp(line, "BrowseLocalProtocols"))
	    BrowseRemoteProtocols |= BROWSE_SLP;
	  if (strcasecmp(line, "BrowseRemoteProtocols"))
	    BrowseLocalProtocols |= BROWSE_SLP;
	}
        else if (!strcasecmp(value, "ldap"))
	{
	  if (strcasecmp(line, "BrowseLocalProtocols"))
	    BrowseRemoteProtocols |= BROWSE_LDAP;
	  if (strcasecmp(line, "BrowseRemoteProtocols"))
	    BrowseLocalProtocols |= BROWSE_LDAP;
	}
        else if (!strcasecmp(value, "all"))
	{
	  if (strcasecmp(line, "BrowseLocalProtocols"))
	    BrowseRemoteProtocols |= BROWSE_ALL;
	  if (strcasecmp(line, "BrowseRemoteProtocols"))
	    BrowseLocalProtocols |= BROWSE_ALL;
	}
	else
	{
	  cupsdLogMessage(L_ERROR, "Unknown browse protocol \"%s\" on line %d.",
	             value, linenum);
          break;
	}

        for (value += valuelen; *value; value ++)
	  if (!isspace(*value) || *value != ',')
	    break;
      }
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
        cupsdLogMessage(L_ERROR, "Unable to initialize browse access control list!");
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
	else if (value[0] == '*' || value[0] == '.' || !isdigit(value[0]))
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
            cupsdLogMessage(L_ERROR, "Bad netmask value %s on line %d.",
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
        cupsdLogMessage(L_ERROR, "Unable to allocate BrowseRelay at line %d - %s.",
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

      if (value[0] == '*' || value[0] == '.' || !isdigit(value[0]))
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
          cupsdLogMessage(L_ERROR, "Bad netmask value %s on line %d.",
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

      if (get_address(value, INADDR_BROADCAST, BrowsePort, AF_INET, &(relay->to)))
      {
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

#ifdef AF_INET6
        if (relay->to.addr.sa_family == AF_INET6)
          cupsdLogMessage(L_INFO, "Relaying from %s to %s:%d", temp, temp2,
                     ntohs(relay->to.ipv6.sin6_port));
	else
#endif /* AF_INET6 */
        cupsdLogMessage(L_INFO, "Relaying from %s to %s:%d", temp, temp2,
                   ntohs(relay->to.ipv4.sin_port));

	NumRelays ++;
      }
      else
      {
        if (relay->from.type == AUTH_NAME)
	  free(relay->from.mask.name.name);

        cupsdLogMessage(L_ERROR, "Bad relay address %s at line %d.", value, linenum);
      }
    }
    else if (!strcasecmp(line, "BrowsePoll"))
    {
     /*
      * BrowsePoll address[:port]
      */

      if (NumPolled == 0)
        poll = malloc(sizeof(cupsd_dirsvc_poll_t));
      else
        poll = realloc(Polled, (NumPolled + 1) * sizeof(cupsd_dirsvc_poll_t));

      if (!poll)
      {
        cupsdLogMessage(L_ERROR, "Unable to allocate BrowsePoll at line %d - %s.",
	           linenum, strerror(errno));
        continue;
      }

      Polled = poll;
      poll   += NumPolled;

     /*
      * Get poll address and port...
      */

      if (get_address(value, INADDR_NONE, ippPort(), AF_INET, &polladdr))
      {
	NumPolled ++;
	memset(poll, 0, sizeof(cupsd_dirsvc_poll_t));

        httpAddrString(&polladdr, poll->hostname, sizeof(poll->hostname));

#ifdef AF_INET6
        if (polladdr.addr.sa_family == AF_INET6)
          poll->port = ntohs(polladdr.ipv6.sin6_port);
	else
#endif /* AF_INET6 */
        poll->port = ntohs(polladdr.ipv4.sin_port);

        cupsdLogMessage(L_INFO, "Polling %s:%d", poll->hostname, poll->port);
      }
      else
        cupsdLogMessage(L_ERROR, "Bad poll address %s at line %d.", value, linenum);
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
	cupsdLogMessage(L_WARN, "Unknown default authorization type %s on line %d.",
	           value, linenum);
	return (0);
      }
    }
    else if (!strcasecmp(line, "User"))
    {
     /*
      * User ID to run as...
      */

      if (isdigit(value[0]))
        User = atoi(value);
      else
      {
        struct passwd *p;	/* Password information */

        endpwent();
	p = getpwnam(value);

	if (p != NULL)
	  User = p->pw_uid;
	else
	  cupsdLogMessage(L_ERROR, "Unknown User \"%s\" on line %d, ignoring!",
	             value, linenum);
      }
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
	  cupsdLogMessage(L_ERROR, "Unknown Group \"%s\" on line %d, ignoring!",
	             value, linenum);
      }
    }
    else if (!strcasecmp(line, "SystemGroup"))
    {
     /*
      * System (admin) group(s)...
      */

      char	*valueptr,	/* Pointer into value */
		quote;		/* Quote character */


      for (i = NumSystemGroups; *value && i < MAX_SYSTEM_GROUPS;)
      {
        if (*value == '\'' || *value == '\"')
	{
	 /*
	  * Scan quoted name...
	  */

	  quote = *value++;

	  for (valueptr = value; *valueptr; valueptr ++)
	    if (*valueptr == quote)
	      break;
	}
	else
	{
	 /*
	  * Scan space or comma-delimited name...
	  */

          for (valueptr = value; *valueptr; valueptr ++)
	    if (isspace(*valueptr) || *valueptr == ',')
	      break;
        }

        if (*valueptr)
          *valueptr++ = '\0';

        group = getgrnam(value);
        if (group)
	{
          cupsdSetString(SystemGroups + i, value);
	  SystemGroupIDs[i] = group->gr_gid;

	  i ++;
	}
	else
	  cupsdLogMessage(L_ERROR, "Unknown SystemGroup \"%s\" on line %d, ignoring!",
	             value, linenum);

        endgrent();

        value = valueptr;

        while (*value == ',' || isspace(*value))
	  value ++;
      }

      if (i)
        NumSystemGroups = i;
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
	cupsdLogMessage(L_WARN, "Unknown HostNameLookups %s on line %d.",
	           value, linenum);
    }
    else if (!strcasecmp(line, "LogLevel"))
    {
     /*
      * Amount of logging to do...
      */

      if (!strcasecmp(value, "debug2"))
        LogLevel = L_DEBUG2;
      else if (!strcasecmp(value, "debug"))
        LogLevel = L_DEBUG;
      else if (!strcasecmp(value, "info"))
        LogLevel = L_INFO;
      else if (!strcasecmp(value, "notice"))
        LogLevel = L_NOTICE;
      else if (!strcasecmp(value, "warn"))
        LogLevel = L_WARN;
      else if (!strcasecmp(value, "error"))
        LogLevel = L_ERROR;
      else if (!strcasecmp(value, "crit"))
        LogLevel = L_CRIT;
      else if (!strcasecmp(value, "alert"))
        LogLevel = L_ALERT;
      else if (!strcasecmp(value, "emerg"))
        LogLevel = L_EMERG;
      else if (!strcasecmp(value, "none"))
        LogLevel = L_NONE;
      else
        cupsdLogMessage(L_WARN, "Unknown LogLevel %s on line %d.", value, linenum);
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
	cupsdLogMessage(L_WARN, "Unknown PrintcapFormat %s on line %d.",
	           value, linenum);
    }
    else if (!strcasecmp(line, "ServerTokens"))
    {
     /*
      * Set the string used for the Server header...
      */

      struct utsname plat;	      /* Platform info */


      uname(&plat);

      if (!strcasecmp(value, "ProductOnly"))
	cupsdSetString(&ServerHeader, "CUPS");
      else if (!strcasecmp(value, "Major"))
	cupsdSetString(&ServerHeader, "CUPS/1");
      else if (!strcasecmp(value, "Minor"))
	cupsdSetString(&ServerHeader, "CUPS/1.1");
      else if (!strcasecmp(value, "Minimal"))
	cupsdSetString(&ServerHeader, CUPS_MINIMAL);
      else if (!strcasecmp(value, "OS"))
	cupsdSetStringf(&ServerHeader, CUPS_MINIMAL " (%s)", plat.sysname);
      else if (!strcasecmp(value, "Full"))
	cupsdSetStringf(&ServerHeader, CUPS_MINIMAL " (%s) IPP/1.1", plat.sysname);
      else if (!strcasecmp(value, "None"))
	cupsdClearString(&ServerHeader);
      else
	cupsdLogMessage(L_WARN, "Unknown ServerTokens %s on line %d.", value, linenum);
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

        cupsdLogMessage(L_ERROR, "Unknown directive %s on line %d.", line,
	           linenum);
        continue;
      }

      switch (var->type)
      {
        case VAR_INTEGER :
	    {
	      int	n;	/* Number */
	      char	*units;	/* Units */


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

	case VAR_BOOLEAN :
	    if (!strcasecmp(value, "true") ||
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
              cupsdLogMessage(L_ERROR, "Unknown boolean value %s on line %d.",
	                 value, linenum);
	    break;

	case VAR_STRING :
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
  cupsd_location_t	*loc,			/* New location */
		*parent;		/* Parent location */
  char		line[HTTP_MAX_BUFFER],	/* Line buffer */
		*value,			/* Value for directive */
		*valptr;		/* Pointer into value */


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
        cupsdLogMessage(L_ERROR, "Syntax error on line %d.", linenum);
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
	  cupsdLogMessage(L_WARN, "Unknown request type %s on line %d!", value,
	             linenum);

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
      cupsdLogMessage(L_ERROR, "Unknown Location directive %s on line %d.",
	         line, linenum);
      return (0);
    }
  }

  cupsdLogMessage(L_ERROR, "Unexpected end-of-file at line %d while reading location!",
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
  cupsd_location_t		*op;		/* Policy operation */
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
        cupsdLogMessage(L_WARN, "Missing </Limit> before </Policy> on line %d!",
	           linenum);

      return (linenum);
    }
    else if (!strcasecmp(line, "<Limit") && !op)
    {
      if (!value)
      {
        cupsdLogMessage(L_ERROR, "Syntax error on line %d.", linenum);
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
	    cupsdLogMessage(L_ERROR, "Bad IPP operation name \"%s\" on line %d!",
	               value, linenum);
          else
	    num_ops ++;
	}
	else
	  cupsdLogMessage(L_ERROR, "Too many operations listed on line %d!",
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
      cupsdLogMessage(L_ERROR, "Missing <Limit ops> directive before %s on line %d.",
                 line, linenum);
      return (0);
    }
    else if (!parse_aaa(op, line, value, linenum))
    {
      if (op)
	cupsdLogMessage(L_ERROR, "Unknown Policy Limit directive %s on line %d.",
	           line, linenum);
      else
	cupsdLogMessage(L_ERROR, "Unknown Policy directive %s on line %d.",
	           line, linenum);

      return (0);
    }
  }

  cupsdLogMessage(L_ERROR, "Unexpected end-of-file at line %d while reading policy \"%s\"!",
             linenum, policy);

  return (0);
}


#ifdef HAVE_CDSASSL
/*
 * 'CDSAGetServerCerts()' - Convert a keychain name into the CFArrayRef
 *                          required by SSLSetCertificate.
 *
 * For now we assumes that there is exactly one SecIdentity in the
 * keychain - i.e. there is exactly one matching cert/private key pair.
 * In the future we will search a keychain for a SecIdentity matching a
 * specific criteria.  We also skip the operation of adding additional
 * non-signing certs from the keychain to the CFArrayRef.
 *
 * To create a self-signed certificate for testing use the certtool.
 * Executing the following as root will do it:
 *
 *     certtool c c v k=CUPS
 */

CFArrayRef
CDSAGetServerCerts(void)
{
  OSStatus		err;		/* Error info */
  SecKeychainRef 	kcRef;		/* Keychain reference */
  SecIdentitySearchRef	srchRef;	/* Search reference */
  SecIdentityRef	identity;	/* Identity */
  CFArrayRef		ca;		/* Certificate array */


  kcRef    = NULL;
  srchRef  = NULL;
  identity = NULL;
  ca       = NULL;
  err      = SecKeychainOpen(ServerCertificate, &kcRef);

  if (err)
    cupsdLogMessage(L_ERROR, "Cannot open keychain \"%s\", error %d.",
               ServerCertificate, err);
  else
  {
   /*
    * Search for "any" identity matching specified key use; 
    * in this app, we expect there to be exactly one. 
    */

    err = SecIdentitySearchCreate(kcRef, CSSM_KEYUSE_SIGN, &srchRef);

    if (err)
      cupsdLogMessage(L_ERROR,
                 "Cannot find signing key in keychain \"%s\", error %d",
                 ServerCertificate, err);
    else
    {
      err = SecIdentitySearchCopyNext(srchRef, &identity);

      if (err)
	cupsdLogMessage(L_ERROR,
	           "Cannot find signing key in keychain \"%s\", error %d",
	           ServerCertificate, err);
      else
      {
	if (CFGetTypeID(identity) != SecIdentityGetTypeID())
	  cupsdLogMessage(L_ERROR, "SecIdentitySearchCopyNext CFTypeID failure!");
	else
	{
	 /* 
	  * Found one. Place it in a CFArray. 
	  * TBD: snag other (non-identity) certs from keychain and add them
	  * to array as well.
	  */

	  ca = CFArrayCreate(NULL, (const void **)&identity, 1, NULL);

	  if (ca == nil)
	    cupsdLogMessage(L_ERROR, "CFArrayCreate error");
	}

	/*CFRelease(identity);*/
      }

      /*CFRelease(srchRef);*/
    }

    /*CFRelease(kcRef);*/
  }

  return ca;
}
#endif /* HAVE_CDSASSL */


/*
 * End of "$Id$".
 */
