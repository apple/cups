
import java.util.*;
import java.io.*;
import java.net.*;

public class IPPURLConnection extends URLConnection
{

  public IPPURLConnection( URL url )
  {
    super(url);
  }

  public boolean usingProxy()
  {
    return(false);
  }

  public void connect()
  {
    return;
  }

  public void disconnect()
  {
    return;
  }

}
