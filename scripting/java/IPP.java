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

import java.io.*;
import java.util.*;



//  ---------------------------------------------------------------------
//
//
public class IPP
{
  IPPRequest     request;        //  The request header
  List           attrs;          //  Attributes list.

  short          state;          //  Current IPP state (of request???)
  int            current;        //  Index into attributes array.
  int            last;           //  Index into attributes array.
  short          current_tag;


  //  ------------------------------------------------------------------
  //
  //  Constructor
  //
  public IPP()
  {
    state       = IPPDefs.IDLE;
    attrs       = new ArrayList();
    current     = -1;
    last        = -1;
    current_tag = IPPDefs.TAG_ZERO;
  }



  public boolean addAttribute( IPPAttribute a )
  {
    attrs.add(a);
    return(true);
  }

  public IPPAttribute getCurrentAttribute()
  {
      if (current >= 0)
        return( (IPPAttribute)attrs.get(current) );
      else
        return( null );
  }

  public void setCurrentAttribute( IPPAttribute p_attr )
  {
      // if (current >= 0)
      // return( (IPPAttribute)attrs.get(current) );
  }

  //  ------------------------------------------------------------------
  //
  //  Find the named attribute of the correct type.
  //
  public IPPAttribute ippFindAttribute( String p_name, int p_type )
  {
    if (p_name.length() < 1)
      return(null);
    current = -1;
    return(ippFindNextAttribute(p_name, p_type));
  }




  //  ------------------------------------------------------------------
  //
  //  Find the next named attribute of the correct type.
  //
  public IPPAttribute ippFindNextAttribute( String p_name, int p_type )
  {
    int value_tag;

    if (p_name.length() < 1)
      return(null);

    if ((current >= 0) && (current < (attrs.size() - 1)))
      current++;
    else
      current = 0;

    for (int i = current; i < attrs.size(); i++)
    {
      IPPAttribute tmp = (IPPAttribute)attrs.get(i);
      value_tag = (tmp.value_tag & IPPDefs.TAG_MASK);
      if ((tmp.name.length() > 0) && (tmp.name == p_name) &&
          (value_tag == p_type || p_type == IPPDefs.TAG_ZERO ||
           (value_tag == IPPDefs.TAG_TEXTLANG && p_type == IPPDefs.TAG_TEXT) ||
           (value_tag == IPPDefs.TAG_NAMELANG && p_type == IPPDefs.TAG_NAME)))
      {
        current = i;
        return(tmp);
      }
    }
    return(null);
  }


  //
  //  Get the size in bytes of an ipp request.
  //
  public int sizeInBytes()
  {
    IPPAttribute a;
    int          bytes = 8;
    int          last_group = IPPDefs.TAG_ZERO;

    for (int i=0; i < attrs.size(); i++)
    {
      a = (IPPAttribute)attrs.get(i);
      bytes += a.sizeInBytes(last_group);
      last_group = a.group_tag;
    }
    bytes++;   // one for the end tag.

    return(bytes);

  }  //  End of IPP.sizeInBytes()


  int ippBytes()
  {
    int i = 0;
    return(0);
  }

  public void setRequestID( short p_id )
  {
    request.request_id = p_id;
  }

  public void setRequestOperationID( short p_operation_id )
  {
    request.operation_id = p_operation_id;
  }





  //
  //  Dump a list
  //
  public void dump_response()
  {
    IPPAttribute a;
    int          last_group = IPPDefs.TAG_ZERO;

    for (int i=0; i < attrs.size(); i++)
    {
      a = (IPPAttribute)attrs.get(i);
      a.dump_values();
      last_group = a.group_tag;
    }
    return;

  }  //  End of IPP.dump_response()



}  // End of IPP class




//
//  End of IPP.java
//
