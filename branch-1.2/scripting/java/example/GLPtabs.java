
import java.awt.*;
import java.awt.event.*;
import javax.swing.*;
import javax.swing.event.*;
import java.net.URL;
import java.net.*;
import java.io.*;
import com.easysw.cups.*;

public class GLPtabs extends JPanel
{
    JTabbedPane  tabPanel      = null;

    //
    //  Panels for the various tabs.
    //
    JPanel       serverPanel   = null;
    JScrollPane  printersPanel = null;
    JPanel       detailPanel   = null;
    JPanel       optionsPanel  = null;

    GLPserver     server   = null;
    GLPprinters   printers = null;
    GLPdetail     detail   = null;
    GLPoptions    options  = null;



    // Constructor
    public GLPtabs() 
    {
      tabPanel = new JTabbedPane();
      tabPanel.setBackground(Color.white);

      serverPanel = new JPanel();
      server = new GLPserver();
      serverPanel = server.getPanel();
      tabPanel.add("Server", serverPanel);
      tabPanel.setBackgroundAt(0,GLPcolors.tabBackgroundColor);
      tabPanel.setForegroundAt(0,GLPcolors.tabForegroundColor);

      printersPanel = new JScrollPane();
      printers = new GLPprinters();
      printersPanel = printers.getPanel();
      tabPanel.add( "Printers", printersPanel );
      tabPanel.setBackgroundAt(1,GLPcolors.tabBackgroundColor);
      tabPanel.setForegroundAt(1,GLPcolors.tabForegroundColor);


      detail = new GLPdetail();
      detailPanel  = detail.getPanel();
      tabPanel.add( "Destination", detailPanel );
      tabPanel.setBackgroundAt(2,GLPcolors.tabBackgroundColor);
      tabPanel.setForegroundAt(2,GLPcolors.tabForegroundColor);

      options         = new GLPoptions();
      optionsPanel    = new JPanel();
      optionsPanel.setBackground(GLPcolors.backgroundColor);
      tabPanel.add( "Options", optionsPanel );
      tabPanel.setBackgroundAt(3,GLPcolors.tabBackgroundColor);
      tabPanel.setForegroundAt(3,GLPcolors.tabForegroundColor);

      tabPanel.setSelectedIndex(0);

      tabPanel.addChangeListener(new ChangeListener()
               {
                 public void stateChanged( ChangeEvent e )
                 {
                   Object source = e.getSource();
                   if (!printers.cupsServerName.equals(GLPvars.getServerName()))
                   {
                     printers.load();
                     printersPanel = printers.getPanel();
                     tabPanel.setComponentAt(1,printersPanel);
                   }
                 }
               });

      JPanel jobsPanel     = new JPanel();
      JPanel filePanel     = new JPanel();
    }



    public void updateServerPanel(String s)
    {
      server.updateServer(s);
      serverPanel = server.getPanel();
      tabPanel.setComponentAt(0,serverPanel);
    }

    public void updateDetailPanel()
    {
      detail.topDetail();
      detailPanel = detail.getPanel();
      tabPanel.setComponentAt(2,detailPanel);
    }

    public void updateOptionsPanel(CupsPrinter cp)
    {
      options.updateOptions(cp);
      optionsPanel = options.getPanel();
      tabPanel.setComponentAt(3,optionsPanel);
    }

    public boolean updatePrintersTab()
    {
      return(true);
    }

    public JTabbedPane getPanel()
    {
      return(tabPanel);
    }

}
