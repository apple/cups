/*
 *	Copyright 1992 Washington State University. All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted.
 * This software is provided "as is" without express or implied warranty.
 */

/* gdev4693.c */
/* Driver for the Tektronix 4693d color plotter. */
#include "gdevprn.h"

/* Thanks to Karl Hakimian (hakimian@yoda.eecs.wsu.edu) */
/* for contributing this code to Aladdin Enterprises. */

#define X_DPI 100
#define Y_DPI 100
#define WIDTH_10THS 85
#define HEIGHT_10THS 110

private dev_proc_print_page(t4693d_print_page);
private dev_proc_map_rgb_color(gdev_t4693d_map_rgb_color);
private dev_proc_map_color_rgb(gdev_t4693d_map_color_rgb);

private gx_device_procs t4693d_procs =
	prn_color_procs(gdev_prn_open, gdev_prn_output_page, gdev_prn_close,
		gdev_t4693d_map_rgb_color, gdev_t4693d_map_color_rgb);

#define t4693d_prn_device(name,depth,max_rgb) {prn_device_body( \
	gx_device_printer,t4693d_procs,name, \
	WIDTH_10THS, HEIGHT_10THS, X_DPI, Y_DPI, 0.25, 0.25, 0.25, 0.25, \
	3,depth,max_rgb,max_rgb,max_rgb + 1,max_rgb + 1, \
	t4693d_print_page)}

gx_device_printer gs_t4693d2_device = t4693d_prn_device("t4693d2",8, 3);
gx_device_printer gs_t4693d4_device = t4693d_prn_device("t4693d4",16, 15);
gx_device_printer gs_t4693d8_device = t4693d_prn_device("t4693d8",24, 255);

private gx_color_index
gdev_t4693d_map_rgb_color(gx_device *dev,
	gx_color_value r, gx_color_value g, gx_color_value b)
{
	ushort bitspercolor = prn_dev->color_info.depth / 3;
	ulong max_value = (1 << bitspercolor) - 1;

	if (bitspercolor == 5) {
		bitspercolor--;
		max_value = (1 << bitspercolor) - 1;
	}

	return ((r*max_value/gx_max_color_value) << (bitspercolor*2)) +
		((g*max_value/gx_max_color_value) << bitspercolor) +
		(b*max_value/gx_max_color_value);
}

private int
gdev_t4693d_map_color_rgb(gx_device *dev, gx_color_index color, ushort prgb[3])
{
	gx_color_value gray = color*gx_max_color_value/dev->color_info.max_gray;
	prgb[0] = gray;
	prgb[1] = gray;
	prgb[2] = gray;
	return(0);
}

private int
t4693d_print_page(gx_device_printer *dev, FILE *ps_stream)
{
	char header[32];
	int depth = prn_dev->color_info.depth;
	int line_size = gdev_mem_bytes_per_scan_line(prn_dev);
	byte *data = (byte *)gs_malloc(line_size, 1, "t4693d_print_page");
	char *p;
	ushort data_size = line_size/prn_dev->width;
	int checksum;
	int lnum;
	int i;
#if !arch_is_big_endian
	byte swap;
#endif

	if (data == 0) return_error(gs_error_VMerror);
	/* build header. */
	p = header;
	*p++ = 0x14;	/* Print request */
	*p++ = 0xc0|20;	/* Length of header */
	*p++ = 0xc0 | ((prn_dev->width >> 6)&0x3f);
	*p++ = 0x80 | (prn_dev->width&0x3f);
	*p++ = 0xc0 | ((prn_dev->height >> 6)&0x3f);
	*p++ = 0x80 | (prn_dev->height&0x3f);
	*p++ = 0xc1;	/* Handshake */
	*p++ = 0xc0;	/* Get number of prints from printer. */
	*p++ = 0xc0;	/* Get pixel shape from printer. */
	*p++ = (depth == 8) ? 0xcb : (depth == 16) ? 0xcc : 0xcd;
	*p++ = 0xc1;	/* Pixel-data order 1. */
	*p++ = 0xc3;	/* Interpolate to maximum size. */
	*p++ = 0xc3;	/* Full color range 1. */
	*p++ = 0xc0;	/* Color conversion from printer. */
	*p++ = 0xc0;	/* Color manipulation from printer. */
	*p++ = 0xc0;	/* B/W inversion from printer. */
	*p++ = 0xc3;	/* Portrait mode centered. */
	*p++ = 0xc9;	/* Use printer default for media and printing. */
	*p++ = 0x95;
	*p++ = 0x81;

	for (checksum = 0, i = 0; &header[i] != p; i++)
		checksum += header[i];
		
	*p++ = ((checksum%128)&0x7f) | 0x80;
	*p = 0x02; /* end of line. */
	/* write header */
	if (fwrite(header,1,22,ps_stream) != 22) {
		fprintf(stderr,"Could not write header (t4693d).\n");
		gs_free(data, line_size, 1, "t4693d_print_page");
		return_error(gs_error_ioerror);
	}

	for (lnum = 0; lnum < prn_dev->height; lnum++) {
		gdev_prn_copy_scan_lines(prn_dev,lnum,data,line_size);

		for (i = 0; i < line_size; i += data_size) {

			switch (depth) {
			case 8:
				data[i] &= 0x3f;
				break;
			case 16:
#if arch_is_big_endian
				data[i] &= 0x0f;
#else
				swap = data[i];
				data[i] = data[i + 1]&0x0f;
				data[i + 1] = swap;
#endif
				break;
			case 24:
				break;
			default:
				fprintf(stderr,"Bad depth (%d) t4693d.\n",depth);
				gs_free(data, line_size, 1, "t4693d_print_page");
				return_error(gs_error_rangecheck);
			}

			if (fwrite(&data[i],1,data_size,ps_stream) != data_size) {
				fprintf(stderr,"Could not write pixel (t4693d).\n");
				gs_free(data, line_size, 1, "t4693d_print_page");
				return_error(gs_error_ioerror);
			}

		}

		if (fputc(0x02,ps_stream) != 0x02) {
			fprintf(stderr,"Could not write EOL (t4693d).\n");
			gs_free(data, line_size, 1, "t4693d_print_page");
			return_error(gs_error_ioerror);
		}

	}

	if (fputc(0x01,ps_stream) != 0x01) {
		fprintf(stderr,"Could not write EOT (t4693d).\n");
		gs_free(data, line_size, 1, "t4693d_print_page");
		return_error(gs_error_ioerror);
	}

	gs_free(data, line_size, 1, "t4693d_print_page");
	return(0);
}
