/*
 * "$Id: conf.c,v 1.56 2000/09/06 18:31:30 mike Exp $"
 *
 *   Configuration routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2000 by Easy Software Products, all rights reserved.
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
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 *   ReadConfiguration()  - Read the cupsd.conf file.
 *   read_configuration() - Read a configuration file.
 *   read_location()      - Read a <Location path> definition.
 *   get_address()        - Get an address + port number from a line.
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"
#include <stdarg.h>
#include <pwd.h>
#include <grp.h>
#include <sys/resource.h>

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
  int	type,		/* Type (int, string, address) */
	size;		/* Size of string */
} var_t;

#define VAR_INTEGER	0
#define VAR_STRING	1
#define VAR_BOOLEAN	2


/*
 * Local globals...
 */

static var_t	variables[] =
{
  { "ServerName",	ServerName,		VAR_STRING,	sizeof(ServerName) },
  { "ServerAdmin",	ServerAdmin,		VAR_STRING,	sizeof(ServerAdmin) },
  { "ServerRoot",	ServerRoot,		VAR_STRING,	sizeof(ServerRoot) },
  { "ServerBin",	ServerBin,		VAR_STRING,	sizeof(ServerBin) },
  { "DocumentRoot",	DocumentRoot,		VAR_STRING,	sizeof(DocumentRoot) },
  { "RequestRoot",	RequestRoot,		VAR_STRING,	sizeof(RequestRoot) },
  { "SystemGroup",	SystemGroup,		VAR_STRING,	sizeof(SystemGroup) },
  { "AccessLog",	AccessLog,		VAR_STRING,	sizeof(AccessLog) },
  { "ErrorLog",		ErrorLog,		VAR_STRING,	sizeof(ErrorLog) },
  { "PageLog",		PageLog,		VAR_STRING,	sizeof(PageLog) },
  { "DataDir",		DataDir,		VAR_STRING,	sizeof(DataDir) },
  { "DefaultCharset",	DefaultCharset,		VAR_STRING,	sizeof(DefaultCharset) },
  { "DefaultLanguage",	DefaultLanguage,	VAR_STRING,	sizeof(DefaultLanguage) },
  { "RIPCache",		RIPCache,		VAR_STRING,	sizeof(RIPCache) },
  { "TempDir",		TempDir,		VAR_STRING,	sizeof(TempDir) },
  { "Printcap",		Printcap,		VAR_STRING,	sizeof(Printcap) },
  { "FontPath",		FontPath,		VAR_STRING,	sizeof(FontPath) },
  { "RemoteRoot",	RemoteRoot,		VAR_STRING,	sizeof(RemoteRoot) },
  { "HostNameLookups",	&HostNameLookups,	VAR_BOOLEAN,	0 },
  { "Timeout",		&Timeout,		VAR_INTEGER,	0 },
  { "KeepAlive",	&KeepAlive,		VAR_BOOLEAN,	0 },
  { "KeepAliveTimeout",	&KeepAliveTimeout,	VAR_INTEGER,	0 },
  { "ImplicitClasses",	&ImplicitClasses,	VAR_BOOLEAN,	0 },
  { "Browsing",		&Browsing,		VAR_BOOLEAN,	0 },
  { "BrowseShortNames",	&BrowseShortNames,	VAR_BOOLEAN,	0 },
  { "BrowsePort",	&BrowsePort,		VAR_INTEGER,	0 },
  { "BrowseInterval",	&BrowseInterval,	VAR_INTEGER,	0 },
  { "BrowseTimeout",	&BrowseTimeout,		VAR_INTEGER,	0 },
  { "MaxClients",	&MaxClients,		VAR_INTEGER,	0 },
  { "MaxLogSize",	&MaxLogSize,		VAR_INTEGER,	0 },
  { "MaxRequestSize",	&MaxRequestSize,	VAR_INTEGER,	0 },
  { "PreserveJobHistory", &JobHistory,		VAR_BOOLEAN,	0 },
  { "PreserveJobFiles",	&JobFiles,		VAR_BOOLEAN,	0 },
  { "FilterLimit",	&FilterLimit,		VAR_INTEGER,	0 }
};
#define NUM_VARS	(sizeof(variables) / sizeof(variables[0]))


/*
 * Local functions...
 */

static int	read_configuration(FILE *fp);
static int	read_location(FILE *fp, char *name, int linenum);
static int	get_address(char *value, unsigned defaddress, int defport,
		            struct sockaddr_in *address);


/*
 * 'ReadConfiguration()' - Read the cupsd.conf file.
 */

int				/* O - 1 if file read successfully, 0 otherwise */
ReadConfiguration(void)
{
  int		i;		/* Looping var */
  FILE		*fp;		/* Configuration file */
  int		status;		/* Return status */
  char		directory[1024];/* Configuration directory */
  struct rlimit	limit;		/* Runtime limit */
  char		*language;	/* Language string */
  struct passwd	*user;		/* Default user */
  struct group	*group;		/* Default group */


 /*
  * Close all network clients and stop all jobs...
  */

  CloseAllClients();
  StopListening();
  StopPolling();
  StopBrowsing();

  if (Clients != NULL)
  {
    free(Clients);
    Clients = NULL;
  }

  StopAllJobs();

  if (AccessFile != NULL)
  {
    fclose(AccessFile);

    AccessFile = NULL;
  }

  if (ErrorFile != NULL)
  {
    fclose(ErrorFile);

    ErrorFile = NULL;
  }

  if (PageFile != NULL)
  {
    fclose(PageFile);

    PageFile = NULL;
  }

 /*
  * Clear the current configuration...
  */

  NeedReload = FALSE;

  FD_ZERO(&InputSet);
  FD_ZERO(&OutputSet);

  DeleteAllLocations();

  DeleteAllClasses();

  gethostname(ServerName, sizeof(ServerName));
  snprintf(ServerAdmin, sizeof(ServerAdmin), "root@%s", ServerName);
  strcpy(ServerRoot, CUPS_SERVERROOT);
  strcpy(ServerBin, CUPS_SERVERBIN);
  strcpy(RequestRoot, CUPS_REQUESTS);
  strcpy(DocumentRoot, CUPS_DOCROOT);
  strcpy(DataDir, CUPS_DATADIR);
  strcpy(AccessLog, CUPS_LOGDIR "/access_log");
  strcpy(ErrorLog, CUPS_LOGDIR "/error_log");
  strcpy(PageLog, CUPS_LOGDIR "/page_log");
  strcpy(Printcap, "");
  strcpy(FontPath, CUPS_FONTPATH);
  strcpy(RemoteRoot, "remroot");

  if ((language = DEFAULT_LANGUAGE) == NULL)
    language = "en";
  else if (strcmp(language, "C") == 0 || strcmp(language, "POSIX") == 0)
    language = "en";

  strncpy(DefaultLanguage, language, sizeof(DefaultLanguage) - 1);
  DefaultLanguage[sizeof(DefaultLanguage) - 1] = '\0';

  strcpy(DefaultCharset, DEFAULT_CHARSET);
  strcpy(RIPCache, "8m");
  if (getenv("TMPDIR") == NULL)
    strcpy(TempDir, CUPS_REQUESTS "/tmp");
  else
  {
    strncpy(TempDir, getenv("TMPDIR"), sizeof(TempDir) - 1);
    TempDir[sizeof(TempDir) - 1] = '\0';
  }

  if (Devices)
  {
    ippDelete(Devices);
    Devices = NULL;
  }

  if (PPDs)
  {
    ippDelete(PPDs);
    PPDs = NULL;
  }

 /*
  * Find the default system group: "sys", "system", or "root"...
  */

  group = getgrnam("sys");
  endgrent();

  if (group != NULL)
  {
    strcpy(SystemGroup, "sys");
    Group = group->gr_gid;
  }
  else
  {
    group = getgrnam("system");
    endgrent();

    if (group != NULL)
    {
      strcpy(SystemGroup, "system");
      Group = group->gr_gid;
    }
    else
    {
      group = getgrnam("root");
      endgrent();

      if (group != NULL)
      {
	strcpy(SystemGroup, "root");
	Group = group->gr_gid;
      }
      else
      {
	strcpy(SystemGroup, "unknown");
	Group = 0;
      }
    }
  }

 /*
  * Find the default user...
  */

  if ((user = getpwnam("lp")) == NULL)
    User = 1;	/* Force to a non-priviledged account */
  else
    User = user->pw_uid;

  endpwent();

  LogLevel         = L_ERROR;
  HostNameLookups  = FALSE;
  Timeout          = DEFAULT_TIMEOUT;
  KeepAlive        = TRUE;
  KeepAliveTimeout = DEFAULT_KEEPALIVE;
  ImplicitClasses  = TRUE;
  FilterLimit      = 0;
  FilterLevel      = 0;
  MaxClients       = 100;
  MaxLogSize       = 1024 * 1024;
  MaxRequestSize   = 0;

  for (i = 0; i < NumRelays; i ++)
    if (Relays[i].from.type == AUTH_NAME)
      free(Relays[i].from.mask.name.name);

  Browsing         = TRUE;
  BrowseShortNames = TRUE;
  BrowsePort       = ippPort();
  BrowseInterval   = DEFAULT_INTERVAL;
  BrowseTimeout    = DEFAULT_TIMEOUT;
  NumBrowsers      = 0;
  NumRelays        = 0;
  NumPolled        = 0;

  NumListeners     = 0;

  DefaultPrinter   = NULL;

  DeleteAllPrinters();

  if (MimeDatabase != NULL)
    mimeDelete(MimeDatabase);

  JobHistory = DEFAULT_HISTORY;
  JobFiles   = DEFAULT_FILES;

  if ((fp = fopen(ConfigurationFile, "r")) == NULL)
    return (0);

  status = read_configuration(fp);

  fclose(fp);

  BrowseACL = FindLocation("CUPS_INTERNAL_BROWSE_ACL");

  if (!status)
    return (0);

  if (DocumentRoot[0] != '/')
  {
    snprintf(directory, sizeof(directory), "%s/%s", ServerRoot, DocumentRoot);
    strcpy(DocumentRoot, directory);
  }

  if (RequestRoot[0] != '/')
  {
    snprintf(directory, sizeof(directory), "%s/%s", ServerRoot, RequestRoot);
    strcpy(RequestRoot, directory);
  }

  if (ServerBin[0] != '/')
  {
    snprintf(directory, sizeof(directory), "%s/%s", ServerRoot, ServerBin);
    strcpy(ServerBin, directory);
  }

#ifdef HAVE_VSYSLOG
  if (strcmp(AccessLog, "syslog") == 0 ||
      strcmp(ErrorLog, "syslog") == 0 ||
      strcmp(PageLog, "syslog") == 0)
    openlog("cupsd", LOG_PID | LOG_NOWAIT | LOG_NDELAY, LOG_LPR);
#endif /* HAVE_VSYSLOG */

  LogMessage(L_DEBUG, "ReadConfiguration() ConfigurationFile=\"%s\"",
             ConfigurationFile);

 /*
  * Make sure the request and temporary directories have the right
  * permissions...
  */

  chown(RequestRoot, User, Group);
  chmod(RequestRoot, 0700);

  if (strncmp(TempDir, RequestRoot, strlen(RequestRoot)) == 0)
  {
    chown(TempDir, User, Group);
    chmod(TempDir, 01700);
  }

 /*
  * Check the MaxClients setting, and then allocate memory for it...
  */

  getrlimit(RLIMIT_NOFILE, &limit);

  if (MaxClients > (limit.rlim_max / 3))
    MaxClients = limit.rlim_max / 3;

  if ((Clients = calloc(sizeof(client_t), MaxClients)) == NULL)
  {
    LogMessage(L_ERROR, "ReadConfiguration() FATAL: unable to allocate memory for %d clients!",
               MaxClients);
    exit(1);
  }
  else
    LogMessage(L_INFO, "ReadConfiguration() Configured for up to %d clients.",
               MaxClients);

 /*
  * Read the MIME type and conversion database...
  */

  MimeDatabase = mimeNew();
  mimeMerge(MimeDatabase, ServerRoot);

 /*
  * Load banners...
  */

  snprintf(directory, sizeof(directory), "%s/banners", DataDir);
  LoadBanners(directory);

 /*
  * Load printers and classes...
  */

  LoadAllPrinters();
  LoadAllClasses();

 /*
  * Load devices and PPDs...
  */

  snprintf(directory, sizeof(directory), "%s/model", DataDir);
  LoadPPDs(directory);

  snprintf(directory, sizeof(directory), "%s/backend", ServerBin);
  LoadDevices(directory);

 /*
  * Add a default browser if browsing is enabled and no browser addresses
  * were defined...
  */

  if (Browsing && NumBrowsers == 0)
  {
    NumBrowsers ++;

    memset(Browsers + 0, 0, sizeof(Browsers[0]));
    Browsers[0].sin_addr.s_addr = htonl(INADDR_BROADCAST);
    Browsers[0].sin_family      = AF_INET;
    Browsers[0].sin_port        = htons(BrowsePort);
  }

 /*
  * Startup all the networking stuff...
  */

  StartListening();
  StartBrowsing();
  StartPolling();

 /*
  * Check for queued jobs...
  */

  CheckJobs();

  return (1);
}


/*
 * 'read_configuration()' - Read a configuration file.
 */

static int				/* O - 1 on success, 0 on failure */
read_configuration(FILE *fp)		/* I - File to read from */
{
  int		i;			/* Looping var */
  int		linenum;		/* Current line number */
  int		len;			/* Length of line */
  char		line[HTTP_MAX_BUFFER],	/* Line from file */
		name[256],		/* Parameter name */
		*nameptr,		/* Pointer into name */
		*value;			/* Pointer to value */
  var_t		*var;			/* Current variable */
  unsigned	address,		/* Address value */
		netmask;		/* Netmask value */
  int		ip[4],			/* IP address components */
		ipcount,		/* Number of components provided */
 		mask[4];		/* IP netmask components */
  dirsvc_relay_t *relay;		/* Relay data */
  dirsvc_poll_t	*poll;			/* Polling data */
  struct sockaddr_in polladdr;		/* Polling address */
  location_t	*location;		/* Browse location */
  static unsigned netmasks[4] =		/* Standard netmasks... */
  {
    0xff000000,
    0xffff0000,
    0xffffff00,
    0xffffffff
  };


 /*
  * Loop through each line in the file...
  */

  linenum = 0;

  while (fgets(line, sizeof(line), fp) != NULL)
  {
    linenum ++;

   /*
    * Skip comment lines...
    */

    if (line[0] == '#')
      continue;

   /*
    * Strip trailing newline, if any...
    */

    len = strlen(line);

    if (line[len - 1] == '\n')
    {
      len --;
      line[len] = '\0';
    }

   /*
    * Extract the name from the beginning of the line...
    */

    for (value = line; isspace(*value); value ++);

    for (nameptr = name; *value != '\0' && !isspace(*value);)
      *nameptr++ = *value++;
    *nameptr = '\0';

    while (isspace(*value))
      value ++;

    if (name[0] == '\0')
      continue;

   /*
    * Decode the directive...
    */

    if (strcmp(name, "<Location") == 0)
    {
     /*
      * <Location path>
      */

      if (line[len - 1] == '>')
      {
        line[len - 1] = '\0';

	linenum = read_location(fp, value, linenum);
	if (linenum == 0)
	  return (0);
      }
      else
      {
        LogMessage(L_ERROR, "ReadConfiguration() Syntax error on line %d.",
	           linenum);
        return (0);
      }
    }
    else if (strcmp(name, "Port") == 0 ||
             strcmp(name, "Listen") == 0)
    {
     /*
      * Add a listening address to the list...
      */

      if (NumListeners < MAX_BROWSERS)
      {
        if (get_address(value, INADDR_ANY, IPP_PORT,
	                &(Listeners[NumListeners].address)))
        {
          LogMessage(L_INFO, "Listening to %x:%d",
                     ntohl(Listeners[NumListeners].address.sin_addr.s_addr),
                     ntohs(Listeners[NumListeners].address.sin_port));
	  NumListeners ++;
        }
	else
          LogMessage(L_ERROR, "Bad %s address %s at line %d.", name,
	             value, linenum);
      }
      else
        LogMessage(L_WARN, "Too many %s directives at line %d.", name,
	           linenum);
    }
    else if (strcmp(name, "BrowseAddress") == 0)
    {
     /*
      * Add a browse address to the list...
      */

      if (NumBrowsers < MAX_BROWSERS)
      {
        if (get_address(value, INADDR_NONE, BrowsePort, Browsers + NumBrowsers))
        {
          LogMessage(L_INFO, "Sending browsing info to %x:%d",
                     ntohl(Browsers[NumBrowsers].sin_addr.s_addr),
                     ntohs(Browsers[NumBrowsers].sin_port));
	  NumBrowsers ++;
        }
	else
          LogMessage(L_ERROR, "Bad BrowseAddress %s at line %d.", value,
	             linenum);
      }
      else
        LogMessage(L_WARN, "Too many BrowseAddress directives at line %d.",
	           linenum);
    }
    else if (strcmp(name, "BrowseOrder") == 0)
    {
     /*
      * "BrowseOrder Deny,Allow" or "BrowseOrder Allow,Deny"...
      */

      if ((location = FindLocation("CUPS_INTERNAL_BROWSE_ACL")) == NULL)
        location = AddLocation("CUPS_INTERNAL_BROWSE_ACL");

      if (location == NULL)
        LogMessage(L_ERROR, "Unable to initialize browse access control list!");
      else if (strncasecmp(value, "deny", 4) == 0)
        location->order_type = AUTH_ALLOW;
      else if (strncasecmp(value, "allow", 5) == 0)
        location->order_type = AUTH_DENY;
      else
        LogMessage(L_ERROR, "Unknown BrowseOrder value %s on line %d.",
	           value, linenum);
    }
    else if (strcmp(name, "BrowseAllow") == 0 ||
             strcmp(name, "BrowseDeny") == 0)
    {
     /*
      * BrowseAllow [From] host/ip...
      * BrowseDeny [From] host/ip...
      */

      if ((location = FindLocation("CUPS_INTERNAL_BROWSE_ACL")) == NULL)
        location = AddLocation("CUPS_INTERNAL_BROWSE_ACL");

      if (location == NULL)
        LogMessage(L_ERROR, "Unable to initialize browse access control list!");
      else
      {
	if (strncasecmp(value, "from ", 5) == 0)
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

	if (strcasecmp(value, "all") == 0)
	{
	 /*
          * All hosts...
	  */

          if (strcmp(name, "BrowseAllow") == 0)
	    AllowIP(location, 0, 0);
	  else
	    DenyIP(location, 0, 0);
	}
	else if (strcasecmp(value, "none") == 0)
	{
	 /*
          * No hosts...
	  */

          if (strcmp(name, "BrowseAllow") == 0)
	    AllowIP(location, ~0, 0);
	  else
	    DenyIP(location, ~0, 0);
	}
	else if (value[0] == '*' || value[0] == '.' || !isdigit(value[0]))
	{
	 /*
          * Host or domain name...
	  */

	  if (value[0] == '*')
	    value ++;

          if (strcmp(name, "BrowseAllow") == 0)
	    AllowHost(location, value);
	  else
	    DenyHost(location, value);
	}
	else
	{
	 /*
          * One of many IP address forms...
	  */

          memset(ip, 0, sizeof(ip));
          ipcount = sscanf(value, "%d.%d.%d.%d", ip + 0, ip + 1, ip + 2, ip + 3);
	  address = (((((ip[0] << 8) | ip[1]) << 8) | ip[2]) << 8) | ip[3];

          if ((value = strchr(value, '/')) != NULL)
	  {
	    value ++;
	    memset(mask, 0, sizeof(mask));
            switch (sscanf(value, "%d.%d.%d.%d", mask + 0, mask + 1,
	                   mask + 2, mask + 3))
	    {
	      case 1 :
	          netmask = (0xffffffff << (32 - mask[0])) & 0xffffffff;
	          break;
	      case 4 :
	          netmask = (((((mask[0] << 8) | mask[1]) << 8) |
		              mask[2]) << 8) | mask[3];
                  break;
	      default :
        	  LogMessage(L_ERROR, "Bad netmask value %s on line %d.",
	        	     value, linenum);
		  netmask = 0xffffffff;
		  break;
	    }
	  }
	  else
	    netmask = netmasks[ipcount - 1];

          if (strcmp(name, "BrowseAllow") == 0)
	    AllowIP(location, address, netmask);
	  else
	    DenyIP(location, address, netmask);
	}
      }
    }
    else if (strcmp(name, "BrowseRelay") == 0)
    {
     /*
      * BrowseRelay [from] source [to] destination
      */

      if (NumRelays >= MAX_BROWSERS)
      {
        LogMessage(L_WARN, "Too many BrowseRelay directives at line %d.",
	           linenum);
        continue;
      }

      relay = Relays + NumRelays;

      memset(relay, 0, sizeof(dirsvc_relay_t));

      if (strncasecmp(value, "from ", 5) == 0)
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

        relay->from.type             = AUTH_NAME;
	relay->from.mask.name.name   = strdup(value);
	relay->from.mask.name.length = strlen(value);
      }
      else
      {
       /*
        * One of many IP address forms...
	*/

        memset(ip, 0, sizeof(ip));
        ipcount = sscanf(value, "%d.%d.%d.%d", ip + 0, ip + 1, ip + 2, ip + 3);
	address = (((((ip[0] << 8) | ip[1]) << 8) | ip[2]) << 8) | ip[3];

        for (; *value; value ++)
	  if (*value == '/' || isspace(*value))
	    break;

        if (*value == '/')
	{
	  value ++;
	  memset(mask, 0, sizeof(mask));
          switch (sscanf(value, "%d.%d.%d.%d", mask + 0, mask + 1,
	                 mask + 2, mask + 3))
	  {
	    case 1 :
	        netmask = (0xffffffff << (32 - mask[0])) & 0xffffffff;
	        break;
	    case 4 :
	        netmask = (((((mask[0] << 8) | mask[1]) << 8) |
		            mask[2]) << 8) | mask[3];
                break;
	    default :
        	LogMessage(L_ERROR, "Bad netmask value %s on line %d.",
	        	   value, linenum);
		netmask = 0xffffffff;
		break;
	  }
	}
	else
	  netmask = netmasks[ipcount - 1];

        relay->from.type            = AUTH_IP;
	relay->from.mask.ip.address = address;
	relay->from.mask.ip.netmask = netmask;
      }

     /*
      * Skip value and trailing whitespace...
      */

      for (; *value; value ++)
	if (isspace(*value))
	  break;

      while (isspace(*value))
        value ++;

      if (strncasecmp(value, "to ", 3) == 0)
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

      if (get_address(value, INADDR_BROADCAST, BrowsePort, &(relay->to)))
      {
        if (relay->from.type == AUTH_NAME)
          LogMessage(L_INFO, "Relaying from %s to %x:%d",
                     ntohl(relay->to.sin_addr.s_addr),
                     ntohs(relay->to.sin_port));
        else
          LogMessage(L_INFO, "Relaying from %x/%x to %x:%d",
                     relay->from.mask.ip.address, relay->from.mask.ip.netmask,
                     ntohl(relay->to.sin_addr.s_addr),
                     ntohs(relay->to.sin_port));

	NumRelays ++;
      }
      else
      {
        if (relay->from.type == AUTH_NAME)
	  free(relay->from.mask.name.name);

        LogMessage(L_ERROR, "Bad relay address %s at line %d.", value, linenum);
      }
    }
    else if (strcmp(name, "BrowsePoll") == 0)
    {
     /*
      * BrowsePoll address[:port]
      */

      if (NumPolled >= MAX_BROWSERS)
      {
        LogMessage(L_WARN, "Too many BrowsePoll directives at line %d.",
	           linenum);
        continue;
      }

     /*
      * Get poll address and port...
      */

      if (get_address(value, INADDR_NONE, ippPort(), &polladdr))
      {
        LogMessage(L_INFO, "Polling %x:%d", ntohl(polladdr.sin_addr.s_addr),
                   ntohs(polladdr.sin_port));

        poll = Polled + NumPolled;
	NumPolled ++;
	memset(poll, 0, sizeof(dirsvc_poll_t));

        address = ntohl(polladdr.sin_addr.s_addr);

	sprintf(poll->hostname, "%d.%d.%d.%d", address >> 24,
	        (address >> 16) & 255, (address >> 8) & 255, address & 255);
        poll->port = ntohs(polladdr.sin_port);
      }
      else
        LogMessage(L_ERROR, "Bad poll address %s at line %d.", value, linenum);
    }
    else if (strcmp(name, "User") == 0)
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
	  LogMessage(L_WARN, "ReadConfiguration() Unknown username \"%s\"",
	             value);
      }
    }
    else if (strcmp(name, "Group") == 0)
    {
     /*
      * Group ID to run as...
      */

      if (isdigit(value[0]))
        Group = atoi(value);
      else
      {
        struct group *g;	/* Group information */

        endgrent();
	g = getgrnam(value);

	if (g != NULL)
	  Group = g->gr_gid;
	else
	  LogMessage(L_WARN, "ReadConfiguration() Unknown groupname \"%s\"",
	             value);
      }
    }
    else if (strcmp(name, "LogLevel") == 0)
    {
     /*
      * Amount of logging to do...
      */

      if (strcmp(value, "debug") == 0)
        LogLevel = L_DEBUG;
      else if (strcmp(value, "info") == 0)
        LogLevel = L_INFO;
      else if (strcmp(value, "warn") == 0)
        LogLevel = L_WARN;
      else if (strcmp(value, "error") == 0)
        LogLevel = L_ERROR;
      else if (strcmp(value, "none") == 0)
        LogLevel = L_NONE;
    }
    else
    {
     /*
      * Find a simple variable in the list...
      */

      for (i = NUM_VARS, var = variables; i > 0; i --, var ++)
        if (strcmp(name, var->name) == 0)
	  break;

      if (i == 0)
      {
       /*
        * Unknown directive!  Output an error message and continue...
	*/

        LogMessage(L_ERROR, "Unknown directive %s on line %d.", name,
	           linenum);
        continue;
      }

      switch (var->type)
      {
        case VAR_INTEGER :
	    *((int *)var->ptr) = atoi(value);
	    break;

	case VAR_BOOLEAN :
	    if (strcasecmp(value, "true") == 0 ||
	        strcasecmp(value, "on") == 0 ||
		strcasecmp(value, "enabled") == 0 ||
		strcasecmp(value, "yes") == 0 ||
		atoi(value) != 0)
              *((int *)var->ptr) = TRUE;
	    else if (strcasecmp(value, "false") == 0 ||
	             strcasecmp(value, "off") == 0 ||
		     strcasecmp(value, "disabled") == 0 ||
		     strcasecmp(value, "no") == 0 ||
		     strcasecmp(value, "0") == 0)
              *((int *)var->ptr) = FALSE;
	    else
              LogMessage(L_ERROR, "Unknown boolean value %s on line %d.",
	                 value, linenum);
	    break;

	case VAR_STRING :
	    strncpy((char *)var->ptr, value, var->size - 1);
	    value[var->size - 1] = '\0';
	    break;
      }
    }
  }

  return (1);
}


/*
 * 'read_location()' - Read a <Location path> definition.
 */

static int			/* O - New line number or 0 on error */
read_location(FILE *fp,		/* I - Configuration file */
              char *location,	/* I - Location name/path */
	      int  linenum)	/* I - Current line number */
{
  location_t	*loc;			/* New location */
  int		len;			/* Length of line */
  char		line[HTTP_MAX_BUFFER],	/* Line buffer */
		name[256],		/* Configuration directive */
		*nameptr,		/* Pointer into name */
		*value;			/* Value for directive */
  unsigned	address,		/* Address value */
		netmask;		/* Netmask value */
  int		ip[4],			/* IP address components */
		ipcount,		/* Number of components provided */
 		mask[4];		/* IP netmask components */
  static unsigned	netmasks[4] =	/* Standard netmasks... */
  {
    0xff000000,
    0xffff0000,
    0xffffff00,
    0xffffffff
  };


  if ((loc = AddLocation(location)) == NULL)
    return (0);

  while (fgets(line, sizeof(line), fp) != NULL)
  {
    linenum ++;

   /*
    * Skip comment lines...
    */

    if (line[0] == '#')
      continue;

   /*
    * Strip trailing newline, if any...
    */

    len = strlen(line);

    if (line[len - 1] == '\n')
    {
      len --;
      line[len] = '\0';
    }

   /*
    * Extract the name from the beginning of the line...
    */

    for (value = line; isspace(*value); value ++);

    for (nameptr = name; *value != '\0' && !isspace(*value);)
      *nameptr++ = *value++;
    *nameptr = '\0';

    while (isspace(*value))
      value ++;

    if (name[0] == '\0')
      continue;

   /*
    * Decode the directive...
    */

    if (strcmp(name, "</Location>") == 0)
      return (linenum);
    else if (strcmp(name, "Order") == 0)
    {
     /*
      * "Order Deny,Allow" or "Order Allow,Deny"...
      */

      if (strncasecmp(value, "deny", 4) == 0)
        loc->order_type = AUTH_ALLOW;
      else if (strncasecmp(value, "allow", 5) == 0)
        loc->order_type = AUTH_DENY;
      else
        LogMessage(L_ERROR, "Unknown Order value %s on line %d.",
	           value, linenum);
    }
    else if (strcmp(name, "Allow") == 0 ||
             strcmp(name, "Deny") == 0)
    {
     /*
      * Allow [From] host/ip...
      * Deny [From] host/ip...
      */

      if (strncasecmp(value, "from", 4) == 0)
      {
       /*
        * Strip leading "from"...
	*/

	value += 4;

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

      if (strcasecmp(value, "all") == 0)
      {
       /*
        * All hosts...
	*/

        if (strcmp(name, "Allow") == 0)
	  AllowIP(loc, 0, 0);
	else
	  DenyIP(loc, 0, 0);
      }
      else  if (strcasecmp(value, "none") == 0)
      {
       /*
        * No hosts...
	*/

        if (strcmp(name, "Allow") == 0)
	  AllowIP(loc, ~0, 0);
	else
	  DenyIP(loc, ~0, 0);
      }
      else if (value[0] == '*' || value[0] == '.' || !isdigit(value[0]))
      {
       /*
        * Host or domain name...
	*/

	if (value[0] == '*')
	  value ++;

        if (strcmp(name, "Allow") == 0)
	  AllowHost(loc, value);
	else
	  DenyHost(loc, value);
      }
      else
      {
       /*
        * One of many IP address forms...
	*/

        memset(ip, 0, sizeof(ip));
        ipcount = sscanf(value, "%d.%d.%d.%d", ip + 0, ip + 1, ip + 2, ip + 3);
	address = (((((ip[0] << 8) | ip[1]) << 8) | ip[2]) << 8) | ip[3];

        if ((value = strchr(value, '/')) != NULL)
	{
	  value ++;
	  memset(mask, 0, sizeof(mask));
          switch (sscanf(value, "%d.%d.%d.%d", mask + 0, mask + 1,
	                 mask + 2, mask + 3))
	  {
	    case 1 :
	        netmask = (0xffffffff << (32 - mask[0])) & 0xffffffff;
	        break;
	    case 4 :
	        netmask = (((((mask[0] << 8) | mask[1]) << 8) |
		            mask[2]) << 8) | mask[3];
                break;
	    default :
        	LogMessage(L_ERROR, "Bad netmask value %s on line %d.",
	        	   value, linenum);
		netmask = 0xffffffff;
		break;
	  }
	}
	else
	  netmask = netmasks[ipcount - 1];

        if (strcmp(name, "Allow") == 0)
	  AllowIP(loc, address, netmask);
	else
	  DenyIP(loc, address, netmask);
      }
    }
    else if (strcmp(name, "AuthType") == 0)
    {
     /*
      * AuthType {none,basic,digest}
      */

      if (strcasecmp(value, "none") == 0)
      {
	loc->type  = AUTH_NONE;
	loc->level = AUTH_ANON;
      }
      else if (strcasecmp(value, "basic") == 0)
      {
	loc->type = AUTH_BASIC;

        if (loc->level == AUTH_ANON)
	  loc->level = AUTH_USER;
      }
      else if (strcasecmp(value, "digest") == 0)
      {
	loc->type = AUTH_DIGEST;

        if (loc->level == AUTH_ANON)
	  loc->level = AUTH_USER;
      }
      else
        LogMessage(L_WARN, "Unknown authorization type %s on line %d.",
	           value, linenum);
    }
    else if (strcmp(name, "AuthClass") == 0)
    {
     /*
      * AuthClass anonymous, user, system, group
      */

      if (strcasecmp(value, "anonymous") == 0)
      {
        loc->type  = AUTH_NONE;
        loc->level = AUTH_ANON;
      }
      else if (strcasecmp(value, "user") == 0)
        loc->level = AUTH_USER;
      else if (strcasecmp(value, "group") == 0)
        loc->level = AUTH_GROUP;
      else if (strcasecmp(value, "system") == 0)
      {
        loc->level = AUTH_GROUP;
	strcpy(loc->group_name, SystemGroup);
      }
      else
        LogMessage(L_WARN, "Unknown authorization class %s on line %d.",
	           value, linenum);
    }
    else if (strcmp(name, "AuthGroupName") == 0)
      strncpy(loc->group_name, value, sizeof(loc->group_name) - 1);
    else
      LogMessage(L_ERROR, "Unknown Location directive %s on line %d.",
	         name, linenum);
  }

  return (0);
}


/*
 * 'get_address()' - Get an address + port number from a line.
 */

static int					/* O - 1 if address good, 0 if bad */
get_address(char               *value,		/* I - Value string */
            unsigned           defaddress,	/* I - Default address */
	    int                defport,		/* I - Default port */
            struct sockaddr_in *address)	/* O - Socket address */
{
  char			hostname[256],		/* Hostname or IP */
			portname[256];		/* Port number or name */
  struct hostent	*host;			/* Host address */
  struct servent	*port;			/* Port number */  


 /*
  * Initialize the socket address to the defaults...
  */

  memset(address, 0, sizeof(struct sockaddr_in));
  address->sin_family      = AF_INET;
  address->sin_addr.s_addr = htonl(defaddress);
  address->sin_port        = htons(defport);

 /*
  * Try to grab a hostname and port number...
  */

  switch (sscanf(value, "%255[^:]:%255s", hostname, portname))
  {
    case 1 :
        if (strchr(hostname, '.') == NULL && defaddress == INADDR_ANY)
	{
	 /*
	  * Hostname is a port number...
	  */

	  strcpy(portname, hostname);
	  hostname[0] = '\0';
	}
        else
          portname[0] = '\0';
        break;
    case 2 :
        break;
    default :
	LogMessage(L_ERROR, "Unable to decode address \"%s\"!", value);
        return (0);
  }

 /*
  * Decode the hostname and port number as needed...
  */

  if (hostname[0] != '\0')
  {
    if (isdigit(hostname[0]))
      address->sin_addr.s_addr = inet_addr(hostname);
    else
    {
      if ((host = gethostbyname(hostname)) == NULL)
      {
        LogMessage(L_ERROR, "gethostbyname(\"%s\") failed - %s!", hostname,
                   strerror(errno));
        return (0);
      }

      memcpy(&(address->sin_addr), host->h_addr, host->h_length);
      address->sin_port = htons(defport);
    }
  }

  if (portname[0] != '\0')
  {
    if (isdigit(portname[0]))
      address->sin_port = htons(atoi(portname));
    else
    {
      if ((port = getservbyname(portname, NULL)) == NULL)
      {
        LogMessage(L_ERROR, "getservbyname(\"%s\") failed - %s!", portname,
                   strerror(errno));
        return (0);
      }
      else
        address->sin_port = htons(port->s_port);
    }
  }

  return (1);
}


/*
 * End of "$Id: conf.c,v 1.56 2000/09/06 18:31:30 mike Exp $".
 */
