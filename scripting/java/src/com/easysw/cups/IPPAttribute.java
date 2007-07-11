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
 * An <code>IPPAttribute</code> object hold attributes for communicating
 * messages to / from the CUPS server.
 *
 * @author	TDB
 * @version	1.0
 * @since	JDK1.3
 */
import java.util.*;

public class IPPAttribute
{
  int      group_tag;
  int      value_tag;
  String   name;
  List     values;



  public IPPAttribute( int p_group_tag, int p_value_tag, 
                       String p_name )
  {
    group_tag = p_group_tag;
    value_tag = p_value_tag;
    name      = p_name;
    values    = new ArrayList();

  }  // End of IPPAttribute constructor.


  //
  //  Add a boolean value to the objects values list.
  //
  public boolean addBoolean( boolean p_bool )
  {
    IPPValue val = new IPPValue( p_bool );
    values.add(val);
    return(true);
  }


  //
  //  Add a set of boolean values to the objects values list.
  //
  public boolean addBooleans( boolean p_bools[] )
  {
    if (p_bools.length < 1)
      return(false);

    for (int i=0; i < p_bools.length; i++ )
    {
      IPPValue val = new IPPValue( p_bools[i] );
      values.add(val);
    }
    return(true);
  }


  //
  //  Add an enum value to the objects values list.
  //
  public boolean addEnum( int p_int )
  {
    IPPValue val = new IPPValue( p_int, true );
    values.add(val);
    return(true);
  }


  //
  //  Add an integer value to the objects values list.
  //
  public boolean addInteger( int p_int )
  {
    IPPValue val = new IPPValue( p_int );
    values.add(val);
    return(true);
  }


  //
  //  Add a set of integer values to the objects values list.
  //
  public boolean addIntegers( int p_ints[] )
  {
    if (p_ints.length < 1)
      return(false);

    for (int i=0; i < p_ints.length; i++ )
    {
      IPPValue val = new IPPValue( p_ints[i] );
      values.add(val);
    }
    return(true);
  }


  //
  //  Add a string value to the objects values list.
  //
  public boolean addString( String p_charset, String p_text )
  {
    String l_value;
    String final_value;

    //
    //  Force the value to English for POSIX locale.
    //
    if ((value_tag == IPPDefs.TAG_LANGUAGE) && (p_text == "C"))
    {
      l_value = "en";
    }
    else
    {
      l_value = p_text;
    }

    
    //
    //  Convert language values to lowercase and _ to - as needed.
    //
    if ((value_tag == IPPDefs.TAG_LANGUAGE) || 
        (value_tag == IPPDefs.TAG_CHARSET))
    {
      StringBuffer temp = new StringBuffer(l_value.length());
      char c;
      for (int i = 0; i < l_value.length(); i++)
      {
        c = l_value.charAt(i);
        if (c == '_')
          c = '-';
        else if (Character.isUpperCase(c))
          c = Character.toLowerCase(c);
        temp.append(c);
      }
      final_value = temp.toString();
    }
    else
    {
      final_value = l_value;
    }
    IPPValue val = new IPPValue( p_charset, final_value );
    values.add(val);
    return(true);
  }


  //
  //  Add a set of strings to the objects values list.
  //
  public boolean addStrings( String p_charset, String p_texts[] )
  {
    if (p_texts.length < 1)
      return(false);

    //
    //  Just call the singular string method to save on coding.
    //
    for (int i=0; i < p_texts.length; i++ )
    {
      addString( p_charset, p_texts[i] );
    }
    return(true);
  }

  //
  //  Add an ipp date value to the objects values list.
  //
  public boolean addDate( char p_date[] )
  {
    IPPValue val = new IPPValue( p_date );
    values.add(val);
    return(true);
  }

  //
  //  Add a range value to the objects values list.
  //
  public boolean addRange( int p_lower, int p_upper )
  {
    IPPValue val = new IPPValue( p_lower, p_upper );
    values.add(val);
    return(true);
  }


  //
  //  Add a set of range values to the objects values list.
  //
  public boolean addRanges( int p_lower[], int p_upper[] )
  {
    //
    // Avoid index out of bounds errors.
    //
    if (p_lower.length != p_upper.length)
      return(false);

    for (int i=0; i < p_lower.length; i++ )
      addRange( p_lower[i], p_upper[i] );
    return(true);
  }


  //
  //  Add a resolution value to the objects values list.
  //
  public boolean addResolution( byte p_units, int p_xres, int p_yres )
  {
    IPPValue val = new IPPValue( p_units, p_xres, p_yres );
    values.add(val);
    return(true);
  }


  //
  //  Add a set of resolution values to the objects values list.
  //
  public boolean addResolutions( byte p_units, int p_xres[], int p_yres[] )
  {
    if (p_xres.length != p_yres.length)
      return(false);

    for (int i=0; i < p_xres.length; i++)
      addResolution( p_units, p_xres[i], p_yres[i] );
    return(true);
  }


  //
  //  Set the attribute as a separator.
  //
  public boolean addSeparator()
  {
    value_tag = IPPDefs.TAG_ZERO;
    group_tag = IPPDefs.TAG_ZERO;
    return(true);
  }



  //
  //  Calculate the size in bytes for an IPP requests attributes.
  //
  public int sizeInBytes(int last_group)
  {
    IPPValue val;
    int      bytes = 0;          // Start with one for the group tag.
   
    //
    //  Add 1 if first time, or group tag changes.
    //
    if (last_group != group_tag)
      bytes ++;

    bytes ++;                    // Add 1 for the value tag.
    bytes += 2;                  // Add 2 for the name length field.
    bytes += name.length();      // Add the length of the name.
 
    for (int i=0; i < values.size(); i++ )
    {
      val = (IPPValue)values.get(i);

      if (i > 0)
      {
        //  If an array of values, add 1 for another value tag, plus
        //  2 for zero length name.
        //
        bytes += 3;
      }

      switch (value_tag)
      {
        case IPPDefs.TAG_INTEGER :
        case IPPDefs.TAG_ENUM :
            bytes += 2;
            bytes += 4;
	    break;

        case IPPDefs.TAG_BOOLEAN :
            bytes += 2;
            bytes ++;
	    break;

        case IPPDefs.TAG_TEXT:
        case IPPDefs.TAG_NAME:
        case IPPDefs.TAG_KEYWORD:
        case IPPDefs.TAG_STRING:
        case IPPDefs.TAG_URI:
        case IPPDefs.TAG_URISCHEME:
        case IPPDefs.TAG_CHARSET:
        case IPPDefs.TAG_LANGUAGE:
        case IPPDefs.TAG_MIMETYPE:
            bytes += 2;
	    bytes += val.text.length();
	    break;

        case IPPDefs.TAG_DATE :
            bytes += 2;
            bytes += 11;
	    break;

        case IPPDefs.TAG_RESOLUTION :
            bytes += 2;
            bytes += 9;
	    break;

        case IPPDefs.TAG_RANGE :
            bytes += 2;
            bytes += 8;
  	  break;

        case IPPDefs.TAG_TEXTLANG :
        case IPPDefs.TAG_NAMELANG :
            bytes += 6;  // 2 overall len, 2 charset len, 2 text len
	    bytes += val.charset.length() +
	             val.text.length();
	    break;

        default :
            bytes += 2;
            if (val.data != null)
              bytes += val.data.length;
  	    break;
      }
    }

    // bytes++;   // 1 byte for end of values tag.

    return(bytes);

  }  // End of IPPAttribute.sizeInBytes()


  //
  //  Get the characters of an attribute
  //
  public byte[] getBytes( int sz, int last_group  )
  {
    IPPValue val;

    int    i,j, n;
    int    bi    = 0;          // Start with one for the group tag.
    byte[] bytes = new byte[sz];

    if (group_tag != last_group)
    {
      bytes[bi++] = (byte)group_tag;
      last_group  = group_tag;
    }
    bytes[bi++] = (byte)value_tag;

    bytes[bi++] = (byte)((name.length() & 0xff00) >> 8);
    bytes[bi++] = (byte)(name.length() & 0xff);
    for (j=0; j < name.length(); j++)
      bytes[bi++] = (byte)name.charAt(j);
   
    for (i=0; i < values.size(); i++ )
    {
      if (i > 0)
      {
        bytes[bi++] = (byte)value_tag;
        bytes[bi++] = (byte)0;
        bytes[bi++] = (byte)0;
      }
   
      val = (IPPValue)values.get(i);
      switch (value_tag)
      {
        case IPPDefs.TAG_INTEGER :
        case IPPDefs.TAG_ENUM :
            bytes[bi++] = (byte)0;
            bytes[bi++] = (byte)4;
            bytes[bi++] = (byte)((val.integer_value & 0xff000000) >> 24);
            bytes[bi++] = (byte)((val.integer_value & 0xff0000) >> 16);
            bytes[bi++] = (byte)((val.integer_value & 0xff00) >> 8);
            bytes[bi++] = (byte)(val.integer_value & 0xff);
	    break;

        case IPPDefs.TAG_BOOLEAN :
            bytes[bi++] = (byte)0;
            bytes[bi++] = (byte)1;
            if (val.boolean_value)
              bytes[bi++] = (byte)1;
            else
              bytes[bi++] = (byte)0;
	    break;

        case IPPDefs.TAG_TEXT :
        case IPPDefs.TAG_NAME :
        case IPPDefs.TAG_KEYWORD :
        case IPPDefs.TAG_STRING :
        case IPPDefs.TAG_URI :
        case IPPDefs.TAG_URISCHEME :
        case IPPDefs.TAG_CHARSET :
        case IPPDefs.TAG_LANGUAGE :
        case IPPDefs.TAG_MIMETYPE :
            bytes[bi++] = (byte)((val.text.length() & 0xff00) >> 8);
            bytes[bi++] = (byte)(val.text.length() & 0xff);
            for (j=0; j < val.text.length(); j++)
            {
              bytes[bi++] = (byte)val.text.charAt(j);
            }
	    break;

        case IPPDefs.TAG_DATE:
            bytes[bi++] = (byte)0;
            bytes[bi++] = (byte)11;
            for (j=0; j < 11; j++)
              bytes[bi++] = (byte)val.date_value[j];
	    break;

        case IPPDefs.TAG_RESOLUTION :
            bytes[bi++] = (byte)0;
            bytes[bi++] = (byte)9;
            bytes[bi++] = (byte)((val.xres & 0xff000000) >> 24);
            bytes[bi++] = (byte)((val.xres & 0xff0000) >> 16);
            bytes[bi++] = (byte)((val.xres & 0xff00) >> 8);
            bytes[bi++] = (byte)(val.xres & 0xff);
            bytes[bi++] = (byte)((val.yres & 0xff000000) >> 24);
            bytes[bi++] = (byte)((val.yres & 0xff0000) >> 16);
            bytes[bi++] = (byte)((val.yres & 0xff00) >> 8);
            bytes[bi++] = (byte)(val.yres & 0xff);
            bytes[bi++] = (byte)val.units;
	    break;

        case IPPDefs.TAG_RANGE :
            bytes[bi++] = (byte)0;
            bytes[bi++] = (byte)8;
            bytes[bi++] = (byte)((val.lower & 0xff000000) >> 24);
            bytes[bi++] = (byte)((val.lower & 0xff0000) >> 16);
            bytes[bi++] = (byte)((val.lower & 0xff00) >> 8);
            bytes[bi++] = (byte)(val.lower & 0xff);
            bytes[bi++] = (byte)((val.upper & 0xff000000) >> 24);
            bytes[bi++] = (byte)((val.upper & 0xff0000) >> 16);
            bytes[bi++] = (byte)((val.upper & 0xff00) >> 8);
            bytes[bi++] = (byte)(val.upper & 0xff);
  	  break;

        case IPPDefs.TAG_TEXTLANG :
        case IPPDefs.TAG_NAMELANG :
            n = val.charset.length() +
                val.text.length() + 4;
            bytes[bi++] = (byte)((n & 0xff00) >> 8);
            bytes[bi++] = (byte)(n & 0xff);

            n = val.charset.length();
            bytes[bi++] = (byte)((n & 0xff00) >> 8);
            bytes[bi++] = (byte)(n & 0xff);
            for (j=0; j < val.charset.length(); j++)
              bytes[bi++] = (byte)val.charset.charAt(j);
            
            n = val.text.length();
            bytes[bi++] = (byte)((n & 0xff00) >> 8);
            bytes[bi++] = (byte)(n & 0xff);
            for (j=0; j < (byte)val.text.length(); j++)
              bytes[bi++] = (byte)val.text.charAt(j);
            
	    break;

        default :
            if (val.data != null)
            {
              n = val.data.length;
              bytes[bi++] = (byte)((n & 0xff00) >> 8);
              bytes[bi++] = (byte)(n & 0xff);
              for (j=0; j < val.data.length; j++)
                bytes[bi++] = (byte)val.data[j];
            }
  	    break;
      }
    }

    return(bytes);

  }  // End of IPPAttribute.getBytes()




  //
  // 
  //
  public void dump_values()
  {
    IPPValue val;
   
    if ((values == null) || (values.size() < 1))
    {
      System.out.println( " ---- Separator ---- \n");
      return;
    }

    for (int i=0; i < values.size(); i++ )
    {
      val = (IPPValue)values.get(i);

      System.out.println("ATTR GTAG: " + group_tag );
      System.out.println("ATTR VTAG: " + value_tag );
      System.out.println("ATTR NAME: " + name );

      // switch (value_tag & ~IPPDefs.TAG_COPY)
      switch (value_tag)
      {
        case IPPDefs.TAG_INTEGER :
        case IPPDefs.TAG_ENUM :
            System.out.println( " INTEGER: " + val.integer_value );
	    break;

        case IPPDefs.TAG_BOOLEAN :
            System.out.println( " BOOLEAN: " + val.boolean_value );
	    break;

        case IPPDefs.TAG_TEXT :
        case IPPDefs.TAG_NAME :
        case IPPDefs.TAG_KEYWORD :
        case IPPDefs.TAG_STRING :
        case IPPDefs.TAG_URI :
        case IPPDefs.TAG_URISCHEME :
        case IPPDefs.TAG_CHARSET :
        case IPPDefs.TAG_LANGUAGE :
        case IPPDefs.TAG_MIMETYPE :
            System.out.println( " CHARSET: " + val.charset + 
                                " TEXT: " + val.text );
	    break;

        case IPPDefs.TAG_DATE :
            System.out.println( " DATE: " + val.unix_time );
	    break;

        case IPPDefs.TAG_RESOLUTION :
            System.out.println( " UNITS: " + val.units +
                                " XRES: " + val.xres +
                                " YRES: " + val.yres  );
	    break;

        case IPPDefs.TAG_RANGE :
            System.out.println( " LOWER: " + val.lower +
                                " UPPER: " + val.upper );
  	  break;

        case IPPDefs.TAG_TEXTLANG :
        case IPPDefs.TAG_NAMELANG :
            System.out.println( " CHARSET: " + val.charset + 
                                " TEXT: " + val.text );
	    break;

        case IPPDefs.TAG_ZERO:
            System.out.println( " ---- Separator ---- \n");
  	    break;

        default :
  	    break;
      }
    }
    return;

  }  




}  // End of IPPAttribute class




