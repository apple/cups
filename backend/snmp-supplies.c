/*
 * "$Id: snmp-supplies.c 12228 2014-10-21 13:42:05Z msweet $"
 *
 * SNMP supplies functions for CUPS.
 *
 * Copyright 2008-2014 by Apple Inc.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * "LICENSE" which should have been included with this file.  If this
 * file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * This file is subject to the Apple OS-Developed Software exception.
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
#define CUPS_SUPPLY_TIMEOUT	2.0	/* Timeout for SNMP lookups */

#define CUPS_DEVELOPER_LOW	0x0001
#define CUPS_DEVELOPER_EMPTY	0x0002
#define CUPS_MARKER_SUPPLY_LOW	0x0004
#define CUPS_MARKER_SUPPLY_EMPTY 0x0008
#define CUPS_OPC_NEAR_EOL	0x0010
#define CUPS_OPC_LIFE_OVER	0x0020
#define CUPS_TONER_LOW		0x0040
#define CUPS_TONER_EMPTY	0x0080
#define CUPS_WASTE_ALMOST_FULL	0x0100
#define CUPS_WASTE_FULL		0x0200
#define CUPS_CLEANER_NEAR_EOL	0x0400	/* Proposed JPS3 */
#define CUPS_CLEANER_LIFE_OVER	0x0800	/* Proposed JPS3 */

#define CUPS_SNMP_NONE		0x0000
#define CUPS_SNMP_CAPACITY	0x0001	/* Supply levels reported as percentages */


/*
 * Local structures...
 */

typedef struct				/**** Printer supply data ****/
{
  char	name[CUPS_SNMP_MAX_STRING],	/* Name of supply */
	color[8];			/* Color: "#RRGGBB" or "none" */
  int	colorant,			/* Colorant index */
	sclass,				/* Supply class */
	type,				/* Supply type */
	max_capacity,			/* Maximum capacity */
	level;				/* Current level value */
} backend_supplies_t;

typedef struct				/**** Printer state table ****/
{
  int		bit;			/* State bit */
  const char	*keyword;		/* IPP printer-state-reasons keyword */
} backend_state_t;


/*
 * Local globals...
 */

static http_addr_t	current_addr;	/* Current address */
static int		current_state = -1;
					/* Current device state bits */
static int		charset = -1;	/* Character set for supply names */
static unsigned		quirks = CUPS_SNMP_NONE;
					/* Quirks we have to work around */
static int		num_supplies = 0;
					/* Number of supplies found */
static backend_supplies_t supplies[CUPS_MAX_SUPPLIES];
					/* Supply information */
static int		supply_state = -1;
					/* Supply state info */

static const int	hrDeviceDescr[] =
			{ CUPS_OID_hrDeviceDescr, 1, -1 };
					/* Device description OID */
static const int	hrPrinterStatus[] =
			{ CUPS_OID_hrPrinterStatus, 1, -1 };
					/* Current state OID */
static const int	hrPrinterDetectedErrorState[] =
			{ CUPS_OID_hrPrinterDetectedErrorState, 1, -1 };
					/* Current printer state bits OID */
static const int	prtGeneralCurrentLocalization[] =
			{ CUPS_OID_prtGeneralCurrentLocalization, 1, -1 };
static const int	prtLocalizationCharacterSet[] =
			{ CUPS_OID_prtLocalizationCharacterSet, 1, 1, -1 },
			prtLocalizationCharacterSetOffset =
			(sizeof(prtLocalizationCharacterSet) /
			 sizeof(prtLocalizationCharacterSet[0]));
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
static const int	prtMarkerSuppliesClass[] =
			{ CUPS_OID_prtMarkerSuppliesClass, -1 },
					/* Class OID */
			prtMarkerSuppliesClassOffset =
			(sizeof(prtMarkerSuppliesClass) /
			 sizeof(prtMarkerSuppliesClass[0]));
			 		/* Offset to supply index */
static const int	prtMarkerSuppliesType[] =
			{ CUPS_OID_prtMarkerSuppliesType, -1 },
					/* Type OID */
			prtMarkerSuppliesTypeOffset =
			(sizeof(prtMarkerSuppliesType) /
			 sizeof(prtMarkerSuppliesType[0]));
			 		/* Offset to supply index */
static const int	prtMarkerSuppliesSupplyUnit[] =
			{ CUPS_OID_prtMarkerSuppliesSupplyUnit, -1 },
					/* Units OID */
			prtMarkerSuppliesSupplyUnitOffset =
			(sizeof(prtMarkerSuppliesSupplyUnit) /
			 sizeof(prtMarkerSuppliesSupplyUnit[0]));
					/* Offset to supply index */

static const backend_state_t printer_states[] =
			{
			  /* { CUPS_TC_lowPaper, "media-low-report" }, */
			  { CUPS_TC_noPaper | CUPS_TC_inputTrayEmpty, "media-empty-warning" },
			  /* { CUPS_TC_lowToner, "toner-low-report" }, */ /* now use prtMarkerSupplies */
			  /* { CUPS_TC_noToner, "toner-empty-warning" }, */ /* now use prtMarkerSupplies */
			  { CUPS_TC_doorOpen, "door-open-report" },
			  { CUPS_TC_jammed, "media-jam-warning" },
			  /* { CUPS_TC_offline, "offline-report" }, */ /* unreliable */
			  /* { CUPS_TC_serviceRequested | CUPS_TC_overduePreventMaint, "service-needed-warning" }, */ /* unreliable */
			  { CUPS_TC_inputTrayMissing, "input-tray-missing-warning" },
			  { CUPS_TC_outputTrayMissing, "output-tray-missing-warning" },
			  { CUPS_TC_markerSupplyMissing, "marker-supply-missing-warning" },
			  { CUPS_TC_outputNearFull, "output-area-almost-full-report" },
			  { CUPS_TC_outputFull, "output-area-full-warning" }
			};

static const backend_state_t supply_states[] =
			{
			  { CUPS_DEVELOPER_LOW, "developer-low-report" },
			  { CUPS_DEVELOPER_EMPTY, "developer-empty-warning" },
			  { CUPS_MARKER_SUPPLY_LOW, "marker-supply-low-report" },
			  { CUPS_MARKER_SUPPLY_EMPTY, "marker-supply-empty-warning" },
			  { CUPS_OPC_NEAR_EOL, "opc-near-eol-report" },
			  { CUPS_OPC_LIFE_OVER, "opc-life-over-warning" },
			  { CUPS_TONER_LOW, "toner-low-report" },
			  { CUPS_TONER_EMPTY, "toner-empty-warning" },
			  { CUPS_WASTE_ALMOST_FULL, "waste-receptacle-almost-full-report" },
			  { CUPS_WASTE_FULL, "waste-receptacle-full-warning" },
			  { CUPS_CLEANER_NEAR_EOL, "cleaner-life-almost-over-report" },
			  { CUPS_CLEANER_LIFE_OVER, "cleaner-life-over-warning" },
			};


/*
 * Local functions...
 */

static void	backend_init_supplies(int snmp_fd, http_addr_t *addr);
static void	backend_walk_cb(cups_snmp_t *packet, void *data);
static void	utf16_to_utf8(cups_utf8_t *dst, const unsigned char *src,
			      size_t srcsize, size_t dstsize, int le);


/*
 * 'backendSNMPSupplies()' - Get the current supplies for a device.
 */

int					/* O - 0 on success, -1 on error */
backendSNMPSupplies(
    int         snmp_fd,		/* I - SNMP socket */
    http_addr_t *addr,			/* I - Printer address */
    int         *page_count,		/* O - Page count */
    int         *printer_state)		/* O - Printer state */
{
  if (!httpAddrEqual(addr, &current_addr))
    backend_init_supplies(snmp_fd, addr);
  else if (num_supplies > 0)
    _cupsSNMPWalk(snmp_fd, &current_addr, CUPS_SNMP_VERSION_1,
		  _cupsSNMPDefaultCommunity(), prtMarkerSuppliesLevel,
		  CUPS_SUPPLY_TIMEOUT, backend_walk_cb, NULL);

  if (page_count)
    *page_count = -1;

  if (printer_state)
    *printer_state = -1;

  if (num_supplies > 0)
  {
    int		i,			/* Looping var */
		percent,		/* Percent full */
		new_state,		/* New state value */
		change_state,		/* State change */
		new_supply_state = 0;	/* Supply state */
    char	value[CUPS_MAX_SUPPLIES * 4],
					/* marker-levels value string */
		*ptr;			/* Pointer into value string */
    cups_snmp_t	packet;			/* SNMP response packet */

   /*
    * Generate the marker-levels value string...
    */

    for (i = 0, ptr = value; i < num_supplies; i ++, ptr += strlen(ptr))
    {
      if (supplies[i].max_capacity > 0 && supplies[i].level >= 0)
	percent = 100 * supplies[i].level / supplies[i].max_capacity;
      else if (supplies[i].level >= 0 && supplies[i].level <= 100 &&
               (quirks & CUPS_SNMP_CAPACITY))
        percent = supplies[i].level;
      else
        percent = 50;

      if (supplies[i].sclass == CUPS_TC_receptacleThatIsFilled)
        percent = 100 - percent;

      if (percent <= 5)
      {
        switch (supplies[i].type)
        {
          case CUPS_TC_toner :
          case CUPS_TC_tonerCartridge :
              if (percent <= 1)
                new_supply_state |= CUPS_TONER_EMPTY;
              else
                new_supply_state |= CUPS_TONER_LOW;
              break;
          case CUPS_TC_ink :
          case CUPS_TC_inkCartridge :
          case CUPS_TC_inkRibbon :
          case CUPS_TC_solidWax :
          case CUPS_TC_ribbonWax :
              if (percent <= 1)
                new_supply_state |= CUPS_MARKER_SUPPLY_EMPTY;
              else
                new_supply_state |= CUPS_MARKER_SUPPLY_LOW;
              break;
          case CUPS_TC_developer :
              if (percent <= 1)
                new_supply_state |= CUPS_DEVELOPER_EMPTY;
              else
                new_supply_state |= CUPS_DEVELOPER_LOW;
              break;
          case CUPS_TC_coronaWire :
          case CUPS_TC_fuser :
          case CUPS_TC_opc :
          case CUPS_TC_transferUnit :
              if (percent <= 1)
                new_supply_state |= CUPS_OPC_LIFE_OVER;
              else
                new_supply_state |= CUPS_OPC_NEAR_EOL;
              break;
#if 0 /* Because no two vendors report waste containers the same, disable SNMP reporting of same */
          case CUPS_TC_wasteInk :
          case CUPS_TC_wastePaper :
          case CUPS_TC_wasteToner :
          case CUPS_TC_wasteWater :
          case CUPS_TC_wasteWax :
              if (percent <= 1)
                new_supply_state |= CUPS_WASTE_FULL;
              else
                new_supply_state |= CUPS_WASTE_ALMOST_FULL;
              break;
#endif /* 0 */
          case CUPS_TC_cleanerUnit :
          case CUPS_TC_fuserCleaningPad :
              if (percent <= 1)
                new_supply_state |= CUPS_CLEANER_LIFE_OVER;
              else
                new_supply_state |= CUPS_CLEANER_NEAR_EOL;
              break;
        }
      }

      if (i)
        *ptr++ = ',';

      if ((supplies[i].max_capacity > 0 || (quirks & CUPS_SNMP_CAPACITY)) &&
          supplies[i].level >= 0)
        snprintf(ptr, sizeof(value) - (size_t)(ptr - value), "%d", percent);
      else
        strlcpy(ptr, "-1", sizeof(value) - (size_t)(ptr - value));
    }

    fprintf(stderr, "ATTR: marker-levels=%s\n", value);

    if (supply_state < 0)
      change_state = 0xffff;
    else
      change_state = supply_state ^ new_supply_state;

    fprintf(stderr, "DEBUG: new_supply_state=%x, change_state=%x\n",
            new_supply_state, change_state);

    for (i = 0;
         i < (int)(sizeof(supply_states) / sizeof(supply_states[0]));
         i ++)
      if (change_state & supply_states[i].bit)
      {
	fprintf(stderr, "STATE: %c%s\n",
		(new_supply_state & supply_states[i].bit) ? '+' : '-',
		supply_states[i].keyword);
      }

    supply_state = new_supply_state;

   /*
    * Get the current printer status bits...
    */

    if (!_cupsSNMPWrite(snmp_fd, addr, CUPS_SNMP_VERSION_1,
                       _cupsSNMPDefaultCommunity(), CUPS_ASN1_GET_REQUEST, 1,
                       hrPrinterDetectedErrorState))
      return (-1);

    if (!_cupsSNMPRead(snmp_fd, &packet, CUPS_SUPPLY_TIMEOUT) ||
        packet.object_type != CUPS_ASN1_OCTET_STRING)
      return (-1);

    if (packet.object_value.string.num_bytes == 2)
      new_state = (packet.object_value.string.bytes[0] << 8) |
		  packet.object_value.string.bytes[1];
    else if (packet.object_value.string.num_bytes == 1)
      new_state = (packet.object_value.string.bytes[0] << 8);
    else
      new_state = 0;

    if (current_state < 0)
      change_state = 0xffff;
    else
      change_state = current_state ^ new_state;

    fprintf(stderr, "DEBUG: new_state=%x, change_state=%x\n", new_state,
            change_state);

    for (i = 0;
         i < (int)(sizeof(printer_states) / sizeof(printer_states[0]));
         i ++)
      if (change_state & printer_states[i].bit)
      {
	fprintf(stderr, "STATE: %c%s\n",
		(new_state & printer_states[i].bit) ? '+' : '-',
		printer_states[i].keyword);
      }

    current_state = new_state;

   /*
    * Get the current printer state...
    */

    if (printer_state)
    {
      if (!_cupsSNMPWrite(snmp_fd, addr, CUPS_SNMP_VERSION_1,
			 _cupsSNMPDefaultCommunity(), CUPS_ASN1_GET_REQUEST, 1,
			 hrPrinterStatus))
	return (-1);

      if (!_cupsSNMPRead(snmp_fd, &packet, CUPS_SUPPLY_TIMEOUT) ||
	  packet.object_type != CUPS_ASN1_INTEGER)
	return (-1);

      *printer_state = packet.object_value.integer;
    }

   /*
    * Get the current page count...
    */

    if (page_count)
    {
      if (!_cupsSNMPWrite(snmp_fd, addr, CUPS_SNMP_VERSION_1,
			 _cupsSNMPDefaultCommunity(), CUPS_ASN1_GET_REQUEST, 1,
			 prtMarkerLifeCount))
	return (-1);

      if (!_cupsSNMPRead(snmp_fd, &packet, CUPS_SUPPLY_TIMEOUT) ||
	  packet.object_type != CUPS_ASN1_COUNTER)
	return (-1);

      *page_count = packet.object_value.counter;
    }

    return (0);
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
		value[CUPS_MAX_SUPPLIES * (CUPS_SNMP_MAX_STRING * 4 + 3)],
					/* Value string */
		*ptr,			/* Pointer into value string */
		*name_ptr;		/* Pointer into name string */
  cups_snmp_t	packet;			/* SNMP response packet */
  ppd_file_t	*ppd;			/* PPD file for this queue */
  ppd_attr_t	*ppdattr;		/* cupsSNMPSupplies attribute */
  static const char * const types[] =	/* Supply types */
		{
		  "other",
		  "unknown",
		  "toner",
		  "waste-toner",
		  "ink",
		  "ink-cartridge",
		  "ink-ribbon",
		  "waste-ink",
		  "opc",
		  "developer",
		  "fuser-oil",
		  "solid-wax",
		  "ribbon-wax",
		  "waste-wax",
		  "fuser",
		  "corona-wire",
		  "fuser-oil-wick",
		  "cleaner-unit",
		  "fuser-cleaning-pad",
		  "transfer-unit",
		  "toner-cartridge",
		  "fuser-oiler",
		  "water",
		  "waste-water",
		  "glue-water-additive",
		  "waste-paper",
		  "binding-supply",
		  "banding-supply",
		  "stitching-wire",
		  "shrink-wrap",
		  "paper-wrap",
		  "staples",
		  "inserts",
		  "covers"
		};


 /*
  * Reset state information...
  */

  current_addr  = *addr;
  current_state = -1;
  num_supplies  = -1;
  charset       = -1;

  memset(supplies, 0, sizeof(supplies));

 /*
  * See if we should be getting supply levels via SNMP...
  */

  if ((ppd = ppdOpenFile(getenv("PPD"))) == NULL ||
      ((ppdattr = ppdFindAttr(ppd, "cupsSNMPSupplies", NULL)) != NULL &&
       ppdattr->value && _cups_strcasecmp(ppdattr->value, "true")))
  {
    ppdClose(ppd);
    return;
  }

  if ((ppdattr = ppdFindAttr(ppd, "cupsSNMPQuirks", NULL)) != NULL)
  {
    if (!_cups_strcasecmp(ppdattr->value, "capacity"))
      quirks |= CUPS_SNMP_CAPACITY;
  }

  ppdClose(ppd);

 /*
  * Get the device description...
  */

  if (!_cupsSNMPWrite(snmp_fd, addr, CUPS_SNMP_VERSION_1,
		     _cupsSNMPDefaultCommunity(), CUPS_ASN1_GET_REQUEST, 1,
		     hrDeviceDescr))
    return;

  if (!_cupsSNMPRead(snmp_fd, &packet, CUPS_SUPPLY_TIMEOUT) ||
      packet.object_type != CUPS_ASN1_OCTET_STRING)
  {
    strlcpy(description, "Unknown", sizeof(description));
    num_supplies = 0;
  }
  else
    strlcpy(description, (char *)packet.object_value.string.bytes,
            sizeof(description));

  fprintf(stderr, "DEBUG2: hrDeviceDesc=\"%s\"\n", description);

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
    *     3 num_supplies charset
    *     device description
    *     supply structures...
    */

    if (cupsFileGets(cachefile, value, sizeof(value)))
    {
      if (sscanf(value, "3 %d%d", &num_supplies, &charset) == 2 &&
          num_supplies <= CUPS_MAX_SUPPLIES &&
          cupsFileGets(cachefile, value, sizeof(value)))
      {
        if (!strcmp(description, value))
	  cupsFileRead(cachefile, (char *)supplies,
	               (size_t)num_supplies * sizeof(backend_supplies_t));
        else
	{
	  num_supplies = -1;
	  charset      = -1;
	}
      }
      else
      {
        num_supplies = -1;
	charset      = -1;
      }
    }

    cupsFileClose(cachefile);
  }

 /*
  * If the cache information isn't correct, scan for supplies...
  */

  if (charset < 0)
  {
   /*
    * Get the configured character set...
    */

    int	oid[CUPS_SNMP_MAX_OID];		/* OID for character set */


    if (!_cupsSNMPWrite(snmp_fd, &current_addr, CUPS_SNMP_VERSION_1,
			_cupsSNMPDefaultCommunity(), CUPS_ASN1_GET_REQUEST, 1,
			prtGeneralCurrentLocalization))
      return;

    if (!_cupsSNMPRead(snmp_fd, &packet, CUPS_SUPPLY_TIMEOUT) ||
	packet.object_type != CUPS_ASN1_INTEGER)
    {
      fprintf(stderr,
              "DEBUG: prtGeneralCurrentLocalization type is %x, expected %x!\n",
	      packet.object_type, CUPS_ASN1_INTEGER);
      return;
    }

    fprintf(stderr, "DEBUG2: prtGeneralCurrentLocalization=%d\n",
            packet.object_value.integer);

    _cupsSNMPCopyOID(oid, prtLocalizationCharacterSet, CUPS_SNMP_MAX_OID);
    oid[prtLocalizationCharacterSetOffset - 2] = packet.object_value.integer;


    if (!_cupsSNMPWrite(snmp_fd, &current_addr, CUPS_SNMP_VERSION_1,
			_cupsSNMPDefaultCommunity(), CUPS_ASN1_GET_REQUEST, 1,
			oid))
      return;

    if (!_cupsSNMPRead(snmp_fd, &packet, CUPS_SUPPLY_TIMEOUT) ||
	packet.object_type != CUPS_ASN1_INTEGER)
    {
      fprintf(stderr,
              "DEBUG: prtLocalizationCharacterSet type is %x, expected %x!\n",
	      packet.object_type, CUPS_ASN1_INTEGER);
      return;
    }

    fprintf(stderr, "DEBUG2: prtLocalizationCharacterSet=%d\n",
	    packet.object_value.integer);
    charset = packet.object_value.integer;
  }

  if (num_supplies < 0)
  {
   /*
    * Walk the printer configuration information...
    */

    _cupsSNMPWalk(snmp_fd, &current_addr, CUPS_SNMP_VERSION_1,
		  _cupsSNMPDefaultCommunity(), prtMarkerSuppliesEntry,
		  CUPS_SUPPLY_TIMEOUT, backend_walk_cb, NULL);
  }

 /*
  * Save the cached information...
  */

  if (num_supplies < 0)
    num_supplies = 0;

  if ((cachefile = cupsFileOpen(cachefilename, "w")) != NULL)
  {
    cupsFilePrintf(cachefile, "3 %d %d\n", num_supplies, charset);
    cupsFilePrintf(cachefile, "%s\n", description);

    if (num_supplies > 0)
      cupsFileWrite(cachefile, (char *)supplies,
                    (size_t)num_supplies * sizeof(backend_supplies_t));

    cupsFileClose(cachefile);
  }

  if (num_supplies <= 0)
    return;

 /*
  * Get the colors...
  */

  for (i = 0; i < num_supplies; i ++)
    strlcpy(supplies[i].color, "none", sizeof(supplies[i].color));

  _cupsSNMPWalk(snmp_fd, &current_addr, CUPS_SNMP_VERSION_1,
                _cupsSNMPDefaultCommunity(), prtMarkerColorantValue,
	        CUPS_SUPPLY_TIMEOUT, backend_walk_cb, NULL);

 /*
  * Output the marker-colors attribute...
  */

  for (i = 0, ptr = value; i < num_supplies; i ++, ptr += strlen(ptr))
  {
    if (i)
      *ptr++ = ',';

    strlcpy(ptr, supplies[i].color, sizeof(value) - (size_t)(ptr - value));
  }

  fprintf(stderr, "ATTR: marker-colors=%s\n", value);

 /*
  * Output the marker-names attribute (the double quoting is necessary to deal
  * with embedded quotes and commas in the marker names...)
  */

  for (i = 0, ptr = value; i < num_supplies; i ++)
  {
    if (i)
      *ptr++ = ',';

    *ptr++ = '\'';
    *ptr++ = '\"';
    for (name_ptr = supplies[i].name; *name_ptr;)
    {
      if (*name_ptr == '\\' || *name_ptr == '\"' || *name_ptr == '\'')
      {
        *ptr++ = '\\';
        *ptr++ = '\\';
        *ptr++ = '\\';
      }

      *ptr++ = *name_ptr++;
    }
    *ptr++ = '\"';
    *ptr++ = '\'';
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
      strlcpy(ptr, "unknown", sizeof(value) - (size_t)(ptr - value));
    else
      strlcpy(ptr, types[type - CUPS_TC_other], sizeof(value) - (size_t)(ptr - value));
  }

  fprintf(stderr, "ATTR: marker-types=%s\n", value);
}


/*
 * 'backend_walk_cb()' - Interpret the supply value responses.
 */

static void
backend_walk_cb(cups_snmp_t *packet,	/* I - SNMP packet */
                void        *data)	/* I - User data (unused) */
{
  int	i, j, k;			/* Looping vars */
  static const char * const colors[][2] =
  {					/* Standard color names */
    { "black",         "#000000" },
    { "blue",          "#0000FF" },
    { "brown",         "#A52A2A" },
    { "cyan",          "#00FFFF" },
    { "dark-gray",     "#404040" },
    { "dark gray",     "#404040" },
    { "dark-yellow",   "#FFCC00" },
    { "dark yellow",   "#FFCC00" },
    { "gold",          "#FFD700" },
    { "gray",          "#808080" },
    { "green",         "#00FF00" },
    { "light-black",   "#606060" },
    { "light black",   "#606060" },
    { "light-cyan",    "#E0FFFF" },
    { "light cyan",    "#E0FFFF" },
    { "light-gray",    "#D3D3D3" },
    { "light gray",    "#D3D3D3" },
    { "light-magenta", "#FF77FF" },
    { "light magenta", "#FF77FF" },
    { "magenta",       "#FF00FF" },
    { "orange",        "#FFA500" },
    { "red",           "#FF0000" },
    { "silver",        "#C0C0C0" },
    { "white",         "#FFFFFF" },
    { "yellow",        "#FFFF00" }
  };


  (void)data;

  if (_cupsSNMPIsOIDPrefixed(packet, prtMarkerColorantValue) &&
      packet->object_type == CUPS_ASN1_OCTET_STRING)
  {
   /*
    * Get colorant...
    */

    i = packet->object_name[prtMarkerColorantValueOffset];

    fprintf(stderr, "DEBUG2: prtMarkerColorantValue.1.%d = \"%s\"\n", i,
            (char *)packet->object_value.string.bytes);

    for (j = 0; j < num_supplies; j ++)
      if (supplies[j].colorant == i)
      {
	for (k = 0; k < (int)(sizeof(colors) / sizeof(colors[0])); k ++)
	  if (!_cups_strcasecmp(colors[k][0],
	                        (char *)packet->object_value.string.bytes))
	  {
	    strlcpy(supplies[j].color, colors[k][1], sizeof(supplies[j].color));
	    break;
	  }
      }
  }
  else if (_cupsSNMPIsOIDPrefixed(packet, prtMarkerSuppliesColorantIndex))
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
  else if (_cupsSNMPIsOIDPrefixed(packet, prtMarkerSuppliesDescription))
  {
   /*
    * Get supply name/description...
    */

    i = packet->object_name[prtMarkerSuppliesDescriptionOffset];
    if (i < 1 || i > CUPS_MAX_SUPPLIES ||
        packet->object_type != CUPS_ASN1_OCTET_STRING)
      return;

    if (i > num_supplies)
      num_supplies = i;

    switch (charset)
    {
      case CUPS_TC_csASCII :
      case CUPS_TC_csUTF8 :
      case CUPS_TC_csUnicodeASCII :
	  strlcpy(supplies[i - 1].name,
	          (char *)packet->object_value.string.bytes,
		  sizeof(supplies[0].name));
          break;

      case CUPS_TC_csISOLatin1 :
      case CUPS_TC_csUnicodeLatin1 :
	  cupsCharsetToUTF8((cups_utf8_t *)supplies[i - 1].name,
	                    (char *)packet->object_value.string.bytes,
		            sizeof(supplies[0].name), CUPS_ISO8859_1);
          break;

      case CUPS_TC_csShiftJIS :
      case CUPS_TC_csWindows31J : /* Close enough for our purposes */
	  cupsCharsetToUTF8((cups_utf8_t *)supplies[i - 1].name,
	                    (char *)packet->object_value.string.bytes,
		            sizeof(supplies[0].name), CUPS_JIS_X0213);
          break;

      case CUPS_TC_csUCS4 :
      case CUPS_TC_csUTF32 :
      case CUPS_TC_csUTF32BE :
      case CUPS_TC_csUTF32LE :
	  cupsUTF32ToUTF8((cups_utf8_t *)supplies[i - 1].name,
	                  (cups_utf32_t *)packet->object_value.string.bytes,
			  sizeof(supplies[0].name));
          break;

      case CUPS_TC_csUnicode :
      case CUPS_TC_csUTF16BE :
      case CUPS_TC_csUTF16LE :
	  utf16_to_utf8((cups_utf8_t *)supplies[i - 1].name,
	                packet->object_value.string.bytes,
			packet->object_value.string.num_bytes,
			sizeof(supplies[0].name), charset == CUPS_TC_csUTF16LE);
          break;

      default :
	 /*
	  * If we get here, the printer is using an unknown character set and
	  * we just want to copy characters that look like ASCII...
	  */

          {
	    char	*src, *dst;	/* Pointers into strings */

           /*
	    * Loop safe because both the object_value and supplies char arrays
	    * are CUPS_SNMP_MAX_STRING elements long.
	    */

            for (src = (char *)packet->object_value.string.bytes,
	             dst = supplies[i - 1].name;
		 *src;
		 src ++)
	    {
	      if ((*src & 0x80) || *src < ' ' || *src == 0x7f)
	        *dst++ = '?';
	      else
	        *dst++ = *src;
	    }

	    *dst = '\0';
	  }
	  break;
    }

    fprintf(stderr, "DEBUG2: prtMarkerSuppliesDescription.1.%d = \"%s\"\n", i,
            supplies[i - 1].name);

  }
  else if (_cupsSNMPIsOIDPrefixed(packet, prtMarkerSuppliesLevel))
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
  else if (_cupsSNMPIsOIDPrefixed(packet, prtMarkerSuppliesMaxCapacity) &&
           !(quirks & CUPS_SNMP_CAPACITY))
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

    if (supplies[i - 1].max_capacity == 0 &&
        packet->object_value.integer > 0)
      supplies[i - 1].max_capacity = packet->object_value.integer;
  }
  else if (_cupsSNMPIsOIDPrefixed(packet, prtMarkerSuppliesClass))
  {
   /*
    * Get marker class...
    */

    i = packet->object_name[prtMarkerSuppliesClassOffset];
    if (i < 1 || i > CUPS_MAX_SUPPLIES ||
        packet->object_type != CUPS_ASN1_INTEGER)
      return;

    fprintf(stderr, "DEBUG2: prtMarkerSuppliesClass.1.%d = %d\n", i,
            packet->object_value.integer);

    if (i > num_supplies)
      num_supplies = i;

    supplies[i - 1].sclass = packet->object_value.integer;
  }
  else if (_cupsSNMPIsOIDPrefixed(packet, prtMarkerSuppliesType))
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
  else if (_cupsSNMPIsOIDPrefixed(packet, prtMarkerSuppliesSupplyUnit))
  {
   /*
    * Get units for capacity...
    */

    i = packet->object_name[prtMarkerSuppliesSupplyUnitOffset];
    if (i < 1 || i > CUPS_MAX_SUPPLIES ||
        packet->object_type != CUPS_ASN1_INTEGER)
      return;

    fprintf(stderr, "DEBUG2: prtMarkerSuppliesSupplyUnit.1.%d = %d\n", i,
            packet->object_value.integer);

    if (i > num_supplies)
      num_supplies = i;

    if (packet->object_value.integer == CUPS_TC_percent)
      supplies[i - 1].max_capacity = 100;
  }
}


/*
 * 'utf16_to_utf8()' - Convert UTF-16 text to UTF-8.
 */

static void
utf16_to_utf8(
    cups_utf8_t         *dst,		/* I - Destination buffer */
    const unsigned char *src,		/* I - Source string */
    size_t		srcsize,	/* I - Size of source string */
    size_t              dstsize,	/* I - Size of destination buffer */
    int                 le)		/* I - Source is little-endian? */
{
  cups_utf32_t	ch,			/* Current character */
		temp[CUPS_SNMP_MAX_STRING],
					/* UTF-32 string */
		*ptr;			/* Pointer into UTF-32 string */


  for (ptr = temp; srcsize >= 2;)
  {
    if (le)
      ch = (cups_utf32_t)(src[0] | (src[1] << 8));
    else
      ch = (cups_utf32_t)((src[0] << 8) | src[1]);

    src += 2;
    srcsize -= 2;

    if (ch >= 0xd800 && ch <= 0xdbff && srcsize >= 2)
    {
     /*
      * Multi-word UTF-16 char...
      */

      cups_utf32_t lch;			/* Lower word */


      if (le)
	lch = (cups_utf32_t)(src[0] | (src[1] << 8));
      else
	lch = (cups_utf32_t)((src[0] << 8) | src[1]);

      if (lch >= 0xdc00 && lch <= 0xdfff)
      {
	src += 2;
	srcsize -= 2;

	ch = (((ch & 0x3ff) << 10) | (lch & 0x3ff)) + 0x10000;
      }
    }

    if (ptr < (temp + CUPS_SNMP_MAX_STRING - 1))
      *ptr++ = ch;
  }

  *ptr = '\0';

  cupsUTF32ToUTF8(dst, temp, (int)dstsize);
}


/*
 * End of "$Id: snmp-supplies.c 12228 2014-10-21 13:42:05Z msweet $".
 */
