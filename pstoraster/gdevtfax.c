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

/* gdevtfax.c */
/* TIFF and fax devices */
#include "gdevprn.h"
#include "gdevtifs.h"			/* for TIFF output only */
#include "strimpl.h"
#include "scfx.h"

/* Define the device parameters. */
#define X_DPI 204
#define Y_DPI 196
#define LINE_SIZE ((X_DPI * 101 / 10 + 7) / 8)	/* max bytes per line */

/* The device descriptors */

dev_proc_open_device(gdev_fax_open);
private dev_proc_print_page(faxg3_print_page);
private dev_proc_print_page(faxg32d_print_page);
private dev_proc_print_page(faxg4_print_page);
private dev_proc_print_page(tiffcrle_print_page);
private dev_proc_print_page(tiffg3_print_page);
private dev_proc_print_page(tiffg32d_print_page);
private dev_proc_print_page(tiffg4_print_page);

struct gx_device_tfax_s {
	gx_device_common;
	gx_prn_device_common;
	gdev_tiff_state tiff;		/* for TIFF output only */
};
typedef struct gx_device_tfax_s gx_device_tfax;

#define tfdev ((gx_device_tfax *)dev)

/* Define procedures that adjust the paper size. */
private gx_device_procs gdev_fax_std_procs =
  prn_procs(gdev_fax_open, gdev_prn_output_page, gdev_prn_close);

gx_device_tfax far_data gs_faxg3_device =
{   prn_device_std_body(gx_device_tfax, gdev_fax_std_procs, "faxg3",
	DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	X_DPI, Y_DPI,
	0,0,0,0,			/* margins */
	1, faxg3_print_page)
};

gx_device_tfax far_data gs_faxg32d_device =
{   prn_device_std_body(gx_device_tfax, gdev_fax_std_procs, "faxg32d",
	DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	X_DPI, Y_DPI,
	0,0,0,0,			/* margins */
	1, faxg32d_print_page)
};

gx_device_tfax far_data gs_faxg4_device =
{   prn_device_std_body(gx_device_tfax, gdev_fax_std_procs, "faxg4",
	DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	X_DPI, Y_DPI,
	0,0,0,0,			/* margins */
	1, faxg4_print_page)
};

gx_device_tfax far_data gs_tiffcrle_device =
{   prn_device_std_body(gx_device_tfax, gdev_fax_std_procs, "tiffcrle",
	DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	X_DPI, Y_DPI,
	0,0,0,0,			/* margins */
	1, tiffcrle_print_page)
};

gx_device_tfax far_data gs_tiffg3_device =
{   prn_device_std_body(gx_device_tfax, gdev_fax_std_procs, "tiffg3",
	DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	X_DPI, Y_DPI,
	0,0,0,0,			/* margins */
	1, tiffg3_print_page)
};

gx_device_tfax far_data gs_tiffg32d_device =
{   prn_device_std_body(gx_device_tfax, gdev_fax_std_procs, "tiffg32d",
	DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	X_DPI, Y_DPI,
	0,0,0,0,			/* margins */
	1, tiffg32d_print_page)
};

gx_device_tfax far_data gs_tiffg4_device =
{   prn_device_std_body(gx_device_tfax, gdev_fax_std_procs, "tiffg4",
	DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	X_DPI, Y_DPI,
	0,0,0,0,			/* margins */
	1, tiffg4_print_page)
};

/* Open the device, adjusting the paper size. */
int
gdev_fax_open(gx_device *dev)
{	if ( dev->width >= 1680 && dev->width <= 1736 )
	  {	/* Adjust width for A4 paper. */
		dev->width = 1728;
	  }
	else if ( dev->width >= 2000 && dev->width <= 2056 )
	  {	/* Adjust width for B4 paper. */
		dev->width = 2048;
	  }
	return gdev_prn_open(dev);
}

/* Initialize the stream state with a set of default parameters. */
/* These select the same defaults as the CCITTFaxEncode filter, */
/* except we set BlackIs1 = true. */
void
gdev_fax_init_state(stream_CFE_state *ss, const gx_device_printer *pdev)
{	(*s_CFE_template.set_defaults)((stream_state *)ss);
	ss->Columns = pdev->width;
	ss->Rows = pdev->height;
	ss->BlackIs1 = true;
}

/* Send the page to the printer. */
int
gdev_stream_print_page(gx_device_printer *pdev, FILE *prn_stream,
  const stream_template _ds *temp, stream_state *ss)
{	gs_memory_t *mem = &gs_memory_default;
	int code;
	stream_cursor_read r;
	stream_cursor_write w;
	int in_size = gdev_mem_bytes_per_scan_line((gx_device *)pdev);
	int lnum;
	byte *in;
	byte *out;
	/* If the file is 'nul', don't even do the writes. */
	bool nul = !strcmp(pdev->fname, "nul");

	/* Initialize the common part of the encoder state. */
	ss->template = temp;
	ss->memory = mem;
	/* Now initialize the encoder. */
	code = (*temp->init)(ss);
	if ( code < 0 )
	  return code;

	/* Allocate the buffers. */
	in = gs_alloc_bytes(mem, temp->min_in_size + in_size + 1, "gdev_stream_print_page(in)");
#define out_size 1000
	out = gs_alloc_bytes(mem, out_size, "gdev_stream_print_page(out)");
	if ( in == 0 || out == 0 )
	{	code = gs_note_error(gs_error_VMerror);
		goto done;
	}

	/* Set up the processing loop. */
	lnum = 0;
	r.ptr = r.limit = in - 1;
	w.ptr = out - 1;
	w.limit = w.ptr + out_size;

	/* Process the image. */
	for ( ; ; )
	{	int status;
		if_debug7('w', "[w]lnum=%d r=0x%lx,0x%lx,0x%lx w=0x%lx,0x%lx,0x%lx\n", lnum,
			  (ulong)in, (ulong)r.ptr, (ulong)r.limit,
			  (ulong)out, (ulong)w.ptr, (ulong)w.limit);
		status = (*temp->process)(ss, &r, &w, lnum == pdev->height);
		if_debug7('w', "...%d, r=0x%lx,0x%lx,0x%lx w=0x%lx,0x%lx,0x%lx\n", status,
			  (ulong)in, (ulong)r.ptr, (ulong)r.limit,
			  (ulong)out, (ulong)w.ptr, (ulong)w.limit);
		switch ( status )
		{
		case 0:			/* need more input data */
		{	uint left;
			if ( lnum == pdev->height )
			  goto ok;
			left = r.limit - r.ptr;
			memcpy(in, r.ptr + 1, left);
			gdev_prn_copy_scan_lines(pdev, lnum++, in + left, in_size);
			r.limit = in + left + in_size - 1;
			r.ptr = in - 1;
		}	break;
		case 1:			/* need to write output */
			if ( !nul )
			  fwrite(out, 1, w.ptr + 1 - out, prn_stream);
			w.ptr = out - 1;
			break;
		}
	}

ok:
	/* Write out any remaining output. */
	if ( !nul )
	  fwrite(out, 1, w.ptr + 1 - out, prn_stream);

done:
	gs_free_object(mem, out, "gdev_stream_print_page(out)");
	gs_free_object(mem, in, "gdev_stream_print_page(in)");
	if ( temp->release != 0 )
	  (*temp->release)(ss);
	return code;
}
/* Print a fax page.  Other fax drivers use this. */
int
gdev_fax_print_page(gx_device_printer *pdev, FILE *prn_stream,
  stream_CFE_state *ss)
{	return gdev_stream_print_page(pdev, prn_stream, &s_CFE_template,
				      (stream_state *)ss);
}

/* Print a 1-D Group 3 page. */
private int
faxg3_print_page(gx_device_printer *pdev, FILE *prn_stream)
{	stream_CFE_state state;
	gdev_fax_init_state(&state, pdev);
	state.EndOfLine = true;
	state.EndOfBlock = false;
	return gdev_fax_print_page(pdev, prn_stream, &state);
}

/* Print a 2-D Group 3 page. */
private int
faxg32d_print_page(gx_device_printer *pdev, FILE *prn_stream)
{	stream_CFE_state state;
	gdev_fax_init_state(&state, pdev);
	state.K = (pdev->y_pixels_per_inch < 100 ? 2 : 4);
	state.EndOfLine = true;
	state.EndOfBlock = false;
	return gdev_fax_print_page(pdev, prn_stream, &state);
}

/* Print a Group 4 page. */
private int
faxg4_print_page(gx_device_printer *pdev, FILE *prn_stream)
{	stream_CFE_state state;
	gdev_fax_init_state(&state, pdev);
	state.K = -1;
	state.EndOfBlock = false;
	return gdev_fax_print_page(pdev, prn_stream, &state);
}

/* ---------------- TIFF output ---------------- */

#include "slzwx.h"
#include "srlx.h"

/* Device descriptors for TIFF formats other than fax. */
private dev_proc_print_page(tifflzw_print_page);
private dev_proc_print_page(tiffpack_print_page);

gx_device_tfax far_data gs_tifflzw_device =
{   prn_device_std_body(gx_device_tfax, prn_std_procs, "tifflzw",
	DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	X_DPI, Y_DPI,
	0,0,0,0,			/* margins */
	1, tifflzw_print_page)
};

gx_device_tfax far_data gs_tiffpack_device =
{   prn_device_std_body(gx_device_tfax, prn_std_procs, "tiffpack",
	DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	X_DPI, Y_DPI,
	0,0,0,0,			/* margins */
	1, tiffpack_print_page)
};

/* Define the TIFF directory we use, beyond the standard entries. */
/* NB: this array is sorted by tag number (assumed below) */
typedef struct tiff_mono_directory_s {
	TIFF_dir_entry	BitsPerSample;
	TIFF_dir_entry	Compression;
	TIFF_dir_entry	Photometric;
	TIFF_dir_entry	FillOrder;
	TIFF_dir_entry	SamplesPerPixel;
	TIFF_dir_entry	T4T6Options;
	TIFF_dir_entry	CleanFaxData;
} tiff_mono_directory;
private const tiff_mono_directory far_data dir_mono_template = {
	{ TIFFTAG_BitsPerSample,	TIFF_SHORT, 1, 1 },
	{ TIFFTAG_Compression,	TIFF_SHORT, 1, Compression_CCITT_T4 },
	{ TIFFTAG_Photometric,	TIFF_SHORT, 1, Photometric_min_is_white },
	{ TIFFTAG_FillOrder,	TIFF_SHORT, 1, FillOrder_LSB2MSB },
	{ TIFFTAG_SamplesPerPixel,	TIFF_SHORT, 1, 1 },
	{ TIFFTAG_T4Options,	TIFF_LONG,  1, 0 },
	{ TIFFTAG_CleanFaxData,	TIFF_SHORT, 1, CleanFaxData_clean },
};

/* Forward references */
private int tfax_begin_page(P3(gx_device_tfax *, FILE *, const tiff_mono_directory *));

/* Print a fax-encoded page. */
private int
tifff_print_page(gx_device_printer *dev, FILE *prn_stream,
  stream_CFE_state *pstate, tiff_mono_directory *pdir)
{	int code;

	tfax_begin_page(tfdev, prn_stream, pdir);
	pstate->FirstBitLowOrder = true;	/* decoders prefer this */
	code = gdev_fax_print_page(dev, prn_stream, pstate);
	gdev_tiff_end_page(&tfdev->tiff, prn_stream);
	return code;
}
private int
tiffcrle_print_page(gx_device_printer *dev, FILE *prn_stream)
{	stream_CFE_state state;
	tiff_mono_directory dir;

	gdev_fax_init_state(&state, dev);
	state.EndOfLine = false;
	state.EncodedByteAlign = true;
	dir = dir_mono_template;
	dir.Compression.value = Compression_CCITT_RLE;
	dir.T4T6Options.tag = TIFFTAG_T4Options;
	dir.T4T6Options.value = T4Options_fill_bits;
	return tifff_print_page(dev, prn_stream, &state, &dir);
}
private int
tiffg3_print_page(gx_device_printer *dev, FILE *prn_stream)
{	stream_CFE_state state;
	tiff_mono_directory dir;

	gdev_fax_init_state(&state, dev);
	state.EndOfLine = true;
	state.EncodedByteAlign = true;
	dir = dir_mono_template;
	dir.Compression.value = Compression_CCITT_T4;
	dir.T4T6Options.tag = TIFFTAG_T4Options;
	dir.T4T6Options.value = T4Options_fill_bits;
	return tifff_print_page(dev, prn_stream, &state, &dir);
}
private int
tiffg32d_print_page(gx_device_printer *dev, FILE *prn_stream)
{	stream_CFE_state state;
	tiff_mono_directory dir;

	gdev_fax_init_state(&state, dev);
	state.K = (dev->y_pixels_per_inch < 100 ? 2 : 4);
	state.EndOfLine = true;
	state.EncodedByteAlign = true;
	dir = dir_mono_template;
	dir.Compression.value = Compression_CCITT_T4;
	dir.T4T6Options.tag = TIFFTAG_T4Options;
	dir.T4T6Options.value = T4Options_2D_encoding | T4Options_fill_bits;
	return tifff_print_page(dev, prn_stream, &state, &dir);
}
private int
tiffg4_print_page(gx_device_printer *dev, FILE *prn_stream)
{	stream_CFE_state state;
	tiff_mono_directory dir;

	gdev_fax_init_state(&state, dev);
	state.K = -1;
	/*state.EncodedByteAlign = false;*/  /* no fill_bits option for T6 */
	dir = dir_mono_template;
	dir.Compression.value = Compression_CCITT_T6;
	dir.T4T6Options.tag = TIFFTAG_T6Options;
	return tifff_print_page(dev, prn_stream, &state, &dir);
}

/* Print an LZW page. */
private int
tifflzw_print_page(gx_device_printer *dev, FILE *prn_stream)
{	tiff_mono_directory dir;
	stream_LZW_state state;
	int code;

	dir = dir_mono_template;
	dir.Compression.value = Compression_LZW;
	dir.FillOrder.value = FillOrder_MSB2LSB;
	tfax_begin_page(tfdev, prn_stream, &dir);
	state.InitialCodeLength = 8;
	state.FirstBitLowOrder = false;
	state.BlockData = false;
	state.EarlyChange = 0;		/****** CHECK THIS ******/
	code = gdev_stream_print_page(dev, prn_stream, &s_LZWE_template,
				      (stream_state *)&state);
	gdev_tiff_end_page(&tfdev->tiff, prn_stream);
	return code;
}

/* Print a PackBits page. */
private int
tiffpack_print_page(gx_device_printer *dev, FILE *prn_stream)
{	tiff_mono_directory dir;
	stream_RLE_state state;
	int code;

	dir = dir_mono_template;
	dir.Compression.value = Compression_PackBits;
	dir.FillOrder.value = FillOrder_MSB2LSB;
	tfax_begin_page(tfdev, prn_stream, &dir);
	state.EndOfData = false;
	state.record_size = gdev_mem_bytes_per_scan_line((gx_device *)dev);
	code = gdev_stream_print_page(dev, prn_stream, &s_RLE_template,
				      (stream_state *)&state);
	gdev_tiff_end_page(&tfdev->tiff, prn_stream);
	return code;
}

#undef tfdev

/* Begin a TIFF fax page. */
private int
tfax_begin_page(gx_device_tfax *tfdev, FILE *fp,
  const tiff_mono_directory *pdir)
{	return gdev_tiff_begin_page((gx_device_printer *)tfdev,
				&tfdev->tiff, fp,
				(const TIFF_dir_entry *)pdir,
				sizeof(*pdir) / sizeof(TIFF_dir_entry),
				NULL, 0);
}
