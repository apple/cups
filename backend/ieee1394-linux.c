/*
 * "$Id$"
 *
 *   Linux IEEE-1394 glue for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007 by Apple Inc.
 *   Copyright 2002 by Easy Software Products, all rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or
 *   without modification, are permitted provided that the
 *   following conditions are met:
 *
 *     1. Redistributions of source code must retain the above
 *	  copyright notice, this list of conditions and the
 *	  following disclaimer.
 *
 *     2. Redistributions in binary form must reproduce the
 *	  above copyright notice, this list of conditions and
 *	  the following disclaimer in the documentation and/or
 *	  other materials provided with the distribution.
 *
 *     3. All advertising materials mentioning features or use
 *	  of this software must display the following
 *	  acknowledgement:
 *
 *	    This product includes software developed by Easy
 *	    Software Products.
 *
 *     4. The name of Easy Software Products may not be used to
 *	  endorse or promote products derived from this software
 *	  without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS
 *   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
 *   BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *   MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *   DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS
 *   BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 *   OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 *   DAMAGE.
 *
 * Contents:
 *
 *   get_device_id()  - Get the IEEE-1284 device ID for a node...
 *   get_unit_type()  - Get the unit type for a node...
 *   show_data()      - Show a data node...
 *   show_dir()       - Show a directory list...
 *   ieee1394_list()  - List the available printer devices.
 *   ieee1394_open()  - Open a printer device.
 *   ieee1394_close() - Close a printer device.
 *   ieee1394_read()  - Read from a printer device.
 *   ieee1394_write() - Write data to a printer device.
 *   ieee1394_error() - Return the last error.
 */

/*
 * Include necessary headers.
 */

#include "ieee1394.h"
#include <cups/debug.h>
#include <libraw1394/raw1394.h>
#include <libraw1394/csr.h>


/*
 * Limits...
 */

#define MAX_NODES	100


/*
 * Structures...
 */

typedef struct
{
  char			uri[HTTP_MAX_URI],/* URI for this node... */
			description[128],/* Description of port */
			make_model[128];/* Make and model */
  int			port,		/* Port where this node is found */
			node;		/* Node number */
  unsigned long long	addr;		/* Management address */
} linux1394_node_t;

typedef struct
{
  raw1394handle_t	handle;		/* Handle for printer device */
  int			node;		/* Node number for printer device */
  unsigned long long	addr;		/* Management address */
} linux1394_dev_t;


/*
 * ORB messages for communication with the device...
 */

typedef struct		/**** Login ORB Message */
{
  unsigned char		passwd_addr[8];	/* Password address */
  unsigned char		resp_addr[8];	/* Login response address */
  unsigned char		notify_excl;	/* Notify and exclusive bits */
  unsigned char		recon_func;	/* Reconnect time and function */
  unsigned char		lun[2];		/* Logical unit number */
  unsigned char		passwd_len[2];	/* Length of password */
  unsigned char		resp_len[2];	/* Length of login response */
  unsigned char		fifo_addr[8];	/* Local status FIFO address */
} login_orb_t;

typedef struct		/**** Login Response Message ****/
{
  unsigned char		length[2];	/* Length of response */
  unsigned char		login_id[2];	/* Login ID */
  unsigned char		cmd_addr[8];	/* Command block agent address */
  unsigned char		reserved[2];	/* Reserved (0) */
  unsigned char		recon_hold[2];	/* Number of seconds to hold login */
} login_resp_t;


/*
 * Local globals...
 */

static char		error_string[1024] = "";
static int		num_nodes;
static linux1394_node_t	nodes[MAX_NODES];


/*
 * 'get_device_id()' - Get the IEEE-1284 device ID for a node...
 */

static char *				/* O - Device ID */
get_device_id(raw1394handle_t    handle,/* I - Handle for device */
              int                node,	/* I - Node number */
	      unsigned long long offset,/* I - Offset to directory */
	      char               *id,	/* O - ID string */
	      int                idlen)	/* I - Size of ID string */
{
  unsigned char		data[1024],	/* Data from ROM */
			*dataptr;	/* Pointer into data */
  int			length;		/* Length of directory */
  int			datalen;	/* Length of data */
  unsigned long long	dataoff;	/* Offset of data */


  DEBUG_printf(("get_device_id(handle = %p, node = %d, offset = %llx, id = %p, idlen = %d)\n",
                handle, node, offset, id, idlen));

  *id = '\0';

 /*
  * Read the directory length from the first quadlet...
  */

  if (raw1394_read(handle, 0xffc0 | node, offset, 4, (quadlet_t *)data) < 0)
    return (NULL);

  offset += 4;

 /*
  * The length is in the upper 16 bits...
  */

  length = (data[0] << 8) | data[1];

  DEBUG_printf(("    length = %d\n", length));

 /*
  * Then read the directory, looking for unit directory or device tags...
  */

  while (length > 0)
  {
    if (raw1394_read(handle, 0xffc0 | node, offset, 4, (quadlet_t *)data) < 0)
      return (NULL);

    DEBUG_printf(("    data = %02X %02X %02X %02X\n", data[0], data[1],
                  data[2], data[3]));

    if (data[0] == 0xd1)
    {
     /*
      * Found the unit directory...
      */

      offset += ((((data[1] << 8) | data[2]) << 8) | data[3]) << 2;

      return (get_device_id(handle, node, offset, id, idlen));
    }
    else if (data[0] == 0x81)
    {
     /*
      * Found potential IEEE-1284 device ID...
      */

      dataoff = offset + (((((data[1] << 8) | data[2]) << 8) | data[3]) << 2);

      if (raw1394_read(handle, 0xffc0 | node, dataoff, 4, (quadlet_t *)data) < 0)
	return (NULL);

      dataoff += 4;

     /*
      * Read the leaf value...
      */

      datalen = (data[0] << 8) | data[1];

      if (datalen > (sizeof(data) / 4))
        datalen = sizeof(data) / 4;

      for (dataptr = data; datalen > 0; datalen --, dataptr += 4, dataoff += 4)
	if (raw1394_read(handle, 0xffc0 | node, dataoff, 4,
	                 (quadlet_t *)dataptr) < 0)
	  return (NULL);

      if (data[0] == 0 && memcmp(data + 8, "MFG:", 4) == 0)
      {
       /*
        * Found the device ID...
	*/

        datalen = dataptr - data - 8;
	if (datalen >= idlen)
	  datalen --;

	memcpy(id, data + 8, datalen);
	id[datalen] = '\0';

        return (id);
      }
    }

    offset += 4;
    length --;
  }

  return (NULL);
}


/*
 * 'get_man_addr()' - Get the management address for a node...
 */

static int				/* O - Unit type */
get_man_addr(raw1394handle_t    handle,	/* I - Handle for device */
             int                node,	/* I - Node number */
	     unsigned long long offset)	/* I - Offset to directory */
{
  unsigned char	data[4];		/* Data from ROM */
  int		length;			/* Length of directory */


  DEBUG_printf(("get_man_addr(handle = %p, node = %d, offset = %llx)\n",
                handle, node, offset));

 /*
  * Read the directory length from the first quadlet...
  */

  if (raw1394_read(handle, 0xffc0 | node, offset, 4, (quadlet_t *)data) < 0)
    return (-1);

  offset += 4;

 /*
  * The length is in the upper 16 bits...
  */

  length = (data[0] << 8) | data[1];

  DEBUG_printf(("    length = %d\n", length));

 /*
  * Then read the directory, looking for unit directory or type tags...
  */

  while (length > 0)
  {
    if (raw1394_read(handle, 0xffc0 | node, offset, 4, (quadlet_t *)data) < 0)
      return (-1);

    DEBUG_printf(("    data = %02X %02X %02X %02X\n", data[0], data[1],
                  data[2], data[3]));

    if (data[0] == 0xd1)
    {
     /*
      * Found the unit directory...
      */

      offset += ((((data[1] << 8) | data[2]) << 8) | data[3]) << 2;

      return (get_man_addr(handle, node, offset));
    }
    else if (data[0] == 0x54)
    {
     /*
      * Found the management address...
      */

      return (((((data[1] << 8) | data[2]) << 8) | data[3]) << 2);
    }

    offset += 4;
    length --;
  }

  return (-1);
}


/*
 * 'get_unit_type()' - Get the unit type for a node...
 */

static int				/* O - Unit type */
get_unit_type(raw1394handle_t    handle,/* I - Handle for device */
              int                node,	/* I - Node number */
	      unsigned long long offset)/* I - Offset to directory */
{
  unsigned char	data[4];		/* Data from ROM */
  int		length;			/* Length of directory */


  DEBUG_printf(("get_unit_type(handle = %p, node = %d, offset = %llx)\n",
                handle, node, offset));

 /*
  * Read the directory length from the first quadlet...
  */

  if (raw1394_read(handle, 0xffc0 | node, offset, 4, (quadlet_t *)data) < 0)
    return (-1);

  offset += 4;

 /*
  * The length is in the upper 16 bits...
  */

  length = (data[0] << 8) | data[1];

  DEBUG_printf(("    length = %d\n", length));

 /*
  * Then read the directory, looking for unit directory or type tags...
  */

  while (length > 0)
  {
    if (raw1394_read(handle, 0xffc0 | node, offset, 4, (quadlet_t *)data) < 0)
      return (-1);

    DEBUG_printf(("    data = %02X %02X %02X %02X\n", data[0], data[1],
                  data[2], data[3]));

    if (data[0] == 0xd1)
    {
     /*
      * Found the unit directory...
      */

      offset += ((((data[1] << 8) | data[2]) << 8) | data[3]) << 2;

      return (get_unit_type(handle, node, offset));
    }
    else if (data[0] == 0x14)
    {
     /*
      * Found the unit type...
      */

      return (data[1] & 0x1f);
    }

    offset += 4;
    length --;
  }

  return (-1);
}


#ifdef DEBUG
/*
 * 'show_data()' - Show a data node...
 */

static void
show_data(raw1394handle_t    handle,	/* I - Handle for device */
          int                node,	/* I - Node number */
	  unsigned long long offset,	/* I - Offset to directory */
	  int                indent)	/* Amount to indent */
{
  int		i;			/* Looping var */
  unsigned char	data[4];		/* Data from ROM */
  int		length;			/* Length of data */


 /*
  * Read the data length from the first quadlet...
  */

  if (raw1394_read(handle, 0xffc0 | node, offset, 4, (quadlet_t *)data) < 0)
    return;

  offset += 4;

 /*
  * The length is in the upper 16 bits...
  */

  length = (data[0] << 8) | data[1];

 /*
  * Then read the data...
  */

  for (i = 0; i < indent; i ++)
    putchar(' ');

  printf("LEAF (%d quadlets)\n", length);

  while (length > 0)
  {
    if (raw1394_read(handle, 0xffc0 | node, offset, 4, (quadlet_t *)data) < 0)
      return;

    for (i = 0; i < indent; i ++)
      putchar(' ');

    printf("%02X %02X %02X %02X    '%c%c%c%c'\n",
           data[0], data[1], data[2], data[3],
           (data[0] < ' ' || data[0] >= 0x7f) ? '.' : data[0],
           (data[1] < ' ' || data[1] >= 0x7f) ? '.' : data[1],
           (data[2] < ' ' || data[2] >= 0x7f) ? '.' : data[2],
           (data[3] < ' ' || data[3] >= 0x7f) ? '.' : data[3]);

    offset += 4;
    length --;
  }
}


/*
 * 'show_dir()' - Show a directory list...
 */

static void
show_dir(raw1394handle_t    handle,	/* I - Handle for device */
         int                node,	/* I - Node number */
	 unsigned long long offset,	/* I - Offset to directory */
	 int                indent)	/* Amount to indent */
{
  int			i;		/* Looping var */
  unsigned char		data[4];	/* Data from ROM */
  int			length;		/* Length of directory */
  int			value;		/* Value in directory */


 /*
  * Read the directory length from the first quadlet...
  */

  if (raw1394_read(handle, 0xffc0 | node, offset, 4, (quadlet_t *)data) < 0)
    return;

  offset += 4;

 /*
  * The length is in the upper 16 bits...
  */

  length = (data[0] << 8) | data[1];

 /*
  * Then read the directory...
  */

  while (length > 0)
  {
    if (raw1394_read(handle, 0xffc0 | node, offset, 4, (quadlet_t *)data) < 0)
      return;

    for (i = 0; i < indent; i ++)
      putchar(' ');

    printf("%02X %02X %02X %02X\n", data[0], data[1], data[2], data[3]);

    value = (((data[1] << 8) | data[2]) << 8) | data[3];

    switch (data[0] & 0xc0)
    {
      case 0x00 :
	  for (i = -4; i < indent; i ++)
	    putchar(' ');

          printf("IMMEDIATE %d\n", value);
	  break;

      case 0x40 :
	  for (i = -4; i < indent; i ++)
	    putchar(' ');

          printf("CSR OFFSET +%06X\n", value);
	  break;

      case 0x80 :
          show_data(handle, node, offset + value * 4, indent + 4);
	  break;

      case 0xc0 :
          show_dir(handle, node, offset + value * 4, indent + 4);
	  break;
    }

    offset += 4;
    length --;
  }
}
#endif /* DEBUG */


/*
 * 'ieee1394_list()' - List the available printer devices.
 */

ieee1394_info_t	*			/* O - Printer information */
ieee1394_list(int *num_devices)		/* O - Number of printers */
{
  int			i, j;		/* Looping vars */
  raw1394handle_t	handle;		/* 1394 handle */
  int			num_ports;	/* Number of ports */
  struct raw1394_portinfo ports[100];	/* Port data... */
  unsigned char		guid[8];	/* Global unique ID */
  int			vendor;		/* Vendor portion of GUID */
  int			unit_type;	/* Unit type */
  int			addr;		/* Management address offset */
  char			id[1024],	/* Device ID string */
			*idptr,		/* Pointer into ID string */
			*idsep;		/* Pointer to separator */
  ieee1394_info_t	*devices;	/* Device list */


 /*
  * Connect to the user-mode driver interface...
  */

  handle    = raw1394_new_handle();
  num_ports = raw1394_get_port_info(handle, ports,
                                    sizeof(ports) / sizeof(ports[0]));

  DEBUG_printf(("num_ports = %d\n", num_ports));

 /*
  * Loop through the ports to discover what nodes are available.
  */

  num_nodes = 0;

  for (i = 0; i < num_ports; i ++)
  {
    DEBUG_printf(("ports[%d] = { nodes = %d, name = \"%s\" }\n", i,
                  ports[i].nodes, ports[i].name));

    raw1394_set_port(handle, i);

    for (j = 0; j < ports[i].nodes; j ++)
    {
      if (raw1394_read(handle, 0xffc0 | j,
                       CSR_REGISTER_BASE + CSR_CONFIG_ROM + 12, 4,
		       (quadlet_t *)guid) < 0)
      {
        DEBUG_printf(("    Node #%d: Unable to contact (%s)!\n", j,
	              strerror(errno)));
        continue;
      }
      else
      {
        raw1394_read(handle, 0xffc0 | j,
                     CSR_REGISTER_BASE + CSR_CONFIG_ROM + 16, 4,
	             (quadlet_t *)(guid + 4));

        DEBUG_printf(("    Node #%d: GUID = %02X%02X%02X%02X%02X%02X%02X%02X\n",
		      j, guid[0], guid[1], guid[2], guid[3], guid[4],
		      guid[5], guid[6], guid[7]));

        vendor    = (((guid[0] << 8) | guid[1]) << 8) | guid[2];
        unit_type = get_unit_type(handle, j,
	                          CSR_REGISTER_BASE + CSR_CONFIG_ROM + 20);

        DEBUG_printf(("vendor = %x, unit_type = %d\n", vendor, unit_type));

        if (unit_type == 2 && num_nodes < MAX_NODES)
	{
	 /*
	  * Found a printer device; add it to the nodes list...
	  */

#ifdef DEBUG
          show_dir(handle, j, CSR_REGISTER_BASE + CSR_CONFIG_ROM + 20, 0);
#endif /* DEBUG */

          memset(nodes + num_nodes, 0, sizeof(linux1394_node_t));

          sprintf(nodes[num_nodes].uri, "ieee1394://%02X%02X%02X%02X%02X%02X%02X%02X",
		  guid[0], guid[1], guid[2], guid[3], guid[4],
		  guid[5], guid[6], guid[7]);

          nodes[num_nodes].port = i;
	  nodes[num_nodes].node = j;

          addr = get_man_addr(handle, j, CSR_REGISTER_BASE + CSR_CONFIG_ROM + 20);

          if (addr < 0)
	    continue;

          nodes[num_nodes].addr = CSR_REGISTER_BASE + addr;

          DEBUG_printf(("Node address = %llx\n", nodes[num_nodes].addr));

          get_device_id(handle, j, CSR_REGISTER_BASE + CSR_CONFIG_ROM + 20,
	                id, sizeof(id));

          if (id[0])
	  {
	   /*
	    * Grab the manufacturer and model name from the device ID
	    * string...
	    */

            idptr = id + 4;
            idsep = strchr(id, ';');
	    if (idsep)
	      *idsep++ = '\0';
	    else
	      idsep = idptr;

	    snprintf(nodes[num_nodes].description,
	             sizeof(nodes[num_nodes].description),
		     "%s Firewire Printer", idptr);

            if ((idptr = strstr(idsep, "DES:")) == NULL)
	      idptr = strstr(idsep, "MDL:");

            if (idptr == NULL)
              strcpy(nodes[num_nodes].make_model, "Unknown");
	    else
	    {
	     /*
	      * Grab the DES or MDL code...
	      */

	      idptr += 4;
	      idsep = strchr(idptr, ';');
	      if (idsep)
	        *idsep = '\0';

              if (strncmp(id + 4, idptr, strlen(id + 4)) == 0)
	      {
	       /*
	        * Use the description directly...
		*/

        	strlcpy(nodes[num_nodes].make_model, idptr,
	        	sizeof(nodes[num_nodes].make_model));
              }
	      else
	      {
	       /*
	        * Add the manufacturer to the front of the name...
		*/

        	snprintf(nodes[num_nodes].make_model,
	        	 sizeof(nodes[num_nodes].make_model),
			 "%s %s", id + 4, idptr);
              }
            }
	  }
	  else
	  {
	   /*
	    * Flag it as an unknown printer...
	    */

	    sprintf(nodes[num_nodes].description,
	            "Unknown%06X Firewire Printer", vendor);
            strcpy(nodes[num_nodes].make_model, "Unknown");
	  }

	  num_nodes ++;
	}
      }
    }
  }

 /*
  * Done querying the Firewire bus...
  */

  raw1394_destroy_handle(handle);

 /*
  * Build an array of device info structures as needed...
  */

  if (num_devices == NULL)
    return (NULL);

  *num_devices = num_nodes;

  if (num_nodes)
  {
    if ((devices = calloc(sizeof(ieee1394_info_t), num_nodes)) != NULL)
    {
      for (i = 0; i < num_nodes; i ++)
      {
        strcpy(devices[i].uri, nodes[i].uri);
	strcpy(devices[i].description, nodes[i].description);
	strcpy(devices[i].make_model, nodes[i].make_model);
      }
    }

    return (devices);
  }
  else
    return (NULL);
}


/*
 * 'ieee1394_open()' - Open a printer device.
 */

ieee1394_dev_t				/* O - Printer device or NULL */
ieee1394_open(const char *uri)		/* I - Device URI */
{
  int			i;		/* Looping var */
  linux1394_dev_t	*ldev;		/* Linux device */


 /*
  * Return early if we can't see any printers...
  */

  if (num_nodes == 0)
    ieee1394_list(NULL);

  if (num_nodes == 0)
  {
    strcpy(error_string, "No IEEE-1394 printers found!");
    return (NULL);
  }

 /*
  * Look for the URI...
  */

  for (i = 0; i < num_nodes; i ++)
    if (strcmp(nodes[i].uri, uri) == 0)
      break;

  if (i >= num_nodes)
  {
    snprintf(error_string, sizeof(error_string), "Device %s not found!", uri);
    return (NULL);
  }

 /*
  * Now create a new device structure...
  */

  if ((ldev = calloc(sizeof(linux1394_dev_t), 1)) == NULL)
  {
    strcpy(error_string, "Out of memory!");
    return (NULL);
  }

  ldev->handle = raw1394_new_handle();
  ldev->node   = nodes[i].node;
  ldev->addr   = nodes[i].addr;

  raw1394_set_port(ldev->handle, nodes[i].port);

  error_string[0] = '\0';

  return ((ieee1394_dev_t)ldev);
}


/*
 * 'ieee1394_close()' - Close a printer device.
 */

int					/* O - 0 on success, -1 on failure */
ieee1394_close(ieee1394_dev_t dev)	/* I - Printer device */
{
  linux1394_dev_t	*ldev;		/* Linux device */


  ldev = (linux1394_dev_t *)dev;

  raw1394_destroy_handle(ldev->handle);

  free(ldev);

  return (0);
}


/*
 * 'ieee1394_read()' - Read from a printer device.
 */

int					/* O - Number of bytes read or -1 */
ieee1394_read(ieee1394_dev_t dev,	/* I - Printer device */
              char           *buffer,	/* I - Read buffer */
	      int            len)	/* I - Max bytes to read */
{
  linux1394_dev_t	*ldev;		/* Linux device */


  ldev = (linux1394_dev_t *)dev;


  return (0);
}


/*
 * 'ieee1394_write()' - Write data to a printer device.
 */

int					/* O - Number of bytes written or -1 */
ieee1394_write(ieee1394_dev_t dev,	/* I - Printer device */
               char           *buffer,	/* I - Buffer to write */
	       int            len)	/* I - Number of bytes to write */
{
  linux1394_dev_t	*ldev;		/* Linux device */


  ldev = (linux1394_dev_t *)dev;


/*  if (raw1394_write(handle, 0xffc0 | j, 0, ,
		       (quadlet_t *)guid) < 0)*/

  return (len);
}


/*
 * 'ieee1394_error()' - Return the last error.
 */

const char *				/* O - Error string or NULL */
ieee1394_error(void)
{
  if (error_string[0])
    return (error_string);
  else
    return (NULL);
}


/*
 * End of "$Id$".
 */
