/*
 * "$Id$"
 *
 *   SNMP test program for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2008 by Apple Inc.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   main()     - Main entry.
 *   scan_oid() - Scan an OID value.
 *   show_oid() - Show the specified OID.
 *   usage()    - Show program usage and exit.
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "string.h"
#include "snmp-private.h"


/*
 * Local functions...
 */

static void	print_packet(cups_snmp_t *packet, void *data);
static int	*scan_oid(const char *s, int *oid, int oidsize);
static int	show_oid(int fd, const char *community,
		         http_addr_t *addr, const char *s, int walk);
static void	usage(void);


/*
 * 'main()' - Main entry.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  int			i;		/* Looping var */
  int			fd = -1;	/* SNMP socket */
  http_addrlist_t	*host = NULL;	/* Address of host */
  int			walk = 0;	/* Walk OIDs? */
  char			*oid = NULL;	/* Last OID shown */
  const char		*community;	/* Community name */


  fputs("_cupsSNMPDefaultCommunity: ", stdout);

  if ((community = _cupsSNMPDefaultCommunity()) == NULL)
  {
    puts("FAIL (NULL community name)");
    return (1);
  }

  printf("PASS (%s)\n", community);

 /*
  * Query OIDs from the command-line...
  */

  for (i = 1; i < argc; i ++)
    if (!strcmp(argv[i], "-c"))
    {
      i ++;

      if (i >= argc)
        usage();
      else
        community = argv[i];
    }
    else if (!strcmp(argv[i], "-d"))
      _cupsSNMPSetDebug(10);
    else if (!strcmp(argv[i], "-w"))
      walk = 1;
    else if (!host)
    {
      if ((host = httpAddrGetList(argv[i], AF_UNSPEC, "161")) == NULL)
      {
	printf("testsnmp: Unable to find \"%s\"!\n", argv[1]);
	return (1);
      }

      if (fd < 0)
      {
	fputs("_cupsSNMPOpen: ", stdout);

	if ((fd = _cupsSNMPOpen(host->addr.addr.sa_family)) < 0)
	{
	  printf("FAIL (%s)\n", strerror(errno));
	  return (1);
	}

	puts("PASS");
      }
    }
    else if (!show_oid(fd, community, &(host->addr), argv[i], walk))
      return (1);
    else
      oid = argv[i];

  if (!host)
    usage();

  if (!oid)
  {
    if (!show_oid(fd, community,  &(host->addr),
                  walk ? "1.3.6.1.2.1.43" :
		         "1.3.6.1.2.1.43.10.2.1.4.1.1", walk))
      return (1);
  }
  
  return (0);
}


/*
 * 'print_packet()' - Print the contents of the response packet.
 */

static void
print_packet(cups_snmp_t *packet,	/* I - SNMP response packet */
             void        *data)		/* I - User data pointer (not used) */
{
  int	i;				/* Looping var */


  (void)data;

  printf("%d", packet->object_name[0]);
  for (i = 1; packet->object_name[i] >= 0; i ++)
    printf(".%d", packet->object_name[i]);
  fputs(" = ", stdout);

  switch (packet->object_type)
  {
    case CUPS_ASN1_BOOLEAN :
	printf("BOOLEAN %s\n",
	       packet->object_value.boolean ? "TRUE" : "FALSE");
	break;

    case CUPS_ASN1_INTEGER :
	printf("INTEGER %d\n", packet->object_value.integer);
	break;

    case CUPS_ASN1_BIT_STRING :
	printf("BIT-STRING \"%s\"\n", packet->object_value.string);
	break;

    case CUPS_ASN1_OCTET_STRING :
	printf("OCTET-STRING \"%s\"\n", packet->object_value.string);
	break;

    case CUPS_ASN1_NULL_VALUE :
	puts("NULL-VALUE");
	break;

    case CUPS_ASN1_OID :
	printf("OID %d", packet->object_value.oid[0]);
	for (i = 1; packet->object_value.oid[i] >= 0; i ++)
	  printf(".%d", packet->object_value.oid[i]);
	putchar('\n');
	break;

    case CUPS_ASN1_HEX_STRING :
	fputs("Hex-STRING", stdout);
	for (i = 0; i < packet->object_value.hex_string.num_bytes; i ++)
	  printf(" %02X", packet->object_value.hex_string.bytes[i]);
	putchar('\n');
	break;

    case CUPS_ASN1_COUNTER :
	printf("Counter %d\n", packet->object_value.counter);
	break;

    case CUPS_ASN1_GAUGE :
	printf("Gauge %u\n", packet->object_value.gauge);
	break;

    case CUPS_ASN1_TIMETICKS :
	printf("Timeticks %u days, %u:%02u:%02u.%02u\n",
	       packet->object_value.timeticks / 8640000,
	       (packet->object_value.timeticks / 360000) % 24,
	       (packet->object_value.timeticks / 6000) % 60,
	       (packet->object_value.timeticks / 100) % 60,
	       packet->object_value.timeticks % 100);
	break;

    default :
	printf("Unknown-%X\n", packet->object_type);
	break;
  }
}


/*
 * 'scan_oid()' - Scan an OID value.
 */

static int *				/* O - OID or NULL on error */
scan_oid(const char *s,			/* I - OID string */
         int        *oid,		/* I - OID array */
	 int        oidsize)		/* I - Size of OID array in integers */
{
  int	i;				/* Index into OID array */
  char	*ptr;				/* Pointer into string */


  for (ptr = (char *)s, i = 0, oidsize --; ptr && *ptr && i < oidsize; i ++)
  {
    if (!isdigit(*ptr & 255))
      return (NULL);

    oid[i] = strtol(ptr, &ptr, 10);
    if (ptr && *ptr == '.')
      ptr ++;
  }

  if (i >= oidsize)
    return (NULL);

  oid[i] = -1;

  return (oid);
}


/*
 * 'show_oid()' - Show the specified OID.
 */

static int				/* O - 1 on success, 0 on error */
show_oid(int         fd,		/* I - SNMP socket */
         const char  *community,	/* I - Community name */
	 http_addr_t *addr,		/* I - Address to query */
         const char  *s,		/* I - OID to query */
	 int         walk)		/* I - Walk OIDs? */
{
  int		i;			/* Looping var */
  int		oid[CUPS_SNMP_MAX_OID];	/* OID */
  cups_snmp_t	packet;			/* SNMP packet */


  if (!scan_oid(s, oid, sizeof(oid) / sizeof(oid[0])))
  {
    puts("testsnmp: Bad OID");
    return (0);
  }

  if (walk)
  {
    printf("_cupsSNMPWalk(%d", oid[0]);
    for (i = 1; oid[i] >= 0; i ++)
      printf(".%d", oid[i]);
    puts("):");

    if (_cupsSNMPWalk(fd, addr, CUPS_SNMP_VERSION_1, community, oid, 5.0,
                     print_packet, NULL) < 0)
    {
      printf("FAIL (%s)\n", strerror(errno));
      return (0);
    }
  }
  else
  {
    printf("_cupsSNMPWrite(%d", oid[0]);
    for (i = 1; oid[i] >= 0; i ++)
      printf(".%d", oid[i]);
    fputs("): ", stdout);

    if (!_cupsSNMPWrite(fd, addr, CUPS_SNMP_VERSION_1, community,
		       CUPS_ASN1_GET_REQUEST, 1, oid))
    {
      printf("FAIL (%s)\n", strerror(errno));
      return (0);
    }

    puts("PASS");

    fputs("_cupsSNMPRead(5.0): ", stdout);

    if (!_cupsSNMPRead(fd, &packet, 5.0))
    {
      puts("FAIL (timeout)");
      return (0);
    }

    if (!_cupsSNMPIsOID(&packet, oid))
    {
      printf("FAIL (bad OID %d", packet.object_name[0]);
      for (i = 1; packet.object_name[i] >= 0; i ++)
	printf(".%d", packet.object_name[i]);
      puts(")");
      return (0);
    }

    if (packet.error)
    {
      printf("FAIL (%s)\n", packet.error);
      return (0);
    }

    puts("PASS");

    print_packet(&packet, NULL);
  }

  return (1);
}


/*
 * 'usage()' - Show program usage and exit.
 */

static void
usage(void)
{
  puts("Usage: testsnmp [options] host-or-ip [oid ...]");
  puts("");
  puts("Options:");
  puts("");
  puts("  -c community    Set community name");
  puts("  -d              Enable debugging");
  puts("  -w              Walk all OIDs under the specified one");

  exit (1);
}


/*
 * End of "$Id$".
 */
