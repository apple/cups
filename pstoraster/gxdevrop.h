/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gxdevrop.h */
/* Extension of gxdevice.h for RasterOp */

/* Define an unaligned implementation of copy_rop. */
dev_proc_copy_rop(gx_copy_rop_unaligned);
dev_proc_strip_copy_rop(gx_strip_copy_rop_unaligned);

/* Define the default and forwarding implementations of [strip_]copy_rop. */
/* (Normally these are never referenced directly.) */
dev_proc_copy_rop(gx_real_default_copy_rop);
dev_proc_copy_rop(gx_forward_copy_rop);
dev_proc_strip_copy_rop(gx_real_default_strip_copy_rop);
dev_proc_strip_copy_rop(gx_forward_strip_copy_rop);
