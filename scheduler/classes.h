/*
 * "$Id$"
 *
 *   Printer class definitions for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2005 by Easy Software Products, all rights reserved.
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
 *       Hollywood, Maryland 20636 USA
 *
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 */


/*
 * Prototypes...
 */

extern cupsd_printer_t	*cupsdAddClass(const char *name);
extern void		cupsdAddPrinterToClass(cupsd_printer_t *c, cupsd_printer_t *p);
extern void		cupsdDeletePrinterFromClass(cupsd_printer_t *c, cupsd_printer_t *p);
extern void		cupsdDeletePrinterFromClasses(cupsd_printer_t *p);
extern void		cupsdDeleteAllClasses(void);
extern cupsd_printer_t	*cupsdFindAvailablePrinter(const char *name);
extern cupsd_printer_t	*cupsdFindClass(const char *name);
extern void		cupsdLoadAllClasses(void);
extern void		cupsdSaveAllClasses(void);
extern void		cupsdUpdateImplicitClasses(void);


/*
 * End of "$Id$".
 */
