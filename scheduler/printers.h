/*
 * "$Id: printers.h,v 1.3 1999/01/24 14:25:11 mike Exp $"
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

/**** INCLUDE LIBCUPS/CUPS.H EVENTUALLY... ****/
/*
 * Printer status codes...
 */

#  define CUPS_PRINTER_IDLE             0x00
#  define CUPS_PRINTER_BUSY             0x01
#  define CUPS_PRINTER_FAULTED          0x02
#  define CUPS_PRINTER_UNAVAILABLE      0x03
#  define CUPS_PRINTER_DISABLED         0x04
#  define CUPS_PRINTER_REJECTING        0x08

/*
 * Printer type/capability codes...
 */

#  define CUPS_PRINTER_BW               0x01    /* Can do B&W printing */
#  define CUPS_PRINTER_COLOR            0x02    /* Can do color printing */
#  define CUPS_PRINTER_DUPLEX           0x04    /* Can do duplexing */
#  define CUPS_PRINTER_ADVANCED         0x08    /* Can sort/staple output */
#  define CUPS_PRINTER_SMALL            0x10    /* Can do letter/a4 */
#  define CUPS_PRINTER_MEDIUM           0x20    /* Can do tabloid/a3 */
#  define CUPS_PRINTER_LARGE            0x40    /* Can do C/D/E/A2/A1/A0 */
#  define CUPS_PRINTER_CLASS            0x80    /* Printer class */

/*
 * Printer information structure...
 */

typedef struct printer_str
{
  struct printer_str *next;		/* Next printer in list */
  char		uri[MAX_URI],		/* Printer URI */
		hostname[MAX_HOST];	/* Host printer resides on */
  unsigned char	name[MAX_NAME],		/* Printer name */
		location_code[MAX_NAME],/* Location code */
		location_text[MAX_NAME],/* Location text */
		info[MAX_NAME],		/* Description */
		more_info[MAX_URI],	/* URL for site-specific info */
		make_model[MAX_NAME],	/* Make and model from PPD file */
		username[MAX_NAME],	/* Username for remote system */
		password[MAX_NAME];	/* Password for remote system */
  int		state,			/* Printer state */
		type;			/* Printer type (color, small, etc.) */
  time_t	state_time;		/* Time at this state */
  char		ppd[MAX_URI],		/* PPD file name */
		device_uri[MAX_URI];	/* Device URI */
  void		*job;			/* Current job in queue */
} printer_t;


/*
 * Globals...
 */

VAR printer_t	*Printers VALUE(NULL);	/* Printer list */


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
 * End of "$Id: printers.h,v 1.3 1999/01/24 14:25:11 mike Exp $".
 */
