/*
 * "$Id$"
 *
 *   Libusb interface code for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007-2008 by Apple Inc.
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

#include <usb.h>


/*
 * Local types...
 */

typedef struct usb_printer_s		/**** USB Printer Data ****/
{
  struct usb_device	*device;	/* Device info */
  int			conf,		/* Configuration */
			iface,		/* Interface */
			altset,		/* Alternate setting */
			write_endp,	/* Write endpoint */
			read_endp;	/* Read endpoint */
  struct usb_dev_handle	*handle;	/* Open handle to device */
} usb_printer_t;

typedef int (*usb_cb_t)(usb_printer_t *, const char *, const char *,
                        const void *);


/*
 * Local functions...
 */

static int		close_device(usb_printer_t *printer);
static usb_printer_t	*find_device(usb_cb_t cb, const void *data);
static int		get_device_id(usb_printer_t *printer, char *buffer,
			              size_t bufsize);
static int		list_cb(usb_printer_t *printer, const char *device_uri,
			        const char *device_id, const void *data);
static char		*make_device_uri(usb_printer_t *printer,
			                 const char *device_id,
					 char *uri, size_t uri_size);
static int		open_device(usb_printer_t *printer, int verbose);
static int		print_cb(usb_printer_t *printer, const char *device_uri,
			         const char *device_id, const void *data);


/*
 * 'list_devices()' - List the available printers.
 */

void
list_devices(void)
{
  fputs("DEBUG: list_devices\n", stderr);
  find_device(list_cb, NULL);
}


/*
 * 'print_device()' - Print a file to a USB device.
 */

int					/* O - Exit status */
print_device(const char *uri,		/* I - Device URI */
             const char *hostname,	/* I - Hostname/manufacturer */
             const char *resource,	/* I - Resource/modelname */
	     char       *options,	/* I - Device options/serial number */
	     int        print_fd,	/* I - File descriptor to print */
	     int        copies,		/* I - Copies to print */
	     int	argc,		/* I - Number of command-line arguments (6 or 7) */
	     char	*argv[])	/* I - Command-line arguments */
{
  usb_printer_t	*printer;		/* Printer */


  fputs("DEBUG: print_device\n", stderr);

  while ((printer = find_device(print_cb, uri)) == NULL)
  {
    _cupsLangPuts(stderr,
		  _("INFO: Waiting for printer to become available...\n"));
    sleep(5);
  }

  close_device(printer);

  return (CUPS_BACKEND_OK);
}


/*
 * 'close_device()' - Close the connection to the USB printer.
 */

static int				/* I - 0 on success, -1 on failure */
close_device(usb_printer_t *printer)	/* I - Printer */
{
  if (printer->handle)
  {
    usb_close(printer->handle);
    printer->handle = NULL;
  }

  return (0);
}


/*
 * 'find_device()' - Find or enumerate USB printers.
 */

static usb_printer_t *			/* O - Found printer */
find_device(usb_cb_t   cb,		/* I - Callback function */
            const void *data)		/* I - User data for callback */
{
  struct usb_bus	*bus;		/* Current bus */
  struct usb_device	*device;	/* Current device */
  struct usb_config_descriptor *confptr;/* Pointer to current configuration */
  struct usb_interface	*ifaceptr;	/* Pointer to current interface */
  struct usb_interface_descriptor *altptr;
					/* Pointer to current alternate setting */
  struct usb_endpoint_descriptor *endpptr;
					/* Pointer to current endpoint */
  int			conf,		/* Current configuration */
			iface,		/* Current interface */
			altset,		/* Current alternate setting */
			protocol,	/* Current protocol */
			endp,		/* Current endpoint */
			read_endp,	/* Current read endpoint */
			write_endp;	/* Current write endpoint */
  char			device_id[1024],/* IEEE-1284 device ID */
			device_uri[1024];
					/* Device URI */
  static usb_printer_t	printer;	/* Current printer */


 /*
  * Initialize libusb...
  */

  usb_init();
  fprintf(stderr, "DEBUG: usb_find_busses=%d\n", usb_find_busses());
  fprintf(stderr, "DEBUG: usb_find_devices=%d\n", usb_find_devices());

 /*
  * Then loop through the devices it found...
  */

  for (bus = usb_get_busses(); bus; bus = bus->next)
    for (device = bus->devices; device; device = device->next)
    {
     /*
      * Ignore devices with no configuration data and anything that is not
      * a printer...
      */

      if (!device->config || !device->descriptor.idVendor ||
          !device->descriptor.idProduct)
	continue;

      for (conf = 0, confptr = device->config;
           conf < device->descriptor.bNumConfigurations;
	   conf ++, confptr ++)
        for (iface = 0, ifaceptr = confptr->interface;
	     iface < confptr->bNumInterfaces;
	     iface ++, ifaceptr ++)
        {
	 /*
	  * Some printers offer multiple interfaces...
	  */

          protocol   = 0;

	  for (altset = 0, altptr = ifaceptr->altsetting;
	       altset < ifaceptr->num_altsetting;
	       altset ++, altptr ++)
          {
	   /*
	    * Currently we only support unidirectional and bidirectional
	    * printers.  Future versions of this code will support the
	    * 1284.4 (packet mode) protocol as well.
	    */

	    if (altptr->bInterfaceClass != USB_CLASS_PRINTER ||
	        altptr->bInterfaceSubClass != 1 ||
		(altptr->bInterfaceProtocol != 1 &&	/* Unidirectional */
		 altptr->bInterfaceProtocol != 2) ||	/* Bidirectional */
		altptr->bInterfaceProtocol < protocol)
	      continue;

	    read_endp  = -1;
	    write_endp = -1;

	    for (endp = 0, endpptr = altptr->endpoint;
	         endp < altptr->bNumEndpoints;
		 endp ++, endpptr ++)
              if ((endpptr->bmAttributes & USB_ENDPOINT_TYPE_MASK) ==
	              USB_ENDPOINT_TYPE_BULK)
	      {
	        if (endpptr->bEndpointAddress & USB_ENDPOINT_DIR_MASK)
		  read_endp = endp;
		else
		  write_endp = endp;
	      }

            if (write_endp >= 0)
	    {
	     /*
	      * Save the best match so far...
	      */

              protocol           = altptr->bInterfaceProtocol;
	      printer.altset     = altset;
	      printer.write_endp = write_endp;
	      printer.read_endp  = read_endp;
	    }
	  }

	  if (protocol > 0)
	  {
	    printer.device = device;
	    printer.conf   = conf;
	    printer.iface  = iface;
	    printer.handle = NULL;

            if (!open_device(&printer, data != NULL))
	    {
	      if (!get_device_id(&printer, device_id, sizeof(device_id)))
	      {
                make_device_uri(&printer, device_id, device_uri,
		                sizeof(device_uri));

	        if ((*cb)(&printer, device_uri, device_id, data))
		  return (&printer);
              }

              close_device(&printer);
	    }
	  }
	}
    }

 /*
  * If we get this far without returning, then we haven't found a printer
  * to print to...
  */

  return (NULL);
}


/*
 * 'get_device_id()' - Get the IEEE-1284 device ID for the printer.
 */

static int				/* O - 0 on success, -1 on error */
get_device_id(usb_printer_t *printer,	/* I - Printer */
              char          *buffer,	/* I - String buffer */
              size_t        bufsize)	/* I - Number of bytes in buffer */
{
  int	length;				/* Length of device ID */


  if (usb_control_msg(printer->handle,
                      USB_TYPE_CLASS | USB_ENDPOINT_IN | USB_RECIP_INTERFACE,
		      0, 0,
		      (printer->iface << 8) |
		          printer->device->config[printer->conf].
			      interface[printer->iface].
			      altsetting[printer->altset].bAlternateSetting,
		      buffer, bufsize, 5000) < 0)
  {
    *buffer = '\0';
    return (-1);
  }

 /*
  * Extract the length of the device ID string from the first two
  * bytes.  The 1284 spec says the length is stored MSB first...
  */

  length = (((unsigned)buffer[0] & 255) << 8) +
	   ((unsigned)buffer[1] & 255);

 /*
  * Check to see if the length is larger than our buffer; first
  * assume that the vendor incorrectly implemented the 1284 spec,
  * and then limit the length to the size of our buffer...
  */

  if (length > (bufsize - 2))
    length = (((unsigned)buffer[1] & 255) << 8) +
	     ((unsigned)buffer[0] & 255);

  if (length > (bufsize - 2))
    length = bufsize - 2;

 /*
  * Copy the device ID text to the beginning of the buffer and
  * nul-terminate.
  */

  memmove(buffer, buffer + 2, length);
  buffer[length] = '\0';

  return (0);
}


/*
 * 'list_cb()' - List USB printers for discovery.
 */

static int				/* O - 0 to continue, 1 to stop */
list_cb(usb_printer_t *printer,		/* I - Printer */
        const char    *device_uri,	/* I - Device URI */
        const char    *device_id,	/* I - IEEE-1284 device ID */
        const void    *data)		/* I - User data (not used) */
{
  char	make_model[1024];		/* Make and model */


 /*
  * Get the device URI and make/model strings...
  */

  backendGetMakeModel(device_id, make_model, sizeof(make_model));

 /*
  * Report the printer...
  */

  printf("direct %s \"%s\" \"%s USB\" \"%s\"\n", device_uri, make_model,
         make_model, device_id);
  fflush(stdout);

 /*
  * Keep going...
  */

  return (0);
}


/*
 * 'make_device_uri()' - Create a device URI for a USB printer.
 */

static char *				/* O - Device URI */
make_device_uri(
    usb_printer_t *printer,		/* I - Printer */
    const char    *device_id,		/* I - IEEE-1284 device ID */
    char          *uri,			/* I - Device URI buffer */
    size_t        uri_size)		/* I - Size of device URI buffer */
{
  char		options[1024];		/* Device URI options */
  int		num_values;		/* Number of 1284 parameters */
  cups_option_t	*values;		/* 1284 parameters */
  const char	*mfg,			/* Manufacturer */
		*mdl,			/* Model */
		*des,			/* Description */
		*sern;			/* Serial number */
  char		tempmfg[256],		/* Temporary manufacturer string */
		tempsern[256],		/* Temporary serial number string */
		*tempptr;		/* Pointer into temp string */


 /*
  * Get the make, model, and serial numbers...
  */

  num_values = _ppdGet1284Values(device_id, &values);

  if ((sern = cupsGetOption("SERIALNUMBER", num_values, values)) == NULL)
    if ((sern = cupsGetOption("SERN", num_values, values)) == NULL)
      if ((sern = cupsGetOption("SN", num_values, values)) == NULL)
      {
       /*
        * Try getting the serial number from the device itself...
	*/

        int length = usb_get_string_simple(printer->handle,
	                                   printer->device->descriptor.
					       iSerialNumber,
				           tempsern, sizeof(tempsern) - 1);
        if (length > 0)
	{
	  tempsern[length] = '\0';
	  sern             = tempsern;
	}
      }

  if ((mfg = cupsGetOption("MANUFACTURER", num_values, values)) == NULL)
    mfg = cupsGetOption("MFG", num_values, values);

  if ((mdl = cupsGetOption("MODEL", num_values, values)) == NULL)
    mdl = cupsGetOption("MDL", num_values, values);

#ifdef __APPLE__
 /*
  * To maintain compatibility with the original IOKit-based backend on Mac OS X,
  * don't map manufacturer names...
  */

  if (!mfg)

#else
 /*
  * To maintain compatibility with the original character device backend on
  * Linux and *BSD, map manufacturer names...
  */

  if (mfg)
  {
    if (!strcasecmp(mfg, "Hewlett-Packard"))
      mfg = "HP";
    else if (!strcasecmp(mfg, "Lexmark International"))
      mfg = "Lexmark";
  }
  else
#endif /* __APPLE__ */
  {
   /*
    * No manufacturer?  Use the model string or description...
    */

    if (mdl)
      _ppdNormalizeMakeAndModel(mdl, tempmfg, sizeof(tempmfg));
    else if ((des = cupsGetOption("DESCRIPTION", num_values, values)) != NULL ||
             (des = cupsGetOption("DES", num_values, values)) != NULL)
      _ppdNormalizeMakeAndModel(des, tempmfg, sizeof(tempmfg));
    else
      strlcpy(tempmfg, "Unknown", sizeof(tempmfg));

    if ((tempptr = strchr(tempmfg, ' ')) != NULL)
      *tempptr = '\0';

    mfg = tempmfg;
  }

 /*
  * Generate the device URI from the manufacturer, model, serial number,
  * and interface number...
  */

  if (sern)
  {
    if (printer->iface > 0)
      snprintf(options, sizeof(options), "?serial=%s&interface=%d", sern,
               printer->iface);
    else
      snprintf(options, sizeof(options), "?serial=%s", sern);
  }
  else if (printer->iface > 0)
    snprintf(options, sizeof(options), "?interface=%d", printer->iface);
  else
    options[0] = '\0';

  httpAssembleURIf(HTTP_URI_CODING_ALL, uri, uri_size, "usb", NULL, mfg, 0,
		   "/%s%s", mdl, options);

  cupsFreeOptions(num_values, values);

  return (uri);
}


/*
 * 'open_device()' - Open a connection to the USB printer.
 */

static int				/* O - 0 on success, -1 on error */
open_device(usb_printer_t *printer,	/* I - Printer */
            int           verbose)	/* I - Update connecting-to-device state? */
{
  int	number;				/* Configuration/interface/altset numbers */


 /*
  * Return immediately if we are already connected...
  */

  if (printer->handle)
    return (0);

 /*
  * Try opening the printer...
  */

  if ((printer->handle = usb_open(printer->device)) == NULL)
    return (-1);

 /*
  * Then set the desired configuration...
  */

  if (verbose)
    fputs("STATE: +connecting-to-device\n", stderr);

  number = printer->device->config[printer->conf].bConfigurationValue;
  while (usb_set_configuration(printer->handle, number) < 0)
  {
    if (errno != EBUSY)
      fprintf(stderr, "DEBUG: Failed to set configuration %d for %04x:%04x\n",
              number, printer->device->descriptor.idVendor,
	      printer->device->descriptor.idProduct);

    goto error;
  }

 /*
  * Claim interfaces as needed...
  */

  number = printer->device->config[printer->conf].interface[printer->iface].
               altsetting[printer->altset].bInterfaceNumber;
  while (usb_claim_interface(printer->handle, number) < 0)
  {
    if (errno != EBUSY)
      fprintf(stderr, "DEBUG: Failed to claim interface %d for %04x:%04x\n",
              number, printer->device->descriptor.idVendor,
	      printer->device->descriptor.idProduct);

    goto error;
  }

  if (number != 0)
    while (usb_claim_interface(printer->handle, 0) < 0)
    {
      if (errno != EBUSY)
	fprintf(stderr, "DEBUG: Failed to claim interface 0 for %04x:%04x\n",
		printer->device->descriptor.idVendor,
		printer->device->descriptor.idProduct);

      goto error;
    }

 /*
  * Set alternate setting...
  */

  number = printer->device->config[printer->conf].interface[printer->iface].
               altsetting[printer->altset].bAlternateSetting;
  while (usb_set_altinterface(printer->handle, number) < 0)
  {
    if (errno != EBUSY)
      fprintf(stderr,
              "DEBUG: Failed to set alternate interface %d for %04x:%04x\n",
              number, printer->device->descriptor.idVendor,
	      printer->device->descriptor.idProduct);

    goto error;
  }

  if (verbose)
    fputs("STATE: -connecting-to-device\n", stderr);

  return (0);

 /*
  * If we get here, there was a hard error...
  */

  error:

  if (verbose)
    fputs("STATE: -connecting-to-device\n", stderr);

  usb_close(printer->handle);
  printer->handle = NULL;

  return (-1);
}


/*
 * 'print_cb()' - Find a USB printer for printing.
 */

static int				/* O - 0 to continue, 1 to stop (found) */
print_cb(usb_printer_t *printer,	/* I - Printer */
         const char    *device_uri,	/* I - Device URI */
         const char    *device_id,	/* I - IEEE-1284 device ID */
         const void    *data)		/* I - User data (make, model, S/N) */
{
  return (!strcmp((char *)data, device_uri));
}


/*
 * End of "$Id$".
 */

