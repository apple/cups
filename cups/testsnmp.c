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
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "string.h"
#include "snmp.h"


/*
 * Local functions...
 */

static int	*scan_oid(char *s, int *oid, int oidsize);
static int	show_oid(int fd, char *s, http_addr_t *addr);


/*
 * 'main()' - Main entry.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  int			i;		/* Looping var */
  int			fd;		/* SNMP socket */
  http_addrlist_t	*host;		/* Address of host */


  if (argc < 2)
  {
    puts("Usage: ./testsnmp host-or-ip");
    return (1);
  }

  if ((host = httpAddrGetList(argv[1], AF_UNSPEC, "161")) == NULL)
  {
    printf("Unable to find \"%s\"!\n", argv[1]);
    return (1);
  }

  fputs("cupsSNMPOpen: ", stdout);

  if ((fd = cupsSNMPOpen()) < 0)
  {
    printf("FAIL (%s)\n", strerror(errno));
    return (1);
  }

  puts("PASS");

  if (argc > 2)
  {
   /*
    * Query OIDs from the command-line...
    */

    for (i = 2; i < argc; i ++)
      if (!strcmp(argv[i], "-d"))
        cupsSNMPSetDebug(10);
      else if (!show_oid(fd, argv[i], &(host->addr)))
        return (1);
  }
  else if (!show_oid(fd, (char *)"1.3.6.1.2.1.43.10.2.1.4.1.1", &(host->addr)))
    return (1);
  
  return (0);
}


/*
 * 'scan_oid()' - Scan an OID value.
 */

static int *				/* O - OID or NULL on error */
scan_oid(char *s,			/* I - OID string */
         int  *oid,			/* I - OID array */
	 int  oidsize)			/* I - Size of OID array in integers */
{
  int	i;				/* Index into OID array */
  char	*ptr;				/* Pointer into string */


  for (ptr = s, i = 0, oidsize --; ptr && *ptr && i < oidsize; i ++)
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
         char        *s,		/* I - OID to query */
	 http_addr_t *addr)		/* I - Address to query */
{
  int		i;			/* Looping var */
  int		root[CUPS_SNMP_MAX_OID],/* Root OID */
		oid[CUPS_SNMP_MAX_OID];	/* OID */
  cups_snmp_t	packet;			/* SNMP packet */
  int		walk = 0;		/* Walk all OIDs? */


  if (!strcmp(s, "-w"))
  {
    scan_oid("1.3.6.1.2.1.43", oid, sizeof(oid) / sizeof(oid[0]));
    walk = 1;
  }
  else if (!scan_oid(s, oid, sizeof(oid) / sizeof(oid[0])))
  {
    puts("FAIL (bad OID)");
    return (0);
  }

  memcpy(root, oid, sizeof(root));

  do
  {
    printf("cupsSNMPWrite(%d", oid[0]);
    for (i = 1; oid[i] >= 0; i ++)
      printf(".%d", oid[i]);
    fputs("): ", stdout);

    if (!cupsSNMPWrite(fd, addr, CUPS_SNMP_VERSION_1, "public",
		       walk ? CUPS_ASN1_GET_NEXT_REQUEST :
		              CUPS_ASN1_GET_REQUEST, 1, oid))
    {
      puts("FAIL");
      return (0);
    }

    puts("PASS");

    fputs("cupsSNMPRead(5000): ", stdout);

    if (!cupsSNMPRead(fd, &packet, 5000))
    {
      puts("FAIL (timeout)");
      return (0);
    }

    if (walk)
    {
      if (!cupsSNMPIsOIDPrefixed(&packet, root))
      {
        puts("PASS (end-of-MIB)");
	return (0);
      }
    }
    else if (!cupsSNMPIsOID(&packet, oid))
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

    if (walk)
    {
      if (cupsSNMPIsOID(&packet, oid))
      {
        puts("FAIL (same OID!)");
	return (0);
      }

      cupsSNMPCopyOID(oid, packet.object_name, sizeof(oid) / sizeof(oid[0]));
    }

    printf("PASS (%d", packet.object_name[0]);
    for (i = 1; packet.object_name[i] >= 0; i ++)
      printf(".%d", packet.object_name[i]);
    fputs(" = ", stdout);

    switch (packet.object_type)
    {
      case CUPS_ASN1_BOOLEAN :
	  printf("BOOLEAN %s)\n",
		 packet.object_value.boolean ? "TRUE" : "FALSE");
	  break;

      case CUPS_ASN1_INTEGER :
	  printf("INTEGER %d)\n", packet.object_value.integer);
	  break;

      case CUPS_ASN1_BIT_STRING :
	  printf("BIT-STRING \"%s\")\n", packet.object_value.string);
	  break;

      case CUPS_ASN1_OCTET_STRING :
	  printf("OCTET-STRING \"%s\")\n", packet.object_value.string);
	  break;

      case CUPS_ASN1_NULL_VALUE :
	  puts("NULL-VALUE)");
	  break;

      case CUPS_ASN1_OID :
	  printf("OID %d", packet.object_value.oid[0]);
	  for (i = 1; packet.object_value.oid[i] >= 0; i ++)
	    printf(".%d", packet.object_value.oid[i]);
	  puts(")");
	  break;

      case CUPS_ASN1_HEX_STRING :
          fputs("Hex-STRING", stdout);
	  for (i = 0; i < packet.object_value.hex_string.num_bytes; i ++)
	    printf(" %02X", packet.object_value.hex_string.bytes[i]);
	  puts(")");
	  break;

      case CUPS_ASN1_COUNTER :
	  printf("Counter %d)\n", packet.object_value.counter);
	  break;

      case CUPS_ASN1_GAUGE :
	  printf("Gauge %u)\n", packet.object_value.gauge);
	  break;

      case CUPS_ASN1_TIMETICKS :
	  printf("Timeticks %u days, %u:%02u:%02u.%02u)\n",
	         packet.object_value.timeticks / 8640000,
	         (packet.object_value.timeticks / 360000) % 24,
		 (packet.object_value.timeticks / 6000) % 60,
		 (packet.object_value.timeticks / 100) % 60,
		 packet.object_value.timeticks % 100);
	  break;

      default :
	  printf("Unknown-%X)\n", packet.object_type);
	  break;
    }
  }
  while (walk);

  return (1);
}


/*
 * End of "$Id$".
 */
