package com.easysw.cups;

/**
 * @version 1.00 06-NOV-2002
 * @author  Apple Inc.
 *
 *   Internet Printing Protocol definitions for the Common UNIX Printing
 *   System (CUPS).
 *
 *   Copyright 2007 by Apple Inc.
 *   Copyright 1997-2002 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 */

/**
 * An <code>IPPValue</code> object is used to hold the 
 * different kinds of values in a generic object.
 *
 * @author	TDB
 * @version	1.0
 * @since	JDK1.3
 */
import java.util.*;

public class IPPValue
{

  int     value_type;        // Type of value for this object.

  int     integer_value;     // Integer value
  boolean boolean_value;     // Boolean value


  char    date_value[];      // Date/time value
  long    unix_time;         // Unix time ....

  //
  //  Resolution Type
  //
  int   xres;
  int   yres;
  byte  units;

  //
  //  Range Type
  //
  int   lower;
  int   upper;

  //
  //  String Type
  //
  String charset;
  String text;

  //
  //  Unknown Type
  //
  int   length;
  char  data[];


  /**
   *  Byte constructor.
   *
   * @param	<code>p_byte</code>	Byte value.
   */
  public IPPValue( byte p_byte )
  {
    value_type    = IPPDefs.TAG_INTEGER;
    integer_value = (int)p_byte; 
  }

  /**
   *  Short constructor.
   *
   * @param	<code>p_short</code>	Short value.
   */
  public IPPValue( short p_short )
  {
    value_type    = IPPDefs.TAG_INTEGER;
    integer_value = (int)p_short; 
  }

  /**
   *  Integer constructor.
   *
   * @param	<code>p_int</code>	Integer value.
   */
  public IPPValue( int p_int )
  {
    value_type    = IPPDefs.TAG_INTEGER;
    integer_value = p_int; 
  }

  /**
   *  Enum constructor.
   *
   * @param	<code>p_int</code>	Integer value - force to IPP enum.
   */
  public IPPValue( int p_int, boolean anything )
  {
    value_type    = IPPDefs.TAG_ENUM;
    integer_value = p_int; 
  }

  /**
   *  Boolean constructor.
   *
   * @param	<code>p_boolean</code>	Boolean value.
   */
  public IPPValue( boolean p_boolean )
  {
    value_type    = IPPDefs.TAG_BOOLEAN;
    boolean_value = p_boolean; 
  }


  /**
   *  Date constructor.  Also set the <code>unix_time</code> member.
   *
   * @param	<code>p_date[]</code>	Character array with date value.
   */
  public IPPValue( char p_date[] )
  {
    value_type = IPPDefs.TAG_DATE;
    date_value = p_date; 
    unix_time  = IPPDateToTime();
  }



  /**
   *  String constructor.  Set the <code>string</code> and 
   *  <code>charset</code> values.
   *
   * @param	<code>p_charset</code>		Charset for string.
   * @param	<code>p_text</code>		Text for string.
   */
  public IPPValue( String p_charset, String p_text )
  {
    value_type = IPPDefs.TAG_STRING;
    charset    = p_charset; 
    text       = p_text; 
  }


  /**
   *  Range constructor.  Automatically swap as needed.
   *
   * @param	<code>p_lower</code>		Integer lower value.
   * @param	<code>p_upper</code>		Integer upper value.
   */
  public IPPValue( int p_lower, int p_upper )
  {
    value_type = IPPDefs.TAG_RANGE;
    if (p_lower < p_upper)
    {
      lower    = p_lower; 
      upper    = p_upper; 
    }
    else
    {
      lower    = p_upper; 
      upper    = p_lower; 
    }
  }


  /**
   *  Resolution constructor. 
   *
   * @param	<code>p_units</code>		Unit of measure.
   * @param	<code>p_xres</code>		X resolution.
   * @param	<code>p_yres</code>		Y resolution.
   */
  public IPPValue( byte p_units, int p_xres, int p_yres )
  {
    value_type    = IPPDefs.TAG_RESOLUTION;
    units         = p_units; 
    xres          = p_xres; 
    yres          = p_yres; 
  }


  /**
   *  Raw data constructor. 
   *
   * @param	<code>p_length</code>		Size of array.
   * @param	<code>p_data[]</code>		Data.
   */
  public IPPValue( int p_length, char p_data[] )
  {
    value_type = IPPDefs.TAG_UNKNOWN;
    length     = p_length; 
    data       = p_data; 
  }



  /**
   *  Convert an IPP Date value to Unix Time.
   *
   * @return	<code>long</code>	Unix time in seconds.
   * @see	<code>IPPCalender</code>
   */
  public long IPPDateToTime()
  {

    //
    //  Compute the offset from GMT in milliseconds.
    //
    int raw_offset = (((int)date_value[9] * 3600) + ((int)date_value[10] * 60)) * 1000;
    if (date_value[8] == '-')
      raw_offset = 0 - raw_offset;

    //
    //  Get the timezone for that offset.
    //
    TimeZone tz = new SimpleTimeZone(raw_offset,"GMT");

    //
    //  Create a subclassed gregorian calendar (sub classed so we have
    //  access to the getTimeInMillis() method).
    //
    IPPCalendar cl = new IPPCalendar();
 
    int year   = ((((int)date_value[0]) << 8) | (((int)date_value[1]) - 1900));
    int month  = ((int)date_value[2]) - 1;
    int day    = (int)date_value[3];
    int hour   = (int)date_value[4];
    int minute = (int)date_value[5];
    int second = (int)date_value[6];

    //
    //  Now set the calendar to the matching time.
    //
    cl.setTimeZone( tz );
    cl.set( year, month, day, hour, minute, second );

    //
    //  And finally get the unix time.
    //
    long the_time = cl.getTimeInMillis();
    the_time /= 1000;

    return(the_time);
  }

}  // End of class
