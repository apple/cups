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

/* Internal definitions for PDF-writing driver. */
#include "gsparam.h"
#include "gxdevice.h"
#include "gxline.h"
#include "scommon.h"

/* ---------------- Statically allocated sizes ---------------- */
/* These should all really be dynamic.... */

/* Define the maximum size of the output file name. */
#define fname_size 80

/* Define the maximum number of pages in a document. */
#define max_pages 100

/* Define the maximum number of contents fragments on a page. */
#define max_contents_ids 300

/* Define the maximum depth of an outline tree. */
/* Note that there is no limit on the breadth of the tree. */
#define max_outline_depth 8

/* Define the maximum size of a destination array string. */
#define max_dest_string 80

/* ================ Types and structures ================ */

/* ---------------- Resources ---------------- */

typedef enum {
  resourceFont,
  resourceEncoding,
  resourceFontDescriptor,
  resourceColorSpace,
  resourceXObject,
  num_resource_types
} pdf_resource_type;
#define pdf_resource_type_names\
  "Font", "Encoding", "FontDescriptor", "ColorSpace", "XObject"
#define pdf_resource_type_structs\
  &st_pdf_font, &st_pdf_resource, &st_pdf_resource, &st_pdf_resource,\
  &st_pdf_resource

#define pdf_resource_common(typ)\
	typ *next;			/* next resource of this type */\
	pdf_resource *prev;		/* previously allocated resource */\
	gs_id rid;			/* optional key */\
	long id
typedef struct pdf_resource_s pdf_resource;
struct pdf_resource_s {
	pdf_resource_common(pdf_resource);
};
#define private_st_pdf_resource()\
  gs_private_st_ptrs2(st_pdf_resource, pdf_resource, "pdf_resource",\
    pdf_resource_enum_ptrs, pdf_resource_reloc_ptrs, next, prev)

/* Font resources */
typedef struct pdf_font_s pdf_font;
struct pdf_font_s {
	pdf_resource_common(pdf_font);
	gs_const_string fname;
};
/****** Doesn't handle the string ******/
#define private_st_pdf_font()\
  gs_private_st_suffix_add0(st_pdf_font, pdf_font, "pdf_font",\
    pdf_font_enum_ptrs, pdf_font_reloc_ptrs, st_pdf_resource)

/* ---------------- Other auxiliary structures ---------------- */

/* Outline nodes and levels */
typedef struct pdf_outline_node_s {
  long id, parent_id, prev_id, first_id, last_id;
  int count;
  gs_string action_string;
} pdf_outline_node;
typedef struct pdf_outline_level_s {
  pdf_outline_node first;
  pdf_outline_node last;
  int left;
} pdf_outline_level;

/* Articles */
typedef struct pdf_bead_s {
  long id, article_id, prev_id, next_id;
  char dest[max_dest_string];
  gs_string rect;
} pdf_bead;
typedef struct pdf_article_s pdf_article;
struct pdf_article_s {
  pdf_article *next;
  gs_string title;
  long id;
  pdf_bead first;
  pdf_bead last;
};
/****** Doesn't handle the strings ******/
#define private_st_pdf_article()\
  gs_private_st_ptrs1(st_pdf_article, pdf_article, "pdf_article",\
    pdf_article_enum_ptrs, pdf_article_reloc_ptrs, next)

/* Named destinations */
typedef struct pdf_named_dest_s pdf_named_dest;
struct pdf_named_dest_s {
  pdf_named_dest *next;
  gs_string key;
  char dest[max_dest_string];
};
/****** Doesn't handle the string ******/
#define private_st_pdf_named_dest()\
  gs_private_st_ptrs1(st_pdf_named_dest, pdf_named_dest, "pdf_named_dest",\
    pdf_named_dest_enum_ptrs, pdf_named_dest_reloc_ptrs, next)

/* ---------------- Device parameters ---------------- */

/* Define the Distiller parameters. */
typedef struct pdf_image_params_s {
	bool Downsample;
	int Resolution;
	bool Encode;
	const char *Filter;
	const stream_template *filter_template;
	int Depth;
	bool AntiAlias;
} pdf_image_params;
typedef struct pdf_distiller_params_s {
		/* General parameters */
	bool DoThumbnails;
	bool LZWEncodePages;
	bool ASCII85EncodePages;
		/* Color sampled image parameters */
	pdf_image_params ColorImage;
	/*ColorImageDict*/
	bool ConvertCMYKImagesToRGB;
		/* Grayscale sampled image parameters */	  
	pdf_image_params GrayImage;
	/*GrayImageDict*/
		/* Monochrome sampled image parameters */
	pdf_image_params MonoImage;
	/*MonoImageDict*/
		/* Font embedding parameters */
	gs_param_string_array AlwaysEmbed;
	gs_param_string_array NeverEmbed;
	bool EmbedAllFonts;
	bool SubsetFonts;
	int MaxSubsetPct;
} pdf_distiller_params;

/* ---------------- The device structure ---------------- */

/* Text state */
typedef struct pdf_text_state_s {
	float character_spacing;
	pdf_font *font;
	float size;
	float word_spacing;
	float horizontal_scaling;
} pdf_text_state;
#define pdf_text_state_default\
  0, NULL, 0, 0, 100

/* Define the device structure. */
typedef enum {
  ImageB = 1,
  ImageC = 2,
  ImageI = 4,
  Text = 8
} pdf_procset;
typedef struct gx_device_pdf_s {
	gx_device_common;
	pdf_distiller_params params;
	bool binary_ok;			/* if true, OK to output binary info */
		/* End of settable parameters. */
		/* Following are set when device is opened. */
	gs_memory_t *pdf_memory;
	char fname[fname_size + 1];
	char tfname[fname_size + 1];
	FILE *file;
	FILE *tfile;
		/* ................ */
	gs_point scale;
	long next_id;
	long root_id;
	long info_id;
	long pages_id;
	long outlines_id;
	int next_page;
	long contents_ids[max_contents_ids];
	int next_contents_id;
	int in_contents;		/* 0 = not in contents, */
					/* 1 = in stream, 2 = in text */
	long contents_length_id;
	long contents_pos;
	gx_color_index fill_color, stroke_color;
	gs_id clip_path_id;
	pdf_procset procsets;		/* used on this page */
	float flatness;
	/* The line width, dash offset, and dash pattern */
	/* are in default user space units. */
	gx_line_params line_params;	/* current values */
#define max_dash 11
	float dash_pattern[max_dash];
	long page_ids[max_pages];
	int pages_referenced;
	pdf_resource *resources[num_resource_types];
	pdf_resource *annots;		/* rid = page # */
	pdf_resource *last_resource;
	gs_string catalog_string;
	gs_string pages_string;
	gs_string page_string;
	pdf_outline_level outline_levels[max_outline_depth];
	int outline_depth;
	int closed_outline_depth;
	int outlines_open;
	pdf_article *articles;
	pdf_named_dest *named_dests;
	pdf_text_state text_state;
} gx_device_pdf;
#define in_document(pdev)\
  ((pdev)->next_contents_id != 0 || (pdev)->last_resource != 0)
#define in_page(pdev)\
  ((pdev)->next_contents_id != 0)

/* ================ Utility procedures ================ */

/* ---------------- Exported by gdevpdf.c ---------------- */

/* ------ Document ------ */

/* Open the document if necessary. */
void pdf_open_document(P1(gx_device_pdf *pdev));

/* ------ Objects ------ */

/* Allocate an ID for a future object. */
long pdf_obj_ref(P1(gx_device_pdf *pdev));

/* Begin an object, optionally allocating an ID. */
long pdf_open_obj(P2(gx_device_pdf *pdev, long id));

/* Begin an object, allocating an ID. */
#define pdf_begin_obj(pdev) pdf_open_obj(pdev, 0)

/* End an object. */
int pdf_end_obj(P1(gx_device_pdf *pdev));

/* ------ Graphics ------ */

/* Set the fill or stroke color. */
int pdf_set_color(P4(gx_device_pdf *pdev, gx_color_index color,
		     gx_color_index *pdcolor, const char *rgs));


/* Set the scale for coordinates according to the current resolution. */
void pdf_set_scale(P1(gx_device_pdf *pdev));

/* ------ Page contents ------ */

/* Begin a page contents part. */
/* Return an error if the page has too many contents parts. */
int pdf_begin_contents(P1(gx_device_pdf *pdev));

/* Close the current contents part if we are in one. */
int pdf_close_contents(P2(gx_device_pdf *pdev, bool last));

/* ------ Resources et al ------ */

/* Begin an aside (resource, annotation, ...). */
int pdf_begin_aside(P4(gx_device_pdf *pdev, pdf_resource **plist,
		       const gs_memory_struct_type_t *pst,
		       pdf_resource **ppres));

/* Begin a resource of a given type. */
int pdf_begin_resource(P3(gx_device_pdf *pdev, pdf_resource_type type,
			  pdf_resource **ppres));

/* Find a resource of a given type by gs_id. */
pdf_resource *pdf_find_resource_by_gs_id(P3(gx_device_pdf *pdev,
					    pdf_resource_type type,
					    gs_id rid));

/* End an aside. */
int pdf_end_aside(P1(gx_device_pdf *pdev));

/* End a resource. */
int pdf_end_resource(P1(gx_device_pdf *pdev));

/* ------ Pages ------ */

/* Reset the state of the current page. */
void pdf_reset_page(P1(gx_device_pdf *pdev));

/* Get or assign the ID for a page. */
/* Returns 0 if the page number is out of range. */
long pdf_page_id(P2(gx_device_pdf *pdev, int page_num));

/* Open a page for writing. */
int pdf_open_page(P2(gx_device_pdf *pdev, bool contents));

/* Write saved page- or document-level information. */
int pdf_write_saved_string(P2(gx_device_pdf *pdev, gs_string *pstr));

/* Write the default entries of the Info dictionary. */
int pdf_write_default_info(P1(gx_device_pdf *pdev));

/* ------ Path drawing ------ */

int pdf_put_clip_path(P2(gx_device_pdf *pdev, const gx_clip_path *pcpath));

/* ------ Output ------ */

/* Print (a) floating point number(s).  This is needed because %f format */
/* always prints a fixed number of digits after the decimal point, */
/* and %g format may use %e format, which PDF disallows. */
/* These functions return a pointer to the next %-element of the */
/* format, or to the terminating 0. */
const char *gprintf1(P3(FILE *file, const char *format, floatp v));
const char *gprintf2(P4(FILE *file, const char *format, floatp v1, floatp v2));
#define gprintf3(file, format, v1, v2, v3)\
  gprintf2(file, gprintf1(file, format, v1), v2, v3)
const char *gprintf4(P6(FILE *file, const char *format, floatp v1, floatp v2,
			floatp v3, floatp v4));
#define gprintf6(file, format, v1, v2, v3, v4, v5, v6)\
  gprintf2(file, gprintf4(file, format, v1, v2, v3, v4), v5, v6)

/* ---------------- Exported by gdevpdfm.c ---------------- */

/* Process a pdfmark (called from pdf_put_params). */
int pdfmark_process(P2(gx_device_pdf *pdev, const gs_param_string_array *pma));

/* Close the current level of the outline tree. */
int pdfmark_close_outline(P1(gx_device_pdf *pdev));

/* Write an article bead. */
int pdfmark_write_article(P2(gx_device_pdf *pdev, const pdf_bead *pbead));

/* ---------------- Exported by gdevpdfp.c ---------------- */

/* Compare a C string and a gs_param_string. */
bool pdf_key_eq(P2(const gs_param_string *pcs, const char *str));

/* ---------------- Exported by gdevpdft.c ---------------- */

/* Process a show operation (called from pdf_put_params). */
int pdfshow_process(P2(gx_device_pdf *pdev, const gs_param_dict *ptd));
