//
import java.io.*;
import java.net.*;
import java.util.*;

public class CUPSPrinter
{
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



  //  ---------------------------------------------
  //
  //  Basic constructor.
  //  
  public CUPSPrinter()
  {
  }




  //  ---------------------------------------------
  //
  //  Move the attributes from an ipp request
  //  into a CUPSPrinter object.
  //
  public void updateAttribute( IPPAttribute a )
  {
    IPPValue v;
    int      i;

    
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





}  //  End of CUPSPrinter class

 
