/*
 * "$Id$"
 *
 *   SNMP definitions for the Common UNIX Printing System (CUPS).
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
 */

#ifndef _CUPS_SNMP_H_
#  define _CUPS_SNMP_H_


/*
 * Include necessary headers.
 */

#include "http.h"


/*
 * Constants...
 */

#define CUPS_SNMP_PORT		161	/* SNMP well-known port */
#define CUPS_SNMP_MAX_OID	128	/* Maximum number of OID numbers */
#define CUPS_SNMP_MAX_PACKET	1472	/* Maximum size of SNMP packet */
#define CUPS_SNMP_MAX_STRING	512	/* Maximum size of string */
#define CUPS_SNMP_VERSION_1	0	/* SNMPv1 */


/*
 * Types...
 */

enum cups_asn1_e			/**** ASN1 request/object types ****/
{
  CUPS_ASN1_END_OF_CONTENTS = 0x00,	/* End-of-contents */
  CUPS_ASN1_BOOLEAN = 0x01,		/* BOOLEAN */
  CUPS_ASN1_INTEGER = 0x02,		/* INTEGER or ENUMERATION */
  CUPS_ASN1_BIT_STRING = 0x03,		/* BIT STRING */
  CUPS_ASN1_OCTET_STRING = 0x04,	/* OCTET STRING */
  CUPS_ASN1_NULL_VALUE = 0x05,		/* NULL VALUE */
  CUPS_ASN1_OID = 0x06,			/* OBJECT IDENTIFIER */
  CUPS_ASN1_SEQUENCE = 0x30,		/* SEQUENCE */
  CUPS_ASN1_HEX_STRING = 0x40,		/* Binary string aka Hex-STRING */
  CUPS_ASN1_COUNTER = 0x41,		/* 32-bit unsigned aka Counter32 */
  CUPS_ASN1_GAUGE = 0x42,		/* 32-bit unsigned aka Gauge32 */
  CUPS_ASN1_TIMETICKS = 0x43,		/* 32-bit unsigned aka Timeticks32 */
  CUPS_ASN1_GET_REQUEST = 0xa0,		/* GetRequest-PDU */
  CUPS_ASN1_GET_NEXT_REQUEST = 0xa1,	/* GetNextRequest-PDU */
  CUPS_ASN1_GET_RESPONSE = 0xa2		/* GetResponse-PDU */
};
typedef enum cups_asn1_e cups_asn1_t;	/**** ASN1 request/object types ****/

struct cups_snmp_hexstring_s		/**** Hex-STRING value ****/
{
  unsigned char	bytes[CUPS_SNMP_MAX_STRING];
					/* Bytes in string */
  int		num_bytes;		/* Number of bytes */
};

union cups_snmp_value_u			/**** Object value ****/
{
  int		boolean;		/* Boolean value */
  int		integer;		/* Integer value */
  unsigned	counter;		/* Counter value */
  unsigned	gauge;			/* Gauge value */
  unsigned	timeticks;		/* Timeticks  value */
  int		oid[CUPS_SNMP_MAX_OID];	/* OID value */
  char		string[CUPS_SNMP_MAX_STRING];
					/* String value */
  struct cups_snmp_hexstring_s hex_string;
					/* Hex string value */
};

typedef struct cups_snmp_s		/**** SNMP data packet ****/
{
  const char	*error;			/* Encode/decode error */
  http_addr_t	address;		/* Source address */
  int		version;		/* Version number */
  char		community[CUPS_SNMP_MAX_STRING];
					/* Community name */
  cups_asn1_t	request_type;		/* Request type */
  int		request_id;		/* request-id value */
  int		error_status;		/* error-status value */
  int		error_index;		/* error-index value */
  int		object_name[CUPS_SNMP_MAX_OID];
					/* object-name value */
  cups_asn1_t	object_type;		/* object-value type */
  union cups_snmp_value_u
		object_value;		/* object-value value */
} cups_snmp_t;

typedef void (*cups_snmp_cb_t)(cups_snmp_t *packet, void *data);

/*
 * Prototypes...
 */

#  ifdef __cplusplus
extern "C" {
#  endif /* __cplusplus */

extern void		cupsSNMPClose(int fd) _CUPS_API_1_4;
extern int		*cupsSNMPCopyOID(int *dst, const int *src, int dstsize)
			    _CUPS_API_1_4;
extern const char	*cupsSNMPDefaultCommunity(void) _CUPS_API_1_4;
extern int		cupsSNMPIsOID(cups_snmp_t *packet, const int *oid)
			    _CUPS_API_1_4;
extern int		cupsSNMPIsOIDPrefixed(cups_snmp_t *packet,
			                      const int *prefix) _CUPS_API_1_4;
extern int		cupsSNMPOpen(int family) _CUPS_API_1_4;
extern cups_snmp_t	*cupsSNMPRead(int fd, cups_snmp_t *packet,
			              double timeout) _CUPS_API_1_4;
extern void		cupsSNMPSetDebug(int level) _CUPS_API_1_4;
extern int		cupsSNMPWalk(int fd, http_addr_t *address, int version,
			             const char *community, const int *prefix,
				     double timeout, cups_snmp_cb_t cb,
				     void *data) _CUPS_API_1_4;
extern int		cupsSNMPWrite(int fd, http_addr_t *address, int version,
				      const char *community,
				      cups_asn1_t request_type,
				      const unsigned request_id,
				      const int *oid) _CUPS_API_1_4;

#  ifdef __cplusplus
}
#  endif /* __cplusplus */
#endif /* !_CUPS_SNMP_H_ */


/*
 * End of "$Id$".
 */
