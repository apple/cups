
import java.io.*;
import java.util.*;
import java.net.*;

public class IPPHttp
{

  //
  //  Class constants.
  //

  public static final int HTTP_WAITING = 0x00;
  public static final int HTTP_OPTIONS = 0x01;
  public static final int HTTP_GET = 0x02;
  public static final int HTTP_GET_SEND = 0x03;
  public static final int HTTP_HEAD = 0x04;
  public static final int HTTP_POST = 0x05;
  public static final int HTTP_POST_RECV = 0x06;
  public static final int HTTP_POST_SEND = 0x07;
  public static final int HTTP_PUT = 0x08;
  public static final int HTTP_PUT_RECV = 0x09;
  public static final int HTTP_DELETE = 0x0A;
  public static final int HTTP_TRACE = 0x0B;
  public static final int HTTP_CLOSE = 0x0C;
  public static final int HTTP_STATUS = 0x0D;


  public static final int HTTP_0_9 = 0x09;
  public static final int HTTP_1_0 = 0x64;
  public static final int HTTP_1_1 = 0x65;


  public static final int HTTP_KEEPALIVE_OFF = 0x00;
  public static final int HTTP_KEEPALIVE_ON = 0x01;

  public static final int HTTP_ENCODE_LENGTH = 0x00;
  public static final int HTTP_ENCODE_CHUNKED = 0x01;


  public static final int HTTP_ENCRYPT_IF_REQUESTED = 0x00;
  public static final int HTTP_ENCRYPT_NEVER = 0x01;
  public static final int HTTP_ENCRYPT_REQUIRED = 0x02;
  public static final int HTTP_ENCRYPT_ALWAYS = 0x03;


  public static final int HTTP_AUTH_NONE = 0x00;
  public static final int HTTP_AUTH_BASIC = 0x01;
  public static final int HTTP_AUTH_MD5 = 0x02;
  public static final int HTTP_AUTH_MD5_SESS = 0x03;
  public static final int HTTP_AUTH_MD5_INT = 0x04;
  public static final int HTTP_AUTH_MD5_SESS_INT = 0x05;


  public static final int HTTP_ERROR = 0xFFFFFFFF;
  public static final int HTTP_CONTINUE = 0x64;
  public static final int HTTP_SWITCHING_PROTOCOLS = 0x65;
  public static final int HTTP_OK = 0xC8;
  public static final int HTTP_CREATED = 0xC9;
  public static final int HTTP_ACCEPTED = 0xCA;
  public static final int HTTP_NOT_AUTHORITATIVE = 0xCB;
  public static final int HTTP_NO_CONTENT = 0xCC;
  public static final int HTTP_RESET_CONTENT = 0xCD;
  public static final int HTTP_PARTIAL_CONTENT = 0xCE;
  public static final int HTTP_MULTIPLE_CHOICES = 0x12C;
  public static final int HTTP_MOVED_PERMANENTLY = 0x12D;
  public static final int HTTP_MOVED_TEMPORARILY = 0x12E;
  public static final int HTTP_SEE_OTHER = 0x12F;
  public static final int HTTP_NOT_MODIFIED = 0x130;
  public static final int HTTP_USE_PROXY = 0x131;
  public static final int HTTP_BAD_REQUEST = 0x190;
  public static final int HTTP_UNAUTHORIZED = 0x191;
  public static final int HTTP_PAYMENT_REQUIRED = 0x192;
  public static final int HTTP_FORBIDDEN = 0x193;
  public static final int HTTP_NOT_FOUND = 0x194;
  public static final int HTTP_METHOD_NOT_ALLOWED = 0x195;
  public static final int HTTP_NOT_ACCEPTABLE = 0x196;
  public static final int HTTP_PROXY_AUTHENTICATION = 0x197;
  public static final int HTTP_REQUEST_TIMEOUT = 0x198;
  public static final int HTTP_CONFLICT = 0x199;
  public static final int HTTP_GONE = 0x19A;
  public static final int HTTP_LENGTH_REQUIRED = 0x19B;
  public static final int HTTP_PRECONDITION = 0x19C;
  public static final int HTTP_REQUEST_TOO_LARGE = 0x19D;
  public static final int HTTP_URI_TOO_LONG = 0x19E;
  public static final int HTTP_UNSUPPORTED_MEDIATYPE = 0x19F;
  public static final int HTTP_UPGRADE_REQUIRED = 0x1AA;
  public static final int HTTP_SERVER_ERROR = 0x1F4;
  public static final int HTTP_NOT_IMPLEMENTED = 0x1F5;
  public static final int HTTP_BAD_GATEWAY = 0x1F6;
  public static final int HTTP_SERVICE_UNAVAILABLE = 0x1F7;
  public static final int HTTP_GATEWAY_TIMEOUT = 0x1F8;


  public static final int HTTP_NOT_SUPPORTED = 0x1F9;


  public static final int HTTP_FIELD_UNKNOWN = 0xFFFFFFFF;
  public static final int HTTP_FIELD_ACCEPT_LANGUAGE = 0x00;
  public static final int HTTP_FIELD_ACCEPT_RANGES = 0x01;
  public static final int HTTP_FIELD_AUTHORIZATION = 0x02;
  public static final int HTTP_FIELD_CONNECTION = 0x03;
  public static final int HTTP_FIELD_CONTENT_ENCODING = 0x04;
  public static final int HTTP_FIELD_CONTENT_LANGUAGE = 0x05;
  public static final int HTTP_FIELD_CONTENT_LENGTH = 0x06;
  public static final int HTTP_FIELD_CONTENT_LOCATION = 0x07;
  public static final int HTTP_FIELD_CONTENT_MD5 = 0x08;
  public static final int HTTP_FIELD_CONTENT_RANGE = 0x09;
  public static final int HTTP_FIELD_CONTENT_TYPE = 0x0A;
  public static final int HTTP_FIELD_CONTENT_VERSION = 0x0B;
  public static final int HTTP_FIELD_DATE = 0x0C;
  public static final int HTTP_FIELD_HOST = 0x0D;
  public static final int HTTP_FIELD_IF_MODIFIED_SINCE = 0x0E;
  public static final int HTTP_FIELD_IF_UNMODIFIED_SINCE = 0x0F;
  public static final int HTTP_FIELD_KEEP_ALIVE = 0x10;
  public static final int HTTP_FIELD_LAST_MODIFIED = 0x11;
  public static final int HTTP_FIELD_LINK = 0x12;
  public static final int HTTP_FIELD_LOCATION = 0x13;
  public static final int HTTP_FIELD_RANGE = 0x14;
  public static final int HTTP_FIELD_REFERER = 0x15;
  public static final int HTTP_FIELD_RETRY_AFTER = 0x16;
  public static final int HTTP_FIELD_TRANSFER_ENCODING = 0x17;
  public static final int HTTP_FIELD_UPGRADE = 0x18;
  public static final int HTTP_FIELD_USER_AGENT = 0x19;
  public static final int HTTP_FIELD_WWW_AUTHENTICATE = 0x1A;
  public static final int HTTP_FIELD_MAX = 0x1B;

  public static final String http_fields[] =
                      {
			  "Accept-Language",
			  "Accept-Ranges",
			  "Authorization",
			  "Connection",
			  "Content-Encoding",
			  "Content-Language",
			  "Content-Length",
			  "Content-Location",
			  "Content-MD5",
			  "Content-Range",
			  "Content-Type",
			  "Content-Version",
			  "Date",
			  "Host",
			  "If-Modified-Since",
			  "If-Unmodified-since",
			  "Keep-Alive",
			  "Last-Modified",
			  "Link",
			  "Location",
			  "Range",
			  "Referer",
			  "Retry-After",
			  "Transfer-Encoding",
			  "Upgrade",
			  "User-Agent",
			  "WWW-Authenticate"
			};
  public static final String days[] =
                      {
			  "Sun",
			  "Mon",
			  "Tue",
			  "Wed",
			  "Thu",
			  "Fri",
			  "Sat"
			};
  public static final String months[] =
			{
			  "Jan",
			  "Feb",
			  "Mar",
			  "Apr",
			  "May",
			  "Jun",
		          "Jul",
			  "Aug",
			  "Sep",
			  "Oct",
			  "Nov",
			  "Dec"
			};



  //
  //  Private class members.
  //
  private URL                 url;
  public Socket               conn;
  public BufferedInputStream  is;
  public BufferedReader       br;
  public BufferedOutputStream os;

  public int                  write_content_length;
  private char                write_buffer[];
  private int                 write_buffer_head;
  private int                 write_buffer_tail;

  public String               read_header_date;
  public String               read_header_server;
  public String               read_header_charset;
  public String               read_header_content_language;
  public String               read_header_content_type;
  public int                  read_header_content_length;

  public char                 read_buffer[];
  private int                 read_buffer_head;
  private int                 read_buffer_tail;

  public int                  error;
  public int                  activity;

  public String               hostname;
  public int                  port;
  public String               path;



  public IPPHttp(String s)
        throws IOException, UnknownHostException
  {
    try
    {
      url      = new URL(s);
      hostname = url.getHost();
      port     = url.getPort();
      path     = url.getPath();

      conn     = new Socket(hostname, port);
      is       = new BufferedInputStream(new DataInputStream(conn.getInputStream()));
      os       = new BufferedOutputStream(new DataOutputStream(conn.getOutputStream()));
    }
    catch(UnknownHostException unknownhostexception)
    {
            throw unknownhostexception;
    }
    catch(IOException ioexception)
    {
            throw ioexception;
    }
  }

  public void writeHeader(String s, int i)
        throws IOException
  {
    try
    {
      String s1 = "POST " + s + " HTTP/1.0\r\n";
      os.write(s1.getBytes(), 0, s1.length());
      s1 = "Content-type: application/ipp\r\n";
      os.write(s1.getBytes(), 0, s1.length());
      s1 = "Content-length: " + i + "\r\n\r\n";
      os.write(s1.getBytes(), 0, s1.length());
      os.flush();
    }
    catch(IOException ioexception)
    {
            throw ioexception;
    }
  }

  public void write(byte abyte0[])
        throws IOException
  {
    try
    {
            os.write(abyte0, 0, abyte0.length);
            os.flush();
    }
    catch(IOException ioexception)
    {
            throw ioexception;
    }
  }



  public int read_header()
        throws IOException
  {
    boolean done = false;
    read_header_content_length = 0;

    String s; 
    while (!done)
    {
        s = read_line();
        // System.out.println( s );

        if (s.startsWith("Content-Length:"))
        {
          String s2 = s.substring(15);
          read_header_content_length = Integer.parseInt(s2.trim(), 10);
        }
        else if (s.startsWith("Content-Language:"))
        {
          String s3 = s.substring(17);
          read_header_content_language = s3.trim();
        } 
        else if (s.startsWith("Server:"))
        {
          String s4 = s.substring(7);
          read_header_server = s4.trim();
        }
        else if (s.startsWith("Date:"))
        {
          String s5 = s.substring(5);
          read_header_date = s5.trim();
        } 
        else if (s.length() == 0)
        {
          done = true;
          return( read_header_content_length );
        }
    }
    return 0;
  }




  public String read_line()
        throws IOException
  {
    StringBuffer sb = new StringBuffer();
    int          c = 0;

    try
    {
      while ((c != -1) && (c != 10))
      {
        c = is.read();
        switch( c )
        {
          case -1:
          case 10:
          case 13:
                 break;
          default: sb.append((char)c);
        }
      }
    }
    catch (IOException e)
    {
      throw(e);
    }
    return(sb.toString());
  }





  public char[] read(int i)
        throws IOException
  {
    char ac[] = new char[i];
    int j = 0;
    try
    {
      for (int k = is.read(); k != -1 && j < i; k = is.read())
      {
        ac[j++] = (char)k;
      }
    }
    catch(IOException ioexception)
    {
      throw ioexception;
    }
    return ac;
  }


  public IPP processResponse()
  {
    IPP              ipp;
    IPPAttribute     attr;       // temp attribute 
    IPPValue         val;        // temp value

    short            vtag;       //  Current value tag 
    short            gtag;       //  Current group tag

    char[]           buffer;

    ipp = new IPP();
    ipp.request = new IPPRequest();

    int read_buffer_bytes     = read_buffer.length;
    int read_buffer_remaining = read_buffer_bytes;
    int bufferidx             = 0;
    int n;


    ipp.current = -1;   // current attritue??
    ipp.last    = -1;   //  last attr?
    attr        = null;
    buffer      = read_buffer;
    gtag        = -1;
    vtag        = -1;

    ipp.state  = IPPDefs.IDLE;
    while ((ipp.state != IPPDefs.TAG_END) &&
           (read_buffer_remaining > 0))
    {
      switch (ipp.state)
      {
        case IPPDefs.IDLE :
            ipp.state++;         /* Avoid common problem... */

        //
        // Get the request header...
        //
        case IPPDefs.HEADER :
          if (read_buffer_remaining < 8)
	  {
	    // System.out.println("\nippRead: Unable to read header!\n" );
	    return (null);
	  }

          //
          //  Verify the major version number...
	  //
	  if (buffer[0] != (char)1)
	  {
	    // System.out.println("ippRead: version number is bad: " 
            //                  + (int)buffer[0] + "." + (int)buffer[1] + "\n");
	    return (null);
	  }

          //
          //  Then copy the request header over...
  	  //
          ipp.request.version[0]  = buffer[bufferidx++];
          ipp.request.version[1]  = buffer[bufferidx++];
          ipp.request.op_status   = (short)((short)buffer[bufferidx] << 8) | 
                                             (short)buffer[bufferidx+1];
          bufferidx += 2;
          ipp.request.request_id  = (int)((int)buffer[bufferidx] << 24) | 
                                         ((int)buffer[bufferidx+1] << 16) |
	                                 ((int)buffer[bufferidx+2] << 8) | 
                                         ((int)buffer[bufferidx+3]);
          bufferidx += 4;
          read_buffer_remaining -= 8;

          ipp.state   = IPPDefs.ATTRIBUTE;
	  ipp.current = -1;
	  ipp.current_tag  = IPPDefs.TAG_ZERO;

          // System.out.println("ippRead: version=" + buffer[0], buffer[1]));
	  // System.out.println("ippRead: op_status=" + ipp.request.op_status);
          // System.out.println("ippRead: request_id=" + ipp.request.request_id);
          break;

      case IPPDefs.ATTRIBUTE :

          while (((short)buffer[bufferidx] > 0) &&
                 (read_buffer_remaining > 0))
          {
	    //
	    //  Read the value tag first.
	    //
            vtag = (short)buffer[bufferidx++];
            read_buffer_remaining--;

	    if (vtag == IPPDefs.TAG_END)
	    {
	      //
	      //  No more attributes left...
	      //

              // System.out.println("ippRead: IPP_TAG_END!");
	      ipp.state = IPPDefs.DATA;
              if (attr != null)
              {
                ipp.addAttribute(attr);
                attr = null;
              }
	      break;
	    }
            else if (vtag < IPPDefs.TAG_UNSUPPORTED_VALUE)
	    {
              if (attr != null)
              {
                ipp.addAttribute(attr);
                attr = null;
              }

              //
              // If we already have added attributes, need to add a
              // separater.
              //
              if (gtag == vtag)
              {
                attr = new IPPAttribute(IPPDefs.TAG_ZERO,IPPDefs.TAG_ZERO,"");
                ipp.addAttribute(attr);
                attr = null;
              }

	      //
	      //  Group tag...  Set the current group and continue...
	      //
              gtag = vtag;

	      ipp.current_tag  = gtag;
	      ipp.current = -1;
	      // System.out.println("ippRead: group tag = " +  tag);
	      continue;
	    }
            // System.out.println("ippRead: value tag = " + tag);

            //
	    // Get the name...
	    //
            n = ((int)buffer[bufferidx] << 8) | (int)buffer[bufferidx+1];
            bufferidx += 2;
            read_buffer_remaining -= 2;

            // System.out.println("ippRead: name length = " + n);

            if (n == 0)
	    {
              //
              //  Parse Error, can't add additional values to null attr
              //
              if (attr == null)
	        return (null);

	      //
	      //  More values for current attribute...
	      //

	      //
	      // Make sure we aren't adding a new value of a different
	      // type...
	      //

	      if (attr.value_tag == IPPDefs.TAG_STRING ||
                  (attr.value_tag >= IPPDefs.TAG_TEXTLANG &&
		   attr.value_tag <= IPPDefs.TAG_MIMETYPE))
              {
	        //
	        // String values can sometimes come across in different
	        // forms; accept sets of differing values...
	        //
	        if (vtag != IPPDefs.TAG_STRING &&
    	           (vtag < IPPDefs.TAG_TEXTLANG || vtag > IPPDefs.TAG_MIMETYPE))
	          return (null);
              }
	      else if (attr.value_tag != vtag)
	        return (null);
	    }
	    else
	    {
              if (attr != null)
              {
                ipp.addAttribute(attr);
                attr = null;
              }

              //
              // New Attribute
              //
              StringBuffer s = new StringBuffer();
              for (int i=0; i < n; i++)
              {
                s.append((char)buffer[bufferidx++]);
                read_buffer_remaining--;
              }
	      // System.out.println("ippRead: name = " + s );
              attr     = new IPPAttribute( gtag, vtag, s.toString() );
	    }
	    n = ((short)buffer[bufferidx] << 8) | (short)buffer[bufferidx+1];
            bufferidx += 2;
            read_buffer_remaining -= 2;
            // System.out.println("ippRead: value length = " + n );

	    switch (vtag)
	    {
	      case IPPDefs.TAG_INTEGER :
	      case IPPDefs.TAG_ENUM :
	  	n = (int)(((int)buffer[bufferidx] << 24) |
                          ((int)buffer[bufferidx+1] << 16) |
                          ((int)buffer[bufferidx+2] << 8) |
                          ((int)buffer[bufferidx+3]));
                bufferidx += 4;
                read_buffer_remaining -= 4;
                attr.addInteger( n );
	        break;

	      case IPPDefs.TAG_BOOLEAN :
                if ((byte)buffer[bufferidx++] > 0)
                  attr.addBoolean(true);
                else
                  attr.addBoolean(false);
                read_buffer_remaining --;
	        break;

	      case IPPDefs.TAG_TEXT :
	      case IPPDefs.TAG_NAME :
	      case IPPDefs.TAG_KEYWORD :
	      case IPPDefs.TAG_STRING :
	      case IPPDefs.TAG_URI :
	      case IPPDefs.TAG_URISCHEME :
	      case IPPDefs.TAG_CHARSET :
	      case IPPDefs.TAG_LANGUAGE :
	      case IPPDefs.TAG_MIMETYPE :
                StringBuffer s = new StringBuffer();
                for (int i=0; i < n; i++ )
                {
                  s.append( (char)buffer[bufferidx++] );
                  read_buffer_remaining --;
                }
		// System.out.println("ippRead: value = " + s );
                attr.addString( "", s.toString() );
	        break;


	      case IPPDefs.TAG_DATE :
                char db[] = new char[11];
                for (int i=0; i < 11; i++ )
                {
                  db[i] = (char)buffer[bufferidx++];
                  read_buffer_remaining --;
                }
                attr.addDate( db );
	        break;


	      case IPPDefs.TAG_RESOLUTION :
                if (read_buffer_remaining < 9)
                  return( null );

                int  x, y;
                byte u;
                x = (int)(((int)buffer[bufferidx] & 0xff000000) << 24) |
                         (((int)buffer[bufferidx+1] & 0x00ff0000) << 16) |
                         (((int)buffer[bufferidx+2] & 0x0000ff00) << 8) |
                         (((int)buffer[bufferidx+3] & 0x000000ff));
                bufferidx += 4;
                read_buffer_remaining -= 4;

                y = (int)(((int)buffer[bufferidx] & 0xff000000) << 24) |
                         (((int)buffer[bufferidx+1] & 0x00ff0000) << 16) |
                         (((int)buffer[bufferidx+2] & 0x0000ff00) << 8) |
                         (((int)buffer[bufferidx+3] & 0x000000ff));
                bufferidx += 4;
                read_buffer_remaining -= 4;

                u = (byte)buffer[bufferidx++];
                read_buffer_remaining--;
                attr.addResolution( u, x, y );         
	        break;

	      case IPPDefs.TAG_RANGE :
	        if (read_buffer_remaining < 8)
		  return (null);

                int lower, upper; 
                lower = (int)(((int)buffer[bufferidx] & 0xff000000) << 24) |
                         (((int)buffer[bufferidx+1] & 0x00ff0000) << 16) |
                         (((int)buffer[bufferidx+2] & 0x0000ff00) << 8) |
                         (((int)buffer[bufferidx+3] & 0x000000ff));
                bufferidx += 4;
                read_buffer_remaining -= 4;

                upper = (int)(((int)buffer[bufferidx] & 0xff000000) << 24) |
                         (((int)buffer[bufferidx+1] & 0x00ff0000) << 16) |
                         (((int)buffer[bufferidx+2] & 0x0000ff00) << 8) |
                         (((int)buffer[bufferidx+3] & 0x000000ff));
                bufferidx += 4;
                read_buffer_remaining -= 4;

                attr.addRange( (short)lower, (short)upper );         
	        break;

	      case IPPDefs.TAG_TEXTLANG :
	      case IPPDefs.TAG_NAMELANG :
	       //
	       // text-with-language and name-with-language are composite
	       // values:
	       //
	       // charset-length
	       // charset
	       // text-length
	       // text
	       //

		n = ((int)buffer[bufferidx] << 8) | (int)buffer[bufferidx+1];
                bufferidx += 2;

                StringBuffer cs = new StringBuffer();
                for (int i=0; i < n; i++ )
                {
                  cs.append( (char)buffer[bufferidx++] );
                  read_buffer_remaining --;
                }

		n = ((int)buffer[bufferidx] << 8) | (int)buffer[bufferidx+1];
                bufferidx += 2;

                StringBuffer tx = new StringBuffer();
                for (int i=0; i < n; i++ )
                {
                  tx.append( (char)buffer[bufferidx++] );
                  read_buffer_remaining --;
                }

                attr.addString( cs.toString(), tx.toString() );
	        break;


              default : /* Other unsupported values */
	        if (n > 0)
		{
                  bufferidx += n;
                  read_buffer_remaining -= n;
		}
	        break;
	    }
	  }  // End of while
          if (attr != null)
          {
            ipp.addAttribute(attr);
            attr = null;
          }
          break;

      case IPPDefs.DATA :
        break;

      default :
        break; /* anti-compiler-warning-code */
    }
  }
  return (ipp);
}





}  // End of IPPHttp class


