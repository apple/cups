/* Copyright (C) 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
  This file is part of GNU Ghostscript.
  
  GNU Ghostscript is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY.  No author or distributor accepts responsibility to
  anyone for the consequences of using it or for whether it serves any
  particular purpose or works at all, unless he says so in writing.  Refer to
  the GNU General Public License for full details.
  
  Everyone is granted permission to copy, modify and redistribute GNU
  Ghostscript, but only under the conditions described in the GNU General
  Public License.  A copy of this license is supposed to have been given to
  you along with GNU Ghostscript so you can know your rights and
  responsibilities.  It should be in a file named COPYING.  Among other
  things, the copyright notice and this notice must be preserved on all
  copies.
  
  Aladdin Enterprises is not affiliated with the Free Software Foundation or
  the GNU Project.  GNU Ghostscript, as distributed by Aladdin Enterprises,
  does not depend on any other GNU software.
*/

/* gdevstc1.c */
/* Epson Stylus-Color Printer-Driver */

/***
     This file holds the sample-implementation of a monochrome-algorithm for
     the stcolor-driver. It is available via

          gs -sDEVICE=stcolor -sDithering=gsmono ...

     Actually this is no dithering-algorithm, it lets ghostscript do the job.
     This achieved, by requesting BYTE-Values between 0 and 1 to be delivered,
     which causes a depth of 1-Bit by default.

 ***/

/*
 * gdevstc.h holds all the includes and the driver-specific definitions, so
 * it is the only include you need. To add a new algorthim, STC_MODI in
 * gdevstc.h should be extended. (see the instructions there)
 */

#include "gdevstc.h"

/*
 * the routine required.
 */

/*ARGSUSED*/
int 
stc_gsmono(stcolor_device *sdev,int npixel,byte *in,byte *buf,byte *out) 
{

/*
 * There are basically 3 Types of calls:
 * npixel < 0    => initialize buf, if this is required
 *                  (happens only if requested)
 * npixel > 0    => process next scanline, if the flag STC_WHITE is set, then
 *                  in == NULL signals, that the basic-driver has decided
 *                  that this scanline is white. (Useful for really dithering
 *                  drivers)
 */

/* ============================================================= */
   if(npixel > 0) {  /* npixel >  0 -> scanline-processing       */
/* ============================================================= */

/*    -----------------------------------------------*/
      if(in != NULL) { /* normal processing          */
/*    -----------------------------------------------*/

         memcpy(out,in,npixel); /* really simple algorithm */

/*    -----------------------------------------------*/
      } else {                  /* skip-notification */
/*    -----------------------------------------------*/

         /* An algorithm may use the output-line as a buffer.
            So it might need to be cleared on white-lines.
         */

         memset(out,0,npixel);

/*    -----------------------------------------------*/
      }                             /* normal / skip */
/*    -----------------------------------------------*/

/* ============================================================= */
   } else {          /* npixel <= 0 -> initialisation            */
/* ============================================================= */
/*
 *    the optional buffer is already allocated by the basic-driver, here
 *    you just need to fill it, for instance, set it all to zeros:
 */
     int buf_size;

/*
 * compute the size of the buffer, according to the requested values
 * the buffer consists of a constant part, e.g. related to the number
 * of color-components, and a number of arrays, which are multiples of
 * the size of a scanline times the number of components.
 * additionally, the size of the scanlines may be expanded by one to the
 * right and to the left.
 */
     buf_size = 
           sdev->stc.dither->bufadd              /* scanline-independend size */
             + (-npixel)                                     /* pixels */
               * (sdev->stc.dither->flags/STC_SCAN)          /* * scanlines */
               * sdev->color_info.num_components;            /* * comp */

     if(buf_size > 0) { /* we obviously have a buffer */
        memset(buf,0,buf_size * sdev->stc.alg_item);
     }                  /* we obviously have a buffer */

/*
 * Usually one should check parameters upon initializaon
 */
     if(sdev->color_info.num_components         !=        1) return -1;

     if((sdev->stc.dither->flags & STC_TYPE)    != STC_BYTE) return -2;

/*
 * must neither have STC_DIRECT nor STC_WHITE
 */
      if((sdev->stc.dither->flags & STC_DIRECT) !=        0) return -3;

   } /* scanline-processing or initialisation */

   return 0; /* negative values are error-codes, that abort printing */
}
