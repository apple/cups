
import java.awt.*;
import java.awt.event.*;
import javax.swing.*;
import java.net.URL;
import java.net.*;
import java.io.*;
import com.easysw.cups.*;

public class GLP implements ActionListener 
{
    private JTabbedPane mainPanel      = null;

    // Constructor
    public GLP() 
    {
      GLPvars.init();

      GLPvars.mainGLPPanel = new JTabbedPane();
      GLPvars.tabs         = new GLPtabs();
      GLPvars.mainGLPPanel = GLPvars.tabs.getPanel(); 
    }

    // Implementation of ActionListener interface.
    public void actionPerformed(ActionEvent event) 
    {
      // if ("comboBoxChanged".equals(event.getActionCommand())) 
      // {
	    // update the icon to display the new phase
	    // phaseIconLabel.setIcon(images[phaseChoices.getSelectedIndex()]);
      // }
    }

    // main method
    public static void main(String[] args) 
    {
	// create a new instance of CupsApplet
	GLP app = new GLP();

	// Create a frame and container for the panels.
	JFrame glpFrame = new JFrame("Java GLP");

	// Set the look and feel.
	try {
	    UIManager.setLookAndFeel(
		UIManager.getCrossPlatformLookAndFeelClassName());
	} catch(Exception e) {}
	
	glpFrame.setContentPane(GLPvars.mainGLPPanel);

        // Exit when the window is closed.
        glpFrame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE); 
        glpFrame.setSize(600,400);
	
	// Show the converter.
	// glpFrame.pack();
	glpFrame.setVisible(true);
    }
}
