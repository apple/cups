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

/* gdevstc2.c */
/* Epson Stylus-Color Printer-Driver */

/***
     This file holds two implementations of the Floyd-Steinberg error
     diffusion-algorithm. This algorithms are intended for high quality
     printing in conjunction with the PostScript-Header stcolor.ps:

          gs -sDEVICE=stcolor <other options> stcolor.ps ...

     Most prominent option is -sDithering=xxx, to select the algorithm:

     fsmono - monochrome Floyd-Steinberg
     fsrgb  - 3-Component Floyd-Steinberg
     fsx4   - 4-Component Floyd-Steinberg (Bad results)

     fscmyk - Modified 4-Component Floyd-Steinberg
              (Algorithmically identical with hscmyk, but slower)

 ***/

#include "gdevstc.h"

#include <stdlib.h>     /* for rand */

/*
   Both algorithms require an error-buffer of 

       3 + 3*num_components +1*scan long-items.

   and must consequently set up to work with longs. 
   It is just a Floyd-Steinberg-algorithm applied to each component.

 */

/*
 * Due to the -selfdefined- ugly coding of the output-data, we need
 * some conversion. But since this includes the black-separation, I
 * did not change the definition.
 *
 * This algorithm stores the 1st component in the LSB, thus it
 * reverts the order used by the basic driver.
 */

static const byte grayvals[2]  = { 0, BLACK };

static const byte  rgbvals[8]  = {
   0, RED, GREEN, RED|GREEN, BLUE, BLUE|RED, BLUE|GREEN, BLUE|RED|GREEN};

static const byte cmykvals[16] = {
      0, CYAN,MAGENTA,CYAN|MAGENTA,YELLOW,YELLOW|CYAN,YELLOW|MAGENTA,BLACK,
  BLACK,BLACK,  BLACK,       BLACK, BLACK,      BLACK,         BLACK,BLACK};

static const byte  *const pixelconversion[5] = {
   NULL, grayvals, NULL, rgbvals, cmykvals};


int 
stc_fs(stcolor_device *sdev,int npixel,byte *bin,byte *bbuf,byte *out) 
{

     long *in  = (long *) bin;
     long *buf = (long *) bbuf;

/* ============================================================= */
   if(npixel > 0) {  /* npixel >  0 -> scanline-processing       */
/* ============================================================= */

      int bstep,pstart,pstop,pstep,p;
      long spotsize,threshold,*errc,*errv;
      const byte *pixel2stc;

      if(buf[0] >= 0) { /* run forward */
        buf[0] = -1;
        bstep  = 1;
        pstep  = sdev->color_info.num_components;
        pstart = 0;
        pstop  = npixel * pstep;

      } else {                  /* run backward */
        buf[0] =  1;
        bstep  = -1;
        pstep  = -sdev->color_info.num_components;
        pstop  = pstep;
        pstart = (1-npixel) * pstep;
        out   += npixel-1;
      }                   /* forward / backward */

/*    --------------------------------------------------------------------- */
      if(in == NULL) return 0;  /* almost ignore the 'white calls' */
/*    --------------------------------------------------------------------- */

      spotsize  = buf[1];
      threshold = buf[2];
      errc      = buf+3;
      errv      = errc + 2*sdev->color_info.num_components;
      pixel2stc = pixelconversion[sdev->color_info.num_components];

      for(p = pstart; p != pstop; p += pstep) { /* loop over pixels */
         int c;     /* component-number */
         int pixel; /* internal pxel-value */

         pixel = 0;

         for(c = 0; c < sdev->color_info.num_components; c++) { /* comp */
            long cv; /* component value */

            cv = in[p+c] + errv[p+c] + errc[c] - ((errc[c]+4)>>3);
            if(cv > threshold) {
               pixel |= 1<<c;
               cv    -= spotsize;
            }
            errv[p+c-pstep] += ((3*cv+8)>>4);        /* 3/16 */
            errv[p+c      ]  = ((5*cv  )>>4)         /* 5/16 */
                             + ((errc[c]+4)>>3);     /* 1/16 (rest) */
            errc[c]          = cv                    /* 8/16 (neu) */
                             - ((5*cv  )>>4)
                             - ((3*cv+8)>>4);
         }                                                      /* comp */

         *out = pixel2stc[pixel];
         out += bstep;
      }                                         /* loop over pixels */


/* ============================================================= */
   } else {          /* npixel <= 0 -> initialisation            */
/* ============================================================= */

      int i,i2do;
      long rand_max;
      double offset,scale;

/*
 * check wether the number of components is valid
 */
      if((sdev->color_info.num_components < 0)                         ||
         (sdev->color_info.num_components >= countof(pixelconversion)) ||
         (pixelconversion[sdev->color_info.num_components] == NULL)) return -1;

/*
 * check wether stcdither & TYPE are correct
 */
      if(( sdev->stc.dither                    == NULL) ||
         ((sdev->stc.dither->flags & STC_TYPE) != STC_LONG))         return -2;

/*
 * check wether the buffer-size is sufficiently large
 */
      if(((sdev->stc.dither->flags/STC_SCAN) < 1) ||
         ( sdev->stc.dither->bufadd          <
          (3 + 3*sdev->color_info.num_components)))                  return -3;
/*
 * must neither have STC_DIRECT nor STC_WHITE
 */
      if(sdev->stc.dither->flags & (STC_DIRECT | STC_WHITE))         return -4;

/*
 * compute initial values
 */
/* -- direction */
     buf[0] = 1;

/* -- "spotsize" */
     scale  = sdev->stc.dither->minmax[1];
     buf[1] = scale + (scale > 0.0 ? 0.5 : -0.5);

/* -- "threshold" */
     offset = sdev->stc.dither->minmax[0];
     scale -= offset;
     if((offset+0.5*scale) > 0.0) buf[2] = offset + 0.5*scale + 0.5;
     else                         buf[2] = offset + 0.5*scale - 0.5;

/*
 *   random values, that do not exceed half of normal value
 */
     i2do  = sdev->color_info.num_components * (3-npixel);
     rand_max = 0;

     if(sdev->stc.flags & STCDFLAG0) {

        for(i = 0; i < i2do; ++i) buf[i+3] = 0;

     } else {

        for(i = 0; i < i2do; ++i) {
           buf[i+3] = rand();
           if(buf[i+3] > rand_max) rand_max = buf[i+3];
        }

        scale = (double) buf[1] / (double) rand_max;

        for(i = 0; i < sdev->color_info.num_components; ++ i)
           buf[i+3] = 0.25000*scale*(buf[i+3]-rand_max/2);

        for(     ; i < i2do; ++i) /* includes 2 additional pixels ! */
           buf[i+3] = 0.28125*scale*(buf[i+3]-rand_max/2);

     }

/* ============================================================= */
   } /* scanline-processing or initialisation */
/* ============================================================= */

   return 0;
}

/*
 * Experimental CMYK-Algorithm
 */

int 
stc_fscmyk(stcolor_device *sdev,int npixel,byte *bin,byte *bbuf,byte *out) 
{
      long *in  = (long *) bin;
      long *buf = (long *) bbuf;

/* ============================================================= */
   if(npixel > 0) {  /* npixel >  0 -> scanline-processing       */
/* ============================================================= */

      int bstep,pstart,pstop,pstep,p;
      long spotsize,threshold,*errc,*errv;

      if(buf[0] >= 0) { /* run forward */
        buf[0] = -1;
        bstep  = 1;
        pstep  = 4;
        pstart = 0;
        pstop  = npixel * pstep;

      } else {                  /* run backward */
        buf[0] =  1;
        bstep  = -1;
        pstep  = -4;
        pstop  = pstep;
        pstart = (1-npixel) * pstep;
        out   += npixel-1;
      }                   /* forward / backward */

      spotsize  = buf[1];
      threshold = buf[2];
      errc      = buf+3;
      errv      = errc + 2*4;

      for(p = 0; p < 4; ++p) errc[p] = 0;

      for(p = pstart; p != pstop; p += pstep) { /* loop over pixels */
         int c;     /* component-number */
         int pixel; /* internal pxel-value */
         long cv,k;

/*
 * Black is treated first, with conventional Floyd-Steinberg
 */
         k  = in[p+3];
         cv = k + errv[p+3] + errc[3] - ((errc[3]+4)>>3);

         if(cv > threshold) {
            pixel  = BLACK;
            cv    -= spotsize;
         } else {
            pixel  = 0;
         }

         errv[p+3-pstep] += ((3*cv+8)>>4);        /* 3/16 */
         errv[p+3      ]  = ((5*cv  )>>4)         /* 5/16 */
                          + ((errc[3]+4)>>3);     /* 1/16 (rest) */
         errc[3]          = cv                    /* 8/16 (neu) */
                          - ((5*cv  )>>4)
                          - ((3*cv+8)>>4);

/*
 * color-handling changes with black fired or not
 */
         if(pixel) {

/* -------- firing of black causes all colors to fire too */

            for(c = 0; c < 3; ++c) {
               cv  = in[p+c] > k ? in[p+c] : k;
               cv += errv[p+c] + errc[c] - ((errc[c]+4)>>3)-spotsize;
               if(cv <= (threshold-spotsize)) cv = threshold-spotsize+1;

               errv[p+c-pstep] += ((3*cv+8)>>4);        /* 3/16 */
               errv[p+c      ]  = ((5*cv  )>>4)         /* 5/16 */
                                + ((errc[c]+4)>>3);     /* 1/16 (rest) */
               errc[c]          = cv                    /* 8/16 (neu) */
                                - ((5*cv  )>>4)
                                - ((3*cv+8)>>4);
            }

         } else {

/* -------- if black did not fire, only colors w. larger values may fire */

            for(c = 0; c < 3; ++c) {

               cv  = in[p+c];

               if(cv > k) { /* May Fire */
                  cv += errv[p+c] + errc[c] - ((errc[c]+4)>>3);
                  if(cv > threshold) {
                     cv -= spotsize;
                     pixel |= CYAN>>c;
                  }
               } else {     /* Must not fire */
                  cv = k + errv[p+c] + errc[c] - ((errc[c]+4)>>3);
                  if(cv > threshold ) cv =  threshold;
               }

               errv[p+c-pstep] += ((3*cv+8)>>4);        /* 3/16 */
               errv[p+c      ]  = ((5*cv  )>>4)         /* 5/16 */
                                + ((errc[c]+4)>>3);     /* 1/16 (rest) */
               errc[c]          = cv                    /* 8/16 (neu) */
                                - ((5*cv  )>>4)
                                - ((3*cv+8)>>4);
            }
         }

         *out = pixel;
         out += bstep;
      }                                         /* loop over pixels */


/* ============================================================= */
   } else {          /* npixel <= 0 -> initialisation            */
/* ============================================================= */

      int i,i2do;
      long rand_max;
      double offset,scale;

/*
 * check wether the number of components is valid
 */
      if(sdev->color_info.num_components != 4)                       return -1;

/*
 * check wether stcdither & TYPE are correct
 */
      if(( sdev->stc.dither                    == NULL) ||
         ((sdev->stc.dither->flags & STC_TYPE) != STC_LONG))         return -2;

/*
 * check wether the buffer-size is sufficiently large
 */
      if(((sdev->stc.dither->flags/STC_SCAN) < 1) ||
         ( sdev->stc.dither->bufadd          <
          (3 + 3*sdev->color_info.num_components)))                  return -3;
/*
 * must neither have STC_DIRECT nor STC_WHITE
 */
      if(sdev->stc.dither->flags & (STC_DIRECT | STC_WHITE))         return -4;

/*
 * compute initial values
 */
/* -- direction */
     buf[0] = 1;

/* -- "spotsize" */
     scale  = sdev->stc.dither->minmax[1];
     buf[1] = scale + (scale > 0.0 ? 0.5 : -0.5);

/* -- "threshold" */
     offset = sdev->stc.dither->minmax[0];
     scale -= offset;
     if(sdev->stc.flags & STCDFLAG1) {
        buf[2] = (sdev->stc.extv[0][sdev->stc.sizv[0]-1] - sdev->stc.extv[0][0])
               * scale / 2.0 + offset;
     } else {
        if((offset+0.5*scale) > 0.0) buf[2] = offset + 0.5*scale + 0.5;
        else                         buf[2] = offset + 0.5*scale - 0.5;
     }

/*
 *   random values, that do not exceed half of normal value
 */
     i2do  = sdev->color_info.num_components * (3-npixel);
     rand_max = 0;

     if(sdev->stc.flags & STCDFLAG0) {

        for(i = 0; i < i2do; ++i) buf[i+3] = 0;

     } else {

        for(i = 0; i < i2do; ++i) {
           buf[i+3] = rand();
           if(buf[i+3] > rand_max) rand_max = buf[i+3];
        }

        scale = (double) buf[1] / (double) rand_max;

        for(i = 0; i < sdev->color_info.num_components; ++ i)
           buf[i+3] = 0.25000*scale*(buf[i+3]-rand_max/2);

        for(     ; i < i2do; ++i) /* includes 2 additional pixels ! */
           buf[i+3] = 0.28125*scale*(buf[i+3]-rand_max/2);

     }

/* ============================================================= */
   } /* scanline-processing or initialisation */
/* ============================================================= */

   return 0;
}
