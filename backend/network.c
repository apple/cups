/*
 * "$Id$"
 *
 *   Common network APIs for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007-2008 by Apple Inc.
 *   Copyright 2006-2007 by Easy Software Products, all rights reserved.
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
 *   backendCheckSideChannel() - Check the side-channel for pending requests.
 *   backendNetworkSideCB()    - Handle common network side-channel commands.
 */

/*
 * Include necessary headers.
 */

#include "backend-private.h"
#include <limits.h>
#ifdef __hpux
#  include <sys/time.h>
#else
#  include <sys/select.h>
#endif /* __hpux */


/*
 * 'backendCheckSideChannel()' - Check the side-channel for pending requests.
 */


void
backendCheckSideChannel(
    int         snmp_fd,		/* I - SNMP socket */
    http_addr_t *addr)			/* I - Address of device */
{
  fd_set	input;			/* Select input set */
  struct timeval timeout;		/* Select timeout */


  FD_ZERO(&input);
  FD_SET(CUPS_SC_FD, &input);

  timeout.tv_sec = timeout.tv_usec = 0;

  if (select(CUPS_SC_FD + 1, &input, NULL, NULL, &timeout) > 0)
    backendNetworkSideCB(-1, -1, snmp_fd, addr, 0);
}


/*
 * 'backendNetworkSideCB()' - Handle common network side-channel commands.
 */

void
backendNetworkSideCB(
    int         print_fd,		/* I - Print file or -1 */
    int         device_fd,		/* I - Device file or -1 */
    int         snmp_fd,		/* I - SNMP socket */
    http_addr_t *addr,			/* I - Address of device */
    int         use_bc)			/* I - Use back-channel data? */
{
  cups_sc_command_t	command;	/* Request command */
  cups_sc_status_t	status;		/* Request/response status */
  char			data[2048];	/* Request/response data */
  int			datalen;	/* Request/response data size */
  const char		*device_id;	/* 1284DEVICEID env var */


  datalen = sizeof(data);

  if (cupsSideChannelRead(&command, &status, data, &datalen, 1.0))
  {
    _cupsLangPuts(stderr, _("WARNING: Failed to read side-channel request!\n"));
    return;
  }

  switch (command)
  {
    case CUPS_SC_CMD_DRAIN_OUTPUT :
       /*
        * Our sockets disable the Nagle algorithm and data is sent immediately.
	*/

        if (device_fd < 0)
	  status = CUPS_SC_STATUS_NOT_IMPLEMENTED;
	else if (backendDrainOutput(print_fd, device_fd))
	  status = CUPS_SC_STATUS_IO_ERROR;
	else 
          status = CUPS_SC_STATUS_OK;

	datalen = 0;
        break;

    case CUPS_SC_CMD_GET_BIDI :
	status  = CUPS_SC_STATUS_OK;
        data[0] = use_bc;
        datalen = 1;
        break;

    case CUPS_SC_CMD_SNMP_GET :
    case CUPS_SC_CMD_SNMP_GET_NEXT :
        fprintf(stderr, "DEBUG: CUPS_SC_CMD_SNMP_%s: %d (%s)\n",
	        command == CUPS_SC_CMD_SNMP_GET ? "GET" : "GET_NEXT", datalen,
		data);

        if (datalen < 2)
	{
	  status  = CUPS_SC_STATUS_BAD_MESSAGE;
	  datalen = 0;
	  break;
	}

        if (snmp_fd >= 0)
	{
	  cups_snmp_t	packet;		/* Packet from printer */


          if (!_cupsSNMPStringToOID(data, packet.object_name, CUPS_SNMP_MAX_OID))
	  {
	    status  = CUPS_SC_STATUS_BAD_MESSAGE;
	    datalen = 0;
	    break;
	  }

          status  = CUPS_SC_STATUS_IO_ERROR;
	  datalen = 0;

          if (_cupsSNMPWrite(snmp_fd, addr, CUPS_SNMP_VERSION_1,
	                     _cupsSNMPDefaultCommunity(),
	                     command == CUPS_SC_CMD_SNMP_GET ?
			         CUPS_ASN1_GET_REQUEST :
				 CUPS_ASN1_GET_NEXT_REQUEST, 1,
			     packet.object_name))
          {
	    if (_cupsSNMPRead(snmp_fd, &packet, 1.0))
	    {
	      char	*dataptr;	/* Pointer into data */
	      int	i;		/* Looping var */


              if (!_cupsSNMPOIDToString(packet.object_name, data, sizeof(data)))
	      {
	        fputs("DEBUG: Bad OID returned!\n", stderr);
	        break;
	      }

	      datalen = (int)strlen(data) + 1;
              dataptr = data + datalen;

	      switch (packet.object_type)
	      {
	        case CUPS_ASN1_BOOLEAN :
		    snprintf(dataptr, sizeof(data) - (dataptr - data), "%d",
		             packet.object_value.boolean);
		    break;

	        case CUPS_ASN1_INTEGER :
		    snprintf(dataptr, sizeof(data) - (dataptr - data), "%d",
		             packet.object_value.integer);
		    break;

	        case CUPS_ASN1_BIT_STRING :
	        case CUPS_ASN1_OCTET_STRING :
		    strlcpy(dataptr, packet.object_value.string,
		            sizeof(data) - (dataptr - data));
		    break;

	        case CUPS_ASN1_OID :
		    _cupsSNMPOIDToString(packet.object_value.oid, dataptr,
		                         sizeof(data) - (dataptr - data));
		    break;

                case CUPS_ASN1_HEX_STRING :
		    for (i = 0;
		         i < packet.object_value.hex_string.num_bytes &&
			     dataptr < (data + sizeof(data) - 3);
			 i ++, dataptr += 2)
		      sprintf(dataptr, "%02X",
		              packet.object_value.hex_string.bytes[i]);
		    break;

                case CUPS_ASN1_COUNTER :
		    snprintf(dataptr, sizeof(data) - (dataptr - data), "%d",
		             packet.object_value.counter);
		    break;

                case CUPS_ASN1_GAUGE :
		    snprintf(dataptr, sizeof(data) - (dataptr - data), "%u",
		             packet.object_value.gauge);
		    break;

                case CUPS_ASN1_TIMETICKS :
		    snprintf(dataptr, sizeof(data) - (dataptr - data), "%u",
		             packet.object_value.timeticks);
		    break;

                default :
	            fprintf(stderr, "DEBUG: Unknown OID value type %02X!\n",
		            packet.object_type);
		    break;
              }

	      fprintf(stderr, "DEBUG: Returning %s %s\n", data, data + datalen);

	      status  = CUPS_SC_STATUS_OK;
	      datalen += (int)strlen(data + datalen);
	    }
	    else
	      fputs("DEBUG: SNMP read error...\n", stderr);
	  }
	  else
	    fputs("DEBUG: SNMP write error...\n", stderr);
	  break;
        }

        status  = CUPS_SC_STATUS_NOT_IMPLEMENTED;
	datalen = 0;
	break;

    case CUPS_SC_CMD_GET_DEVICE_ID :
        if (snmp_fd >= 0)
	{
	  cups_snmp_t	packet;		/* Packet from printer */
	  static const int ppmPrinterIEEE1284DeviceId[] =
	  		{ CUPS_OID_ppmPrinterIEEE1284DeviceId,1,-1 };


          status  = CUPS_SC_STATUS_IO_ERROR;
	  datalen = 0;

          if (_cupsSNMPWrite(snmp_fd, addr, CUPS_SNMP_VERSION_1,
	                     _cupsSNMPDefaultCommunity(),
	                     CUPS_ASN1_GET_REQUEST, 1,
			     ppmPrinterIEEE1284DeviceId))
          {
	    if (_cupsSNMPRead(snmp_fd, &packet, 1.0) &&
	        packet.object_type == CUPS_ASN1_OCTET_STRING)
	    {
	      strlcpy(data, packet.object_value.string, sizeof(data));
	      datalen = (int)strlen(data);
	      status  = CUPS_SC_STATUS_OK;
	    }
	  }

	  break;
        }

	if ((device_id = getenv("1284DEVICEID")) != NULL)
	{
	  strlcpy(data, device_id, sizeof(data));
	  datalen = (int)strlen(data);
	  break;
	}

    default :
        status  = CUPS_SC_STATUS_NOT_IMPLEMENTED;
	datalen = 0;
	break;
  }

  cupsSideChannelWrite(command, status, data, datalen, 1.0);
}


/*
 * End of "$Id$".
 */
