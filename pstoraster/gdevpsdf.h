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

/*$Id: gdevpsdf.h,v 1.1 2000/03/13 19:00:47 mike Exp $ */
/* Common output syntax and parameters for PostScript and PDF writers */

#ifndef gdevpsdf_INCLUDED
#  define gdevpsdf_INCLUDED

#include "gdevvec.h"
#include "gsparam.h"
#include "strimpl.h"
#include "scfx.h"

/* ---------------- Distiller parameters ---------------- */

/* Parameters for controlling distillation of images. */
typedef struct psdf_image_params_s {
    stream_state *ACSDict;	/* JPEG */
    bool AntiAlias;
    bool AutoFilter;
    int Depth;
    stream_state *Dict;		/* JPEG or CCITTFax */
    bool Downsample;
    enum psdf_downsample_type {
	ds_Average,
	ds_Subsample
    } DownsampleType;
    bool Encode;
    const char *Filter;
    int Resolution;
    const stream_template *filter_template;
} psdf_image_params;

#define psdf_image_param_defaults(af, res, f, ft)\
  NULL/*ACSDict*/, 0/*false*/, af, -1, NULL/*Dict*/, 0/*false*/,\
  ds_Subsample, 1/*true*/, f, res, ft

/* Declare templates for default image compression filters. */
extern const stream_template s_CFE_template;

/* Complete distiller parameters. */
typedef struct psdf_distiller_params_s {

    /* General parameters */

    bool ASCII85EncodePages;
    enum psdf_auto_rotate_pages {
	arp_None,
	arp_All,
	arp_PageByPage
    } AutoRotatePages;
    bool CompressPages;
    long ImageMemory;
    bool LZWEncodePages;
    bool PreserveHalftoneInfo;
    bool PreserveOPIComments;
    bool PreserveOverprintSettings;
    enum psdf_transfer_function_info {
	tfi_Preserve,
	tfi_Apply,
	tfi_Remove
    } TransferFunctionInfo;
    enum psdf_ucr_and_bg_info {
	ucrbg_Preserve,
	ucrbg_Remove
    } UCRandBGInfo;
    bool UseFlateCompression;
#define psdf_general_param_defaults(ascii)\
  ascii, arp_None, 1/*true*/, 250000, 0/*false*/,\
  0/*false*/, 0/*false*/, 0/*false*/, tfi_Apply, ucrbg_Remove, 1	/*true */

    /* Color sampled image parameters */

    psdf_image_params ColorImage;
    enum psdf_color_conversion_strategy {
	ccs_LeaveColorUnchanged,
	ccs_UseDeviceDependentColor,
	ccs_UseDeviceIndependentColor
    } ColorConversionStrategy;
    bool ConvertCMYKImagesToRGB;
    bool ConvertImagesToIndexed;
#define psdf_color_image_param_defaults\
  { psdf_image_param_defaults(1/*true*/, 72, 0, 0) },\
  ccs_LeaveColorUnchanged, 1/*true*/, 0		/*false */

    /* Grayscale sampled image parameters */

    psdf_image_params GrayImage;
#define psdf_gray_image_param_defaults\
  { psdf_image_param_defaults(1/*true*/, 72, 0, 0) }

    /* Monochrome sampled image parameters */

    psdf_image_params MonoImage;
#define psdf_mono_image_param_defaults\
  { psdf_image_param_defaults(0/*false*/, 300, "CCITTFaxEncode", &s_CFE_template) }

    /* Font embedding parameters */

    gs_param_string_array AlwaysEmbed;
    gs_param_string_array NeverEmbed;
    bool EmbedAllFonts;
    bool SubsetFonts;
    int MaxSubsetPct;
#define psdf_font_param_defaults\
  	    { 0, 0, 1/*true*/ }, { 0, 0, 1/*true*/ },\
	   1/*true*/, 1/*true*/, 20

} psdf_distiller_params;

/* Define PostScript/PDF versions, corresponding roughly to Adobe versions. */
typedef enum {
    psdf_version_level1 = 1000,	/* Red Book Level 1 */
    psdf_version_level1_color = 1100,	/* Level 1 + colorimage + CMYK color */
    psdf_version_level2 = 2000,	/* Red Book Level 2 */
    psdf_version_level2_plus = 2017,	/* Adobe release 2017 */
    psdf_version_ll3 = 3010	/* LanguageLevel 3, release 3010 */
} psdf_version;

/* Define the extended device structure. */
#define gx_device_psdf_common\
	gx_device_vector_common;\
	psdf_version version;\
	bool binary_ok;		/* derived from ASCII85EncodePages */\
	psdf_distiller_params params
typedef struct gx_device_psdf_s {
    gx_device_psdf_common;
} gx_device_psdf;

#define psdf_initial_values(version, ascii)\
	vector_initial_values,\
	version,\
	!(ascii),\
	 { psdf_general_param_defaults(ascii),\
	   psdf_color_image_param_defaults,\
	   psdf_gray_image_param_defaults,\
	   psdf_mono_image_param_defaults,\
	   psdf_font_param_defaults\
	 }

/* st_device_psdf is never instantiated per se, but we still need to */
/* extern its descriptor for the sake of subclasses. */
extern_st(st_device_psdf);
#define public_st_device_psdf()	/* in gdevpsdf.c */\
  gs_public_st_suffix_add0_final(st_device_psdf, gx_device_psdf,\
    "gx_device_psdf", device_psdf_enum_ptrs,\
    device_psdf_reloc_ptrs, gx_device_finalize, st_device_vector)
#define st_device_psdf_max_ptrs (st_device_vector_max_ptrs)

/* Get/put parameters. */
dev_proc_get_params(gdev_psdf_get_params);
dev_proc_put_params(gdev_psdf_put_params);

/* Put a Boolean or integer parameter. */
int psdf_put_bool_param(P4(gs_param_list * plist, gs_param_name param_name,
			   bool * pval, int ecode));
int psdf_put_int_param(P4(gs_param_list * plist, gs_param_name param_name,
			  int *pval, int ecode));

/* ---------------- Vector implementation procedures ---------------- */

	/* Imager state */
int psdf_setlinewidth(P2(gx_device_vector * vdev, floatp width));
int psdf_setlinecap(P2(gx_device_vector * vdev, gs_line_cap cap));
int psdf_setlinejoin(P2(gx_device_vector * vdev, gs_line_join join));
int psdf_setmiterlimit(P2(gx_device_vector * vdev, floatp limit));
int psdf_setdash(P4(gx_device_vector * vdev, const float *pattern,
		    uint count, floatp offset));
int psdf_setflat(P2(gx_device_vector * vdev, floatp flatness));
int psdf_setlogop(P3(gx_device_vector * vdev, gs_logical_operation_t lop,
		     gs_logical_operation_t diff));

	/* Other state */
int psdf_setfillcolor(P2(gx_device_vector * vdev, const gx_drawing_color * pdc));
int psdf_setstrokecolor(P2(gx_device_vector * vdev, const gx_drawing_color * pdc));

	/* Paths */
#define psdf_dopath gdev_vector_dopath
int psdf_dorect(P6(gx_device_vector * vdev, fixed x0, fixed y0, fixed x1,
		   fixed y1, gx_path_type_t type));
int psdf_beginpath(P2(gx_device_vector * vdev, gx_path_type_t type));
int psdf_moveto(P7(gx_device_vector * vdev, floatp x0, floatp y0,
		   floatp x, floatp y, bool first, gx_path_type_t type));
int psdf_lineto(P6(gx_device_vector * vdev, floatp x0, floatp y0,
		   floatp x, floatp y, gx_path_type_t type));
int psdf_curveto(P10(gx_device_vector * vdev, floatp x0, floatp y0,
		     floatp x1, floatp y1, floatp x2,
		     floatp y2, floatp x3, floatp y3, gx_path_type_t type));
int psdf_closepath(P6(gx_device_vector * vdev, floatp x0, floatp y0,
		      floatp x_start, floatp y_start, gx_path_type_t type));

/* ---------------- Binary (image) data procedures ---------------- */

/* Define the structure for writing binary data. */
typedef struct psdf_binary_writer_s {
    stream *strm;
    gx_device_psdf *dev;
} psdf_binary_writer;

/* Begin writing binary data. */
int psdf_begin_binary(P2(gx_device_psdf * pdev, psdf_binary_writer * pbw));

/* Add an encoding filter.  The client must have allocated the stream state, */
/* if any, using pdev->v_memory. */
int psdf_encode_binary(P3(psdf_binary_writer * pbw,
		      const stream_template * template, stream_state * ss));

/* Add a 2-D CCITTFax encoding filter. */
int psdf_CFE_binary(P4(psdf_binary_writer * pbw, int w, int h, bool invert));

/* Set up compression and downsampling filters for an image. */
/* Note that this may modify the image parameters. */
/* If pctm is NULL, downsampling is not used. */
/* pis only provides UCR and BG information for CMYK => RGB conversion. */
int psdf_setup_image_filters(P5(gx_device_psdf * pdev, psdf_binary_writer * pbw,
				gs_image_t * pim, const gs_matrix * pctm,
				const gs_imager_state * pis));

/* Finish writing binary data. */
int psdf_end_binary(P1(psdf_binary_writer * pbw));

/* ------ Symbolic data printing ------ */

/* Print a PostScript string in the most efficient form. */
#define print_binary_ok 1
#define print_ASCII85_ok 2
void psdf_write_string(P4(stream * s, const byte * str, uint size,
			  int print_ok));

/*
 * Create a stream that just keeps track of how much has been written
 * to it.  We use this for measuring data that will be stored rather
 * than written to an actual stream.  This too should probably migrate
 * to stream.c....
 */
int psdf_alloc_position_stream(P2(stream ** ps, gs_memory_t * mem));

/*
 * Create/release a parameter list for printing (non-default) filter
 * parameters.  This should probably migrate to a lower level....
 */
typedef struct param_printer_params_s {
    const char *prefix;		/* before entire object, if any params */
    const char *suffix;		/* after entire object, if any params */
    const char *item_prefix;	/* before each param */
    const char *item_suffix;	/* after each param */
} param_printer_params_t;

#define param_printer_params_default_values 0, 0, 0, "\n"
extern const param_printer_params_t param_printer_params_default;
int psdf_alloc_param_printer(P5(gs_param_list ** pplist,
			     const param_printer_params_t * ppp, stream * s,
				int print_ok, gs_memory_t * mem));
void psdf_free_param_printer(P1(gs_param_list * plist));

/* Write out a Type 1 font definition. */
#ifndef gs_font_type1_DEFINED
#  define gs_font_type1_DEFINED
typedef struct gs_font_type1_s gs_font_type1;

#endif
int psdf_embed_type1_font(P2(stream * s, gs_font_type1 * pfont));

/* ---------------- Other procedures ---------------- */

/* Set the fill or stroke color.  rgs is "rg" or "RG". */
int psdf_set_color(P3(gx_device_vector * vdev, const gx_drawing_color * pdc,
		      const char *rgs));

#endif /* gdevpsdf_INCLUDED */
