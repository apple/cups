
import java.util.*;

class IPPCalendar extends GregorianCalendar
{
  public long getTimeInMillis()
  {
    return(super.getTimeInMillis());
  }

  public int getUnixTime()
  {
    return( (int)(getTimeInMillis() / 1000) );
  }

}
