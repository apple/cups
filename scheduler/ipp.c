/*
 * "$Id: ipp.c,v 1.2 1999/01/24 14:25:11 mike Exp $"
 *
 *   Internet Printing Protocol support functions for the Common UNIX
 *   Printing System (CUPS).
 *
 *   Copyright 1997-1999 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Easy Software Products and are protected by Federal
 *   copyright law.  Distribution and use rights are outlined in the file
 *   "LICENSE.txt" which should have been included with this file.  If this
 *   file is missing or damaged please contact Easy Software Products
 *   at:
 *
 *       Attn: CUPS Licensing Information
 *       Easy Software Products
 *       44145 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 *   ReadIPP()  - Read data for an IPP request.
 *   WriteIPP() - Write data for an IPP request.
 *   dt2unix()  - Convert from RFC 1903 Date/Time format to UNIX time in
 *                seconds.
 *   unix2dt()  - Convert from UNIX time to RFC 1903 format.
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"


/*
 * Local functions...
 */

static time_t	dt2unix(uchar *dt);
static uchar	*unix2dt(time_t t, uchar *dt);


/*
 * 'ReadIPP()' - Read data for an IPP request.
 */

int				/* O - 1 = success, 0 = failure */
ReadIPP(connection_t  *con,	/* I - Connection to read from */
        ipp_request_t *req)	/* I - Request to read from */
{
  return (1);
}


/*
 * 'WriteIPP()' - Write data for an IPP request.
 */

int				/* O - 1 = success, 0 = failure */
WriteIPP(connection_t  *con,	/* I - Connection to read from */
         ipp_request_t *req)	/* I - Request to read from */
{
  return (1);
}


/*
 * 'dt2unix()' - Convert from RFC 1903 Date/Time format to UNIX time in
 *               seconds.
 */

static time_t			/* O - UNIX time value */
dt2unix(uchar *dt)		/* I - Time record */
{
  struct tm	date;		/* UNIX date/time info */
  time_t	t;		/* Computed time */


  memset(&date, 0, sizeof(date));

 /*
  * RFC-1903 date/time format is:
  *
  *    Byte(s)  Description
  *    -------  -----------
  *    0-1      Year (0 to 65535)
  *    2        Month (1 to 12)
  *    3        Day (1 to 31)
  *    4        Hours (0 to 23)
  *    5        Minutes (0 to 59)
  *    6        Seconds (0 to 60, 60 = "leap second")
  *    7        Deciseconds (0 to 9)
  *    8        +/- UTC
  *    9        UTC hours (0 to 11)
  *    10       UTC minutes (0 to 59)
  */

  date.tm_year = ((dt[0] << 8) | dt[1]) - 1900;
  date.tm_mon  = dt[2] - 1;
  date.tm_mday = dt[3];
  date.tm_hour = dt[4];
  date.tm_min  = dt[5];
  date.tm_sec  = dt[6];

  t = mktime(&date);

  if (dt[8] == '-')
    t += dt[9] * 60 + dt[10];
  else
    t -= dt[9] * 60 + dt[10];

  return (t);
}


/*
 * 'unix2dt()' - Convert from UNIX time to RFC 1903 format.
 */

static uchar *			/* O - New time record */
unix2dt(time_t t,		/* I - UNIX time value */
        uchar  *dt)		/* I - Time record */
{
  struct tm	*date;		/* UNIX date/time info */


 /*
  * RFC-1903 date/time format is:
  *
  *    Byte(s)  Description
  *    -------  -----------
  *    0-1      Year (0 to 65535)
  *    2        Month (1 to 12)
  *    3        Day (1 to 31)
  *    4        Hours (0 to 23)
  *    5        Minutes (0 to 59)
  *    6        Seconds (0 to 60, 60 = "leap second")
  *    7        Deciseconds (0 to 9)
  *    8        +/- UTC
  *    9        UTC hours (0 to 11)
  *    10       UTC minutes (0 to 59)
  */

  date = gmtime(&t);
  date->tm_year += 1900;

  dt[0]  = date->tm_year >> 8;
  dt[1]  = date->tm_year;
  dt[2]  = date->tm_mon + 1;
  dt[3]  = date->tm_mday;
  dt[4]  = date->tm_hour;
  dt[5]  = date->tm_min;
  dt[6]  = date->tm_sec;
  dt[7]  = 0;
  dt[8]  = '+';
  dt[9]  = 0;
  dt[10] = 0;

  return (dt);
}


/*
 * End of "$Id: ipp.c,v 1.2 1999/01/24 14:25:11 mike Exp $".
 */
