/*
 * "$Id: printers.h,v 1.8 1999/04/21 14:14:57 mike Exp $"
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
 *       44145 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 */

/*
 * Printer information structure...
 */

typedef struct printer_str
{
  struct printer_str *next;		/* Next printer in list */
  char		uri[HTTP_MAX_URI],	/* Printer URI */
		hostname[HTTP_MAX_HOST],/* Host printer resides on */
		name[IPP_MAX_NAME],	/* Printer name */
		location[IPP_MAX_NAME],	/* Location code */
		info[IPP_MAX_NAME],	/* Description */
		more_info[HTTP_MAX_URI],/* URL for site-specific info */
		username[MAX_USERPASS],	/* Username for remote system */
		password[MAX_USERPASS];	/* Password for remote system */
  int		accepting;		/* Accepting jobs? */
  ipp_pstate_t	state;			/* Printer state */
  char		state_message[1024];	/* Printer state message */
  cups_ptype_t	type;			/* Printer type (color, small, etc.) */
  time_t	state_time;		/* Time at this state */
  char		device_uri[HTTP_MAX_URI],/* Device URI */
		backend[1024];		/* Backend to use */
  mime_type_t	*filetype;		/* Pseudo-filetype for printer */
  void		*job;			/* Current job in queue */
  ipp_t		*attrs;			/* Attributes supported by this printer */
} printer_t;


/*
 * Globals...
 */

VAR printer_t		*Printers VALUE(NULL);	/* Printer list */
VAR char		DefaultPrinter[IPP_MAX_NAME] VALUE("");
						/* Default printer */

/*
 * Prototypes...
 */

extern printer_t	*AddPrinter(char *name);
extern void		DeleteAllPrinters(void);
extern void		DeletePrinter(printer_t *p);
extern printer_t	*FindPrinter(char *name);
extern void		LoadAllPrinters(void);
extern void		SaveAllPrinters(void);
extern void		StartPrinter(printer_t *p);
extern void		StopPrinter(printer_t *p);


/*
 * End of "$Id: printers.h,v 1.8 1999/04/21 14:14:57 mike Exp $".
 */
