/*
 * Printer class definitions for the CUPS scheduler.
 *
 * Copyright 2007-2011 by Apple Inc.
 * Copyright 1997-2005 by Easy Software Products, all rights reserved.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more information.
 */


/*
 * Prototypes...
 */

extern cupsd_printer_t	*cupsdAddClass(const char *name);
extern void		cupsdAddPrinterToClass(cupsd_printer_t *c,
			                       cupsd_printer_t *p);
extern int		cupsdDeletePrinterFromClass(cupsd_printer_t *c,
			                            cupsd_printer_t *p);
extern int		cupsdDeletePrinterFromClasses(cupsd_printer_t *p);
extern cupsd_printer_t	*cupsdFindAvailablePrinter(const char *name);
extern cupsd_printer_t	*cupsdFindClass(const char *name);
extern void		cupsdLoadAllClasses(void);
extern void		cupsdSaveAllClasses(void);
