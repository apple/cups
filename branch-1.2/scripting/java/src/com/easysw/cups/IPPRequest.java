package com.easysw.cups;

/**
 * @version 1.00 06-NOV-2002
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

/**
 * An <code>IPPRequest</code> object is used to hold the 
 * status and id's of a request.
 *
 * @author	TDB
 * @version	1.0
 * @since	JDK1.3
 */
public class IPPRequest
{
  char[]  version;         //  Proto version number
  int   request_id;        //  Unique ID

  int   op_status; 
  short operation_id;
  short status_code;

  /**
   *  Constructor
   */
  public IPPRequest()
  {
    version  = new char[2];
  }

  /**
   *  Constructor using request id and operation id.
   *
   * @param	<code>p_request_id</code>	ID of request.
   * @param	<code>p_operation_id</code>	Operation ID for request.
   *
   * @see	<code>IPPDefs</code>
   */
  public IPPRequest( int p_request_id, short p_operation_id )
  {
    version  = new char[2];
    version[0]   = (char)1;
    version[1]   = (char)1;
    request_id   = p_request_id;
    operation_id = p_operation_id;
  }

  /**
   * Set the current status of a request.
   *
   * @param	<code>p_status_code</code>	Status code.
   * @see	<code>IPPDefs</code>
   */
  public void setStatus( short p_status_code )
  {
    status_code = p_status_code;
  }

  /**
   * Set the operation status of a request.
   *
   * @param	<code>p_status_code</code>	Operation status code.
   * @see	<code>IPPDefs</code>
   */
  public void setOpStatus( short p_status_code )
  {
    op_status = p_status_code;
  }
  
}  // End of IPPRequest class




