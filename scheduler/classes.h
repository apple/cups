/*
 * "$Id: classes.h,v 1.5 1999/02/19 22:07:03 mike Exp $"
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
 *       44145 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 */


/*
 * Class information structure...
 */

typedef struct class_str
{
  struct class_str *next;		/* Next class in list */
  char		uri[HTTP_MAX_URI],	/* Class URI */
		hostname[HTTP_MAX_HOST],/* Host class resides on */
		name[IPP_MAX_NAME],	/* Class name */
		location[IPP_MAX_NAME],	/* Location */
		info[IPP_MAX_NAME],	/* Description */
		more_info[HTTP_MAX_URI];/* URL for site-specific info */
  int		num_printers;		/* Number of printers in class */
  printer_t	**printers;		/* Printers in class */
} class_t;


/*
 * Globals...
 */

VAR class_t	*Classes VALUE(NULL);	/* List of printer classes... */


/*
 * Prototypes...
 */

extern class_t		*AddClass(char *name);
extern void		AddPrinterToClass(class_t *c, printer_t *p);
extern void		DeleteAllClasses(void);
extern void		DeleteClass(class_t *c);
extern printer_t	*FindAvailablePrinter(char *name);
extern class_t		*FindClass(char *name);
extern void		LoadAllClasses(void);
extern void		SaveAllClasses(void);


/*
 * End of "$Id: classes.h,v 1.5 1999/02/19 22:07:03 mike Exp $".
 */
