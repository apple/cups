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

/* gdevstc.c */
/* Epson Stylus-Color Printer-Driver */

/***
 *** This file was "copied" from gdevcdj.c (ghostscript-3.12), which was
 *** contributed by:
 ***    George Cameron      - g.cameron@biomed.abdn.ac.ukis
 ***    Koert Zeilstra      - koert@zen.cais.com
 ***    Eckhard Rueggeberg  - eckhard@ts.go.dlr.de
 ***
 *** Some of the ESC/P2-code was drawn from gdevescp.c, contributed by
 ***    Richard Brown       - rab@eos.ncsu.edu
 ***
 *** The POSIX-Interrupt-Code is from (Compile-Time-Option -DSTC_SIGNAL)
 ***    Frederic Loyer      - loyer@ensta.fr
 ***
 *** And several improvements are based on discussions with
 ***    Brian Converse      - BCONVERSE@ids.net
 ***    Bill Davidson       - bdavidson@ra.isisnet.com
 ***    Gero Guenther       - gero@cs.tu-berlin.de
 ***    Jason Patterson     - jason@reflections.com.au
 ***    ? Rueschstroer      - rue@ibe.med.uni-muenchen.de
 ***    Steven Singer       - S.Singer@ph.surrey.ac.uk
 ***
 *** And the remaining little rest, mainly the bugs, were written by me:
 *** Gunther Hess           - gunther@elmos.de
 ***
 *** P.S.: there is some documentation, see devices.doc
 ***
 *** Revision-History:
 *** 16-DEC-1994  1.1  - initial Version (GS-Dithering & Plain-Write)
     ...
 *** 30-JAN-1995  1.11 - FS-Improvements, u/sWeave, 1/4/24-Bits
 ***  5-MAR-1995  1.12 - L. Peter Deutsch - updated put_params routine
                         (first distributed version with gs3.33)
 *** 26-APR-1995  1.13 - merged Peters fixes with algorithmic changes:
                         Changed 24Bit-Mode, added 32Bit-Mode (moves colors)
                         [Arrgh: much better than 1.12, but patch was lost]
 ***  5-JUN-1995  1.14 - Added Color-Correction & Transfer-Curves
                         (Several Beta-Testers, but not distributed)
     ...
 *** 24-JUL-1995  1.16 - Made dithering-Algorithms external-functions.
                         (Mailed for Beta-Distribution)
 *** 10-AUG-1995  1.17 - Several Bug-Fixes and some new features:
                         CMYK10-Coding added
                         Readonly Parameters added
                          "Algorithms", "BitsPerComponent", "Version"
                         Parameters Flag0-4, Model, OutputCode
                         (mailed for distribution)
 *** 14-SEP-1995  1.18   Fixes Bugs with Borland C (gs3.47)
 *** 23-SEP-1995  1.19 - reorganized printcode + bug-fixing
 *** 24-SEP-1995  1.20 - Little Cleanup for the release
 *** 25-SEP-1995  1.21 - Readonly-Parameters added to put_params.
 *** 31-Dec-1995  1.22 - Sanitary Engineering on the code
 *** 16-Jan-1996  1.23 - Added String escp_Release
 ***  8-May-1996  1.90 - Reintroduced Deltarow & Fixed MEMORY-BUG!
 ***/

#include "gdevstc.h"
#ifdef    STC_SIGNAL
#  include <signal.h>
#endif /* STC_SIGNAL */
/***
 *** Mode-Table - the various algorithms
 *** (The intention is, that this source can live alone)
 ***/

private stc_proc_dither(stc_gscmyk);   /* resides in this file */
private stc_proc_dither(stc_hscmyk);   /* resides in this file */

#include <stdlib.h> /* for rand, used in stc_hscmyk */

private const stc_dither_t stc_dither[] = {
  {"gscmyk", stc_gscmyk, DeviceCMYK|STC_BYTE|STC_DIRECT,0,{0.0,1.0}},
  {"hscmyk", stc_hscmyk,
  DeviceCMYK|STC_LONG|STC_CMYK10|STC_DIRECT|1*STC_SCAN,1+2*4,
                                                  {0.0,    1023.0}},
  STC_MODI
  { NULL   , NULL      , 0,                  0,{0.0,0.0}}
};

/***
 ***  forward-declarations of routines
 ***/

/* Primary Device functions
 * (I've the idea to rename the driver to stc)
 */
private dev_proc_print_page(stc_print_page);
private dev_proc_open_device(stc_open);
private dev_proc_close_device(stc_close);
private dev_proc_get_params(stc_get_params);
private dev_proc_put_params(stc_put_params);

/*
 * Color-Mapping-functions.
 */

/* routines for monochrome monochrome modi */
private dev_proc_map_rgb_color(stc_map_gray_color);
private dev_proc_map_color_rgb(stc_map_color_gray);

/* routines for RGB-Modi */
private dev_proc_map_rgb_color(stc_map_rgb_color);
private dev_proc_map_color_rgb(stc_map_color_rgb);

/* routines for general CMYK-Modi */
private dev_proc_map_cmyk_color(stc_map_cmyk_color);
private dev_proc_map_color_rgb(stc_map_color_cmyk);

/* routines for 10Bit/Component CMYK */
private dev_proc_map_cmyk_color(stc_map_cmyk10_color);
private dev_proc_map_color_rgb(stc_map_color_cmyk10);

/***
 *** Table of Device-Procedures
 ***/
private gx_device_procs stcolor_procs = {
        stc_open,
        gx_default_get_initial_matrix,
        gx_default_sync_output,
        gdev_prn_output_page,
        stc_close,
        NULL,
        stc_map_color_cmyk,
        NULL,   /* fill_rectangle */
        NULL,   /* tile_rectangle */
        NULL,   /* copy_mono */
        NULL,   /* copy_color */
        NULL,   /* draw_line */
        gx_default_get_bits,
        stc_get_params,
        stc_put_params,
        stc_map_cmyk_color
};

/***
 *** A local dummy-array for extvals
 ***/

private float defext[] = { 0.0, 1.0 };

/***
 *** Main device-control structure
 ***/
stcolor_device far_data gs_stcolor_device = {
   prn_device_body(stcolor_device, stcolor_procs, "stcolor",
      DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
      X_DPI,  Y_DPI,
      STC_L_MARGIN,STC_B_MARGIN,STC_R_MARGIN,STC_T_MARGIN,
      4,  4, 1, 1, 2, 2,            /* default: cmyk-direct */
      stc_print_page),
     {STCNWEAVE,                    /* stcflags:  noWeave/bidirectional */
      1,                            /* stcbits:   matches the default */
      stc_dither,                   /* stcdither: first algorithm */
      NULL,                         /* stcam:     NULL -> not used */
      { NULL, NULL, NULL, NULL},    /* extcode:   none defined yet */
      {    0,    0,    0,    0},    /* sizcode:   0, since no extcode yet */
      { NULL, NULL, NULL, NULL},    /* stccode:   computed by put_params */
      {defext,defext,defext,defext},/* extvals:   default */
      {    2,    2,    2,    2},    /* sizvals:   default countof(defext) */
      { NULL, NULL, NULL, NULL},    /* stcvals:   computed by put_params */
      {    0,    0,    0},          /* white-run */
      {    0,    0,    0},          /* white-end */
      {NULL,0,false},               /* algorithm-table */
      {NULL,0,false},               /* initialization-String (BOP) */
      {NULL,0,false},               /* release-String (EOP) */
      0,0,0,0,                      /* New escp-stuff */
      1}                            /* itemsize used by algorithm */
};
/***
 *** Test for white scan-lines
 ***/
private bool stc_iswhite(P3(stcolor_device *, int, byte *));

/***
 *** Functions used for conversion inside the print-loop
 ***/
#define stc_proc_iconvert(Name) \
byte * Name(P4(stcolor_device *sd,byte *ext_data,int prt_pixels,byte *alg_line))

private stc_proc_iconvert(stc_any_depth);    /* general input-conversion */
private stc_proc_iconvert(stc_rgb24_long);   /* 24Bit RGB  -> long's */

private stc_proc_iconvert(stc_cmyk32_long);  /* 32Bit CMYK -> long's */
private stc_proc_iconvert(stc_any_direct);   /* use ext_data as input */

private stc_proc_iconvert(stc_cmyk10_byte);  /* CMYK10->vals-> any type */
private stc_proc_iconvert(stc_cmyk10_long);  /* CMYK10->vals-> any type */
private stc_proc_iconvert(stc_cmyk10_float); /* CMYK10->vals-> any type */
private stc_proc_iconvert(stc_cmyk10_dbyte); /* CMYK10 direct bytes */
private stc_proc_iconvert(stc_cmyk10_dlong); /* CMYK10 direct longs */

/***
 *** Print-functions
 ***/
private void stc_print_weave(P2(stcolor_device *sd,FILE *prn_stream));
private void stc_print_bands(P2(stcolor_device *sd,FILE *prn_stream));
private void stc_print_delta(P2(stcolor_device *sd,FILE *prn_stream));
private int  stc_print_setup(P1(stcolor_device *sd));

/***
 *** compute the ESC/P2 specific values
 ***/

private int 
stc_print_setup(stcolor_device *sd) 
{

/*
 * Compute the resolution-parameters
 */
   sd->stc.escp_u = 3600.0 / sd->y_pixels_per_inch; /* y-units */
   sd->stc.escp_h = 3600.0 / sd->x_pixels_per_inch; /* x-units */
   sd->stc.escp_v = sd->stc.flags & (STCUWEAVE | STCNWEAVE) ?
                    sd->stc.escp_u : 40;
/*
 * Initialize color
 */
   sd->stc.escp_c = 0; /* preselect-black */

/*
 * Band-Width
 */
   if((sd->stc.flags & STCBAND) == 0) {
      if(sd->stc.escp_v != sd->stc.escp_u)            sd->stc.escp_m = 15;
      else if(STCSTCII == (sd->stc.flags & STCMODEL)) sd->stc.escp_m =  1;
      else if(  sd->stc.flags & STCUWEAVE)            sd->stc.escp_m =  1;
      else if((sd->stc.escp_v == sd->stc.escp_u) &&
              (sd->stc.escp_u == 5))                  sd->stc.escp_m =  1;
      else                                            sd->stc.escp_m =  1;
   }

/*
 * Page-Dimensions
 */
   if((sd->stc.flags & STCWIDTH ) == 0)
       sd->stc.escp_width = sd->width -
           (dev_l_margin(sd)+dev_r_margin(sd))*sd->x_pixels_per_inch;

   if((sd->stc.flags & STCHEIGHT) == 0)
       sd->stc.escp_height = sd->height;

   if((sd->stc.flags & STCTOP) == 0)
       sd->stc.escp_top = dev_t_margin(sd)*sd->y_pixels_per_inch;

   if((sd->stc.flags & STCBOTTOM) == 0)
      sd->stc.escp_bottom = sd->height - dev_b_margin(sd)*sd->y_pixels_per_inch;

   if((sd->stc.flags & STCINIT) == 0) { /* No Initialization-String defined */
      int need  = 8  /* Reset, Graphics-Mode 1 */
                + 6  /* MicroWeave */
                + 6  /* Select Units */
                + 7  /* Set Page-Length */
                + 9  /* Set Margins */
                + 3; /* Select Unidirectionality */
      byte *bp  = (byte *) (sd->stc.escp_init.data);

      if(need != sd->stc.escp_init.size) {  /* Reallocate */

         if(NULL != (bp = gs_malloc(need,1,"stcolor/init"))) { /* Replace */
            if(0 != sd->stc.escp_init.size)
               gs_free((byte *)sd->stc.escp_init.data,sd->stc.escp_init.size,1,
                       "stcolor/init");
            sd->stc.escp_init.data       = bp;
            sd->stc.escp_init.size       = need;
            sd->stc.escp_init.persistent = false;
         }  else {                                             /* Replace */
             return_error(gs_error_VMerror);
         }
      }

      if(need != 39) return_error(gs_error_unregistered);

      memcpy(bp,
/*                       1 1 11  1 11  1   1 1  2 22 2  2 22  2 22 3  3 3333  3 33*/
/* 0 1  2 34  5  6 7  8 90 1 23  4 56  7   8 9  0 12 3  4 56  7 89 0  1 2345  6 78*/
"\033@\033(G\001\0\1\033(i\1\0w\033(U\001\000u\033(C\2\000hh\033(c\4\000ttbb\033U",
             need);


      if((sd->stc.flags & STCUWEAVE) != 0) bp[13] = '\1';
      else                                 bp[13] = '\0';

      bp[19] =  sd->stc.escp_u;

      bp[25] =  sd->stc.escp_height     & 0xff;
      bp[26] = (sd->stc.escp_height>>8) & 0xff;

      bp[32] =  sd->stc.escp_top        & 0xff;
      bp[33] = (sd->stc.escp_top>>8)    & 0xff;
      bp[34] =  sd->stc.escp_bottom     & 0xff;
      bp[35] = (sd->stc.escp_bottom>>8) & 0xff;

      if(sd->stc.flags & STCUNIDIR)        bp[38] = 1;
      else                                 bp[38] = 0;

   }                                    /* No Initialization-String defined */

   if((sd->stc.flags & STCRELEASE) == 0) { /* No Release-String defined */
      int need  = 3;  /* ESC @ \f */
      byte *bp  = (byte *) (sd->stc.escp_release.data);

      if(need != sd->stc.escp_release.size) {  /* Reallocate */

         if(NULL != (bp = gs_malloc(need,1,"stcolor/release"))) { /* Replace */
            if(0 != sd->stc.escp_release.size)
               gs_free((byte *)sd->stc.escp_release.data,sd->stc.escp_release.size,1,
                       "stcolor/release");
            sd->stc.escp_release.data       = bp;
            sd->stc.escp_release.size       = need;
            sd->stc.escp_release.persistent = false;
         }  else {                                             /* Replace */
             return_error(gs_error_VMerror);
         }
      }

      if(need != 3) return_error(gs_error_unregistered);

      memcpy(bp,"\033@\f",need);

   }                                    /* No Release-String defined */

   return 0;
}

/***
 *** stc_print_page: here we go to do the nasty work
 ***/

private int
stc_print_page(gx_device_printer * pdev, FILE * prn_stream)
{
   stcolor_device *sd    = (stcolor_device *) pdev;
   long            flags = sd == NULL ? 0 : sd->stc.flags;

   int  npass;           /* # of print-passes (softweave) */

   int    ext_size;      /* size of a ghostscript-scanline */
   byte  *ext_line;      /* dyn: for this scanline */

   int    alg_size;      /* size of a scanline for the dithering-algorithm */
   byte  *alg_line;      /* dyn: 1 scanline for the dithering-algorithm */
   int    buf_size;      /* size of the private-buffer for dither-function */
   byte  *buf;           /* dyn: the private buffer */

   int    prt_pixels;    /* Number of pixels printed */
   byte  *col_line;      /* A Line with a byte per pixel */

#define OK4GO        ((flags &   STCOK4GO)              != 0)
#define SORRY        ( flags &= ~STCOK4GO)

   if(0 > (npass = stc_print_setup(sd))) return_error(npass);

   npass = sd->stc.escp_v / sd->stc.escp_u;

/***
 *** Allocate dynamic memory
 ***/

   ext_size   = gdev_prn_raster(sd);
   ext_line   = gs_malloc(ext_size,1,"stc_print_page/ext_line");
   if(ext_line == NULL) SORRY;

   prt_pixels        = sd->stc.escp_width;
   sd->stc.prt_size  = (prt_pixels+7)/8;
   prt_pixels        =  sd->stc.prt_size * 8;

   sd->stc.prt_scans  = sd->height -
      (dev_t_margin(sd)+dev_b_margin(sd))*sd->y_pixels_per_inch;

   col_line   = gs_malloc(prt_pixels,1,"stc_print_page/col_line");
   if(col_line == NULL) SORRY;

   alg_size  = prt_pixels;
   alg_size *= sd->color_info.num_components;

   if((sd->stc.dither->flags & STC_DIRECT) ||
      ((sd->stc.bits                 == 8) &&
       (sd->stc.alg_item                     == 1)))  {
      alg_line = NULL;
   } else {
      alg_line = gs_malloc(alg_size,sd->stc.alg_item,"stc_print_page/alg_line");
      if(alg_line == NULL) SORRY;
   }

   buf_size = sd->stc.dither->bufadd
            + alg_size*(sd->stc.dither->flags/STC_SCAN);
   if(buf_size > 0) {
      buf    = gs_malloc(buf_size,sd->stc.alg_item,"stc_print_page/buf");
      if(buf == NULL) SORRY;
   } else {
      buf = NULL;
   }

/*
 * compute the number of printer-buffers
 */

    for(sd->stc.prt_buf   = 16; sd->stc.prt_buf < (sd->stc.escp_m * npass);
        sd->stc.prt_buf <<= 1);
    if(sd->color_info.num_components > 1) sd->stc.prt_buf *= 4;

    sd->stc.prt_width = gs_malloc(sd->stc.prt_buf,sizeof(int),
                        "stc_print_page/prt_width");
    if(sd->stc.prt_width == NULL) SORRY;

    sd->stc.prt_data  = gs_malloc(sd->stc.prt_buf,sizeof(byte *),
                        "stc_print_page/prt_data");

    if(sd->stc.prt_data == NULL) {
       SORRY;
    } else {
       int i;

       for(i = 0; i < sd->stc.prt_buf; ++i) {
          sd->stc.prt_data[i] = gs_malloc(sd->stc.prt_size,1,
                                "stc_print_page/prt");
          if(sd->stc.prt_data[i] == NULL) SORRY;
       }
    }

    sd->stc.seed_size = (sd->stc.prt_size + 2*sizeof(int) - 1)/sizeof(int);
    {
       int i;
       for(i = 0; i < sd->color_info.num_components; ++i) {
          if((flags & STCCOMP) == STCDELTA) {
             sd->stc.seed_row[i] = gs_malloc(sd->stc.seed_size,sizeof(int),
                                   "stc_print_page/seed_row");
             if(sd->stc.seed_row[i] == NULL) SORRY;
             else memset(sd->stc.seed_row[i],0,sd->stc.seed_size*sizeof(int));
          } else {
             sd->stc.seed_row[i] = NULL;
          }
       }
       while(i < countof(sd->stc.seed_row)) sd->stc.seed_row[i++] = NULL;
    }

    switch(flags & STCCOMP) {
       case STCPLAIN:
          sd->stc.escp_size = 64 + sd->stc.prt_size;
          break;
       case STCDELTA:
          sd->stc.escp_size = 64 + 2 * sd->stc.prt_size;
          break;
       default:
          sd->stc.escp_size = 64 +
                              sd->stc.prt_size + (sd->stc.prt_size + 127)/128;
          break;
    }

    sd->stc.escp_data = gs_malloc(sd->stc.escp_size,1,
                                  "stc_print_page/escp_data");
    if(sd->stc.escp_data == NULL) SORRY;

/*
 * If we're still ok, we can print something
 */

   if(OK4GO) {

      int ncolor;
      int buf_i;
      stc_proc_iconvert((*iconvert)) = stc_any_depth;

/*
 * initialize col_line
 */
      if(sd->color_info.num_components == 3) {
         memset(col_line,RED|GREEN|BLUE,prt_pixels);
      } else {
         memset(col_line,0,             prt_pixels);
      }

/*
 * select proper conversion for input to algorithm
 */
      if(     (sd->stc.dither->flags & STC_DIRECT ) ||
              ((sd->stc.bits                 == 8) &&
               (sd->stc.alg_item                     == 1)))
         iconvert = stc_any_direct;
      else if((sd->color_info.num_components ==  3) &&
              (sd->color_info.depth          == 24) &&
              (sizeof(long)                  == sd->stc.alg_item))
         iconvert = stc_rgb24_long;
      else if(sd->stc.flags & STCCMYK10) {
         if(     ((sd->stc.dither->flags & STC_TYPE) == STC_BYTE) &&
                 ( sd->stc.dither->minmax[0]         ==    0.0  ))
            iconvert = stc_cmyk10_dbyte;
         else if ((sd->stc.dither->flags & STC_TYPE) == STC_BYTE)
            iconvert = stc_cmyk10_byte;
         else if(((sd->stc.dither->flags & STC_TYPE) == STC_LONG) &&
                 ( sd->stc.dither->minmax[0]         ==    0.0  ) &&
                 ( sd->stc.dither->minmax[1]         <= 1023.0  ))
            iconvert = stc_cmyk10_dlong;
         else if( (sd->stc.dither->flags & STC_TYPE) == STC_LONG)
            iconvert = stc_cmyk10_long;
         else
            iconvert = stc_cmyk10_float;
      }
      else if((sd->color_info.num_components ==  4) &&
              (sd->color_info.depth          == 32) &&
              (sizeof(long)                  == sd->stc.alg_item))
         iconvert = stc_cmyk32_long;

/*
 * initialize the algorithm
 */

      if((*sd->stc.dither->fun)(sd,-prt_pixels,alg_line,buf,col_line) < 0)
         SORRY;

/*
 * Main-Print-Loop
 */

      if(OK4GO) {
#ifdef    STC_SIGNAL
         sigset_t stc_int_mask, stc_int_save, stc_int_pending;

         sigemptyset(&stc_int_mask);
         sigaddset(&stc_int_mask,SIGINT);
         sigprocmask(SIG_BLOCK,&stc_int_mask, &stc_int_save);
#endif /* STC_SIGNAL */


         if(sd->color_info.num_components > 1) ncolor = 4;
         else                                  ncolor = 1;

/*
 * Decide, wether we Adjust Linefeeds or not. (I hate it here)
 */
         if((0   == ((sd->stc.escp_m*sd->stc.escp_u) % 10)) &&
            (256  > ((sd->stc.escp_m*sd->stc.escp_u) / 10))) sd->stc.escp_lf = sd->stc.escp_m;
         else                                                sd->stc.escp_lf = 0;

/*
 * prepare run-values, then loop over scans
 */
         sd->stc.stc_y      =  0; /* current printer y-Position */
         sd->stc.buf_y      =  0; /* Top-Position within the buffer */
         sd->stc.prt_y      =  0; /* physical position of the printer */
         buf_i              =  0; /* next free line in buffer */
         sd->stc.flags     &= ~STCPRINT; /* no data yet */

         while(sd->stc.stc_y < sd->stc.prt_scans) {  /* Until all scans are processed */
            int need;

            need = sd->stc.stc_y + npass * sd->stc.escp_m;

            if(sd->stc.buf_y < need) { /* Nr. 5 (give me input) */

/* read as much as the buffer can hold */
               if(ncolor == 1) need = sd->stc.stc_y +  sd->stc.prt_buf;
               else            need = sd->stc.stc_y + (sd->stc.prt_buf>>2);

               for(;sd->stc.buf_y < need;
                    buf_i = (sd->stc.prt_buf-1) & (buf_i+ncolor),
                    ++sd->stc.buf_y) {

                  int color;
                  byte *ext_data;
                  byte *alg_data;

/* initialize output data 1st -> may take shortcut */

                  for(color = 0; color < ncolor; ++color) {
                     memset(sd->stc.prt_data[buf_i+color],0,sd->stc.prt_size);
                     sd->stc.prt_width[buf_i+color] = 0;
                  }


/* "read data", immediately continue if all is white */

                  if(sd->stc.buf_y < sd->stc.prt_scans) {  /* Test for White */

                     gdev_prn_get_bits(pdev,sd->stc.buf_y,ext_line,&ext_data);

                     color = stc_iswhite(sd,prt_pixels,ext_data) ? ext_size : 0;

                  } else {

                     color = ext_size;

                  }                        /* Test for White */

                  if(color >= ext_size) {  /* bypass processing */

                     if(sd->stc.dither->flags & STC_WHITE)
                        (*sd->stc.dither->fun)(sd,prt_pixels,NULL,buf,col_line);
                     continue;

                  }                        /* bypass processing */

/* convert data for the various cases */

                  alg_data = (*iconvert)(sd,ext_data,prt_pixels,alg_line);


/*
 * invoke the dithering-algorithm
 */

                  (*sd->stc.dither->fun)(sd,prt_pixels,alg_data,buf,col_line);
/*
 * convert col_line to printer-format (separate colors)
 */
                  switch(sd->color_info.num_components) {
                  case 1: /* Black & White: just merge into 8 Bytes */
                  {
                      byte *bytein,*byteout;
                      int   width;

                      bytein  = col_line;
                      byteout = sd->stc.prt_data[buf_i];

                      for(width = 1; width <= sd->stc.prt_size; ++width) {
                          byte tmp = 0;
                          byte i;

                          for(i = 128; i; i >>= 1) if(*bytein++) tmp  |= i;

                          if(tmp != 0) sd->stc.prt_width[buf_i] = width;

                          *byteout++ = tmp;
                      }
                  }
                  break;
                  case 3: /* convert rgb into cmyk */
                  {
                      byte *bytein;
                      int   width;

                      bytein  = col_line;

                      for(width = 0; width < sd->stc.prt_size; ++width) {
                         byte i,tmp,cmyk[4];

                         memset(cmyk,0,4);

                         for(i = 128; i; i >>= 1) {
                            static const byte rgb2cmyk[] = {
                               BLACK,            /* 0->Black */
                               CYAN | MAGENTA,   /* 1->BLUE  */
                               CYAN | YELLOW,    /* 2->GREEN */
                               CYAN,             /* 3->CYAN  */
                               MAGENTA | YELLOW, /* 4->RED   */
                               MAGENTA,          /* 5->MAGENTA */
                               YELLOW,           /* 6->YELLOW */
                               0};               /* 7->WHITE */

                            tmp = rgb2cmyk[(*bytein++) & 7];

                            if(tmp & BLACK)   cmyk[3] |= i;
                            if(tmp & YELLOW)  cmyk[2] |= i;
                            if(tmp & MAGENTA) cmyk[1] |= i;
                            if(tmp & CYAN)    cmyk[0] |= i;
                         }

                         for(i = 0; i < 4; ++i) {
                            if(cmyk[i] != 0) sd->stc.prt_width[buf_i+i] = width+1;
                            sd->stc.prt_data[buf_i+i][width] = cmyk[i];
                         }
                      }
                  }
                  break;
                  case 4: /* split cmyk */
                  {
                      byte *bytein;
                      int   width;

                      bytein  = col_line;

                      for(width = 0; width < sd->stc.prt_size; ++width) {
                         byte i,tmp,cmyk[4];

                         memset(cmyk,0,4);

                         for(i = 128; i; i >>= 1) {
                            tmp = (*bytein++) & 15;
                            if(tmp & BLACK)   cmyk[3] |= i;
                            if(tmp & YELLOW)  cmyk[2] |= i;
                            if(tmp & MAGENTA) cmyk[1] |= i;
                            if(tmp & CYAN)    cmyk[0] |= i;
                         }

                         for(i = 0; i < 4; ++i) {
                            if(cmyk[i] != 0) sd->stc.prt_width[buf_i+i] = width+1;
                            sd->stc.prt_data[buf_i+i][width] = cmyk[i];
                         }
                      }
                  }
                  break;
                  default: break;
                  }
               }
            }                  /* Nr. 5 (give me input) */

/*
 *    Nr. 5 has got enough input, now we should print it
 */
            if((flags & STCCOMP) == STCDELTA) stc_print_delta(sd,prn_stream);
            else if(npass > 1)                stc_print_weave(sd,prn_stream);
            else                              stc_print_bands(sd,prn_stream);

#ifdef    STC_SIGNAL
            sigpending(&stc_int_pending);
            if(sigismember(&stc_int_pending,SIGINT)) {
               fputs("\033@[Aborted]\f", prn_stream);
               fflush(prn_stream);
               sigprocmask(SIG_SETMASK,&stc_int_save,NULL);
               break;
            }
#endif /* STC_SIGNAL */

         }                           /* Until all scans are processed */

         if(sd->stc.flags & STCPRINT) {
            if((flags & STCCOMP) == STCDELTA) fputc(0xe3,prn_stream);
            fwrite(sd->stc.escp_release.data,1,sd->stc.escp_release.size,prn_stream);
            fflush(prn_stream);
         }
#ifdef    STC_SIGNAL
         sigprocmask(SIG_SETMASK,&stc_int_save,NULL);
#endif /* STC_DIGNAL */
  
      }
   }

/***
 *** Release the dynamic memory
 ***/

   if(ext_line != NULL)
      gs_free(ext_line,ext_size,1,"stc_print_page/ext_line");

   if(col_line != NULL)
      gs_free(col_line,prt_pixels,1,"stc_print_page/col_line");

   if(alg_line != NULL)
      gs_free(alg_line,alg_size,sd->stc.alg_item,
         "stc_print_page/alg_line");

   if(buf != NULL)
      gs_free(buf,buf_size,sd->stc.alg_item,"stc_print_page/buf");

    if(sd->stc.prt_width != NULL)
       gs_free(sd->stc.prt_width,sd->stc.prt_buf,sizeof(int),
       "stc_print_page/prt_width");

    if(sd->stc.prt_data != NULL) {
       int i;

       for(i = 0; i < sd->stc.prt_buf; ++i) {
          if(sd->stc.prt_data[i] != NULL)
             gs_free(sd->stc.prt_data[i],sd->stc.prt_size,1,
             "stc_print_page/prt");
       }

       gs_free(sd->stc.prt_data,sd->stc.prt_buf,sizeof(byte *),
       "stc_print_page/prt_data");
    }

    {
       int i;
       for(i = 0; i < sd->color_info.num_components; ++i) {
          if(sd->stc.seed_row[i] != NULL)
            gs_free(sd->stc.seed_row[i],sd->stc.seed_size,sizeof(int),
            "stc_print_page/seed_row");
       }
    }

    if(sd->stc.escp_data != NULL)
       gs_free(sd->stc.escp_data,sd->stc.escp_size,1,
       "stc_print_page/escp_data");

   return OK4GO ? 0 : gs_error_undefined;
}

/*
 * white-check
 */
private bool 
stc_iswhite(stcolor_device *sd, int prt_pixels,byte *ext_data)
{
   long  b2do = (prt_pixels*sd->color_info.depth+7)>>3;
   int   bcmp = 4 * countof(sd->stc.white_run);
   byte *wht  = (byte *) sd->stc.white_run;

   while(b2do >= bcmp) {
      if(memcmp(ext_data,wht,bcmp)) break;
      ext_data += bcmp;
      b2do     -= bcmp;
   }

   if((b2do > 0) && (b2do < bcmp))
      b2do  = memcmp(ext_data,sd->stc.white_end,b2do);

   return b2do ? false : true;
}

/***
 *** A bunch of routines that convert gslines into algorithms format.
 ***/
private byte *
stc_any_depth(stcolor_device *sd,byte *ext_data,int prt_pixels,byte *alg_line)
{ /* general conversion */

   int p,c,       niext,         nbits;
   gx_color_index ciext,ci,cimsk,cvmsk;
   byte          *ap = alg_line;

   nbits =  sd->stc.bits;
   cvmsk = ((gx_color_index) 1<<nbits) - 1;

/* it is nonsense to use this algorithm for this cases, but if it claims
 * generality, it should deliver correct results in this cases too */
   if(sd->color_info.depth == (sd->color_info.num_components<<3)) nbits = 8;

   cimsk = cvmsk;
   for(c = 1; c < sd->color_info.num_components; ++c)
       cimsk = (cimsk<<nbits) | cvmsk;

   ciext = 0;
   niext = 0;

   for(p = 0; p < prt_pixels; ++p) { /* over pixels */

      ci = ciext;
      for(c =  sd->color_info.depth-niext; c >= 8; c -= 8)
         ci  = (ci<<8) | *ext_data++;

      if(c > 0) {         /* partial byte required */

         niext  = 8 - c;
         ciext  = *ext_data++;
         ci     = (ci<<c) | (ciext>>niext);
         ciext &= (1L<<niext)-1;

      } else if(c < 0) { /* some bits left in ciext */

         niext  = -c;
         ciext &= (1L<<niext)-1;
         ci     = ci>>niext;

      } else {           /* entire ciext used */

         niext = 0;
         ciext = 0;

      }                  /* ciext-adjust */

      ci &= cimsk;

#     define stc_storeapc(T) \
         ((T *)ap)[c] = ((T *)(sd->stc.vals[c]))[ci & cvmsk];

      for(c = sd->color_info.num_components; c--;) { /* comp */
         STC_TYPESWITCH(sd->stc.dither,stc_storeapc)
         ci >>= nbits;
      }                                              /* comp */

#     undef  stc_storeapc

      ap += sd->color_info.num_components * sd->stc.alg_item;

   }                                 /* over pixels */

   return alg_line;
} /* general conversion */

/*
 * rgb-data with depth=24, can use a faster algorithm
 */
private byte *
stc_rgb24_long(stcolor_device *sd,byte *ext_data,int prt_pixels,byte *alg_line)
{ /* convert 3 bytes into appropriate long-Values */
  register int   p;
  register long *out   = (long *) alg_line;
  register long *rvals = (long *) (sd->stc.vals[0]);
  register long *gvals = (long *) (sd->stc.vals[1]);
  register long *bvals = (long *) (sd->stc.vals[2]);

  for(p = prt_pixels; p; --p) {
     *out++ = rvals[*ext_data++];
     *out++ = gvals[*ext_data++];
     *out++ = bvals[*ext_data++];
  }

  return alg_line;
} /* convert 3 bytes into appropriate long-Values */

/*
 * cmyk-data with depth=32, can use a faster algorithm
 */
private byte *
stc_cmyk32_long(stcolor_device *sd,byte *ext_data,int prt_pixels,byte *alg_line)
{ /* convert 4 bytes into appropriate long-Values */
  register int   p;
  register long *out   = (long *) alg_line;
  register long *cvals = (long *) (sd->stc.vals[0]);
  register long *mvals = (long *) (sd->stc.vals[1]);
  register long *yvals = (long *) (sd->stc.vals[2]);
  register long *kvals = (long *) (sd->stc.vals[3]);

  for(p = prt_pixels; p; --p) {
     *out++ = cvals[*ext_data++];
     *out++ = mvals[*ext_data++];
     *out++ = yvals[*ext_data++];
     *out++ = kvals[*ext_data++];
  }

  return alg_line;
} /* convert 4 bytes into appropriate long-Values */

/*
 * handle indirect encoded cmyk-data
 */
#define STC_CMYK10_ANY(T)\
                                                                            \
      register int p               = prt_pixels;                            \
      register stc_pixel      ci,k,n,mode;                                  \
      register stc_pixel      *in  = (stc_pixel *) ext_data;                \
      register T              *out = (T *) alg_line;                        \
      register T              *cv  = (T *) sd->stc.vals[0];                 \
      register T              *mv  = (T *) sd->stc.vals[1];                 \
      register T              *yv  = (T *) sd->stc.vals[2];                 \
      register T              *kv  = (T *) sd->stc.vals[3];                 \
                                                                            \
      while(p--) {                                                          \
         ci   = *in++;                                                      \
         mode = ci & 3;                                                     \
         k    = (ci>>2) & 0x3ff;                                            \
         if(mode == 3) {                                                    \
            *out++ = cv[0];                                                 \
            *out++ = mv[0];                                                 \
            *out++ = yv[0];                                                 \
            *out++ = kv[k];                                                 \
         } else {                                                           \
            out[3] = kv[k];                                                 \
            n = (ci>>12) & 0x3ff;                                           \
            if(mode == 2) { out[2] = yv[k]; }                               \
            else          { out[2] = yv[n]; n = (ci>>22) & 0x3ff; }         \
            if(mode == 1) { out[1] = mv[k]; }                               \
            else          { out[1] = mv[n]; n = (ci>>22) & 0x3ff; }         \
            if(mode == 0)   out[0] = cv[k];                                 \
            else            out[0] = cv[n];                                 \
            out += 4;                                                       \
         }                                                                  \
      }                                                                     \
                                                                            \
      return alg_line;

private byte *
stc_cmyk10_byte(stcolor_device *sd,
                byte *ext_data,int prt_pixels,byte *alg_line)
{
   STC_CMYK10_ANY(byte)
}
private byte *
stc_cmyk10_long(stcolor_device *sd,
                byte *ext_data,int prt_pixels,byte *alg_line)
{
   STC_CMYK10_ANY(long)
}
private byte *
stc_cmyk10_float(stcolor_device *sd,
                byte *ext_data,int prt_pixels,byte *alg_line)
{
   STC_CMYK10_ANY(float)
}

#undef  STC_CMYK10_ANY

#define STC_CMYK10_DANY(T)\
                                                                            \
      register int p               = prt_pixels;                            \
      register stc_pixel       ci,k,n,mode;                                 \
      register stc_pixel      *in  = (stc_pixel *) ext_data;                \
      register T              *out = (T *) alg_line;                        \
                                                                            \
      while(p--) {                                                          \
         ci   = *in++;                                                      \
         mode = ci & 3;                                                     \
         k    = (ci>>2) & 0x3ff;                                            \
         if(mode == 3) {                                                    \
            *out++ = 0;                                                     \
            *out++ = 0;                                                     \
            *out++ = 0;                                                     \
            *out++ = k;                                                     \
         } else {                                                           \
            out[3] = k;                                                     \
            n = (ci>>12) & 0x3ff;                                           \
            if(mode == 2) { out[2] = k; }                                   \
            else          { out[2] = n; n = (ci>>22) & 0x3ff; }             \
            if(mode == 1) { out[1] = k; }                                   \
            else          { out[1] = n; n = (ci>>22) & 0x3ff; }             \
            if(mode == 0)   out[0] = k;                                     \
            else            out[0] = n;                                     \
            out += 4;                                                       \
         }                                                                  \
      }                                                                     \
                                                                            \
      return alg_line;


private byte *
stc_cmyk10_dbyte(stcolor_device *sd,
                byte *ext_data,int prt_pixels,byte *alg_line)
{
   STC_CMYK10_DANY(byte)
}
private byte *
stc_cmyk10_dlong(stcolor_device *sd,
                byte *ext_data,int prt_pixels,byte *alg_line)
{
   STC_CMYK10_DANY(long)
}

#undef  STC_CMYK10_DANY

/*
 * if the algorithm uses bytes & bytes are in ext_data, use them
 */
/*ARGSUSED*/
private byte *
stc_any_direct(stcolor_device *sd,byte *ext_data,int prt_pixels,byte *alg_line)
{ /* return ext_data */
  return ext_data;
} /* return ext_data */

/* ----------------------------------------------------------------------- */
/* stc_rle: epson ESC/P2 RLE-Encoding
 */
private int 
stc_rle(byte *out,const byte *in,int width)
{

   int used = 0;
   int crun,cdata;
   byte run;

   if(in != NULL) { /* Data present */

      crun = 1;

      while(width > 0) { /* something to compress */

         run = in[0];

         while((width > crun) && (run == in[crun])) if(++crun == 129) break;

         if((crun > 2) || (crun == width)) { /* use this run */

            *out++ = (257 - crun) & 0xff; *out++ = run; used += 2;

            width -= crun; in    += crun;
            crun = 1;

         } else {                            /* ignore this run */

            for(cdata = crun; (width > cdata) && (crun < 4);) {
               if(run  == in[cdata]) crun += 1;
               else run = in[cdata], crun  = 1;
               if(++cdata == 128) break;
            }

            if(crun < 3) crun   = 0;    /* ignore trailing run */
            else         cdata -= crun;

            *out++ = cdata-1;     used++;
            memcpy(out,in,cdata); used += cdata; out   += cdata;

            width -= cdata; in    += cdata;

         }              /* use/ignore run */

      }                  /* something to compress */

   } else {         /* Empty scans to fill bands */

      while(width > 0) {
         crun   = width > 129 ? 129 : width;
         width -= crun;
         *out++ = (257 - crun) & 0xff;
         *out++ = 0;
         used  += 2;
      }
   }                /* Data present or empty */
   return used;
}


/*
 * Horizontal & vertical positioning, color-selection, "ESC ."
 */
private int 
stc_print_escpcmd(stcolor_device *sd, FILE *prn_stream,
   int escp_used, int color,int m,int wbytes)
{

   int dy  = sd->stc.stc_y - sd->stc.prt_y; /* number of units to skip */
   int nlf;

/* ESC-R color codes, used only here */
   static const byte stc_colors[] = { 0x02, 0x01, 0x04, 0x00 }; /* CMYK */

/*
 * initialize the printer, if necessary
 */
   if(0 == (sd->stc.flags & STCPRINT)) {

      fwrite(sd->stc.escp_init.data,1,sd->stc.escp_init.size,prn_stream);

      if(0 < sd->stc.escp_lf) { /* Adjust Linefeed */
         fputc('\033',        prn_stream);
         fputc('+',           prn_stream);
         fputc(((sd->stc.escp_m*sd->stc.escp_u) / 10),prn_stream);
      }                         /* Adjust Linefeed */
      sd->stc.flags |= STCPRINT;
   }

   sd->stc.escp_data[escp_used++]  = '\r';     /* leftmost position */

   if(dy) {                                    /* position the printer */
      if(( sd->stc.escp_lf      >  0) && /* Linefeed allowed */
         ((dy % sd->stc.escp_lf) == 0))   /* and possible */
            nlf = dy / sd->stc.escp_lf;
      else  nlf = 7;
         
      if(nlf > 6) {
         sd->stc.escp_data[escp_used++]  = '\033';
         sd->stc.escp_data[escp_used++]  = '(';
         sd->stc.escp_data[escp_used++]  = 'V';
         sd->stc.escp_data[escp_used++]  = '\002';
         sd->stc.escp_data[escp_used++]  = '\000';
         sd->stc.escp_data[escp_used++]  =  sd->stc.stc_y       & 0xff;
         sd->stc.escp_data[escp_used++]  = (sd->stc.stc_y >> 8) & 0xff;
      } else {
         while(nlf--) sd->stc.escp_data[escp_used++] = '\n';
      }
      sd->stc.prt_y = sd->stc.stc_y;
   }                                           /* position the printer */

   if((sd->color_info.num_components > 1) &&
      (sd->stc.escp_c != stc_colors[color])) { /* select color */
       sd->stc.escp_data[escp_used++]  = '\033';
       sd->stc.escp_data[escp_used++]  = 'r';
       sd->stc.escp_c                  = stc_colors[color];
       sd->stc.escp_data[escp_used++]  = sd->stc.escp_c;
   }                                           /* select color */

/*
 * Build the command used
 */
   sd->stc.escp_data[escp_used++] = '\033';
   sd->stc.escp_data[escp_used++] = '.';
   sd->stc.escp_data[escp_used++] =
       (sd->stc.flags & STCCOMP) == STCPLAIN ? 0 : 1;
   sd->stc.escp_data[escp_used++] = sd->stc.escp_v;
   sd->stc.escp_data[escp_used++] = sd->stc.escp_h;
   sd->stc.escp_data[escp_used++] = m;
   sd->stc.escp_data[escp_used++] = (wbytes<<3) & 0xff; /* width in Pixels */
   sd->stc.escp_data[escp_used++] = (wbytes>>5) & 0xff;

   return escp_used;
}

/*
 * compute width of a group of scanlines
 */
private int 
stc_bandwidth(stcolor_device *sd,int color,int m,int npass)
{
   int ncolor = sd->color_info.num_components == 1 ? 1 : 4;
   int buf_a  = (sd->stc.prt_buf-1) & (sd->stc.stc_y * ncolor + color);
   int w      = 0;

   while(m-- > 0) { /* check width */
      if(sd->stc.prt_width[buf_a] > w) w = sd->stc.prt_width[buf_a];
      buf_a = (sd->stc.prt_buf-1) & (buf_a + ncolor * npass);
   }                       /* check width */

   return w;
}

/*
 * Multi-Pass Printing-Routine
 */
private void 
stc_print_weave(stcolor_device *sd, FILE *prn_stream)
{

   int escp_used,nprint,nspace,color,buf_a,iprint,w;

   int npass  = sd->stc.escp_v / sd->stc.escp_u;
   int ncolor = sd->color_info.num_components == 1 ? 1 : 4;


   while(sd->stc.stc_y < sd->stc.prt_scans) {

/*
 * compute spacing & used heads (seems to work with odd escp_m)
 */
      if(sd->stc.stc_y >= sd->stc.escp_m) { /* in normal mode */
         nprint = sd->stc.escp_m;
         nspace = sd->stc.escp_m;
      } else if((sd->stc.stc_y) < npass) {                /* initialisation */
         nprint = sd->stc.escp_m - sd->stc.stc_y * ((sd->stc.escp_m+1)/npass);
         nspace = 1;
      } else {                                   /* switch to normal */
         nprint = sd->stc.escp_m - sd->stc.stc_y * ((sd->stc.escp_m+1)/npass);
         nspace = sd->stc.escp_m - sd->stc.stc_y;
      }
      iprint = sd->stc.stc_y + npass * nprint;
      if(sd->stc.buf_y < iprint) break;

      escp_used = 0;
      for(color = 0; color < ncolor; ++color) { /* print the colors */

         if(0 == (w = stc_bandwidth(sd,color,nprint,npass))) continue;

         escp_used = stc_print_escpcmd(sd,prn_stream,
                                       escp_used,color,sd->stc.escp_m,w);

         buf_a = (sd->stc.prt_buf-1) & (sd->stc.stc_y * ncolor + color);
         for(iprint = 0; iprint < nprint; ++iprint) { /* send data */

            if((sd->stc.flags & STCCOMP) == STCPLAIN) {
               memcpy(sd->stc.escp_data+escp_used,sd->stc.prt_data[buf_a],w);
               escp_used += w;
            } else {
               escp_used += stc_rle(sd->stc.escp_data+escp_used,
                                    sd->stc.prt_data[buf_a],w);
            }

            fwrite(sd->stc.escp_data,1,escp_used,prn_stream);
            escp_used = 0;

            buf_a = (sd->stc.prt_buf-1) & (buf_a + ncolor * npass);

         }                                            /* send data */

         while(iprint++ < sd->stc.escp_m) {  /* add empty rows */

            if((sd->stc.flags & STCCOMP) == STCPLAIN) {
               memset(sd->stc.escp_data+escp_used,0,w);
               escp_used += w;
            } else {
               escp_used += stc_rle(sd->stc.escp_data+escp_used,NULL,w);
            }

            fwrite(sd->stc.escp_data,1,escp_used,prn_stream);
            escp_used = 0;
         }                               /* add empty rows */
      }                                             /* print the colors */

      sd->stc.stc_y += nspace;
   }
}

/*
 * Single-Pass printing-Routine
 */
private void 
stc_print_bands(stcolor_device *sd, FILE *prn_stream)
{

   int escp_used,color,buf_a,iprint,w,m;

   int ncolor = sd->color_info.num_components == 1 ? 1 : 4;

   while(sd->stc.stc_y < sd->stc.prt_scans) {

/*
 * find the begin of the band
 */
      for(w = 0; sd->stc.stc_y < sd->stc.buf_y; ++sd->stc.stc_y) {
         buf_a = (sd->stc.prt_buf-1) & (sd->stc.stc_y * ncolor);
         for(color = 0; color < ncolor; ++color)
            if(sd->stc.prt_width[buf_a+color] > w)
               w = sd->stc.prt_width[buf_a+color];
         if(w != 0) break;
      }
      if(w == 0) break;
/*
 * adjust the band-height
 */
      w = sd->stc.prt_scans - sd->stc.stc_y;
      if((w < sd->stc.escp_m) && (sd->stc.escp_v != 40)) {
         if(w < 8)       m =  1;
         else if(w < 24) m =  8;
         else            m = 24;
      } else {
         m = sd->stc.escp_m;
      }

      if(sd->stc.buf_y < (sd->stc.stc_y+m)) break;

      escp_used = 0;
      for(color = 0; color < ncolor; ++color) { /* print the colors */

         if(0 == (w = stc_bandwidth(sd,color,m,1))) continue; /* shortcut */

         escp_used = stc_print_escpcmd(sd,prn_stream,escp_used,color,m,w);

         buf_a = (sd->stc.prt_buf-1) & (sd->stc.stc_y * ncolor + color);
         for(iprint = 0; iprint < m; ++iprint) { /* send data */

            if((sd->stc.flags & STCCOMP) == STCPLAIN) {
               memcpy(sd->stc.escp_data+escp_used,sd->stc.prt_data[buf_a],w);
               escp_used += w;
            } else {
               escp_used += stc_rle(sd->stc.escp_data+escp_used,
                                    sd->stc.prt_data[buf_a],w);
            }

            fwrite(sd->stc.escp_data,1,escp_used,prn_stream);
            escp_used = 0;

            buf_a = (sd->stc.prt_buf-1) & (buf_a + ncolor);

         }                                            /* send data */

      }                                             /* print the colors */

      sd->stc.stc_y += m;
   }
}
/* ----------------------------------------------------------------------- */

private int 
stc_deltarow(byte *out,const byte *in,int width,byte *seed)
{

   int istop,nmove,ndata,i,j;
   int *wseed = (int *) seed;
   int used   = 0;

   seed += sizeof(int);

   if((in != NULL) && (width > 0)) { /* Data present */

      istop = width < wseed[0] ? wseed[0] : width;

      i = 0;
      while(i < istop) {

         for(j = i; j < istop; ++j) if(in[j] != seed[j]) break;

         nmove = j - i;

         if(nmove > 0) { /* issue a move */
           i     = j;
           if(i == istop) break;

           if(       nmove <   8) {
              out[used++] = 0x40 | nmove;
           } else if(nmove < 128) {
              out[used++] = 0x51;
              out[used++] = nmove;
           } else {
              out[used++] = 0x52;
              out[used++] = 0xff & nmove;
              out[used++] = 0xff & (nmove>>8);
           }
         }           /* issue a move */

/*
 * find the end of this run
 */
         nmove = 0;
         for(j = i+1; (j < istop) && ((nmove < 4)); ++j) {
            if(in[j] == seed[j]) nmove += 1;
            else                 nmove  = 0;
         }

         ndata = j-i-nmove;

         nmove = stc_rle(out+used+3,in+i,ndata);
         if(nmove < 16) {
            out[used++] = 0x20 | nmove;
            for(j = 0; j < nmove; ++j) out[used+j] = out[used+j+2];
         } else if(nmove < 256) {
            out[used++] = 0x31;
            out[used++] = nmove;
            for(j = 0; j < nmove; ++j) out[used+j] = out[used+j+1];
         } else {
            out[used++] = 0x32;
            out[used++] = 0xff & nmove;
            out[used++] = 0xff & (nmove>>8);
         }
         used += nmove;
         i    += ndata;
      }

      memcpy(seed,in,istop);
      wseed[0] = width;

   } else if(wseed[0] > 0) { /* blank line, but seed has data */

      out[used++] = 0xe1; /* clear row */
      memset(seed,0,wseed[0]);
      wseed[0] = 0;

   }

   return used;
}

/*
 * Slightly different single-pass printing
 */
private void
stc_print_delta(stcolor_device *sd, FILE *prn_stream)
{

   int color,buf_a,w;
   int escp_used = 0; 
   int ncolor = sd->color_info.num_components == 1 ? 1 : 4;

   while(sd->stc.stc_y < sd->stc.prt_scans) {

/*
 * find the begin of the band
 */
      for(w = 0; sd->stc.stc_y < sd->stc.buf_y; ++sd->stc.stc_y) {
         buf_a = (sd->stc.prt_buf-1) & (sd->stc.stc_y * ncolor);
         for(color = 0; color < ncolor; ++color)
            if(sd->stc.prt_width[buf_a+color] > w)
               w = sd->stc.prt_width[buf_a+color];
         if(w != 0) break;
      }

      if(sd->stc.buf_y == sd->stc.stc_y) break;

      escp_used = 0;

/*
 * Send Initialization & ESC . 3 once
 */
      if(0 == (sd->stc.flags & STCPRINT)) {

         sd->stc.flags |= STCPRINT;

         fwrite(sd->stc.escp_init.data,1,sd->stc.escp_init.size,prn_stream);

         sd->stc.escp_data[escp_used++] = '\033';
         sd->stc.escp_data[escp_used++] = '.';
         sd->stc.escp_data[escp_used++] =  3;
         sd->stc.escp_data[escp_used++] = sd->stc.escp_v;
         sd->stc.escp_data[escp_used++] = sd->stc.escp_h;
         sd->stc.escp_data[escp_used++] = sd->stc.escp_m;
         sd->stc.escp_data[escp_used++] = 0;
         sd->stc.escp_data[escp_used++] = 0;
         sd->stc.escp_data[escp_used++] = 0xe4; /* MOVXBYTE */
      }

      if(sd->stc.stc_y != sd->stc.prt_y) { /* really position the printer */
         w = sd->stc.stc_y - sd->stc.prt_y;
         if(       w <  16) {
            sd->stc.escp_data[escp_used++] = 0x60 | w;
         } else if(w < 256) {
            sd->stc.escp_data[escp_used++] = 0x71;
            sd->stc.escp_data[escp_used++] = w;
         } else {
            sd->stc.escp_data[escp_used++] = 0x72;
            sd->stc.escp_data[escp_used++] = 0xff & w;
            sd->stc.escp_data[escp_used++] = 0xff & (w>>8);
         }
         sd->stc.prt_y = sd->stc.stc_y;
      }                                    /* really position the printer */

      for(color = 0; color < ncolor; ++color) { /* print the colors */

/* Color-Selection */
         if(color == (ncolor-1)) {
            sd->stc.escp_data[escp_used++] = 0x80; /* Black */
         } else {
            switch(color) {
            case 1:  sd->stc.escp_data[escp_used++] = 0x81; break; /* M */
            case 2:  sd->stc.escp_data[escp_used++] = 0x84; break; /* Y */
            default: sd->stc.escp_data[escp_used++] = 0x82; break; /* C */
            }
         }

/* Data-Transfer */
         buf_a = (sd->stc.prt_buf-1) & (sd->stc.stc_y * ncolor + color);

         w = stc_deltarow(sd->stc.escp_data+escp_used,
             sd->stc.prt_data[buf_a],sd->stc.prt_width[buf_a],
             sd->stc.seed_row[color]);

         if(w == 0) escp_used -= 1;
         else       escp_used += w;

         if(escp_used > 0) fwrite(sd->stc.escp_data,1,escp_used,prn_stream);
         escp_used = 0;

      }                                             /* print the colors */

      sd->stc.stc_y += 1;

   }

}

/* ----------------------------------------------------------------------- */

/***
 *** Free-Data: release the specific-Arrays
 ***/
private void 
stc_freedata(stc_t *stc)
{
   int i,j;

   for(i = 0; i < 4; ++i) {
      if(stc->code[i] != NULL) {

         for(j = 0; j < i; ++j) if(stc->code[i] == stc->code[j]) break;

         if(i == j) gs_free(stc->code[i],1<<stc->bits,sizeof(gx_color_value),
                           "stcolor/code");
      }

      if(stc->vals[i] != NULL) {

         for(j = 0; j < i; ++j)
            if(stc->vals[i] == stc->vals[j]) break;

         if(i == j) gs_free(stc->vals[i],1<<stc->bits,sd->stc.alg_item,
                           "stcolor/transfer");
      }
   }

   for(i = 0; i < 4; ++i) {
      stc->code[i] = NULL;
      stc->vals[i] = NULL;
   }
}

/***
 *** open the device and initialize margins & arrays
 ***/

private int 
stc_open(gx_device *pdev) /* setup margins & arrays */
{
  stcolor_device *sd = (stcolor_device *) pdev;
  int i,j,code;
  gx_color_index white;
  byte *bpw,*bpm;

  code = 0;
/*
 * Establish Algorithm-Table, if not present
 */
  if(sd->stc.algorithms.size == 0) {
     gs_param_string *dp;
     for(i = 0; stc_dither[i].name != NULL; ++i); /* count 'em */
     sd->stc.algorithms.size = i;
     dp = gs_malloc(i,sizeof(gs_param_string),
                                        "stcolor/algorithms");
     if(dp == NULL) {
        code = gs_error_VMerror;
        sd->stc.algorithms.size = 0;
     } else {
        sd->stc.algorithms.data       = dp;
        sd->stc.algorithms.persistent = true;
        for(i = 0; stc_dither[i].name != NULL; ++i) {
        param_string_from_string(dp[i],stc_dither[i].name);
        }
     }
  }

# define stc_sizeofitem(T) sd->stc.alg_item = sizeof(T)
  STC_TYPESWITCH(sd->stc.dither,stc_sizeofitem)

  stc_print_setup(sd);

/*
 * Establish internal Value & Code-Arrays
 */


  for(i = 0; i < sd->color_info.num_components; ++i) { /* comp */

     if((sd->stc.sizc[i] >  1) && (sd->stc.extc[i] != NULL)) { /* code req. */

        for(j = 0; j < i; ++j) if(sd->stc.extc[i] == sd->stc.extc[j]) break;

        if(i == j) { /* new one */
           sd->stc.code[i] = gs_malloc(1<<sd->stc.bits,sizeof(gx_color_value),
                             "stcolor/code");

           if(sd->stc.code[i] == NULL) { /* error */
              code = gs_error_VMerror;
           } else {                      /* success */
/*
 * Try making things easier:
 *     normalize values to 0.0/1.0-Range
 *     X-Axis:   Color-Values (implied)
 *     Y-Values: Indices      (given)
 */
              unsigned long ly,iy;
              double ystep,xstep,fx,fy;

/* normalize */

              fx =  1e18;
              fy = -1e18;
              for(ly = 0; ly < sd->stc.sizc[i]; ++ly) {
                 if(sd->stc.extc[i][ly] < fx) fx = sd->stc.extc[i][ly];
                 if(sd->stc.extc[i][ly] > fy) fy = sd->stc.extc[i][ly];
              }
              if((fx != 0.0) || (fy != 1.0)) {
                 fy = 1.0 / (fy - fx);
                 for(ly = 0; ly < sd->stc.sizc[i]; ++ly)
                    sd->stc.extc[i][ly] = fy * (sd->stc.extc[i][ly]-fx);
              }

/* interpolate */
              ystep = 1.0 / (double)((1<<sd->stc.bits)-1);
              xstep = 1.0 / (double)( sd->stc.sizc[i] -1);

              iy = 0;
              for(ly = 0; ly < (1<<sd->stc.bits); ++ly) {
                 fy = ystep * ly;
                 while(((iy+1) < sd->stc.sizc[i]) &&
                       (  fy   > sd->stc.extc[i][iy+1])) ++iy;
                 fx  = iy + (fy - sd->stc.extc[i][iy])
                            / (sd->stc.extc[i][iy+1] - sd->stc.extc[i][iy]);
                 fx *= xstep * gx_max_color_value;

                 fx = fx < 0.0 ? 0.0 :
                      (fx > gx_max_color_value ? gx_max_color_value : fx);

                 sd->stc.code[i][ly] = fx;
                 if((fx-sd->stc.code[i][ly]) >= 0.5) sd->stc.code[i][ly] += 1;
              }
           }                             /* error || success */

        } else {     /* shared one */

           sd->stc.code[i] = sd->stc.code[j];

        }           /* new || shared one */
     }                                                         /* code req. */

     if((sd->stc.sizv[i] >  1) && (sd->stc.extv[i] != NULL)) { /* vals req. */

        for(j = 0; j < i; ++j)
           if((sd->stc.extc[i] == sd->stc.extc[j]) &&
              (sd->stc.extv[i] == sd->stc.extv[j])) break;

        if(i == j) { /* new one */

             sd->stc.vals[i] =
                gs_malloc(1<<sd->stc.bits,sd->stc.alg_item,"stcolor/transfer");

           if(sd->stc.vals[i] == NULL) {

              code = gs_error_VMerror;

           } else {                      /* success */


              if(sd->stc.code[i] == NULL) { /* linear */

                 byte  *Out  = sd->stc.vals[i];
                 int    Nout = 1<<sd->stc.bits;
                 double Omin = sd->stc.dither->minmax[0];
                 double Omax = sd->stc.dither->minmax[1];
                 float *In   = sd->stc.extv[i];
                 int    Nin  = sd->stc.sizv[i];
                 unsigned long I,io;
                 double Istep,Ostep,Y;
                 byte   Ovb; long Ovl;

                 Istep = 1.0 / (double) ((Nin)-1);
                 Ostep = 1.0 / (double) ((Nout)-1);

                 for(io = 0; io < (Nout); ++io) {
                    I = (long)(io * ((Nin)-1))/((Nout)-1);

                    if((I+1) < (Nin))
                       Y = In[I] + (In[I+1]-In[I])
                                     * ((double) io * Ostep - (double)I * Istep)
                                               /  (double) Istep;
                    else
                       Y = In[I] + (In[I]-In[I-1])
                                     * ((double) io * Ostep - (double)I * Istep)
                                               /  (double) Istep;

                    Y = Omin + (Omax-Omin) * Y;
                    Y = Y < Omin ? Omin : (Y > Omax ? Omax : Y);


                    switch(sd->stc.dither->flags & STC_TYPE) {
                       case STC_BYTE:
                          Ovb = Y;
                          if(((Y-Ovb) >= 0.5) && ((Ovb+1) <= Omax)) Ovb += 1;
                          Out[io] = Ovb;
                          break;
                       case STC_LONG:
                          Ovl = Y;
                          if(((Y-Ovl) >= 0.5) && ((Ovl+1) <= Omax)) Ovl += 1;
                          if(((Ovl-Y) >= 0.5) && ((Ovl-1) >= Omax)) Ovl -= 1;
                          ((long *)Out)[io] = Ovl;
                          break;
                       default:
                          ((float *)Out)[io] = Y;
                          break;
                    }
                 }

              } else {                     /* encoded */
                 unsigned long j,o;
                 double xstep,x,y;

                 xstep = 1.0 / (double) (sd->stc.sizv[i]-1);

/*
 * The following differs in so far from the previous, that the desired
 * X-Values are stored in another array.
 */
                 for(o = 0; o < (1<<sd->stc.bits); ++o) { /* code-loop */

                    x = sd->stc.code[i][o]; x /= gx_max_color_value;

                    j = x / xstep;

                    if((j+1) < sd->stc.sizv[i]) {
                       y  = sd->stc.extv[i][j];
                       y += (sd->stc.extv[i][j+1]-y)*(x-(double)j*xstep)/xstep;
                    } else {
                       y  = sd->stc.extv[i][j];
                       y += (y-sd->stc.extv[i][j-1])*(x-(double)j*xstep)/xstep;
                    }

                    y = sd->stc.dither->minmax[0]
                      +(sd->stc.dither->minmax[1]-sd->stc.dither->minmax[0])*y;

#                   define stc_adjvals(T)                                             \
                     ((T *)(sd->stc.vals[i]))[o] = y;                                 \
                                                                                      \
                    if(((y-((T *)(sd->stc.vals[i]))[o]) >= 0.5) &&                    \
                       ((1+((T *)(sd->stc.vals[i]))[o]) <= sd->stc.dither->minmax[1]))\
                       ((T *)(sd->stc.vals[i]))[o]      += 1;                         \
                                                                                      \
                    if(((((T *)(sd->stc.vals[i]))[o]-y) >= 0.5) &&                    \
                       ((((T *)(sd->stc.vals[i]))[o]-1) >= sd->stc.dither->minmax[0]))\
                       ((T *)(sd->stc.vals[i]))[o]      -= 1;

                    STC_TYPESWITCH(sd->stc.dither,stc_adjvals)

#                   undef stc_adjvals
                 }                                       /* code-loop */
              }                            /* lineaer / encoded */
           }                             /* error || success */

        } else {     /* shared one */

           sd->stc.vals[i] = sd->stc.vals[j];

        }           /* new || shared one */
     }                                                         /* vals req. */
  }                                                    /* comp */

  if(code == 0) {

      sd->stc.flags |= STCOK4GO;

/*
 * Arrgh: open-procedure seems to be the right-place, but it is
 *        necessary to establish the defaults for omitted procedures too.
 */

      switch(sd->color_info.num_components) { /* Establish color-procs */
      case 1:
         set_dev_proc(sd,map_rgb_color, stc_map_gray_color);
         set_dev_proc(sd,map_cmyk_color,gx_default_map_cmyk_color);
         set_dev_proc(sd,map_color_rgb, stc_map_color_gray);
         white = stc_map_gray_color((gx_device *) sd,
                    gx_max_color_value,gx_max_color_value,gx_max_color_value);
         break;
      case 3:
         set_dev_proc(sd,map_rgb_color, stc_map_rgb_color);
         set_dev_proc(sd,map_cmyk_color,gx_default_map_cmyk_color);
         set_dev_proc(sd,map_color_rgb, stc_map_color_rgb);
         white = stc_map_rgb_color((gx_device *) sd,
                    gx_max_color_value,gx_max_color_value,gx_max_color_value);
         break;
      default:
         set_dev_proc(sd,map_rgb_color, gx_default_map_rgb_color);
         if(sd->stc.flags & STCCMYK10) {
            set_dev_proc(sd,map_cmyk_color,stc_map_cmyk10_color);
            set_dev_proc(sd,map_color_rgb, stc_map_color_cmyk10);
            white = stc_map_cmyk10_color((gx_device *) sd,0,0,0,0);
         } else {
            set_dev_proc(sd,map_cmyk_color,stc_map_cmyk_color);
            set_dev_proc(sd,map_color_rgb, stc_map_color_cmyk);
            white = stc_map_cmyk_color((gx_device *) sd,0,0,0,0);
         }
         break;                               /* Establish color-procs */
      }


/*
 * create at least a Byte
 */
      if(sd->color_info.depth < 2) white |= (white<<1);
      if(sd->color_info.depth < 4) white |= (white<<2);
      if(sd->color_info.depth < 8) white |= (white<<4);

/*
 * copy the Bytes
 */
      bpw = (byte *) sd->stc.white_run;

      if(sd->color_info.depth < 16) {
         for(i = 0; i < sizeof(sd->stc.white_run); i += 1) {
            bpw[i] = 0xff & white;
         }
      } else if(sd->color_info.depth < 24) {
         for(i = 0; i < sizeof(sd->stc.white_run); i += 2) {
            bpw[i]   = 0xff & (white>>8);
            bpw[i+1] = 0xff &  white;
         }
      } else if(sd->color_info.depth < 32) {
         for(i = 0; i < sizeof(sd->stc.white_run); i += 3) {
            bpw[i]   = 0xff & (white>>16);
            bpw[i+1] = 0xff & (white>> 8);
            bpw[i+2] = 0xff &  white;
         }
      } else {
         for(i = 0; i < sizeof(sd->stc.white_run); i += 4) {
            bpw[i]   = 0xff & (white>>24);
            bpw[i+1] = 0xff & (white>>16);
            bpw[i+2] = 0xff & (white>> 8);
            bpw[i+3] = 0xff &  white;
         }
      }
/*
 *    compute the trailer
 */
      j  = sd->width -
          (dev_l_margin(sd)+dev_r_margin(sd))*sd->x_pixels_per_inch;
      j  = j * sd->color_info.depth;            /* the Bit-count */
      j  = j % (32*countof(sd->stc.white_run)); /* remaining Bits */

      bpm = (byte *) sd->stc.white_end;
      for(i = 0; i < (4*countof(sd->stc.white_end)); ++i) {
         if(       j <= 0) {
            bpm[i] = 0;
         } else if(j >= 8) {
            bpm[i] = 0xff;
            j -= 8;
         } else {
            bpm[i] = 0xff ^ ((1<<(8-j))-1);
            j  = 0;
         }
         bpm[i] &= bpw[i];
      }

/*
 * Call super-class open
 */

      return gdev_prn_open(pdev);

   } else {

      stc_freedata(&sd->stc);

      return_error(code);
   }

}

/***
 *** stc_close: release the internal data
 ***/
private int 
stc_close(gx_device *pdev)
{
   stc_freedata(&((stcolor_device *) pdev)->stc);
   ((stcolor_device *) pdev)->stc.flags &= ~STCOK4GO;
   return gdev_prn_close(pdev);
}


/***
 *** Function for Bit-Truncation, including direct-byte-transfer
 ***/
private gx_color_value 
stc_truncate(stcolor_device *sd,int i,gx_color_value v)
{

   if(sd->stc.bits < gx_color_value_bits) {
      if(sd->stc.code[i] != NULL) {
/*
 * Perform binary search in the code-array
 */
         long  s;
         gx_color_value *p;

         s = sd->stc.bits > 1 ? 1L<<(sd->stc.bits-2) : 0L;
         p = sd->stc.code[i]+(1L<<(sd->stc.bits-1));

         while(s > 0) {
            if(v > *p) {
               p += s;
            } else if(v < p[-1]) {
               p -= s;
            } else {
               if((v-p[-1]) < (p[0]-v)) p -= 1;
               break;
            }
            s >>= 1;
         }
         if((v-p[-1]) < (p[0]-v)) p -= 1;
         v = p - sd->stc.code[i];

      } else {

         v >>= gx_color_value_bits-sd->stc.bits;

      }

/*
      V = (((1L<<D->stc.bits)-1)*V+(gx_max_color_value>>1))\
          /gx_max_color_value;                             \
*/
   }
   return v;
}

private gx_color_value
stc_truncate1(stcolor_device *sd,int i,gx_color_value v)
{

   return sd->stc.vals[i][stc_truncate(sd,i,v)];
}

/***
 *** Expansion of indices for reverse-mapping
 ***/
private gx_color_value 
stc_expand(stcolor_device *sd,int i,gx_color_index col)
{

   gx_color_index cv;
   gx_color_index l = (1<<sd->stc.bits)-1;

   if(sd->stc.code[i] != NULL) {

      cv  = sd->stc.code[i][col & l];

   } else if(sd->stc.bits < gx_color_value_bits) {

      cv  = (col & l)<<(gx_color_value_bits-sd->stc.bits);
      cv += (col & l)/l * ((1<<(gx_color_value_bits-sd->stc.bits))-1);

   } else if(sd->stc.bits > gx_color_value_bits) {

      cv  = (col & l)>>(sd->stc.bits-gx_color_value_bits);

   } else {

      cv  = col & l;

   }

   return cv;
}

/***
 *** color-mapping of gray-scales
 ***/
private gx_color_index 
stc_map_gray_color(gx_device *pdev,
        gx_color_value r, gx_color_value g, gx_color_value b)
{

   stcolor_device *sd = (stcolor_device *) pdev;
   gx_color_index rv;

   if((r == g) && (g == b)) {

      rv = gx_max_color_value - r;

   } else if(sd->stc.am != NULL) {
      float *m,fv;

      m   = sd->stc.am;

      fv  = gx_max_color_value;
      fv -= *m++ * (float) r; fv -= *m++ * (float) g; fv -= *m   * (float) b;

      if(     fv < 0.0)                      rv = 0;
      else if((fv+0.5) > gx_max_color_value) rv = gx_max_color_value;
      else                                   rv = fv+0.5;

   } else {

      rv  = ((gx_color_index)gx_max_color_value)<<3;
      rv -= (gx_color_index) 3 * r;
      rv -= (gx_color_index) 3 * g;
      rv -= ((gx_color_index)b)<<1;
      rv  = (rv+4)>>3;
      if(rv > gx_max_color_value) rv = gx_max_color_value;

   }

   if(( sd->stc.bits                      ==    8) &&
      ((sd->stc.dither->flags & STC_TYPE) == STC_BYTE))
      rv = stc_truncate1(sd,0,(gx_color_value)rv);
   else
      rv =  stc_truncate(sd,0,(gx_color_value)rv);

   return rv;
}

private int 
stc_map_color_gray(gx_device *pdev, gx_color_index color,gx_color_value prgb[3])
{
   stcolor_device *sd = (stcolor_device *) pdev;
   gx_color_index l = ((gx_color_index)1<<sd->stc.bits)-1;

   prgb[0] = gx_max_color_value - stc_expand(sd,0,color & l);
   prgb[1] = prgb[0]; prgb[2] = prgb[0];

   return 0;
}

/***
 *** color-mapping of rgb-values
 ***/
private gx_color_index 
stc_map_rgb_color(gx_device *pdev,
                  gx_color_value r, gx_color_value g, gx_color_value b)
{

   stcolor_device *sd = (stcolor_device *) pdev;
   int          shift = sd->color_info.depth == 24 ? 8 : sd->stc.bits;
   gx_color_index  rv = 0;

   if((sd->stc.am != NULL) && ((r != g) || (g != b))) {
      float *m,fr,fg,fb,fv;

      m  = sd->stc.am;
      fr = r; fg = g; fb = b;

      fv = *m++ * fr; fv += *m++ * fg; fv += *m++ * fb;

      if(     fv < 0.0)                      r = 0;
      else if((fv+0.5) > gx_max_color_value) r = gx_max_color_value;
      else                                   r = fv+0.5;

      fv = *m++ * fr; fv += *m++ * fg; fv += *m++ * fb;

      if(     fv < 0.0)                      g = 0;
      else if((fv+0.5) > gx_max_color_value) g = gx_max_color_value;
      else                                   g = fv+0.5;

      fv = *m++ * fr; fv += *m++ * fg; fv += *m++ * fb;

      if(     fv < 0.0)                      b = 0;
      else if((fv+0.5) > gx_max_color_value) b = gx_max_color_value;
      else                                   b = fv+0.5;

   }

   if(( sd->stc.bits                      ==    8) &&
      ((sd->stc.dither->flags & STC_TYPE) == STC_BYTE)) {
      rv =               stc_truncate1(sd,0,r);
      rv = (rv<<shift) | stc_truncate1(sd,1,g);
      rv = (rv<<shift) | stc_truncate1(sd,2,b);
   } else {
      rv =                stc_truncate(sd,0,r);
      rv = (rv<<shift) |  stc_truncate(sd,1,g);
      rv = (rv<<shift) |  stc_truncate(sd,2,b);
   }

   return rv;
}

private int 
stc_map_color_rgb(gx_device *pdev, gx_color_index color,gx_color_value prgb[3])
{

   stcolor_device *sd = (stcolor_device *) pdev;
   int          shift = sd->color_info.depth == 24 ? 8 : sd->stc.bits;
   gx_color_index l =   ((gx_color_index)1<<sd->stc.bits)-1;

   prgb[0] = stc_expand(sd,0,((color>>(shift<<1)) & l));
   prgb[1] = stc_expand(sd,1,((color>> shift    ) & l));
   prgb[2] = stc_expand(sd,2,( color              & l));

   return 0;
}

/***
 *** color-mapping of cmyk-values
 ***/
private gx_color_index 
stc_map_cmyk_color(gx_device *pdev,
        gx_color_value c, gx_color_value m, gx_color_value y,gx_color_value k)
{

   stcolor_device *sd = (stcolor_device *) pdev;
   int          shift = sd->color_info.depth == 32 ? 8 : sd->stc.bits;
   gx_color_index rv = 0;

   if((c == m) && (m == y)) {

      k = c > k ? c : k;
      c = m = y = 0;

      if(( sd->stc.bits                      ==    8) &&
      ((sd->stc.dither->flags & STC_TYPE) == STC_BYTE)) {
         k  = stc_truncate1(sd,3,k);
      } else {
         k  =  stc_truncate(sd,3,k);
      }

   } else {

      if(sd->stc.am != NULL) {

         float *a,fc,fm,fy,fk,fv;

         if(k == 0) { /* no separated black yet */
            k  = c < m ? c : m;
            k  = k < y ? k : y;
            if(k) { /* no black at all */
               c -= k;
               m -= k;
               y -= k;
           }       /* no black at all */
         }            /* no separated black yet */

         a  = sd->stc.am;
         fc = c; fm = m; fy = y; fk = k;

         fv = *a++ * fc; fv += *a++ * fm; fv += *a++ * fy; fv += *a++ * fk;
         if(     fv < 0.0)                      c = 0;
         else if((fv+0.5) > gx_max_color_value) c = gx_max_color_value;
         else                                   c = fv+0.5;

         fv = *a++ * fc; fv += *a++ * fm; fv += *a++ * fy; fv += *a++ * fk;
         if(     fv < 0.0)                      m = 0;
         else if((fv+0.5) > gx_max_color_value) m = gx_max_color_value;
         else                                   m = fv+0.5;

         fv = *a++ * fc; fv += *a++ * fm; fv += *a++ * fy; fv += *a++ * fk;
         if(     fv < 0.0)                      y = 0;
         else if((fv+0.5) > gx_max_color_value) y = gx_max_color_value;
         else                                   y = fv+0.5;

         fv = *a++ * fc; fv += *a++ * fm; fv += *a++ * fy; fv += *a++ * fk;
         if(     fv < 0.0)                      k = 0;
         else if((fv+0.5) > gx_max_color_value) k = gx_max_color_value;
         else                                   k = fv+0.5;

      } else if(k == 0) {

         k  = c < m ? c : m;
         k  = k < y ? k : y;
      }

      if(( sd->stc.bits                      ==    8) &&
         ((sd->stc.dither->flags & STC_TYPE) == STC_BYTE)) {
         c = stc_truncate1(sd,0,c);
         m = stc_truncate1(sd,1,m);
         y = stc_truncate1(sd,2,y);
         k = stc_truncate1(sd,3,k);
      } else {
         c = stc_truncate(sd,0,c);
         m = stc_truncate(sd,1,m);
         y = stc_truncate(sd,2,y);
         k = stc_truncate(sd,3,k);
      }
   }

   rv =               c;
   rv = (rv<<shift) | m;
   rv = (rv<<shift) | y;
   rv = (rv<<shift) | k;

   if(rv == gx_no_color_index) rv ^= 1;

   return rv;
}

private int 
stc_map_color_cmyk(gx_device *pdev, gx_color_index color,gx_color_value prgb[3])
{

   stcolor_device *sd = (stcolor_device *) pdev;
   int          shift = sd->color_info.depth == 32 ? 8 : sd->stc.bits;
   gx_color_index   l = ((gx_color_index)1<<sd->stc.bits)-1;
   gx_color_value c,m,y,k;

   k = stc_expand(sd,3, color & l); color >>= shift;
   y = stc_expand(sd,2, color & l); color >>= shift;
   m = stc_expand(sd,1, color & l); color >>= shift;
   c = stc_expand(sd,0, color & l);

   if((c == m) && (m == y)) {
      prgb[0] = gx_max_color_value-k;
      prgb[1] = prgb[0];
      prgb[2] = prgb[0];
   } else {
      prgb[0] = gx_max_color_value-c;
      prgb[1] = gx_max_color_value-m;
      prgb[2] = gx_max_color_value-y;
   }
   return 0;
}

/***
 *** color-mapping of cmyk10-values
 ***/
private gx_color_index 
stc_map_cmyk10_color(gx_device *pdev,
        gx_color_value c, gx_color_value m, gx_color_value y,gx_color_value k)
{

   stcolor_device *sd = (stcolor_device *) pdev;
   int             mode;
   gx_color_index rv  = 0;

   if((c == m) && (m == y)) {

      k = c > k ? c : k;
      c = m = y = 0;
      mode = 3;

   } else {

      if(sd->stc.am != NULL) {

         float *a,fc,fm,fy,fk,fv;

         k  = c < m ? c : m;
         k  = k < y ? k : y;
         if(k) { /* no black at all */
            c -= k;
            m -= k;
            y -= k;
         }       /* no black at all */

         a  = sd->stc.am;
         fc = c; fm = m; fy = y; fk = k;

         fv = *a++ * fc; fv += *a++ * fm; fv += *a++ * fy; fv += *a++ * fk;
         if(     fv < 0.0)                      c = 0;
         else if((fv+0.5) > gx_max_color_value) c = gx_max_color_value;
         else                                   c = fv+0.5;

         fv = *a++ * fc; fv += *a++ * fm; fv += *a++ * fy; fv += *a++ * fk;
         if(     fv < 0.0)                      m = 0;
         else if((fv+0.5) > gx_max_color_value) m = gx_max_color_value;
         else                                   m = fv+0.5;

         fv = *a++ * fc; fv += *a++ * fm; fv += *a++ * fy; fv += *a++ * fk;
         if(     fv < 0.0)                      y = 0;
         else if((fv+0.5) > gx_max_color_value) y = gx_max_color_value;
         else                                   y = fv+0.5;

      }

      if(c < m) {
        if(c < y) { k = c; c = 0; mode = 0; }
        else      { k = y; y = 0; mode = 2; }
      } else {
        if(m < y) { k = m; m = 0; mode = 1; }
        else      { k = y; y = 0; mode = 2; }
      }
   }

/*
 * truncate only the values that require it
 */
   if(c) c = stc_truncate(sd,0,c);
   if(m) m = stc_truncate(sd,1,m);
   if(y) y = stc_truncate(sd,2,y);
   if(k) k = stc_truncate(sd,3,k);

/*
 * make sure that truncation-white becomes white.
 */
   if((c|m|y) == 0) mode = 3;

/*
 * check wether value-arrays can be bypassed
 */
   if(((sd->stc.dither->flags & STC_TYPE) == STC_BYTE) &&
      ( sd->stc.dither->minmax[0]         ==    0.0 )) {
      c = sd->stc.vals[0][c];
      m = sd->stc.vals[1][m];
      y = sd->stc.vals[2][y];
      k = sd->stc.vals[3][k];
   } else if(((sd->stc.dither->flags & STC_TYPE) == STC_LONG) &&
             ( sd->stc.dither->minmax[0]         ==     0.0 ) &&
             ( sd->stc.dither->minmax[1]         <=  1023.0 )) {
      c = ((long *)(sd->stc.vals[0]))[c];
      m = ((long *)(sd->stc.vals[1]))[m];
      y = ((long *)(sd->stc.vals[2]))[y];
      k = ((long *)(sd->stc.vals[3]))[k];
   }                                                       /* direct */
/*
 * compute the long-representation of gx_color_index
 */
   switch(mode) {
   case 0:
      rv = (((gx_color_index) m)<<22)|
           (((gx_color_index) y)<<12)|
           (((gx_color_index) k)<< 2)|mode;
      break;
   case 1:
      rv = (((gx_color_index) c)<<22)|
           (((gx_color_index) y)<<12)|
           (((gx_color_index) k)<< 2)|mode;
      break;
   case 2:
      rv = (((gx_color_index) c)<<22)|
           (((gx_color_index) m)<<12)|
           (((gx_color_index) k)<< 2)|mode;
      break;
   default:
      rv = (((gx_color_index) k)<< 2)|mode;
      break;
   }

/*
 * We may need some swapping
 */
#if !arch_is_big_endian
   {
      union { stc_pixel cv; byte bv[4]; } ui,uo;
      ui.cv = rv;
      uo.bv[0] = ui.bv[3];
      uo.bv[1] = ui.bv[2];
      uo.bv[2] = ui.bv[1];
      uo.bv[3] = ui.bv[0];
      rv       = uo.cv;
   }
#endif
   return rv;
}

private int 
stc_map_color_cmyk10(gx_device *pdev, gx_color_index color,
                     gx_color_value prgb[3])
{

   stcolor_device *sd = (stcolor_device *) pdev;
   gx_color_value c,m,y;

/*
 * We may need some swapping
 */
#if !arch_is_big_endian
   union { stc_pixel cv; byte bv[4]; } ui,uo;
   ui.cv = color;
   uo.bv[0] = ui.bv[3];
   uo.bv[1] = ui.bv[2];
   uo.bv[2] = ui.bv[1];
   uo.bv[3] = ui.bv[0];
   color    = uo.cv;
#endif

   c    =   stc_expand(sd,3,(color>>2)&0x3ff);

   switch(color & 3) {
     case 0:
        m = stc_expand(sd,1,(color>>22) & 0x3ff);
        y = stc_expand(sd,2,(color>>12) & 0x3ff);
        break;
     case 1:
        m = c;
        c = stc_expand(sd,0,(color>>22) & 0x3ff);
        y = stc_expand(sd,2,(color>>12) & 0x3ff);
        break;
     case 2:
        y = c;
        c = stc_expand(sd,0,(color>>22) & 0x3ff);
        m = stc_expand(sd,1,(color>>12) & 0x3ff);
        break;
     default:
        m = c;
        y = c;
        break;
   }

   prgb[0] = gx_max_color_value - c;
   prgb[1] = gx_max_color_value - m;
   prgb[2] = gx_max_color_value - y;

   return 0;
}

/***
 *** Macros for parameter-handling
 ***/

#define set_param_array(A, D, S)\
    {A.data = D; A.size = S; A.persistent = false;}

#define stc_write_null(N)                        \
    set_param_array(pfa,defext,countof(defext))  \
    code = param_write_null(plist,N);            \
    if (code < 0) return code;

#define stc_write_xarray(I,Coding,Transfer)                  \
    if(sd->stc.sizc[I] > 0) {                                \
       set_param_array(pfa, sd->stc.extc[I],sd->stc.sizc[I]) \
       code = param_write_float_array(plist,Coding,&pfa);    \
    } else {                                                 \
       code = param_write_null(plist,Coding);                \
    }                                                        \
    if ( code < 0 ) return code;                             \
                                                             \
    if(sd->stc.sizv[I] > 0)                                  \
       set_param_array(pfa, sd->stc.extv[I],sd->stc.sizv[I]) \
    else                                                     \
       set_param_array(pfa,defext,countof(defext))           \
    code = param_write_float_array(plist,Transfer,&pfa);     \
    if ( code < 0 ) return code;

#define stc_read_null(N)                                   \
    code = param_read_null(plist,N);                       \
    if(code == gs_error_typecheck)                         \
       code = param_read_float_array(plist,N,&pfa);        \
    if(code < 0) param_signal_error(plist,N,code);         \
    error = error > code ? code : error;

#define stc_read_xarray(I,Coding,Transfer)                 \
    code = param_read_float_array(plist,Coding,&pfa);      \
    if((error == 0) && (code == 0)) {                      \
       if(pfa.size > 1) {                                  \
          sd->stc.extc[I] = (float *) pfa.data;            \
          sd->stc.sizc[I] = pfa.size;                      \
       } else {                                            \
          code = gs_error_rangecheck;                      \
       }                                                   \
    } else if(code < 0) {                                  \
       code = param_read_null(plist,Coding);               \
       if(code == 0) {                                     \
          sd->stc.extc[I] = NULL;                          \
          sd->stc.sizc[I] = 0;                             \
       }                                                   \
    }                                                      \
    if(code < 0) param_signal_error(plist,Coding,code);    \
    error = error > code ? code : error;                   \
    code = param_read_float_array(plist,Transfer,&pfa);    \
    if((error == 0) && (code == 0)) {                      \
       sd->stc.extv[I] = (float *) pfa.data;               \
       sd->stc.sizv[I] = pfa.size;                         \
    } else if(code < 0) {                                  \
       code = param_read_null(plist,Transfer);             \
       if(code == 0) {                                     \
          sd->stc.extv[I] = defext;                        \
          sd->stc.sizv[I] = countof(defext);               \
       }                                                   \
    }                                                      \
    if(code < 0) param_signal_error(plist,Transfer,code);  \
    error = error > code ? code : error;

/***
 *** Get parameters == Make them accessable via PostScript
 ***/

private int 
stc_get_params(gx_device *pdev, gs_param_list *plist)
{
   int code,nc;
   gs_param_string      ps;
   gs_param_float_array pfa;
   bool btmp;
   stcolor_device *sd = (stcolor_device *) pdev;

   code = gdev_prn_get_params(pdev, plist);
   if ( code < 0 ) return code;

/*
 * Export some readonly-Parameters, used by stcinfo.ps
 */
   param_string_from_string(ps,"1.91");
   code = param_write_string(plist,"Version",&ps);
   if ( code < 0 ) return code;

   code = param_write_int(plist,"BitsPerComponent",&sd->stc.bits);
   if ( code < 0 ) return code;

   if(sd->stc.algorithms.size > 0) {
     code = param_write_string_array(plist,"Algorithms",&sd->stc.algorithms);
   } else {
     code = param_write_null(plist,"Algorithms");
   }
   if ( code < 0 ) return code;

/*
 * Export OutputCode
 */
   switch(sd->stc.flags & STCCOMP) {
   case STCPLAIN: param_string_from_string(ps,"plain");     break;
   case STCDELTA: param_string_from_string(ps,"deltarow");  break;
   default:       param_string_from_string(ps,"runlength"); break;
   }
   code = param_write_string(plist,"OutputCode",&ps);
   if ( code < 0 ) return code;
/*
 * Export Model
 */
   switch(sd->stc.flags & STCMODEL) {
   case STCST800: param_string_from_string(ps,"st800");   break;
   case STCSTCII: param_string_from_string(ps,"stcii");   break;
   default:       param_string_from_string(ps,"stc");     break;
   }
   code = param_write_string(plist,"Model",&ps);
   if ( code < 0 ) return code;

/*
 * Export the booleans
 */
#define stc_write_flag(Mask,Name)                \
   btmp = sd->stc.flags & (Mask) ? true : false; \
   code = param_write_bool(plist,Name,&btmp);    \
   if ( code < 0 ) return code;

   stc_write_flag(STCUNIDIR,"Unidirectional")
   stc_write_flag(STCUWEAVE,"Microweave")
   btmp = sd->stc.flags & (STCUNIDIR|STCUWEAVE) ? false : true;
   code = param_write_bool(plist,"Softweave",&btmp);
   if ( code < 0 ) return code;
   stc_write_flag(STCNWEAVE,"noWeave")
   stc_write_flag(STCDFLAG0, "Flag0")
   stc_write_flag(STCDFLAG1, "Flag1")
   stc_write_flag(STCDFLAG2, "Flag2")
   stc_write_flag(STCDFLAG3, "Flag3")
   stc_write_flag(STCDFLAG4, "Flag4")

#undef stc_write_flag

#  define stc_write_int(Mask,Name,Val)         \
      code = param_write_int(plist,Name,&Val); \
      if ( code < 0 ) return code

   stc_write_int(STCBAND,  "escp_Band",  sd->stc.escp_m);
   stc_write_int(STCWIDTH, "escp_Width", sd->stc.escp_width);
   stc_write_int(STCHEIGHT,"escp_Height",sd->stc.escp_height);
   stc_write_int(STCTOP,   "escp_Top",   sd->stc.escp_top);
   stc_write_int(STCBOTTOM,"escp_Bottom",sd->stc.escp_bottom);

#  undef stc_write_int

   code = param_write_string(plist,"escp_Init",&sd->stc.escp_init);
   code = param_write_string(plist,"escp_Release",&sd->stc.escp_release);

   if(sd->stc.dither != NULL) {
      param_string_from_string(ps,sd->stc.dither->name);
      code = param_write_string(plist,"Dithering",&ps);
   } else {
      code = param_write_null(plist,"Dithering");
   }
   if ( code < 0 ) return code;

   nc = sd->color_info.num_components;

   if(sd->stc.am != NULL) {
      if(     nc == 1) set_param_array(pfa, sd->stc.am, 3)
      else if(nc == 3) set_param_array(pfa, sd->stc.am, 9)
      else             set_param_array(pfa, sd->stc.am,16)
      code = param_write_float_array(plist,"ColorAdjustMatrix",&pfa);
   } else {
      code = param_write_null(plist,"ColorAdjustMatrix");
   }
   if ( code < 0 ) return code;

   if(nc == 1) {        /* DeviceGray */

      stc_write_xarray(0,"Kcoding","Ktransfer");

      stc_write_null("Rcoding"); stc_write_null("Rtransfer");
      stc_write_null("Gcoding"); stc_write_null("Gtransfer");
      stc_write_null("Bcoding"); stc_write_null("Btransfer");

      stc_write_null("Ccoding"); stc_write_null("Ctransfer");
      stc_write_null("Mcoding"); stc_write_null("Mtransfer");
      stc_write_null("Ycoding"); stc_write_null("Ytransfer");

   } else if(nc == 3) { /* DeviceRGB */

      stc_write_xarray(0,"Rcoding","Rtransfer");
      stc_write_xarray(1,"Gcoding","Gtransfer");
      stc_write_xarray(2,"Bcoding","Btransfer");

      stc_write_null("Ccoding"); stc_write_null("Ctransfer");
      stc_write_null("Mcoding"); stc_write_null("Mtransfer");
      stc_write_null("Ycoding"); stc_write_null("Ytransfer");
      stc_write_null("Kcoding"); stc_write_null("Ktransfer");

   } else {             /* DeviceCMYK */

      stc_write_xarray(0,"Ccoding","Ctransfer");
      stc_write_xarray(1,"Mcoding","Mtransfer");
      stc_write_xarray(2,"Ycoding","Ytransfer");
      stc_write_xarray(3,"Kcoding","Ktransfer");

      stc_write_null("Rcoding"); stc_write_null("Rtransfer");
      stc_write_null("Gcoding"); stc_write_null("Gtransfer");
      stc_write_null("Bcoding"); stc_write_null("Btransfer");

   }
   return code;
}

/***
 *** put parameters == Store them in the device-structure
 ***/

private int 
stc_put_params(gx_device *pdev, gs_param_list *plist)
{
   int code,error,i,l;
   bool b1,b2,b3;
   float fv,*fp;
   gs_param_string      ps;
   gs_param_string_array psa;
   gs_param_float_array pfa;
   stcolor_device *sd = (stcolor_device *) pdev;
   gx_device_color_info oldcolor;
   stc_t                oldstc;

/*
 * save old Values
 */
   memcpy(&oldcolor,&sd->color_info,sizeof(oldcolor));
   memcpy(&oldstc  ,&sd->stc       ,sizeof(oldstc  ));

/*
 * Arrrgh:
 * With Version 3.4x and above my simple minded read-only Parameters
 * do not work any more. So read them here for heavens sake.
 */
   code = param_read_string(plist,"Version",&ps);
   code = param_read_int(plist,"BitsPerComponent",&i);
   code = param_read_string_array(plist,"Algorithms",&psa);

/*
 * Fetch Major-Parameters (Model, Dithering, BitsPerPixel/BitsPerComponent)
 */
   error = 0;

   code  = param_read_string(plist,"Model",&ps);
   if(code == 0) {   /* Analyze the Model-String */
/*
 * Arrgh: I should have known, that internal strings are not zero-terminated.
 */
      for(l = ps.size; (l > 0) && (ps.data[l-1] == 0); --l);
#     define stc_putcmp(Name) \
        ((strlen(Name) != l) || (0 != strncmp(Name,ps.data,l)))

      sd->stc.flags &= ~STCMODEL;
      if(     !stc_putcmp("st800"))  sd->stc.flags |= STCST800;
      else if(!stc_putcmp("stcii"))  sd->stc.flags |= STCSTCII;

   }                 /* Analyze the Model-String */
   if(code < 0) param_signal_error(plist,"Model",code);
   error = error > code ? code : error;

/* If we're running for st800, #components must be 1 */
   if(((sd->stc.flags & STCMODEL) == STCST800) &&
      (( sd->color_info.num_components > 1) ||
       ( sd->stc.dither                == NULL) ||
       ((sd->stc.dither->flags & 7)    > 1))) {
        sd->color_info.num_components  = 1;
        sd->stc.dither = NULL;
    }

/* Weaving isn't a feature for the st800 */
   if((sd->stc.flags & STCMODEL) == STCST800) {
      sd->stc.flags &= ~STCUWEAVE;
      sd->stc.flags |=  STCNWEAVE;
   } else if((sd->stc.flags & STCMODEL) == STCSTCII) { /* no SoftWeave */
      sd->stc.flags |=  STCNWEAVE;
   }

   code  = param_read_string(plist,"Dithering",&ps);
   if(code == 0) {                     /* lookup new value new value */

      for(l = ps.size; (l > 0) && (ps.data[l-1] == 0); --l);

      for(i = 0; stc_dither[i].name != NULL; ++i)
         if(!stc_putcmp(stc_dither[i].name)) break;

   } else if(sd->stc.dither != NULL) {  /* compute index of given value */

      i = sd->stc.dither - stc_dither;

   } else {                            /* find matching value */

      for(i = 0; stc_dither[i].name != NULL; ++i)
         if((stc_dither[i].flags & 7) == sd->color_info.num_components) break;

   }                                   /* we've got an index */

   if(stc_dither[i].name != NULL) { /* establish data */

/*
 * Establish new dithering algorithm & color-model
 */
      sd->stc.dither                = stc_dither+i;
      sd->color_info.num_components = sd->stc.dither->flags & 7;
  STC_TYPESWITCH(sd->stc.dither,stc_sizeofitem)
# undef stc_sizeofitem
      if(((sd->stc.flags & STCMODEL)    == STCST800) &&
         ( sd->color_info.num_components > 1       ))
         code = gs_error_rangecheck;

/*
 * reset Parameters related to the color-model, if it changed
 */

      if(sd->color_info.num_components != oldcolor.num_components) {

         for(i = 0; i < sd->color_info.num_components; ++i) {
            sd->stc.extv[i]   = (float *) defext;
            sd->stc.sizv[i]   = countof(defext);

            sd->stc.extc[i] = NULL;
            sd->stc.sizc[i] = 0;

         }

         sd->stc.am = NULL;

      } else { /* guarantee, that extvals is present */

         for(i = 0; i < sd->color_info.num_components; ++i) {
            if(sd->stc.sizv[i] < 2) {
               sd->stc.extv[i]   = (float *) defext;
               sd->stc.sizv[i]   = countof(defext);
            }
         }
      }

      for(i = sd->color_info.num_components; i < 4; ++ i) { /* clear unused */
         sd->stc.extv[i]   = NULL;
         sd->stc.sizv[i]   = 0;
         sd->stc.vals[i]   = NULL;

         sd->stc.extc[i] = NULL;
         sd->stc.sizc[i] = 0;
         sd->stc.code[i] = NULL;

      }                                                     /* clear unused */

/*
 * Guess default depth from range of values
 */
      if((sd->stc.dither != oldstc.dither)||(oldstc.vals[0] == NULL)) {

         if((sd->stc.dither->flags & STC_CMYK10) != 0) {

            sd->stc.flags       |= STCCMYK10;
            sd->stc.bits         = 10;
            sd->color_info.depth = 32;

         } else {

            sd->stc.flags       &= ~STCCMYK10;

            if((sd->stc.dither->flags & STC_FLOAT) != STC_FLOAT) {
               fv = 2.0;
               for(i = 1;(i  < gx_color_value_bits) &&
                  (fv <= (sd->stc.dither->minmax[1]-sd->stc.dither->minmax[0]));
                 ++i) fv *= 2.0;

            } else {
               i = 8; /* arbitrary */
            }

            if((i*sd->color_info.num_components) > (sizeof(stc_pixel)*8)) {

               sd->stc.bits         = (sizeof(stc_pixel)*8) /
                                       sd->color_info.num_components;
               sd->color_info.depth = sd->stc.bits * sd->color_info.num_components;

            } else {

               sd->stc.bits         = i;
               sd->color_info.depth = sd->stc.bits * sd->color_info.num_components;

            }
         }
      }

   } else {

      code = gs_error_rangecheck;

   }               /* verify new value */
   if(code < 0) param_signal_error(plist,"Dithering",code);
   error = error > code ? code : error;

/*
 * now fetch the desired depth, if the algorithm allows it
 */
/*
 * Arrrgh: We get code == 0, even if nobody sets BitsPerPixel.
 *         The value is the old one, but this may cause trouble
 *         with CMYK10.
 */
   code = param_read_int(plist, "BitsPerPixel", &i);
   if((error == 0) && (code == 0) &&
      (((sd->stc.flags & STCCMYK10) == 0) || (i != sd->color_info.depth))) {

      if((1 > i) || (i > (sizeof(stc_pixel)*8)))
         code = gs_error_rangecheck;
      else
         sd->color_info.depth = i;

      sd->stc.bits = i / sd->color_info.num_components;

      if(1 > sd->stc.bits) code = gs_error_rangecheck;

      if((sd->stc.dither->flags & STC_DIRECT) &&
         (sd->stc.dither->flags & STC_CMYK10))
         code           = gs_error_rangecheck;
      else
         sd->stc.flags &= ~STCCMYK10;

   }
   if(code < 0) param_signal_error(plist,"BitsPerPixel",code);
   error = error > code ? code : error;

/*
 * Fetch OutputCode
 */
   code  = param_read_string(plist,"OutputCode",&ps);
   if(code == 0) {   /* Analyze the OutputCode-String */

      for(l = ps.size; (l > 0) && (ps.data[l-1] == 0); --l);

      sd->stc.flags &= ~STCCOMP;
      if(!stc_putcmp("plain"))         sd->stc.flags |= STCPLAIN;
      else if(!stc_putcmp("deltarow")) sd->stc.flags |= STCDELTA;

   }                 /* Analyze the OutputCode-String */
   if((sd->stc.flags & STCCOMP) == STCDELTA) {
      sd->stc.flags |=  STCUWEAVE;
      sd->stc.flags &= ~STCNWEAVE;
   }
   if(code < 0) param_signal_error(plist,"OutputCode",code);
   error = error > code ? code : error;

/*
 * fetch the weave-mode (noWeave wins)
 */
   b1 = sd->stc.flags & STCUWEAVE ? true : false;
   b2 = sd->stc.flags & STCNWEAVE ? true : false;
   b3 = sd->stc.flags & (STCUWEAVE|STCNWEAVE) ? false : true;

   code = param_read_bool(plist,"Microweave",&b1);
   if(code < 0) {
      param_signal_error(plist,"Microweave",code);
   } else if(code == 0) {
      if(b1) { b2 = false; b3 = false; }
   }
   error = error > code ? code : error;

   code = param_read_bool(plist,"noWeave",&b2);
   if(code < 0) {
      param_signal_error(plist,"noWeave",code);
   } else if (code == 0) {
      if(b2) { b1 = false; b3 = false; }
   }
   error = error > code ? code : error;

   code = param_read_bool(plist,"Softweave",&b3);
   if(code < 0) {
      param_signal_error(plist,"Softweave",code);
   } else if (code == 0) {
      if(b3) { b1 = false; b2 = false; }
   }
   error = error > code ? code : error;

   if(b1) sd->stc.flags |=  STCUWEAVE;
   else   sd->stc.flags &= ~STCUWEAVE;

   if(b2) sd->stc.flags |=  STCNWEAVE;
   else   sd->stc.flags &= ~STCNWEAVE;

/*
 * Check the simple Flags
 */
#  define stc_read_flag(Mask,Name)                \
      code = param_read_bool(plist,Name,&b1);     \
      if(code < 0) {                              \
         param_signal_error(plist,Name,code);     \
      } else if(code == 0) {                      \
         if(b1 == true) sd->stc.flags |=  Mask;   \
         else           sd->stc.flags &= ~(Mask); \
      }                                           \
      error = error > code ? code : error;

   stc_read_flag(STCUNIDIR,"Unidirectional")
   stc_read_flag(STCDFLAG0, "Flag0")
   stc_read_flag(STCDFLAG1, "Flag1")
   stc_read_flag(STCDFLAG2, "Flag2")
   stc_read_flag(STCDFLAG3, "Flag3")
   stc_read_flag(STCDFLAG4, "Flag4")

/*
 * Now deal with the escp-Stuff
 */
#  define stc_read_int(Mask,Name,Val)             \
      code = param_read_int(plist,Name,&Val);     \
      if(code < 0)                                \
         param_signal_error(plist,Name,code);     \
      else if(code == 0)                          \
         sd->stc.flags |= Mask;                   \
      error = error > code ? code : error

   stc_read_int(STCBAND,  "escp_Band",  sd->stc.escp_m);
   stc_read_int(STCWIDTH, "escp_Width", sd->stc.escp_width);
   stc_read_int(STCHEIGHT,"escp_Height",sd->stc.escp_height);
   stc_read_int(STCTOP,   "escp_Top",   sd->stc.escp_top);
   stc_read_int(STCBOTTOM,"escp_Bottom",sd->stc.escp_bottom);

#  undef stc_read_int

   code = param_read_string(plist,"escp_Init",&sd->stc.escp_init);
   if(code == 0) sd->stc.flags |= STCINIT;
   error = error > code ? code : error;

   code = param_read_string(plist,"escp_Release",&sd->stc.escp_release);
   if(code == 0) sd->stc.flags |= STCRELEASE;
   error = error > code ? code : error;

/*
 * ColorAdjustMatrix must match the required size,
 * setting it explicitly to null, erases old matrix
 */
   code = param_read_float_array(plist,"ColorAdjustMatrix",&pfa);
   if((error == 0) && (code == 0)) {
      if(((sd->color_info.num_components == 1) && (pfa.size ==  3)) ||
         ((sd->color_info.num_components == 3) && (pfa.size ==  9)) ||
         ((sd->color_info.num_components == 4) && (pfa.size == 16)))
         sd->stc.am = (float *) pfa.data;
      else
         code =  gs_error_rangecheck;
   } else if(code < 0) {
      code = param_read_null(plist,"ColorAdjustMatrix");
      if(code == 0) sd->stc.am = NULL;
   }
   if(code < 0) param_signal_error(plist,"ColorAdjustMatrix",code);
   error = error > code ? code : error;

/*
 * Read the external array-Parameters
 */
   if(sd->color_info.num_components == 1) {        /* DeviceGray */

      stc_read_xarray(0,"Kcoding","Ktransfer");

      stc_read_null("Rcoding"); stc_read_null("Rtransfer");
      stc_read_null("Gcoding"); stc_read_null("Gtransfer");
      stc_read_null("Bcoding"); stc_read_null("Btransfer");

      stc_read_null("Ccoding"); stc_read_null("Ctransfer");
      stc_read_null("Mcoding"); stc_read_null("Mtransfer");
      stc_read_null("Ycoding"); stc_read_null("Ytransfer");

   } else if(sd->color_info.num_components == 3) { /* DeviceRGB */

      stc_read_xarray(0,"Rcoding","Rtransfer");
      stc_read_xarray(1,"Gcoding","Gtransfer");
      stc_read_xarray(2,"Bcoding","Btransfer");

      stc_read_null("Ccoding"); stc_read_null("Ctransfer");
      stc_read_null("Mcoding"); stc_read_null("Mtransfer");
      stc_read_null("Ycoding"); stc_read_null("Ytransfer");
      stc_read_null("Kcoding"); stc_read_null("Ktransfer");

   } else {                                        /* DeviceCMYK */

      stc_read_xarray(0,"Ccoding","Ctransfer");
      stc_read_xarray(1,"Mcoding","Mtransfer");
      stc_read_xarray(2,"Ycoding","Ytransfer");
      stc_read_xarray(3,"Kcoding","Ktransfer");

      stc_read_null("Rcoding"); stc_read_null("Rtransfer");
      stc_read_null("Gcoding"); stc_read_null("Gtransfer");
      stc_read_null("Bcoding"); stc_read_null("Btransfer");

   }
/*
 * Update remaining color_info values
 */
   if(error == 0) {

/*    compute #values from the component-bits */
      sd->color_info.max_gray  = sd->stc.bits < gx_color_value_bits ?
                            (1<<sd->stc.bits)-1 : gx_max_color_value;

/*    An integer-algorithm might reduce the number of values */
      if(((sd->stc.dither->flags & STC_TYPE) != STC_FLOAT) &&
         ((sd->stc.dither->minmax[1]-sd->stc.dither->minmax[0]) <
           sd->color_info.max_gray))
         sd->color_info.max_gray =
                sd->stc.dither->minmax[1]-sd->stc.dither->minmax[0]+0.5;

      sd->color_info.max_color = sd->color_info.num_components < 3 ? 0 :
                                 sd->color_info.max_gray;
      sd->color_info.dither_grays =
          sd->color_info.max_gray < gx_max_color_value ?
          sd->color_info.max_gray+1  : gx_max_color_value;
      sd->color_info.dither_colors  = sd->color_info.num_components < 3 ? 0 :
          sd->color_info.dither_grays;
   }

/*
 * Call superclass-Update
 */

   code = gdev_prn_put_params(pdev, plist);
   error = error > code ? code : error;

/*
 * Arrrgh, writing BitsPerPixel is really *VERY* special:
 *    gdev_prn_put_params verifies, that the external value
 *    is written, if not, it raises a rangecheck-error.
 *    On the other hand ghostscript is quite unhappy with odd
 *    values, so we do the necessary rounding *AFTER* the
 *    "superclass-Update".
 */

   if(sd->color_info.depth ==  3) sd->color_info.depth = 4;
   else if(sd->color_info.depth > 4)
      sd->color_info.depth =  (sd->color_info.depth+7) & ~7;

/*
 * Allocate the storage for the arrays in memory
 */
   if(error == 0) { /* Allocate new external-arrays */

     for(i = 0; i < sd->color_info.num_components; ++i){ /* Active components */
        int j;

        if((sd->stc.extv[i] != oldstc.extv[i]) &&
           (sd->stc.extv[i] != defext        )) { /* Value-Arrays */

           for(j = 0; j < i; ++j)
              if((sd->stc.sizv[j] == sd->stc.sizv[i]) &&
                 (memcmp(sd->stc.extv[j],sd->stc.extv[i],
                         sd->stc.sizv[i]*sizeof(float)) == 0)) break;

           if(j < i) {
              sd->stc.extv[i] = sd->stc.extv[j];
           } else {
              fp = gs_malloc(sd->stc.sizv[i],sizeof(float),"stc_put_params");
              if(fp != NULL)
                 memcpy(fp,sd->stc.extv[i],sd->stc.sizv[i]*sizeof(float));
               else
                 code = gs_error_VMerror;
               sd->stc.extv[i] = fp;
           }
        }                                         /* Value-Arrays */

        if((sd->stc.sizc[i] > 1) &&
           (sd->stc.extc[i] != oldstc.extc[i])) { /* Code-Arrays */

           for(j = 0; j < i; ++j)
              if((sd->stc.sizc[j] == sd->stc.sizc[i]) &&
                 (memcmp(sd->stc.extc[j],sd->stc.extc[i],
                         sd->stc.sizc[i]*sizeof(float)) == 0)) break;

           if(j < i) {
              sd->stc.extc[i] = sd->stc.extc[j];
           } else {
              fp = gs_malloc(sd->stc.sizc[i],sizeof(float),"stc_put_params");
              if(fp != NULL)
                 memcpy(fp,sd->stc.extc[i],sd->stc.sizc[i]*sizeof(float));
               else
                 code = gs_error_VMerror;
               sd->stc.extc[i] = fp;
           }
        }                                         /* Code-Arrays */

     }                                                   /* Active components */

     if((sd->stc.am != NULL) && (sd->stc.am != oldstc.am)) {
        if(     sd->color_info.num_components == 1) i =  3;
        else if(sd->color_info.num_components == 3) i =  9;
        else                                        i = 16;
        fp = gs_malloc(i,sizeof(float),"stc_put_params");
        if(fp != NULL) memcpy(fp,sd->stc.am,i*sizeof(float));
        else           code = gs_error_VMerror;
        sd->stc.am = fp;
     }

     if(sd->stc.escp_init.data != oldstc.escp_init.data) {
        byte *ip = NULL;

        if(sd->stc.escp_init.size > 0) {
           ip = gs_malloc(sd->stc.escp_init.size,1,"stcolor/init");
           if(ip == NULL) {
              code = gs_error_VMerror;
              sd->stc.escp_init.size = 0;
           } else {
              memcpy(ip,sd->stc.escp_init.data,sd->stc.escp_init.size);
           }
        }
        sd->stc.escp_init.data       = ip;
        sd->stc.escp_init.persistent = false;
     }

     if(sd->stc.escp_release.data != oldstc.escp_release.data) {
        byte *ip = NULL;

        if(sd->stc.escp_release.size > 0) {
           ip = gs_malloc(sd->stc.escp_release.size,1,"stcolor/release");
           if(ip == NULL) {
              code = gs_error_VMerror;
              sd->stc.escp_release.size = 0;
           } else {
              memcpy(ip,sd->stc.escp_release.data,sd->stc.escp_release.size);
           }
        }
        sd->stc.escp_release.data       = ip;
        sd->stc.escp_release.persistent = false;
     }

     if(code < 0) { /* free newly allocated arrays */

        if((sd->stc.am != NULL) && (sd->stc.am != oldstc.am)) {
           if(     sd->color_info.num_components == 1) i =  3;
           else if(sd->color_info.num_components == 3) i =  9;
           else                                        i = 16;
           gs_free(sd->stc.am,i,sizeof(float),"stc_put_params");
        }

        if((sd->stc.escp_init.data != NULL) &&
           (sd->stc.escp_init.data != oldstc.escp_init.data))
           gs_free((byte *) sd->stc.escp_init.data,sd->stc.escp_init.size,1,
              "stcolor/init");

        if((sd->stc.escp_release.data != NULL) &&
           (sd->stc.escp_release.data != oldstc.escp_release.data))
           gs_free((byte *) sd->stc.escp_release.data,sd->stc.escp_release.
              size,1,"stcolor/release");

        for(i = 0; i < sd->color_info.num_components; ++i) { /* components */
           int j;

           if((sd->stc.extc[i] != NULL) &&
              (sd->stc.extc[i] != defext) &&
              (sd->stc.extc[i] != oldstc.extc[i])) {

              for(j = 0; j < i; ++j)
                 if(sd->stc.extc[i] == sd->stc.extc[j]) break;

              if(i == j) gs_free(sd->stc.extc[i],sd->stc.sizc[i],sizeof(float),
                            "stc_put_params");
           }

           if((sd->stc.extv[i] != NULL) &&
              (sd->stc.extv[i] != oldstc.extv[i]) &&
              (sd->stc.extv[i] != defext)) {

              for(j = 0; j < i; ++j)
                 if(sd->stc.extv[i] == sd->stc.extv[j]) break;

              if(i == j) gs_free(sd->stc.extv[i],sd->stc.sizv[i],sizeof(float),
                            "stc_put_params");
           }
        }                                                    /* components */
     }              /* free newly allocated arrays */
   }                /* Allocate new arrays */
   error = error > code ? code : error;

/*
 * finally decide upon restore or release of old, unused data
 */
   if(error != 0) { /* Undo changes */

      memcpy(&sd->color_info,&oldcolor,sizeof(oldcolor));
      memcpy(&sd->stc       ,&oldstc  ,sizeof(oldstc  ));
   } else {        /* undo / release */

      if((oldstc.escp_init.data != NULL) &&
         (oldstc.escp_init.data != sd->stc.escp_init.data)) {
            gs_free((byte *)oldstc.escp_init.data,
                            oldstc.escp_init.size,1,"stcolor/init");
      }

      if((oldstc.escp_release.data != NULL) &&
         (oldstc.escp_release.data != sd->stc.escp_release.data)) {
            gs_free((byte *)oldstc.escp_release.data,
                            oldstc.escp_release.size,1,"stcolor/release");
      }

      if((oldstc.am != NULL) && (oldstc.am != sd->stc.am)) {
         if(     oldcolor.num_components == 1) i =  3;
         else if(oldcolor.num_components == 3) i =  9;
         else                                  i = 16;
         gs_free(oldstc.am,i,sizeof(float),"stc_put_params");
      }

      for(i = 0; i < 4; ++i) {
         int j;

         if((oldstc.extc[i] != NULL) &&
            (oldstc.extc[i] != sd->stc.extc[i]) &&
            (oldstc.dither  != NULL) &&
            (oldstc.extc[i] != defext)) {

            for(j = 0; j < i; ++j) if(oldstc.extc[i] == oldstc.extc[j]) break;

            if(i == j) gs_free(oldstc.extc[i],oldstc.sizc[i],sizeof(float),
                            "stc_put_params");
         }

         if((oldstc.extv[i] != NULL) &&
            (oldstc.extv[i] != sd->stc.extv[i]) &&
            (oldstc.extv[i] != defext)) {

            for(j = 0; j < i; ++j) if(oldstc.extv[i] == oldstc.extv[j]) break;

            if(i == j) gs_free(oldstc.extv[i],oldstc.sizv[i],sizeof(float),
                            "stc_put_params");
         }
      }

/*
 * Close the device if colormodel changed or recomputation
 * of internal arrays is required
 */
      if(sd->is_open) { /* we might need to close it */
         bool doclose = false;
         if((sd->color_info.num_components != oldcolor.num_components) ||
            (sd->color_info.depth          != oldcolor.depth         ) ||
            (sd->stc.bits                  != oldstc.bits            ) ||
            (sd->stc.dither                != oldstc.dither          ))
            doclose = true;

         for(i = 0; i < sd->color_info.num_components; ++i) {
            if(sd->stc.extv[i] != oldstc.extv[i]) doclose = true;
            if(sd->stc.extc[i] != oldstc.extc[i]) doclose = true;
         }
         if(doclose) {
            stc_freedata(&oldstc);
            for(i = 0; i < 4; ++i) {
               sd->stc.vals[i] = NULL;
               sd->stc.code[i] = NULL;
            }

            gs_closedevice(pdev);
         }
      }                 /* we might need to close it */

   }

   return error;
}
/*
 * 1Bit CMYK-Algorithm
 */

private int
stc_gscmyk(stcolor_device *sdev,int npixel,byte *in,byte *buf,byte *out)
{

   byte *ip = in;
   int   error = 0;


/* ============================================================= */
   if(npixel > 0) {  /* npixel >  0 -> scanline-processing       */
/* ============================================================= */

      int p;

/*
 *    simply split the two pixels rsiding in a byte
 */
      for(p = npixel; p > 0; --p) { /* loop over pixels */
         byte tmp =*ip++;

         *out++ = (tmp>>4) & 15;
         if(--p <= 0) break;

         *out++ =  tmp     & 15;

      }                                   /* loop over pixels */

/* ============================================================= */
   } else {          /* npixel <= 0 -> initialisation            */
/* ============================================================= */

/*    we didn't check for the white-calls above, so this may cause errors */
      if(sdev->stc.dither->flags & STC_WHITE)              error = -1;

/*    if we're not setup for bytes, this is an error too */
      if((sdev->stc.dither->flags & STC_TYPE) != STC_BYTE) error = -2;

/*    This IS a direct-driver, so STC_DIRECT must be set! */
      if((sdev->stc.dither->flags & STC_DIRECT) == 0)      error = -3;

/*    and cmyk-mode is the only supported mode */
      if(sdev->color_info.num_components != 4)             error = -4;

/*    and we support only 4Bit-Depth here */
      if(sdev->color_info.depth != 4)                      error = -5;

/* ============================================================= */
   } /* scanline-processing or initialisation */
/* ============================================================= */

   return error;
}

/*
 * The following is an algorithm under test
 */
private int
stc_hscmyk(stcolor_device *sdev,int npixel,byte *in,byte *buf,byte *out)
{

/* ============================================================= */
   if(npixel < 0) {  /* npixel <= 0 -> initialisation            */
/* ============================================================= */

      int i,i2do;
      long *lp = (long *) buf;

/* CMYK-only algorithm */
      if( sdev->color_info.num_components != 4)                      return -1;

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
          (1 + 2*sdev->color_info.num_components)))                  return -3;

/*
 * must have STC_CMYK10, STC_DIRECT, but not STC_WHITE
 */
      if((sdev->stc.dither->flags & STC_CMYK10) == 0)                return -4;
      if((sdev->stc.dither->flags & STC_DIRECT) == 0)                return -5;
      if((sdev->stc.dither->flags & STC_WHITE ) != 0)                return -6;

/*
 * Must have values between 0-1023.0
 */
      if((sdev->stc.dither->minmax[0] !=    0.0) ||
         (sdev->stc.dither->minmax[1] != 1023.0))                    return -7;
/*
 * initialize buffer
 */

     i2do            = 1 + 8 - 4 * npixel;
     lp[0] = 0;

      if(sdev->stc.flags & STCDFLAG0) {
        for(i = 1; i < i2do; ++i) lp[i] = 0;
      } else {
        for(i = 1; i < i2do; ++i)  lp[i] = (rand() % 381) - 190;
      }

/* ============================================================= */
   } else {  /* npixel > 0 && in != NULL  -> scanline-processing */
/* ============================================================= */

      long errc[4],*errv;
      int             step  = buf[0] ? -1 : 1;
      stc_pixel *ip    =  (stc_pixel *) in;

      buf[0] = ~ buf[0];
      errv   =  (long *) buf + 5;

      if(step < 0) {
        ip   += npixel-1;
        out  += npixel-1;
        errv += 4*(npixel-1);
      }

      errc[0] = 0; errc[1] = 0; errc[2] = 0; errc[3] = 0;

      while(npixel-- > 0) {

         register  stc_pixel ci,mode;
         register  long           k,v,n;
         register  int pixel; /* internal pixel-value */

         ci      = *ip; ip += step;

         mode    = ci & 3;
         k       = (ci>>2) & 0x3ff;
         pixel   = 0;

         v       = k+errv[3]+((7*errc[3])>>4);

         if(mode == 3) { /* only Black allowed to fire */

            if(v > 511) {
               v     -= 1023;
               pixel  = BLACK;
            }
            errv[3-(step<<2)] += ((3*v+8)>>4);        /* 3/16 */
            errv[3]            = ((5*v+errc[3]+8)>>4);/* 5/16 +1/16 (rest) */
            errc[3]            = v;

            errv[0] = errv[0] < -190 ? -190 : errv[0] < 190 ? errv[0] : 190;
            errv[1] = errv[1] < -190 ? -190 : errv[1] < 190 ? errv[1] : 190;
            errv[2] = errv[2] < -190 ? -190 : errv[2] < 190 ? errv[2] : 190;

            errc[0] = 0; errc[1] = 0; errc[2] = 0;

         } else if(v > 511) { /* black known to fire */

            v    -= 1023;
            pixel = BLACK;

            errv[3-(step<<2)] += ((3*v+8)>>4);        /* 3/16 */
            errv[3]            = ((5*v+errc[3]+8)>>4);/* 5/16 +1/16 (rest) */
            errc[3]            = v;

            n = (ci>>12) & 0x3ff;

            if(mode == 2) { v = k; }
            else          { v = n; n = (ci>>22) & 0x3ff; }

            v += errv[2]+((7*errc[2])>>4)-1023;
            if(v < -511) v = -511;
            errv[2-(step<<2)] += ((3*v+8)>>4);        /* 3/16 */
            errv[2]            = ((5*v+errc[2]+8)>>4);/* 5/16 +1/16 (rest) */
            errc[2]            = v;

            if(mode == 1) { v = k; }
            else          { v = n; n = (ci>>22) & 0x3ff; }

            v += errv[1]+((7*errc[1])>>4)-1023;
            if(v < -511) v = -511;
            errv[1-(step<<2)] += ((3*v+8)>>4);        /* 3/16 */
            errv[1]            = ((5*v+errc[1]+8)>>4);/* 5/16 +1/16 (rest) */
            errc[1]            = v;

            if(mode == 0) v = k;
            else          v = n;

            v += errv[0]+((7*errc[0])>>4)-1023;
            if(v < -511) v = -511;
            errv[0-(step<<2)] += ((3*v+8)>>4);        /* 3/16 */
            errv[0]            = ((5*v+errc[0]+8)>>4);/* 5/16 +1/16 (rest) */
            errc[0]            = v;

         } else { /* Black does not fire initially */

            long kv = v; /* Black computed after colors */

            n = (ci>>12) & 0x3ff;

            if(mode == 2) { v = k; }
            else          { v = n; n = (ci>>22) & 0x3ff; }

            v += errv[2]+((7*errc[2])>>4);
            if(v > 511) {
               pixel |= YELLOW;
               v     -= 1023;
            }
            errv[2-(step<<2)] += ((3*v+8)>>4);        /* 3/16 */
            errv[2]            = ((5*v+errc[2]+8)>>4);/* 5/16 +1/16 (rest) */
            errc[2]            = v;

            if(mode == 1) { v = k; }
            else          { v = n; n = (ci>>22) & 0x3ff; }

            v += errv[1]+((7*errc[1])>>4);
            if(v > 511) {
               pixel |= MAGENTA;
               v     -= 1023;
            }
            errv[1-(step<<2)] += ((3*v+8)>>4);        /* 3/16 */
            errv[1]            = ((5*v+errc[1]+8)>>4);/* 5/16 +1/16 (rest) */
            errc[1]            = v;

            if(mode == 0) v = k;
            else          v = n;

            v += errv[0]+((7*errc[0])>>4);
            if(v > 511) {
               pixel |= CYAN;
               v     -= 1023;
            }
            errv[0-(step<<2)] += ((3*v+8)>>4);        /* 3/16 */
            errv[0]            = ((5*v+errc[0]+8)>>4);/* 5/16 +1/16 (rest) */
            errc[0]            = v;

            v = kv;
            if(pixel == (CYAN|MAGENTA|YELLOW)) {
               pixel = BLACK;
               v     = v > 511 ? v-1023 : -511;
            }
            errv[3-(step<<2)] += ((3*v+8)>>4);        /* 3/16 */
            errv[3]            = ((5*v+errc[3]+8)>>4);/* 5/16 +1/16 (rest) */
            errc[3]            = v;

         }

         errv += step<<2;
         *out  = pixel; out += step;

      }                                         /* loop over pixels */

/* ============================================================= */
   } /* initialisation, white or scanline-processing             */
/* ============================================================= */

   return 0;
}
