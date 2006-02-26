package com.easysw.cups;

/**
 * @version 1.2 26-FEB-2006
 * @author  Easy Software Products
 *
 *   Internet Printing Protocol definitions for the Common UNIX Printing
 *   System (CUPS).
 *
 *   Copyright 1997-2006 by Easy Software Products.
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
 *       Hollywood, Maryland 20636 USA
 *
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org/
 */

/**
 * An <code>Cups</code> object is used for connecting to servers,
 * reading and writing data, and performing common CUPS operations.
 *
 * @author	TDB
 * @version	1.2
 * @since	JDK1.3
 */

import java.io.*;
import java.util.*;
import java.net.*;

public class Cups
{

    static final int REQ_STATE_CREATE_HTTP       = 0;
    static final int REQ_STATE_WRITE_HTTP_HEADER = 1;
    static final int REQ_STATE_WRITE_IPP_HEADER  = 2;
    static final int REQ_STATE_WRITE_IPP_ATTRS   = 3;
    static final int REQ_STATE_FINISH_IPP_ATTRS  = 4;
    static final int REQ_STATE_READ_RESPONSE     = 5;
    static final int REQ_STATE_DONE              = 6;
    static final String[] req_state_names =
                          { "Create HTTP", 
                            "Write Http Header", 
                            "Write IPP Header",
                            "Write IPP Attrs", 
                            "Finish IPP Attrs", 
                            "Read Response",
                            "Done" 
                          };


    static final int FILEREQ_STATE_CREATE_HTTP       = 0;
    static final int FILEREQ_STATE_WRITE_HTTP_HEADER = 1;
    static final int FILEREQ_STATE_WRITE_IPP_HEADER  = 2;
    static final int FILEREQ_STATE_WRITE_IPP_ATTRS   = 3;
    static final int FILEREQ_STATE_FINISH_IPP_ATTRS  = 4;
    static final int FILEREQ_STATE_WRITE_FILE_DATA   = 5;
    static final int FILEREQ_STATE_READ_RESPONSE     = 6;
    static final int FILEREQ_STATE_DONE              = 7;
    static final String[] filereq_state_names =
                          { "Create HTTP", 
                            "Write Http Header", 
                            "Write IPP Header",
                            "Write IPP Attrs", 
                            "Finish IPP Attrs", 
                            "Write File Data", 
                            "Read Response",
                            "Done" 
                          };


    IPP		ipp;               //  IPP Request
    IPPHttp     http;              //  Connection to server


    String      protocol;          //  Protocol name
    String      address;           //  address/name of server
    int         port;              //  Port #
    String      path;              //  Path ....
    String      dest;              //  Name of destination printer
    String      instance;          //  Instance of printer

    //
    //  encrypt, user, and passwd are not fully implemented!
    //
    boolean     encrypt;           //  Open encrypted connection.
    String      user;              //  User to login as.
    String      passwd;            //  Password if needed.

    String      site;              //  URL of site.

    int         last_error;        // Last error #
    String      error_text;        // Text for error
    
    /**
     * Void constructor.
     */
    public Cups()
    {
      http     = null;
      ipp      = null;

      protocol = "http";
      address  = "localhost";
      port     = 631;
      path     = "/";
      site     = "http://localhost:631/";
      dest     = "";
      instance = "";
      user     = "";
      passwd   = "";
      encrypt  = false;
    }

    /**
     * Constructor using a <code>URL</code>.
     *
     * @param	<code>p_url</code>	A <code>URL</code> object.
     */
    public Cups( URL p_url )
    {
      http     = null;
      ipp      = null;

      protocol = p_url.getProtocol() + "://";
      address  = p_url.getHost();
      port     = p_url.getPort();
      path     = p_url.getPath();

      site     = protocol + address;
      if (port > 0)
        site = site + ":" + port;

      if (path.length() > 0)
        site = site + path;

      dest     = "";
      instance = "";
      user     = "";
      passwd   = "";
      encrypt  = false;
    }


    /**
     * Set the value of the <code>protocol</code> member.  Valid values
     * are ipp or http.
     *
     * @param	<code>p_protocol</code>		String with protocol.
     */
    public void setProtocol( String p_protocol )
    {
      protocol = p_protocol;
      site     = protocol + "://" + address + ":" + port + path;
    }

    /**
     * Set the value of the <code>server</code> member.  This is an
     * IP address or a hostname.
     *
     * @param	<code>p_server</code>		IP address or hostname.
     */
    public void setServer( String p_server )
    {
      address = p_server;
      site     = protocol + "://" + address + ":" + port + path;
    }


    /**
     * Set the value of the <code>port</code> member.  
     *
     * @param	<code>p_port</code>		Port number.
     */
    public void setPort( int p_port )
    {
      port = p_port;
      site = protocol + "://" + address + ":" + port + path;
    }


    /**
     * Set the value of the <code>user</code> member.  
     *
     * @param	<code>p_user</code>		User name.
     */
    public void setUser( String p_user )
    {
      user = p_user;
    }


    /**
     * Set the value of the <code>passwd</code> member.  
     *
     * @param	<code>p_passwd</code>		Password.
     */
    public void setPasswd( String p_passwd )
    {
      passwd = p_passwd;
    }


    /**
     * Set the value of the <code>dest</code> member.  
     *
     * @param	<code>p_dest</code>		Destination.
     */
    public void setDest( String p_dest )
    {
      dest = p_dest;
    }


    /**
     * Set the value of the <code>instance</code> member.  
     *
     * @param	<code>p_instance</code>		Instance.
     */
    public void setInstance( String p_instance)
    {
      instance = p_instance;
    }


    /**
     * Set the value of the <code>encrypt</code> member.  
     *
     * @param	<code>p_enrypt</code>		Yes or no.
     */
    public void setEncrypt( boolean p_encrypt )
    {
      encrypt = p_encrypt;
    }


    /**
     * Get the value of the <code>encrypt</code> member.  
     *
     * @return 	<code>boolean</code>		Encryption on or off.
     */
    public boolean getEncrypt()
    {
      return(encrypt);
    }


    /**
     * Set the value of the <code>path</code> member.  This is the
     * path that will be used in the POST method.
     *
     * @param	<code>p_path</code>		Path on server.
     */
    public void setPath( String p_path )
    {
      path = p_path;
      site = protocol + "://" + address + ":" + port + path;
    }



    public boolean doRequest(String from) throws IOException
    {
      // System.out.println("doRequest From: " + from );
      return(doRequest());
    }



    /**
     * Do a CUPS request to the server.
     *
     * @param	<code>p_dest</code>		Destination name.
     * @return  <code>boolean</code>            True on success, false otherwise
     * @throw   <code>IOException</code>
     */
    public boolean doRequest() throws IOException
    {
      IPPAttribute attr;
      int state  = REQ_STATE_CREATE_HTTP;
      int errors = 0;

      while (true)
      {
        switch( state )
        {

          case REQ_STATE_CREATE_HTTP:
            String url_str = site + dest;

            try
            {
              if (user.length() > 0 && passwd.length() > 0)
                http = new IPPHttp(url_str, "", user, passwd );
              else
                http = new IPPHttp(url_str);
              state++;
            }
            catch (IOException e)
            {
              throw(e);
            }
            break;



          case REQ_STATE_WRITE_HTTP_HEADER:
            //
            //  Send the HTTP header.
            //
            switch( http.writeHeader( http.path, ipp.sizeInBytes() ))
            {
              case IPPHttp.HTTP_FORBIDDEN:
              case IPPHttp.HTTP_NOT_FOUND:
              case IPPHttp.HTTP_BAD_REQUEST:
              case IPPHttp.HTTP_METHOD_NOT_ALLOWED:
              case IPPHttp.HTTP_PAYMENT_REQUIRED:
              case IPPHttp.HTTP_UPGRADE_REQUIRED:
              case IPPHttp.HTTP_ERROR:
              case IPPHttp.HTTP_UNAUTHORIZED:
                     errors++;
                     if (errors < 5)
                     {
                       if (!http.reConnect())
                       {
                         System.out.println("Could not reConnect(0)!");
                         return(false);
                       }
                     }
                     else
                     {
                       return(false);
                     }
                     break;

              default: state++;
            }
            break;



          case REQ_STATE_WRITE_IPP_HEADER:
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
            if (http.checkForResponse() >= IPPHttp.HTTP_BAD_REQUEST)
            {
               errors++;
               if (errors < 5)
               {
                 if (http.reConnect())
                   state = REQ_STATE_WRITE_HTTP_HEADER;
                 else
                 {
                   System.out.println("Could not reConnect(1)\n");
                   return(false);
                 }
               }
               else
               {
                 return(false);
               }
            }
            else state++;
            break;


          case REQ_STATE_WRITE_IPP_ATTRS:
            //
            //  Send the attributes list.
            //
            byte[]  bytes;
            int     sz;
            int     last_group = -1;
            boolean auth_error = false;
            for (int i=0; i < ipp.attrs.size() && !auth_error; i++)
            {
              attr = (IPPAttribute)ipp.attrs.get(i);
              sz    = attr.sizeInBytes(last_group);
              bytes = attr.getBytes(sz,last_group);
              last_group = attr.group_tag;
              http.write(bytes);

              //
              //  Check for server response between each attribute.
              //
              if (http.checkForResponse() >= IPPHttp.HTTP_BAD_REQUEST)
              {
                 errors++;
                 if (errors < 5)
                 {
                   if (!http.reConnect())
                   {
                     System.out.println("Could not reConnect(2)");
                     return(false);
                   }
                   state = REQ_STATE_WRITE_HTTP_HEADER;
                   auth_error = true;
                 }
                 else
                 {
                   return(false);
                 }
              }

            }
            if (!auth_error)
              state++;
            break;



          case REQ_STATE_FINISH_IPP_ATTRS:
            //
            //  Send the end of attributes tag.
            //
            byte[] footer = new byte[1];
            footer[0] = (byte)IPPDefs.TAG_END; 
            http.write( footer );

            //
            //  Keep checking .....
            //
            if (http.checkForResponse() >= IPPHttp.HTTP_BAD_REQUEST)
            {
              errors++;
              if (errors < 5)
              {
                   if (!http.reConnect())
                   {
                     System.out.println("Could not reConnect(3)");
                     return(false);
                   }
                   state = REQ_STATE_WRITE_HTTP_HEADER;
              }
              else
              {
                return(false);
              }
            }
            else state++;
            break;



          case REQ_STATE_READ_RESPONSE:
            //
            //   Now read back response
            //
            int read_length;
            read_length = http.read_header();
            switch( http.status )
            {
               case IPPHttp.HTTP_OK: 
                 break;

               case IPPHttp.HTTP_UNAUTHORIZED: 
                    http.reConnect();
                    state = REQ_STATE_WRITE_HTTP_HEADER;
                    errors = 0;
                 break;

               default:
                  errors++;
                  if (errors < 5)
                  {
                    if (!http.reConnect())
                    {
                      System.out.println("Could not reConnect(4)");
                      return(false);
                    }
                    state = REQ_STATE_WRITE_HTTP_HEADER;
                  }
                  else 
                  {
                    System.out.println("Too many errors: " + errors );
                    return(false);
                  }
                  break;
            }

            if ((read_length > 0) && (state == REQ_STATE_READ_RESPONSE))
            {
              http.read_buffer = http.read(read_length);
              ipp = http.processResponse();
              state++;
            }
            break;

          case REQ_STATE_DONE:
            //
            //  success.
            //
            http.conn.close();
            http = null;
            return(true);
        }
      }

    }  // End of doRequest



    /**
     * Send a FILE to the CUPS server.
     *
     * @param	<code>file</code>		File to send.
     * @return  <code>boolean</code>            True on success, false otherwise
     * @throw   <code>IOException</code>
     */
    public boolean doRequest(File file) throws IOException
    {
      IPPAttribute     attr;
      int              state  = FILEREQ_STATE_CREATE_HTTP;
      int              errors = 0;
      FileInputStream  fis    = null;

      while (true)
      {
        switch( state )
        {

          case FILEREQ_STATE_CREATE_HTTP:
            String url_str = site + dest;
            try
            {
              if (user.length() > 0 && passwd.length() > 0)
                http = new IPPHttp(url_str, "", user, passwd );
              else
                http = new IPPHttp(url_str);
              state++;
            }
            catch (IOException e)
            {
              throw(e);
            }
            break;



          case FILEREQ_STATE_WRITE_HTTP_HEADER:

            if (fis != null)
            {
              fis.close();
            }

            //
            //  Open an input stream to the file.
            //
            try
            {
              fis = new FileInputStream(file);
            }
            catch (IOException e)
            {
              last_error = -1;
              error_text = "Error opening file input stream.";
              throw(e);
            }

            //
            //  Send the HTTP header.
            //
            int ippSz = ipp.sizeInBytes() + (int)file.length();
            switch( http.writeHeader( http.path, ippSz ))
            {
              case IPPHttp.HTTP_FORBIDDEN:
              case IPPHttp.HTTP_NOT_FOUND:
              case IPPHttp.HTTP_BAD_REQUEST:
              case IPPHttp.HTTP_METHOD_NOT_ALLOWED:
              case IPPHttp.HTTP_PAYMENT_REQUIRED:
              case IPPHttp.HTTP_UPGRADE_REQUIRED:
              case IPPHttp.HTTP_ERROR:
              case IPPHttp.HTTP_UNAUTHORIZED:
                     errors++;
                     if (errors < 5)
                     {
                       http.reConnect();
                     }
                     else
                       return(false);
                     break;

              default: state++;
            }
            break;



          case FILEREQ_STATE_WRITE_IPP_HEADER:
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
            if (http.checkForResponse() >= IPPHttp.HTTP_BAD_REQUEST)
            {
               errors++;
               if (errors < 5)
               {
                 http.reConnect();
                 state = FILEREQ_STATE_WRITE_HTTP_HEADER;
               }
               else
                 return(false);
            }
            else state++;
            break;


          case FILEREQ_STATE_WRITE_IPP_ATTRS:
            //
            //  Send the attributes list.
            //
            byte[]  bytes;
            int     sz;
            int     last_group = -1;
            boolean auth_error = false;
            for (int i=0; i < ipp.attrs.size() && !auth_error; i++)
            {
              attr = (IPPAttribute)ipp.attrs.get(i);
              sz    = attr.sizeInBytes(last_group);
              bytes = attr.getBytes(sz,last_group);
              last_group = attr.group_tag;
              http.write(bytes);

              //
              //  Check for server response between each attribute.
              //
              if (http.checkForResponse() >= IPPHttp.HTTP_BAD_REQUEST)
              {
                 errors++;
                 if (errors < 5)
                 {
                   http.reConnect();
                   state = FILEREQ_STATE_WRITE_HTTP_HEADER;
                   auth_error = true;
                 }
                 else
                   return(false);
              }

            }
            if (!auth_error)
              state++;
            break;



          case FILEREQ_STATE_FINISH_IPP_ATTRS:
            //
            //  Send the end of attributes tag.
            //
            byte[] footer = new byte[1];
            footer[0] = (byte)IPPDefs.TAG_END; 
            http.write( footer );

            //
            //  Keep checking .....
            //
            if (http.checkForResponse() >= IPPHttp.HTTP_BAD_REQUEST)
            {
              errors++;
              if (errors < 5)
              {
                   http.reConnect();
                   state = FILEREQ_STATE_WRITE_HTTP_HEADER;
              }
              else
                return(false);
            }
            else state++;
            break;



          case FILEREQ_STATE_WRITE_FILE_DATA:
            //
            //  Send the file data - this could be improved on ALOT.
            //
            int    count; 
            byte[] b      = new byte[1024];
            while ((state == FILEREQ_STATE_WRITE_FILE_DATA) &&
                   ((count = fis.read(b)) != -1))
            {
                //
                //  Keep checking .....
                //
                if (http.checkForResponse() >= IPPHttp.HTTP_BAD_REQUEST)
                {
                  errors++;
                  if (errors < 5)
                  {
                    http.reConnect();
                    state = FILEREQ_STATE_WRITE_HTTP_HEADER;
                  }
                  else
                  {
                    return(false);
                  }
                }
                else
                {
                  if (count > 0)
                    http.write( b, count );
                }

            } // while 

            if (state == FILEREQ_STATE_WRITE_FILE_DATA)
            {
                fis.close();
                fis = null;
                state++;
            }
            break;



          case FILEREQ_STATE_READ_RESPONSE:
            //
            //   Now read back response
            //
            int read_length;
            read_length = http.read_header();
            switch( http.status )
            {
               case IPPHttp.HTTP_OK: 
                 break;

               case IPPHttp.HTTP_UNAUTHORIZED: 
                    http.reConnect();
                    state = FILEREQ_STATE_WRITE_HTTP_HEADER;
                    errors = 0;
                 break;

               default:
                  errors++;
                  if (errors < 5)
                  {
                    http.reConnect();
                    state = FILEREQ_STATE_WRITE_HTTP_HEADER;
                  }
                  else 
                  {
                    return(false);
                  }
                  break;
            }

            if ((read_length > 0) && (state == FILEREQ_STATE_READ_RESPONSE))
            {
              http.read_buffer = http.read(read_length);
              ipp = http.processResponse();
              state++;
            }
            break;

          case FILEREQ_STATE_DONE:
            //
            //  success.
            //
            http.conn.close();
            http = null;
            return(true);
        }
      }

    }  // End of doRequest(file)







    /**
     * Get a list of jobs.
     * 
     * @param	<code>showMyJobs</code>		Show only jobs for user.
     * @param   <code>showCompleted</code>      Show completed OR active jobs.
     *
     * @return	<code>CupsJob[]</code>		Array of job objects, or null.
     * @throw	<code>IOException</code>
     */
    public CupsJob[] cupsGetJobs( boolean showMyJobs, boolean showCompleted ) 
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


      ipp = new IPP();
      ipp.request = new IPPRequest( 1, (short)IPPDefs.GET_JOBS );

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
                            "printer-uri" );

      if (site != null)
        a.addString( "", site );  
      else
        a.addString( "", "ipp://localhost/jobs" );  // Default ...
      // a.dump_values();
      ipp.addAttribute(a);
            
      
      //
      //  Add the requesting user name
      //  **FIX** This should be fixed to use the user member.
      //
      a = new IPPAttribute( IPPDefs.TAG_OPERATION, IPPDefs.TAG_NAME,
                            "requesting-user-name" );
      a.addString( "", "root" );
      ipp.addAttribute(a);

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

      //
      //  Get ALL attributes - to get only listed ones,
      //  uncomment this and fill in req_attrs.
      //
      // a = new IPPAttribute( IPPDefs.TAG_OPERATION, IPPDefs.TAG_KEYWORD,
      //                      "requested-attributes" );
      // a.addStrings( "", req_attrs );
      // ipp.addAttribute(a);
      
      //
      //  Do the request and process the response.
      //
      if (doRequest("cupsGetJobs"))
      {


        //
        //   First we have to figure out how many jobs there are
        //   so we can create the array.
        //

        //
        //  Skip past leading attributes
        //
        int i = 0;
        int group_tag = -1;
        while ((i < ipp.attrs.size()) && (group_tag != IPPDefs.TAG_JOB))
        {
          a = (IPPAttribute)ipp.attrs.get(i);
          group_tag = a.group_tag;
          if (group_tag != IPPDefs.TAG_JOB)
            i++;
        }

        int num_jobs = 0;
        group_tag = IPPDefs.TAG_JOB;
        while ((i < ipp.attrs.size()) && (group_tag == IPPDefs.TAG_JOB))
        { 
          a = (IPPAttribute)ipp.attrs.get(i++);
          if ((a != null) && (a.name.compareTo("job-id") == 0))
            num_jobs++;
          // a.dump_values();
        }

        if (num_jobs < 1)
          return(null);


        //
        //  Now create the array of the proper size.
        //
        int n = 0;
        CupsJob[] jobs = new CupsJob[num_jobs];
        for (n=0; n < num_jobs; n++)
        {
          jobs[n] = new CupsJob();
        }




        //
        //  Skip past leading attributes
        //
        group_tag = -1;
        i = 0;
        while ((i < ipp.attrs.size()) && (group_tag != IPPDefs.TAG_JOB))
        {
          a = (IPPAttribute)ipp.attrs.get(i);
          group_tag = a.group_tag;
          if (group_tag != IPPDefs.TAG_JOB)
            i++;
        }

        //
        //  Now we actually fill the array with the job data.
        //
        n = 0;
        for (;i < ipp.attrs.size(); i++)
        {
          a = (IPPAttribute)ipp.attrs.get(i);

          if (a.group_tag == IPPDefs.TAG_ZERO) 
          {
             n++;
             continue;
          }
          else 
          {
            try
            {
              jobs[n].updateAttribute( a );
            }
            catch (ArrayIndexOutOfBoundsException e)
            {
              return(jobs);
            }
          }
        }
        return( jobs );
      }
      return(null);

    }  //  End of cupsGetJobs



    /**
     * Get a list of printers.
     *
     * @return	<code>String[]</code>		Array of printers, or null.
     * @throw	<code>IOException</code>
     */
    public String[] cupsGetPrinters() 
           throws IOException
    {

      IPPAttribute a;

      ipp = new IPP();

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
            

      if (doRequest("cupsGetPrinters"))
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




    /** 
     * Get default destination.
     *
     * @return	<code>String</code>	Name of default printer, or null.
     * @throw	<code>IOException</code>
     */
    public String cupsGetDefault() 
           throws IOException
    {

      IPPAttribute a;


      ipp = new IPP();
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
            

      if (doRequest("cupsGetDefault"))
      {
        if ((ipp == null) || (ipp.attrs == null))
          return(null);

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






    /** 
     *  Get printer attributes
     *
     * @param	<code>printer_name</code>	Name of printer to get info for.
     * @return	<code>List</code>	        List of attributes.
     * @throw	<code>IOException</code>
     *
     * @see	<code>CupsPrinter</code>
     */
    public List cupsGetPrinterAttributes( String printer_name ) 
           throws IOException
    {

      IPPAttribute a;

      ipp = new IPP();

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
      String p_uri = "ipp://" + address + ":" +
	     port + "/printers/" + printer_name;
      a.addString( "", p_uri );
      ipp.addAttribute(a);

      if (doRequest("cupsGetPrinterAttributes"))
      {
        return(ipp.attrs);
      }  

      return(null);

    }  // End of cupsGetPrinterAttributes



    
    /** 
     *  Print a file.
     *
     *  @param	<code>p_filename</code>		Path of file to print.
     *	@param	<code>p_attrs[]</code>		Array of print job attributes.
     *
     *  @return	<code>CupsJob</code>		Object with job info.
     *
     *  @throw	<code>IOException</code>
     *
     *  @see	<code>CupsJob</code>
     */
    public CupsJob cupsPrintFile( String p_filename,
                                  IPPAttribute p_attrs[] )
           throws IOException
    {

      CupsJob      job;
      IPPAttribute a;
      File         file;


      file = new File(p_filename);
      if (!file.exists())
      {
        last_error = -1;
        error_text = "File does not exist.";
        return(null);
      }

      if (!file.canRead())
      {
        last_error = -1;
        error_text = "File cannot be read.";
        return(null);
      }


      ipp = new IPP();
      //
      //  Fill in the required attributes
      //
      ipp.request = new IPPRequest( 1, (short)IPPDefs.PRINT_JOB );

      a = new IPPAttribute( IPPDefs.TAG_OPERATION, IPPDefs.TAG_CHARSET,
                            "attributes-charset" );
      a.addString( "", "iso-8859-1" );  
      ipp.addAttribute(a);

      // ------------
      a = new IPPAttribute( IPPDefs.TAG_OPERATION, IPPDefs.TAG_LANGUAGE,
                            "attributes-natural-language" );
      a.addString( "", "en" );  
      ipp.addAttribute(a);
            
      // ------------
      a = new IPPAttribute( IPPDefs.TAG_OPERATION, IPPDefs.TAG_URI,
                            "printer-uri" );
      a.addString( "", site + dest );  
      ipp.addAttribute(a);

      // ------------
      // **FIX**  Fix this later.
/*
      a = new IPPAttribute( IPPDefs.TAG_OPERATION, IPPDefs.TAG_NAME,
                            "requesting-user-name" );
      // a.addString( "", p_username );  
      a.addString( "", "root");  
      ipp.addAttribute(a);
*/

      // ------------
      a = new IPPAttribute( IPPDefs.TAG_OPERATION, IPPDefs.TAG_NAME,
                            "job-name" );
      a.addString( "", file.getName() );  
      ipp.addAttribute(a);

      if (p_attrs != null)
      {
        for (int i=0; i < p_attrs.length; i++)
        {
          a = p_attrs[i];
          ipp.addAttribute(a);
        }
      }

      if (doRequest(file))
      {
        job = new CupsJob();
        for (int i=0; i < ipp.attrs.size(); i++)
        {
           a = (IPPAttribute)ipp.attrs.get(i);
           job.updateAttribute(a);
        }
        return(job);

      }  // if doRequest ...

      return(null);

    }  // End of cupsPrintFile





    /**
     *  Cancel a job - send a job cancel request to the server.
     *
     * @param	<code>printer_name</code>	Destination.
     * @param	<code>p_job_id</code>		ID of job.
     * @param	<code>p_user_name</code>	Requesting user name.		
     *
     * @throw	<code>IOException</code>
     */
    public int cupsCancelJob( String printer_name,
                              int    p_job_id,
                              String p_user_name ) 
           throws IOException
    {

      IPPAttribute a;

      ipp = new IPP();

      //
      //  Fill in the required attributes
      //
      ipp.request = new IPPRequest( 1, (short)IPPDefs.CANCEL_JOB );

      a = new IPPAttribute( IPPDefs.TAG_OPERATION, IPPDefs.TAG_CHARSET,
                            "attributes-charset" );
      a.addString( "", "iso-8859-1" );  
      ipp.addAttribute(a);

      // ------------
      a = new IPPAttribute( IPPDefs.TAG_OPERATION, IPPDefs.TAG_LANGUAGE,
                            "attributes-natural-language" );
      a.addString( "", "en" );  
      ipp.addAttribute(a);
            
      // ------------
      a = new IPPAttribute( IPPDefs.TAG_OPERATION, IPPDefs.TAG_URI,
                            "printer-uri" );
      a.addString( "", site + dest );  
      ipp.addAttribute(a);
            
      // ------------
      a = new IPPAttribute( IPPDefs.TAG_OPERATION, IPPDefs.TAG_INTEGER,
                            "job-id" );
      a.addInteger( p_job_id );  
      ipp.addAttribute(a);
            
      // ------------
      a = new IPPAttribute( IPPDefs.TAG_OPERATION, IPPDefs.TAG_NAME,
                            "requesting-user-name" );
      a.addString( "", p_user_name );  
      ipp.addAttribute(a);

      if (doRequest("cupsCancelJob"))
      {
        for (int i=0; i < ipp.attrs.size(); i++)
        {
          a = (IPPAttribute)ipp.attrs.get(i);
          a.dump_values();
        }
        return(0);

      }  // if doRequest ...

      return(0);

    }  // End of cupsCancelJob




  public List cupsGetPrinterStatus(String printer_name) 
	throws IOException
  {
      IPPAttribute a;
      String       p_uri;

      ipp = new IPP();

      //
      //  Fill in the required attributes
      //
      ipp.request = new IPPRequest(1,(short)IPPDefs.GET_PRINTER_ATTRIBUTES);

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
      p_uri = "ipp://" + address + ":" + 
              port + "/printers/" + printer_name;  
      a.addString( "", p_uri );
      ipp.addAttribute(a);
            
      if (doRequest("cupsGetPrinterStatus"))
      {
        return(ipp.attrs);
      } 
      return(null);
  }





}  // End of Cups class

