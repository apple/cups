/*
 * "$Id: ipp.c,v 1.2 1999/01/24 14:18:43 mike Exp $"
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

static ipp_attribute_t	*add_attr(ipp_t *ipp, int num_values);


void
_ippAddBoolean(ipp_t *ipp,
               uchar group,
	       char  *name,
	       char  value)
{
}


void
_ippAddBooleans(ipp_t *ipp,
                uchar group,
		char  *name,
		int   num_values,
		char  *values)
{
}


void
_ippAddDate(ipp_t *ipp,
            uchar group,
	    char  *name,
	    uchar *value)
{
}


void
_ippAddEnum(ipp_t *ipp,
            uchar group,
	    char  *name,
	    int   value)
{
}


void
_ippAddEnums(ipp_t *ipp,
             uchar group,
	     char  *name,
	     int   num_values,
	     int   *values)
{
}


void
_ippAddInteger(ipp_t *ipp,
               uchar group,
	       char  *name,
	       int   value)
{
}


void
_ippAddIntegers(ipp_t *ipp,
                uchar group,
	        char  *name,
		int   num_values,
		int   *values)
{
}


void
_ippAddLString(ipp_t *ipp,
               uchar group,
	       char  *name,
	       char  *charset,
	       uchar *value)
{
}


void
_ippAddLStrings(ipp_t *ipp,
                uchar group,
		char  *name,
		int   num_values,
		char  *charset,
		uchar **values)
{
}



void
_ippAddRange(ipp_t *ipp,
             uchar group,
	     char  *name,
	     int   lower,
	     int   upper)
{
}


void
_ippAddResolution(ipp_t *ipp,
                  uchar group,
		  char  *name,
		  int   units,
		  int   xres,
		  int   yres)
{
}


void
_ippAddString(ipp_t *ipp,
              uchar group,
	      char  *name,
	      uchar *value)
{
}


void
_ippAddStrings(ipp_t *ipp,
               uchar group,
	       char  *name,
	       int   num_values,
	       uchar **values)
{
}


/*
 * '_ippDateToTime()' - Convert from RFC 1903 Date/Time format to UNIX time
 *                      in seconds.
 */

time_t
_ippDateToTime(uchar *date)
{
  struct tm	unixdate;	/* UNIX date/time info */
  time_t	t;		/* Computed time */


  memset(&unixdate, 0, sizeof(unixdate));

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

  unixdate.tm_year = ((date[0] << 8) | date[1]) - 1900;
  unixdate.tm_mon  = date[2] - 1;
  unixdate.tm_mday = date[3];
  unixdate.tm_hour = date[4];
  unixdate.tm_min  = date[5];
  unixdate.tm_sec  = date[6];

  t = mktime(&unixdate);

  if (date[8] == '-')
    t += date[9] * 3600 + date[10] * 60;
  else
    t -= date[9] * 3600 + date[10] * 60;

  return (t);
}


void
_ippDelete(ipp_t *ipp)
{
}


ipp_t
*_ippNew(void)
{
}


/*
 * _ippLength()' - Compute the length of an IPP request.
 */

size_t
_ippLength(ipp_t *ipp)
{

}


/*
 * '_ippRead()' - Read data for an IPP request.
 */

int
_ippRead(http_t *http,
         ipp_t  *ipp)
{
  return (1);
}


/*
 * '_ipp_TimeToDate()' - Convert from UNIX time to RFC 1903 format.
 */

uchar *				/* O - RFC-1903 date/time data */
_ipp_TimeToDate(time_t t)	/* I - UNIX time value */
{
  struct tm	*unixdate;	/* UNIX unixdate/time info */
  static uchar	date[11];	/* RFC-1903 date/time data */


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

  unixdate = gmtime(&t);
  unixdate->tm_year += 1900;

  date[0]  = unixdate->tm_year >> 8;
  date[1]  = unixdate->tm_year;
  date[2]  = unixdate->tm_mon + 1;
  date[3]  = unixdate->tm_mday;
  date[4]  = unixdate->tm_hour;
  date[5]  = unixdate->tm_min;
  date[6]  = unixdate->tm_sec;
  date[7]  = 0;
  date[8]  = '+';
  date[9]  = 0;
  date[10] = 0;

  return (date);
}


/*
 * '_ippWrite()' - Write data for an IPP request.
 */

int
_ippWrite(http_t *http,
          ipp_t  *ipp)
{
  return (1);
}


static ipp_attribute_t *
add_attr(ipp_t *ipp,
         int   num_values)
{
}


/*
 * End of "$Id: ipp.c,v 1.2 1999/01/24 14:18:43 mike Exp $".
 */
