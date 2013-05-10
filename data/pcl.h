/*
 * "$Id$"
 *
 *   This file contains model number definitions for the CUPS unified
 *   PCL driver.
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

/* General PCL Support */
#define PCL_PAPER_SIZE		0x1		/* Use ESC&l#A */
#define PCL_INKJET		0x2		/* Use inkjet commands */

/* Raster Support */
#define PCL_RASTER_END_COLOR	0x100		/* Use ESC*rC */
#define PCL_RASTER_CID		0x200		/* Use ESC*v#W */
#define PCL_RASTER_CRD		0x400		/* Use ESC*g#W */
#define PCL_RASTER_SIMPLE	0x800		/* Use ESC*r#U */
#define PCL_RASTER_RGB24	0x1000		/* Use 24-bit RGB mode */

/* PJL Support */
#define PCL_PJL			0x10000		/* Use PJL Commands */
#define PCL_PJL_PAPERWIDTH	0x20000		/* Use PJL PAPERWIDTH/LENGTH */
#define PCL_PJL_HPGL2		0x40000		/* Enter HPGL2 */
#define PCL_PJL_PCL3GUI		0x80000		/* Enter PCL3GUI */
#define PCL_PJL_RESOLUTION	0x100000	/* Use PJL SET RESOLUTION */


/*
 * End of "$Id$".
 */
