/**
 * @version 0.00 06-NOV-2001
 * @author  Easy Software Products
 *
 *   Internet Printing Protocol definitions for the Common UNIX Printing
 *   System (CUPS).
 *
 *   Copyright 1997-2002 by Easy Software Products.
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
 *       44141 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 */

public class IPPValue
{
  int   integer_value;     // Integer value
  char  boolean_value;     // Boolean value
  char  date_value[];      // Date/time value

  //
  //  Resolution Type
  //
  int   xres;
  int   yres;
  short units;
  short resolution;

  //
  //  Range Type
  //
  int   lower;
  int   upper;
  int   range;

  //
  //  String Type
  //
  String charset;
  String text;
  String string;

  //
  //  Unknown Type
  //
  int    length;
  char   data[];
  

}
