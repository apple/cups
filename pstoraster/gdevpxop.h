/* Copyright (C) 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
  This file is part of GNU Ghostscript.
  
  GNU Ghostscript is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY.  No author or distributor accepts responsibility
  to anyone for the consequences of using it or for whether it serves any
  particular purpose or works at all, unless he says so in writing.  Refer
  to the GNU General Public License for full details.
  
  Everyone is granted permission to copy, modify and redistribute GNU
  Ghostscript, but only under the conditions described in the GNU General
  Public License.  A copy of this license is supposed to have been given
  to you along with GNU Ghostscript so you can know your rights and
  responsibilities.  It should be in a file named COPYING.  Among other
  things, the copyright notice and this notice must be preserved on all
  copies.
  
  Aladdin Enterprises supports the work of the GNU Project, but is not
  affiliated with the Free Software Foundation or the GNU Project.  GNU
  Ghostscript, as distributed by Aladdin Enterprises, does not require any
  GNU software to build or run it.
*/

/*$Id: gdevpxop.h,v 1.1 2000/03/13 19:00:47 mike Exp $ */
/* Operator and other tag definitions for PCL XL */

#ifndef gdevpxop_INCLUDED
#  define gdevpxop_INCLUDED

typedef enum {
/*0x */
    pxtNull = 0x00, pxt01, pxt02, pxt03,
    pxt04, pxt05, pxt06, pxt07,
    pxt08, pxtHT, pxtLF, pxtVT,
    pxtFF, pxtCR, pxt0e, pxt0f,
/*1x */
    pxt10, pxt11, pxt12, pxt13,
    pxt14, pxt15, pxt16, pxt17,
    pxt18, pxt19, pxt1a, pxt1b,
    pxt1c, pxt1d, pxt1e, pxt1f,
/*2x */
    pxtSpace, pxt21, pxt22, pxt23,
    pxt24, pxt25, pxt26, pxt_beginASCII,
    pxt_beginBinaryMSB, pxt_beginBinaryLSB, pxt2a, pxt2b,
    pxt2c, pxt2d, pxt2e, pxt2f,
/*3x */
    pxt30, pxt31, pxt32, pxt33,
    pxt34, pxt35, pxt36, pxt37,
    pxt38, pxt39, pxt3a, pxt3b,
    pxt3c, pxt3d, pxt3e, pxt3f,
/*4x */
    pxt40, pxtBeginSession, pxtEndSession, pxtBeginPage,
    pxtEndPage, pxt45, pxt46, pxtComment,
    pxtOpenDataSource, pxtCloseDataSource, pxt4a, pxt4b,
    pxt4c, pxt4d, pxt4e, pxtBeginFontHeader,
/*5x */
    pxtReadFontHeader, pxtEndFontHeader, pxtBeginChar, pxtReadChar,
    pxtEndChar, pxtRemoveFont, pxtSetCharAttributes /*2.0 */ , pxt57,
    pxt58, pxt59, pxt5a, pxtBeginStream,
    pxtReadStream, pxtEndStream, pxtExecStream, pxtRemoveStream /*2.0 */ ,
/*6x */
    pxtPopGS, pxtPushGS, pxtSetClipReplace, pxtSetBrushSource,
    pxtSetCharAngle, pxtSetCharScale, pxtSetCharShear, pxtSetClipIntersect,
    pxtSetClipRectangle, pxtSetClipToPage, pxtSetColorSpace, pxtSetCursor,
    pxtSetCursorRel, pxtSetHalftoneMethod, pxtSetFillMode, pxtSetFont,
/*7x */
    pxtSetLineDash, pxtSetLineCap, pxtSetLineJoin, pxtSetMiterLimit,
    pxtSetPageDefaultCTM, pxtSetPageOrigin, pxtSetPageRotation, pxtSetPageScale,
    pxtSetPaintTxMode, pxtSetPenSource, pxtSetPenWidth, pxtSetROP,
    pxtSetSourceTxMode, pxtSetCharBoldValue, pxt7e, pxtSetClipMode,
/*8x */
    pxtSetPathToClip, pxtSetCharSubMode, pxt82, pxt83,
    pxtCloseSubPath, pxtNewPath, pxtPaintPath, pxt87,
    pxt88, pxt89, pxt8a, pxt8b,
    pxt8c, pxt8d, pxt8e, pxt8f,
/*9x */
    pxt90, pxtArcPath, pxt92, pxtBezierPath,
    pxt94, pxtBezierRelPath, pxtChord, pxtChordPath,
    pxtEllipse, pxtEllipsePath, pxt9a, pxtLinePath,
    pxt9c, pxtLineRelPath, pxtPie, pxtPiePath,
/*ax */
    pxtRectangle, pxtRectanglePath, pxtRoundRectangle, pxtRoundRectanglePath,
    pxta4, pxta5, pxta6, pxta7,
    pxtText, pxtTextPath, pxtaa, pxtab,
    pxtac, pxtad, pxtae, pxtaf,
/*bx */
    pxtBeginImage, pxtReadImage, pxtEndImage, pxtBeginRastPattern,
    pxtReadRastPattern, pxtEndRastPattern, pxtBeginScan, pxtb7,
    pxtEndScan, pxtScanLineRel, pxtba, pxtbb,
    pxtbc, pxtbd, pxtbe, pxtbf,
/*cx */
    pxt_ubyte, pxt_uint16, pxt_uint32, pxt_sint16,
    pxt_sint32, pxt_real32, pxtc6, pxtc7,
    pxt_ubyte_array, pxt_uint16_array, pxt_uint32_array, pxt_sint16_array,
    pxt_sint32_array, pxt_real32_array, pxtce, pxtcf,
/*dx */
    pxt_ubyte_xy, pxt_uint16_xy, pxt_uint32_xy, pxt_sint16_xy,
    pxt_sint32_xy, pxt_real32_xy, pxtd6, pxtd7,
    pxtd8, pxtd9, pxtda, pxtdb,
    pxtdc, pxtdd, pxtde, pxtdf,
/*ex */
    pxt_ubyte_box, pxt_uint16_box, pxt_uint32_box, pxt_sint16_box,
    pxt_sint32_box, pxt_real32_box, pxte6, pxte7,
    pxte8, pxte9, pxtea, pxteb,
    pxtec, pxted, pxtee, pxtef,
/*fx */
    pxtf0, pxtf1, pxtf2, pxtf3,
    pxtf4, pxtf5, pxtf6, pxtf7,
    pxt_attr_ubyte, pxt_attr_uint16, pxt_dataLength, pxt_dataLengthByte,
    pxtfc, pxtfd, pxtfe, pxtff
} px_tag_t;

#endif /* gdevpxop_INCLUDED */
