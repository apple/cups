/*
 * "$Id: printers.h,v 1.13 2000/01/03 19:02:33 mike Exp $"
 *
 *   Printer definitions for the Common UNIX Printing System (CUPS) scheduler.
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
 * Printer/class information structure...
 */

typedef struct printer_str
{
  struct printer_str *next;		/* Next printer in list */
  char		uri[HTTP_MAX_URI],	/* Printer URI */
		hostname[HTTP_MAX_HOST],/* Host printer resides on */
		name[IPP_MAX_NAME],	/* Printer name */
		location[IPP_MAX_NAME],	/* Location code */
		info[IPP_MAX_NAME],	/* Description */
		more_info[HTTP_MAX_URI];/* URL for site-specific info */
  int		accepting;		/* Accepting jobs? */
  ipp_pstate_t	state;			/* Printer state */
  char		state_message[1024];	/* Printer state message */
  time_t	state_time;		/* Time at this state */
  cups_ptype_t	type;			/* Printer type (color, small, etc.) */
  time_t	browse_time;		/* Last time update was sent/received */
  char		device_uri[HTTP_MAX_URI],/* Device URI */
		backend[1024];		/* Backend to use */
  mime_type_t	*filetype;		/* Pseudo-filetype for printer */
  void		*job;			/* Current job in queue */
  ipp_t		*attrs;			/* Attributes supported by this printer */
  int		num_printers;		/* Number of printers in class */
  struct printer_str **printers;	/* Printers in class */
} printer_t;


/*
 * Globals...
 */

VAR printer_t		*Printers VALUE(NULL);	/* Printer list */
VAR printer_t		*DefaultPrinter VALUE(NULL);
						/* Default printer */

/*
 * Prototypes...
 */

extern printer_t	*AddPrinter(const char *name);
extern void		DeleteAllPrinters(void);
extern void		DeletePrinter(printer_t *p);
extern printer_t	*FindPrinter(const char *name);
extern void		LoadAllPrinters(void);
extern void		SaveAllPrinters(void);
extern void		SetPrinterAttrs(printer_t *p);
extern void		SetPrinterState(printer_t *p, ipp_pstate_t s);
extern void		SortPrinters(void);
#define			StartPrinter(p) SetPrinterState((p), IPP_PRINTER_IDLE)
extern void		StopPrinter(printer_t *p);


/*
 * End of "$Id: printers.h,v 1.13 2000/01/03 19:02:33 mike Exp $".
 */
