//
import java.io.*;
import java.net.*;

public class CUPSJob
{
    public int		job_id;
    public String       job_more_info;
    public String       job_uri;
    public String       job_printer_uri;
    public long         job_printer_up_time;
    public String       job_name;
    public String       job_originating_user_name;
    public String       document_format;
    public String       job_originating_host_name;
    public int          job_priority;
    public int          job_state;
    public int          job_media_sheets_completed;
    public int          job_k_octets;
    public long         time_at_creation;
    public long         time_at_processing;
    public long         time_at_completed;
    public String       job_hold_until;
    public String       job_sheets;
    public String       job_state_reasons;


    public CUPSJob()
    {
    }

    public void updateAttribute( IPPAttribute a )
    {
      IPPValue val;
     
      val = (IPPValue)a.values.get(0);
      if (a.name.compareTo("job-more-info") == 0)
      {
        job_more_info = val.text;
      }
      else if (a.name.compareTo("job-uri") == 0)
      {
        job_uri = val.text;
      }
      else if (a.name.compareTo("job-printer-up-time") == 0)
      {
        job_printer_up_time = val.integer_value;
      }
      else if (a.name.compareTo("job-originating-user-name") == 0)
      {
        job_originating_user_name = val.text;
      }
      else if (a.name.compareTo("document-format") == 0)
      {
        document_format = val.text;
      }
      else if (a.name.compareTo("job-priority") == 0)
      {
        job_priority = val.integer_value;
      }
      else if (a.name.compareTo("job-originating-host-name") == 0)
      {
        job_originating_host_name = val.text;
      }
      else if (a.name.compareTo("job-id") == 0)
      {
        job_id = val.integer_value;
      }
      else if (a.name.compareTo("job-state") == 0)
      {
        job_state = val.integer_value;
      }
      else if (a.name.compareTo("job-media-sheets-completed") == 0)
      {
        job_media_sheets_completed = val.integer_value;
      }
      else if (a.name.compareTo("job-printer-uri") == 0)
      {
        job_printer_uri = val.text;
      }
      else if (a.name.compareTo("job-name") == 0)
      {
        job_name = val.text;
      }
      else if (a.name.compareTo("job-k-octets") == 0)
      {
        job_k_octets = val.integer_value;
      }
      else if (a.name.compareTo("time-at-creation") == 0)
      {
        time_at_creation = val.integer_value;
      }
      else if (a.name.compareTo("time-at-processing") == 0)
      {
        time_at_processing = val.integer_value;
      }
      else if (a.name.compareTo("time-at-completed") == 0)
      {
        time_at_completed = val.integer_value;
      }
      else if (a.name.compareTo("job-hold-until") == 0)
      {
        job_hold_until = val.text;
      }
      else if (a.name.compareTo("job-sheets") == 0)
      {
        job_sheets = val.text;
      }
      else if (a.name.compareTo("job-state-reasons") == 0)
      {
        job_state_reasons = val.text;
      }
    }
}

//  eof ....
