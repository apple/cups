/* Copyright (C) 1993, 1994 Aladdin Enterprises.  All rights reserved.
  
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

/* gdevevga.c */
/* IBM PC EGA and VGA display drivers */
/* All of the real code is in gdevpcfb.c. */
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gxdevice.h"
#include "gdevpcfb.h"

/* ------ Internal routines ------ */

/* We can't catch signals.... */
void
pcfb_set_signals(gx_device *dev)
{
}

/* Read the device state */
void
pcfb_get_state(pcfb_bios_state *pbs)
{	registers regs;
	regs.h.ah = 0xf;
	int86(0x10, &regs, &regs);
	pbs->display_mode = regs.h.al;
	pbs->text_page = regs.h.bh;
	regs.h.ah = 0x3;
	int86(0x10, &regs, &regs);
	pbs->text_cursor_mode = regs.rshort.cx;
	regs.rshort.ax = 0x1130;
	regs.h.bh = 0;
	int86(0x10, &regs, &regs);
	switch ( regs.rshort.cx )
	{
	case 0x08: pbs->text_font = 0x1112; break;	/* 8 x 8 */
	case 0x10: pbs->text_font = 0x1114; break;	/* 8 x 16 */
	default:   pbs->text_font = 0x1111;		/* 8 x 14 */
	}
	regs.h.ah = 0x8;
	regs.h.bh = pbs->text_page;
	int86(0x10, &regs, &regs);
	pbs->text_attribute = regs.h.ah;
	pbs->border_color = (regs.h.ah >> 4);
	regs.rshort.ax = 0x1a00;
	int86(0x10, &regs, &regs);
	if ( regs.h.al == 0x1a && regs.h.bl == 0x8 )
	  {	regs.rshort.ax = 0x1008;
		int86(0x10, &regs, &regs);
		pbs->border_color = regs.h.bh;
	  }
	if ( pbs->display_mode != 3 )
	  {	pbs->display_mode = 3;
		pbs->text_font = 0x1112;
		pbs->text_cursor_mode = 0x0607;
		pbs->text_attribute = 7;
		pbs->text_page = 0;
	  }
}

/* Set the device mode */
void
pcfb_set_mode(int mode)
{	registers regs;
	regs.h.ah = 0;
	regs.h.al = mode;
	int86(0x10, &regs, &regs);
}

/* Restore the device state */
void
pcfb_set_state(const pcfb_bios_state *pbs)
{	registers regs;
	pcfb_set_mode(pbs->display_mode);
	regs.rshort.ax = 0x500;		/* force display of page 0 */
	int86(0x10, &regs, &regs);
	regs.rshort.ax = pbs->text_font;
	regs.h.bl = 0;
	int86(0x10, &regs, &regs);
	regs.h.ah = 0x3;
	regs.h.bh = 0;
	int86(0x10, &regs, &regs);	/* Get cursor to reset MCGA */
	regs.h.al = pbs->text_page;
	regs.h.ah = 0x5;
	int86(0x10, &regs, &regs);
	regs.rshort.cx = pbs->text_cursor_mode;
	regs.h.ah = 0x1;
	int86(0x10, &regs, &regs);
	regs.rshort.ax = 0x1001;
	regs.h.bh = pbs->border_color;
	int86(0x10, &regs, &regs);
}
