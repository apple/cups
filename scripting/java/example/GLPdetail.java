
import java.awt.*;
import java.awt.event.*;
import javax.swing.*;
import java.net.URL;
import java.net.*;
import java.io.*;
import com.easysw.cups.*;

public class GLPdetail implements ActionListener 
{
    private JPanel  mainPanel     = null;
    private JPanel  controlPanel  = null;
    private JPanel  detailPanel   = null;
    private JScrollPane  jobPanel = null;
    private BoxLayout mainLayout  = null;

    private JLabel  tmp           = null;

    private JButton completedButton = null;
    private JButton myJobsButton    = null;
    private JButton printFileButton = null;

    private String completedText = "Show Completed";
    private String myJobsText    = "Show My Jobs";
    private String printFileText = "Print a file";

    private GLPprinterDetail detail = null;
    private GLPjobList       joblist = null;
    private CupsPrinter      printer = null;
    private Cups             cups   = null;


    public GLPdetail()
    {
      URL           u;

      mainPanel = new JPanel();
      mainPanel.setBackground(GLPcolors.backgroundColor);

      //  Create the buttons panel
      controlPanel = new JPanel();
      controlPanel.setLayout(new GridLayout(1,3,2,2));
      controlPanel.setBorder(BorderFactory.createEmptyBorder(5,5,5,5));
      controlPanel.setBackground(GLPcolors.backgroundColor);
      completedButton = new JButton(completedText);
      myJobsButton    = new JButton(myJobsText);
      printFileButton = new JButton(printFileText);
   
      completedButton.setActionCommand(completedText);
      completedButton.addActionListener(this); 

      myJobsButton.setActionCommand(myJobsText);
      myJobsButton.addActionListener(this); 

      printFileButton.setActionCommand(printFileText);
      printFileButton.addActionListener(this); 

      completedButton.setBackground(GLPcolors.buttonBackgroundColor);
      completedButton.setForeground(GLPcolors.buttonForegroundColor);

      myJobsButton.setBackground(GLPcolors.buttonBackgroundColor);
      myJobsButton.setForeground(GLPcolors.buttonForegroundColor);

      printFileButton.setBackground(GLPcolors.buttonBackgroundColor);
      printFileButton.setForeground(GLPcolors.buttonForegroundColor);

      controlPanel.add(completedButton);
      controlPanel.add(myJobsButton);
      controlPanel.add(printFileButton);

      //  -----------------------------------------------------------
      //
      //  Now get the printer objects
      //
      if (GLPvars.selectedPrinterName != null)
      {
        try
        {
            u    = new URL("http://" + GLPvars.getServerName() + 
                           ":631/printers/" + GLPvars.selectedPrinterName );
            cups = new Cups(u);
            cups.setUser(GLPvars.cupsUser);
            cups.setPasswd(GLPvars.cupsPasswd);
            printer = new CupsPrinter( cups, GLPvars.selectedPrinterName );

            detail = new GLPprinterDetail( printer );
            joblist = new GLPjobList(printer);

            detailPanel = detail.getPanel();
            jobPanel    = joblist.getPanel();

            mainLayout = new BoxLayout(mainPanel, BoxLayout.Y_AXIS);
            mainPanel.setLayout(mainLayout);
            mainPanel.add(detailPanel);
            mainPanel.add(controlPanel);
            mainPanel.add(jobPanel);
        }
        catch (IOException e)
        {
          tmp = new JLabel("Error loading printer: " + GLPvars.selectedPrinterName);
          mainPanel.add(tmp);
          return;
        }
      }
      else
      {
        tmp = new JLabel("No printer selected.");
        mainPanel.add(tmp);
      }


    } // 



    public void topDetail()
    {
      URL           u;

      mainPanel = new JPanel();
      mainPanel.setBackground(GLPcolors.backgroundColor);

      //  Create the buttons panel
      controlPanel = new JPanel();
      controlPanel.setLayout(new GridLayout(1,3,2,2));
      controlPanel.setBorder(BorderFactory.createEmptyBorder(5,5,5,5));
      controlPanel.setBackground(GLPcolors.backgroundColor);
      completedButton = new JButton();
      myJobsButton    = new JButton();
      printFileButton = new JButton(printFileText);
   
      if (GLPvars.showCompletedJobs)
      {
        completedButton.setText("Show Active");
        completedButton.setActionCommand("Show Active");
      }
      else
      {
        completedButton.setText("Show Completed");
        completedButton.setActionCommand("Show Completed");
      }
      completedButton.addActionListener(this); 

      if (GLPvars.showMyJobs)
      {
        myJobsButton.setText("Show All Users");
        myJobsButton.setActionCommand("Show All Users");
      }
      else
      {
        myJobsButton.setText("Show My Jobs");
        myJobsButton.setActionCommand("Show My Jobs");
      }
      myJobsButton.addActionListener(this); 

      printFileButton.setActionCommand(printFileText);
      printFileButton.addActionListener(this); 
   
      completedButton.setBackground(GLPcolors.buttonBackgroundColor);
      completedButton.setForeground(GLPcolors.buttonForegroundColor);

      myJobsButton.setBackground(GLPcolors.buttonBackgroundColor);
      myJobsButton.setForeground(GLPcolors.buttonForegroundColor);

      printFileButton.setBackground(GLPcolors.buttonBackgroundColor);
      printFileButton.setForeground(GLPcolors.buttonForegroundColor);

      controlPanel.add(completedButton);
      controlPanel.add(myJobsButton);
      controlPanel.add(printFileButton);

      //  -----------------------------------------------------------
      //
      //  Now get the printer objects
      //
      if (GLPvars.selectedPrinterName != null)
      {
        try
        {
            u    = new URL("http://" + GLPvars.getServerName() + 
                           ":631/printers/" + GLPvars.selectedPrinterName );  
            cups = new Cups(u);
            cups.setUser(GLPvars.cupsUser);
            cups.setPasswd(GLPvars.cupsPasswd);
            printer = new CupsPrinter( cups, GLPvars.selectedPrinterName );

            detail  = new GLPprinterDetail( printer );
            joblist = new GLPjobList(printer);

            detailPanel = detail.getPanel();

            jobPanel    = joblist.getPanel();

            mainLayout = new BoxLayout(mainPanel, BoxLayout.Y_AXIS);
            mainPanel.setLayout(mainLayout);
            mainPanel.add(detailPanel);
            mainPanel.add(controlPanel);
            mainPanel.add(jobPanel);
        }
        catch (IOException e)
        {
          tmp = new JLabel("Error loading printer: " + GLPvars.selectedPrinterName);
          mainPanel.add(tmp);
          return;
        }
      }
      else
      {
        tmp = new JLabel("No printer selected.");
        mainPanel.add(tmp);
      }
    }

    public JPanel getPanel()
    {
      return(mainPanel);
    }

    public void actionPerformed(ActionEvent e)
    {
      // String source = e.getActionCommand();
      Object source = e.getSource();
      if (source == completedButton)
      {
        if (GLPvars.showCompletedJobs)
        {
          GLPvars.showCompletedJobs = !GLPvars.showCompletedJobs;
          completedButton.setText("Show Active");
          completedButton.setActionCommand("Show Active");
          GLPvars.tabs.updateDetailPanel();
          GLPvars.tabs.tabPanel.setSelectedIndex(2);
        }
        else
        {
          completedButton.setText("Show Completed");
          completedButton.setActionCommand("Show Completed");
          GLPvars.showCompletedJobs = !GLPvars.showCompletedJobs;
          GLPvars.tabs.updateDetailPanel();
          GLPvars.tabs.tabPanel.setSelectedIndex(2);
        }
      }
      else if (source == myJobsButton)
      {
        if (GLPvars.showMyJobs)
        {
          GLPvars.showMyJobs = !GLPvars.showMyJobs;
          myJobsButton.setText("Show All Users");
          myJobsButton.setActionCommand("Show All Users");
          GLPvars.tabs.updateDetailPanel();
          GLPvars.tabs.tabPanel.setSelectedIndex(2);
        }
        else
        {
          GLPvars.showMyJobs = !GLPvars.showMyJobs;
          myJobsButton.setText("Show My Jobs");
          myJobsButton.setActionCommand("Show My Jobs");
          GLPvars.tabs.updateDetailPanel();
          GLPvars.tabs.tabPanel.setSelectedIndex(2);
        }
      }
      else if (source == printFileButton)
      {
        if (printer != null)
        {
          GLPvars.tabs.updateOptionsPanel(printer);
          GLPvars.tabs.tabPanel.setSelectedIndex(3);
        }
      }
    }

}

