/* Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gdevpdfx.h,v 1.3 2000/03/13 19:00:47 mike Exp $ */
/* Internal definitions for PDF-writing driver. */

#ifndef gdevpdfx_INCLUDED
#  define gdevpdfx_INCLUDED

#include "gsparam.h"
#include "gxdevice.h"
#include "gxline.h"
#include "stream.h"
#include "gdevpstr.h"
#include "gdevpsdf.h"

/* ---------------- Statically allocated sizes ---------------- */
/* These should all really be dynamic.... */

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
    /* Standard PDF resources. */
    resourceFont,
    resourceEncoding,
    resourceFontDescriptor,
    resourceColorSpace,
    resourceImageXObject,
    /* Internally used resources. */
    resourceCharProc,
    resourceNamedObject,
    num_resource_types
} pdf_resource_type;

#define pdf_resource_type_names\
  "Font", "Encoding", "FontDescriptor", "ColorSpace", "XObject",\
   0, 0, 0
#define pdf_resource_type_structs\
  &st_pdf_font, &st_pdf_resource, &st_pdf_resource, &st_pdf_resource,\
  &st_pdf_resource, &st_pdf_char_proc, &st_pdf_named_object

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
typedef struct pdf_char_proc_s pdf_char_proc;	/* forward reference */
typedef struct pdf_font_s pdf_font;
typedef struct pdf_font_name_s {
    byte chars[40];		/* arbitrary, must be large enough for */
    /* the 14 built-in fonts */
    uint size;
} pdf_font_name;
struct pdf_font_s {
    pdf_resource_common(pdf_font);
    pdf_font_name fname;
    bool used_on_page;
    char frname[6 + 1];		/* xxxxxx\0 */
    /* Encoding differences for base fonts. */
    byte chars_used[32];	/* 1 bit per character code */
    gs_const_string *differences;
    long diff_id;
    /* Bookkeeping for embedded fonts. */
    int num_chars;
#define font_is_embedded(font) ((font)->num_chars != 0)
    pdf_char_proc *char_procs;
    int max_y_offset;
    /* Pseudo-characters for spacing. */
    /* The range should be determined by the device resolution.... */
#define x_space_min 24
#define x_space_max 150
    byte spaces[x_space_max - x_space_min + 1];
};

#define private_st_pdf_font()\
  gs_private_st_suffix_add2(st_pdf_font, pdf_font, "pdf_font",\
    pdf_font_enum_ptrs, pdf_font_reloc_ptrs, st_pdf_resource,\
    differences, char_procs)

/* CharProc pseudo-resources for embedded fonts */
struct pdf_char_proc_s {
    pdf_resource_common(pdf_char_proc);
    pdf_font *font;
    pdf_char_proc *char_next;	/* next char_proc for same font */
    int width, height;
    int x_width;		/* X escapement */
    int y_offset;		/* of character (0,0) */
    byte char_code;
};

#define private_st_pdf_char_proc()\
  gs_private_st_suffix_add2(st_pdf_char_proc, pdf_char_proc,\
    "pdf_char_proc", pdf_char_proc_enum_ptrs,\
    pdf_char_proc_reloc_ptrs, st_pdf_resource, font, char_next)

/* Named object pseudo-resources. */
/*
 * The elements of arrays are stored sorted in decreasing index order.
 * The elements of dictionaries are not sorted.
 * The elements of streams don't use the key, and are stored in
 * reverse order.
 */
typedef struct pdf_named_element_s pdf_named_element;
struct pdf_named_element_s {
    pdf_named_element *next;
    gs_string key;		/* if array, data = 0, size = index */
    gs_string value;
};
typedef enum {
    named_unknown,		/* forward reference */
    named_array, named_dict, named_stream,	/* OBJ or predefined */
    named_graphics,		/* BP/EP */
    named_other			/* ANN, DEST, LNK, PS */
} pdf_named_object_type;

#define private_st_pdf_named_element()	/* in gdevpdfo.c */\
  gs_private_st_composite(st_pdf_named_element, pdf_named_element,\
    "pdf_named_element", pdf_named_elt_enum_ptrs, pdf_named_elt_reloc_ptrs)
typedef struct pdf_named_object_s pdf_named_object;
struct pdf_named_object_s {
    pdf_resource_common(pdf_named_object);
    pdf_named_object_type type;
    gs_string key;
    pdf_named_element *elements;	/* (extra key/value pairs for graphics) */
    bool open;			/* stream, graphics */
    struct gr_ {		/* graphics only */
	pdf_named_object *enclosing;
    } graphics;
};

#define public_st_pdf_named_object()	/* in gdevpdfo.c */\
  gs_public_st_composite(st_pdf_named_object, pdf_named_object,\
    "pdf_named_object", pdf_named_obj_enum_ptrs, pdf_named_obj_reloc_ptrs)

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
    long id, article_id, prev_id, next_id, page_id;
    gs_rect rect;
} pdf_bead;
typedef struct pdf_article_s pdf_article;
struct pdf_article_s {
    pdf_article *next;
    gs_string title;
    gs_string info;
    long id;
    pdf_bead first;
    pdf_bead last;
};

#define private_st_pdf_article()\
  gs_private_st_ptrs1_strings2(st_pdf_article, pdf_article, "pdf_article",\
    pdf_article_enum_ptrs, pdf_article_reloc_ptrs, next, title, info)

/* Named destinations */
typedef struct pdf_named_dest_s pdf_named_dest;
struct pdf_named_dest_s {
    pdf_named_dest *next;
    gs_string key;
    char dest[max_dest_string];
};

#define private_st_pdf_named_dest()\
  gs_private_st_ptrs1_strings1(st_pdf_named_dest, pdf_named_dest,\
    "pdf_named_dest", pdf_named_dest_enum_ptrs, pdf_named_dest_reloc_ptrs,\
    next, key)

/* ---------------- The device structure ---------------- */

/* Text state */
typedef struct pdf_text_state_s {
    /* State parameters */
    float character_spacing;
    pdf_font *font;
    floatp size;
    float word_spacing;
    float horizontal_scaling;
    /* Bookkeeping */
    gs_matrix matrix;		/* relative to device space, not user space */
    gs_point line_start;
    gs_point current;
#define max_text_buffer 200	/* arbitrary, but overflow costs 5 chars */
    byte buffer[max_text_buffer];
    int buffer_count;
} pdf_text_state;

#define pdf_text_state_default\
  0, NULL, 0, 0, 100,\
  { identity_matrix_body }, { 0, 0 }, { 0, 0 }, { 0 }, 0

/* Resource lists */
#define num_resource_chains 16
typedef struct pdf_resource_list_s {
    pdf_resource *chains[num_resource_chains];
} pdf_resource_list;

/* Define the hash function for gs_ids. */
#define gs_id_hash(rid) ((rid) + ((rid) / num_resource_chains))

/* Define the bookkeeping for an open stream. */
typedef struct pdf_stream_position_s {
    long length_id;
    long start_pos;
} pdf_stream_position;

/* Define the device structure. */
typedef enum {
    NoMarks = 0,
    ImageB = 1,
    ImageC = 2,
    ImageI = 4,
    Text = 8
} pdf_procset;
typedef enum {
    pdf_in_none,
    pdf_in_stream,
    pdf_in_text,
    pdf_in_string
} pdf_context;
typedef struct gx_device_pdf_s {
    gx_device_psdf_common;
    /* PDF-specific distiller parameters */
    float CompatibilityLevel;
    /* End of distiller parameters */
    /* Other parameters */
    bool ReAssignCharacters;
    bool ReEncodeCharacters;
    long FirstObjectNumber;
    /* End of parameters */
    /* Following are set when device is opened. */
    enum {
	pdf_compress_none,
	pdf_compress_LZW,	/* not currently used, thanks to Unisys */
	pdf_compress_Flate
    } compression;
#define pdf_memory v_memory
    char tfname[gp_file_name_sizeof];
    FILE *tfile;
    char rfname[gp_file_name_sizeof];
    FILE *rfile;
    stream *rstrm;
    byte *rstrmbuf;
    stream *rsave_strm;
    pdf_font *open_font;
    long embedded_encoding_id;
    /* ................ */
    long next_id;
    /* The following 2 IDs, and only these, are allocated */
    /* when the file is opened. */
    long root_id;
    long info_id;
#define pdf_num_initial_ids 2
    long pages_id;
    long outlines_id;
    int next_page;
    long contents_id;
    pdf_context context;
    long contents_length_id;
    long contents_pos;
    pdf_procset procsets;	/* used on this page */
    float flatness;
/****** SHOULD USE state ******/
    /* The line width, dash offset, and dash pattern */
    /* are in default user space units. */
    gx_line_params line_params;	/* current values */
/****** SHOULD USE state ******/
    pdf_text_state text;
    long space_char_ids[x_space_max - x_space_min + 1];
#define initial_num_page_ids 50
    long *page_ids;
    int num_page_ids;
    int pages_referenced;
    pdf_resource_list resources[num_resource_types];
    pdf_resource *annots;	/* rid = page # */
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
    pdf_named_object *named_objects;
    pdf_named_object *open_graphics;
} gx_device_pdf;

#define is_in_page(pdev)\
  ((pdev)->contents_id != 0)
#define is_in_document(pdev)\
  (is_in_page(pdev) || (pdev)->last_resource != 0)

/* Enumerate the individual pointers in a gx_device_pdf */
#define gx_device_pdf_do_ptrs(m)\
 m(0,rstrm) m(1,rstrmbuf) m(2,rsave_strm) m(3,open_font)\
 m(4,line_params.dash.pattern) m(5,text.font) m(6,page_ids) m(7,annots)\
 m(8,last_resource) m(9,articles) m(10,named_dests)\
 m(11,named_objects) m(12,open_graphics)
#define gx_device_pdf_num_ptrs 13	/* + num_resource_types */
#define gx_device_pdf_do_strings(m)\
 m(0,catalog_string) m(1,pages_string) m(2,page_string)
#define gx_device_pdf_num_strings 3	/* + max_outline_depth * 2 */
#define st_device_pdf_max_ptrs\
  (st_device_psdf_max_ptrs + gx_device_pdf_num_ptrs +\
   gx_device_pdf_num_strings + num_resource_types * num_resource_chains +\
   max_outline_depth * 2)

#define private_st_device_pdfwrite()	/* in gdevpdf.c */\
  gs_private_st_composite_final(st_device_pdfwrite, gx_device_pdf,\
    "gx_device_pdf", device_pdfwrite_enum_ptrs, device_pdfwrite_reloc_ptrs,\
    device_pdfwrite_finalize)

/* ================ Utility procedures ================ */

/* ---------------- Exported by gdevpdf.c ---------------- */

/* ------ Document ------ */

/* Initialize the IDs allocated at startup. */
void pdf_initialize_ids(P1(gx_device_pdf * pdev));

/* Open the document if necessary. */
void pdf_open_document(P1(gx_device_pdf * pdev));

/* ------ Objects ------ */

/* Allocate an ID for a future object. */
long pdf_obj_ref(P1(gx_device_pdf * pdev));

/* Read the current position in the output stream. */
long pdf_stell(P1(gx_device_pdf * pdev));

/* Begin an object, optionally allocating an ID. */
long pdf_open_obj(P2(gx_device_pdf * pdev, long id));

/* Begin an object, allocating an ID. */
#define pdf_begin_obj(pdev) pdf_open_obj(pdev, 0)

/* End an object. */
int pdf_end_obj(P1(gx_device_pdf * pdev));

/* ------ Graphics ------ */

/* Reset the graphics state parameters to initial values. */
void pdf_reset_graphics(P1(gx_device_pdf * pdev));

/* Set the fill or stroke color. */
int pdf_set_color(P4(gx_device_pdf * pdev, gx_color_index color,
		     gx_drawing_color * pdcolor, const char *rgs));

/* Write matrix values. */
void pdf_put_matrix(P4(gx_device_pdf * pdev, const char *before,
		       const gs_matrix * pmat, const char *after));

/* Write a name, with escapes for unusual characters. */
void pdf_put_name(P3(const gx_device_pdf * pdev, const byte * nstr, uint size));

/* Write a string in its shortest form ( () or <> ). */
void pdf_put_string(P3(const gx_device_pdf * pdev, const byte * str, uint size));

/* Write a value, treating names specially. */
void pdf_put_value(P3(const gx_device_pdf * pdev, const byte * vstr, uint size));

/* ------ Page contents ------ */

/* Open a page contents part. */
/* Return an error if the page has too many contents parts. */
int pdf_open_contents(P2(gx_device_pdf * pdev, pdf_context context));

/* Close the current contents part if we are in one. */
int pdf_close_contents(P2(gx_device_pdf * pdev, bool last));

/* ------ Resources et al ------ */

/* Begin an object logically separate from the contents. */
/* (I.e., an object in the resource file.) */
long pdf_open_separate(P2(gx_device_pdf * pdev, long id));

#define pdf_begin_separate(pdev) pdf_open_separate(pdev, 0L)

/* Begin an aside (resource, annotation, ...). */
int pdf_begin_aside(P4(gx_device_pdf * pdev, pdf_resource ** plist,
		       const gs_memory_struct_type_t * pst,
		       pdf_resource ** ppres));

/* Begin a resource of a given type. */
int pdf_begin_resource(P4(gx_device_pdf * pdev, pdf_resource_type type,
			  gs_id rid, pdf_resource ** ppres));

/* Allocate a resource, but don't open the stream. */
int pdf_alloc_resource(P4(gx_device_pdf * pdev, pdf_resource_type type,
			  gs_id rid, pdf_resource ** ppres));

/* Find a resource of a given type by gs_id. */
pdf_resource *pdf_find_resource_by_gs_id(P3(gx_device_pdf * pdev,
					    pdf_resource_type type,
					    gs_id rid));

/* End a separate object. */
#define pdf_end_separate(pdev) pdf_end_aside(pdev)

/* End an aside. */
int pdf_end_aside(P1(gx_device_pdf * pdev));

/* End a resource. */
int pdf_end_resource(P1(gx_device_pdf * pdev));

/* ------ Pages ------ */

/* Get or assign the ID for a page. */
/* Returns 0 if the page number is out of range. */
long pdf_page_id(P2(gx_device_pdf * pdev, int page_num));

/* Open a page for writing. */
int pdf_open_page(P2(gx_device_pdf * pdev, pdf_context context));

/* Write saved page- or document-level information. */
int pdf_write_saved_string(P2(gx_device_pdf * pdev, gs_string * pstr));

/* Write the default entries of the Info dictionary. */
int pdf_write_default_info(P1(gx_device_pdf * pdev));

/* ------ Path drawing ------ */

bool pdf_must_put_clip_path(P2(gx_device_pdf * pdev, const gx_clip_path * pcpath));

int pdf_put_clip_path(P2(gx_device_pdf * pdev, const gx_clip_path * pcpath));

/* ---------------- Exported by gdevpdfm.c ---------------- */

/* Compare a C string and a gs_param_string. */
bool pdf_key_eq(P2(const gs_param_string * pcs, const char *str));

/* Scan an integer out of a parameter string. */
int pdfmark_scan_int(P2(const gs_param_string * pstr, int *pvalue));

/* Define the type for a pdfmark-processing procedure. */
/* If nameable is false, the objname argument is always NULL. */
#define pdfmark_proc(proc)\
  int proc(P5(gx_device_pdf *pdev, gs_param_string *pairs, uint count,\
	      const gs_matrix *pctm, const gs_param_string *objname))
/* Define an entry in a table of pdfmark-processing procedures. */
#define pdfmark_nameable 1	/* allows _objdef */
#define pdfmark_odd_ok 2	/* OK if odd # of parameters */
#define pdfmark_keep_name 4	/* don't substitute reference for name */
				/* in 1st argument */
typedef struct pdfmark_name_s {
    const char *mname;
         pdfmark_proc((*proc));
    byte options;
} pdfmark_name;

/* Process a pdfmark (called from pdf_put_params). */
int pdfmark_process(P2(gx_device_pdf * pdev, const gs_param_string_array * pma));

/* Close the current level of the outline tree. */
int pdfmark_close_outline(P1(gx_device_pdf * pdev));

/* Finish writing an article. */
int pdfmark_write_article(P2(gx_device_pdf * pdev, const pdf_article * part));

/* ---------------- Exported by gdevpdfo.c ---------------- */

/* Define the syntax of object names. */
#define pdfmark_objname_is_valid(data, size)\
  ((size) >= 2 && (data)[0] == '{' &&\
   memchr(data, '}', size) == (data) + (size) - 1)

/* Define the table of named-object pdfmark types. */
extern const pdfmark_name pdfmark_names_named[];

/* Replace object names with object references in a (parameter) string. */
int pdfmark_replace_names(P3(gx_device_pdf * pdev, const gs_param_string * from,
			     gs_param_string * to));

/* Write and free an entire list of named objects. */
int pdfmark_write_and_free_named(P2(gx_device_pdf * pdev,
				    pdf_named_object ** ppno));

/* ---------------- Exported by gdevpdft.c ---------------- */

/* Process a show operation (called from pdf_put_params). */
int pdfshow_process(P3(gx_device_pdf * pdev, gs_param_list * plist,
		       const gs_param_string * pts));

/* Begin a CharProc for an embedded (bitmap) font. */
int pdf_begin_char_proc(P8(gx_device_pdf * pdev, int w, int h, int x_width,
			   int y_offset, gs_id id, pdf_char_proc ** ppcp,
			   pdf_stream_position * ppos));

/* End a CharProc. */
int pdf_end_char_proc(P2(gx_device_pdf * pdev, pdf_stream_position * ppos));

/* Put out a reference to an image as a character in an embedded font. */
int pdf_do_char_image(P3(gx_device_pdf * pdev, const pdf_char_proc * pcp,
			 const gs_matrix * pimat));

#endif /* gdevpdfx_INCLUDED */
