
import java.awt.*;
import java.awt.event.*;
import javax.swing.*;
import java.net.URL;
import java.net.*;
import java.io.*;
import com.easysw.cups.*;

public class GLPprinters implements ActionListener 
{

    private Cups   cups = null;
    public  String cupsServerName = "";

    private JScrollPane        scrollPane = null;
    private JPanel             mainPanel = null;
    private JPanel             serverPanel = null;
    private JPanel             maskPanel = null;

    private GridBagLayout      mainLayout = null;
    private GridBagConstraints mainConst = null;
    private GridBagLayout      maskLayout = null;
    private GridBagConstraints maskConst = null;

    private JLabel             serverLabel = null;

    JTextField nameTextField = null;
    protected static final String maskFieldString  = "Printer Name:";
    protected static final String maskButtonString = "Apply";

    private String currentMask = "";


    // Constructor
    public GLPprinters() 
    {
      cupsServerName = GLPvars.getServerName();
      load();
    }

    public void load() 
    {
        String[]      printer_names;
        String        default_printer;
        int           num_printers = 0;
        int           y = 0, i = 0;
        URL           u;
        CupsPrinter   cp;


        //  -----------------------------------------------------------
        //
        //  First get a list of printer names.
        //
        try
        {
          u = new URL("http://" + GLPvars.getServerName() + ":631/");
          cups = new Cups(u);

          // If authorization is required ....
          // cups.setUser(GLPvars.cupsUser);
          // cups.setPasswd(GLPvars.cupsPasswd);

          printer_names = cups.cupsGetPrinters();
          if (printer_names != null)
            num_printers  = printer_names.length;
          else
            num_printers = 0;
        }
        catch (IOException e)
        {
	  mainPanel   = new JPanel();
          mainPanel.setLayout(new BorderLayout());
	  mainPanel.setBorder(BorderFactory.createEmptyBorder(5,5,5,5));
          mainPanel.setBackground(GLPcolors.backgroundColor);
          JLabel errorLabel = new JLabel("Error loading printers from " +
                                         GLPvars.getServerName());
          errorLabel.setForeground(Color.red);
          mainPanel.add( errorLabel, BorderLayout.CENTER );
          scrollPane = new JScrollPane(mainPanel);
          return;
        }

        //  -----------------------------------------------------------
        //
        //  Now get the printer objects
        //
        CupsPrinter[] printers = new CupsPrinter[num_printers];
        for (i=0; i < num_printers; i++)
        {
          try
          {
            u    = new URL("http://" + GLPvars.getServerName() + 
                           ":631/printers/" + printer_names[i] );  
            cups = new Cups(u);

            // If authorization is required ....
            // cups.setUser(GLPvars.cupsUser);
            // cups.setPasswd(GLPvars.cupsPasswd);

            printers[i] = new CupsPrinter( cups, printer_names[i] );
          }
          catch (IOException e)
          {
            // return(null);
          }
        }


        //
        //  Keep track in case it changes.
        //
        cupsServerName = GLPvars.getServerName();

        if (printer_names != null)
          num_printers = printer_names.length;
        else
          num_printers = 0;

        // default_printer = c.cupsGetDefault();

	// Create the main panel to contain the two sub panels.
	mainPanel   = new JPanel();
        mainLayout  = new GridBagLayout();
        mainConst   = new GridBagConstraints();

	mainPanel.setLayout(mainLayout);
	mainPanel.setBorder(BorderFactory.createEmptyBorder(5,5,5,5));
        mainPanel.setBackground(GLPcolors.backgroundColor);

        //  --------------------------------------------------------------
        //
        //  Add the server name label
        //
	serverPanel   = new JPanel();
        serverPanel.setLayout( new BorderLayout());
        serverPanel.setBackground(GLPcolors.backgroundColor);
        serverLabel = new JLabel("Printers on " + GLPvars.getServerName());
        serverLabel.setForeground(GLPcolors.foregroundColor);
        serverPanel.add(serverLabel, BorderLayout.NORTH );

        mainConst.gridwidth = GridBagConstraints.RELATIVE;
        mainConst.gridx     = 0;
        mainConst.gridy     = y++;
        mainConst.fill      = GridBagConstraints.BOTH;
        mainConst.weightx   = 0.0;
        mainConst.weighty   = 0.0;
        mainConst.ipadx     = 4;
        mainConst.ipady     = 4;
        mainLayout.setConstraints( serverPanel, mainConst );
        mainPanel.add(serverPanel);

        //  --------------------------------------------------------------
        //
        //  Add the printer masking panel
        //
	maskPanel   = new JPanel();
        maskLayout  = new GridBagLayout();
        maskConst   = new GridBagConstraints();

	maskPanel.setLayout(maskLayout);
	maskPanel.setBorder(BorderFactory.createEmptyBorder(5,5,5,5));
        maskPanel.setBackground(GLPcolors.backgroundColor);

        JPanel localMaskPanel = buildMaskPanel();
        maskConst.gridwidth = GridBagConstraints.RELATIVE;
        maskConst.gridx     = 0;
        maskConst.gridy     = 0;
        maskConst.fill      = GridBagConstraints.NONE;
        maskConst.weightx   = 0.0;
        maskConst.weighty   = 0.0;
        maskConst.ipadx     = 4;
        maskConst.ipady     = 4;
        maskLayout.setConstraints( localMaskPanel, maskConst );
        maskPanel.add(localMaskPanel);

        //
        //  Add the masking panel to the main panel.
        //
        mainConst.gridwidth = GridBagConstraints.RELATIVE;
        mainConst.gridx     = 0;
        mainConst.gridy     = y++;
        mainConst.fill      = GridBagConstraints.BOTH;
        mainConst.weightx   = 0.0;
        mainConst.weighty   = 0.0;
        mainConst.ipadx     = 4;
        mainConst.ipady     = 4;
        mainLayout.setConstraints( maskPanel, mainConst );
        mainPanel.add(maskPanel);



        //  --------------------------------------------------------------
        //
        //  Add the printers
        //
        double weight = 1.0 / (double)printers.length;
        for (i=0; i < printers.length; i++)
        {
          JPanel subPanel = printerInfoPanel( printers[i] );
          mainConst.gridwidth = GridBagConstraints.RELATIVE;
          mainConst.gridx     = 0;
          mainConst.gridy     = y++;
          mainConst.fill      = GridBagConstraints.BOTH;
          mainConst.weightx   = 1.0;
          mainConst.weighty   = weight;
          mainConst.ipady     = 4;
          mainLayout.setConstraints( subPanel, mainConst );
          mainPanel.add(subPanel);
        }

        //  ------------------------------------------------
        //
        //  Put the whole thing into a scroll pane.
        //
        scrollPane = new JScrollPane(mainPanel);
    }



    //  -----------------------------------------------------------
    //
    //  Build an info panel for an individual printer.
    //
    private JPanel printerInfoPanel( CupsPrinter cp ) 
    {
        JPanel             printerPanel = new JPanel();
        BoxLayout          printerBox;

        JPanel             leftHeader   = new JPanel();
        JPanel             rightHeader  = new JPanel();

        JPanel             leftPane     = new JPanel();
        JPanel             rightPane    = new JPanel();

        GridBagLayout      leftLayout   = new GridBagLayout();
        GridBagLayout      rightLayout  = new GridBagLayout();

        GridBagConstraints leftConst    = new GridBagConstraints();
        GridBagConstraints rightConst   = new GridBagConstraints();


        JLabel      printerIconLabel   = null;
        JLabel      printerInfoLabel   = null;
        JLabel      printerNameLabel   = null;
        JLabel      printerMakeLabel   = null;

        JTable      printerStatusTable = null;

        printerBox = new BoxLayout(printerPanel, BoxLayout.X_AXIS);
	printerPanel.setLayout(printerBox);
	printerPanel.setBorder(BorderFactory.createEmptyBorder(5,5,5,5));
        printerPanel.setBackground(GLPcolors.backgroundColor);

    	// Add border around the panel.


        //  ------------------------------------------------------------
        //  Left pane
        //  ------------------------------------------------------------
        leftPane.setLayout(leftLayout);
        leftPane.setBackground(GLPcolors.backgroundColor);

        leftHeader.setLayout(new BorderLayout());
        leftHeader.setBackground(GLPcolors.highlightColor);
	leftHeader.setBorder(BorderFactory.createEmptyBorder(3,3,3,3));

        printerNameLabel = new JLabel(cp.getPrinterName());
        printerNameLabel.setForeground(Color.black);
        leftHeader.add( printerNameLabel, BorderLayout.WEST);
        leftConst.gridwidth = GridBagConstraints.RELATIVE;
        leftConst.gridx     = 0;
        leftConst.gridy     = 0;
        leftConst.fill      = GridBagConstraints.HORIZONTAL;
        leftConst.weightx   = 0.0;
        leftConst.weighty   = 0.0;
        leftConst.ipady     = 4;
        leftLayout.setConstraints( leftHeader, leftConst );
        leftPane.add(leftHeader);

	String imageName = "./images/printer-" + cp.getStateText() + ".gif";
	URL iconURL = ClassLoader.getSystemResource(imageName);
	ImageIcon icon = new ImageIcon(iconURL);
        JButton printerButton = new JButton( "<html><center><b>" + 
                                             cp.getPrinterName() + 
                                             "</b></center></html>",
                                            icon );
	printerButton.setBorder(BorderFactory.createEmptyBorder(3,3,3,3));
        printerButton.setBackground(GLPcolors.backgroundColor);
        printerButton.setActionCommand( cp.getPrinterName() );
        printerButton.addActionListener(this);
        printerButton.setToolTipText("Click to go to " + cp.getPrinterName() +
                                     "'s extended informtion page.");

        leftConst.gridwidth = GridBagConstraints.REMAINDER;
        leftConst.gridx     = 0;
        leftConst.gridy     = 1;
        leftConst.fill      = GridBagConstraints.BOTH;
        leftConst.weightx   = 1.0;
        leftConst.weighty   = 1.0;
        leftConst.ipady     = 4;
        leftLayout.setConstraints( printerButton, leftConst );
	leftPane.add(printerButton);


        //  ------------------------------------------------------------
        //  Right pane
        //  ------------------------------------------------------------
        rightPane.setLayout(rightLayout);
        rightPane.setBackground(GLPcolors.backgroundColor);

        rightHeader.setLayout(new BorderLayout());
        rightHeader.setBackground(GLPcolors.highlightColor);
	rightHeader.setBorder(BorderFactory.createEmptyBorder(3,3,3,3));

        printerMakeLabel = new JLabel(cp.getMakeAndModel());
        printerMakeLabel.setForeground(Color.black);
        rightHeader.add( printerMakeLabel, BorderLayout.WEST);

        rightConst.gridwidth = GridBagConstraints.RELATIVE;
        rightConst.gridx     = 0;
        rightConst.gridy     = 0;
        rightConst.fill      = GridBagConstraints.HORIZONTAL;
        rightConst.weightx   = 0.0;
        rightConst.weighty   = 0.0;
        rightConst.ipady     = 4;
        rightLayout.setConstraints( rightHeader, rightConst );
        rightPane.add(rightHeader);


        Font   labelFont = new Font("Serif",Font.BOLD, 12 );
        // Font   textFont = new Font("Serif", Font.NORMAL, 12 );
        Font   messageFont = new Font("Serif", Font.ITALIC, 12 );

        JLabel pdNameLabel     = new JLabel("Name");
        JLabel pdLocationLabel = new JLabel("Location");
        JLabel pdStatusLabel   = new JLabel("Status");
        JLabel pdMessageLabel  = new JLabel("Message");

        JLabel pdNameText     = new JLabel(cp.getPrinterName());
        JLabel pdLocationText = new JLabel(cp.getLocation());
        JLabel pdStatusText   = new JLabel(cp.getStateText());
        JLabel pdMessageText  = new JLabel(cp.getStateReasons());

        pdNameLabel.setFont(labelFont);
        pdLocationLabel.setFont(labelFont);
        pdStatusLabel.setFont(labelFont);
        pdMessageLabel.setFont(labelFont);
        pdMessageText.setFont(messageFont);

        pdNameLabel.setForeground(Color.black);
        pdLocationLabel.setForeground(Color.black);
        pdStatusLabel.setForeground(Color.black);
        pdMessageLabel.setForeground(Color.black);

        JPanel tablePane;
        if ((cp.getStateReasons().length() > 0) &&
            (!cp.getStateReasons().equals("none")))
        {
          tablePane = new JPanel(new GridLayout(4,2,2,2));
          tablePane.add(pdNameLabel);
          tablePane.add(pdNameText);

          tablePane.add(pdLocationLabel);
          tablePane.add(pdLocationText);

          tablePane.add(pdStatusLabel);
          tablePane.add(pdStatusText);

          tablePane.add(pdMessageLabel);
          tablePane.add(pdMessageText);
        }
        else
        {
          tablePane = new JPanel(new GridLayout(3,2,2,2));
          tablePane.add(pdNameLabel);
          tablePane.add(pdNameText);

          tablePane.add(pdLocationLabel);
          tablePane.add(pdLocationText);

          tablePane.add(pdStatusLabel);
          tablePane.add(pdStatusText);

        }
        tablePane.setBackground(GLPcolors.backgroundColor);

        // printerStatusTable.setShowGrid(false);
        rightConst.gridwidth = GridBagConstraints.REMAINDER;
        rightConst.gridx     = 0;
        rightConst.gridy     = 1;
        rightConst.fill      = GridBagConstraints.BOTH;
        rightConst.weightx   = 1.0;
        rightConst.weighty   = 1.0;
        rightConst.ipady     = 4;
        rightLayout.setConstraints( tablePane, rightConst );
	rightPane.add(tablePane);

        printerPanel.add(leftPane);
        printerPanel.add(rightPane);

        return(printerPanel);
    }





    public JPanel buildMaskPanel()
    {

      // Create the main panel to contain the two sub panels.
      JPanel             namePanel   = new JPanel();
      GridBagLayout      nameLayout  = new GridBagLayout();
      GridBagConstraints nameConst   = new GridBagConstraints();

      namePanel.setLayout(nameLayout);
      namePanel.setBorder(BorderFactory.createEmptyBorder(5,5,5,5));
      namePanel.setBackground(GLPcolors.backgroundColor);


      //Create a regular text field.
      nameTextField = new JTextField(16);
      nameTextField.setActionCommand(maskFieldString);
      nameTextField.addActionListener(this);
      nameTextField.setText("");

      //Create some labels for the fields.
      JLabel nameFieldLabel = new JLabel(maskFieldString);
      nameFieldLabel.setForeground(GLPcolors.foregroundColor);
      nameFieldLabel.setLabelFor(nameTextField);

      //  Text
      nameConst.gridwidth = GridBagConstraints.RELATIVE;
      nameConst.gridx     = 0;
      nameConst.gridy     = 0;
      nameConst.fill      = GridBagConstraints.HORIZONTAL;
      nameConst.weightx   = 0.0;
      nameConst.weighty   = 0.0;
      nameConst.ipadx     = 4;
      nameConst.ipady     = 4;
      nameLayout.setConstraints( nameFieldLabel, nameConst );
      namePanel.add(nameFieldLabel);

      nameConst.gridwidth = GridBagConstraints.RELATIVE;
      nameConst.gridx     = 1;
      nameConst.gridy     = 0;
      nameConst.fill      = GridBagConstraints.HORIZONTAL;
      nameConst.weightx   = 0.0;
      nameConst.weighty   = 0.0;
      nameConst.ipadx     = 4;
      nameConst.ipady     = 4;
      nameLayout.setConstraints( nameTextField, nameConst );
      namePanel.add(nameTextField);

      JButton applyButton = new JButton(maskButtonString);
      applyButton.setBorder(BorderFactory.createCompoundBorder(
			    BorderFactory.createRaisedBevelBorder(),
			    BorderFactory.createEmptyBorder(2,2,2,2)));
      applyButton.setActionCommand(maskButtonString);
      applyButton.addActionListener(this);
      nameConst.gridx     = 2;
      nameConst.gridy     = 0;
      nameConst.fill      = GridBagConstraints.NONE;
      nameLayout.setConstraints( applyButton, nameConst );
      nameConst.weightx   = 0.0;
      nameConst.weighty   = 0.0;
      nameConst.ipadx     = 4;
      nameConst.ipady     = 4;
      namePanel.add(applyButton);

      return(namePanel);
    }


    public void actionPerformed(ActionEvent e) 
    {
        if (e.getActionCommand().equals(maskFieldString)) 
        {
          String s = nameTextField.getText();
          if (s.length() > 1)
          {
            currentMask = s;
          }
        } 
        else if (e.getActionCommand().equals(maskButtonString)) 
        {
          String s = nameTextField.getText();
          if (s.length() > 1)
          {
            currentMask = s;
          }
        } 
        else
        {
          GLPvars.selectedPrinterName = e.getActionCommand();
          GLPvars.tabs.updateDetailPanel();
          GLPvars.tabs.tabPanel.setSelectedIndex(2);
        }
    }


    public JScrollPane getPanel()
    {
      return(scrollPane);
    }
}
