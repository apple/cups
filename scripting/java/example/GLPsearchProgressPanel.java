import javax.swing.JTable;
import javax.swing.ListSelectionModel;
import javax.swing.event.ListSelectionListener;
import javax.swing.event.ListSelectionEvent;
import javax.swing.JScrollPane;
import javax.swing.JPanel;
import javax.swing.JFrame;
import javax.swing.*;
import java.awt.*;
import java.awt.event.*;
import com.easysw.cups.*;

public class GLPsearchProgressPanel 
{
    private JProgressBar progressBar;
    private Timer timer;
    private JButton searchButton;
    private JLabel  progressLabel;
    private GLPsearch  tasks[];
    private JPanel     panel = null;

    public GLPsearchProgressPanel() 
    {
        //Create the demo's UI.
        searchButton = new JButton("Search");
        searchButton.setActionCommand("Search");
        searchButton.addActionListener(new ButtonListener());

        progressLabel = new JLabel("Search your local subnet for CUPS servers");
        progressLabel.setBackground(GLPcolors.backgroundColor);
        progressLabel.setForeground(GLPcolors.foregroundColor);

        progressBar = new JProgressBar(0, 254);
        progressBar.setValue(0);
        progressBar.setBorderPainted(true);
        progressBar.setOrientation(JProgressBar.HORIZONTAL);
        progressBar.setBackground(GLPcolors.backgroundColor);
        progressBar.setForeground( Color.blue );
        progressBar.setStringPainted(true);

        panel = new JPanel();
        panel.setLayout(new BorderLayout());
        panel.setBackground(GLPcolors.backgroundColor);

        panel.add(progressLabel,BorderLayout.NORTH);
        panel.add(progressBar,  BorderLayout.CENTER);
        panel.add(searchButton, BorderLayout.EAST);

        //Create a timer.
        timer = new Timer(300, new ActionListener() 
        {

            public void actionPerformed(ActionEvent evt) 
            {
                int n = 0;
                for (int i=0; i < 8; i++)
                {
                  if (tasks[i] != null)
                    n += tasks[i].getValue();
                }
                progressBar.setValue(n);


                //
                //  See if all the threads completed yet.
                //
                int d = 0;
                for (int j=0; j < 8; j++ ) 
                {
                  if (tasks[j] != null)
                  {
                    if (tasks[j].done())
                    {
                      d++;
                    }
                  }
                  else d++;  //  Thread removed ???
                }

                if (d >= 8) 
                {
                    timer.stop();
                    progressBar.setValue(progressBar.getMinimum());
                    searchButton.setActionCommand("Search");
                    searchButton.setText("Search");
                    progressLabel.setText("Search local subnet for CUPS servers");

                    String[] servers = GLPvars.getServerList();
                    if ((servers != null) && (servers.length > 0))
                    {
                      GLPvars.searchTM = new GLPjobTableModel(servers.length,1);
                      GLPvars.searchTM.setColumnName(0,"Search Results");
                      for (int i=0; i < servers.length; i++)
                        GLPvars.searchTM.setValueAt(servers[i],i,0);
                      GLPvars.searchTable = new JTable(GLPvars.searchTM);


                      GLPvars.searchTable.setSelectionMode(ListSelectionModel.SINGLE_SELECTION);
                      ListSelectionModel rowSM = GLPvars.searchTable.getSelectionModel();
                      rowSM.addListSelectionListener(new ListSelectionListener()
                      {
                        public void valueChanged(ListSelectionEvent e) 
                        {
                          //Ignore extra messages.
                          if (e.getValueIsAdjusting()) return;
        
                          ListSelectionModel lsm =
                            (ListSelectionModel)e.getSource();
                          if (lsm.isSelectionEmpty()) 
                          {
                            //no rows are selected
                          } 
                          else 
                          {
                            int selectedRow = lsm.getMinSelectionIndex();
                            String newServer = (String)GLPvars.searchTM.getValueAt(selectedRow,0);
                            GLPvars.setServerName(newServer);
                            GLPvars.tabs.updateServerPanel(GLPvars.cupsServerName);
                          }
                        }
                      });
                      GLPvars.tabs.updateServerPanel(GLPvars.cupsServerName);
                    }
                    //  DEBUG

                } //  threads complete?



            }
        });
    }




    /**
     * The actionPerformed method in this class
     * is called when the user presses the start button.
     */
    class ButtonListener implements ActionListener 
    {

      public void actionPerformed(ActionEvent evt) 
      {

          if (evt.getActionCommand().equals("Search"))
          {
            progressLabel.setText("Searching .....");
            //
            //  Create the search threads .... 
            //
            tasks = new GLPsearch[8];
            for (int i=0; i < 8; i++)
              tasks[i] = new GLPsearch(i+1);

            searchButton.setActionCommand("Stop");
            searchButton.setText("Stop");
            for (int i=0; i < 8; i++)
            {
              if (tasks[i] != null)
              {
                tasks[i].start();
              }
            }
            timer.start();
          }
          else if (evt.getActionCommand().equals("Stop"))
          {
            progressLabel.setText("Search local subnet for CUPS servers");

            for (int i=0; i < 8; i++)
            {
              if (tasks[i] != null)
              {
                tasks[i].interrupt();
              }
              // tasks[i] = null;
            }

            searchButton.setActionCommand("Search");
            searchButton.setText("Search");

          }  // Stop event

      }  //  actionPerformed
    }  // end of class

    public JPanel getPanel()
    {
      return(panel);
    }
    

}
