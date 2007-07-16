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
 * Digest MD5 password routines.
 *
 * @author	TDB
 * @version	1.0
 * @since	JDK1.3
 */
import java.security.*; 

public class IPPMD5 
{ 
  public MessageDigest        md       = null; 
  static private IPPMD5       md5      = null; 
  private static final char[] hexChars = {'0','1','2','3','4','5','6','7',
                                          '8','9','a','b','c','d','e','f'}; 

  /** 
   * Constructor is private so you must use the getInstance method 
   */ 
  private IPPMD5() throws NoSuchAlgorithmException 
  { 
    md = MessageDigest.getInstance("MD5"); 
  } 


  /** 
   * This returns the singleton instance 
   */ 
  public static IPPMD5 getInstance() throws NoSuchAlgorithmException 
  {       

    if (md5 == null) 
    { 
      md5 = new IPPMD5(); 
    } 
    return (md5); 
  } 


  public String hashData(byte[] dataToHash) 
  { 
    return(hexStringFromBytes((calculateHash(dataToHash)))); 
  } 


  private byte[] calculateHash(byte[] dataToHash) 
  { 
    md.update(dataToHash, 0, dataToHash.length); 
    return(md.digest()); 
  } 


  public String hexStringFromBytes(byte[] b) 
  { 
    String hex = ""; 
    int msb; 
    int lsb = 0; 
    int i; 

    // MSB maps to idx 0 
    for (i = 0; i < b.length; i++) 
    { 
      msb = ((int)b[i] & 0x000000FF) / 16; 
      lsb = ((int)b[i] & 0x000000FF) % 16; 
      // System.out.println("I: " + i + "  B: " + b[i] + " MSB: " + 
      //                    msb + " LSB: " + lsb );
      hex = hex + hexChars[msb] + hexChars[lsb]; 
    } 
    return(hex); 
  } 





  public String MD5Digest( String user, String passwd, String realm,
                           String method, String resource, String nonce )
  {
    String tmp;
    String A1, A2;
    String auth_string = "";
    
    try
    {
      tmp = user + ":" + realm + ":" + passwd;
      md = MessageDigest.getInstance("MD5"); 
      A1 = hexStringFromBytes(md.digest(tmp.getBytes()));
    
      tmp = method + ":" + resource;
      md = MessageDigest.getInstance("MD5"); 
      A2 = hexStringFromBytes(md.digest(tmp.getBytes()));

      tmp = A1 + ":" + nonce + ":" + A2;
      md = MessageDigest.getInstance("MD5"); 
      auth_string = hexStringFromBytes(md.digest(tmp.getBytes()));
      return(auth_string); 
    }
    catch (NoSuchAlgorithmException e)
    {
    }
    return("");

  }


} 
