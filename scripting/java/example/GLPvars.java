
import java.util.*;
import javax.swing.*;
import com.easysw.cups.*;

public class GLPvars
{
    //  Current selected server name or address.
    public    static String       cupsServerName = null;

    // Current selected printer name.
    public    static String       selectedPrinterName = null;

    // Current user name and password.
    public    static String       cupsUser       = "root";
    public    static String       cupsPasswd	 = "";

    //  So we can access the tabs from other classes.
    public    static JTabbedPane  mainGLPPanel   = null;
    public    static GLPtabs      tabs           = null;

    //  List of servers found using search.
    protected static List         serverList     = null;

    // So we can update the search results from the search classes.
    public static GLPjobTableModel searchTM = null;
    public static JTable           searchTable = null;

    // What kind of jobs to list.
    public    static boolean      showMyJobs        = false;
    public    static boolean      showCompletedJobs = false;

    // Constructor
    public GLPvars() 
    {
      cupsServerName = "localhost";
      serverList = new ArrayList();
    }

    public static void init()
    {
      cupsServerName = "localhost";
      serverList = new ArrayList();
    }


    public static String getServerName()
    {
       return(cupsServerName);
    }

    public static void setServerName( String name )
    {
       cupsServerName = name;
    }


    //
    //  Reset the server list.
    //
    public static void clearServerList()
    {
      serverList = null;
    }


    //
    //  Add a cups server to the server list.
    //
    public static void addToServerList( String serverName )
    {
       if (serverList != null)
         serverList.add(serverName);
    }

    //
    //  Get the full server list (if any).
    //
    public static String[] getServerList()
    {
      if (serverList != null)
      {
        String[] servers = new String[serverList.size()];
        for (int i=0; i < serverList.size(); i++)
          servers[i] = (String)serverList.get(i);
        return(servers);
      }
      return(null);
    }



}
