/* Copyright (C) 1989, 1995 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gdevpcfb.h,v 1.3 2000/03/13 19:00:47 mike Exp $ */
/* IBM PC frame buffer definitions */

#ifndef gdevpcfb_INCLUDED
#  define gdevpcfb_INCLUDED

#ifdef __MSDOS__
#  include "dos_.h"
typedef union REGS registers;

#endif

/* For testing, the 16-color display may be defined as a monochrome, */
/* 8-color, or 16-color device. */
#define ega_bits_of_color 2	/* 0, 1, or 2 */
#define rgb_max ega_bits_of_color

/* Define the short (integer) version of "transparent" color. */
/* ****** Depends on gx_no_color_index being all 1's. ***** */
#define no_color ((int)gx_no_color_index)

/* Procedures */

	/* See gxdevice.h for the definitions of the procedures. */

dev_proc_open_device(ega_open);
dev_proc_close_device(ega_close);
dev_proc_fill_rectangle(ega_fill_rectangle);
dev_proc_tile_rectangle(ega_tile_rectangle);
dev_proc_copy_mono(ega_copy_mono);
dev_proc_copy_color(ega_copy_color);
dev_proc_get_bits(ega_get_bits);

/* Structure for saving state of BIOS variables. */
typedef struct pcfb_bios_state_s {
    int display_mode;		/* must be first, see pcfb_save_state */
    /* in gdevpcfb.c */
    byte text_page;
    uint text_cursor_mode;
    uint text_font;
    byte text_attribute;
    byte border_color;
} pcfb_bios_state;

/* Procedures used by gdevpcfb.c */
void pcfb_set_signals(P1(gx_device *));
void pcfb_get_state(P1(pcfb_bios_state *));
void pcfb_set_mode(P1(int));
void pcfb_set_state(P1(const pcfb_bios_state *));

/* Types for frame buffer pointers. */
typedef byte *fb_ptr;
typedef volatile byte *volatile_fb_ptr;

/* Define the nominal page height in inches. */
#ifdef A4
#  define PAGE_HEIGHT_INCHES 11.69
#else
#  define PAGE_HEIGHT_INCHES 11.0
#endif

/* The device descriptor */
typedef struct gx_device_ega_s gx_device_ega;
struct gx_device_ega_s {
    gx_device_common;
    int raster;			/* frame buffer bytes per line */
    int fb_seg_mult;		/* multiplier for segment part */
    /* of frame buffer pointer */
    int fb_byte_mult;		/* multiplier for word part ditto */
#define mk_fb_ptr(x, y)\
  (fb_dev->fb_byte_mult == 0 ?\
   (fb_ptr)MK_PTR(regen + (y) * (fb_dev->fb_seg_mult), (x) >> 3) :\
   (fb_ptr)MK_PTR(regen + ((y) >> 4) * (fb_dev->fb_seg_mult),\
		 (((y) & 15) * fb_dev->fb_byte_mult) + ((x) >> 3)))
    int video_mode;
};

/* Macro for creating instances */
/* The initial parameters map an appropriate fraction of */
/* the screen to a full-page coordinate space. */
/* This may or may not be what is desired! */
#define ega_device(dev_name, procs, fb_raster, screen_height, aspect_ratio, video_mode)\
   {	std_device_dci_body(gx_device_ega, &procs, dev_name,\
	  fb_raster * 8, screen_height,\
	  (screen_height * (aspect_ratio)) / PAGE_HEIGHT_INCHES,	/* x dpi */\
	  screen_height / PAGE_HEIGHT_INCHES,		/* y dpi */\
	  (rgb_max ? 3 : 1),	/* num_components */\
	  4,			/* depth */\
	  (rgb_max ? rgb_max : 1),	/* max_gray */\
	  rgb_max,\
	  (rgb_max ? rgb_max + 1 : 2),	/* dither_grays */\
	  (rgb_max ? rgb_max + 1 : 0)	/* dither_colors */\
	),\
	 { 0 },			/* std_procs */\
	fb_raster,\
	(fb_raster & 15 ? fb_raster : fb_raster >> 4),\
	(fb_raster & 15 ? fb_raster : 0),\
	video_mode\
   }

/* Define the device port and register numbers, and the regen map base */
#define seq_addr 0x3c4
#define s_map 2
#define set_s_map(mask) outport2(seq_addr, s_map, mask)
#define graph_addr 0x3ce
#define g_const 0		/* set/reset */
#define set_g_const(color) outport2(graph_addr, g_const, color)
#define g_const_map 1		/* enable set/reset */
#define set_g_const_map(map) outport2(graph_addr, g_const_map, map)
#define g_function 3
#  define gf_WRITE 0
#  define gf_AND 8
#  define gf_OR 0x10
#  define gf_XOR 0x18
#define set_g_function(func) outport2(graph_addr, g_function, func)
#define g_read_plane 4
#define set_g_read_plane(plane) outport2(graph_addr, g_read_plane, plane)
#define g_mode 5
#  define gm_DATA 0
#  define gm_FILL 2
#define set_g_mode(mode) outport2(graph_addr, g_mode, mode)
#define g_mask 8
#define set_g_mask(mask) outport2(graph_addr, g_mask, mask)
#define select_g_mask() outportb(graph_addr, g_mask)
#define out_g_mask(mask) outportb(graph_addr+1, mask)
#define regen 0xa000

/* Define access to the frame buffer and the video registers */
/* according to whether we are on a DOS system or a Unix system. */

#if defined(M_UNIX) || defined(M_XENIX) || defined(UNIX) || defined(SYSV) || defined(__linux__)

		/* SCO Unix/Xenix, AT&T SVR4, or Linux. */

#undef outportb

#if defined(__GNUC__)
	/* Inline assembly version for gcc */
	/* Under SCO, requires installing the gnu assembler as "as" */
static inline void 
outportb(int port, int data)
{
    __asm__ volatile ("outb %0,%1"::
		      "a"      ((unsigned char)data),
		      "d"      ((unsigned short)port));
}
static inline void 
outport2(int port, int index, int data)
{
    __asm__ volatile ("movb %0,%%ah; movb %1,%%al; outw %%ax,%2"::
		      "qmi"    ((unsigned char)data),
		      "qmi"    ((unsigned char)index),
		      "d"      ((unsigned short)port):
		      "eax");
}
#else
void outportb(P2(uint, byte));
void outport2(P3(uint, byte, byte));

#endif

/* Redefine mk_fb_ptr -- no segmented addressing. */

#undef mk_fb_ptr
extern fb_ptr fb_addr;

#define mk_fb_ptr(x, y)	(fb_addr + (y) * (fb_dev->raster) + ((x) >> 3))

#else

		/* MS-DOS */

/* outportb is defined in dos_.h */
#define outport2(port, index, data)\
  (outportb(port, index), outportb((port)+1, data))

#endif

/* Fetch and discard a byte.  Prevent the compiler from */
/* optimizing this away. */
static unsigned char byte_discard_;

#define byte_discard(expr) byte_discard_ = (expr)

#endif /* gdevpcfb_INCLUDED */
