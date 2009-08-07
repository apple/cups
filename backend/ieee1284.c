/*
 * "$Id: ieee1284.c 7687 2008-06-24 01:28:36Z mike $"
 *
 *   IEEE-1284 support functions for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007-2008 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   "LICENSE" which should have been included with this file.  If this
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   backendGetDeviceID()  - Get the IEEE-1284 device ID string and
 *                           corresponding URI.
 *   backendGetMakeModel() - Get the make and model string from the device ID.
 */

/*
 * Include necessary headers.
 */

#include "backend-private.h"

#ifdef __linux
#  include <sys/ioctl.h>
#  include <linux/lp.h>
#  define IOCNR_GET_DEVICE_ID		1
#  define LPIOC_GET_DEVICE_ID(len)	_IOC(_IOC_READ, 'P', IOCNR_GET_DEVICE_ID, len)
#  include <linux/parport.h>
#  include <linux/ppdev.h>
#  include <unistd.h>
#  include <fcntl.h>
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
 * 'backendGetDeviceID()' - Get the IEEE-1284 device ID string and
 *                          corresponding URI.
 */

int					/* O - 0 on success, -1 on failure */
backendGetDeviceID(
    int        fd,			/* I - File descriptor */
    char       *device_id,		/* O - 1284 device ID */
    int        device_id_size,		/* I - Size of buffer */
    char       *make_model,		/* O - Make/model */
    int        make_model_size,		/* I - Size of buffer */
    const char *scheme,			/* I - URI scheme */
    char       *uri,			/* O - Device URI */
    int        uri_size)		/* I - Size of buffer */
{
#ifdef __APPLE__ /* This function is a no-op */
  return (-1);

#else /* Get the device ID from the specified file descriptor... */
#  ifdef __linux
  int	length;				/* Length of device ID info */
  int   got_id = 0;
#  endif /* __linux */
#  if defined(__sun) && defined(ECPPIOC_GETDEVID)
  struct ecpp_device_id did;		/* Device ID buffer */
#  endif /* __sun && ECPPIOC_GETDEVID */


  DEBUG_printf(("backendGetDeviceID(fd=%d, device_id=%p, device_id_size=%d, "
                "make_model=%p, make_model_size=%d, scheme=\"%s\", "
		"uri=%p, uri_size=%d)\n", fd, device_id, device_id_size,
		make_model, make_model_size, scheme ? scheme : "(null)",
		uri, uri_size));

 /*
  * Range check input...
  */

  if (!device_id || device_id_size < 32)
  {
    DEBUG_puts("backendGetDeviceID: Bad args!");
    return (-1);
  }

  if (make_model)
    *make_model = '\0';

  if (fd >= 0)
  {
   /*
    * Get the device ID string...
    */

    *device_id = '\0';

#  ifdef __linux
    if (ioctl(fd, LPIOC_GET_DEVICE_ID(device_id_size), device_id))
    {
     /*
      * Linux has to implement things differently for every device it seems.
      * Since the standard parallel port driver does not provide a simple
      * ioctl() to get the 1284 device ID, we have to open the "raw" parallel
      * device corresponding to this port and do some negotiation trickery
      * to get the current device ID.
      */

      if (uri && !strncmp(uri, "parallel:/dev/", 14))
      {
	char	devparport[16];		/* /dev/parportN */
	int	devparportfd,		/* File descriptor for raw device */
		  mode;			/* Port mode */


       /*
	* Since the Linux parallel backend only supports 4 parallel port
	* devices, just grab the trailing digit and use it to construct a
	* /dev/parportN filename...
	*/

	snprintf(devparport, sizeof(devparport), "/dev/parport%s",
		 uri + strlen(uri) - 1);

	if ((devparportfd = open(devparport, O_RDWR | O_NOCTTY)) != -1)
	{
	 /*
	  * Claim the device...
	  */

	  if (!ioctl(devparportfd, PPCLAIM))
	  {
	    fcntl(devparportfd, F_SETFL, fcntl(devparportfd, F_GETFL) | O_NONBLOCK);

	    mode = IEEE1284_MODE_COMPAT;

	    if (!ioctl(devparportfd, PPNEGOT, &mode))
	    {
	     /*
	      * Put the device into Device ID mode...
	      */

	      mode = IEEE1284_MODE_NIBBLE | IEEE1284_DEVICEID;

	      if (!ioctl(devparportfd, PPNEGOT, &mode))
	      {
	       /*
		* Read the 1284 device ID...
		*/

		if ((length = read(devparportfd, device_id,
				   device_id_size - 1)) >= 2)
		{
		  device_id[length] = '\0';
		  got_id = 1;
		}
	      }
	    }

	   /*
	    * Release the device...
	    */

	    ioctl(devparportfd, PPRELEASE);
	  }

	  close(devparportfd);
	}
      }
    }
    else
      got_id = 1;

    if (got_id)
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

      if (length > device_id_size)
	length = (((unsigned)device_id[1] & 255) << 8) +
		 ((unsigned)device_id[0] & 255);

      if (length > device_id_size)
	length = device_id_size;

     /*
      * The length field counts the number of bytes in the string
      * including the length field itself (2 bytes).
      */

      length -= 2;

     /*
      * Copy the device ID text to the beginning of the buffer and
      * nul-terminate.
      */

      memmove(device_id, device_id + 2, length);
      device_id[length] = '\0';
    }
#    ifdef DEBUG
    else
      DEBUG_printf(("backendGetDeviceID: ioctl failed - %s\n",
                    strerror(errno)));
#    endif /* DEBUG */
#  endif /* __linux */

#   if defined(__sun) && defined(ECPPIOC_GETDEVID)
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
#    ifdef DEBUG
    else
      DEBUG_printf(("backendGetDeviceID: ioctl failed - %s\n",
                    strerror(errno)));
#    endif /* DEBUG */
#  endif /* __sun && ECPPIOC_GETDEVID */
  }

  DEBUG_printf(("backendGetDeviceID: device_id=\"%s\"\n", device_id));

  if (scheme && uri)
    *uri = '\0';

  if (!*device_id)
    return (-1);

 /*
  * Get the make and model...
  */

  if (make_model)
    backendGetMakeModel(device_id, make_model, make_model_size);

 /*
  * Then generate a device URI...
  */

  if (scheme && uri && uri_size > 32)
  {
    int			num_values;	/* Number of keys and values */
    cups_option_t	*values;	/* Keys and values in device ID */
    const char		*mfg,		/* Manufacturer */
			*mdl,		/* Model */
			*sern;		/* Serial number */
    char		temp[256],	/* Temporary manufacturer string */
			*tempptr;	/* Pointer into temp string */


   /*
    * Get the make, model, and serial numbers...
    */

    num_values = _ppdGet1284Values(device_id, &values);

    if ((sern = cupsGetOption("SERIALNUMBER", num_values, values)) == NULL)
      if ((sern = cupsGetOption("SERN", num_values, values)) == NULL)
        sern = cupsGetOption("SN", num_values, values);

    if ((mfg = cupsGetOption("MANUFACTURER", num_values, values)) == NULL)
      mfg = cupsGetOption("MFG", num_values, values);

    if ((mdl = cupsGetOption("MODEL", num_values, values)) == NULL)
      mdl = cupsGetOption("MDL", num_values, values);

    if (mfg)
    {
      if (!strcasecmp(mfg, "Hewlett-Packard"))
        mfg = "HP";
      else if (!strcasecmp(mfg, "Lexmark International"))
        mfg = "Lexmark";
    }
    else
    {
      strlcpy(temp, make_model, sizeof(temp));

      if ((tempptr = strchr(temp, ' ')) != NULL)
        *tempptr = '\0';

      mfg = temp;
    }

    if (!strncasecmp(mdl, mfg, strlen(mfg)))
    {
      mdl += strlen(mfg);

      while (isspace(*mdl & 255))
        mdl ++;
    }

   /*
    * Generate the device URI from the manufacturer, make_model, and
    * serial number strings.
    */

    httpAssembleURIf(HTTP_URI_CODING_ALL, uri, uri_size, scheme, NULL, mfg, 0,
                     "/%s%s%s", mdl, sern ? "?serial=" : "", sern ? sern : "");

    cupsFreeOptions(num_values, values);
  }

  return (0);
#endif /* __APPLE__ */
}


/*
 * 'backendGetMakeModel()' - Get the make and model string from the device ID.
 */

int					/* O - 0 on success, -1 on failure */
backendGetMakeModel(
    const char *device_id,		/* O - 1284 device ID */
    char       *make_model,		/* O - Make/model */
    int        make_model_size)		/* I - Size of buffer */
{
  int		num_values;		/* Number of keys and values */
  cups_option_t	*values;		/* Keys and values */
  const char	*mfg,			/* Manufacturer string */
		*mdl,			/* Model string */
		*des;			/* Description string */


  DEBUG_printf(("backendGetMakeModel(device_id=\"%s\", "
                "make_model=%p, make_model_size=%d)\n", device_id,
		make_model, make_model_size));

 /*
  * Range check input...
  */

  if (!device_id || !*device_id || !make_model || make_model_size < 32)
  {
    DEBUG_puts("backendGetMakeModel: Bad args!");
    return (-1);
  }

  *make_model = '\0';

 /*
  * Look for the description field...
  */

  num_values = _ppdGet1284Values(device_id, &values);

  if ((mdl = cupsGetOption("MODEL", num_values, values)) == NULL)
    mdl = cupsGetOption("MDL", num_values, values);

  if (mdl)
  {
   /*
    * Build a make-model string from the manufacturer and model attributes...
    */

    if ((mfg = cupsGetOption("MANUFACTURER", num_values, values)) == NULL)
      mfg = cupsGetOption("MFG", num_values, values);

    if (!mfg || !strncasecmp(mdl, mfg, strlen(mfg)))
    {
     /*
      * Just copy the model string, since it has the manufacturer...
      */

      _ppdNormalizeMakeAndModel(mdl, make_model, make_model_size);
    }
    else
    {
     /*
      * Concatenate the make and model...
      */

      char	temp[1024];		/* Temporary make and model */

      snprintf(temp, sizeof(temp), "%s %s", mfg, mdl);
      _ppdNormalizeMakeAndModel(temp, make_model, make_model_size);
    }
  }
  else if ((des = cupsGetOption("DESCRIPTION", num_values, values)) != NULL ||
           (des = cupsGetOption("DES", num_values, values)) != NULL)
  {
   /*
    * Make sure the description contains something useful, since some
    * printer manufacturers (HP) apparently don't follow the standards
    * they helped to define...
    *
    * Here we require the description to be 8 or more characters in length,
    * containing at least one space and one letter.
    */

    if (strlen(des) >= 8)
    {
      const char	*ptr;		/* Pointer into description */
      int		letters,	/* Number of letters seen */
			spaces;		/* Number of spaces seen */


      for (ptr = des, letters = 0, spaces = 0; *ptr; ptr ++)
      {
	if (isspace(*ptr & 255))
	  spaces ++;
	else if (isalpha(*ptr & 255))
	  letters ++;

	if (spaces && letters)
	  break;
      }

      if (spaces && letters)
        _ppdNormalizeMakeAndModel(des, make_model, make_model_size);
    }
  }

  if (!make_model[0])
  {
   /*
    * Use "Unknown" as the printer make and model...
    */

    strlcpy(make_model, "Unknown", make_model_size);
  }

  cupsFreeOptions(num_values, values);

  return (0);
}


/*
 * End of "$Id: ieee1284.c 7687 2008-06-24 01:28:36Z mike $".
 */
