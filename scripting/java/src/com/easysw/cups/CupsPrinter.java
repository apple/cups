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
 * A <code>CupsPrinter</code> holds printer attribute / status information,
 * and has methods to process CUPS server responses.
 *
 * @author	TDB
 * @version	1.0
 * @since	JDK1.3
 */

import java.io.*;
import java.net.*;
import java.util.*;

public class CupsPrinter
{

  //
  //  Printer attributes / status members.
  //
  String          printer_name;

  String          printer_location;
  String          printer_info;
  String          printer_more_info;

  String[]        printer_uri_supported;          // Strings
  String[]        uri_authentication_supported;   // Strings
  String[]        uri_security_supported;         // Strings

  String          attributes_charset;
  String          attributes_natural_language;

  int             printer_state;
  String          printer_state_text;
  String          printer_state_reasons;

  boolean         printer_is_accepting_jobs;

  long            printer_up_time;
  long            printer_current_time;

  int             queued_job_count;

  String[]        pdl_override_supported;
  String[]        ipp_versions_supported;

  int[]           operations_supported;    //  Integers

  boolean         multiple_document_jobs_supported;
  int             multiple_operation_time_out;
  int[]           multiple_document_handling_supported;  // Integers

  String          charset_configured;
  String          natural_language_configured;
  String          generated_natural_language_supported;
  String[]        charset_supported;       //  Strings

  String          document_format_default;
  String[]        document_format_supported;   // Strings

  String[]        compression_supported;       //  Strings

  int             job_priority_default;
  int             job_priority_supported;

  int             copies_default;
  int             lower_copies_supported;
  int             upper_copies_supported;

  boolean         page_ranges_supported;

  int             number_up_default;
  int[]           number_up_supported;    // integers


  int             orientation_requested_default;
  int[]           orientation_requested_supported;   //  Integers
 
  int             job_quota_period;
  int             job_k_limit;
  int             job_page_limit;

  String          job_sheets_default;     // Should this be a list too?
  String[]        job_sheets_supported;   // Strings

  String          device_uri;

  boolean         color_supported;
  int             pages_per_minute;

  String          printer_make_and_model;

  String          media_default;
  String[]        media_supported;      //  Strings
  
  int             finishings_default;
  int[]           finishings_supported;   //  Integers

  int             printer_type;



  /**
   *  Constructor.  Does not get status or attributes.
   *
   * @param	<code>c</code>		Cups object.
   * 
   * @see	<code>Cups</code>
   */   
  public CupsPrinter(Cups c)
  {
    setDefaults();
  }

  /**
   *  Constructor with name.  Get status and attributes.
   *
   * @param	<code>c</code>		Cups object.
   * @param	<code>name</code>	Name of printer.
   * 
   * @see	<code>Cups</code>
   */
  public CupsPrinter(Cups c, String name)
  {
    setDefaults();
    printer_name = name;

    //
    //
    getStatus(c);
    getAttributes(c);
  }



  /**
   * Initialize the members with mostly sane values.
   *
   */
  public void setDefaults()
  {
    printer_name = "";
    printer_location = "";
    printer_info = "";
    printer_more_info = "";
    printer_uri_supported = null;
    uri_authentication_supported = null;
    uri_security_supported = null;
    attributes_charset = "us-ascii";
    attributes_natural_language = "en";
    printer_state = -1;
    printer_state_text    = "";
    printer_state_reasons = "";
    printer_is_accepting_jobs = false;
    printer_up_time = 0;
    printer_current_time = 0;
    queued_job_count = 0;
    pdl_override_supported = null;
    ipp_versions_supported = null;
    operations_supported = null;
    multiple_document_jobs_supported = false;
    multiple_operation_time_out      = 0;
    multiple_document_handling_supported = null;
    charset_configured = "";
    natural_language_configured = "";
    generated_natural_language_supported = "";
    charset_supported = null;
    document_format_default = "";
    document_format_supported = null; 
    compression_supported = null;
    job_priority_default   = -1;
    job_priority_supported = -1;
    copies_default         = 1;
    lower_copies_supported = 1;
    upper_copies_supported = 1;
    page_ranges_supported = false;
    number_up_default = 0;
    number_up_supported = null;
    orientation_requested_default = 0;
    orientation_requested_supported = null;
    job_quota_period = 0;
    job_k_limit      = 0;
    job_page_limit   = 0;
    job_sheets_default = "none,none";
    job_sheets_supported = null;
    device_uri = "";
    color_supported = false;
    pages_per_minute = 0;
    printer_make_and_model = "";
    media_default = "";
    media_supported = null;
    finishings_default = 0;
    finishings_supported = null;
    printer_type = 0;
  }


  /**
   * Get the printer's status.
   *
   * @param	<code>c</code>		Cups object.
   *
   * @return	<code>Boolean</code>	True on success.
   *
   * @see	<code>Cups</code>
   */
  public boolean getStatus(Cups c) 
  {
     List         attrs;
     IPPAttribute a;
     String       p_uri;

     try
     {
       attrs = c.cupsGetPrinterStatus(printer_name);
       for (int i=0; i < attrs.size(); i++)
       {
         a = (IPPAttribute)attrs.get(i);
         updateAttribute(a);
       }  
       return(true);
     }
     catch (IOException e)
     {
       return(false);
     }
  }



  /**
   * Get the printer's attributes.
   *
   * @param	<code>c</code>		Cups object.
   *
   * @return	<code>Boolean</code>	True on success.
   *
   * @see	<code>Cups</code>
   */
  public boolean getAttributes(Cups c)
  {
     List         attrs;
     IPPAttribute a;
     String       p_uri;

     try
     {
       attrs = c.cupsGetPrinterAttributes(printer_name);
       for (int i=0; i < attrs.size(); i++)
       {
         a = (IPPAttribute)attrs.get(i);
         updateAttribute(a);
       } 
       return(true);
     }
     catch (IOException e)
     {
      return(false);
     }
  }




  /**
   *  Process an attribute from the cups.doRequest() method and move
   *  the values into local members.
   *
   * @param	<code>a</code>		IPPAttribute.
   *
   * @see	<code>IPPAttributes</code>
   * @see	<code>IPPValues</code>
   */
  public void updateAttribute( IPPAttribute a )
  {
    IPPValue v;
    int      i;

    // a.dump_values();

    if (a.name.compareTo("printer-name") == 0)
    {
      v = (IPPValue)a.values.get(0);
      printer_name = v.text;
    }
    else if (a.name.compareTo("printer-location") == 0)
    {
      v = (IPPValue)a.values.get(0);
      printer_location = v.text;
    }
    else if (a.name.compareTo("printer-info") == 0)
    {
      v = (IPPValue)a.values.get(0);
      printer_info = v.text;
    }
    else if (a.name.compareTo("printer-more-info") == 0)
    {
      v = (IPPValue)a.values.get(0);
      printer_more_info = v.text;
    }
    else if (a.name.compareTo("printer-uri-supported") == 0)
    {
      printer_uri_supported = new String[a.values.size()];
      for (i=0; i < a.values.size(); i++)
      {
        v = (IPPValue)a.values.get(i);
        printer_uri_supported[i] = v.text;
      }
    }
    else if (a.name.compareTo("uri-authentication-supported") == 0)
    {
      uri_authentication_supported = new String[a.values.size()];
      for (i=0; i < a.values.size(); i++)
      {
        v = (IPPValue)a.values.get(i);
        uri_authentication_supported[i] = v.text;
      }
    }
    else if (a.name.compareTo("uri-security-supported") == 0)
    {
      uri_security_supported = new String[a.values.size()];
      for (i=0; i < a.values.size(); i++)
      {
        v = (IPPValue)a.values.get(i);
        uri_security_supported[i] = v.text;
      }
    }
    else if (a.name.compareTo("attributes-charset") == 0)
    {
      v = (IPPValue)a.values.get(0);
      attributes_charset = v.text;
    }
    else if (a.name.compareTo("attributes-natural-language") == 0)
    {
      v = (IPPValue)a.values.get(0);
      attributes_natural_language = v.text;
    }
    else if (a.name.compareTo("printer-state") == 0)
    {
      v = (IPPValue)a.values.get(0);
      printer_state = v.integer_value;
      switch( printer_state )
      {
        case 3: printer_state_text = "idle";
                break;
        case 4: printer_state_text = "processing";
                break;
        case 5: printer_state_text = "stopped";
                break;
      }
    }
    else if (a.name.compareTo("printer-state-reasons") == 0)
    {
      v = (IPPValue)a.values.get(0);
      printer_state_reasons = v.text;
    }
    else if (a.name.compareTo("printer-is-accepting-jobs") == 0)
    {
      v = (IPPValue)a.values.get(0);
      printer_is_accepting_jobs = v.boolean_value;
    }
    else if (a.name.compareTo("printer-up-time") == 0)
    {
      v = (IPPValue)a.values.get(0);
      printer_up_time = v.integer_value;
    }
    else if (a.name.compareTo("printer-current-time") == 0)
    {
      v = (IPPValue)a.values.get(0);
      printer_current_time = v.unix_time;   //  *** FIX ***
    }
    else if (a.name.compareTo("queue-job-count") == 0)
    {
      v = (IPPValue)a.values.get(0);
      queued_job_count = v.integer_value;  
    }
    else if (a.name.compareTo("pdl-override-supported") == 0)
    {
      pdl_override_supported = new String[a.values.size()];
      for (i=0; i < a.values.size(); i++)
      {
        v = (IPPValue)a.values.get(i);
        pdl_override_supported[i] = v.text;
      }
    }
    else if (a.name.compareTo("ipp-versions-supported") == 0)
    {
      ipp_versions_supported = new String[a.values.size()];
      for (i=0; i < a.values.size(); i++)
      {
        v = (IPPValue)a.values.get(i);
        ipp_versions_supported[i] = v.text;
      }
    }
    else if (a.name.compareTo("operations-supported") == 0)
    {
      operations_supported = new int[a.values.size()];
      for (i=0; i < a.values.size(); i++)
      {
        v = (IPPValue)a.values.get(i);
        operations_supported[i] = v.integer_value;
      }
    }
    else if (a.name.compareTo("multiple-document-jobs-supported") == 0)
    {
      v = (IPPValue)a.values.get(0);
      multiple_document_jobs_supported = v.boolean_value;  
    }
    else if (a.name.compareTo("multiple-operation-time-out") == 0)
    {
      v = (IPPValue)a.values.get(0);
      multiple_operation_time_out = v.integer_value;  
    }
    else if (a.name.compareTo("multiple-document-handling-supported") == 0)
    {
      multiple_document_handling_supported = new int[a.values.size()];
      for (i=0; i < a.values.size(); i++)
      {
        v = (IPPValue)a.values.get(i);
        multiple_document_handling_supported[i] = v.integer_value;
      }
    }
    else if (a.name.compareTo("charset-configured") == 0)
    {
      v = (IPPValue)a.values.get(0);
      charset_configured = v.text;  
    }
    else if (a.name.compareTo("natural-language-configured") == 0)
    {
      v = (IPPValue)a.values.get(0);
      natural_language_configured = v.text;  
    }
    else if (a.name.compareTo("generated-natural-language-supported") == 0)
    {
      //  *** Should this be a list too?
      v = (IPPValue)a.values.get(0);
      generated_natural_language_supported = v.text;  
    }
    else if (a.name.compareTo("charset-supported") == 0)
    {
      charset_supported = new String[a.values.size()];
      for (i=0; i < a.values.size(); i++)
      {
        v = (IPPValue)a.values.get(i);
        charset_supported[i] = v.text;
      }
    }
    else if (a.name.compareTo("document-format-default") == 0)
    {
      v = (IPPValue)a.values.get(0);
      document_format_default = v.text;  
    }
    else if (a.name.compareTo("document-format-supported") == 0)
    {
      document_format_supported = new String[a.values.size()];
      for (i=0; i < a.values.size(); i++)
      {
        v = (IPPValue)a.values.get(i);
        document_format_supported[i] = v.text;
      }
    }
    else if (a.name.compareTo("compression-supported") == 0)
    {
      compression_supported = new String[a.values.size()];
      for (i=0; i < a.values.size(); i++)
      {
        v = (IPPValue)a.values.get(i);
        compression_supported[i] = v.text;
      }
    }
    else if (a.name.compareTo("job-priority-default") == 0)
    {
      v = (IPPValue)a.values.get(0);
      job_priority_default = v.integer_value;  
    }
    else if (a.name.compareTo("job-priority-supported") == 0)
    {
      //  *** Should be a list?  ***
      v = (IPPValue)a.values.get(0);
      job_priority_supported = v.integer_value;  
    }
    else if (a.name.compareTo("copies-default") == 0)
    {
      v = (IPPValue)a.values.get(0);
      copies_default = v.integer_value;  
    }
    else if (a.name.compareTo("copies-supported") == 0)
    {
      v = (IPPValue)a.values.get(0);
      lower_copies_supported = v.lower;  
      upper_copies_supported = v.upper;  
    }
    else if (a.name.compareTo("page-ranges-supported") == 0)
    {
      v = (IPPValue)a.values.get(0);
      page_ranges_supported = v.boolean_value;  
    }
    else if (a.name.compareTo("number-up-default") == 0)
    {
      v = (IPPValue)a.values.get(0);
      number_up_default = v.integer_value;  
    }
    else if (a.name.compareTo("number-up-supported") == 0)
    {
      number_up_supported = new int[a.values.size()];
      for (i=0; i < a.values.size(); i++)
      {
        v = (IPPValue)a.values.get(i);
        number_up_supported[i] = v.integer_value;
      }
    }
    else if (a.name.compareTo("orientation-requested-default") == 0)
    {
      v = (IPPValue)a.values.get(0);
      orientation_requested_default = v.integer_value;  
    }
    else if (a.name.compareTo("orientation-requested-supported") == 0)
    {
      orientation_requested_supported = new int[a.values.size()];
      for (i=0; i < a.values.size(); i++)
      {
        v = (IPPValue)a.values.get(i);
        orientation_requested_supported[i] = v.integer_value;
      }
    }
    else if (a.name.compareTo("job-quota-period") == 0)
    {
      v = (IPPValue)a.values.get(0);
      job_quota_period = v.integer_value;  
    }
    else if (a.name.compareTo("job-k-limit") == 0)
    {
      v = (IPPValue)a.values.get(0);
      job_k_limit = v.integer_value;  
    }
    else if (a.name.compareTo("job-page-limit") == 0)
    {
      v = (IPPValue)a.values.get(0);
      job_page_limit = v.integer_value;  
    }
    else if (a.name.compareTo("job-sheets-default") == 0)
    {
      v = (IPPValue)a.values.get(0);
      job_sheets_default = v.text;  
    }
    else if (a.name.compareTo("job-sheets-supported") == 0)
    {
      job_sheets_supported = new String[a.values.size()];
      for (i=0; i < a.values.size(); i++)
      {
        v = (IPPValue)a.values.get(i);
        job_sheets_supported[i] = v.text;
      }
    }
    else if (a.name.compareTo("device-uri") == 0)
    {
      v = (IPPValue)a.values.get(0);
      device_uri = v.text;  
    }
    else if (a.name.compareTo("color-supported") == 0)
    {
      v = (IPPValue)a.values.get(0);
      color_supported = v.boolean_value;  
    }
    else if (a.name.compareTo("pages-per-minute") == 0)
    {
      v = (IPPValue)a.values.get(0);
      pages_per_minute = v.integer_value;  
    }
    else if (a.name.compareTo("printer-make-and-model") == 0)
    {
      v = (IPPValue)a.values.get(0);
      printer_make_and_model = v.text;  
    }
    else if (a.name.compareTo("media-default") == 0)
    {
      v = (IPPValue)a.values.get(0);
      media_default = v.text;  
    }
    else if (a.name.compareTo("media-supported") == 0)
    {
      media_supported = new String[a.values.size()];
      for (i=0; i < a.values.size(); i++)
      {
        v = (IPPValue)a.values.get(i);
        media_supported[i] = v.text;
      }
    }
    else if (a.name.compareTo("finishings-default") == 0)
    {
      v = (IPPValue)a.values.get(0);
      finishings_default = v.integer_value;  
    }
    else if (a.name.compareTo("finishings-supported") == 0)
    {
      finishings_supported = new int[a.values.size()];
      for (i=0; i < a.values.size(); i++)
      {
        v = (IPPValue)a.values.get(i);
        finishings_supported[i] = v.integer_value;
      }
    }
    else if (a.name.compareTo("printer-type") == 0)
    {
      v = (IPPValue)a.values.get(0);
      printer_type = v.integer_value;  
    }

  }  // End of updateAttribute()


  /**
   * Get the printer name.
   *
   * @return	<code>String</code>	Printer Name.
   */
  public String getPrinterName()
  {
    return(printer_name);
  }

  /**
   * Get the printer state text.
   *
   * @return	<code>String</code>	State text.
   */
  public String getStateText()
  {
    return(printer_state_text);
  }

  /**
   * Get the printer state reasons.
   *
   * @return	<code>String</code>	State reason.
   */
  public String getStateReasons()
  {
    return(printer_state_reasons);
  }

  /**
   * Get the printer location.
   *
   * @return	<code>String</code>	State location.
   */
  public String getLocation()
  {
    return(printer_location);
  }

  /**
   * Get the printer make and model.
   *
   * @return	<code>String</code>	Make and model.
   */
  public String getMakeAndModel()
  {
    return(printer_make_and_model);
  }



  /**
   * Get the default job sheets.
   *
   * @return	<code>String</code>	Default job sheets.
   */
  public String getJobSheetsDefault()
  {
    return(job_sheets_default);
  }

  /**
   * Get the printer job sheets supported.
   *
   * @return	<code>String[]</code>	Array of supported job sheets.
   */
  public String[] getJobSheetsSupported()
  {
    return(job_sheets_supported);
  }


  /**
   * Get the default orientation.
   *
   * @return	<code>int</code>	Default page orientation.
   */
  public int getOrientationDefault()
  {
    return(orientation_requested_default);
  }

  /**
   * Get the printer orientation supported.
   *
   * @return	<code>int[]</code>	Array of supported orientations.
   */
  public int[] getOrientationSupported()
  {
    return(orientation_requested_supported);
  }


  /**
   * Get the printer lower copies supported.
   *
   * @return	<code>int</code>	Lower of the range.
   */
  public int getLowerCopiesSupported()
  {
    return(lower_copies_supported);
  }


  /**
   * Get the printer upper copies supported.
   *
   * @return	<code>int</code>	Upper of the range.
   */
  public int getUpperCopiesSupported()
  {
    return(upper_copies_supported);
  }


  /**
   * Get the printer number of copies default.
   *
   * @return	<code>int</code>	Default number of copies.
   */
  public int getCopiesDefault()
  {
    return(copies_default);
  }


  /**
   * Get whether the printer supports page ranges.
   *
   * @return	<code>boolean</code>	True or false.
   */
  public boolean getPageRangesSupported()
  {
    return(page_ranges_supported);
  }



  /**
   *  Debug method.
   */
  void dump()
  {
    int i;

    System.out.println("Printer Name: " + printer_name );
    System.out.println("Location:     " + printer_location );
    System.out.println("Printer Info: " + printer_info );
    System.out.println("More Info:    " + printer_more_info );

    if (printer_uri_supported != null)
    {
      System.out.println("Printer URI's Supported: ");
      for (i=0; i < printer_uri_supported.length; i++)
      { 
        System.out.println("  " + printer_uri_supported[i] ); 
      }
    }

    if (uri_authentication_supported != null)
    {
      System.out.println("URI Authentication Supported: ");
      for (i=0; i < uri_authentication_supported.length; i++)
      {  
        System.out.println("  " + uri_authentication_supported[i] );
      }
    }

    if (uri_security_supported != null)
    {
      System.out.println("URI Security Supported: ");
      for (i=0; i < uri_security_supported.length; i++)
      { 
        System.out.println("  " + uri_security_supported[i] );
      }
    }

    System.out.println("Attributes Charset: " + attributes_charset );
    System.out.println("Attributes Natural Language: " + attributes_natural_language );

    System.out.println("Printer State: " + printer_state );
    System.out.println("Printer State Text: " + printer_state_text );
    System.out.println("Printer State Reasons: " + printer_state_reasons );

    if (printer_is_accepting_jobs)
      System.out.println("Accepting Jobs:  Yes");
    else
      System.out.println("Accepting Jobs:  No");


}



}  //  End of CupsPrinter class

 
