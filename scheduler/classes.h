/*
 * "$Id: classes.h,v 1.7 1999/06/18 18:36:44 mike Exp $"
 *
 *   Printer class definitions for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-1999 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Easy Software Products and are protected by Federal
 *   copyright law.  Distribution and use rights are outlined in the file
 *   "LICENSE.txt" which should have been included with this file.  If this
 *   file is missing or damaged please contact Easy Software Products
 *   at:
 *
 *       Attn: CUPS Licensing Information
 *       Easy Software Products
 *       44141 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 */


/*
 * Prototypes...
 */

extern printer_t	*AddClass(char *name);
extern void		AddPrinterToClass(printer_t *c, printer_t *p);
extern void		DeletePrinterFromClass(printer_t *c, printer_t *p);
extern void		DeletePrinterFromClasses(printer_t *p);
extern void		DeleteAllClasses(void);
extern printer_t	*FindAvailablePrinter(char *name);
extern printer_t	*FindClass(char *name);
extern void		LoadAllClasses(void);
extern void		SaveAllClasses(void);


/*
 * End of "$Id: classes.h,v 1.7 1999/06/18 18:36:44 mike Exp $".
 */
