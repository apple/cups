/* Copyright (C) 1992, 1993, 1994, 1996 by Aladdin Enterprises.  All rights reserved.
  
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
 
/* ---------------------------------------------------------- */ 
/* gdevimgn.c - version 1.4 */
/* Imagen ImPRESS printer driver */

/* This driver uses the Impress bitmap operation to print the
   page image. */
/* -------------------------------------------------------- */ 

/* Written by Alan Millar (AMillar@bolis.sf-bay.org) August 4 1992.
      Basic bitmap dump.  */
/* Updated by Alan Millar Sept 21 1992.  Added resolution handling 
      for 75, 150, and 300 dpi. */
/* Updated by Alan Millar June 05 1993.  General cleanup for
   beta test release. */
/* Updated by Alan Millar June 21 1993.  v1.3.  Combined multipage
   output into single imPress document.  Quote fewer special
   chars in byte stream mode.  imPress document header options
   can be set from environment variable IMPRESSHEADER */
/* Updated by Alan Millar July 04 1993.  v1.4. 
   New makefile option USE_BYTE_STREAM instead of changing source.
   Swatch output redone to eliminate ALL blank swatches (swatchMap).
   Buffer copying changed to multi-byte operations (BIGTYPE).
   Page margins and A4 paper settings fixed, at least for Canon CX.
   */

/* -------------------------------------------------------- */ 
/* Instructions:  

   - Add "imagen.dev" to DEVICE_DEVS in the makefile.  For example:
	DEVICE_DEVS2=laserjet.dev imagen.dev

   - Include or exclude USE_BYTE_STREAM in makefile as appropriate
     If you are compiling on Unix, re-run "tar_cat" to update the makefile
      from devs.mak

   - At run time, specify the resolution on the GS command line
      by using -r300 or -r150 or -r75
   - At run time, specify any imPress document options in
      the IMPRESSHEADER environment variable.
   */
 
/* -------------------------------------------------------- */
/* Hardware/software combinations tested:
   - ImageStation IP3 8/300 with parallel byte-stream interface,
       using GS 2.6.1 on Linux with GCC 2.3.3;
       earlier using GS 2.5.1 on MS-Dos with Turbo C++ 1.0
   - Sequenced-packet-protocol interface untested.
   */
/* -------------------------------------------------------- */
/* Bugs/Enhancements:
   - Driver does not use any Impress language features for 
     drawing lines/arcs
   - Driver does not use resident or downloadable fonts.
   - Buffer output instead of system call for each byte?
  */
 
/* -------------------------------------------------------- */ 
#include "gdevprn.h" 
/* #include <stdio.h> should not be used in drivers */
#include <stdlib.h>
 
/* -------------------------------------------------------- */ 
/* Working Constants */

/* Byte stream quoting: convert special characters to hex.
   Specify by including/excluding -DUSE_BYTE_STREAM in makefile.
   This should match printer's hardware interface configuration.
   If printer interface is serial with sequenced-packet-protocol 
     spooler software (ImageStation config# 11 = 01), then don't use it.
     Imagen "ipr" spooler software should not use byte stream.
   If printer interface is Centronics parallel byte stream, 
     (ImageStation config# 11 = 03), then use byte stream.  */

#ifdef USE_BYTE_STREAM
#  define BYTE_STREAM 1
#else
#  define BYTE_STREAM 0
#endif

/* Byte stream quote character (ImageStation config# 15).
   Only needed when using byte stream */
#define QUOTE_CHAR (char) 0x02
/* Byte stream end-of-file character (ImageStation config# 14). */
#define EOF_CHAR   (char) 0x04 
/* Other special characters to quote.  Put them here if spooler or
   hardware uses flow control, etc.   If not needed, set to
   a redundant value such as EOF_CHAR */
#define EXTRA_QUOTE1 (char) 0x11   /* ^Q */
#define EXTRA_QUOTE2 (char) 0x13   /* ^S */
#define EXTRA_QUOTE3 EOF_CHAR
#define EXTRA_QUOTE4 EOF_CHAR

/* -------------------------------------------------------- */ 
/* imPress header default options.  
   Can be overridden at run-time with IMPRESSHEADER env variable */

#define IMPRESSHEADER "jobheader onerror, prerasterization off"

/* -------------------------------------------------------- */ 

#define CANON_CX

/* Printer engine max resolution.  300 for Canon CX models such as 
   ImageStation IP3.  Others (240?) unverified */ 
#ifdef CANON_CX
#  define MAX_DPI 300 
#endif
#ifndef MAX_DPI
#  define MAX_DPI 300 
#endif

/* Determine imPress scaling factor from GS resolution.  
   Magnify can be 0, 1, or 2. 
    0 = MAX_DPI, 1 = MAX_DPI / 2, 2 = MAX_DPI / 4 
   Assuming MAX_DPI is 300, you can specify -r75 or -r150
    or -r300 on the GS command line  */ 
#define getMagnification  ( \
  ( pdev->x_pixels_per_inch > (MAX_DPI >> 1) ) ? 0 : \
  ( pdev->x_pixels_per_inch > (MAX_DPI >> 2) ) ? 1 : \
  2 )
 
/* Page dimensions from gdevprn.h - specify -DA4 in makefile for A4 paper */ 
#define WIDTH_10THS   DEFAULT_WIDTH_10THS
#define HEIGHT_10THS  DEFAULT_HEIGHT_10THS
 
/* Width in inches of unprintable edge of paper.  May need fine tuning.
   Canon CX engine in ImageStation IP3 8/300 will only print 8 inches
   wide on any paper size.  May vary for other engines */

#ifdef CANON_CX
#  define MARG_L 0.15
#  define MARG_R ( (float)WIDTH_10THS / 10.0 - 8.0 - MARG_L)
#endif
#ifndef MARG_L
#  define MARG_L 0.2
#endif
#ifndef MARG_R
#  define MARG_R 0.2
#endif
#define MARG_T 0.1 
#define MARG_B 0.2 
 

/* Flag for displaying debug messages at run-time.  Higher
	number = higher detail */
#define IM_DEBUG 0
#define DebugMsg(Level,P1,P2) if (Level<=IM_DEBUG) {fprintf(stderr,P1,P2 );}

/*-------------------------------------------*/ 
  /* Impress bitmaps are made up of 32x32 bit swatches. 
     A swatch is four bytes (32 bits) wide by 32 bytes high,
     totalling 128 bytes.  */
#define HorzBytesPerSw 4
#define HorzBitsPerSw (HorzBytesPerSw * 8)
#define VertBytesPerSw 32
#define TotalBytesPerSw (HorzBytesPerSw * VertBytesPerSw)
 
/*-------------------------------------------*/ 
/* Attempt at optimization to something faster than byte-by-byte copying.  
   imPress swatches are 4 bytes wide, so type must align on a 4-byte 
   boundary.  Swatch interleaving restricts the copy to 4 bytes in a row.  
   Type must be numeric where value is zero when all bytes in it are zero. */
#if arch_sizeof_long == 4
#  define BIGTYPE unsigned long int
#else
#  if arch_sizeof_short == 4
#    define BIGTYPE unsigned short int
#  else
#    if arch_sizeof_short == 2
#      define BIGTYPE unsigned short
#    endif
#  endif
#endif
#ifndef BIGTYPE
#define BIGTYPE byte
#endif

#define BIGSIZE ( sizeof( BIGTYPE ) )

/*-------------------------------------------*/ 
/*	IMAGEN imPress Command opcodes					*/ 
/* from DVIIMP.C */ 
#define iSP		128	/* advance one space			*/ 
#define	iSP1		129	/* advance one space + 1 pixel		*/ 
#define iMPLUS		131	/* Move one pixel forward		*/ 
#define	iMMINUS		132	/* Move one pixel back			*/ 
#define iMMOVE		133	/* Move in main advance direction	*/ 
#define iSMOVE		134	/* Move in secondary advance direction	*/ 
 
#define iABS_H		135	/* Move to H position			*/ 
#define iREL_H		136	/* Move in H direction			*/ 
#define iABS_V		137	/* Move to V position			*/ 
#define iREL_V		138	/* Move in V direction			*/ 
 
#define	iCRLF		197	/* move to beginning of next line	*/ 
 
#define	iSET_HV_SYSTEM	205	/* Define new coordinate system		*/ 
#define	iSET_ADV_DIRS	206	/* Define advance directions		*/ 
 
#define	iPAGE		213	/* Set H and V to 0			*/ 
#define iENDPAGE	219	/* print the current page		*/ 
 
#define iBITMAP		235	/* Print a full bitmap			*/ 
#define iSET_MAGNIFICATION 236 
				/* magnify the page by 1, 2, 4		*/ 
#define iNOOP		254	/* no operation				*/ 
#define iEOF		255	/* end of impress document		*/ 
 
/*-------------------------------------------*/ 
/*-------------------------------------------*/ 
/* The device descriptor */ 

private dev_proc_print_page(imagen_print_page); 
private dev_proc_open_device(imagen_prn_open);
private dev_proc_close_device(imagen_prn_close);

gx_device_procs imagen_procs =
	prn_procs(imagen_prn_open, gdev_prn_output_page, imagen_prn_close);

#define ppdev ((gx_device_printer *)pdev)

/*-------------------------------------------*/ 
gx_device_printer far_data gs_imagen_device = 
  prn_device(/*prn_std_procs*/ imagen_procs,
	"imagen", 
	WIDTH_10THS, 
	HEIGHT_10THS, 
	MAX_DPI,				/* x_dpi */ 
	MAX_DPI,				/* y_dpi */ 
	MARG_L,MARG_R,MARG_T,MARG_B,		/* margins */ 
	1, imagen_print_page); 
 
/*-------------------------------------------*/ 

/*-------------------------------------------*/ 
private void 
iWrite(FILE *Out, byte Val) 
{ /* iWrite */ 
  char *hexList = "0123456789ABCDEF"; 
 
  /* if we are doing byte-stream, quote characters that would otherwise
     match EOF and QUOTE itself, or other special chars */
  /* Imagen quoting takes one character and writes out the QUOTE
     character followed by the hex digits of the quoted character */
  if (BYTE_STREAM && 
     (   Val == QUOTE_CHAR   || Val == EOF_CHAR
      || Val == EXTRA_QUOTE1 || Val == EXTRA_QUOTE2
      || Val == EXTRA_QUOTE3 || Val == EXTRA_QUOTE4 ) ) { 
    fputc (QUOTE_CHAR, Out); 
    fputc ((char) hexList[Val / 0x10], Out); 
    fputc ((char) hexList[Val % 0x10], Out); 
  } else { /* quoted char */ 
    /* Not doing quoting, just send it out */
    fputc(Val, Out); 
  } /* quoted char */ 
} /* iWrite */ 
 
/* Write out 16bit, high byte first */ 
void 
iWrite2(FILE *Out, int Val) 
{ /* iWrite2 */ 
  iWrite(Out,(byte) (Val >> 8) & 0x00FF ); 
  iWrite(Out,(byte) Val        & 0x00FF ); 
} /* iWrite2 */ 
 
/* --------------------------------------------------------- */ 

private int
imagen_prn_open(gx_device *pdev)
{ /* imagen_prn_open */
  int	code;

  char *impHeader;

  /* ----------------------------------------- */
  DebugMsg(1,"%s\n","Start of imagen_prn_open");
  DebugMsg(2,"BIGSIZE = %ld \n",BIGSIZE);

  code = gdev_prn_open(pdev);
  if ( code < 0 ) return code;

  /* ----------------------------------------- */

  DebugMsg(2,"opening file: %s\n",ppdev->fname);
  code = gdev_prn_open_printer(pdev, 1);
  if ( code < 0 ) return code;

  impHeader = getenv("IMPRESSHEADER");
  if (impHeader == NULL ) {
    impHeader = IMPRESSHEADER ;
  } /* if impHeader */

  fprintf(ppdev->file,"@document(language impress, %s)",impHeader); 
  
  code = gdev_prn_close_printer(pdev);
  if ( code < 0 ) return code;

  /* ----------------------------------------- */
  DebugMsg(1,"%s\n","End of imagen_prn_open");

  return code;
} /* imagen_prn_open */

private int
imagen_prn_close(gx_device *pdev)
{ /* imagen_prn_close */
  int		code;

  /* ----------------------------------------- */
  DebugMsg(1,"%s\n","Start of imagen_prn_close");

  code = gdev_prn_open_printer(pdev, 1);
  if ( code < 0 ) return code;

  /* Write imPress end of document marker */
  iWrite(ppdev->file,iEOF); 

  /* And byte stream end of file */
  if (BYTE_STREAM) { 
    /* DON'T use iWrite because actual EOF should not be quoted! */
    fputc(EOF_CHAR,ppdev->file); 
  } /* if byte stream */ 

  fflush(ppdev->file);
  
  code = gdev_prn_close_printer(pdev);
  if ( code < 0 ) return code;

  code = gdev_prn_close(pdev);

  DebugMsg(1,"%s\n","End of imagen_prn_close");

  return(code);
} /* imagen_prn_close */

/*-------------------------------------------*/ 
/* Send the page to the printer. */ 
private int 
imagen_print_page(gx_device_printer *pdev, FILE *prn_stream) 
{ 
  int line_size = gdev_mem_bytes_per_scan_line((gx_device *)pdev); 
  /* input buffer: one line of bytes rasterized by gs */
  byte *in = (byte *)gs_malloc(BIGSIZE, line_size / BIGSIZE + 1,
	"imagen_print_page(in)"); 
  /* output buffer: 32 lines, interleaved into imPress swatches */
  byte *out; 
  /* working pointer into output buffer */	
  byte *swatch; 
  byte *temp;
  /* map of which swatches in a row are completely blank, or are non-blank */
  byte *swatchMap;
  /* starting line number on page of a row of swatches */
  int lnum ; 
  /* line number within a row of swatches */
  int swatchLine; 
  /* ending line number of row of swatches */
  int lastLine; 
  /* how many swatches can fit on a row */
  int swatchCount; 
  /* index into row of non-blank swatch */
  int startSwatch; 
  int endSwatch; 
  /* Scaling factor for resolution */
  int Magnify; 
  /* page totals */
  int totalBlankSwatches;
  int totalGreySwatches;

  /* ----------------------------------------- */
  /* Start of routine                          */
  /* ----------------------------------------- */

  DebugMsg(1,"%s\n","Start of imagen_print_page");

  /* ----------------------------------------- */
  Magnify = getMagnification ; 
 
  /* Impress bitmaps are made up of 32x32 bit swatches. 
     A swatch is four bytes wide by 32 bytes high. 
     See how many swatches will fit horizontally. */ 
 
  swatchCount = (line_size + HorzBytesPerSw - 1) / HorzBytesPerSw; 

  totalBlankSwatches = 0 ;
  totalGreySwatches = 0 ;
  DebugMsg(2,"Swatch count = %d\n",swatchCount);
  DebugMsg(2,"Line size = %d\n",line_size );

  out = (byte *)gs_malloc(TotalBytesPerSw , swatchCount + 1, 
		    "imagen_print_page(out)"); 

  swatchMap = (byte *)gs_malloc(BIGSIZE,swatchCount / BIGSIZE + 1,
	"imagen_print_page(swatchMap)" );
 
  if ( in == 0 || out == 0 ) 
    return -1; 
 
  /* Initialize the page */ 
  iWrite(prn_stream,iPAGE); 
 
  /* Tell ImPress what resolution we will be using */
  iWrite(prn_stream,iSET_MAGNIFICATION); 
      iWrite(prn_stream,Magnify); 
 
  /*------------------------------------------------------*/
  /* main loop down page */ 
  lnum = 0;
  while (lnum <= pdev->height) { 

    /* erase swatch map.  */ 
    for (swatch = swatchMap; swatch < swatchMap + swatchCount ; 
		swatch += BIGSIZE ) { 
      * (BIGTYPE *)swatch = (BIGTYPE) 0; 
    } /* for  */ 
 
    /* get scan lines to fill swatches */ 
    swatchLine = 0; 
    lastLine = VertBytesPerSw - 1; 

    /* Check if we don't have a full-height row of swatches at end of page */
    if (lnum + lastLine > pdev->height ) {
      /* back up last row so it overlaps with previous.  Not a problem
	on a laser printer, because the overlapping part will be identical */
      lnum = pdev->height - lastLine ; 
    }; /* not full height */
 
    DebugMsg (3,"lnum = %d \n",lnum);

    /* ------------------------------------------------------- */
    /* get 32 lines and interleave into a row of swatches */ 
    for (swatchLine = 0 ; swatchLine <= lastLine; swatchLine++) { 
      /* blank out end of buffer for BIGSIZE overlap */
      for (temp = in + line_size; temp < in + line_size + BIGSIZE;temp++){
	*temp = 0;
      } /* for temp */

      /* get one line */ 
      gdev_prn_copy_scan_lines(pdev, lnum + swatchLine, in, line_size); 
      DebugMsg(5,"Got scan line %d ", lnum + swatchLine);
      DebugMsg(5,"line %d \n", swatchLine); 
 
      /* interleave scan line into swatch buffer */ 
      /* a swatch is a 4 byte * 32 byte square.  Swatches are placed 
	 next to each other.  The first scan line maps into the first 
	 four bytes of the first swatch, then the first four of the second 
	 swatch, etc. 
	 To get this on the page: 
	   A1  A1  A1  A1  B1  B1  B1  B1  C1  C1  C1  C1 
	   A2  A2  A2  A2  B2  B2  B2  B2  C2  C2  C2  C2 
	   ... 
	   A32 A32 A32 A32 B32 B32 B32 B32 C32 C32 C32 C32 
	 You have to send it as: 
	   A1 A1 A1 A1 A2 ... A32 B1 B1 .. B32 C1 C1 ... C32   */ 
 
      /* set initial offset into swatch buffer based on which 
	 line in the swatch we are processing */ 
      swatch = out + swatchLine * HorzBytesPerSw; 
      DebugMsg(5,"offset: swatch = %d \n",(int) (swatch - out) );
      temp = in; 
      while ( temp < in + line_size ) { 
	/* copy multi-byte to swatch buffer */ 
	* (BIGTYPE *)swatch = * (BIGTYPE *)temp; 
	if ( * (BIGTYPE *)temp ) {
	  /* mark map if not blank */
	  swatchMap[(swatch - out)/TotalBytesPerSw] = (byte) 1 ;
	} /* if not zero */

	temp   += (BIGSIZE > HorzBytesPerSw) ? HorzBytesPerSw : BIGSIZE ; 
	swatch += (BIGSIZE > HorzBytesPerSw) ? HorzBytesPerSw : BIGSIZE ; 

	/* if we copied four bytes, skip to next swatch */ 
	if ( ((temp - in) % HorzBytesPerSw ) == 0 ) { 
	  swatch += (TotalBytesPerSw - HorzBytesPerSw) ; 
	} /* if need to skip */
      } /* while < line_size */ 
 
    } /* for swatchLine */ 
 
    /* ------------------------------------------------- */
    /* we now have full swatches. */ 
    /* Send to printer */ 
 
    /* go through swatch map to find non-blank swatches.
       Skip over completely blank swatches */ 
    startSwatch = 0;
    while (startSwatch < swatchCount ) {
      if (swatchMap[startSwatch] == 0 ) {
	/* skip blank swatch */
	DebugMsg(6,"Skip blank %d \n",startSwatch);
	totalBlankSwatches++;
	startSwatch++;
      } else { /* if swatch == 0 */
	/* we hit a non-blank swatch. */
	totalGreySwatches++;

	/* See how many there are in a row */
	endSwatch = startSwatch;
	while ( (endSwatch < swatchCount) && swatchMap[endSwatch] ) {
	  endSwatch++;
	  totalGreySwatches++;
	} /* while */
	/* endSwatch is one past last non-blank swatch */
	DebugMsg(6,"Grey swatches %d ",startSwatch);
	DebugMsg(6,"until %d \n",endSwatch);

	/* vertical position: scan line, shifted for magnification */ 
	iWrite(prn_stream, iABS_V); 
	  iWrite2(prn_stream, lnum << Magnify); 
 
	/* horizontal position = swatch number * 32 bits/swatch */ 
	iWrite(prn_stream,iABS_H); 
	   iWrite2(prn_stream, startSwatch * HorzBitsPerSw << Magnify ); 
	iWrite(prn_stream,iBITMAP);  /* start bitmap */ 
	  iWrite(prn_stream,0x07);     /* bit OR with page */ 
	  iWrite(prn_stream,(endSwatch - startSwatch)); /* horizontal 
		number of swatches */ 
	  iWrite(prn_stream, 1) ; /* vertical number of swatches */ 
	/* write out swatch buffer */ 
	for (swatch = out + startSwatch * TotalBytesPerSw;
		swatch < out + endSwatch * TotalBytesPerSw; swatch++) { 
	  iWrite(prn_stream,*swatch); 
	} /* for swatch */ 

	/* swatches have been printed, see if there are still
	   more in this row */
	startSwatch = endSwatch;
      } /* if swatch == 0 */

    } /* while startSwatch */
   
    /* Whole row of swatches is done.  Go on to next row of swatches */
    lnum += lastLine + 1; 
 
  } /* while lnum */ 
 
  /* Eject the page */ 
  iWrite(prn_stream,iENDPAGE); 
 
  fflush(prn_stream); 
 
  gs_free((char *)swatchMap, BIGSIZE, swatchCount / BIGSIZE + 1,
	"imagen_print_page(swatchMap)" );
  gs_free((char *)out, TotalBytesPerSw, swatchCount+1, "imagen_print_page(out)"); 
  gs_free((char *)in, BIGSIZE, line_size / BIGSIZE + 1, "imagen_print_page(in)"); 
  /* ----------------------------------------- */

  DebugMsg(1,"Debug: Grey: %d \n",totalGreySwatches);
  DebugMsg(1,"Debug: Blank: %d \n",totalBlankSwatches );
  DebugMsg(1,"%s\n","End of imagen_print_page");

  /* ----------------------------------------- */
  return 0; 
 
} /* imagen_print_page */ 
