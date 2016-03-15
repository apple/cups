/*
 * "$Id: colorman.h 3833 2012-05-23 22:51:18Z msweet $"
 *
 *   Color management definitions for the CUPS scheduler.
 *
 *   Copyright 2007-2012 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 */

/*
 * Prototypes...
 */

extern void	cupsdRegisterColor(cupsd_printer_t *p);
extern void	cupsdStartColor(void);
extern void	cupsdStopColor(void);
extern void	cupsdUnregisterColor(cupsd_printer_t *p);


/*
 * End of "$Id: colorman.h 3833 2012-05-23 22:51:18Z msweet $".
 */
