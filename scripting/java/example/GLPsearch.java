
import java.awt.*;
import java.awt.event.*;
import javax.swing.*;
import java.net.URL;
import java.net.*;
import java.io.*;
import com.easysw.cups.*;

public class GLPsearch extends Thread
{
    String       localHostName      = null;
    String       localHostIP        = null;
    String       localSubNet        = null;
    InetAddress  localHostAddress   = null;

    private      int current_octet  = 1;
    private      int count          = 0;
    private      int thread_num     = 0;
    private      boolean is_done    = false;
    private      boolean is_completed  = false;
      

    // Constructor
    public GLPsearch( int t_num ) 
    {
      thread_num = t_num;
      try
      {
        localHostAddress = InetAddress.getLocalHost();
      }
      catch (UnknownHostException e)
      {
      }
      localHostName    = localHostAddress.getHostName();
      // localHostIP      = localHostAddress.getHostAddress();
      localHostIP = "192.168.1.100";
      int i = localHostIP.lastIndexOf(".");
      localSubNet      = localHostIP.substring(0,i);
    }



    // Constructor
    public GLPsearch(int t_num, String subnet) 
    {
      thread_num = t_num;
      try
      {
        localHostAddress = InetAddress.getLocalHost();
      }
      catch (UnknownHostException e)
      {
      }
      localHostName    = localHostAddress.getHostName();
      localHostIP      = localHostAddress.getHostAddress();
      localSubNet      = subnet;
    }


    public void run()
    {
        Cups   cups = null;
        String host = "";
        String test = "";
        InetAddress  lookupAddress;
        URL    u    = null;

        is_done = false;
        for (int x = thread_num+1; x < 255 && !is_done; x += 8 )
        {
           count++;
           current_octet = x;
           host = localSubNet + "." + x;
           try
           {
             // System.out.println("Checking " + host + "  ...");
             u    = new URL("http://" + host + ":631/printers");
             cups = new Cups(u);
             test = cups.cupsGetDefault();
             if ((test != null) && (test.length() > 0))
             {
               lookupAddress = InetAddress.getByName(host);
               GLPvars.addToServerList(lookupAddress.getHostName());
             }
             else
             {
               // System.out.println(thread_num + ": No server at: " + host );
             }
           }
           catch (IOException e)
           {
             // System.out.println(thread_num + ": No server at: " + host );
           }
        }
        if (!is_done)
          is_completed = true;
        is_done = true;
    }

    public void interrupt()
    {
        is_done = true;
    }

    public boolean completed()
    {
        return(is_completed);
    }

    public boolean done()
    {
        return(is_done);
    }

    public int getValue()
    {
        return(count);
    }

}
