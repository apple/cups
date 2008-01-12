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
 *   main() - Main entry.
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
      if (!show_oid(fd, argv[i], &(host->addr)))
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
    if (*ptr == '.')
      ptr ++;
  }

  if (i >= oidsize)
    return (NULL);

  oid[i] = 0;

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
  int		oid[255];		/* OID */
  cups_snmp_t	packet;			/* SNMP packet */


  printf("cupsSNMPWrite(%s): ", s);

  if (!scan_oid(s, oid, sizeof(oid) / sizeof(oid[0])))
  {
    puts("FAIL (bad OID)");
    return (0);
  }

  if (!cupsSNMPWrite(fd, addr, CUPS_SNMP_VERSION_1, "public",
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

  if (!cupsSNMPIsOID(&packet, oid))
  {
    puts("FAIL (bad OID)");
    return (0);
  }

  if (packet.error)
  {
    printf("FAIL (%s)\n", packet.error);
    return (0);
  }

  switch (packet.object_type)
  {
    case CUPS_ASN1_BOOLEAN :
        printf("PASS (BOOLEAN %s)\n",
	       packet.object_value.boolean ? "TRUE" : "FALSE");
        break;

    case CUPS_ASN1_INTEGER :
        printf("PASS (INTEGER %d)\n", packet.object_value.integer);
        break;

    case CUPS_ASN1_BIT_STRING :
        printf("PASS (BIT-STRING \"%s\")\n", packet.object_value.string);
        break;

    case CUPS_ASN1_OCTET_STRING :
        printf("PASS (OCTET-STRING \"%s\")\n", packet.object_value.string);
        break;

    case CUPS_ASN1_NULL_VALUE :
        puts("PASS (NULL-VALUE)");
        break;

    case CUPS_ASN1_OID :
        printf("PASS (OID %d", packet.object_value.oid[0]);
	for (i = 1; packet.object_value.oid[i]; i ++)
	  printf(".%d", packet.object_value.oid[i]);
	puts(")");
        break;

    case CUPS_ASN1_COUNTER :
        printf("PASS (Counter %d)\n", packet.object_value.counter);
        break;

    case CUPS_ASN1_GAUGE:
        printf("PASS (Gauge %u)\n", packet.object_value.gauge);
        break;

    default :
        printf("PASS (Unknown-%X)\n", packet.object_type);
	break;
  }

  return (1);
}


/*
 * End of "$Id$".
 */
