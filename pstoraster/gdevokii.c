/* Copyright (C) 1989, 1992, 1995 Aladdin Enterprises.  All rights reserved.
  
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

/* gdevokii.c */
/*
 * Okidata IBM compatible dot-matrix printer driver for Ghostscript.
 *
 * This device is for the Okidata Microline IBM compatible 9 pin dot
 * matrix printers.  It is derived from the Epson 9 pin printer driver
 * using the standard 1/72" vertical pin spacing and the 60/120/240
 * dpi horizontal resolutions.  The vertical feed resolution however
 * is 1/144" and the Okidata implements the standard 1/216" requests
 * through "scaling":
 *
 *   (power on)
 *   "\033J\001" (vertical feed 1/216")  => Nothing happens
 *   "\033J\001" (vertical feed 1/216")  => Advance 1/144"
 *   "\033J\001" (vertical feed 1/216")  => Advance 1/144"
 *   "\033J\001" (vertical feed 1/216")  => Nothing happens
 *   (and so on)
 *
 * The simple minded accounting used here keep track of when the
 * page actually advances assumes the printer starts in a "power on"
 * state.
 * 
 * Supported resolutions are:
 *
 *    60x72      60x144
 *   120x72     120x144
 *   240x72     240x144
 *
 */
#include "gdevprn.h"

/*
 * Valid values for X_DPI:
 *
 *     60, 120, 240
 *
 * The value specified at compile time is the default value used if the
 * user does not specify a resolution at runtime.
 */

#ifndef X_DPI
#  define X_DPI 120
#endif

/*
 * Valid values for Y_DPI:
 *
 *     72, 144
 *
 * The value specified at compile time is the default value used if the
 * user does not specify a resolution at runtime.
 */

#ifndef Y_DPI
#  define Y_DPI 72
#endif

/* The device descriptor */
private dev_proc_print_page(okiibm_print_page);

/* Okidata IBM device */
gx_device_printer far_data gs_okiibm_device =
  prn_device(prn_std_procs, "okiibm",
	DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	X_DPI, Y_DPI,
	0.25, 0.0, 0.25, 0.0,			/* margins */
	1, okiibm_print_page);

/* ------ Internal routines ------ */

/* Forward references */
private void okiibm_output_run(P6(byte *, int, int, char, FILE *, int));

/* Send the page to the printer. */
private int
okiibm_print_page1(gx_device_printer *pdev, FILE *prn_stream, int y_9pin_high,
  const char *init_string, int init_length,
  const char *end_string, int end_length)
{	
	static const char graphics_modes_9[5] =
	{	
	-1, 0 /*60*/, 1 /*120*/, -1, 3 /*240*/
	};

	int in_y_mult = (y_9pin_high ? 2 : 1);
	int line_size = gdev_mem_bytes_per_scan_line((gx_device *)pdev);
	/* Note that in_size is a multiple of 8. */
	int in_size = line_size * (8 * in_y_mult);
	byte *buf1 = (byte *)gs_malloc(in_size, 1, "okiibm_print_page(buf1)");
	byte *buf2 = (byte *)gs_malloc(in_size, 1, "okiibm_print_page(buf2)");
	byte *in = buf1;
	byte *out = buf2;
	int out_y_mult = 1;
	int x_dpi = pdev->x_pixels_per_inch;
	char start_graphics = graphics_modes_9[x_dpi / 60];
	int first_pass = (start_graphics == 3 ? 1 : 0);
	int last_pass = first_pass * 2;
	int y_passes = (y_9pin_high ? 2 : 1);
	int skip = 0, lnum = 0, pass, ypass;
	int y_step = 0;

	/* Check allocations */
	if ( buf1 == 0 || buf2 == 0 )
	{	if ( buf1 ) 
		  gs_free((char *)buf1, in_size, 1, "okiibm_print_page(buf1)");
		if ( buf2 ) 
		  gs_free((char *)buf2, in_size, 1, "okiibm_print_page(buf2)");
		return_error(gs_error_VMerror);
	}

	/* Initialize the printer. */
	fwrite(init_string, 1, init_length, prn_stream);

	/* Print lines of graphics */
	while ( lnum < pdev->height )
	{	
		byte *in_data;
		byte *inp;
		byte *in_end;
		byte *out_end;
		int lcnt;

		/* Copy 1 scan line and test for all zero. */
		gdev_prn_get_bits(pdev, lnum, in, &in_data);
		if ( in_data[0] == 0 &&
		     !memcmp((char *)in_data, (char *)in_data + 1, line_size - 1)
		   )
	    	{	
			lnum++;
			skip += 2 / in_y_mult;
			continue;
		}

		/*
		 * Vertical tab to the appropriate position.
		 * The skip count is in 1/144" steps.  If total
		 * vertical request is not a multiple od 1/72"
		 * we need to make sure the page is actually
		 * going to advance.
		 */
		if ( skip & 1 )
		{
			int n = 1 + (y_step == 0 ? 1 : 0);
			fprintf(prn_stream, "\033J%c", n);
			y_step = (y_step + n) % 3;
			skip -= 1;
		}
		skip = skip / 2 * 3;
		while ( skip > 255 )
		{	
			fputs("\033J\377", prn_stream);
			skip -= 255;
		}
		if ( skip )
		{
			fprintf(prn_stream, "\033J%c", skip);
		}

		/* Copy the the scan lines. */
	    	lcnt = gdev_prn_copy_scan_lines(pdev, lnum, in, in_size);
		if ( lcnt < 8 * in_y_mult )
		{	/* Pad with lines of zeros. */
			memset(in + lcnt * line_size, 0,
			       in_size - lcnt * line_size);
		}

		if ( y_9pin_high )
		{	/* Shuffle the scan lines */
			byte *p;
			int i;
			static const char index[] =
			{  0, 2, 4, 6, 8, 10, 12, 14,
			   1, 3, 5, 7, 9, 11, 13, 15
			};
			for ( i = 0; i < 16; i++ )
			{
				memcpy( out + (i * line_size),
				        in + (index[i] * line_size),
				        line_size);
			}
			p = in;
			in = out;
			out = p;
		}

	for ( ypass = 0; ypass < y_passes; ypass++ )
	{
	    for ( pass = first_pass; pass <= last_pass; pass++ )
	    {
		/* We have to 'transpose' blocks of 8 pixels x 8 lines, */
		/* because that's how the printer wants the data. */

	        if ( pass == first_pass )
	        {
		    out_end = out;
		    inp = in;
		    in_end = inp + line_size;
    
    	            for ( ; inp < in_end; inp++, out_end += 8 )
    	            { 
    		        gdev_prn_transpose_8x8(inp + (ypass * 8 * line_size), 
					       line_size, out_end, 1);
		    }
		    /* Remove trailing 0s. */
		    while ( out_end > out && out_end[-1] == 0 )
	            {
		       	out_end--;
		    }
		}

		/* Transfer whatever is left and print. */
		if ( out_end > out )
	        {
		    okiibm_output_run(out, (int)(out_end - out),
			           out_y_mult, start_graphics,
				   prn_stream, pass);
	        }
	    	fputc('\r', prn_stream);
	    }
	    if ( ypass < y_passes - 1 )
	    {
		int n = 1 + (y_step == 0 ? 1 : 0);
		fprintf(prn_stream, "\033J%c", n);
		y_step = (y_step + n) % 3;
	    }
	}
	skip = 16 - y_passes + 1;		/* no skip on last Y pass */
	lnum += 8 * in_y_mult;
	}

	/* Reinitialize the printer. */
	fwrite(end_string, 1, end_length, prn_stream);
	fflush(prn_stream);

	gs_free((char *)buf2, in_size, 1, "okiibm_print_page(buf2)");
	gs_free((char *)buf1, in_size, 1, "okiibm_print_page(buf1)");
	return 0;
}

/* Output a single graphics command. */
/* pass=0 for all columns, 1 for even columns, 2 for odd columns. */
private void
okiibm_output_run(byte *data, int count, int y_mult,
  char start_graphics, FILE *prn_stream, int pass)
{	
	int xcount = count / y_mult;

	fputc(033, prn_stream);
	fputc("KLYZ"[start_graphics], prn_stream);
	fputc(xcount & 0xff, prn_stream);
	fputc(xcount >> 8, prn_stream);
	if ( !pass )
	{
		fwrite(data, 1, count, prn_stream);
	}
	else
	{	
		/* Only write every other column of y_mult bytes. */
		int which = pass;
		register byte *dp = data;
		register int i, j;

		for ( i = 0; i < xcount; i++, which++ )
		{
			for ( j = 0; j < y_mult; j++, dp++ )
			{
				putc(((which & 1) ? *dp : 0), prn_stream);
			}
		}
	}
}

/* The print_page procedures are here, to avoid a forward reference. */

private const char okiibm_init_string[]	= { 0x18 };
private const char okiibm_end_string[]	= { 0x0c };
private const char okiibm_one_direct[]	= { 0x1b, 0x55, 0x01 };
private const char okiibm_two_direct[]	= { 0x1b, 0x55, 0x00 };

private int
okiibm_print_page(gx_device_printer *pdev, FILE *prn_stream)
{
	char init_string[16], end_string[16];
	int init_length, end_length;

	init_length = sizeof(okiibm_init_string);
	memcpy(init_string, okiibm_init_string, init_length);

	end_length = sizeof(okiibm_end_string);
	memcpy(end_string, okiibm_end_string, end_length);

	if ( pdev->y_pixels_per_inch > 72 &&
	     pdev->x_pixels_per_inch > 60 )
	{
		/* Unidirectional printing for the higher resolutions. */
		memcpy( init_string + init_length, okiibm_one_direct,
		        sizeof(okiibm_one_direct) );
		init_length += sizeof(okiibm_one_direct);

		memcpy( end_string + end_length, okiibm_two_direct,
		        sizeof(okiibm_two_direct) );
		end_length += sizeof(okiibm_two_direct);
	}
	
	return okiibm_print_page1( pdev, prn_stream, 
				   pdev->y_pixels_per_inch > 72 ? 1 : 0,
				   init_string, init_length,
				   end_string, end_length );
}
