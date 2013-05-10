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
 * Class to convert a status code to text.
 *
 * @author	TDB
 * @version	1.0
 * @since	JDK1.3
 */

public class IPPStatus
{
  int      status;
  String   status_text;

  /**
   * Constructor,  access the <code>status_text</code> member
   * after creation.
   *
   * @param	<code>p_status</code>	Status code to convert.
   * @see	<code>IPPDefs</code>
   */
  public IPPStatus( int p_status )
  {
    status = p_status;
    switch( status )
    {
      case IPPDefs.OK:
         status_text = "OK";
         break;
      case IPPDefs.OK_SUBST:
         status_text = "OK, substituted";
         break;
      case IPPDefs.OK_CONFLICT:
         status_text = "OK, conflict";
         break;
      case IPPDefs.OK_IGNORED_SUBSCRIPTIONS:
         status_text = "OK, ignored subscriptions";
         break;
      case IPPDefs.OK_IGNORED_NOTIFICATIONS:
         status_text = "OK, ignored notifications";
         break;
      case IPPDefs.OK_TOO_MANY_EVENTS:
         status_text = "OK, too many events";
         break;
      case IPPDefs.OK_BUT_CANCEL_SUBSCRIPTION:
         status_text = "OK, but cancel subscription";
         break;
      case IPPDefs.REDIRECTION_OTHER_SITE:
         status_text = "Redirected to other site";
         break;
      case IPPDefs.BAD_REQUEST:
         status_text = "Bad request";
         break;
      case IPPDefs.FORBIDDEN:
         status_text = "Forbidden";
         break;
      case IPPDefs.NOT_AUTHENTICATED:
         status_text = "Not authenticated";
         break;
      case IPPDefs.NOT_AUTHORIZED:
         status_text = "Not authorized";
         break;
      case IPPDefs.NOT_POSSIBLE:
         status_text = "Not possible";
         break;
      case IPPDefs.TIMEOUT:
         status_text = "Timeout";
         break;
      case IPPDefs.NOT_FOUND:
         status_text = "Not found";
         break;
      case IPPDefs.GONE:
         status_text = "Gone";
         break;
      case IPPDefs.REQUEST_ENTITY:
         status_text = "Request entity";
         break;
      case IPPDefs.REQUEST_VALUE:
         status_text = "Request value";
         break;
      case IPPDefs.DOCUMENT_FORMAT:
         status_text = "Document format";
         break;
      case IPPDefs.ATTRIBUTES:
         status_text = "Attributes";
         break;
      case IPPDefs.URI_SCHEME:
         status_text = "URI scheme";
         break;
      case IPPDefs.CHARSET:
         status_text = "Charset";
         break;
      case IPPDefs.CONFLICT:
         status_text = "Conflict";
         break;
      case IPPDefs.COMPRESSION_NOT_SUPPORTED:
         status_text = "Compression not supported";
         break;
      case IPPDefs.COMPRESSION_ERROR:
         status_text = "Compression error";
         break;
      case IPPDefs.DOCUMENT_FORMAT_ERROR:
         status_text = "Document format error";
         break;
      case IPPDefs.DOCUMENT_ACCESS_ERROR:
         status_text = "Document access error";
         break;
      case IPPDefs.ATTRIBUTES_NOT_SETTABLE:
         status_text = "Attributes not settable";
         break; 
      case IPPDefs.IGNORED_ALL_SUBSCRIPTIONS:
         status_text = "Ignored all subscriptions";
         break;
      case IPPDefs.TOO_MANY_SUBSCRIPTIONS:
         status_text = "Too many subscriptions";
         break;
      case IPPDefs.IGNORED_ALL_NOTIFICATIONS:
         status_text = "Ingored all notifications";
         break;
      case IPPDefs.PRINT_SUPPORT_FILE_NOT_FOUND:
         status_text = "Support file not found";
         break;
      case IPPDefs.INTERNAL_ERROR:
         status_text = "Internal error";
         break;
      case IPPDefs.OPERATION_NOT_SUPPORTED:
         status_text = "Operation not supported";
         break;
      case IPPDefs.SERVICE_UNAVAILABLE:
         status_text = "Service unavailable";
         break;
      case IPPDefs.VERSION_NOT_SUPPORTED:
         status_text = "Version not supported";
         break;
      case IPPDefs.DEVICE_ERROR:
         status_text = "Device error";
         break;
      case IPPDefs.TEMPORARY_ERROR:
         status_text = "Temporary error";
         break;
      case IPPDefs.NOT_ACCEPTING:
         status_text = "Not accepting";
         break;
      case IPPDefs.PRINTER_BUSY:
         status_text = "Printer busy";
         break;
      case IPPDefs.ERROR_JOB_CANCELLED:
         status_text = "Error, job cancelled";
         break;
      case IPPDefs.MULTIPLE_JOBS_NOT_SUPPORTED:
         status_text = "Multiple jobs not supported";
         break;
      case IPPDefs.PRINTER_IS_DEACTIVATED:
         status_text = "Printer is de-activated";
         break;
      default:
         status_text = "Unknown error";
    }
  }





}  // End of IPPStatus class



