

import java.io.*;
import java.net.*;

public class CUPS
{

    IPP		ipp;               //  IPP Request
    IPPHttp     http;              //  Connection to server

    String      site;              //  URL of site.
    String      dest;              //  Name of destination printer
    String      instance;          //  Instance of printer
    
    public CUPS()
    {
      site     = "http://localhost:631/";
      dest     = "";
    }

    public CUPS( URL p_url )
    {
      site     = "http://" + p_url.getHost() + ":" + 
                 p_url.getPort();
      if (p_url.getPath().length() > 0)
        site = site + p_url.getPath();  
      dest     = "";
    }


    public CUPS( String p_dest )
    {
      site     = "http://localhost:631/";
      dest     = p_dest;
      instance = "";
    }


    public CUPS( URL p_url, String p_dest )
    {
      site     = "http://" + p_url.getHost() + ":" + 
                 p_url.getPort();
      if (p_url.getPath().length() > 0)
        site = site + p_url.getPath();  
      dest     = p_dest;
    }

    public CUPS( String p_dest, String p_instance )
    {
      site     = "http://localhost:631/";
      dest     = p_dest;
      instance = p_instance;
    }


    //  -----------------------------------------------------------
    //
    //  Do a CUPS request.
    //
    public boolean doRequest() throws IOException
    {
      IPPAttribute attr;

      //
      //  Connect if needed.
      //
      if (http == null)
      {
        String url_str = site + dest;
System.out.println("\nDest: " + url_str + "\n");

        try
        {
          http = new IPPHttp(url_str);
        }
        catch (IOException e)
        {
          throw(e);
        }        
      }

      //
      //  Send the HTTP header.
      //
      http.writeHeader( http.path, ipp.sizeInBytes() );

      //
      //  Send the request header.
      //
      byte[] header = new byte[8];
      header[0] = (byte)1; 
      header[1] = (byte)1; 
      header[2] = (byte)((ipp.request.operation_id & 0xff00) >> 8);
      header[3] = (byte)(ipp.request.operation_id & 0xff);
      header[4] = (byte)((ipp.request.request_id & 0xff000000) >> 24);
      header[5] = (byte)((ipp.request.request_id & 0xff0000) >> 16);
      header[6] = (byte)((ipp.request.request_id & 0xff00) >> 8);
      header[7] = (byte)(ipp.request.request_id & 0xff);
      http.write( header );

      //
      //  Send the attributes list.
      //
      byte[] bytes;
      int    sz;
      int    last_group = -1;
      for (int i=0; i < ipp.attrs.size(); i++)
      {
        attr = (IPPAttribute)ipp.attrs.get(i);
        sz    = attr.sizeInBytes(last_group);
        bytes = attr.getBytes(sz,last_group);
        last_group = attr.group_tag;
        http.write(bytes);
      }

      //
      //  Send the end of attributes tag.
      //
      byte[] footer = new byte[1];
      footer[0] = (byte)IPPDefs.TAG_END; 
      http.write( footer );

      //  ------------------------------------------
      //
      //   Now read back response
      //

      int read_length;

      read_length = http.read_header();
      if (read_length > 0)
      {
        http.read_buffer = http.read(read_length);
        http.conn.close();
        ipp = http.processResponse();

        return( true );
      }

      return( false );

    }  // End of doRequest




    //  -----------------------------------------------------------
    //
    //  Get a list of jobs.
    //
    public CUPSJob[] cupsGetJobs( boolean showMyJobs, boolean showCompleted ) 
           throws IOException
    {

      IPPAttribute a;

      String req_attrs[] = /* Requested attributes */
	     {
		  "job-id",
		  "job-priority",
		  "job-k-octets",
		  "job-state",
		  "time-at-completed",
		  "time-at-creation",
		  "time-at-processing",
		  "job-printer-uri",
		  "document-format",
		  "job-name",
		  "job-originating-user-name"
	     };


      //
      //  Create a new IPP request if needed.
      //
      if (ipp == null)
      {
        ipp = new IPP();
        ipp.request = new IPPRequest( 1, (short)IPPDefs.GET_JOBS );
      }

      a = new IPPAttribute( IPPDefs.TAG_OPERATION, IPPDefs.TAG_CHARSET,
                            "attributes-charset" );
      a.addString( "", "iso-8859-1" );  
      ipp.addAttribute(a);
            

      a = new IPPAttribute( IPPDefs.TAG_OPERATION, IPPDefs.TAG_LANGUAGE,
                            "attributes-natural-language" );
      a.addString( "", "en" );  
      ipp.addAttribute(a);
            

      //
      //  Add the printer uri
      //
      a = new IPPAttribute( IPPDefs.TAG_OPERATION, IPPDefs.TAG_URI,
                            "job-uri" );

      if (dest != null)
        a.addString( "", "ipp://localhost/printers/" + dest );  
      else
        a.addString( "", "ipp://localhost/jobs" );  
      ipp.addAttribute(a);
            
      //
      //  Add the requesting user name
      //
      //a = new IPPAttribute( IPPDefs.TAG_OPERATION, IPPDefs.TAG_NAME,
      //                      "requesting-user-name" );
      //a.addString( "", "root" );
      //ipp.addAttribute(a);

      //
      //  Show only my jobs?
      //
      if (showMyJobs)
      {
        a = new IPPAttribute( IPPDefs.TAG_OPERATION, IPPDefs.TAG_BOOLEAN,
                              "my-jobs" );
        a.addBoolean( true );
        ipp.addAttribute(a);
      }
  
      //
      //  Show completed jobs?
      //
      if (showCompleted)
      {
        a = new IPPAttribute( IPPDefs.TAG_OPERATION, IPPDefs.TAG_KEYWORD,
                              "which-jobs" );
        a.addString( "", "completed" );
        ipp.addAttribute(a);
      }

      // a = new IPPAttribute( IPPDefs.TAG_OPERATION, IPPDefs.TAG_KEYWORD,
      //                      "requested-attributes" );
      // a.addStrings( "", req_attrs );
      // ipp.addAttribute(a);
      
      if (doRequest())
      {
        int num_jobs = 0;
        for (int i=0; i < ipp.attrs.size(); i++)
        {
          a = (IPPAttribute)ipp.attrs.get(i);
          if (a.name.compareTo("job-id") == 0)
            num_jobs++;
        }

        if (num_jobs < 1)
          return(null);

        CUPSJob[] jobs = new CUPSJob[num_jobs];
        int     n = 0;
        for (int i=0; i < ipp.attrs.size(); i++)
        {
          a = (IPPAttribute)ipp.attrs.get(i);
          if (a.group_tag < 2)
            continue;
          else if (a.value_tag == IPPDefs.TAG_ZERO)
          {
             n++;
             continue;
          }
          else jobs[n].updateAttribute( a );
        }
        return( jobs );
      }
      return(null);

    }  //  End of cupsGetJobs



    //  -----------------------------------------------------------
    // 
    //  Get a list of printers.
    //
    public String[] cupsGetPrinters() 
           throws IOException
    {

      IPPAttribute a;

      //
      //  Create a new IPP request if needed.
      //
      if (ipp == null)
      {
        ipp = new IPP();
      }

      //
      //  Fill in the required attributes
      //
      ipp.request = new IPPRequest( 1, (short)IPPDefs.CUPS_GET_PRINTERS );

      a = new IPPAttribute( IPPDefs.TAG_OPERATION, IPPDefs.TAG_CHARSET,
                            "attributes-charset" );
      a.addString( "", "iso-8859-1" );  
      ipp.addAttribute(a);
            

      a = new IPPAttribute( IPPDefs.TAG_OPERATION, IPPDefs.TAG_LANGUAGE,
                            "attributes-natural-language" );
      a.addString( "", "en" );  
      ipp.addAttribute(a);
            

      if (doRequest())
      {
        int num_printers = 0;
        for (int i=0; i < ipp.attrs.size(); i++)
        {
          a = (IPPAttribute)ipp.attrs.get(i);
          if ((a.name.compareTo("printer-name") == 0) &&
              (a.value_tag == IPPDefs.TAG_NAME))
          {
            num_printers++;
          }
        }
        if (num_printers < 1)
          return(null);

        String[] printers = new String[num_printers];
        IPPValue val;
        int     n = 0;
        for (int i=0; i < ipp.attrs.size(); i++)
        {
          a = (IPPAttribute)ipp.attrs.get(i);
          if (a.group_tag < 2)
            continue;

          if ((a.name.compareTo("printer-name") == 0) &&
              (a.value_tag == IPPDefs.TAG_NAME))
          {
            val = (IPPValue)a.values.get(0);
            if (val != null)
            {
              printers[n] = val.text;
              n++;
            }
          }
        }
        return( printers );

      }  // if doRequest ...

      return(null);

    }  // End of cupsGetPrinters




    //  -----------------------------------------------------------
    // 
    //  Get default destination
    //
    public String cupsGetDefault() 
           throws IOException
    {

      IPPAttribute a;

      //
      //  Create a new IPP request if needed.
      //
      if (ipp == null)
      {
        ipp = new IPP();
      }

      //
      //  Fill in the required attributes
      //
      ipp.request = new IPPRequest( 1, (short)IPPDefs.CUPS_GET_DEFAULT);

      a = new IPPAttribute( IPPDefs.TAG_OPERATION, IPPDefs.TAG_CHARSET,
                            "attributes-charset" );
      a.addString( "", "iso-8859-1" );  
      ipp.addAttribute(a);
            

      a = new IPPAttribute( IPPDefs.TAG_OPERATION, IPPDefs.TAG_LANGUAGE,
                            "attributes-natural-language" );
      a.addString( "", "en" );  
      ipp.addAttribute(a);
            

      if (doRequest())
      {
        int num_printers = 0;
        for (int i=0; i < ipp.attrs.size(); i++)
        {
          a = (IPPAttribute)ipp.attrs.get(i);
          if ((a.name.compareTo("printer-name") == 0) &&
              (a.value_tag == IPPDefs.TAG_NAME))
          {
            IPPValue val = (IPPValue)a.values.get(0);
            if (val != null)
            {
              return( val.text );
            }
          }
        }
      }  // if doRequest ...

      return(null);

    }  // End of cupsGetDefault


    //  -----------------------------------------------------------
    // 
    //  Get printer attributes
    //
    public CUPSPrinter cupsGetPrinterAttributes( String printer_name ) 
           throws IOException
    {

      IPPAttribute a;
      CUPSPrinter  cp = new CUPSPrinter();

      //
      //  Create a new IPP request if needed.
      //
      if (ipp == null)
      {
        ipp = new IPP();
      }

      //
      //  Fill in the required attributes
      //
      ipp.request = new IPPRequest( 1, (short)IPPDefs.GET_PRINTER_ATTRIBUTES );

      a = new IPPAttribute( IPPDefs.TAG_OPERATION, IPPDefs.TAG_CHARSET,
                            "attributes-charset" );
      a.addString( "", "iso-8859-1" );  
      ipp.addAttribute(a);
            

      a = new IPPAttribute( IPPDefs.TAG_OPERATION, IPPDefs.TAG_LANGUAGE,
                            "attributes-natural-language" );
      a.addString( "", "en" );  
      ipp.addAttribute(a);
            
      a = new IPPAttribute( IPPDefs.TAG_OPERATION, IPPDefs.TAG_URI,
                            "printer-uri" );
      a.addString( "", site + dest );  
      ipp.addAttribute(a);
            
      a = new IPPAttribute( IPPDefs.TAG_OPERATION, IPPDefs.TAG_NAME,
                            "printer-name" );
      a.addString( "", printer_name );  
      ipp.addAttribute(a);
            

      if (doRequest())
      {
        for (int i=0; i < ipp.attrs.size(); i++)
        {
          a = (IPPAttribute)ipp.attrs.get(i);
          cp.updateAttribute( a );
        }
        return(cp);

      }  // if doRequest ...

      return(null);

    }  // End of cupsGetPrinterAttributes



}  // End of CUPS class


