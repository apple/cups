/*
 * MD5 password support for CUPS (deprecated).
 *
 * Copyright 2007-2017 by Apple Inc.
 * Copyright 1997-2005 by Easy Software Products.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * missing or damaged, see the license at "http://www.cups.org/".
 *
 * This file is subject to the Apple OS-Developed Software exception.
 */

/*
 * Include necessary headers...
 */

#include <cups/cups.h>
#include "http-private.h"
#include "string-private.h"


/*
 * 'httpMD5()' - Compute the MD5 sum of the username:group:password.
 *
 * @deprecated@
 */

char *					/* O - MD5 sum */
httpMD5(const char *username,		/* I - User name */
        const char *realm,		/* I - Realm name */
        const char *passwd,		/* I - Password string */
	char       md5[33])		/* O - MD5 string */
{
  unsigned char		sum[16];	/* Sum data */
  char			line[256];	/* Line to sum */


 /*
  * Compute the MD5 sum of the user name, group name, and password.
  */

  snprintf(line, sizeof(line), "%s:%s:%s", username, realm, passwd);
  cupsHashData("md5", (unsigned char *)line, strlen(line), sum, sizeof(sum));

 /*
  * Return the sum...
  */

  return ((char *)cupsHashString(sum, sizeof(sum), md5, 33));
}


/*
 * 'httpMD5Final()' - Combine the MD5 sum of the username, group, and password
 *                    with the server-supplied nonce value, method, and
 *                    request-uri.
 *
 * @deprecated@
 */

char *					/* O - New sum */
httpMD5Final(const char *nonce,		/* I - Server nonce value */
             const char *method,	/* I - METHOD (GET, POST, etc.) */
	     const char *resource,	/* I - Resource path */
             char       md5[33])	/* IO - MD5 sum */
{
  unsigned char		sum[16];	/* Sum data */
  char			line[1024];	/* Line of data */
  char			a2[33];		/* Hash of method and resource */


 /*
  * First compute the MD5 sum of the method and resource...
  */

  snprintf(line, sizeof(line), "%s:%s", method, resource);
  cupsHashData("md5", (unsigned char *)line, strlen(line), sum, sizeof(sum));
  cupsHashString(sum, sizeof(sum), a2, sizeof(a2));

 /*
  * Then combine A1 (MD5 of username, realm, and password) with the nonce
  * and A2 (method + resource) values to get the final MD5 sum for the
  * request...
  */

  snprintf(line, sizeof(line), "%s:%s:%s", md5, nonce, a2);
  cupsHashData("md5", (unsigned char *)line, strlen(line), sum, sizeof(sum));

  return ((char *)cupsHashString(sum, sizeof(sum), md5, 33));
}


/*
 * 'httpMD5String()' - Convert an MD5 sum to a character string.
 *
 * @deprecated@
 */

char *					/* O - MD5 sum in hex */
httpMD5String(const unsigned char *sum,	/* I - MD5 sum data */
              char                md5[33])
					/* O - MD5 sum in hex */
{
  return ((char *)cupsHashString(sum, 16, md5, 33));
}
