
import java.awt.*;
import java.awt.event.*;
import javax.swing.*;
import java.net.URL;
import java.net.*;
import java.io.*;
import com.easysw.cups.*;

public class GLPprinterDetail
{

    private CupsPrinter        printer = null;
    private JScrollPane        scrollPane = null;
    private JPanel             mainPanel = null;


    // Constructor
    public GLPprinterDetail( CupsPrinter cp ) 
    {
      printer = cp;
      load();
    }

    public void load() 
    {
      mainPanel = new JPanel();
      mainPanel = printerInfoPanel( printer );
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

	String imageName = "./images/printer-" + 
                           cp.getStateText() + ".gif";
	URL iconURL = ClassLoader.getSystemResource(imageName);
	ImageIcon icon = new ImageIcon(iconURL);
        JButton printerButton = new JButton( "<html><center><b>" + 
                                             cp.getPrinterName() + 
                                             "</b></center></html>",
                                            icon );
	printerButton.setBorder(BorderFactory.createEmptyBorder(3,3,3,3));
        printerButton.setBackground(GLPcolors.backgroundColor);
        printerButton.setActionCommand( cp.getPrinterName() );
        // printerButton.addActionListener(this);
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

        JLabel pdServerLabel   = new JLabel("Server");
        JLabel pdNameLabel     = new JLabel("Name");
        JLabel pdLocationLabel = new JLabel("Location");
        JLabel pdStatusLabel   = new JLabel("Status");
        JLabel pdMessageLabel  = new JLabel("Message");

        JLabel pdServerText   = new JLabel(GLPvars.cupsServerName);
        JLabel pdNameText     = new JLabel(cp.getPrinterName());
        JLabel pdLocationText = new JLabel(cp.getLocation());
        JLabel pdStatusText   = new JLabel(cp.getStateText());
        JLabel pdMessageText  = new JLabel(cp.getStateReasons());

        pdServerLabel.setFont(labelFont);
        pdNameLabel.setFont(labelFont);
        pdLocationLabel.setFont(labelFont);
        pdStatusLabel.setFont(labelFont);
        pdMessageLabel.setFont(labelFont);
        pdMessageText.setFont(messageFont);

        pdServerLabel.setForeground(Color.black);
        pdNameLabel.setForeground(Color.black);
        pdLocationLabel.setForeground(Color.black);
        pdStatusLabel.setForeground(Color.black);
        pdMessageLabel.setForeground(Color.black);


        JPanel tablePane;
        if ((cp.getStateReasons().length() > 0) &&
            (!cp.getStateReasons().equals("none")))
        {
          tablePane = new JPanel(new GridLayout(5,2,2,2));
          tablePane.add(pdServerLabel);
          tablePane.add(pdServerText);

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
          tablePane = new JPanel(new GridLayout(4,2,2,2));
          tablePane.add(pdServerLabel);
          tablePane.add(pdServerText);

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


    public JPanel getPanel()
    {
      return(mainPanel);
    }
}
