package com.easysw.cups;

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

import java.util.*;

/**
 * An <code>IPP</code> object is used to hold the various
 * attributes and status of an ipp request..
 *
 * @author	TDB
 * @version	1.0
 * @since	JDK1.3
 */
public class IPP
{
  public IPPRequest     request;        //  The request header
  public IPPStatus      status;         //  Status of request
  public List    	attrs;          //  Attributes list.

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
    attrs       = new LinkedList();
    current     = -1;
    last        = -1;
    current_tag = IPPDefs.TAG_ZERO;
  }



  /**
   * Add an attribute to the attibutes list
   * for later parsing.
   *
   *  @param	a			<code>IPPAttribute</code> to add.
   *  @return	<code>true</code>	always returns <code>true</code>
   *                                    for now.
   *
   * @see IPPAttribute
   * @see IPPValue
   */
  public boolean addAttribute( IPPAttribute a )
  {
    attrs.add(a);
    return(true);
  }

  /**
   * Get the current attribute pointed at by
   * <code>current</code>.
   *
   * @return	<code>IPPAttribute</code>	Return the current attribute.
   * @return	<code>null</code>		Return <code>null</code>
   *                                            if <code>current</code> is 0.
   */
  public IPPAttribute getCurrentAttribute()
  {
      if (current >= 0)
        return( (IPPAttribute)attrs.get(current) );
      else
        return( null );
  }

  // public void setCurrentAttribute( IPPAttribute p_attr )
  // {
  //   if (current >= 0)
  //     return( (IPPAttribute)attrs.get(current) );
  // }


  /**
   *
   *  Find the named attribute of the correct type.  Both must match.
   *  This methos searches from the beginning of the attribute list,
   *  rather than from the current position.
   *
   * @param	p_name		<code>String</code> containing the name.
   * @param     p_type		<code>int</code> attribute type.
   *
   * @return	<code>IPPAttribute</code>	Matching attribute if found.
   * @return	<code>null</code>		<code>null</code> if not found.
   */
  public IPPAttribute ippFindAttribute( String p_name, int p_type )
  {
    if (p_name.length() < 1)
      return(null);
    current = -1;
    return(ippFindNextAttribute(p_name, p_type));
  }




  /**
   *
   *  Find the named attribute of the correct type.  Both must match.
   *  This methos searches from the current position in the attribute list.
   *
   * @param	p_name		<code>String</code> containing the name.
   * @param     p_type		<code>int</code> attribute type.
   *
   * @return	<code>IPPAttribute</code>	Matching attribute if found.
   * @return	<code>null</code>		<code>null</code> if not found.
   */
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


  /**
   *  Get the size in bytes of an <code>IPP</code> request.
   *
   * @return	<code>int</code>	Number of bytes for the request.
   */
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



  /**
   *  Set the <code>IPP</code> request ID.
   *
   *  @param	p_id	<code>short</code> request id.
   */
  public void setRequestID( short p_id )
  {
    request.request_id = p_id;
  }

  /**
   *  Set the <code>IPP</code> operation ID.
   *
   *  @param	p_operation_id	<code>short</code> operation id.
   */
  public void setRequestOperationID( short p_operation_id )
  {
    request.operation_id = p_operation_id;
  }


  //
  //  Dump a list - for debugging.
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
