
import java.awt.*;
import java.awt.event.*;
import javax.swing.*;
import java.net.URL;
import java.net.*;
import java.io.*;
import java.util.*;
import com.easysw.cups.*;

public class GLPjobList implements ActionListener 
{
    Cups        cups      = null;
    GridLayout  mainLayout = null;
    JPanel      errorPanel = null;
    JScrollPane jobPane   = null;

    public GLPjobList(CupsPrinter cp) 
    {
      load(cp);
    }

    // Constructor
    public void load(CupsPrinter cp) 
    {
        URL         u;
        CupsJob[]   jobs;
        int         num_jobs = 0;

        try
        {
          u = new URL("http://" + GLPvars.cupsServerName + ":631/printers/" +
                      cp.getPrinterName());
          cups = new Cups(u);
          jobs = cups.cupsGetJobs(GLPvars.showMyJobs, 
                                  GLPvars.showCompletedJobs );
        }
        catch (IOException e)
        {
          GLPjobTableModel tm = new GLPjobTableModel(1,1);
          tm.setValueAt("Error getting job list(IOException)",0,0);
          JTable table = new JTable(tm);
          jobPane = new JScrollPane(table);
          jobPane.setBackground(GLPcolors.backgroundColor);
          return;
        }

        if (jobs == null)
        {
          String job_user = "";
          String job_type = ""; 
          if (GLPvars.showCompletedJobs)
            job_type = "No completed jobs";
          else
            job_type = "No active jobs";
          if (GLPvars.showMyJobs)
             job_user = " for " + GLPvars.cupsUser; 

          GLPjobTableModel tm = new GLPjobTableModel(1,1);
          tm.setValueAt(job_type + job_user + ".",0,0);
          JTable table = new JTable(tm);
          jobPane = new JScrollPane(table);
          jobPane.setBackground(GLPcolors.backgroundColor);
          return;
        }

        num_jobs = jobs.length;
        int jobcount = 0;
        for (int i=0; i < num_jobs; i++)
        {
          if (jobs[i].job_id < 0)
            continue;
          jobcount++;
        }

        if (jobcount < 1)
        {
          GLPjobTableModel tm = new GLPjobTableModel(1,1);
          String comp_str = "active";
          if (GLPvars.showCompletedJobs)
            comp_str = "completed";

          tm.setValueAt("No " + comp_str + " jobs on " + 
                        cp.getPrinterName(),0,0);
          JTable table = new JTable(tm);
          jobPane = new JScrollPane(table);
          jobPane.setBackground(GLPcolors.backgroundColor);
          return;
        }

        GLPjobTableModel tm = new GLPjobTableModel(jobcount,6);
        tm.setColumnName(0,"ID");
        tm.setColumnName(1,"Name");
        tm.setColumnName(2,"User");
        tm.setColumnName(3,"Create Time");
        tm.setColumnName(4,"Size");
        tm.setColumnName(5,"Status");
       
        String szString;
        Date   date = new Date();
        int currjob = 0;
        for (int i=0; i < num_jobs; i++)
        {
          //
          //  Bug in cupsGetJobs?
          //
          if (jobs[i].job_id < 0)
            continue;

          tm.setValueAt( new Integer( jobs[i].job_id), currjob, 0 );
          tm.setValueAt( (Object)jobs[i].job_name, currjob, 1 );
          tm.setValueAt( (Object)jobs[i].job_originating_user_name,currjob,2);
          
          date.setTime(jobs[i].time_at_creation * 1000);
          tm.setValueAt( date.toString(), currjob, 3 );

          if (jobs[i].job_k_octets < 1000)
            szString = Integer.toString(jobs[i].job_k_octets) + "k";
          else
            szString = Double.toString((float)jobs[i].job_k_octets / 1000.0) + "mb";
          tm.setValueAt( szString, currjob, 4 );
          tm.setValueAt( jobs[i].jobStatusText(), currjob, 5 );
          currjob++;
        }

        JTable table = new JTable( tm );

        jobPane = new JScrollPane(table);
        jobPane.setBackground(GLPcolors.backgroundColor);
        jobPane.setVerticalScrollBarPolicy(JScrollPane.VERTICAL_SCROLLBAR_ALWAYS);
        jobPane.setHorizontalScrollBarPolicy(JScrollPane.HORIZONTAL_SCROLLBAR_AS_NEEDED);
    }

    public void actionPerformed(ActionEvent e) 
    {
     //   if (e.getActionCommand().equals(maskFieldString)) 
     //   {
     //   } 
    }


    public JScrollPane getPanel()
    {
      return(jobPane);
    }

}
