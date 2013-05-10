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
 * Implementation of the URLConnection interface.
 *
 * @author	TDB
 * @version	1.0
 * @since	JDK1.3
 */

import java.util.*;
import java.io.*;
import java.net.*;

public class IPPURLConnection extends URLConnection
{

  /**
   * Constructor.
   */
  public IPPURLConnection( URL url )
  {
    super(url);
  }

  /**
   * Determine if using proxy.
   *
   * @return	<code>boolean</code>	Always <code>false</code> for now.
   */
  public boolean usingProxy()
  {
    return(false);
  }

  /**
   * Not used.
   */
  public void connect()
  {
    return;
  }

  /**
   * Not used.
   */
  public void disconnect()
  {
    return;
  }

}  // end of class
