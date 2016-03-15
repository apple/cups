/*
 * "$Id: ippfind.c 12639 2015-05-19 02:36:30Z msweet $"
 *
 * Utility to find IPP printers via Bonjour/DNS-SD and optionally run
 * commands such as IPP and Bonjour conformance tests.  This tool is
 * inspired by the UNIX "find" command, thus its name.
 *
 * Copyright 2008-2015 by Apple Inc.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * This file is subject to the Apple OS-Developed Software exception.
 */

/*
 * Include necessary headers.
 */

#define _CUPS_NO_DEPRECATED
#include <cups/cups-private.h>
#ifdef WIN32
#  include <process.h>
#  include <sys/timeb.h>
#else
#  include <sys/wait.h>
#endif /* WIN32 */
#include <regex.h>
#ifdef HAVE_DNSSD
#  include <dns_sd.h>
#elif defined(HAVE_AVAHI)
#  include <avahi-client/client.h>
#  include <avahi-client/lookup.h>
#  include <avahi-common/simple-watch.h>
#  include <avahi-common/domain.h>
#  include <avahi-common/error.h>
#  include <avahi-common/malloc.h>
#  define kDNSServiceMaxDomainName AVAHI_DOMAIN_NAME_MAX
#endif /* HAVE_DNSSD */

#ifndef WIN32
extern char **environ;			/* Process environment variables */
#endif /* !WIN32 */


/*
 * Structures...
 */

typedef enum ippfind_exit_e		/* Exit codes */
{
  IPPFIND_EXIT_TRUE = 0,		/* OK and result is true */
  IPPFIND_EXIT_FALSE,			/* OK but result is false*/
  IPPFIND_EXIT_BONJOUR,			/* Browse/resolve failure */
  IPPFIND_EXIT_SYNTAX,			/* Bad option or syntax error */
  IPPFIND_EXIT_MEMORY			/* Out of memory */
} ippfind_exit_t;

typedef enum ippfind_op_e		/* Operations for expressions */
{
  /* "Evaluation" operations */
  IPPFIND_OP_NONE,			/* No operation */
  IPPFIND_OP_AND,			/* Logical AND of all children */
  IPPFIND_OP_OR,			/* Logical OR of all children */
  IPPFIND_OP_TRUE,			/* Always true */
  IPPFIND_OP_FALSE,			/* Always false */
  IPPFIND_OP_IS_LOCAL,			/* Is a local service */
  IPPFIND_OP_IS_REMOTE,			/* Is a remote service */
  IPPFIND_OP_DOMAIN_REGEX,		/* Domain matches regular expression */
  IPPFIND_OP_NAME_REGEX,		/* Name matches regular expression */
  IPPFIND_OP_HOST_REGEX,		/* Hostname matches regular expression */
  IPPFIND_OP_PORT_RANGE,		/* Port matches range */
  IPPFIND_OP_PATH_REGEX,		/* Path matches regular expression */
  IPPFIND_OP_TXT_EXISTS,		/* TXT record key exists */
  IPPFIND_OP_TXT_REGEX,			/* TXT record key matches regular expression */
  IPPFIND_OP_URI_REGEX,			/* URI matches regular expression */

  /* "Output" operations */
  IPPFIND_OP_EXEC,			/* Execute when true */
  IPPFIND_OP_LIST,			/* List when true */
  IPPFIND_OP_PRINT_NAME,		/* Print URI when true */
  IPPFIND_OP_PRINT_URI,			/* Print name when true */
  IPPFIND_OP_QUIET			/* No output when true */
} ippfind_op_t;

typedef struct ippfind_expr_s		/* Expression */
{
  struct ippfind_expr_s
		*prev,			/* Previous expression */
		*next,			/* Next expression */
		*parent,		/* Parent expressions */
		*child;			/* Child expressions */
  ippfind_op_t	op;			/* Operation code (see above) */
  int		invert;			/* Invert the result */
  char		*key;			/* TXT record key */
  regex_t	re;			/* Regular expression for matching */
  int		range[2];		/* Port number range */
  int		num_args;		/* Number of arguments for exec */
  char		**args;			/* Arguments for exec */
} ippfind_expr_t;

typedef struct ippfind_srv_s		/* Service information */
{
#ifdef HAVE_DNSSD
  DNSServiceRef	ref;			/* Service reference for query */
#elif defined(HAVE_AVAHI)
  AvahiServiceResolver *ref;		/* Resolver */
#endif /* HAVE_DNSSD */
  char		*name,			/* Service name */
		*domain,		/* Domain name */
		*regtype,		/* Registration type */
		*fullName,		/* Full name */
		*host,			/* Hostname */
		*resource,		/* Resource path */
		*uri;			/* URI */
  int		num_txt;		/* Number of TXT record keys */
  cups_option_t	*txt;			/* TXT record keys */
  int		port,			/* Port number */
		is_local,		/* Is a local service? */
		is_processed,		/* Did we process the service? */
		is_resolved;		/* Got the resolve data? */
} ippfind_srv_t;


/*
 * Local globals...
 */

#ifdef HAVE_DNSSD
static DNSServiceRef dnssd_ref;		/* Master service reference */
#elif defined(HAVE_AVAHI)
static AvahiClient *avahi_client = NULL;/* Client information */
static int	avahi_got_data = 0;	/* Got data from poll? */
static AvahiSimplePoll *avahi_poll = NULL;
					/* Poll information */
#endif /* HAVE_DNSSD */

static int	address_family = AF_UNSPEC;
					/* Address family for LIST */
static int	bonjour_error = 0;	/* Error browsing/resolving? */
static double	bonjour_timeout = 1.0;	/* Timeout in seconds */
static int	ipp_version = 20;	/* IPP version for LIST */


/*
 * Local functions...
 */

#ifdef HAVE_DNSSD
static void DNSSD_API	browse_callback(DNSServiceRef sdRef,
			                DNSServiceFlags flags,
				        uint32_t interfaceIndex,
				        DNSServiceErrorType errorCode,
				        const char *serviceName,
				        const char *regtype,
				        const char *replyDomain, void *context)
					__attribute__((nonnull(1,5,6,7,8)));
static void DNSSD_API	browse_local_callback(DNSServiceRef sdRef,
					      DNSServiceFlags flags,
					      uint32_t interfaceIndex,
					      DNSServiceErrorType errorCode,
					      const char *serviceName,
					      const char *regtype,
					      const char *replyDomain,
					      void *context)
					      __attribute__((nonnull(1,5,6,7,8)));
#elif defined(HAVE_AVAHI)
static void		browse_callback(AvahiServiceBrowser *browser,
					AvahiIfIndex interface,
					AvahiProtocol protocol,
					AvahiBrowserEvent event,
					const char *serviceName,
					const char *regtype,
					const char *replyDomain,
					AvahiLookupResultFlags flags,
					void *context);
static void		client_callback(AvahiClient *client,
					AvahiClientState state,
					void *context);
#endif /* HAVE_AVAHI */

static int		compare_services(ippfind_srv_t *a, ippfind_srv_t *b);
static const char	*dnssd_error_string(int error);
static int		eval_expr(ippfind_srv_t *service,
			          ippfind_expr_t *expressions);
static int		exec_program(ippfind_srv_t *service, int num_args,
			             char **args);
static ippfind_srv_t	*get_service(cups_array_t *services,
				     const char *serviceName,
				     const char *regtype,
				     const char *replyDomain)
				     __attribute__((nonnull(1,2,3,4)));
static double		get_time(void);
static int		list_service(ippfind_srv_t *service);
static ippfind_expr_t	*new_expr(ippfind_op_t op, int invert,
			          const char *value, const char *regex,
			          char **args);
#ifdef HAVE_DNSSD
static void DNSSD_API	resolve_callback(DNSServiceRef sdRef,
			                 DNSServiceFlags flags,
				         uint32_t interfaceIndex,
				         DNSServiceErrorType errorCode,
				         const char *fullName,
				         const char *hostTarget, uint16_t port,
				         uint16_t txtLen,
				         const unsigned char *txtRecord,
				         void *context)
				         __attribute__((nonnull(1,5,6,9, 10)));
#elif defined(HAVE_AVAHI)
static int		poll_callback(struct pollfd *pollfds,
			              unsigned int num_pollfds, int timeout,
			              void *context);
static void		resolve_callback(AvahiServiceResolver *res,
					 AvahiIfIndex interface,
					 AvahiProtocol protocol,
					 AvahiResolverEvent event,
					 const char *serviceName,
					 const char *regtype,
					 const char *replyDomain,
					 const char *host_name,
					 const AvahiAddress *address,
					 uint16_t port,
					 AvahiStringList *txt,
					 AvahiLookupResultFlags flags,
					 void *context);
#endif /* HAVE_DNSSD */
static void		set_service_uri(ippfind_srv_t *service);
static void		show_usage(void) __attribute__((noreturn));
static void		show_version(void) __attribute__((noreturn));


/*
 * 'main()' - Browse for printers.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  int			i,		/* Looping var */
			have_output = 0,/* Have output expression */
			status = IPPFIND_EXIT_FALSE;
					/* Exit status */
  const char		*opt,		/* Option character */
			*search;	/* Current browse/resolve string */
  cups_array_t		*searches;	/* Things to browse/resolve */
  cups_array_t		*services;	/* Service array */
  ippfind_srv_t		*service;	/* Current service */
  ippfind_expr_t	*expressions = NULL,
					/* Expression tree */
			*temp = NULL,	/* New expression */
			*parent = NULL,	/* Parent expression */
			*current = NULL,/* Current expression */
			*parens[100];	/* Markers for parenthesis */
  int			num_parens = 0;	/* Number of parenthesis */
  ippfind_op_t		logic = IPPFIND_OP_AND;
					/* Logic for next expression */
  int			invert = 0;	/* Invert expression? */
  int			err;		/* DNS-SD error */
#ifdef HAVE_DNSSD
  fd_set		sinput;		/* Input set for select() */
  struct timeval	stimeout;	/* Timeout for select() */
#endif /* HAVE_DNSSD */
  double		endtime;	/* End time */
  static const char * const ops[] =	/* Node operation names */
  {
    "NONE",
    "AND",
    "OR",
    "TRUE",
    "FALSE",
    "IS_LOCAL",
    "IS_REMOTE",
    "DOMAIN_REGEX",
    "NAME_REGEX",
    "HOST_REGEX",
    "PORT_RANGE",
    "PATH_REGEX",
    "TXT_EXISTS",
    "TXT_REGEX",
    "URI_REGEX",
    "EXEC",
    "LIST",
    "PRINT_NAME",
    "PRINT_URI",
    "QUIET"
  };


 /*
  * Initialize the locale...
  */

  _cupsSetLocale(argv);

 /*
  * Create arrays to track services and things we want to browse/resolve...
  */

  searches = cupsArrayNew(NULL, NULL);
  services = cupsArrayNew((cups_array_func_t)compare_services, NULL);

 /*
  * Parse command-line...
  */

  if (getenv("IPPFIND_DEBUG"))
    for (i = 1; i < argc; i ++)
      fprintf(stderr, "argv[%d]=\"%s\"\n", i, argv[i]);

  for (i = 1; i < argc; i ++)
  {
    if (argv[i][0] == '-')
    {
      if (argv[i][1] == '-')
      {
       /*
        * Parse --option options...
        */

        if (!strcmp(argv[i], "--and"))
        {
          if (logic == IPPFIND_OP_OR)
          {
            _cupsLangPuts(stderr, _("ippfind: Cannot use --and after --or."));
            show_usage();
          }

          if (!current)
          {
            _cupsLangPuts(stderr,
                          _("ippfind: Missing expression before \"--and\"."));
            show_usage();
          }

	  temp = NULL;
        }
        else if (!strcmp(argv[i], "--domain"))
        {
          i ++;
          if (i >= argc)
          {
            _cupsLangPrintf(stderr,
                            _("ippfind: Missing regular expression after %s."),
                            "--domain");
            show_usage();
          }

          if ((temp = new_expr(IPPFIND_OP_DOMAIN_REGEX, invert, NULL, argv[i],
                               NULL)) == NULL)
            return (IPPFIND_EXIT_MEMORY);
        }
        else if (!strcmp(argv[i], "--exec"))
        {
          i ++;
          if (i >= argc)
          {
            _cupsLangPrintf(stderr, _("ippfind: Expected program after %s."),
                            "--exec");
            show_usage();
          }

          if ((temp = new_expr(IPPFIND_OP_EXEC, invert, NULL, NULL,
                               argv + i)) == NULL)
            return (IPPFIND_EXIT_MEMORY);

          while (i < argc)
            if (!strcmp(argv[i], ";"))
              break;
            else
              i ++;

          if (i >= argc)
          {
            _cupsLangPrintf(stderr, _("ippfind: Expected semi-colon after %s."),
                            "--exec");
            show_usage();
          }

          have_output = 1;
        }
        else if (!strcmp(argv[i], "--false"))
        {
          if ((temp = new_expr(IPPFIND_OP_FALSE, invert, NULL, NULL,
                               NULL)) == NULL)
            return (IPPFIND_EXIT_MEMORY);
        }
        else if (!strcmp(argv[i], "--help"))
        {
          show_usage();
        }
        else if (!strcmp(argv[i], "--host"))
        {
          i ++;
          if (i >= argc)
          {
            _cupsLangPrintf(stderr,
                            _("ippfind: Missing regular expression after %s."),
                            "--host");
            show_usage();
          }

          if ((temp = new_expr(IPPFIND_OP_HOST_REGEX, invert, NULL, argv[i],
                               NULL)) == NULL)
            return (IPPFIND_EXIT_MEMORY);
        }
        else if (!strcmp(argv[i], "--ls"))
        {
          if ((temp = new_expr(IPPFIND_OP_LIST, invert, NULL, NULL,
                               NULL)) == NULL)
            return (IPPFIND_EXIT_MEMORY);

          have_output = 1;
        }
        else if (!strcmp(argv[i], "--local"))
        {
          if ((temp = new_expr(IPPFIND_OP_IS_LOCAL, invert, NULL, NULL,
                               NULL)) == NULL)
            return (IPPFIND_EXIT_MEMORY);
        }
        else if (!strcmp(argv[i], "--name"))
        {
          i ++;
          if (i >= argc)
          {
            _cupsLangPrintf(stderr,
                            _("ippfind: Missing regular expression after %s."),
                            "--name");
            show_usage();
          }

          if ((temp = new_expr(IPPFIND_OP_NAME_REGEX, invert, NULL, argv[i],
                               NULL)) == NULL)
            return (IPPFIND_EXIT_MEMORY);
        }
        else if (!strcmp(argv[i], "--not"))
        {
          invert = 1;
        }
        else if (!strcmp(argv[i], "--or"))
        {
          if (!current)
          {
            _cupsLangPuts(stderr,
                          _("ippfind: Missing expression before \"--or\"."));
            show_usage();
          }

          logic = IPPFIND_OP_OR;

          if (parent && parent->op == IPPFIND_OP_OR)
          {
           /*
            * Already setup to do "foo --or bar --or baz"...
            */

            temp = NULL;
          }
          else if (!current->prev && parent)
          {
           /*
            * Change parent node into an OR node...
            */

            parent->op = IPPFIND_OP_OR;
            temp       = NULL;
          }
          else if (!current->prev)
          {
           /*
            * Need to group "current" in a new OR node...
            */

	    if ((temp = new_expr(IPPFIND_OP_OR, 0, NULL, NULL,
				 NULL)) == NULL)
	      return (IPPFIND_EXIT_MEMORY);

            temp->parent    = parent;
            temp->child     = current;
            current->parent = temp;

            if (parent)
              parent->child = temp;
            else
              expressions = temp;

	    parent = temp;
	    temp   = NULL;
	  }
	  else
	  {
	   /*
	    * Need to group previous expressions in an AND node, and then
	    * put that in an OR node...
	    */

	    if ((temp = new_expr(IPPFIND_OP_AND, 0, NULL, NULL,
				 NULL)) == NULL)
	      return (IPPFIND_EXIT_MEMORY);

	    while (current->prev)
	    {
	      current->parent = temp;
	      current         = current->prev;
	    }

	    current->parent = temp;
	    temp->child     = current;
	    current         = temp;

	    if ((temp = new_expr(IPPFIND_OP_OR, 0, NULL, NULL,
				 NULL)) == NULL)
	      return (IPPFIND_EXIT_MEMORY);

            temp->parent    = parent;
            current->parent = temp;

            if (parent)
              parent->child = temp;
            else
              expressions = temp;

	    parent = temp;
	    temp   = NULL;
	  }
        }
        else if (!strcmp(argv[i], "--path"))
        {
          i ++;
          if (i >= argc)
          {
            _cupsLangPrintf(stderr,
                            _("ippfind: Missing regular expression after %s."),
                            "--path");
            show_usage();
          }

          if ((temp = new_expr(IPPFIND_OP_PATH_REGEX, invert, NULL, argv[i],
                               NULL)) == NULL)
            return (IPPFIND_EXIT_MEMORY);
        }
        else if (!strcmp(argv[i], "--port"))
        {
          i ++;
          if (i >= argc)
          {
            _cupsLangPrintf(stderr,
                            _("ippfind: Expected port range after %s."),
                            "--port");
            show_usage();
          }

          if ((temp = new_expr(IPPFIND_OP_PORT_RANGE, invert, argv[i], NULL,
                               NULL)) == NULL)
            return (IPPFIND_EXIT_MEMORY);
        }
        else if (!strcmp(argv[i], "--print"))
        {
          if ((temp = new_expr(IPPFIND_OP_PRINT_URI, invert, NULL, NULL,
                               NULL)) == NULL)
            return (IPPFIND_EXIT_MEMORY);

          have_output = 1;
        }
        else if (!strcmp(argv[i], "--print-name"))
        {
          if ((temp = new_expr(IPPFIND_OP_PRINT_NAME, invert, NULL, NULL,
                               NULL)) == NULL)
            return (IPPFIND_EXIT_MEMORY);

          have_output = 1;
        }
        else if (!strcmp(argv[i], "--quiet"))
        {
          if ((temp = new_expr(IPPFIND_OP_QUIET, invert, NULL, NULL,
                               NULL)) == NULL)
            return (IPPFIND_EXIT_MEMORY);

          have_output = 1;
        }
        else if (!strcmp(argv[i], "--remote"))
        {
          if ((temp = new_expr(IPPFIND_OP_IS_REMOTE, invert, NULL, NULL,
                               NULL)) == NULL)
            return (IPPFIND_EXIT_MEMORY);
        }
        else if (!strcmp(argv[i], "--true"))
        {
          if ((temp = new_expr(IPPFIND_OP_TRUE, invert, NULL, argv[i],
                               NULL)) == NULL)
            return (IPPFIND_EXIT_MEMORY);
        }
        else if (!strcmp(argv[i], "--txt"))
        {
          i ++;
          if (i >= argc)
          {
            _cupsLangPrintf(stderr, _("ippfind: Expected key name after %s."),
                            "--txt");
            show_usage();
          }

          if ((temp = new_expr(IPPFIND_OP_TXT_EXISTS, invert, argv[i], NULL,
                               NULL)) == NULL)
            return (IPPFIND_EXIT_MEMORY);
        }
        else if (!strncmp(argv[i], "--txt-", 6))
        {
          const char *key = argv[i] + 6;/* TXT key */

          i ++;
          if (i >= argc)
          {
            _cupsLangPrintf(stderr,
                            _("ippfind: Missing regular expression after %s."),
                            argv[i - 1]);
            show_usage();
          }

          if ((temp = new_expr(IPPFIND_OP_TXT_REGEX, invert, key, argv[i],
                               NULL)) == NULL)
            return (IPPFIND_EXIT_MEMORY);
        }
        else if (!strcmp(argv[i], "--uri"))
        {
          i ++;
          if (i >= argc)
          {
            _cupsLangPrintf(stderr,
                            _("ippfind: Missing regular expression after %s."),
                            "--uri");
            show_usage();
          }

          if ((temp = new_expr(IPPFIND_OP_URI_REGEX, invert, NULL, argv[i],
                               NULL)) == NULL)
            return (IPPFIND_EXIT_MEMORY);
        }
        else if (!strcmp(argv[i], "--version"))
        {
          show_version();
        }
        else
        {
	  _cupsLangPrintf(stderr, _("%s: Unknown option \"%s\"."),
			  "ippfind", argv[i]);
	  show_usage();
	}

        if (temp)
        {
         /*
          * Add new expression...
          */

	  if (logic == IPPFIND_OP_AND &&
	      current && current->prev &&
	      parent && parent->op != IPPFIND_OP_AND)
          {
           /*
            * Need to re-group "current" in a new AND node...
            */

            ippfind_expr_t *tempand;	/* Temporary AND node */

	    if ((tempand = new_expr(IPPFIND_OP_AND, 0, NULL, NULL,
				    NULL)) == NULL)
	      return (IPPFIND_EXIT_MEMORY);

           /*
            * Replace "current" with new AND node at the end of this list...
            */

            current->prev->next = tempand;
            tempand->prev       = current->prev;
            tempand->parent     = parent;

           /*
            * Add "current to the new AND node...
            */

            tempand->child  = current;
            current->parent = tempand;
            current->prev   = NULL;
	    parent          = tempand;
	  }

         /*
          * Add the new node at current level...
          */

	  temp->parent = parent;
	  temp->prev   = current;

	  if (current)
	    current->next = temp;
	  else if (parent)
	    parent->child = temp;
	  else
	    expressions = temp;

	  current = temp;
          invert  = 0;
          logic   = IPPFIND_OP_AND;
          temp    = NULL;
        }
      }
      else
      {
       /*
        * Parse -o options
        */

        for (opt = argv[i] + 1; *opt; opt ++)
        {
          switch (*opt)
          {
            case '4' :
                address_family = AF_INET;
                break;

            case '6' :
                address_family = AF_INET6;
                break;

            case 'P' :
		i ++;
		if (i >= argc)
		{
		  _cupsLangPrintf(stderr,
				  _("ippfind: Expected port range after %s."),
				  "-P");
		  show_usage();
		}

		if ((temp = new_expr(IPPFIND_OP_PORT_RANGE, invert, argv[i],
		                     NULL, NULL)) == NULL)
		  return (IPPFIND_EXIT_MEMORY);
		break;

            case 'T' :
                i ++;
                if (i >= argc)
		{
		  _cupsLangPrintf(stderr,
				  _("%s: Missing timeout for \"-T\"."),
				  "ippfind");
		  show_usage();
		}

                bonjour_timeout = atof(argv[i]);
                break;

            case 'V' :
                i ++;
                if (i >= argc)
		{
		  _cupsLangPrintf(stderr,
				  _("%s: Missing version for \"-V\"."),
				  "ippfind");
		  show_usage();
		}

                if (!strcmp(argv[i], "1.1"))
                  ipp_version = 11;
                else if (!strcmp(argv[i], "2.0"))
                  ipp_version = 20;
                else if (!strcmp(argv[i], "2.1"))
                  ipp_version = 21;
                else if (!strcmp(argv[i], "2.2"))
                  ipp_version = 22;
                else
                {
                  _cupsLangPrintf(stderr, _("%s: Bad version %s for \"-V\"."),
                                  "ippfind", argv[i]);
                  show_usage();
                }
                break;

            case 'd' :
		i ++;
		if (i >= argc)
		{
		  _cupsLangPrintf(stderr,
				  _("ippfind: Missing regular expression after "
				    "%s."), "-d");
		  show_usage();
		}

		if ((temp = new_expr(IPPFIND_OP_DOMAIN_REGEX, invert, NULL,
		                     argv[i], NULL)) == NULL)
		  return (IPPFIND_EXIT_MEMORY);
                break;

            case 'h' :
		i ++;
		if (i >= argc)
		{
		  _cupsLangPrintf(stderr,
				  _("ippfind: Missing regular expression after "
				    "%s."), "-h");
		  show_usage();
		}

		if ((temp = new_expr(IPPFIND_OP_HOST_REGEX, invert, NULL,
		                     argv[i], NULL)) == NULL)
		  return (IPPFIND_EXIT_MEMORY);
                break;

            case 'l' :
		if ((temp = new_expr(IPPFIND_OP_LIST, invert, NULL, NULL,
				     NULL)) == NULL)
		  return (IPPFIND_EXIT_MEMORY);

		have_output = 1;
                break;

            case 'n' :
		i ++;
		if (i >= argc)
		{
		  _cupsLangPrintf(stderr,
				  _("ippfind: Missing regular expression after "
				    "%s."), "-n");
		  show_usage();
		}

		if ((temp = new_expr(IPPFIND_OP_NAME_REGEX, invert, NULL,
		                     argv[i], NULL)) == NULL)
		  return (IPPFIND_EXIT_MEMORY);
                break;

            case 'p' :
		if ((temp = new_expr(IPPFIND_OP_PRINT_URI, invert, NULL, NULL,
				     NULL)) == NULL)
		  return (IPPFIND_EXIT_MEMORY);

		have_output = 1;
                break;

            case 'q' :
		if ((temp = new_expr(IPPFIND_OP_QUIET, invert, NULL, NULL,
				     NULL)) == NULL)
		  return (IPPFIND_EXIT_MEMORY);

		have_output = 1;
                break;

            case 'r' :
		if ((temp = new_expr(IPPFIND_OP_IS_REMOTE, invert, NULL, NULL,
				     NULL)) == NULL)
		  return (IPPFIND_EXIT_MEMORY);
                break;

            case 's' :
		if ((temp = new_expr(IPPFIND_OP_PRINT_NAME, invert, NULL, NULL,
				     NULL)) == NULL)
		  return (IPPFIND_EXIT_MEMORY);

		have_output = 1;
                break;

            case 't' :
		i ++;
		if (i >= argc)
		{
		  _cupsLangPrintf(stderr,
				  _("ippfind: Missing key name after %s."),
				  "-t");
		  show_usage();
		}

		if ((temp = new_expr(IPPFIND_OP_TXT_EXISTS, invert, argv[i],
		                     NULL, NULL)) == NULL)
		  return (IPPFIND_EXIT_MEMORY);
                break;

            case 'u' :
		i ++;
		if (i >= argc)
		{
		  _cupsLangPrintf(stderr,
				  _("ippfind: Missing regular expression after "
				    "%s."), "-u");
		  show_usage();
		}

		if ((temp = new_expr(IPPFIND_OP_URI_REGEX, invert, NULL,
		                     argv[i], NULL)) == NULL)
		  return (IPPFIND_EXIT_MEMORY);
                break;

            case 'x' :
		i ++;
		if (i >= argc)
		{
		  _cupsLangPrintf(stderr,
				  _("ippfind: Missing program after %s."),
				  "-x");
		  show_usage();
		}

		if ((temp = new_expr(IPPFIND_OP_EXEC, invert, NULL, NULL,
				     argv + i)) == NULL)
		  return (IPPFIND_EXIT_MEMORY);

		while (i < argc)
		  if (!strcmp(argv[i], ";"))
		    break;
		  else
		    i ++;

		if (i >= argc)
		{
		  _cupsLangPrintf(stderr,
				  _("ippfind: Missing semi-colon after %s."),
				  "-x");
		  show_usage();
		}

		have_output = 1;
                break;

            default :
                _cupsLangPrintf(stderr, _("%s: Unknown option \"-%c\"."),
                                "ippfind", *opt);
                show_usage();
          }

	  if (temp)
	  {
	   /*
	    * Add new expression...
	    */

	    if (logic == IPPFIND_OP_AND &&
	        current && current->prev &&
	        parent && parent->op != IPPFIND_OP_AND)
	    {
	     /*
	      * Need to re-group "current" in a new AND node...
	      */

	      ippfind_expr_t *tempand;	/* Temporary AND node */

	      if ((tempand = new_expr(IPPFIND_OP_AND, 0, NULL, NULL,
				      NULL)) == NULL)
		return (IPPFIND_EXIT_MEMORY);

	     /*
	      * Replace "current" with new AND node at the end of this list...
	      */

	      current->prev->next = tempand;
	      tempand->prev       = current->prev;
	      tempand->parent     = parent;

	     /*
	      * Add "current to the new AND node...
	      */

	      tempand->child  = current;
	      current->parent = tempand;
	      current->prev   = NULL;
	      parent          = tempand;
	    }

	   /*
	    * Add the new node at current level...
	    */

	    temp->parent = parent;
	    temp->prev   = current;

	    if (current)
	      current->next = temp;
	    else if (parent)
	      parent->child = temp;
	    else
	      expressions = temp;

	    current = temp;
	    invert  = 0;
	    logic   = IPPFIND_OP_AND;
	    temp    = NULL;
	  }
        }
      }
    }
    else if (!strcmp(argv[i], "("))
    {
      if (num_parens >= 100)
      {
        _cupsLangPuts(stderr, _("ippfind: Too many parenthesis."));
        show_usage();
      }

      if ((temp = new_expr(IPPFIND_OP_AND, invert, NULL, NULL, NULL)) == NULL)
	return (IPPFIND_EXIT_MEMORY);

      parens[num_parens++] = temp;

      if (current)
      {
	temp->parent  = current->parent;
	current->next = temp;
	temp->prev    = current;
      }
      else
	expressions = temp;

      parent  = temp;
      current = NULL;
      invert  = 0;
      logic   = IPPFIND_OP_AND;
    }
    else if (!strcmp(argv[i], ")"))
    {
      if (num_parens <= 0)
      {
        _cupsLangPuts(stderr, _("ippfind: Missing open parenthesis."));
        show_usage();
      }

      current = parens[--num_parens];
      parent  = current->parent;
      invert  = 0;
      logic   = IPPFIND_OP_AND;
    }
    else if (!strcmp(argv[i], "!"))
    {
      invert = 1;
    }
    else
    {
     /*
      * _regtype._tcp[,subtype][.domain]
      *
      *   OR
      *
      * service-name[._regtype._tcp[.domain]]
      */

      cupsArrayAdd(searches, argv[i]);
    }
  }

  if (num_parens > 0)
  {
    _cupsLangPuts(stderr, _("ippfind: Missing close parenthesis."));
    show_usage();
  }

  if (!have_output)
  {
   /*
    * Add an implicit --print-uri to the end...
    */

    if ((temp = new_expr(IPPFIND_OP_PRINT_URI, 0, NULL, NULL, NULL)) == NULL)
      return (IPPFIND_EXIT_MEMORY);

    if (current)
    {
      while (current->parent)
	current = current->parent;

      current->next = temp;
      temp->prev    = current;
    }
    else
      expressions = temp;
  }

  if (cupsArrayCount(searches) == 0)
  {
   /*
    * Add an implicit browse for IPP printers ("_ipp._tcp")...
    */

    cupsArrayAdd(searches, "_ipp._tcp");
  }

  if (getenv("IPPFIND_DEBUG"))
  {
    int		indent = 4;		/* Indentation */

    puts("Expression tree:");
    current = expressions;
    while (current)
    {
     /*
      * Print the current node...
      */

      printf("%*s%s%s\n", indent, "", current->invert ? "!" : "",
             ops[current->op]);

     /*
      * Advance to the next node...
      */

      if (current->child)
      {
        current = current->child;
        indent += 4;
      }
      else if (current->next)
        current = current->next;
      else if (current->parent)
      {
        while (current->parent)
        {
	  indent -= 4;
          current = current->parent;
          if (current->next)
            break;
        }

        current = current->next;
      }
      else
        current = NULL;
    }

    puts("\nSearch items:");
    for (search = (const char *)cupsArrayFirst(searches);
	 search;
	 search = (const char *)cupsArrayNext(searches))
      printf("    %s\n", search);
  }

 /*
  * Start up browsing/resolving...
  */

#ifdef HAVE_DNSSD
  if ((err = DNSServiceCreateConnection(&dnssd_ref)) != kDNSServiceErr_NoError)
  {
    _cupsLangPrintf(stderr, _("ippfind: Unable to use Bonjour: %s"),
                    dnssd_error_string(err));
    return (IPPFIND_EXIT_BONJOUR);
  }

#elif defined(HAVE_AVAHI)
  if ((avahi_poll = avahi_simple_poll_new()) == NULL)
  {
    _cupsLangPrintf(stderr, _("ippfind: Unable to use Bonjour: %s"),
                    strerror(errno));
    return (IPPFIND_EXIT_BONJOUR);
  }

  avahi_simple_poll_set_func(avahi_poll, poll_callback, NULL);

  avahi_client = avahi_client_new(avahi_simple_poll_get(avahi_poll),
			          0, client_callback, avahi_poll, &err);
  if (!avahi_client)
  {
    _cupsLangPrintf(stderr, _("ippfind: Unable to use Bonjour: %s"),
                    dnssd_error_string(err));
    return (IPPFIND_EXIT_BONJOUR);
  }
#endif /* HAVE_DNSSD */

  for (search = (const char *)cupsArrayFirst(searches);
       search;
       search = (const char *)cupsArrayNext(searches))
  {
    char		buf[1024],	/* Full name string */
			*name = NULL,	/* Service instance name */
			*regtype,	/* Registration type */
			*domain;	/* Domain, if any */

    strlcpy(buf, search, sizeof(buf));
    if (buf[0] == '_')
    {
      regtype = buf;
    }
    else if ((regtype = strstr(buf, "._")) != NULL)
    {
      name = buf;
      *regtype++ = '\0';
    }
    else
    {
      name    = buf;
      regtype = "_ipp._tcp";
    }

    for (domain = regtype; *domain; domain ++)
      if (*domain == '.' && domain[1] != '_')
      {
        *domain++ = '\0';
        break;
      }

    if (!*domain)
      domain = NULL;

    if (name)
    {
     /*
      * Resolve the given service instance name, regtype, and domain...
      */

      if (!domain)
        domain = "local.";

      service = get_service(services, name, regtype, domain);

#ifdef HAVE_DNSSD
      service->ref = dnssd_ref;
      err          = DNSServiceResolve(&(service->ref),
                                       kDNSServiceFlagsShareConnection, 0, name,
				       regtype, domain, resolve_callback,
				       service);

#elif defined(HAVE_AVAHI)
      service->ref = avahi_service_resolver_new(avahi_client, AVAHI_IF_UNSPEC,
                                                AVAHI_PROTO_UNSPEC, name,
                                                regtype, domain,
                                                AVAHI_PROTO_UNSPEC, 0,
                                                resolve_callback, service);
      if (service->ref)
        err = 0;
      else
        err = avahi_client_errno(avahi_client);
#endif /* HAVE_DNSSD */
    }
    else
    {
     /*
      * Browse for services of the given type...
      */

#ifdef HAVE_DNSSD
      DNSServiceRef	ref;		/* Browse reference */

      ref = dnssd_ref;
      err = DNSServiceBrowse(&ref, kDNSServiceFlagsShareConnection, 0, regtype,
                             domain, browse_callback, services);

      if (!err)
      {
	ref = dnssd_ref;
	err = DNSServiceBrowse(&ref, kDNSServiceFlagsShareConnection,
			       kDNSServiceInterfaceIndexLocalOnly, regtype,
			       domain, browse_local_callback, services);
      }

#elif defined(HAVE_AVAHI)
      if (avahi_service_browser_new(avahi_client, AVAHI_IF_UNSPEC,
                                    AVAHI_PROTO_UNSPEC, regtype, domain, 0,
                                    browse_callback, services))
        err = 0;
      else
        err = avahi_client_errno(avahi_client);
#endif /* HAVE_DNSSD */
    }

    if (err)
    {
      _cupsLangPrintf(stderr, _("ippfind: Unable to browse or resolve: %s"),
                      dnssd_error_string(err));

      if (name)
        printf("name=\"%s\"\n", name);

      printf("regtype=\"%s\"\n", regtype);

      if (domain)
        printf("domain=\"%s\"\n", domain);

      return (IPPFIND_EXIT_BONJOUR);
    }
  }

 /*
  * Process browse/resolve requests...
  */

  if (bonjour_timeout > 1.0)
    endtime = get_time() + bonjour_timeout;
  else
    endtime = get_time() + 300.0;

  while (get_time() < endtime)
  {
    int		process = 0;		/* Process services? */

#ifdef HAVE_DNSSD
    int fd = DNSServiceRefSockFD(dnssd_ref);
					/* File descriptor for DNS-SD */

    FD_ZERO(&sinput);
    FD_SET(fd, &sinput);

    stimeout.tv_sec  = 0;
    stimeout.tv_usec = 500000;

    if (select(fd + 1, &sinput, NULL, NULL, &stimeout) < 0)
      continue;

    if (FD_ISSET(fd, &sinput))
    {
     /*
      * Process responses...
      */

      DNSServiceProcessResult(dnssd_ref);
    }
    else
    {
     /*
      * Time to process services...
      */

      process = 1;
    }

#elif defined(HAVE_AVAHI)
    avahi_got_data = 0;

    if (avahi_simple_poll_iterate(avahi_poll, 500) > 0)
    {
     /*
      * We've been told to exit the loop.  Perhaps the connection to
      * Avahi failed.
      */

      return (IPPFIND_EXIT_BONJOUR);
    }

    if (!avahi_got_data)
    {
     /*
      * Time to process services...
      */

      process = 1;
    }
#endif /* HAVE_DNSSD */

    if (process)
    {
     /*
      * Process any services that we have found...
      */

      int	active = 0,		/* Number of active resolves */
		resolved = 0,		/* Number of resolved services */
		processed = 0;		/* Number of processed services */

      for (service = (ippfind_srv_t *)cupsArrayFirst(services);
           service;
           service = (ippfind_srv_t *)cupsArrayNext(services))
      {
        if (service->is_processed)
          processed ++;

        if (service->is_resolved)
          resolved ++;

        if (!service->ref && !service->is_resolved)
        {
         /*
          * Found a service, now resolve it (but limit to 50 active resolves...)
          */

          if (active < 50)
          {
#ifdef HAVE_DNSSD
	    service->ref = dnssd_ref;
	    err          = DNSServiceResolve(&(service->ref),
					     kDNSServiceFlagsShareConnection, 0,
					     service->name, service->regtype,
					     service->domain, resolve_callback,
					     service);

#elif defined(HAVE_AVAHI)
	    service->ref = avahi_service_resolver_new(avahi_client,
						      AVAHI_IF_UNSPEC,
						      AVAHI_PROTO_UNSPEC,
						      service->name,
						      service->regtype,
						      service->domain,
						      AVAHI_PROTO_UNSPEC, 0,
						      resolve_callback,
						      service);
	    if (service->ref)
	      err = 0;
	    else
	      err = avahi_client_errno(avahi_client);
#endif /* HAVE_DNSSD */

	    if (err)
	    {
	      _cupsLangPrintf(stderr,
	                      _("ippfind: Unable to browse or resolve: %s"),
			      dnssd_error_string(err));
	      return (IPPFIND_EXIT_BONJOUR);
	    }

	    active ++;
          }
        }
        else if (service->is_resolved && !service->is_processed)
        {
	 /*
	  * Resolved, not process this service against the expressions...
	  */

          if (service->ref)
          {
#ifdef HAVE_DNSSD
	    DNSServiceRefDeallocate(service->ref);
#else
            avahi_service_resolver_free(service->ref);
#endif /* HAVE_DNSSD */

	    service->ref = NULL;
	  }

          if (eval_expr(service, expressions))
            status = IPPFIND_EXIT_TRUE;

          service->is_processed = 1;
        }
        else if (service->ref)
          active ++;
      }

     /*
      * If we have processed all services we have discovered, then we are done.
      */

      if (processed == cupsArrayCount(services) && bonjour_timeout <= 1.0)
        break;
    }
  }

  if (bonjour_error)
    return (IPPFIND_EXIT_BONJOUR);
  else
    return (status);
}


#ifdef HAVE_DNSSD
/*
 * 'browse_callback()' - Browse devices.
 */

static void DNSSD_API
browse_callback(
    DNSServiceRef       sdRef,		/* I - Service reference */
    DNSServiceFlags     flags,		/* I - Option flags */
    uint32_t            interfaceIndex,	/* I - Interface number */
    DNSServiceErrorType errorCode,	/* I - Error, if any */
    const char          *serviceName,	/* I - Name of service/device */
    const char          *regtype,	/* I - Type of service */
    const char          *replyDomain,	/* I - Service domain */
    void                *context)	/* I - Services array */
{
 /*
  * Only process "add" data...
  */

  (void)sdRef;
  (void)interfaceIndex;

  if (errorCode != kDNSServiceErr_NoError || !(flags & kDNSServiceFlagsAdd))
    return;

 /*
  * Get the device...
  */

  get_service((cups_array_t *)context, serviceName, regtype, replyDomain);
}


/*
 * 'browse_local_callback()' - Browse local devices.
 */

static void DNSSD_API
browse_local_callback(
    DNSServiceRef       sdRef,		/* I - Service reference */
    DNSServiceFlags     flags,		/* I - Option flags */
    uint32_t            interfaceIndex,	/* I - Interface number */
    DNSServiceErrorType errorCode,	/* I - Error, if any */
    const char          *serviceName,	/* I - Name of service/device */
    const char          *regtype,	/* I - Type of service */
    const char          *replyDomain,	/* I - Service domain */
    void                *context)	/* I - Services array */
{
  ippfind_srv_t	*service;		/* Service */


 /*
  * Only process "add" data...
  */

  (void)sdRef;
  (void)interfaceIndex;

  if (errorCode != kDNSServiceErr_NoError || !(flags & kDNSServiceFlagsAdd))
    return;

 /*
  * Get the device...
  */

  service = get_service((cups_array_t *)context, serviceName, regtype,
                        replyDomain);
  service->is_local = 1;
}
#endif /* HAVE_DNSSD */


#ifdef HAVE_AVAHI
/*
 * 'browse_callback()' - Browse devices.
 */

static void
browse_callback(
    AvahiServiceBrowser    *browser,	/* I - Browser */
    AvahiIfIndex           interface,	/* I - Interface index (unused) */
    AvahiProtocol          protocol,	/* I - Network protocol (unused) */
    AvahiBrowserEvent      event,	/* I - What happened */
    const char             *name,	/* I - Service name */
    const char             *type,	/* I - Registration type */
    const char             *domain,	/* I - Domain */
    AvahiLookupResultFlags flags,	/* I - Flags */
    void                   *context)	/* I - Services array */
{
  AvahiClient	*client = avahi_service_browser_get_client(browser);
					/* Client information */
  ippfind_srv_t	*service;		/* Service information */


  (void)interface;
  (void)protocol;
  (void)context;

  switch (event)
  {
    case AVAHI_BROWSER_FAILURE:
	fprintf(stderr, "DEBUG: browse_callback: %s\n",
		avahi_strerror(avahi_client_errno(client)));
	bonjour_error = 1;
	avahi_simple_poll_quit(avahi_poll);
	break;

    case AVAHI_BROWSER_NEW:
       /*
	* This object is new on the network. Create a device entry for it if
	* it doesn't yet exist.
	*/

	service = get_service((cups_array_t *)context, name, type, domain);

	if (flags & AVAHI_LOOKUP_RESULT_LOCAL)
	  service->is_local = 1;
	break;

    case AVAHI_BROWSER_REMOVE:
    case AVAHI_BROWSER_ALL_FOR_NOW:
    case AVAHI_BROWSER_CACHE_EXHAUSTED:
        break;
  }
}


/*
 * 'client_callback()' - Avahi client callback function.
 */

static void
client_callback(
    AvahiClient      *client,		/* I - Client information (unused) */
    AvahiClientState state,		/* I - Current state */
    void             *context)		/* I - User data (unused) */
{
  (void)client;
  (void)context;

 /*
  * If the connection drops, quit.
  */

  if (state == AVAHI_CLIENT_FAILURE)
  {
    fputs("DEBUG: Avahi connection failed.\n", stderr);
    bonjour_error = 1;
    avahi_simple_poll_quit(avahi_poll);
  }
}
#endif /* HAVE_AVAHI */


/*
 * 'compare_services()' - Compare two devices.
 */

static int				/* O - Result of comparison */
compare_services(ippfind_srv_t *a,	/* I - First device */
                 ippfind_srv_t *b)	/* I - Second device */
{
  return (strcmp(a->name, b->name));
}


/*
 * 'dnssd_error_string()' - Return an error string for an error code.
 */

static const char *			/* O - Error message */
dnssd_error_string(int error)		/* I - Error number */
{
#  ifdef HAVE_DNSSD
  switch (error)
  {
    case kDNSServiceErr_NoError :
        return ("OK.");

    default :
    case kDNSServiceErr_Unknown :
        return ("Unknown error.");

    case kDNSServiceErr_NoSuchName :
        return ("Service not found.");

    case kDNSServiceErr_NoMemory :
        return ("Out of memory.");

    case kDNSServiceErr_BadParam :
        return ("Bad parameter.");

    case kDNSServiceErr_BadReference :
        return ("Bad service reference.");

    case kDNSServiceErr_BadState :
        return ("Bad state.");

    case kDNSServiceErr_BadFlags :
        return ("Bad flags.");

    case kDNSServiceErr_Unsupported :
        return ("Unsupported.");

    case kDNSServiceErr_NotInitialized :
        return ("Not initialized.");

    case kDNSServiceErr_AlreadyRegistered :
        return ("Already registered.");

    case kDNSServiceErr_NameConflict :
        return ("Name conflict.");

    case kDNSServiceErr_Invalid :
        return ("Invalid name.");

    case kDNSServiceErr_Firewall :
        return ("Firewall prevents registration.");

    case kDNSServiceErr_Incompatible :
        return ("Client library incompatible.");

    case kDNSServiceErr_BadInterfaceIndex :
        return ("Bad interface index.");

    case kDNSServiceErr_Refused :
        return ("Server prevents registration.");

    case kDNSServiceErr_NoSuchRecord :
        return ("Record not found.");

    case kDNSServiceErr_NoAuth :
        return ("Authentication required.");

    case kDNSServiceErr_NoSuchKey :
        return ("Encryption key not found.");

    case kDNSServiceErr_NATTraversal :
        return ("Unable to traverse NAT boundary.");

    case kDNSServiceErr_DoubleNAT :
        return ("Unable to traverse double-NAT boundary.");

    case kDNSServiceErr_BadTime :
        return ("Bad system time.");

    case kDNSServiceErr_BadSig :
        return ("Bad signature.");

    case kDNSServiceErr_BadKey :
        return ("Bad encryption key.");

    case kDNSServiceErr_Transient :
        return ("Transient error occurred - please try again.");

    case kDNSServiceErr_ServiceNotRunning :
        return ("Server not running.");

    case kDNSServiceErr_NATPortMappingUnsupported :
        return ("NAT doesn't support NAT-PMP or UPnP.");

    case kDNSServiceErr_NATPortMappingDisabled :
        return ("NAT supports NAT-PNP or UPnP but it is disabled.");

    case kDNSServiceErr_NoRouter :
        return ("No Internet/default router configured.");

    case kDNSServiceErr_PollingMode :
        return ("Service polling mode error.");

#ifndef WIN32
    case kDNSServiceErr_Timeout :
        return ("Service timeout.");
#endif /* !WIN32 */
  }

#  elif defined(HAVE_AVAHI)
  return (avahi_strerror(error));
#  endif /* HAVE_DNSSD */
}


/*
 * 'eval_expr()' - Evaluate the expressions against the specified service.
 *
 * Returns 1 for true and 0 for false.
 */

static int				/* O - Result of evaluation */
eval_expr(ippfind_srv_t  *service,	/* I - Service */
	  ippfind_expr_t *expressions)	/* I - Expressions */
{
  int			logic,		/* Logical operation */
			result;		/* Result of current expression */
  ippfind_expr_t	*expression;	/* Current expression */
  const char		*val;		/* TXT value */

 /*
  * Loop through the expressions...
  */

  if (expressions && expressions->parent)
    logic = expressions->parent->op;
  else
    logic = IPPFIND_OP_AND;

  for (expression = expressions; expression; expression = expression->next)
  {
    switch (expression->op)
    {
      default :
      case IPPFIND_OP_AND :
      case IPPFIND_OP_OR :
          if (expression->child)
            result = eval_expr(service, expression->child);
          else
            result = expression->op == IPPFIND_OP_AND;
          break;
      case IPPFIND_OP_TRUE :
          result = 1;
          break;
      case IPPFIND_OP_FALSE :
          result = 0;
          break;
      case IPPFIND_OP_IS_LOCAL :
          result = service->is_local;
          break;
      case IPPFIND_OP_IS_REMOTE :
          result = !service->is_local;
          break;
      case IPPFIND_OP_DOMAIN_REGEX :
          result = !regexec(&(expression->re), service->domain, 0, NULL, 0);
          break;
      case IPPFIND_OP_NAME_REGEX :
          result = !regexec(&(expression->re), service->name, 0, NULL, 0);
          break;
      case IPPFIND_OP_HOST_REGEX :
          result = !regexec(&(expression->re), service->host, 0, NULL, 0);
          break;
      case IPPFIND_OP_PORT_RANGE :
          result = service->port >= expression->range[0] &&
                   service->port <= expression->range[1];
          break;
      case IPPFIND_OP_PATH_REGEX :
          result = !regexec(&(expression->re), service->resource, 0, NULL, 0);
          break;
      case IPPFIND_OP_TXT_EXISTS :
          result = cupsGetOption(expression->key, service->num_txt,
				 service->txt) != NULL;
          break;
      case IPPFIND_OP_TXT_REGEX :
          val = cupsGetOption(expression->key, service->num_txt,
			      service->txt);
	  if (val)
	    result = !regexec(&(expression->re), val, 0, NULL, 0);
	  else
	    result = 0;

	  if (getenv("IPPFIND_DEBUG"))
	    printf("TXT_REGEX of \"%s\": %d\n", val, result);
          break;
      case IPPFIND_OP_URI_REGEX :
          result = !regexec(&(expression->re), service->uri, 0, NULL, 0);
          break;
      case IPPFIND_OP_EXEC :
          result = exec_program(service, expression->num_args,
				expression->args);
          break;
      case IPPFIND_OP_LIST :
          result = list_service(service);
          break;
      case IPPFIND_OP_PRINT_NAME :
          _cupsLangPuts(stdout, service->name);
          result = 1;
          break;
      case IPPFIND_OP_PRINT_URI :
          _cupsLangPuts(stdout, service->uri);
          result = 1;
          break;
      case IPPFIND_OP_QUIET :
          result = 1;
          break;
    }

    if (expression->invert)
      result = !result;

    if (logic == IPPFIND_OP_AND && !result)
      return (0);
    else if (logic == IPPFIND_OP_OR && result)
      return (1);
  }

  return (logic == IPPFIND_OP_AND);
}


/*
 * 'exec_program()' - Execute a program for a service.
 */

static int				/* O - 1 if program terminated
					       successfully, 0 otherwise. */
exec_program(ippfind_srv_t *service,	/* I - Service */
             int           num_args,	/* I - Number of command-line args */
             char          **args)	/* I - Command-line arguments */
{
  char		**myargv,		/* Command-line arguments */
		**myenvp,		/* Environment variables */
		*ptr,			/* Pointer into variable */
		domain[1024],		/* IPPFIND_SERVICE_DOMAIN */
		hostname[1024],		/* IPPFIND_SERVICE_HOSTNAME */
		name[256],		/* IPPFIND_SERVICE_NAME */
		port[32],		/* IPPFIND_SERVICE_PORT */
		regtype[256],		/* IPPFIND_SERVICE_REGTYPE */
		scheme[128],		/* IPPFIND_SERVICE_SCHEME */
		uri[1024],		/* IPPFIND_SERVICE_URI */
		txt[100][256];		/* IPPFIND_TXT_foo */
  int		i,			/* Looping var */
		myenvc,			/* Number of environment variables */
		status;			/* Exit status of program */
#ifndef WIN32
  char		program[1024];		/* Program to execute */
  int		pid;			/* Process ID */
#endif /* !WIN32 */


 /*
  * Environment variables...
  */

  snprintf(domain, sizeof(domain), "IPPFIND_SERVICE_DOMAIN=%s",
           service->domain);
  snprintf(hostname, sizeof(hostname), "IPPFIND_SERVICE_HOSTNAME=%s",
           service->host);
  snprintf(name, sizeof(name), "IPPFIND_SERVICE_NAME=%s", service->name);
  snprintf(port, sizeof(port), "IPPFIND_SERVICE_PORT=%d", service->port);
  snprintf(regtype, sizeof(regtype), "IPPFIND_SERVICE_REGTYPE=%s",
           service->regtype);
  snprintf(scheme, sizeof(scheme), "IPPFIND_SERVICE_SCHEME=%s",
           !strncmp(service->regtype, "_http._tcp", 10) ? "http" :
               !strncmp(service->regtype, "_https._tcp", 11) ? "https" :
               !strncmp(service->regtype, "_ipp._tcp", 9) ? "ipp" :
               !strncmp(service->regtype, "_ipps._tcp", 10) ? "ipps" : "lpd");
  snprintf(uri, sizeof(uri), "IPPFIND_SERVICE_URI=%s", service->uri);
  for (i = 0; i < service->num_txt && i < 100; i ++)
  {
    snprintf(txt[i], sizeof(txt[i]), "IPPFIND_TXT_%s=%s", service->txt[i].name,
             service->txt[i].value);
    for (ptr = txt[i] + 12; *ptr && *ptr != '='; ptr ++)
      *ptr = (char)_cups_toupper(*ptr);
  }

  for (i = 0, myenvc = 7 + service->num_txt; environ[i]; i ++)
    if (strncmp(environ[i], "IPPFIND_", 8))
      myenvc ++;

  if ((myenvp = calloc(sizeof(char *), (size_t)(myenvc + 1))) == NULL)
  {
    _cupsLangPuts(stderr, _("ippfind: Out of memory."));
    exit(IPPFIND_EXIT_MEMORY);
  }

  for (i = 0, myenvc = 0; environ[i]; i ++)
    if (strncmp(environ[i], "IPPFIND_", 8))
      myenvp[myenvc++] = environ[i];

  myenvp[myenvc++] = domain;
  myenvp[myenvc++] = hostname;
  myenvp[myenvc++] = name;
  myenvp[myenvc++] = port;
  myenvp[myenvc++] = regtype;
  myenvp[myenvc++] = scheme;
  myenvp[myenvc++] = uri;

  for (i = 0; i < service->num_txt && i < 100; i ++)
    myenvp[myenvc++] = txt[i];

 /*
  * Allocate and copy command-line arguments...
  */

  if ((myargv = calloc(sizeof(char *), (size_t)(num_args + 1))) == NULL)
  {
    _cupsLangPuts(stderr, _("ippfind: Out of memory."));
    exit(IPPFIND_EXIT_MEMORY);
  }

  for (i = 0; i < num_args; i ++)
  {
    if (strchr(args[i], '{'))
    {
      char	temp[2048],		/* Temporary string */
		*tptr,			/* Pointer into temporary string */
		keyword[256],		/* {keyword} */
		*kptr;			/* Pointer into keyword */

      for (ptr = args[i], tptr = temp; *ptr; ptr ++)
      {
        if (*ptr == '{')
        {
         /*
          * Do a {var} substitution...
          */

          for (kptr = keyword, ptr ++; *ptr && *ptr != '}'; ptr ++)
            if (kptr < (keyword + sizeof(keyword) - 1))
              *kptr++ = *ptr;

          if (*ptr != '}')
          {
            _cupsLangPuts(stderr,
                          _("ippfind: Missing close brace in substitution."));
            exit(IPPFIND_EXIT_SYNTAX);
          }

          *kptr = '\0';
          if (!keyword[0] || !strcmp(keyword, "service_uri"))
	    strlcpy(tptr, service->uri, sizeof(temp) - (size_t)(tptr - temp));
	  else if (!strcmp(keyword, "service_domain"))
	    strlcpy(tptr, service->domain, sizeof(temp) - (size_t)(tptr - temp));
	  else if (!strcmp(keyword, "service_hostname"))
	    strlcpy(tptr, service->host, sizeof(temp) - (size_t)(tptr - temp));
	  else if (!strcmp(keyword, "service_name"))
	    strlcpy(tptr, service->name, sizeof(temp) - (size_t)(tptr - temp));
	  else if (!strcmp(keyword, "service_path"))
	    strlcpy(tptr, service->resource, sizeof(temp) - (size_t)(tptr - temp));
	  else if (!strcmp(keyword, "service_port"))
	    strlcpy(tptr, port + 21, sizeof(temp) - (size_t)(tptr - temp));
	  else if (!strcmp(keyword, "service_scheme"))
	    strlcpy(tptr, scheme + 22, sizeof(temp) - (size_t)(tptr - temp));
	  else if (!strncmp(keyword, "txt_", 4))
	  {
	    const char *txt = cupsGetOption(keyword + 4, service->num_txt, service->txt);
	    if (txt)
	      strlcpy(tptr, txt, sizeof(temp) - (size_t)(tptr - temp));
	    else
	      *tptr = '\0';
	  }
	  else
	  {
	    _cupsLangPrintf(stderr, _("ippfind: Unknown variable \"{%s}\"."),
	                    keyword);
	    exit(IPPFIND_EXIT_SYNTAX);
	  }

	  tptr += strlen(tptr);
	}
	else if (tptr < (temp + sizeof(temp) - 1))
	  *tptr++ = *ptr;
      }

      *tptr = '\0';
      myargv[i] = strdup(temp);
    }
    else
      myargv[i] = strdup(args[i]);
  }

#ifdef WIN32
  if (getenv("IPPFIND_DEBUG"))
  {
    printf("\nProgram:\n    %s\n", args[0]);
    puts("\nArguments:");
    for (i = 0; i < num_args; i ++)
      printf("    %s\n", myargv[i]);
    puts("\nEnvironment:");
    for (i = 0; i < myenvc; i ++)
      printf("    %s\n", myenvp[i]);
  }

  status = _spawnvpe(_P_WAIT, args[0], myargv, myenvp);

#else
 /*
  * Execute the program...
  */

  if (strchr(args[0], '/') && !access(args[0], X_OK))
    strlcpy(program, args[0], sizeof(program));
  else if (!cupsFileFind(args[0], getenv("PATH"), 1, program, sizeof(program)))
  {
    _cupsLangPrintf(stderr, _("ippfind: Unable to execute \"%s\": %s"),
                    args[0], strerror(ENOENT));
    exit(IPPFIND_EXIT_SYNTAX);
  }

  if (getenv("IPPFIND_DEBUG"))
  {
    printf("\nProgram:\n    %s\n", program);
    puts("\nArguments:");
    for (i = 0; i < num_args; i ++)
      printf("    %s\n", myargv[i]);
    puts("\nEnvironment:");
    for (i = 0; i < myenvc; i ++)
      printf("    %s\n", myenvp[i]);
  }

  if ((pid = fork()) == 0)
  {
   /*
    * Child comes here...
    */

    execve(program, myargv, myenvp);
    exit(1);
  }
  else if (pid < 0)
  {
    _cupsLangPrintf(stderr, _("ippfind: Unable to execute \"%s\": %s"),
                    args[0], strerror(errno));
    exit(IPPFIND_EXIT_SYNTAX);
  }
  else
  {
   /*
    * Wait for it to complete...
    */

    while (wait(&status) != pid)
      ;
  }
#endif /* WIN32 */

 /*
  * Free memory...
  */

  for (i = 0; i < num_args; i ++)
    free(myargv[i]);

  free(myargv);
  free(myenvp);

 /*
  * Return whether the program succeeded or crashed...
  */

  if (getenv("IPPFIND_DEBUG"))
  {
#ifdef WIN32
    printf("Exit Status: %d\n", status);
#else
    if (WIFEXITED(status))
      printf("Exit Status: %d\n", WEXITSTATUS(status));
    else
      printf("Terminating Signal: %d\n", WTERMSIG(status));
#endif /* WIN32 */
  }

  return (status == 0);
}


/*
 * 'get_service()' - Create or update a device.
 */

static ippfind_srv_t *			/* O - Service */
get_service(cups_array_t *services,	/* I - Service array */
	    const char   *serviceName,	/* I - Name of service/device */
	    const char   *regtype,	/* I - Type of service */
	    const char   *replyDomain)	/* I - Service domain */
{
  ippfind_srv_t	key,			/* Search key */
		*service;		/* Service */
  char		fullName[kDNSServiceMaxDomainName];
					/* Full name for query */


 /*
  * See if this is a new device...
  */

  key.name    = (char *)serviceName;
  key.regtype = (char *)regtype;

  for (service = cupsArrayFind(services, &key);
       service;
       service = cupsArrayNext(services))
    if (_cups_strcasecmp(service->name, key.name))
      break;
    else if (!strcmp(service->regtype, key.regtype))
      return (service);

 /*
  * Yes, add the service...
  */

  service           = calloc(sizeof(ippfind_srv_t), 1);
  service->name     = strdup(serviceName);
  service->domain   = strdup(replyDomain);
  service->regtype  = strdup(regtype);

  cupsArrayAdd(services, service);

 /*
  * Set the "full name" of this service, which is used for queries and
  * resolves...
  */

#ifdef HAVE_DNSSD
  DNSServiceConstructFullName(fullName, serviceName, regtype, replyDomain);
#else /* HAVE_AVAHI */
  avahi_service_name_join(fullName, kDNSServiceMaxDomainName, serviceName,
                          regtype, replyDomain);
#endif /* HAVE_DNSSD */

  service->fullName = strdup(fullName);

  return (service);
}


/*
 * 'get_time()' - Get the current time-of-day in seconds.
 */

static double
get_time(void)
{
#ifdef WIN32
  struct _timeb curtime;		/* Current Windows time */

  _ftime(&curtime);

  return (curtime.time + 0.001 * curtime.millitm);

#else
  struct timeval	curtime;	/* Current UNIX time */

  if (gettimeofday(&curtime, NULL))
    return (0.0);
  else
    return (curtime.tv_sec + 0.000001 * curtime.tv_usec);
#endif /* WIN32 */
}


/*
 * 'list_service()' - List the contents of a service.
 */

static int				/* O - 1 if successful, 0 otherwise */
list_service(ippfind_srv_t *service)	/* I - Service */
{
  http_addrlist_t	*addrlist;	/* Address(es) of service */
  char			port[10];	/* Port number of service */


  snprintf(port, sizeof(port), "%d", service->port);

  if ((addrlist = httpAddrGetList(service->host, address_family, port)) == NULL)
  {
    _cupsLangPrintf(stdout, "%s unreachable", service->uri);
    return (0);
  }

  if (!strncmp(service->regtype, "_ipp._tcp", 9) ||
      !strncmp(service->regtype, "_ipps._tcp", 10))
  {
   /*
    * IPP/IPPS printer
    */

    http_t		*http;		/* HTTP connection */
    ipp_t		*request,	/* IPP request */
			*response;	/* IPP response */
    ipp_attribute_t	*attr;		/* IPP attribute */
    int			i,		/* Looping var */
			count,		/* Number of values */
			version,	/* IPP version */
			paccepting;	/* printer-is-accepting-jobs value */
    ipp_pstate_t	pstate;		/* printer-state value */
    char		preasons[1024],	/* Comma-delimited printer-state-reasons */
			*ptr,		/* Pointer into reasons */
			*end;		/* End of reasons buffer */
    static const char * const rattrs[] =/* Requested attributes */
    {
      "printer-is-accepting-jobs",
      "printer-state",
      "printer-state-reasons"
    };

   /*
    * Connect to the printer...
    */

    http = httpConnect2(service->host, service->port, addrlist, address_family,
			!strncmp(service->regtype, "_ipps._tcp", 10) ?
			    HTTP_ENCRYPTION_ALWAYS :
			    HTTP_ENCRYPTION_IF_REQUESTED,
			1, 30000, NULL);

    httpAddrFreeList(addrlist);

    if (!http)
    {
      _cupsLangPrintf(stdout, "%s unavailable", service->uri);
      return (0);
    }

   /*
    * Get the current printer state...
    */

    response = NULL;
    version  = ipp_version;

    do
    {
      request = ippNewRequest(IPP_OP_GET_PRINTER_ATTRIBUTES);
      ippSetVersion(request, version / 10, version % 10);
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL,
                   service->uri);
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                   "requesting-user-name", NULL, cupsUser());
      ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                    "requested-attributes",
                    (int)(sizeof(rattrs) / sizeof(rattrs[0])), NULL, rattrs);

      response = cupsDoRequest(http, request, service->resource);

      if (cupsLastError() == IPP_STATUS_ERROR_BAD_REQUEST && version > 11)
        version = 11;
    }
    while (cupsLastError() > IPP_STATUS_OK_EVENTS_COMPLETE && version > 11);

   /*
    * Show results...
    */

    if (cupsLastError() > IPP_STATUS_OK_EVENTS_COMPLETE)
    {
      _cupsLangPrintf(stdout, "%s: unavailable", service->uri);
      return (0);
    }

    if ((attr = ippFindAttribute(response, "printer-state",
                                 IPP_TAG_ENUM)) != NULL)
      pstate = (ipp_pstate_t)ippGetInteger(attr, 0);
    else
      pstate = IPP_PSTATE_STOPPED;

    if ((attr = ippFindAttribute(response, "printer-is-accepting-jobs",
                                 IPP_TAG_BOOLEAN)) != NULL)
      paccepting = ippGetBoolean(attr, 0);
    else
      paccepting = 0;

    if ((attr = ippFindAttribute(response, "printer-state-reasons",
                                 IPP_TAG_KEYWORD)) != NULL)
    {
      strlcpy(preasons, ippGetString(attr, 0, NULL), sizeof(preasons));

      for (i = 1, count = ippGetCount(attr), ptr = preasons + strlen(preasons),
               end = preasons + sizeof(preasons) - 1;
           i < count && ptr < end;
           i ++, ptr += strlen(ptr))
      {
        *ptr++ = ',';
        strlcpy(ptr, ippGetString(attr, i, NULL), (size_t)(end - ptr + 1));
      }
    }
    else
      strlcpy(preasons, "none", sizeof(preasons));

    ippDelete(response);
    httpClose(http);

    _cupsLangPrintf(stdout, "%s %s %s %s", service->uri,
                    ippEnumString("printer-state", pstate),
                    paccepting ? "accepting-jobs" : "not-accepting-jobs",
                    preasons);
  }
  else if (!strncmp(service->regtype, "_http._tcp", 10) ||
           !strncmp(service->regtype, "_https._tcp", 11))
  {
   /*
    * HTTP/HTTPS web page
    */

    http_t		*http;		/* HTTP connection */
    http_status_t	status;		/* HEAD status */


   /*
    * Connect to the web server...
    */

    http = httpConnect2(service->host, service->port, addrlist, address_family,
			!strncmp(service->regtype, "_ipps._tcp", 10) ?
			    HTTP_ENCRYPTION_ALWAYS :
			    HTTP_ENCRYPTION_IF_REQUESTED,
			1, 30000, NULL);

    httpAddrFreeList(addrlist);

    if (!http)
    {
      _cupsLangPrintf(stdout, "%s unavailable", service->uri);
      return (0);
    }

    if (httpGet(http, service->resource))
    {
      _cupsLangPrintf(stdout, "%s unavailable", service->uri);
      return (0);
    }

    do
    {
      status = httpUpdate(http);
    }
    while (status == HTTP_STATUS_CONTINUE);

    httpFlush(http);
    httpClose(http);

    if (status >= HTTP_STATUS_BAD_REQUEST)
    {
      _cupsLangPrintf(stdout, "%s unavailable", service->uri);
      return (0);
    }

    _cupsLangPrintf(stdout, "%s available", service->uri);
  }
  else if (!strncmp(service->regtype, "_printer._tcp", 13))
  {
   /*
    * LPD printer
    */

    int	sock;				/* Socket */


    if (!httpAddrConnect(addrlist, &sock))
    {
      _cupsLangPrintf(stdout, "%s unavailable", service->uri);
      httpAddrFreeList(addrlist);
      return (0);
    }

    _cupsLangPrintf(stdout, "%s available", service->uri);
    httpAddrFreeList(addrlist);

    httpAddrClose(NULL, sock);
  }
  else
  {
    _cupsLangPrintf(stdout, "%s unsupported", service->uri);
    httpAddrFreeList(addrlist);
    return (0);
  }

  return (1);
}


/*
 * 'new_expr()' - Create a new expression.
 */

static ippfind_expr_t *			/* O - New expression */
new_expr(ippfind_op_t op,		/* I - Operation */
         int          invert,		/* I - Invert result? */
         const char   *value,		/* I - TXT key or port range */
	 const char   *regex,		/* I - Regular expression */
	 char         **args)		/* I - Pointer to argument strings */
{
  ippfind_expr_t	*temp;		/* New expression */


  if ((temp = calloc(1, sizeof(ippfind_expr_t))) == NULL)
    return (NULL);

  temp->op = op;
  temp->invert = invert;

  if (op == IPPFIND_OP_TXT_EXISTS || op == IPPFIND_OP_TXT_REGEX)
    temp->key = (char *)value;
  else if (op == IPPFIND_OP_PORT_RANGE)
  {
   /*
    * Pull port number range of the form "number", "-number" (0-number),
    * "number-" (number-65535), and "number-number".
    */

    if (*value == '-')
    {
      temp->range[1] = atoi(value + 1);
    }
    else if (strchr(value, '-'))
    {
      if (sscanf(value, "%d-%d", temp->range, temp->range + 1) == 1)
        temp->range[1] = 65535;
    }
    else
    {
      temp->range[0] = temp->range[1] = atoi(value);
    }
  }

  if (regex)
  {
    int err = regcomp(&(temp->re), regex, REG_NOSUB | REG_ICASE | REG_EXTENDED);

    if (err)
    {
      char	message[256];		/* Error message */

      regerror(err, &(temp->re), message, sizeof(message));
      _cupsLangPrintf(stderr, _("ippfind: Bad regular expression: %s"),
                      message);
      exit(IPPFIND_EXIT_SYNTAX);
    }
  }

  if (args)
  {
    int	num_args;			/* Number of arguments */

    for (num_args = 1; args[num_args]; num_args ++)
      if (!strcmp(args[num_args], ";"))
        break;

     temp->num_args = num_args;
     temp->args     = malloc((size_t)num_args * sizeof(char *));
     memcpy(temp->args, args, (size_t)num_args * sizeof(char *));
  }

  return (temp);
}


#ifdef HAVE_AVAHI
/*
 * 'poll_callback()' - Wait for input on the specified file descriptors.
 *
 * Note: This function is needed because avahi_simple_poll_iterate is broken
 *       and always uses a timeout of 0 (!) milliseconds.
 *       (Avahi Ticket #364)
 */

static int				/* O - Number of file descriptors matching */
poll_callback(
    struct pollfd *pollfds,		/* I - File descriptors */
    unsigned int  num_pollfds,		/* I - Number of file descriptors */
    int           timeout,		/* I - Timeout in milliseconds (unused) */
    void          *context)		/* I - User data (unused) */
{
  int	val;				/* Return value */


  (void)timeout;
  (void)context;

  val = poll(pollfds, num_pollfds, 500);

  if (val > 0)
    avahi_got_data = 1;

  return (val);
}
#endif /* HAVE_AVAHI */


/*
 * 'resolve_callback()' - Process resolve data.
 */

#ifdef HAVE_DNSSD
static void DNSSD_API
resolve_callback(
    DNSServiceRef       sdRef,		/* I - Service reference */
    DNSServiceFlags     flags,		/* I - Data flags */
    uint32_t            interfaceIndex,	/* I - Interface */
    DNSServiceErrorType errorCode,	/* I - Error, if any */
    const char          *fullName,	/* I - Full service name */
    const char          *hostTarget,	/* I - Hostname */
    uint16_t            port,		/* I - Port number (network byte order) */
    uint16_t            txtLen,		/* I - Length of TXT record data */
    const unsigned char *txtRecord,	/* I - TXT record data */
    void                *context)	/* I - Service */
{
  char			key[256],	/* TXT key value */
			*value;		/* Value from TXT record */
  const unsigned char	*txtEnd;	/* End of TXT record */
  uint8_t		valueLen;	/* Length of value */
  ippfind_srv_t		*service = (ippfind_srv_t *)context;
					/* Service */


 /*
  * Only process "add" data...
  */

  (void)sdRef;
  (void)flags;
  (void)interfaceIndex;
  (void)fullName;

   if (errorCode != kDNSServiceErr_NoError)
  {
    _cupsLangPrintf(stderr, _("ippfind: Unable to browse or resolve: %s"),
		    dnssd_error_string(errorCode));
    bonjour_error = 1;
    return;
  }

  service->is_resolved = 1;
  service->host        = strdup(hostTarget);
  service->port        = ntohs(port);

  value = service->host + strlen(service->host) - 1;
  if (value >= service->host && *value == '.')
    *value = '\0';

 /*
  * Loop through the TXT key/value pairs and add them to an array...
  */

  for (txtEnd = txtRecord + txtLen; txtRecord < txtEnd; txtRecord += valueLen)
  {
   /*
    * Ignore bogus strings...
    */

    valueLen = *txtRecord++;

    memcpy(key, txtRecord, valueLen);
    key[valueLen] = '\0';

    if ((value = strchr(key, '=')) == NULL)
      continue;

    *value++ = '\0';

   /*
    * Add to array of TXT values...
    */

    service->num_txt = cupsAddOption(key, value, service->num_txt,
                                     &(service->txt));
  }

  set_service_uri(service);
}


#elif defined(HAVE_AVAHI)
static void
resolve_callback(
    AvahiServiceResolver   *resolver,	/* I - Resolver */
    AvahiIfIndex           interface,	/* I - Interface */
    AvahiProtocol          protocol,	/* I - Address protocol */
    AvahiResolverEvent     event,	/* I - Event */
    const char             *serviceName,/* I - Service name */
    const char             *regtype,	/* I - Registration type */
    const char             *replyDomain,/* I - Domain name */
    const char             *hostTarget,	/* I - FQDN */
    const AvahiAddress     *address,	/* I - Address */
    uint16_t               port,	/* I - Port number */
    AvahiStringList        *txt,	/* I - TXT records */
    AvahiLookupResultFlags flags,	/* I - Lookup flags */
    void                   *context)	/* I - Service */
{
  char		key[256],		/* TXT key */
		*value;			/* TXT value */
  ippfind_srv_t	*service = (ippfind_srv_t *)context;
					/* Service */
  AvahiStringList *current;		/* Current TXT key/value pair */


  (void)address;

  if (event != AVAHI_RESOLVER_FOUND)
  {
    bonjour_error = 1;

    avahi_service_resolver_free(resolver);
    avahi_simple_poll_quit(avahi_poll);
    return;
  }

  service->is_resolved = 1;
  service->host        = strdup(hostTarget);
  service->port        = port;

  value = service->host + strlen(service->host) - 1;
  if (value >= service->host && *value == '.')
    *value = '\0';

 /*
  * Loop through the TXT key/value pairs and add them to an array...
  */

  for (current = txt; current; current = current->next)
  {
   /*
    * Ignore bogus strings...
    */

    if (current->size > (sizeof(key) - 1))
      continue;

    memcpy(key, current->text, current->size);
    key[current->size] = '\0';

    if ((value = strchr(key, '=')) == NULL)
      continue;

    *value++ = '\0';

   /*
    * Add to array of TXT values...
    */

    service->num_txt = cupsAddOption(key, value, service->num_txt,
                                     &(service->txt));
  }

  set_service_uri(service);
}
#endif /* HAVE_DNSSD */


/*
 * 'set_service_uri()' - Set the URI of the service.
 */

static void
set_service_uri(ippfind_srv_t *service)	/* I - Service */
{
  char		uri[1024];		/* URI */
  const char	*path,			/* Resource path */
		*scheme;		/* URI scheme */


  if (!strncmp(service->regtype, "_http.", 6))
  {
    scheme = "http";
    path   = cupsGetOption("path", service->num_txt, service->txt);
  }
  else if (!strncmp(service->regtype, "_https.", 7))
  {
    scheme = "https";
    path   = cupsGetOption("path", service->num_txt, service->txt);
  }
  else if (!strncmp(service->regtype, "_ipp.", 5))
  {
    scheme = "ipp";
    path   = cupsGetOption("rp", service->num_txt, service->txt);
  }
  else if (!strncmp(service->regtype, "_ipps.", 6))
  {
    scheme = "ipps";
    path   = cupsGetOption("rp", service->num_txt, service->txt);
  }
  else if (!strncmp(service->regtype, "_printer.", 9))
  {
    scheme = "lpd";
    path   = cupsGetOption("rp", service->num_txt, service->txt);
  }
  else
    return;

  if (!path || !*path)
    path = "/";

  if (*path == '/')
  {
    service->resource = strdup(path);
  }
  else
  {
    snprintf(uri, sizeof(uri), "/%s", path);
    service->resource = strdup(uri);
  }

  httpAssembleURI(HTTP_URI_CODING_ALL, uri, sizeof(uri), scheme, NULL,
		  service->host, service->port, service->resource);
  service->uri = strdup(uri);
}


/*
 * 'show_usage()' - Show program usage.
 */

static void
show_usage(void)
{
  _cupsLangPuts(stderr, _("Usage: ippfind [options] regtype[,subtype]"
                          "[.domain.] ... [expression]\n"
                          "       ippfind [options] name[.regtype[.domain.]] "
                          "... [expression]\n"
                          "       ippfind --help\n"
                          "       ippfind --version"));
  _cupsLangPuts(stderr, "");
  _cupsLangPuts(stderr, _("Options:"));
  _cupsLangPuts(stderr, _("  -4                      Connect using IPv4."));
  _cupsLangPuts(stderr, _("  -6                      Connect using IPv6."));
  _cupsLangPuts(stderr, _("  -T seconds              Set the browse timeout in "
                          "seconds."));
  _cupsLangPuts(stderr, _("  -V version              Set default IPP "
                          "version."));
  _cupsLangPuts(stderr, _("  --help                  Show this help."));
  _cupsLangPuts(stderr, _("  --version               Show program version."));
  _cupsLangPuts(stderr, "");
  _cupsLangPuts(stderr, _("Expressions:"));
  _cupsLangPuts(stderr, _("  -P number[-number]      Match port to number or range."));
  _cupsLangPuts(stderr, _("  -d regex                Match domain to regular expression."));
  _cupsLangPuts(stderr, _("  -h regex                Match hostname to regular expression."));
  _cupsLangPuts(stderr, _("  -l                      List attributes."));
  _cupsLangPuts(stderr, _("  -n regex                Match service name to regular expression."));
  _cupsLangPuts(stderr, _("  -p                      Print URI if true."));
  _cupsLangPuts(stderr, _("  -q                      Quietly report match via exit code."));
  _cupsLangPuts(stderr, _("  -r                      True if service is remote."));
  _cupsLangPuts(stderr, _("  -s                      Print service name if true."));
  _cupsLangPuts(stderr, _("  -t key                  True if the TXT record contains the key."));
  _cupsLangPuts(stderr, _("  -u regex                Match URI to regular expression."));
  _cupsLangPuts(stderr, _("  -x utility [argument ...] ;\n"
                          "                          Execute program if true."));
  _cupsLangPuts(stderr, _("  --domain regex          Match domain to regular expression."));
  _cupsLangPuts(stderr, _("  --exec utility [argument ...] ;\n"
                          "                          Execute program if true."));
  _cupsLangPuts(stderr, _("  --host regex            Match hostname to regular expression."));
  _cupsLangPuts(stderr, _("  --ls                    List attributes."));
  _cupsLangPuts(stderr, _("  --local                 True if service is local."));
  _cupsLangPuts(stderr, _("  --name regex            Match service name to regular expression."));
  _cupsLangPuts(stderr, _("  --path regex            Match resource path to regular expression."));
  _cupsLangPuts(stderr, _("  --port number[-number]  Match port to number or range."));
  _cupsLangPuts(stderr, _("  --print                 Print URI if true."));
  _cupsLangPuts(stderr, _("  --print-name            Print service name if true."));
  _cupsLangPuts(stderr, _("  --quiet                 Quietly report match via exit code."));
  _cupsLangPuts(stderr, _("  --remote                True if service is remote."));
  _cupsLangPuts(stderr, _("  --txt key               True if the TXT record contains the key."));
  _cupsLangPuts(stderr, _("  --txt-* regex           Match TXT record key to regular expression."));
  _cupsLangPuts(stderr, _("  --uri regex             Match URI to regular expression."));
  _cupsLangPuts(stderr, "");
  _cupsLangPuts(stderr, _("Modifiers:"));
  _cupsLangPuts(stderr, _("  ( expressions )         Group expressions."));
  _cupsLangPuts(stderr, _("  ! expression            Unary NOT of expression."));
  _cupsLangPuts(stderr, _("  --not expression        Unary NOT of expression."));
  _cupsLangPuts(stderr, _("  --false                 Always false."));
  _cupsLangPuts(stderr, _("  --true                  Always true."));
  _cupsLangPuts(stderr, _("  expression expression   Logical AND."));
  _cupsLangPuts(stderr, _("  expression --and expression\n"
                          "                          Logical AND."));
  _cupsLangPuts(stderr, _("  expression --or expression\n"
                          "                          Logical OR."));
  _cupsLangPuts(stderr, "");
  _cupsLangPuts(stderr, _("Substitutions:"));
  _cupsLangPuts(stderr, _("  {}                      URI"));
  _cupsLangPuts(stderr, _("  {service_domain}        Domain name"));
  _cupsLangPuts(stderr, _("  {service_hostname}      Fully-qualified domain name"));
  _cupsLangPuts(stderr, _("  {service_name}          Service instance name"));
  _cupsLangPuts(stderr, _("  {service_port}          Port number"));
  _cupsLangPuts(stderr, _("  {service_regtype}       DNS-SD registration type"));
  _cupsLangPuts(stderr, _("  {service_scheme}        URI scheme"));
  _cupsLangPuts(stderr, _("  {service_uri}           URI"));
  _cupsLangPuts(stderr, _("  {txt_*}                 Value of TXT record key"));
  _cupsLangPuts(stderr, "");
  _cupsLangPuts(stderr, _("Environment Variables:"));
  _cupsLangPuts(stderr, _("  IPPFIND_SERVICE_DOMAIN  Domain name"));
  _cupsLangPuts(stderr, _("  IPPFIND_SERVICE_HOSTNAME\n"
                          "                          Fully-qualified domain name"));
  _cupsLangPuts(stderr, _("  IPPFIND_SERVICE_NAME    Service instance name"));
  _cupsLangPuts(stderr, _("  IPPFIND_SERVICE_PORT    Port number"));
  _cupsLangPuts(stderr, _("  IPPFIND_SERVICE_REGTYPE DNS-SD registration type"));
  _cupsLangPuts(stderr, _("  IPPFIND_SERVICE_SCHEME  URI scheme"));
  _cupsLangPuts(stderr, _("  IPPFIND_SERVICE_URI     URI"));
  _cupsLangPuts(stderr, _("  IPPFIND_TXT_*           Value of TXT record key"));

  exit(IPPFIND_EXIT_TRUE);
}


/*
 * 'show_version()' - Show program version.
 */

static void
show_version(void)
{
  _cupsLangPuts(stderr, CUPS_SVERSION);

  exit(IPPFIND_EXIT_TRUE);
}


/*
 * End of "$Id: ippfind.c 12639 2015-05-19 02:36:30Z msweet $".
 */
