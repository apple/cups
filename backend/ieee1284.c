/*
 * "$Id$"
 *
 *   IEEE-1284 support functions for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2006 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Easy Software Products and are protected by Federal
 *   copyright law.  Distribution and use rights are outlined in the file
 *   "LICENSE" which should have been included with this file.  If this
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
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   get_device_id() - Get the IEEE-1284 device ID string and corresponding
 *                     URI.
 */

/*
 * Include necessary headers.
 */

#include <cups/debug.h>
#ifdef __linux
#  include <sys/ioctl.h>
#  include <linux/lp.h>
#  define IOCNR_GET_DEVICE_ID		1
#  define LPIOC_GET_DEVICE_ID(len)	_IOC(_IOC_READ, 'P', IOCNR_GET_DEVICE_ID, len)
#endif /* __linux */

#ifdef __sun
#  ifdef __sparc
#    include <sys/ecppio.h>
#  else
#    include <sys/ioccom.h>
#    include <sys/ecppsys.h>
#  endif /* __sparc */
#endif /* __sun */


/*
 * 'get_device_id()' - Get the IEEE-1284 device ID string and
 *                     corresponding URI.
 */

int					/* O - 0 on success, -1 on failure */
get_device_id(
    int        fd,			/* I - File descriptor */
    char       *device_id,		/* O - 1284 device ID */
    int        device_id_size,		/* I - Size of buffer */
    char       *make_model,		/* O - Make/model */
    int        make_model_size,		/* I - Size of buffer */
    const char *scheme,			/* I - URI scheme */
    char       *uri,			/* O - Device URI */
    int        uri_size)		/* I - Size of buffer */
{
  char	*attr,				/* 1284 attribute */
  	*delim,				/* 1284 delimiter */
	*uriptr,			/* Pointer into URI */
	*mfg,				/* Manufacturer string */
	*mdl,				/* Model string */
	serial_number[1024];		/* Serial number string */
#ifdef __linux
  int	length;				/* Length of device ID info */
#endif /* __linux */
#ifdef __sun
  struct ecpp_device_id did;		/* Device ID buffer */
#endif /* __sun */

  DEBUG_printf(("get_device_id(fd=%d, device_id=%p, device_id_size=%d, "
                "make_model=%p, make_model_size=%d, scheme=\"%s\", "
		"uri=%p, uri_size=%d)\n", fd, device_id, device_id_size,
		make_model, make_model_size, scheme ? scheme : "(null)",
		uri, uri_size));

 /*
  * Range check input...
  */

  if (fd < 0 ||
      !device_id || device_id_size < 32 ||
      !make_model || make_model_size < 32)
  {
    DEBUG_puts("get_device_id: Bad args!");
    return (-1);
  }

  *device_id  = '\0';
  *make_model = '\0';

  if (uri)
    *uri = '\0';

 /*
  * Get the device ID string...
  */

#ifdef __linux
  if (!ioctl(fd, LPIOC_GET_DEVICE_ID(device_id_size), device_id))
  {
   /*
    * Extract the length of the device ID string from the first two
    * bytes.  The 1284 spec says the length is stored MSB first...
    */

    length = (((unsigned)device_id[0] & 255) << 8) +
	     ((unsigned)device_id[1] & 255);

   /*
    * Check to see if the length is larger than our buffer; first
    * assume that the vendor incorrectly implemented the 1284 spec,
    * and then limit the length to the size of our buffer...
    */

    if (length > (device_id_size - 2))
      length = (((unsigned)device_id[1] & 255) << 8) +
	       ((unsigned)device_id[0] & 255);

    if (length > (device_id_size - 2))
      length = device_id_size - 2;

   /*
    * Copy the device ID text to the beginning of the buffer and
    * nul-terminate.
    */

    memmove(device_id, device_id + 2, length);
    device_id[length] = '\0';
  }
#  ifdef DEBUG
  else
    printf("get_device_id: ioctl failed - %s\n", strerror(errno));
#  endif /* DEBUG */
#endif /* __linux */

#if defined(__sun) && defined(ECPPIOC_GETDEVID)
  did.mode = ECPP_CENTRONICS;
  did.len  = device_id_size - 1;
  did.rlen = 0;
  did.addr = device_id;

  if (!ioctl(fd, ECPPIOC_GETDEVID, &did))
  {
   /*
    * Nul-terminate the device ID text.
    */

    if (did.rlen < (device_id_size - 1))
      device_id[did.rlen] = '\0';
    else
      device_id[device_id_size - 1] = '\0';
  }
#  ifdef DEBUG
  else
    printf("get_device_id: ioctl failed - %s\n", strerror(errno));
#  endif /* DEBUG */
#endif /* __sun && ECPPIOC_GETDEVID */

  DEBUG_printf(("get_device_id: device_id=\"%s\"\n", device_id));

  if (!*device_id)
    return (-1);

 /*
  * Look for the description field...
  */

  if ((attr = strstr(device_id, "DES:")) != NULL)
    attr += 4;
  else if ((attr = strstr(device_id, "DESCRIPTION:")) != NULL)
    attr += 12;

  if (attr)
  {
   /*
    * Make sure the description contains something useful, since some
    * printer manufacturers (HP) apparently don't follow the standards
    * they helped to define...
    *
    * Here we require the description to be 8 or more characters in length,
    * containing at least one space and one letter.
    */

    if ((delim = strchr(attr, ';')) == NULL)
      delim = attr + strlen(attr);

    if ((delim - attr) < 8)
      attr = NULL;
    else
    {
      char	*ptr;			/* Pointer into description */
      int	letters,		/* Number of letters seen */
		spaces;			/* Number of spaces seen */


      for (ptr = attr, letters = 0, spaces = 0; ptr < delim; ptr ++)
      {
        if (isspace(*ptr & 255))
	  spaces ++;
	else if (isalpha(*ptr & 255))
	  letters ++;

        if (spaces && letters)
	  break;
      }

      if (!spaces || !letters)
        attr = NULL;
    }
  }

  if ((mfg = strstr(device_id, "MANUFACTURER:")) != NULL)
    mfg += 13;
  else if ((mfg = strstr(device_id, "MFG:")) != NULL)
    mfg += 4;

  if ((mdl = strstr(device_id, "MODEL:")) != NULL)
    mdl += 6;
  else if ((mdl = strstr(device_id, "MDL:")) != NULL)
    mdl += 4;

  if (attr)
  {
   /*
    * Use description...
    */

    if (!strncasecmp(attr, "Hewlett-Packard hp ", 19))
    {
     /*
      * Check for a common HP bug...
      */

      strlcpy(make_model, "HP ", make_model_size);
      strlcpy(make_model + 3, attr + 19, make_model_size - 3);
    }
    else if (!strncasecmp(attr, "Hewlett-Packard ", 16))
    {
      strlcpy(make_model, "HP ", make_model_size);
      strlcpy(make_model + 3, attr + 16, make_model_size - 3);
    }
    else
    {
      strlcpy(make_model, attr, make_model_size);
    }
  }
  else if (mfg && mdl)
  {
   /*
    * Build a make-model string from the manufacturer and model attributes...
    */

    if (!strncasecmp(mfg, "Hewlett-Packard", 15))
      strlcpy(make_model, "HP", make_model_size);
    else
      strlcpy(make_model, mfg, make_model_size);

    if ((delim = strchr(make_model, ';')) != NULL)
      *delim = '\0';

    if (!strncasecmp(make_model, mdl, strlen(make_model)))
    {
     /*
      * Just copy model string, since it has the manufacturer...
      */

      strlcpy(make_model, mdl, make_model_size);
    }
    else
    {
     /*
      * Concatenate the make and model...
      */

      strlcat(make_model, " ", make_model_size);
      strlcat(make_model, mdl, make_model_size);
    }
  }
  else
  {
   /*
    * Use "Unknown" as the printer make and model...
    */

    strlcpy(make_model, "Unknown", make_model_size);
  }

  if ((delim = strchr(make_model, ';')) != NULL)
    *delim = '\0';

  if (scheme && uri && uri_size > 32)
  {
   /*
    * Look for the serial number field...
    */

    if ((attr = strstr(device_id, "SERN:")) != NULL)
      attr += 5;
    else if ((attr = strstr(device_id, "SERIALNUMBER:")) != NULL)
      attr += 13;
    else if ((attr = strstr(device_id, ";SN:")) != NULL)
      attr += 4;

    if (attr)
    {
      strlcpy(serial_number, attr, sizeof(serial_number));

      if ((delim = strchr(serial_number, ';')) != NULL)
	*delim = '\0';
    }
    else
      serial_number[0] = '\0';

   /*
    * Generate the device URI from the make_model and serial number strings.
    */

    snprintf(uri, uri_size, "%s://", scheme);
    for (uriptr = uri + strlen(uri), delim = make_model;
	 *delim && uriptr < (uri + uri_size - 1);
	 delim ++)
      if (*delim == ' ')
      {
	delim ++;
	*uriptr++ = '/';
	break;
      }
      else
	*uriptr++ = *delim;

    for (; *delim && uriptr < (uri + uri_size - 3); delim ++)
      if (*delim == ' ')
      {
	*uriptr++ = '%';
	*uriptr++ = '2';
	*uriptr++ = '0';
      }
      else
	*uriptr++ = *delim;

    *uriptr = '\0';

    if (serial_number[0])
    {
     /*
      * Add the serial number to the URI...
      */

      strlcat(uri, "?serial=", uri_size);
      strlcat(uri, serial_number, uri_size);
    }
  }

  return (0);
}


/*
 * End of "$Id$".
 */
