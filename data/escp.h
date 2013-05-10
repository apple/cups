/*
 * "$Id$"
 *
 *   This file contains model number definitions for the CUPS unified
 *   ESC/P driver.
 *
 *   Copyright 2007 by Apple Inc.
 *   Copyright 1997-2005 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 */

/* General ESC/P Support */
#define ESCP_DOTMATRIX		0x1		/* Dot matrix printer? */
#define ESCP_MICROWEAVE		0x2		/* Use microweave command? */
#define ESCP_STAGGER		0x4		/* Are color jets staggered? */
#define ESCP_ESCK		0x8		/* Use print mode command?*/
#define ESCP_EXT_UNITS		0x10		/* Use extended unit commands? */
#define ESCP_EXT_MARGINS	0x20		/* Use extended margin command */
#define ESCP_USB		0x40		/* Send USB packet mode escape? */
#define ESCP_PAGE_SIZE		0x80		/* Use page size command */
#define ESCP_RASTER_ESCI	0x100		/* Use ESC i graphics command */

/* Remote mode support */
#define ESCP_REMOTE		0x1000		/* Use remote mode commands? */


/*
 * End of "$Id$".
 */
