
import javax.swing.table.*; 
import javax.swing.event.TableModelListener; 
import javax.swing.event.TableModelEvent; 
import com.easysw.cups.*;

public class GLPjobTableModel extends AbstractTableModel 
                      implements TableModelListener 
{
    protected TableModel model; 
    private final Object[][] rowData;
    private final String[]   colNames;
    private int rowCount = 0;
    private int colCount = 0;

    public GLPjobTableModel( int num_rows, int num_cols )
    {
       rowData  = new Object[num_rows][num_cols];
       colNames = new String[num_cols];
       rowCount = num_rows;
       colCount = num_cols;
    }

    public TableModel getModel() 
    {
        return model;
    }

    public void setModel(TableModel model) 
    {
        this.model = model; 
        model.addTableModelListener(this); 
    }

    // By default, implement TableModel by forwarding all messages 
    // to the model. 

    public Object getValueAt(int aRow, int aColumn) 
    {
        if (rowCount >= aRow && colCount >= aColumn)
          return (rowData[aRow][aColumn]); 
        else
          return(null);
    }
        
    public void setValueAt(Object aValue, int aRow, int aColumn) 
    {
        if (rowCount >= aRow && colCount >= aColumn)
          rowData[aRow][aColumn] = aValue;
    }

    public int getRowCount() 
    {
        return (rowCount);
    }

    public int getColumnCount() 
    {
        return (colCount);
    }
        
    public void setColumnName(int aColumn, String aName) 
    {
        if (colCount >= aColumn)
        {
          colNames[aColumn] = aName;
        }
    }
        
    public String getColumnName(int aColumn) 
    {
        if (colCount >= aColumn)
          return (colNames[aColumn]); 
        else
          return("");
    }

    public Class getColumnClass(int aColumn) 
    {
        if (colCount >= aColumn)
        {
         if (rowData[0][aColumn] != null)
           return (rowData[0][aColumn].getClass()); 
         else
           return( null );
        }
        else return(null);
    }
        
    public boolean isCellEditable(int row, int column) 
    {
         return(false); 
    }


//
// Implementation of the TableModelListener interface, 
//
    // By default forward all events to all the listeners. 
    public void tableChanged(TableModelEvent e) 
    {
        fireTableChanged(e);
    }
}
