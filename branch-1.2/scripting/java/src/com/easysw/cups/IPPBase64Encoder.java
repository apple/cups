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
 * The <code>IPPBase64Encoder</code> overrides part of the sun encoder.
 *
 * @author	TDB
 * @version	1.0
 * @since	JDK1.3
 */


class IPPBase64Encoder extends sun.misc.BASE64Encoder
{
  protected void encodeLineSuffix(java.io.OutputStream out)
            throws java.io.IOException
  {
    // do nothing. 
  }
}

