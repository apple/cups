
import java.awt.*;
import java.awt.event.*;
import javax.swing.*;
import java.net.URL;
import java.net.*;
import java.io.*;
import com.easysw.cups.*;

public class GLPserver implements ActionListener 
{

    private   JPanel serverPanel = null;
    private   JPanel searchPanel = null;

    private   GLPjobTableModel tm = null;
    private   JTable           table = null;

    protected JLabel currentLabel;
    protected JLabel currentLabelValue;
    protected JLabel actionLabel;
    protected JTextField serverTextField;

    protected static final String serverFieldString = "New Server Name:";
    protected static final String applyButtonString = "Apply Changes";


    // Constructor
    public GLPserver() 
    {
      GLPvars.searchTM = new GLPjobTableModel(1,1);
      GLPvars.searchTM.setColumnName( 0, "Search Results" );
      GLPvars.searchTM.setValueAt("No search results",0,0);
      GLPvars.searchTable = new JTable(GLPvars.searchTM);

      load();
    }

    public void load() 
    {
        GridBagLayout      serverLayout;
        GridBagConstraints serverConst;

        // Create the main panel to contain the two sub panels.
        serverPanel   = new JPanel();
        serverLayout  = new GridBagLayout();
        serverConst   = new GridBagConstraints();

        serverPanel.setLayout(serverLayout);
        serverPanel.setBorder(BorderFactory.createEmptyBorder(5,5,5,5));
        serverPanel.setBackground(GLPcolors.backgroundColor);


        //Create a regular text field.
        serverTextField = new JTextField(32);
        serverTextField.setActionCommand(serverFieldString);
        serverTextField.addActionListener(this);

        //Create some labels for the fields.
        JLabel serverFieldLabel = new JLabel(serverFieldString);
        serverFieldLabel.setForeground(GLPcolors.foregroundColor);
        serverFieldLabel.setLabelFor(serverTextField);

        currentLabel = new JLabel("Current Server Name: ");
        currentLabel.setForeground(GLPcolors.foregroundColor);
        currentLabelValue = new JLabel(GLPvars.getServerName());

        serverConst.gridwidth = GridBagConstraints.RELATIVE;
        serverConst.gridx     = 0;
        serverConst.gridy     = 0;
        serverConst.fill      = GridBagConstraints.HORIZONTAL;
        serverConst.weightx   = 0.4;
        serverConst.weighty   = 0.0;
        serverConst.ipadx     = 4;
        serverConst.ipady     = 4;
        serverLayout.setConstraints( currentLabel, serverConst );
        serverPanel.add(currentLabel);

        serverConst.gridwidth = GridBagConstraints.RELATIVE;
        serverConst.gridx     = 1;
        serverConst.gridy     = 0;
        serverConst.fill      = GridBagConstraints.HORIZONTAL;
        serverConst.weightx   = 0.4;
        serverConst.weighty   = 0.0;
        serverConst.ipadx     = 4;
        serverConst.ipady     = 4;
        serverLayout.setConstraints( currentLabelValue, serverConst );
        serverPanel.add(currentLabelValue);

        //  Text
        serverConst.gridwidth = GridBagConstraints.RELATIVE;
        serverConst.gridx     = 0;
        serverConst.gridy     = 1;
        serverConst.fill      = GridBagConstraints.HORIZONTAL;
        serverConst.weightx   = 0.4;
        serverConst.weighty   = 0.0;
        serverConst.ipadx     = 4;
        serverConst.ipady     = 4;
        serverLayout.setConstraints( serverFieldLabel, serverConst );
        serverPanel.add(serverFieldLabel);

        serverConst.gridwidth = GridBagConstraints.RELATIVE;
        serverConst.gridx     = 1;
        serverConst.gridy     = 1;
        serverConst.fill      = GridBagConstraints.HORIZONTAL;
        serverConst.weightx   = 0.4;
        serverConst.weighty   = 0.0;
        serverConst.ipadx     = 4;
        serverConst.ipady     = 4;
        serverLayout.setConstraints( serverTextField, serverConst );
        serverPanel.add(serverTextField);

        JButton applyButton = new JButton(applyButtonString);
	applyButton.setBorder(BorderFactory.createCompoundBorder(
			    BorderFactory.createRaisedBevelBorder(),
			    BorderFactory.createEmptyBorder(5,5,5,5)));
        applyButton.setActionCommand(applyButtonString);
        applyButton.addActionListener(this);
        serverConst.gridx     = 1;
        serverConst.gridy     = 2;
        serverConst.fill      = GridBagConstraints.NONE;
        serverLayout.setConstraints( applyButton, serverConst );
        serverConst.weightx   = 0.4;
        serverConst.weighty   = 0.0;
        serverConst.ipadx     = 4;
        serverConst.ipady     = 4;
        serverPanel.add(applyButton);

        GLPsearchProgressPanel progress = new GLPsearchProgressPanel();
        searchPanel = progress.getPanel();
        serverConst.gridx     = 1;
        serverConst.gridy     = 3;
        serverConst.fill      = GridBagConstraints.HORIZONTAL;
        serverConst.weightx   = 0.6;
        serverConst.weighty   = 0.3;
        serverConst.ipadx     = 4;
        serverConst.ipady     = 4;
        serverLayout.setConstraints( searchPanel, serverConst );
        serverPanel.add(searchPanel);

        serverConst.gridx     = 1;
        serverConst.gridy     = 4;
        serverConst.fill      = GridBagConstraints.HORIZONTAL;
        serverConst.weightx   = 1.0;
        serverConst.weighty   = 0.5;
        serverConst.ipadx     = 4;
        serverConst.ipady     = 4;
        serverLayout.setConstraints( GLPvars.searchTable, serverConst );
        serverPanel.add(GLPvars.searchTable);

    }


    public void actionPerformed(ActionEvent e) 
    {
        if (e.getActionCommand().equals(serverFieldString)) 
        {
          String s = serverTextField.getText();
          if (s.length() > 1)
          {
            GLPvars.setServerName(s);
            serverTextField.setText("");
            currentLabelValue.setText(GLPvars.getServerName());
          }
        } 
        else if (e.getActionCommand().equals(applyButtonString)) 
        {
          String s = serverTextField.getText();
          if (s.length() > 1)
          {
            GLPvars.setServerName(s);
            serverTextField.setText("");
            currentLabelValue.setText(GLPvars.getServerName());
          }
        } 
    }

    public void updateServer( String server )
    {
        GLPvars.setServerName(server);
        load();
        serverTextField.setText("");
        currentLabelValue.setText(GLPvars.getServerName());
    }


    public JPanel getPanel()
    {
      return(serverPanel);
    }

}
