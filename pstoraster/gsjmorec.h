/* Copyright (C) 1994, 1995 Aladdin Enterprises.  All rights reserved.
  
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

/* gsjmorec.h */
/* "Wrapper" for Independent JPEG Group code jmorecfg.h */

#include "jmcorig.h"

/* Remove unwanted / unneeded features. */
#undef DCT_IFAST_SUPPORTED
#undef DCT_FLOAT_SUPPORTED
/*
 * Note: on machines with fast floating point, it might make more sense
 * to use the float DCT?
 */
#undef C_MULTISCAN_FILES_SUPPORTED
#undef C_PROGRESSIVE_SUPPORTED
#undef ENTROPY_OPT_SUPPORTED
#undef INPUT_SMOOTHING_SUPPORTED

/****** Comment out the next two lines to add progressive decoding. ******/
#undef D_MULTISCAN_FILES_SUPPORTED
#undef D_PROGRESSIVE_SUPPORTED

#undef BLOCK_SMOOTHING_SUPPORTED
#undef IDCT_SCALING_SUPPORTED
#undef UPSAMPLE_SCALING_SUPPORTED
#undef UPSAMPLE_MERGING_SUPPORTED
#undef QUANT_1PASS_SUPPORTED
#undef QUANT_2PASS_SUPPORTED
/*
 * Read "JPEG" files with up to 64 blocks/MCU for Adobe compatibility.
 * Note that this #define will have no effect in pre-v6 IJG versions.
 */
#define D_MAX_BLOCKS_IN_MCU   64
