/*
 * "$Id: cups.h,v 1.4 1999/02/05 17:40:50 mike Exp $"
 *
 *   API definitions for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-1999 by Easy Software Products.
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

#ifndef _CUPS_CUPS_H_
#  define _CUPS_CUPS_H_

/*
 * Include necessary headers...
 */

#  include <cups/ipp.h>
#  include <cups/mime.h>
#  include <cups/ppd.h>


/*
 * C++ magic...
 */

#  ifdef _cplusplus
extern "C" {
#  endif /* _cplusplus */


/*
 * Constants...
 */

#  define CUPS_VERSION		1.0
#  define CUPS_DATE_ANY		-1


/*
 * Types and structures...
 */

typedef enum				/**** Printer Type/Capability Bits ****/
{
  CUPS_PRINTER_CLASS = 0x0001,		/* Printer class */
  CUPS_PRINTER_REMOTE = 0x0002,		/* Remote printer or class */
  CUPS_PRINTER_BW = 0x0004,		/* Can do B&W printing */
  CUPS_PRINTER_COLOR = 0x0008,		/* Can do color printing */
  CUPS_PRINTER_DUPLEX = 0x0010,		/* Can do duplexing */
  CUPS_PRINTER_STAPLE = 0x0020,		/* Can staple output */
  CUPS_PRINTER_COPIES = 0x0040,		/* Can do copies */
  CUPS_PRINTER_COLLATE = 0x0080,	/* Can collage copies */
  CUPS_PRINTER_PUNCH = 0x0100,		/* Can punch output */
  CUPS_PRINTER_COVER = 0x0200,		/* Can cover output */
  CUPS_PRINTER_BIND = 0x0400,		/* Can bind output */
  CUPS_PRINTER_SORT = 0x0800,		/* Can sort output */
  CUPS_PRINTER_SMALL = 0x1000,		/* Can do Letter/Legal/A4 */
  CUPS_PRINTER_MEDIUM = 0x2000,		/* Can do Tabloid/B/C/A3/A2 */
  CUPS_PRINTER_LARGE = 0x4000,		/* Can do D/E/A1/A0 */
  CUPS_PRINTER_VARIABLE = 0x8000	/* Can do variable sizes */
} cups_ptype_t;


/*
 * Types & structures...
 */

typedef struct				/**** Printer Information ****/
{
  char		name[IPP_MAX_NAME],	/* Printer or class name */
		uri[HTTP_MAX_URI];	/* Universal resource identifier */
  unsigned char	info[IPP_MAX_NAME],	/* Printer or class info/description */
		location[IPP_MAX_NAME];	/* Location text */
  ipp_pstate_t	state;			/* Printer state */
  unsigned char	message[IPP_MAX_NAME];	/* State text */
  cups_ptype_t	type;			/* Printer type/capability codes */
} cups_browse_t;

typedef struct				/**** Printer Options ****/
{
  char		*name;			/* Name of option */
  char		*value;			/* Value of option */
} cups_option_t;


/*
 * Functions...
 */

extern int		cupsCancelJob(char *printer, int job);
extern int		cupsGetClasses(char ***classes);
extern char		*cupsGetPPD(char *printer);
extern int		cupsGetPrinters(char ***printers);
extern int		cupsPrintFile(char *printer, char *filename,
			              int num_options, cups_option_t *options);

extern int		cupsAddOption(char *name, char *value, int num_options,
			              cups_option_t **options);
extern void		cupsFreeOptions(int num_options, cups_option_t *options);
extern char		*cupsGetOption(char *name, int num_options,
			               cups_option_t *options);
extern int		cupsParseOptions(char *arg, cups_option_t **options);

#  ifdef _cplusplus
}
#  endif /* _cplusplus */

#endif /* !_CUPS_CUPS_H_ */

/*
 * End of "$Id: cups.h,v 1.4 1999/02/05 17:40:50 mike Exp $".
 */
