/*
 * "$Id$"
 *
 *   Device scanning mini-daemon for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007-2008 by Apple Inc.
 *   Copyright 1997-2006 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
 */

/*
 * Include necessary headers...
 */

#include "util.h"
#include <cups/array.h>
#include <cups/dir.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <poll.h>


/*
 * Constants...
 */

#define MAX_BACKENDS	200		/* Maximum number of backends we'll run */


/*
 * Backend information...
 */

typedef struct
{
  char		*backend;		/* Name of backend */
  int		pid,			/* Process ID */
		status;			/* Exit status */
  cups_file_t	*pipe;			/* Pipe from backend stdout */
  int		count;			/* Number of devices found */
} cupsd_backend_t;


/*
 * Device information structure...
 */

typedef struct
{
  char	device_class[128],		/* Device class */
	device_make_and_model[128],	/* Make and model, if known */
	device_info[128],		/* Device info/description */
	device_uri[1024],		/* Device URI */
	device_id[1024];		/* 1284 Device ID */
} cupsd_device_t;


/*
 * Local globals...
 */

static int		num_backends = 0,
					/* Total backends */
			active_backends = 0;
					/* Active backends */
static cupsd_backend_t	backends[MAX_BACKENDS];
					/* Array of backends */
static struct pollfd	backend_fds[MAX_BACKENDS];
					/* Array for poll() */
static cups_array_t	*devices;	/* Array of devices */
static int		normal_user;	/* Normal user ID */
static int		device_limit;	/* Maximum number of devices */
static int		send_class,	/* Send device-class attribute? */
			send_info,	/* Send device-info attribute? */
			send_make_and_model,
					/* Send device-make-and-model attribute? */
			send_uri,	/* Send device-uri attribute? */
			send_id;	/* Send device-id attribute? */


/*
 * Local functions...
 */

static int		add_device(const char *device_class,
				   const char *device_make_and_model,
				   const char *device_info,
				   const char *device_uri,
				   const char *device_id);
static int		compare_devices(cupsd_device_t *p0,
			                cupsd_device_t *p1);
static double		get_current_time(void);
static void		get_device(cupsd_backend_t *backend);
static int		start_backend(const char *backend, int root);
static void		stop_backend(cupsd_backend_t *backend);


/*
 * 'main()' - Scan for devices and return an IPP response.
 *
 * Usage:
 *
 *    cups-deviced request_id limit options
 */

int					/* O - Exit code */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  int		i;			/* Looping var */
  int		request_id;		/* Request ID */
  int		timeout;		/* Timeout in seconds */
  const char	*server_bin;		/* CUPS_SERVERBIN environment variable */
  char		filename[1024];		/* Backend directory filename */
  cups_dir_t	*dir;			/* Directory pointer */
  cups_dentry_t *dent;			/* Directory entry */
  double	current_time,		/* Current time */
		end_time;		/* Ending time */
  int		num_options;		/* Number of options */
  cups_option_t	*options;		/* Options */
  const char	*requested;		/* requested-attributes option */
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;		/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */


  setbuf(stderr, NULL);

 /*
  * Check the command-line...
  */

  if (argc != 5)
  {
    fputs("Usage: cups-deviced request-id limit user-id options\n", stderr);

    return (1);
  }

  request_id = atoi(argv[1]);
  if (request_id < 1)
  {
    fprintf(stderr, "cups-deviced: Bad request ID %d!\n", request_id);

    return (1);
  }

  device_limit = atoi(argv[2]);
  if (device_limit < 0)
  {
    fprintf(stderr, "cups-deviced: Bad limit %d!\n", device_limit);

    return (1);
  }

  normal_user = atoi(argv[3]);
  if (normal_user <= 0)
  {
    fprintf(stderr, "cups-deviced: Bad user %d!\n", normal_user);

    return (1);
  }

  timeout = 10; // TODO: Add timeout argument

  num_options = cupsParseOptions(argv[4], 0, &options);
  requested   = cupsGetOption("requested-attributes", num_options, options);

  if (!requested || strstr(requested, "all"))
  {
    send_class          = 1;
    send_info           = 1;
    send_make_and_model = 1;
    send_uri            = 1;
    send_id             = 1;
  }
  else
  {
    send_class          = strstr(requested, "device-class") != NULL;
    send_info           = strstr(requested, "device-info") != NULL;
    send_make_and_model = strstr(requested, "device-make-and-model") != NULL;
    send_uri            = strstr(requested, "device-uri") != NULL;
    send_id             = strstr(requested, "device-id") != NULL;
  }

 /*
  * Try opening the backend directory...
  */

  if ((server_bin = getenv("CUPS_SERVERBIN")) == NULL)
    server_bin = CUPS_SERVERBIN;

  snprintf(filename, sizeof(filename), "%s/backend", server_bin);

  if ((dir = cupsDirOpen(filename)) == NULL)
  {
    fprintf(stderr, "ERROR: [cups-deviced] Unable to open backend directory "
                    "\"%s\": %s", filename, strerror(errno));

    return (1);
  }

 /*
  * Setup the devices array...
  */

  devices = cupsArrayNew((cups_array_func_t)compare_devices, NULL);

 /*
  * Loop through all of the device backends...
  */

  while ((dent = cupsDirRead(dir)) != NULL)
  {
   /*
    * Skip entries that are not executable files...
    */

    if (!S_ISREG(dent->fileinfo.st_mode) ||
        !isalnum(dent->filename[0] & 255) ||
        (dent->fileinfo.st_mode & (S_IRUSR | S_IXUSR)) != (S_IRUSR | S_IXUSR))
      continue;

   /*
    * Backends without permissions for normal users run as root,
    * all others run as the unprivileged user...
    */

    start_backend(dent->filename,
                  !(dent->fileinfo.st_mode & (S_IRWXG | S_IRWXO)));
  }

  cupsDirClose(dir);

 /*
  * Collect devices...
  */

  if (getenv("SOFTWARE"))
    puts("Content-Type: application/ipp\n");

  cupsdSendIPPHeader(IPP_OK, request_id);
  cupsdSendIPPGroup(IPP_TAG_OPERATION);
  cupsdSendIPPString(IPP_TAG_CHARSET, "attributes-charset", "utf-8");
  cupsdSendIPPString(IPP_TAG_LANGUAGE, "attributes-natural-language", "en-US");

  end_time = get_current_time() + timeout;

  while (active_backends > 0 && (current_time = get_current_time()) < end_time)
  {
   /*
    * Collect the output from the backends...
    */

    timeout = (int)(1000 * (end_time - current_time));

    if (poll(backend_fds, num_backends, timeout) > 0)
    {
      for (i = 0; i < num_backends; i ++)
        if (backend_fds[i].revents)
	  get_device(backends + i);
    }
  }

  cupsdSendIPPTrailer();

 /*
  * Terminate any remaining backends and exit...
  */

  if (active_backends > 0)
  {
    for (i = 0; i < num_backends; i ++)
      if (backends[i].pid)
        stop_backend(backends + i);
  }

  return (0);
}


/*
 * 'add_device()' - Add a new device to the list.
 */

static int				/* O - 0 on success, -1 on error */
add_device(
    const char *device_class,		/* I - Device class */
    const char *device_make_and_model,	/* I - Device make and model */
    const char *device_info,		/* I - Device information */
    const char *device_uri,		/* I - Device URI */
    const char *device_id)		/* I - 1284 device ID */
{
  cupsd_device_t	*device,	/* New device */
			*temp;		/* Found device */


 /*
  * Allocate memory for the device record...
  */

  if ((device = calloc(1, sizeof(cupsd_device_t))) == NULL)
  {
    fputs("ERROR: [cups-deviced] Ran out of memory allocating a device!\n",
          stderr);
    return (-1);
  }

 /*
  * Copy the strings over...
  */

  strlcpy(device->device_class, device_class, sizeof(device->device_class));
  strlcpy(device->device_make_and_model, device_make_and_model,
          sizeof(device->device_make_and_model));
  strlcpy(device->device_info, device_info, sizeof(device->device_info));
  strlcpy(device->device_uri, device_uri, sizeof(device->device_uri));
  strlcpy(device->device_id, device_id, sizeof(device->device_id));

 /*
  * Add the device to the array and return...
  */

  if ((temp = cupsArrayFind(devices, device)) != NULL)
  {
   /*
    * Avoid duplicates!
    */

    free(device);
  }
  else
  {
    cupsArrayAdd(devices, device);

    if (device_limit <= 0 || cupsArrayCount(devices) < device_limit)
    {
     /*
      * Send device info...
      */

      cupsdSendIPPGroup(IPP_TAG_PRINTER);
      if (send_class)
	cupsdSendIPPString(IPP_TAG_KEYWORD, "device-class",
	                   device->device_class);
      if (send_info)
	cupsdSendIPPString(IPP_TAG_TEXT, "device-info", device->device_info);
      if (send_make_and_model)
	cupsdSendIPPString(IPP_TAG_TEXT, "device-make-and-model",
			   device->device_make_and_model);
      if (send_uri)
	cupsdSendIPPString(IPP_TAG_URI, "device-uri", device->device_uri);
      if (send_id)
	cupsdSendIPPString(IPP_TAG_TEXT, "device-id", device->device_id);

      fflush(stdout);
    }
  }

  return (0);
}


/*
 * 'compare_devices()' - Compare device names to eliminate duplicates.
 */

static int				/* O - Result of comparison */
compare_devices(cupsd_device_t *d0,	/* I - First device */
                cupsd_device_t *d1)	/* I - Second device */
{
  int		diff;			/* Difference between strings */


 /*
  * Sort devices by device-info, device-class, and device-uri...
  */

  if ((diff = cupsdCompareNames(d0->device_info, d1->device_info)) != 0)
    return (diff);
  else if ((diff = strcasecmp(d0->device_class, d1->device_class)) != 0)
    return (diff);
  else
    return (strcasecmp(d0->device_uri, d1->device_uri));
}


/*
 * 'get_current_time()' - Get the current time as a double value in seconds.
 */

static double				/* O - Time in seconds */
get_current_time(void)
{
  struct timeval	curtime;	/* Current time */


  gettimeofday(&curtime, NULL);

  return (curtime.tv_sec + 0.000001 * curtime.tv_usec);
}


/*
 * 'get_device()' - Get a device from a backend.
 */

static void
get_device(cupsd_backend_t *backend)	/* I - Backend to read from */
{
  char	line[2048],			/* Line from backend */
	dclass[64],			/* Device class */
	uri[1024],			/* Device URI */
	info[128],			/* Device info */
	make_model[256],		/* Make and model */
	device_id[1024];		/* 1284 device ID */


  if (cupsFileGets(backend->pipe, line, sizeof(line)))
  {
   /*
    * Each line is of the form:
    *
    *   class URI "make model" "name" ["1284 device ID"]
    */

    device_id[0] = '\0';

    if (sscanf(line,
	       "%63s%1023s%*[ \t]\"%255[^\"]\"%*[ \t]\"%127[^\"]\""
	       "%*[ \t]\"%1023[^\"]",
	       dclass, uri, make_model, info, device_id) < 4)
    {
     /*
      * Bad format; strip trailing newline and write an error message.
      */

      if (line[strlen(line) - 1] == '\n')
	line[strlen(line) - 1] = '\0';

      fprintf(stderr, "ERROR: [cups-deviced] Bad line from \"%s\": %s\n",
	      backend->backend, line);
    }
    else
    {
     /*
      * Add the device to the array of available devices...
      */

      if (!add_device(dclass, make_model, info, uri, device_id))
        fprintf(stderr, "DEBUG: [cups-deviced] Added device \"%s\"...\n", uri);
    }
  }
  else
  {
   /*
    * End of file...
    */

    if (waitpid(backend->pid, &(backend->status), WNOHANG) == backend->pid)
    {
      backend->pid = 0;
      cupsFileClose(backend->pipe);
      backend_fds[backend - backends].events = 0;
      active_backends --;
      // TODO: report status...
    }
  }
}


/*
 * 'start_backend()' - Run a backend to gather the available devices.
 */

static int				/* O - 0 on success, -1 on error */
start_backend(const char *name,		/* I - Backend to run */
              int        root)		/* I - Run as root? */
{
  const char		*server_bin;	/* CUPS_SERVERBIN environment variable */
  char			program[1024];	/* Full path to backend */
  int			fds[2];		/* Pipe file descriptors */
  cupsd_backend_t	*backend;	/* Current backend */


  if (num_backends >= MAX_BACKENDS)
  {
    fprintf(stderr, "ERROR: Too many backends (%d)!\n", num_backends);
    return (-1);
  }

  if (pipe(fds))
  {
    fprintf(stderr, "ERROR: Unable to create a pipe for \"%s\" - %s\n",
            name, strerror(errno));
    return (-1);
  }

  if ((server_bin = getenv("CUPS_SERVERBIN")) == NULL)
    server_bin = CUPS_SERVERBIN;

  snprintf(program, sizeof(program), "%s/backend/%s", server_bin, name);

  backend = backends + num_backends;

  if ((backend->pid = fork()) < 0)
  {
   /*
    * Error!
    */

    fprintf(stderr, "ERROR: [cups-deviced] Unable to fork for \"%s\" - %s\n",
            program, strerror(errno));
    close(fds[0]);
    close(fds[1]);
    return (-1);
  }
  else if (!backend->pid)
  {
   /*
    * Child comes here...
    */

    if (!getuid() && !root)
      setuid(normal_user);		/* Run as restricted user */

    close(0);				/* </dev/null */
    open("/dev/null", O_RDONLY);

    close(1);				/* >pipe */
    dup(fds[1]);

    close(fds[0]);			/* Close copies of pipes */
    close(fds[1]);

    execl(program, name, (char *)0);	/* Run it! */
    fprintf(stderr, "ERROR: [cups-deviced] Unable to execute \"%s\" - %s\n",
            program, strerror(errno));
    exit(1);
  }

 /*
  * Parent comes here, allocate a backend and open the input side of the
  * pipe...
  */

  close(fds[1]);

  backend_fds[num_backends].fd     = fds[0];
  backend_fds[num_backends].events = POLLIN;

  backend->backend = strdup(name);
  backend->status  = 0;
  backend->pipe    = cupsFileOpenFd(fds[0], "r");
  backend->count   = 0;

  active_backends ++;
  num_backends ++;

  return (0);
}


/*
 * 'stop_backend()' - Stop a backend.
 */

static void
stop_backend(cupsd_backend_t *backend)	/* I - Backend to stop */
{
  kill(backend->pid, SIGTERM);

  free(backend->backend);
}


/*
 * End of "$Id$".
 */
