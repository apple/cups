
import java.awt.*;
import java.awt.event.*;
import javax.swing.*;
import javax.swing.filechooser.*;
import java.net.URL;
import java.net.*;
import java.io.*;
import com.easysw.cups.*;

public class GLPoptions implements ActionListener 
{
    Cups        cups    = null;
    CupsJob     job     = null;
    CupsPrinter printer = null;

    String fileName    = "";

    JPanel             mainPanel;
    JTextField         fileTextField;
    JButton            printButton;
    GridBagLayout      mainLayout;
    GridBagConstraints mainConst;

    //
    //  Print options;
    //
    String[]            jobSheetsNames;
    String[]		orientationNames;
    int[]		orientationValues;
    String[]		qualityNames;
    int[]		qualityValues;

    String              jobSheetsOption   = "";
    int                 orientationOption = -1;
    int                 qualityOption     = -1;

    int			pageLowerOption   = 0;
    int			pageUpperOption   = 0;
    boolean             pagePrintAll      = true;

    int			numCopiesOption   = 1;
    int			numLowerCopiesOption   = -1;
    int			numUpperCopiesOption   = -1;

    int                 selectedJobSheets   = 0;
    int                 selectedOrientation = 0;
    int                 selectedQuality     = 0;

    JComboBox           orientationBox;
    JComboBox           jobSheetsBox;
    JTextField          numCopiesField;
    JCheckBox           printAllCheckBox;
    JTextField          pageLowerField;
    JTextField          pageUpperField;
    MyTextListener      textListener = new MyTextListener();
 

    // Constructor
    public GLPoptions() 
    {
	mainPanel   = new JPanel();
        mainPanel.setLayout(new BorderLayout());
        mainPanel.setBackground(GLPcolors.backgroundColor);
        JLabel label = new JLabel("No printer selected");
        label.setForeground(GLPcolors.foregroundColor);
        mainPanel.add(label,BorderLayout.CENTER);
    }

    // Constructor
    public GLPoptions(CupsPrinter cp) 
    {
      printer = cp;
      if (printer != null)
      {
        load(printer);
      }
      else
      {
	mainPanel   = new JPanel();
        mainPanel.setLayout(new BorderLayout());
        mainPanel.setBackground(GLPcolors.backgroundColor);
        JLabel label = new JLabel("No printer selected");
        label.setForeground(GLPcolors.foregroundColor);
        mainPanel.add(label,BorderLayout.CENTER);
      }
    }


    private void load( CupsPrinter cp )
    {

        fillOptionValues();

	// Create the main panel to contain the two sub panels.
	mainPanel   = new JPanel();
        mainLayout  = new GridBagLayout();
        mainConst   = new GridBagConstraints();

	mainPanel.setLayout(mainLayout);
	mainPanel.setBorder(BorderFactory.createEmptyBorder(5,5,5,5));
        mainPanel.setBackground(GLPcolors.backgroundColor);

        String tmp_s = "Printing to " + printer.getPrinterName() + 
                       " on " + GLPvars.cupsServerName;
        JLabel printerNameText = new JLabel(tmp_s);
        printerNameText.setForeground(GLPcolors.foregroundColor);
        mainConst.gridwidth = GridBagConstraints.RELATIVE;
        mainConst.gridx     = 0;
        mainConst.gridy     = 0;
        mainConst.fill      = GridBagConstraints.NONE;
        mainConst.weightx   = 0.0;
        mainConst.weighty   = 0.0;
        mainConst.ipady     = 4;
        mainLayout.setConstraints( printerNameText, mainConst );
        mainPanel.add(printerNameText);

        JPanel filePanel = buildFilePanel();
        mainConst.gridwidth = GridBagConstraints.RELATIVE;
        mainConst.gridx     = 0;
        mainConst.gridy     = 1;
        mainConst.fill      = GridBagConstraints.HORIZONTAL;
        mainConst.weightx   = 1.0;
        mainConst.weighty   = 0.1;
        mainConst.ipady     = 4;
        mainLayout.setConstraints( filePanel, mainConst );
        mainPanel.add(filePanel);

        JPanel orientationPanel = buildOrientationComboBox();
        mainConst.gridwidth = GridBagConstraints.RELATIVE;
        mainConst.gridx     = 0;
        mainConst.gridy     = 2;
        mainConst.fill      = GridBagConstraints.NONE;
        mainConst.weightx   = 0.8;
        mainConst.weighty   = 0.1;
        mainConst.ipady     = 4;
        mainLayout.setConstraints( orientationPanel, mainConst );
        mainPanel.add(orientationPanel);

        JPanel jobSheetsPanel = buildJobSheetsComboBox();
        mainConst.gridwidth = GridBagConstraints.RELATIVE;
        mainConst.gridx     = 0;
        mainConst.gridy     = 3;
        mainConst.fill      = GridBagConstraints.NONE;
        mainConst.weightx   = 0.8;
        mainConst.weighty   = 0.1;
        mainConst.ipady     = 4;
        mainLayout.setConstraints( jobSheetsPanel, mainConst );
        mainPanel.add(jobSheetsPanel);

        JPanel numCopiesPanel = buildNumCopiesPanel();
        mainConst.gridwidth = GridBagConstraints.RELATIVE;
        mainConst.gridx     = 0;
        mainConst.gridy     = 4;
        mainConst.fill      = GridBagConstraints.HORIZONTAL;
        mainConst.weightx   = 1.0;
        mainConst.weighty   = 0.1;
        mainConst.ipady     = 4;
        mainLayout.setConstraints( numCopiesPanel, mainConst );
        mainPanel.add(numCopiesPanel);

        JPanel pageRangePanel = buildPageRangePanel();
        mainConst.gridwidth = GridBagConstraints.RELATIVE;
        mainConst.gridx     = 0;
        mainConst.gridy     = 5;
        mainConst.fill      = GridBagConstraints.HORIZONTAL;
        mainConst.weightx   = 1.0;
        mainConst.weighty   = 0.1;
        mainConst.ipady     = 4;
        mainLayout.setConstraints( pageRangePanel, mainConst );
        mainPanel.add(pageRangePanel);

        JPanel buttonPanel = buildButtonPanel();
        mainConst.gridwidth = GridBagConstraints.RELATIVE;
        mainConst.gridx     = 0;
        mainConst.gridy     = 6;
        mainConst.fill      = GridBagConstraints.NONE;
        mainConst.weightx   = 1.0;
        mainConst.weighty   = 0.1;
        mainConst.ipady     = 4;
        mainLayout.setConstraints( buttonPanel, mainConst );
        mainPanel.add(buttonPanel);

    }


    //  --------------------------------------------------------------
    //
    //  Filename / Browse panel
    //
    public JPanel buildFilePanel()
    {
      JPanel             localPanel    = new JPanel();
      final JFileChooser fc            = new JFileChooser();

      localPanel.setBackground(GLPcolors.backgroundColor);
      localPanel.setLayout(new BorderLayout());

      //Create a regular text field.
      fileTextField = new JTextField(50);
      fileTextField.addActionListener(this);

      //Create some labels for the fields.
      JLabel fileFieldLabel = new JLabel(" File to print: ");
      fileFieldLabel.setForeground(GLPcolors.foregroundColor);
      // fileFieldLabel.setLabelFor(fileTextField);

      localPanel.add( fileFieldLabel, BorderLayout.WEST );
      localPanel.add( fileTextField, BorderLayout.CENTER );

      //Create the open button
      JButton openButton = new JButton("Browse ..." );
      openButton.addActionListener(new ActionListener() 
      {
         public void actionPerformed(ActionEvent e) 
         {
           int returnVal = fc.showOpenDialog(mainPanel);
           if (returnVal == JFileChooser.APPROVE_OPTION) 
           {
             File file = fc.getSelectedFile();
             // fileTextField.setText(file.getPath() + file.getName());
             fileTextField.setText(file.getPath());
             fileName = file.getPath();
           } 
         }
      });
      openButton.setBackground(GLPcolors.buttonBackgroundColor);
      openButton.setForeground(GLPcolors.buttonForegroundColor);
      localPanel.add(openButton, BorderLayout.EAST );
      return(localPanel);
    }



    public JPanel buildOrientationComboBox()
    {
      JPanel localPanel = new JPanel();
      localPanel.setLayout(new BorderLayout());
      localPanel.setBackground(GLPcolors.backgroundColor);

      JLabel localLabel = new JLabel("Page Orientation:  ");
      localLabel.setBackground(GLPcolors.backgroundColor);
      localLabel.setForeground(GLPcolors.foregroundColor);

      orientationBox = new JComboBox(orientationNames);
      if (selectedOrientation > 0)
        orientationBox.setSelectedIndex(selectedOrientation);
      orientationBox.addActionListener(this);
      orientationBox.setBackground(GLPcolors.backgroundColor);

      localPanel.add(localLabel,BorderLayout.WEST);
      localPanel.add(orientationBox,BorderLayout.CENTER);

      return(localPanel);
    }


    public JPanel buildJobSheetsComboBox()
    {
      JPanel localPanel = new JPanel();
      localPanel.setLayout(new BorderLayout());
      localPanel.setBackground(GLPcolors.backgroundColor);

      JLabel localLabel = new JLabel("Job Sheets:  ");
      localLabel.setBackground(GLPcolors.backgroundColor);
      localLabel.setForeground(GLPcolors.foregroundColor);

      jobSheetsBox = new JComboBox(jobSheetsNames);
      if (selectedJobSheets > 0)
        jobSheetsBox.setSelectedIndex(selectedJobSheets);
      jobSheetsBox.addActionListener(this);
      jobSheetsBox.setBackground(GLPcolors.backgroundColor);

      localPanel.add(localLabel,BorderLayout.WEST);
      localPanel.add(jobSheetsBox,BorderLayout.CENTER);

      return(localPanel);
    }

    public JPanel buildNumCopiesPanel()
    {
      JPanel localPanel = new JPanel();
      localPanel.setLayout(new FlowLayout());
      localPanel.setBackground(GLPcolors.backgroundColor);

      JLabel localLabel = new JLabel("Number of copies:  ");
      localLabel.setBackground(GLPcolors.backgroundColor);
      localLabel.setForeground(GLPcolors.foregroundColor);

      numCopiesField = new JTextField(3);
      if (numCopiesOption > 0)
        numCopiesField.setText(new Integer(numCopiesOption).toString());
      numCopiesField.addActionListener(this);
      numCopiesField.addFocusListener(textListener);
      numCopiesField.setBackground(GLPcolors.backgroundColor);

      localPanel.add(localLabel);
      localPanel.add(numCopiesField);

      return(localPanel);
    }


    public JPanel buildPageRangePanel()
    {
      JPanel localPanel = new JPanel();
      localPanel.setLayout(new FlowLayout());
      localPanel.setBackground(GLPcolors.backgroundColor);

      printAllCheckBox = new JCheckBox("Print all", pagePrintAll );
      printAllCheckBox.setBackground(GLPcolors.backgroundColor);
      printAllCheckBox.setForeground(GLPcolors.foregroundColor);
      printAllCheckBox.addActionListener(this);

      JLabel localLabel = new JLabel("-or- pages:  ");
      localLabel.setBackground(GLPcolors.backgroundColor);
      localLabel.setForeground(GLPcolors.foregroundColor);

      JLabel localLabel2 = new JLabel(" to ");
      localLabel2.setBackground(GLPcolors.backgroundColor);
      localLabel2.setForeground(GLPcolors.foregroundColor);

      pageLowerField = new JTextField(4);
      pageUpperField = new JTextField(4);

      pageLowerField.addActionListener(this);
      pageUpperField.addActionListener(this);
      pageLowerField.addFocusListener(textListener);
      pageUpperField.addFocusListener(textListener);

      pageLowerField.setBackground(GLPcolors.backgroundColor);
      pageUpperField.setBackground(GLPcolors.backgroundColor);
      pageLowerField.setEnabled(false);
      pageUpperField.setEnabled(false);

      localPanel.add(printAllCheckBox);
      localPanel.add(localLabel);
      localPanel.add(pageLowerField);
      localPanel.add(localLabel2);
      localPanel.add(pageUpperField);

      return(localPanel);
    }




    public JPanel buildTextPanel()
    {
      JPanel localPanel = new JPanel();
      return(localPanel);
    }

    public JPanel buildButtonPanel()
    {
      JPanel localPanel = new JPanel();
      localPanel.setLayout(new BorderLayout());
      printButton = new JButton(" Print ");
      printButton.setBackground(GLPcolors.buttonBackgroundColor);
      printButton.setForeground(GLPcolors.buttonForegroundColor);
      printButton.addActionListener( this );
      localPanel.add(printButton, BorderLayout.WEST ); 
      return(localPanel);
    }



    public void updateOptions(CupsPrinter cp)
    {
      printer = cp;
      if (printer != null)
      {
        load(printer);
      }
      else
      {
	mainPanel   = new JPanel();
        mainPanel.setLayout(new BorderLayout());
        mainPanel.setBackground(GLPcolors.backgroundColor);
        JLabel label = new JLabel("No printer selected");
        label.setForeground(GLPcolors.foregroundColor);
        mainPanel.add(label,BorderLayout.CENTER);
      }
    }

    public JPanel getPanel()
    {
      return(mainPanel);
    }




    public CupsJob printFile( String filename )
    {
      Cups    cups;
      CupsJob job;
      URL     u;
      IPPAttribute attrs[];
      
      attrs = buildPrintAttributes();

for (int i=0; i < attrs.length; i++)
  attrs[i].dump_values();

      try
      {
        u = new URL("http://" + GLPvars.getServerName() + 
                    ":631/printers/" + printer.getPrinterName() );
        cups = new Cups(u);
        job = cups.cupsPrintFile(filename,attrs);
        return(job);
      }
      catch (IOException e)
      {
        return(null);
      }
    }




    private void fillOptionValues()
    {
      IPPAttribute  a;
      int           i, n;

      //
      //  Job sheets ....
      //
      jobSheetsNames = printer.getJobSheetsSupported();
      if (printer.getJobSheetsDefault() != "none")
      {
        for (i=0; i < jobSheetsNames.length; i++)
          if (jobSheetsNames[i] == printer.getJobSheetsDefault())
            selectedJobSheets = i;
      }

      //
      //  Orientation ....
      //
      orientationNames  = new String[printer.getOrientationSupported().length];
      orientationValues = printer.getOrientationSupported();
      for (i=0; i < printer.getOrientationSupported().length; i++)
      {
        if (orientationValues[i] == printer.getOrientationDefault())
          selectedOrientation = i;
        switch( orientationValues[i] )
        {
          case IPPDefs.PORTRAIT: 
                       orientationNames[i] = "Portrait";
                       break;
          case IPPDefs.LANDSCAPE: 
                       orientationNames[i] = "Landscape";
                       break;
          case IPPDefs.REVERSE_LANDSCAPE: 
                       orientationNames[i] = "Reverse Landscape";
                       break;
          case IPPDefs.REVERSE_PORTRAIT: 
                       orientationNames[i] = "Reverse Portrait";
                       break;
        }
      }

      if (printer.getLowerCopiesSupported() == 
          printer.getUpperCopiesSupported())
      {
        numCopiesOption = printer.getCopiesDefault();        
      }
      else
      {
        numCopiesOption      = printer.getLowerCopiesSupported();
        numLowerCopiesOption = printer.getLowerCopiesSupported();
        numUpperCopiesOption = printer.getUpperCopiesSupported();
      }
    }


    private IPPAttribute[] buildPrintAttributes()
    {
      IPPAttribute      a;
      IPPAttribute[]    attrs;
      int               num_attrs = 0;

      if (orientationOption >= 0)
        num_attrs++;
      if (jobSheetsOption.length() > 0)
        num_attrs++;
      if (numCopiesOption > 1)
        num_attrs++;
      if ((pageLowerOption > 0) && (pageUpperOption > 0) && (!pagePrintAll))
        num_attrs++;

      if (num_attrs > 0)
        attrs = new IPPAttribute[num_attrs];
      else
        return(null);

      int i = 0;
      if (jobSheetsOption.length() > 0)
      {
        attrs[i] = new IPPAttribute( IPPDefs.TAG_OPERATION,
                                     IPPDefs.TAG_NAME,
                                     "job-sheets" );
        attrs[i].addString( "", jobSheetsOption );
        i++;
      }
      if (orientationOption >= IPPDefs.PORTRAIT)
      {
        attrs[i] = new IPPAttribute( IPPDefs.TAG_JOB,
                                     IPPDefs.TAG_ENUM,
                                     "orientation-requested" );
        attrs[i].addEnum( orientationOption );
        i++;
      }
      if (numCopiesOption > 1)
      {
        attrs[i] = new IPPAttribute( IPPDefs.TAG_JOB,
                                     IPPDefs.TAG_INTEGER,
                                     "copies" );
        attrs[i].addInteger( numCopiesOption );
        i++;
      }
      if ((pageLowerOption > 0) && (pageUpperOption > 0) && (!pagePrintAll))
      {
        attrs[i] = new IPPAttribute( IPPDefs.TAG_JOB,
                                     IPPDefs.TAG_RANGE,
                                     "page-ranges" );
        attrs[i].addRange( pageLowerOption, pageUpperOption );
        i++;
      }
      return(attrs);
    }



    // Implementation of ActionListener interface.
    public void actionPerformed(ActionEvent e) 
    {
      Object source = e.getSource();

      //
      //  Name typed in
      //
      if (source == printAllCheckBox)
      {
        JCheckBox cb = (JCheckBox)source;
        pagePrintAll = cb.isSelected();
        pageLowerField.setEnabled(!pagePrintAll);
        pageUpperField.setEnabled(!pagePrintAll);
      }
      else if (source == pageLowerField)
      {
        String s = pageLowerField.getText();
        if (s.length() > 1)
        {
          pageLowerOption = new Integer(s).intValue();
          // if (pageLowerOption > 0)
          //  printAllCheckBox.setChecked(false);
        }
      }
      else if (source == pageUpperField)
      {
        String s = pageUpperField.getText();
        if (s.length() > 1)
        {
          pageUpperOption = new Integer(s).intValue();
          // if (pageUpperOption > 0)
          //  printAllCheckBox.setChecked(false);
        }
      }
      else if (source == orientationBox)
      {
        JComboBox cb = (JComboBox)source;
        selectedOrientation = cb.getSelectedIndex();
        orientationOption = orientationValues[selectedOrientation];
      }
      else if (source == jobSheetsBox)
      {
        JComboBox cb = (JComboBox)source;
        selectedJobSheets = cb.getSelectedIndex();
        jobSheetsOption = jobSheetsNames[selectedJobSheets];
      }
      else if (source == numCopiesField)
      {
        String s = numCopiesField.getText();
        if (s.length() >= 1)
        {
          numCopiesOption = new Integer(s).intValue();
        }
      }
      else if (source == fileTextField) 
      {
        String s = fileTextField.getText();
        if (s.length() > 1)
        {
          fileName = s;
        } 
      }
      else if (source == printButton)
      {
        if (fileName.length() > 1)
        {
          job = printFile( fileName );
          if (job != null)
          {
            fileName = "";
            fileTextField.setText("");
            JOptionPane.showMessageDialog(mainPanel, 
                        "Job " + printer.getPrinterName() + "-" +
                        new Integer(job.job_id).toString() + 
                        " queued.");
          }
        }
      }
    }





    public class MyTextListener implements FocusListener
    {

      public void focusGained(FocusEvent e)
      {
      }

      public void focusLost(FocusEvent e)
      {
        JTextField txtField = (JTextField)e.getSource();
        if (txtField == numCopiesField)
        {
          String s = numCopiesField.getText();
          if (s.length() >= 1)
          {
            numCopiesOption = new Integer(s).intValue();
          }
        }
        else if (txtField == pageLowerField)
        {
          String s = pageLowerField.getText();
          if (s.length() >= 1)
          {
            pageLowerOption = new Integer(s).intValue();
          }
        }
        else if (txtField == pageUpperField)
        {
          String s = pageUpperField.getText();
          if (s.length() >= 1)
          {
            pageUpperOption = new Integer(s).intValue();
          }
        }
      }
    }

}
