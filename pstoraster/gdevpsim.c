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

/* gdevpsim.c */
/* PostScript image output device */
#include "gdevprn.h"

/*
 * This driver does what the ps2image utility used to do:
 * It produces a bitmap in the form of a PostScript file that can be
 * fed to any PostScript printer.  It uses a run-length compression
 * method that executes quickly (unlike some produced by PostScript
 * drivers!).
 */

/* Define the device parameters. */
#ifndef X_DPI
#  define X_DPI 300
#endif
#ifndef Y_DPI
#  define Y_DPI 300
#endif

/* The device descriptor */
private dev_proc_print_page(psmono_print_page);

gx_device_printer far_data gs_psmono_device =
  prn_device(prn_std_procs, "psmono",
	DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	X_DPI, Y_DPI,
	0, 0, 0, 0,		/* margins */
	1, psmono_print_page);

/*
 * The following setup code gets written to the PostScript file.
 * We would have to break it up any because the Watcom compiler has
 * a limit of 512 characters in a single token, so we make a virtue out of
 * necessity and make each line a separate string.
 */
private const char far_data *psmono_setup[] = {
"%!PS",
"  /maxrep 31 def		% max repeat count",
"		% Initialize the strings for filling runs (lazily).",
"     /.ImageFill",
"      { maxrep string dup 0 1 maxrep 1 sub { 3 index put dup } for",
"	.ImageFills 4 2 roll put",
"      } bind def",
"     /.ImageFills [",
"     0 1 255 { /.ImageFill cvx 2 array astore cvx } for",
"     ] def",
"		% Initialize the procedure table for input dispatching.",
"     /.ImageProcs [",
"		% Stack: <buffer> <file> <xdigits> <previous> <byte>",
"     32 { { pop .ImageItem } } repeat",
"     16 { {	% 0x20-0x2f: (N-0x20) data bytes follow",
"      32 sub 3 -1 roll add 3 index exch 0 exch getinterval 2 index exch",
"      readhexstring pop exch pop 0 exch dup",
"     } bind } repeat",
"     16 { {	% 0x30-0x3f: prefix hex digit (N-0x30) to next count",
"      48 sub 3 -1 roll add 4 bitshift exch .ImageItem",
"     } bind } repeat",
"     32 { {	% 0x40-0x5f: repeat last data byte (N-0x40) times",
"      64 sub .ImageFills 2 index dup length 1 sub get get exec",
"      exch 0 exch getinterval",
"     } bind } repeat",
"     160 { { pop .ImageItem } } repeat",
"     ] readonly def",
"		% Read one item from a compressed image.",
"		% Stack contents: <buffer> <file> <xdigits> <previous>",
"  /.ImageItem",
"   { 2 index read pop dup .ImageProcs exch get exec",
"   } bind def",
"		% Read and print an entire compressed image.",
"  /.ImageRead		% <xres> <yres> <width> <height> .ImageRead -",
"   { gsave 1 [",
"     6 -2 roll exch 72 div 0 0 4 -1 roll -72 div 0 7 index",
"     ] { .ImageItem }",
"     4 index 7 add 8 idiv string currentfile 0 ()",
"     9 4 roll",
"     image pop pop pop pop",
"     grestore showpage",
"   } def"
};

#define data_run_code 0x20
#define xdigit_code 0x30
#define max_data_per_line 35
#define repeat_run_code 0x40
#define max_repeat_run 31

/* Send the page to the printer. */
private void write_data_run(P4(const byte *, int, FILE *, byte));
private int
psmono_print_page(gx_device_printer *pdev, FILE *prn_stream)
{	int line_size = gdev_mem_bytes_per_scan_line((gx_device *)pdev);
	int lnum;
	byte *line = (byte *)gs_malloc(line_size, 1, "psmono_print_page");

	if ( line == 0 )
	  return_error(gs_error_VMerror);

	/* If this is the first page of the file, */
	/* write the setup code. */
	if ( gdev_prn_file_is_new(pdev) )
	  {	int i;
		for ( i = 0; i < countof(psmono_setup); i++ )
		  fprintf(prn_stream, "%s\r\n", psmono_setup[i]);
	  }

	/* Write the .ImageRead command. */
	fprintf(prn_stream,
		"%g %g %d %d .ImageRead\r\n",
		pdev->HWResolution[0], pdev->HWResolution[1],
		pdev->width, pdev->height);

	/* Compress each scan line in turn. */
	for ( lnum = 0; lnum < pdev->height; lnum++ )
	  {	const byte *p;
		int left = line_size;
		byte *data;
		gdev_prn_get_bits(pdev, lnum, line, &data);
		p = data;
		/* Loop invariant: p + left = data + line_size. */
#define min_repeat_run 10
		while ( left >= min_repeat_run )
		  {	/* Detect a maximal run of non-repeated data. */
			const byte *p1 = p;
			int left1 = left;
			byte b;
			int count;
			while ( left1 >= min_repeat_run &&
			        ((b = *p1) != p1[1] ||
				 b != p1[2] || b != p1[3] || b != p1[4] ||
				 b != p1[5] || b != p1[6] || b != p1[7] ||
				 b != p1[8] || b != p1[9])
			      )
			  ++p1, --left1;
			if ( left1 < min_repeat_run )
			  break;		/* no repeated data left */
			write_data_run(p, (int)(p1 - p + 1), prn_stream, 0xff);
			/* Detect a maximal run of repeated data. */
			p = ++p1 + (min_repeat_run - 1);
			left = --left1 - (min_repeat_run - 1);
			while ( left > 0 && *p == b )
			  ++p, --left;
			for ( count = p - p1; count > max_repeat_run;
			      count -= max_repeat_run
			    )
			  {	putc(repeat_run_code + max_repeat_run,
				     prn_stream);
			  }
			putc(repeat_run_code + count, prn_stream);
		  }
		/* Write the remaining data, if any. */
		write_data_run(p, left, prn_stream, 0xff);
	  }

	/* Clean up and return. */
	fputs("\r\n", prn_stream);
	gs_free((char *)line, line_size, 1, "psmono_print_page");
	return 0;
}

/* Write a run of data on the file. */
private void
write_data_run(const byte *data, int count, FILE *f, byte invert)
{	register const byte *p = data;
	register const char _ds *hex_digits = "0123456789abcdef";
	int left = count;
	char line[sizeof(count) * 2 + max_data_per_line * 2 + 3];
	char *q = line;

	/* Write the count. */

	if ( !count )
	  return;
	{ int shift = sizeof(count) * 8;
	  while ( (shift -= 4) > 0 && (count >> shift) == 0 ) ;
	  for ( ; shift > 0; shift -= 4 )
	    *q++ = xdigit_code + ((count >> shift) & 0xf);
	  *q++ = data_run_code + (count & 0xf);
	}

	/* Write the data. */

	while ( left > 0 )
	  {	register int wcount = min(left, max_data_per_line);
		left -= wcount;
		for ( ; wcount > 0; ++p, --wcount )
		  {	byte b = *p ^ invert;
			*q++ = hex_digits[b >> 4];
			*q++ = hex_digits[b & 0xf];
		  }
		*q++ = '\r';
		*q++ = '\n';
		fwrite(line, 1, q - line, f);
		q = line;
	  }

}
