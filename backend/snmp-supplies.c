/*
 * "$Id$"
 *
 *   SNMP supplies functions for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2008 by Apple Inc.
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
 *   backendSNMPSupplies()   - Get the current supplies for a device.
 *   backend_init_supplies() - Initialize the supplies list.
 *   backend_walk_cb()       - Interpret the supply value responses...
 */

/*
 * Include necessary headers.
 */

#include "backend-private.h"
#include <cups/array.h>


/*
 * Local constants...
 */

#define CUPS_MAX_SUPPLIES	32	/* Maximum number of supplies for a printer */


/*
 * Local structures...
 */

typedef struct
{
  char	name[CUPS_SNMP_MAX_STRING],	/* Name of supply */
	color[8];			/* Color: "#RRGGBB" or "none" */
  int	colorant,			/* Colorant index */
	type,				/* Supply type */
	max_capacity,			/* Maximum capacity */
	level;				/* Current level value */
} backend_supplies_t;


/*
 * Local globals...
 */

static http_addr_t	current_addr;	/* Current address */
static int		num_supplies = 0;
					/* Number of supplies found */
static backend_supplies_t supplies[CUPS_MAX_SUPPLIES];
					/* Supply information */

static const int	hrDeviceDescr[] =
			{ CUPS_OID_hrDeviceDescr, 1, -1 };
					/* Device description OID */
static const int	hrPrinterStatus[] =
			{ CUPS_OID_hrPrinterStatus, 1, -1 };
					/* Current state OID */
static const int	hrPrinterDetectedErrorState[] =
			{ CUPS_OID_hrPrinterDetectedErrorState, 1, -1 };
					/* Current printer state bits OID */
static const int	prtMarkerColorantValue[] =
			{ CUPS_OID_prtMarkerColorantValue, -1 },
					/* Colorant OID */
			prtMarkerColorantValueOffset =
			(sizeof(prtMarkerColorantValue) /
			 sizeof(prtMarkerColorantValue[0]));
					/* Offset to colorant index */
static const int	prtMarkerLifeCount[] =
			{ CUPS_OID_prtMarkerLifeCount, 1, 1, -1 };
					/* Page counter OID */
static const int	prtMarkerSuppliesEntry[] =
			{ CUPS_OID_prtMarkerSuppliesEntry, -1 };
					/* Supplies OID */
static const int	prtMarkerSuppliesColorantIndex[] =
			{ CUPS_OID_prtMarkerSuppliesColorantIndex, -1 },
					/* Colorant index OID */
			prtMarkerSuppliesColorantIndexOffset =
			(sizeof(prtMarkerSuppliesColorantIndex) /
			 sizeof(prtMarkerSuppliesColorantIndex[0]));
			 		/* Offset to supply index */
static const int	prtMarkerSuppliesDescription[] =
			{ CUPS_OID_prtMarkerSuppliesDescription, -1 },
					/* Description OID */
			prtMarkerSuppliesDescriptionOffset =
			(sizeof(prtMarkerSuppliesDescription) /
			 sizeof(prtMarkerSuppliesDescription[0]));
			 		/* Offset to supply index */
static const int	prtMarkerSuppliesLevel[] =
			{ CUPS_OID_prtMarkerSuppliesLevel, -1 },
					/* Level OID */
			prtMarkerSuppliesLevelOffset =
			(sizeof(prtMarkerSuppliesLevel) /
			 sizeof(prtMarkerSuppliesLevel[0]));
			 		/* Offset to supply index */
static const int	prtMarkerSuppliesMaxCapacity[] =
			{ CUPS_OID_prtMarkerSuppliesMaxCapacity, -1 },
					/* Max capacity OID */
			prtMarkerSuppliesMaxCapacityOffset =
			(sizeof(prtMarkerSuppliesMaxCapacity) /
			 sizeof(prtMarkerSuppliesMaxCapacity[0]));
			 		/* Offset to supply index */
static const int	prtMarkerSuppliesType[] =
			{ CUPS_OID_prtMarkerSuppliesType, -1 },
					/* Type OID */
			prtMarkerSuppliesTypeOffset =
			(sizeof(prtMarkerSuppliesType) /
			 sizeof(prtMarkerSuppliesType[0]));
			 		/* Offset to supply index */


/*
 * Local functions...
 */

static void	backend_init_supplies(int snmp_fd, http_addr_t *addr);
static void	backend_walk_cb(cups_snmp_t *packet, void *data);


/*
 * 'backendSNMPSupplies()' - Get the current supplies for a device.
 */

int					/* O - Page count */
backendSNMPSupplies(
    int         snmp_fd,		/* I - SNMP socket */
    http_addr_t *addr,			/* I - Printer address */
    int         *printer_state)		/* O - Printer state */
{
  if (!httpAddrEqual(addr, &current_addr))
    backend_init_supplies(snmp_fd, addr);
  else if (num_supplies > 0)
    cupsSNMPWalk(snmp_fd, &current_addr, CUPS_SNMP_VERSION_1,
		 cupsSNMPDefaultCommunity(), prtMarkerSuppliesLevel, 500,
		 backend_walk_cb, NULL);

  *printer_state = -1;

  if (num_supplies > 0)
  {
    int		i;			/* Looping var */
    char	value[CUPS_MAX_SUPPLIES * 4],
					/* marker-levels value string */
		*ptr;			/* Pointer into value string */
    cups_snmp_t	packet;			/* SNMP response packet */


   /*
    * Generate the marker-levels value string...
    */

    for (i = 0, ptr = value; i < num_supplies; i ++, ptr += strlen(ptr))
    {
      if (i)
        *ptr++ = ',';

      sprintf(ptr, "%d", 100 * supplies[i].level / supplies[i].max_capacity);
    }

    fprintf(stderr, "ATTR: marker-levels=%s\n", value);

   /*
    * Get the current printer status bits...
    */

    if (!cupsSNMPWrite(snmp_fd, addr, CUPS_SNMP_VERSION_1,
                       cupsSNMPDefaultCommunity(), CUPS_ASN1_GET_REQUEST, 1,
                       hrPrinterDetectedErrorState))
      return (-1);

    if (!cupsSNMPRead(snmp_fd, &packet, 500) ||
        packet.object_type != CUPS_ASN1_OCTET_STRING)
      return (-1);

    i = ((packet.object_value.string[0] & 255) << 8) |
        (packet.object_value.string[1] & 255);

    if (i & CUPS_TC_lowPaper)
      fputs("STATE: +media-low-warning\n", stderr);
    else
      fputs("STATE: -media-low-warning\n", stderr);

    if (i & (CUPS_TC_noPaper | CUPS_TC_inputTrayEmpty))
      fputs("STATE: +media-empty-error\n", stderr);
    else
      fputs("STATE: -media-empty-error\n", stderr);

    if (i & CUPS_TC_lowToner)
      fputs("STATE: +toner-low-warning\n", stderr);
    else
      fputs("STATE: -toner-low-warning\n", stderr);

    if (i & CUPS_TC_noToner)
      fputs("STATE: +toner-empty-error\n", stderr);
    else
      fputs("STATE: -toner-empty-error\n", stderr);

    if (i & CUPS_TC_doorOpen)
      fputs("STATE: +door-open-report\n", stderr);
    else
      fputs("STATE: -door-open-report\n", stderr);

    if (i & CUPS_TC_jammed)
      fputs("STATE: +media-jam-error\n", stderr);
    else
      fputs("STATE: -media-jam-error\n", stderr);

    if (i & CUPS_TC_offline)
      fputs("STATE: +offline-report\n", stderr);
    else
      fputs("STATE: -offline-report\n", stderr);

    if (i & (CUPS_TC_serviceRequested | CUPS_TC_overduePreventMaint))
      fputs("STATE: +service-needed-error\n", stderr);
    else
      fputs("STATE: -service-needed-error\n", stderr);

    if (i & CUPS_TC_inputTrayMissing)
      fputs("STATE: +input-tray-missing-error\n", stderr);
    else
      fputs("STATE: -input-tray-missing-error\n", stderr);

    if (i & CUPS_TC_outputTrayMissing)
      fputs("STATE: +output-tray-missing-error\n", stderr);
    else
      fputs("STATE: -output-tray-missing-error\n", stderr);

    if (i & CUPS_TC_markerSupplyMissing)
      fputs("STATE: +marker-supply-missing-error\n", stderr);
    else
      fputs("STATE: -marker-supply-missing-error\n", stderr);

    if (i & CUPS_TC_outputNearFull)
      fputs("STATE: +output-area-almost-full-warning\n", stderr);
    else
      fputs("STATE: -output-area-almost-full-warning\n", stderr);

    if (i & CUPS_TC_outputFull)
      fputs("STATE: +output-area-full-error\n", stderr);
    else
      fputs("STATE: -output-area-full-error\n", stderr);

   /*
    * Get the current printer state...
    */

    if (!cupsSNMPWrite(snmp_fd, addr, CUPS_SNMP_VERSION_1,
                       cupsSNMPDefaultCommunity(), CUPS_ASN1_GET_REQUEST, 1,
                       hrPrinterStatus))
      return (-1);

    if (!cupsSNMPRead(snmp_fd, &packet, 500) ||
        packet.object_type != CUPS_ASN1_INTEGER)
      return (-1);

    *printer_state = packet.object_value.integer;

   /*
    * Get the current page count...
    */

    if (!cupsSNMPWrite(snmp_fd, addr, CUPS_SNMP_VERSION_1,
                       cupsSNMPDefaultCommunity(), CUPS_ASN1_GET_REQUEST, 1,
                       prtMarkerLifeCount))
      return (-1);

    if (!cupsSNMPRead(snmp_fd, &packet, 500) ||
        packet.object_type != CUPS_ASN1_COUNTER)
      return (-1);

    return (packet.object_value.counter);
  }
  else
    return (-1);
}


/*
 * 'backend_init_supplies()' - Initialize the supplies list.
 */

static void
backend_init_supplies(
    int         snmp_fd,		/* I - SNMP socket */
    http_addr_t *addr)			/* I - Printer address */
{
  int		i,			/* Looping var */
		type;			/* Current marker type */
  cups_file_t	*cachefile;		/* Cache file */
  const char	*cachedir;		/* CUPS_CACHEDIR value */
  char		addrstr[1024],		/* Address string */
		cachefilename[1024],	/* Cache filename */
		description[CUPS_SNMP_MAX_STRING],
					/* Device description string */
		value[CUPS_MAX_SUPPLIES * (CUPS_SNMP_MAX_STRING * 2 + 3)],
					/* Value string */
		*ptr,			/* Pointer into value string */
		*name_ptr;		/* Pointer into name string */
  cups_snmp_t	packet;			/* SNMP response packet */
  static const char * const types[] =	/* Supply types */
		{
		  "other",
		  "unknown",
		  "toner",
		  "wasteToner",
		  "ink",
		  "inkCartridge",
		  "inkRibbon",
		  "wasteInk",
		  "opc",
		  "developer",
		  "fuserOil",
		  "solidWax",
		  "ribbonWax",
		  "wasteWax",
		  "fuser",
		  "coronaWire",
		  "fuserOilWick",
		  "cleanerUnit",
		  "fuserCleaningPad",
		  "transferUnit",
		  "tonerCartridge",
		  "fuserOiler",
		  "water",
		  "wasteWater",
		  "glueWaterAdditive",
		  "wastePaper",
		  "bindingSupply",
		  "bandingSupply",
		  "stitchingWire",
		  "shrinkWrap",
		  "paperWrap",
		  "staples",
		  "inserts",
		  "covers"
		};


 /*
  * Reset state information...
  */

  current_addr = *addr;
  num_supplies = -1;

  memset(supplies, 0, sizeof(supplies));

 /*
  * Get the device description...
  */

  if (!cupsSNMPWrite(snmp_fd, addr, CUPS_SNMP_VERSION_1,
		     cupsSNMPDefaultCommunity(), CUPS_ASN1_GET_REQUEST, 1,
		     hrDeviceDescr))
    return;

  if (!cupsSNMPRead(snmp_fd, &packet, 500) ||
      packet.object_type != CUPS_ASN1_OCTET_STRING)
  {
    strlcpy(description, "Unknown", sizeof(description));
    num_supplies = 0;
  }
  else
    strlcpy(description, packet.object_value.string, sizeof(description));

 /*
  * See if we have already queried this device...
  */

  httpAddrString(addr, addrstr, sizeof(addrstr));

  if ((cachedir = getenv("CUPS_CACHEDIR")) == NULL)
    cachedir = CUPS_CACHEDIR;

  snprintf(cachefilename, sizeof(cachefilename), "%s/%s.snmp", cachedir,
           addrstr);

  if ((cachefile = cupsFileOpen(cachefilename, "r")) != NULL)
  {
   /*
    * Yes, read the cache file:
    *
    *     1 num_supplies
    *     device description
    *     supply structures...
    */

    if (cupsFileGets(cachefile, value, sizeof(value)))
    {
      if (sscanf(value, "1 %d", &num_supplies) == 1 &&
          num_supplies <= CUPS_MAX_SUPPLIES &&
          cupsFileGets(cachefile, value, sizeof(value)))
      {
        if ((ptr = value + strlen(value) - 1) >= value && *ptr == '\n')
	  *ptr = '\n';

        if (!strcmp(description, value))
	  cupsFileRead(cachefile, (char *)supplies,
	               num_supplies * sizeof(backend_supplies_t));
        else
	  num_supplies = -1;
      }
      else
        num_supplies = -1;
    }

    cupsFileClose(cachefile);
  }

 /*
  * If the cache information isn't correct, scan for supplies...
  */

  if (num_supplies < 0)
  {
   /*
    * Walk the printer configuration information...
    */

    cupsSNMPWalk(snmp_fd, &current_addr, CUPS_SNMP_VERSION_1,
		 cupsSNMPDefaultCommunity(), prtMarkerSuppliesEntry, 500,
		 backend_walk_cb, NULL);
  }

 /*
  * Save the cached information...
  */

  if (num_supplies < 0)
    num_supplies = 0;

  if ((cachefile = cupsFileOpen(cachefilename, "w")) != NULL)
  {
    cupsFilePrintf(cachefile, "1 %d\n", num_supplies);
    cupsFilePrintf(cachefile, "%s\n", description);

    if (num_supplies > 0)
      cupsFileWrite(cachefile, (char *)supplies,
                    num_supplies * sizeof(backend_supplies_t));

    cupsFileClose(cachefile);
  }

  if (num_supplies <= 0)
    return;

 /*
  * Get the colors...
  */

  for (i = 0; i < num_supplies; i ++)
    strcpy(supplies[i].color, "none");

  cupsSNMPWalk(snmp_fd, &current_addr, CUPS_SNMP_VERSION_1,
               cupsSNMPDefaultCommunity(), prtMarkerColorantValue, 500,
	       backend_walk_cb, NULL);

 /*
  * Output the marker-colors attribute...
  */

  for (i = 0, ptr = value; i < num_supplies; i ++, ptr += strlen(ptr))
  {
    if (i)
      *ptr++ = ',';

    strcpy(ptr, supplies[i].color);
  }

  fprintf(stderr, "ATTR: marker-colors=%s\n", value);

 /*
  * Output the marker-names attribute...
  */

  for (i = 0, ptr = value; i < num_supplies; i ++)
  {
    if (i)
      *ptr++ = ',';

    *ptr++ = '\"';
    for (name_ptr = supplies[i].name; *name_ptr;)
    {
      if (*name_ptr == '\\' || *name_ptr == '\"')
        *ptr++ = '\\';

      *ptr++ = *name_ptr++;
    }
    *ptr++ = '\"';
  }

  *ptr = '\0';

  fprintf(stderr, "ATTR: marker-names=%s\n", value);

 /*
  * Output the marker-types attribute...
  */

  for (i = 0, ptr = value; i < num_supplies; i ++, ptr += strlen(ptr))
  {
    if (i)
      *ptr++ = ',';

    type = supplies[i].type;

    if (type < CUPS_TC_other || type > CUPS_TC_covers)
      strcpy(ptr, "unknown");
    else
      strcpy(ptr, types[type - CUPS_TC_other]);
  }

  fprintf(stderr, "ATTR: marker-types=%s\n", value);
}


/*
 * 'backend_walk_cb()' - Interpret the supply value responses...
 */

static void
backend_walk_cb(cups_snmp_t *packet,	/* I - SNMP packet */
                void        *data)	/* I - User data (unused) */
{
  int	i, j, k;			/* Looping vars */
  static const char * const colors[8][2] =
  {					/* Standard color names */
    { "black",   "#000000" },
    { "blue",    "#0000FF" },
    { "cyan",    "#00FFFF" },
    { "green",   "#00FF00" },
    { "magenta", "#FF00FF" },
    { "red",     "#FF0000" },
    { "white",   "#FFFFFF" },
    { "yellow",  "#FFFF00" }
  };


  (void)data;

  if (cupsSNMPIsOIDPrefixed(packet, prtMarkerColorantValue) &&
      packet->object_type == CUPS_ASN1_OCTET_STRING)
  {
   /*
    * Get colorant...
    */

    i = packet->object_name[prtMarkerColorantValueOffset];

    fprintf(stderr, "DEBUG2: prtMarkerColorantValue.1.%d = \"%s\"\n", i,
            packet->object_value.string);

    for (j = 0; j < num_supplies; j ++)
      if (supplies[j].colorant == i)
      {
	for (k = 0; k < (int)(sizeof(colors) / sizeof(colors[0])); k ++)
	  if (!strcmp(colors[k][0], packet->object_value.string))
	  {
	    strcpy(supplies[j].color, colors[k][1]);
	    break;
	  }
      }
  }
  else if (cupsSNMPIsOIDPrefixed(packet, prtMarkerSuppliesColorantIndex))
  {
   /*
    * Get colorant index...
    */

    i = packet->object_name[prtMarkerSuppliesColorantIndexOffset];
    if (i < 1 || i > CUPS_MAX_SUPPLIES ||
        packet->object_type != CUPS_ASN1_INTEGER)
      return;

    fprintf(stderr, "DEBUG2: prtMarkerSuppliesColorantIndex.1.%d = %d\n", i,
            packet->object_value.integer);

    if (i > num_supplies)
      num_supplies = i;

    supplies[i - 1].colorant = packet->object_value.integer;
  }
  else if (cupsSNMPIsOIDPrefixed(packet, prtMarkerSuppliesDescription))
  {
   /*
    * Get supply name/description...
    */

    i = packet->object_name[prtMarkerSuppliesDescriptionOffset];
    if (i < 1 || i > CUPS_MAX_SUPPLIES ||
        packet->object_type != CUPS_ASN1_OCTET_STRING)
      return;

    fprintf(stderr, "DEBUG2: prtMarkerSuppliesDescription.1.%d = \"%s\"\n", i,
            packet->object_value.string);

    if (i > num_supplies)
      num_supplies = i;

    strlcpy(supplies[i - 1].name, packet->object_value.string,
            sizeof(supplies[0].name));
  }
  else if (cupsSNMPIsOIDPrefixed(packet, prtMarkerSuppliesLevel))
  {
   /*
    * Get level...
    */

    i = packet->object_name[prtMarkerSuppliesLevelOffset];
    if (i < 1 || i > CUPS_MAX_SUPPLIES ||
        packet->object_type != CUPS_ASN1_INTEGER)
      return;

    fprintf(stderr, "DEBUG2: prtMarkerSuppliesLevel.1.%d = %d\n", i,
            packet->object_value.integer);

    if (i > num_supplies)
      num_supplies = i;

    supplies[i - 1].level = packet->object_value.integer;
  }
  else if (cupsSNMPIsOIDPrefixed(packet, prtMarkerSuppliesMaxCapacity))
  {
   /*
    * Get max capacity...
    */

    i = packet->object_name[prtMarkerSuppliesMaxCapacityOffset];
    if (i < 1 || i > CUPS_MAX_SUPPLIES ||
        packet->object_type != CUPS_ASN1_INTEGER)
      return;

    fprintf(stderr, "DEBUG2: prtMarkerSuppliesMaxCapacity.1.%d = %d\n", i,
            packet->object_value.integer);

    if (i > num_supplies)
      num_supplies = i;

    supplies[i - 1].max_capacity = packet->object_value.integer;
  }
  else if (cupsSNMPIsOIDPrefixed(packet, prtMarkerSuppliesType))
  {
   /*
    * Get marker type...
    */

    i = packet->object_name[prtMarkerSuppliesTypeOffset];
    if (i < 1 || i > CUPS_MAX_SUPPLIES ||
        packet->object_type != CUPS_ASN1_INTEGER)
      return;

    fprintf(stderr, "DEBUG2: prtMarkerSuppliesType.1.%d = %d\n", i,
            packet->object_value.integer);

    if (i > num_supplies)
      num_supplies = i;

    supplies[i - 1].type = packet->object_value.integer;
  }
}


/*
 * End of "$Id$".
 */
