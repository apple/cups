/* Copyright (C) 1992 Aladdin Enterprises.  All rights reserved.
  
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

/* gp_dosfb.c */
/* MS-DOS frame buffer swapping routines for Ghostscript */
#include <conio.h>
#include "malloc_.h"
#include "memory_.h"
#include "gx.h"
#include "gp.h"
#include "gserrors.h"
#include "gxdevice.h"

/* On MS-DOS machines, we maintain a console image in memory, */
/* and swap screens on request. */
#define cw_width 80
#define cw_height 25
typedef struct text_line_s {
	int end;
	char text[cw_width + 1];
} text_line;
typedef struct {
	text_line *line;
	text_line lines[cw_height];
} ds_text_screen;

private ds_text_screen *console;

private int console_is_current;

/* Buffer one scan line of graphics. */
#define row_buf_size 1280
private char graphics_file_name[] = "_temp_.gfb";

/* Forward references */
private int save_graphics(P1(gx_device *));
private int restore_graphics(P1(gx_device *));

/* Initialize the console buffer. */
void
gp_init_console(void)
{	console = (ds_text_screen *)gs_malloc(1, sizeof(ds_text_screen), "gp_init_console(dosfb)");
	if ( console != 0 )
	{	memset(&console->lines, 0, sizeof(console->lines));
		console->line = &console->lines[0];
		console_is_current = 0;
	}
	else
		console_is_current = 1;
}

/* Write a string to the console. */
void
gp_console_puts(const char *str, uint size)
{	register ds_text_screen *cop = console;
	register text_line *lip;
	if ( console == 0 )
	{	fwrite(str, 1, size, stdout);
		return;
	}
	lip = cop->line;
	for ( ; size ; str++, size-- )
	  switch ( *str )
	   {
	case '\n':
		if ( lip == &cop->lines[cw_height - 1] )
		   {	/* Scroll up */
			memcpy(&cop->lines[0], &cop->lines[1],
			       sizeof(text_line) * (cw_height - 1));
		   }
		else
			cop->line = ++lip;
		lip->end = 0;
		break;
	case '\t':
		gp_console_puts("        ", 8 - (lip->end & 7));
		lip = cop->line;
		break;
	default:
		if ( lip->end == cw_width )
		   {	gp_console_puts("\n", 1);
			lip = cop->line;
		   }
		lip->text[lip->end++] = *str;
	   }
}

/* Make the console current on the screen. */
int
gp_make_console_current(gx_device *dev)
{	int code = 0;
	if ( console == 0 )
		return 0;
	if ( !console_is_current )
		code = save_graphics(dev);
	/* Transfer the console buffer to the screen. */
	/* Unfortunately, there is no standard way to clear the screen. */
	/* Output the ANSI sequence and hope for the best. */
	cputs("\r\033[2J\r    \r");
	   {	int i;
		register text_line *lip;
		for ( i = 0, lip = &console->lines[0]; i < cw_height; i++, lip++ )
		   {	if ( i != 0 ) cputs("\r\n");
			lip->text[lip->end] = 0;
			cputs(lip->text);
		   }
	   }
	console_is_current = 1;
	return code;
}

/* Make the graphics current on the screen. */
int
gp_make_graphics_current(gx_device *dev)
{	if ( console == 0 )
		return 0;
	if ( console_is_current )
	   {	int code = restore_graphics(dev);
		if ( code < 0 ) return code;
		console_is_current = 0;
	   }
	return 0;
}

/* ------ Internal routines ------ */

/* We compress the pixmap just a little, by noting */
/* replicated bytes at the beginning and end of a line. */
typedef struct { ushort pre, post; } row_head;

/* Save the graphics screen on a file. */
private int
save_graphics(gx_device *dev)
{	uint row_size = gx_device_raster(dev, 0);
	char row_buf[row_buf_size];
	FILE *gfile;
	int y;
	if ( row_size > row_buf_size ) return -1;
	gfile = fopen(graphics_file_name, "wb");
	if ( gfile == 0 ) return gs_error_ioerror;
	for ( y = 0; y < dev->height; y++ )
	   {	char _ss *row = row_buf;
		(*dev_proc(dev, get_bits))(dev, y, row, NULL);
		   {	row_head head;
			char _ss *beg = row, *end = row + row_size - 1;
			while ( end > beg && *end == end[-1] ) end--;
			if ( beg < end )
				while ( *beg == beg[1] ) beg++;
			head.pre = beg - row;
			head.post = end + 1 - row;
			fwrite((char *)&head, 1, sizeof(head), gfile);
			fwrite(beg, head.post - head.pre, 1, gfile);
			row += row_size;
		   }
	   }
	fclose(gfile);
	return 0;
}

/* Restore the graphics screen from a file. */
private int
restore_graphics(gx_device *dev)
{	FILE *gfile;
	uint row_size = gx_device_raster(dev, 0);
	char row_buf[row_buf_size];
	int y;
	if ( row_size > row_buf_size ) return -1;
	gfile = fopen(graphics_file_name, "rb");
	if ( gfile == 0 ) return gs_error_ioerror;
	for ( y = 0; y < dev->height; y ++ )
	   {	row_head head;
		char _ss *beg, *end;
		fread((char *)&head, 1, sizeof(head), gfile);
		beg = row_buf + head.pre;
		end = row_buf + head.post;
		fread(beg, 1, end - beg, gfile);
		if ( head.pre )
			memset(row_buf, *beg, head.pre);
		if ( head.post < row_size )
			memset(end, end[-1], row_size - head.post);
		(*dev_proc(dev, copy_color))(dev, row_buf, 0, row_size, gx_no_bitmap_id, 0, y, dev->width, 1);
	   }
	fclose(gfile);
	return 0;
}
