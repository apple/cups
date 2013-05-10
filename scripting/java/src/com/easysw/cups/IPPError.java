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
 * An <code>IPPError</code> object is used for error conversion.
 *
 * @author	TDB
 * @version	1.0
 * @since	JDK1.3
 */

import java.util.*;
import java.io.*;


public class IPPError
{
  private int       error_number;
  private String    error_string;


  /**
   * Constructor that sets <code>error_string</code> after creation.
   *
   * @param	<code>error_number</code>	Error number to convert.
   * @see	<code>IPPDefs</code>
   */
  public IPPError(int p_error)
  {
    error_number = p_error;
    error_string = ippErrorString( error_number );
  }


  /**
   *  Get the string associated with an error number.
   *
   * @param	<code>error</code>	Error number to convert.
   * @see	<code>IPPDefs</code>
   */
  private String ippErrorString( int error )
  {
    String unknown;
    String status_oks[] =	// "OK" status codes 
	   {
		  "successful-ok",
		  "successful-ok-ignored-or-substituted-attributes",
		  "successful-ok-conflicting-attributes",
		  "successful-ok-ignored-subscriptions",
		  "successful-ok-ignored-notifications",
		  "successful-ok-too-many-events",
		  "successful-ok-but-cancel-subscription"
	   };

    String status_400s[] =	// Client errors 
	   {
		  "client-error-bad-request",
		  "client-error-forbidden",
		  "client-error-not-authenticated",
		  "client-error-not-authorized",
		  "client-error-not-possible",
		  "client-error-timeout",
		  "client-error-not-found",
		  "client-error-gone",
		  "client-error-request-entity-too-large",
		  "client-error-request-value-too-long",
		  "client-error-document-format-not-supported",
		  "client-error-attributes-or-values-not-supported",
		  "client-error-uri-scheme-not-supported",
		  "client-error-charset-not-supported",
		  "client-error-conflicting-attributes",
		  "client-error-compression-not-supported",
		  "client-error-compression-error",
		  "client-error-document-format-error",
		  "client-error-document-access-error",
		  "client-error-attributes-not-settable",
		  "client-error-ignored-all-subscriptions",
		  "client-error-too-many-subscriptions",
		  "client-error-ignored-all-notifications",
		  "client-error-print-support-file-not-found"
	   };

    String status_500s[] =	// Server errors 
	   {
		  "server-error-internal-error",
		  "server-error-operation-not-supported",
		  "server-error-service-unavailable",
		  "server-error-version-not-supported",
		  "server-error-device-error",
		  "server-error-temporary-error",
		  "server-error-not-accepting-jobs",
		  "server-error-busy",
		  "server-error-job-canceled",
		  "server-error-multiple-document-jobs-not-supported",
		  "server-error-printer-is-deactivated"
	   };


   //
   // See if the error code is a known value...
   //
    if ((error >= IPPDefs.OK) && (error <= IPPDefs.OK_BUT_CANCEL_SUBSCRIPTION))
    {
      return (status_oks[error]);
    }
    else if (error == IPPDefs.REDIRECTION_OTHER_SITE)
    {
      return ("redirection-other-site");
    }
    else if ((error >= IPPDefs.BAD_REQUEST) && 
           (error <= IPPDefs.PRINT_SUPPORT_FILE_NOT_FOUND))
    {
      return (status_400s[error - IPPDefs.BAD_REQUEST]);
    }
    else if ((error >= IPPDefs.INTERNAL_ERROR) && 
             (error <= IPPDefs.PRINTER_IS_DEACTIVATED))
    {
      return (status_500s[error - IPPDefs.INTERNAL_ERROR]);
    }

   //
   //  No, build an "unknown-xxxx" error string...
   //

    unknown = "unknown" +  error;

    return (unknown);
  }


}   // End of IPPError class



