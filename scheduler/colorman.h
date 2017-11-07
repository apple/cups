/*
 * Color management definitions for the CUPS scheduler.
 *
 * Copyright 2007-2012 by Apple Inc.
 * Copyright 1997-2007 by Easy Software Products, all rights reserved.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more information.
 */

/*
 * Prototypes...
 */

extern void	cupsdRegisterColor(cupsd_printer_t *p);
extern void	cupsdStartColor(void);
extern void	cupsdStopColor(void);
extern void	cupsdUnregisterColor(cupsd_printer_t *p);
