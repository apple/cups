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

/* gdevstc.h */
/* Epson Stylus-Color Printer-Driver */
#ifndef   gdevstc_INCLUDED
#  define gdevstc_INCLUDED

/***
 *** This holds most of the declarations used by gdevstc.c/stcolor.
 *** It should be included by the dithering-routines and should be
 *** modified to include the separately compilable routines.
 ***/

/*** Ghostscript-Headers ***/

#include "gdevprn.h"
#include "gsparam.h"
#include "gsstate.h"

/*** Private Type for 32Bit-Pixels ***/
#if     arch_log2_sizeof_int < 2  /* int is too small */
   typedef unsigned long stc_pixel;
#else                             /* int is sufficient */
   typedef unsigned int  stc_pixel;
#endif                            /* use int or long ? */

/*** Auxillary-Device Structure ***/

typedef struct stc_s {
   long            flags;      /* some mode-flags */
   int             bits;       /* the number of bits per component */
   const struct stc_dither_s  *dither;     /* dithering-mode */
   float          *am;         /* 3/9/16-E. vector/matrix */

   float          *extc[4];    /* Given arrays for stccode */
   uint            sizc[4];    /* Size of extcode-arrays */
   gx_color_value *code[4];    /* cv -> internal */

   float          *extv[4];    /* Given arrays for stcvals */
   uint            sizv[4];    /* Size of extvals-arrays */
   byte           *vals[4];    /* internal -> dithering */

   stc_pixel  white_run[3];    /* the white-pattern */
   stc_pixel  white_end[3];    /* the white-Trailer */
   gs_param_string_array
                   algorithms; /* Names of the available algorithms */

   gs_param_string escp_init;     /* Initialization-Sequence */
   gs_param_string escp_release;  /* Initialization-Sequence */
   int             escp_width; /* Number of Pixels printed */
   int             escp_height;/* Height send to the Printer */
   int             escp_top;   /* Top-Margin, send to the printer */
   int             escp_bottom;/* Bottom-Margin, send to the printer */

   int             alg_item;   /* Size of the items used by the algorithm */

   int             prt_buf;    /* Number of buffers */
   int             prt_size;   /* Size of the Printer-buffer */
   int             escp_size;  /* Size of the ESC/P2-buffer */
   int             seed_size;  /* Size of the seed-buffers */

   int             escp_u;     /* print-resolution (3600 / ydpi )*/
   int             escp_c;     /* selected color */
   int             escp_v;     /* spacing within band */
   int             escp_h;     /* 3600 / xdpi */
   int             escp_m;     /* number of heads */
   int             escp_lf;    /* linefeed in units */

   int             prt_y;      /* print-coordinate */
   int             stc_y;      /* Next line 2b printed */
   int             buf_y;      /* Next line 2b loaded into the buffer */
   int             prt_scans;  /* number of lines printed */


   int            *prt_width;  /* Width of buffered lines */
   byte          **prt_data;   /* Buffered printer-lines */
   byte           *escp_data;  /* Buffer for ESC/P2-Data */
   byte           *seed_row[4];/* Buffer for delta-row compression (prt_size) */

} stc_t;

/*** Main-Device Structure ***/

typedef struct stcolor_device_s {
	gx_device_common;
	gx_prn_device_common;
        stc_t stc;
} stcolor_device;

#define STCDFLAG0  0x000001L /* Algorithm-Bit 0 */
#define STCDFLAG1  0x000002L /* Algorithm-Bit 1 */
#define STCDFLAG2  0x000004L /* Algorithm-Bit 2 */
#define STCDFLAG3  0x000008L /* Algorithm-Bit 3 */
#define STCDFLAG4  0x000010L /* Algorithm-Bit 4 */
#define STCCMYK10  0x000020L /* CMYK10-Coding active */

#define STCUNIDIR  0x000040L /* Unidirectional, if set */
#define STCUWEAVE  0x000080L /* Hardware Microweave */
#define STCNWEAVE  0x000100L /* Software Microweave disabled */

#define STCOK4GO   0x000200L /* stc_put_params was o.k. */

#define STCCOMP    0x000C00L /* RLE, Plain (>= 1.18) */
#define STCPLAIN   0x000400L /* No compression */
#define STCDELTA   0x000800L /* Delta-Row */

#define STCMODEL   0x00f000L /* STC, ST800 */
#define STCST800   0x001000L /* Monochrome-Variant */
#define STCSTCII   0x002000L /* Stylus Color II */

#define STCBAND    0x010000L /* Initialization defined */
#define STCHEIGHT  0x020000L /* Page-Length set */
#define STCWIDTH   0x040000L /* Page-Length set */
#define STCTOP     0x080000L /* Top-Margin set */
#define STCBOTTOM  0x100000L /* Bottom-Margin set */
#define STCINIT    0x200000L /* Initialization defined */
#define STCRELEASE 0x400000L /* Release defined */

#define STCPRINT   0x800000L /* Data printed */

/*** Datatype for the array of dithering-Algorithms ***/

#define stc_proc_dither(name) \
 int name(P5(stcolor_device *sdev,int npixel,byte *in,byte *buf,byte *out))

typedef struct stc_dither_s {
  const char *name; /* Mode-Name for Dithering */
  stc_proc_dither((*fun));
  uint        flags;
  uint        bufadd;
  double      minmax[2];
} stc_dither_t;

/*
 * Color-Values for the output
 */
#define BLACK   1 /* in monochrome-Mode as well as in CMYK-Mode */
#define RED     4 /* in RGB-Mode */
#define GREEN   2
#define BLUE    1
#define CYAN    8 /* in CMYK-Mode */
#define MAGENTA 4
#define YELLOW  2

/*** A Macro to ease Type-depending things with the stc_p-union ***/

#define STC_TYPESWITCH(Dither,Action)             \
   switch((Dither)->flags & STC_TYPE)  {          \
   case STC_BYTE: Action(byte); break;            \
   case STC_LONG: Action(long); break;            \
   default:       Action(float); break;}

/***
 *** MODIFY HERE to include your routine:
 ***
 *** 1. Declare it here
 *** 2. Add it to the definition of STC_MODI
 *** 3. Add your file to the dependency-list in the Makefile & devices.mak
 ***/

/* Step 1. */
stc_proc_dither(stc_gsmono);  /* resides in gdevstc1.c */
stc_proc_dither(stc_fs);      /* resides in gdevstc2.c */
stc_proc_dither(stc_fscmyk);  /* resides in gdevstc2.c too */
stc_proc_dither(stc_gsrgb);   /* resides in gdevstc3.c */
stc_proc_dither(stc_fs2);     /* resides in gdevstc4.c */


/* Values used to assemble flags */
#define DeviceGray  1 /* ProcessColorModel = DeviceGray  */
#define DeviceRGB   3 /* ProcessColorModel = DeviceRGB   */
#define DeviceCMYK  4 /* ProcessColorModel = DeviceCMYK  */

#define STC_BYTE    8 /* Pass Bytes  to the Dithering-Routine */
#define STC_LONG   16 /* Pass Longs  to the Dithering-Routine */
#define STC_FLOAT  24 /* Pass Floats to the Dithering-Routine */
#define STC_TYPE   24 /* all the type-bits */

#define STC_CMYK10 32 /* Special 32-Bit CMYK-Coding */
#define STC_DIRECT 64 /* Suppress conversion of Scanlines */
#define STC_WHITE 128 /* Call Algorithm for white lines too (out == NULL) */
#define STC_SCAN  256 /* multiply by number of scanlines in buffer */

/* Step 2. */
/* Items: 1. Name to activate it
          2. Name of the dithering-function
          3. Several flags ored together, including # of buffered scanlines
          4. Additional buffer-space (bytes/longs/floats)
          5. Array of double with minimum and maximum-value
   Keep the last line as it is.
 */

#define STC_MODI \
{"gsmono", stc_gsmono, DeviceGray|STC_BYTE,0,{0.0,1.0}},\
{"gsrgb" , stc_gsrgb , DeviceRGB |STC_BYTE,0,{0.0,1.0}},\
{"fsmono", stc_fs, \
  DeviceGray|STC_LONG|1*STC_SCAN,3+3*1,{0.0,16777215.0}},\
{"fsrgb",  stc_fs, \
  DeviceRGB |STC_LONG|1*STC_SCAN,3+3*3,{0.0,16777215.0}},\
{"fsx4",   stc_fs, \
  DeviceCMYK|STC_LONG|1*STC_SCAN,3+3*4,{0.0,16777215.0}},\
{"fscmyk", stc_fscmyk, \
  DeviceCMYK|STC_LONG|1*STC_SCAN,3+3*4,{0.0,16777215.0}},\
{"fs2", stc_fs2, \
  DeviceRGB |STC_BYTE|STC_WHITE|1*STC_SCAN,0,{0.0,255.0}},


#ifndef   X_DPI
#define   X_DPI   360
#endif /* X_DPI */
#ifndef   Y_DPI 
#define   Y_DPI   360
#endif /* Y_DPI */

#ifndef   STC_L_MARGIN
#  define STC_L_MARGIN 0.125 /* yields 45 Pixel@360DpI */
#endif /* STC_L_MARGIN */
#ifndef   STC_B_MARGIN
#  define STC_B_MARGIN 0.555 /* yields 198 Pixel@#60DpI (looses 1mm) */
#endif /* STC_B_MARGIN */
/*
 * Right-Margin: Should match maximum print-width of 8".
 */

#ifndef   STC_R_MARGIN
#  ifdef A4
#    define STC_R_MARGIN 0.175 /* Yields 63 Pixel@360DpI */
#  else
#    define STC_R_MARGIN 0.375 /* 135 Pixel */
#  endif
#endif /* STC_R_MARGIN */
#ifndef   STC_T_MARGIN
#  define STC_T_MARGIN 0.125
#endif /* STC_T_MARGIN */

#endif
